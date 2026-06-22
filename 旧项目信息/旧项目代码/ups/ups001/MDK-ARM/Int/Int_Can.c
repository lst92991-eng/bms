#include "Int_Can.h"

/**
 * @brief can的初始化
 */
void Int_Can_Init(void)
{
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterActivation = ENABLE;
    // 1  过滤器的类型 精准or模糊  16位 32位
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDLIST;  // 精准
    sFilterConfig.FilterScale = CAN_FILTERSCALE_16BIT; // 16位 标准帧

    sFilterConfig.FilterIdHigh = (can_id_charge << 5) & 0xFFFF;
    sFilterConfig.FilterIdLow = (can_id_discharge << 5) & 0xFFFF;
    sFilterConfig.FilterBank = 0; // 过滤器组号

    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0; // 邮箱

    HAL_CAN_ConfigFilter(&hcan, &sFilterConfig);
    HAL_CAN_Start(&hcan);
}

/**
 * @brief 接受数据
 * @param buff 接收数据的容器
 * @param buff_len 容器的长度
 * @return 实际接收到的数据长度
 */
uint16_t Int_Can_Recv(uint16_t *can_id, uint8_t *buff, uint16_t buff_len)
{
    // 1 检查收件邮箱是否有数据
    if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0)
    {
        return 0;
    }
    // 2 收数据
    CAN_RxHeaderTypeDef pHeader;
    HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &pHeader, buff);

    // 打印
    printf("can_id: %x, can_len: %d  data: ", pHeader.StdId, pHeader.DLC);
    for (size_t i = 0; i < pHeader.DLC; i++)
    {
        printf("%02x ", buff[i]);
    }
    printf("\r\n");
    *can_id = pHeader.StdId;
    return pHeader.DLC;
}

/**
 * @brief 发送数据
 */
void Int_Can_Send(uint16_t send_can_id, uint8_t *data, uint16_t data_len)
{

    printf("can try send \r\n");
    // 1  检查邮箱是否满
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0)
    {
        vTaskDelay(1);
    }
    CAN_TxHeaderTypeDef pHeader;
    pHeader.IDE = CAN_ID_STD;   // 标准帧
    pHeader.RTR = CAN_RTR_DATA; // 数据帧
    pHeader.StdId = send_can_id;
    pHeader.DLC = data_len;

    uint32_t tx_mailbox;
    HAL_CAN_AddTxMessage(&hcan, &pHeader, data, &tx_mailbox);

    // 打印
    printf("can_id: %x, can_len: %d  data: ", pHeader.StdId, pHeader.DLC);
    for (size_t i = 0; i < pHeader.DLC; i++)
    {
        printf("%02x ", data[i]);
    }
    printf("\r\n");
}
