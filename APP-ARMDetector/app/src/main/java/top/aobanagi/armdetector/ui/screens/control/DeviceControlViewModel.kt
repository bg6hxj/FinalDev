package top.aobanagi.armdetector.ui.screens.control

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import top.aobanagi.armdetector.data.repository.CommandRepository
import top.aobanagi.armdetector.data.repository.CommandRepository.CommandType
import top.aobanagi.armdetector.data.repository.SensorRepository

class DeviceControlViewModel : ViewModel() {
    private val sensorRepository = SensorRepository()
    private val commandRepository = CommandRepository()
    
    private val _uiState = MutableStateFlow(DeviceControlUiState())
    val uiState: StateFlow<DeviceControlUiState> = _uiState.asStateFlow()
    
    init {
        fetchDevices()
    }
    
    fun selectDevice(deviceId: String) {
        _uiState.update { currentState ->
            currentState.copy(
                selectedDevice = deviceId,
                operationStatus = "",
                isError = false
            )
        }
    }
    
    fun restartDevice() {
        val deviceId = _uiState.value.selectedDevice
        if (deviceId.isEmpty()) return
        
        viewModelScope.launch {
            try {
                _uiState.update { it.copy(isLoading = true, operationStatus = "") }
                
                val response = commandRepository.sendCommand(
                    deviceId = deviceId,
                    commandType = CommandType.RESTART_DEVICE
                )
                
                _uiState.update { currentState ->
                    currentState.copy(
                        isLoading = false,
                        operationStatus = "命令已发送：${response.message}",
                        isError = false,
                        currentRequestId = response.requestId
                    )
                }
                
                // 轮询命令状态
                pollCommandStatus(response.requestId)
                
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        isLoading = false,
                        operationStatus = "发送命令失败: ${e.message}",
                        isError = true
                    )
                }
            }
        }
    }
    
    private fun pollCommandStatus(requestId: String) {
        viewModelScope.launch {
            try {
                var attempts = 0
                var completed = false
                
                while (attempts < 10 && !completed) {
                    kotlinx.coroutines.delay(2000) // 每2秒查询一次
                    
                    val status = commandRepository.getCommandStatus(requestId)
                    
                    when (status.status) {
                        "success" -> {
                            _uiState.update { currentState ->
                                currentState.copy(
                                    operationStatus = "命令执行成功",
                                    isError = false
                                )
                            }
                            completed = true
                        }
                        "error" -> {
                            _uiState.update { currentState ->
                                currentState.copy(
                                    operationStatus = "命令执行失败: ${status.response}",
                                    isError = true
                                )
                            }
                            completed = true
                        }
                        else -> {
                            _uiState.update { currentState ->
                                currentState.copy(
                                    operationStatus = "命令执行中...",
                                    isError = false
                                )
                            }
                        }
                    }
                    
                    attempts++
                }
                
                if (!completed) {
                    _uiState.update { currentState ->
                        currentState.copy(
                            operationStatus = "命令状态查询超时，请稍后检查设备状态",
                            isError = true
                        )
                    }
                }
                
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        operationStatus = "查询命令状态失败: ${e.message}",
                        isError = true
                    )
                }
            }
        }
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
            } catch (e: Exception) {
                _uiState.update { currentState ->
                    currentState.copy(
                        operationStatus = "获取设备列表失败: ${e.message}",
                        isError = true
                    )
                }
            }
        }
    }
}

data class DeviceControlUiState(
    val availableDevices: List<String> = emptyList(),
    val selectedDevice: String = "",
    val isLoading: Boolean = false,
    val operationStatus: String = "",
    val isError: Boolean = false,
    val currentRequestId: String = ""
)