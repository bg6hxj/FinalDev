# 使用轻量级的 nginx 镜像
FROM nginx:alpine

# 复制 HTML, CSS, JS 文件到 nginx 的 www 目录
COPY ./public/ /usr/share/nginx/html/

# 复制自定义的 nginx 配置文件
COPY nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"] 