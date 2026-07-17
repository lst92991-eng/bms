#ifndef INT_CANFD_H
#define INT_CANFD_H

#include <stdint.h>

typedef enum
{
    INT_CANFD_OK = 0,
    INT_CANFD_EMPTY,
    INT_CANFD_BUSY,
    INT_CANFD_PARAM,
    INT_CANFD_HAL
} Int_CanFd_StatusTypeDef;

typedef struct
{
    uint16_t id;
    uint8_t len;
    uint8_t data[64];
} Int_CanFd_FrameTypeDef;

/* 初始化或重新启动 FDCAN1，仅将指定的 11 位标准 ID 接收到 FIFO0。 */
Int_CanFd_StatusTypeDef Int_CanFd_Init(uint16_t rx_std_id);

/* 发送标准 11 位 ID、CAN FD、无 BRS 的数据帧。 */
Int_CanFd_StatusTypeDef Int_CanFd_Send(uint16_t std_id, const uint8_t *data, uint8_t len);

/* 轮询 FIFO0；没有数据时立即返回 INT_CANFD_EMPTY。 */
Int_CanFd_StatusTypeDef Int_CanFd_Receive(Int_CanFd_FrameTypeDef *frame);

#endif /* INT_CANFD_H */
