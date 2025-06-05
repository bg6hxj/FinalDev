package handlers

import (
	"net/http"
	"strconv"
	"time"

	"github.com/arm-detector/backend/internal/models"
	"github.com/arm-detector/backend/internal/service"
	"github.com/gin-gonic/gin"
)

// SensorHandler 传感器数据API处理器
type SensorHandler struct {
	sensorService *service.SensorService
}

// NewSensorHandler 创建新的传感器数据处理器
func NewSensorHandler(sensorService *service.SensorService) *SensorHandler {
	return &SensorHandler{
		sensorService: sensorService,
	}
}

// GetSensorDataList 获取传感器数据列表
func (h *SensorHandler) GetSensorDataList(c *gin.Context) {
	// 解析查询参数
	var query models.SensorDataQuery
	
	// 设备ID
	if deviceID := c.Query("device_id"); deviceID != "" {
		query.DeviceID = deviceID
	}
	
	// 开始时间
	if startTimeStr := c.Query("start_time"); startTimeStr != "" {
		startTime, err := time.Parse(time.RFC3339, startTimeStr)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "无效的开始时间格式，请使用ISO8601格式"})
			return
		}
		query.StartTime = startTime.UTC() // 确保转换为UTC
	} else {
		// 默认为过去30分钟
		query.StartTime = time.Now().Add(-30 * time.Minute).UTC() // 使用UTC
	}
	
	// 结束时间
	if endTimeStr := c.Query("end_time"); endTimeStr != "" {
		endTime, err := time.Parse(time.RFC3339, endTimeStr)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "无效的结束时间格式，请使用ISO8601格式"})
			return
		}
		query.EndTime = endTime.UTC() // 确保转换为UTC
	} else {
		// 默认为当前时间
		query.EndTime = time.Now().UTC() // 使用UTC
	}
	
	// 页码
	if pageStr := c.Query("page"); pageStr != "" {
		page, err := strconv.Atoi(pageStr)
		if err != nil || page < 1 {
			c.JSON(http.StatusBadRequest, gin.H{"error": "无效的页码"})
			return
		}
		query.Page = page
	} else {
		query.Page = 1 // 默认页码
	}
	
	// 每页数量
	if pageSizeStr := c.Query("page_size"); pageSizeStr != "" {
		pageSize, err := strconv.Atoi(pageSizeStr)
		if err != nil || pageSize < 1 {
			c.JSON(http.StatusBadRequest, gin.H{"error": "无效的每页数量"})
			return
		}
		query.PageSize = pageSize
	} else {
		query.PageSize = 20 // 默认每页数量
	}
	
	// 是否跳过分页限制
	if skipPaginationStr := c.Query("skip_pagination"); skipPaginationStr == "true" {
		query.SkipPagination = true
	}
	
	// 查询数据
	response, err := h.sensorService.QuerySensorData(&query)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "查询传感器数据失败"})
		return
	}
	
	c.JSON(http.StatusOK, response)
}

// GetSensorData 获取单条传感器数据
func (h *SensorHandler) GetSensorData(c *gin.Context) {
	// 解析ID参数
	idStr := c.Param("id")
	id, err := strconv.ParseInt(idStr, 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的ID"})
		return
	}
	
	// 获取数据
	data, err := h.sensorService.GetSensorData(id)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "传感器数据不存在"})
		return
	}
	
	c.JSON(http.StatusOK, data)
} 