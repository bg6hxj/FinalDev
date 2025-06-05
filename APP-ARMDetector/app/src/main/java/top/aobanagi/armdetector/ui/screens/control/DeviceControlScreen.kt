package top.aobanagi.armdetector.ui.screens.control

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Videocam
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import top.aobanagi.armdetector.ui.components.DeviceSelector

@Composable
fun DeviceControlScreen(viewModel: DeviceControlViewModel = viewModel()) {
    val uiState by viewModel.uiState.collectAsState()
    val context = LocalContext.current
    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        Text(
            "设备控制", 
            style = MaterialTheme.typography.titleLarge,
            fontWeight = FontWeight.Bold
        )
        
        Spacer(modifier = Modifier.height(16.dp))
        
        // 设备选择器
        DeviceSelector(
            devices = uiState.availableDevices,
            selectedDevice = uiState.selectedDevice,
            onDeviceSelected = { viewModel.selectDevice(it) }
        )
        
        Spacer(modifier = Modifier.height(16.dp))
        
        if (uiState.selectedDevice.isEmpty()) {
            Box(
                modifier = Modifier.fillMaxWidth().padding(32.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "请选择设备",
                    style = MaterialTheme.typography.bodyLarge
                )
            }
        } else {
            // 控制卡片
            Card(
                modifier = Modifier.fillMaxWidth(),
                elevation = CardDefaults.cardElevation(4.dp)
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    // 设备重启按钮
                    Button(
                        onClick = { viewModel.restartDevice() },
                        modifier = Modifier.fillMaxWidth(),
                        enabled = !uiState.isLoading
                    ) {
                        Icon(
                            imageVector = Icons.Default.Refresh,
                            contentDescription = "重启",
                            modifier = Modifier.padding(end = 8.dp)
                        )
                        Text("重启设备")
                    }
                    
                    Spacer(modifier = Modifier.height(16.dp))
                    
                    // 查看摄像头按钮
                    Button(
                        onClick = { openCameraStream(context, uiState.selectedDevice) },
                        modifier = Modifier.fillMaxWidth(),
                        enabled = !uiState.isLoading
                    ) {
                        Icon(
                            imageVector = Icons.Default.Videocam,
                            contentDescription = "摄像头",
                            modifier = Modifier.padding(end = 8.dp)
                        )
                        Text("查看摄像头")
                    }
                    
                    Spacer(modifier = Modifier.height(16.dp))
                    
                    // 摄像头设置按钮
                    Button(
                        onClick = { openCameraSettings(context, uiState.selectedDevice) },
                                                modifier = Modifier.fillMaxWidth(),
                        enabled = !uiState.isLoading
                    ) {
                        Icon(
                            imageVector = Icons.Default.Settings,
                            contentDescription = "设置",
                            modifier = Modifier.padding(end = 8.dp)
                        )
                        Text("摄像头设置")
                    }
                    
                    // 操作反馈
                    if (uiState.isLoading) {
                        Spacer(modifier = Modifier.height(16.dp))
                        Box(
                            modifier = Modifier.fillMaxWidth(),
                            contentAlignment = Alignment.Center
                        ) {
                            CircularProgressIndicator(
                                color = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.width(32.dp)
                            )
                        }
                    }
                    
                    if (uiState.operationStatus.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(16.dp))
                        Text(
                            text = uiState.operationStatus,
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (uiState.isError) Color.Red else Color.Green
                        )
                    }
                }
            }
        }
    }
}

private fun openCameraStream(context: Context, deviceId: String) {
    try {
        val url = "http://113.44.13.140:8181/stream?device=$deviceId" // 需要根据实际情况修改
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
        context.startActivity(intent)
    } catch (e: Exception) {
        // 处理无法打开URL的情况
    }
}

private fun openCameraSettings(context: Context, deviceId: String) {
    try {
        val url = "http://113.44.13.140:2288/" // 需要根据实际情况修改
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
        context.startActivity(intent)
    } catch (e: Exception) {
        // 处理无法打开URL的情况
    }
}