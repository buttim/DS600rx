//@main.c
//https://notbe.cn/2019/05/12/u014798590_90138776.html
#include "N76E003.h"
#include "oled.h"
#include "oledfont.h"

#undef SCL
#undef SDA
#define SCL P30
#define SDA P17
#define OLED_SCLK_Clr() SCL = 0
#define OLED_SCLK_Set() SCL = 1
#define OLED_SDIN_Clr() SDA = 0
#define OLED_SDIN_Set() SDA = 1

#define SYS_CLK_EN 0
#define SYS_SEL 2
#define SYS_DIV_EN 0 // 0: Fsys=Fosc, 1: Fsys = Fosc/(2*CKDIV)
#define SYS_DIV 1
#define I2C_CLOCK 128
#define TEST_OK 0x00

#define OLED_CMD 0  //写命令
#define OLED_DATA 1 //写数据
#define OLED_MODE 0
#define SIZE 16
#define XLevelL 0x02
#define XLevelH 0x10
#define Max_Column 128
#define Max_Row 64
#define Brightness 0xFF
#define X_WIDTH 128
#define Y_WIDTH 64

/**********************************************
//IIC GPIO Init
(必须配置为开漏模式，并加上拉电阻)
**********************************************/
void Init_I2C(void) {
  P17_OpenDrain_Mode;					// Modify SCL pin to Open drain mode.
  P30_OpenDrain_Mode;					// Modify SDA pin to Open drain mode.
}

/**********************************************
//IIC Start
**********************************************/
void IIC_Start() {
  OLED_SCLK_Set();
  OLED_SDIN_Set();
  OLED_SDIN_Clr();
  OLED_SCLK_Clr();
}

/**********************************************
//IIC Stop
**********************************************/
void IIC_Stop() {
  OLED_SCLK_Set();
  OLED_SDIN_Clr();
  OLED_SDIN_Set();
}

/**********************************************
//IIC Ack
**********************************************/
void IIC_Wait_Ack() {
  while (SDA) //判断是否接收到应答信号
      ;
  OLED_SCLK_Set();
  OLED_SCLK_Clr();
}
/**********************************************
// IIC Write byte
**********************************************/

void Write_IIC_Byte(uint8_t IIC_Byte) {
  uint8_t i;
  uint8_t m, da;
  da = IIC_Byte;
  OLED_SCLK_Clr();
  for (i = 0; i < 8; i++) {
    m = da;
    m &= 0x80;
    if (m == 0x80)
      OLED_SDIN_Set();
    else
      OLED_SDIN_Clr();
    da <<= 1;
    OLED_SCLK_Set();
    OLED_SCLK_Clr();
  }
}
/**********************************************
// IIC Write Command
**********************************************/
void Write_IIC_Command(uint8_t IIC_Command) {
  IIC_Start();
  Write_IIC_Byte(0x78); // Slave address,SA0=0
  IIC_Wait_Ack();
  Write_IIC_Byte(0x00); // write command
  IIC_Wait_Ack();
  Write_IIC_Byte(IIC_Command);
  IIC_Wait_Ack();
  IIC_Stop();
}
/**********************************************
// IIC Write Data
**********************************************/
void Write_IIC_Data(uint8_t IIC_Data) {
  IIC_Start();
  Write_IIC_Byte(0x78); // D/C#=0; R/W#=0
  IIC_Wait_Ack();
  Write_IIC_Byte(0x40); // write data
  IIC_Wait_Ack();
  Write_IIC_Byte(IIC_Data);
  IIC_Wait_Ack();
  IIC_Stop();
}
/**********************************************
// IIC WriteReadCmd
**********************************************/
void OLED_WR_Byte(uint8_t dat, uint8_t cmd) {
  if (cmd)
    Write_IIC_Data(dat);
  else
    Write_IIC_Command(dat);
}
/********************************************
// fill_Picture
********************************************/
void fill_picture(uint8_t fill_Data) {
  uint8_t m, n;
  for (m = 0; m < 8; m++) {
    OLED_WR_Byte(0xb0 + m, 0); // page0-page1
    OLED_WR_Byte(0x00, 0);     // low column start address
    OLED_WR_Byte(0x10, 0);     // high column start address
    for (n = 0; n < 128; n++) {
      OLED_WR_Byte(fill_Data, 1);
    }
  }
}
//坐标设置

void OLED_Set_Pos(uint8_t x, uint8_t y) {
  OLED_WR_Byte(0xb0 + y, OLED_CMD);
  OLED_WR_Byte(((x & 0xf0) >> 4) | 0x10, OLED_CMD);
  OLED_WR_Byte((x & 0x0f), OLED_CMD);
}
//开启OLED显示
void OLED_Display_On(void) {
  OLED_WR_Byte(0X8D, OLED_CMD); // SET DCDC命令
  OLED_WR_Byte(0X14, OLED_CMD); // DCDC ON
  OLED_WR_Byte(0XAF, OLED_CMD); // DISPLAY ON
}
//关闭OLED显示
void OLED_Display_Off(void) {
  OLED_WR_Byte(0X8D, OLED_CMD); // SET DCDC命令
  OLED_WR_Byte(0X10, OLED_CMD); // DCDC OFF
  OLED_WR_Byte(0XAE, OLED_CMD); // DISPLAY OFF
}

/********************************************
// OLED Clear Display
********************************************/
void OLED_Clear(void) {
  uint8_t i, n;
  for (i = 0; i < 8; i++) {
    OLED_WR_Byte(0xb0 + i, OLED_CMD); //设置页地址（0~7）
    OLED_WR_Byte(0x00, OLED_CMD);     //设置显示位置—列低地址
    OLED_WR_Byte(0x10, OLED_CMD);     //设置显示位置—列高地址
    for (n = 0; n < 128; n++)
      OLED_WR_Byte(0, OLED_DATA);
  } //更新显示
}

/********************************************
//OLED Display Char
//x:0~127
//y:0~63
//size:16/12
********************************************/
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t Char_Size) {
  uint8_t c = 0, i = 0;
  c = chr - ' '; //得到偏移后的值
  if (x > Max_Column - 1) {
    x = 0;
    y = y + 2;
  }
  if (Char_Size == 16) {
    OLED_Set_Pos(x, y);
    for (i = 0; i < 8; i++)
      OLED_WR_Byte(F8X16[c * 16 + i], OLED_DATA);
    OLED_Set_Pos(x, y + 1);
    for (i = 0; i < 8; i++)
      OLED_WR_Byte(F8X16[c * 16 + i + 8], OLED_DATA);
  } else {
    OLED_Set_Pos(x, y);
    for (i = 0; i < 6; i++)
      OLED_WR_Byte(F6x8[c][i], OLED_DATA);
  }
}
/********************************************
//OLED Pow
********************************************/
uint32_t oled_pow(uint8_t m, uint8_t n) {
  uint32_t result = 1;
  while (n--)
    result *= m;
  return result;
}

/********************************************
//OLED Display Number
//x:0~127
//y:0~63
//num
//len
//size:16/12
********************************************/
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len,
                  uint8_t Num_size) {
  uint8_t t, temp;
  uint8_t enshow = 0;
  for (t = 0; t < len; t++) {
    temp = (num / oled_pow(10, len - t - 1)) % 10;
    if (enshow == 0 && t < (len - 1)) {
      if (temp == 0) {
        OLED_ShowChar(x + (Num_size / 2) * t, y, ' ', Num_size);
        continue;
      } else
        enshow = 1;
    }
    OLED_ShowChar(x + (Num_size / 2) * t, y, temp + '0', Num_size);
  }
}

/********************************************
//Show a String
********************************************/
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *chr, uint8_t Char_Size) {
  uint8_t j = 0;
  while (chr[j] != '\0') {
    OLED_ShowChar(x, y, chr[j], Char_Size);
    x += 8;
    if (x > 120) {
      x = 0;
      y += 2;
    }
    j++;
  }
}

/********************************************
//Show Img
********************************************/
void OLED_DrawBMP(uint8_t x0, uint8_t y0, uint8_t x1,
                  uint8_t y1, uint8_t BMP[]) {
  unsigned j = 0;
  uint8_t x, y;

  for (y = y0; y < y1; y++) {
    OLED_Set_Pos(x0, y);
    for (x = x0; x < x1; x++)
      OLED_WR_Byte(BMP[j++], OLED_DATA);
  }
}

/********************************************
//Init OLED
********************************************/
void OLED_Init(void) {
  Init_I2C();
  OLED_WR_Byte(0xAE, OLED_CMD); //--display off
  OLED_WR_Byte(0x00, OLED_CMD); //---set low column address
  OLED_WR_Byte(0x10, OLED_CMD); //---set high column address
  OLED_WR_Byte(0x40, OLED_CMD); //--set start line address
  OLED_WR_Byte(0xB0, OLED_CMD); //--set page address
  OLED_WR_Byte(0x81, OLED_CMD); // contract control
  OLED_WR_Byte(0xFF, OLED_CMD); //--128
  OLED_WR_Byte(0xA1, OLED_CMD); // set segment remap
  OLED_WR_Byte(0xA6, OLED_CMD); //--normal / reverse
  OLED_WR_Byte(0xA8, OLED_CMD); //--set multiplex ratio(1 to 64)
  OLED_WR_Byte(0x3F, OLED_CMD); //--1/32 duty
  OLED_WR_Byte(0xC8, OLED_CMD); // Com scan direction
  OLED_WR_Byte(0xD3, OLED_CMD); //-set display offset
  OLED_WR_Byte(0x00, OLED_CMD); //

  OLED_WR_Byte(0xD5, OLED_CMD); // set osc division
  OLED_WR_Byte(0x80, OLED_CMD); //

  OLED_WR_Byte(0xD8, OLED_CMD); // set area color mode off
  OLED_WR_Byte(0x05, OLED_CMD); //

  OLED_WR_Byte(0xD9, OLED_CMD); // Set Pre-Charge Period
  OLED_WR_Byte(0xF1, OLED_CMD); //

  OLED_WR_Byte(0xDA, OLED_CMD); // set com pin configuartion
  OLED_WR_Byte(0x12, OLED_CMD); //

  OLED_WR_Byte(0xDB, OLED_CMD); // set Vcomh
  OLED_WR_Byte(0x30, OLED_CMD); //

  OLED_WR_Byte(0x8D, OLED_CMD); // set charge pump enable
  OLED_WR_Byte(0x14, OLED_CMD); //

  OLED_WR_Byte(0xAF, OLED_CMD); //--turn on oled panel
}
