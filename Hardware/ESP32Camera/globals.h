#ifndef GLOBALS_H
#define GLOBALS_H

#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#define CONFIG_HTTPD_MAX_REQ_HDR_LEN 4096 // 尝试设置为2048字节
// 或尝试 #undef CONFIG_HTTPD_MAX_REQ_HDR_LEN
// #define CONFIG_HTTPD_MAX_REQ_HDR_LEN 2048
#include <WebServer.h> // 或您使用的其他Web服务器库

// 全局变量声明
extern Preferences preferences;  // 用于保存配置
extern String mqttClientId;      // MQTT客户端ID
extern String deviceName;        // 设备名称
extern int STATUS_LED;           // 状态LED引脚
extern bool camera_enabled;      // 相机是否启用

// 函数声明
void mqttCallback(char* topic, byte* payload, unsigned int length);
void connectToMQTT();
void processSTM32Data(String data);
String getDeviceId();
void handleTakePhotoCommand();
void handleCommand(String payload);
void saveDeviceName(String name);

#endif // GLOBALS_H