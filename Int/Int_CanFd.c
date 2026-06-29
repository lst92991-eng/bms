#include "Int_CanFd.h"

#include <stdio.h>
#include <string.h>

#include "fdcan.h"

static bool s_canfd_started = false;

static bool Int_CanFd_LenToDlc(uint16_t len, uint32_t *dlc)
{
    switch (len)
    {
    case 0u:  *dlc = FDCAN_DLC_BYTES_0;  return true;
    case 1u:  *dlc = FDCAN_DLC_BYTES_1;  return true;
    case 2u:  *dlc = FDCAN_DLC_BYTES_2;  return true;
    case 3u:  *dlc = FDCAN_DLC_BYTES_3;  return true;
    case 4u:  *dlc = FDCAN_DLC_BYTES_4;  return true;
    case 5u:  *dlc = FDCAN_DLC_BYTES_5;  return true;
    case 6u:  *dlc = FDCAN_DLC_BYTES_6;  return true;
    case 7u:  *dlc = FDCAN_DLC_BYTES_7;  return true;
    case 8u:  *dlc = FDCAN_DLC_BYTES_8;  return true;
    case 12u: *dlc = FDCAN_DLC_BYTES_12; return true;
    case 16u: *dlc = FDCAN_DLC_BYTES_16; return true;
    case 20u: *dlc = FDCAN_DLC_BYTES_20; return true;
    case 24u: *dlc = FDCAN_DLC_BYTES_24; return true;
    case 32u: *dlc = FDCAN_DLC_BYTES_32; return true;
    case 48u: *dlc = FDCAN_DLC_BYTES_48; return true;
    case 64u: *dlc = FDCAN_DLC_BYTES_64; return true;
    default:  return false;
    }
}

static bool Int_CanFd_DlcToLen(uint32_t dlc, uint8_t *len)
{
    switch (dlc)
    {
    case FDCAN_DLC_BYTES_0:  *len = 0u;  return true;
    case FDCAN_DLC_BYTES_1:  *len = 1u;  return true;
    case FDCAN_DLC_BYTES_2:  *len = 2u;  return true;
    case FDCAN_DLC_BYTES_3:  *len = 3u;  return true;
    case FDCAN_DLC_BYTES_4:  *len = 4u;  return true;
    case FDCAN_DLC_BYTES_5:  *len = 5u;  return true;
    case FDCAN_DLC_BYTES_6:  *len = 6u;  return true;
    case FDCAN_DLC_BYTES_7:  *len = 7u;  return true;
    case FDCAN_DLC_BYTES_8:  *len = 8u;  return true;
    case FDCAN_DLC_BYTES_12: *len = 12u; return true;
    case FDCAN_DLC_BYTES_16: *len = 16u; return true;
    case FDCAN_DLC_BYTES_20: *len = 20u; return true;
    case FDCAN_DLC_BYTES_24: *len = 24u; return true;
    case FDCAN_DLC_BYTES_32: *len = 32u; return true;
    case FDCAN_DLC_BYTES_48: *len = 48u; return true;
    case FDCAN_DLC_BYTES_64: *len = 64u; return true;
    default: return false;
    }
}

void Int_CanFd_PrintFrame(uint32_t can_id, const uint8_t *data, uint16_t len)
{
    printf("can_id:%lx len:%u data:", (unsigned long)can_id, len);
    for (uint16_t i = 0u; i < len; i++)
    {
        printf(" %02x", data[i]);
    }
    printf("\r\n");
}

Int_CanFd_StatusTypeDef Int_CanFd_Init(void)
{
    FDCAN_FilterTypeDef filter = {0};

    if (hfdcan1.State == HAL_FDCAN_STATE_BUSY)
    {
        s_canfd_started = true;
        return INT_CANFD_OK;
    }

    if (hfdcan1.State != HAL_FDCAN_STATE_READY)
    {
        return INT_CANFD_ERROR;
    }

    /*
     * bring-up 阶段先接收所有标准帧，业务 ID 过滤留给 APP/协议层教学步骤。
     * 扩展帧和远程帧先拒收，避免总线上无关报文干扰调试。
     */
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0u;
    filter.FilterType = FDCAN_FILTER_RANGE;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x000u;
    filter.FilterID2 = INT_CANFD_STD_ID_MAX;

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK)
    {
        return INT_CANFD_ERROR;
    }

    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                     FDCAN_REJECT,
                                     FDCAN_REJECT,
                                     FDCAN_REJECT_REMOTE,
                                     FDCAN_REJECT_REMOTE) != HAL_OK)
    {
        return INT_CANFD_ERROR;
    }

    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
    {
        return INT_CANFD_ERROR;
    }

    s_canfd_started = true;
    return INT_CANFD_OK;
}

Int_CanFd_StatusTypeDef Int_CanFd_Send(uint32_t send_can_id, const uint8_t *data, uint16_t data_len)
{
    FDCAN_TxHeaderTypeDef header = {0};
    uint8_t tx_data[INT_CANFD_MAX_DATA_LEN] = {0};
    uint32_t dlc;

    if ((send_can_id > INT_CANFD_STD_ID_MAX) ||
        ((data == 0) && (data_len > 0u)) ||
        !Int_CanFd_LenToDlc(data_len, &dlc))
    {
        return INT_CANFD_ERROR;
    }

    if (hfdcan1.State != HAL_FDCAN_STATE_BUSY)
    {
        return INT_CANFD_ERROR;
    }

    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0u)
    {
        return INT_CANFD_TIMEOUT;
    }

    if (data_len > 0u)
    {
        memcpy(tx_data, data, data_len);
    }

    header.Identifier = send_can_id;
    header.IdType = FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = dlc;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_OFF;
    header.FDFormat = (data_len > 8u) ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    header.MessageMarker = 0u;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &header, tx_data) != HAL_OK)
    {
        return INT_CANFD_ERROR;
    }

    Int_CanFd_PrintFrame(send_can_id, tx_data, data_len);
    return INT_CANFD_OK;
}

uint16_t Int_CanFd_Recv(uint32_t *can_id, uint8_t *buff, uint16_t buff_len)
{
    FDCAN_RxHeaderTypeDef header = {0};
    uint8_t rx_data[INT_CANFD_MAX_DATA_LEN] = {0};
    uint8_t len;

    if ((can_id == 0) || (buff == 0) || (hfdcan1.State != HAL_FDCAN_STATE_BUSY))
    {
        return 0u;
    }

    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) == 0u)
    {
        return 0u;
    }

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &header, rx_data) != HAL_OK)
    {
        return 0u;
    }

    if (!Int_CanFd_DlcToLen(header.DataLength, &len) || (buff_len < len))
    {
        return 0u;
    }

    *can_id = header.Identifier;
    memcpy(buff, rx_data, len);
    Int_CanFd_PrintFrame(*can_id, buff, len);
    return len;
}

bool Int_CanFd_IsStarted(void)
{
    return s_canfd_started && (hfdcan1.State == HAL_FDCAN_STATE_BUSY);
}
