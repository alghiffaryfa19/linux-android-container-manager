# Chat Export — Debugging Lindroid / EVDI / KWin Wayland

**Tanggal:** 9 September 2026  
**Topik:** Lindroid graphics stack, `evdi-lindroid`, EVDI bridge, KWin Wayland, EGL/libhybris

---

## 1. Arsitektur Saat Ini

Lingkungan yang sedang didiagnosis adalah Linux rootfs berbasis Android, dengan target menjalankan Plasma/KWin Wayland melalui stack grafis custom:

- Native DRM GPU: `/dev/dri/card0`
- Custom DRM driver: `evdi-lindroid`
- `card1` = `evdi-lindroid.0`
- Connector: `card1-Virtual-3`
- EVDI userspace/library
- `evdi-bridge` untuk menerima DMA-BUF dari Android/display daemon
- Plasma/KWin Wayland
- libhybris dan custom compatibility layers

Node DRM yang tersedia:

```text
/dev/dri/card0
/dev/dri/card1
/dev/dri/card2
/dev/dri/card3
```

Render nodes sebelumnya:

```text
/dev/dri/renderD128
/dev/dri/renderD129
/dev/dri/renderD130
```

---

## 2. Kondisi DRM card1

`card1` adalah custom `evdi-lindroid`.

Connector:

```text
/sys/class/drm/card1-Virtual-3/status
connected
```

Mode:

```text
/sys/class/drm/card1-Virtual-3/modes
3048x1906
```

Namun:

```text
/sys/class/drm/card1-Virtual-3/enabled
disabled
```

DPMS:

```text
On
```

EDID:

```text
/sys/class/drm/card1-Virtual-3/edid
```

File EDID kosong, ukuran 0 byte.

Ini kemudian terbukti menjadi masalah untuk Cage, tetapi bukan penyebab utama kegagalan KWin terbaru.

---

## 3. KWin Awalnya Gagal di Session / Seat

Log awal KWin:

```text
No backend specified, automatically choosing drm
kwin_core: Could not determine the active graphical session
kwin_wayland_drm: failed to open drm device at "/dev/dri/card1"
No suitable DRM devices have been found
```

Environment yang digunakan:

```bash
export XDG_RUNTIME_DIR=/run/user/0
export XDG_SESSION_TYPE=wayland
export WAYLAND_DISPLAY=wayland-0
export LANG=C.UTF-8
export LC_ALL=C.UTF-8
export KWIN_DRM_DEVICES=/dev/dri/card1
```

`/run/user/0` tersedia dengan mode 700.

`systemd-logind` berjalan.

---

## 4. systemd-udevd dan /sys Read-only

`/sys` harus tetap read-only.

Kondisi saat ini:

```text
findmnt -no OPTIONS /sys
ro,relatime,seclabel
```

Jangan melakukan:

```bash
mount -o remount,rw /sys
```

karena sebelumnya menyebabkan kernel panic.

`systemd-udevd` awalnya tidak mau berjalan karena:

```text
ConditionPathIsReadWrite=/sys was not met
```

Kemudian dibuat override:

```text
/etc/systemd/system/systemd-udevd.service.d/override.conf
```

Isi:

```ini
[Unit]
ConditionPathIsReadWrite=
```

Setelah:

```bash
systemctl daemon-reload
systemctl start systemd-udevd
```

udevd berhasil aktif.

Trigger:

```bash
udevadm trigger --subsystem-match=drm
```

menghasilkan error pada native `card0` karena `/sys` read-only:

```text
card0: Failed to write 'change' to '/sys/devices/.../drm/card0/uevent': Read-only file system
```

Hal tersebut dapat diterima.

---

## 5. udev Property card1

Setelah udevd aktif:

```bash
udevadm info --query=property --name=/dev/dri/card1
```

menghasilkan:

```text
DEVPATH=/devices/evdi-lindroid/evdi-lindroid.0/drm/card1
DEVNAME=/dev/dri/card1
DEVTYPE=drm_minor
MAJOR=226
MINOR=1
SUBSYSTEM=drm
USEC_INITIALIZED=2265350052
ID_PATH=platform-evdi-lindroid.0
ID_PATH_TAG=platform-evdi-lindroid_0
ID_FOR_SEAT=drm-platform-evdi-lindroid_0
DEVLINKS=/dev/dri/by-path/platform-evdi-lindroid.0-card
TAGS=:seat:master-of-seat:uaccess:
CURRENT_TAGS=:seat:master-of-seat:uaccess:
```

Jadi card1 sudah memiliki tag seat.

---

## 6. logind / seat0

```bash
loginctl show-seat seat0
```

menghasilkan:

```text
Id=seat0
ActiveSession=c2
CanTTY=yes
CanGraphical=yes
Sessions=c2
IdleHint=no
```

`loginctl seat-status seat0` menunjukkan:

```text
seat0
Sessions: *c2
Devices: n/a

/sys/devices/evdi-lindroid/evdi-lindroid.0/drm/card1
    [MASTER] drm:card1
    [MASTER] drm:card1-Virtual-3

drm:renderD129

/sys/devices/evdi-lindroid/evdi-lindroid.1/drm/card2

drm:renderD130

/sys/devices/evdi-lindroid/evdi-lindroid.2/drm/card3
```

Dengan kondisi ini, masalah session/seat KWin sudah terlewati.

---

## 7. Bukti KWin Sekarang Membuka card1

Sebelumnya `strace` tidak menemukan:

```text
openat(... "/dev/dri/card1" ...)
```

karena KWin gagal sebelum mencapai DRM.

Setelah perbaikan udev/logind, KWin sudah benar-benar membuka card1.

Pernah terlihat:

```text
root       61  F.... evdi_bridge
lindroid 1901  ....m kwin_wayland
```

KWin PID 1901.

Proses:

```bash
ps -o pid,ppid,stat,%cpu,%mem,etime,wchan:32,cmd -p 1901
```

pernah menghasilkan:

```text
PID    PPID STAT %CPU %MEM ELAPSED WCHAN     CMD
1901 1896 Sl 98.8 1.0 05:23 do_wait /usr/bin/kwin_wayland --wayland-fd 7 -
```

Jadi KWin sudah melewati masalah membuka DRM device.

---

# 8. Cage / kwin-evdi.service

Service bernama:

```text
kwin-evdi.service
```

ternyata menjalankan Cage compositor, bukan KWin.

Cage gagal berulang kali dengan:

```text
[ERROR] [backend/drm/util.c:65] Failed to parse EDID
kwin-evdi.service: Failed with result 'signal'
```

Penyebab langsungnya adalah EDID kosong pada:

```text
card1-Virtual-3
```

Jika Plasma KWin ingin digunakan, Cage sebaiknya dihentikan agar tidak berkompetisi sebagai compositor.

Untuk menghentikan:

```bash
systemctl stop kwin-evdi.service
```

Jika ingin permanen:

```bash
systemctl disable kwin-evdi.service
```

Namun EDID Cage bukan prioritas sebelum EGL KWin diperbaiki.

---

# 9. EVDI Bridge

Bridge menerima koneksi Android melalui:

```text
/tmp/display_daemon.sock
```

Menerima FD:

```text
data_fd
shm_fd
fence_fd
audio_fd
```

Kemudian menerima DMA-BUF.

Log terbaru:

```text
Connected to Android app!
Got connection FDs: data_fd=7, shm_fd=8, fence_fd=6, audio_fd=9
Screen info: 3048x1906 (format: 1, refresh: 120000 mHz)
Waiting for DMA-BUFs on data_fd...
Received 4 DMA-BUFs!
Buffer 0: 3048x1906 stride=13312 format=0x1 modifier=0x0
Checking DRM card0...
Checking DRM card1...
Opening /dev/dri/card1...
Opened /dev/dri/card1 successfully (fd=18)
Found EVDI device at /dev/dri/card1
Using /dev/dri/card1 (fd=18)
Display mode:
    logical : 3048x1906@60Hz
    stride  : 13312 bytes
    buffer  : 3328x1906
Connecting EVDI: 3048x1906@60Hz
Connected virtual display 3048x1906@60Hz
DRM master dropped successfully
Waiting for EVDI connector to become active...
Bridge loop running. Waiting for EVDI events...
```

Android melaporkan:

```text
3048x1906
120000 mHz
```

Tetapi bridge saat ini sengaja menggunakan:

```text
3048x1906@60Hz
```

Jangan ubah ke 120 Hz dulu.

---

# 10. Perbaikan pada EVDI Bridge

Ada dua bug yang sebelumnya ditemukan.

## Bug 1 — drmDropMaster dipanggil dua kali

Pemanggilan kedua menghasilkan:

```text
EINVAL
```

Sudah diperbaiki sehingga hanya sekali:

```text
DRM master dropped successfully
```

## Bug 2 — Mode width salah menggunakan stride

Kode lama menggunakan:

```c
aligned_w = infos[0].stride / 4;
```

Hasilnya:

```text
3328
```

Padahal 3328 adalah lebar storage buffer berdasarkan stride, bukan logical display mode.

Sekarang:

```text
logical : 3048x1906
stride  : 13312 bytes
buffer  : 3328x1906
```

Ini lebih benar.

---

# 11. Potensi Masalah Bridge Berikutnya

Setelah:

```c
drmDropMaster()
```

bridge tetap menjalankan private EVDI ioctl.

KWin kemudian perlu mengambil DRM master.

Ini belum menjadi blocker utama karena KWin sudah sampai EGL initialization.

Watchdog bridge juga pernah diperbaiki agar tidak menutup:

```text
g_evdi_fd
```

Watchdog masih membaca arbitrary bytes dari client socket, sehingga secara arsitektur ada kemungkinan ia mengonsumsi control message di masa depan, tetapi bukan prioritas saat ini.

---

# 12. KWin Failure Terbaru — EGL

Ini adalah titik paling penting saat ini.

Log KWin:

```text
systemd[92]: Starting plasma-kwin_wayland.service - KDE Window Manager...
systemd[92]: Started plasma-kwin_wayland.service - KDE Window Manager.

kwin_wayland_wrapper[1901]: No backend specified, automatically choosing drm

kwin_xkbcommon: XKB: couldn't find a Compose file for locale "en_US.UTF8" ...

kwin_wayland_wrapper[1901]: library "libc.so" not found
(repeated ~30 times)

kwin_wayland_drm: "EGL_EXT_platform_base" client extension is not supported by the platform

kwin_scene_opengl: Creating the OpenGL rendering failed: "Could not initialize egl"

kwin_core: Could not fulfill the requested compositing mode in KWIN_COMPOSE: 1 . Exiting.

KCrash: Application 'kwin_wayland' crashing...
```

Interpretasi:

1. KWin sudah melewati session/seat.
2. KWin sudah membuka `/dev/dri/card1`.
3. KWin mencapai EGL initialization.
4. EGL/OpenGL initialization gagal.
5. KWin menggunakan/requested OpenGL compositing.
6. Karena EGL gagal, KWin keluar.

Blocker utama sekarang:

```text
EGL initialization
```

Bukan lagi:

```text
session/seat
DRM device open
```

---

# 13. Tentang Error libc.so

KWin mencetak:

```text
library "libc.so" not found
```

Awalnya terlihat seperti libc hilang.

Namun kemudian ditemukan:

```bash
/lib/aarch64-linux-gnu/libc.so
/lib/aarch64-linux-gnu/libc.so.6
```

Jadi ini bukan sekadar file libc ELF yang hilang.

Kesimpulan sementara:

```text
library "libc.so" not found
```

kemungkinan berasal dari custom graphics loader/component, bukan dynamic linker standar.

---

# 14. Library EGL

Ditemukan:

```text
/lib/aarch64-linux-gnu/libEGL.so -> libEGL.so.1
/lib/aarch64-linux-gnu/libEGL.so.1 -> libEGL.so.1.1.0
/lib/aarch64-linux-gnu/libEGL.so.1.1.0
```

dan:

```text
/usr/lib/aarch64-linux-gnu/libEGL.so -> libEGL.so.1
/usr/lib/aarch64-linux-gnu/libEGL.so.1 -> libEGL.so.1.1.0
/usr/lib/aarch64-linux-gnu/libEGL.so.1.1.0
```

libGL:

```text
/usr/lib/aarch64-linux-gnu/libGL.so
/usr/lib/aarch64-linux-gnu/libGL.so.1
/usr/lib/aarch64-linux-gnu/libGL.so.1.7.0
```

---

# 15. ldconfig EGL / GL

```bash
ldconfig -p | grep -Ei 'libEGL|libGLES|libGL|gbm'
```

menunjukkan antara lain:

```text
libgbm.so.1
libEGL_mesa.so.0
libEGL_libhybris.so.0
libEGL.so.1
libGLESv2.so.2
libGLESv2_libhybris.so.2
libGLESv1_CM_libhybris.so.1
```

Juga terdapat GLVND:

```text
libGLdispatch
libGLX
libGLX_mesa
libGL
```

---

# 16. GLVND Vendor JSON

Ditemukan:

```text
/usr/share/glvnd/egl_vendor.d/50_mesa.json
```

isi:

```json
{
    "file_format_version" : "1.0.0",
    "ICD" : {
        "library_path" : "libEGL_mesa.so.0"
    }
}
```

dan:

```text
/usr/share/glvnd/egl_vendor.d/10_libhybris.json
```

isi:

```json
{
    "file_format_version" : "1.0.0",
    "ICD" : {
        "library_path" : "libEGL_libhybris.so.0"
    }
}
```

libEGL adalah GLVND.

`strings` pada libEGL menunjukkan:

```text
/etc/glvnd/egl_vendor.d:/usr/share/glvnd/egl_vendor.d
__EGL_VENDOR_LIBRARY_FILENAMES
__EGL_VENDOR_LIBRARY_DIRS
```

---

# 17. readelf libEGL

```bash
readelf -d /lib/aarch64-linux-gnu/libEGL.so.1
```

menghasilkan dependency utama:

```text
NEEDED libGLdispatch.so.0
NEEDED libc.so.6
NEEDED ld-linux-aarch64.so.1
SONAME libEGL.so.1
```

Jadi dynamic linker standar menemukan libc.

---

# 18. Dependency KWin

```bash
ldd /usr/bin/kwin_wayland | grep -Ei 'egl|gles|gl|gbm|hybris'
```

menghasilkan antara lain:

```text
libgbm
libQt6OpenGL
libGLESv2
libEGL
libOpenGL
libGLdispatch
```

Tidak terlihat dependency `not found`.

---

# 19. libhybris

Terdapat library:

```text
libEGL_libhybris.so.0.0.0
libGLESv1_CM_libhybris.so.1.0.1
libGLESv2_libhybris.so.2.0.0
libhybris-common.so.1.0.0
libhybris-eglplatformcommon.so.1.0.0
libhybris-hwcomposerwindow.so.1.0.0
libhybris-platformcommon.so.1.0.0
libhybris-vulkanplatformcommon.so.1.0.0
```

`ldconfig` juga menunjukkan:

```text
libhybris-vulkanplatformcommon.so.1
libhybris-platformcommon.so.1
libhybris-hwcomposerwindow.so.1
libhybris-eglplatformcommon.so.1
libhybris-common.so.1
libhardware.so.2
libgralloc.so.1
libdrihybris.so
libui.so.1
```

---

# 20. ldd libhybris

```bash
ldd libEGL_libhybris.so.0
```

menunjukkan dependency seperti:

```text
libhybris-common.so.1
libstdc++.so.6
libc.so.6
ld-linux-aarch64.so.1
libgcc_s.so.1
libm.so.6
```

Tidak ada:

```text
not found
```

`libGLESv2_libhybris.so.2` juga tidak menunjukkan dependency hilang.

---

# 21. Mesa EGL

```bash
ldd libEGL_mesa.so.0
```

menunjukkan dependency seperti:

```text
libgallium-25.0.7-2+deb13u1.so
libgbm
libdrm
xcb
wayland
LLVM
zlib
```

Tidak terlihat dependency `not found`.

Namun probe sederhana dengan Mesa menggunakan:

```text
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
```

menghasilkan:

```text
EGL test
eglInitialize: FAILED
EGL error: 0x3001
```

Error:

```text
0x3001 = EGL_NOT_INITIALIZED
```

Catatan: ini belum membuktikan Mesa tidak bisa bekerja dengan KWin karena probe menggunakan `eglGetDisplay(EGL_DEFAULT_DISPLAY)`. KWin menggunakan platform/GBM/DRM yang lebih spesifik.

---

# 22. Probe EGL — Temuan Kritis

Dibuat probe:

```text
/tmp/egltest
```

Saat dijalankan:

```text
EGL test
library "libui_compat_layer.so" not found
Segmentation fault
```

Saat dipaksa menggunakan Mesa:

```bash
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json /tmp/egltest
```

hasil:

```text
EGL test
eglInitialize: FAILED
EGL error: 0x3001
```

Saat dipaksa menggunakan libhybris:

```bash
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_libhybris.json /tmp/egltest
```

hasil:

```text
EGL test
library "libui_compat_layer.so" not found
Segmentation fault
```

Ini adalah temuan terpenting:

```text
libhybris EGL mencoba memuat libui_compat_layer.so
```

Library tersebut tidak ditemukan pada environment probe saat ini.

Setelah gagal load, komponen tersebut kemudian crash.

---

# 23. Custom Compatibility Layer

Dalam pekerjaan sebelumnya terdapat:

```text
/dev/lindroid_libs/libhwc2_compat_layer.so
/dev/lindroid_libs/libui_compat_layer.so
```

Juga pernah digunakan:

```text
/dev/lindroid_libs/*.so
```

dan pernah diperiksa dengan:

```bash
file /dev/lindroid_libs/*.so
readelf -d /dev/lindroid_libs/libhwc2_compat_layer.so
readelf -d /dev/lindroid_libs/libui_compat_layer.so
```

Pada rootfs tertentu `file` dan `readelf` sempat belum terinstall.

Keberadaan aktual `libui_compat_layer.so` pada rootfs saat ini belum diverifikasi pada titik terakhir percakapan.

---

# 24. Diagnosis Saat Ini

Status problem:

```text
Android display daemon
        |
        v
DMA-BUF
        |
        v
evdi-bridge
        |
        v
/dev/dri/card1
        |
        v
evdi-lindroid
        |
        v
KWin Wayland
        |
        v
EGL
        |
        +---- libhybris
        |       |
        |       +---- libui_compat_layer.so NOT FOUND
        |                    |
        |                    v
        |                 crash
        |
        +---- Mesa
                |
                +---- EGL_DEFAULT_DISPLAY
                       EGL_NOT_INITIALIZED
```

Dengan kata lain:

**KWin sudah berhasil mencapai DRM device, tetapi gagal di EGL/OpenGL initialization.**

Untuk jalur libhybris, masalah konkret yang sudah terbukti adalah:

```text
libui_compat_layer.so
```

tidak ditemukan oleh loader.

---

# 25. Langkah Diagnostik Berikutnya

Jangan langsung membuat symlink atau mengubah GLVND secara permanen.

Pertama cari library:

```bash
find / -name 'libui_compat_layer.so*' -o -name 'libhwc2_compat_layer.so*' 2>/dev/null
```

Lalu:

```bash
ldconfig -p | grep -E 'libui_compat_layer|libhwc2_compat_layer'
```

Cari referensi library:

```bash
grep -Rali 'libui_compat_layer.so' \
    /lib/aarch64-linux-gnu \
    /usr/lib/aarch64-linux-gnu \
    /dev/lindroid_libs \
    2>/dev/null | head -100
```

Untuk mengetahui loader sebenarnya:

```bash
LD_DEBUG=libs \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_libhybris.json \
/tmp/egltest 2>&1 | \
grep -E 'libui_compat|libEGL|libhybris|calling init|error' | head -100
```

Jika library ditemukan di `/dev/lindroid_libs`, jangan copy dulu. Uji dengan:

```bash
LD_LIBRARY_PATH=/dev/lindroid_libs:$LD_LIBRARY_PATH /tmp/egltest
```

dan:

```bash
LD_LIBRARY_PATH=/dev/lindroid_libs:$LD_LIBRARY_PATH \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_libhybris.json \
/tmp/egltest
```

Jika ditemukan:

```bash
UI=$(find / -name 'libui_compat_layer.so*' 2>/dev/null | head -1)
echo "$UI"
ldd "$UI"
ldd "$UI" | grep 'not found'
```

---

# 26. Environment KWin yang Perlu Dicek

Root shell bukan environment user `lindroid`, sehingga:

```bash
systemctl --user show-environment
```

dari root tidak bisa dijadikan acuan.

KWin berjalan sebagai user:

```text
lindroid
```

Jika PID KWin aktif:

```bash
pgrep -a kwin_wayland
```

kemudian:

```bash
tr '\0' '\n' < /proc/PID/environ | \
grep -E 'KWIN|EGL|GBM|LIBGL|LD_LIBRARY'
```

Ini penting untuk melihat apakah service KWin memiliki:

```text
LD_LIBRARY_PATH
EGL variables
GBM variables
KWIN_COMPOSE
```

dan environment graphics lainnya.

---

# 27. Hal yang Jangan Dilakukan Dulu

Jangan dulu:

```text
remount /sys menjadi RW
```

karena menyebabkan kernel panic.

Jangan langsung:

```text
mengubah GLVND vendor JSON
```

Jangan langsung membuat:

```text
symlink libui_compat_layer.so
```

Jangan langsung mengubah:

```text
EDID
```

Jangan fokus ke Cage sebelum EGL KWin selesai.

Jangan menyimpulkan Mesa rusak hanya dari:

```text
eglGetDisplay(EGL_DEFAULT_DISPLAY)
EGL_NOT_INITIALIZED
```

Karena KWin kemungkinan menggunakan explicit GBM/DRM platform.

---

# 28. Status Akhir

### Sudah berhasil

- `/dev/dri/card1` tersedia.
- `evdi-lindroid` terdeteksi.
- Connector `Virtual-3` terdeteksi.
- Mode `3048x1906` tersedia.
- udevd berhasil aktif walaupun `/sys` read-only.
- card1 mendapatkan seat tag.
- logind mengenali card1.
- KWin berhasil melewati session/seat.
- KWin berhasil membuka `/dev/dri/card1`.
- EVDI bridge berhasil menerima DMA-BUF.
- EVDI bridge berhasil connect ke card1.
- DRM master berhasil dilepas sekali.

### Masih bermasalah

1. KWin EGL initialization:
   ```text
   EGL_EXT_platform_base client extension is not supported
   Could not initialize egl
   ```

2. libhybris:
   ```text
   library "libui_compat_layer.so" not found
   Segmentation fault
   ```

3. Cage:
   ```text
   Failed to parse EDID
   ```

4. card1 EDID kosong:
   ```text
   /sys/class/drm/card1-Virtual-3/edid
   ```
   ukuran 0 byte.

### Prioritas berikutnya

**Cari dan validasi `libui_compat_layer.so`, kemudian uji EGL libhybris dengan `LD_LIBRARY_PATH` yang benar.**

