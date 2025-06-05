#!/bin/sh
# 等待PostgreSQL数据库准备就绪

set -e

host="$1"
port="$2"
user="$3"
password="$4"
dbname="$5"
shift 5
cmd="$@"

until PGPASSWORD=$password psql -h "$host" -p "$port" -U "$user" -d "postgres" -c '\q'; do
  >&2 echo "Postgres 还没有准备好 - 等待..."
  sleep 1
done

# 检查数据库是否存在，如果不存在则创建
echo "检查数据库是否存在..."
if PGPASSWORD=$password psql -h "$host" -p "$port" -U "$user" -lqt | cut -d \| -f 1 | grep -qw "$dbname"; then
    echo "$dbname 数据库已存在"
else
    echo "创建 $dbname 数据库..."
    PGPASSWORD=$password psql -h "$host" -p "$port" -U "$user" -c "CREATE DATABASE $dbname;" postgres
    echo "$dbname 数据库已创建"
fi

# 运行传入的命令
>&2 echo "Postgres 已准备就绪，执行: $cmd"
exec $cmd 