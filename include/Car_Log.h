#ifndef _CAR_LOG_H_
#define _CAR_LOG_H_

#include <stdio.h>
#include <stdarg.h>

#define LOG_FILE_PATH "./car_log.txt"
#define LOG_MAX_FILE_SIZE (1024 * 1024)  // 1MB 日志文件最大大小
#define LOG_MAX_BACKUP_FILES 3  // 最大备份文件数量

typedef enum{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_MAX
}LogLevel;

extern LogLevel current_log_level;

// 日志系统初始化
int log_init(const char* log_path, LogLevel level);

// 设置日志级别
void log_set_level(LogLevel level);

// 获取日志级别字符串
const char* log_level_to_string(LogLevel level);

// 记录日志消息
void log_message(LogLevel level, const char* file, int line, const char* format, ...);

// 关闭日志系统
void log_close(void);

// 日志宏定义
#define LOG_DEBUG(format, ...) log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)

#endif