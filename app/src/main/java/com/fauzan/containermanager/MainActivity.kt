package com.fauzan.containermanager

import android.net.Uri
import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedReader
import java.io.File
import java.io.FileOutputStream
import java.io.InputStreamReader

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    ContainerManagerApp()
                }
            }
        }
    }
}

@Composable
fun ContainerManagerApp() {
    var outputLog by remember { mutableStateOf("Ready to start...") }
    var isExtracting by remember { mutableStateOf(false) }
    var isContainerRunning by remember { mutableStateOf(false) }
    val coroutineScope = rememberCoroutineScope()
    val context = LocalContext.current

    val tarballPickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            coroutineScope.launch {
                isExtracting = true
                outputLog = "Starting extraction process...\n"
                val res = extractTarball(context, uri)
                outputLog += res
                isExtracting = false
            }
        }
    }

    if (isContainerRunning) {
        ContainerDisplayScreen(
            onStopContainer = {
                coroutineScope.launch {
                    outputLog = "Stopping container...\n"
                    val res = executeSuCommand(ContainerScript.getStopScript())
                    outputLog += res
                    isContainerRunning = false
                }
            }
        )
    } else {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "Linux Container Manager",
                fontSize = 24.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(bottom = 32.dp)
            )
            
            Button(
                onClick = { tarballPickerLauncher.launch("application/gzip") },
                enabled = !isExtracting,
                modifier = Modifier.fillMaxWidth().padding(bottom = 16.dp)
            ) {
                Text(if (isExtracting) "Extracting... Please wait." else "Select RootFS Tarball (.tar.gz)")
            }

            if (isExtracting) {
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth().padding(bottom = 16.dp))
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceEvenly
            ) {
                Button(onClick = {
                    coroutineScope.launch {
                        outputLog = "Starting container...\n"
                        val res = executeSuCommand(ContainerScript.getStartScript())
                        outputLog += res
                        if (!res.contains("ERROR")) {
                            isContainerRunning = true
                        }
                    }
                }, enabled = !isExtracting) {
                    Text("Start Container")
                }

                Button(
                    onClick = {
                        coroutineScope.launch {
                            outputLog = "Stopping container...\n"
                            val res = executeSuCommand(ContainerScript.getStopScript())
                            outputLog += res
                        }
                    },
                    enabled = !isExtracting,
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
                ) {
                    Text("Stop Container")
                }
            }

            Spacer(modifier = Modifier.height(32.dp))

            Text(text = "Logs:", fontWeight = FontWeight.Bold, modifier = Modifier.align(Alignment.Start))
            
            Surface(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(top = 8.dp),
                color = MaterialTheme.colorScheme.surfaceVariant,
                shape = MaterialTheme.shapes.medium
            ) {
                Text(
                    text = outputLog,
                    modifier = Modifier.padding(16.dp),
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                    fontSize = 12.sp
                )
            }
        }
    }
}

@Composable
fun ContainerDisplayScreen(onStopContainer: () -> Unit) {
    BackHandler(onBack = {
        DisplayManager.stopDisplay()
        onStopContainer()
    })

    Box(modifier = Modifier.fillMaxSize()) {
        AndroidView(
            factory = { context ->
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            DisplayManager.startDisplay(holder.surface)
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int
                        ) {
                            // Handle surface size changes if necessary
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            DisplayManager.stopDisplay()
                        }
                    })
                }
            },
            modifier = Modifier.fillMaxSize()
        )
    }
}

suspend fun extractTarball(context: android.content.Context, uri: Uri): String = withContext(Dispatchers.IO) {
    try {
        val cacheFile = File(context.cacheDir, "rootfs_temp.tar.gz")
        context.contentResolver.openInputStream(uri)?.use { input ->
            FileOutputStream(cacheFile).use { output ->
                input.copyTo(output)
            }
        }

        val destDir = ContainerScript.ROOTFS_DIR
        
        val cmd = """
            echo "Creating target directory: ${'$'}destDir"
            mkdir -p ${'$'}destDir
            echo "Extracting tarball... This may take a while."
            tar -xzpf ${cacheFile.absolutePath} -C ${'$'}destDir
            echo "Extraction complete!"
            rm -f ${cacheFile.absolutePath}
        """.trimIndent()
        
        executeSuCommand(cmd)
    } catch (e: Exception) {
        "Exception during extraction: ${e.message}\n"
    }
}

suspend fun executeSuCommand(command: String): String = withContext(Dispatchers.IO) {
    try {
        val process = Runtime.getRuntime().exec(arrayOf("su", "-c", command))
        val reader = BufferedReader(InputStreamReader(process.inputStream))
        val errorReader = BufferedReader(InputStreamReader(process.errorStream))
        
        val output = StringBuilder()
        var line: String?
        while (reader.readLine().also { line = it } != null) {
            output.append(line).append("\n")
        }
        while (errorReader.readLine().also { line = it } != null) {
            output.append("ERROR: ").append(line).append("\n")
        }
        
        process.waitFor()
        output.toString()
    } catch (e: Exception) {
        "Exception: ${e.message}\n"
    }
}
