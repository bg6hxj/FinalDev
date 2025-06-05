// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include "camera_index.h"
#include <Preferences.h>
#include <WiFi.h>           // 添加WiFi库头文件
#include <PubSubClient.h>   // 添加PubSubClient库头文件
#include <HTTPClient.h>     // 添加HTTP客户端库头文件
#include "globals.h"  // 包含全局变量头文件



#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif

// Enable LED FLASH setting
#define CONFIG_LED_ILLUMINATOR_ENABLED 1

// LED FLASH setup
#if CONFIG_LED_ILLUMINATOR_ENABLED

#define LED_LEDC_GPIO            22  //configure LED pin
#define CONFIG_LED_MAX_INTENSITY 255

int led_duty = 0;
bool isStreaming = false;

#endif

typedef struct {
  httpd_req_t *req;
  size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

typedef struct {
  size_t size;   //number of values used for filtering
  size_t index;  //current value index
  size_t count;  //value count
  int sum;
  int *values;  //array to be filled with values
} ra_filter_t;

static ra_filter_t ra_filter;

static ra_filter_t *ra_filter_init(ra_filter_t *filter, size_t sample_size) {
  memset(filter, 0, sizeof(ra_filter_t));

  filter->values = (int *)malloc(sample_size * sizeof(int));
  if (!filter->values) {
    return NULL;
  }
  memset(filter->values, 0, sample_size * sizeof(int));

  filter->size = sample_size;
  return filter;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
static int ra_filter_run(ra_filter_t *filter, int value) {
  if (!filter->values) {
    return value;
  }
  filter->sum -= filter->values[filter->index];
  filter->values[filter->index] = value;
  filter->sum += filter->values[filter->index];
  filter->index++;
  filter->index = filter->index % filter->size;
  if (filter->count < filter->size) {
    filter->count++;
  }
  return filter->sum / filter->count;
}
#endif

#if CONFIG_LED_ILLUMINATOR_ENABLED
void enable_led(bool en) {  // Turn LED On or Off
  int duty = en ? led_duty : 0;
  if (en && isStreaming && (led_duty > CONFIG_LED_MAX_INTENSITY)) {
    duty = CONFIG_LED_MAX_INTENSITY;
  }
  ledcWrite(LED_LEDC_GPIO, duty);
  //ledc_set_duty(CONFIG_LED_LEDC_SPEED_MODE, CONFIG_LED_LEDC_CHANNEL, duty);
  //ledc_update_duty(CONFIG_LED_LEDC_SPEED_MODE, CONFIG_LED_LEDC_CHANNEL);
  log_i("Set LED intensity to %d", duty);
}
#endif

static esp_err_t bmp_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  uint64_t fr_start = esp_timer_get_time();
#endif
  fb = esp_camera_fb_get();
  if (!fb) {
    log_e("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/x-windows-bmp");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char ts[32];
  snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
  httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

  uint8_t *buf = NULL;
  size_t buf_len = 0;
  bool converted = frame2bmp(fb, &buf, &buf_len);
  esp_camera_fb_return(fb);
  if (!converted) {
    log_e("BMP Conversion failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  res = httpd_resp_send(req, (const char *)buf, buf_len);
  free(buf);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  uint64_t fr_end = esp_timer_get_time();
#endif
  log_i("BMP: %llums, %uB", (uint64_t)((fr_end - fr_start) / 1000), buf_len);
  return res;
}

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len) {
  jpg_chunking_t *j = (jpg_chunking_t *)arg;
  if (!index) {
    j->len = 0;
  }
  if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK) {
    return 0;
  }
  j->len += len;
  return len;
}

static esp_err_t capture_handler(httpd_req_t *req) {
  // 检查相机是否已禁用
  if (!camera_enabled) {
    // 如果相机已禁用，返回适当的错误信息
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"error\":\"Camera is disabled\", \"message\":\"The camera function has been disabled. Please enable it in settings and restart the device.\"}");
  }

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  int64_t fr_start = esp_timer_get_time();
#endif

#if CONFIG_LED_ILLUMINATOR_ENABLED
  enable_led(true);
  vTaskDelay(150 / portTICK_PERIOD_MS);  // The LED needs to be turned on ~150ms before the call to esp_camera_fb_get()
  fb = esp_camera_fb_get();              // or it won't be visible in the frame. A better way to do this is needed.
  enable_led(false);
#else
  fb = esp_camera_fb_get();
#endif

  if (!fb) {
    log_e("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char ts[32];
  snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
  httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  size_t fb_len = 0;
#endif
  if (fb->format == PIXFORMAT_JPEG) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    fb_len = fb->len;
#endif
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  } else {
    jpg_chunking_t jchunk = {req, 0};
    res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
    httpd_resp_send_chunk(req, NULL, 0);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    fb_len = jchunk.len;
#endif
  }
  esp_camera_fb_return(fb);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  int64_t fr_end = esp_timer_get_time();
#endif
  log_i("JPG: %uB %ums", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000));
  return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  // 检查相机是否已禁用
  if (!camera_enabled) {
    // 如果相机已禁用，返回适当的错误信息
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"error\":\"Camera is disabled\", \"message\":\"The camera function has been disabled. Please enable it in settings and restart the device.\"}");
  }

  camera_fb_t *fb = NULL;
  struct timeval _timestamp;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[128];

  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

#if CONFIG_LED_ILLUMINATOR_ENABLED
  isStreaming = true;
  enable_led(true);
#endif

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      log_e("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _timestamp.tv_sec = fb->timestamp.tv_sec;
      _timestamp.tv_usec = fb->timestamp.tv_usec;
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          log_e("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      log_e("Send frame failed");
      break;
    }
    int64_t fr_end = esp_timer_get_time();

    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;

    frame_time /= 1000;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
#endif
    log_i(
      "MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)", (uint32_t)(_jpg_buf_len), (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time, avg_frame_time,
      1000.0 / avg_frame_time
    );
  }

#if CONFIG_LED_ILLUMINATOR_ENABLED
  isStreaming = false;
  enable_led(false);
#endif

  return res;
}

static esp_err_t parse_get(httpd_req_t *req, char **obuf) {
  char *buf = NULL;
  size_t buf_len = 0;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      *obuf = buf;
      return ESP_OK;
    }
    free(buf);
  }
  httpd_resp_send_404(req);
  return ESP_FAIL;
}

static esp_err_t cmd_handler(httpd_req_t *req) {
  char *buf = NULL;
  char variable[32];
  char value[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK || httpd_query_key_value(buf, "val", value, sizeof(value)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int val = atoi(value);
  log_i("%s = %d", variable, val);
  sensor_t *s = esp_camera_sensor_get();
  int res = 0;

  if (!strcmp(variable, "framesize")) {
    if (s->pixformat == PIXFORMAT_JPEG) {
      res = s->set_framesize(s, (framesize_t)val);
    }
  } else if (!strcmp(variable, "quality")) {
    res = s->set_quality(s, val);
  } else if (!strcmp(variable, "contrast")) {
    res = s->set_contrast(s, val);
  } else if (!strcmp(variable, "brightness")) {
    res = s->set_brightness(s, val);
  } else if (!strcmp(variable, "saturation")) {
    res = s->set_saturation(s, val);
  } else if (!strcmp(variable, "gainceiling")) {
    res = s->set_gainceiling(s, (gainceiling_t)val);
  } else if (!strcmp(variable, "colorbar")) {
    res = s->set_colorbar(s, val);
  } else if (!strcmp(variable, "awb")) {
    res = s->set_whitebal(s, val);
  } else if (!strcmp(variable, "agc")) {
    res = s->set_gain_ctrl(s, val);
  } else if (!strcmp(variable, "aec")) {
    res = s->set_exposure_ctrl(s, val);
  } else if (!strcmp(variable, "hmirror")) {
    res = s->set_hmirror(s, val);
  } else if (!strcmp(variable, "vflip")) {
    res = s->set_vflip(s, val);
  } else if (!strcmp(variable, "awb_gain")) {
    res = s->set_awb_gain(s, val);
  } else if (!strcmp(variable, "agc_gain")) {
    res = s->set_agc_gain(s, val);
  } else if (!strcmp(variable, "aec_value")) {
    res = s->set_aec_value(s, val);
  } else if (!strcmp(variable, "aec2")) {
    res = s->set_aec2(s, val);
  } else if (!strcmp(variable, "dcw")) {
    res = s->set_dcw(s, val);
  } else if (!strcmp(variable, "bpc")) {
    res = s->set_bpc(s, val);
  } else if (!strcmp(variable, "wpc")) {
    res = s->set_wpc(s, val);
  } else if (!strcmp(variable, "raw_gma")) {
    res = s->set_raw_gma(s, val);
  } else if (!strcmp(variable, "lenc")) {
    res = s->set_lenc(s, val);
  } else if (!strcmp(variable, "special_effect")) {
    res = s->set_special_effect(s, val);
  } else if (!strcmp(variable, "wb_mode")) {
    res = s->set_wb_mode(s, val);
  } else if (!strcmp(variable, "ae_level")) {
    res = s->set_ae_level(s, val);
  }
#if CONFIG_LED_ILLUMINATOR_ENABLED
  else if (!strcmp(variable, "led_intensity")) {
    led_duty = val;
    if (isStreaming) {
      enable_led(true);
    }
  }
#endif
  // 添加对启用和禁用相机的命令处理
  else if (!strcmp(variable, "enable_camera")) {
    // 保存相机启用设置到preferences
    preferences.putBool("camera_en", true);
    log_i("Camera will be enabled on next restart");
    // 返回JSON响应
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"status\":\"success\",\"msg\":\"Camera will be enabled after restart\"}");
  }
  else if (!strcmp(variable, "disable_camera")) {
    // 保存相机禁用设置到preferences
    preferences.putBool("camera_en", false);
    log_i("Camera will be disabled on next restart");
    // 返回JSON响应
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"status\":\"success\",\"msg\":\"Camera will be disabled after restart\"}");
  }
  else if (!strcmp(variable, "restart")) {
    // 重启ESP32
    log_i("Restarting system...");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"status\":\"success\",\"msg\":\"Restarting system...\"}");
    // 稍等片刻让响应发送完成
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
  }
  else {
    log_i("Unknown command: %s", variable);
    res = -1;
  }

  if (res < 0) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static int print_reg(char *p, sensor_t *s, uint16_t reg, uint32_t mask) {
  return sprintf(p, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));
}

static esp_err_t status_handler(httpd_req_t *req) {
  static char json_response[1024];

  sensor_t *s = esp_camera_sensor_get();
  char *p = json_response;
  *p++ = '{';

  // 添加相机状态信息
  p += sprintf(p, "\"camera_enabled\":%s,", camera_enabled ? "true" : "false");

  // 只有当相机启用时才添加相机详细信息
  if (camera_enabled && s) {
    p += sprintf(p, "\"framesize\":%u,\"quality\":%u,\"brightness\":%d,\"contrast\":%d,\"saturation\":%d,\"sharpness\":%d,\"special_effect\":%u,\"wb_mode\":%u,\"awb\":%u,\"awb_gain\":%u,\"vflip\":%u,\"hmirror\":%u,\"aec\":%u,\"aec2\":%u,\"ae_level\":%d,\"aec_value\":%u,\"agc\":%u,\"agc_gain\":%u,\"gainceiling\":%u,\"bpc\":%u,\"wpc\":%u,\"raw_gma\":%u,\"lenc\":%u,\"hmirror\":%u,\"dcw\":%u,\"colorbar\":%u",
           s->status.framesize, s->status.quality,
           s->status.brightness, s->status.contrast,
           s->status.saturation, s->status.sharpness,
           s->status.special_effect, s->status.wb_mode,
           s->status.awb, s->status.awb_gain,
           s->status.vflip, s->status.hmirror,
           s->status.aec, s->status.aec2,
           s->status.ae_level, s->status.aec_value,
           s->status.agc, s->status.agc_gain,
           s->status.gainceiling, s->status.bpc,
           s->status.wpc, s->status.raw_gma,
           s->status.lenc, s->status.hmirror,
           s->status.dcw, s->status.colorbar);
#if CONFIG_LED_ILLUMINATOR_ENABLED
    p += sprintf(p, ",\"led_intensity\":%u", led_duty);
    p += sprintf(p, ",\"led_status\":%u", isStreaming);
#endif
  } else {
    p += sprintf(p, "\"msg\":\"Camera is disabled or sensor not available\"");
  }
  
  *p++ = '}';
  *p++ = 0;
  
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t xclk_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _xclk[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "xclk", _xclk, sizeof(_xclk)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int xclk = atoi(_xclk);
  log_i("Set XCLK: %d MHz", xclk);

  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_xclk(s, LEDC_TIMER_0, xclk);
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t reg_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _reg[32];
  char _mask[32];
  char _val[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "reg", _reg, sizeof(_reg)) != ESP_OK || httpd_query_key_value(buf, "mask", _mask, sizeof(_mask)) != ESP_OK
      || httpd_query_key_value(buf, "val", _val, sizeof(_val)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int reg = atoi(_reg);
  int mask = atoi(_mask);
  int val = atoi(_val);
  log_i("Set Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, val);

  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_reg(s, reg, mask, val);
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t greg_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _reg[32];
  char _mask[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "reg", _reg, sizeof(_reg)) != ESP_OK || httpd_query_key_value(buf, "mask", _mask, sizeof(_mask)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int reg = atoi(_reg);
  int mask = atoi(_mask);
  sensor_t *s = esp_camera_sensor_get();
  int res = s->get_reg(s, reg, mask);
  if (res < 0) {
    return httpd_resp_send_500(req);
  }
  log_i("Get Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, res);

  char buffer[20];
  const char *val = itoa(res, buffer, 10);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, val, strlen(val));
}

static int parse_get_var(char *buf, const char *key, int def) {
  char _int[16];
  if (httpd_query_key_value(buf, key, _int, sizeof(_int)) != ESP_OK) {
    return def;
  }
  return atoi(_int);
}

static esp_err_t pll_handler(httpd_req_t *req) {
  char *buf = NULL;

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }

  int bypass = parse_get_var(buf, "bypass", 0);
  int mul = parse_get_var(buf, "mul", 0);
  int sys = parse_get_var(buf, "sys", 0);
  int root = parse_get_var(buf, "root", 0);
  int pre = parse_get_var(buf, "pre", 0);
  int seld5 = parse_get_var(buf, "seld5", 0);
  int pclken = parse_get_var(buf, "pclken", 0);
  int pclk = parse_get_var(buf, "pclk", 0);
  free(buf);

  log_i("Set Pll: bypass: %d, mul: %d, sys: %d, root: %d, pre: %d, seld5: %d, pclken: %d, pclk: %d", bypass, mul, sys, root, pre, seld5, pclken, pclk);
  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_pll(s, bypass, mul, sys, root, pre, seld5, pclken, pclk);
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t win_handler(httpd_req_t *req) {
  char *buf = NULL;

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }

  int startX = parse_get_var(buf, "sx", 0);
  int startY = parse_get_var(buf, "sy", 0);
  int endX = parse_get_var(buf, "ex", 0);
  int endY = parse_get_var(buf, "ey", 0);
  int offsetX = parse_get_var(buf, "offx", 0);
  int offsetY = parse_get_var(buf, "offy", 0);
  int totalX = parse_get_var(buf, "tx", 0);
  int totalY = parse_get_var(buf, "ty", 0);  // codespell:ignore totaly
  int outputX = parse_get_var(buf, "ox", 0);
  int outputY = parse_get_var(buf, "oy", 0);
  bool scale = parse_get_var(buf, "scale", 0) == 1;
  bool binning = parse_get_var(buf, "binning", 0) == 1;
  free(buf);

  log_i(
    "Set Window: Start: %d %d, End: %d %d, Offset: %d %d, Total: %d %d, Output: %d %d, Scale: %u, Binning: %u", startX, startY, endX, endY, offsetX, offsetY,
    totalX, totalY, outputX, outputY, scale, binning  // codespell:ignore totaly
  );
  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_res_raw(s, startX, startY, endX, endY, offsetX, offsetY, totalX, totalY, outputX, outputY, scale, binning);  // codespell:ignore totaly
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t index_handler(httpd_req_t *req) {
  extern bool camera_enabled;
  camera_enabled = true;
  
  Serial.println("Entry into web.");
  
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  
  // 即使摄像头禁用，也返回网页，但网页会显示相机禁用信息
  sensor_t *s = NULL;
  if (camera_enabled) {
    s = esp_camera_sensor_get();
  }
  
  if (camera_enabled && s != NULL) {
    if (s->id.PID == OV3660_PID) {
      return httpd_resp_send(req, (const char *)index_ov3660_html_gz, index_ov3660_html_gz_len);
    } else if (s->id.PID == OV5640_PID) {
      return httpd_resp_send(req, (const char *)index_ov5640_html_gz, index_ov5640_html_gz_len);
    } else {
      return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
    }
  } else {
    // 如果相机被禁用，返回通用网页
    return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
  }
}

// 添加MQTT相关设置页面和API
static esp_err_t mqtt_settings_handler(httpd_req_t *req){
  extern const char* mqttServer;
  extern const int mqttPort;
  extern const char* mqttUser;
  extern const char* mqttPassword;
  extern const char* mqttTopic;
  extern PubSubClient mqttClient;
  
  char *buf;
  size_t buf_len;
  char variable[32] = {0,};
  char value[128] = {0,};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
          httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    
    char *mqtt_settings_html = (char *)malloc(2048);
    if (!mqtt_settings_html) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    sprintf(mqtt_settings_html, 
      "<!DOCTYPE html>"
      "<html lang=\"zh-CN\">"
      "<head>"
      "<meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>ESP32 MQTT设置</title>"
      "<style>"
      "body{font-family:Arial,sans-serif;margin:0;padding:0;background:#f4f4f4;color:#333;}"
      ".container{max-width:800px;margin:0 auto;padding:20px;}"
      "h1{color:#0071c5;}"
      "form{background:white;padding:20px;border-radius:5px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}"
      "label{display:block;margin-bottom:5px;font-weight:bold;}"
      "input[type=text],input[type=password],input[type=number]{width:100%%;padding:8px;margin-bottom:15px;border:1px solid #ddd;border-radius:3px;}"
      "button{background:#0071c5;color:white;border:none;padding:10px 15px;border-radius:3px;cursor:pointer;}"
      "button:hover{background:#005999;}"
      ".alert{padding:10px;background:#f8d7da;color:#721c24;border-radius:3px;margin-bottom:15px;display:none;}"
      ".success{background:#d4edda;color:#155724;}"
      "</style>"
      "</head>"
      "<body>"
      "<div class=\"container\">"
      "<h1>ESP32 MQTT设置</h1>"
      "<div id=\"statusMsg\" class=\"alert\"></div>"
      "<form id=\"mqttForm\">"
      "<div>"
      "<label for=\"server\">MQTT服务器地址:</label>"
      "<input type=\"text\" id=\"server\" name=\"server\" value=\"%s\" required>"
      "</div>"
      "<div>"
      "<label for=\"port\">MQTT服务器端口:</label>"
      "<input type=\"number\" id=\"port\" name=\"port\" value=\"%d\" required>"
      "</div>"
      "<div>"
      "<label for=\"user\">MQTT用户名:</label>"
      "<input type=\"text\" id=\"user\" name=\"user\" value=\"%s\">"
      "</div>"
      "<div>"
      "<label for=\"password\">MQTT密码:</label>"
      "<input type=\"password\" id=\"password\" name=\"password\" value=\"%s\">"
      "</div>"
      "<div>"
      "<label for=\"topic\">MQTT主题:</label>"
      "<input type=\"text\" id=\"topic\" name=\"topic\" value=\"%s\" required>"
      "</div>"
      "<button type=\"button\" onclick=\"saveMqttSettings()\">保存设置</button>"
      "<button type=\"button\" onclick=\"testConnection()\">测试连接</button>"
      "<button type=\"button\" onclick=\"window.location.href='/'\">返回</button>"
      "</div>"
      "</form>"
      "<script>"
      "function saveMqttSettings() {"
      "  const server = document.getElementById('server').value;"
      "  const port = document.getElementById('port').value;"
      "  const user = document.getElementById('user').value;"
      "  const password = document.getElementById('password').value;"
      "  const topic = document.getElementById('topic').value;"
      "  "
      "  if (!server || !port || !topic) {"
      "    showMessage('所有必填字段都必须填写', false);"
      "    return;"
      "  }"
      "  "
      "  fetch(`/mqtt_settings?var=save&server=${server}&port=${port}&user=${user}&password=${password}&topic=${topic}`)"
      "    .then(response => response.text())"
      "    .then(data => {"
      "      showMessage('设置已保存，设备将重启应用新设置', true);"
      "      setTimeout(() => { window.location.href = '/'; }, 3000);"
      "    })"
      "    .catch(error => {"
      "      showMessage('保存设置失败: ' + error, false);"
      "    });"
      "}"
      "  "
      "function testConnection() {"
      "  const server = document.getElementById('server').value;"
      "  const port = document.getElementById('port').value;"
      "  const user = document.getElementById('user').value;"
      "  const password = document.getElementById('password').value;"
      "  "
      "  if (!server || !port) {"
      "    showMessage('服务器地址和端口必须填写', false);"
      "    return;"
      "  }"
      "  "
      "  showMessage('正在测试连接...', true);"
      "  "
      "  fetch(`/mqtt_settings?var=test&server=${server}&port=${port}&user=${user}&password=${password}`)"
      "    .then(response => response.text())"
      "    .then(data => {"
      "      if (data.includes('success')) {"
      "        showMessage('连接成功!', true);"
      "      } else {"
      "        showMessage('连接失败: ' + data, false);"
      "      }"
      "    })"
      "    .catch(error => {"
      "      showMessage('测试连接失败: ' + error, false);"
      "    });"
      "}"
      "  "
      "function showMessage(message, isSuccess) {"
      "  const statusMsg = document.getElementById('statusMsg');"
      "  statusMsg.textContent = message;"
      "  statusMsg.style.display = 'block';"
      "  "
      "  if (isSuccess) {"
      "    statusMsg.classList.add('success');"
      "    statusMsg.classList.remove('alert');"
      "  } else {"
      "    statusMsg.classList.add('alert');"
      "    statusMsg.classList.remove('success');"
      "  }"
      "}"
      "</script>"
      "</body>"
      "</html>",
      mqttServer, mqttPort, mqttUser, mqttPassword, mqttTopic);
    
    httpd_resp_send(req, mqtt_settings_html, strlen(mqtt_settings_html));
    free(mqtt_settings_html);
    return ESP_OK;
  }

  // 处理MQTT设置的保存和测试
  if (strcmp(variable, "save") == 0) {
    // 实现保存MQTT设置的逻辑，需要与ESP32Camera.ino中的全局变量通信
    // 或保存到Preferences中
    char server[64] = {0};
    char port[8] = {0};
    char user[32] = {0};
    char pass[32] = {0};
    char topic[64] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
      buf = (char*)malloc(buf_len);
      if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        if (httpd_query_key_value(buf, "server", server, sizeof(server)) == ESP_OK &&
            httpd_query_key_value(buf, "port", port, sizeof(port)) == ESP_OK &&
            httpd_query_key_value(buf, "user", user, sizeof(user)) == ESP_OK &&
            httpd_query_key_value(buf, "password", pass, sizeof(pass)) == ESP_OK &&
            httpd_query_key_value(buf, "topic", topic, sizeof(topic)) == ESP_OK) {
          
          // 使用全局的preferences变量
          preferences.begin("mqtt_config", false);
          preferences.putString("server", server);
          preferences.putInt("port", atoi(port));
          preferences.putString("user", user);
          preferences.putString("password", pass);
          preferences.putString("topic", topic);
          preferences.end();
          
          httpd_resp_set_type(req, "text/plain");
          httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
          httpd_resp_send(req, "Settings saved", -1);
          
          // 设置一个延迟重启标志，以便能完成当前响应
          preferences.begin("system", false);
          preferences.putBool("restart_flag", true);
          preferences.end();
          
          return ESP_OK;
        }
      }
      free(buf);
    }
  } else if (strcmp(variable, "test") == 0) {
    // 实现测试MQTT连接的逻辑
    char server[64] = {0};
    char port[8] = {0};
    char user[32] = {0};
    char pass[32] = {0};
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
      buf = (char*)malloc(buf_len);
      if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        if (httpd_query_key_value(buf, "server", server, sizeof(server)) == ESP_OK &&
            httpd_query_key_value(buf, "port", port, sizeof(port)) == ESP_OK) {
          
          httpd_query_key_value(buf, "user", user, sizeof(user));
          httpd_query_key_value(buf, "password", pass, sizeof(pass));
          
          // 创建临时的MQTT客户端进行测试
          WiFiClient testClient;
          PubSubClient testMqtt(testClient);
          
          testMqtt.setServer(server, atoi(port));
          bool connected = false;
          
          if (strlen(user) > 0) {
            connected = testMqtt.connect("ESP32_Test", user, pass);
          } else {
            connected = testMqtt.connect("ESP32_Test");
          }
          
          testMqtt.disconnect();
          
          httpd_resp_set_type(req, "text/plain");
          httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
          
          if (connected) {
            httpd_resp_send(req, "success", -1);
          } else {
            char response[32];
            sprintf(response, "failed: %d", testMqtt.state());
            httpd_resp_send(req, response, -1);
          }
          
          free(buf);
          return ESP_OK;
        }
      }
      free(buf);
    }
  }
  
  httpd_resp_send_404(req);
  return ESP_FAIL;
}

// 外部声明
extern const char* mqttServer;  // MQTT服务器地址，也用作HTTP服务器
extern String mqttClientId;     // 设备ID
extern bool camera_enabled;     // 相机是否启用

// WebSocket相关函数声明
void handleTakePhotoCommand();

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
  };

  httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri = "/control",
    .method = HTTP_GET,
    .handler = cmd_handler,
    .user_ctx = NULL
  };

  httpd_uri_t capture_uri = {
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = capture_handler,
    .user_ctx = NULL
  };

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  httpd_uri_t bmp_uri = {
    .uri = "/bmp",
    .method = HTTP_GET,
    .handler = bmp_handler,
    .user_ctx = NULL
  };

  httpd_uri_t xclk_uri = {
    .uri = "/xclk",
    .method = HTTP_GET,
    .handler = xclk_handler,
    .user_ctx = NULL
  };

  httpd_uri_t reg_uri = {
    .uri = "/reg",
    .method = HTTP_GET,
    .handler = reg_handler,
    .user_ctx = NULL
  };

  httpd_uri_t greg_uri = {
    .uri = "/greg",
    .method = HTTP_GET,
    .handler = greg_handler,
    .user_ctx = NULL
  };

  httpd_uri_t pll_uri = {
    .uri = "/pll",
    .method = HTTP_GET,
    .handler = pll_handler,
    .user_ctx = NULL
  };

  httpd_uri_t win_uri = {
    .uri = "/resolution",
    .method = HTTP_GET,
    .handler = win_handler,
    .user_ctx = NULL
  };

  httpd_uri_t mqtt_settings_uri = {
    .uri = "/mqtt_settings",
    .method = HTTP_GET,
    .handler = mqtt_settings_handler,
    .user_ctx = NULL
  };

  ra_filter_init(&ra_filter, 20);

  log_i("Starting web server on port: '%d'", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &bmp_uri);

    httpd_register_uri_handler(camera_httpd, &xclk_uri);
    httpd_register_uri_handler(camera_httpd, &reg_uri);
    httpd_register_uri_handler(camera_httpd, &greg_uri);
    httpd_register_uri_handler(camera_httpd, &pll_uri);
    httpd_register_uri_handler(camera_httpd, &win_uri);
    httpd_register_uri_handler(camera_httpd, &mqtt_settings_uri);
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  log_i("Starting stream server on port: '%d'", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setupLedFlash(int pin) {
#if CONFIG_LED_ILLUMINATOR_ENABLED
  ledcAttach(pin, 5000, 8);
#else
  log_i("LED flash is disabled -> CONFIG_LED_ILLUMINATOR_ENABLED = 0");
#endif
}

// 处理WebSocket中的take_photo命令
void handleTakePhotoCommand() {
  if (!camera_enabled) {
    log_e("相机已禁用，无法拍照");
    return;
  }
  
  log_i("收到拍照命令，正在拍照并上传...");

  // 闪烁指示灯
  digitalWrite(STATUS_LED, LOW);
  delay(100);
  digitalWrite(STATUS_LED, HIGH);
  
  // 拍照
  camera_fb_t *fb = NULL;
  
#if CONFIG_LED_ILLUMINATOR_ENABLED
  enable_led(true);
  vTaskDelay(150 / portTICK_PERIOD_MS);  // 打开LED灯后等待150ms
  fb = esp_camera_fb_get();
  enable_led(false);
#else
  fb = esp_camera_fb_get();
#endif

  if (!fb) {
    log_e("拍照失败");
    return;
  }

  // 构建URL
  String url = "http://";
  url += mqttServer;  // 使用MQTT服务器地址作为HTTP服务器地址
  url += ":8080/api/photos/upload?device_id=";
  url += mqttClientId;
  url += "&timestamp=";
  
  // 添加时间戳
  time_t now;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y%m%dT%H%M%S", &timeinfo);
    url += timeStr;
  } else {
    // 如果获取时间失败，使用毫秒时间戳
    url += millis();
  }
  
  log_i("上传URL: %s", url.c_str());
  
  // 通过HTTP POST上传照片
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, url);
  
  // 发送HTTP POST请求，包含图像数据
  int httpResponseCode = http.POST(fb->buf, fb->len);
  
  if (httpResponseCode > 0) {
    log_i("HTTP响应代码: %d", httpResponseCode);
    String response = http.getString();
    log_i("响应内容: %s", response.c_str());
  } else {
    log_e("HTTP POST错误: %s", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
  
  // 释放帧缓冲区
  esp_camera_fb_return(fb);
}
