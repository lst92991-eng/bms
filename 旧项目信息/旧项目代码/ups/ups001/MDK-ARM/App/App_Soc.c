#include "App_Soc.h"
#include "Com_Bq769.h"

#include <math.h>



static App_Soc_State_t soc_state = {0};

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/**
 * @brief 由 OCV 表估计当前点附近的局部斜率 dV/dSOC
 * @param idx_percent 0~100（与表索引一致）
 * @return dV/dSOC，单位：mV / 1.0 SOC
 */
static float App_Soc_GetDvPerSocFromTable(uint8_t idx_percent)
{
    int dv_mv = 0;

    if (idx_percent >= 100)
    {
        dv_mv = voltage_soc_ocv_table[100] - voltage_soc_ocv_table[99];
    }
    else
    {
        dv_mv = voltage_soc_ocv_table[idx_percent + 1] - voltage_soc_ocv_table[idx_percent];
    }

    if (dv_mv <= 0)
        dv_mv = 1; // 防止表平台/异常导致除0

    // 1% = 0.01SOC，因此 mV/% * 100 = mV/1.0SOC
    return (float)dv_mv * 100.0f;
}

void App_Soc_ConfigInit(App_Soc_Config_t *cfg)
{
    cfg->capacity_ah = 0.6f;                 // TODO：按你的电池真实容量修改
    cfg->r0_ohm = 0.050f;                    // TODO：按你的电池包内阻整定（教程里有方法）
    cfg->cell_num = 6;                       // 本项目 UPS_CELL_NUM=6

    // 过程噪声：越大越容易跟随电压校正（适合电流测量漂移大的情况）
    cfg->p_sigma_soc_per_s = 1e-4f;

    // SOC测量噪声：把“电压查表得到的SOC”当作一个测量值 z（0~1）
    // 值越小：越信电压查表；值越大：越信库仑计量（更平滑）
    cfg->r_sigma_soc = 0.03f; // 3%

    // 仅在小电流（近似OCV）时使用电压观测校正SOC
    cfg->max_update_current_a = 0.10f; // A（可按你的系统静置电流噪声调整）
    // “冷却”门控：只有连续满足 |I|<=max_update_current_a 达到该次数，才进行电压观测更新
    // 等效静置时间约为：update_need_low_current_cnt * App_Soc_Update() 调用周期
    cfg->update_need_low_current_cnt = 5;

    cfg->r_sigma_mv = 10.0f; //10mv 的抖动

}

void App_Soc_Init()
{
    App_Soc_ConfigInit(&soc_state.cfg );

    //低电流计数器 为0时则可以进行电压观测更新
    soc_state.low_current_cnt = soc_state.cfg.update_need_low_current_cnt-1; //默认只差一次就达到观测标准

    //当前电量 0.0 - 1.0
    soc_state.soc=  0.0f;
    soc_state.inited = false;

    // 初始协方差：给一个“中等不确定度”  预测值的方差
    // 经验值：0.05^2 表示 SOC 初始误差大约 ±5%
    soc_state.P = 0.05f * 0.05f;
  
}

 

float App_Soc_GetSocPercent(void)
{
    if (!soc_state.inited)
        return 0.0f;

    return clampf(soc_state.soc * 100.0f, 0.0f, 100.0f);
}


float App_Soc_Update(uint32_t delta_ms, float pack_mv, float current_a)
{
 
    if (delta_ms == 0)
        return App_Soc_GetSocPercent();

    const float dt_s = (float)delta_ms / 1000.0f;

    /**
     * ----------------------------
     * 1) 预测(Predict) —— 库仑计量
     * ----------------------------
     *  
     *
     * 约定：I>0 充电 -> SOC上升；I<0 放电 -> SOC下降
     */
    // 安时法 ///////////////////////
     //  用电量的变化量 / 总电流（转为安秒)  得到变化百分比
    float soc_delta = (current_a * dt_s) / (soc_state.cfg.capacity_ah * 3600.0f);
     // 累加到当前电量百分比上
    soc_state.soc = clampf(soc_state.soc + soc_delta, 0.0f, 1.0f);  
     /////////////////////////////////

    // q_soc_per_s 是 预测过程每秒的误差增量，单位 soc^2/s
    //  预测误差根据时间累积：P = P + q_soc_per_s*dt
    soc_state.P = soc_state.P + soc_state.cfg.p_sigma_soc_per_s * dt_s;
    printf("P = %f\n", soc_state.P);
 

    /**
     * --------------------------------------------
     * 2) 观测更新(Update) —— 简化版：电压 -> SOC测量值 z，然后做 1D KF 更新
     * --------------------------------------------
     * 思路：
     * - 在小电流（近似静置/OCV）时，端电压更接近 OCV 曲线
     * - 直接用 Com_BQ769_getPercentByVoltage() 把“每节电芯电压”映射成一个 SOC测量值 z
     * - 这时测量量与状态同维度（SOC），对应你图里的 1D KF：
     *     K = P / (P + R)
     *     xnew = xp + K * (z - xp)
     *     Pn = K * R 
     */

     //统计低电流次数 如果连续低电流次数超过阈值说明电流比较稳定处于静置状态则进行，查表观测更新
    bool low_current_ok = (fabsf(current_a) <= soc_state.cfg.max_update_current_a);
    if (pack_mv > 0.0f && low_current_ok)
    {
        if (soc_state.low_current_cnt < soc_state.cfg.update_need_low_current_cnt)
            soc_state.low_current_cnt++;
    }
    else
    {
        soc_state.low_current_cnt = 0;
    }


    // 观测更新 条件: 1 电池电压测量正常  2 持续低电流次数超过阈值 3 低电流比较稳定了
    if (pack_mv > 0.0f &&
        low_current_ok &&
        soc_state.low_current_cnt >= soc_state.cfg.update_need_low_current_cnt)
    {

        //标识 初始化过
        soc_state.inited = true;

        float soc_xp = soc_state.soc; // 预测后的状态（SOC，0~1）
        float P = soc_state.P;  // 预测后的方差
    


        // 做一个最简单的 I*R0 补偿，让端电压更接近 OCV
        float v_ocv_pack_mv = pack_mv - current_a * soc_state.cfg.r0_ohm * 1000.0f;
        float v_cell_mv_avg = v_ocv_pack_mv / (float)soc_state.cfg.cell_num;
 

        uint16_t v_cell_mv = (uint16_t)roundf(v_cell_mv_avg);  

        // 查表获得soc
        uint8_t z_percent = Com_BQ769_getPercentByVoltage(v_cell_mv);
        float z = (float)z_percent / 100.0f;

        /**
         * R 时观测值产生的方差，这个值受 磷酸铁锂 soc与vol关系的斜率的影响 因为soc在平台期，对电压非常不敏感容易产生误差
         * 所以斜率越小 R的误差越大
         *
         */
        float dv_per_soc = App_Soc_GetDvPerSocFromTable(z_percent); // mV/1.0SOC   产生斜率 每0.01soc 产生的电压
        float measure_sigma = soc_state.cfg.r_sigma_mv / dv_per_soc;   // 0~1     斜率越小 误差越大
        float measure_sigma_final = fmaxf(soc_state.cfg.r_sigma_soc, measure_sigma);//取两者最大
        float R = measure_sigma_final * measure_sigma_final; // 观测值方差  

        //  K = P/(P+R)   计算出卡尔曼增益值
        float K = P / (P + R);
        printf("Kalman:%f\n", K);

        // xnew = xp + K*(z-xp)
        float xnew = clampf(soc_xp + K * (z - soc_xp), 0.0f, 1.0f);

        //  Pn = (1-K)*P  把观测值误差的方差融合入到预测的方差中
        float Pn = (1.0f - K) * P;
        printf("Pn:%f\n", Pn);

        // 更新状态
        soc_state.soc = xnew;
        soc_state.P = Pn;
    }
 

    return App_Soc_GetSocPercent();
}
