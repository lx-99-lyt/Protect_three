#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include "Car_Log.h"

// 全局变量
LogLevel current_log_level = LOG_LEVEL_DEBUG;
static FILE* log_file_handle = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static char log_file_path[256] = LOG_FILE_PATH;

// 日志级别字符串
static const char* level_str[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "UNKNOWN"
};

// 获取日志级别字符串
const char* log_level_to_string(LogLevel level)
{
    if (level >= LOG_LEVEL_MAX) {
        return level_str[4]; // "UNKNOWN"
    }
    return level_str[level];
}

// 检查日志文件大小，如果超过限制则轮转
// 支持多个备份文件，例如: car_log.txt.old.1, car_log.txt.old.2, car_log.txt.old.3
static void log_rotate_if_needed(void)
{
    if (log_file_handle == NULL) {
        return;
    }

    struct stat st;
    if (stat(log_file_path, &st) == 0 && st.st_size >= LOG_MAX_FILE_SIZE) {
        // 关闭当前日志文件
        fclose(log_file_handle);
        log_file_handle = NULL;

        // 删除最旧的备份文件（如果已达到最大数量）
        char oldest_backup[256];
        snprintf(oldest_backup, sizeof(oldest_backup), "%s.old.%d", log_file_path, LOG_MAX_BACKUP_FILES);
        remove(oldest_backup);

        // 重命名现有的备份文件，将编号递增
        char old_path[256];
        char new_path[256];
        for (int i = LOG_MAX_BACKUP_FILES - 1; i >= 1; i--) {
            snprintf(old_path, sizeof(old_path), "%s.old.%d", log_file_path, i);
            snprintf(new_path, sizeof(new_path), "%s.old.%d", log_file_path, i + 1);
            rename(old_path, new_path);
        }

        // 将当前日志文件重命名为 .old.1
        char first_backup[256];
        snprintf(first_backup, sizeof(first_backup), "%s.old.1", log_file_path);
        rename(log_file_path, first_backup);

        // 重新打开日志文件
        log_file_handle = fopen(log_file_path, "a");
        if (log_file_handle == NULL) {
            fprintf(stderr, "Failed to reopen log file after rotation: %s\n", log_file_path);
        }
    }
}

// 日志系统初始化
int log_init(const char* log_path, LogLevel level)
{
    pthread_mutex_lock(&log_mutex);

    // 如果已经有打开的日志文件，先关闭
    if (log_file_handle != NULL) {
        fclose(log_file_handle);
        log_file_handle = NULL;
    }

    // 设置日志路径
    if (log_path != NULL) {
        strncpy(log_file_path, log_path, sizeof(log_file_path) - 1);
        log_file_path[sizeof(log_file_path) - 1] = '\0';
    }

    // 设置日志级别
    if (level >= LOG_LEVEL_DEBUG && level < LOG_LEVEL_MAX) {
        current_log_level = level;
    }

    // 打开日志文件
    log_file_handle = fopen(log_file_path, "a");
    if (log_file_handle == NULL) {
        pthread_mutex_unlock(&log_mutex);
        fprintf(stderr, "Failed to open log file: %s\n", log_file_path);
        return -1;
    }

    pthread_mutex_unlock(&log_mutex);

    LOG_INFO("Log system initialized, log file: %s, level: %s", log_file_path, log_level_to_string(current_log_level));
    return 0;
}

// 设置日志级别
void log_set_level(LogLevel level)
{
    if (level >= LOG_LEVEL_DEBUG && level < LOG_LEVEL_MAX) {
        pthread_mutex_lock(&log_mutex);
        current_log_level = level;
        pthread_mutex_unlock(&log_mutex);
    }
}

// 记录日志消息
void log_message(LogLevel level, const char* file, int line, const char* format, ...)
{
    if (level < current_log_level) {
        return;
    }

    pthread_mutex_lock(&log_mutex);

    // 检查日志文件是否需要轮转
    log_rotate_if_needed();

    // 获取当前时间
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 提取文件名（去掉路径）
    const char* filename = file;
    const char* p = file;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }

    // 输出到控制台
    fprintf(stdout, "[%s] [%s] %s:%d ", time_str, level_str[level], filename, line);
    
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);

    // 输出到日志文件
    if (log_file_handle != NULL) {
        fprintf(log_file_handle, "[%s] [%s] %s:%d ", time_str, level_str[level], filename, line);
        
        va_start(args, format);
        vfprintf(log_file_handle, format, args);
        va_end(args);
        fprintf(log_file_handle, "\n");
        fflush(log_file_handle);
    }

    pthread_mutex_unlock(&log_mutex);
}

// 关闭日志系统
void log_close(void)
{
    pthread_mutex_lock(&log_mutex);

    if (log_file_handle != NULL) {
        // 先解锁，否则 LOG_INFO 内部调用 log_message 会引起死锁
        pthread_mutex_unlock(&log_mutex);

        LOG_INFO("Log system closing");

        pthread_mutex_lock(&log_mutex);
        if (log_file_handle != NULL) {
            fclose(log_file_handle);
            log_file_handle = NULL;
        }
    }

    pthread_mutex_unlock(&log_mutex);
    pthread_mutex_destroy(&log_mutex);
}