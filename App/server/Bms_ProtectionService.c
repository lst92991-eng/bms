#include "Bms_ProtectionService.h"

#include <stddef.h>

#include "Int_BQ76952_BSP.h"

static void Bms_ProtectionService_AddReason(Bms_ProtectionReasonTypeDef *reasons,
                                            size_t max_count,
                                            size_t *count,
                                            Bms_ProtectionReasonCodeTypeDef code,
                                            const char *text)
{
    if (count == NULL)
    {
        return;
    }

    if ((reasons != NULL) && (*count < max_count))
    {
        reasons[*count].code = code;
        reasons[*count].text = text;
    }

    (*count)++;
}

bool Bms_ProtectionService_IsMonitorFaultActive(const Bms_ContextTypeDef *ctx)
{
    const uint16_t alarm_fault_mask = (BQ76952_ALARM_SSBC_MASK |
                                       BQ76952_ALARM_SSA_MASK |
                                       BQ76952_ALARM_PF_MASK |
                                       BQ76952_ALARM_MSK_SFALERT_MASK |
                                       BQ76952_ALARM_MSK_PFALERT_MASK |
                                       BQ76952_ALARM_XDSG_MASK |
                                       BQ76952_ALARM_SHUTV_MASK |
                                       BQ76952_ALARM_FUSE_MASK);

    if (ctx == NULL)
    {
        return true;
    }

    if (ctx->pack.comm_fault ||
        !ctx->pack.cells_valid ||
        !ctx->pack.current_valid ||
        !ctx->pack.temp_valid ||
        ctx->protection.fault_active)
    {
        return true;
    }

    if (((ctx->protection.safety_status_a |
          ctx->protection.safety_status_b |
          ctx->protection.safety_status_c) != 0u) ||
        ((ctx->protection.pf_status_a |
          ctx->protection.pf_status_b |
          ctx->protection.pf_status_c |
          ctx->protection.pf_status_d) != 0u) ||
        ((ctx->protection.alarm_raw & alarm_fault_mask) != 0u))
    {
        return true;
    }

    if ((ctx->protection.fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) == 0u)
    {
        return true;
    }

    return false;
}

bool Bms_ProtectionService_IsDischargeShortCircuit(const Bms_ContextTypeDef *ctx)
{
    if (ctx == NULL)
    {
        return true;
    }

    return ((ctx->protection.safety_status_a & BQ76952_SAFETY_A_SCD_MASK) != 0u);
}

size_t Bms_ProtectionService_CollectMonitorFaultReasons(const Bms_ContextTypeDef *ctx,
                                                        Bms_ProtectionReasonTypeDef *reasons,
                                                        size_t max_count)
{
    size_t count = 0u;

    if (ctx == NULL)
    {
        Bms_ProtectionService_AddReason(reasons,
                                        max_count,
                                        &count,
                                        BMS_PROTECTION_REASON_NULL_CONTEXT,
                                        "BMS模型上下文为空，保护服务按故障处理");
        return count;
    }

    if (ctx->pack.comm_fault)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_COMM_FAULT,
                                        "BQ通信失败或最近一次采样通信异常");
    }
    if (!ctx->pack.cells_valid)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_CELLS_INVALID,
                                        "电芯电压采样无效");
    }
    if (!ctx->pack.current_valid)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_CURRENT_INVALID,
                                        "电流采样无效");
    }
    if (!ctx->pack.temp_valid)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_TEMP_INVALID,
                                        "温度采样无效");
    }
    if (ctx->protection.fault_active)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_APP_FAULT,
                                        "APP电池管理总故障置位");
    }
    if ((ctx->protection.safety_status_a & BQ76952_SAFETY_A_SCD_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_SCD,
                                        "SCD放电短路保护触发");
    }
    if ((ctx->protection.safety_status_a & BQ76952_SAFETY_A_OCD2_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_OCD2,
                                        "OCD2放电过流二级保护触发");
    }
    if ((ctx->protection.safety_status_a & BQ76952_SAFETY_A_OCD1_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_OCD1,
                                        "OCD1放电过流一级保护触发");
    }
    if ((ctx->protection.safety_status_a & BQ76952_SAFETY_A_CUV_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_CUV,
                                        "CUV单体欠压保护触发");
    }
    if ((ctx->protection.safety_status_a & BQ76952_SAFETY_A_COV_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_COV,
                                        "COV单体过压保护触发");
    }
    if ((ctx->protection.safety_status_b & BQ76952_SAFETY_B_OTF_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_OTF,
                                        "FET过温保护触发");
    }
    if ((ctx->protection.safety_status_b & BQ76952_SAFETY_B_OTD_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_OTD,
                                        "放电过温保护触发");
    }
    if ((ctx->protection.safety_status_b & BQ76952_SAFETY_B_UTD_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_UTD,
                                        "放电低温保护触发");
    }
    if ((ctx->protection.safety_status_c & BQ76952_SAFETY_C_OCD3_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_OCD3,
                                        "OCD3放电过流三级保护触发");
    }
    if ((ctx->protection.safety_status_c & BQ76952_SAFETY_C_SCDL_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_SCD_LATCH,
                                        "SCD短路锁存触发");
    }
    if ((ctx->protection.safety_status_c & BQ76952_SAFETY_C_OCDL_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_OCD_LATCH,
                                        "OCD过流锁存触发");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_SSA_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_ALARM_SSA,
                                        "SSA置位，Safety Status A存在告警");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_SSBC_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_ALARM_SSBC,
                                        "SSBC置位，Safety Status B/C存在告警");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_PF_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_ALARM_PF,
                                        "PF告警置位，需要读取永久失效状态");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_MSK_SFALERT_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_ALARM_SF_ALERT,
                                        "SF_ALERT聚合告警置位，需要结合Safety Alert分析");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_MSK_PFALERT_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_ALARM_PF_ALERT,
                                        "PF_ALERT聚合告警置位，需要结合PF状态分析");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_XDSG_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_XDSG,
                                        "XDSG置位，BQ当前禁止/关闭放电FET");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_SHUTV_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_SHUTV,
                                        "SHUTV置位，BQ检测到关断电压相关告警");
    }
    if ((ctx->protection.alarm_raw & BQ76952_ALARM_FUSE_MASK) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_FUSE,
                                        "FUSE置位，熔丝相关告警");
    }
    if ((ctx->protection.fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) == 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_DSG_OFF,
                                        "DSG实际未打开，放电主通道已断开");
    }
    if ((ctx->protection.pf_status_a |
         ctx->protection.pf_status_b |
         ctx->protection.pf_status_c |
         ctx->protection.pf_status_d) != 0u)
    {
        Bms_ProtectionService_AddReason(reasons, max_count, &count,
                                        BMS_PROTECTION_REASON_PF,
                                        "PF永久失效状态非零");
    }

    return count;
}
