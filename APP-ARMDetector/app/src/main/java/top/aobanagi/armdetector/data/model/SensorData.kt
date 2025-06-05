package top.aobanagi.armdetector.data.model

import com.google.gson.annotations.SerializedName
import java.time.ZonedDateTime
import java.time.ZoneId
import java.time.format.DateTimeFormatter

data class SensorData(
    val id: Long,
    @SerializedName("device_id")
    val deviceId: String,
    val timestamp: String,
    val temperature: Double,
    val humidity: Double,
    @SerializedName("co_ppm")
    val coPpm: Double,
    @SerializedName("dust_density")
    val dustDensity: Double,
    @SerializedName("alarm_status")
    val alarmStatus: String,
    @SerializedName("received_at")
    val receivedAt: String
) {
    fun getFormattedTimestamp(): String {
        return try {
            val time = ZonedDateTime.parse(timestamp)
                .withZoneSameInstant(ZoneId.of("Asia/Shanghai")) // 转换为GMT+8时区
            val formatter = DateTimeFormatter.ofPattern("HH:mm:ss")
            time.format(formatter)
        } catch (e: Exception) {
            try {
                // 尝试使用ISO格式解析
                val isoFormatter = DateTimeFormatter.ISO_OFFSET_DATE_TIME
                val time = ZonedDateTime.parse(timestamp, isoFormatter)
                    .withZoneSameInstant(ZoneId.of("Asia/Shanghai"))
                val formatter = DateTimeFormatter.ofPattern("HH:mm:ss")
                time.format(formatter)
            } catch (e2: Exception) {
                timestamp
            }
        }
    }
}

data class SensorDataResponse(
    val data: List<SensorData>,
    val pagination: Pagination
)

data class Pagination(
    @SerializedName("current_page")
    val currentPage: Int,
    @SerializedName("page_size")
    val pageSize: Int,
    @SerializedName("total_items")
    val totalItems: Int,
    @SerializedName("total_pages")
    val totalPages: Int
)