#ifndef __INT_CAN_H_
#define __INT_CAN_H_
#include "can.h"
#include "FreeRTOS.h"
#include "task.h"

#define can_id_charge 0x0001
#define can_id_discharge 0x0002
#define can_id_mcu_poweroff 0x0003

#define can_id_data_vol 0x0011
#define can_id_data_soc 0x0012
#define can_id_data_cur 0x0013
#define can_id_data_tempr 0x0014
/**
 * @brief can的初始化
 */
void Int_Can_Init(void);

/**
 * @brief 接受数据
 * @param buff 接收数据的容器
 * @param buff_len 容器的长度
 * @return 实际接收到的数据长度
 */
uint16_t Int_Can_Recv(uint16_t *can_id, uint8_t *buff, uint16_t buff_len);

/**
 * @brief 发送数据
 */
void Int_Can_Send(uint16_t send_can_id, uint8_t *data, uint16_t data_len);

#endif // __INT_CAN_H_
