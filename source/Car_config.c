#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "main.h"
#include "Car_Log.h"

#define CAR_INFO_PATH "./car_info.ini"

Car_Device g_car;  // 全局车辆设备信息实例

void Car_Device_Destroy(Car_Device* car)
{
    pthread_mutex_destroy(&car->car_mutex);
}

void Car_Device_Init(Car_Device* g_car)
{
    // 先清零所有字段，再设置非零默认值
    memset(g_car, 0, sizeof(*g_car));
    pthread_mutex_init(&g_car->car_mutex, NULL);

    // 设置非零默认值
    g_car->status.battery_voltage = 12.0f;
    g_car->status.fuel = 50.0f;
    g_car->status.gear = 'P';
    g_car->status.hand_brake = 1;
    g_car->status.oil_temp = 40.0f;
    g_car->status.water_temp = 30.0f;

    g_car->seat.angle_back = 90;

    g_car->air.temp_set = 20;
}

int Car_Device_Load(Car_Device* g_car)
{
    FILE* fp = fopen(CAR_INFO_PATH, "r");
    if (fp == NULL)
    {
        LOG_WARN("Open file %s failed, using default values", CAR_INFO_PATH);
        return 0; // 文件不存在时使用默认值，不视为错误
    }

    char line[256];
    char key[64];
    char value[64];
    int section = 0; // 0: unknown, 1: door, 2: status, 3: seat, 4: air, 5: fault, 6: dvr

    while(fgets(line, sizeof(line), fp) != NULL)
    {
        // 去除换行符
        line[strcspn(line, "\n\r")] = '\0';
        
        // 跳过空行和注释
        if(line[0] == '#' || line[0] == '\0'){
            continue;
        }
        
        // 检查是否是节标记
        if(line[0] == '[' && line[strlen(line)-1] == ']') {
            char section_name[32];
            size_t section_len = strlen(line) - 2;
            if (section_len >= sizeof(section_name)) {
                section_len = sizeof(section_name) - 1;
            }
            strncpy(section_name, line+1, section_len);
            section_name[section_len] = '\0';
            
            if(strcmp(section_name, "door") == 0) section = 1;
            else if(strcmp(section_name, "status") == 0) section = 2;
            else if(strcmp(section_name, "seat") == 0) section = 3;
            else if(strcmp(section_name, "air") == 0) section = 4;
            else if(strcmp(section_name, "fault") == 0) section = 5;
            else if(strcmp(section_name, "dvr") == 0) section = 6;
            else section = 0;
            continue;
        }
        
        // 解析键值对
        char* equal_pos = strchr(line, '=');
        if(equal_pos == NULL){
            continue;
        }
        
        // 分割键和值，使用安全的字符串复制
        size_t key_len = equal_pos - line;
        if (key_len >= sizeof(key)) {
            key_len = sizeof(key) - 1;
        }
        strncpy(key, line, key_len);
        key[key_len] = '\0';
        
        size_t value_max_len = sizeof(value) - 1;
        size_t value_src_len = strlen(equal_pos + 1);
        size_t value_copy_len = (value_src_len < value_max_len) ? value_src_len : value_max_len;
        strncpy(value, equal_pos + 1, value_copy_len);
        value[value_copy_len] = '\0';
        
        // 去除键和值两端的空格
        char* key_trim = key;
        while(*key_trim == ' ') key_trim++;
        char* key_end = key_trim + strlen(key_trim) - 1;
        while(key_end > key_trim && *key_end == ' ') key_end--;
        *(key_end + 1) = '\0';
        
        char* value_trim = value;
        while(*value_trim == ' ') value_trim++;
        char* value_end = value_trim + strlen(value_trim) - 1;
        while(value_end > value_trim && *value_end == ' ') value_end--;
        *(value_end + 1) = '\0';
        
        // 根据节和键名设置值
        pthread_mutex_lock(&g_car->car_mutex);
        switch(section) {
            case 1: // door
                if(strcmp(key_trim, "door_front_left") == 0) g_car->door.door_front_left = atoi(value_trim);
                else if(strcmp(key_trim, "door_front_right") == 0) g_car->door.door_front_right = atoi(value_trim);
                else if(strcmp(key_trim, "door_back_left") == 0) g_car->door.door_back_left = atoi(value_trim);
                else if(strcmp(key_trim, "door_back_right") == 0) g_car->door.door_back_right = atoi(value_trim);
                else if(strcmp(key_trim, "door_trunk") == 0) g_car->door.door_trunk = atoi(value_trim);
                else if(strcmp(key_trim, "lock_status") == 0) g_car->door.lock_status = atoi(value_trim);
                break;
            case 2: // status
                if(strcmp(key_trim, "speed") == 0) g_car->status.speed = atof(value_trim);
                else if(strcmp(key_trim, "rpm") == 0) g_car->status.rpm = atoi(value_trim);
                else if(strcmp(key_trim, "water_temp") == 0) g_car->status.water_temp = atof(value_trim);
                else if(strcmp(key_trim, "oil_temp") == 0) g_car->status.oil_temp = atof(value_trim);
                else if(strcmp(key_trim, "fuel") == 0) g_car->status.fuel = atof(value_trim);
                else if(strcmp(key_trim, "battery_voltage") == 0) g_car->status.battery_voltage = atof(value_trim);
                else if(strcmp(key_trim, "gear") == 0) g_car->status.gear = value_trim[0];
                else if(strcmp(key_trim, "hand_brake") == 0) g_car->status.hand_brake = atoi(value_trim);
                break;
            case 3: // seat
                if(strcmp(key_trim, "seat_front_left") == 0) g_car->seat.seat_front_left = atoi(value_trim);
                else if(strcmp(key_trim, "seat_front_right") == 0) g_car->seat.seat_front_right = atoi(value_trim);
                else if(strcmp(key_trim, "angle_back") == 0) g_car->seat.angle_back = atoi(value_trim);
                else if(strcmp(key_trim, "heat") == 0) g_car->seat.heat = atoi(value_trim);
                else if(strcmp(key_trim, "ventilate") == 0) g_car->seat.ventilate = atoi(value_trim);
                break;
            case 4: // air
                if(strcmp(key_trim, "ac_switch") == 0) g_car->air.ac_switch = atoi(value_trim);
                else if(strcmp(key_trim, "fan_speed") == 0) g_car->air.fan_speed = atoi(value_trim);
                else if(strcmp(key_trim, "temp_set") == 0) g_car->air.temp_set = atoi(value_trim);
                else if(strcmp(key_trim, "inner_cycle") == 0) g_car->air.inner_cycle = atoi(value_trim);
                break;
            case 5: // fault
                if(strcmp(key_trim, "fault_count") == 0) g_car->fault.fault_count = atoi(value_trim);
                else if(strcmp(key_trim, "warning_light") == 0) g_car->fault.warning_light = atoi(value_trim);
                // 故障码数组需要特殊处理
                else if(strncmp(key_trim, "fault_code", 10) == 0) {
                    int index = atoi(key_trim + 10);
                    if(index >= 0 && index < MAX_FAULT_CODE) {
                        g_car->fault.fault_code[index] = atoi(value_trim);
                    }
                }
                break;
            case 6: // dvr
                if(strcmp(key_trim, "record_status") == 0) g_car->dvr.record_status = atoi(value_trim);
                else if(strcmp(key_trim, "video_sd_exist") == 0) g_car->dvr.video_sd_exist = atoi(value_trim);
                else if(strcmp(key_trim, "video_time") == 0) g_car->dvr.video_time = atoi(value_trim);
                else if(strcmp(key_trim, "file_path") == 0) {
                    strncpy(g_car->dvr.file_path, value_trim, DEVICE_NAME_LEN);
                    g_car->dvr.file_path[DEVICE_NAME_LEN - 1] = '\0';
                }
                break;
        }
        pthread_mutex_unlock(&g_car->car_mutex);
    }

    fclose(fp);
    LOG_INFO("Loaded car info from %s", CAR_INFO_PATH);
    return 0;
}

int Car_Device_Save(Car_Device* g_car)
{
    // 使用临时文件实现原子写入，避免断电时文件损坏
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", CAR_INFO_PATH);
    
    FILE* fp = fopen(temp_path, "w");
    if (fp == NULL)
    {
        LOG_ERROR("Open temp file %s failed", temp_path);
        return -1;
    }

    fprintf(fp, "# Car configuration file\n");
    fprintf(fp, "# This file is automatically generated, do not edit manually\n\n");
    
    pthread_mutex_lock(&g_car->car_mutex);
    
    // door section
    fprintf(fp, "[door]\n");
    fprintf(fp, "door_front_left = %d\n", g_car->door.door_front_left);
    fprintf(fp, "door_front_right = %d\n", g_car->door.door_front_right);
    fprintf(fp, "door_back_left = %d\n", g_car->door.door_back_left);
    fprintf(fp, "door_back_right = %d\n", g_car->door.door_back_right);
    fprintf(fp, "door_trunk = %d\n", g_car->door.door_trunk);
    fprintf(fp, "lock_status = %d\n", g_car->door.lock_status);
    fprintf(fp, "\n");
    
    // status section
    fprintf(fp, "[status]\n");
    fprintf(fp, "speed = %.2f\n", g_car->status.speed);
    fprintf(fp, "rpm = %d\n", g_car->status.rpm);
    fprintf(fp, "water_temp = %.2f\n", g_car->status.water_temp);
    fprintf(fp, "oil_temp = %.2f\n", g_car->status.oil_temp);
    fprintf(fp, "fuel = %.2f\n", g_car->status.fuel);
    fprintf(fp, "battery_voltage = %.2f\n", g_car->status.battery_voltage);
    fprintf(fp, "gear = %c\n", g_car->status.gear);
    fprintf(fp, "hand_brake = %d\n", g_car->status.hand_brake);
    fprintf(fp, "\n");
    
    // seat section
    fprintf(fp, "[seat]\n");
    fprintf(fp, "seat_front_left = %d\n", g_car->seat.seat_front_left);
    fprintf(fp, "seat_front_right = %d\n", g_car->seat.seat_front_right);
    fprintf(fp, "angle_back = %d\n", g_car->seat.angle_back);
    fprintf(fp, "heat = %d\n", g_car->seat.heat);
    fprintf(fp, "ventilate = %d\n", g_car->seat.ventilate);
    fprintf(fp, "\n");
    
    // air section
    fprintf(fp, "[air]\n");
    fprintf(fp, "ac_switch = %d\n", g_car->air.ac_switch);
    fprintf(fp, "fan_speed = %d\n", g_car->air.fan_speed);
    fprintf(fp, "temp_set = %d\n", g_car->air.temp_set);
    fprintf(fp, "inner_cycle = %d\n", g_car->air.inner_cycle);
    fprintf(fp, "\n");
    
    // fault section
    fprintf(fp, "[fault]\n");
    fprintf(fp, "fault_count = %d\n", g_car->fault.fault_count);
    fprintf(fp, "warning_light = %d\n", g_car->fault.warning_light);
    for(int i = 0; i < MAX_FAULT_CODE; i++) {
        if(g_car->fault.fault_code[i] != 0) {
            fprintf(fp, "fault_code_%d = %d\n", i, g_car->fault.fault_code[i]);
        }
    }
    fprintf(fp, "\n");
    
    // dvr section
    fprintf(fp, "[dvr]\n");
    fprintf(fp, "record_status = %d\n", g_car->dvr.record_status);
    fprintf(fp, "video_sd_exist = %d\n", g_car->dvr.video_sd_exist);
    fprintf(fp, "video_time = %d\n", g_car->dvr.video_time);
    fprintf(fp, "file_path = %s\n", g_car->dvr.file_path);
    
    pthread_mutex_unlock(&g_car->car_mutex);
    
    fclose(fp);
    
    // 原子性地重命名临时文件为正式文件
    if(rename(temp_path, CAR_INFO_PATH) != 0) {
        LOG_ERROR("Rename temp file to %s failed", CAR_INFO_PATH);
        remove(temp_path);
        return -1;
    }
    
    LOG_INFO("Saved car info to %s", CAR_INFO_PATH);
    return 0;
}
