//@main.c
#include "N76E003.h"

#include "delay.h"
#include "radio.h"

uint8_t _channel = DEFAULT_CHANNEL;

uint8_t SPITransfer(uint8_t x) {
  SPDR = x;
  Timer3_Delay10us(1);
  while (!(SPSR & 0x80))
    ;
  clr_SPIF;
  return SPDR;
}

uint16_t LT8920ReadRegister(uint8_t reg) {
  uint8_t h, l;

  SS = 0;
  Timer3_Delay10us(1);

  SPITransfer(REGISTER_READ | reg);
  h = SPITransfer(0);
  l = SPITransfer(0);

  SS = 1;
  return (h << 8) | l;
}

uint8_t LT8920WriteRegister2(uint8_t reg, uint8_t high, uint8_t low) {
  uint8_t result;

  SS = 0;
  Timer3_Delay10us(1);
  result = SPITransfer(REGISTER_WRITE | reg);
  SPITransfer(high);
  SPITransfer(low);

  SS = 1;
  return result;
}

uint8_t LT8920WriteRegister(uint8_t reg, uint16_t val) {
  return LT8920WriteRegister2(reg, val >> 8, val);
}

void LT8920Begin() {
  LT8920WriteRegister(0,0x6FE0);
  LT8920WriteRegister(2,0x6617);
  LT8920WriteRegister(4,0x9CC9);
  LT8920WriteRegister(5,0x6637);
  LT8920WriteRegister(7,0x0000);
  LT8920WriteRegister(8,0x6C90);
  LT8920WriteRegister(9,0x1840);
  LT8920WriteRegister(11,0x0008);
  LT8920WriteRegister(13,0x48BD);
  LT8920WriteRegister(22,0x00FF);
  LT8920WriteRegister(23,0x8005);
  LT8920WriteRegister(24,0x0067);
  LT8920WriteRegister(26,0x19E0);
  LT8920WriteRegister(27,0x1300);
  LT8920WriteRegister(32,0x6800);
  LT8920WriteRegister(33,0x3FC7);
  LT8920WriteRegister(34,0x2000);
  LT8920WriteRegister(35,0x0300);
  LT8920WriteRegister(40,0x4401);
  LT8920WriteRegister(41,0xB400);
  LT8920WriteRegister(42,0xFDB0);
  LT8920WriteRegister(44,0x0800);
  LT8920WriteRegister(45,0x0552);
  LT8920WriteRegister(39,0xF3AA);
  LT8920WriteRegister(36,0x180C);
  LT8920WriteRegister(52,0x8080);
}

void LT8920SetCurrentControl(uint8_t power, uint8_t gain) {
  LT8920WriteRegister(R_CURRENT,
                      ((power << CURRENT_POWER_SHIFT) & CURRENT_POWER_MASK) |
                          ((gain << CURRENT_GAIN_SHIFT) & CURRENT_GAIN_MASK));
}

void LT8920StartListening() {
  LT8920WriteRegister(R_CHANNEL, _channel & CHANNEL_MASK); // turn off rx/tx
  Timer3_Delay10us(300);
  LT8920WriteRegister(R_FIFO_CONTROL, 0x0080); // flush rx
  LT8920WriteRegister(R_CHANNEL, (_channel & CHANNEL_MASK) |
                                     (1 << CHANNEL_RX_BIT)); // enable RX
  Timer3_Delay10us(500);
}

int LT8920Read(uint8_t *buffer, size_t maxBuffer) {
  uint16_t value = LT8920ReadRegister(R_STATUS);
  uint8_t pos = 0;
  if ((value & (1 << STATUS_CRC_BIT)) == 0) {
    // CRC ok

    uint16_t val = LT8920ReadRegister(R_FIFO);
    uint8_t packetSize = val >> 8;
    if (maxBuffer < packetSize + 1) {
      // BUFFER TOO SMALL
      return -2;
    }

    buffer[pos++] = val & 0xFF;
    while (pos < packetSize) {
      val = LT8920ReadRegister(R_FIFO);
      buffer[pos++] = val >> 8;
      buffer[pos++] = val & 0xFF;
    }

    return packetSize;
  } else
    // CRC error
    return -1;
}

void LT8920SetSyncWord(uint32_t syncWordLow, uint32_t syncWordHigh) {
  LT8920WriteRegister(R_SYNCWORD1, syncWordLow);
  LT8920WriteRegister(R_SYNCWORD2, syncWordLow >> 16);
  LT8920WriteRegister(R_SYNCWORD3, syncWordHigh);
  LT8920WriteRegister(R_SYNCWORD4, syncWordHigh >> 16);
}

void LT8920SetSyncWordLength(uint8_t option) {
  option &= 0x03;

  LT8920WriteRegister(32, (LT8920ReadRegister(32) & 0x0300) | (option << 11));
}

bool LT8920SendPacket(uint8_t *val, size_t packetSize) {
  uint8_t pos;
  if (packetSize < 1 || packetSize > 255)
    return false;

  // LT8920WriteRegister(R_CHANNEL, 0x0000);
  LT8920WriteRegister(R_FIFO_CONTROL, 0); // 0x8000);  //flush tx

  ////////////////////////////////////////////////////////
  LT8920WriteRegister(R_CHANNEL, (_channel & CHANNEL_MASK) |
                                     (1 << CHANNEL_TX_BIT)); // enable TX
  ////////////////////////////////////////////////////////

  // packets are sent in 16bit words, and the first word will be the packet
  // size. start spitting out words until we are done.

  pos = 0;
  LT8920WriteRegister2(R_FIFO, packetSize, val[pos++]);
  while (pos < packetSize) {
    uint8_t msb = val[pos++];
    uint8_t lsb = val[pos++];

    LT8920WriteRegister2(R_FIFO, msb, lsb);
  }

  // LT8920WriteRegister(R_CHANNEL,  (_channel & CHANNEL_MASK) |
  // (1<<CHANNEL_TX_BIT));   //enable TX

  // Wait until the packet is sent.
  /*while (digitalRead(_pin_pktflag) == 0)
  {
      //do nothing.
  }*/

  return true;
}

void LT8920SetChannel(uint8_t channel) {
  _channel = channel;
  LT8920WriteRegister(R_CHANNEL, (_channel & CHANNEL_MASK));
}

bool LT8920Available() { return (LT8920ReadRegister(48) & (1 << 6)) != 0; }

#define _BV(n) (1 << (n))

void LT8920ScanRSSI(uint16_t *buffer, uint8_t start_channel,
                    uint8_t num_channels) {
  // LT8920WriteRegister(R_CHANNEL, _BV(CHANNEL_RX_BIT));
  //
  // //add read mode.
  LT8920WriteRegister(R_FIFO_CONTROL, 0x8080); // flush rx
  // LT8920writeRegister(R_CHANNEL, 0x0000);

  // set number of channels to scan.
  LT8920WriteRegister(42, (LT8920ReadRegister(42) & 0b0000001111111111) |
                              ((num_channels - 1 & 0b111111) << 10));

  // set channel scan offset.
  LT8920WriteRegister(43, (LT8920ReadRegister(43) & 0b0000000011111111) |
                              ((start_channel & 0b1111111) << 8));
  LT8920WriteRegister(43,
                      (LT8920ReadRegister(43) & 0b0111111111111111) | _BV(15));

  while (!LT8920Available())
    ;

  // read the results.
  uint8_t pos = 0;
  while (pos < num_channels) {
    uint16_t data = LT8920ReadRegister(R_FIFO);
    buffer[pos++] = data >> 8;
  }
}

void LT8920BeginScanRSSI(uint8_t start_channel, uint8_t num_channels) {
  // LT8920WriteRegister(R_CHANNEL, _BV(CHANNEL_RX_BIT));
  //
  // //add read mode.
  LT8920WriteRegister(R_FIFO_CONTROL, 0x8080); // flush rx
  // LT8920writeRegister(R_CHANNEL, 0x0000);

  // set number of channels to scan.
  LT8920WriteRegister(42, (LT8920ReadRegister(42) & 0b0000001111111111) |
                              ((num_channels - 1 & 0b111111) << 10));

  // set channel scan offset.
  LT8920WriteRegister(43, (LT8920ReadRegister(43) & 0b0000000011111111) |
                              ((start_channel & 0b1111111) << 8));
  LT8920WriteRegister(43,
                      (LT8920ReadRegister(43) & 0b0111111111111111) | _BV(15));
}

void LT8920EndScanRSSI(uint16_t *buffer, uint8_t num_channels) {
  while (!LT8920Available())
    ;

  // read the results.
  uint8_t pos = 0;
  while (pos < num_channels) {
    uint16_t data = LT8920ReadRegister(R_FIFO);
    buffer[pos++] = data >> 8;
  }
}