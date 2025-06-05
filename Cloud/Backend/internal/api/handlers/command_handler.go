package handlers

import (
	"net/http"

	"github.com/arm-detector/backend/internal/models"
	"github.com/arm-detector/backend/internal/service"
	"github.com/gin-gonic/gin"
)

// CommandHandler 命令API处理器
type CommandHandler struct {
	commandService *service.CommandService
}

// NewCommandHandler 创建新的命令处理器
func NewCommandHandler(commandService *service.CommandService) *CommandHandler {
	return &CommandHandler{
		commandService: commandService,
	}
}

// SendCommand 发送命令到设备
func (h *CommandHandler) SendCommand(c *gin.Context) {
	// 获取设备ID
	deviceID := c.Param("device_id")
	if deviceID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "设备ID不能为空"})
		return
	}
	
	// 解析请求体
	var cmdReq models.CommandRequest
	if err := c.BindJSON(&cmdReq); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的请求格式"})
		return
	}
	
	// 验证命令
	if err := h.commandService.ValidateCommand(cmdReq.Command); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	
	// 发送命令
	response, err := h.commandService.SendCommand(deviceID, &cmdReq)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "发送命令失败: " + err.Error()})
		return
	}
	
	c.JSON(http.StatusAccepted, response)
}

// GetCommandStatus 获取命令执行状态
func (h *CommandHandler) GetCommandStatus(c *gin.Context) {
	// 获取请求ID
	requestID := c.Param("request_id")
	if requestID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "请求ID不能为空"})
		return
	}
	
	// 获取命令状态
	log, err := h.commandService.GetCommandStatus(requestID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "找不到指定命令"})
		return
	}
	
	c.JSON(http.StatusOK, log)
} 