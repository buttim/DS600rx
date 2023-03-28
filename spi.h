#ifndef __SPI_H
#define __SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void SPIInit();
uint8_t SPITransfer(uint8_t x);
#endif