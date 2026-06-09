#include "interrupt.h"
#include "usart.h"
#include <string.h>
#include "mavlink.h"

/* MAVLink 系统与组件 ID，可按需修改 */
#ifndef MAVLINK_SYSTEM_ID
#define MAVLINK_SYSTEM_ID    1
#endif
#ifndef MAVLINK_COMPONENT_ID
#define MAVLINK_COMPONENT_ID 1
#endif

/* 计时与输入状态全局变量说明： */
/* counter_500ms: 用于累计 TIM5 中断次数，用于每 ~500ms 切换运行指示 LED */
uint8_t counter_500ms = 0;
/* mode: 存放 4 路拨码/模式开关的 GPIO 电平状态（0/1） */
uint8_t mode[4] = {0};

extern int16_t encoder[4]; /* 四路编码器累积位置（来自 encoder 模块） */
extern int16_t move[4];    /* 四路摇杆/移动量（来自 adc_get 模块） */
extern uint8_t act;        /* 屏幕上报的动作字节（来自 screen 模块） */
extern uint8_t kfs[4][3];  /* 屏幕上报的按键矩阵（每元素 0~3） */
extern uint8_t kfs_put[2][3]; /* 屏幕上报的 R2 九宫格数据（每元素 0~1） */

uint8_t buf[MAVLINK_MAX_PACKET_LEN];
/* msg 必须放在函数体外，sizeof(mavlink_message_t) ≈ 295 字节，
   放在 ISR 栈上会导致栈溢出，破坏 huart2 状态等全局变量 */
mavlink_message_t msg;

/**
 * 功能说明：将遥控器数据按 MAVLink REMOTE_CONTROL_STATE 协议打包，并通过 UART6 DMA 发送。
 *          替代原有的 Pack_And_Send_E28()。
 *          双重守卫：E28 模块空闲(PB15=SET) + STM32 UART6 DMA 就绪(READY)。
 * 参数：
 *  - mode_val:     4 路模式拨码，已打包为 uint8_t（bit0~bit3 对应 mode[0]~mode[3]）
 *  - encoder_val:  4 路编码器值（int16_t[4]）
 *  - move_val:     4 路摇杆/移动量（int16_t[4]）
 *  - act_val:      动作字节（uint8_t）
 *  - kfs_val:      梅林 KFS 矩阵 4x3（uint8_t[4][3]，每元素 0~3）
 *  - kfs_put_val:  R2 九宫格矩阵 2x3（uint8_t[2][3]，每元素 0~1）
 * 返回值：
 *  - 1: 成功发起 DMA 发送
 *  - 0: E28 忙或 UART6 正忙，未发送
 */
uint8_t MAVLink_Pack_And_Send(uint8_t mode_val,
                              int16_t encoder_val[4],
                              int16_t move_val[4],
                              uint8_t *act_val,
                              uint8_t kfs_val[4][3],
                              uint8_t kfs_put_val[2][3])
{
    /* 双重守卫：与原有逻辑一致 */
    if ((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == GPIO_PIN_SET) &&
        (huart6.gState == HAL_UART_STATE_READY))
    {
        /* ---- 1. 将 kfs[4][3] 打包为 uint32_t ----
         * 屏幕上报: 0=未更新, 1=空, 2=R1, 3=R2, 4=假
         * val>0 时才减1, 避免 val=0 时 uint8_t 下溢 (0-1=255) */
        uint32_t kfs_packed = 0;
        for (uint8_t i = 0; i < 4; i++)
        {
            for (uint8_t j = 0; j < 3; j++)
            {
                uint8_t v = kfs_val[i][j];
                kfs_packed |= ((uint32_t)(v ? ((v - 1) & 0x03) : 0)) << (2 * (i * 3 + j));
            }
        }

        /* ---- 2. 将 kfs_put[2][3] 打包为 uint8_t ----
         * 屏幕上报: 0=未更新, 1=不放, 2=放
         * val>0 时才减1, 避免 val=0 时 uint8_t 下溢 */
        uint8_t kfs_put_packed = 0;
        for (uint8_t i = 0; i < 2; i++)
        {
            for (uint8_t j = 0; j < 3; j++)
            {
                uint8_t v = kfs_put_val[i][j];
                kfs_put_packed |= ((uint8_t)(v ? ((v - 1) & 0x01) : 0)) << (i * 3 + j);
            }
        }

        /* ---- 3. 处理 act 动作指令 ----
         * act 编码 0xXY: X = 动作ID(1~6), Y = 状态(1=松开, 2=按下)
         * 动作1~3: 按下/未按下双稳态，始终不清零，松开时转为 0x00
         * 动作4~6: 单次触发，发送后立即清零
         */
        uint8_t act_send = *act_val;
        uint8_t action_id  = (act_send >> 4) & 0x0F;
        uint8_t action_state = act_send & 0x0F;

        /* 动作1~3 松开事件：转为未按下态(0)，并持续上报 */
        if (action_id >= 1 && action_id <= 3 && action_state == 1)
        {
            *act_val = 0x00;
            act_send = 0x00;
        }

        /* ---- 4. MAVLink 打包 ---- */
        mavlink_msg_remote_control_state_pack(
            MAVLINK_SYSTEM_ID,
            MAVLINK_COMPONENT_ID,
            &msg,
            mode_val,
            encoder_val[0], encoder_val[1], encoder_val[2], encoder_val[3],
            move_val[0],    move_val[1],    move_val[2],    move_val[3],
            act_send,
            kfs_packed,
            kfs_put_packed);

        /* ---- 5. 转为发送缓冲区并通过 DMA 发出 ---- */
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        HAL_UART_Transmit_DMA(&huart6, buf, len);

        /* ---- 6. 发送成功后清零 act ----
         * 动作1~3: 不清零（按下/未按下双稳态保持）
         * 动作4~6: 清零（单次触发）
         */
        if (action_id >= 4 && action_id <= 6)
        {
            *act_val = 0x00;
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * 功能说明：TIM5 的周期中断回调，在中断上下文执行，负责周期性任务：
 *  - 指示灯闪烁计数
 *  - 调用编码器更新函数
 *  - 读取四路模式拨码，打包并通过 MAVLink 发送遥控器状态（非阻塞）
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

        /* 模式选择：读取 4 路拨码开关并打包为一个 uint8_t */
        mode[0] = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);
        mode[1] = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9);
        mode[2] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13);
        mode[3] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);

        uint8_t mode_packed = 0;
        mode_packed |= (mode[0] & 0x01) << 0;
        mode_packed |= (mode[1] & 0x01) << 1;
        mode_packed |= (mode[2] & 0x01) << 2;
        mode_packed |= (mode[3] & 0x01) << 3;

        /* mode[0] 拨下时发送遥控器状态 */
        if(mode[0] == 0)
        {
            /* kfs/kfs_put 保持屏幕最新值不清零；
               act 动作1~3 双稳态不清零，动作4~6 触发后自动清零 */
            MAVLink_Pack_And_Send(mode_packed, encoder, move, &act, kfs, kfs_put);
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
