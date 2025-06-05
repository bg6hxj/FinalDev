package top.aobanagi.armdetector.data.repository

import top.aobanagi.armdetector.data.api.ApiClient
import top.aobanagi.armdetector.data.model.Command
import top.aobanagi.armdetector.data.model.CommandResponse
import top.aobanagi.armdetector.data.model.CommandStatus

class CommandRepository {
    private val apiService = ApiClient.apiService
    
    suspend fun sendCommand(deviceId: String, commandType: CommandType, parameters: Map<String, Any> = emptyMap()): CommandResponse {
        val command = Command(
            command = commandType.value,
            parameters = parameters
        )
        
        return apiService.sendCommand(deviceId, command)
    }
    
    suspend fun getCommandStatus(requestId: String): CommandStatus {
        return apiService.getCommandStatus(requestId)
    }
    
    enum class CommandType(val value: String) {
        RESTART_DEVICE("restart_device"),
        TAKE_PHOTO("take_photo"),
        RENAME_DEVICE("rename_device")
    }
}