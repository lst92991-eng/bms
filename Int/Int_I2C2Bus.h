#ifndef INT_I2C2_BUS_H
#define INT_I2C2_BUS_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 创建 I2C2 静态递归互斥量；调度器启动前调用。 */
bool Int_I2C2Bus_Init(void);

/** @brief 任务上下文获取 I2C2 总线所有权；ISR 调用会直接失败。 */
bool Int_I2C2Bus_Lock(uint32_t timeout_ms);

/** @brief 释放一次递归总线所有权。 */
void Int_I2C2Bus_Unlock(void);

#endif /* INT_I2C2_BUS_H */
