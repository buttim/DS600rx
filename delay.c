//@main.c

#include "N76E003.h"
#include "delay.h"

void Timer3_Delay100ms(uint32_t u32CNT) {
  T3CON = 0x07; // Timer3 Clock = Fsys/128
  set_TR3;      // Trigger Timer3
  while (u32CNT != 0) {
    RL3 = LOBYTE(TIMER_DIV128_VALUE_100ms); 
    RH3 = HIBYTE(TIMER_DIV128_VALUE_100ms);
    while ((T3CON & SET_BIT4) != SET_BIT4)
      ; // Check Timer3 Time-Out Flag
    clr_TF3;
    u32CNT--;
  }
  clr_TR3; // Stop Timer3
}

void Timer3_Delay10us(uint32_t u32CNT) {
  T3CON = 0x07; // Timer3 Clock = Fsys/128
  set_TR3;      // Trigger Timer3
  while (u32CNT != 0) {
    RL3 = LOBYTE(TIMER_DIV4_VALUE_10us); 
    RH3 = HIBYTE(TIMER_DIV4_VALUE_10us);
    while ((T3CON & SET_BIT4) != SET_BIT4)
      ; // Check Timer3 Time-Out Flag
    clr_TF3;
    u32CNT--;
  }
  clr_TR3; // Stop Timer3
}