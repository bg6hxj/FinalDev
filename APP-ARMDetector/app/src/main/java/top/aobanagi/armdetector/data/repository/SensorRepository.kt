package top.aobanagi.armdetector.data.repository

import top.aobanagi.armdetector.data.api.ApiClient
import top.aobanagi.armdetector.data.model.SensorData
import top.aobanagi.armdetector.data.model.SensorDataResponse
import java.time.ZonedDateTime
import java.time.format.DateTimeFormatter

class SensorRepository {
    private val apiService = ApiClient.apiService

    suspend fun getSensorData(
        deviceId: String? = null,
        startTime: ZonedDateTime? = null,
        endTime: ZonedDateTime? = null,
        page: Int = 1,
        pageSize: Int = 20,
        skipPagination: Boolean = false
    ): SensorDataResponse {
        val formatter = DateTimeFormatter.ISO_OFFSET_DATE_TIME
        val formattedStartTime = startTime?.format(formatter)
        val formattedEndTime = endTime?.format(formatter)

        return apiService.getSensorData(
            deviceId = deviceId,
            startTime = formattedStartTime,
            endTime = formattedEndTime,
            page = page,
            pageSize = pageSize,
            skipPagination = skipPagination
        )
    }

    suspend fun getLatestSensorData(deviceId: String? = null): List<SensorData> {
        val endTime = ZonedDateTime.now()
        val startTime = endTime.minusMinutes(30)

        val response = getSensorData(
            deviceId = deviceId,
            startTime = startTime,
            endTime = endTime,
            page = 1,
            pageSize = 1
        )

        return response.data
    }

    suspend fun getAlertData(): List<SensorData> {
        val endTime = ZonedDateTime.now()
        val startTime = endTime.minusHours(24)

        val response = getSensorData(
            startTime = startTime,
            endTime = endTime,
            pageSize = 10
        )

        return response.data.filter { it.alarmStatus != "normal" }
    }

    suspend fun getHistoricalData(
        deviceId: String?,
        timeRange: TimeRange,
        skipPagination: Boolean = true,
        startTime: ZonedDateTime? = null,
        endTime: ZonedDateTime? = null
    ): List<SensorData> {
        val effectiveEndTime = endTime ?: ZonedDateTime.now()
        val effectiveStartTime = startTime ?: when(timeRange) {
            TimeRange.LAST_MINUTE -> effectiveEndTime.minusMinutes(1)
            TimeRange.LAST_5_MINUTES -> effectiveEndTime.minusMinutes(5)
            TimeRange.LAST_30_MINUTES -> effectiveEndTime.minusMinutes(30)
            TimeRange.LAST_HOUR -> effectiveEndTime.minusHours(1)
            TimeRange.LAST_6_HOURS -> effectiveEndTime.minusHours(6)
            TimeRange.LAST_12_HOURS -> effectiveEndTime.minusHours(12)
            TimeRange.LAST_DAY -> effectiveEndTime.minusDays(1)
            TimeRange.LAST_3_DAYS -> effectiveEndTime.minusDays(3)
        }

        val response = getSensorData(
            deviceId = deviceId,
            startTime = effectiveStartTime,
            endTime = effectiveEndTime,
            skipPagination = skipPagination,
            pageSize = getOptimalPageSize(timeRange)
        )

        return response.data
    }

    private fun getOptimalPageSize(timeRange: TimeRange): Int {
        return when(timeRange) {
            TimeRange.LAST_MINUTE -> 60
            TimeRange.LAST_5_MINUTES -> 100
            TimeRange.LAST_30_MINUTES -> 180
            TimeRange.LAST_HOUR -> 180
            TimeRange.LAST_6_HOURS -> 360
            TimeRange.LAST_12_HOURS -> 480
            TimeRange.LAST_DAY -> 576
            TimeRange.LAST_3_DAYS -> 864
        }
    }

    enum class TimeRange {
        LAST_MINUTE,
        LAST_5_MINUTES,
        LAST_30_MINUTES,
        LAST_HOUR,
        LAST_6_HOURS,
        LAST_12_HOURS,
        LAST_DAY,
        LAST_3_DAYS
    }
}