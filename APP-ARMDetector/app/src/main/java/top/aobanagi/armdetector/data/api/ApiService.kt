package top.aobanagi.armdetector.data.api

import retrofit2.http.*
import top.aobanagi.armdetector.data.model.*

interface ApiService {
    @GET("api/v1/sensor")// 获取传感器数据
    suspend fun getSensorData(
        @Query("device_id") deviceId: String? = null,
        @Query("start_time") startTime: String? = null,
        @Query("end_time") endTime: String? = null,
        @Query("page") page: Int = 1,
        @Query("page_size") pageSize: Int = 20,
        @Query("skip_pagination") skipPagination: Boolean = false
    ): SensorDataResponse

    @GET("api/v1/sensor/{id}")// 获取单条传感器数据
    suspend fun getSensorDataById(@Path("id") id: Long): SensorData

    @POST("api/v1/device/{deviceId}/command")// 发送命令到设备
    suspend fun sendCommand(
        @Path("deviceId") deviceId: String,
        @Body command: Command
    ): CommandResponse

    @GET("api/v1/command/{requestId}")// 查询命令执行状态
    suspend fun getCommandStatus(@Path("requestId") requestId: String): CommandStatus
}