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
#define OLED

__xdata uint8_t buf[PACKET_LENGTH], oldBuf[PACKET_LENGTH];
__code __at(0x2EE0) const uint8_t channels[] = {34, 74, 4, 44, 29, 69, 9, 49/*0,0,0,0,0,0,0,0*/};
__xdata uint16_t pwm[6],oldPwm[6];
__xdata int nChannel = 0, nConsecutiveTimeout=0;
__xdata volatile unsigned long millis = 0;
__xdata unsigned long tLast = 0, nPersi = 0, nRicevuti = 0;
__xdata bool pairing;

void initPWM() {
  uint8_t EA_SAVE,EA_SAV=0;

  PWM_IMDEPENDENT_MODE;
  PWM_CLOCK_DIV_8;
  PWMPH=0x9C;	//50Hz
  PWMPL=0x40;

  PWM5_P15_OUTPUT_ENABLE;
  PWM5H=1500>>8;
  PWM5L=1500&0xFF;

  PWM2_P05_OUTPUT_ENABLE;
  PWM2H=1500>>8;
  PWM2L=1500&0xFF;

  PWM1_P14_OUTPUT_ENABLE;
  PWM1H=1000>>8;
  PWM1L=1000&0xFF;

  set_PWMRUN;
}

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

void buf2pwm(uint8_t buf[],uint16_t pwm[]) {
  //TODO: mettere a posto
  pwm[0] = ((buf[2] & 0x07) << 8) | buf[1];
  pwm[1] = (((buf[3]<<4) | (buf[2]>>4))&0x07FF)*2;
  pwm[2] = ((buf[5] & 0x1) << 8) | buf[4];
  pwm[3] = (buf[6]&0x0F) | ((buf[5]&0xFE)<< 4);
  pwm[4] = (buf[7] << 4) | (buf[6] >> 4);
  pwm[5] = buf[8]<<4 | ((buf[0]&0xF0)<<4);
  
  //TODO: proporzioni
  PWM1H=pwm[0]>>8;
  PWM1L=pwm[0]&0xF;
  PWM5H=pwm[1]>>8;
  PWM5L=pwm[1]&0xF;
  PWM2H=pwm[2]>>8;
  PWM2L=pwm[2]&0xF;
  set_LOAD;
}

void hop(int n) {
  LT8920StopListening();
  nChannel += n;
  nChannel %= sizeof channels / sizeof(*channels);
  LT8920StartListening(channels[nChannel]);
}

void main() {
  TIMER1_MODE0_ENABLE;
  Set_All_GPIO_Quasi_Mode;
  InitialUART0_Timer1(115200);
  
  pairing=channels[0]==0 && channels[1]==0;

  puts("\x1b[2J\x1b[H\033[31;1mVIA\033[0m");

  initTimer2();

  P04 = 0; // LT8920 reset
  Timer3_Delay100ms(1);
  P04 = 1;
  Timer3_Delay100ms(1);

  SPIInit();
  Timer3_Delay100ms(1);

  LT8920Begin(pairing);
  
#ifdef OLED
  OLED_Init();
  OLED_Clear();
#endif

  if (pairing) {
    puts("pairing");
#ifdef OLED
    OLED_Clear();
    OLED_ShowString(40,0,"Pairing",16);
#endif
  }

  LT8920StartListening(pairing ? 33 : channels[0]);
  putstring("in ascolto sul canale ");
  printNum(pairing ? 33 : channels[nChannel], 2);
  putchar('\n');

  millis = 0;
  set_EA;
  
  initPWM();

  while (true) {
    if (!P20) {
      puts("\npairing");
      pairing=true;
      LT8920StopListening();
      LT8920Begin(true);
      /*printNum(millis, 8);
      putchar(':');
      putstring("pacchetti: ");
      printNum(nRicevuti, 6);
      putstring(" persi: ");
      printNum(nPersi, 6);
      putstring(" pacchetti/s: ");
      if (millis > 1000)
        printNum(nRicevuti / (millis / 1000), 5);
      putchar('\n');
      memset(oldBuf, 0, sizeof oldBuf);*/
#ifdef OLED
      OLED_Clear();
      OLED_ShowString(40,0,"Pairing",16);
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
	if (nConsecutiveTimeout == 5) {
	  OLED_Clear();
	  OLED_ShowString(60,0,"?",16);
	  memset(oldBuf,0,sizeof oldBuf);
	  nConsecutiveTimeout++;
	  puts("\nperso collegamento");
	}
	else if (nConsecutiveTimeout<5)
	  nConsecutiveTimeout++;
	continue;
      } else
        set_EA;
    }

    uint16_t status=LT8920ReadRegister(R_STATUS);
    if ((status&_BV(STATUS_PKT_FLAG_BIT))!=0/* || (status&_BV(STATUS_SYNCWORD_RECV_BIT))!=0*/)
      continue;
    if (0!=(status & _BV(STATUS_CRC_BIT))) {
      hop(1);
      continue;
    }

    int n = LT8920Read(buf, sizeof buf);

    if (n > 0) {
      if (pairing && n == 20) {
        printNum(millis, 10);
        putchar(':');
        for (int i = 0; i < n; i++) {
          printHex8(buf[i]);
          putchar(' ');
        }
        putchar('\n');
	write_data_flash((int)channels,buf+4,8);
	pairing=false;
	putstring("in ascolto sul canale ");
	printNum(channels[nChannel], 2);
	putchar('\n');
	clr_EA;
	millis=0;
	set_EA;
	LT8920Begin(false);
        LT8920StartListening(channels[0]);
#ifdef OLED
	OLED_Clear();
#endif
      } else if (!pairing && n == 9) {
	nConsecutiveTimeout=0;
        tLast = millis;
        ++nRicevuti;
        if (memcmp(buf, oldBuf, n) != 0) {
	  buf2pwm(buf,pwm);
	  int r=millis<1000?0:nRicevuti/(millis/1000);
	  OLED_ShowString(60,0," ",16);
	  OLED_ShowNum(0,0,pwm[0],4,16);
	  OLED_ShowNum(0,2,pwm[1],4,16);
	  OLED_ShowNum(95,0,pwm[2],4,16);
	  OLED_ShowNum(95,2,pwm[3],4,16);
	  OLED_ShowNum(95,4,pwm[4],4,16);
	  OLED_ShowNum(95,6,pwm[5],4,16);
	  OLED_ShowString(0,6,"pkt/s",16);
	  OLED_ShowNum(45,6,r,3,16);
	  memcpy(oldPwm,pwm,sizeof pwm);
          memcpy(oldBuf, buf, n);

	  for (int i = 0; i < n; i++) {
	    printHex8(buf[i]);
	    putchar(i==n-1?'\n':' ');
	  }

	  //~ for (int i = 0; i < 6; i++) {
	    //~ printNum(pwm[i], 4);
	    //~ putchar(' ');
	  //~ }
	  //~ putchar('\n');
        }
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