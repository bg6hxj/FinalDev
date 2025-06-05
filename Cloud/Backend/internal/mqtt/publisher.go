package mqtt

import (
	"encoding/json"
	"fmt"
	"time"

	"github.com/arm-detector/backend/internal/models"
	"github.com/google/uuid"
)

// CommandPublisher 命令发布器
type CommandPublisher struct {
	client                *Client
	commandTopicTemplate  string
}

// NewCommandPublisher 创建新的命令发布器
func NewCommandPublisher(client *Client, commandTopicTemplate string) *CommandPublisher {
	return &CommandPublisher{
		client:               client,
		commandTopicTemplate: commandTopicTemplate,
	}
}

// PublishCommand 发布命令到指定设备
func (p *CommandPublisher) PublishCommand(deviceID string, cmdReq *models.CommandRequest) (*models.DeviceCommand, error) {
	// 检查MQTT客户端连接状态
	if !p.client.IsConnected() {
		return nil, fmt.Errorf("MQTT客户端未连接")
	}
	
	// 生成请求ID
	requestID := uuid.New().String()
	
	// 创建命令对象
	command := &models.DeviceCommand{
		Command:    cmdReq.Command,
		Parameters: cmdReq.Parameters,
		RequestID:  requestID,
		Timestamp:  time.Now().UTC().Format(time.RFC3339),
	}
	
	// 序列化为JSON
	payload, err := json.Marshal(command)
	if err != nil {
		return nil, fmt.Errorf("序列化命令失败: %w", err)
	}
	
	// 构建主题
	topic := fmt.Sprintf(p.commandTopicTemplate, deviceID)
	
	// 发布消息
	if err := p.client.Publish(topic, 1, false, payload); err != nil {
		return nil, fmt.Errorf("发布命令失败: %w", err)
	}
	
	return command, nil
} 