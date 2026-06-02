#ifndef _E28_H_
#define _E28_H_

#include "main.h"
#include "usart.h"

uint8_t Pack_And_Send_E28(int16_t encoder_val[4], int16_t move_val[4], uint8_t act_val, uint8_t kfs_val[4][3], uint8_t kfs_put_val[2][3]);

#endif