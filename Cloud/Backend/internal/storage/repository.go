package storage

import (
	"fmt"
	"log"
	"time"

	"github.com/arm-detector/backend/internal/models"
)

// Repository 定义数据库操作接口
type Repository interface {
	// 传感器数据操作
	SaveSensorData(data *models.SensorData) error
	GetSensorData(id int64) (*models.SensorData, error)
	QuerySensorData(query *models.SensorDataQuery) (*models.SensorDataResponse, error)
	
	// 命令日志操作
	SaveCommandLog(log *models.CommandLog) error
	UpdateCommandStatus(requestID string, status string, response string) error
	GetCommandLog(requestID string) (*models.CommandLog, error)
}

// PostgresRepository 实现Repository接口
type PostgresRepository struct {
	db *Database
}

// NewRepository 创建新的数据库仓库
func NewRepository(db *Database) Repository {
	return &PostgresRepository{db: db}
}

// SaveSensorData 保存传感器数据到数据库
func (r *PostgresRepository) SaveSensorData(data *models.SensorData) error {
	query := `
		INSERT INTO sensor_data (device_id, timestamp, temperature, humidity, co_ppm, dust_density, alarm_status, received_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
		RETURNING id
	`
	
	// 确保时间戳是UTC
	data.Timestamp = data.Timestamp.UTC()
	data.ReceivedAt = time.Now().UTC()
	
	err := r.db.DB.QueryRow(
		query,
		data.DeviceID,
		data.Timestamp,
		data.Temperature,
		data.Humidity,
		data.CoPPM,
		data.DustDensity,
		data.AlarmStatus,
		data.ReceivedAt,
	).Scan(&data.ID)
	
	if err != nil {
		return fmt.Errorf("保存传感器数据失败: %w", err)
	}
	
	return nil
}

// GetSensorData 根据ID获取传感器数据
func (r *PostgresRepository) GetSensorData(id int64) (*models.SensorData, error) {
	query := `
		SELECT id, device_id, timestamp, temperature, humidity, co_ppm, dust_density, alarm_status, received_at
		FROM sensor_data
		WHERE id = $1
	`
	
	var data models.SensorData
	err := r.db.DB.Get(&data, query, id)
	if err != nil {
		return nil, fmt.Errorf("获取传感器数据失败: %w", err)
	}
	
	return &data, nil
}

// QuerySensorData 根据查询条件获取传感器数据列表
func (r *PostgresRepository) QuerySensorData(query *models.SensorDataQuery) (*models.SensorDataResponse, error) {
	// 构建查询条件
	var conditions []string
	var args []interface{}
	var whereClause string
	var argIndex int = 1

	if query.DeviceID != "" {
		conditions = append(conditions, fmt.Sprintf("device_id = $%d", argIndex))
		args = append(args, query.DeviceID)
		argIndex++
	}

	if !query.StartTime.IsZero() {
		// 确保StartTime是UTC时间
		query.StartTime = query.StartTime.UTC()
		conditions = append(conditions, fmt.Sprintf("timestamp >= $%d", argIndex))
		args = append(args, query.StartTime)
		argIndex++
	}

	if !query.EndTime.IsZero() {
		// 确保EndTime是UTC时间
		query.EndTime = query.EndTime.UTC()
		conditions = append(conditions, fmt.Sprintf("timestamp <= $%d", argIndex))
		args = append(args, query.EndTime)
		argIndex++
	}

	if len(conditions) > 0 {
		whereClause = " WHERE " + conditions[0]
		for i := 1; i < len(conditions); i++ {
			whereClause += " AND " + conditions[i]
		}
	}

	// 计算总记录数
	countQuery := "SELECT COUNT(*) FROM sensor_data" + whereClause
	var totalItems int64
	err := r.db.DB.Get(&totalItems, countQuery, args...)
	if err != nil {
		return nil, fmt.Errorf("统计记录数失败: %w", err)
	}

	// 计算分页信息
	if query.Page < 1 {
		query.Page = 1
	}
	if query.PageSize < 1 {
		query.PageSize = 20
	}

	// 根据是否跳过分页来设置limit和offset
	var limit, offset int
	if query.SkipPagination {
		// 使用页面大小作为直接的限制，不使用偏移
		limit = query.PageSize
		offset = 0
	} else {
		// 正常分页
		offset = (query.Page - 1) * query.PageSize
		limit = query.PageSize
	}
	
	totalPages := (int(totalItems) + query.PageSize - 1) / query.PageSize

	// 查询数据
	dataQuery := fmt.Sprintf(`
		SELECT id, device_id, timestamp, temperature, humidity, co_ppm, dust_density, alarm_status, received_at
		FROM sensor_data
		%s
		ORDER BY timestamp DESC
		LIMIT $%d OFFSET $%d
	`, whereClause, argIndex, argIndex+1)

	args = append(args, limit, offset)
	var sensorData []models.SensorData
	err = r.db.DB.Select(&sensorData, dataQuery, args...)
	if err != nil {
		return nil, fmt.Errorf("查询传感器数据失败: %w", err)
	}

	// 添加调试日志
	if len(sensorData) > 0 {
		log.Printf("DEBUG: Fetched SensorData from DB. First entry timestamp: %s, Location: %s, UTC value: %s",
			sensorData[0].Timestamp.String(),
			sensorData[0].Timestamp.Location().String(),
			sensorData[0].Timestamp.UTC().String())
		log.Printf("DEBUG: Fetched SensorData from DB. First entry received_at: %s, Location: %s, UTC value: %s",
			sensorData[0].ReceivedAt.String(),
			sensorData[0].ReceivedAt.Location().String(),
			sensorData[0].ReceivedAt.UTC().String())
	}

	// 构建响应
	result := &models.SensorDataResponse{
		Data: sensorData,
		Pagination: models.PaginationResult{
			CurrentPage: query.Page,
			PageSize:    query.PageSize,
			TotalItems:  totalItems,
			TotalPages:  totalPages,
		},
	}

	return result, nil
}

// SaveCommandLog 保存命令日志
func (r *PostgresRepository) SaveCommandLog(log *models.CommandLog) error {
	query := `
		INSERT INTO command_logs (device_id, command, request_id, status, sent_at, response)
		VALUES ($1, $2, $3, $4, $5, $6)
		RETURNING id
	`
	
	err := r.db.DB.QueryRow(
		query,
		log.DeviceID,
		log.Command,
		log.RequestID,
		log.Status,
		log.SentAt,
		log.Response,
	).Scan(&log.ID)
	
	if err != nil {
		return fmt.Errorf("保存命令日志失败: %w", err)
	}
	
	return nil
}

// UpdateCommandStatus 更新命令状态
func (r *PostgresRepository) UpdateCommandStatus(requestID string, status string, response string) error {
	query := `
		UPDATE command_logs
		SET status = $1, response = $2
		WHERE request_id = $3
	`
	
	_, err := r.db.DB.Exec(query, status, response, requestID)
	if err != nil {
		return fmt.Errorf("更新命令状态失败: %w", err)
	}
	
	return nil
}

// GetCommandLog 获取命令日志
func (r *PostgresRepository) GetCommandLog(requestID string) (*models.CommandLog, error) {
	query := `
		SELECT id, device_id, command, request_id, status, sent_at, response
		FROM command_logs
		WHERE request_id = $1
	`
	
	var log models.CommandLog
	err := r.db.DB.Get(&log, query, requestID)
	if err != nil {
		return nil, fmt.Errorf("获取命令日志失败: %w", err)
	}
	
	return &log, nil
} 