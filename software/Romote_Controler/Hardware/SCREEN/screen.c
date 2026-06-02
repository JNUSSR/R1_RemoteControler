#include "screen.h"

/* 接收缓冲区：DMA 接收缓冲区，用于 USART2 的 Receive-to-Idle 模式 */
uint8_t screen_rx_buffer[SCREEN_RX_BUFFER_SIZE];

/* 屏幕上报数据（均为 volatile，因为在中断/回调中更新） */
/* act: 屏幕上报的动作/指令字节 */
volatile uint8_t act = 0;
/* kfs: 4x3 键矩阵上报数据 */
volatile uint8_t kfs[4][3];
/* kfs_put: 2x3 额外按键/配置数据（已初始化为 0） */
volatile uint8_t kfs_put[2][3] = {0};

/**
 * 功能说明：解析接收到的触摸屏自定义帧并更新全局状态；完成后重新启动 DMA 接收。
 * 参数：
 *  - huart: 触发回调的 UART 句柄（期望为 USART2）
 *  - Size:  本次有效接收的字节数
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    { 
        for (int i = 0; i < Size; )
        {
            // ===== 解析 0xAA 包 (2字节: 1头 + 1数据) =====
            if (screen_rx_buffer[i] == 0xAA)
            {
                if (i + 1 < Size) 
                {
                    act = screen_rx_buffer[i+1];
                    memset(&screen_rx_buffer[i], 0, 2);
                    i += 2;
                }
                else break;
            }
            // ===== 解析 0xBB 包 (13字节: 1头 + 12数据) =====
            else if (screen_rx_buffer[i] == 0xBB)
            {
                if (i + 13 < Size)
                {
                    int data_ptr = i + 1;
                    for(int j = 0; j < 4; j++)
                    {
                        for(int k = 0; k < 3; k++)
                        {
                            kfs[j][k] = screen_rx_buffer[data_ptr++];
                        }
                    }
                    memset(&screen_rx_buffer[i], 0, 13);
                    i += 13; 
                }
                else break;
            }
            // ===== 解析 0xCC 包 (7字节: 1头 + 6数据) =====
            else if (screen_rx_buffer[i] == 0xCC)
            {
                if (i + 7 < Size)
                {
                    int data_ptr = i + 1;
                    for(int k = 0; k < 2; k++)
                    {
                        for(int j = 0; j < 3; j++)
                        {
                            kfs_put[k][j] = screen_rx_buffer[data_ptr++];
                        }
                    }
                    memset(&screen_rx_buffer[i], 0, 7);
                    i += 7;
                }
                else break;
            }
            // ===== 没匹配到帧头，滑动窗口向后移 1 字节 =====
            else
            {
                i++; 
            }
        }
        
        // 重新开启 DMA 接收（从缓冲区 0 位置重新开始）
        HAL_UARTEx_ReceiveToIdle_DMA(huart, screen_rx_buffer, sizeof(screen_rx_buffer));
    }
}