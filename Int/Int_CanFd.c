#include "Int_CanFd.h"

#include <stdbool.h>
#include <string.h>

#include "fdcan.h"

enum
{
    INT_CANFD_STD_ID_MAX = 0x7FFu
};

static bool Int_CanFd_LengthToDlc(uint8_t len, uint32_t *dlc)
{
    if (dlc == NULL)
    {
        return false;
    }

    switch (len)
    {
        case 0u:  *dlc = FDCAN_DLC_BYTES_0;  break;
        case 1u:  *dlc = FDCAN_DLC_BYTES_1;  break;
        case 2u:  *dlc = FDCAN_DLC_BYTES_2;  break;
        case 3u:  *dlc = FDCAN_DLC_BYTES_3;  break;
        case 4u:  *dlc = FDCAN_DLC_BYTES_4;  break;
        case 5u:  *dlc = FDCAN_DLC_BYTES_5;  break;
        case 6u:  *dlc = FDCAN_DLC_BYTES_6;  break;
        case 7u:  *dlc = FDCAN_DLC_BYTES_7;  break;
        case 8u:  *dlc = FDCAN_DLC_BYTES_8;  break;
        case 12u: *dlc = FDCAN_DLC_BYTES_12; break;
        case 16u: *dlc = FDCAN_DLC_BYTES_16; break;
        case 20u: *dlc = FDCAN_DLC_BYTES_20; break;
        case 24u: *dlc = FDCAN_DLC_BYTES_24; break;
        case 32u: *dlc = FDCAN_DLC_BYTES_32; break;
        case 48u: *dlc = FDCAN_DLC_BYTES_48; break;
        case 64u: *dlc = FDCAN_DLC_BYTES_64; break;
        default:  return false;
    }

    return true;
}

static bool Int_CanFd_DlcToLength(uint32_t dlc, uint8_t *len)
{
    if (len == NULL)
    {
        return false;
    }

    switch (dlc)
    {
        case FDCAN_DLC_BYTES_0:  *len = 0u;  break;
        case FDCAN_DLC_BYTES_1:  *len = 1u;  break;
        case FDCAN_DLC_BYTES_2:  *len = 2u;  break;
        case FDCAN_DLC_BYTES_3:  *len = 3u;  break;
        case FDCAN_DLC_BYTES_4:  *len = 4u;  break;
        case FDCAN_DLC_BYTES_5:  *len = 5u;  break;
        case FDCAN_DLC_BYTES_6:  *len = 6u;  break;
        case FDCAN_DLC_BYTES_7:  *len = 7u;  break;
        case FDCAN_DLC_BYTES_8:  *len = 8u;  break;
        case FDCAN_DLC_BYTES_12: *len = 12u; break;
        case FDCAN_DLC_BYTES_16: *len = 16u; break;
        case FDCAN_DLC_BYTES_20: *len = 20u; break;
        case FDCAN_DLC_BYTES_24: *len = 24u; break;
        case FDCAN_DLC_BYTES_32: *len = 32u; break;
        case FDCAN_DLC_BYTES_48: *len = 48u; break;
        case FDCAN_DLC_BYTES_64: *len = 64u; break;
        default:                 return false;
    }

    return true;
}

static bool Int_CanFd_ProtocolReady(void)
{
    FDCAN_ProtocolStatusTypeDef protocol_status = {0};

    return (HAL_FDCAN_GetProtocolStatus(&hfdcan1, &protocol_status) == HAL_OK) &&
           (protocol_status.BusOff == 0u);
}

Int_CanFd_StatusTypeDef Int_CanFd_Init(uint16_t rx_std_id)
{
    FDCAN_FilterTypeDef filter = {0};

    if (rx_std_id > INT_CANFD_STD_ID_MAX)
    {
        return INT_CANFD_PARAM;
    }

    if (hfdcan1.State == HAL_FDCAN_STATE_BUSY)
    {
        /*
         * 运行期重连必须真正让控制器重新进入 INIT，再 Start 退出 bus-off；
         * 仅依据 HAL 的 BUSY 状态不能证明协议引擎仍可收发。
         */
        (void)HAL_FDCAN_AbortTxRequest(&hfdcan1,
                                       FDCAN_TX_BUFFER0 |
                                       FDCAN_TX_BUFFER1 |
                                       FDCAN_TX_BUFFER2);
        (void)HAL_FDCAN_Stop(&hfdcan1);
    }

    if (hfdcan1.State != HAL_FDCAN_STATE_READY)
    {
        /*
         * Stop 超时会把 HAL 句柄置为 ERROR。DeInit/Init 用于打破该吸收态，
         * 否则 APP 的周期重试永远只能再次得到 HAL 错误。
         */
        if ((HAL_FDCAN_DeInit(&hfdcan1) != HAL_OK) ||
            (HAL_FDCAN_Init(&hfdcan1) != HAL_OK))
        {
            return INT_CANFD_HAL;
        }
    }

    if (hfdcan1.State != HAL_FDCAN_STATE_READY)
    {
        return INT_CANFD_HAL;
    }

    /* 精确范围只接收 APP 指定的查询 ID，远程帧由全局过滤器拒绝。 */
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0u;
    filter.FilterType = FDCAN_FILTER_RANGE;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = rx_std_id;
    filter.FilterID2 = rx_std_id;

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK)
    {
        return INT_CANFD_HAL;
    }

    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                     FDCAN_REJECT,
                                     FDCAN_REJECT,
                                     FDCAN_REJECT_REMOTE,
                                     FDCAN_REJECT_REMOTE) != HAL_OK)
    {
        return INT_CANFD_HAL;
    }

    return (HAL_FDCAN_Start(&hfdcan1) == HAL_OK) ? INT_CANFD_OK : INT_CANFD_HAL;
}

Int_CanFd_StatusTypeDef Int_CanFd_Send(uint16_t std_id, const uint8_t *data, uint8_t len)
{
    FDCAN_TxHeaderTypeDef header = {0};
    uint32_t dlc;

    if ((std_id > INT_CANFD_STD_ID_MAX) ||
        ((len > 0u) && (data == NULL)) ||
        !Int_CanFd_LengthToDlc(len, &dlc))
    {
        return INT_CANFD_PARAM;
    }

    if (hfdcan1.State != HAL_FDCAN_STATE_BUSY)
    {
        return INT_CANFD_HAL;
    }
    if (!Int_CanFd_ProtocolReady())
    {
        return INT_CANFD_HAL;
    }

    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0u)
    {
        return INT_CANFD_BUSY;
    }

    header.Identifier = std_id;
    header.IdType = FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = dlc;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_ON;
    header.FDFormat = FDCAN_FD_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    header.MessageMarker = 0u;

    return (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &header, data) == HAL_OK)
               ? INT_CANFD_OK
               : INT_CANFD_HAL;
}

Int_CanFd_StatusTypeDef Int_CanFd_Receive(Int_CanFd_FrameTypeDef *frame)
{
    FDCAN_RxHeaderTypeDef header = {0};
    uint8_t data[64];
    uint8_t len;

    if (frame == NULL)
    {
        return INT_CANFD_PARAM;
    }

    if (hfdcan1.State != HAL_FDCAN_STATE_BUSY)
    {
        return INT_CANFD_HAL;
    }
    if (!Int_CanFd_ProtocolReady())
    {
        return INT_CANFD_HAL;
    }

    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) == 0u)
    {
        return INT_CANFD_EMPTY;
    }

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &header, data) != HAL_OK)
    {
        return INT_CANFD_HAL;
    }

    if ((header.IdType != FDCAN_STANDARD_ID) ||
        (header.RxFrameType != FDCAN_DATA_FRAME) ||
        (header.FDFormat != FDCAN_FD_CAN) ||
        (header.BitRateSwitch != FDCAN_BRS_ON) ||
        (header.Identifier > INT_CANFD_STD_ID_MAX) ||
        !Int_CanFd_DlcToLength(header.DataLength, &len))
    {
        return INT_CANFD_PARAM;
    }

    frame->id = (uint16_t)header.Identifier;
    frame->len = len;
    if (len > 0u)
    {
        (void)memcpy(frame->data, data, len);
    }

    return INT_CANFD_OK;
}
