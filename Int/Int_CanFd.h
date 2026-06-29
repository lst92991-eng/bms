#ifndef INT_CANFD_H
#define INT_CANFD_H

#include <stdbool.h>
#include <stdint.h>

#define INT_CANFD_MAX_DATA_LEN  64u
#define INT_CANFD_STD_ID_MAX    0x7FFu

typedef enum
{
    INT_CANFD_OK = 0,
    INT_CANFD_ERROR,
    INT_CANFD_TIMEOUT
} Int_CanFd_StatusTypeDef;

void Int_CanFd_PrintFrame(uint32_t can_id, const uint8_t *data, uint16_t len);
Int_CanFd_StatusTypeDef Int_CanFd_Init(void);
Int_CanFd_StatusTypeDef Int_CanFd_Send(uint32_t send_can_id, const uint8_t *data, uint16_t data_len);
uint16_t Int_CanFd_Recv(uint32_t *can_id, uint8_t *buff, uint16_t buff_len);
bool Int_CanFd_IsStarted(void);

#endif
