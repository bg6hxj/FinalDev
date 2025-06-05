package top.aobanagi.armdetector.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import top.aobanagi.armdetector.data.model.SensorData
import top.aobanagi.armdetector.ui.theme.AlarmCritical
import top.aobanagi.armdetector.ui.theme.AlarmWarning
import top.aobanagi.armdetector.ui.theme.AlarmNormal
import java.time.ZoneId
import java.time.ZonedDateTime
import java.time.format.DateTimeFormatter

@Composable
fun AlertItem(alert: SensorData, modifier: Modifier = Modifier) {
    // 如果告警状态为None或normal，不显示该项
    if (alert.alarmStatus.equals("None", ignoreCase = true) ||
        alert.alarmStatus.equals("normal", ignoreCase = true) ||
        alert.alarmStatus.equals("OK", ignoreCase = true) ||
        alert.alarmStatus.isBlank() ||
        alert.alarmStatus.isEmpty()) {
        return
    }

    // 使用更丰富的告警类型处理函数
    val alarmInfo = getAlarmInfo(alert.alarmStatus)
    val alertColor = alarmInfo.color
    val alarmIcon = alarmInfo.icon
    val alarmDisplayName = alarmInfo.displayName

    val alertTime = try {
        val time = ZonedDateTime.parse(alert.timestamp)
            .withZoneSameInstant(ZoneId.of("Asia/Shanghai")) // 转换为GMT+8时区
        val formatter = DateTimeFormatter.ofPattern("MM-dd HH:mm:ss")
        time.format(formatter)
    } catch (e: Exception) {
        alert.timestamp
    }

    Card(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        elevation = CardDefaults.cardElevation(2.dp),
        shape = RoundedCornerShape(8.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(alertColor.copy(alpha = 0.1f))
                .border(1.dp, alertColor, RoundedCornerShape(8.dp))
                .padding(12.dp)
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = "设备: ${alert.deviceId}",
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onSurface
                )

                Text(
                    text = "$alarmIcon $alarmDisplayName",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Bold,
                    color = alertColor
                )

                // 显示相关传感器数据
                val shouldShowTemp = alert.alarmStatus.contains("temp", ignoreCase = true) ||
                        alert.alarmStatus.contains("temperature", ignoreCase = true)
                val shouldShowHumidity = alert.alarmStatus.contains("humid", ignoreCase = true) ||
                        alert.alarmStatus.contains("humidity", ignoreCase = true)
                val shouldShowCO = alert.alarmStatus.contains("co", ignoreCase = true) ||
                        alert.alarmStatus.contains("carbon", ignoreCase = true)
                val shouldShowDust = alert.alarmStatus.contains("dust", ignoreCase = true) ||
                        alert.alarmStatus.contains("pm", ignoreCase = true)

                // 始终展示所有可能的相关数据
                Text(
                    text = "温度: ${alert.temperature}°C",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = if (shouldShowTemp) FontWeight.Bold else FontWeight.Normal
                )

                Text(
                    text = "湿度: ${alert.humidity}%",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = if (shouldShowHumidity) FontWeight.Bold else FontWeight.Normal
                )

                Text(
                    text = "CO: ${alert.coPpm} PPM",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = if (shouldShowCO) FontWeight.Bold else FontWeight.Normal
                )

                Text(
                    text = "粉尘: ${alert.dustDensity} µg/m³",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = if (shouldShowDust) FontWeight.Bold else FontWeight.Normal
                )
            }

            Text(
                text = alertTime,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

// 警报信息数据类
data class AlarmInfo(
    val color: androidx.compose.ui.graphics.Color,
    val icon: String,
    val displayName: String
)

// 根据警报状态获取警报信息
fun getAlarmInfo(alarmStatus: String): AlarmInfo {
    if (alarmStatus.isBlank()) {
        return AlarmInfo(
            color = AlarmNormal,
            icon = "❓",
            displayName = "未知警报"
        )
    }

    val status = alarmStatus.lowercase()

    // 粉尘相关警报
    if (status.contains("dust")) {
        return when {
            status.contains("critical") -> {
                AlarmInfo(
                    color = AlarmCritical,
                    icon = "⚠️",
                    displayName = "粉尘浓度严重超标"
                )
            }
            status.contains("high") || status.contains("warning") -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "💨",
                    displayName = "粉尘浓度过高"
                )
            }
            else -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "💨",
                    displayName = "粉尘浓度异常"
                )
            }
        }
    }
    // 温度相关警报
    else if (status.contains("temp") || status.contains("temperature")) {
        return when {
            status.contains("high") -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "🔥",
                    displayName = "温度过高"
                )
            }
            status.contains("low") -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "❄️",
                    displayName = "温度过低"
                )
            }
            status.contains("critical") -> {
                AlarmInfo(
                    color = AlarmCritical,
                    icon = "🔥",
                    displayName = "温度严重异常"
                )
            }
            else -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "🌡️",
                    displayName = "温度异常"
                )
            }
        }
    }
    // 湿度相关警报
    else if (status.contains("humid") || status.contains("humidity")) {
        return when {
            status.contains("high") -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "💧",
                    displayName = "湿度过高"
                )
            }
            status.contains("low") -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "🏜️",
                    displayName = "湿度过低"
                )
            }
            status.contains("critical") -> {
                AlarmInfo(
                    color = AlarmCritical,
                    icon = "💧",
                    displayName = "湿度严重异常"
                )
            }
            else -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "💧",
                    displayName = "湿度异常"
                )
            }
        }
    }
    // CO相关警报
    else if (status.contains("co") || status.contains("carbon")) {
        return when {
            status.contains("critical") -> {
                AlarmInfo(
                    color = AlarmCritical,
                    icon = "☁️",
                    displayName = "一氧化碳浓度严重超标"
                )
            }
            else -> {
                AlarmInfo(
                    color = AlarmWarning,
                    icon = "☁️",
                    displayName = "一氧化碳浓度过高"
                )
            }
        }
    }
    // 烟雾相关警报
    else if (status.contains("smoke")) {
        return AlarmInfo(
            color = AlarmCritical,
            icon = "🔥",
            displayName = "检测到烟雾"
        )
    }
    // 气体相关警报
    else if (status.contains("gas")) {
        return AlarmInfo(
            color = AlarmCritical,
            icon = "☢️",
            displayName = "检测到气体泄漏"
        )
    }
    // 一般高值警报
    else if (status.contains("high")) {
        return AlarmInfo(
            color = AlarmWarning,
            icon = "⚠️",
            displayName = "数值过高警报"
        )
    }
    // 紧急或危险警报
    else if (status.contains("critical") || status.contains("danger")) {
        return AlarmInfo(
            color = AlarmCritical,
            icon = "🔴",
            displayName = "紧急警报"
        )
    }
    // 警告级别警报
    else if (status.contains("warning") || status.contains("alert")) {
        return AlarmInfo(
            color = AlarmWarning,
            icon = "⚠️",
            displayName = "警告"
        )
    }
    // 未知类型警报
    else {
        return AlarmInfo(
            color = AlarmWarning,
            icon = "⚠️",
            displayName = translateAlarmStatus(alarmStatus)
        )
    }
}

// 翻译常见警报状态文本为中文
fun translateAlarmStatus(statusText: String): String {
    if (statusText.isBlank()) return "未知警报"

    // 移除可能的数字和符号，仅保留核心文本
    val simplifiedStatus = statusText.replace(Regex("[0-9.]+"), "").trim()

    // 常见警报状态的映射表
    val translations = mapOf(
        "dust high" to "粉尘浓度过高",
        "co high" to "一氧化碳浓度过高",
        "temperature high" to "温度过高",
        "humidity high" to "湿度过高",
        "temperature low" to "温度过低",
        "humidity low" to "湿度过低",
        "alarm" to "警报",
        "warning" to "警告",
        "error" to "错误",
        "alert" to "警报",
        "critical" to "严重警报",
        "danger" to "危险警报",
        "emergency" to "紧急情况",
        "pm high" to "颗粒物浓度过高",
        "smoke detected" to "检测到烟雾",
        "gas detected" to "检测到气体泄漏"
    )

    // 查找最匹配的翻译
    for ((key, value) in translations) {
        if (simplifiedStatus.lowercase().contains(key.lowercase())) {
            return value
        }
    }

    // 如果找不到匹配项，保留原文并添加备注
    return "警报: $statusText"
}