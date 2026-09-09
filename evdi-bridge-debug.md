# EVDI Bridge Debugging Session

## Ringkasan
Sesi debugging bridge EVDI ke Android display pada sistem Lindroid (Android container di Linux).
**Hardware:** Qualcomm SM8550, Kernel 5.15 GKI, aarch64

---

## Masalah Awal: `EVDI_CONNECT failed: Invalid argument`

### Log Awal
```
[evdi-bridge] EVDI device 0 opened successfully (fd=18)!
[evdi-bridge] EVDI_CONNECT failed: Invalid argument
[evdi-bridge] Connected virtual display 3048x1906@120Hz
```

### Root Cause
1. **Device salah** — `card0` adalah `msm_drm` (GPU fisik), bukan EVDI
2. **Width tidak aligned** — Android melaporkan `width=3048` tapi stride `13312` (= 3328×4)
3. **Refresh rate terlalu tinggi** — EVDI hanya support max 60Hz
4. **Sesi sebelumnya tidak di-disconnect** — EVDI masih anggap ada koneksi aktif

### Fix Yang Diterapkan

#### 1. Deteksi EVDI via sysfs (skip card0)
```c
static int evdi_open_direct(int index) {
    char uevent_path[128];
    snprintf(uevent_path, sizeof(uevent_path),
             "/sys/class/drm/card%d/device/uevent", index);
    FILE *f = fopen(uevent_path, "r");
    if (!f) return -1;

    char line[256];
    int is_evdi = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "DRIVER=evdi")) { is_evdi = 1; break; }
    }
    fclose(f);
    if (!is_evdi) return -1;

    char path[64];
    snprintf(path, sizeof(path), "/dev/dri/card%d", index);
    return open(path, O_RDWR);
}
```

#### 2. Disconnect stale session + aligned width + clamp 60Hz
```c
// Clamp refresh rate
if (disp_hz == 0 || disp_hz > 60) disp_hz = 60;

// Disconnect stale session
struct drm_evdi_connect dis = {0};
drm_ioctl(evdi_fd, DRM_IOCTL_EVDI_CONNECT, &dis);
usleep(50000);

// Gunakan aligned width dari stride
uint32_t aligned_w = (infos[0].stride > 0) ? (infos[0].stride / 4) : disp_w;

struct drm_evdi_connect cmd = {
    .connected    = 1,
    .dev_index    = evdi_idx,  // bukan hardcode 0
    .width        = aligned_w, // 3328, bukan 3048
    .height       = disp_h,
    .refresh_rate = disp_hz,   // 60, bukan 120
    .display_id   = 0
};
```

#### 3. Bail-out jika EVDI_CONNECT gagal
```c
if (drm_ioctl(evdi_fd, DRM_IOCTL_EVDI_CONNECT, &cmd) < 0) {
    perror("[evdi-bridge] EVDI_CONNECT failed");
    close(evdi_fd);
    g_evdi_fd = -1;
    goto cleanup;
}
```

#### 4. Hapus lseek pada DMA-BUF (bisa hang)
```c
// SEBELUM (bisa hang):
// off_t real_size = lseek(dma_fds[i], 0, SEEK_END);
// SESUDAH:
map_sizes[i] = (size_t)infos[i].stride * infos[i].height;
```

### Hasil
```
[evdi-bridge] EVDI device 1 opened successfully (fd=18)!
[evdi-bridge] Connected virtual display 3048x1906 (aligned 3328)@60Hz
[evdi-bridge] Bridge loop running. Waiting for EVDI events...
```
✅ **EVDI_CONNECT berhasil!**

---

## Masalah 2: kwin_wayland Crash

### Diagnosis
```
kwin_wayland_drm: "EGL_EXT_platform_base" client extension is not supported
kwin_scene_opengl: Creating the OpenGL rendering failed: "Could not initialize egl"
kwin_core: Could not fulfill the requested compositing mode in KWIN_COMPOSE: 1. Exiting.
```

### Root Cause
- `libhybris` tidak support `EGL_EXT_platform_base` yang dibutuhkan kwin DRM backend
- `GreeterEnvironment` di SDDM hanya berlaku untuk greeter, **bukan** user session
- kwin distart via `plasma-kwin_wayland.service` (systemd user service), tidak baca `profile.d`

### Fix Yang Dicoba

#### Set env via systemd user environment.d
```bash
mkdir -p /home/lindroid/.config/environment.d/
cat > /home/lindroid/.config/environment.d/kwin-evdi.conf << 'EOF'
KWIN_DRM_DEVICES=/dev/dri/card1:/dev/dri/card2:/dev/dri/card0
GBM_BACKEND=hybris
__GLX_VENDOR_LIBRARY_NAME=mesa
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
EGL_PLATFORM=drm
KWIN_COMPOSE=Q
EOF
```

#### Paksa software rendering
```
KWIN_COMPOSE=Q  # QPainter, bypass GPU
```

---

## Masalah 3: SDDM Stuck di "Logind interface found"

### Diagnosis
```
sddm: Logind interface found
# Lalu SIGTERM — tidak pernah lanjut ke "Adding new display"
```

### Root Cause
- **Tidak ada TTY/VT** di container (`/dev/tty` tidak ada)
- SDDM Wayland mode butuh VT allocation via logind
- Tidak ada session logind aktif (`loginctl list-sessions` → No sessions)

---

## Masalah 4: kwin Tidak Bisa Buka card1

### Diagnosis
```
kwin_wayland_drm: failed to open drm device at "/dev/dri/card1"
kwin_wayland_drm: No suitable DRM devices have been found
```

Padahal:
- `python3 open('/dev/dri/card1')` → OK
- `fuser /dev/dri/card1` → kosong (tidak ada yang pegang)
- Permissions: `crw-rw-rw-.` (world-readable)

### Root Cause
kwin enumerate DRM devices via **udev** dengan filter `ID_SEAT`. Card1 tidak punya `ID_SEAT=seat0` di udev properties, sehingga kwin skip device ini.

```bash
# Verifikasi — ID_SEAT tidak ada
udevadm info --query=property --name=/dev/dri/card1 | grep SEAT
# Output: ID_FOR_SEAT=drm-platform-evdi-lindroid_0
# (tidak ada ID_SEAT=seat0)
```

### Fix Yang Dicoba
```bash
cat > /etc/udev/rules.d/99-evdi-seat.rules << 'EOF'
SUBSYSTEM=="drm", KERNEL=="card*", ENV{ID_PATH}=="platform-evdi*", ENV{ID_SEAT}="seat0"
EOF
```
❌ Tidak berhasil — container tidak bisa reload udev rules (`Failed to send reload request`)

---

## Masalah 5: Ganti ke cage (wlroots-based)

### Instalasi
```bash
apt-get install -y cage
# Berhasil install: cage, libwlroots-0.18, libseat1, dll
```

### Test
```bash
XDG_RUNTIME_DIR=/tmp/kwin-runtime \
WLR_BACKENDS=drm \
WLR_DRM_DEVICES=/dev/dri/card1 \
WLR_RENDERER=pixman \
LIBSEAT_BACKEND=noop \
cage -- /bin/true 2>&1
```

**Output:**
```
00:00:00.006 [ERROR] [backend/drm/util.c:65] Failed to parse EDID
# Exit 0 — BERHASIL!
```

`LIBSEAT_BACKEND=noop` adalah kuncinya — bypass semua logind/VT requirement.

### Masalah Lanjutan
Cage crash SIGBUS saat menjalankan aplikasi nyata:
```
Bus error  (cage -- /usr/bin/bash -c "sleep 5")
```

Kemungkinan: wlroots pixman renderer butuh **DRM dumb buffers** yang tidak didukung EVDI driver ini.

---

## Investigasi /dev/tty

### Temuan Penting
```bash
zcat /proc/config.gz | grep CONFIG_VT
# CONFIG_VT=y  ← kernel sudah support VT!

ls /dev/tty
# ls: cannot access '/dev/tty': No such file or directory
```

**`CONFIG_VT=y` sudah aktif di kernel**, tapi TTY devices tidak di-mount ke container.

### Fix
```bash
# Buat TTY devices manual
mknod /dev/tty c 5 0
chmod 666 /dev/tty

mknod /dev/tty0 c 4 0
chmod 620 /dev/tty0

mknod /dev/console c 5 1
chmod 600 /dev/console
```

**Ini kemungkinan besar akan solve SIGBUS di cage** karena libseat tidak bisa alokasi VT tanpa `/dev/tty0`.

---

## Status Saat Ini

| Komponen | Status |
|----------|--------|
| evdi-bridge start | ✅ |
| Android app connect | ✅ |
| EVDI_CONNECT | ✅ (setelah fix aligned_w + dev_index) |
| DMA-BUF receive | ✅ |
| Connector card1-Virtual-3 | ✅ connected |
| kwin/compositor bisa buka card1 | ❌ |
| create_buf events di bridge | ❌ (belum ada compositor yang render) |
| Display tampil di Android | ❌ |

---

## Langkah Selanjutnya

1. **Buat `/dev/tty` dan `/dev/tty0`** → test cage lagi
2. Kalau cage berhasil tanpa SIGBUS → test dengan `startplasma-wayland`
3. Tambahkan `drmDropMaster(evdi_fd)` di bridge setelah EVDI_CONNECT
4. Pastikan `mknod` TTY devices dijalankan saat container start (tambah ke startup script)

---

## File Penting

- Bridge source: `overlay/usr/src/evdi_bridge/evdi_bridge.c`
- SDDM config: `/etc/sddm.conf`
- udev rules: `/etc/udev/rules.d/99-evdi-seat.rules`
- kwin env: `/home/lindroid/.config/environment.d/kwin-evdi.conf`
- Cage service: `/etc/systemd/system/kwin-evdi.service`

---

## Perintah Diagnostic Berguna

```bash
# Cek EVDI devices
for i in 0 1 2 3; do
  echo -n "card$i: "
  cat /sys/class/drm/card$i/device/uevent 2>/dev/null | grep DRIVER || echo "?"
done

# Cek connector status
cat /sys/class/drm/card1-Virtual-3/status

# Monitor bridge events
journalctl -fu evdi-bridge

# Monitor semua sekaligus
journalctl -f -u evdi-bridge -u sddm

# Test cage minimal
XDG_RUNTIME_DIR=/tmp/kwin-runtime \
WLR_BACKENDS=drm \
WLR_DRM_DEVICES=/dev/dri/card1 \
WLR_RENDERER=pixman \
LIBSEAT_BACKEND=noop \
WLR_NO_HARDWARE_CURSORS=1 \
cage -- /bin/true 2>&1
```
