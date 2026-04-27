#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "main.h"
#include "Car_Log.h"
//#include "Car_Daemon.h"

// 自动保存时间间隔（秒）
#define AUTO_SAVE_INTERVAL 30

// 车速达到20km/h时自动上锁车门
#define SPEED_LOCK_THRESHOLD 20.0f

// 信号处理标志
static volatile int keep_running = 1;
static float g_last_speed = 0.0f;  // 记录上次车速

// 信号处理函数
static void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nReceived signal %d, shutting down gracefully...\n", signum);
        keep_running = 0;
    }
}

// 向车门模块发送锁定/解锁命令
static void send_door_lock_cmd(unsigned char lock)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("Main: Failed to create socket for door: %s", strerror(errno));
        return;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH_DOOR, sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Main: Failed to connect to door module: %s", strerror(errno));
        close(fd);
        return;
    }
    
    Msg cmd, resp;
    memset(&cmd, 0, sizeof(Msg));
    cmd.msg_type = MSG_TYPE_CMD;
    cmd.cmd_type = CMD_WRITE_STATUS;
    cmd.mod_id = MOD_DOOR;
    cmd.item_id = DOOR_ITEM_LOCK_STATUS;
    cmd.value_type = VAL_TYPE_U8;
    cmd.value.u8 = lock;
    
    if (send(fd, &cmd, sizeof(Msg), 0) < 0) {
        LOG_ERROR("Main: Failed to send lock command: %s", strerror(errno));
        close(fd);
        return;
    }
    
    memset(&resp, 0, sizeof(Msg));
    if (recv(fd, &resp, sizeof(Msg), 0) > 0) {
        if (resp.result == 0) {
            LOG_INFO("Main: Door %s successful (speed=%.1f km/h)", 
                     lock ? "lock" : "unlock", g_last_speed);
        } else {
            LOG_WARN("Main: Door %s failed", lock ? "lock" : "unlock");
        }
    }
    
    close(fd);
}

// 检查车速并自动上锁
static void check_speed_auto_lock(float current_speed)
{
    // 车速从低于阈值上升到达到或超过阈值时，自动上锁
    if (g_last_speed < SPEED_LOCK_THRESHOLD && current_speed >= SPEED_LOCK_THRESHOLD) {
        LOG_INFO("Main: Speed reached %.1f km/h, auto locking doors", current_speed);
        send_door_lock_cmd(1);  // 1 = 上锁
    }
    
    // 记录当前车速
    g_last_speed = current_speed;
}

int main(int argc, char** argv)
{
    //daemon_init();
    
    int ret = 0;
    
    // 初始化日志系统
    ret = log_init(NULL, LOG_LEVEL_DEBUG);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize log system\n");
    }
    
    // 车辆信息初始化
    Car_Device_Init(&g_car);
    
    LOG_INFO("Starting car central controller...");
    LOG_INFO("Use car_ctl command to control submodules.");
    
    // 加载车辆信息
    ret = Car_Device_Load(&g_car);
    if (ret < 0) {
        LOG_ERROR("Load car info failed!");
        log_close();
        return -1;
    }
    
    // 注册信号处理函数
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    LOG_INFO("Central controller started. Press Ctrl+C to shutdown.");
    LOG_INFO("Submodules should be started before this program.");
    
    // 主循环：负责定期保存配置和监测车速自动上锁
    time_t last_save_time = time(NULL);
    time_t last_speed_check = time(NULL);
    
    while (keep_running) {
        time_t now = time(NULL);
        
        // 定期自动保存
        if (now - last_save_time >= AUTO_SAVE_INTERVAL) {
            Car_Device_Save(&g_car);
            LOG_INFO("Auto-save triggered: %ld seconds elapsed", (long)(now - last_save_time));
            last_save_time = now;
        }
        
        // 定期监测车速并自动上锁（每秒检查一次）
        if (now - last_speed_check >= 1) {
            // 从Car_Status模块读取当前车速
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd >= 0) {
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, SOCKET_PATH_STATUS, sizeof(addr.sun_path) - 1);
                
                if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                    Msg cmd, resp;
                    memset(&cmd, 0, sizeof(Msg));
                    cmd.msg_type = MSG_TYPE_CMD;
                    cmd.cmd_type = CMD_READ_STATUS;
                    cmd.mod_id = MOD_STATUS;
                    cmd.item_id = STATUS_ITEM_SPEED;
                    
                    if (send(fd, &cmd, sizeof(Msg), 0) >= 0) {
                        memset(&resp, 0, sizeof(Msg));
                        if (recv(fd, &resp, sizeof(Msg), 0) > 0 && resp.result == 0) {
                            // 更新本地车速并检查是否需要自动上锁
                            check_speed_auto_lock(resp.value.f32);
                        }
                    }
                }
                close(fd);
            }
            last_speed_check = now;
        }
        
        // 守护进程模式下 stdin 已重定向到 /dev/null，无法使用交互式菜单
        // 使用 sleep 等待，通过信号退出
        sleep(1);
    }
    
    LOG_INFO("Shutting down central controller...");
    
    // 保存车辆信息
    ret = Car_Device_Save(&g_car);
    if (ret < 0) {
        LOG_ERROR("Save car info failed!");
    }
    
    // 销毁车辆信息
    Car_Device_Destroy(&g_car);
    
    // 关闭日志系统
    log_close();
    
    LOG_INFO("Central controller shutdown complete.");
    return 0;
}