#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "main.h"
#include "mavlink.h"
#include "encoder.h"
#include "tim.h"

/* MAVLink 遥控器状态打包并 DMA 发送（替代原 Pack_And_Send_E28）
   act_val 为指针：动作1~3按下态不清零保持持续上报，松开/单次动作自动清零 */
uint8_t MAVLink_Pack_And_Send(uint8_t mode_val,
                              int16_t encoder_val[4],
                              int16_t move_val[4],
                              uint8_t *act_val,
                              uint8_t kfs_val[4][3],
                              uint8_t kfs_put_val[2][3]);

#endif
