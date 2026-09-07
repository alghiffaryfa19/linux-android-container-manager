package android.hardware.graphics.common;
import android.hardware.graphics.common.HardwareBufferDescription;
import android.hardware.common.NativeHandle;


parcelable HardwareBuffer {
    HardwareBufferDescription description;
    NativeHandle handle;
}
