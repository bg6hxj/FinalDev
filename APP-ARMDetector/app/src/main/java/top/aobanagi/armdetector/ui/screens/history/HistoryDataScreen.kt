package top.aobanagi.armdetector.ui.screens.history

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember

import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState

import androidx.lifecycle.viewmodel.compose.viewModel
import top.aobanagi.armdetector.ui.components.DeviceSelector
import top.aobanagi.armdetector.ui.theme.CoDangerous
import top.aobanagi.armdetector.ui.theme.DustDangerous
import top.aobanagi.armdetector.ui.theme.HumidityHigh
import top.aobanagi.armdetector.ui.theme.TemperatureWarm
import android.graphics.Paint
import android.graphics.Typeface
import kotlin.math.pow

@Composable
fun HistoryDataScreen(viewModel: HistoryViewModel = viewModel()) {
    val uiState by viewModel.uiState.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        Text(
            "历史数据图表",
            style = MaterialTheme.typography.titleLarge,
            fontWeight = FontWeight.Bold
        )

        Spacer(modifier = Modifier.height(16.dp))

        // 设备选择器
        DeviceSelector(
            devices = uiState.availableDevices,
            selectedDevice = uiState.selectedDevice,
            onDeviceSelected = { viewModel.selectDevice(it) }
        )

        Spacer(modifier = Modifier.height(8.dp))

        // 数据类型选择器
        DataTypeSelector(
            selectedType = uiState.selectedDataType,
            onTypeSelected = { viewModel.selectDataType(it) }
        )

        Spacer(modifier = Modifier.height(8.dp))

        // 时间范围选择器
        TimeRangeSelector(
            selectedRange = uiState.selectedTimeRange,
            onRangeSelected = { viewModel.selectTimeRange(it) }
        )

        Spacer(modifier = Modifier.height(16.dp))

        // 图表展示
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f),
            elevation = CardDefaults.cardElevation(4.dp)
        ) {
            if (uiState.isLoading) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    CircularProgressIndicator(color = MaterialTheme.colorScheme.primary)
                }
            } else if (uiState.errorMessage != null) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = uiState.errorMessage ?: "",
                        color = Color.Red,
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
            } else if (uiState.chartData.isEmpty()) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "无数据可显示",
                        style = MaterialTheme.typography.bodyLarge
                    )
                }
            } else {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        getChartTitle(uiState.selectedDataType),
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold
                    )

                    Spacer(modifier = Modifier.height(16.dp))

                    // 简单的自定义图表
                    SimpleLineChart(
                        data = uiState.chartData,
                        dataType = uiState.selectedDataType,
                        timestamps = uiState.timestamps,
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f)
                    )
                }
            }
        }
    }
}

@Composable
fun DataTypeSelector(
    selectedType: DataType,
    onTypeSelected: (DataType) -> Unit,
    modifier: Modifier = Modifier
) {
    val types = DataType.values()

    Column(modifier = modifier.fillMaxWidth()) {
        Text(
            "数据类型",
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(bottom = 4.dp)
        )

        Row(
            modifier = Modifier.fillMaxWidth()
        ) {
            types.forEach { type ->
                androidx.compose.material3.FilterChip(
                    selected = type == selectedType,
                    onClick = { onTypeSelected(type) },
                    label = { Text(getDataTypeLabel(type)) },
                    modifier = Modifier.padding(end = 8.dp)
                )
            }
        }
    }
}

@Composable
fun TimeRangeSelector(
    selectedRange: TimeRange,
    onRangeSelected: (TimeRange) -> Unit,
    modifier: Modifier = Modifier
) {
    val ranges = TimeRange.values()
    val scrollState = rememberScrollState()

    Column(modifier = modifier.fillMaxWidth()) {
        Text(
            "时间范围",
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(bottom = 4.dp)
        )

        // 使用horizontalScroll包装Row，使其可以水平滚动
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(scrollState)
        ) {
            ranges.forEach { range ->
                androidx.compose.material3.FilterChip(
                    selected = range == selectedRange,
                    onClick = { onRangeSelected(range) },
                    label = { Text(getTimeRangeLabel(range)) },
                    modifier = Modifier.padding(end = 8.dp)
                )
            }
        }
    }
}

@Composable
fun SimpleLineChart(
    data: List<Float>,
    dataType: DataType,
    timestamps: List<String> = emptyList(),
    modifier: Modifier = Modifier
) {
    if (data.isEmpty()) return

    val chartColor = getChartColor(dataType)

    // 添加状态跟踪选中的数据点
    val (selectedPointIndex, setSelectedPointIndex) = remember { mutableStateOf<Int?>(null) }

    Box(modifier = modifier) {
        Canvas(modifier = Modifier
            .fillMaxSize()
            .pointerInput(Unit) {
                detectTapGestures { offset ->
                    // 检测点击位置是否接近任意数据点
                    val canvasSize = size
                    val width = canvasSize.width
                    val height = canvasSize.height
                    val padding = 60f

                    val xStep = (width - 2 * padding) / (data.size - 1).coerceAtLeast(1)
                    val maxValue = data.maxOrNull() ?: 0f
                    val minValue = data.minOrNull() ?: 0f
                    val yRange = (maxValue - minValue).coerceAtLeast(1f)

                    // 寻找最近的点
                    var closestPointIndex = -1
                    var minDistance = Float.MAX_VALUE

                    for (i in data.indices) {
                        val x = padding + i * xStep
                        val y = height - padding - ((data[i] - minValue) / yRange) * (height - 2 * padding)

                        val distance = kotlin.math.sqrt((offset.x - x).pow(2) + (offset.y - y).pow(2))

                        if (distance < minDistance && distance < 30f) { // 30dp是触摸区域半径
                            minDistance = distance
                            closestPointIndex = i
                        }
                    }

                    // 如果找到了接近的点，设置为选中状态
                    setSelectedPointIndex(if (closestPointIndex >= 0) closestPointIndex else null)
                }
            }
        ) {
            val width = size.width
            val height = size.height
            val padding = 60f  // 增加内边距以容纳坐标轴标签

            val maxValue = data.maxOrNull() ?: 0f
            val minValue = data.minOrNull() ?: 0f

            // 计算Y轴范围，确保数据居中显示
            val dataRange = (maxValue - minValue).coerceAtLeast(1f)
            val yAxisMargin = dataRange * 0.1f // 增加10%的边距
            val yMin = (minValue - yAxisMargin).coerceAtMost(minValue * 0.9f)
            val yMax = (maxValue + yAxisMargin).coerceAtLeast(maxValue * 1.1f)
            val yRange = (yMax - yMin).coerceAtLeast(1f)

            val xStep = (width - 2 * padding) / (data.size - 1).coerceAtLeast(1)

            // 绘制背景颜色
            drawRect(
                color = Color(0xFFEBF5FB).copy(alpha = 0.5f), // 淡蓝色背景
                size = size
            )

            // 绘制网格线（淡色）
            val gridColor = Color.LightGray

            // 水平网格线
            val yStepCount = 5
            val yStepSize = (height - 2 * padding) / yStepCount

            for (i in 0..yStepCount) {
                val y = height - padding - i * yStepSize

                // 绘制水平网格线
                drawLine(
                    color = gridColor,
                    start = Offset(padding, y),
                    end = Offset(width - padding, y),
                    strokeWidth = 1f
                )
            }

            // 垂直网格线相关设置
            val gridLabelCount = minOf(5, data.size)
            if (data.size > 1 && gridLabelCount > 1) {
                val xLabelInterval = (data.size - 1) / (gridLabelCount - 1)

                for (i in 0 until gridLabelCount) {
                    val index = i * xLabelInterval
                    if (index < data.size) {
                        val x = padding + index * xStep

                        // 绘制垂直网格线
                        drawLine(
                            color = gridColor,
                            start = Offset(x, padding),
                            end = Offset(x, height - padding),
                            strokeWidth = 1f
                        )
                    }
                }
            }

            // 绘制坐标轴
            val axisColor = Color.Gray
            val axisStrokeWidth = 2f

            // 绘制Y轴
            drawLine(
                color = axisColor,
                start = Offset(padding, padding),
                end = Offset(padding, height - padding),
                strokeWidth = axisStrokeWidth
            )

            // 绘制X轴
            drawLine(
                color = axisColor,
                start = Offset(padding, height - padding),
                end = Offset(width - padding, height - padding),
                strokeWidth = axisStrokeWidth
            )

            // 准备画笔
            val textPaint = Paint().apply {
                color = android.graphics.Color.BLACK
                textSize = 32f
                textAlign = Paint.Align.CENTER
                typeface = Typeface.DEFAULT
            }

            val yAxisTitle = when(dataType) {
                DataType.TEMPERATURE -> "温度 (°C)"
                DataType.HUMIDITY -> "湿度 (%)"
                DataType.CO -> "一氧化碳 (PPM)"
                DataType.DUST -> "粉尘 (µg/m³)"
            }

            // 使用原生canvas绘制Y轴标题
            drawContext.canvas.nativeCanvas.apply {
                save()
                rotate(-90f, padding - 35f, height / 2)
                drawText(
                    yAxisTitle,
                    padding - 35f,
                    height / 2,
                    textPaint
                )
                restore()

                // 绘制X轴标题
                drawText(
                    "时间",
                    width / 2,
                    height - 10f,
                    textPaint
                )
            }

            // 绘制Y轴刻度（5个刻度）
            val valueStep = yRange / yStepCount
            val valuePaint = Paint().apply {
                color = android.graphics.Color.BLACK
                textSize = 30f
                textAlign = Paint.Align.RIGHT
            }

            for (i in 0..yStepCount) {
                val y = height - padding - i * yStepSize
                // 绘制刻度线
                drawLine(
                    color = axisColor,
                    start = Offset(padding - 5, y),
                    end = Offset(padding, y),
                    strokeWidth = axisStrokeWidth
                )

                // 绘制值标签
                val value = yMin + i * valueStep
                val valueText = String.format("%.1f", value)
                drawContext.canvas.nativeCanvas.drawText(
                    valueText,
                    padding - 10f,  // 标签位置向左偏移
                    y + 5f,         // 轻微垂直调整以居中
                    valuePaint
                )
            }

            // 绘制X轴刻度（根据数据点数量确定，最多5个）
            val xLabelCount = minOf(5, data.size)
            val xLabelPaint = Paint().apply {
                color = android.graphics.Color.BLACK
                textSize = 30f
                textAlign = Paint.Align.CENTER
            }

            if (data.size > 1 && xLabelCount > 1) {
                val xLabelInterval = (data.size - 1) / (xLabelCount - 1)

                for (i in 0 until xLabelCount) {
                    val index = i * xLabelInterval
                    if (index < data.size) {
                        val x = padding + index * xStep

                        // 绘制刻度线
                        drawLine(
                            color = axisColor,
                            start = Offset(x, height - padding),
                            end = Offset(x, height - padding + 5),
                            strokeWidth = axisStrokeWidth
                        )

                        // 使用时间戳格式化为时:分:秒格式
                        val xLabel = if (timestamps.isNotEmpty() && index < timestamps.size) {
                            timestamps[index]
                        } else {
                            if (i == 0) "开始" else if (i == xLabelCount - 1) "结束" else "${(i * 100 / (xLabelCount - 1))}%"
                        }

                        drawContext.canvas.nativeCanvas.drawText(
                            xLabel,
                            x,
                            height - padding + 25f,
                            xLabelPaint
                        )
                    }
                }
            }

            // 绘制数据线和点
            val strokeWidth = 2f
            val path = Path()

            // 开始画线
            for (i in data.indices) {
                val x = padding + i * xStep
                val y = height - padding - ((data[i] - yMin) / yRange) * (height - 2 * padding)

                if (i == 0) {
                    path.moveTo(x, y)
                } else {
                    path.lineTo(x, y)
                }

                // 画点
                val pointRadius = if (i == selectedPointIndex) 8f else 4f
                val pointColor = if (i == selectedPointIndex) chartColor.copy(alpha = 1f) else chartColor.copy(alpha = 0.8f)

                drawCircle(
                    color = pointColor,
                    radius = pointRadius,
                    center = Offset(x, y)
                )
            }

            // 绘制折线
            drawPath(
                path = path,
                color = chartColor,
                style = Stroke(width = strokeWidth * 2)
            )

            // 如果有选中的点，显示详细信息
            selectedPointIndex?.let { index ->
                if (index in data.indices) {
                    val x = padding + index * xStep
                    val y = height - padding - ((data[index] - yMin) / yRange) * (height - 2 * padding)

                    // 绘制提示框
                    val tooltipWidth = 200f
                    val tooltipHeight = 80f
                    val tooltipX = (x - tooltipWidth / 2).coerceIn(padding, width - padding - tooltipWidth)
                    val tooltipY = if (y > height / 2) y - tooltipHeight - 20 else y + 20

                    drawRoundRect(
                        color = Color.White,
                        topLeft = Offset(tooltipX, tooltipY),
                        size = androidx.compose.ui.geometry.Size(tooltipWidth, tooltipHeight),
                        cornerRadius = androidx.compose.ui.geometry.CornerRadius(8f),
                        style = androidx.compose.ui.graphics.drawscope.Fill
                    )

                    drawRoundRect(
                        color = chartColor.copy(alpha = 0.8f),
                        topLeft = Offset(tooltipX, tooltipY),
                        size = androidx.compose.ui.geometry.Size(tooltipWidth, tooltipHeight),
                        cornerRadius = androidx.compose.ui.geometry.CornerRadius(8f),
                        style = Stroke(width = 2f)
                    )

                    // 绘制提示文本
                    val tooltipTextPaint = Paint().apply {
                        color = android.graphics.Color.BLACK
                        textSize = 30f
                        textAlign = Paint.Align.LEFT
                    }

                    drawContext.canvas.nativeCanvas.apply {
                        // 绘制指标名称
                        val typeName = getDataTypeLabel(dataType)
                        drawText(
                            typeName,
                            tooltipX + 10,
                            tooltipY + 30,
                            tooltipTextPaint
                        )

                        // 绘制数值
                        val valueText = String.format("%.2f", data[index])
                        val unit = when(dataType) {
                            DataType.TEMPERATURE -> "°C"
                            DataType.HUMIDITY -> "%"
                            DataType.CO -> "PPM"
                            DataType.DUST -> "µg/m³"
                        }
                        drawText(
                            "$valueText $unit",
                            tooltipX + 10,
                            tooltipY + 65,
                            tooltipTextPaint.apply {
                                color = android.graphics.Color.parseColor("#FF4081")
                                isFakeBoldText = true
                            }
                        )
                    }
                }
            }
        }
    }
}

fun getDataTypeLabel(type: DataType): String {
    return when (type) {
        DataType.TEMPERATURE -> "温度"
        DataType.HUMIDITY -> "湿度"
        DataType.CO -> "一氧化碳"
        DataType.DUST -> "粉尘"
    }
}

fun getTimeRangeLabel(range: TimeRange): String {
    return when (range) {
        TimeRange.LAST_1_MINUTE -> "1分钟"
        TimeRange.LAST_5_MINUTES -> "5分钟"
        TimeRange.LAST_10_MINUTES -> "10分钟"
        TimeRange.LAST_30_MINUTES -> "30分钟"
        TimeRange.LAST_HOUR -> "1小时"
        TimeRange.LAST_6_HOURS -> "6小时"
        TimeRange.LAST_DAY -> "24小时"
    }
}

fun getChartTitle(type: DataType): String {
    return when (type) {
        DataType.TEMPERATURE -> "温度趋势图 (°C)"
        DataType.HUMIDITY -> "湿度趋势图 (%)"
        DataType.CO -> "一氧化碳浓度趋势图 (PPM)"
        DataType.DUST -> "粉尘浓度趋势图 (µg/m³)"
    }
}

fun getChartColor(type: DataType): Color {
    return when (type) {
        DataType.TEMPERATURE -> TemperatureWarm
        DataType.HUMIDITY -> HumidityHigh
        DataType.CO -> CoDangerous
        DataType.DUST -> DustDangerous
    }
}

//// 删除重复声明的枚举类，使用HistoryViewModel.kt中的定义
//typealias DataType = top.aobanagi.armdetector.ui.screens.history.DataType
//typealias TimeRange = top.aobanagi.armdetector.ui.screens.history.TimeRange