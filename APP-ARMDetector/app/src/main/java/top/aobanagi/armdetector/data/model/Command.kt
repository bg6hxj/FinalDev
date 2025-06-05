package top.aobanagi.armdetector.data.model

import com.google.gson.annotations.SerializedName

data class Command(
    val command: String,
    val parameters: Map<String, Any> = emptyMap()
)

data class CommandResponse(
    val message: String,
    @SerializedName("request_id") 
    val requestId: String
)

data class CommandStatus(
    val id: Long,
    @SerializedName("device_id") 
    val deviceId: String,
    val command: String,
    @SerializedName("request_id") 
    val requestId: String,
    val status: String,  // "pending", "success", "error"
    @SerializedName("sent_at") 
    val sentAt: String,
    val response: String
)