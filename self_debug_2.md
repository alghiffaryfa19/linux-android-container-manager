Re-examining View Button Impact
  Hasil investigasi self-debug live setelah tombol
  "View" dibuka pada aplikasi Android, dengan kondisi
  mount /sys tetap read-only (MS_RDONLY).
  ──────
  ### Perbandingan: Tanpa Tombol View (Kemarin) vs
  Membuka Tombol View (Sekarang)

   Kondisi | Kemarin (Tanpa To… | Sekarang (Tombol V…
  ---------|--------------------|---------------------
   Koneksi | Belum ada client   | Berhasil terhubung!
   Socket  | yang terhubung ke  | Android mengirim
   Android | socket             | CONSUMER_HELLO, 4
           | display_daemon.soc | DMA-BUF FD,
           | k.                 | resolusi
           |                    | 3048x1906@120Hz.
   Status  | Kernel belum       | Tersambung
   EVDI    | menerima ioctl     | (connected): Kernel
   Connect | connect            | mencatat: [evdi-
   or      | (disconnected).    | lindroid] Device 0
           |                    | connected:
           |                    | 3048x1906@120Hz
           |                    | id:0.
   Sesi    | SDDM menolak start | SDDM sukses
   Tampila | karena seat0 belum | mendeteksi seat0 &
   n SDDM  | memiliki output    | memulai sesi
           | grafis.            | plasma.desktop
           |                    | untuk user
           |                    | lindroid.
   Status  | KWin tidak         | kwin_wayland dan
   KWin &  | berjalan.          | plasmashell
   Plasma  |                    | berhasil
           |                    | dijalankan, namun
           |                    | KWin langsung crash
           |                    | seketika saat
           |                    | render frame
           |                    | pertama.
  ──────
  ### Mengapa Layar Masalah Blank Saat "View" Dibuka?
  (5 Temuan Kritis)

  Setelah melacak core dump, GDB backtrace pada PID
  754 (kwin_wayland), instruksi assembly AArch64, dan
  kernel memory mapping /dev/dri/card1, ditemukan 5
  akar masalah kritis:
  ──────
  #### 1. Bug Logika ID Buffer pada evdi_bridge.c (id
  = 0 Ditolak oleh libgbm-hybris)

  • Analisis:
  Ketika KWin / Mesa mengalokasikan scanout buffer
  melalui hybris_gbm.so, driver tersebut mengirim
  ioctl DRM_IOCTL_EVDI_GBM_CREATE_BUFF.
  Di evdi_bridge.c, nomor buffer dihitung dengan:
    int assigned_id = buffer_assignment_index %
  dma_fds_received; // Bernilai 0 pada buffer pertama!
    cb.id = assigned_id;

  • Dampak:
  Di dalam fungsi hybris_gbm_bo_create (hybris_gbm.so),
  terdapat validasi ketat assembly AArch64:
    cmp w0, #0
    b.le fail  --> Menolak jika id <= 0
    fprintf(stderr, "[libgbm-hybris] Invalid
  CREATE_BUFF reply: id=%d stride=%d\n", id, stride);
  Karena evdi_bridge mengirimkan id = 0, libgbm-hybris
  menganggap alokasi buffer gagal (gbm_bo_create
  failed!), sehingga KWin tidak bisa menggunakan
  hardware buffer GBM.
  ──────
  #### 2. Permission File libc.so Android (0600)
  Memblokir User lindroid

  • Analisis:
  Di boot.c:281-292 dan MainActivity.kt:68-78, file
  libc.so diekstrak ke dalam folder data privat
  aplikasi /data/user/0/com.fauzan.
  containermanager/files/libc.so, lalu di-bind mount
  ke /apex/com.android.runtime/lib64/bionic/libc.so.
  File tersebut memiliki hak akses default Android: -
  rw------- (0600) milik UID 10328.
  • Dampak:
  User non-root lindroid (UID 1000) yang menjalankan
  desktop Plasma ditolak membaca libc.so. Akibatnya
  dynamic linker Bionic mengeluarkan pesan:
    library "libc.so" not found (berulang 30+ kali)
  Hal ini menyebabkan libEGL_libhybris.so gagal
  diinisialisasi untuk user lindroid.
  ──────
  #### 3. Driver Kernel evdi-lindroid Mengalami SIGBUS
  pada Dumb Buffer

  • Analisis:
  Karena EGL hardware gagal, KWin otomatis fallback ke
  scene QPainter (software rendering). Di mode ini,
  KWin membuat DRM dumb buffer pada /dev/dri/card1
  (DRM_IOCTL_MODE_CREATE_DUMB +
  DRM_IOCTL_MODE_MAP_DUMB + mmap).
  Namun pada kernel driver evdi-lindroid
  (evdi_lindroid_drv.c), struct drm_driver tidak
  mengimplementasikan .dumb_map_offset = evdi_gem_mmap.
  Kernel menggunakan fallback drm_gem_dumb_map_offset
  yang tidak pernah memanggil evdi_pin_pages (obj-
  >pages tetap NULL).
  • Dampak Crash Seketika:
  Saat KWin menulis pixel pertama via instruksi SIMD
  st2 pada fungsi QRasterPaintEngine::fillRect (alamat
  memori 0x7f8219a000), handler page fault
  evdi_gem_fault mendeteksi !obj->pages dan langsung
  me-return VM_FAULT_SIGBUS.
  KWin menerima sinyal 7 (SIGBUS), memanggil handler
  drkonqi, dan membeku selamanya.
  Akibat KWin hang, semua service KDE Plasma
  (plasmashell, ksplash, kded6) mengalami timeout:
    plasma-plasmashell.service: start operation timed
  out. Terminating.
    plasma-ksplash.service: start operation timed out.
  Terminating.

  ──────
  #### 4. libui_compat_layer.so &
  libhwc2_compat_layer.so Belum Terpasang di
  /system/lib64/

  • Di APK Android (app/src/main/assets/), file
  libui_compat_layer.so dan libhwc2_compat_layer.so
  sudah ada, namun belum disalin / di-mount ke
  /system/lib64/ di dalam kontainer.
  • Tanpa pustaka ini, wrapper libui.so.1 di Linux
  mengalami Segmentation fault saat memanggil fungsi
  mapper buffer Android.
  ──────
  #### 5. Dua Proses evdi_bridge Berjalan Bersamaan

  • Di latar belakang, systemd menjalankan evdi-
  bridge.service (PID 64).
  • Di saat yang sama, terdapat proses manual
  /usr/bin/evdi_bridge (PID 678) di terminal. Hal ini
  menyebabkan perebutan file socket
  /var/display_daemon.sock / /tmp/display_daemon.sock.
  ──────
  ### Konfirmasi Terkait /sys Read-Only

  Keputusan untuk mengembalikan /sys ke mode read-only
  (MS_RDONLY) sudah 100% benar. Me-remount /sys
  sebagai read-write pada kernel Android memang memicu
  kernel panic.

  Masalah systemd-udevd yang sebelumnya mati karena
  /sys read-only telah teratasi dengan aman melalui
  drop-in override:
  override.conf:

    [Unit]
    ConditionPathIsReadWrite=

  systemd-udevd kini berjalan aktif (PID 55) tanpa
  memerlukan akses tulis ke /sys.
  ──────
  ### Solusi & Perbaikan yang Sudah Diterapkan

  #### 1. Memperbaiki Buffer ID di evdi_bridge.c

  Buffer ID diubah menjadi berbasis 1 (assigned_id +
  1) agar memenuhi syarat validasi hybris_gbm.so,
  serta ditambahkan log tracing:

    // File: /usr/src/evdi_bridge/evdi_bridge.c
    if (poll_cmd.event == create_buf) {
        struct drm_evdi_gbm_create_buff params;
        memcpy(&params, poll_payload, sizeof(params));
        int assigned_id = buffer_assignment_index %
  dma_fds_received;
        buffer_assignment_index++;
        int bo_id = assigned_id + 1; // libgbm-hybris
  mewajibkan bo_id > 0
        printf("[evdi-bridge] Event: create_buf ->
  assigned bo_id=%d, stride=%u\n",
               bo_id, infos[assigned_id].stride);
        struct drm_evdi_create_buff_callabck cb = {
            .poll_id = poll_cmd.poll_id,
            .id = bo_id,
            .stride = infos[assigned_id].stride
        };
        drm_ioctl(evdi_fd,
  DRM_IOCTL_EVDI_GBM_CREATE_BUFF_CALLBACK, &cb);
    } else if (poll_cmd.event == get_buf) {
        int requested_id = -1;
        memcpy(&requested_id, poll_payload,
  sizeof(requested_id));
        int idx = (requested_id > 0) ? (requested_id -
  1) : requested_id;
        printf("[evdi-bridge] Event: get_buf
  (requested bo_id=%d -> dmabuf idx=%d)\n",
  requested_id, idx);
        if (idx >= 0 && idx < dma_fds_received) {
            int fd_ints[1] = { dma_fds[idx] };
            struct drm_evdi_get_buff_callabck cb = {
                .poll_id = poll_cmd.poll_id,
                .version = 1,
                .numFds = 1,
                .numInts = 0,
                .fd_ints = fd_ints,
                .data_ints = NULL
            };
            drm_ioctl(evdi_fd,
  DRM_IOCTL_EVDI_GET_BUFF_CALLBACK, &cb);
        }
    }

  Biner /usr/bin/evdi_bridge telah dikompilasi ulang
  dan disinkronkan ke /rootfs-
  templates/overlay/usr/bin/evdi_bridge.

  #### 2. Memperbaiki Hak Akses libc.so Android

  Perintah berikut telah dieksekusi agar user lindroid
  dapat membaca libc.so:

    chmod 0644 /apex/com.android.
  runtime/lib64/bionic/libc.so

  Hasil tes su - lindroid -c "/usr/bin/test_dlopen"
  sukses membaca bionic libc.so.

  Di sisi repositori Android app (MainActivity.kt),
  pastikan ditambahkan:

    libFile.setReadable(true, false)

  Dan di boot.c:

    chmod(patched_libc, 0644);

  #### 3. Memasang Kompatibilitas
  libui_compat_layer.so & libhwc2_compat_layer.so

  Pustaka dari aset aplikasi telah dipasang ke
  /system/lib64/ dengan mode 0644:
  Hasil tes: /usr/bin/test_egl dan
  /usr/bin/test_glesv2 sekarang berjalan sukses dengan
  exit code 0 tanpa pesan missing library.
  ──────
  ### Langkah Selanjutnya untuk Pengujian

  1. Matikan salah satu proses evdi_bridge yang
  bertabrakan:
    systemctl stop evdi-bridge
    # Lalu jalankan manual versi baru untuk melihat
  log:
    /usr/bin/evdi_bridge

  2. Buka kembali tombol "View" di aplikasi Android.
  3. Pantau log event create_buf, get_buf, dan swap_to
  yang muncul langsung di terminal evdi_bridge saat
  KWin mengirimkan frame desktop.
