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

// DVR模块本地状态
static Car_Dvr g_dvr = {0};
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
    response->mod_id = MOD_DVR;
    response->result = 0;  // 默认成功
    
    switch (cmd->cmd_type) {
        case CMD_READ_STATUS:
        case CMD_GET_ALL:
            // 读取指定项或获取全部状态
            if (cmd->cmd_type == CMD_GET_ALL) {
                // 返回全部DVR状态
                response->item_id = 0;  // 0表示全部
                response->value_type = VAL_TYPE_STR_U8;
                memcpy(response->value.arr_u8, &g_dvr, sizeof(Car_Dvr));
                response->result = 0;
            } else {
                // 读取指定项
                response->item_id = cmd->item_id;
                switch (cmd->item_id) {
                    case DVR_ITEM_RECORD_STATUS:
                        response->value.u8 = g_dvr.record_status;
                        response->value_type = VAL_TYPE_U8;
                        break;
                    case DVR_ITEM_VIDEO_SD_EXIST:
                        response->value.u8 = g_dvr.video_sd_exist;
                        response->value_type = VAL_TYPE_U8;
                        break;
                    case DVR_ITEM_VIDEO_TIME:
                        response->value.i32 = g_dvr.video_time;
                        response->value_type = VAL_TYPE_I32;
                        break;
                    case DVR_ITEM_FILE_PATH:
                        response->value_type = VAL_TYPE_STR;
                        strncpy(response->value.str, g_dvr.file_path, DEVICE_NAME_LEN - 1);
                        response->value.str[DEVICE_NAME_LEN - 1] = '\0';
                        break;
                    default:
                        response->result = -1;
                        break;
                }
            }
            break;
            
        case CMD_WRITE_STATUS:
            // 执行操作：修改DVR状态
            response->item_id = cmd->item_id;
            switch (cmd->item_id) {
                case DVR_ITEM_RECORD_STATUS:
                    if (cmd->value_type == VAL_TYPE_U8) {
                        g_dvr.record_status = cmd->value.u8;
                        response->value.u8 = cmd->value.u8;
                        response->value_type = VAL_TYPE_U8;
                    } else {
                        response->result = -1;
                    }
                    break;
                case DVR_ITEM_VIDEO_SD_EXIST:
                    if (cmd->value_type == VAL_TYPE_U8) {
                        g_dvr.video_sd_exist = cmd->value.u8;
                        response->value.u8 = cmd->value.u8;
                        response->value_type = VAL_TYPE_U8;
                    } else {
                        response->result = -1;
                    }
                    break;
                case DVR_ITEM_VIDEO_TIME:
                    if (cmd->value_type == VAL_TYPE_I32) {
                        g_dvr.video_time = cmd->value.i32;
                        response->value.i32 = cmd->value.i32;
                        response->value_type = VAL_TYPE_I32;
                    } else {
                        response->result = -1;
                    }
                    break;
                case DVR_ITEM_FILE_PATH:
                    if (cmd->value_type == VAL_TYPE_STR) {
                        strncpy(g_dvr.file_path, cmd->value.str, DEVICE_NAME_LEN - 1);
                        g_dvr.file_path[DEVICE_NAME_LEN - 1] = '\0';
                        response->value_type = VAL_TYPE_STR;
                        strncpy(response->value.str, cmd->value.str, DEVICE_NAME_LEN - 1);
                        response->value.str[DEVICE_NAME_LEN - 1] = '\0';
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
    
    // 初始化DVR状态
    memset(&g_dvr, 0, sizeof(Car_Dvr));
    
    // 删除旧的socket文件
    unlink(SOCKET_PATH_DVR);
    
    // 创建Unix Socket服务器
    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket < 0) {
        LOG_ERROR("Car_Dvr: socket creation failed: %s", strerror(errno));
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH_DVR, sizeof(server_addr.sun_path) - 1);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Car_Dvr: bind failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }
    
    if (listen(server_socket, 5) < 0) {
        LOG_ERROR("Car_Dvr: listen failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }
    
    LOG_INFO("Car_Dvr: Server started, listening on %s", SOCKET_PATH_DVR);
    
    // epoll设置
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        LOG_ERROR("Car_Dvr: epoll_create1 failed: %s", strerror(errno));
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
                    LOG_ERROR("Car_Dvr: accept failed: %s", strerror(errno));
                    continue;
                }
                ev.data.fd = client_socket;
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket, &ev);
                LOG_INFO("Car_Dvr: Main program connected (fd=%d)", client_socket);
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
                    LOG_INFO("Car_Dvr: Main program disconnected (fd=%d)", fd);
                    continue;
                }
                
                if (len != sizeof(Msg)) {
                    LOG_WARN("Car_Dvr: Invalid message size");
                    continue;
                }
                
                // 验证消息类型
                if (cmd.msg_type != MSG_TYPE_CMD) {
                    LOG_WARN("Car_Dvr: Invalid message type");
                    continue;
                }
                
                // 处理命令
                handle_command(&cmd, &response);
                
                // 发送响应
                int ret = send(fd, &response, sizeof(Msg), 0);
                if (ret < 0) {
                    LOG_ERROR("Car_Dvr: send response failed: %s", strerror(errno));
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    LOG_INFO("Car_Dvr: Executed cmd=%d, item=%d, result=%d",
                           cmd.cmd_type, cmd.item_id, response.result);
                }
            }
        }
    }
    
    // 清理
    LOG_INFO("Car_Dvr: Shutting down...");
    close(epfd);
    close(server_socket);
    unlink(SOCKET_PATH_DVR);
    
    return 0;
}