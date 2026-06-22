#include "App_Main.h"

/**
 * @brief can接收任务
 */
void can_recv_task(void *arg)
{
    printf("can_recv_task\r\n");
    uint8_t buff[8];
    uint16_t len;
    Int_Can_Init();

    uint16_t can_id;
    while (1)
    {

        vTaskDelay(100);
        len = Int_Can_Recv(&can_id, buff, 8);
        if (len > 0)
        {
            printf("can recv: status: %d\r\n", buff[0]);
            printf("can recv: can_id: %d\r\n", can_id);
            if (can_id == can_id_discharge)
            {
                App_Batman_SetDischargeState(buff[0]);
            }
            else if (can_id == can_id_charge)
            {
                App_Batman_SetChargeState(buff[0]);
            }
        }
    }
}

/**
 * @brief 电池管理任务
 */
void batman_task(void *arg)
{

    App_Batman_Init();
    vTaskDelay(100);
    // Int_BQ769_WakeUp();
    int interval_ms = 5000;
    while (1)
    {
        App_Batman_loadCellsVoltage();
        App_Batman_loadBatVoltage();
        App_Batman_loadCurrent();
        App_Batman_loadTemperature();

        App_Batman_CalcSoc(interval_ms);   
        //
        vTaskDelay(interval_ms);
        //  printf("batman task .......\r\n");
    }
}

/**
 * @brief ups管理任务
 */
void upsman_task(void *arg)
{
    uint8_t second = 60;
    while (1)
    {
        vTaskDelay(1000);
        App_UpsMan_24vPowerCheck(); // 每秒检查一次
        if (second-- == 0)
        {
            second = 60;
            App_UpsMan_McuPowerOffOnUV(); // 每分钟
        }
    }
}
/**
 * @brief 电池均衡任务
 */
void cell_balance_task(void *arg)
{
    vTaskDelay(10 * 1000);
    while (1)
    {
        App_Batman_CellBalance();
        vTaskDelay(60 * 1000);
    }
}

/**
 * @brief 屏幕展示任务
 */
void display_task(void *arg)
{
    App_Display_Init();
    while (1)
    {
        App_Display_Show();
        printf("display task .......\r\n");
        vTaskDelay(5000);
    }
}

/**
 * @brief 库伦计
 */
void coulomb_task(void *arg)
{

    while (1)
    {
        App_Batman_CalcCoulomb(100);
        vTaskDelay(100);
    }
}

/**
 * @brief 启动任务
 * @param
 * @return void
 */
void App_Main(void)
{

    // 任务1
    xTaskCreate(batman_task, "batman_task", 512, NULL, 1, NULL);

    // 任务2
    xTaskCreate(display_task, "display_task", 512, NULL, 2, NULL);
    // 任务3
      xTaskCreate(cell_balance_task, "cell_balance_task", 512, NULL, 2, NULL);

    // 任务4
   // xTaskCreate(can_recv_task, "can_recv_task", 512, NULL, 3, NULL);
    xTaskCreate(coulomb_task, "coulomb_task", 512, NULL, 3, NULL);

    // 任务4  ups任务
    // xTaskCreate(upsman_task, "ups_task", 512, NULL, 3, NULL);

    // // 任务 4  can 接收任务
    // xTaskCreate(can_recv_task, "can_recv_task", 512, NULL, 3, NULL);

    vTaskStartScheduler();
}
