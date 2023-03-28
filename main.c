//! make
//! nuvoflash -q -w APROM "build\%name%.bin"
#include "N76E003.h"
#include "common.h"
#include "delay.h"
#include "iap.h"
#include "oled.h"
#include "radio.h"
#include "spi.h"
#include <string.h>

#define PACKET_LENGTH 32 // 10
#define T2 (0xFFFF - 16000)

__xdata uint8_t buf[PACKET_LENGTH], oldBuf[PACKET_LENGTH];
__code __at(0x3600) const uint8_t channels[] = {34, 74, 4, 44, 29, 69, 9, 49};
__xdata uint16_t pwm[6];
__xdata int nChannel = 0;
__xdata volatile unsigned long millis = 0;
__xdata unsigned long tLast = 0, nPersi = 0, nRicevuti = 0;
bool pairing = false;
__xdata uint8_t defaultData[]={0x14,0x42,0x84,0x20,0x96,0xB0,0x84,0x25,0x2C};

void tim2() __interrupt 5 __using 1 {
  millis++;
  clr_TR2;
  TH2 = HIBYTE(T2);
  TL2 = LOBYTE(T2);
  TF2 = 0;
  set_TR2;
}

void initTimer2() {
  clr_T2DIV2; // Timer2 Clock = Fsys/512
  clr_T2DIV1;
  clr_T2DIV0;
  set_ET2;
  TH2 = HIBYTE(T2);
  TL2 = LOBYTE(T2);
  set_TR2; // Start Timer2
}

int putchar(int c) {
  Send_Data_To_UART0(c);
  return c;
}

void putstring(const char *p) {
  while (*p)
    putchar(*p++);
}

unsigned char _sdcc_external_startup(void) {
  // disables Power On Reset
  /* clang-format off */
  __asm  
    mov	0xC7, #0xAA  
    mov	0xC7, #0x55  
    mov	0xFD, #0x5A  
    mov	0xC7, #0xAA  
    mov	0xC7, #0x55  
    mov	0xFD, #0xA5  
  __endasm;
  /* clang-format on */
  int EA_SAVE;
  clr_WDTEN;
  return 0;
}

char nibbleToHex(uint8_t n) {
  n &= 0xF;
  return (n < 10 ? '0' : 'A' - 10) + n;
}

void printHex16(uint16_t n) {
  putchar(nibbleToHex(n >> 12));
  putchar(nibbleToHex(n >> 8));
  putchar(nibbleToHex(n >> 4));
  putchar(nibbleToHex(n));
}

void printHex8(uint8_t n) {
  putchar(nibbleToHex(n >> 4));
  putchar(nibbleToHex(n));
}

void printNum(unsigned long n, int len) {
  __xdata static char s[15];

  if (len > sizeof s - 1) {
    puts("???");
    return;
  }

  s[len] = 0;
  for (int i = 0; i < len; i++) {
    s[len - i - 1] = '0' + n % 10;
    n /= 10;
  }
  putstring(s);
}

void hop(int n) {
  LT8920StopListening();
  nChannel += n;
  nChannel %= sizeof channels / sizeof(*channels);
  LT8920StartListening(channels[nChannel]);
  Timer3_Delay10us(1);
#ifdef OLED
  OLED_ShowNum(100, 0, channels[nChannel], 2, 16);
  OLED_ShowString(5, 6, "                  ", 16);
#endif
}

void main() {
  TIMER1_MODE0_ENABLE;
  Set_All_GPIO_Quasi_Mode;
  InitialUART0_Timer1(115200);

  puts("\x1b[2J\x1b[HVIA");
  if (pairing)
    puts("pairing");

  /*uint8_t x;
  read_data_flash((int)&channels,&x,1);
  printHex8(x);
  putchar('\n');*/

  initTimer2();

  P04 = 0; // LT8920 reset
  Timer3_Delay100ms(1);
  P04 = 1;
  Timer3_Delay100ms(1);

  SPIInit();
  Timer3_Delay100ms(1);

  LT8920Begin(pairing);
  
  //! OLED_Init();
  //! OLED_Clear();

  LT8920StartListening(pairing ? 33 : channels[0]);
  putstring("in ascolto sul canale ");
  printNum(channels[nChannel], 2);
  putchar('\n');
#ifdef OLED
  OLED_ShowNum(100, 0, channels[0], 2, 16);
  OLED_ShowString(40, 0, "channel", 16);
#endif
  millis = 0;
  set_EA;

  __xdata int nEqual=0;
  P14=0;
  while (true) {
    if (!P20) {
      printNum(millis, 8);
      putchar(':');
      putstring("pacchetti: ");
      printNum(nRicevuti, 6);
      putstring(" persi: ");
      printNum(nPersi, 6);
      putstring(" pacchetti/s: ");
      if (millis > 1000)
        printNum(nRicevuti / (millis / 1000), 5);
      putchar('\n');
      memset(oldBuf, 0, sizeof oldBuf);
#ifdef OLED
      OLED_Clear();
#endif
      while (!P20)
        ;
      Timer3_Delay100ms(2);
    }

    if (!pairing) {
      clr_EA;
      if (millis - tLast > 42) { // timeout waiting for packet
        tLast = millis;
        set_EA;
        hop(1);
	++nPersi;
        /*if (nRicevuti > 0) {
          printNum(millis, 8);
          putchar(':');
          putstring("persi ");
          printNum(nPersi, 6);
          putstring(" ricevuti ");
          printNum(nRicevuti, 6);
          putchar('\r');
        }*/
	continue;
      } else
        set_EA;
    }

    uint16_t status=LT8920ReadRegister(R_STATUS);
    if ((status&_BV(STATUS_PKT_FLAG_BIT))!=0 || (status&_BV(STATUS_SYNCWORD_RECV_BIT))!=0)
      continue;
    if (0!=(status & _BV(STATUS_CRC_BIT))) {
      hop(1);
      continue;
    }
#ifndef OLED
    //P14 = 0;
#endif
    int n = LT8920Read(buf, sizeof buf);
#ifndef OLED
    //P14 = 1;
#endif

    if (n > 0) {
      if (pairing && n == 20) {
        printNum(millis, 10);
        putchar(':');
        for (int i = 0; i < n; i++) {
          printHex8(buf[i]);
          putchar(' ');
        }
        putchar('\n');
        LT8920StartListening(33);
      } else if (!pairing && n == 9) {
        tLast = millis;
        ++nRicevuti;
        //~ printNum(millis,10);
        if (memcmp(buf, oldBuf, n) != 0) {
	  if (nEqual==0 || memcmp(oldBuf,defaultData,n)==0)
	    putchar(' ');
	  else
	    putchar('*');
	  printNum(nEqual+1,8);
	  puts("");
	  nEqual=0;
	  
          pwm[0] = ((buf[2] & 0x0F) << 8) | buf[1];
	  pwm[1] = (buf[3]<<4) | (buf[2]>>4);
          pwm[2] = ((buf[5] & 0x0F) << 8) | buf[4];
          pwm[3] = ((buf[6] & 0x0F) << 8) | buf[5];
          pwm[4] = (buf[7] << 4) | (buf[6] >> 4);
	  pwm[5] = ((buf[8] & 0xF0)<<4) | buf[0];

	  putchar('[');
	  for (int i = 0; i < n; i++) {
	    putstring("0x");
	    printHex8(buf[i]);
	    putchar(',');
	  }
	  putstring("] ");

	  //~ if (buf[0]!=0x14) {
	    //~ P14=1;
	    //~ P14=0;
	    //~ putstring("STOP");
	    //~ while (true)
	      //~ ;
	  //~ }
	  
          memcpy(oldBuf, buf, n);

	  /*for (int i = 0; i < 6; i++) {
	    putchar(' ');
	    printNum(pwm[i], 4);
	  }
	  puts("             ");*/
        }
	else
	  nEqual++;
	hop(1);
      } else {
        putstring("\nlunghezza sbagliata: ");
        printNum(n, 3);
        putchar('\n');
	if (pairing)
	  LT8920StartListening(33);
	else
	  hop(1);
#ifdef OLED
        OLED_ShowString(0, 2, "len?", 16);
        OLED_ShowNum(20, 2, n, 3, 16);
#endif
      }
    } else {
      putstring("err 0x");
      printHex16(-n);
      putchar('\n');
      Timer3_Delay100ms(1);
      if (pairing)
	  LT8920StartListening(33);
      else
	hop(1);
#ifdef OLED
      OLED_ShowString(5, 6, "err", 16);
      OLED_ShowNum(30, 6, -n, 3, 16);
#endif
    }
  }
}