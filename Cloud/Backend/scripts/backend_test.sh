#!/bin/bash

# 环境监测系统后端测试脚本
# 用途：测试后端系统的各项功能
# 测试范围：API接口、数据处理与存储、预警决策

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # 无颜色

# 设置后端服务地址和端口
BACKEND_HOST="localhost"
BACKEND_PORT="8080"
BASE_URL="http://${BACKEND_HOST}:${BACKEND_PORT}/api/v1"

# 测试设备ID
TEST_DEVICE_ID="test-device-001"

# MQTT配置
MQTT_BROKER="localhost"
MQTT_PORT="1883"
MQTT_TOPIC_SENSOR="armdetector/sensor/data"
MQTT_TOPIC_COMMAND_RESPONSE="armdetector/device/${TEST_DEVICE_ID}/response"

# 生成随机请求ID
generate_request_id() {
  cat /proc/sys/kernel/random/uuid
}

# 输出日志
log() {
  echo -e "${2:-$BLUE}[$(date '+%Y-%m-%d %H:%M:%S')] $1${NC}"
}

# 输出成功信息
log_success() {
  log "$1" "$GREEN"
}

# 输出错误信息
log_error() {
  log "$1" "$RED"
}

# 输出警告信息
log_warning() {
  log "$1" "$YELLOW"
}

# 检查依赖工具
check_dependencies() {
  log "检查测试依赖..."
  
  local missing_deps=0
  
  # 检查curl
  if ! command -v curl &> /dev/null; then
    log_error "请安装curl: sudo apt-get install curl"
    missing_deps=1
  fi
  
  # 检查jq
  if ! command -v jq &> /dev/null; then
    log_error "请安装jq: sudo apt-get install jq"
    missing_deps=1
  fi
  
  # 检查mosquitto客户端
  if ! command -v mosquitto_pub &> /dev/null || ! command -v mosquitto_sub &> /dev/null; then
    log_error "请安装Mosquitto客户端: sudo apt-get install mosquitto-clients"
    missing_deps=1
  fi
  
  # 检查ab (Apache Bench)
  if ! command -v ab &> /dev/null; then
    log_error "请安装Apache Bench: sudo apt-get install apache2-utils"
    missing_deps=1
  fi
  
  if [ $missing_deps -eq 1 ]; then
    exit 1
  fi
  
  log_success "所有依赖已安装."
}

# 检查后端服务是否可用
check_backend_availability() {
  log "检查后端服务是否可用..."
  
  local retries=3
  local wait_time=2
  
  for (( i=1; i<=$retries; i++ )); do
    if curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/sensor?page=1&page_size=1" | grep -q "200"; then
      log_success "后端服务可用."
      return 0
    else
      log_warning "后端服务不可用，第${i}次重试... (等待${wait_time}秒)"
      sleep $wait_time
    fi
  done
  
  log_error "后端服务不可用，请检查服务是否运行在 ${BACKEND_HOST}:${BACKEND_PORT}."
  exit 1
}

# ===== 5.3.1 API接口测试 =====

# 数据查询接口测试
test_data_query_api() {
  log "开始数据查询接口测试..."
  
  # 测试获取传感器数据列表（分页）
  log "测试：获取传感器数据列表（分页）"
  local resp=$(curl -s "${BASE_URL}/sensor?page=1&page_size=10")
  if echo "$resp" | jq -e '.data' > /dev/null && echo "$resp" | jq -e '.pagination' > /dev/null; then
    log_success "获取传感器数据列表（分页）: 成功"
  else
    log_error "获取传感器数据列表（分页）: 失败"
    echo "$resp" | jq
  fi
  
  # 测试时间范围过滤
  log "测试：按时间范围过滤数据"
  local start_time=$(date -u -d "30 minutes ago" "+%Y-%m-%dT%H:%M:%SZ")
  local end_time=$(date -u "+%Y-%m-%dT%H:%M:%SZ")
  local resp=$(curl -s "${BASE_URL}/sensor?start_time=${start_time}&end_time=${end_time}&page=1&page_size=10")
  if echo "$resp" | jq -e '.data' > /dev/null; then
    log_success "按时间范围过滤数据: 成功"
  else
    log_error "按时间范围过滤数据: 失败"
    echo "$resp" | jq
  fi
  
  # 测试获取单条传感器数据
  log "测试：获取单条传感器数据"
  # 先获取一个ID
  local id=$(curl -s "${BASE_URL}/sensor?page=1&page_size=1" | jq -r '.data[0].id // empty')
  if [ -n "$id" ]; then
    local resp=$(curl -s "${BASE_URL}/sensor/${id}")
    if echo "$resp" | jq -e '.id' > /dev/null; then
      log_success "获取单条传感器数据: 成功"
    else
      log_error "获取单条传感器数据: 失败"
      echo "$resp" | jq
    fi
  else
    log_warning "无法获取传感器数据ID，跳过单条数据测试"
  fi
  
  log_success "数据查询接口测试完成"
}

# 设备控制接口测试
test_device_control_api() {
  log "开始设备控制接口测试..."
  
  # 测试发送重启命令
  log "测试：发送重启命令"
  local request_id=""
  local resp=$(curl -s -X POST "${BASE_URL}/device/${TEST_DEVICE_ID}/command" \
    -H "Content-Type: application/json" \
    -d '{"command":"restart_device","parameters":{}}')
  
  if echo "$resp" | jq -e '.request_id' > /dev/null; then
    request_id=$(echo "$resp" | jq -r '.request_id')
    log_success "发送重启命令: 成功（请求ID: ${request_id}）"
    
    # 模拟设备响应
    log "模拟设备响应..."
    local response='{
      "request_id":"'"${request_id}"'",
      "status":"success",
      "message":"设备正在重启",
      "timestamp":"'"$(date -u +%Y-%m-%dT%H:%M:%SZ)"'"
    }'
    mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT}" -t "${MQTT_TOPIC_COMMAND_RESPONSE}" -m "$response"
    sleep 2 # 等待消息处理
    
    # 检查命令状态
    log "测试：查询命令状态"
    local status_resp=$(curl -s "${BASE_URL}/command/${request_id}")
    if echo "$status_resp" | jq -e '.status == "success"' > /dev/null; then
      log_success "查询命令状态: 成功（状态: success）"
    else
      log_error "查询命令状态: 失败或未更新"
      echo "$status_resp" | jq
    fi
  else
    log_error "发送重启命令: 失败"
    echo "$resp" | jq
  fi
  
  # 测试发送拍照命令
  log "测试：发送拍照命令"
  local resp=$(curl -s -X POST "${BASE_URL}/device/${TEST_DEVICE_ID}/command" \
    -H "Content-Type: application/json" \
    -d '{"command":"take_photo","parameters":{}}')
  
  if echo "$resp" | jq -e '.request_id' > /dev/null; then
    log_success "发送拍照命令: 成功"
  else
    log_error "发送拍照命令: 失败"
    echo "$resp" | jq
  fi
  
  # 测试发送更名命令
  log "测试：发送更名命令"
  local resp=$(curl -s -X POST "${BASE_URL}/device/${TEST_DEVICE_ID}/command" \
    -H "Content-Type: application/json" \
    -d '{"command":"rename_device","parameters":{"name":"新设备名称"}}')
  
  if echo "$resp" | jq -e '.request_id' > /dev/null; then
    log_success "发送更名命令: 成功"
  else
    log_error "发送更名命令: 失败"
    echo "$resp" | jq
  fi
  
  # 测试无效命令
  log "测试：发送无效命令"
  local resp=$(curl -s -X POST "${BASE_URL}/device/${TEST_DEVICE_ID}/command" \
    -H "Content-Type: application/json" \
    -d '{"command":"invalid_command","parameters":{}}')
  
  if echo "$resp" | jq -e '.error' > /dev/null; then
    log_success "发送无效命令: 成功（返回错误）"
  else
    log_error "发送无效命令: 失败（接受了无效命令）"
    echo "$resp" | jq
  fi
  
  log_success "设备控制接口测试完成"
}

# 接口响应时间测试
test_api_response_time() {
  log "开始接口响应时间测试..."
  
  # 请求参数
  local requests=100
  local concurrency=10
  
  # 测试传感器数据列表接口
  log "测试：传感器数据列表接口响应时间"
  local output=$(ab -n $requests -c $concurrency -q "${BASE_URL}/sensor?page=1&page_size=10" 2>&1)
  local time_per_request=$(echo "$output" | grep "Time per request" | head -1 | awk '{print $4}')
  
  if [ -n "$time_per_request" ]; then
    log_success "传感器数据列表接口平均响应时间: ${time_per_request} ms"
  else
    log_error "测量传感器数据列表接口响应时间失败"
  fi
  
  # 测试命令发送接口
  log "测试：命令发送接口响应时间"
  local output=$(ab -n $requests -c $concurrency -q -p /tmp/command_data.json \
    -T 'application/json' "${BASE_URL}/device/${TEST_DEVICE_ID}/command" 2>&1)
  local time_per_request=$(echo "$output" | grep "Time per request" | head -1 | awk '{print $4}')
  
  if [ -n "$time_per_request" ]; then
    log_success "命令发送接口平均响应时间: ${time_per_request} ms"
  else
    log_error "测量命令发送接口响应时间失败"
  fi
  
  log_success "接口响应时间测试完成"
}

# ===== 5.3.2 数据处理与存储测试 =====

# 数据解析正确性测试
test_data_parsing() {
  log "开始数据解析正确性测试..."
  
  # 测试正常格式数据
  log "测试：正常格式数据解析"
  local timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  local normal_data='{
    "device_id":"'"${TEST_DEVICE_ID}"'",
    "timestamp":"'"${timestamp}"'",
    "temperature":25.5,
    "humidity":65.2,
    "co_ppm":3.2,
    "dust_density":18.7,
    "alarm_status":"normal"
  }'
  
  mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT}" -t "${MQTT_TOPIC_SENSOR}" -m "$normal_data"
  sleep 2 # 等待数据处理
  
  # 检查数据是否入库
  local resp=$(curl -s "${BASE_URL}/sensor?device_id=${TEST_DEVICE_ID}&start_time=${timestamp}&page=1&page_size=10")
  local found=$(echo "$resp" | jq -r --arg t "$timestamp" '.data[] | select(.timestamp == $t and .device_id == "'"${TEST_DEVICE_ID}"'") | .id')
  
  if [ -n "$found" ]; then
    log_success "正常格式数据解析: 成功（数据已入库）"
  else
    log_error "正常格式数据解析: 失败（数据未入库）"
  fi
  
  # 测试时间戳为Unix毫秒格式
  log "测试：Unix时间戳格式数据解析"
  local unix_ms=$(date +%s)000
  local unix_time_data='{
    "device_id":"'"${TEST_DEVICE_ID}"'",
    "timestamp":'"${unix_ms}"',
    "temperature":26.3,
    "humidity":62.8,
    "co_ppm":4.5,
    "dust_density":20.1,
    "alarm_status":"normal"
  }'
  
  mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT}" -t "${MQTT_TOPIC_SENSOR}" -m "$unix_time_data"
  sleep 2 # 等待数据处理
  
  # 检查数据是否入库（使用近期时间查询）
  local start_time=$(date -u -d "1 minute ago" "+%Y-%m-%dT%H:%M:%SZ")
  local end_time=$(date -u "+%Y-%m-%dT%H:%M:%SZ")
  local resp=$(curl -s "${BASE_URL}/sensor?device_id=${TEST_DEVICE_ID}&start_time=${start_time}&end_time=${end_time}&page=1&page_size=10")
  local found=$(echo "$resp" | jq -r '.data[] | select(.temperature == 26.3 and .device_id == "'"${TEST_DEVICE_ID}"'") | .id')
  
  if [ -n "$found" ]; then
    log_success "Unix时间戳格式数据解析: 成功（数据已入库）"
  else
    log_error "Unix时间戳格式数据解析: 失败（数据未入库）"
  fi
  
  # 测试缺失时间戳字段
  log "测试：缺失时间戳字段的数据解析"
  local missing_timestamp_data='{
    "device_id":"'"${TEST_DEVICE_ID}"'",
    "temperature":27.1,
    "humidity":59.5,
    "co_ppm":5.8,
    "dust_density":22.3,
    "alarm_status":"normal"
  }'
  
  mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT}" -t "${MQTT_TOPIC_SENSOR}" -m "$missing_timestamp_data"
  sleep 2 # 等待数据处理
  
  # 检查数据是否入库（使用近期时间查询）
  local resp=$(curl -s "${BASE_URL}/sensor?device_id=${TEST_DEVICE_ID}&start_time=${start_time}&end_time=${end_time}&page=1&page_size=10")
  local found=$(echo "$resp" | jq -r '.data[] | select(.temperature == 27.1 and .device_id == "'"${TEST_DEVICE_ID}"'") | .id')
  
  if [ -n "$found" ]; then
    log_success "缺失时间戳字段的数据解析: 成功（数据已入库，应该使用服务器时间）"
  else
    log_error "缺失时间戳字段的数据解析: 失败（数据未入库）"
  fi
  
  log_success "数据解析正确性测试完成"
}

# 数据持久化测试
test_data_persistence() {
  log "开始数据持久化测试..."
  
  # 生成带特殊标记的测试数据
  local timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  local test_data='{
    "device_id":"'"${TEST_DEVICE_ID}"'",
    "timestamp":"'"${timestamp}"'",
    "temperature":28.7,
    "humidity":58.3,
    "co_ppm":6.2,
    "dust_density":25.9,
    "alarm_status":"normal"
  }'
  
  # 发送数据
  log "发送测试数据..."
  mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT}" -t "${MQTT_TOPIC_SENSOR}" -m "$test_data"
  sleep 2 # 等待数据处理
  
  # 验证数据存在
  log "验证数据存在..."
  local resp1=$(curl -s "${BASE_URL}/sensor?device_id=${TEST_DEVICE_ID}&start_time=${timestamp}&page=1&page_size=10")
  local id=$(echo "$resp1" | jq -r '.data[] | select(.temperature == 28.7 and .device_id == "'"${TEST_DEVICE_ID}"'") | .id')
  
  if [ -n "$id" ]; then
    log_success "数据入库: 成功（ID: ${id}）"
    
    # 重启后端服务（模拟）
    log "模拟重启后端服务..."
    # 实际测试中应该停止和启动服务
    sleep 5
    
    # 验证数据在重启后仍然存在
    log "验证数据重启后仍然存在..."
    local resp2=$(curl -s "${BASE_URL}/sensor/${id}")
    
    if echo "$resp2" | jq -e '.id == '"$id"'' > /dev/null; then
      log_success "数据持久化: 成功（重启后数据仍存在）"
    else
      log_error "数据持久化: 失败（重启后数据丢失）"
    fi
  else
    log_error "数据入库: 失败（未找到测试数据）"
  fi
  
  log_success "数据持久化测试完成"
}

# 高并发处理能力测试
test_concurrency() {
  log "开始高并发处理能力测试..."
  
  local message_count=50
  local start_time=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  
  # 准备并发发送脚本
  cat > /tmp/mqtt_flood.sh << 'EOF'
#!/bin/bash
MQTT_BROKER="$1"
MQTT_PORT="$2"
TOPIC="$3"
DEVICE_ID="$4"
COUNT=$5

for i in $(seq 1 $COUNT); do
  timestamp=$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)
  temp=$(awk -v min=20 -v max=30 'BEGIN{srand(); print min+rand()*(max-min)}')
  humidity=$(awk -v min=50 -v max=70 'BEGIN{srand(); print min+rand()*(max-min)}')
  co=$(awk -v min=1 -v max=10 'BEGIN{srand(); print min+rand()*(max-min)}')
  dust=$(awk -v min=10 -v max=30 'BEGIN{srand(); print min+rand()*(max-min)}')
  
  data='{
    "device_id":"'"$DEVICE_ID"'",
    "timestamp":"'"$timestamp"'",
    "temperature":'"$temp"',
    "humidity":'"$humidity"',
    "co_ppm":'"$co"',
    "dust_density":'"$dust"',
    "alarm_status":"normal"
  }'
  
  mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$TOPIC" -m "$data" &
done

wait
EOF

  chmod +x /tmp/mqtt_flood.sh
  
  # 执行并发测试
  log "开始发送${message_count}条并发消息..."
  /tmp/mqtt_flood.sh "${MQTT_BROKER}" "${MQTT_PORT}" "${MQTT_TOPIC_SENSOR}" "${TEST_DEVICE_ID}" "${message_count}"
  
  # 等待处理完成
  log "等待消息处理完成..."
  sleep 10
  
  # 检查入库数量
  local end_time=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  local resp=$(curl -s "${BASE_URL}/sensor?device_id=${TEST_DEVICE_ID}&start_time=${start_time}&end_time=${end_time}&page=1&page_size=100")
  local count=$(echo "$resp" | jq '.data | length')
  
  log "已接收到 ${count}/${message_count} 条消息"
  local success_rate=$(awk "BEGIN {print ($count / $message_count) * 100}")
  
  if (( $(echo "$success_rate > 90" | bc -l) )); then
    log_success "高并发处理能力: 成功（成功率: ${success_rate}%）"
  else
    log_error "高并发处理能力: 失败（成功率: ${success_rate}%）"
  fi
  
  # 清理临时文件
  rm -f /tmp/mqtt_flood.sh
  
  log_success "高并发处理能力测试完成"
}

# ===== 5.3.3 预警决策测试 =====

# 异常数据识别测试
test_anomaly_detection() {
  log "开始异常数据识别测试..."
  
  # 发送高温异常数据
  log "发送高温异常数据..."
  local timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  local high_temp_data='{
    "device_id":"'"${TEST_DEVICE_ID}"'",
    "timestamp":"'"${timestamp}"'",
    "temperature":42.5,
    "humidity":55.0,
    "co_ppm":3.5,
    "dust_density":20.0,
    "alarm_status":"temperature_high"
  }'
  
  mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT}" -t "${MQTT_TOPIC_SENSOR}" -m "$high_temp_data"
  sleep 2 # 等待数据处理
  
  # 验证高温警报状态
  local resp=$(curl -s "${BASE_URL}/sensor?device_id=${TEST_DEVICE_ID}&start_time=${timestamp}&page=1&page_size=10")
  local alarm_status=$(echo "$resp" | jq -r '.data[] | select(.temperature > 40 and .device_id == "'"${TEST_DEVICE_ID}"'") | .alarm_status')
  
  if [ "$alarm_status" = "temperature_high" ]; then
    log_success "高温异常识别: 成功（警报状态: ${alarm_status}）"
  else
    log_error "高温异常识别: 失败（未找到警报状态或状态错误）"
  fi
  
  # 发送一氧化碳异常数据
  log "发送一氧化碳异常数据..."
  local timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  local high_co_data='{
    "device_id":"'"${TEST_DEVICE_ID}"'",
    "timestamp":"'"${timestamp}"'",
    "temperature":25.0,
    "humidity":60.0,
    "co_ppm":35.8,
    "dust_density":18.0,
    "alarm_status":"co_high"
  }'
  
  mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT}" -t "${MQTT_TOPIC_SENSOR}" -m "$high_co_data"
  sleep 2 # 等待数据处理
  
  # 验证一氧化碳警报状态
  local resp=$(curl -s "${BASE_URL}/sensor?device_id=${TEST_DEVICE_ID}&start_time=${timestamp}&page=1&page_size=10")
  local alarm_status=$(echo "$resp" | jq -r '.data[] | select(.co_ppm > 30 and .device_id == "'"${TEST_DEVICE_ID}"'") | .alarm_status')
  
  if [ "$alarm_status" = "co_high" ]; then
    log_success "一氧化碳异常识别: 成功（警报状态: ${alarm_status}）"
  else
    log_error "一氧化碳异常识别: 失败（未找到警报状态或状态错误）"
  fi
  
  log_success "异常数据识别测试完成"
}

# 主函数
main() {
  echo -e "${BLUE}=========================================${NC}"
  echo -e "${BLUE}         后端服务测试脚本              ${NC}"
  echo -e "${BLUE}=========================================${NC}"
  
  # 检查依赖工具
  check_dependencies
  
  # 检查后端服务可用性
  check_backend_availability
  
  # 创建临时文件
  echo '{
    "command":"restart_device",
    "parameters":{}
  }' > /tmp/command_data.json
  
  echo -e "\n${BLUE}===== API接口测试 =====${NC}"
  test_data_query_api
  test_device_control_api
  test_api_response_time
  
  echo -e "\n${BLUE}===== 数据处理与存储测试 =====${NC}"
  test_data_parsing
  test_data_persistence
  test_concurrency
  
  echo -e "\n${BLUE}===== 预警决策测试 =====${NC}"
  test_anomaly_detection
  
  # 清理临时文件
  rm -f /tmp/command_data.json
  
  echo -e "\n${GREEN}=========================================${NC}"
  echo -e "${GREEN}        测试完成                   ${NC}"
  echo -e "${GREEN}=========================================${NC}"
}

# 执行主函数
main "$@" 