package middleware

import (
	"log"
	"time"

	"github.com/gin-gonic/gin"
)

// Logger 返回一个Gin中间件，用于记录HTTP请求日志
func Logger() gin.HandlerFunc {
	return func(c *gin.Context) {
		// 开始时间
		startTime := time.Now()
		
		// 处理请求
		c.Next()
		
		// 结束时间
		endTime := time.Now()
		
		// 执行时间
		latency := endTime.Sub(startTime)
		
		// 请求方法
		reqMethod := c.Request.Method
		
		// 请求路由
		reqURI := c.Request.RequestURI
		
		// 状态码
		statusCode := c.Writer.Status()
		
		// 客户端IP
		clientIP := c.ClientIP()
		
		// 记录日志
		log.Printf("[HTTP] | %3d | %13v | %15s | %s | %s",
			statusCode,
			latency,
			clientIP,
			reqMethod,
			reqURI,
		)
	}
} 