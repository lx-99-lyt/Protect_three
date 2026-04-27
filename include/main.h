#ifndef _MAIN_H
#define _MAIN_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// 主程序Socket路径（子模块连接主程序用，保留兼容）
#define UNIX_SOCKET_PATH "/tmp/car_socket.sock"

// 子模块Socket路径（主程序连接子模块用）
#define SOCKET_PATH_DOOR   "/tmp/car_door.sock"
#define SOCKET_PATH_STATUS "/tmp/car_status.sock"
#define SOCKET_PATH_SEAT   "/tmp/car_seat.sock"
#define SOCKET_PATH_AIR    "/tmp/car_air.sock"
#define SOCKET_PATH_FAULT  "/tmp/car_fault.sock"
#define SOCKET_PATH_DVR    "/tmp/car_dvr.sock"

#define MAX_FAULT_CODE 10
#define DEVICE_NAME_LEN 32

// Common buffer size for socket communication (response "OK\n" is only 3 bytes)
#define CLIENT_BUF_SIZE 64

// Module IDs
#define MOD_DOOR 1
#define MOD_STATUS 2
#define MOD_SEAT 3
#define MOD_AIR 4
#define MOD_FAULT 5
#define MOD_DVR 6

// Message types: 主程序主动发送命令，子模块被动响应
#define MSG_TYPE_CMD      0x01  // 主程序发送的命令
#define MSG_TYPE_RESPONSE 0x02  // 子模块返回的响应

// Command types: 主程序对子模块的操作类型
#define CMD_READ_STATUS   0x01  // 读取状态
#define CMD_WRITE_STATUS  0x02  // 写入状态/执行操作
#define CMD_GET_ALL       0x03  // 获取模块全部状态

// Door item IDs
typedef enum {
    DOOR_ITEM_FRONT_LEFT = 1,
    DOOR_ITEM_FRONT_RIGHT,
    DOOR_ITEM_BACK_LEFT,
    DOOR_ITEM_BACK_RIGHT,
    DOOR_ITEM_TRUNK,
    DOOR_ITEM_LOCK_STATUS
} DoorItemId;

// Status item IDs
typedef enum {
    STATUS_ITEM_SPEED = 1,
    STATUS_ITEM_RPM,
    STATUS_ITEM_WATER_TEMP,
    STATUS_ITEM_OIL_TEMP,
    STATUS_ITEM_FUEL,
    STATUS_ITEM_BATTERY_VOLTAGE,
    STATUS_ITEM_GEAR,
    STATUS_ITEM_HAND_BRAKE
} StatusItemId;

// Seat item IDs
typedef enum {
    SEAT_ITEM_FRONT_LEFT = 1,
    SEAT_ITEM_FRONT_RIGHT,
    SEAT_ITEM_ANGLE_BACK,
    SEAT_ITEM_HEAT,
    SEAT_ITEM_VENTILATE
} SeatItemId;

// Air item IDs
typedef enum {
    AIR_ITEM_AC_SWITCH = 1,
    AIR_ITEM_FAN_SPEED,
    AIR_ITEM_TEMP_SET,
    AIR_ITEM_INNER_CYCLE
} AirItemId;

// Fault item IDs
typedef enum {
    FAULT_ITEM_FAULT_CODE = 1,
    FAULT_ITEM_FAULT_COUNT,
    FAULT_ITEM_WARNING_LIGHT
} FaultItemId;

// DVR item IDs
typedef enum {
    DVR_ITEM_RECORD_STATUS = 1,
    DVR_ITEM_VIDEO_SD_EXIST,
    DVR_ITEM_VIDEO_TIME,
    DVR_ITEM_FILE_PATH
} DvrItemId;

#define MAX_STR_U8 64
#define MAX_ARRAY_LEN 32

//车门状态结构体
typedef struct {
    unsigned char door_front_left;
    unsigned char door_front_right;
    unsigned char door_back_left;
    unsigned char door_back_right;
    unsigned char door_trunk;
    unsigned char lock_status;
}Car_Door;

//车辆基本状态
typedef struct{
    float speed;    //车速
    int rpm;    //发动机转速
    float water_temp;   //水温
    float oil_temp;     //油温
    float fuel;   //油量 0-100%
    float battery_voltage;  //电瓶电压
    unsigned char gear;     //档位 P R D N S
    unsigned char hand_brake;   //手刹 0-释放 1-按下
}Car_Status;

//座椅控制
typedef struct{
    unsigned char seat_front_left;  //主驾0-无1-有
    unsigned char seat_front_right;    //副驾0-无 1-有
    int angle_back;   //靠背角度
    unsigned char heat; //座椅加热 0-3
    unsigned char ventilate;    //座椅通风 0-2
}Car_Seat;

//空调状态
typedef struct{
    unsigned char ac_switch;    //AC开关 0-关 1-开
    unsigned char fan_speed;    //风速 0-5
    int temp_set;   //设定温度
    unsigned char inner_cycle;  //内循环 0-关 1-开
}Car_Air;

//故障信息
typedef struct{
    int fault_code[MAX_FAULT_CODE]; //故障码
    int fault_count;    //故障码数量
    unsigned char warning_light;    //报警灯0-关 1-开
}Car_Fault;

//行车记录仪
typedef struct{
    unsigned char record_status;    //记录状态 0-关 1-开
    unsigned char video_sd_exist;   //SD卡是否存在 0-无 1-有sd卡
    int video_time;     //视频时长
    char file_path[DEVICE_NAME_LEN];    //文件路径
}Car_Dvr;

//整车总结构体
typedef struct{
    Car_Status status;
    Car_Door door;
    Car_Seat seat;
    Car_Air air;
    Car_Fault fault;
    Car_Dvr dvr;
    pthread_mutex_t car_mutex;
}Car_Device;

//类型标记枚举
typedef enum{
    VAL_TYPE_U8,
    VAL_TYPE_I32,
    VAL_TYPE_F32,
    VAL_TYPE_STR,
    VAL_TYPE_STR_U8,
    VAL_TYPE_STR_I32,
    VAL_TYPE_STR_F32
}ValType;

//联合体：存储数据类型
typedef union{
    unsigned char u8;
    int i32;
    float f32;
    char str[MAX_STR_U8];
    uint8_t arr_u8[MAX_ARRAY_LEN];
    int arr_i32[MAX_ARRAY_LEN];
    float arr_f32[MAX_ARRAY_LEN];
}Value;

//消息结构体（扩展：支持主程序主动请求模式）
typedef struct{
    uint8_t msg_type;   // 消息类型: MSG_TYPE_CMD / MSG_TYPE_RESPONSE
    uint8_t cmd_type;   // 命令类型: CMD_READ_STATUS / CMD_WRITE_STATUS / CMD_GET_ALL
    int mod_id;         // 模块ID
    uint8_t item_id;    // 子项ID
    ValType value_type; // 值类型
    Value value;        // 值
    int result;         // 响应结果: 0=成功, -1=失败
}Msg;

extern Car_Device g_car;

void Car_Device_Init(Car_Device* g_car);
int Car_Device_Load(Car_Device* g_car);
int Car_Device_Save(Car_Device* g_car);
void Car_Device_Destroy(Car_Device* g_car);

#endif
