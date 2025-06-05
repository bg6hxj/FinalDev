package top.aobanagi.armdetector.ui.screens.realtime

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import top.aobanagi.armdetector.data.repository.SensorRepository
import java.time.ZonedDateTime
import java.time.format.DateTimeFormatter
import java.util.Timer
import java.util.TimerTask
import java.time.ZoneId

class RealtimeViewModel : ViewModel() {
    private val sensorRepository = SensorRepository()

    private val _uiState = MutableStateFlow(RealtimeUiState())
    val uiState: StateFlow<RealtimeUiState> = _uiState.asStateFlow()

    private var timer: Timer? = null

    init {
        fetchDevices()
        startRefreshTimer()
    }

    fun selectDevice(deviceId: String) {
        _uiState.update { currentState ->
            currentState.copy(selectedDevice = deviceId)
        }
        fetchLatestData(deviceId)
    }

    private fun fetchDevices() {
        viewModelScope.launch {
            try {
                val response = sensorRepository.getSensorData(pageSize = 100, skipPagination = true)
                val devices = response.data.map { it.deviceId }.distinct()

                _uiState.update { currentState ->
                    currentState.copy(
                        availableDevices = devices,
                        selectedDevice = devices.firstOrNull() ?: ""
                    )
                }

                // 如果有设备，获取第一个设备的最新数据
                if (devices.isNotEmpty()) {
                    fetchLatestData(devices.first())
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        errorMessage = "获取设备列表失败: ${e.message}"
                    )
                }
            }
        }
    }

    private fun fetchLatestData(deviceId: String) {
        viewModelScope.launch {
            try {
                _uiState.update { it.copy(isLoading = true) }

                val latestData = sensorRepository.getLatestSensorData(deviceId)

                if (latestData.isNotEmpty()) {
                    val data = latestData.first()

                    // 确保时区转换正确，使用更健壮的方式处理时间
                    val formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")
                    val timestamp = try {
                        // 首先尝试直接解析时间戳
                        val parsedTime = ZonedDateTime.parse(data.timestamp)
                        // 确保转换到GMT+8时区
                        val shanghaiTime = parsedTime.withZoneSameInstant(ZoneId.of("Asia/Shanghai"))
                        // 格式化为易读的时间字符串
                        shanghaiTime.format(formatter)
                    } catch (e: Exception) {
                        try {
                            // 如果标准解析失败，尝试ISO格式解析
                            val isoFormatter = DateTimeFormatter.ISO_OFFSET_DATE_TIME
                            val parsedTime = ZonedDateTime.parse(data.timestamp, isoFormatter)
                            parsedTime.withZoneSameInstant(ZoneId.of("Asia/Shanghai")).format(formatter)
                        } catch (e2: Exception) {
                            // 如果所有解析方法都失败，显示原始时间戳
                            "北京时间 ${data.timestamp} (未转换)"
                        }
                    }

                    _uiState.update { currentState ->
                        currentState.copy(
                            temperature = data.temperature,
                            humidity = data.humidity,
                            coPpm = data.coPpm,
                            dustDensity = data.dustDensity,
                            alarmStatus = data.alarmStatus,
                            lastUpdateTime = timestamp,
                            isLoading = false,
                            errorMessage = null
                        )
                    }
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        isLoading = false,
                        errorMessage = "获取实时数据失败: ${e.message}"
                    )
                }
            }
        }
    }

    private fun startRefreshTimer() {
        timer = Timer().apply {
            scheduleAtFixedRate(object : TimerTask() {
                override fun run() {
                    val currentDevice = _uiState.value.selectedDevice
                    if (currentDevice.isNotEmpty()) {
                        fetchLatestData(currentDevice)
                    }
                }
            }, 5000, 5000) // 每5秒刷新一次
        }
    }

    override fun onCleared() {
        super.onCleared()
        timer?.cancel()
        timer = null
    }
}

data class RealtimeUiState(
    val availableDevices: List<String> = emptyList(),
    val selectedDevice: String = "",
    val temperature: Double = 0.0,
    val humidity: Double = 0.0,
    val coPpm: Double = 0.0,
    val dustDensity: Double = 0.0,
    val alarmStatus: String = "unknown",
    val lastUpdateTime: String = "",
    val isLoading: Boolean = true,
    val errorMessage: String? = null
)