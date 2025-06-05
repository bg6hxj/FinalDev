package top.aobanagi.armdetector.ui.screens.history

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import top.aobanagi.armdetector.data.model.SensorData
import top.aobanagi.armdetector.data.repository.SensorRepository
import top.aobanagi.armdetector.data.repository.SensorRepository.TimeRange as RepoTimeRange

class HistoryViewModel : ViewModel() {
    private val sensorRepository = SensorRepository()

    private val _uiState = MutableStateFlow(HistoryUiState())
    val uiState: StateFlow<HistoryUiState> = _uiState.asStateFlow()

    init {
        fetchDevices()
    }

    fun selectDevice(deviceId: String) {
        _uiState.update { currentState ->
            currentState.copy(selectedDevice = deviceId)
        }
        fetchHistoricalData()
    }

    fun selectDataType(dataType: DataType) {
        _uiState.update { currentState ->
            currentState.copy(selectedDataType = dataType)
        }
        fetchHistoricalData()
    }

    fun selectTimeRange(timeRange: TimeRange) {
        _uiState.update { currentState ->
            currentState.copy(selectedTimeRange = timeRange)
        }
        fetchHistoricalData()
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

                // 如果有设备，获取第一个设备的历史数据
                if (devices.isNotEmpty()) {
                    fetchHistoricalData()
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        errorMessage = "获取设备列表失败: ${e.message}",
                        isLoading = false
                    )
                }
            }
        }
    }

    private fun fetchHistoricalData() {
        val deviceId = _uiState.value.selectedDevice
        if (deviceId.isEmpty()) return

        viewModelScope.launch {
            try {
                _uiState.update { it.copy(isLoading = true) }

                val repoTimeRange = when (_uiState.value.selectedTimeRange) {
                    TimeRange.LAST_1_MINUTE -> RepoTimeRange.LAST_MINUTE
                    TimeRange.LAST_5_MINUTES -> RepoTimeRange.LAST_5_MINUTES
                    TimeRange.LAST_10_MINUTES -> RepoTimeRange.LAST_5_MINUTES // 对于10分钟，我们获取10分钟的数据
                    TimeRange.LAST_30_MINUTES -> RepoTimeRange.LAST_30_MINUTES
                    TimeRange.LAST_HOUR -> RepoTimeRange.LAST_HOUR
                    TimeRange.LAST_6_HOURS -> RepoTimeRange.LAST_6_HOURS
                    TimeRange.LAST_DAY -> RepoTimeRange.LAST_DAY
                }

                // 特殊处理10分钟的情况
                if (_uiState.value.selectedTimeRange == TimeRange.LAST_10_MINUTES) {
                    // 获取最近10分钟的数据（分两次获取：0-5分钟 和 5-10分钟）
                    val recentData = sensorRepository.getHistoricalData(
                        deviceId = deviceId,
                        timeRange = RepoTimeRange.LAST_5_MINUTES,
                        skipPagination = true
                    )

                    val olderData = sensorRepository.getHistoricalData(
                        deviceId = deviceId,
                        timeRange = RepoTimeRange.LAST_5_MINUTES,
                        skipPagination = true,
                        // 计算5-10分钟前的时间范围
                        endTime = java.time.ZonedDateTime.now().minusMinutes(5),
                        startTime = java.time.ZonedDateTime.now().minusMinutes(10)
                    )

                    // 合并数据
                    val combinedData = olderData + recentData

                    processHistoryData(combinedData)
                } else {
                    // 其他时间范围，正常获取数据
                    val historyData = sensorRepository.getHistoricalData(
                        deviceId = deviceId,
                        timeRange = repoTimeRange,
                        skipPagination = true
                    )

                    processHistoryData(historyData)
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        isLoading = false,
                        errorMessage = "获取历史数据失败: ${e.message}"
                    )
                }
            }
        }
    }

    private fun processHistoryData(historyData: List<SensorData>) {
        if (historyData.isNotEmpty()) {
            // 按时间顺序排序，确保图表数据正确
            val sortedData = historyData.distinctBy { it.id }.sortedBy { data ->
                try {
                    java.time.ZonedDateTime.parse(data.timestamp)
                } catch (e: Exception) {
                    java.time.ZonedDateTime.now() // 解析失败使用当前时间作为备选
                }
            }

            val chartData = extractChartData(
                data = sortedData,
                dataType = _uiState.value.selectedDataType
            )

            // 提取时间戳数据
            val timestampList = sortedData.map { it.getFormattedTimestamp() }

            _uiState.update { currentState ->
                currentState.copy(
                    chartData = chartData,
                    timestamps = timestampList,
                    isLoading = false,
                    errorMessage = null
                )
            }
        } else {
            _uiState.update { currentState ->
                currentState.copy(
                    chartData = emptyList(),
                    timestamps = emptyList(),
                    isLoading = false,
                    errorMessage = null
                )
            }
        }
    }

    private fun extractChartData(data: List<SensorData>, dataType: DataType): List<Float> {
        return data.map { sensorData ->
            when (dataType) {
                DataType.TEMPERATURE -> sensorData.temperature.toFloat()
                DataType.HUMIDITY -> sensorData.humidity.toFloat()
                DataType.CO -> sensorData.coPpm.toFloat()
                DataType.DUST -> sensorData.dustDensity.toFloat()
            }
        }
    }
}

data class HistoryUiState(
    val availableDevices: List<String> = emptyList(),
    val selectedDevice: String = "",
    val selectedDataType: DataType = DataType.TEMPERATURE,
    val selectedTimeRange: TimeRange = TimeRange.LAST_10_MINUTES,
    val chartData: List<Float> = emptyList(),
    val timestamps: List<String> = emptyList(),
    val isLoading: Boolean = true,
    val errorMessage: String? = null
)

enum class DataType {
    TEMPERATURE, HUMIDITY, CO, DUST
}

enum class TimeRange {
    LAST_1_MINUTE, LAST_5_MINUTES, LAST_10_MINUTES, LAST_30_MINUTES, LAST_HOUR, LAST_6_HOURS, LAST_DAY
}