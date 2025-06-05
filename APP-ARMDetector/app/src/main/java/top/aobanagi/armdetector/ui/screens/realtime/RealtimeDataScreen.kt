package top.aobanagi.armdetector.ui.screens.realtime

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Air
import androidx.compose.material.icons.filled.Co2
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material.icons.filled.WaterDrop
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import top.aobanagi.armdetector.ui.components.DeviceSelector
import top.aobanagi.armdetector.ui.components.SensorDataRow
import top.aobanagi.armdetector.ui.theme.AlarmCritical
import top.aobanagi.armdetector.ui.theme.AlarmNormal
import top.aobanagi.armdetector.ui.theme.AlarmWarning
import top.aobanagi.armdetector.ui.theme.CoDangerous
import top.aobanagi.armdetector.ui.theme.CoNormal
import top.aobanagi.armdetector.ui.theme.CoWarning
import top.aobanagi.armdetector.ui.theme.DustDangerous
import top.aobanagi.armdetector.ui.theme.DustNormal
import top.aobanagi.armdetector.ui.theme.DustWarning
import top.aobanagi.armdetector.ui.theme.HumidityHigh
import top.aobanagi.armdetector.ui.theme.HumidityLow
import top.aobanagi.armdetector.ui.theme.HumidityNormal
import top.aobanagi.armdetector.ui.theme.TemperatureCold
import top.aobanagi.armdetector.ui.theme.TemperatureNormal
import top.aobanagi.armdetector.ui.theme.TemperatureWarm
import top.aobanagi.armdetector.ui.components.AlarmInfo
import top.aobanagi.armdetector.ui.components.getAlarmInfo

@Composable
fun RealtimeDataScreen(viewModel: RealtimeViewModel = viewModel()) {
    val uiState by viewModel.uiState.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        Text(
            "实时数据",
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

        if (uiState.isLoading) {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                CircularProgressIndicator(color = MaterialTheme.colorScheme.primary)
            }
        } else if (uiState.errorMessage != null) {
            Text(
                text = uiState.errorMessage ?: "",
                color = Color.Red,
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.padding(16.dp)
            )
        } else if (uiState.selectedDevice.isEmpty()) {
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
            // 实时数据卡片
            Card(
                modifier = Modifier.fillMaxWidth(),
                elevation = CardDefaults.cardElevation(4.dp)
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        text = "数据更新时间: ${uiState.lastUpdateTime}",
                        style = MaterialTheme.typography.bodyMedium,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.padding(bottom = 16.dp)
                    )

                    // 温度指标
                    SensorDataRow(
                        icon = Icons.Default.Thermostat,
                        title = "温度",
                        value = "${uiState.temperature}°C",
                        color = getTemperatureColor(uiState.temperature)
                    )

                    // 湿度指标
                    SensorDataRow(
                        icon = Icons.Default.WaterDrop,
                        title = "湿度",
                        value = "${uiState.humidity}%",
                        color = getHumidityColor(uiState.humidity)
                    )

                    // CO浓度指标
                    SensorDataRow(
                        icon = Icons.Default.Co2,
                        title = "一氧化碳",
                        value = "${uiState.coPpm} PPM",
                        color = getCoColor(uiState.coPpm)
                    )

                    // 粉尘浓度指标
                    SensorDataRow(
                        icon = Icons.Default.Air,
                        title = "粉尘浓度",
                        value = "${uiState.dustDensity} µg/m³",
                        color = getDustColor(uiState.dustDensity)
                    )

                    Spacer(modifier = Modifier.height(16.dp))

                    // 警报状态
                    val alarmInfo = getAlarmInfo(uiState.alarmStatus)
                    Text(
                        text = "警报状态: ${alarmInfo.icon} ${alarmInfo.displayName}",
                        style = MaterialTheme.typography.bodyLarge,
                        color = alarmInfo.color,
                        fontWeight = FontWeight.Bold
                    )
                }
            }
        }
    }
}

// 获取温度颜色
private fun getTemperatureColor(temp: Double): Color {
    return when {
        temp > 30 -> TemperatureWarm
        temp < 10 -> TemperatureCold
        else -> TemperatureNormal
    }
}

// 获取湿度颜色
private fun getHumidityColor(humidity: Double): Color {
    return when {
        humidity > 80 -> HumidityHigh
        humidity < 20 -> HumidityLow
        else -> HumidityNormal
    }
}

// 获取CO浓度颜色
private fun getCoColor(coPpm: Double): Color {
    return when {
        coPpm > 50 -> CoDangerous
        coPpm > 20 -> CoWarning
        else -> CoNormal
    }
}

// 获取粉尘浓度颜色
private fun getDustColor(dustDensity: Double): Color {
    return when {
        dustDensity > 100 -> DustDangerous
        dustDensity > 50 -> DustWarning
        else -> DustNormal
    }
}

// 此方法已不再使用，保留为注释以便参考
/*
private fun getAlarmStatusColor(status: String): Color {
    return when {
        status.contains("critical") -> AlarmCritical
        status.contains("warning") -> AlarmWarning
        else -> AlarmNormal
    }
}
*/