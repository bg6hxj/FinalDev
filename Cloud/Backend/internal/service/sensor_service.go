package service

import (
	"fmt"

	"github.com/arm-detector/backend/internal/models"
	"github.com/arm-detector/backend/internal/storage"
)

// SensorService 传感器数据服务
type SensorService struct {
	repo storage.Repository
}

// NewSensorService 创建新的传感器服务
func NewSensorService(repo storage.Repository) *SensorService {
	return &SensorService{
		repo: repo,
	}
}

// GetSensorData 根据ID获取传感器数据
func (s *SensorService) GetSensorData(id int64) (*models.SensorData, error) {
	data, err := s.repo.GetSensorData(id)
	if err != nil {
		return nil, fmt.Errorf("获取传感器数据失败: %w", err)
	}
	
	return data, nil
}

// QuerySensorData 查询传感器数据列表
func (s *SensorService) QuerySensorData(query *models.SensorDataQuery) (*models.SensorDataResponse, error) {
	// 处理默认值
	if query.Page <= 0 {
		query.Page = 1
	}
	
	if query.PageSize <= 0 {
		query.PageSize = 20
	} else if query.PageSize > 100 && !query.SkipPagination {
		// 限制最大页面大小，除非特别指定跳过分页
		query.PageSize = 100
	} else if query.SkipPagination {
		// 当跳过分页时，允许较大的数据集，但仍设置一个上限以防止过度消耗服务器资源
		// 通常历史数据图表不需要超过此数量的数据点
		if query.PageSize > 3000 {
			query.PageSize = 3000
		}
	}
	
	// 查询数据
	response, err := s.repo.QuerySensorData(query)
	if err != nil {
		return nil, fmt.Errorf("查询传感器数据失败: %w", err)
	}
	
	return response, nil
} 