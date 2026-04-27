/**
 * car_ctl - 车载设备命令行控制工具
 *
 * 用法:
 *   car_ctl <模块> get_all
 *   car_ctl <模块> read <字段>
 *   car_ctl <模块> write <字段> <值>
 *
 * 模块列表:
 *   door | status | seat | air | fault | dvr
 *
 * 示例:
 *   car_ctl air get_all
 *   car_ctl air read ac_switch
 *   car_ctl air write ac_switch 1
 *   car_ctl air write fan_speed 3
 *   car_ctl air write temp_set 25
 *   car_ctl door write lock_status 1
 *   car_ctl seat write heat 2
 *   car_ctl status get_all
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include "main.h"
#include "Car_Log.h"

// ============================================================
// 模块信息表
// ============================================================
typedef struct {
    const char *name;
    int mod_id;
    const char *sock_path;
} ModuleInfo;

static const ModuleInfo g_modules[] = {
    {"door",   MOD_DOOR,   SOCKET_PATH_DOOR},
    {"status", MOD_STATUS, SOCKET_PATH_STATUS},
    {"seat",   MOD_SEAT,   SOCKET_PATH_SEAT},
    {"air",    MOD_AIR,    SOCKET_PATH_AIR},
    {"fault",  MOD_FAULT,  SOCKET_PATH_FAULT},
    {"dvr",    MOD_DVR,    SOCKET_PATH_DVR},
    {NULL, 0, NULL}
};

// ============================================================
// 字段信息表
// ============================================================
typedef struct {
    const char *name;
    int mod_id;
    uint8_t item_id;
    ValType value_type;
    const char *desc;
} ItemInfo;

static const ItemInfo g_items[] = {
    /* door */
    {"front_left",      MOD_DOOR,   DOOR_ITEM_FRONT_LEFT,        VAL_TYPE_U8,     "前左门 (0=关 1=开)"},
    {"front_right",     MOD_DOOR,   DOOR_ITEM_FRONT_RIGHT,       VAL_TYPE_U8,     "前右门 (0=关 1=开)"},
    {"back_left",       MOD_DOOR,   DOOR_ITEM_BACK_LEFT,         VAL_TYPE_U8,     "后左门 (0=关 1=开)"},
    {"back_right",      MOD_DOOR,   DOOR_ITEM_BACK_RIGHT,        VAL_TYPE_U8,     "后右门 (0=关 1=开)"},
    {"trunk",           MOD_DOOR,   DOOR_ITEM_TRUNK,             VAL_TYPE_U8,     "后备箱 (0=关 1=开)"},
    {"lock_status",     MOD_DOOR,   DOOR_ITEM_LOCK_STATUS,       VAL_TYPE_U8,     "车锁   (0=解锁 1=上锁)"},
    /* status */
    {"speed",           MOD_STATUS, STATUS_ITEM_SPEED,           VAL_TYPE_F32,    "车速 km/h"},
    {"rpm",             MOD_STATUS, STATUS_ITEM_RPM,             VAL_TYPE_I32,    "转速 RPM"},
    {"water_temp",      MOD_STATUS, STATUS_ITEM_WATER_TEMP,      VAL_TYPE_F32,    "水温 °C"},
    {"oil_temp",        MOD_STATUS, STATUS_ITEM_OIL_TEMP,        VAL_TYPE_F32,    "油温 °C"},
    {"fuel",            MOD_STATUS, STATUS_ITEM_FUEL,            VAL_TYPE_F32,    "油量 0-100%%"},
    {"battery_voltage", MOD_STATUS, STATUS_ITEM_BATTERY_VOLTAGE, VAL_TYPE_F32,    "电压 V"},
    {"gear",            MOD_STATUS, STATUS_ITEM_GEAR,            VAL_TYPE_U8,     "档位 (P/R/D/N/S ASCII值)"},
    {"hand_brake",      MOD_STATUS, STATUS_ITEM_HAND_BRAKE,      VAL_TYPE_U8,     "手刹 (0=释放 1=拉起)"},
    /* seat */
    {"seat_front_left", MOD_SEAT,   SEAT_ITEM_FRONT_LEFT,        VAL_TYPE_U8,     "主驾就坐 (0=无 1=有)"},
    {"seat_front_right",MOD_SEAT,   SEAT_ITEM_FRONT_RIGHT,       VAL_TYPE_U8,     "副驾就坐 (0=无 1=有)"},
    {"angle_back",      MOD_SEAT,   SEAT_ITEM_ANGLE_BACK,        VAL_TYPE_I32,    "靠背角度 (度)"},
    {"heat",            MOD_SEAT,   SEAT_ITEM_HEAT,              VAL_TYPE_U8,     "座椅加热 (0-3)"},
    {"ventilate",       MOD_SEAT,   SEAT_ITEM_VENTILATE,         VAL_TYPE_U8,     "座椅通风 (0-2)"},
    /* air */
    {"ac_switch",       MOD_AIR,    AIR_ITEM_AC_SWITCH,          VAL_TYPE_U8,     "AC开关  (0=关 1=开)"},
    {"fan_speed",       MOD_AIR,    AIR_ITEM_FAN_SPEED,          VAL_TYPE_U8,     "风速    (0-5)"},
    {"temp_set",        MOD_AIR,    AIR_ITEM_TEMP_SET,           VAL_TYPE_I32,    "设定温度 °C"},
    {"inner_cycle",     MOD_AIR,    AIR_ITEM_INNER_CYCLE,        VAL_TYPE_U8,     "内循环  (0=关 1=开)"},
    /* fault */
    {"warning_light",   MOD_FAULT,  FAULT_ITEM_WARNING_LIGHT,    VAL_TYPE_U8,     "报警灯 (0=关 1=开)"},
    /* dvr */
    {"record_status",   MOD_DVR,    DVR_ITEM_RECORD_STATUS,      VAL_TYPE_U8,     "录像状态  (0=关 1=开)"},
    {"video_sd_exist",  MOD_DVR,    DVR_ITEM_VIDEO_SD_EXIST,     VAL_TYPE_U8,     "SD卡      (0=无 1=有)"},
    {"video_time",      MOD_DVR,    DVR_ITEM_VIDEO_TIME,         VAL_TYPE_I32,    "视频时长  (秒)"},
    {"file_path",       MOD_DVR,    DVR_ITEM_FILE_PATH,          VAL_TYPE_STR,    "文件路径"},
    {NULL, 0, 0, 0, NULL}
};

// ============================================================
// 查找函数
// ============================================================
static const ModuleInfo *find_module(const char *name)
{
    for (int i = 0; g_modules[i].name != NULL; i++) {
        if (strcmp(g_modules[i].name, name) == 0) return &g_modules[i];
    }
    return NULL;
}

static const ItemInfo *find_item(int mod_id, const char *name)
{
    for (int i = 0; g_items[i].name != NULL; i++) {
        if (g_items[i].mod_id == mod_id && strcmp(g_items[i].name, name) == 0)
            return &g_items[i];
    }
    return NULL;
}

// ============================================================
// Socket 通信
// ============================================================
static int send_command(const char *sock_path, Msg *cmd, Msg *resp)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("创建socket失败: %s", strerror(errno));
        return -1;
    }

    struct timeval tv = {.tv_sec = 3, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("连接模块失败 (%s): %s", sock_path, strerror(errno));
        close(fd);
        return -1;
    }

    if (send(fd, cmd, sizeof(Msg), 0) < 0) {
        LOG_ERROR("发送命令失败: %s", strerror(errno));
        close(fd);
        return -1;
    }

    memset(resp, 0, sizeof(Msg));
    if (recv(fd, resp, sizeof(Msg), 0) < 0) {
        LOG_ERROR("接收响应失败: %s", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// ============================================================
// 打印 get_all 响应
// ============================================================
static void print_get_all(int mod_id, Msg *resp)
{
    if (resp->result != 0) {
        LOG_ERROR("模块返回失败");
        return;
    }
    if (resp->value_type != VAL_TYPE_STR_U8) {
        LOG_ERROR("响应类型不是 VAL_TYPE_STR_U8");
        return;
    }

    switch (mod_id) {
        case MOD_DOOR: {
            Car_Door d;
            memcpy(&d, resp->value.arr_u8, sizeof(Car_Door));
            printf("=== 车门状态 ===\n");
            printf("  前左门:   %s\n", d.door_front_left  ? "开" : "关");
            printf("  前右门:   %s\n", d.door_front_right ? "开" : "关");
            printf("  后左门:   %s\n", d.door_back_left   ? "开" : "关");
            printf("  后右门:   %s\n", d.door_back_right  ? "开" : "关");
            printf("  后备箱:   %s\n", d.door_trunk       ? "开" : "关");
            printf("  车锁:     %s\n", d.lock_status      ? "上锁" : "解锁");
            break;
        }
        case MOD_STATUS: {
            Car_Status s;
            memcpy(&s, resp->value.arr_u8, sizeof(Car_Status));
            printf("=== 车辆状态 ===\n");
            printf("  车速:     %.1f km/h\n", s.speed);
            printf("  转速:     %d RPM\n",    s.rpm);
            printf("  水温:     %.1f °C\n",   s.water_temp);
            printf("  油温:     %.1f °C\n",   s.oil_temp);
            printf("  油量:     %.1f%%\n",    s.fuel);
            printf("  电压:     %.2f V\n",    s.battery_voltage);
            /* gear 存储方式：ASCII字符 */
            if (s.gear >= 32 && s.gear < 127)
                printf("  档位:     %c\n",   (char)s.gear);
            else
                printf("  档位:     %d\n",   s.gear);
            printf("  手刹:     %s\n", s.hand_brake ? "拉起" : "释放");
            break;
        }
        case MOD_SEAT: {
            Car_Seat s;
            memcpy(&s, resp->value.arr_u8, sizeof(Car_Seat));
            printf("=== 座椅状态 ===\n");
            printf("  主驾:     %s\n", s.seat_front_left  ? "有人" : "无人");
            printf("  副驾:     %s\n", s.seat_front_right ? "有人" : "无人");
            printf("  靠背角度: %d°\n",  s.angle_back);
            printf("  座椅加热: %d 级\n", s.heat);
            printf("  座椅通风: %d 级\n", s.ventilate);
            break;
        }
        case MOD_AIR: {
            Car_Air a;
            memcpy(&a, resp->value.arr_u8, sizeof(Car_Air));
            printf("=== 空调状态 ===\n");
            printf("  AC开关:   %s\n",   a.ac_switch  ? "开" : "关");
            printf("  风速:     %d 级\n", a.fan_speed);
            printf("  设定温度: %d °C\n", a.temp_set);
            printf("  内循环:   %s\n",   a.inner_cycle ? "开" : "关");
            break;
        }
        case MOD_FAULT: {
            Car_Fault f;
            memcpy(&f, resp->value.arr_u8, sizeof(Car_Fault));
            printf("=== 故障信息 ===\n");
            printf("  故障码数量: %d\n", f.fault_count);
            if (f.fault_count > 0) {
                printf("  故障码:   ");
                for (int i = 0; i < f.fault_count && i < MAX_FAULT_CODE; i++)
                    printf("0x%04X ", f.fault_code[i]);
                printf("\n");
            }
            printf("  报警灯:   %s\n", f.warning_light ? "亮" : "灭");
            break;
        }
        case MOD_DVR: {
            Car_Dvr d;
            memcpy(&d, resp->value.arr_u8, sizeof(Car_Dvr));
            printf("=== 行车记录仪 ===\n");
            printf("  录像:     %s\n",   d.record_status  ? "录制中" : "停止");
            printf("  SD卡:     %s\n",   d.video_sd_exist ? "已插入" : "未插入");
            printf("  视频时长: %d 秒\n", d.video_time);
            printf("  文件路径: %s\n",   d.file_path[0] ? d.file_path : "(空)");
            break;
        }
        default:
            LOG_ERROR("未知模块 ID");
            break;
    }
}

// ============================================================
// 打印单字段读取响应
// ============================================================
static void print_read_response(const ItemInfo *item, Msg *resp)
{
    if (resp->result != 0) {
        LOG_ERROR("模块返回失败");
        return;
    }
    printf("[%s] %s = ", item->name, item->desc);
    switch (item->value_type) {
        case VAL_TYPE_U8:
            printf("%u\n", resp->value.u8);
            break;
        case VAL_TYPE_I32:
            printf("%d\n", resp->value.i32);
            break;
        case VAL_TYPE_F32:
            printf("%.2f\n", resp->value.f32);
            break;
        case VAL_TYPE_STR:
            printf("%s\n", resp->value.str);
            break;
        default:
            printf("(类型 %d)\n", item->value_type);
            break;
    }
}

// ============================================================
// 解析写入的值
// ============================================================
static int parse_value(const ItemInfo *item, const char *str_val, Value *out)
{
    memset(out, 0, sizeof(Value));
    switch (item->value_type) {
        case VAL_TYPE_U8:
            out->u8 = (unsigned char)atoi(str_val);
            break;
        case VAL_TYPE_I32:
            out->i32 = atoi(str_val);
            break;
        case VAL_TYPE_F32:
            out->f32 = (float)atof(str_val);
            break;
        case VAL_TYPE_STR:
            strncpy(out->str, str_val, MAX_STR_U8 - 1);
            out->str[MAX_STR_U8 - 1] = '\0';
            break;
        default:
            LOG_ERROR("不支持命令行写入该字段类型");
            return -1;
    }
    return 0;
}

// ============================================================
// 打印帮助
// ============================================================
static void print_help(const char *prog)
{
    printf("用法:\n");
    printf("  %s <模块> get_all\n", prog);
    printf("  %s <模块> read  <字段>\n", prog);
    printf("  %s <模块> write <字段> <值>\n\n", prog);
    printf("模块:\n");
    printf("  door   status   seat   air   fault   dvr\n\n");
    printf("字段列表:\n");
    const char *last_mod = "";
    for (int i = 0; g_items[i].name != NULL; i++) {
        const ModuleInfo *m = NULL;
        for (int j = 0; g_modules[j].name != NULL; j++) {
            if (g_modules[j].mod_id == g_items[i].mod_id) { m = &g_modules[j]; break; }
        }
        if (m && strcmp(m->name, last_mod) != 0) {
            printf("\n  [%s]\n", m->name);
            last_mod = m->name;
        }
        printf("    %-20s %s\n", g_items[i].name, g_items[i].desc);
    }
    printf("\n示例:\n");
    printf("  %s air get_all\n", prog);
    printf("  %s air read ac_switch\n", prog);
    printf("  %s air write ac_switch 1\n", prog);
    printf("  %s air write temp_set 25\n", prog);
    printf("  %s door write lock_status 1\n", prog);
    printf("  %s seat write heat 2\n", prog);
}

// ============================================================
// main
// ============================================================
int main(int argc, char **argv)
{
    // 初始化日志系统
    log_init(NULL, LOG_LEVEL_INFO);

    if (argc < 3) {
        print_help(argv[0]);
        log_close();
        return 1;
    }

    const char *mod_name = argv[1];
    const char *cmd_str  = argv[2];

    /* 查找模块 */
    const ModuleInfo *mod = find_module(mod_name);
    if (mod == NULL) {
        LOG_ERROR("未知模块: %s", mod_name);
        fprintf(stderr, "可用模块: door status seat air fault dvr\n");
        log_close();
        return 1;
    }

    Msg cmd, resp;
    memset(&cmd, 0, sizeof(Msg));
    memset(&resp, 0, sizeof(Msg));
    cmd.msg_type = MSG_TYPE_CMD;
    cmd.mod_id   = mod->mod_id;

    /* ---- get_all ---- */
    if (strcmp(cmd_str, "get_all") == 0) {
        cmd.cmd_type = CMD_GET_ALL;
        cmd.item_id  = 0;
        LOG_INFO("发送 get_all 命令到模块 %s", mod_name);
        if (send_command(mod->sock_path, &cmd, &resp) != 0) {
            log_close();
            return 1;
        }
        print_get_all(mod->mod_id, &resp);
        log_close();
        return resp.result == 0 ? 0 : 1;
    }

    /* ---- read / write 都需要字段名 ---- */
    if (argc < 4) {
        LOG_ERROR("%s 命令需要指定字段名", cmd_str);
        log_close();
        return 1;
    }

    const char *item_name = argv[3];
    const ItemInfo *item = find_item(mod->mod_id, item_name);
    if (item == NULL) {
        LOG_ERROR("模块 [%s] 没有字段: %s", mod_name, item_name);
        log_close();
        return 1;
    }

    /* ---- read ---- */
    if (strcmp(cmd_str, "read") == 0) {
        cmd.cmd_type = CMD_READ_STATUS;
        cmd.item_id  = item->item_id;
        LOG_INFO("发送 read 命令到模块 %s, 字段 %s", mod_name, item_name);
        if (send_command(mod->sock_path, &cmd, &resp) != 0) {
            log_close();
            return 1;
        }
        print_read_response(item, &resp);
        log_close();
        return resp.result == 0 ? 0 : 1;
    }

    /* ---- write ---- */
    if (strcmp(cmd_str, "write") == 0) {
        if (argc < 5) {
            LOG_ERROR("write 命令需要指定值");
            log_close();
            return 1;
        }
        const char *val_str = argv[4];
        Value val;
        if (parse_value(item, val_str, &val) != 0) {
            log_close();
            return 1;
        }

        cmd.cmd_type   = CMD_WRITE_STATUS;
        cmd.item_id    = item->item_id;
        cmd.value_type = item->value_type;
        cmd.value      = val;

        LOG_INFO("发送 write 命令到模块 %s, 字段 %s, 值 %s", mod_name, item_name, val_str);
        if (send_command(mod->sock_path, &cmd, &resp) != 0) {
            log_close();
            return 1;
        }

        if (resp.result == 0) {
            printf("[成功] %s.%s = %s\n", mod_name, item_name, val_str);
            LOG_INFO("写入成功: %s.%s = %s", mod_name, item_name, val_str);
        } else {
            LOG_ERROR("模块拒绝写入 (result=%d)", resp.result);
            log_close();
            return 1;
        }
        log_close();
        return 0;
    }

    LOG_ERROR("未知命令: %s", cmd_str);
    fprintf(stderr, "支持的命令: get_all | read | write\n");
    log_close();
    return 1;
}