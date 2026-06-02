#include "e28.h"

/* 简短说明：
 * 与 E28 模块的数据打包与非阻塞发送模块。
 * 所有发送通过 DMA 发起，具备硬件（AUX）与软件（DMA状态）双重防冲突保护。
 */

// 1. 强制 1 字节对齐，确保 sizeof(packet_e28) 严格等于 31 字节
#pragma pack(push, 1)
typedef struct
{
    uint8_t head;
    uint8_t encoder[4][2];   /* 每路编码器高/低字节 */
    uint8_t move[4][2];      /* 移动相关数据（高/低字节） */
    uint8_t act;             /* 动作字节 */
    uint8_t kfs[4][3];       /* 梅林矩阵 4x3 */
	uint8_t kfs_put[2][3];	 /* R2九宫格矩阵 2x3 */
    uint8_t tail;
} Packet_E28;
#pragma pack(pop)

Packet_E28 packet_e28; /* 全局临时发送包：构造后通过 DMA 发出，大小应为 31 字节 */

/**
 * 功能说明：打包并通过 DMA 将编码器、摇杆及按键矩阵等数据发送到 E28 模块。
 * 参数：
 *  - encoder_val: 4 路编码器值（int16_t[4]）
 *  - move_val:    4 路移动相关值（int16_t[4]）
 *  - act_val:     动作字节（uint8_t）
 *  - kfs_val:     梅林KFS矩阵 4x3（uint8_t[4][3]）
 *  - kfs_put_val: R2九宫格KFS矩阵 2x3（uint8_t[2][3]）
 * 返回值：
 *  - 1: 成功发起发送
 *  - 0: 未发送（例如 E28 忙或本地 UART 正在发送）
 */
uint8_t Pack_And_Send_E28(int16_t encoder_val[4], int16_t move_val[4], uint8_t act_val, uint8_t kfs_val[4][3], uint8_t kfs_put_val[2][3])
{
    // 2. 双重守卫：只有当 E28 模块空闲(SET) 且 STM32 本地串口 DMA 发送完成(READY) 时才允许打包
    if ((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == GPIO_PIN_SET) && 
        (huart6.gState == HAL_UART_STATE_READY))
    {
        packet_e28.head = 0xAA;

        for (uint8_t i = 0; i < 4; i++)
        {
            packet_e28.encoder[i][0] = (uint8_t)((uint16_t)encoder_val[i] >> 8);
            packet_e28.encoder[i][1] = (uint8_t)((uint16_t)encoder_val[i] & 0xFF);
        }

        for (uint8_t i = 0; i < 4; i++)
        {
            packet_e28.move[i][0] = (uint8_t)((uint16_t)move_val[i] >> 8);
            packet_e28.move[i][1] = (uint8_t)((uint16_t)move_val[i] & 0xFF);
        }

        packet_e28.act = act_val;

        for (uint8_t i = 0; i < 4; i++)
        {
            for (uint8_t j = 0; j < 3; j++)
            {
                packet_e28.kfs[i][j] = kfs_val[i][j];
            }
        }
		
		for (uint8_t i = 0; i < 2; i++)
        {
            for (uint8_t j = 0; j < 3; j++)
            {
                packet_e28.kfs_put[i][j] = kfs_put_val[i][j];
            }
        }

        packet_e28.tail = 0xBB;

        // 此时安全发送，sizeof 确保为 31 字节
        HAL_UART_Transmit_DMA(&huart6, (uint8_t *)&packet_e28, sizeof(packet_e28));
		return 1;
    }
	else
	{
		return 0;
	}
}