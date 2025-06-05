package top.aobanagi.armdetector.ui.screens.dashboard

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import top.aobanagi.armdetector.data.model.SensorData
import top.aobanagi.armdetector.data.repository.SensorRepository
import java.util.Timer
import java.util.TimerTask

class DashboardViewModel : ViewModel() {
    private val sensorRepository = SensorRepository()

    private val _uiState = MutableStateFlow(DashboardUiState())
    val uiState: StateFlow<DashboardUiState> = _uiState.asStateFlow()

    private var timer: Timer? = null

    init {
        fetchSystemStatus()
        startRefreshTimer()
    }

    // 添加公开方法供UI调用手动刷新
    fun refreshData() {
        viewModelScope.launch {
            try {
                _uiState.update { it.copy(isRefreshing = true) }
                val alerts = sensorRepository.getAlertData()

                _uiState.update { currentState ->
                    currentState.copy(
                        systemStatus = "运行中",
                        onlineDevices = getUniqueDeviceCount(alerts),
                        recentAlerts = alerts.sortedByDescending { it.timestamp }.take(5),
                        isRefreshing = false,
                        errorMessage = null
                    )
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        isRefreshing = false,
                        errorMessage = "获取数据失败: ${e.message}"
                    )
                }
            }
        }
    }

    private fun fetchSystemStatus() {
        viewModelScope.launch {
            try {
                _uiState.update { it.copy(isLoading = true, isRefreshing = false) }
                val alerts = sensorRepository.getAlertData()

                _uiState.update { currentState ->
                    currentState.copy(
                        systemStatus = "运行中",
                        onlineDevices = getUniqueDeviceCount(alerts),
                        recentAlerts = alerts.sortedByDescending { it.timestamp }.take(5),
                        isLoading = false,
                        isRefreshing = false,
                        errorMessage = null
                    )
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        isLoading = false,
                        isRefreshing = false,
                        errorMessage = "获取数据失败: ${e.message}"
                    )
                }
            }
        }
    }

    private fun getUniqueDeviceCount(data: List<SensorData>): Int {
        return data.map { it.deviceId }.distinct().size
    }

    private fun startRefreshTimer() {
        timer = Timer().apply {
            scheduleAtFixedRate(object : TimerTask() {
                override fun run() {
                    fetchSystemStatus()
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

data class DashboardUiState(
    val systemStatus: String = "加载中...",
    val onlineDevices: Int = 0,
    val recentAlerts: List<SensorData> = emptyList(),
    val isLoading: Boolean = true,
    val errorMessage: String? = null,
    val isRefreshing: Boolean = false
)