# Graphics compatibility probe

This probe is intentionally display-less. Run it inside the Android 16 container, not on the host:

```sh
cd /path/to/graphics-probe
EGL_PLATFORM=null HYBRIS_EGLPLATFORM=null sh ./build-and-run.sh
```

It checks, in order:

- vendor EGL initialization;
- Android gralloc allocation for RGBA8888;
- native-handle FD/integer counts and serialization;
- `EGLImageKHR` creation and GLES rendering;
- native-buffer lock and pixel readback.

A useful pass ends with `HYBRIS_PROBE PASS`. The output line `HANDLE fds=... ints=...` is important: do not reduce a multi-FD Android allocation to one FD if a later path reconstructs the native buffer.

This probe does not open EVDI, acquire the HWC client, stop SurfaceFlinger, or present a frame. It isolates libhybris/gralloc/EGL from the compositor and bridge.
