#include "App_CanBms.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "App_BatMan.h"
#include "App_Power.h"
#include "Com_SOC.h"
#include "Int_BQ76952_BSP.h"
#include "Int_CanFd.h"

/**
 * @file App_CanBms.c
 * @brief BMS CAN FD 查询、周期遥测和事件上报应用层。
 *
 * CAN 驱动采用轮询接口，因此本模块应由独立周期任务调用。发送 FIFO 忙时只保留
 * 一帧待发送数据并在下个周期重试；周期状态采用合并策略，避免总线阻塞后补发大量
 * 过期状态。事件使用小型队列保存触发瞬间的数据。
 */

enum
{
    APP_CAN_BMS_PERIODIC_STATUS_MS = 1000u,
    APP_CAN_BMS_REINIT_PERIOD_MS = 1000u,
    APP_CAN_BMS_PENDING_TIMEOUT_MS = 250u,
    APP_CAN_BMS_RX_BUDGET = 4u,
    APP_CAN_BMS_EVENT_QUEUE_CAPACITY = 6u,

    APP_CAN_BMS_LOW_SOC_PERCENT = 15u,
    APP_CAN_BMS_CRITICAL_SOC_PERCENT = 5u,
    APP_CAN_BMS_SOC_RECOVER_PERCENT = 18u,

    APP_CAN_BMS_INVALID_PERCENT = 0xFFu,
    APP_CAN_BMS_ERROR_RESPONSE_LEN = 8u,
    APP_CAN_BMS_CELL_RESPONSE_LEN = 20u,
    APP_CAN_BMS_EVENT_LEN = 20u,
    APP_CAN_BMS_PROTECTION_RESPONSE_LEN = 24u,
    APP_CAN_BMS_SOC_DIAGNOSTICS_LEN = 20u,
    APP_CAN_BMS_STATUS_LEN = 32u,
    APP_CAN_BMS_SOH_RESPONSE_LEN = 32u
};

typedef struct
{
    bool online;
    bool fault;
    bool discharge_allowed;
    bool soc_valid;
    bool soh_valid;
    bool soh_learning;
    App_Power_StateTypeDef power_state;
    float soc_percent;
    float soc_raw_percent;
    Com_SOC_SeedSourceTypeDef soc_seed_source;
    uint8_t soc_confidence;
    uint32_t soc_age_ms;
    uint32_t soc_anchor_sequence;
    App_Power_StopReasonTypeDef stop_reason;
    uint8_t soh_percent;
    uint8_t soh_confidence;
    uint8_t health_score;
    uint8_t fet_status;
    uint16_t cell_mv[APP_BATMAN_CELL_COUNT];
    uint16_t cell_min_mv;
    uint16_t cell_max_mv;
    uint16_t cell_delta_mv;
    uint16_t balance_mask;
    uint32_t pack_mv;
    int32_t current_ma;
    int16_t temp_cell_c;
    uint32_t charge_throughput_mah;
    uint32_t discharge_throughput_mah;
    uint32_t cycle_count;
    uint32_t learned_capacity_mah;
    uint32_t learning_discharge_mah;
    uint16_t learning_count;
    uint16_t alarm_status;
    uint16_t alarm_raw;
    uint16_t battery_status;
    uint16_t manufacturing_status;
    uint8_t safety_alert_a;
    uint8_t safety_alert_b;
    uint8_t safety_alert_c;
    uint8_t safety_status_a;
    uint8_t safety_status_b;
    uint8_t safety_status_c;
    uint8_t pf_status_a;
    uint8_t pf_status_b;
    uint8_t pf_status_c;
    uint8_t pf_status_d;
} App_CanBms_SnapshotTypeDef;

typedef struct
{
    uint16_t id;
    uint8_t len;
    uint8_t data[64];
} App_CanBms_TxFrameTypeDef;

typedef struct
{
    uint8_t data[APP_CAN_BMS_EVENT_LEN];
} App_CanBms_EventFrameTypeDef;

typedef enum
{
    APP_CAN_BMS_SEND_COMPLETE = 0,
    APP_CAN_BMS_SEND_DEFERRED,
    APP_CAN_BMS_SEND_DROPPED
} App_CanBms_SendResultTypeDef;

static bool s_initialized;
static bool s_can_ready;
static bool s_soc_seen;
static bool s_low_soc_active;
static bool s_critical_soc_active;
static bool s_fault_seen;
static bool s_fault_active;
static uint8_t s_unsolicited_sequence;
static uint32_t s_status_elapsed_ms;
static uint32_t s_reinit_elapsed_ms;

static bool s_tx_pending;
static uint32_t s_pending_elapsed_ms;
static App_CanBms_TxFrameTypeDef s_pending_frame;

static App_CanBms_EventFrameTypeDef s_event_queue[APP_CAN_BMS_EVENT_QUEUE_CAPACITY];
static uint8_t s_event_head;
static uint8_t s_event_count;

static void App_CanBms_WriteU16Le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8u);
}

static void App_CanBms_WriteI16Le(uint8_t *data, int16_t value)
{
    App_CanBms_WriteU16Le(data, (uint16_t)value);
}

static void App_CanBms_WriteU32Le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8u) & 0xFFu);
    data[2] = (uint8_t)((value >> 16u) & 0xFFu);
    data[3] = (uint8_t)(value >> 24u);
}

static void App_CanBms_WriteI32Le(uint8_t *data, int32_t value)
{
    App_CanBms_WriteU32Le(data, (uint32_t)value);
}

static uint32_t App_CanBms_AddElapsed(uint32_t elapsed, uint32_t interval_ms, uint32_t limit_ms)
{
    if ((elapsed >= limit_ms) || (interval_ms >= (limit_ms - elapsed)))
    {
        return limit_ms;
    }

    return elapsed + interval_ms;
}

static bool App_CanBms_IsSocValid(const App_CanBms_SnapshotTypeDef *snapshot)
{
    return snapshot->soc_valid && (snapshot->soc_percent >= 0.0f) &&
           (snapshot->soc_percent <= 100.0f);
}

static uint8_t App_CanBms_MakePercent(float value, bool valid)
{
    if (!valid || (value < 0.0f) || (value > 100.0f))
    {
        return APP_CAN_BMS_INVALID_PERCENT;
    }

    return (uint8_t)(value + 0.5f);
}

/**
 * @brief 在临界区内复制 APP 快照，避免 CAN 任务读到 BatMan 任务的半更新数据。
 */
static void App_CanBms_CaptureSnapshot(App_CanBms_SnapshotTypeDef *snapshot)
{
    uint8_t i;
    Com_SOC_ResultTypeDef soc;

    taskENTER_CRITICAL();
    Com_SOC_GetResult(&soc);
    snapshot->online = App_BatMan_IsOnline();
    snapshot->fault = fault_active;
    snapshot->discharge_allowed = App_Power_IsDischargeAllowed();
    snapshot->soh_valid = soh_capacity_valid;
    snapshot->soh_learning = soh_learning_active;
    snapshot->power_state = App_Power_GetState();
    snapshot->soc_valid = soc.valid;
    snapshot->soc_percent = soc.display_percent;
    snapshot->soc_raw_percent = soc.soc_percent;
    snapshot->soc_seed_source = soc.seed_source;
    snapshot->soc_confidence = soc.confidence_percent;
    snapshot->soc_age_ms = soc.age_ms;
    snapshot->soc_anchor_sequence = soc.anchor_event_sequence;
    snapshot->stop_reason = App_Power_GetStopReason();
    snapshot->soh_percent = soh_percent;
    snapshot->soh_confidence = soh_confidence_percent;
    snapshot->health_score = health_score_percent;
    snapshot->fet_status = fet_status;
    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        snapshot->cell_mv[i] = cell_mv[i];
    }
    snapshot->cell_min_mv = cell_min_mv;
    snapshot->cell_max_mv = cell_max_mv;
    snapshot->cell_delta_mv = cell_delta_mv;
    snapshot->balance_mask = balance_mask;
    snapshot->pack_mv = pack_mv;
    snapshot->current_ma = current_ma;
    snapshot->temp_cell_c = temp_cell_c;
    snapshot->charge_throughput_mah = charge_throughput_mah;
    snapshot->discharge_throughput_mah = discharge_throughput_mah;
    snapshot->cycle_count = cycle_count;
    snapshot->learned_capacity_mah = soh_learned_capacity_mah;
    snapshot->learning_discharge_mah = soh_learning_discharge_mah;
    snapshot->learning_count = soh_capacity_learning_count;
    snapshot->alarm_status = alarm_status;
    snapshot->alarm_raw = alarm_raw;
    snapshot->battery_status = battery_status;
    snapshot->manufacturing_status = manufacturing_status;
    snapshot->safety_alert_a = safety_alert_a;
    snapshot->safety_alert_b = safety_alert_b;
    snapshot->safety_alert_c = safety_alert_c;
    snapshot->safety_status_a = safety_status_a;
    snapshot->safety_status_b = safety_status_b;
    snapshot->safety_status_c = safety_status_c;
    snapshot->pf_status_a = pf_status_a;
    snapshot->pf_status_b = pf_status_b;
    snapshot->pf_status_c = pf_status_c;
    snapshot->pf_status_d = pf_status_d;
    taskEXIT_CRITICAL();
}

static uint16_t App_CanBms_MakeFlags(const App_CanBms_SnapshotTypeDef *snapshot)
{
    uint16_t flags = 0u;

    if (snapshot->online)
    {
        flags |= APP_CAN_BMS_FLAG_BQ_ONLINE;
    }
    if (snapshot->fault)
    {
        flags |= APP_CAN_BMS_FLAG_FAULT_ACTIVE;
    }
    if (snapshot->discharge_allowed)
    {
        flags |= APP_CAN_BMS_FLAG_DISCHARGE_ALLOWED;
    }
    if (App_CanBms_IsSocValid(snapshot))
    {
        flags |= APP_CAN_BMS_FLAG_SOC_VALID;
    }
    if (snapshot->soh_valid)
    {
        flags |= APP_CAN_BMS_FLAG_SOH_VALID;
    }
    if (snapshot->soh_learning)
    {
        flags |= APP_CAN_BMS_FLAG_SOH_LEARNING;
    }
    if (s_low_soc_active)
    {
        flags |= APP_CAN_BMS_FLAG_LOW_SOC;
    }
    if (s_critical_soc_active)
    {
        flags |= APP_CAN_BMS_FLAG_CRITICAL_SOC;
    }
    if (s_can_ready)
    {
        flags |= APP_CAN_BMS_FLAG_CAN_READY;
    }

    return flags;
}

static uint16_t App_CanBms_MakeLogicalBalanceMask(uint16_t hardware_mask)
{
    static const uint16_t hardware_cell_mask[APP_BATMAN_CELL_COUNT] = {
        BQ76952_CELL_MASK_6S_HW_CELL1,
        BQ76952_CELL_MASK_6S_HW_CELL2,
        BQ76952_CELL_MASK_6S_HW_CELL3,
        BQ76952_CELL_MASK_6S_HW_CELL4,
        BQ76952_CELL_MASK_6S_HW_CELL5,
        BQ76952_CELL_MASK_6S_HW_CELL6};
    uint16_t logical_mask = 0u;
    uint8_t i;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((hardware_mask & hardware_cell_mask[i]) != 0u)
        {
            logical_mask |= (uint16_t)(1u << i);
        }
    }

    return logical_mask;
}

/**
 * @brief 填充状态摘要公共区，查询应答和周期上报共用同一字段布局。
 */
static void App_CanBms_FillStatusBody(uint8_t data[APP_CAN_BMS_STATUS_LEN],
                                      const App_CanBms_SnapshotTypeDef *snapshot)
{
    bool soc_valid = App_CanBms_IsSocValid(snapshot);
    uint16_t flags = App_CanBms_MakeFlags(snapshot);

    App_CanBms_WriteU16Le(&data[4], flags);
    data[6] = (uint8_t)snapshot->power_state;
    data[7] = snapshot->fet_status;
    data[8] = App_CanBms_MakePercent(snapshot->soc_percent, soc_valid);
    data[9] = snapshot->soc_confidence;
    data[10] = snapshot->soh_valid ? snapshot->soh_percent : APP_CAN_BMS_INVALID_PERCENT;
    data[11] = snapshot->soh_confidence;
    App_CanBms_WriteU32Le(&data[12], snapshot->pack_mv);
    App_CanBms_WriteI32Le(&data[16], snapshot->current_ma);
    App_CanBms_WriteU16Le(&data[20], snapshot->cell_min_mv);
    App_CanBms_WriteU16Le(&data[22], snapshot->cell_max_mv);
    App_CanBms_WriteU16Le(&data[24], snapshot->cell_delta_mv);
    App_CanBms_WriteI16Le(&data[26], snapshot->temp_cell_c);
    App_CanBms_WriteU16Le(&data[28], App_CanBms_MakeLogicalBalanceMask(snapshot->balance_mask));
    data[30] = snapshot->health_score;
    data[31] = (uint8_t)snapshot->stop_reason;
}

static void App_CanBms_BuildEvent(uint8_t event_code,
                                  const App_CanBms_SnapshotTypeDef *snapshot,
                                  uint8_t data[APP_CAN_BMS_EVENT_LEN])
{
    bool soc_valid = App_CanBms_IsSocValid(snapshot);

    (void)memset(data, 0, APP_CAN_BMS_EVENT_LEN);
    data[0] = APP_CAN_BMS_PROTOCOL_VERSION;
    data[1] = event_code;
    data[2] = s_unsolicited_sequence++;
    data[3] = (uint8_t)snapshot->stop_reason;
    App_CanBms_WriteU16Le(&data[4], App_CanBms_MakeFlags(snapshot));
    data[6] = App_CanBms_MakePercent(snapshot->soc_percent, soc_valid);
    data[7] = (uint8_t)snapshot->power_state;
    App_CanBms_WriteU16Le(&data[8], snapshot->cell_min_mv);
    App_CanBms_WriteU16Le(&data[10], snapshot->cell_max_mv);
    App_CanBms_WriteU32Le(&data[12], snapshot->pack_mv);
    App_CanBms_WriteI32Le(&data[16], snapshot->current_ma);
}

/**
 * @brief 保存状态变化事件；队列满时淘汰最旧事件，保证最新安全状态可见。
 */
static void App_CanBms_QueueEvent(uint8_t event_code, const App_CanBms_SnapshotTypeDef *snapshot)
{
    uint8_t tail;

    if (s_event_count >= APP_CAN_BMS_EVENT_QUEUE_CAPACITY)
    {
        s_event_head = (uint8_t)((s_event_head + 1u) % APP_CAN_BMS_EVENT_QUEUE_CAPACITY);
        s_event_count--;
    }

    tail = (uint8_t)((s_event_head + s_event_count) % APP_CAN_BMS_EVENT_QUEUE_CAPACITY);
    App_CanBms_BuildEvent(event_code, snapshot, s_event_queue[tail].data);
    s_event_count++;
}

static void App_CanBms_UpdateEventState(const App_CanBms_SnapshotTypeDef *snapshot)
{
    bool soc_valid = App_CanBms_IsSocValid(snapshot);

    if (!s_fault_seen)
    {
        s_fault_seen = true;
        s_fault_active = snapshot->fault;
        if (snapshot->fault)
        {
            App_CanBms_QueueEvent(APP_CAN_BMS_EVENT_FAULT_ACTIVE, snapshot);
        }
    }
    else if (snapshot->fault != s_fault_active)
    {
        s_fault_active = snapshot->fault;
        App_CanBms_QueueEvent(snapshot->fault ? APP_CAN_BMS_EVENT_FAULT_ACTIVE
                                              : APP_CAN_BMS_EVENT_FAULT_CLEARED,
                              snapshot);
    }

    /* SOC 无效时保持原告警锁存，不能把通信丢失误判成低电恢复。 */
    if (!soc_valid)
    {
        return;
    }

    if (!s_soc_seen)
    {
        s_soc_seen = true;
        if (snapshot->soc_percent <= (float)APP_CAN_BMS_CRITICAL_SOC_PERCENT)
        {
            s_low_soc_active = true;
            s_critical_soc_active = true;
            App_CanBms_QueueEvent(APP_CAN_BMS_EVENT_CRITICAL_SOC, snapshot);
        }
        else if (snapshot->soc_percent <= (float)APP_CAN_BMS_LOW_SOC_PERCENT)
        {
            s_low_soc_active = true;
            App_CanBms_QueueEvent(APP_CAN_BMS_EVENT_LOW_SOC, snapshot);
        }
        return;
    }

    if (!s_low_soc_active)
    {
        if (snapshot->soc_percent <= (float)APP_CAN_BMS_CRITICAL_SOC_PERCENT)
        {
            s_low_soc_active = true;
            s_critical_soc_active = true;
            App_CanBms_QueueEvent(APP_CAN_BMS_EVENT_CRITICAL_SOC, snapshot);
        }
        else if (snapshot->soc_percent <= (float)APP_CAN_BMS_LOW_SOC_PERCENT)
        {
            s_low_soc_active = true;
            App_CanBms_QueueEvent(APP_CAN_BMS_EVENT_LOW_SOC, snapshot);
        }
        return;
    }

    if (!s_critical_soc_active &&
        (snapshot->soc_percent <= (float)APP_CAN_BMS_CRITICAL_SOC_PERCENT))
    {
        s_critical_soc_active = true;
        App_CanBms_QueueEvent(APP_CAN_BMS_EVENT_CRITICAL_SOC, snapshot);
    }

    /* 低电和严重低电均保持锁存，只有达到统一恢复阈值才解除，避免阈值附近重复告警。 */
    if (snapshot->soc_percent >= (float)APP_CAN_BMS_SOC_RECOVER_PERCENT)
    {
        s_low_soc_active = false;
        s_critical_soc_active = false;
        App_CanBms_QueueEvent(APP_CAN_BMS_EVENT_SOC_RECOVERED, snapshot);
    }
}

static void App_CanBms_SavePending(uint16_t id, const uint8_t *data, uint8_t len)
{
    s_pending_frame.id = id;
    s_pending_frame.len = len;
    (void)memcpy(s_pending_frame.data, data, len);
    s_pending_elapsed_ms = 0u;
    s_tx_pending = true;
}

static App_CanBms_SendResultTypeDef
App_CanBms_SendOrDefer(uint16_t id, const uint8_t *data, uint8_t len)
{
    Int_CanFd_StatusTypeDef ret = Int_CanFd_Send(id, data, len);

    if (ret == INT_CANFD_OK)
    {
        return APP_CAN_BMS_SEND_COMPLETE;
    }
    if ((ret == INT_CANFD_BUSY) || (ret == INT_CANFD_HAL))
    {
        App_CanBms_SavePending(id, data, len);
        if (ret == INT_CANFD_HAL)
        {
            s_can_ready = false;
            s_reinit_elapsed_ms = APP_CAN_BMS_REINIT_PERIOD_MS;
        }
        return APP_CAN_BMS_SEND_DEFERRED;
    }

    return APP_CAN_BMS_SEND_DROPPED;
}

static bool App_CanBms_FlushPending(void)
{
    Int_CanFd_StatusTypeDef ret;

    if (!s_tx_pending)
    {
        return true;
    }

    ret = Int_CanFd_Send(s_pending_frame.id, s_pending_frame.data, s_pending_frame.len);
    if (ret == INT_CANFD_OK)
    {
        s_tx_pending = false;
        s_pending_elapsed_ms = 0u;
        return true;
    }
    if (ret == INT_CANFD_PARAM)
    {
        /* 内部帧长度固定且合法；若仍返回 PARAM，丢弃该帧避免永久阻塞队头。 */
        s_tx_pending = false;
        s_pending_elapsed_ms = 0u;
        return true;
    }
    if (ret == INT_CANFD_HAL)
    {
        s_can_ready = false;
        s_reinit_elapsed_ms = APP_CAN_BMS_REINIT_PERIOD_MS;
    }

    return false;
}

static void App_CanBms_PrioritizeEvent(void)
{
    if (s_tx_pending && (s_pending_frame.id != APP_CAN_BMS_ID_EVENT) && (s_event_count > 0u))
    {
        /* 故障/低电事件可淘汰尚未进入硬件 FIFO 的旧查询应答或周期帧。 */
        s_tx_pending = false;
        s_pending_elapsed_ms = 0u;
    }
}

static void App_CanBms_AgePending(uint32_t interval_ms)
{
    if (!s_tx_pending)
    {
        return;
    }

    s_pending_elapsed_ms =
        App_CanBms_AddElapsed(s_pending_elapsed_ms, interval_ms, APP_CAN_BMS_PENDING_TIMEOUT_MS);
    if (s_pending_elapsed_ms >= APP_CAN_BMS_PENDING_TIMEOUT_MS)
    {
        /*
         * 事件最终可由周期状态收敛；不能让一帧永久占住软件队头。
         * 超时后丢弃旧帧并强制真正 Stop/Start 控制器。
         */
        s_tx_pending = false;
        s_pending_elapsed_ms = 0u;
        s_can_ready = false;
        s_reinit_elapsed_ms = APP_CAN_BMS_REINIT_PERIOD_MS;
    }
}

static void App_CanBms_BuildQueryResponse(const Int_CanFd_FrameTypeDef *request,
                                          const App_CanBms_SnapshotTypeDef *snapshot,
                                          App_CanBms_TxFrameTypeDef *response)
{
    uint8_t command = (request->len > 1u) ? request->data[1] : 0u;
    uint8_t sequence = (request->len > 2u) ? request->data[2] : 0u;
    uint8_t i;

    response->id = APP_CAN_BMS_ID_QUERY_RESPONSE;
    response->len = APP_CAN_BMS_ERROR_RESPONSE_LEN;
    (void)memset(response->data, 0, sizeof(response->data));
    response->data[0] = APP_CAN_BMS_PROTOCOL_VERSION;
    response->data[1] = command;
    response->data[2] = sequence;

    if (request->len != 3u)
    {
        response->data[3] = APP_CAN_BMS_RESULT_MALFORMED_REQUEST;
        return;
    }
    if (request->data[0] != APP_CAN_BMS_PROTOCOL_VERSION)
    {
        response->data[3] = APP_CAN_BMS_RESULT_UNSUPPORTED_VERSION;
        return;
    }

    switch (command)
    {
        case APP_CAN_BMS_QUERY_STATUS_SUMMARY:
            response->len = APP_CAN_BMS_STATUS_LEN;
            response->data[3] = App_CanBms_IsSocValid(snapshot) ? APP_CAN_BMS_RESULT_OK
                                                                : APP_CAN_BMS_RESULT_DATA_INVALID;
            App_CanBms_FillStatusBody(response->data, snapshot);
            break;

        case APP_CAN_BMS_QUERY_CELL_VOLTAGES:
            response->len = APP_CAN_BMS_CELL_RESPONSE_LEN;
            response->data[3] =
                snapshot->online ? APP_CAN_BMS_RESULT_OK : APP_CAN_BMS_RESULT_DATA_INVALID;
            for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
            {
                App_CanBms_WriteU16Le(&response->data[4u + ((uint16_t)i * 2u)],
                                      snapshot->cell_mv[i]);
            }
            App_CanBms_WriteU32Le(&response->data[16], snapshot->pack_mv);
            break;

        case APP_CAN_BMS_QUERY_SOH_STATISTICS:
            response->len = APP_CAN_BMS_SOH_RESPONSE_LEN;
            response->data[3] =
                snapshot->soh_valid ? APP_CAN_BMS_RESULT_OK : APP_CAN_BMS_RESULT_DATA_INVALID;
            response->data[4] =
                snapshot->soh_valid ? snapshot->soh_percent : APP_CAN_BMS_INVALID_PERCENT;
            response->data[5] = snapshot->health_score;
            response->data[6] = snapshot->soh_confidence;
            response->data[7] =
                (uint8_t)((snapshot->soh_valid ? 1u : 0u) | (snapshot->soh_learning ? 2u : 0u));
            App_CanBms_WriteU32Le(&response->data[8], snapshot->learned_capacity_mah);
            App_CanBms_WriteU32Le(&response->data[12], snapshot->charge_throughput_mah);
            App_CanBms_WriteU32Le(&response->data[16], snapshot->discharge_throughput_mah);
            App_CanBms_WriteU32Le(&response->data[20], snapshot->cycle_count);
            App_CanBms_WriteU32Le(&response->data[24], snapshot->learning_discharge_mah);
            App_CanBms_WriteU16Le(&response->data[28], snapshot->learning_count);
            break;

        case APP_CAN_BMS_QUERY_PROTECTION_STATUS:
            response->len = APP_CAN_BMS_PROTECTION_RESPONSE_LEN;
            response->data[3] =
                snapshot->online ? APP_CAN_BMS_RESULT_OK : APP_CAN_BMS_RESULT_DATA_INVALID;
            App_CanBms_WriteU16Le(&response->data[4], snapshot->alarm_status);
            App_CanBms_WriteU16Le(&response->data[6], snapshot->alarm_raw);
            App_CanBms_WriteU16Le(&response->data[8], snapshot->battery_status);
            App_CanBms_WriteU16Le(&response->data[10], snapshot->manufacturing_status);
            response->data[12] = snapshot->fet_status;
            response->data[13] = snapshot->safety_alert_a;
            response->data[14] = snapshot->safety_alert_b;
            response->data[15] = snapshot->safety_alert_c;
            response->data[16] = snapshot->safety_status_a;
            response->data[17] = snapshot->safety_status_b;
            response->data[18] = snapshot->safety_status_c;
            response->data[19] = snapshot->pf_status_a;
            response->data[20] = snapshot->pf_status_b;
            response->data[21] = snapshot->pf_status_c;
            response->data[22] = snapshot->pf_status_d;
            response->data[23] = snapshot->fault ? 1u : 0u;
            break;

        case APP_CAN_BMS_QUERY_SOC_DIAGNOSTICS:
            response->len = APP_CAN_BMS_SOC_DIAGNOSTICS_LEN;
            response->data[3] = App_CanBms_IsSocValid(snapshot) ? APP_CAN_BMS_RESULT_OK
                                                                : APP_CAN_BMS_RESULT_DATA_INVALID;
            response->data[4] = snapshot->soc_valid ? 1u : 0u;
            response->data[5] = (uint8_t)snapshot->soc_seed_source;
            response->data[6] = snapshot->soc_confidence;
            response->data[7] = (uint8_t)snapshot->stop_reason;
            App_CanBms_WriteU32Le(&response->data[8], snapshot->soc_age_ms);
            App_CanBms_WriteU32Le(&response->data[12], snapshot->soc_anchor_sequence);
            App_CanBms_WriteU16Le(&response->data[16],
                                  snapshot->soc_valid
                                      ? (uint16_t)(snapshot->soc_raw_percent * 100.0f + 0.5f)
                                      : 0xFFFFu);
            App_CanBms_WriteU16Le(
                &response->data[18],
                snapshot->soc_valid ? (uint16_t)(snapshot->soc_percent * 100.0f + 0.5f) : 0xFFFFu);
            break;

        default:
            response->data[3] = APP_CAN_BMS_RESULT_UNSUPPORTED_COMMAND;
            break;
    }
}

static bool App_CanBms_SendNextEvent(void)
{
    App_CanBms_SendResultTypeDef send_result;

    if (s_event_count == 0u)
    {
        return true;
    }

    send_result = App_CanBms_SendOrDefer(
        APP_CAN_BMS_ID_EVENT, s_event_queue[s_event_head].data, APP_CAN_BMS_EVENT_LEN);
    /* DEFERRED 时待发送槽已接管帧；内部参数错误也必须出队，避免永久卡住事件队头。 */
    s_event_head = (uint8_t)((s_event_head + 1u) % APP_CAN_BMS_EVENT_QUEUE_CAPACITY);
    s_event_count--;

    return send_result != APP_CAN_BMS_SEND_DEFERRED;
}

static bool App_CanBms_ProcessQueries(const App_CanBms_SnapshotTypeDef *snapshot)
{
    Int_CanFd_FrameTypeDef request;
    App_CanBms_TxFrameTypeDef response;
    Int_CanFd_StatusTypeDef receive_result;
    App_CanBms_SendResultTypeDef send_result;
    uint8_t count;

    for (count = 0u; count < APP_CAN_BMS_RX_BUDGET; count++)
    {
        receive_result = Int_CanFd_Receive(&request);
        if (receive_result == INT_CANFD_EMPTY)
        {
            return true;
        }
        if (receive_result == INT_CANFD_HAL)
        {
            s_can_ready = false;
            s_reinit_elapsed_ms = APP_CAN_BMS_REINIT_PERIOD_MS;
            return false;
        }
        if (receive_result != INT_CANFD_OK)
        {
            continue;
        }
        if (request.id != APP_CAN_BMS_ID_QUERY_REQUEST)
        {
            continue;
        }

        App_CanBms_BuildQueryResponse(&request, snapshot, &response);
        send_result = App_CanBms_SendOrDefer(response.id, response.data, response.len);
        if (send_result == APP_CAN_BMS_SEND_DEFERRED)
        {
            return false;
        }
    }

    return true;
}

static bool App_CanBms_SendPeriodicStatus(const App_CanBms_SnapshotTypeDef *snapshot)
{
    uint8_t data[APP_CAN_BMS_STATUS_LEN] = {0};
    App_CanBms_SendResultTypeDef send_result;

    if (s_status_elapsed_ms < APP_CAN_BMS_PERIODIC_STATUS_MS)
    {
        return true;
    }

    data[0] = APP_CAN_BMS_PROTOCOL_VERSION;
    data[1] = APP_CAN_BMS_QUERY_STATUS_SUMMARY;
    data[2] = s_unsolicited_sequence++;
    data[3] = 0u;
    App_CanBms_FillStatusBody(data, snapshot);

    send_result =
        App_CanBms_SendOrDefer(APP_CAN_BMS_ID_PERIODIC_STATUS, data, APP_CAN_BMS_STATUS_LEN);
    /* 忙时待发送帧已经保存；内部错误也按周期节流，不能在每次任务里连续重试。 */
    s_status_elapsed_ms = 0u;

    return send_result != APP_CAN_BMS_SEND_DEFERRED;
}

static bool App_CanBms_TryRestoreCan(uint32_t interval_ms)
{
    if (s_can_ready)
    {
        return true;
    }

    s_reinit_elapsed_ms =
        App_CanBms_AddElapsed(s_reinit_elapsed_ms, interval_ms, APP_CAN_BMS_REINIT_PERIOD_MS);
    if (s_reinit_elapsed_ms < APP_CAN_BMS_REINIT_PERIOD_MS)
    {
        return false;
    }

    s_reinit_elapsed_ms = 0u;
    s_can_ready = (Int_CanFd_Init(APP_CAN_BMS_ID_QUERY_REQUEST) == INT_CANFD_OK);
    return s_can_ready;
}

bool App_CanBms_Init(void)
{
    s_initialized = true;
    s_can_ready = false;
    s_soc_seen = false;
    s_low_soc_active = false;
    s_critical_soc_active = false;
    s_fault_seen = false;
    s_fault_active = false;
    s_unsolicited_sequence = 0u;
    s_status_elapsed_ms = APP_CAN_BMS_PERIODIC_STATUS_MS;
    s_reinit_elapsed_ms = 0u;
    s_tx_pending = false;
    s_pending_elapsed_ms = 0u;
    s_event_head = 0u;
    s_event_count = 0u;

    s_can_ready = (Int_CanFd_Init(APP_CAN_BMS_ID_QUERY_REQUEST) == INT_CANFD_OK);
    return s_can_ready;
}

void App_CanBms_Task(uint32_t interval_ms)
{
    App_CanBms_SnapshotTypeDef snapshot;

    if (!s_initialized)
    {
        (void)App_CanBms_Init();
    }

    s_status_elapsed_ms =
        App_CanBms_AddElapsed(s_status_elapsed_ms, interval_ms, APP_CAN_BMS_PERIODIC_STATUS_MS);
    App_CanBms_CaptureSnapshot(&snapshot);
    App_CanBms_UpdateEventState(&snapshot);
    App_CanBms_PrioritizeEvent();
    App_CanBms_AgePending(interval_ms);

    if (!App_CanBms_TryRestoreCan(interval_ms))
    {
        return;
    }
    if (!App_CanBms_FlushPending())
    {
        return;
    }

    /* 状态变化事件优先于查询和周期帧，避免低电/故障提示被查询流量长期压住。 */
    if (!App_CanBms_SendNextEvent())
    {
        return;
    }
    if (!App_CanBms_ProcessQueries(&snapshot))
    {
        return;
    }
    (void)App_CanBms_SendPeriodicStatus(&snapshot);
}
