#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>  // MQTT客户端库
#include <ArduinoJson.h>   // JSON处理库
#include <Preferences.h>   // 持久存储库
#include <time.h>          // 时间相关函数
#include <HTTPClient.h>     // HTTP客户端库
#include "globals.h"       // 全局变量和函数声明

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE  // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_CAMS3_UNIT  // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "蹭网者亡";
const char *password = "LuLiZhaoHuang214";
// const char *ssid = "10111111";
// const char *password = "abababa1234562";

// ===========================
// MQTT服务器设置
// ===========================
// MQTT配置变量，使其成为全局
const char* mqttServer = "113.44.13.140";  // 默认值，也用作WebSocket服务器地址
int mqttPort = 1883;                       // 默认值
const char* mqttUser = "";                 // 默认值
const char* mqttPassword = "";             // 默认值
String mqttClientId = "ESP32Camera";       // MQTT客户端ID，将在setup中更新
const char* mqttTopic = "armdetector/sensor/data"; // 默认值

// 相机相关变量
bool camera_enabled = true;              // 相机是否启用，为全局变量

// ===========================
// STM32串口通信设置
// ===========================
#define STM32_RX 13  // ESP32 GPIO13 连接到 STM32 TX
#define STM32_TX 15  // ESP32 GPIO15 连接到 STM32 RX
#define STM32_BAUD 115200  // STM32 串口波特率

// 状态指示灯
int STATUS_LED = 33;  // 状态指示灯引脚

// 全局变量
WiFiClient espClient;
PubSubClient mqttClient(espClient);
HardwareSerial stm32Serial(1); // 使用UART1与STM32通信
Preferences preferences;       // 持久化存储 (新代码，实际定义对象)

// 数据解析缓冲区
String dataBuffer = "";
unsigned long lastDataTime = 0;
unsigned long lastConnectAttempt = 0;
const int connectInterval = 5000; // 重连间隔5秒

// 函数声明
void startCameraServer();
void setupLedFlash(int pin);
void connectToMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void processSTM32Data(String data);
String getDeviceId();  // 新增：获取设备ID的函数声明
void handleTakePhotoCommand();  // 新增：拍照命令处理函数

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // 初始化状态指示灯
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // 初始化STM32串口
  stm32Serial.begin(STM32_BAUD, SERIAL_8N1, STM32_RX, STM32_TX);
  
  // 生成或从存储中读取设备ID
  mqttClientId = getDeviceId();
  Serial.print("设备ID: ");
  Serial.println(mqttClientId);
  
  // 初始化Preferences
  preferences.begin("mqtt_config", false);
  
  // 从Preferences加载MQTT配置
  String savedServer = preferences.getString("server", "");
  if (savedServer.length() > 0) {
    mqttServer = strdup(savedServer.c_str());
    mqttPort = preferences.getInt("port", 1883);
    
    String savedUser = preferences.getString("user", "");
    if (savedUser.length() > 0) {
      mqttUser = strdup(savedUser.c_str());
    }
    
    String savedPassword = preferences.getString("password", "");
    if (savedPassword.length() > 0) {
      mqttPassword = strdup(savedPassword.c_str());
    }
    
    String savedTopic = preferences.getString("topic", "");
    if (savedTopic.length() > 0) {
      mqttTopic = strdup(savedTopic.c_str());
    }
    
    Serial.println("已从存储加载MQTT配置:");
    Serial.print("服务器: ");
    Serial.println(mqttServer);
    Serial.print("端口: ");
    Serial.println(mqttPort);
    Serial.print("主题: ");
    Serial.println(mqttTopic);
  } else {
    Serial.println("使用默认MQTT配置");
  }
  
  // 关闭Preferences
  preferences.end();

  // 检查重启标志
  preferences.begin("system", false);
  bool shouldRestart = preferences.getBool("restart_flag", false);
  if (shouldRestart) {
    // 清除重启标志
    preferences.putBool("restart_flag", false);
    preferences.end();
    
    // 延迟2秒后重启
    Serial.println("设置已更新，设备将在2秒后重启...");
    delay(2000);
    ESP.restart();
    return;
  }
  preferences.end();

  // 检查相机是否启用
  preferences.begin("camera", true);
  camera_enabled = preferences.getBool("enabled", true);
  preferences.end();
  Serial.println(camera_enabled ? "相机已启用" : "相机已禁用");

  // 相机初始化
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV5640_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); // 闪烁指示灯
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  // LED常亮表示WiFi连接成功
  digitalWrite(STATUS_LED, HIGH);
  
  // 设置MQTT服务器
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  
  // 连接MQTT服务器
  connectToMQTT();

  // 配置NTP服务器，同步时间
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("正在同步时间...");
  
  // 等待获取时间
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    Serial.println("时间同步成功");
    Serial.print("当前时间: ");
    Serial.print(timeinfo.tm_year + 1900);
    Serial.print("-");
    Serial.print(timeinfo.tm_mon + 1);
    Serial.print("-");
    Serial.print(timeinfo.tm_mday);
    Serial.print(" ");
    Serial.print(timeinfo.tm_hour);
    Serial.print(":");
    Serial.print(timeinfo.tm_min);
    Serial.print(":");
    Serial.println(timeinfo.tm_sec);
  } else {
    Serial.println("无法获取时间，使用设备启动时间");
  }

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  // 检查WiFi连接
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi连接丢失，尝试重连...");
    WiFi.begin(ssid, password);
    digitalWrite(STATUS_LED, LOW); // 关闭指示灯表示连接断开
  }
  
  // 检查MQTT连接
  if (!mqttClient.connected()) {
    // 尝试重新连接MQTT，但不要太频繁
    unsigned long now = millis();
    if (now - lastConnectAttempt > connectInterval) {
      lastConnectAttempt = now;
      connectToMQTT();
    }
  } else {
    // MQTT保持连接
    mqttClient.loop();
  }
  
  // 从STM32读取数据
  while (stm32Serial.available()) {
    char c = stm32Serial.read();
    
    // 找到数据帧结束符
    if (c == '\n') {
      // 如果有完整的数据帧，进行处理
      if (dataBuffer.length() > 0) {
        // 处理数据帧
        processSTM32Data(dataBuffer);
        dataBuffer = ""; // 清空缓冲区
      }
    } else if (c != '\r') { // 忽略回车符
      dataBuffer += c;
    }
  }
  
  // 相机网络服务器在另一个任务中运行
  delay(10);
}

// 连接到MQTT服务器
void connectToMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法连接MQTT");
    return;
  }
  
  Serial.println("正在连接MQTT服务器...");
  
  // 尝试连接
  if (mqttUser != "" && mqttPassword != "") {
    if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("MQTT连接成功");
      digitalWrite(STATUS_LED, HIGH); // 连接成功，LED常亮
      
      // 订阅命令主题
      String commandTopic = "armdetector/device/" + mqttClientId + "/command";
      mqttClient.subscribe(commandTopic.c_str());
      Serial.println("已订阅命令主题: " + commandTopic);
    } else {
      Serial.print("MQTT连接失败，状态码: ");
      Serial.println(mqttClient.state());
      digitalWrite(STATUS_LED, LOW); // 连接失败，LED熄灭
    }
  } else {
    if (mqttClient.connect(mqttClientId.c_str())) {
      Serial.println("MQTT连接成功");
      digitalWrite(STATUS_LED, HIGH); // 连接成功，LED常亮
      
      // 订阅命令主题
      String commandTopic = "armdetector/device/" + mqttClientId + "/command";
      mqttClient.subscribe(commandTopic.c_str());
      Serial.println("已订阅命令主题: " + commandTopic);
    } else {
      Serial.print("MQTT连接失败，状态码: ");
      Serial.println(mqttClient.state());
      digitalWrite(STATUS_LED, LOW); // 连接失败，LED熄灭
    }
  }
}

// MQTT消息回调函数
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("收到消息 [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // 解析命令JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("命令解析失败: ");
    Serial.println(error.c_str());
    return;
  }
  
  // 获取命令参数
  String command = doc["command"].as<String>();
  String requestId = doc["request_id"].as<String>();
  
  Serial.print("命令类型: ");
  Serial.println(command);
  
  // 命令响应主题
  String responseTopic = "armdetector/device/" + mqttClientId + "/response";
  
  // 创建响应JSON
  StaticJsonDocument<256> response;
  response["request_id"] = requestId;
  response["timestamp"] = doc["timestamp"].as<String>();
  
  // 处理不同类型的命令
  if (command == "restart_device") {
    // 处理重启命令
    Serial.println("收到重启命令，设备将在3秒后重启...");
    
    // 发送响应
    response["status"] = "success";
    response["message"] = "设备正在重启";
    
    String responseStr;
    serializeJson(response, responseStr);
    mqttClient.publish(responseTopic.c_str(), responseStr.c_str());
    
    // 短暂延迟，确保消息发送出去
    delay(1000);
    
    // 重启设备
    ESP.restart();
  } 
  else if (command == "rename_device") {
    // 处理重命名命令
    if (doc["parameters"].containsKey("name")) {
      String newName = doc["parameters"]["name"].as<String>();
      Serial.print("重命名设备为: ");
      Serial.println(newName);
      
      // 保存新名称到Preferences
      preferences.begin("device_info", false);
      preferences.putString("name", newName);
      preferences.end();
      
      // 发送响应
      response["status"] = "success";
      response["message"] = "设备已重命名";
      response["data"]["name"] = newName;
    } else {
      response["status"] = "error";
      response["message"] = "缺少名称参数";
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    mqttClient.publish(responseTopic.c_str(), responseStr.c_str());
  }
  else if (command == "take_photo") {
    // 处理拍照命令
    if (camera_enabled) {
      Serial.println("执行拍照命令");
      handleTakePhotoCommand();
      
      response["status"] = "success";
      response["message"] = "拍照完成";
    } else {
      response["status"] = "error";
      response["message"] = "相机未启用";
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    mqttClient.publish(responseTopic.c_str(), responseStr.c_str());
  }
  else {
    // 未知命令
    Serial.print("未知命令: ");
    Serial.println(command);
    
    response["status"] = "error";
    response["message"] = "不支持的命令类型";
    
    String responseStr;
    serializeJson(response, responseStr);
    mqttClient.publish(responseTopic.c_str(), responseStr.c_str());
  }
}

// 处理从STM32接收的数据
void processSTM32Data(String data) {
  Serial.print("收到STM32数据: ");
  Serial.println(data);
  
  // 记录收到数据的时间
  lastDataTime = millis();
  
  // 解析数据帧格式: "T:[temperature],H:[humidity],CO:[co_ppm],DUST:[dust_density],ALARM:[alarm_status]\r\n"
  // 例如: "T:25,H:60,CO:15.2,DUST:30.5,ALARM:None\r\n"
  
  // 创建JSON文档
  StaticJsonDocument<256> jsonDoc;
  
  // 格式化时间戳为ISO 8601格式
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // 如果获取时间失败，使用毫秒时间戳
    jsonDoc["timestamp"] = millis();
  } else {
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S+08:00", &timeinfo);
    jsonDoc["timestamp"] = timeStr;
  }
  
  // 提取各个参数
  int tIndex = data.indexOf("T:");
  int hIndex = data.indexOf(",H:");
  int coIndex = data.indexOf(",CO:");
  int dustIndex = data.indexOf(",DUST:");
  int alarmIndex = data.indexOf(",ALARM:");
  
  if (tIndex >= 0 && hIndex >= 0 && coIndex >= 0 && dustIndex >= 0 && alarmIndex >= 0) {
    // 提取温度
    String tempStr = data.substring(tIndex + 2, hIndex);
    jsonDoc["temperature"] = tempStr.toInt();
    
    // 提取湿度
    String humidStr = data.substring(hIndex + 3, coIndex);
    jsonDoc["humidity"] = humidStr.toInt();
    
    // 提取CO浓度
    String coStr = data.substring(coIndex + 4, dustIndex);
    jsonDoc["co_ppm"] = coStr.toFloat();
    
    // 提取粉尘浓度
    String dustStr = data.substring(dustIndex + 6, alarmIndex);
    jsonDoc["dust_density"] = dustStr.toFloat();
    
    // 提取报警状态
    String alarmStr = data.substring(alarmIndex + 7);
    jsonDoc["alarm_status"] = alarmStr;
    
    // 添加设备ID（确保格式一致）
    jsonDoc["device_id"] = mqttClientId;
    
    // 将JSON转换为字符串
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    
    // 通过MQTT发送
    if (mqttClient.connected()) {
      Serial.print("发送MQTT数据: ");
      Serial.println(jsonString);
      Serial.print("使用主题: ");
      Serial.println(mqttTopic);
      
      if (mqttClient.publish(mqttTopic, jsonString.c_str())) {
        Serial.println("MQTT消息发送成功");
        // 短闪烁指示灯表示数据发送成功
        digitalWrite(STATUS_LED, LOW);
        delay(100);
        digitalWrite(STATUS_LED, HIGH);
      } else {
        Serial.println("MQTT消息发送失败");
      }
    } else {
      Serial.println("MQTT未连接，无法发送数据");
    }
  } else {
    Serial.println("数据格式错误");
  }
}

// 获取设备ID，格式为"ESP32-xxxx"，xxxx为MAC地址的后四位
String getDeviceId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  // 将MAC地址最后两个字节（四位十六进制）格式化为字符串
  char macStr[9];
  snprintf(macStr, sizeof(macStr), "%02X%02X", mac[4], mac[5]);
  
  // 构造设备ID
  String deviceId = "ESP32-" + String(macStr);
  
  return deviceId;
}

// 拍照函数声明 - 实现在app_httpd.cpp中
void handleTakePhotoCommand();
