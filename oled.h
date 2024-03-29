#ifndef __OLED_H
#define __OLED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define OLED_DATA 1

void OLED_Init();
void OLED_Clear();
void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *chr,uint8_t Char_Size);
void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint8_t len,uint8_t Num_size);
void OLED_Set_Pos(uint8_t x, uint8_t y) ;
void OLED_WR_Byte(uint8_t dat, uint8_t cmd);
#endif