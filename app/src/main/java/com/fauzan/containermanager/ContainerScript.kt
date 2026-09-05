package com.fauzan.containermanager

import android.content.Context
import android.os.Build
import java.io.File
import java.io.FileOutputStream

object ContainerScript {
    
    // The boot binary path after it has been deployed
    var bootBinaryPath: String = ""

    fun deployBootBinary(context: Context) {
        val abi = Build.SUPPORTED_ABIS[0]
        val entryName = "lib/${abi}/libcontainer_boot.so"
        val outDir = File(context.filesDir, "bin")
        if (!outDir.exists()) outDir.mkdirs()
        
        val outFile = File(outDir, "container_boot")
        try {
            val apkFile = java.util.zip.ZipFile(context.applicationInfo.sourceDir)
            val entry = apkFile.getEntry(entryName)
            if (entry != null) {
                apkFile.getInputStream(entry).use { input ->
                    FileOutputStream(outFile).use { output ->
                        input.copyTo(output)
                    }
                }
                outFile.setExecutable(true)
                bootBinaryPath = outFile.absolutePath
            } else {
                System.err.println("Could not find boot binary for ABI $abi in APK")
            }
            apkFile.close()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun getStartScript(rootfsPath: String): String {
        return """
            #!/system/bin/sh
            
            MNT="$rootfsPath"
            
            if [ -z "$bootBinaryPath" ] || [ ! -f "$bootBinaryPath" ]; then
                echo "[ERROR] Boot binary not deployed properly."
                exit 1
            fi
            
            echo "[*] Preparing Container Environment via native boot.c..."
            
            # Execute the native standalone boot binary
            $bootBinaryPath "${'$'}MNT"
            
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
            
            # Clean up PID file
            rm -f ${'$'}MNT/container.pid
            
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
