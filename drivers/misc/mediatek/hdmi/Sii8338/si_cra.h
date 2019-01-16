#ifndef __SI_CRA_H__
#define __SI_CRA_H__
#include "si_common.h"
typedef uint16_t SiiReg_t;
bool_t SiiCraInitialize(void);
SiiResultCodes_t SiiCraGetLastResult(void);
///bool_t SiiRegInstanceSet(SiiReg_t virtualAddress, prefuint_t newInstance);
void SiiRegReadBlock(SiiReg_t virtualAddr, uint8_t *pBuffer, uint16_t count);
uint8_t SiiRegRead(SiiReg_t virtualAddr);
void SiiRegWriteBlock(SiiReg_t virtualAddr, const uint8_t *pBuffer, uint16_t count);
void SiiRegWrite(SiiReg_t virtualAddr, uint8_t value);
void SiiRegModify(SiiReg_t virtualAddr, uint8_t mask, uint8_t value);
void SiiRegBitsSet(SiiReg_t virtualAddr, uint8_t bitMask, bool_t setBits);
void SiiRegBitsSetNew(SiiReg_t virtualAddr, uint8_t bitMask, bool_t setBits);
void SiiRegEdidReadBlock(SiiReg_t segmentAddr, SiiReg_t virtualAddr, uint8_t *pBuffer,
			 uint16_t count);
#endif
