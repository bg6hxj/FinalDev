# 构建阶段
FROM golang:1.21-alpine AS builder

RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories

# 设置构建参数
WORKDIR /app
COPY . .

ENV GOPROXY=https://goproxy.cn,direct
ENV GONOSUMDB=*
ENV GO111MODULE=on

# 安装依赖并编译
RUN go mod download
RUN CGO_ENABLED=0 GOOS=linux go build -a -o server ./cmd/server

# 运行阶段
FROM alpine:latest

# 更换软件源为http，尝试解决apk add卡住的问题
RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories

# 安装必要的软件包
RUN apk --no-cache add ca-certificates tzdata postgresql-client bash

# 设置工作目录
WORKDIR /app

# 从构建阶段复制编译好的应用
COPY --from=builder /app/server /app/
COPY --from=builder /app/configs /app/configs
COPY --from=builder /app/migrations /app/migrations
COPY --from=builder /app/scripts/wait-for-db.sh /app/wait-for-db.sh

# 确保脚本可执行
RUN chmod +x /app/wait-for-db.sh

# 设置时区
ENV TZ=Asia/Shanghai

# 暴露端口
EXPOSE 8080

# 运行应用
ENTRYPOINT ["/app/wait-for-db.sh", "db", "5432", "postgres", "postgres", "arm_detector_db", "./server"] 