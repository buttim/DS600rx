//@main.c
#include "N76E003.h"
#include "spi.h"

void SPIInit(void) {
  P12_PushPull_Mode;
  P10_PushPull_Mode;
  P00_PushPull_Mode;
  P01_Input_Mode;

  clr_SPIEN;
  set_DISMODF; 	// SS General purpose I/O ( No Mode Fault )
  clr_SSOE;

  clr_LSBFE;
  
  bool EA_SAVE;
  set_SPIS1;
  set_SPIS0;

  clr_CPOL; 
  set_CPHA;

  set_MSTR;
  SPICLK_DIV2; 
  set_SPIEN;
  clr_SPIF;
}

uint8_t SPITransfer(uint8_t x) {
  SPDR = x;
  while (!(SPSR & SPSR_SPIF))
    ;
  clr_SPIF;
  return SPDR;
}
