package android.hardware.common;
import android.os.ParcelFileDescriptor;

@VintfStability
parcelable NativeHandle {
    ParcelFileDescriptor[] fds;
    int[] ints;
}
