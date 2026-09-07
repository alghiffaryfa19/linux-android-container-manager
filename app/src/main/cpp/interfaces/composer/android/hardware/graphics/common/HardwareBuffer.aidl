package android.hardware.graphics.common;
import android.hardware.graphics.common.HardwareBufferDescription;
import android.hardware.common.NativeHandle;

@VintfStability
parcelable HardwareBuffer {
    HardwareBufferDescription description;
    NativeHandle handle;
}
