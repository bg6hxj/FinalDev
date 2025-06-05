package mqtt

import (
	"fmt"
	"log"
	"time"

	"github.com/arm-detector/backend/internal/config"
	mqtt "github.com/eclipse/paho.mqtt.golang"
)

// Client MQTT客户端
type Client struct {
	mqttClient mqtt.Client
	config     *config.MQTTConfig
}

// NewClient 创建新的MQTT客户端
func NewClient(cfg *config.MQTTConfig) (*Client, error) {
	// 创建MQTT客户端选项
	opts := mqtt.NewClientOptions()
	opts.AddBroker(cfg.BrokerURL)
	opts.SetClientID(cfg.ClientID)
	
	if cfg.Username != "" {
		opts.SetUsername(cfg.Username)
		opts.SetPassword(cfg.Password)
	}
	
	// 设置自动重连
	opts.SetAutoReconnect(true)
	opts.SetMaxReconnectInterval(1 * time.Minute)
	opts.SetKeepAlive(30 * time.Second)
	
	// 连接丢失回调
	opts.SetConnectionLostHandler(func(client mqtt.Client, err error) {
		log.Printf("MQTT连接丢失: %v", err)
	})
	
	// 重连成功回调
	opts.SetOnConnectHandler(func(client mqtt.Client) {
		log.Println("MQTT已连接")
	})
	
	// 创建客户端实例
	client := mqtt.NewClient(opts)
	
	// 连接MQTT服务器
	if token := client.Connect(); token.Wait() && token.Error() != nil {
		return nil, fmt.Errorf("连接MQTT服务器失败: %w", token.Error())
	}
	
	return &Client{
		mqttClient: client,
		config:     cfg,
	}, nil
}

// IsConnected 检查MQTT客户端是否已连接
func (c *Client) IsConnected() bool {
	return c.mqttClient.IsConnected()
}

// Subscribe 订阅主题
func (c *Client) Subscribe(topic string, qos byte, callback mqtt.MessageHandler) error {
	token := c.mqttClient.Subscribe(topic, qos, callback)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("订阅主题 %s 失败: %w", topic, token.Error())
	}
	
	log.Printf("已订阅主题: %s", topic)
	return nil
}

// Publish 发布消息到指定主题
func (c *Client) Publish(topic string, qos byte, retained bool, payload []byte) error {
	token := c.mqttClient.Publish(topic, qos, retained, payload)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("发布消息到 %s 失败: %w", topic, token.Error())
	}
	
	return nil
}

// Disconnect 断开MQTT连接
func (c *Client) Disconnect() {
	if c.mqttClient.IsConnected() {
		c.mqttClient.Disconnect(250) // 等待250ms完成正在进行的传输
	}
} 