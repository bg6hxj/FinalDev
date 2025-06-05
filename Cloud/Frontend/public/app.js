document.addEventListener('DOMContentLoaded', () => {
    const API_BASE_URL = '/api/v1'; // 根据实际后端部署情况调整，或通过nginx代理

    // DOM Elements
    const systemStatusEl = document.querySelector('#system-status .value');
    const deviceStatusSummaryEl = document.querySelector('#device-status-summary .value');
    const alarmListEl = document.getElementById('alarm-list');
    // ... ESP32 Live Data Card elements
    const liveDeviceIDSel = document.getElementById('device-select-live');
    const liveDeviceIDEl = document.getElementById('live-device-id');
    const liveTempEl = document.getElementById('live-temp');
    const liveHumidityEl = document.getElementById('live-humidity');
    const liveCoEl = document.getElementById('live-co');
    const liveDustEl = document.getElementById('live-dust');
    const liveAlarmEl = document.getElementById('live-alarm');
    const liveTimestampEl = document.getElementById('live-timestamp');
    // ... ESP32 Control elements
    const controlDeviceSel = document.getElementById('device-select-control');
    const restartDeviceBtn = document.getElementById('restart-device-btn');
    const controlStatusEl = document.getElementById('control-status');
    // ... Chart elements and controls
    const chartDeviceSel = document.getElementById('device-select-chart');
    const chartMetricSel = document.getElementById('metric-select-chart');
    const chartTimeRangeSel = document.getElementById('time-range-chart');
    const refreshChartBtn = document.getElementById('refresh-chart-btn');
    const canvasEl = document.getElementById('data-chart');
    const ctx = canvasEl.getContext('2d');
    let historicalChart;

    let knownDeviceIds = [];

    async function fetchData(endpoint, params = {}) {
        const urlParams = new URLSearchParams(params);
        const url = `${API_BASE_URL}${endpoint}?${urlParams.toString()}`;
        console.log(`Fetching data from: ${url}`);
        try {
            // 设置合理的请求超时时间
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), 15000); // 15秒超时
            
            const response = await fetch(url, { signal: controller.signal });
            clearTimeout(timeoutId);
            
            if (!response.ok) {
                console.error(`API Error: ${response.status} ${response.statusText} for ${url}`);
                const errorData = await response.text();
                console.error('Error details:', errorData);
                return null;
            }
            
            const data = await response.json();
            console.log(`API 返回数据 (${endpoint}): ${JSON.stringify(data).substring(0, 200)}...`);
            return data;
        } catch (error) {
            if (error.name === 'AbortError') {
                console.error('请求超时:', url);
                systemStatusEl.textContent = '错误 (请求超时)';
            } else {
                console.error('Fetch error:', error.message);
                systemStatusEl.textContent = '错误 (无法连接到后端)';
            }
            systemStatusEl.style.color = 'red';
            return null;
        }
    }

    function updateSystemStatus(success = true) {
        if (success) {
            systemStatusEl.textContent = '运行中';
            systemStatusEl.style.color = 'green';
        } else {
            systemStatusEl.textContent = '错误';
            systemStatusEl.style.color = 'red';
        }
    }
    
    async function updateKnownDeviceIds() {
        console.log('Updating known device IDs...');
        const params = { page: 1, page_size: 100 }; 
        const response = await fetchData('/sensor', params);
        if (response && response.data) {
            console.log('Device data received for ID update:', response.data);
            const deviceIdsFromData = [...new Set(response.data.map(d => d.device_id).filter(id => id != null))];
            console.log('Extracted device IDs:', deviceIdsFromData);
            
            if (JSON.stringify(knownDeviceIds.sort()) !== JSON.stringify(deviceIdsFromData.sort())) {
                knownDeviceIds = deviceIdsFromData;
                populateDeviceSelectors();
                 console.log('Device selectors populated.');
            }
            updateDeviceStatusSummary(response.data); // Pass original data for summary
        } else {
            console.log('No data received for device ID update.');
             populateDeviceSelectors(); // Still call to handle empty case
        }
    }

    function populateDeviceSelectors() {
        [liveDeviceIDSel, chartDeviceSel, controlDeviceSel].forEach(selector => {
            const currentSelectedValue = selector.value;
            selector.innerHTML = ''; 
            if (knownDeviceIds.length === 0) {
                const option = document.createElement('option');
                option.value = "";
                option.textContent = "暂无设备";
                selector.appendChild(option);
            } else {
                knownDeviceIds.forEach(id => {
                    const option = document.createElement('option');
                    option.value = id;
                    option.textContent = id;
                    selector.appendChild(option);
                });
                // Try to restore previous selection or default to first
                if (knownDeviceIds.includes(currentSelectedValue)) {
                    selector.value = currentSelectedValue;
                } else if (knownDeviceIds.length > 0) {
                    selector.value = knownDeviceIds[0];
                }
            }
        });
    }
    
    function updateDeviceStatusSummary(allSensorData) {
        if (allSensorData && allSensorData.length > 0) {
             const uniqueActiveDevices = new Set();
             const now = new Date();
             const activityThreshold = 5 * 60 * 1000; // 5 minutes in milliseconds

             knownDeviceIds.forEach(deviceId => {
                const latestDataForDevice = allSensorData
                    .filter(d => d.device_id === deviceId)
                    .sort((a,b) => new Date(b.timestamp) - new Date(a.timestamp))[0];
                
                if (latestDataForDevice) {
                    const lastSeen = new Date(latestDataForDevice.timestamp);
                    if ((now - lastSeen) < activityThreshold) {
                        uniqueActiveDevices.add(deviceId);
                    }
                }
             });
             deviceStatusSummaryEl.textContent = `${uniqueActiveDevices.size} (5分钟内活跃)`;
        } else {
            deviceStatusSummaryEl.textContent = `0`;
        }
    }

    // 新增日期格式化函数，按原始UTC时间格式化
    function formatUTCDate(date) {
        if (!date) return '未知时间';
        
        // 保持原始UTC时间，不转换为本地时区
        const year = date.getUTCFullYear();
        const month = (date.getUTCMonth() + 1).toString().padStart(2, '0');
        const day = date.getUTCDate().toString().padStart(2, '0');
        const hours = (date.getUTCHours()).toString().padStart(2, '0');
        const minutes = date.getUTCMinutes().toString().padStart(2, '0');
        const seconds = date.getUTCSeconds().toString().padStart(2, '0');
        
        return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    }
    
    // 修改后的函数，用于格式化为本地时间
    function formatLocalDateTime(date) {
        if (!date) return '未知时间';
        
        const year = date.getFullYear();
        const month = (date.getMonth() + 1).toString().padStart(2, '0');
        const day = date.getDate().toString().padStart(2, '0');
        const hours = date.getHours().toString().padStart(2, '0');
        const minutes = date.getMinutes().toString().padStart(2, '0');
        const seconds = date.getSeconds().toString().padStart(2, '0');
        
        return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    }

    // 新增函数：为实时数据显示将UTC时间格式化为自定义的UTC+8时间
    function formatCustomTimeForLiveData(date) {
        if (!date) return '未知时间';

        // 获取原始Date对象的UTC毫秒数
        const originalUTCMilliseconds = date.getTime();
        // 计算UTC+8的毫秒数
        const targetMilliseconds = originalUTCMilliseconds + (8 * 60 * 60 * 1000);
        // 创建一个新的Date对象代表UTC+8的时间点
        const targetDate = new Date(targetMilliseconds);

        // 使用这个新的Date对象的UTC方法来提取年、月、日、时、分、秒
        // 因为targetDate本身是在UTC时间线上偏移了8小时，所以它的UTC部分就是我们想要的UTC+8的显示部分
        const year = targetDate.getUTCFullYear();
        const month = (targetDate.getUTCMonth() + 1).toString().padStart(2, '0');
        const day = targetDate.getUTCDate().toString().padStart(2, '0');
        const hours = targetDate.getUTCHours().toString().padStart(2, '0');
        const minutes = targetDate.getUTCMinutes().toString().padStart(2, '0');
        const seconds = targetDate.getUTCSeconds().toString().padStart(2, '0');
            
        return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    }

    // Initial Load and Periodic Updates
    async function initializePage() {
        console.log('Initializing page...');
        
        // 初始化设备控制模块
        initDeviceControl();
        
        // 初始化图表区域尺寸
        resetCanvasSize();
        
        // 初始化设备选择器
        try {
            await updateKnownDeviceIds();
            populateDeviceSelectors();
            
            // 设置默认选择值
            liveDeviceIDSel.selectedIndex = 0;
            chartDeviceSel.selectedIndex = 0;
            chartMetricSel.value = 'temperature';
            chartTimeRangeSel.value = '5m'; // 默认显示过去5分钟
            
            // 获取实时数据
            await fetchAndDisplayLiveDataCard();
            
            // 初始加载所有数据
            fetchAndDisplayLatestAlarms();
            renderHistoricalChart(); 

            // 实时数据卡片定期更新 (每10秒)
            setInterval(fetchAndDisplayLiveDataCard, 10000); 
            
            // 警报列表更新频率，改为更频繁 (每10秒)
            setInterval(fetchAndDisplayLatestAlarms, 10000);
            
            // 设备ID列表更新 (每60秒)
            setInterval(() => {
                updateKnownDeviceIds().then(() => {
                     // 设备状态摘要可能在将来独立显示
                });
            }, 60000); 
            
            console.log('Page initialization complete, timers started.');
        } catch (error) {
            console.error('Page initialization error:', error);
            updateSystemStatus(false);
        }
    }
    
    // 添加自动刷新状态显示
    let lastRefreshTime = new Date();
    
    // 定时更新页面刷新状态
    setInterval(() => {
        const now = new Date();
        const secondsSinceLastRefresh = Math.floor((now - lastRefreshTime) / 1000);
        
        const statusEl = document.querySelector('#alarm-refresh-status');
        if (statusEl) {
            if (secondsSinceLastRefresh < 60) {
                statusEl.textContent = `${secondsSinceLastRefresh}秒前刷新`;
            } else {
                const minutes = Math.floor(secondsSinceLastRefresh / 60);
                statusEl.textContent = `${minutes}分钟前刷新`;
            }
        }
    }, 1000);
    
    // 修改警报获取函数，添加刷新时间更新
    async function fetchAndDisplayLatestAlarms() {
        console.log('正在刷新警报列表...');
        const params = { page: 1, page_size: 20 }; 
        const response = await fetchData('/sensor', params);
        
        // 更新刷新时间
        lastRefreshTime = new Date();
        
        alarmListEl.innerHTML = ''; 
        if (response && response.data) {
            updateSystemStatus(true);
            const alarms = response.data
                .filter(d => d.alarm_status && d.alarm_status.toLowerCase() !== 'normal' && d.alarm_status.toLowerCase() !== 'ok' && d.alarm_status.toLowerCase() !== 'none')
                .sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp)) 
                .slice(0, 5); 

            if (alarms.length === 0) {
                alarmListEl.innerHTML = '<li class="no-alarm">暂无警报</li>';
            } else {
                alarms.forEach(alarm => {
                    const li = document.createElement('li');
                    const timestamp = alarm.timestamp ? formatLocalDateTime(new Date(alarm.timestamp)) : '未知时间';
                    
                    // 确定警报类型和显示样式
                    const alarmType = getAlarmType(alarm.alarm_status);
                    li.className = `alarm-item ${alarmType.className}`;
                    
                    // 构建警报内容HTML结构
                    li.innerHTML = `
                        <div class="alarm-header">
                            <span class="alarm-icon">${alarmType.icon}</span>
                            <span class="alarm-status">${alarmType.displayName}</span>
                            <span class="alarm-time">${timestamp}</span>
                        </div>
                        <div class="alarm-device">设备 ${alarm.device_id || '未知设备'}</div>
                        <div class="alarm-details">
                            <span class="detail-item"><span class="detail-label">温度:</span> ${alarm.temperature !== undefined ? alarm.temperature.toFixed(1) : 'N/A'}°C</span>
                            <span class="detail-item"><span class="detail-label">湿度:</span> ${alarm.humidity !== undefined ? alarm.humidity.toFixed(1) : 'N/A'}%</span>
                            <span class="detail-item"><span class="detail-label">CO:</span> ${alarm.co_ppm !== undefined ? alarm.co_ppm.toFixed(1) : 'N/A'} PPM</span>
                            <span class="detail-item"><span class="detail-label">粉尘:</span> ${alarm.dust_density !== undefined ? alarm.dust_density.toFixed(1) : 'N/A'} µg/m³</span>
                        </div>
                    `;
                    
                    alarmListEl.appendChild(li);
                });
            }
        } else {
             alarmListEl.innerHTML = '<li class="error-alarm">无法加载告警信息</li>';
             updateSystemStatus(false);
        }
        
        // 更新最后刷新状态显示
        const refreshStatusEl = document.querySelector('#alarm-refresh-status');
        if (refreshStatusEl) {
            const formattedTime = new Date().toLocaleTimeString();
            refreshStatusEl.textContent = `最后刷新: ${formattedTime}`;
            refreshStatusEl.style.display = 'block';
        }
    }

    // 根据警报状态确定类型、图标和中文显示名称
    function getAlarmType(alarmStatus) {
        if (!alarmStatus) return { className: 'alarm-unknown', icon: '❓', displayName: '未知警报' };
        
        const status = alarmStatus.toLowerCase();
        
        // 粉尘相关警报
        if (status.includes('dust')) {
            if (status.includes('high')) {
                return { 
                    className: 'alarm-dust', 
                    icon: '💨', 
                    displayName: '粉尘浓度过高' 
                };
            } else if (status.includes('critical')) {
                return { 
                    className: 'alarm-critical', 
                    icon: '⚠️', 
                    displayName: '粉尘浓度严重超标' 
                };
            } else {
                return { 
                    className: 'alarm-dust', 
                    icon: '💨', 
                    displayName: '粉尘浓度异常' 
                };
            }
        } 
        // 温度相关警报
        else if (status.includes('temp') || status.includes('temperature')) {
            if (status.includes('high')) {
                return { 
                    className: 'alarm-temp', 
                    icon: '🔥', 
                    displayName: '温度过高' 
                };
            } else if (status.includes('low')) {
                return { 
                    className: 'alarm-temp', 
                    icon: '❄️', 
                    displayName: '温度过低' 
                };
            } else {
                return { 
                    className: 'alarm-temp', 
                    icon: '🌡️', 
                    displayName: '温度异常' 
                };
            }
        } 
        // 湿度相关警报
        else if (status.includes('humid') || status.includes('humidity')) {
            if (status.includes('high')) {
                return { 
                    className: 'alarm-humid', 
                    icon: '💧', 
                    displayName: '湿度过高' 
                };
            } else if (status.includes('low')) {
                return { 
                    className: 'alarm-humid', 
                    icon: '🏜️', 
                    displayName: '湿度过低' 
                };
            } else {
                return { 
                    className: 'alarm-humid', 
                    icon: '💧', 
                    displayName: '湿度异常' 
                };
            }
        } 
        // CO相关警报
        else if (status.includes('co') || status.includes('carbon')) {
            return { 
                className: 'alarm-co', 
                icon: '☁️', 
                displayName: '一氧化碳浓度过高' 
            };
        } 
        // 一般高值警报
        else if (status.includes('high')) {
            return { 
                className: 'alarm-high', 
                icon: '⚠️', 
                displayName: '数值过高警报' 
            };
        } 
        // 紧急或危险警报
        else if (status.includes('critical') || status.includes('danger')) {
            return { 
                className: 'alarm-critical', 
                icon: '🔴', 
                displayName: '紧急警报' 
            };
        } 
        // 未知类型警报
        else {
            // 尝试从英文状态中提取有用信息并翻译
            return { 
                className: 'alarm-generic', 
                icon: '⚠️', 
                displayName: translateAlarmStatus(alarmStatus) 
            };
        }
    }
    
    // 将常见的警报状态文本翻译为中文
    function translateAlarmStatus(statusText) {
        if (!statusText) return '未知警报';
        
        // 移除可能的数字和符号，仅保留核心文本
        const simplifiedStatus = statusText.replace(/[0-9.]+/g, '').trim();
        
        // 常见警报状态的映射表
        const translations = {
            'dust high': '粉尘浓度过高',
            'co high': '一氧化碳浓度过高',
            'temperature high': '温度过高',
            'humidity high': '湿度过高',
            'temperature low': '温度过低',
            'humidity low': '湿度过低',
            'alarm': '警报',
            'warning': '警告',
            'error': '错误',
            'alert': '警报',
            'critical': '严重警报',
            'danger': '危险警报',
            'emergency': '紧急情况',
            'pm high': '颗粒物浓度过高',
            'smoke detected': '检测到烟雾',
            'gas detected': '检测到气体泄漏'
        };
        
        // 查找最匹配的翻译
        for (const [key, value] of Object.entries(translations)) {
            if (simplifiedStatus.toLowerCase().includes(key.toLowerCase())) {
                return value;
            }
        }
        
        // 如果找不到匹配项，保留原文并添加备注
        return `警报: ${statusText}`;
    }

    async function fetchAndDisplayLiveDataCard() {
        const selectedDeviceId = liveDeviceIDSel.value;
        if (!selectedDeviceId) {
            liveDeviceIDEl.textContent = 'N/A';
            [liveTempEl, liveHumidityEl, liveCoEl, liveDustEl, liveAlarmEl, liveTimestampEl].forEach(el => el.textContent = '--');
            return;
        }

        const params = { device_id: selectedDeviceId, page: 1, page_size: 1 }; 
        const response = await fetchData('/sensor', params);
        if (response && response.data && response.data.length > 0) {
            updateSystemStatus(true);
            const data = response.data[0];
            liveDeviceIDEl.textContent = data.device_id || 'N/A';
            liveTempEl.textContent = data.temperature !== undefined ? data.temperature.toFixed(2) : '--';
            liveHumidityEl.textContent = data.humidity !== undefined ? data.humidity.toFixed(2) : '--';
            liveCoEl.textContent = data.co_ppm !== undefined ? data.co_ppm.toFixed(2) : '--';
            liveDustEl.textContent = data.dust_density !== undefined ? data.dust_density.toFixed(2) : '--';
            liveAlarmEl.textContent = data.alarm_status || '--';
            liveTimestampEl.textContent = data.timestamp ? formatLocalDateTime(new Date(data.timestamp)) : '--';
        } else {
            liveDeviceIDEl.textContent = selectedDeviceId;
            [liveTempEl, liveHumidityEl, liveCoEl, liveDustEl, liveAlarmEl, liveTimestampEl].forEach(el => el.textContent = '无数据');
        }
    }

    async function renderHistoricalChart() {
        const deviceId = chartDeviceSel.value;
        const metric = chartMetricSel.value;
        const timeRange = chartTimeRangeSel.value;

        console.log(`尝试渲染图表。设备: ${deviceId}, 指标: ${metric}, 时间范围: ${timeRange}`);

        if (!deviceId) {
            console.log('未选择设备，清除图表。');
            if (historicalChart) {
                historicalChart.destroy();
                historicalChart = null;
            }
            // 重置Canvas尺寸
            resetCanvasSize();
            ctx.font = '16px Arial';
            ctx.textAlign = 'center';
            ctx.fillText('请选择一个设备以显示图表', canvasEl.width/2, canvasEl.height/2);
            return;
        }

        // 获取系统当前时间（假设这是服务器的最新数据时间点）
        let endTime = new Date();
        // 根据选定的时间范围确定查询的起始时间
        let startTime;
        switch (timeRange) {
            case '1m': startTime = new Date(endTime.getTime() - 1 * 60 * 1000); break;
            case '5m': startTime = new Date(endTime.getTime() - 5 * 60 * 1000); break;
            case '10m': startTime = new Date(endTime.getTime() - 10 * 60 * 1000); break;
            case '30m': startTime = new Date(endTime.getTime() - 30 * 60 * 1000); break;
            case '1h': startTime = new Date(endTime.getTime() - 1 * 60 * 60 * 1000); break;
            case '3h': startTime = new Date(endTime.getTime() - 3 * 60 * 60 * 1000); break;
            case '6h': startTime = new Date(endTime.getTime() - 6 * 60 * 60 * 1000); break;
            case '12h': startTime = new Date(endTime.getTime() - 12 * 60 * 60 * 1000); break;
            case '1d': startTime = new Date(endTime.getTime() - 1 * 24 * 60 * 60 * 1000); break;
            case '2d': startTime = new Date(endTime.getTime() - 2 * 24 * 60 * 60 * 1000); break;
            case '3d': startTime = new Date(endTime.getTime() - 3 * 24 * 60 * 60 * 1000); break;
            default: startTime = new Date(endTime.getTime() - 5 * 60 * 1000); // 默认5分钟
        }

        // 日期时间格式化和处理
        // 移除毫秒部分并确保时区处理正确
        const formatDateForAPI = (date) => {
            // 截取ISO字符串，移除小数部分的毫秒并保留Z表示UTC
            return date.toISOString().split('.')[0] + 'Z';
        };
        
        const formattedStartTime = formatDateForAPI(startTime);
        const formattedEndTime = formatDateForAPI(endTime);

        console.log(`查询时间范围: 开始=${formattedStartTime}, 结束=${formattedEndTime}`);

        // 根据时间范围计算合适的数据点数量
        let pageSizeByRange;
        switch (timeRange) {
            case '1m': pageSizeByRange = 60; break;    // 每秒一个点
            case '5m': pageSizeByRange = 150; break;   // 每2秒一个点
            case '10m': pageSizeByRange = 200; break;  // 每3秒一个点
            case '30m': pageSizeByRange = 300; break;  // 每6秒一个点
            case '1h': pageSizeByRange = 360; break;   // 每10秒一个点
            case '3h': pageSizeByRange = 540; break;   // 每20秒一个点
            case '6h': pageSizeByRange = 720; break;   // 每30秒一个点
            case '12h': pageSizeByRange = 720; break;  // 每1分钟一个点
            case '1d': pageSizeByRange = 1440; break;  // 每1分钟一个点
            case '2d': pageSizeByRange = 1440; break;  // 每2分钟一个点
            case '3d': pageSizeByRange = 1440; break;  // 每3分钟一个点
            default: pageSizeByRange = 200;
        }

        const params = {
            device_id: deviceId,
            start_time: formattedStartTime,
            end_time: formattedEndTime,
            page_size: pageSizeByRange,
            skip_pagination: "true" // 告诉后端跳过分页限制，返回所有匹配的数据
        };
        console.log('请求图表数据参数:', JSON.stringify(params));

        try {
            // 清理之前的图表
            if (historicalChart) {
                historicalChart.destroy();
                historicalChart = null;
                console.log('已销毁之前的图表。');
            }
            // 重置Canvas尺寸
            resetCanvasSize();
            
            // 显示加载消息
            ctx.font = '16px Arial';
            ctx.textAlign = 'center';
            ctx.fillText('正在加载数据...', canvasEl.width/2, canvasEl.height/2);
            
            const response = await fetchData('/sensor', params);
            
            // 处理API响应为null或data为null的情况
            if (!response) {
                throw new Error('API请求失败');
            }
            
            // 处理data为null或数据长度为0的特殊情况
            if (response.data === null || response.data.length === 0) {
                console.log('API返回的data为null或为空数组，保持原有时间范围再试一次');
                // 使用更大的 page_size 并确保跳过分页限制
                const retryParams = { 
                    device_id: deviceId, 
                    start_time: formattedStartTime,
                    end_time: formattedEndTime,
                    page_size: pageSizeByRange * 2, // 尝试更大的 page_size
                    skip_pagination: "true"
                };
                
                console.log('重试查询参数:', JSON.stringify(retryParams));
                const retryResponse = await fetchData('/sensor', retryParams);
                
                if (!retryResponse || !retryResponse.data || retryResponse.data.length === 0) {
                    resetCanvasSize();
                    ctx.fillText(`设备 ${deviceId} 在选定时间范围内无数据`, canvasEl.width/2, canvasEl.height/2);
                    console.log('在指定的时间范围内没有数据');
                    return;
                }
                
                // 使用重试后获得的数据
                response.data = retryResponse.data;
                console.log(`找到 ${response.data.length} 条在指定时间范围内的数据记录`);
            }
            
            updateSystemStatus(true);
            console.log(`收到 ${response.data.length} 条数据点`);
            
            // 确保数据按时间排序
            const sortedData = response.data.sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));
            console.log('数据按时间戳排序:', sortedData.slice(0, 3)); // 仅显示前3条用于调试
            
            // 提取图表数据
            const chartData = sortedData.map(d => {
                // 将时间戳转换为本地时间字符串
                const timestamp = d.timestamp ? new Date(d.timestamp) : null;
                // 提取当前选择的指标值，确保存在且有效
                let value = null;
                
                if (d[metric] !== undefined && d[metric] !== null) {
                    // 确保值是数字
                    value = typeof d[metric] === 'string' ? parseFloat(d[metric]) : d[metric];
                    
                    // 检查是否为有效数字
                    if (isNaN(value)) {
                        console.warn(`无效的${metric}值:`, d[metric]);
                        value = null;
                    }
                }
                
                return {
                    timestamp: timestamp,
                    value: value
                };
            }).filter(item => item.timestamp && item.value !== null);
            
            console.log(`处理 ${chartData.length} 个有效数据点`);
            
            if (chartData.length === 0) {
                resetCanvasSize();
                ctx.fillText(`设备 ${deviceId} 在选定时间范围内无有效 ${metric} 数据`, canvasEl.width/2, canvasEl.height/2);
                console.log('筛选后无有效数据点');
                return;
            }
            
            // 准备图表数据
            const labels = chartData.map(d => formatLocalDateTime(d.timestamp));
            const values = chartData.map(d => d.value);
            
            console.log('图表标签示例:', labels.slice(0, 5));
            console.log('图表数值示例:', values.slice(0, 5));
            
            // 创建新图表
            historicalChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: getMetricLabel(metric, deviceId),
                        data: values,
                        borderColor: 'rgb(75, 192, 192)',
                        backgroundColor: 'rgba(75, 192, 192, 0.2)',
                        tension: 0.1,
                        fill: true,
                        spanGaps: true,
                        pointRadius: 3,
                        pointHoverRadius: 5
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true, // 修改为true，保持纵横比
                    animation: {
                        duration: 1000
                    },
                    scales: {
                        x: {
                            title: {
                                display: true,
                                text: '时间'
                            },
                            ticks: {
                                maxRotation: 45,
                                minRotation: 45,
                                maxTicksLimit: 10 // 限制X轴刻度数量，避免过于拥挤
                            }
                        },
                        y: {
                            beginAtZero: metric === 'co_ppm' || metric === 'dust_density' ? true : false,
                            title: {
                                display: true,
                                text: getMetricUnit(metric)
                            }
                        }
                    },
                    plugins: {
                        tooltip: {
                            mode: 'index',
                            intersect: false,
                            callbacks: {
                                label: function(context) {
                                    let label = context.dataset.label || '';
                                    if (label) {
                                        label += ': ';
                                    }
                                    if (context.parsed.y !== null) {
                                        label += context.parsed.y.toFixed(2) + ' ' + getMetricUnitShort(metric);
                                    }
                                    return label;
                                }
                            }
                        },
                        legend: {
                            position: 'top',
                        }
                    }
                }
            });
            console.log('图表渲染成功。');
        } catch (error) {
            console.error('图表渲染错误:', error);
            resetCanvasSize();
            ctx.font = '16px Arial';
            ctx.textAlign = 'center';
            ctx.fillText(`图表渲染错误: ${error.message}`, canvasEl.width/2, canvasEl.height/2);
            updateSystemStatus(false);
        }
    }
    
    // 重置Canvas尺寸函数
    function resetCanvasSize() {
        // 清除Canvas内容
        ctx.clearRect(0, 0, canvasEl.width, canvasEl.height);
        
        // 获取父容器尺寸
        const container = canvasEl.parentElement;
        const containerWidth = container.clientWidth;
        
        // 设置固定高度（根据需要调整，这里设置为容器宽度的65%以保持合适的纵横比）
        const targetHeight = Math.floor(containerWidth * 0.65);
        
        // 重置Canvas元素尺寸
        canvasEl.style.height = `${targetHeight}px`;
        canvasEl.style.width = '100%';
        
        // 设置Canvas实际像素尺寸
        canvasEl.width = containerWidth;
        canvasEl.height = targetHeight;
        
        console.log(`已重置Canvas尺寸: ${containerWidth}x${targetHeight}`);
    }
    
    // 辅助函数：获取指标友好名称
    function getMetricLabel(metric, deviceId) {
        const metricLabels = {
            'temperature': `温度（设备 ${deviceId}）`,
            'humidity': `湿度（设备 ${deviceId}）`,
            'co_ppm': `一氧化碳浓度（设备 ${deviceId}）`,
            'dust_density': `粉尘浓度（设备 ${deviceId}）`
        };
        return metricLabels[metric] || metric;
    }
    
    // 辅助函数：获取指标单位
    function getMetricUnit(metric) {
        const metricUnits = {
            'temperature': '温度 (°C)',
            'humidity': '湿度 (%)',
            'co_ppm': '一氧化碳浓度 (PPM)',
            'dust_density': '粉尘浓度 (µg/m³)'
        };
        return metricUnits[metric] || metric;
    }
    
    // 辅助函数：获取指标简短单位
    function getMetricUnitShort(metric) {
        const metricUnits = {
            'temperature': '°C',
            'humidity': '%',
            'co_ppm': 'PPM',
            'dust_density': 'µg/m³'
        };
        return metricUnits[metric] || '';
    }

    // Event Listeners
    liveDeviceIDSel.addEventListener('change', fetchAndDisplayLiveDataCard);
    chartDeviceSel.addEventListener('change', renderHistoricalChart);
    chartMetricSel.addEventListener('change', renderHistoricalChart);
    chartTimeRangeSel.addEventListener('change', renderHistoricalChart);
    refreshChartBtn.addEventListener('click', renderHistoricalChart);

    // ESP32控制模块功能
    function initDeviceControl() {
        restartDeviceBtn.addEventListener('click', async () => {
            const selectedDeviceId = controlDeviceSel.value;
            if (!selectedDeviceId) {
                showControlStatus('请选择设备', 'error');
                return;
            }
            
            if (!confirm(`确定要重启设备 ${selectedDeviceId} 吗？`)) {
                return;
            }
            
            await restartDevice(selectedDeviceId);
        });
    }
    
    // 显示控制操作的状态消息
    function showControlStatus(message, type = 'success') {
        controlStatusEl.textContent = message;
        controlStatusEl.className = 'status-message';
        
        if (type === 'success') {
            controlStatusEl.classList.add('status-success');
        } else if (type === 'error') {
            controlStatusEl.classList.add('status-error');
        }
        
        // 5秒后自动隐藏消息
        setTimeout(() => {
            controlStatusEl.style.display = 'none';
        }, 5000);
    }
    
    // 重启ESP32设备
    async function restartDevice(deviceId) {
        try {
            restartDeviceBtn.disabled = true;
            restartDeviceBtn.textContent = '正在重启...';
            
            const url = `${API_BASE_URL}/device/${deviceId}/command`;
            const response = await fetch(url, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    command: 'restart_device',
                    parameters: {}
                })
            });
            
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`请求失败 (${response.status}): ${errorText}`);
            }
            
            const data = await response.json();
            console.log('重启命令已发送:', data);
            
            showControlStatus(`重启命令已发送至 ${deviceId}`, 'success');
        } catch (error) {
            console.error('重启设备出错:', error);
            showControlStatus(`重启设备失败: ${error.message}`, 'error');
        } finally {
            restartDeviceBtn.disabled = false;
            restartDeviceBtn.textContent = '重启设备';
        }
    }

    initializePage();
}); 