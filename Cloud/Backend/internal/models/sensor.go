package models

import (
	"time"
)

// SensorData 表示从ESP32接收到的传感器数据
type SensorData struct {
	ID          int64     `json:"id" db:"id"`
	DeviceID    string    `json:"device_id" db:"device_id"`
	Timestamp   time.Time `json:"timestamp" db:"timestamp"`
	Temperature float64   `json:"temperature" db:"temperature"`
	Humidity    float64   `json:"humidity" db:"humidity"`
	CoPPM       float64   `json:"co_ppm" db:"co_ppm"`
	DustDensity float64   `json:"dust_density" db:"dust_density"`
	AlarmStatus string    `json:"alarm_status" db:"alarm_status"`
	ReceivedAt  time.Time `json:"received_at" db:"received_at"`
}

// SensorDataInput 表示来自MQTT的原始传感器数据
type SensorDataInput struct {
	DeviceID    string    `json:"device_id"`
	Timestamp   interface{} `json:"timestamp"`
	Temperature float64   `json:"temperature"`
	Humidity    float64   `json:"humidity"`
	CoPPM       float64   `json:"co_ppm"`
	DustDensity float64   `json:"dust_density"`
	AlarmStatus string    `json:"alarm_status"`
}

// SensorDataQuery 包含查询传感器数据的参数
type SensorDataQuery struct {
	DeviceID       string    `form:"device_id"`
	StartTime      time.Time `form:"start_time"`
	EndTime        time.Time `form:"end_time"`
	Page           int       `form:"page,default=1"`
	PageSize       int       `form:"page_size,default=20"`
	SkipPagination bool      `form:"skip_pagination"` // 是否跳过分页限制
}

// PaginationResult 包含分页信息
type PaginationResult struct {
	CurrentPage int   `json:"current_page"`
	PageSize    int   `json:"page_size"`
	TotalItems  int64 `json:"total_items"`
	TotalPages  int   `json:"total_pages"`
}

// SensorDataResponse 传感器数据的API响应格式
type SensorDataResponse struct {
	Data       []SensorData     `json:"data"`
	Pagination PaginationResult `json:"pagination"`
} 