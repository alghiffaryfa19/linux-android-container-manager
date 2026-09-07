package android.hardware.graphics.common;
import android.hardware.graphics.common.BufferUsage;
import android.hardware.graphics.common.PixelFormat;

@VintfStability
parcelable HardwareBufferDescription {
    int width;
    int height;
    int layers;
    PixelFormat format;
    BufferUsage usage;
    int stride;
}
