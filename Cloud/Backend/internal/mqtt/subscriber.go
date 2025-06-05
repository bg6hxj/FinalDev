package mqtt

import (
	"encoding/json"
	"fmt"
	"log"
	"strconv"
	"time"

	"github.com/arm-detector/backend/internal/models"
	"github.com/arm-detector/backend/internal/storage"
	mqtt "github.com/eclipse/paho.mqtt.golang"
)

// SensorDataHandler 传感器数据处理器
type SensorDataHandler struct {
	client     *Client
	repo       storage.Repository
	sensorTopic string
	responseTopic string
}

// NewSensorDataHandler 创建新的传感器数据处理器
func NewSensorDataHandler(client *Client, repo storage.Repository, sensorTopic, responseTopic string) *SensorDataHandler {
	return &SensorDataHandler{
		client: client,
		repo: repo,
		sensorTopic: sensorTopic,
		responseTopic: responseTopic,
	}
}

// Start 开始监听传感器数据
func (h *SensorDataHandler) Start() error {
	// 订阅传感器数据主题
	err := h.client.Subscribe(h.sensorTopic, 1, h.handleSensorData)
	if err != nil {
		return fmt.Errorf("订阅传感器数据主题失败: %w", err)
	}
	
	// 订阅命令响应主题 (如果配置了)
	if h.responseTopic != "" {
		err = h.client.Subscribe(h.responseTopic, 1, h.handleCommandResponse)
		if err != nil {
			return fmt.Errorf("订阅命令响应主题失败: %w", err)
		}
	}
	
	return nil
}

// handleSensorData 处理接收到的传感器数据
func (h *SensorDataHandler) handleSensorData(client mqtt.Client, msg mqtt.Message) {
	log.Printf("收到传感器数据: %s, 主题: %s", string(msg.Payload()), msg.Topic())
	
	// 解析JSON数据
	var input models.SensorDataInput
	if err := json.Unmarshal(msg.Payload(), &input); err != nil {
		log.Printf("解析传感器数据失败: %v", err)
		return
	}
	
	// 转换时间戳
	var timestamp time.Time
	switch ts := input.Timestamp.(type) {
	case string:
		var err error
		timestamp, err = time.Parse(time.RFC3339, ts)
		if err != nil {
			log.Printf("解析RFC3339时间戳字符串 '%s' 失败: %v", ts, err)
			// 尝试解析是否为纯数字字符串代表的毫秒数
			ms, parseErr := strconv.ParseInt(ts, 10, 64)
			if parseErr == nil {
				timestamp = time.UnixMilli(ms).UTC() // 确保转换为UTC
				log.Printf("将时间戳数字字符串 '%s' 解析为毫秒时间: %v", ts, timestamp)
			} else {
				log.Printf("无法将时间戳字符串 '%s' 解析为时间，使用当前UTC时间", ts)
				timestamp = time.Now().UTC() // 使用当前UTC时间
			}
		} else {
			// 显式转换为UTC，防止时区信息丢失
			timestamp = timestamp.UTC()
			log.Printf("成功解析时间戳字符串为UTC时间: %v", timestamp)
		}
	case float64: // JSON 数字默认解析为 float64
		timestamp = time.UnixMilli(int64(ts)).UTC() // 确保转换为UTC
		log.Printf("将时间戳数字 %f 解析为毫秒UTC时间: %v", ts, timestamp)
	default:
		log.Printf("未知的时间戳格式 '%v' (%T)，使用当前UTC时间", input.Timestamp, input.Timestamp)
		timestamp = time.Now().UTC() // 使用当前UTC时间
	}
	
	// 创建SensorData对象
	sensorData := &models.SensorData{
		DeviceID:    input.DeviceID,
		Timestamp:   timestamp, // 此时已是UTC
		Temperature: input.Temperature,
		Humidity:    input.Humidity,
		CoPPM:       input.CoPPM,
		DustDensity: input.DustDensity,
		AlarmStatus: input.AlarmStatus,
		ReceivedAt:  time.Now().UTC(), // 确保使用UTC时间
	}
	
	// 保存到数据库
	if err := h.repo.SaveSensorData(sensorData); err != nil {
		log.Printf("保存传感器数据失败: %v", err)
		return
	}
	
	log.Printf("传感器数据已保存: 设备ID=%s, 温度=%.1f, 湿度=%.1f", 
		sensorData.DeviceID, sensorData.Temperature, sensorData.Humidity)
}

// handleCommandResponse 处理设备发送的命令响应
func (h *SensorDataHandler) handleCommandResponse(client mqtt.Client, msg mqtt.Message) {
	log.Printf("收到命令响应: %s, 主题: %s", string(msg.Payload()), msg.Topic())
	
	// 解析JSON数据
	var response models.CommandResponse
	if err := json.Unmarshal(msg.Payload(), &response); err != nil {
		log.Printf("解析命令响应失败: %v", err)
		return
	}
	
	// 更新命令状态
	responseJSON, _ := json.Marshal(response)
	if err := h.repo.UpdateCommandStatus(response.RequestID, response.Status, string(responseJSON)); err != nil {
		log.Printf("更新命令状态失败: %v", err)
		return
	}
	
	log.Printf("命令状态已更新: 请求ID=%s, 状态=%s", response.RequestID, response.Status)
} 