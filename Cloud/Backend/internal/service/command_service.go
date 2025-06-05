package service

import (
	"encoding/json"
	"fmt"
	"time"

	"github.com/arm-detector/backend/internal/models"
	"github.com/arm-detector/backend/internal/mqtt"
	"github.com/arm-detector/backend/internal/storage"
)

// CommandService 设备命令服务
type CommandService struct {
	repo        storage.Repository
	publisher   *mqtt.CommandPublisher
}

// NewCommandService 创建新的命令服务
func NewCommandService(repo storage.Repository, publisher *mqtt.CommandPublisher) *CommandService {
	return &CommandService{
		repo:      repo,
		publisher: publisher,
	}
}

// SendCommand 发送命令到设备
func (s *CommandService) SendCommand(deviceID string, cmdReq *models.CommandRequest) (*models.APICommandResponse, error) {
	// 发送命令到MQTT
	command, err := s.publisher.PublishCommand(deviceID, cmdReq)
	if err != nil {
		return nil, fmt.Errorf("发送命令失败: %w", err)
	}
	
	// 将命令记录到数据库
	paramsJSON, _ := json.Marshal(cmdReq.Parameters)
	commandLog := &models.CommandLog{
		DeviceID:  deviceID,
		Command:   cmdReq.Command,
		RequestID: command.RequestID,
		Status:    models.StatusPending,
		SentAt:    time.Now(),
		Response:  string(paramsJSON),
	}
	
	if err := s.repo.SaveCommandLog(commandLog); err != nil {
		return nil, fmt.Errorf("保存命令日志失败: %w", err)
	}
	
	// 构造API响应
	response := &models.APICommandResponse{
		Message:   "命令发送成功",
		RequestID: command.RequestID,
	}
	
	return response, nil
}

// GetCommandStatus 获取命令执行状态
func (s *CommandService) GetCommandStatus(requestID string) (*models.CommandLog, error) {
	log, err := s.repo.GetCommandLog(requestID)
	if err != nil {
		return nil, fmt.Errorf("获取命令日志失败: %w", err)
	}
	
	return log, nil
}

// ValidateCommand 验证命令是否支持
func (s *CommandService) ValidateCommand(command string) error {
	// 检查命令是否支持
	switch command {
	case models.CommandRenameDevice, models.CommandTakePhoto, models.CommandRestartDevice:
		return nil
	default:
		return fmt.Errorf("不支持的命令: %s", command)
	}
} 