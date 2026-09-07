Export Chat — Lindroid / EVDI / DRM / KWin Debugging

Tanggal: 8 September 2026
Topik: Lindroid, EVDI, custom DRM driver, DMA-BUF, libhybris, KWin, Plasma Wayland

---

1. Tujuan Proyek

Percakapan ini membahas pengembangan dan debugging stack graphics Linux/Android pada perangkat yang menggunakan lingkungan Android-derived.

Komponen utama:

- Linux rootfs / Lindroid
- Custom kernel driver "evdi-lindroid"
- EVDI userspace library
- "evdi_bridge"
- Android/Anland display path
- DRM/KMS
- DMA-BUF
- libhybris
- KWin
- Plasma Wayland

Target akhirnya adalah membuat display virtual dari Android dapat digunakan oleh Linux/Plasma melalui DRM/KMS sehingga KWin dapat menggunakan output tersebut.

Arsitektur yang sedang dikerjakan kira-kira:

Android Display
      │
      ▼
 Android / Anland
      │
      ▼
 evdi_bridge
      │
      ▼
 EVDI userspace library
      │
      ▼
 evdi-lindroid kernel DRM driver
      │
      ▼
 /dev/dri/card1
      │
      ▼
 card1-Virtual-3
      │
      ▼
 DRM/KMS
      │
      ▼
 KWin Wayland
      │
      ▼
 Plasma

---

2. Perangkat dan Environment

Perangkat yang banyak dibahas adalah Xiaomi Pad 6s Pro dengan codename:

sheng

Linux rootfs yang digunakan memiliki environment Debian/Linux dan dijalankan dalam konteks Lindroid.

DRM nodes yang tersedia:

/dev/dri/card0
/dev/dri/card1
/dev/dri/card2

/dev/dri/renderD128
/dev/dri/renderD129
/dev/dri/renderD130

Custom DRM device adalah:

/dev/dri/card1
/dev/dri/renderD129

Driver:

evdi-lindroid

---

3. Custom EVDI Driver

Driver custom yang digunakan:

evdi-lindroid

Driver tersebut mendaftarkan DRM device dan beberapa virtual connector.

Pada:

/sys/class/drm/card1/

terdapat:

card1-Virtual-3
card1-Virtual-4
card1-Virtual-5
card1-Virtual-6
card1-Virtual-7

Symlink device:

device -> ../../../evdi-lindroid.0

---

4. Status DRM Driver

Perintah:

cat /sys/class/drm/card1/device/driver/module/version

tidak menghasilkan output.

Namun:

cat /sys/class/drm/card1/device/uevent

menghasilkan:

DRIVER=evdi-lindroid
MODALIAS=platform:evdi-lindroid

Hal ini menunjukkan bahwa device memang menggunakan driver:

evdi-lindroid

---

5. DRM Connector

Status terakhir semua connector adalah:

card1-Virtual-3 disconnected
card1-Virtual-4 disconnected
card1-Virtual-5 disconnected
card1-Virtual-6 disconnected
card1-Virtual-7 disconnected

Tidak terdapat mode pada connector tersebut.

Namun sebelumnya pernah ditemukan kondisi:

card1-Virtual-3 connected
3048x1906

sedangkan:

card1-Virtual-4 disconnected

Ini merupakan petunjuk penting karena membuktikan bahwa driver custom sebenarnya pernah mampu membuat:

Virtual-3

menjadi connected dengan mode:

3048x1906

---

6. "/dev/dri"

Kondisi device:

card0
card1
card2
renderD128
renderD129
renderD130

Permission yang terlihat:

root:gid 1003
crw-rw-rw

Pada pengecekan tertentu:

fuser -v /dev/dri/card0
fuser -v /dev/dri/card1
fuser -v /dev/dri/renderD129

tidak menunjukkan proses.

Ketika bridge aktif pada kondisi tertentu sebelumnya:

fuser -v /dev/dri/card1

pernah menghasilkan:

USER        PID ACCESS COMMAND
/dev/dri/card1:      root         97 F.... evdi_bridge

Artinya "evdi_bridge" memang pernah membuka "card1".

---

7. "evdi_bridge"

Service yang digunakan:

evdi-bridge.service

Executable:

/usr/bin/evdi_bridge

Status service ketika dijalankan:

Active: active (running)
Main PID: 266 (evdi_bridge)

Namun service aktif tidak otomatis membuat connector menjadi connected.

Saat:

systemctl start evdi-bridge

dijalankan, connector tetap:

card1-Virtual-3 disconnected
card1-Virtual-4 disconnected
card1-Virtual-5 disconnected
card1-Virtual-6 disconnected
card1-Virtual-7 disconnected

---

8. Log Service

Sebelumnya:

journalctl -u evdi-bridge -f

menghasilkan hanya:

Sep 05 19:24:50 lindroid systemd[1]: Started evdi-bridge.service - EVDI to Anland Bridge Service.

Tidak ada log tambahan yang menjelaskan proses connect, mode, EDID, atau hotplug.

Ini menjadi salah satu bagian yang perlu diselidiki.

---

9. "modetest"

Percobaan awal:

modetest -D /dev/dri/card1

menghasilkan:

failed to open device '(null)' with busid '/dev/dri/card1': No such file or directory

Kemudian dicoba:

modetest -D evdi-lindroid

dan menghasilkan error serupa.

Setelah menjalankan:

modetest -h

diketahui bahwa build "modetest" yang digunakan menyediakan:

-M module
-D device

Pada build ini, "-D" bukan cara yang benar untuk memberikan path "/dev/dri/card1".

---

10. "modetest -M evdi-lindroid"

Perintah:

modetest -M evdi-lindroid

berhasil.

Output penting:

opened device `Lindroid Virtual Display Interface` on driver `evdi-lindroid` (version 1.0.0 at NEVER)

Hal ini membuktikan:

- DRM driver berhasil diregistrasikan.
- libdrm dapat menemukan driver.
- DRM device dapat dibuka.
- Nama driver adalah "evdi-lindroid".
- Versi driver yang dilaporkan adalah "1.0.0".

Driver memiliki:

5 connectors
5 encoders
5 CRTCs
5 primary planes

Connector:

Virtual-3
Virtual-4
Virtual-5
Virtual-6
Virtual-7

Namun semuanya:

disconnected

Tidak ada mode.

Format buffer yang tersedia:

XRGB8888
ARGB8888

Fitur DRM yang tersedia termasuk:

Atomic
PRIME
Dumb buffers

---

11. "drm_info"

Perintah:

drm_info /dev/dri/card1

berhasil.

Informasi driver:

Driver: evdi-lindroid
Lindroid Virtual Display Interface
version 1.0.0 (NEVER)

Capabilities:

DRM_CLIENT_CAP_ATOMIC supported
DRM_CAP_DUMB_BUFFER = 1
DRM_CAP_PRIME = 3

Available nodes:

primary
render

Framebuffer size:

Width [640,8192]
Height [480,8192]

Semua connector:

Status: disconnected

CRTC:

CRTC_ID=0
ACTIVE=0
MODE_ID=0

Kesimpulannya:

«Kernel DRM registration bekerja, tetapi tidak ada output yang sedang connected/active.»

---

12. XDG Runtime Directory

Untuk mencoba menjalankan Wayland session sebagai root dibuat:

mkdir -p /run/user/0
chmod 700 /run/user/0
chown root:root /run/user/0

Environment:

export XDG_RUNTIME_DIR=/run/user/0
export XDG_SESSION_TYPE=wayland
export LANG=C.UTF-8
export LC_ALL=C.UTF-8

---

13. Plasma Wayland

Perintah:

dbus-run-session startplasma-wayland

menghasilkan error systemd user DBus:

Activating service 'org.freedesktop.systemd1' ... failed: Process org.freedesktop.systemd1 exited with status 1

Kemudian KWin:

No backend specified, automatically choosing drm
kwin_core: Could not determine the active graphical session

Lalu:

kwin_wayland_drm: failed to open drm device at "/dev/dri/card1"
kwin_wayland_drm: failed to open drm device at "/dev/dri/card2"
kwin_wayland_drm: failed to open drm device at "/dev/dri/card0"
kwin_wayland_drm: No suitable DRM devices have been found

Pesan tersebut berulang dan Plasma kemudian shutdown.

---

14. Qt Wayland

Setelah KWin gagal:

qt.qpa.wayland: Could not load the Qt platform plugin "wayland" in "" even though it was found.
qt.qpa.wayland: Loading shell integration failed.

Kemudian:

Could not find the Qt platform plugin "xcb"

Plugin yang tersedia:

offscreen
minimal
minimalegl
wayland-egl
wayland
vkkhrdisplay
linuxfb
eglfs

Interpretasi:

Error Qt tersebut kemungkinan merupakan efek lanjutan karena compositor/session Wayland gagal start.

Prioritas debugging bukan Qt terlebih dahulu, tetapi DRM/KMS output.

---

15. "evdi_open"

Dalam source custom EVDI:

/tmp/evdi-src

pernah ditemukan:

evdi_handle evdi = evdi_open(0);

Pencarian:

grep -RniE \
'evdi_open|Opened.*slave|slave drm|evdi-lindroid|Failed to open' \
library module

menghasilkan temuan pada binary:

library/libevdi.so.1

yang mengandung:

evdi_open

Ini menunjukkan userspace library EVDI custom memang memiliki fungsi/API:

evdi_open

Namun karena ini merupakan fork/custom implementation, API lainnya tidak boleh diasumsikan tanpa melihat source.

---

16. Diagnosis Sementara

Kernel driver:

evdi-lindroid

sudah berfungsi pada level dasar.

Bukti:

modetest -M evdi-lindroid

berhasil membuka:

Lindroid Virtual Display Interface

dan:

drm_info /dev/dri/card1

juga berhasil.

Jadi masalah bukan sekadar:

/dev/dri/card1 tidak ada

atau:

driver gagal register

Masalah yang lebih spesifik adalah:

connector belum connected

dan:

connector belum mempunyai mode

Padahal sebelumnya pernah ada:

card1-Virtual-3
connected
3048x1906

Dengan demikian, titik debugging utama adalah hubungan:

evdi_bridge
    ↓
EVDI userspace
    ↓
evdi-lindroid kernel driver
    ↓
connector state
    ↓
mode / EDID

---

17. Kemungkinan Penyebab

Kemungkinan utama:

"evdi_bridge" aktif tetapi belum berhasil melakukan mekanisme userspace yang diperlukan oleh custom EVDI fork untuk mengubah connector menjadi connected atau memberikan mode.

Kemungkinan lain:

- bridge tidak mendapatkan informasi display Android.
- Android/Anland belum menyediakan output.
- EVDI device belum di-connect.
- event EVDI belum diproses.
- mode belum diberikan.
- EDID belum diteruskan.
- hotplug event belum dipicu.
- bridge membuka DRM tetapi berhenti sebelum konfigurasi connector.
- ada dependency/library yang salah.
- API custom EVDI berbeda dengan upstream.
- "evdi_bridge" sebenarnya menunggu event dari Android.
- KWin dijalankan sebelum connector tersedia.

Karena sebelumnya connector "Virtual-3" pernah connected, sangat mungkin ada trigger tertentu yang belum terjadi pada kondisi sekarang.

---

18. Perintah Debugging yang Disarankan

Periksa service

systemctl cat evdi-bridge

---

Cari API EVDI di executable

strings /usr/bin/evdi_bridge | grep -Ei \
'evdi_open|evdi_connect|evdi_disconnect|evdi_enable|evdi_mode|connect|disconnect|mode|plug'

---

Periksa dependency

ldd /usr/bin/evdi_bridge

---

Cari API pada source

grep -RniE \
'evdi_open|evdi_connect|evdi_disconnect|evdi_enable|evdi_mode' \
/tmp/evdi-src 2>/dev/null

Jangan mengasumsikan semua fungsi tersebut ada. Hasil source harus dijadikan acuan.

---

Periksa kernel log

journalctl -k -b | grep -Ei \
'evdi|drm|connector|hotplug|edid|lindroid'

---

Periksa sysfs driver

readlink -f /sys/class/drm/card1/device

Kemudian:

find /sys/devices/evdi-lindroid/evdi-lindroid.0 -maxdepth 2 -type f -print

---

19. Trace "evdi_bridge"

Hentikan service:

systemctl stop evdi-bridge

Kemudian jalankan foreground:

strace -f -s 256 \
-o /tmp/evdi_bridge.strace \
/usr/bin/evdi_bridge

Setelah itu cari aktivitas yang berkaitan dengan DRM/EVDI:

grep -Ei \
'card1|renderD129|/dev/dri|ioctl|evdi|drm|connect|mode' \
/tmp/evdi_bridge.strace | tail -200

Tujuannya adalah melihat apakah bridge:

- membuka "/dev/dri/card1"
- membuka "/dev/dri/renderD129"
- memanggil ioctl DRM
- memanggil EVDI API
- mendapatkan event
- mencoba mengubah mode
- mencoba connect/disconnect
- gagal membuka library/device

---

20. Tangkap Kernel Log Saat Bridge Start

Sebelum bridge:

dmesg -C

Kemudian:

systemctl start evdi-bridge
sleep 2
dmesg

Hal yang dicari:

evdi
drm
connector
hotplug
edid
lindroid

---

21. Cek Semua Connector

Setelah bridge aktif:

for d in /sys/class/drm/card1-*; do
    echo "=== $d ==="
    cat "$d/status" 2>/dev/null
    cat "$d/modes" 2>/dev/null
done

Target yang diharapkan:

=== /sys/class/drm/card1-Virtual-3 ===
connected
3048x1906

---

22. Target Debugging Tahap Pertama

Jangan langsung mengejar Plasma.

Target pertama adalah:

card1-Virtual-3

menjadi:

connected

dan:

modes

menjadi:

3048x1906

Setelah itu baru diuji:

modetest -M evdi-lindroid

dan kemudian:

KWin / Plasma Wayland

---

23. Hubungan Dengan KWin

Pesan:

kwin_wayland_drm: failed to open drm device at "/dev/dri/card1"

perlu ditafsirkan hati-hati.

Karena:

modetest -M evdi-lindroid

sudah berhasil membuka driver, pesan KWin tersebut tidak otomatis berarti:

/dev/dri/card1

tidak ada atau kernel driver rusak.

KWin membutuhkan DRM device yang usable sebagai display backend.

Saat ini:

card1

memiliki connector tetapi:

disconnected

dan:

no mode

sehingga KWin tidak mempunyai output display yang dapat digunakan.

Karena itu error:

No suitable DRM devices have been found

merupakan indikator yang lebih penting daripada sekadar pesan "failed to open".

---

24. Prioritas Masalah

Urutan debugging yang disarankan:

1. evdi_bridge
       ↓
2. EVDI userspace API
       ↓
3. connector state
       ↓
4. EDID/mode
       ↓
5. DRM atomic modeset
       ↓
6. framebuffer / DMA-BUF
       ↓
7. KWin DRM backend
       ↓
8. Plasma Wayland

Jangan menghabiskan waktu pada Qt "xcb" atau shell integration sebelum DRM output sudah usable.

---

25. Catatan Tentang EVDI API

Karena menggunakan custom fork, jangan langsung menganggap API upstream EVDI tersedia.

Fungsi yang sudah terbukti ada:

evdi_open(0);

Fungsi seperti:

evdi_connect
evdi_disconnect
evdi_enable
evdi_mode

harus diverifikasi dari source/library.

---

26. Bukti Penting dari Debugging Sebelumnya

Ada beberapa fakta penting:

Driver dapat dibuka

modetest -M evdi-lindroid

berhasil.

Driver terdeteksi

evdi-lindroid

DRM node tersedia

/dev/dri/card1
/dev/dri/renderD129

Connector pernah connected

card1-Virtual-3 connected

Mode pernah tersedia

3048x1906

"evdi_bridge" pernah membuka card1

/dev/dri/card1: root 97 F.... evdi_bridge

Ini berarti pipeline kernel/DRM tidak sepenuhnya rusak.

Masalah lebih mungkin berada pada state/event/connection flow antara bridge dan driver.

---

27. Kondisi Terakhir

Kondisi saat ini dapat dirangkum:

Kernel:
    evdi-lindroid       OK
    /dev/dri/card1      OK
    renderD129          OK

DRM:
    modetest            OK
    drm_info            OK
    connectors          TERDAFTAR
    connector state     DISCONNECTED
    modes               KOSONG

evdi_bridge:
    binary              ADA
    service             BISA RUNNING
    connector trigger   BELUM TERBUKTI

KWin:
    DRM backend         GAGAL
    usable DRM output   TIDAK ADA

Plasma:
    startplasma-wayland GAGAL

Qt:
    Wayland plugin      ADA
    xcb plugin          TIDAK ADA
    tetapi ini bukan blocker pertama

---

28. Fokus Percakapan Selanjutnya

Langkah berikut yang paling berguna adalah mendapatkan output dari:

systemctl cat evdi-bridge

strings /usr/bin/evdi_bridge | grep -Ei \
'evdi_open|evdi_connect|evdi_disconnect|evdi_enable|evdi_mode|connect|disconnect|mode|plug'

ldd /usr/bin/evdi_bridge

grep -RniE \
'evdi_open|evdi_connect|evdi_disconnect|evdi_enable|evdi_mode' \
/tmp/evdi-src 2>/dev/null

dan:

journalctl -k -b | grep -Ei \
'evdi|drm|connector|hotplug|edid|lindroid'

Jika diperlukan, gunakan:

strace -f -s 256 \
-o /tmp/evdi_bridge.strace \
/usr/bin/evdi_bridge

kemudian:

grep -Ei \
'card1|renderD129|/dev/dri|ioctl|evdi|drm|connect|mode' \
/tmp/evdi_bridge.strace | tail -200

Output tersebut akan menentukan apakah masalah berada di:

- "evdi_bridge"
- EVDI userspace API
- Android/Anland display input
- DRM connector state
- mode/EDID
- atau event/hotplug.

---

29. Kesimpulan

Status proyek saat ini sudah melewati tahap paling dasar.

Custom DRM driver:

evdi-lindroid

sudah terdaftar dan dapat dibuka oleh libdrm.

Yang belum berhasil secara konsisten adalah membuat:

card1-Virtual-3

berubah dari:

disconnected

menjadi:

connected

dengan:

3048x1906

Setelah state tersebut kembali, KWin dapat diuji kembali.

Jadi blocker utama saat ini adalah:

EVDI / evdi_bridge → connector connection + mode

bukan instalasi Plasma atau keberadaan "/dev/dri/card1".
