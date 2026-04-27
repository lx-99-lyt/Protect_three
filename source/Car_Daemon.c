#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include "Car_Daemon.h"

void daemon_init(void)
{
    pid_t pid;
    pid = fork();
    if(pid < 0){
        exit(1);
    }
    if(pid > 0){
        exit(0);
    }
    setsid();
    pid = fork();
    if(pid < 0){
        exit(1);
    }
    if(pid > 0){
        exit(0);
    }
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    chdir("/");
    
    // 将 stdin/stdout/stderr 重定向到 /dev/null，而不是简单关闭
    // 这样后续代码如果误写 stdout 不会报错，也不会丢失日志
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2) {
            close(devnull);
        }
    }
}
