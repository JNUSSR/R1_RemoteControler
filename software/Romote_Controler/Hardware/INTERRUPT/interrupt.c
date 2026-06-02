#include "interrupt.h"

/* 计时与输入状态全局变量说明： */
/* counter_500ms: 用于累计 TIM5 中断次数，用于每 ~500ms 切换运行指示 LED */
uint8_t counter_500ms = 0;
/* mode: 存放 4 路拨码/模式开关的 GPIO 电平状态（0/1） */
uint8_t mode[4] = {0};

extern int16_t encoder[4]; /* 四路编码器累积位置（来自 encoder 模块） */
extern int16_t move[4];    /* 四路摇杆/移动量（来自 adc_get 模块） */
extern uint8_t act;        /* 屏幕上报的动作字节（来自 screen 模块） */
extern uint8_t kfs[4][3];  /* 屏幕上报的按键矩阵 */
extern uint8_t kfs_put[2][3]; /* 屏幕上报的额外 2x3 数据 */

/**
 * 功能说明：TIM5 的周期中断回调，在中断上下文执行，负责周期性任务：
 *  - 指示灯闪烁计数
 *  - 调用编码器更新函数
 *  - 读取四路模式拨码并在 mode[0]==0 时打包并发送 E28 包（非阻塞）
 * 参数：
 *  - htim: 触发回调的定时器句柄（期望为 TIM5）
 * 返回值：无
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM5)
    {
		/* 程序运行指示灯 */
        counter_500ms++;
        if(counter_500ms >= 10)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2);
            counter_500ms = 0;
        }
		
		/* 编码器旋转位置获取 */
		Get_Encoder();

        /* 模式选择 */
        mode[0] = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);
        mode[1] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9);
        mode[2] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13);
        mode[3] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
		if(mode[0] == 0) 
		{
			//摇杆，编码器，动作指令，KFS状态发送
			if(Pack_And_Send_E28(encoder, move, act, kfs, kfs_put))
			{
				act = 0X00;
				memset(kfs, 0, sizeof(kfs));
				memset(kfs_put, 0, sizeof(kfs_put));
			}
		}
        if(mode[1] == 0)
		{
			
		}
        if(mode[2] == 0)
		{

		}
        if(mode[3] == 0)
		{
			
		}
    }
}
