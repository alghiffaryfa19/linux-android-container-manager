package com.fauzan.containermanager

import android.content.Context
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
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
import java.util.UUID

data class ContainerInfo(val id: String, val name: String, val path: String)

private const val PREFS_NAME = "ContainerPrefs"
private const val KEY_CONTAINERS = "containers_list"

fun saveContainers(context: Context, containers: List<ContainerInfo>) {
    val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    val jsonString = containers.joinToString(";") { "${it.id},${it.name},${it.path}" }
    prefs.edit().putString(KEY_CONTAINERS, jsonString).apply()
}

fun loadContainers(context: Context): List<ContainerInfo> {
    val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    val jsonString = prefs.getString(KEY_CONTAINERS, "") ?: ""
    if (jsonString.isEmpty()) return emptyList()
    return jsonString.split(";").mapNotNull {
        val parts = it.split(",")
        if (parts.size == 3) ContainerInfo(parts[0], parts[1], parts[2]) else null
    }
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Extract native boot binary for container init
        ContainerScript.deployBinaries(this)
        
        // Extract Lindroid libc.so quirk
        val libFile = File(filesDir, "libc.so")
        if (!libFile.exists()) {
            try {
                assets.open("libc.so").use { input ->
                    FileOutputStream(libFile).use { output ->
                        input.copyTo(output)
                    }
                }
            } catch (e: java.io.IOException) {
                e.printStackTrace()
            }
        }
        
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ContainerManagerApp() {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    
    var containers by remember { mutableStateOf(loadContainers(context)) }
    var runningContainers by remember { mutableStateOf(setOf<String>()) }

    var outputLog by remember { mutableStateOf("Ready to start...") }
    var isExtracting by remember { mutableStateOf(false) }

    val tarballPickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            coroutineScope.launch {
                isExtracting = true
                outputLog = "Starting extraction process...\n"
                val containerName = "Container ${containers.size + 1}"
                val newContainer = extractTarball(context, uri, containerName)
                if (newContainer != null) {
                    val updatedList = containers + newContainer
                    containers = updatedList
                    saveContainers(context, updatedList)
                    outputLog = "Successfully added $containerName\n"
                } else {
                    outputLog = "Failed to extract container.\n"
                }
                isExtracting = false
            }
        }
    }

    Scaffold(
            topBar = {
                TopAppBar(title = { Text("Linux Container Manager") })
            },
            floatingActionButton = {
                FloatingActionButton(onClick = {
                    if (!isExtracting) {
                        tarballPickerLauncher.launch("application/gzip")
                    }
                }) {
                    Icon(Icons.Filled.Add, contentDescription = "Add Container")
                }
            }
        ) { padding ->
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(padding)
                    .padding(16.dp)
            ) {
                if (isExtracting) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth().padding(bottom = 16.dp))
                    Text("Extracting container, please wait...")
                    Spacer(modifier = Modifier.height(16.dp))
                }

                LazyColumn(modifier = Modifier.weight(1f)) {
                    items(containers) { container ->
                        val isRunning = runningContainers.contains(container.id)
                        Card(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 8.dp),
                            elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
                        ) {
                            Column(modifier = Modifier.padding(16.dp)) {
                                Text(text = container.name, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                                Text(text = "Path: ${container.path}", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                Spacer(modifier = Modifier.height(16.dp))
                                
                                Row(
                                    modifier = Modifier.fillMaxWidth(),
                                    horizontalArrangement = Arrangement.End
                                ) {
                                    if (isRunning) {
                                        Button(
                                            onClick = {
                                                val intent = android.content.Intent(context, ContainerDisplayActivity::class.java).apply {
                                                    addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
                                                    putExtra("CONTAINER_ID", container.id)
                                                }
                                                context.startActivity(intent)
                                            },
                                            modifier = Modifier.padding(end = 8.dp)
                                        ) {
                                            Text("View")
                                        }
                                        Button(
                                            onClick = {
                                                val cmd = "su -c \"${ContainerScript.bootBinaryPath} ${container.path} enter\""
                                                val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                                                val clip = android.content.ClipData.newPlainText("CLI Command", cmd)
                                                clipboard.setPrimaryClip(clip)
                                                android.widget.Toast.makeText(context, "Command copied! Paste in Termux", android.widget.Toast.LENGTH_LONG).show()
                                                
                                                val intent = context.packageManager.getLaunchIntentForPackage("com.termux")
                                                if (intent != null) {
                                                    context.startActivity(intent)
                                                }
                                            },
                                            modifier = Modifier.padding(end = 8.dp),
                                            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary)
                                        ) {
                                            Text("CLI")
                                        }
                                        Button(
                                            onClick = {
                                                coroutineScope.launch {
                                                    outputLog = "Stopping ${container.name}...\n"
                                                    val res = executeSuCommand(ContainerScript.getStopScript(container.path))
                                                    outputLog = res
                                                    runningContainers = runningContainers - container.id
                                                }
                                            },
                                            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
                                        ) {
                                            Text("Stop")
                                        }
                                    } else {
                                        Button(
                                            onClick = {
                                                coroutineScope.launch {
                                                    outputLog = "Starting ${container.name}...\n"
                                                    val res = executeSuCommand(ContainerScript.getStartScript(container.path))
                                                    outputLog = res
                                                    if (!res.contains("ERROR")) {
                                                        runningContainers = runningContainers + container.id
                                                    }
                                                }
                                            },
                                            modifier = Modifier.padding(end = 8.dp)
                                        ) {
                                            Text("Start")
                                        }
                                        Button(
                                            onClick = {
                                                val cmd = "su -c \"${ContainerScript.bootBinaryPath} ${container.path} enter\""
                                                val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                                                val clip = android.content.ClipData.newPlainText("CLI Command", cmd)
                                                clipboard.setPrimaryClip(clip)
                                                android.widget.Toast.makeText(context, "Command copied! Paste in Termux", android.widget.Toast.LENGTH_LONG).show()
                                                
                                                val intent = context.packageManager.getLaunchIntentForPackage("com.termux")
                                                if (intent != null) {
                                                    context.startActivity(intent)
                                                }
                                            },
                                            modifier = Modifier.padding(end = 8.dp),
                                            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary)
                                        ) {
                                            Text("CLI")
                                        }
                                        Button(
                                            onClick = {
                                                coroutineScope.launch {
                                                    outputLog = "Deleting ${container.name}...\n"
                                                    executeSuCommand("rm -rf ${container.path}")
                                                    val updatedList = containers.filter { it.id != container.id }
                                                    containers = updatedList
                                                    saveContainers(context, updatedList)
                                                    outputLog = "Deleted ${container.name}\n"
                                                }
                                            },
                                            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
                                        ) {
                                            Text("Delete")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))
                Text(text = "Logs:", fontWeight = FontWeight.Bold)
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(120.dp)
                        .padding(top = 8.dp),
                    color = MaterialTheme.colorScheme.surfaceVariant,
                    shape = MaterialTheme.shapes.medium
                ) {
                    SelectionContainer {
                        Text(
                            text = outputLog,
                            modifier = Modifier
                                .padding(16.dp)
                                .verticalScroll(rememberScrollState()),
                            fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                            fontSize = 12.sp
                        )
                    }
                }
            }
        }
}

@Composable
fun ContainerDisplayScreen(containerPath: String, onBack: () -> Unit) {
    val coroutineScope = rememberCoroutineScope()
    
    BackHandler(onBack = {
        onBack()
    })

    Box(modifier = Modifier.fillMaxSize()) {
        AndroidView(
            factory = { context ->
                var permissionJob: kotlinx.coroutines.Job? = null
                var looping = false
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            looping = true
                            coroutineScope.launch(Dispatchers.Main) {
                                DisplayManager.startDisplay(context, holder.surface, containerPath)
                            }
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int
                        ) {
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            looping = false
                            DisplayManager.stopDisplay(holder.surface)
                        }
                    })
                }
            },
            modifier = Modifier.fillMaxSize()
        )
    }
}

suspend fun extractTarball(context: Context, uri: Uri, containerName: String): ContainerInfo? = withContext(Dispatchers.IO) {
    try {
        val cacheFile = File(context.cacheDir, "rootfs_temp_${System.currentTimeMillis()}.tar.gz")
        context.contentResolver.openInputStream(uri)?.use { input ->
            FileOutputStream(cacheFile).use { output ->
                input.copyTo(output)
            }
        }

        val id = UUID.randomUUID().toString()
        val containersDir = File(context.filesDir, "containers")
        val destDir = File(containersDir, "container_$id").absolutePath
        
        val cmd = """
            echo "Creating target directory: $destDir"
            mkdir -p $destDir
            echo "Extracting tarball... This may take a while."
            tar -xzpf ${cacheFile.absolutePath} -C $destDir
            echo "Extraction complete!"
            rm -f ${cacheFile.absolutePath}
        """.trimIndent()
        
        val res = executeSuCommand(cmd)
        if (res.contains("ERROR")) {
            null
        } else {
            ContainerInfo(id, containerName, destDir)
        }
    } catch (e: Exception) {
        null
    }
}

suspend fun executeSuCommand(command: String): String = withContext(Dispatchers.IO) {
    try {
        val result = com.topjohnwu.superuser.Shell.cmd(command).exec()
        val output = StringBuilder()
        
        for (line in result.out) {
            output.append(line).append("\n")
        }
        for (line in result.err) {
            output.append("ERROR: ").append(line).append("\n")
        }
        
        output.toString()
    } catch (e: Exception) {
        "Exception: ${e.message}\n"
    }
}
