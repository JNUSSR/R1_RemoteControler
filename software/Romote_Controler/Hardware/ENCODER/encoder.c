#include "encoder.h"

/* encoder: 存储四路编码器的累积位置（带符号，范围受限于 ±200） */
int16_t encoder[4] = {0};
/* count_last: 记录每路编码器上次读取的硬件计数（uint16_t，直接来自 TIM CNT 寄存器） */
uint16_t count_last[4] = {0};

/**
 * 功能说明：读取编码器相对于上次的脉冲增量，自动处理 16-bit 回绕
 * 参数：
 *  - htim: 指向对应编码器计数器的 TIM_HandleTypeDef
 *  - last_count: 指向保存上次计数的变量，函数会更新该变量
 * 返回值：
 *  - int16_t: 与上次读取相比的计数增量（正/负表示旋转方向）
 */
int16_t Get_Encoder_Delta(TIM_HandleTypeDef *htim, uint16_t *last_count)
{
    uint16_t curr_count = __HAL_TIM_GET_COUNTER(htim);
    int16_t delta = (int16_t)(curr_count - *last_count);
    *last_count = curr_count;
    return delta;
}

/**
 * 功能说明：更新全局 encoder[] 数组的值，应被周期性调用（例如在 TIM 回调中）
 * 参数：无
 * 返回值：无
 */
void Get_Encoder(void)
{
	/* 编码器增量计算与累加 */
	int16_t delta1 = Get_Encoder_Delta(&htim4, &count_last[0]);
	if (delta1 != 0) 
	{
		encoder[0] -= delta1;
		if(encoder[0] >=  200) encoder[0] = 200;
		if(encoder[0] <= -200) encoder[0] = -200;
	}
	int16_t delta2 = Get_Encoder_Delta(&htim1, &count_last[1]);
	if (delta2 != 0) 
	{
		encoder[1] -= delta2;
		if(encoder[1] >=  200) encoder[1] = 200;
		if(encoder[1] <= -200) encoder[1] = -200;
	}
	int16_t delta3 = Get_Encoder_Delta(&htim2, &count_last[2]);
	if (delta3 != 0) 
	{
		encoder[2] += delta3;
		if(encoder[2] >=  200) encoder[2] = 200;
		if(encoder[2] <= -200) encoder[2] = -200;
	}
	int16_t delta4 = Get_Encoder_Delta(&htim3, &count_last[3]);
	if (delta4 != 0) 
	{
		encoder[3] -= delta4;
		if(encoder[3] >=  200) encoder[3] = 200;
		if(encoder[3] <= -200) encoder[3] = -200;
	}
}