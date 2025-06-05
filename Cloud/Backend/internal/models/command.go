package models

import (
	"encoding/json"
	"time"
)

// CommandRequest 表示从API接收到的控制命令请求
type CommandRequest struct {
	Command    string          `json:"command" binding:"required"`
	Parameters json.RawMessage `json:"parameters"`
}

// DeviceCommand 表示将通过MQTT发送到设备的命令
type DeviceCommand struct {
	Command    string          `json:"command"`
	Parameters json.RawMessage `json:"parameters,omitempty"`
	RequestID  string          `json:"request_id"`
	Timestamp  string          `json:"timestamp"`
}

// CommandResponse 表示设备发送回来的命令执行响应
type CommandResponse struct {
	RequestID string          `json:"request_id"`
	Status    string          `json:"status"`
	Message   string          `json:"message,omitempty"`
	Data      json.RawMessage `json:"data,omitempty"`
	Timestamp string          `json:"timestamp"`
}

// CommandLog 记录命令执行的日志
type CommandLog struct {
	ID        int64     `json:"id" db:"id"`
	DeviceID  string    `json:"device_id" db:"device_id"`
	Command   string    `json:"command" db:"command"`
	RequestID string    `json:"request_id" db:"request_id"`
	Status    string    `json:"status" db:"status"`
	SentAt    time.Time `json:"sent_at" db:"sent_at"`
	Response  string    `json:"response,omitempty" db:"response"`
}

// APICommandResponse 表示API返回给前端的命令响应
type APICommandResponse struct {
	Message   string `json:"message"`
	RequestID string `json:"request_id"`
}

// 定义已知命令类型的常量
const (
	CommandRenameDevice = "rename_device"
	CommandTakePhoto    = "take_photo"
	CommandRestartDevice = "restart_device"
)

// 定义命令状态常量
const (
	StatusSuccess = "success"
	StatusError   = "error"
	StatusPending = "pending"
) 