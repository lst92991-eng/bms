#include "App_BatMan_Internal.h"

#include <stddef.h>

#include "Com_BQ76952.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"
#include "main.h"

/**
 * @file App_BatMan_Sample.c
 * @brief BQ76952 有预算、失败即停、整帧提交的遥测采样。
 *
 * 所有读数先进入栈上 frame。任一 transport/protocol/deadline 错误都会立即中止，
 * 旧公开快照保持不变，只更新帧错误状态和有效性门禁。
 */

enum
{
    APP_BATMAN_SAMPLE_BUDGET_MS = 100u,
    APP_BATMAN_TEMP_VALID_MIN_C = -40,
    APP_BATMAN_TEMP_VALID_MAX_C = 100
};

typedef struct
{
    uint32_t start_ms;
    Int_BQ76952_StatusTypeDef error;
} App_BatMan_FrameContextTypeDef;

typedef struct
{
    uint16_t cell_mv[APP_BATMAN_CELL_COUNT];
    uint32_t stack_mv;
    uint32_t pack_mv;
    int32_t current_ma;
    float current_a;
    uint16_t cell_min_mv;
    uint16_t cell_max_mv;
    uint16_t cell_avg_mv;
    uint16_t cell_delta_mv;
    int16_t temp_ic_c;
    int16_t temp_ts1_c;
    int16_t temp_ts3_c;
    int16_t temp_cell_c;
    int16_t temp_fet_c;
    bool temp_ts1_valid;
    bool temp_ts3_valid;
    bool temp_cell_valid;
    uint16_t alarm_status;
    uint16_t alarm_raw;
    uint16_t battery_status;
    uint16_t manufacturing_status;
    uint8_t fet_status;
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
} App_BatMan_SampleFrameTypeDef;

static App_BatMan_FrameStatusTypeDef s_frame_status;

static uint32_t App_BatMan_FrameElapsedMs(const App_BatMan_FrameContextTypeDef *context)
{
    return HAL_GetTick() - context->start_ms;
}
static bool App_BatMan_FrameHasBudget(App_BatMan_FrameContextTypeDef *context)
{
    if (App_BatMan_FrameElapsedMs(context) >= APP_BATMAN_SAMPLE_BUDGET_MS)
    {
        context->error = INT_BQ76952_ERROR_TIMEOUT;
        return false;
    }
    return true;
}

static bool
App_BatMan_ReadDirectU8(App_BatMan_FrameContextTypeDef *context, uint8_t command, uint8_t *value)
{
    Int_BQ76952_StatusTypeDef ret;

    if (!App_BatMan_FrameHasBudget(context))
    {
        return false;
    }
    ret = Int_BQ76952_ReadDirect(command, value, 1u);
    if (ret != INT_BQ76952_OK)
    {
        context->error = ret;
        return false;
    }
    return App_BatMan_FrameHasBudget(context);
}

static bool
App_BatMan_ReadDirectU16(App_BatMan_FrameContextTypeDef *context, uint8_t command, uint16_t *value)
{
    uint8_t data[2];
    Int_BQ76952_StatusTypeDef ret;

    if (!App_BatMan_FrameHasBudget(context))
    {
        return false;
    }
    ret = Int_BQ76952_ReadDirect(command, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        context->error = ret;
        return false;
    }
    if (!App_BatMan_FrameHasBudget(context))
    {
        return false;
    }
    *value = App_BatMan_ReadU16Le(data);
    return true;
}

static bool App_BatMan_ReadManufacturingStatus(App_BatMan_FrameContextTypeDef *context,
                                               uint16_t *value)
{
    uint8_t data[2];
    Int_BQ76952_StatusTypeDef ret;

    if (!App_BatMan_FrameHasBudget(context))
    {
        return false;
    }
    ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        context->error = ret;
        return false;
    }
    if (!App_BatMan_FrameHasBudget(context))
    {
        return false;
    }
    *value = App_BatMan_ReadU16Le(data);
    return true;
}

static bool App_BatMan_IsTempValid(int16_t temp_c)
{
    return ((temp_c >= APP_BATMAN_TEMP_VALID_MIN_C) && (temp_c <= APP_BATMAN_TEMP_VALID_MAX_C));
}

static int32_t App_BatMan_ScaleCc2CurrentMa(int32_t raw_current_ma)
{
    int32_t scaled = raw_current_ma * APP_BATMAN_CC2_RAW_NUMERATOR;

    if (scaled >= 0)
    {
        scaled += (int32_t)(APP_BATMAN_CC2_RAW_DENOMINATOR / 2u);
    }
    else
    {
        scaled -= (int32_t)(APP_BATMAN_CC2_RAW_DENOMINATOR / 2u);
    }
    return scaled / (int32_t)APP_BATMAN_CC2_RAW_DENOMINATOR;
}

static bool App_BatMan_LoadCells(App_BatMan_FrameContextTypeDef *context,
                                 App_BatMan_SampleFrameTypeDef *frame)
{
    static const uint8_t commands[APP_BATMAN_CELL_COUNT] = {BQ76952_CMD_CELL1_VOLTAGE,
                                                            BQ76952_CMD_CELL2_VOLTAGE,
                                                            BQ76952_CMD_CELL6_VOLTAGE,
                                                            BQ76952_CMD_CELL9_VOLTAGE,
                                                            BQ76952_CMD_CELL12_VOLTAGE,
                                                            BQ76952_CMD_CELL16_VOLTAGE};
    uint8_t i;
    uint32_t sum_mv = 0u;

    frame->cell_min_mv = 0xFFFFu;
    frame->cell_max_mv = 0u;
    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (!App_BatMan_ReadDirectU16(context, commands[i], &frame->cell_mv[i]))
        {
            return false;
        }
        sum_mv += frame->cell_mv[i];
        if (frame->cell_mv[i] < frame->cell_min_mv)
        {
            frame->cell_min_mv = frame->cell_mv[i];
        }
        if (frame->cell_mv[i] > frame->cell_max_mv)
        {
            frame->cell_max_mv = frame->cell_mv[i];
        }
    }

    frame->cell_avg_mv = (uint16_t)(sum_mv / APP_BATMAN_CELL_COUNT);
    frame->cell_delta_mv = (uint16_t)(frame->cell_max_mv - frame->cell_min_mv);
    return true;
}

static bool App_BatMan_LoadMeasurements(App_BatMan_FrameContextTypeDef *context,
                                        App_BatMan_SampleFrameTypeDef *frame)
{
    uint16_t raw;
    int32_t raw_current_ma;

    if (!App_BatMan_ReadDirectU16(context, BQ76952_CMD_STACK_VOLTAGE, &raw))
    {
        return false;
    }
    frame->stack_mv = (uint32_t)raw * APP_BATMAN_STACK_RAW_TO_MV;
    frame->pack_mv = frame->stack_mv;

    if (!App_BatMan_ReadDirectU16(context, BQ76952_CMD_CC2_CURRENT, &raw))
    {
        return false;
    }
    raw_current_ma = (int32_t)((int16_t)raw) * APP_BATMAN_CC2_RAW_POLARITY;
    frame->current_ma = App_BatMan_ScaleCc2CurrentMa(raw_current_ma);
    frame->current_a = (float)frame->current_ma / 1000.0f;
    return true;
}

static bool App_BatMan_LoadTemperatures(App_BatMan_FrameContextTypeDef *context,
                                        App_BatMan_SampleFrameTypeDef *frame)
{
    uint16_t raw;
    int16_t temp;

    if (!App_BatMan_ReadDirectU16(context, BQ76952_CMD_INT_TEMPERATURE, &raw))
    {
        return false;
    }
    frame->temp_ic_c = Com_BQ76952_Temp0p1KToC((int16_t)raw);
    frame->temp_fet_c = frame->temp_ic_c;

    if (!App_BatMan_ReadDirectU16(context, BQ76952_CMD_TS1_TEMPERATURE, &raw))
    {
        return false;
    }
    temp = Com_BQ76952_Temp0p1KToC((int16_t)raw);
    frame->temp_ts1_valid = App_BatMan_IsTempValid(temp);
    /* 保留探头原始换算结果用于诊断；不得用 IC 温度伪装成有效 TS1。 */
    frame->temp_ts1_c = temp;

    /*
     * TS3 manifest 当前明确为 disabled：实装 NTC/位置/模型尚未闭环，禁止读取
     * 未配置通道并把随机值当成第二个有效保护温度。
     */
    frame->temp_ts3_valid = false;
    frame->temp_ts3_c = 0;

    if (frame->temp_ts1_valid && frame->temp_ts3_valid)
    {
        frame->temp_cell_c =
            (frame->temp_ts1_c > frame->temp_ts3_c) ? frame->temp_ts1_c : frame->temp_ts3_c;
        frame->temp_cell_valid = true;
    }
    else if (frame->temp_ts1_valid)
    {
        frame->temp_cell_c = frame->temp_ts1_c;
        frame->temp_cell_valid = true;
    }
    else if (frame->temp_ts3_valid)
    {
        frame->temp_cell_c = frame->temp_ts3_c;
        frame->temp_cell_valid = true;
    }
    else
    {
        frame->temp_cell_c = frame->temp_ic_c;
        frame->temp_cell_valid = false;
    }
    return true;
}

static bool App_BatMan_LoadStatus(App_BatMan_FrameContextTypeDef *context,
                                  App_BatMan_SampleFrameTypeDef *frame)
{
    return App_BatMan_ReadDirectU16(context, BQ76952_CMD_ALARM_STATUS, &frame->alarm_status) &&
           App_BatMan_ReadDirectU16(context, BQ76952_CMD_ALARM_RAW_STATUS, &frame->alarm_raw) &&
           App_BatMan_ReadDirectU16(context, BQ76952_CMD_BATTERY_STATUS, &frame->battery_status) &&
           App_BatMan_ReadManufacturingStatus(context, &frame->manufacturing_status) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_FET_STATUS, &frame->fet_status) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_SAFETY_ALERT_A, &frame->safety_alert_a) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_SAFETY_ALERT_B, &frame->safety_alert_b) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_SAFETY_ALERT_C, &frame->safety_alert_c) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_SAFETY_STATUS_A, &frame->safety_status_a) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_SAFETY_STATUS_B, &frame->safety_status_b) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_SAFETY_STATUS_C, &frame->safety_status_c) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_PF_STATUS_A, &frame->pf_status_a) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_PF_STATUS_B, &frame->pf_status_b) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_PF_STATUS_C, &frame->pf_status_c) &&
           App_BatMan_ReadDirectU8(context, BQ76952_CMD_PF_STATUS_D, &frame->pf_status_d);
}

static uint32_t App_BatMan_MakeFaultFlags(const App_BatMan_SampleFrameTypeDef *frame)
{
    uint8_t i;
    uint32_t flags = APP_BATMAN_FAULT_NONE;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((frame->cell_mv[i] < APP_BATMAN_CELL_VALID_MIN_MV) ||
            (frame->cell_mv[i] > APP_BATMAN_CELL_VALID_MAX_MV))
        {
            flags |= APP_BATMAN_FAULT_CELL_RANGE;
        }
    }
    if ((frame->safety_status_a != 0u) || (frame->safety_status_b != 0u) ||
        (frame->safety_status_c != 0u))
    {
        flags |= APP_BATMAN_FAULT_SAFETY;
    }
    if ((frame->pf_status_a != 0u) || (frame->pf_status_b != 0u) || (frame->pf_status_c != 0u) ||
        (frame->pf_status_d != 0u))
    {
        flags |= APP_BATMAN_FAULT_PERMANENT_FAILURE;
    }
    if (!frame->temp_cell_valid)
    {
        flags |= APP_BATMAN_FAULT_CELL_TEMPERATURE_INVALID;
    }
    if (!App_BatMan_IsConfigValid())
    {
        flags |= APP_BATMAN_FAULT_CONFIG_INVALID;
    }
    if (!App_BatMan_ObserveFetStatus(frame->fet_status))
    {
        flags |= APP_BATMAN_FAULT_FET_CONTROL_INVALID;
    }
    return flags;
}

static App_BatMan_FrameStateTypeDef App_BatMan_FrameStateFromError(Int_BQ76952_StatusTypeDef error)
{
    if (error == INT_BQ76952_ERROR_TIMEOUT)
    {
        return APP_BATMAN_FRAME_DEADLINE_EXCEEDED;
    }
    if (error == INT_BQ76952_ERROR_HAL)
    {
        return APP_BATMAN_FRAME_TRANSPORT_ERROR;
    }
    return APP_BATMAN_FRAME_PROTOCOL_ERROR;
}

static void App_BatMan_PublishFrame(const App_BatMan_SampleFrameTypeDef *frame,
                                    const App_BatMan_FrameContextTypeDef *context)
{
    uint8_t i;
    uint32_t flags;
    uint32_t primask = __get_PRIMASK();

    flags = App_BatMan_MakeFaultFlags(frame);

    __disable_irq();
    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        cell_mv[i] = frame->cell_mv[i];
    }
    stack_mv = frame->stack_mv;
    pack_mv = frame->pack_mv;
    current_ma = frame->current_ma;
    current_a = frame->current_a;
    cell_min_mv = frame->cell_min_mv;
    cell_max_mv = frame->cell_max_mv;
    cell_avg_mv = frame->cell_avg_mv;
    cell_delta_mv = frame->cell_delta_mv;
    temp_ic_c = frame->temp_ic_c;
    temp_ts1_c = frame->temp_ts1_c;
    temp_ts3_c = frame->temp_ts3_c;
    temp_cell_c = frame->temp_cell_c;
    temp_fet_c = frame->temp_fet_c;
    alarm_status = frame->alarm_status;
    alarm_raw = frame->alarm_raw;
    battery_status = frame->battery_status;
    manufacturing_status = frame->manufacturing_status;
    fet_status = frame->fet_status;
    safety_alert_a = frame->safety_alert_a;
    safety_alert_b = frame->safety_alert_b;
    safety_alert_c = frame->safety_alert_c;
    safety_status_a = frame->safety_status_a;
    safety_status_b = frame->safety_status_b;
    safety_status_c = frame->safety_status_c;
    pf_status_a = frame->pf_status_a;
    pf_status_b = frame->pf_status_b;
    pf_status_c = frame->pf_status_c;
    pf_status_d = frame->pf_status_d;
    s_cells_sample_valid = true;
    s_current_sample_valid = true;
    s_temp_cell_sample_valid = frame->temp_cell_valid;
    s_comm_fault = false;
    s_fault_flags = flags;
    fault_active = (flags != APP_BATMAN_FAULT_NONE);
    s_frame_status.state = APP_BATMAN_FRAME_VALID;
    s_frame_status.driver_status = (uint8_t)INT_BQ76952_OK;
    s_frame_status.sequence++;
    s_frame_status.last_valid_tick_ms = HAL_GetTick();
    s_frame_status.elapsed_ms = App_BatMan_FrameElapsedMs(context);
    if (primask == 0u)
    {
        __enable_irq();
    }
}

static void App_BatMan_RejectFrame(const App_BatMan_FrameContextTypeDef *context)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_cells_sample_valid = false;
    s_current_sample_valid = false;
    s_temp_cell_sample_valid = false;
    s_comm_fault = true;
    s_fault_flags |= APP_BATMAN_FAULT_COMMUNICATION;
    fault_active = true;
    s_frame_status.state = App_BatMan_FrameStateFromError(context->error);
    s_frame_status.driver_status = (uint8_t)context->error;
    s_frame_status.elapsed_ms = App_BatMan_FrameElapsedMs(context);
    if (primask == 0u)
    {
        __enable_irq();
    }
}

void App_BatMan_ResetSampleState(void)
{
    s_cells_sample_valid = false;
    s_current_sample_valid = false;
    s_temp_cell_sample_valid = false;
    s_frame_status.state = APP_BATMAN_FRAME_NEVER;
    s_frame_status.driver_status = (uint8_t)INT_BQ76952_OK;
    s_frame_status.sequence = 0u;
    s_frame_status.last_valid_tick_ms = 0u;
    s_frame_status.age_ms = 0xFFFFFFFFu;
    s_frame_status.elapsed_ms = 0u;
}

bool App_BatMan_Sample(void)
{
    App_BatMan_FrameContextTypeDef context;
    App_BatMan_SampleFrameTypeDef frame = {0};
    bool frame_complete;

    context.start_ms = HAL_GetTick();
    context.error = Int_BQ76952_BeginTransaction(APP_BATMAN_SAMPLE_BUDGET_MS);
    if (context.error != INT_BQ76952_OK)
    {
        App_BatMan_RejectFrame(&context);
        return false;
    }

    /* 内层所有 direct/subcommand 访问继承本帧同一个绝对 deadline。 */
    frame_complete = App_BatMan_LoadCells(&context, &frame) &&
                     App_BatMan_LoadMeasurements(&context, &frame) &&
                     App_BatMan_LoadTemperatures(&context, &frame) &&
                     App_BatMan_LoadStatus(&context, &frame) && App_BatMan_FrameHasBudget(&context);
    Int_BQ76952_EndTransaction();

    if (!frame_complete)
    {
        App_BatMan_RejectFrame(&context);
        return false;
    }

    App_BatMan_PublishFrame(&frame, &context);
    return true;
}

bool App_BatMan_IsCellTemperatureValid(void)
{
    return s_temp_cell_sample_valid;
}

uint32_t App_BatMan_GetFaultFlags(void)
{
    uint32_t flags = s_fault_flags;
    App_BatMan_FetControlStateTypeDef fet_control;

    if (s_comm_fault)
    {
        flags |= APP_BATMAN_FAULT_COMMUNICATION;
    }
    if (!s_temp_cell_sample_valid)
    {
        flags |= APP_BATMAN_FAULT_CELL_TEMPERATURE_INVALID;
    }
    if (!App_BatMan_IsConfigValid())
    {
        flags |= APP_BATMAN_FAULT_CONFIG_INVALID;
    }
    App_BatMan_GetFetControlState(&fet_control);
    if (!fet_control.request_valid)
    {
        flags |= APP_BATMAN_FAULT_FET_CONTROL_INVALID;
    }
    return flags;
}

void App_BatMan_GetFrameStatus(App_BatMan_FrameStatusTypeDef *status)
{
    uint32_t primask;

    if (status == NULL)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *status = s_frame_status;
    if (primask == 0u)
    {
        __enable_irq();
    }

    if (status->sequence == 0u)
    {
        status->age_ms = 0xFFFFFFFFu;
    }
    else
    {
        status->age_ms = HAL_GetTick() - status->last_valid_tick_ms;
    }
}
