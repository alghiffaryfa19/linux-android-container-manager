# Laporan Self-Debug Mendalam: Analisis & Solusi Layar Blank KDE Plasma pada Linux Android Container

## Ringkasan Eksekutif (Executive Summary)
Investigasi live debugging dilakukan langsung di dalam kontainer Lindroid (`aarch64`) dan repositori [`linux-android-container-manager`](file:///root/linux-android-container-manager) serta [`rootfs-templates`](file:///root/rootfs-templates). 

Ditemukan **4 Akar Masalah Kritis (Root Causes)** yang menyebabkan layar tetap blank (tidak ada tampilan KDE Plasma):
1. **Kehilangan DMA-BUF File Descriptors di `evdi_bridge.c`**: Menggunakan `recv()` alih-alih `recv_fds()` pada socket `data_fd` menyebabkan kernel membuang descriptor DMA-BUF. Hal ini mengakibatkan `dma_fds_received = 0`, menyebabkan crash `SIGFPE (Division by zero)` atau `SIGBUS` saat KWin mencoba menggambar background desktop.
2. **Infinite Loop CPU 83% & Blocking Mismatch pada `evdi_bridge.c`**: Pesan `CTRL_MSG_SCREEN_INFO` dari Android tidak pernah dibaca dari `client_sock`, menyebabkan `select()` terus-menerus mendeteksi socket siap baca (spin loop CPU tanpa jeda). Selain itu, DRM loopback driver tidak mengimplementasikan poll kustom untuk event EVDI, sehingga `select(evdi_fd)` tidak pernah memicu event handler.
3. **`systemd-udevd` Tidak Berjalan Karena `/sys` Mount Read-Only (`MS_RDONLY`) di `boot.c`**: Karena `/sys` di-mount read-only, kondisi `ConditionPathIsReadWrite=/sys` pada `systemd-udevd` gagal terpenuhi sehingga `udevd` mati. Akibatnya:
   - Symlink `/dev/dri/by-path/platform-evdi-lindroid.0-card` **tidak pernah terbuat** (KWin gagal dengan `ENOENT`).
   - `systemd-logind` tidak mendeteksi perangkat grafis pada `seat0` (`CanGraphical = false`), sehingga **SDDM menolak memulai display / greeter**.
4. **Konfigurasi SDDM & Inkompatibilitas Tema Qt 6**:
   - `plasma-wayland.conf` mengacu pada `LD_PRELOAD=/usr/lib/libtls-padding.so` yang tidak ada dan tidak menyertakan argumen `--drm` pada KWin.
   - Tema default Debian Breeze meminta `sddm-greeter-qt6` yang tidak tersedia di Debian Trixie (hanya ada Qt 5 greeter). Tanpa autologin, SDDM langsung berhenti.

Semua masalah di atas telah diperbaiki dan diverifikasi langsung pada sistem kontainer yang sedang berjalan.

---

## Detail Analisis & Akar Masalah

### 1. Bug Fatal SCM_RIGHTS pada `evdi_bridge.c` (Baris 252)
- **Gejala**: Logcat menunjukkan Android sukses mengirim 4 DMA-BUF (`do_connect: pushed dmabufs successfully`), namun KWin langsung crash dengan signal 7 (`SIGBUS`) pada `QRasterPaintEngine::fillRect` saat memanggil `renderBackground`.
- **Penyebab**:
  ```c
  // Kode Sebelumnya (Bermasalah):
  if (recv(data_fd, &msg_buf, sizeof(msg_buf), MSG_WAITALL) <= 0)
  ```
  Fungsi `recv()` standar tidak membaca data tambahan (`SCM_RIGHTS ancillary data`). Akibatnya:
  - Array `dma_fds` tetap kosong (`dma_fds_received = 0`).
  - `dmsg.size` berisi sampah stack tak terdefinisi.
  - Saat event `create_buf` atau `get_buf` dipanggil oleh KWin melalui EVDI ioctl, buffer assignment index modulo 0 menyebabkan crash atau mengirim FD acak ke kernel.
- **Solusi**: Diganti menggunakan `recv_fds(data_fd, &dmsg, sizeof(dmsg), dma_fds, MAX_BUFS, &dma_fds_received)` diikuti dengan validasi ketat `dma_fds_received > 0`.

---

### 2. Draining `CTRL_MSG_SCREEN_INFO` & Watchdog Thread di `evdi_bridge.c`
- **Penyebab**:
  Di sisi Android (`native_consumer.c:803`), setelah handshake Android langsung mengirimkan resolusi layar:
  ```c
  set_screen_info(s->ctx, s->screen_w, s->screen_h, PIXEL_FORMAT_RGBA_8888, s->refresh_mhz);
  ```
  `evdi_bridge.c` tidak pernah melakukan `recv` untuk pesan ini. Di dalam loop `select()`:
  ```c
  recv(client_sock, &dummy, 1, MSG_PEEK);
  ```
  `MSG_PEEK` hanya mengintip data tanpa menghapusnya dari buffer socket kernel. Akibatnya, `client_sock` selalu berada dalam status `POLLIN`, membuat `select()` berputar seketika tanpa tidur (menghabiskan 83% CPU kernel).
- **Solusi**:
  1. Handshake `sinfo_msg` didrain dan dibaca secara tuntas segera setelah `CONSUMER_HELLO`.
  2. Resolusi virtual display diambil langsung dari `sinfo_msg` (`disp_w`, `disp_h`, `disp_hz`).
  3. Mengganti `select()` campuran dengan blocking ioctl `DRM_IOCTL_EVDI_POLL` yang efisien (0% idle CPU) disertai thread *watchdog* khusus yang memantau penutupan socket Android.

---

### 3. Masalah `sysfs` Read-Only di `boot.c` & Matinya `udev`
- **Penyebab**:
  Di `app/src/main/cpp/boot.c:202`:
  ```c
  mount("sysfs", "sys", "sysfs", MS_RDONLY, NULL);
  ```
  Karena `/sys` berstatus `ro`:
  1. Unit `systemd-udevd.service` memiliki klausul:
     ```ini
     ConditionPathIsReadWrite=/sys
     ```
     Kondisi ini gagal, sehingga `udevd` tidak aktif.
  2. Tanpa udevd, direktori `/dev/dri/by-path/` kosong.
  3. `systemd-logind` memeriksa device tag `master-of-seat`. Tanpa event dari udev, properti seat0 menjadi:
     ```
     org.freedesktop.login1.Seat CanGraphical = false
     ```
  4. SDDM memeriksa `CanGraphical`. Karena bernilai `false`, SDDM berasumsi sistem tidak memiliki monitor fisik/grafis dan **tidak pernah memunculkan desktop**.
- **Solusi**:
  1. Di [`boot.c`](file:///root/linux-android-container-manager/app/src/main/cpp/boot.c): `/sys` di-mount read-write (`0` alih-alih `MS_RDONLY`), serta ditambahkan pembuatan symlink direktori `/dev/dri/by-path/` secara manual saat kontainer booting.
  2. Di rootfs template: Ditambahkan systemd override `/etc/systemd/system/systemd-udevd.service.d/override.conf` yang mengosongkan `ConditionPathIsReadWrite=` agar udevd tetap dapat berjalan dalam skenario kontainer apa pun.

---

### 4. Konfigurasi Autologin SDDM & Perbaikan KWin Environment
- **Penyebab**:
  Debian Trixie membawa tema KDE 6 (`sddm-theme-debian-breeze`) dengan tag `QtVersion=6`, sedangkan binary `/usr/bin/sddm-greeter` yang terinstal dikompilasi dengan Qt 5. Hal ini membuat SDDM greeter gagal memuat tema login screen.
- **Solusi**:
  Mengaktifkan **Autologin langsung ke user `lindroid`** dengan session `plasma.desktop`:
  ```ini
  [Autologin]
  User=lindroid
  Session=plasma.desktop
  Relogin=false
  ```
  Dengan autologin, SDDM langsung mengeksekusi `startplasma-wayland` tanpa memerlukan greeter UI yang bermasalah.

---

## Perubahan Kode yang Diterapkan

### 1. Repositori `rootfs-templates`
- **File**: [`overlay/usr/src/evdi_bridge/evdi_bridge.c`](file:///root/rootfs-templates/overlay/usr/src/evdi_bridge/evdi_bridge.c)
  - Diperbarui dengan implementasi `recv_fds` yang benar untuk DMA-BUF.
  - Penanganan pesan `CTRL_MSG_SCREEN_INFO`.
  - Loop event blocking `DRM_IOCTL_EVDI_POLL` hemat daya (0% CPU) dengan watchdog thread.
  - Pemanggilan otomatis `udevadm trigger` dan `systemctl restart sddm` saat display EVDI terhubung.
- **File**: [`overlay/etc/sddm.conf.d/plasma-wayland.conf`](file:///root/rootfs-templates/overlay/etc/sddm.conf.d/plasma-wayland.conf)
  - Mengarahkan DRM device ke `/dev/dri/card1:/dev/dri/card0`.
  - Menghapus preload invalid `/usr/lib/libtls-padding.so`.
  - Menambahkan flag `--drm` pada `CompositorCommand`.
- **File**: [`overlay/etc/sddm.conf.d/autologin.conf`](file:///root/rootfs-templates/overlay/etc/sddm.conf.d/autologin.conf)
  - Mengonfigurasi autologin untuk user `lindroid`.
- **File**: [`overlay/etc/systemd/system/systemd-udevd.service.d/override.conf`](file:///root/rootfs-templates/overlay/etc/systemd/system/systemd-udevd.service.d/override.conf)
  - Mengaktifkan `systemd-udevd` di dalam kontainer.

### 2. Repositori `linux-android-container-manager`
- **File**: [`app/src/main/cpp/boot.c`](file:///root/linux-android-container-manager/app/src/main/cpp/boot.c)
  - Baris 202: Mengubah mount `sysfs` menjadi read-write.
  - Baris 250: Menambahkan pembuatan symlink `/dev/dri/by-path/` secara otomatis saat boot.
- **File**: [`app/src/main/cpp/native_consumer.c`](file:///root/linux-android-container-manager/app/src/main/cpp/native_consumer.c)
  - Menambahkan pengecekan null sebelum memanggil `GetObjectClass` pada `on_fallback` dan `on_exit_fallback` untuk mencegah Android crash `SIGABRT (JNI DETECTED ERROR: java_object == null)`.

---

## Status Verifikasi Sistem Terakhir (Live Verification)
Pada pengujian live di dalam kontainer:
- `evdi-bridge.service`: **Active (running)**, mendengarkan di `/var/display_daemon.sock` dengan penggunaan memori 184 KB dan 0% CPU.
- `systemd-udevd.service`: **Active (running)**, mengelola DRM uevents.
- `systemd-logind.service`: Properti `org.freedesktop.login1.Seat CanGraphical` bernilai **`true`**.
- `sddm.service`: Berhasil mengautentikasi user `lindroid` dan meluncurkan `plasma.desktop` (`startplasma-wayland` -> `kwin_wayland` -> `plasmashell`).
