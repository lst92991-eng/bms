#ifndef APP_CAN_BMS_H
#define APP_CAN_BMS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file App_CanBms.h
 * @brief BMS CAN FD 只读遥测协议。
 *
 * 线上的多字节整数均按 little-endian 编码。上位机只能查询状态，协议中没有
 * 充放电 MOS、预充或故障清除等远程控制命令。
 *
 * 帧格式：
 * - 0x600 查询请求固定 3 B：[版本, 命令, 主机序号]。
 * - 0x580 查询应答公共头 4 B：[版本, 命令, 主机序号, 结果]。
 * - 0x180 周期状态 32 B：[版本, 0x01, 设备序号, 保留] + 状态摘要。
 * - 0x100 事件 20 B：[版本, 事件码, 设备序号, 保留] + 触发瞬间快照。
 *
 * 电压、电流、容量和温度单位分别为 mV、mA、mAh、摄氏度；有符号电流正值
 * 表示充电、负值表示放电。百分比无效值编码为 0xFF。
 */

enum
{
    APP_CAN_BMS_PROTOCOL_VERSION = 1u,

    APP_CAN_BMS_ID_EVENT = 0x100u,
    APP_CAN_BMS_ID_PERIODIC_STATUS = 0x180u,
    APP_CAN_BMS_ID_QUERY_RESPONSE = 0x580u,
    APP_CAN_BMS_ID_QUERY_REQUEST = 0x600u,

    APP_CAN_BMS_QUERY_STATUS_SUMMARY = 0x01u,
    APP_CAN_BMS_QUERY_CELL_VOLTAGES = 0x02u,
    APP_CAN_BMS_QUERY_SOH_STATISTICS = 0x03u,
    APP_CAN_BMS_QUERY_PROTECTION_STATUS = 0x04u,

    APP_CAN_BMS_EVENT_LOW_SOC = 0x01u,
    APP_CAN_BMS_EVENT_CRITICAL_SOC = 0x02u,
    APP_CAN_BMS_EVENT_SOC_RECOVERED = 0x03u,
    APP_CAN_BMS_EVENT_FAULT_ACTIVE = 0x04u,
    APP_CAN_BMS_EVENT_FAULT_CLEARED = 0x05u,

    APP_CAN_BMS_RESULT_OK = 0x00u,
    APP_CAN_BMS_RESULT_UNSUPPORTED_VERSION = 0x01u,
    APP_CAN_BMS_RESULT_UNSUPPORTED_COMMAND = 0x02u,
    APP_CAN_BMS_RESULT_DATA_INVALID = 0x03u,
    APP_CAN_BMS_RESULT_MALFORMED_REQUEST = 0x04u
};

enum
{
    APP_CAN_BMS_FLAG_BQ_ONLINE = (1u << 0),
    APP_CAN_BMS_FLAG_FAULT_ACTIVE = (1u << 1),
    APP_CAN_BMS_FLAG_DISCHARGE_ALLOWED = (1u << 2),
    APP_CAN_BMS_FLAG_SOC_VALID = (1u << 3),
    APP_CAN_BMS_FLAG_SOH_VALID = (1u << 4),
    APP_CAN_BMS_FLAG_SOH_LEARNING = (1u << 5),
    APP_CAN_BMS_FLAG_LOW_SOC = (1u << 6),
    APP_CAN_BMS_FLAG_CRITICAL_SOC = (1u << 7),
    APP_CAN_BMS_FLAG_CAN_READY = (1u << 8)
};

/**
 * @brief 初始化 CAN FD 驱动和 BMS 协议状态。
 * @return true 表示 CAN FD 已进入可收发状态；失败后周期任务会继续尝试恢复。
 */
bool App_CanBms_Init(void);

/**
 * @brief 轮询查询请求，并推进周期状态、低电量和故障事件上报。
 * @param interval_ms 本函数相邻两次调用的时间间隔，单位 ms。
 */
void App_CanBms_Task(uint32_t interval_ms);

#endif /* APP_CAN_BMS_H */
