#include "globals.h"

// 全局变量声明，这些变量在ESP32Camera.ino中定义
// 避免重复定义导致链接错误
extern bool camera_enabled;
extern int STATUS_LED;

// 注意: 这些变量在ESP32Camera.ino中定义
// Preferences preferences; 