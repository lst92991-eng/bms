#include "Int_BQ769.h"

struct _Register_Group BQ769_RegisterGroup;

/**
 * CRC-8 校验算法实现
 * 多项式: x^8 + x^2 + x + 1 (0x07)
 * 初始值: 0x00
 *
 * @param data 输入数据指针
 * @param size 数据长度
 * @return CRC-8 校验值
 */
static uint8_t
crc8_calculate(uint8_t *data, uint16_t size)
{
    uint8_t crc = 0;           // 初始值为0
    uint8_t polynomial = 0x07; // 多项式 x^8 + x^2 + x + 1 对应的值

    for (uint16_t i = 0; i < size; i++)
    {
        crc ^= data[i]; // 将当前数据字节与CRC寄存器异或

        for (int j = 0; j < 8; j++)
        {
            // 检查最高位是否为1
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ polynomial;
            }
            else
            {
                crc <<= 1;
            }
        }

        crc &= 0xFF; // 确保只有8位有效
    }

    return crc;
}

/**
 * @brief 唤醒
 */
void Int_BQ769_WakeUp(void)
{
    HAL_GPIO_WritePin(BQ769_WAKEUP_GPIO_Port, BQ769_WAKEUP_Pin, GPIO_PIN_SET);
    vTaskDelay(10); // 拉高5ms
    HAL_GPIO_WritePin(BQ769_WAKEUP_GPIO_Port, BQ769_WAKEUP_Pin, GPIO_PIN_RESET);

    vTaskDelay(3000); // 芯片启动延迟
}

/**
 * @brief 休眠
 */
void Int_BQ769_Ship(void)
{
    //   Int_BQ769_WriteReg(0x04, 0x01);
    //   Int_BQ769_WriteReg(0x04, 0x02);
    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.SHUT_A = 0;
    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.SHUT_B = 0;
    Int_BQ769_WriteReg(BQ_SYS_CTRL1, BQ769_RegisterGroup.SysCtrl1.SysCtrl1Byte);
    vTaskDelay(1000);

    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.SHUT_A = 0;
    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.SHUT_B = 1;
    Int_BQ769_WriteReg(BQ_SYS_CTRL1, BQ769_RegisterGroup.SysCtrl1.SysCtrl1Byte);

    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.SHUT_A = 1;
    BQ769_RegisterGroup.SysCtrl1.SysCtrl1Bit.SHUT_B = 0;
    Int_BQ769_WriteReg(BQ_SYS_CTRL1, BQ769_RegisterGroup.SysCtrl1.SysCtrl1Byte);
    vTaskDelay(1000); // 给睡眠时间
}

/**
 * @brief 重置
 */
void Int_BQ769_Reset(void)
{
    Int_BQ769_Ship();

    Int_BQ769_WakeUp();
}

/**
 * @brief 向BQ769的寄存器写入数据（单个字节)
 */
void Int_BQ769_WriteReg(uint8_t reg, uint8_t data)
{
    uint8_t crc_buff[3] = {BQ769_I2C_ADDRESS_WRITE, reg, data};
    uint8_t crc8 = crc8_calculate(crc_buff, 3);
    uint8_t write_data[2] = {data, crc8};
    taskENTER_CRITICAL();
    HAL_I2C_Mem_Write(&hi2c2, BQ769_I2C_ADDRESS_WRITE, reg, I2C_MEMADD_SIZE_8BIT, write_data, 2, 1000);
    taskEXIT_CRITICAL();
}

/**
 * @brief 从BQ769的寄存器读取多个字节
 */
void Int_BQ769_ReadReg(uint8_t reg, uint8_t *buff, uint16_t read_len)
{
    uint8_t recv_buff[read_len * 2];
    taskENTER_CRITICAL();
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c2, BQ769_I2C_ADDRESS_READ, reg, I2C_MEMADD_SIZE_8BIT, recv_buff, read_len * 2, 1000);
    taskEXIT_CRITICAL();
    if (status != HAL_OK)
    {
        printf("I2C Read Error:%d\n", status);
    }

    for (size_t i = 0; i < read_len; i++)
    {
        if (i == 0)
        {
            uint8_t crc_buff[2] = {BQ769_I2C_ADDRESS_READ,
                                   recv_buff[0]};

            uint8_t crc8 = crc8_calculate(crc_buff, 2);
            if (crc8 != recv_buff[1])
            {
                printf("CRC Error index:%d\n", i);
                return;
            }
        }
        else
        {
            uint8_t crc_buff[1] = {recv_buff[i * 2]};

            uint8_t crc8 = crc8_calculate(crc_buff, 1); // 此处修改长度为1
            if (crc8 != recv_buff[i * 2 + 1])
            {
                printf("CRC Error index:%d\n", i);
                return;
            }
        }
        buff[i] = recv_buff[i * 2];
    }
    vTaskDelay(2);
}
