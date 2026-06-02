#include "init.h"

extern uint16_t adc_buffer[4];
extern uint8_t screen_rx_buffer[SCREEN_RX_BUFFER_SIZE];

/**
 * @brief 系统初始化函数
 *
 * 该函数在主程序初始化阶段调用一次，负责启动并配置在运行时需要持续工作的外设：
 * - 启动四路编码器接口定时器（使能编码器计数）
 * - 启动 TIM5 基本定时器并使能中断（用于周期性任务）
 * - 启动 ADC1 的 DMA 连续转换，将采样结果写入 adc_buffer
 * - 启动 USART2 的 Receive-to-Idle DMA，以非阻塞方式接收触摸屏上报数据
 */
void Init(void)
{
	HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
	HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
	HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
	HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    HAL_TIM_Base_Start_IT(&htim5);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, 5);
	HAL_UARTEx_ReceiveToIdle_DMA(&huart2, screen_rx_buffer, sizeof(screen_rx_buffer));
}