#ifndef INT_CANFD_H
#define INT_CANFD_H

typedef enum
{
    INT_CANFD_OK = 0,
    INT_CANFD_ERROR
} Int_CanFd_StatusTypeDef;

/* 只初始化当前板级 FDCAN；业务收发 API 待协议层需求明确后再增加。 */
Int_CanFd_StatusTypeDef Int_CanFd_Init(void);

#endif /* INT_CANFD_H */
