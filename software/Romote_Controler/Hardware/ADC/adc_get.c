/**
 * @file    adc_get.c
 * @brief   ADC(DMA) 数据处理：摇杆去中心并一阶低通滤波，电池电压换算与滤波
 */

#include "adc_get.h"

/* DMA 填充的 ADC 原始数据缓存（通道顺序必须与 ADC 配置一致） */
uint16_t adc_buffer[5];

/* 处理后供其他模块读取的值（move: 四路摇杆偏移，battery_voltage: 过滤后的电压，battery_percent: 换算后的百分比） */
int16_t move[4] = {0};
float battery_voltage = 0.0f;
uint8_t battery_percent = 0;

/**
 * 功能说明：ADC DMA 完成回调，处理摇杆与电池通道数据并更新全局变量
 * 参数：
 *  - hadc: 触发回调的 ADC 句柄（期望为 ADC1）
 * 返回值：无
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        /* 中心值与滤波系数 */
        const int16_t center = 2048;
        const float alpha = 0.9f; /* 旧值权重 */

        /* 按注释的通道顺序读取并滤波 */
        int16_t left_X_new  = (int16_t)(adc_buffer[0] - center);
        move[0]  = (int16_t)(move[0] * alpha + left_X_new * (1.0f - alpha));

        int16_t left_Y_new  = (int16_t)(adc_buffer[1] - center);
        move[1]  = (int16_t)(move[1] * alpha + left_Y_new * (1.0f - alpha));

        int16_t right_X_new = -(int16_t)(adc_buffer[2] - center);
        move[2] = (int16_t)(move[2] * alpha + right_X_new * (1.0f - alpha));

        int16_t right_Y_new = -(int16_t)(adc_buffer[3] - center - 35); /* 右摇杆 Y 轴有个小偏移 */
        move[3] = (int16_t)(move[3] * alpha + right_Y_new * (1.0f - alpha));

        float battery_voltage_new = (float)adc_buffer[4] * (3.3f / 4095.0f) * 3.0f;
        battery_voltage = battery_voltage * alpha + battery_voltage_new * (1.0f - alpha);
		battery_percent = Get_Battery_Percentage(battery_voltage);
    }
}

// 定义电压-电量结构体
typedef struct {
    float voltage;
    float percentage;
} BatteryMap_t;

// 建立查找表（按电压从大到小排列）
#define MAP_SIZE 11
const BatteryMap_t battery_table[MAP_SIZE] = {
    {8.40f, 100.0f},
    {8.10f,  90.0f},
    {7.90f,  80.0f},
    {7.74f,  70.0f},
    {7.60f,  60.0f},
    {7.50f,  50.0f},
    {7.40f,  40.0f},
    {7.30f,  30.0f},
    {7.14f,  20.0f},
    {6.80f,  10.0f},
    {6.00f,   0.0f}
};

/**
 * 功能说明：根据电池电压查表并线性插值计算剩余电量百分比
 * 参数：
 *  - voltage: 电池总电压，单位 V
 * 返回值：
 *  - uint8_t: 对应的剩余电量百分比（0-100）
 */
uint8_t Get_Battery_Percentage(float voltage)
{
    // 边界情况处理
    if (voltage >= battery_table[0].voltage) return 100.0f;
    if (voltage <= battery_table[MAP_SIZE - 1].voltage) return 0.0f;

    // 遍历表格寻找所处的区间
    for (int i = 0; i < MAP_SIZE - 1; i++) 
    {
        if (voltage >= battery_table[i + 1].voltage && voltage <= battery_table[i].voltage) 
        {
            // 线性插值计算
            float v_high = battery_table[i].voltage;
            float v_low  = battery_table[i + 1].voltage;
            float p_high = battery_table[i].percentage;
            float p_low  = battery_table[i + 1].percentage;

            // 插值公式：P = P_low + (V - V_low) * (P_high - P_low) / (V_high - V_low)
            uint8_t percentage = p_low + ((voltage - v_low) * (p_high - p_low) / (v_high - v_low));
            return percentage;
        }
    }
    return 0; 
}