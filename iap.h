#ifndef __IAP_H
#define __IAP_H
#define  CID_READ            0x0B
#define  DID_READ            0x0C
#define  PAGE_ERASE_AP       0x22
#define  BYTE_READ_AP        0x00
#define  BYTE_PROGRAM_AP     0x21
//#define  PAGE_SIZE           128u
#define  ERASE_FAIL          0x70
#define  PROGRAM_FAIL        0x71
#define  IAPFF_FAIL          0x72
#define  IAP_PASS            0x00

void write_data_flash(unsigned int u16_addr, unsigned char *pDat,unsigned int num);
void read_data_flash(unsigned int u16_addr, unsigned char *pDat,
                     unsigned int num);
#endif