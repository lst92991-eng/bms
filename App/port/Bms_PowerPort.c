#include "Bms_PowerPort.h"

#include "Bms_Sc8815Port.h"
#include "Bms_BqConfigPort.h"

bool Bms_PowerPort_ApplyMainOutput(bool charge_enable, bool discharge_enable)
{
    /*
     * 关闭充电请求必须先于 BQ FET 动作，避免故�?低电�?SC8815 继续释放功率环路�?     */
    if (!charge_enable)
    {
        Bms_Sc8815Port_RequestCharge(false);
    }

    if (!Bms_BqConfigPort_SetMainFets(charge_enable, discharge_enable))
    {
        Bms_Sc8815Port_RequestCharge(false);
        return false;
    }

    Bms_Sc8815Port_RequestCharge(charge_enable);
    return true;
}

bool Bms_PowerPort_ApplyPreDischarge(bool charge_enable)
{
    if (!charge_enable)
    {
        Bms_Sc8815Port_RequestCharge(false);
    }

    if (!Bms_BqConfigPort_SetPreDischargeFet(charge_enable))
    {
        Bms_Sc8815Port_RequestCharge(false);
        return false;
    }

    Bms_Sc8815Port_RequestCharge(charge_enable);
    return true;
}

bool Bms_PowerPort_AllMainFetsOff(void)
{
    return Bms_BqConfigPort_AllMainFetsOff();
}

void Bms_PowerPort_SetChargeRequest(bool enable)
{
    Bms_Sc8815Port_RequestCharge(enable);
}

bool Bms_PowerPort_IsInputPresent(uint32_t valid_vbus_mv)
{
    return (Bms_Sc8815Port_IsAcOk() ||
            (Bms_Sc8815Port_GetVbusMv() >= valid_vbus_mv));
}

bool Bms_PowerPort_HasScFault(void)
{
    return Bms_Sc8815Port_HasFault();
}

uint32_t Bms_PowerPort_GetVbusMv(void)
{
    return Bms_Sc8815Port_GetVbusMv();
}
