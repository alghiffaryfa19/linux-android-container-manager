#define _GNU_SOURCE

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EGL_NATIVE_BUFFER_HYBRIS
#define EGL_NATIVE_BUFFER_HYBRIS 0x3140
#endif

#define HYBRIS_USAGE_SW_READ_RARELY 0x00000002
#define HYBRIS_USAGE_HW_TEXTURE     0x00000100
#define HYBRIS_USAGE_HW_RENDER      0x00000200
#define HYBRIS_USAGE_HW_COMPOSER    0x00000800
#define HYBRIS_FORMAT_RGBA_8888     1

typedef EGLBoolean (*create_buffer_fn)(EGLint, EGLint, EGLint, EGLint,
                                       EGLint *, EGLClientBuffer *);
typedef EGLBoolean (*release_buffer_fn)(EGLClientBuffer);
typedef EGLBoolean (*lock_buffer_fn)(EGLClientBuffer, EGLint, EGLint, EGLint,
                                     EGLint, EGLint, void **);
typedef EGLBoolean (*unlock_buffer_fn)(EGLClientBuffer);
typedef void (*get_info_fn)(EGLClientBuffer, int *, int *);
typedef void (*serialize_fn)(EGLClientBuffer, int *, int *);
typedef void (*image_texture_fn)(GLenum, EGLImageKHR);

typedef struct {
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    PFNEGLCREATEIMAGEKHRPROC create_image;
    PFNEGLDESTROYIMAGEKHRPROC destroy_image;
    image_texture_fn image_texture;
    create_buffer_fn create_buffer;
    release_buffer_fn release_buffer;
    lock_buffer_fn lock_buffer;
    unlock_buffer_fn unlock_buffer;
    get_info_fn get_info;
    serialize_fn serialize;
} probe_state;

static void *required_proc(const char *name)
{
    void *proc = (void *)eglGetProcAddress(name);
    if (!proc)
        fprintf(stderr, "MISSING %s\n", name);
    return proc;
}

static int fail(const char *step)
{
    fprintf(stderr, "FAIL %s: EGL error 0x%04x\n", step, eglGetError());
    return 1;
}

int main(void)
{
    probe_state state = {0};
    EGLConfig config;
    EGLint count = 0;
    EGLint stride = 0;
    EGLClientBuffer buffer = NULL;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    int result = 1;
    const EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint pbuffer_attrs[] = {
        EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE
    };
    const EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
    };
    const EGLint usage = HYBRIS_USAGE_SW_READ_RARELY |
                         HYBRIS_USAGE_HW_TEXTURE |
                         HYBRIS_USAGE_HW_RENDER |
                         HYBRIS_USAGE_HW_COMPOSER;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("HYBRIS_PROBE platform=%s\n", getenv("EGL_PLATFORM") ?: "default");

    state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state.display == EGL_NO_DISPLAY ||
        !eglInitialize(state.display, NULL, NULL))
        return fail("eglInitialize");
    printf("EGL vendor=%s version=%s\n",
           eglQueryString(state.display, EGL_VENDOR),
           eglQueryString(state.display, EGL_VERSION));

    if (!eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(state.display, config_attrs, &config, 1, &count) ||
        count != 1)
        goto cleanup_fail;
    state.surface = eglCreatePbufferSurface(state.display, config, pbuffer_attrs);
    state.context = eglCreateContext(state.display, config, EGL_NO_CONTEXT,
                                      context_attrs);
    if (state.surface == EGL_NO_SURFACE || state.context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(state.display, state.surface, state.surface, state.context))
        goto cleanup_fail;

    state.create_image = (PFNEGLCREATEIMAGEKHRPROC)required_proc("eglCreateImageKHR");
    state.destroy_image = (PFNEGLDESTROYIMAGEKHRPROC)required_proc("eglDestroyImageKHR");
    state.image_texture = (image_texture_fn)
        required_proc("glEGLImageTargetTexture2DOES");
    state.create_buffer = (create_buffer_fn)required_proc("eglHybrisCreateNativeBuffer");
    state.release_buffer = (release_buffer_fn)required_proc("eglHybrisReleaseNativeBuffer");
    state.lock_buffer = (lock_buffer_fn)required_proc("eglHybrisLockNativeBuffer");
    state.unlock_buffer = (unlock_buffer_fn)required_proc("eglHybrisUnlockNativeBuffer");
    state.get_info = (get_info_fn)required_proc("eglHybrisGetNativeBufferInfo");
    state.serialize = (serialize_fn)required_proc("eglHybrisSerializeNativeBuffer");
    if (!state.create_image || !state.destroy_image || !state.image_texture ||
        !state.create_buffer || !state.release_buffer || !state.lock_buffer ||
        !state.unlock_buffer || !state.get_info || !state.serialize)
        goto cleanup_fail;

    if (!state.create_buffer(256, 256, usage, HYBRIS_FORMAT_RGBA_8888,
                             &stride, &buffer) || !buffer)
        goto cleanup_fail;
    printf("GRALLOC PASS stride=%d\n", stride);

    int num_ints = 0;
    int num_fds = 0;
    state.get_info(buffer, &num_ints, &num_fds);
    printf("HANDLE fds=%d ints=%d\n", num_fds, num_ints);
    if (num_fds <= 0 || num_fds > 32 || num_ints < 0 || num_ints > 1024)
        goto cleanup_fail;

    int *ints = calloc((size_t)num_ints, sizeof(*ints));
    int *fds = calloc((size_t)num_fds, sizeof(*fds));
    if ((!ints && num_ints) || !fds)
        goto cleanup_fail;
    state.serialize(buffer, ints, fds);
    printf("HANDLE SERIALIZE PASS\n");
    free(ints);
    free(fds);

    image = state.create_image(state.display, EGL_NO_CONTEXT,
                               EGL_NATIVE_BUFFER_HYBRIS, buffer, NULL);
    if (image == EGL_NO_IMAGE_KHR)
        goto cleanup_fail;
    glGenTextures(1, &(GLuint){0});
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    state.image_texture(GL_TEXTURE_2D, image);
    glClearColor(0.25f, 0.50f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    if (glGetError() != GL_NO_ERROR)
        goto cleanup_fail;
    printf("EGLIMAGE GLES PASS\n");

    void *pixels = NULL;
    if (!state.lock_buffer(buffer, HYBRIS_USAGE_SW_READ_RARELY, 0, 0,
                           256, 256, &pixels) || !pixels)
        goto cleanup_fail;
    printf("READBACK PASS rgba=%u,%u,%u,%u\n",
           ((unsigned char *)pixels)[0], ((unsigned char *)pixels)[1],
           ((unsigned char *)pixels)[2], ((unsigned char *)pixels)[3]);
    state.unlock_buffer(buffer);
    result = 0;

cleanup_fail:
    if (image != EGL_NO_IMAGE_KHR)
        state.destroy_image(state.display, image);
    if (buffer)
        state.release_buffer(buffer);
    if (state.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state.context != EGL_NO_CONTEXT)
            eglDestroyContext(state.display, state.context);
        if (state.surface != EGL_NO_SURFACE)
            eglDestroySurface(state.display, state.surface);
        eglTerminate(state.display);
    }
    if (result)
        fprintf(stderr, "HYBRIS_PROBE FAIL\n");
    else
        printf("HYBRIS_PROBE PASS\n");
    return result;
}
