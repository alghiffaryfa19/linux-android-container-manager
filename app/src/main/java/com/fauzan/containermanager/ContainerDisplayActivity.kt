package com.fauzan.containermanager

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier

class ContainerDisplayActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val containerId = intent.getStringExtra("CONTAINER_ID") ?: ""
        val containers = loadContainers(this)
        val container = containers.firstOrNull { it.id == containerId }
        val containerPath = container?.path ?: ""
        
        setContent {
            MaterialTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    ContainerDisplayScreen(containerPath = containerPath, onBack = { finish() })
                }
            }
        }
    }
}
