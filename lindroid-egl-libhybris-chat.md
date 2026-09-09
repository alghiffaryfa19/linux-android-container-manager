# Chat Export — Lindroid / EGL / libhybris / KWin Wayland

> Export dibuat dari percakapan yang tersedia pada konteks sesi ini. Beberapa pesan sebelumnya ditandai sebagai “Skipped messages”, sehingga isi yang tidak tersedia tidak dapat dimasukkan secara verbatim.

## 2026-09-09 — Pemeriksaan library EGL/GL/GLES/GBM

### User

```text
root@lindroid:/# ls -l /lib/aarch64-linux-gnu/libEGL.so*
ls -l /usr/lib/aarch64-linux-gnu/libEGL.so*
ls -l /usr/lib/aarch64-linux-gnu/libGL.so*
lrwxrwxrwx. 1 root root     11 Nov  1  2024 /lib/aarch64-linux-gnu/libEGL.so -> libEGL.so.1
lrwxrwxrwx. 1 root root     15 Nov  1  2024 /lib/aarch64-linux-gnu/libEGL.so.1 -> libEGL.so.1.1.0
-rw-r--r--. 1 root root 133664 Nov  1  2024 /lib/aarch64-linux-gnu/libEGL.so.1.1.0
lrwxrwxrwx. 1 root root     11 Nov  1  2024 /usr/lib/aarch64-linux-gnu/libEGL.so -> libEGL.so.1
lrwxrwxrwx. 1 root root     15 Nov  1  2024 /usr/lib/aarch64-linux-gnu/libEGL.so.1 -> libEGL.so.1.1.0
-rw-r--r--. 1 root root 133664 Nov  1  2024 /usr/lib/aarch64-linux-gnu/libEGL.so.1.1.0
lrwxrwxrwx. 1 root root     10 Nov  1  2024 /usr/lib/aarch64-linux-gnu/libGL.so -> libGL.so.1
lrwxrwxrwx. 1 root root     14 Nov  1  2024 /usr/lib/aarch64-linux-gnu/libGL.so.1 -> libGL.so.1.7.0
-rw-r--r--. 1 root root 985280 Nov  1  2024 /usr/lib/aarch64-linux-gnu/libGL.so.1.7.0
```

```text
root@lindroid:/# ldconfig -p | grep -Ei 'libEGL|libGLES|libGL|gbm'
        libgbm.so.1 => /lib/aarch64-linux-gnu/libgbm.so.1
        libGLdispatch.so.0 => /lib/aarch64-linux-gnu/libGLdispatch.so.0
        libGLX_mesa.so.0 => /lib/aarch64-linux-gnu/libGLX_mesa.so.0
        libGLX.so.0 => /lib/aarch64-linux-gnu/libGLX.so.0
        libGLU.so.1 => /lib/aarch64-linux-gnu/libGLU.so.1
        libGLESv2_libhybris.so.2 => /lib/aarch64-linux-gnu/libGLESv2_libhybris.so.2
        libGLESv2.so.2 => /lib/aarch64-linux-gnu/libGLESv2.so.2
        libGLESv2.so => /lib/aarch64-linux-gnu/libGLESv2.so
        libGLESv1_CM_libhybris.so.1 => /lib/aarch64-linux-gnu/libGLESv1_CM_libhybris.so.1
        libGLESv1_CM.so.1 => /lib/aarch64-linux-gnu/libGLESv1_CM.so.1
        libGLESv1_CM.so => /lib/aarch64-linux-gnu/libGLESv1_CM.so
        libGL.so.1 => /lib/aarch64-linux-gnu/libGL.so.1
        libGL.so => /lib/aarch64-linux-gnu/libGL.so
        libEGL_mesa.so.0 => /lib/aarch64-linux-gnu/libEGL_mesa.so.0
        libEGL_libhybris.so.0 => /lib/aarch64-linux-gnu/libEGL_libhybris.so.0
        libEGL.so.1 => /lib/aarch64-linux-gnu/libEGL.so.1
        libEGL.so => /lib/aarch64-linux-gnu/libEGL.so
```

```text
root@lindroid:/# find /usr /lib -type f \( \
    -name 'libEGL.so*' -o \
    -name 'libGLES*.so*' -o \
    -name 'libGL.so*' -o \
    -name 'libgbm.so*' \
  \) 2>/dev/null
/usr/lib/aarch64-linux-gnu/libGLESv1_CM_libhybris.so.1.0.1
/usr/lib/aarch64-linux-gnu/libGL.so.1.7.0
/usr/lib/aarch64-linux-gnu/libGLESv2_libhybris.so.2.0.0
/usr/lib/aarch64-linux-gnu/libEGL.so.1.1.0
/usr/lib/aarch64-linux-gnu/libGLESv2.so.2.1.0
/usr/lib/aarch64-linux-gnu/libgbm.so.1.0.0
/usr/lib/aarch64-linux-gnu/libGLESv1_CM.so.1.2.0
```

```text
root@lindroid:/# readlink -f /usr/lib/aarch64-linux-gnu/libEGL.so.1
/usr/lib/aarch64-linux-gnu/libEGL.so.1.1.0

root@lindroid:/# readlink -f /usr/lib/aarch64-linux-gnu/libGLESv2.so.2
/usr/lib/aarch64-linux-gnu/libGLESv2.so.2.1.0
```

```text
root@lindroid:/# ldd /usr/bin/kwin_wayland | grep -Ei 'egl|gles|gl|gbm|hybris'
        libgbm.so.1 => /lib/aarch64-linux-gnu/libgbm.so.1
        libQt6OpenGL.so.6 => /lib/aarch64-linux-gnu/libQt6OpenGL.so.6
        libGLESv2.so.2 => /lib/aarch64-linux-gnu/libGLESv2.so.2
        libEGL.so.1 => /lib/aarch64-linux-gnu/libEGL.so.1
        libOpenGL.so.0 => /lib/aarch64-linux-gnu/libOpenGL.so.0
        libGLdispatch.so.0 => /lib/aarch64-linux-gnu/libGLdispatch.so.0
```

```text
root@lindroid:/# find /usr/share/glvnd /etc/glvnd /usr/lib/aarch64-linux-gnu \
  -type f 2>/dev/null | grep -Ei 'egl.*json|egl_vendor'
/usr/share/glvnd/egl_vendor.d/50_mesa.json
/usr/share/glvnd/egl_vendor.d/10_libhybris.json
```

```text
root@lindroid:/# echo "$KWIN_COMPOSE"

root@lindroid:/# systemctl --user show-environment | grep -E 'KWIN|EGL|GBM|LIBGL'
Failed to connect to user scope bus via local transport: No such file or directory

root@lindroid:/# systemctl --user cat plasma-kwin_wayland.service
Failed to connect to user scope bus via local transport: No such file or directory
```

## 2026-09-09 — Pemeriksaan EGL vendor dan dependency

### User

```text
root@lindroid:/# cat /usr/share/glvnd/egl_vendor.d/10_libhybris.json
{
    "file_format_version" : "1.0.0",
    "ICD" : {
        "library_path" : "libEGL_libhybris.so.0"
    }
}

root@lindroid:/# cat /usr/share/glvnd/egl_vendor.d/50_mesa.json
{
    "file_format_version" : "1.0.0",
    "ICD" : {
        "library_path" : "libEGL_mesa.so.0"
    }
}
```

```text
root@lindroid:/# ls -l /lib/aarch64-linux-gnu/libc.so*
-rw-r--r--. 1 root root     291 Apr 27 20:09 /lib/aarch64-linux-gnu/libc.so
-rwxr-xr-x. 1 root root 1716616 Apr 27 20:09 /lib/aarch64-linux-gnu/libc.so.6

root@lindroid:/# ldconfig -p | grep -E '^libc\.so'
```

### ldd libEGL_libhybris

```text
linux-vdso.so.1
libhybris-common.so.1 => /lib/aarch64-linux-gnu/libhybris-common.so.1
libstdc++.so.6 => /lib/aarch64-linux-gnu/libstdc++.so.6
libc.so.6 => /lib/aarch64-linux-gnu/libc.so.6
/lib/ld-linux-aarch64.so.1
libgcc_s.so.1 => /lib/aarch64-linux-gnu/libgcc_s.so.1
libm.so.6 => /lib/aarch64-linux-gnu/libm.so.6
```

### ldd libGLESv2_libhybris

```text
linux-vdso.so.1
libhybris-common.so.1 => /lib/aarch64-linux-gnu/libhybris-common.so.1
libc.so.6 => /lib/aarch64-linux-gnu/libc.so.6
libstdc++.so.6 => /lib/aarch64-linux-gnu/libstdc++.so.6
/lib/ld-linux-aarch64.so.1
libgcc_s.so.1 => /lib/aarch64-linux-gnu/libgcc_s.so.1
libm.so.6 => /lib/aarch64-linux-gnu/libm.so.6
```

### ldd libEGL_mesa

```text
linux-vdso.so.1
libgallium-25.0.7-2+deb13u1.so => /lib/aarch64-linux-gnu/libgallium-25.0.7-2+deb13u1.so
libgbm.so.1 => /lib/aarch64-linux-gnu/libgbm.so.1
libexpat.so.1 => /lib/aarch64-linux-gnu/libexpat.so.1
libX11-xcb.so.1 => /lib/aarch64-linux-gnu/libX11-xcb.so.1
libxcb.so.1 => /lib/aarch64-linux-gnu/libxcb.so.1
libxcb-randr.so.0 => /lib/aarch64-linux-gnu/libxcb-randr.so.0
libxcb-xfixes.so.0 => /lib/aarch64-linux-gnu/libxcb-xfixes.so.0
libxcb-shm.so.0 => /lib/aarch64-linux-gnu/libxcb-shm.so.0
libdrm.so.2 => /lib/aarch64-linux-gnu/libdrm.so.2
libwayland-client.so.0 => /lib/aarch64-linux-gnu/libwayland-client.so.0
libwayland-server.so.0 => /lib/aarch64-linux-gnu/libwayland-server.so.0
libxcb-dri3.so.0 => /lib/aarch64-linux-gnu/libxcb-dri3.so.0
libxcb-present.so.0 => /lib/aarch64-linux-gnu/libxcb-present.so.0
libm.so.6
libc.so.6
/lib/ld-linux-aarch64.so.1
libLLVM.so.19.1
libz.so.1
libzstd.so.1
libsensors.so.5
libxcb-sync.so.1
libxshmfence.so.1
libelf.so.1
libdrm_amdgpu.so.1
libstdc++.so.6
libgcc_s.so.1
libXau.so.6
libXdmcp.so.6
libffi.so.8
libedit.so.2
libz3.so.4
libxml2.so.2
libtinfo.so.6
libbsd.so.0
liblzma.so.5
libmd.so.0
```

## 2026-09-09 — Pemeriksaan symbol EGL

### User

```text
root@lindroid:/# nm -D /lib/aarch64-linux-gnu/libEGL_libhybris.so.0 2>/dev/null | grep -E 'eglInitialize|eglGetPlatformDisplay|eglQueryString|eglGetDisplay'
```

Tidak ada output.

```text
root@lindroid:/# nm -D /lib/aarch64-linux-gnu/libEGL_mesa.so.0 2>/dev/null | grep -E 'eglInitialize|eglGetPlatformDisplay|eglQueryString|eglGetDisplay'
```

Tidak ada output.

```text
root@lindroid:/# command -v nm
/usr/bin/nm
```

### ldconfig hybris / hardware / gralloc

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
libGLESv2_libhybris.so.2
libGLESv1_CM_libhybris.so.1
libEGL_libhybris.so.0
```

### File hybris

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

### readelf libEGL.so.1

```text
0x0000000000000001 (NEEDED) Shared library: [libGLdispatch.so.0]
0x0000000000000001 (NEEDED) Shared library: [libc.so.6]
0x0000000000000001 (NEEDED) Shared library: [ld-linux-aarch64.so.1]
0x000000000000000e (SONAME) Library soname: [libEGL.so.1]
```

### strings GLVND

```text
__glDispatchNewVendorID
apiState->currentVendor == vendor
Out of memory allocating device/vendor map
1.5 libglvnd
../src/EGL/libeglvendor.c
/etc/glvnd/egl_vendor.d:/usr/share/glvnd/egl_vendor.d
__EGL_VENDOR_LIBRARY_FILENAMES
__EGL_VENDOR_LIBRARY_DIRS
ERROR: Could not allocate vendor library path name
eglDebugMessageControlKHR failed in vendor library with error 0x%04x. Error reporting may not work correctly.
eglLabelObjectKHR failed in vendor library with error 0x%04x. Thread label may not be reported correctly.
__glvndWinsysDispatchFindIndex(name) < 0
InternalMakeCurrentVendor
LoadVendor
glvndSetupPthreads
```

### Percobaan LD_DEBUG

```text
root@lindroid:/# LD_DEBUG=libs \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
true 2>&1 | head
```

Tidak ada output.

## 2026-09-09 — Program egltest

### User membuat program

```c
#include <stdio.h>
#include <EGL/egl.h>

int main(void)
{
    printf("EGL test\n");

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (dpy == EGL_NO_DISPLAY) {
        printf("eglGetDisplay: FAILED\n");
        printf("EGL error: 0x%04x\n", eglGetError());
        return 1;
    }

    EGLint major = 0, minor = 0;

    if (!eglInitialize(dpy, &major, &minor)) {
        printf("eglInitialize: FAILED\n");
        printf("EGL error: 0x%04x\n", eglGetError());
        return 2;
    }

    printf("EGL initialized: %d.%d\n", major, minor);

    const char *vendor = eglQueryString(dpy, EGL_VENDOR);
    const char *version = eglQueryString(dpy, EGL_VERSION);
    const char *extensions = eglQueryString(dpy, EGL_EXTENSIONS);

    printf("EGL_VENDOR: %s\n", vendor ? vendor : "(null)");
}
```

### Hasil compile

Percobaan awal gagal karena `EGL_VERSION_STRING` tidak tersedia:

```text
/tmp/egltest.c:6:39: error: ‘EGL_VERSION_STRING’ undeclared
```

Kemudian program diperbaiki dan berhasil dikompilasi:

```text
root@lindroid:/# gcc /tmp/egltest.c -o /tmp/egltest -lEGL
root@lindroid:/# ls -l /tmp/egltest
-rwxr-xr-x. 1 root root 70712 Sep  8 15:03 /tmp/egltest
```

### Hasil menjalankan egltest

Tanpa override vendor:

```text
root@lindroid:/# /tmp/egltest
EGL test
library "libui_compat_layer.so" not found
Segmentation fault
```

Dengan Mesa:

```text
root@lindroid:/# __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
/tmp/egltest
EGL test
eglInitialize: FAILED
EGL error: 0x3001
```

Dengan libhybris:

```text
root@lindroid:/# __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_libhybris.json \
/tmp/egltest
EGL test
library "libui_compat_layer.so" not found
Segmentation fault
```

## Kesimpulan teknis sementara

1. `libEGL.so.1` yang digunakan sistem adalah GLVND:
   - SONAME: `libEGL.so.1`
   - bergantung pada `libGLdispatch.so.0`.
2. Terdapat dua EGL vendor:
   - Mesa: `50_mesa.json`
   - libhybris: `10_libhybris.json`
3. Jalur libhybris berhasil ditemukan oleh dynamic linker, tetapi saat EGL vendor libhybris dipakai muncul:
   - `library "libui_compat_layer.so" not found`
   - kemudian segmentation fault.
4. Saat vendor Mesa dipaksa, `eglInitialize()` gagal dengan EGL error `0x3001` (`EGL_NOT_INITIALIZED`).
5. Ini menunjukkan masalah utama bukan sekadar symlink `libEGL.so`, tetapi jalur inisialisasi EGL vendor, khususnya dependency/compatibility layer libhybris.
6. `kwin_wayland` memang terhubung ke GLVND (`libEGL.so.1`, `libGLdispatch.so.0`) serta `libGLESv2.so.2` dan `libgbm.so.1`.
7. Langkah diagnosis berikutnya yang paling relevan adalah mencari dan memastikan keberadaan/SONAME/dependency `libui_compat_layer.so`, kemudian memeriksa bagaimana `libEGL_libhybris.so.0` memuat library tersebut dan apakah library itu sendiri memiliki dependency yang hilang atau ABI mismatch.

## Catatan

Beberapa bagian percakapan sebelum data di atas tidak tersedia di konteks saat export karena sistem menandainya sebagai pesan yang di-skip. Export ini mempertahankan data teknis yang tersedia pada konteks aktif.
