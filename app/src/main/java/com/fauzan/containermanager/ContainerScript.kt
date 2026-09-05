package com.fauzan.containermanager

object ContainerScript {
    
    fun getStartScript(rootfsPath: String): String {
        return """
            #!/system/bin/sh
            
            MNT="$rootfsPath"
            
            echo "[*] Preparing Container Environment (Droidspaces style)..."
            
            if [ ! -d "${'$'}MNT" ]; then
                echo "[ERROR] RootFS directory not found at ${'$'}MNT"
                exit 1
            fi
            
            # Start the container inside a new isolated mount namespace
            # We run it in the background so the Android UI doesn't block forever
            # Redirect output to prevent the pipe from hanging executeSuCommand
            nohup unshare -m /system/bin/sh -c "
                echo \"[*] Entering isolated mount namespace...\"
                
                # Prevent our mounts from leaking back to the Android host
                mount --make-rprivate /
                
                # Bind-mount the rootfs to itself so it can be used for pivot_root
                mount -o bind,rec \"${'$'}MNT\" \"${'$'}MNT\"
                cd \"${'$'}MNT\"
                
                # Pre-create standard directories
                echo \"[*] Creating standard directories...\"
                mkdir -p .old_root proc sys dev tmp run
                
                # Mount virtual filesystems
                echo \"[*] Mounting virtual filesystems...\"
                mount -t proc proc proc
                mount -t sysfs sysfs sys
                mount -t tmpfs tmpfs tmp -o mode=1777
                mount -t tmpfs tmpfs run -o mode=755
                
                # Safely bind-mount host /dev to container's /dev
                mount -o bind /dev dev
                
                # Relocate the root filesystem
                echo \"[*] Pivoting root...\"
                pivot_root . .old_root || {
                    echo \"[*] pivot_root failed, falling back to MS_MOVE + chroot\"
                    mount --move . /
                    chroot .
                }
                
                cd /
                
                # Cleanup the old host root mount if pivot_root was successful
                if [ -d \"/.old_root/sys\" ]; then
                    umount -l /.old_root
                    rmdir /.old_root
                fi
                
                echo \"[*] Container Started Successfully!\"
                
                # Execute the native OS init system
                exec /sbin/init
            " > "${'$'}MNT/container.log" 2>&1 < /dev/null &
            
            # Wait a moment for initialization and then output the log so UI can show it
            sleep 3
            echo "[*] Container init log:"
            cat "${'$'}MNT/container.log"
        """.trimIndent()
    }

    fun getStopScript(rootfsPath: String): String {
        return """
            #!/system/bin/sh
            
            MNT="$rootfsPath"
            
            echo "[*] Stopping Container..."
            
            # Kill the container processes (targeting the init system if we started it)
            pkill -f '/sbin/init'
            
            # Unmount system dirs (lazy unmount) just in case they were mounted in the host namespace
            echo "[*] Cleaning up host mounts (if any)..."
            umount -l ${'$'}MNT/tmp 2>/dev/null
            umount -l ${'$'}MNT/sys 2>/dev/null
            umount -l ${'$'}MNT/proc 2>/dev/null
            umount -l ${'$'}MNT/dev 2>/dev/null
            
            echo "[*] Container Stopped!"
        """.trimIndent()
    }
}
