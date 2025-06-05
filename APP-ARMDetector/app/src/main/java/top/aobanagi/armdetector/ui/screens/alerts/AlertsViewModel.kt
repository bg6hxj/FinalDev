package top.aobanagi.armdetector.ui.screens.alerts

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

class AlertsViewModel : ViewModel() {
    private val sensorRepository = SensorRepository()

    private val _uiState = MutableStateFlow(AlertsUiState())
    val uiState: StateFlow<AlertsUiState> = _uiState.asStateFlow()

    private var timer: Timer? = null

    init {
        fetchAlerts()
        startRefreshTimer()
    }

    private fun fetchAlerts() {
        viewModelScope.launch {
            try {
                _uiState.update { it.copy(isLoading = true) }
                val alerts = sensorRepository.getAlertData()

                _uiState.update { currentState ->
                    currentState.copy(
                        alerts = alerts.sortedByDescending { it.timestamp },
                        isLoading = false,
                        errorMessage = null
                    )
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        isLoading = false,
                        errorMessage = "获取警报数据失败: ${e.message}"
                    )
                }
            }
        }
    }

    private fun startRefreshTimer() {
        timer = Timer().apply {
            scheduleAtFixedRate(object : TimerTask() {
                override fun run() {
                    fetchAlerts()
                }
            }, 10000, 10000) // 每10秒刷新一次
        }
    }

    // 添加公开方法供UI调用手动刷新
    fun refreshData() {
        viewModelScope.launch {
            try {
                _uiState.update { it.copy(isRefreshing = true) }
                val alerts = sensorRepository.getAlertData()

                _uiState.update { currentState ->
                    currentState.copy(
                        alerts = alerts.sortedByDescending { it.timestamp },
                        isRefreshing = false,
                        errorMessage = null
                    )
                }
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        isRefreshing = false,
                        errorMessage = "获取警报数据失败: ${e.message}"
                    )
                }
            }
        }
    }

    override fun onCleared() {
        super.onCleared()
        timer?.cancel()
        timer = null
    }
}

data class AlertsUiState(
    val alerts: List<SensorData> = emptyList(),
    val isLoading: Boolean = true,
    val errorMessage: String? = null,
    val isRefreshing: Boolean = false
)