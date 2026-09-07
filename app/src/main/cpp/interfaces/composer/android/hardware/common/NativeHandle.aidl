package android.hardware.common;
import android.os.ParcelFileDescriptor;


parcelable NativeHandle {
    ParcelFileDescriptor[] fds;
    int[] ints;
}
