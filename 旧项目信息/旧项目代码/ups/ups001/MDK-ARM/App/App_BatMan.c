#include "App_BatMan.h"
#include "App_Soc.h"

static uint16_t gain_uv = 0;
static int8_t offset_mv = 0;

uint16_t cell_mv[UPS_CELL_NUM];

// 要读取的电池寄存器的位置
static uint8_t cell_reg_indexes[6] = {0, 1, 4, 5, 6, 9};

uint32_t bat_mv;
float current_a;

int8_t tempr;

extern struct _Register_Group BQ769_RegisterGroup;
/**
 * @brief  设置参数
 *
 */
static void configParameters(void)
{
    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Byte = 0;
    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.ADC_EN = 1;
    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.TEMP_SEL = 1;
    Int_BQ769_WriteReg(BQ_SYS_CTRL1, BQ769_RegisterGroup.SysCtrl1.SysCtrl1Byte);

    BQ769_RegisterGroup.SysCtrl2.SysCtrl2Byte = 0;
    BQ769_RegisterGroup.SysCtrl2.SysCtrl2Bit.CC_EN = 1;
    BQ769_RegisterGroup.SysCtrl2.SysCtrl2Bit.DSG_ON = 1;
    BQ769_RegisterGroup.SysCtrl2.SysCtrl2Bit.CHG_ON = 1;
    Int_BQ769_WriteReg(BQ_SYS_CTRL2, BQ769_RegisterGroup.SysCtrl2.SysCtrl2Byte);

    Int_BQ769_WriteReg(BQ_CC_CFG, 0x19); // 库伦计数器的优化
}

/**
 * @brief 保护设置
 */
void configProtectSet(void)
{
    // 短路保护
    BQ769_RegisterGroup.Protect1.Protect1Byte = 0;
    BQ769_RegisterGroup.Protect1.Protect1Bit.RSNS = 1;
      BQ769_RegisterGroup.Protect1.Protect1Bit.SCD_DELAY = BMS_SCD_DELAY_400us;
      BQ769_RegisterGroup.Protect1.Protect1Bit.SCD_THRESH = 7; // 200mv
     Int_BQ769_WriteReg(BQ_PROTECT1, BQ769_RegisterGroup.Protect1.Protect1Byte);
    // 过流保护
    BQ769_RegisterGroup.Protect2.Protect2Byte = 0;
    BQ769_RegisterGroup.Protect2.Protect2Bit.OCD_DELAY = BMS_OCD_DELAY_160ms;
    BQ769_RegisterGroup.Protect2.Protect2Bit.OCD_THRESH = 6; // 50mv
    Int_BQ769_WriteReg(BQ_PROTECT2, BQ769_RegisterGroup.Protect2.Protect2Byte);
    // 过压欠压的延迟
    BQ769_RegisterGroup.Protect3.Protect3Byte = 0;
    // BQ769_RegisterGroup.Protect3.Protect3Bit.OV_DELAY = BMS_OV_DELAY_1s;
    BQ769_RegisterGroup.Protect3.Protect3Bit.UV_DELAY = BMS_UV_DELAY_4s;
    Int_BQ769_WriteReg(BQ_PROTECT3, BQ769_RegisterGroup.Protect3.Protect3Byte);

    // 欠压和过压的值
    uint16_t ov_adc = (uint16_t)(TLB_OV_PROTECT * 1000.0 - offset_mv) / (gain_uv / 1000.0);

    uint8_t ov_adc_reg = ov_adc >> 4 & 0xff;
    Int_BQ769_WriteReg(BQ_OV_TRIP, ov_adc_reg);

    uint16_t uv_adc = (uint16_t)(TLB_UV_PROTECT * 1000.0 - offset_mv) / (gain_uv / 1000.0);

    uint8_t uv_adc_reg = uv_adc >> 4 & 0xff;
    Int_BQ769_WriteReg(BQ_UV_TRIP, uv_adc_reg);
}
/**
 * @brief  从寄存器中取出 gain值 和 offset值
 */
static void loadGainAndOffset(void)
{
    uint8_t gain_uv_43;
    uint8_t gain_uv_210;

    Int_BQ769_ReadReg(BQ_ADCGAIN1, (uint8_t *)&gain_uv_43, 1);
    Int_BQ769_ReadReg(BQ_ADCGAIN2, (uint8_t *)&gain_uv_210, 1);
    // GAIN=365μV/LSB+(ADCGAIN<4:0>in decimal)×(1μV/LSB)
    gain_uv = 365 + ((gain_uv_43 << 1) & 0x18) | ((gain_uv_210 >> 5) & 0x07);

    printf("gain_uv:%d\r\n", gain_uv);

    Int_BQ769_ReadReg(BQ_ADCOFFSET, (uint8_t *)&offset_mv, 1);

    printf("offset_mv:%d\r\n", offset_mv);
}

/**
 * @brief 电池管理的初始化
 *
 1   bq769 重置 唤醒
2   从寄存器中取出 gain值 和 offset值
3   参数设置   ：  启用各种功能  充放电引脚  读数功能
4   阈值设置   ：  电池（欠压  过压) 、  放电过程 电路的 过流 和短路
 */
void App_Batman_Init(void)
{
    // 1   bq769 重置
    Int_BQ769_Reset();
    // 2   从寄存器中取出 gain值 和 offset值

    loadGainAndOffset();
    // 3   参数设置   ：  启用各种功能  充放电引脚  读数功能
    configParameters();
    // 4   阈值设置   ：  电池（欠压  过压) 、  放电过程 电路的 过流 和短路
    configProtectSet();

    // 5   读取数据检查
    uint8_t bq_data[8];
    Int_BQ769_ReadReg(BQ_SYS_CTRL1, bq_data, 8);
    uint8_t state = 0xff;
    Int_BQ769_ReadReg(BQ_SYS_STAT, &state, 1);
    printf("first state:%02x \n", state);
    vTaskDelay(2000);

    // 尝试清零状态寄存器
    // Int_BQ769_WriteReg(BQ_SYS_STAT, 0x00);
    Int_BQ769_ReadReg(BQ_SYS_STAT, &state, 1);

    printf("state:%02x \n", state);
    printf("sysctrl1:%02x \n", bq_data[0]);
    printf("sysctrl2:%02x \n", bq_data[1]);
    // printf("protect1:%02x \n", bq_data[2]);
    // printf("protect2:%02x \n", bq_data[3]);
    // printf("protect3:%02x \n", bq_data[4]);
    // printf("ov_trip:%02x \n", bq_data[5]);
    // printf("uv_trip:%02x \n", bq_data[6]);
    // printf("cc_cfg:%02x \n", bq_data[7]);
    printf("config init success\n");

    /**
     * 初始化SOC EKF
     */
    App_Soc_Init();
}

/**
 * @brief 获取各个电池电压

    //1  从寄存器中取得10个字节的数组
    //2 循环 5 次  从数组中提取分辨率  在根据公式计算出实际物理电压值
 */
void App_Batman_loadCellsVoltage(void)
{
    // 1  从寄存器中取得10个字节的数组
    uint8_t cell_data[2]; //  高位一个字节 低位一个字节
                          // Int_BQ769_ReadReg(BQ_VC1_HI, cell_data, 10 * 2);
                          // // // 2 循环 n 次  从数组中提取分辨率  在根据公式计算出实际物理电压值
                          // for (size_t i = 0; i < 10; i++)
                          // {
                          //     // 高低位拼接为一个uint16_t
                          //     uint16_t cell_adc = (cell_data[i * 2] << 8) | cell_data[i * 2 + 1];

    //     cell_mv[i] = (cell_adc * gain_uv / 1000.0) + offset_mv;
    // }
    uint8_t bq_data[8];
    // // Int_BQ769_WriteReg(BQ_SYS_STAT, 0x00);
    uint8_t state = 0x00;
    Int_BQ769_ReadReg(BQ_SYS_STAT, &state, 1);
    Int_BQ769_ReadReg(BQ_SYS_CTRL1, bq_data, 8);
    printf("state:%02x \n", state);
    printf("sysctrl1:%02x \n", bq_data[0]);
    printf("sysctrl2:%02x \n", bq_data[1]);
    // Int_BQ769_WriteReg(BQ_SYS_CTRL2, 0x43);
    // printf("protect1:%02x \n", bq_data[2]);
    // printf("protect2:%02x \n", bq_data[3]);
    // printf("protect3:%02x \n", bq_data[4]);
    // printf("ov_trip:%02x \n", bq_data[5]);
    // printf("uv_trip:%02x \n", bq_data[6]);
    // printf("cc_cfg:%02x \n", bq_data[7]);
    // printf("config init success\n");
    for (size_t i = 0; i < UPS_CELL_NUM; i++)
    {
        uint8_t cell_reg_index = cell_reg_indexes[i];
        Int_BQ769_ReadReg(BQ_VC1_HI + cell_reg_index * 2, cell_data, 2);
        uint16_t cell_adc = (cell_data[0] << 8) | cell_data[1];

        cell_mv[i] = (cell_adc * gain_uv / 1000.0) + offset_mv;
    }

    // 测试打印
    for (size_t i = 0; i < UPS_CELL_NUM; i++)
    {
        printf("cell_mv[%d]: %d mv\n ", i, cell_mv[i]);
    }
}
/**
 * @brief 获取 整个电池总电压

    //1  从寄存器中取得2个字节的数组
    // 2 提取分辨率  在根据公式计算出实际物理电压值
 */
void App_Batman_loadBatVoltage(void)
{
    // 1  从寄存器中取得2个字节的数组
    uint8_t bat_data[2]; //  高位一个字节 低位一个字节
    Int_BQ769_ReadReg(BQ_BAT_HI, bat_data, 2);
    // 2 提取分辨率  在根据公式计算出实际物理电压值

    // 高低位拼接为一个uint16_t
    uint16_t bat_adc = (bat_data[0] << 8) | bat_data[1];

    bat_mv = (4 * bat_adc * gain_uv / 1000.0) + (UPS_CELL_NUM * offset_mv);

    // 测试打印

    printf("bat_mv : %d mv\n ", bat_mv);
}

/**
 * @brief 充放电电路的电流值
 * //1 从寄存器取 分辨率
 * //2 根据公式计算物理电压值
 * //3 把电压值除电阻 获得电流值
 */
void App_Batman_loadCurrent(void)
{
    // 1 从寄存器取 分辨率
    uint8_t cc_data[2];
    Int_BQ769_ReadReg(BQ_CC_HI, cc_data, 2);
    int16_t cc_adc = (cc_data[0] << 8) | cc_data[1];
    // 2 根据公式计算物理电压值
    float cc_mv = cc_adc * 8.44 / 1000.0;
    // 3 把电压值除电阻 获得电流值
    current_a = cc_mv / 5.0;
    // 4 取绝对值
    // current_a = current_a > 0 ? current_a : -current_a;

    printf("current_a : %.2f A\n ", current_a);
}

/**
 * @brief 获取热敏温度
 * //1 从寄存器中获取Ts1的adc值
 * //2 根据公式计算ts1物理电压值
 * //3 根据公式计算热敏电阻的电阻值
 * //4 用电阻值查表获得对于的温度
 */
void App_Batman_loadTemperature(void)
{

    // 1 从寄存器中获取Ts1的adc值
    uint8_t ts1_data[2];
    Int_BQ769_ReadReg(BQ_TS1_HI, ts1_data, 2);
    uint16_t ts1_adc = (ts1_data[0] << 8) | ts1_data[1];
    // 2 根据公式计算ts1物理电压值
    float ts1_v = ts1_adc * 382.0 / 1000000.0;
    // 3 根据公式计算热敏电阻的电阻值
    uint32_t ts1_r = (10000 * ts1_v) / (3.3 - ts1_v);

    printf("ts1_r = %d\n", ts1_r);
    tempr = Com_BQ769_getTemperByResist(ts1_r);

    printf("tempr = %d C \n", tempr);
}

/**
 * @brief 获取电池包电量百分比
 *
 * 1  计算平均电池的电压
 * 2  通过查表获得电量百分比
 * 3  根据温度补偿值 修正电量
 */
/**
void App_Batman_loadBatSocPercent(void)
{

    //  * 1  计算平均电池的电压
    uint16_t cell_mv_avg = 0;
    uint32_t cell_mv_sum = 0;
    for (int i = 0; i < UPS_CELL_NUM; i++)
    {
        cell_mv_sum += cell_mv[i];
    }
    cell_mv_avg = cell_mv_sum / UPS_CELL_NUM;

    //  * 2  通过查表获得电量百分比

    soc_percent = Com_BQ769_getPercentByVoltage(cell_mv_avg);

    printf("soc_percent:%d %%\r\n ", soc_percent);

    //  * 3  根据温度补偿值 修正电量

    if (tempr >= 0 && tempr <= 40)
    {
        soc_percent = soc_percent * (1 + (-0.005) * (tempr - 25));
    }
    else if (tempr < 0)
    {
        soc_percent = soc_percent * (1 + (-0.005) * (0 - 25) + (-0.005 + 0.002) * (tempr - 0));
    }
    else if (tempr > 40)
    {
        soc_percent = soc_percent * (1 + (-0.005) * (40 - 25) + (-0.005 - 0.003) * (tempr - 40));
    }
    printf("soc_percent after tempr fix:%d %%\r\n", soc_percent);
}

 */

#define STABLE_COLDDOWN_MS 1000 * 60 * 1 // 2分钟
float default_coulomb_mas = 600 * 3600;
float cur_coulomb_mas = 0;
float soc_percent = 0;
float display_soc_percent = 0;
uint16_t cur_stable_colddown_ms = 0;
bool is_stable = true;

void App_Batman_CalcCoulomb(uint16_t delta_ms)
{
    // 1  计算电流mah的毫安时
    cur_coulomb_mas = cur_coulomb_mas + current_a * delta_ms; // 安毫秒 == 毫安秒
}

void App_Batman_CalcSocByCC(void)
{
    // 根据电流mah的毫安时统计电量百分比
    soc_percent = cur_coulomb_mas / default_coulomb_mas;
}



// 电量百分比滤波 IIR滤波
void App_Batman_SocFilter(void)
{
    //alpha 越大，display_soc_percent 向 soc_percent 靠拢越快，所以充电显示会上升更“跟手”。
    const float alpha_charge = 0.25f;
    const float alpha_discharge = 0.12f;
    float alpha = (current_a > 0.01f) ? alpha_charge : alpha_discharge; // 0.01A为充电阈值

    if (display_soc_percent == 0)
    {
        display_soc_percent = soc_percent;
        return;
    }
    // IIR更新
    float candidate = display_soc_percent + alpha * (soc_percent - display_soc_percent);

    if (current_a > 0.01f && candidate < display_soc_percent)
    {
        // 充电：只允许向上
        return;
    }
    else if (current_a <= 0 && candidate > display_soc_percent)
    {
        // 放电 或 进制： 不允许增加
        return;
    }

    // 夹紧范围
    if (candidate < 0.0f)
        candidate = 0.0f;
    if (candidate > 100.0f)
        candidate = 100.0f;

    display_soc_percent = candidate;
    printf("candidate:%f %%\r\n ", candidate);
}
 

void App_Batman_CalcSoc(uint16_t interval_ms)
{
    /**
     * 使用 EKF(卡尔曼) 进行 SOC 估算：
     * - 预测：电流积分推进 SOC
     * - 校正：端电压 + OCV曲线，对SOC做观测校正（大电流时会自动弱化/跳过）
     *
     * 这样无需“稳态判断 + 切换策略”，滤波器会根据噪声模型自动融合电流/电压信息。
     */
    soc_percent = App_Soc_Update(interval_ms, (float)bat_mv, current_a);
    printf(" final soc_percent:%f %%\r\n ", soc_percent);
    // 复用原来的显示平滑滤波（可选）。这里把滤波后的值回写到 soc_percent 供显示使用。
    App_Batman_SocFilter();
    soc_percent = display_soc_percent;

    //如果soc_percent 大于99.99则显示100
    if( soc_percent>99)
    {
        soc_percent = 100;
    }
    printf(" final display_soc_percent:%f %%\r\n ", display_soc_percent);
}

/**
 * @brief 电池均衡
 *
 * 1  选出最小电压的电池 的电压
2   循环比较选出候选
       温度不满足 0    、 压差不满足 0 、 电压不足 0
3    再进行一次筛选
         如果发现相邻的电池候选  二选一  ，留大的
4   把均衡标志位写入均衡寄存器

 */
void App_Batman_CellBalance(void)
{

    //  1  选出最小电压的电池 的电压
    uint16_t min_voltage_mv = 5000;
    uint8_t cell_to_balance[UPS_CELL_NUM];
    for (size_t i = 0; i < UPS_CELL_NUM; i++)
    {
        if (cell_mv[i] < min_voltage_mv)
        {
            min_voltage_mv = cell_mv[i];
        }
    }

    printf("min_voltage_mv:%d mv\r\n", min_voltage_mv);
    // 2   循环比较选出候选
    //        温度不满足 0    、 压差不满足 0 、 电压不足 0
    for (size_t i = 0; i < UPS_CELL_NUM; i++)
    {
        if (tempr >= TLB_BALANCE_MIN_TEMPR && tempr <= TLB_BALANCE_MAX_TEMPR && cell_mv[i] - min_voltage_mv >= (TLB_BALANCE_DIFF_VOLTAGE * 1000) && cell_mv[i] >= (TLB_BALANCE_VOLTAGE * 1000))
        {
            cell_to_balance[i] = 1;
        }
        else
        {
            cell_to_balance[i] = 0;
        }
    }
    for (size_t i = 0; i < UPS_CELL_NUM; i++)
    {
        printf("cadi cell_to_balance[%d]: %d\n", i, cell_to_balance[i]);
    }

    // 3    再进行一次筛选
    //          如果发现相邻的电池候选  二选一  ，留大的
    for (size_t i = 0; i < UPS_CELL_NUM - 1; i++)
    {
        if (cell_to_balance[i] == 1 && cell_to_balance[i + 1] == 1)
        {
            if (cell_mv[i] < cell_mv[i + 1])
            {
                cell_to_balance[i] = 0;
            }
            else
            {
                cell_to_balance[i + 1] = 0;
            }
        }
    }

    for (size_t i = 0; i < UPS_CELL_NUM; i++)
    {
        printf("cell_to_balance[%d]: %d\n", i, cell_to_balance[i]);
    }

    // 4   把均衡标志位写入均衡寄存器
    BQ769_RegisterGroup.CellBal1.CellBal1Bit.CB1 = cell_to_balance[0];
    BQ769_RegisterGroup.CellBal1.CellBal1Bit.CB2 = cell_to_balance[1];
    BQ769_RegisterGroup.CellBal1.CellBal1Bit.CB5 = cell_to_balance[2];
    BQ769_RegisterGroup.CellBal2.CellBal2Bit.CB6 = cell_to_balance[3];
    BQ769_RegisterGroup.CellBal2.CellBal2Bit.CB7 = cell_to_balance[4];
    BQ769_RegisterGroup.CellBal2.CellBal2Bit.CB10 = cell_to_balance[5];

    Int_BQ769_WriteReg(BQ_CELLBAL1, BQ769_RegisterGroup.CellBal1.CellBal1Byte);
    Int_BQ769_WriteReg(BQ_CELLBAL2, BQ769_RegisterGroup.CellBal2.CellBal2Byte);

    // 5   打印结果
    uint8_t cell_balance_status;
    Int_BQ769_ReadReg(BQ_CELLBAL1, &cell_balance_status, 1);
    printf("cell_balance_status1 = %x\n", cell_balance_status);
    for (size_t i = 0; i < 5; i++)
    {
        printf("vc1-5%d balance:%s\n", i, (cell_balance_status >> i) & 0x01 ? "on" : "off");
    }

    Int_BQ769_ReadReg(BQ_CELLBAL2, &cell_balance_status, 1);
    printf("cell_balance_status2 = %x\n", cell_balance_status);
    for (size_t i = 0; i < 5; i++)
    {
        printf("vc6-10%d balance:%s\n", i + 6, (cell_balance_status >> i) & 0x01 ? "on" : "off");
    }
}

/**
 * @brief 设定充电状态
 */
void App_Batman_SetChargeState(uint8_t charge_state)
{
    // 先读取 修改 再写入
    Int_BQ769_ReadReg(BQ_SYS_CTRL2, (uint8_t *)&BQ769_RegisterGroup.SysCtrl2.SysCtrl2Byte, 1);

    BQ769_RegisterGroup.SysCtrl2.SysCtrl2Bit.CHG_ON = charge_state;
    Int_BQ769_WriteReg(BQ_SYS_CTRL2, BQ769_RegisterGroup.SysCtrl2.SysCtrl2Byte);
}

/**
 * @brief 设定放电状态
 */
void App_Batman_SetDischargeState(uint8_t discharge_state)
{
    // 先读取 修改 再写入
    Int_BQ769_ReadReg(BQ_SYS_CTRL2, (uint8_t *)&BQ769_RegisterGroup.SysCtrl2.SysCtrl2Byte, 1);

    BQ769_RegisterGroup.SysCtrl2.SysCtrl2Bit.DSG_ON = discharge_state;
    Int_BQ769_WriteReg(BQ_SYS_CTRL2, BQ769_RegisterGroup.SysCtrl2.SysCtrl2Byte);
}
