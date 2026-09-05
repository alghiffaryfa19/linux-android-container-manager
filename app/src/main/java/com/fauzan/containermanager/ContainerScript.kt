package com.fauzan.containermanager

object ContainerScript {
    
    const val ROOTFS_DIR = "/data/local/tmp/rootfs"

    fun getStartScript(): String {
        return """
            #!/system/bin/sh
            
            MNT="$ROOTFS_DIR"
            
            echo "[*] Preparing Container Environment..."
            
            if [ ! -d "${'$'}MNT" ]; then
                echo "[ERROR] RootFS directory not found at ${'$'}MNT"
                exit 1
            fi
            
            # Mount system dirs (bind mount directly to the directory)
            echo "[*] Mounting system directories (/dev, /proc, /sys, /tmp)"
            mount -o bind /dev ${'$'}MNT/dev
            mount -t proc proc ${'$'}MNT/proc
            mount -t sysfs sysfs ${'$'}MNT/sys
            mount -t tmpfs tmpfs ${'$'}MNT/tmp
            
            echo "[*] Container Started Successfully!"
            
            # Execute chroot
            # chroot ${'$'}MNT /bin/bash -c "su - root -c '/start-anland.sh &'"
        """.trimIndent()
    }

    fun getStopScript(): String {
        return """
            #!/system/bin/sh
            
            MNT="$ROOTFS_DIR"
            
            echo "[*] Stopping Container..."
            
            # Unmount system dirs (lazy unmount)
            echo "[*] Unmounting system directories"
            umount -l ${'$'}MNT/tmp
            umount -l ${'$'}MNT/sys
            umount -l ${'$'}MNT/proc
            umount -l ${'$'}MNT/dev
            
            echo "[*] Container Stopped!"
        """.trimIndent()
    }
}
