#include "Int_CanFd.h"

#include "fdcan.h"

enum
{
    INT_CANFD_STD_ID_MAX = 0x7FFu
};

Int_CanFd_StatusTypeDef Int_CanFd_Init(void)
{
    FDCAN_FilterTypeDef filter = {0};

    if (hfdcan1.State == HAL_FDCAN_STATE_BUSY)
    {
        return INT_CANFD_OK;
    }

    if (hfdcan1.State != HAL_FDCAN_STATE_READY)
    {
        return INT_CANFD_ERROR;
    }

    /* 当前只保留启动时的标准帧接收配置；项目尚未实现 CAN 业务收发。 */
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

    return (HAL_FDCAN_Start(&hfdcan1) == HAL_OK) ? INT_CANFD_OK : INT_CANFD_ERROR;
}
