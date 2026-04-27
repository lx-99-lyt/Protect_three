#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include "main.h"
#include "Car_Log.h"

#define MAX_EVENTS 10

// 车辆状态模块本地状态
static Car_Status g_status = {0};
static volatile int keep_running = 1;

// 信号处理
static void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        keep_running = 0;
    }
}

// 处理主程序发来的命令
static int handle_command(Msg* cmd, Msg* response)
{
    // 初始化响应
    memset(response, 0, sizeof(Msg));
    response->msg_type = MSG_TYPE_RESPONSE;
    response->mod_id = MOD_STATUS;
    response->result = 0;  // 默认成功
    
    switch (cmd->cmd_type) {
        case CMD_READ_STATUS:
        case CMD_GET_ALL:
            // 读取指定项或获取全部状态
            if (cmd->cmd_type == CMD_GET_ALL) {
                // 返回全部车辆状态
                response->item_id = 0;  // 0表示全部
                response->value_type = VAL_TYPE_STR_U8;
                memcpy(response->value.arr_u8, &g_status, sizeof(Car_Status));
                response->result = 0;
            } else {
                // 读取指定项
                response->item_id = cmd->item_id;
                switch (cmd->item_id) {
                    case STATUS_ITEM_SPEED:
                        response->value.f32 = g_status.speed;
                        response->value_type = VAL_TYPE_F32;
                        break;
                    case STATUS_ITEM_RPM:
                        response->value.i32 = g_status.rpm;
                        response->value_type = VAL_TYPE_I32;
                        break;
                    case STATUS_ITEM_WATER_TEMP:
                        response->value.f32 = g_status.water_temp;
                        response->value_type = VAL_TYPE_F32;
                        break;
                    case STATUS_ITEM_OIL_TEMP:
                        response->value.f32 = g_status.oil_temp;
                        response->value_type = VAL_TYPE_F32;
                        break;
                    case STATUS_ITEM_FUEL:
                        response->value.f32 = g_status.fuel;
                        response->value_type = VAL_TYPE_F32;
                        break;
                    case STATUS_ITEM_BATTERY_VOLTAGE:
                        response->value.f32 = g_status.battery_voltage;
                        response->value_type = VAL_TYPE_F32;
                        break;
                    case STATUS_ITEM_GEAR:
                        response->value.u8 = g_status.gear;
                        response->value_type = VAL_TYPE_U8;
                        break;
                    case STATUS_ITEM_HAND_BRAKE:
                        response->value.u8 = g_status.hand_brake;
                        response->value_type = VAL_TYPE_U8;
                        break;
                    default:
                        response->result = -1;
                        break;
                }
            }
            break;
            
        case CMD_WRITE_STATUS:
            // 执行操作：修改车辆状态
            response->item_id = cmd->item_id;
            switch (cmd->item_id) {
                case STATUS_ITEM_SPEED:
                    if (cmd->value_type == VAL_TYPE_F32) {
                        g_status.speed = cmd->value.f32;
                        response->value.f32 = cmd->value.f32;
                        response->value_type = VAL_TYPE_F32;
                    } else {
                        response->result = -1;
                    }
                    break;
                case STATUS_ITEM_RPM:
                    if (cmd->value_type == VAL_TYPE_I32) {
                        g_status.rpm = cmd->value.i32;
                        response->value.i32 = cmd->value.i32;
                        response->value_type = VAL_TYPE_I32;
                    } else {
                        response->result = -1;
                    }
                    break;
                case STATUS_ITEM_WATER_TEMP:
                    if (cmd->value_type == VAL_TYPE_F32) {
                        g_status.water_temp = cmd->value.f32;
                        response->value.f32 = cmd->value.f32;
                        response->value_type = VAL_TYPE_F32;
                    } else {
                        response->result = -1;
                    }
                    break;
                case STATUS_ITEM_OIL_TEMP:
                    if (cmd->value_type == VAL_TYPE_F32) {
                        g_status.oil_temp = cmd->value.f32;
                        response->value.f32 = cmd->value.f32;
                        response->value_type = VAL_TYPE_F32;
                    } else {
                        response->result = -1;
                    }
                    break;
                case STATUS_ITEM_FUEL:
                    if (cmd->value_type == VAL_TYPE_F32) {
                        g_status.fuel = cmd->value.f32;
                        response->value.f32 = cmd->value.f32;
                        response->value_type = VAL_TYPE_F32;
                    } else {
                        response->result = -1;
                    }
                    break;
                case STATUS_ITEM_BATTERY_VOLTAGE:
                    if (cmd->value_type == VAL_TYPE_F32) {
                        g_status.battery_voltage = cmd->value.f32;
                        response->value.f32 = cmd->value.f32;
                        response->value_type = VAL_TYPE_F32;
                    } else {
                        response->result = -1;
                    }
                    break;
                case STATUS_ITEM_GEAR:
                    if (cmd->value_type == VAL_TYPE_U8) {
                        g_status.gear = cmd->value.u8;
                        response->value.u8 = cmd->value.u8;
                        response->value_type = VAL_TYPE_U8;
                    } else {
                        response->result = -1;
                    }
                    break;
                case STATUS_ITEM_HAND_BRAKE:
                    if (cmd->value_type == VAL_TYPE_U8) {
                        g_status.hand_brake = cmd->value.u8;
                        response->value.u8 = cmd->value.u8;
                        response->value_type = VAL_TYPE_U8;
                    } else {
                        response->result = -1;
                    }
                    break;
                default:
                    response->result = -1;
                    break;
            }
            break;
            
        default:
            response->result = -1;
            break;
    }
    
    return 0;
}

int main(int argc, char const **argv)
{
    int server_socket = -1;
    struct sockaddr_un server_addr;
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 初始化车辆状态，设置与主程序一致的非零默认值
    memset(&g_status, 0, sizeof(Car_Status));
    g_status.battery_voltage = 12.0f;
    g_status.fuel = 50.0f;
    g_status.gear = 'P';
    g_status.hand_brake = 1;
    g_status.oil_temp = 40.0f;
    g_status.water_temp = 30.0f;
    
    // 删除旧的socket文件
    unlink(SOCKET_PATH_STATUS);
    
    // 创建Unix Socket服务器
    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket < 0) {
        LOG_ERROR("Car_Status: socket creation failed: %s", strerror(errno));
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH_STATUS, sizeof(server_addr.sun_path) - 1);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Car_Status: bind failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }
    
    if (listen(server_socket, 5) < 0) {
        LOG_ERROR("Car_Status: listen failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }
    
    LOG_INFO("Car_Status: Server started, listening on %s", SOCKET_PATH_STATUS);
    
    // epoll设置
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        LOG_ERROR("Car_Status: epoll_create1 failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }
    
    struct epoll_event ev, events[MAX_EVENTS];
    ev.data.fd = server_socket;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev);
    
    // 主循环：被动等待主程序命令
    while (keep_running) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1000);
        
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (fd == server_socket) {
                // 处理新连接
                int client_socket = accept(server_socket, NULL, NULL);
                if (client_socket < 0) {
                    LOG_ERROR("Car_Status: accept failed: %s", strerror(errno));
                    continue;
                }
                ev.data.fd = client_socket;
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket, &ev);
                LOG_INFO("Car_Status: Main program connected (fd=%d)", client_socket);
            } else {
                // 处理主程序命令
                Msg cmd, response;
                memset(&cmd, 0, sizeof(Msg));
                memset(&response, 0, sizeof(Msg));
                
                int len = read(fd, &cmd, sizeof(Msg));
                if (len <= 0) {
                    // 连接断开
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    LOG_INFO("Car_Status: Main program disconnected (fd=%d)", fd);
                    continue;
                }
                
                if (len != sizeof(Msg)) {
                    LOG_WARN("Car_Status: Invalid message size");
                    continue;
                }
                
                // 验证消息类型
                if (cmd.msg_type != MSG_TYPE_CMD) {
                    LOG_WARN("Car_Status: Invalid message type");
                    continue;
                }
                
                // 处理命令
                handle_command(&cmd, &response);
                
                // 发送响应
                int ret = send(fd, &response, sizeof(Msg), 0);
                if (ret < 0) {
                    LOG_ERROR("Car_Status: send response failed: %s", strerror(errno));
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    LOG_INFO("Car_Status: Executed cmd=%d, item=%d, result=%d",
                           cmd.cmd_type, cmd.item_id, response.result);
                }
            }
        }
    }
    
    // 清理
    LOG_INFO("Car_Status: Shutting down...");
    close(epfd);
    close(server_socket);
    unlink(SOCKET_PATH_STATUS);
    
    return 0;
}