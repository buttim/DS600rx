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

#define PACKET_LENGTH 32//10
#define T2 (0xFFFF-16000)

__xdata uint8_t buf[PACKET_LENGTH], 
  oldBuf[PACKET_LENGTH];
__code __at(0x3600) uint8_t channels[] = {34, 74, 4, 44, 29, 69, 9, 49};
__xdata uint16_t pwm[6];
__xdata int nChannel = 0;
__xdata volatile unsigned long millis=0;

void tim2() __interrupt 5 __using 1 {
  millis++;
  clr_TR2;
  TH2=HIBYTE(T2);
  TL2=LOBYTE(T2);
  TF2=0;
  set_TR2;
}

void initTimer2() {
  clr_T2DIV2; // Timer2 Clock = Fsys/512
  clr_T2DIV1;
  clr_T2DIV0;
  set_ET2;
  TH2=HIBYTE(T2);
  TL2=LOBYTE(T2);
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

void printNum(unsigned long n,int len) {
  __xdata static char s[15];
  
  if (len>sizeof s-1) {
    puts("???");
    return;
  }
  
  s[len]=0;
  for (int i=0;i<len;i++) {
    s[len-i-1]='0'+n%10;
    n/=10;
  }
  putstring(s);
}

void hop(int n) {
  LT8920StopListening();
  nChannel+=n;
  nChannel %= sizeof channels / sizeof(*channels);
  LT8920StartListening(channels[nChannel]);
  //!OLED_ShowNum(100, 0, channels[nChannel], 2, 16);
  //!OLED_ShowString(5, 6, "                  ", 16);
}

unsigned tLast=0;
int nPersi=0, nRicevuti=0;
unsigned long nCicli;
bool pairing=false;//true;

void main() {
  TIMER1_MODE0_ENABLE;
  Set_All_GPIO_Quasi_Mode;
  InitialUART0_Timer1(115200);

  puts("\x1b[2J\x1b[HVIA");
  if (pairing) puts("pairing");
  
  /*uint8_t x;
  read_data_flash((int)&channels,&x,1);
  printHex8(x);
  putchar('\n');*/

  initTimer2();
  set_EA;
  
  P04 = 0; // LT8920 reset
  Timer3_Delay100ms(1);
  P04 = 1;
  Timer3_Delay100ms(1);
  
  SPI_Initial();
  Timer3_Delay100ms(1);

  LT8920Begin(pairing);
  //!OLED_Init();
  //!OLED_Clear();

  LT8920StartListening(pairing?33:channels[0]);
  putstring("in ascolto sul canale ");
  printNum(channels[nChannel],2);
  putchar('\n');
  //!OLED_ShowNum(100,0,channels[0],2,16);
  //!OLED_ShowString(40,0,"channel",16);

  millis=0;

  while (true) {
    nCicli++;

    if (!P20) {
      printHex16(LT8920ReadRegister(R_STATUS));
      putchar(' ');
      printHex16(LT8920ReadRegister(7));
      putchar('\n');
      printNum(millis,8);
      putchar(':');
      putstring("pacchetti: ");
      printNum(nRicevuti,6);
      putstring(" pacchetti/s: ");
      if (millis>0) {
	int r=nRicevuti/(millis/1000);
	printNum(r,5);
	putstring(" cicli: ");
	printNum(nCicli,12);
	putstring(" cicli/s: ");
	printNum(nCicli/(millis/1000),12);
      }
      putchar('\n');
      nPersi=nRicevuti=0;
      memset(oldBuf, 0, sizeof oldBuf);
      ////!OLED_Clear();

      while (!P20)
        ;
      Timer3_Delay100ms(2);
    }
    
    if (!pairing) {
      clr_EA;
      //TODO: longer timeout when resyncing
      if (millis-tLast>44) {	//timeout waiting for packet
	tLast=millis;
	set_EA;
	hop(3);
	if (nRicevuti>0) {
	  printNum(millis,8);
	  putchar(':');
	  putstring("persi ");
	  printNum(++nPersi,6);
	  putstring(" ricevuti ");
	  printNum(nRicevuti,6);
	  putchar('\n');
	}
      }
      else
	set_EA;
    }

    if (!LT8920Available()) continue;
    P14=0;
    int n = LT8920Read(buf, sizeof buf);
    P14=1;
    if (n == 0) {
      putstring("stato: ");
      printHex16(LT8920ReadRegister(R_STATUS));
      putchar(' ');
      printNum(LT8920ReadRegister(R_FIFO_CONTROL)&0x3F,2);
      putchar('\n');
      LT8920StartListening(pairing?33:channels[nChannel]);
      continue;
    }
    if (n > 0) {
      if (pairing && n==20) {
	printNum(millis,10);
	putchar(':');
	for (int i = 0; i < n; i++) {
	  printHex8(buf[i]);
	  putchar(' ');
	}
	putchar('\n');
	LT8920StartListening(33);
      }
      else if (!pairing && n==9) {
        hop(1);
	tLast=millis;
	++nRicevuti;
	printNum(millis,10);
	putchar(':');
        if (memcmp(buf, oldBuf, n) != 0) {
          //putchar('\r');
          for (int i = 0; i < n; i++) {
            printHex8(buf[i]);
            putchar(' ');
          }
          putchar('\r');
          memcpy(oldBuf, buf, sizeof buf);
	  //putstring(" / ");
          // pwm[0]=(buf[0]<<4)+(buf[1]>>4);
          //pwm[1] = (buf[1] <<4) | (buf[2] );
	  //printHex16(pwm[1]);
          // pwm[2]=(buf[3]<<4)+(buf[4]>>4);
          // pwm[3]=((buf[4]&0xF0)<<8)+(buf[5]);
          // pwm[4]=(buf[6]<<4)+(buf[7]>>4);
          // pwm[5]=((buf[7]&0xF0)<<8)+(buf[8]);

          /*for (int i=0;i<6;i++) {
            printHex16(pwm[i]);
            putchar(' ');
          }
          putchar('\n');*/
        }
	putchar('\r');
	LT8920StartListening(pairing?33:channels[nChannel]);
      } else {
	  putstring("lunghezza sbagliata: ");
	  printNum(n,3);
	  putchar('\n');
	  LT8920StartListening(pairing?33:channels[nChannel]);
        //!OLED_ShowString(0, 2, "len?", 16);
        //!OLED_ShowNum(20, 2, n, 3, 16);
      }
    } else /*if (n < -2)*/ {
      putstring("\n\nerr 0x");
      printHex16(-n);
      putchar('\n');
      Timer3_Delay100ms(2);
      LT8920StopListening();
      //!OLED_ShowString(5, 6, "err", 16);
      //!OLED_ShowNum(30, 6, -n, 3, 16);
    }
  }
}