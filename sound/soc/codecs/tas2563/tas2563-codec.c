/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2021 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE.See the GNU General Public License for more details.
**
** File:
**     tas2563-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2563 High Performance 4W Smart
**     Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2563_CODEC
#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/crc8.h>

#include "tas2563.h"
#include "tas2563-codec.h"

#define	PPC_DRIVER_CRCCHK			0x00000200
#define	PPC_DRIVER_CONFDEV			0x00000000
#define	PPC_DRIVER_MTPLLSRC			0x00000400
#define	PPC_DRIVER_CFGDEV_NONCRC	0x00000101

#define TAS2563_CAL_NAME    "/mnt/vendor/persist/audio/tas2563_cal.bin"
#define RESTART_MAX 3

#define TAS2563_UDELAY 0xFFFFFFFE
#define TAS2563_MDELAY 0xFFFFFFFE
#define KCONTROL_CODEC

#define TAS2563_BLOCK_PLL				0x00
#define TAS2563_BLOCK_PGM_ALL			0x0d
#define TAS2563_BLOCK_PGM_DEV_A			0x01
#define TAS2563_BLOCK_PGM_DEV_B			0x08
#define TAS2563_BLOCK_CFG_COEFF_DEV_A	0x03
#define TAS2563_BLOCK_CFG_COEFF_DEV_B	0x0a
#define TAS2563_BLOCK_CFG_PRE_DEV_A		0x04
#define TAS2563_BLOCK_CFG_PRE_DEV_B		0x0b
#define TAS2563_BLOCK_CFG_POST			0x05
#define TAS2563_BLOCK_CFG_POST_POWER	0x06

static int tas2563_set_fmt(struct tas2563_priv *pTAS2563, unsigned int fmt);
static void tas2563_clear_firmware(struct TFirmware *pFirmware);

static int fw_parse(struct tas2563_priv *pTAS2563,
	struct TFirmware *pFirmware, unsigned char *pData, unsigned int nSize);
static bool tas2563_get_coefficient_in_block(struct tas2563_priv *pTAS2563,
	struct TBlock *pBlock, int nReg, int *pnValue);
int tas2563_set_program(struct tas2563_priv *pTAS2563, unsigned int nProgram, int nConfig);
static int tas2563_set_calibration(struct tas2563_priv *pTAS2563, int nCalibration);
static int tas2563_load_configuration(struct tas2563_priv *pTAS2563,
	unsigned int nConfiguration, bool bLoadSame);
static int tas2563_load_coefficient(struct tas2563_priv *pTAS2563,
	int nPrevConfig, int nNewConfig, bool bPowerOn);

static unsigned int tas2563_codec_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	int nResult = 0;
	unsigned int value = 0;

	nResult = pTAS2563->read(pTAS2563, reg, &value);

	if (nResult < 0)
		dev_err(pTAS2563->dev, "%s, ERROR, reg=0x%x, E=%d\n",
			__func__, reg, nResult);
	else
		dev_info(pTAS2563->dev, "%s, reg: 0x%x, value: 0x%x\n",
				__func__, reg, value);

	if (nResult >= 0)
		return value;
	else
		return nResult;
}

static const unsigned char crc8_lookup_table[CRC8_TABLE_SIZE] = {
0x00, 0x4D, 0x9A, 0xD7, 0x79, 0x34, 0xE3, 0xAE, 0xF2, 0xBF, 0x68, 0x25, 0x8B, 0xC6, 0x11, 0x5C,
0xA9, 0xE4, 0x33, 0x7E, 0xD0, 0x9D, 0x4A, 0x07, 0x5B, 0x16, 0xC1, 0x8C, 0x22, 0x6F, 0xB8, 0xF5,
0x1F, 0x52, 0x85, 0xC8, 0x66, 0x2B, 0xFC, 0xB1, 0xED, 0xA0, 0x77, 0x3A, 0x94, 0xD9, 0x0E, 0x43,
0xB6, 0xFB, 0x2C, 0x61, 0xCF, 0x82, 0x55, 0x18, 0x44, 0x09, 0xDE, 0x93, 0x3D, 0x70, 0xA7, 0xEA,
0x3E, 0x73, 0xA4, 0xE9, 0x47, 0x0A, 0xDD, 0x90, 0xCC, 0x81, 0x56, 0x1B, 0xB5, 0xF8, 0x2F, 0x62,
0x97, 0xDA, 0x0D, 0x40, 0xEE, 0xA3, 0x74, 0x39, 0x65, 0x28, 0xFF, 0xB2, 0x1C, 0x51, 0x86, 0xCB,
0x21, 0x6C, 0xBB, 0xF6, 0x58, 0x15, 0xC2, 0x8F, 0xD3, 0x9E, 0x49, 0x04, 0xAA, 0xE7, 0x30, 0x7D,
0x88, 0xC5, 0x12, 0x5F, 0xF1, 0xBC, 0x6B, 0x26, 0x7A, 0x37, 0xE0, 0xAD, 0x03, 0x4E, 0x99, 0xD4,
0x7C, 0x31, 0xE6, 0xAB, 0x05, 0x48, 0x9F, 0xD2, 0x8E, 0xC3, 0x14, 0x59, 0xF7, 0xBA, 0x6D, 0x20,
0xD5, 0x98, 0x4F, 0x02, 0xAC, 0xE1, 0x36, 0x7B, 0x27, 0x6A, 0xBD, 0xF0, 0x5E, 0x13, 0xC4, 0x89,
0x63, 0x2E, 0xF9, 0xB4, 0x1A, 0x57, 0x80, 0xCD, 0x91, 0xDC, 0x0B, 0x46, 0xE8, 0xA5, 0x72, 0x3F,
0xCA, 0x87, 0x50, 0x1D, 0xB3, 0xFE, 0x29, 0x64, 0x38, 0x75, 0xA2, 0xEF, 0x41, 0x0C, 0xDB, 0x96,
0x42, 0x0F, 0xD8, 0x95, 0x3B, 0x76, 0xA1, 0xEC, 0xB0, 0xFD, 0x2A, 0x67, 0xC9, 0x84, 0x53, 0x1E,
0xEB, 0xA6, 0x71, 0x3C, 0x92, 0xDF, 0x08, 0x45, 0x19, 0x54, 0x83, 0xCE, 0x60, 0x2D, 0xFA, 0xB7,
0x5D, 0x10, 0xC7, 0x8A, 0x24, 0x69, 0xBE, 0xF3, 0xAF, 0xE2, 0x35, 0x78, 0xD6, 0x9B, 0x4C, 0x01,
0xF4, 0xB9, 0x6E, 0x23, 0x8D, 0xC0, 0x17, 0x5A, 0x06, 0x4B, 0x9C, 0xD1, 0x7F, 0x32, 0xE5, 0xA8
};

static int isInPageYRAM(struct tas2563_priv *pTAS2563, struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (nBook == TAS2563_YRAM_BOOK1) {
		if (nPage == TAS2563_YRAM1_PAGE) {
			if (nReg >= TAS2563_YRAM1_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else if ((nReg + len) > TAS2563_YRAM1_START_REG) {
				pCRCData->mnOffset = TAS2563_YRAM1_START_REG;
				pCRCData->mnLen = len - (TAS2563_YRAM1_START_REG - nReg);
				nResult = 1;
			} else
				nResult = 0;
		} else if (nPage == TAS2563_YRAM3_PAGE) {
			if (nReg > TAS2563_YRAM3_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2563_YRAM3_START_REG) {
				if ((nReg + len) > TAS2563_YRAM3_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = TAS2563_YRAM3_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len - 1)) < TAS2563_YRAM3_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2563_YRAM3_START_REG;
					pCRCData->mnLen = len - (TAS2563_YRAM3_START_REG - nReg);
					nResult = 1;
				}
			}
		}
	} else if (nBook == TAS2563_YRAM_BOOK2) {
		if (nPage == TAS2563_YRAM5_PAGE) {
			if (nReg > TAS2563_YRAM5_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2563_YRAM5_START_REG) {
				if ((nReg + len) > TAS2563_YRAM5_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = TAS2563_YRAM5_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len - 1)) < TAS2563_YRAM5_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2563_YRAM5_START_REG;
					pCRCData->mnLen = len - (TAS2563_YRAM5_START_REG - nReg);
					nResult = 1;
				}
			}
		}
	} else
		nResult = 0;

	return nResult;
}

static int isInBlockYRAM(struct tas2563_priv *pTAS2563, struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult;

	if (nBook == TAS2563_YRAM_BOOK1) {
		if (nPage < TAS2563_YRAM2_START_PAGE)
			nResult = 0;
		else if (nPage <= TAS2563_YRAM2_END_PAGE) {
			if (nReg > TAS2563_YRAM2_END_REG)
				nResult = 0;
			else if (nReg >= TAS2563_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len - 1)) < TAS2563_YRAM2_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2563_YRAM2_START_REG;
					pCRCData->mnLen = nReg + len - TAS2563_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else
			nResult = 0;
	} else if (nBook == TAS2563_YRAM_BOOK2) {
		if (nPage < TAS2563_YRAM4_START_PAGE)
			nResult = 0;
		else if (nPage <= TAS2563_YRAM4_END_PAGE) {
			if (nReg > TAS2563_YRAM2_END_REG)
				nResult = 0;
			else if (nReg >= TAS2563_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len - 1)) < TAS2563_YRAM2_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2563_YRAM2_START_REG;
					pCRCData->mnLen = nReg + len - TAS2563_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else
			nResult = 0;
	} else
		nResult = 0;

	return nResult;
}


static int isYRAM(struct tas2563_priv *pTAS2563, struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult;

	nResult = isInPageYRAM(pTAS2563, pCRCData, nBook, nPage, nReg, len);

	if (nResult == 0)
		nResult = isInBlockYRAM(pTAS2563, pCRCData, nBook, nPage, nReg, len);

	return nResult;
}

/*
 * crc8 - calculate a crc8 over the given input data.
 *
 * table: crc table used for calculation.
 * pdata: pointer to data buffer.
 * nbytes: number of bytes in data buffer.
 * crc:	previous returned crc8 value.
 */
static u8 ti_crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata, size_t nbytes, u8 crc)
{
	/* loop over the buffer data */
	while (nbytes-- > 0)
		crc = table[(crc ^ *pdata++) & 0xff];

	return crc;
}

static int doSingleRegCheckSum(struct tas2563_priv *pTAS2563, 
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char nValue)
{
	int nResult = 0;
	struct TYCRC sCRCData;
	unsigned int nData1 = 0;

	if ((nBook == TAS2563_BOOK_ID(TAS2563_SA_COEFF_SWAP_REG))
		&& (nPage == TAS2563_PAGE_ID(TAS2563_SA_COEFF_SWAP_REG))
		&& (nReg >= TAS2563_PAGE_REG(TAS2563_SA_COEFF_SWAP_REG))
		&& (nReg <= (TAS2563_PAGE_REG(TAS2563_SA_COEFF_SWAP_REG) + 4))) {
		/* DSP swap command, pass */
		nResult = 0;
		goto end;
	}

	nResult = isYRAM(pTAS2563, &sCRCData, nBook, nPage, nReg, 1);
	if (nResult == 1) {
		nResult = pTAS2563->read(pTAS2563, TAS2563_REG(nBook, nPage, nReg), &nData1);
		if (nResult < 0)
			goto end;

		if (nData1 != nValue) {
			dev_err(pTAS2563->dev, "error2 (line %d),B[0x%x]P[0x%x]R[0x%x] W[0x%x], R[0x%x]\n",
				__LINE__, nBook, nPage, nReg, nValue, nData1);
			nResult = -EAGAIN;
			goto end;
		}

		nResult = ti_crc8(crc8_lookup_table, &nValue, 1, 0);
	}

end:

	return nResult;
}

static int doMultiRegCheckSum(struct tas2563_priv *pTAS2563, 
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned int len)
{
	int nResult = 0, i;
	unsigned char nCRCChkSum = 0;
	unsigned char nBuf1[128];
	struct TYCRC TCRCData;
	
	return 0;

	if ((nReg + len-1) > 127) {
		nResult = -EINVAL;
		dev_err(pTAS2563->dev, "firmware error\n");
		goto end;
	}

	if ((nBook == TAS2563_BOOK_ID(TAS2563_SA_COEFF_SWAP_REG))
		&& (nPage == TAS2563_PAGE_ID(TAS2563_SA_COEFF_SWAP_REG))
		&& (nReg == TAS2563_PAGE_REG(TAS2563_SA_COEFF_SWAP_REG))
		&& (len == 4)) {
		/* DSP swap command, pass */
		nResult = 0;
		goto end;
	}

	nResult = isYRAM(pTAS2563, &TCRCData, nBook, nPage, nReg, len);
	dev_info(pTAS2563->dev, "isYRAM: nBook 0x%x, nPage 0x%x, nReg 0x%x\n", nBook, nPage, nReg);
	dev_info(pTAS2563->dev, "isYRAM: TCRCData.mnLen 0x%x, len 0x%x, nResult %d\n", TCRCData.mnLen, len, nResult);
	dev_info(pTAS2563->dev, "TCRCData.mnOffset %x\n", TCRCData.mnOffset);
	if (nResult == 1) {
		if (len == 1) {
			dev_err(pTAS2563->dev, "firmware error\n");
			nResult = -EINVAL;
			goto end;
		} else {
			nResult = pTAS2563->bulk_read(pTAS2563, TAS2563_REG(nBook, nPage, TCRCData.mnOffset), nBuf1, TCRCData.mnLen);
			if (nResult < 0)
				goto end;

			for (i = 0; i < TCRCData.mnLen; i++) {
				if ((nBook == TAS2563_BOOK_ID(TAS2563_SA_COEFF_SWAP_REG))
					&& (nPage == TAS2563_PAGE_ID(TAS2563_SA_COEFF_SWAP_REG))
					&& ((i + TCRCData.mnOffset)
						>= TAS2563_PAGE_REG(TAS2563_SA_COEFF_SWAP_REG))
					&& ((i + TCRCData.mnOffset)
						<= (TAS2563_PAGE_REG(TAS2563_SA_COEFF_SWAP_REG) + 4))) {
					/* DSP swap command, bypass */
					continue;
				} else
					nCRCChkSum += ti_crc8(crc8_lookup_table, &nBuf1[i], 1, 0);
			}

			nResult = nCRCChkSum;
		}
	}

end:

	return nResult;
}


static int tas2563_load_block(struct tas2563_priv *pTAS2563, struct TBlock *pBlock)
{
	int nResult = 0;
	unsigned int nCommand = 0;
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nOffset;
	unsigned char nData;
	unsigned int nLength;
	unsigned int nSleep;
	unsigned char nCRCChkSum = 0;
	unsigned int nValue;
	int nRetry = 6;
	unsigned char *pData = pBlock->mpData;

	dev_info(pTAS2563->dev, "TAS2563 load block: Type = %d, commands = %d\n",
		pBlock->mnType, pBlock->mnCommands);
start:
	if (pBlock->mbPChkSumPresent) {
		nResult = pTAS2563->write(pTAS2563, TAS2563_I2CChecksum, 0);
		if (nResult < 0)
			goto end;
	}

	if (pBlock->mbYChkSumPresent)
		nCRCChkSum = 0;

	nCommand = 0;

	while (nCommand < pBlock->mnCommands) {
		pData = pBlock->mpData + nCommand * 4;

		nBook = pData[0];
		nPage = pData[1];
		nOffset = pData[2];
		nData = pData[3];

		nCommand++;

		if (nOffset <= 0x7F) {
			nResult = pTAS2563->write(pTAS2563, TAS2563_REG(nBook, nPage, nOffset), nData);
			if (nResult < 0)
				goto end;
			if (pBlock->mbYChkSumPresent) {
				nResult = doSingleRegCheckSum(pTAS2563, nBook, nPage, nOffset, nData);
				if (nResult < 0)
					goto check;
				nCRCChkSum += (unsigned char)nResult;
			}
		} else if (nOffset == 0x81) {
			nSleep = (nBook << 8) + nPage;
			msleep(nSleep);
		} else if (nOffset == 0x85) {
			pData += 4;
			nLength = (nBook << 8) + nPage;
			nBook = pData[0];
			nPage = pData[1];
			nOffset = pData[2];
			if (nLength > 1) {
				nResult = pTAS2563->bulk_write(pTAS2563, TAS2563_REG(nBook, nPage, nOffset), pData + 3, nLength);
				if (nResult < 0)
					goto end;
				if (pBlock->mbYChkSumPresent) {
					nResult = doMultiRegCheckSum(pTAS2563, nBook, nPage, nOffset, nLength);
					if (nResult < 0)
						goto check;
					nCRCChkSum += (unsigned char)nResult;
				}
			} else {
				nResult = pTAS2563->write(pTAS2563, TAS2563_REG(nBook, nPage, nOffset), pData[3]);
				if (nResult < 0)
					goto end;
				if (pBlock->mbYChkSumPresent) {
					nResult = doSingleRegCheckSum(pTAS2563, nBook, nPage, nOffset, pData[3]);
					if (nResult < 0)
						goto check;
					nCRCChkSum += (unsigned char)nResult;
				}
			}

			nCommand++;

			if (nLength >= 2)
				nCommand += ((nLength - 2) / 4) + 1;
		}
	}
	if (pBlock->mbPChkSumPresent) {
		nResult = pTAS2563->read(pTAS2563, TAS2563_I2CChecksum, &nValue);
                dev_err(pTAS2563->dev, "Block PChkSum: FW = 0x%x, Reg = 0x%x\n",
				pBlock->mnPChkSum, (nValue&0xff));

		if (nResult < 0)
			goto end;
		if ((nValue&0xff) != pBlock->mnPChkSum) {
			dev_err(pTAS2563->dev, "Block PChkSum Error: FW = 0x%x, Reg = 0x%x\n",
				pBlock->mnPChkSum, (nValue&0xff));
			nResult = -EAGAIN;
				pTAS2563->mnErrCode |= ERROR_PRAM_CRCCHK;
			goto check;
		}

		nResult = 0;
		pTAS2563->mnErrCode &= ~ERROR_PRAM_CRCCHK;
		dev_info(pTAS2563->dev, "Block[0x%x] PChkSum match\n", pBlock->mnType);
	}

	if (pBlock->mbYChkSumPresent) {
		//TBD, open it when FW ready
                dev_err(pTAS2563->dev, "Block YChkSum: FW = 0x%x, YCRC = 0x%x\n",
				pBlock->mnYChkSum, nCRCChkSum);
/*
		if (nCRCChkSum != pBlock->mnYChkSum) {
			dev_err(pTAS2563->dev, "Block YChkSum Error: FW = 0x%x, YCRC = 0x%x\n",
				pBlock->mnYChkSum, nCRCChkSum);
			nResult = -EAGAIN;
			pTAS2563->mnErrCode |= ERROR_YRAM_CRCCHK;
			goto check;
		}
*/
		pTAS2563->mnErrCode &= ~ERROR_YRAM_CRCCHK;
		nResult = 0;
		dev_info(pTAS2563->dev, "Block[0x%x] YChkSum match\n", pBlock->mnType);
	}

check:
	if (nResult == -EAGAIN) {
		nRetry--;
		if (nRetry > 0)
			goto start;
	}

end:
	if (nResult < 0) {
		dev_err(pTAS2563->dev, "Block (%d) load error\n",
				pBlock->mnType);
	}
	return nResult;
}


static int tas2563_load_data(struct tas2563_priv *pTAS2563, struct TData *pData, unsigned int nType)
{
	int nResult = 0;
	unsigned int nBlock;
	struct TBlock *pBlock;

	dev_info(pTAS2563->dev,
		"TAS2563 load data: %s, Blocks = %d, Block Type = %d\n", pData->mpName, pData->mnBlocks, nType);

	for (nBlock = 0; nBlock < pData->mnBlocks; nBlock++) {
		pBlock = &(pData->mpBlocks[nBlock]);
		if (pBlock->mnType == nType) {
			nResult = tas2563_load_block(pTAS2563, pBlock);
			if (nResult < 0)
				break;
		}
	}

	return nResult;
}

void tas2563_clear_firmware(struct TFirmware *pFirmware)
{
	unsigned int n, nn;

	if (!pFirmware)
		return;

	kfree(pFirmware->mpDescription);

	if (pFirmware->mpPLLs != NULL) {
		for (n = 0; n < pFirmware->mnPLLs; n++) {
			kfree(pFirmware->mpPLLs[n].mpDescription);
			kfree(pFirmware->mpPLLs[n].mBlock.mpData);
		}
		kfree(pFirmware->mpPLLs);
	}

	if (pFirmware->mpPrograms != NULL) {
		for (n = 0; n < pFirmware->mnPrograms; n++) {
			kfree(pFirmware->mpPrograms[n].mpDescription);
			kfree(pFirmware->mpPrograms[n].mData.mpDescription);
			for (nn = 0; nn < pFirmware->mpPrograms[n].mData.mnBlocks; nn++)
				kfree(pFirmware->mpPrograms[n].mData.mpBlocks[nn].mpData);
			kfree(pFirmware->mpPrograms[n].mData.mpBlocks);
		}
		kfree(pFirmware->mpPrograms);
	}

	if (pFirmware->mpConfigurations != NULL) {
		for (n = 0; n < pFirmware->mnConfigurations; n++) {
			kfree(pFirmware->mpConfigurations[n].mpDescription);
			kfree(pFirmware->mpConfigurations[n].mData.mpDescription);
			for (nn = 0; nn < pFirmware->mpConfigurations[n].mData.mnBlocks; nn++)
				kfree(pFirmware->mpConfigurations[n].mData.mpBlocks[nn].mpData);
			kfree(pFirmware->mpConfigurations[n].mData.mpBlocks);
		}
		kfree(pFirmware->mpConfigurations);
	}

	if (pFirmware->mpCalibrations != NULL) {
		for (n = 0; n < pFirmware->mnCalibrations; n++) {
			kfree(pFirmware->mpCalibrations[n].mpDescription);
			kfree(pFirmware->mpCalibrations[n].mData.mpDescription);
			for (nn = 0; nn < pFirmware->mpCalibrations[n].mData.mnBlocks; nn++)
				kfree(pFirmware->mpCalibrations[n].mData.mpBlocks[nn].mpData);
			kfree(pFirmware->mpCalibrations[n].mData.mpBlocks);
		}
		kfree(pFirmware->mpCalibrations);
	}

	memset(pFirmware, 0x00, sizeof(struct TFirmware));
}

static int tas2563_load_configuration(struct tas2563_priv *pTAS2563,
	unsigned int nConfiguration, bool bLoadSame)
{
	int nResult = 0;
	struct TConfiguration *pCurrentConfiguration = NULL;
	struct TConfiguration *pNewConfiguration = NULL;

	dev_info(pTAS2563->dev, "%s: %d\n", __func__, nConfiguration);

	if ((!pTAS2563->mpFirmware->mpPrograms) ||
		(!pTAS2563->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2563->dev, "%s, Firmware not loaded\n", __func__);
		nResult = 0;
		goto end;
	}

	if (nConfiguration >= pTAS2563->mpFirmware->mnConfigurations) {
		dev_err(pTAS2563->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		nResult = 0;
		goto end;
	}

	if ((!pTAS2563->mbLoadConfigurationPrePowerUp)
		&& (nConfiguration == pTAS2563->mnCurrentConfiguration)
		&& (!bLoadSame)) {
		dev_info(pTAS2563->dev, "Configuration %d is already loaded\n",
			nConfiguration);
		nResult = 0;
		goto end;
	}

	pCurrentConfiguration =
		&(pTAS2563->mpFirmware->mpConfigurations[pTAS2563->mnCurrentConfiguration]);
	pNewConfiguration =
		&(pTAS2563->mpFirmware->mpConfigurations[nConfiguration]);
	if (pNewConfiguration->mnProgram != pCurrentConfiguration->mnProgram) {
		dev_err(pTAS2563->dev, "Configuration %d, %s doesn't share the same program as current %d\n",
			nConfiguration, pNewConfiguration->mpName, pCurrentConfiguration->mnProgram);
		nResult = 0;
		goto end;
	}

	if (pTAS2563->mbPowerUp) {
		pTAS2563->mbLoadConfigurationPrePowerUp = false;
		nResult = tas2563_load_coefficient(pTAS2563, pTAS2563->mnCurrentConfiguration, nConfiguration, true);
	} else {
		dev_info(pTAS2563->dev,
			"TAS2563 was powered down, will load coefficient when power up\n");
		pTAS2563->mbLoadConfigurationPrePowerUp = true;
		pTAS2563->mnNewConfiguration = nConfiguration;
	}

end:

/*	if (nResult < 0) {
		if (pTAS2563->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK))
			failsafe(pTAS2563);
	}
*/
	return nResult;
}

static int tas2563_load_calibration(struct tas2563_priv *pTAS2563,	char *pFileName)
{
	int nResult = 0;

	int nFile;
	mm_segment_t fs;
	unsigned char pBuffer[1000];
	int nSize = 0;

	dev_info(pTAS2563->dev, "%s:\n", __func__);

	fs = get_fs();
	set_fs(KERNEL_DS);
	nFile = sys_open(pFileName, O_RDONLY, 0);

	dev_info(pTAS2563->dev, "TAS2563 calibration file = %s, handle = %d\n",
		pFileName, nFile);

	if (nFile >= 0) {
		nSize = sys_read(nFile, pBuffer, 1000);
		sys_close(nFile);
	} else {
		dev_err(pTAS2563->dev, "TAS2563 cannot open calibration file: %s\n",
			pFileName);
	}

	set_fs(fs);

	if (!nSize)
		goto end;

	tas2563_clear_firmware(pTAS2563->mpCalFirmware);
	dev_info(pTAS2563->dev, "TAS2563 calibration file size = %d\n", nSize);
	nResult = fw_parse(pTAS2563, pTAS2563->mpCalFirmware, pBuffer, nSize);

	if (nResult)
		dev_err(pTAS2563->dev, "TAS2563 calibration file is corrupt\n");
	else
		dev_info(pTAS2563->dev, "TAS2563 calibration: %d calibrations\n",
			pTAS2563->mpCalFirmware->mnCalibrations);
end:

	return nResult;
}

static int tas2563_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

	int nResult = 0;

	nResult = pTAS2563->write(pTAS2563, reg, value);
	if (nResult < 0)
		dev_err(pTAS2563->dev, "%s, ERROR, reg=0x%x, E=%d\n",
			__func__, reg, nResult);
	else
		dev_info(pTAS2563->dev, "%s, reg: 0x%x, 0x%x\n",
			__func__, reg, value);

	return nResult;

}

static void fw_print_header(struct tas2563_priv *pTAS2563, struct TFirmware *pFirmware)
{
	dev_info(pTAS2563->dev, "FW Size       = %d", pFirmware->mnFWSize);
	dev_info(pTAS2563->dev, "Checksum      = 0x%04X", pFirmware->mnChecksum);
	dev_info(pTAS2563->dev, "PPC Version   = 0x%04X", pFirmware->mnPPCVersion);
	dev_info(pTAS2563->dev, "FW  Version    = 0x%04X", pFirmware->mnFWVersion);
	dev_info(pTAS2563->dev, "Driver Version= 0x%04X", pFirmware->mnDriverVersion);
	dev_info(pTAS2563->dev, "Timestamp     = %d", pFirmware->mnTimeStamp);
	dev_info(pTAS2563->dev, "DDC Name      = %s", pFirmware->mpDDCName);
	dev_info(pTAS2563->dev, "Description   = %s", pFirmware->mpDescription);
}

inline unsigned int fw_convert_number(unsigned char *pData)
{
	return pData[3] + (pData[2] << 8) + (pData[1] << 16) + (pData[0] << 24);
}

static int fw_parse_header(struct tas2563_priv *pTAS2563,
	struct TFirmware *pFirmware, unsigned char *pData, unsigned int nSize)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned char pMagicNumber[] = { 0x35, 0x35, 0x35, 0x32 };

	if (nSize < 104) {
		dev_err(pTAS2563->dev, "Firmware: Header too short");
		return -EINVAL;
	}

	if (memcmp(pData, pMagicNumber, 4)) {
		dev_err(pTAS2563->dev, "Firmware: Magic number doesn't match");
		return -EINVAL;
	}
	pData += 4;

	pFirmware->mnFWSize = fw_convert_number(pData);
	pData += 4;
	dev_info(pTAS2563->dev, "firmware size: %d", pFirmware->mnFWSize);

	pFirmware->mnChecksum = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnPPCVersion = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnFWVersion = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnDriverVersion = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnTimeStamp = fw_convert_number(pData);
	pData += 4;
	dev_info(pTAS2563->dev, "FW timestamp: %d", pFirmware->mnTimeStamp);

	memcpy(pFirmware->mpDDCName, pData, 64);
	pData += 64;

	n = strlen(pData);
	pFirmware->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
	pData += n + 1;
	if ((pData - pDataStart) >= nSize) {
		dev_err(pTAS2563->dev, "Firmware: Header too short after DDC description");
		return -EINVAL;
	}

	pFirmware->mnDeviceFamily = fw_convert_number(pData);
	pData += 4;
	if (pFirmware->mnDeviceFamily != 0) {
		dev_err(pTAS2563->dev,
			"deviceFamily %d, not TAS device", pFirmware->mnDeviceFamily);
		return -EINVAL;
	}

	pFirmware->mnDevice = fw_convert_number(pData);
	pData += 4;

	if (pFirmware->mnDevice != 5) {
		dev_err(pTAS2563->dev,
			"device %d, not TAS2563", pFirmware->mnDevice);
		return -EINVAL;
	}

	fw_print_header(pTAS2563, pFirmware);
	return pData - pDataStart;
}

static int fw_parse_block_data(struct tas2563_priv *pTAS2563, struct TFirmware *pFirmware,
	struct TBlock *pBlock, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;

	dev_info(pTAS2563->dev, "%s, %d", __func__, __LINE__);

	pBlock->mnType = fw_convert_number(pData);
	pData += 4;
	dev_info(pTAS2563->dev, "%s, %d", __func__, __LINE__);

	if (pFirmware->mnDriverVersion >= PPC_DRIVER_CRCCHK) {
		pBlock->mbPChkSumPresent = pData[0];
		pData++;

		pBlock->mnPChkSum = pData[0];
		pData++;

		pBlock->mbYChkSumPresent = pData[0];
		pData++;

		pBlock->mnYChkSum = pData[0];
		pData++;
	} else {
		pBlock->mbPChkSumPresent = 0;
		pBlock->mbYChkSumPresent = 0;
	}

	pBlock->mnCommands = fw_convert_number(pData);
	pData += 4;

	n = pBlock->mnCommands * 4;
	pBlock->mpData = kmemdup(pData, n, GFP_KERNEL);
	pData += n;
	dev_info(pTAS2563->dev, "%s, %d", __func__, __LINE__);
	return pData - pDataStart;
}

static int fw_parse_data(struct tas2563_priv *pTAS2563, struct TFirmware *pFirmware,
	struct TData *pImageData, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int nBlock;
	unsigned int n;

	dev_info(pTAS2563->dev, "%s, %d", __func__, __LINE__);
	memcpy(pImageData->mpName, pData, 64);
	pData += 64;

	n = strlen(pData);
	pImageData->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
	pData += n + 1;

	pImageData->mnBlocks = (pData[0] << 8) + pData[1];
	pData += 2;

	pImageData->mpBlocks =
		kmalloc(sizeof(struct TBlock) * pImageData->mnBlocks, GFP_KERNEL);

	for (nBlock = 0; nBlock < pImageData->mnBlocks; nBlock++) {
		n = fw_parse_block_data(pTAS2563, pFirmware,
			&(pImageData->mpBlocks[nBlock]), pData);
		pData += n;
	}
	return pData - pDataStart;
}

static int fw_parse_program_data(struct tas2563_priv *pTAS2563,
	struct TFirmware *pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nProgram;
	struct TProgram *pProgram;

	pFirmware->mnPrograms = (pData[0] << 8) + pData[1];
	pData += 2;

	if (pFirmware->mnPrograms == 0)
		goto end;

	pFirmware->mpPrograms =
		kmalloc(sizeof(struct TProgram) * pFirmware->mnPrograms, GFP_KERNEL);
	for (nProgram = 0; nProgram < pFirmware->mnPrograms; nProgram++) {
		pProgram = &(pFirmware->mpPrograms[nProgram]);
		memcpy(pProgram->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pProgram->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		pProgram->mnAppMode = pData[0];
		pData++;

		pProgram->mnI2sMode = pData[0];
		pData++;
		dev_info(pTAS2563->dev, "FW i2sMode: %d", pProgram->mnI2sMode);

		pProgram->mnISnsPD = pData[0];
		pData++;

		pProgram->mnVSnsPD = pData[0];
		pData++;

		pProgram->mnPowerLDG = pData[0];
		pData++;

		n = fw_parse_data(pTAS2563, pFirmware, &(pProgram->mData), pData);
		pData += n;
		dev_info(pTAS2563->dev, "program data number: %d", n);
	}

end:

	return pData - pDataStart;
}

static int fw_parse_configuration_data(struct tas2563_priv *pTAS2563,
	struct TFirmware *pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nConfiguration;
	struct TConfiguration *pConfiguration;

	pFirmware->mnConfigurations = (pData[0] << 8) + pData[1];
	pData += 2;

	if (pFirmware->mnConfigurations == 0)
		goto end;

	pFirmware->mpConfigurations =
		kmalloc(sizeof(struct TConfiguration) * pFirmware->mnConfigurations,
		GFP_KERNEL);
	for (nConfiguration = 0; nConfiguration < pFirmware->mnConfigurations;
		nConfiguration++) {
		pConfiguration = &(pFirmware->mpConfigurations[nConfiguration]);
		memcpy(pConfiguration->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pConfiguration->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

/*
		if ((pFirmware->mnDriverVersion >= PPC_DRIVER_CONFDEV)
			|| ((pFirmware->mnDriverVersion >= PPC_DRIVER_CFGDEV_NONCRC)
				&& (pFirmware->mnDriverVersion < PPC_DRIVER_CRCCHK))) {*/
			pConfiguration->mnDevices = (pData[0] << 8) + pData[1];
			pData += 2;
/*		} else
			pConfiguration->mnDevices = 1;*/

		pConfiguration->mnProgram = pData[0];
		pData++;
		dev_info(pTAS2563->dev, "configuration, mnProgram: %d", pConfiguration->mnProgram);

		pConfiguration->mnSamplingRate = fw_convert_number(pData);
		pData += 4;
		dev_info(pTAS2563->dev, "configuration samplerate: %d", pConfiguration->mnSamplingRate);

		//if (pFirmware->mnDriverVersion >= PPC_DRIVER_MTPLLSRC) {
			pConfiguration->mnPLLSrc = pData[0];
			pData++;

			pConfiguration->mnPLLSrcRate = fw_convert_number(pData);
			pData += 4;
		//}

		pConfiguration->mnFsRate = (pData[0] << 8) + pData[1];
		pData += 2;
		dev_info(pTAS2563->dev, "Fs rate: %d", pConfiguration->mnFsRate);

		n = fw_parse_data(pTAS2563, pFirmware, &(pConfiguration->mData), pData);
		pData += n;
	}

end:

	return pData - pDataStart;
}

int fw_parse_calibration_data(struct tas2563_priv *pTAS2563,
	struct TFirmware *pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nCalibration;
	struct TCalibration *pCalibration;

	pFirmware->mnCalibrations = (pData[0] << 8) + pData[1];
	pData += 2;

	if (pFirmware->mnCalibrations == 0)
		goto end;

	pFirmware->mpCalibrations =
		kmalloc(sizeof(struct TCalibration) * pFirmware->mnCalibrations, GFP_KERNEL);
	for (nCalibration = 0;
		nCalibration < pFirmware->mnCalibrations;
		nCalibration++) {
		pCalibration = &(pFirmware->mpCalibrations[nCalibration]);
		memcpy(pCalibration->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pCalibration->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		pCalibration->mnProgram = pData[0];
		pData++;

		pCalibration->mnConfiguration = pData[0];
		pData++;

		n = fw_parse_data(pTAS2563, pFirmware, &(pCalibration->mData), pData);
		pData += n;
	}

end:

	return pData - pDataStart;
}

static int fw_parse(struct tas2563_priv *pTAS2563,
	struct TFirmware *pFirmware, unsigned char *pData, unsigned int nSize)
{
	int nPosition = 0;

	nPosition = fw_parse_header(pTAS2563, pFirmware, pData, nSize);
	dev_info(pTAS2563->dev, "header size: %d, line: %d\n", nPosition, __LINE__);
	if (nPosition < 0) {
		dev_err(pTAS2563->dev, "Firmware: Wrong Header");
		return -EINVAL;
	}

	if (nPosition >= nSize) {
		dev_err(pTAS2563->dev, "Firmware: Too short");
		return -EINVAL;
	}

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_program_data(pTAS2563, pFirmware, pData);
	dev_info(pTAS2563->dev, "program size: %d, line: %d\n", nPosition, __LINE__);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_configuration_data(pTAS2563, pFirmware, pData);
	dev_info(pTAS2563->dev, "config size: %d, line: %d\n", nPosition, __LINE__);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	if (nSize > 64)
		nPosition = fw_parse_calibration_data(pTAS2563, pFirmware, pData);
	dev_info(pTAS2563->dev, "calib size: %d, line: %d\n", nPosition, __LINE__);
	return 0;
}


static int tas2563_codec_suspend(struct snd_soc_codec *codec)
{
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2563->codec_lock);

	dev_info(pTAS2563->dev, "%s\n", __func__);
	pTAS2563->runtime_suspend(pTAS2563);

	mutex_unlock(&pTAS2563->codec_lock);
	return ret;
}

static int tas2563_codec_resume(struct snd_soc_codec *codec)
{
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2563->codec_lock);

	dev_info(pTAS2563->dev, "%s\n", __func__);
	pTAS2563->runtime_resume(pTAS2563);

	mutex_unlock(&pTAS2563->codec_lock);
	return ret;
}

static int tas2563_set_power_state(struct tas2563_priv *pTAS2563, int state)
{
	int nResult = 0, irqreg;
	/*unsigned int nValue;*/
	const char *pFWName;
	struct TProgram *pProgram;

	dev_info(pTAS2563->dev, "set power state: %d\n", state);

	if ((pTAS2563->mpFirmware->mnPrograms == 0)
		|| (pTAS2563->mpFirmware->mnConfigurations == 0)) {
		dev_err(pTAS2563->dev, "%s, firmware not loaded\n", __func__);
		pFWName = TAS2563_FW_NAME;
		nResult = request_firmware_nowait(THIS_MODULE, 1, pFWName,
			pTAS2563->dev, GFP_KERNEL, pTAS2563, tas2563_fw_ready);

		if(nResult < 0) {
			dev_err(pTAS2563->dev, "%s, firmware is not loaded, return %d\n",
					__func__, nResult);
			goto end;
		}
	}
	/* check safe guard*/
	/* TBD, add back when FW ready
	nResult = pTAS2563->read(pTAS2563, TAS2563_SAFE_GUARD_REG, &nValue);
	if (nResult < 0)
		goto end;
	if ((nValue&0xff) != TAS2563_SAFE_GUARD_PATTERN) {
		dev_err(pTAS2563->dev, "ERROR safe guard failure!\n");
		nResult = -EPIPE;
		goto end;
	}
	*/

	pProgram = &(pTAS2563->mpFirmware->mpPrograms[pTAS2563->mnCurrentProgram]);
	dev_info(pTAS2563->dev, "%s, state: %d, mbPowerup %d\n", __func__, state, pTAS2563->mbPowerUp);
	if (state != TAS2563_POWER_SHUTDOWN) {
		if (!pTAS2563->mbPowerUp) {
			if (!pTAS2563->mbCalibrationLoaded) {
				nResult = tas2563_set_calibration(pTAS2563, 0xFF);
				if((nResult > 0) || (nResult == 0))
					pTAS2563->mbCalibrationLoaded = true;
			}

			if (pTAS2563->mbLoadConfigurationPrePowerUp) {
				dev_info(pTAS2563->dev, "load coefficient before power\n");
				pTAS2563->mbLoadConfigurationPrePowerUp = false;
				nResult = tas2563_load_coefficient(pTAS2563,
					pTAS2563->mnCurrentConfiguration, pTAS2563->mnNewConfiguration, false);
				if (nResult < 0)
					goto end;
			}
		}
	}

	switch (state) {
	case TAS2563_POWER_ACTIVE:
/*
		nResult = pTAS2563->update_bits(pTAS2563, TAS2563_PowerControl,
			TAS2563_PowerControl_OperationalMode10_Mask |
			TAS2563_PowerControl_ISNSPower_Mask |
			TAS2563_PowerControl_VSNSPower_Mask,
			TAS2563_PowerControl_OperationalMode10_Active |
			TAS2563_PowerControl_VSNSPower_Active |
			TAS2563_PowerControl_ISNSPower_Active);
		if (nResult < 0)
			return nResult;
*/

//Clear latched IRQ before power on
		pTAS2563->update_bits(pTAS2563, TAS2563_InterruptConfiguration,
					TAS2563_InterruptConfiguration_CLEARLATINT_Mask,
					TAS2563_InterruptConfiguration_CLEARLATINT_CLEAR);
		pTAS2563->mbPowerUp = true;

		pTAS2563->read(pTAS2563, TAS2563_LatchedInterruptReg0, &irqreg);
		dev_info(pTAS2563->dev, "IRQ reg is: %d, %d\n", irqreg, __LINE__);

//		pTAS2563->enableIRQ(pTAS2563, true);
		schedule_delayed_work(&pTAS2563->irq_work, msecs_to_jiffies(10));

		break;

	case TAS2563_POWER_MUTE:
		nResult = pTAS2563->update_bits(pTAS2563, TAS2563_PowerControl,
			TAS2563_PowerControl_OperationalMode10_Mask |
			TAS2563_PowerControl_ISNSPower_Mask |
			TAS2563_PowerControl_VSNSPower_Mask,
			TAS2563_PowerControl_OperationalMode10_Mute |
			TAS2563_PowerControl_VSNSPower_Active |
			TAS2563_PowerControl_ISNSPower_Active);
			pTAS2563->mbPowerUp = true;
		break;

	case TAS2563_POWER_SHUTDOWN:
		nResult = pTAS2563->update_bits(pTAS2563, TAS2563_PowerControl,
			TAS2563_PowerControl_OperationalMode10_Mask,
			TAS2563_PowerControl_OperationalMode10_Shutdown);
			pTAS2563->mbPowerUp = false;
			pTAS2563->enableIRQ(pTAS2563, false);
		break;

	default:
		dev_err(pTAS2563->dev, "wrong power state setting %d\n", state);

	}

end:
	pTAS2563->mnPowerState = state;
	return nResult;
}

static int tas2563_dac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
#else
	struct snd_soc_codec *codec = w->codec;
#endif
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_info(pTAS2563->dev, "SND_SOC_DAPM_POST_PMU\n");
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_info(pTAS2563->dev, "SND_SOC_DAPM_PRE_PMD\n");
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget tas2563_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("ASI1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0, tas2563_dac_event,
	SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT"),
	SND_SOC_DAPM_SIGGEN("VMON"),
	SND_SOC_DAPM_SIGGEN("IMON")
};

static const struct snd_soc_dapm_route tas2563_audio_map[] = {
	{"DAC", NULL, "ASI1"},
	{"OUT", NULL, "DAC"},
};

static int tas2563_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

        dev_info(pTAS2563->dev, "%s\n", __func__);
	mutex_lock(&pTAS2563->codec_lock);
	if (mute) {
                dev_info(pTAS2563->dev, "mute: %s\n", __func__);
		tas2563_set_power_state(pTAS2563, TAS2563_POWER_SHUTDOWN);
	} else {
                dev_info(pTAS2563->dev, "unmute: %s\n", __func__);
		tas2563_set_power_state(pTAS2563, TAS2563_POWER_ACTIVE);
	}
	mutex_unlock(&pTAS2563->codec_lock);
	return 0;
}


static int tas2563_slot_config(struct snd_soc_codec *codec, struct tas2563_priv *pTAS2563, int blr_clk_ratio)
{
	int ret = 0;
	ret = pTAS2563->update_bits(pTAS2563,
			TAS2563_TDMConfigurationReg5, 0xff, 0x42);
	if(ret < 0)
		return ret;

	ret = pTAS2563->update_bits(pTAS2563,
			TAS2563_TDMConfigurationReg6, 0xff, 0x40);

	return ret;
}

static int tas2563_set_slot(struct tas2563_priv *pTAS2563, int slot_width)
{
	int ret = 0;
	dev_info(pTAS2563->dev, "%s, slot_width:%d\n", __func__, slot_width);

	switch (slot_width) {
	case 16:
	ret = pTAS2563->update_bits(pTAS2563, 
		TAS2563_TDMConfigurationReg2,
		TAS2563_TDMConfigurationReg2_RXSLEN10_Mask,
		TAS2563_TDMConfigurationReg2_RXSLEN10_16Bits);
	break;

	case 24:
	ret = pTAS2563->update_bits(pTAS2563, 
		TAS2563_TDMConfigurationReg2,
		TAS2563_TDMConfigurationReg2_RXSLEN10_Mask,
		TAS2563_TDMConfigurationReg2_RXSLEN10_24Bits);
	break;

	case 32:
	ret = pTAS2563->update_bits(pTAS2563, 
		TAS2563_TDMConfigurationReg2,
		TAS2563_TDMConfigurationReg2_RXSLEN10_Mask,
		TAS2563_TDMConfigurationReg2_RXSLEN10_32Bits);
	break;

	case 0:
	/* Do not change slot width */
	break;

	default:
		dev_info(pTAS2563->dev, "slot width not supported");
		ret = -EINVAL;
	}

	if (ret >= 0)
		pTAS2563->mnSlot_width = slot_width;

	return ret;
}

static int tas2563_set_bitwidth(struct tas2563_priv *pTAS2563, int bitwidth)
{
	int slot_width_tmp = 0;
	dev_info(pTAS2563->dev, "%s %d\n", __func__, __LINE__);

	switch (bitwidth) {
	case SNDRV_PCM_FORMAT_S16_LE:
			pTAS2563->update_bits(pTAS2563, 
			TAS2563_TDMConfigurationReg2,
			TAS2563_TDMConfigurationReg2_RXWLEN32_Mask,
			TAS2563_TDMConfigurationReg2_RXWLEN32_16Bits);
			pTAS2563->mnCh_size = 16;
			if (pTAS2563->mnSlot_width == 0)
				slot_width_tmp = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
			pTAS2563->update_bits(pTAS2563, 
			TAS2563_TDMConfigurationReg2,
			TAS2563_TDMConfigurationReg2_RXWLEN32_Mask,
			TAS2563_TDMConfigurationReg2_RXWLEN32_24Bits);
			pTAS2563->mnCh_size = 24;
			if (pTAS2563->mnSlot_width == 0)
				slot_width_tmp = 32;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
			pTAS2563->update_bits(pTAS2563,
			TAS2563_TDMConfigurationReg2,
			TAS2563_TDMConfigurationReg2_RXWLEN32_Mask,
			TAS2563_TDMConfigurationReg2_RXWLEN32_32Bits);
			pTAS2563->mnCh_size = 32;
			if (pTAS2563->mnSlot_width == 0)
				slot_width_tmp = 32;
		break;

	default:
		dev_info(pTAS2563->dev, "Not supported params format\n");
	}

	/* If machine driver did not call set slot width */
	if (pTAS2563->mnSlot_width == 0)
		tas2563_set_slot(pTAS2563, slot_width_tmp);

	dev_info(pTAS2563->dev, "mnCh_size: %d\n", pTAS2563->mnCh_size);
	pTAS2563->mnPCMFormat = bitwidth;

	return 0;
}

static int tas2563_set_samplerate(struct tas2563_priv *pTAS2563, int samplerate)
{
	switch (samplerate) {
	case 48000:
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_48KHz);
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_44_1_48kHz);
			break;
	case 44100:
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz);
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_44_1_48kHz);
			break;
	case 96000:
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_48KHz);
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_88_2_96kHz);
			break;
	case 88200:
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz);
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_88_2_96kHz);
			break;
	case 19200:
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_48KHz);
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_176_4_192kHz);
			break;
	case 17640:
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATERAMP_44_1KHz);
			pTAS2563->update_bits(pTAS2563,
				TAS2563_TDMConfigurationReg0,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_Mask,
				TAS2563_TDMConfigurationReg0_SAMPRATE31_176_4_192kHz);
			break;
	default:
			dev_info(pTAS2563->dev, "%s, unsupported sample rate, %d\n", __func__, samplerate);

	}

	pTAS2563->mnSamplingRate = samplerate;
	return 0;
}

int tas2563_load_default(struct tas2563_priv *pTAS2563)
{
	int ret = 0;
	
	dev_info(pTAS2563->dev, "%s, %d, ret = %d", __func__, __LINE__, ret);

	ret = tas2563_set_slot(pTAS2563, pTAS2563->mnSlot_width);
	if (ret < 0)
		goto end;
	dev_info(pTAS2563->dev, "%s, %d, ret = %d", __func__, __LINE__, ret);

	/* proper TX format */
	ret = pTAS2563->write(pTAS2563, TAS2563_TDMConfigurationReg4, 0x01);
	if(ret < 0)
		goto end;

	/*if setting format was not called by asoc, then set it default*/
	if(pTAS2563->mnASIFormat == 0)
                pTAS2563->mnASIFormat = SND_SOC_DAIFMT_CBS_CFS 
				| SND_SOC_DAIFMT_IB_NF 
				| SND_SOC_DAIFMT_I2S;
	ret = tas2563_set_fmt(pTAS2563, pTAS2563->mnASIFormat);

	if (ret < 0)
		goto end;
	dev_info(pTAS2563->dev, "%s, %d, ret = %d", __func__, __LINE__, ret);

	ret = tas2563_set_bitwidth(pTAS2563, pTAS2563->mnPCMFormat);
	if (ret < 0)
		goto end;
	dev_info(pTAS2563->dev, "%s, %d, ret = %d", __func__, __LINE__, ret);

	ret = tas2563_set_samplerate(pTAS2563, pTAS2563->mnSamplingRate);
	if (ret < 0)
		goto end;

/*Enable TDM IRQ */
	ret = pTAS2563->update_bits(pTAS2563, TAS2563_InterruptMaskReg0,
			TAS2563_InterruptMaskReg0_TDMClockErrorINTMASK_Mask,
			TAS2563_InterruptMaskReg0_TDMClockErrorINTMASK_Unmask);
/* disable the DMA5 deglitch filter and halt timer */
	ret = pTAS2563->update_bits(pTAS2563, TAS2563_CLKERR_Config,
			TAS2563_CLKERR_Config_DMA5FILTER_Mask,
			TAS2563_CLKERR_Config_DMA5FILTER_Disable);
/* disable clk halt timer */
	ret = pTAS2563->update_bits(pTAS2563, TAS2563_InterruptConfiguration,
			TAS2563_InterruptConfiguration_CLKHALT_Mask,
			TAS2563_InterruptConfiguration_CLKHALT_Disable);

end:
/* Load default failed, restart later */
	dev_info(pTAS2563->dev, "%s, %d, ret = %d", __func__, __LINE__, ret);
	if (ret < 0)
		schedule_delayed_work(&pTAS2563->irq_work,
				msecs_to_jiffies(1000));
	return ret;
}

#if 0
static void failsafe(struct tas2563_priv *pTAS2563)
{
	dev_err(pTAS2563->dev, "%s\n", __func__);
	pTAS2563->mnErrCode |= ERROR_FAILSAFE;
	if (hrtimer_active(&pTAS2563->mtimerwork))
		hrtimer_cancel(&pTAS2563->mtimerwork);

	if(pTAS2563->mnRestart < RESTART_MAX)
	{
		pTAS2563->mnRestart ++;
		msleep(100);
		dev_err(pTAS2563->dev, "I2C COMM error, restart SmartAmp.\n");
		schedule_delayed_work(&pTAS2563->irq_work, msecs_to_jiffies(100));
		return;
	}
	pTAS2563->enableIRQ(pTAS2563, false);
	tas2563_set_power_state(pTAS2563, TAS2563_POWER_SHUTDOWN);

	pTAS2563->mbPowerUp = false;
	pTAS2563->hw_reset(pTAS2563);
	pTAS2563->write(pTAS2563, TAS2563_SoftwareReset, TAS2563_SoftwareReset_SoftwareReset_Reset);
	udelay(1000);
	if (pTAS2563->mpFirmware != NULL)
		tas2563_clear_firmware(pTAS2563->mpFirmware);
}
#endif

/*
* tas2563_load_coefficient
*/
static int tas2563_load_coefficient(struct tas2563_priv *pTAS2563,
	int nPrevConfig, int nNewConfig, bool bPowerOn)
{
	int nResult = 0;
//	struct TPLL *pPLL;
	struct TProgram *pProgram;
	struct TConfiguration *pPrevConfiguration;
	struct TConfiguration *pNewConfiguration;
	bool bRestorePower = false;

	if (!pTAS2563->mpFirmware->mnConfigurations) {
		dev_err(pTAS2563->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	if (nNewConfig >= pTAS2563->mpFirmware->mnConfigurations) {
		dev_err(pTAS2563->dev, "%s, invalid configuration New=%d, total=%d\n",
			__func__, nNewConfig, pTAS2563->mpFirmware->mnConfigurations);
		goto end;
	}

	if (nPrevConfig < 0)
		pPrevConfiguration = NULL;
	else if (nPrevConfig == nNewConfig) {
		dev_info(pTAS2563->dev, "%s, config [%d] already loaded\n",
			__func__, nNewConfig);
		goto end;
	} else
		pPrevConfiguration = &(pTAS2563->mpFirmware->mpConfigurations[nPrevConfig]);

	pNewConfiguration = &(pTAS2563->mpFirmware->mpConfigurations[nNewConfig]);
	pTAS2563->mnCurrentConfiguration = nNewConfig;
	pTAS2563->mnCurrentSampleRate = pNewConfiguration->mnSamplingRate;

	dev_info(pTAS2563->dev, "load configuration %s conefficient pre block\n",
		pNewConfiguration->mpName);
	nResult = tas2563_load_data(pTAS2563, &(pNewConfiguration->mData), TAS2563_BLOCK_CFG_PRE_DEV_A);
	if (nResult < 0)
		goto end;

//prog_coefficient:
	dev_info(pTAS2563->dev, "load new configuration: %s, coeff block data\n",
		pNewConfiguration->mpName);
	nResult = tas2563_load_data(pTAS2563, &(pNewConfiguration->mData),
		TAS2563_BLOCK_CFG_COEFF_DEV_A);
	if (nResult < 0)
		goto end;

	if (pTAS2563->mpCalFirmware->mnCalibrations) {
		nResult = tas2563_set_calibration(pTAS2563, pTAS2563->mnCurrentCalibration);
		if (nResult < 0)
			goto end;
	}

	if (bRestorePower) {
		pTAS2563->clearIRQ(pTAS2563);
		dev_info(pTAS2563->dev, "device powered up, load startup\n");
		nResult = tas2563_set_power_state(pTAS2563, TAS2563_POWER_MUTE);
		if (nResult < 0)
			goto end;

		dev_info(pTAS2563->dev,
			"device powered up, load unmute\n");
		nResult = tas2563_set_power_state(pTAS2563, TAS2563_POWER_ACTIVE);
		if (nResult < 0)
			goto end;
		if (pProgram->mnAppMode == TAS2563_APP_TUNINGMODE) {
			pTAS2563->enableIRQ(pTAS2563, true);
			if (!hrtimer_active(&pTAS2563->mtimerwork)) {
				pTAS2563->mnDieTvReadCounter = 0;
				hrtimer_start(&pTAS2563->mtimerwork,
					ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
		}
	}
end:

	pTAS2563->mnNewConfiguration = pTAS2563->mnCurrentConfiguration;
	return nResult;
}

static bool tas2563_get_coefficient_in_data(struct tas2563_priv *pTAS2563,
	struct TData *pData, int blockType, int nReg, int *pnValue)
{
	bool bFound = false;
	struct TBlock *pBlock;
	int i;

	for (i = 0; i < pData->mnBlocks; i++) {
		pBlock = &(pData->mpBlocks[i]);
		if (pBlock->mnType == blockType) {
			bFound = tas2563_get_coefficient_in_block(pTAS2563,
						pBlock, nReg, pnValue);
			if (bFound)
				break;
		}
	}

	return bFound;
}

static bool tas2563_find_Tmax_in_configuration(struct tas2563_priv *pTAS2563,
	struct TConfiguration *pConfiguration, int *pnTMax)
{
	struct TData *pData;
	bool bFound = false;
	int nBlockType, nReg, nCoefficient;

	nReg = TAS2563_CALI_T_REG;

	nBlockType = TAS2563_BLOCK_CFG_COEFF_DEV_A;

	pData = &(pConfiguration->mData);
	bFound = tas2563_get_coefficient_in_data(pTAS2563, pData, nBlockType, nReg, &nCoefficient);
	if (bFound)
		*pnTMax = nCoefficient;

	return bFound;
}

void tas2563_fw_ready(const struct firmware *pFW, void *pContext)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *) pContext;
	int nResult;
	unsigned int nProgram = 0;
	unsigned int nSampleRate = 0;

#ifdef CONFIG_TAS2563_CODEC
	mutex_lock(&pTAS2563->codec_lock);
#endif

#ifdef CONFIG_TAS2563_MISC
	mutex_lock(&pTAS2563->file_lock);
#endif

	dev_info(pTAS2563->dev, "%s:\n", __func__);

	if (unlikely(!pFW) || unlikely(!pFW->data)) {
		dev_err(pTAS2563->dev, "%s firmware is not loaded.\n",
			TAS2563_FW_NAME);
		goto end;
	}

	if (pTAS2563->mpFirmware->mpConfigurations) {
		nProgram = pTAS2563->mnCurrentProgram;
		nSampleRate = pTAS2563->mnCurrentSampleRate;
		dev_info(pTAS2563->dev, "clear current firmware\n");
		tas2563_clear_firmware(pTAS2563->mpFirmware);
	}

	nResult = fw_parse(pTAS2563, pTAS2563->mpFirmware, (unsigned char *)(pFW->data), pFW->size);
	release_firmware(pFW);
	if (nResult < 0) {
		dev_err(pTAS2563->dev, "firmware is corrupt\n");
		goto end;
	}

	if (!pTAS2563->mpFirmware->mnPrograms) {
		dev_err(pTAS2563->dev, "firmware contains no programs\n");
		nResult = -EINVAL;
		goto end;
	}

	if (!pTAS2563->mpFirmware->mnConfigurations) {
		dev_err(pTAS2563->dev, "firmware contains no configurations\n");
		nResult = -EINVAL;
		goto end;
	}

	if (nProgram >= pTAS2563->mpFirmware->mnPrograms) {
		dev_info(pTAS2563->dev,
			"no previous program, set to default\n");
		nProgram = 0;
	}

	pTAS2563->mnCurrentSampleRate = nSampleRate;
	nResult = tas2563_set_program(pTAS2563, nProgram, -1);

end:

#ifdef CONFIG_TAS2563_CODEC
	mutex_unlock(&pTAS2563->codec_lock);
#endif

#ifdef CONFIG_TAS2563_MISC
	mutex_unlock(&pTAS2563->file_lock);
#endif
}

static bool tas2563_get_coefficient_in_block(struct tas2563_priv *pTAS2563,
	struct TBlock *pBlock, int nReg, int *pnValue)
{
	int nCoefficient = 0;
	bool bFound = false;
	unsigned char *pCommands;
	int nBook, nPage, nOffset, len;
	int i, n;

	pCommands = pBlock->mpData;
	for (i = 0 ; i < pBlock->mnCommands;) {
		nBook = pCommands[4 * i + 0];
		nPage = pCommands[4 * i + 1];
		nOffset = pCommands[4 * i + 2];
		if ((nOffset < 0x7f) || (nOffset == 0x81))
			i++;
		else if (nOffset == 0x85) {
			len = ((int)nBook << 8) | nPage;
			nBook = pCommands[4 * i + 4];
			nPage = pCommands[4 * i + 5];
			nOffset = pCommands[4 * i + 6];
			n = 4 * i + 7;
			i += 2;
			i += ((len - 1) / 4);
			if ((len - 1) % 4)
				i++;
			if ((nBook != TAS2563_BOOK_ID(nReg))
				|| (nPage != TAS2563_PAGE_ID(nReg)))
				continue;
			if (nOffset > TAS2563_PAGE_REG(nReg))
				continue;
			if ((len + nOffset) >= (TAS2563_PAGE_REG(nReg) + 4)) {
				n += (TAS2563_PAGE_REG(nReg) - nOffset);
				nCoefficient = ((int)pCommands[n] << 24)
						| ((int)pCommands[n + 1] << 16)
						| ((int)pCommands[n + 2] << 8)
						| (int)pCommands[n + 3];
				bFound = true;
				break;
			}
		} else {
			dev_err(pTAS2563->dev, "%s, format error %d\n", __func__, nOffset);
			break;
		}
	}

	if (bFound) {
		*pnValue = nCoefficient;
		dev_info(pTAS2563->dev, "%s, B[0x%x]P[0x%x]R[0x%x]=0x%x\n", __func__,
			TAS2563_BOOK_ID(nReg), TAS2563_PAGE_ID(nReg), TAS2563_PAGE_REG(nReg),
			nCoefficient);
	}

	return bFound;
}


int tas2563_set_program(struct tas2563_priv *pTAS2563,
	unsigned int nProgram, int nConfig)
{
	struct TProgram *pProgram;
	unsigned int nConfiguration = 0;
	unsigned int nSampleRate = 0;
	bool bFound = false;
	int nResult = 0;

	if ((!pTAS2563->mpFirmware->mpPrograms) ||
		(!pTAS2563->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2563->dev, "%s, Firmware not loaded\n", __func__);
		nResult = 0;
		goto end;
	}

	if (nProgram >= pTAS2563->mpFirmware->mnPrograms) {
		dev_err(pTAS2563->dev, "TAS2563: Program %d doesn't exist\n",
			nProgram);
		nResult = 0;
		goto end;
	}

	if (nConfig < 0) {
		nConfiguration = 0;
		nSampleRate = pTAS2563->mnCurrentSampleRate;
		while (!bFound && (nConfiguration < pTAS2563->mpFirmware->mnConfigurations)) {
			if (pTAS2563->mpFirmware->mpConfigurations[nConfiguration].mnProgram == nProgram) {
				if (nSampleRate == 0) {
					bFound = true;
					dev_info(pTAS2563->dev, "find default configuration %d\n", nConfiguration);
				} else if (nSampleRate == pTAS2563->mpFirmware->mpConfigurations[nConfiguration].mnSamplingRate) {
					bFound = true;
					dev_info(pTAS2563->dev, "find matching configuration %d\n", nConfiguration);
				} else {
					nConfiguration++;
				}
			} else {
				nConfiguration++;
			}
		}
		if (!bFound) {
			dev_err(pTAS2563->dev,
				"Program %d, no valid configuration found for sample rate %d, ignore\n",
				nProgram, nSampleRate);
			nResult = 0;
			goto end;
		}
	} else {
		if (pTAS2563->mpFirmware->mpConfigurations[nConfig].mnProgram != nProgram) {
			dev_err(pTAS2563->dev, "%s, configuration program doesn't match\n", __func__);
			nResult = 0;
			goto end;
		}
		nConfiguration = nConfig;
	}

	pProgram = &(pTAS2563->mpFirmware->mpPrograms[nProgram]);
	if (pTAS2563->mbPowerUp) {
		dev_info(pTAS2563->dev,
			"device powered up, power down to load program %d (%s)\n",
			nProgram, pProgram->mpName);
		if (hrtimer_active(&pTAS2563->mtimerwork))
			hrtimer_cancel(&pTAS2563->mtimerwork);

		if (pProgram->mnAppMode == TAS2563_APP_TUNINGMODE)
			pTAS2563->enableIRQ(pTAS2563, false);

		nResult = tas2563_set_power_state(pTAS2563, TAS2563_POWER_SHUTDOWN);
		if (nResult < 0)
			goto end;
	}

	pTAS2563->hw_reset(pTAS2563);
	nResult = pTAS2563->write(pTAS2563, TAS2563_SoftwareReset, 0x01);
	if (nResult < 0)
		goto end;
	msleep(1);

	dev_info(pTAS2563->dev, "load program %d (%s)\n", nProgram, pProgram->mpName);
	nResult = tas2563_load_data(pTAS2563, &(pProgram->mData), TAS2563_BLOCK_PGM_DEV_A);
	if (nResult < 0)
		goto end;
	pTAS2563->mnCurrentProgram = nProgram;

	nResult = tas2563_load_coefficient(pTAS2563, -1, nConfiguration, false);
	if (nResult < 0)
		goto end;

	nResult = tas2563_load_default(pTAS2563);
	if (nResult < 0)
		goto end;

	// Enable IV data
	nResult = pTAS2563->update_bits(pTAS2563, TAS2563_PowerControl,
				TAS2563_PowerControl_ISNSPower_Mask |
				TAS2563_PowerControl_VSNSPower_Mask,
				TAS2563_PowerControl_VSNSPower_Active |
				TAS2563_PowerControl_ISNSPower_Active);
	if (nResult < 0)
		dev_info(pTAS2563->dev, "Enable IV Data Failed: %s\n", __func__);

	if (pTAS2563->mbPowerUp) {
//		pTAS2563->clearIRQ(pTAS2563);
		dev_info(pTAS2563->dev, "device powered up, load startup\n");
		nResult = tas2563_set_power_state(pTAS2563, TAS2563_POWER_MUTE);

		if (nResult < 0)
			goto end;
		if (pProgram->mnAppMode == TAS2563_APP_TUNINGMODE) {
			if (nResult < 0) {
				tas2563_set_power_state(pTAS2563, TAS2563_POWER_SHUTDOWN);
				pTAS2563->mbPowerUp = false;
				goto end;
			}
		}
		dev_info(pTAS2563->dev, "device powered up, load unmute\n");
		tas2563_set_power_state(pTAS2563, TAS2563_POWER_ACTIVE);
		if (nResult < 0)
			goto end;

		if (pProgram->mnAppMode == TAS2563_APP_TUNINGMODE) {
			pTAS2563->enableIRQ(pTAS2563, true);
			if (!hrtimer_active(&pTAS2563->mtimerwork)) {
				pTAS2563->mnDieTvReadCounter = 0;
				hrtimer_start(&pTAS2563->mtimerwork,
					ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
		}
	}

end:

/*	if (nResult < 0) {
		if (pTAS2563->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK))
			failsafe(pTAS2563);
	}*/
	return nResult;
}

static int tas2563_set_calibration(struct tas2563_priv *pTAS2563, int nCalibration)
{
	struct TCalibration *pCalibration = NULL;
	struct TConfiguration *pConfiguration;
	struct TProgram *pProgram;
	int nTmax = 0;
	bool bFound = false;
	int nResult = 0;

	if ((!pTAS2563->mpFirmware->mpPrograms)
		|| (!pTAS2563->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2563->dev, "%s, Firmware not loaded\n\r", __func__);
		nResult = 0;
		goto end;
	}

	if (nCalibration == 0x00FF) {
		nResult = tas2563_load_calibration(pTAS2563, TAS2563_CAL_NAME);
		if (nResult < 0) {
			dev_info(pTAS2563->dev, "load new calibration file %s fail %d\n",
				TAS2563_CAL_NAME, nResult);
			goto end;
		}
		nCalibration = 0;
	}

	if (nCalibration >= pTAS2563->mpCalFirmware->mnCalibrations) {
		dev_err(pTAS2563->dev,
			"Calibration %d doesn't exist\n", nCalibration);
		nResult = 0;
		goto end;
	}

	pTAS2563->mnCurrentCalibration = nCalibration;
	if (pTAS2563->mbLoadConfigurationPrePowerUp)
		goto end;

	pCalibration = &(pTAS2563->mpCalFirmware->mpCalibrations[nCalibration]);
	pProgram = &(pTAS2563->mpFirmware->mpPrograms[pTAS2563->mnCurrentProgram]);
	pConfiguration = &(pTAS2563->mpFirmware->mpConfigurations[pTAS2563->mnCurrentConfiguration]);
	if (pProgram->mnAppMode == TAS2563_APP_TUNINGMODE) {
		if (pTAS2563->mbBypassTMax) {
			bFound = tas2563_find_Tmax_in_configuration(pTAS2563, pConfiguration, &nTmax);
			if (bFound && (nTmax == TAS2563_COEFFICIENT_TMAX)) {
				dev_info(pTAS2563->dev, "%s, config[%s] bypass load calibration\n",
					__func__, pConfiguration->mpName);
				goto end;
			}
		}

		dev_info(pTAS2563->dev, "%s, load calibration\n", __func__);
		nResult = tas2563_load_data(pTAS2563, &(pCalibration->mData), TAS2563_BLOCK_CFG_COEFF_DEV_A);
		if (nResult < 0)
			goto end;
	}

end:
	if (nResult < 0) {
		tas2563_clear_firmware(pTAS2563->mpCalFirmware);
		nResult = tas2563_set_program(pTAS2563, pTAS2563->mnCurrentProgram, pTAS2563->mnCurrentConfiguration);
	}

	return nResult;
}

bool tas2563_get_Cali_prm_r0(struct tas2563_priv *pTAS2563, int *prm_r0)
{
	struct TCalibration *pCalibration;
	struct TData *pData;
	int nReg;
	int nCali_Re;
	bool bFound = false;
	int nBlockType;

	if (!pTAS2563->mpCalFirmware->mnCalibrations) {
		dev_err(pTAS2563->dev, "%s, no calibration data\n", __func__);
		goto end;
	}

	nReg = TAS2563_CALI_R0_REG;
	nBlockType = TAS2563_BLOCK_CFG_COEFF_DEV_A;

	pCalibration = &(pTAS2563->mpCalFirmware->mpCalibrations[pTAS2563->mnCurrentCalibration]);
	pData = &(pCalibration->mData);

	bFound = tas2563_get_coefficient_in_data(pTAS2563, pData, nBlockType, nReg, &nCali_Re);

end:

	if (bFound)
		*prm_r0 = nCali_Re;

	return bFound;
}

int tas2563_set_config(struct tas2563_priv *pTAS2563, int config)
{
	struct TConfiguration *pConfiguration;
	struct TProgram *pProgram;
	unsigned int nProgram = pTAS2563->mnCurrentProgram;
	unsigned int nConfiguration = config;
	int nResult = 0;

	if ((!pTAS2563->mpFirmware->mpPrograms) ||
		(!pTAS2563->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2563->dev, "%s, Firmware not loaded\n", __func__);
		nResult = -EINVAL;
		goto end;
	}

	if (nConfiguration >= pTAS2563->mpFirmware->mnConfigurations) {
		dev_err(pTAS2563->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		nResult = -EINVAL;
		goto end;
	}

	pConfiguration = &(pTAS2563->mpFirmware->mpConfigurations[nConfiguration]);
	pProgram = &(pTAS2563->mpFirmware->mpPrograms[nProgram]);

	if (nProgram != pConfiguration->mnProgram) {
		dev_err(pTAS2563->dev,
			"Configuration %d, %s with Program %d isn't compatible with existing Program %d, %s\n",
			nConfiguration, pConfiguration->mpName, pConfiguration->mnProgram,
			nProgram, pProgram->mpName);
		nResult = -EINVAL;
		goto end;
	}

	nResult = tas2563_load_configuration(pTAS2563, nConfiguration, false);

end:

	return nResult;
}

static int tas2563_configuration_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2563->codec_lock);

	pValue->value.integer.value[0] = pTAS2563->mnCurrentConfiguration;
	dev_info(pTAS2563->dev, "tas2563_configuration_get = %d\n",
		pTAS2563->mnCurrentConfiguration);

	mutex_unlock(&pTAS2563->codec_lock);
	return 0;
}

static int tas2563_configuration_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	unsigned int nConfiguration = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2563->codec_lock);

	dev_info(pTAS2563->dev, "%s = %d\n", __func__, nConfiguration);
	ret = tas2563_set_config(pTAS2563, nConfiguration);

	mutex_unlock(&pTAS2563->codec_lock);
	return ret;
}


static int tas2563_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	int nResult = 0;
	int blr_clk_ratio;

	dev_info(pTAS2563->dev, "%s, format: %d\n", __func__,
		params_format(params));

	mutex_lock(&pTAS2563->codec_lock);

	nResult = tas2563_set_bitwidth(pTAS2563, params_format(params));
	if(nResult < 0)
	{
		dev_info(pTAS2563->dev, "set bitwidth failed, %d\n", nResult);
		goto ret;
	}

	blr_clk_ratio = params_channels(params) * pTAS2563->mnCh_size;
	dev_info(pTAS2563->dev, "blr_clk_ratio: %d\n", blr_clk_ratio);
	if(blr_clk_ratio != 0)
		tas2563_slot_config(pTAS2563->codec, pTAS2563, blr_clk_ratio);

	dev_info(pTAS2563->dev, "%s, sample rate: %d\n", __func__,
		params_rate(params));

	nResult = tas2563_set_samplerate(pTAS2563, params_rate(params));

ret:
	mutex_unlock(&pTAS2563->codec_lock);
	return nResult;
}

static int tas2563_set_fmt(struct tas2563_priv *pTAS2563, unsigned int fmt)
{
	u8 tdm_rx_start_slot = 0, asi_cfg_1 = 0;
	int ret = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		asi_cfg_1 = 0x00;
		break;
	default:
		dev_err(pTAS2563->dev, "ASI format master is not found\n");
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		dev_info(pTAS2563->dev, "INV format: NBNF\n");
		asi_cfg_1 |= TAS2563_TDMConfigurationReg1_RXEDGE_Rising;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		dev_info(pTAS2563->dev, "INV format: IBNF\n");
		asi_cfg_1 |= TAS2563_TDMConfigurationReg1_RXEDGE_Falling;
		break;
	default:
		dev_err(pTAS2563->dev, "ASI format Inverse is not found\n");
		ret = -EINVAL;
	}

	pTAS2563->update_bits(pTAS2563, TAS2563_TDMConfigurationReg1,
		TAS2563_TDMConfigurationReg1_RXEDGE_Mask,
		asi_cfg_1);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case (SND_SOC_DAIFMT_I2S):
		tdm_rx_start_slot = 1;
		break;
	case (SND_SOC_DAIFMT_DSP_A):
	case (SND_SOC_DAIFMT_DSP_B):
		tdm_rx_start_slot = 1;
		break;
	case (SND_SOC_DAIFMT_LEFT_J):
		tdm_rx_start_slot = 0;
		break;
	default:
	dev_err(pTAS2563->dev, "DAI Format is not found, fmt=0x%x\n", fmt);
	ret = -EINVAL;
		break;
	}

	pTAS2563->update_bits(pTAS2563, TAS2563_TDMConfigurationReg1,
		TAS2563_TDMConfigurationReg1_RXOFFSET51_Mask,
	(tdm_rx_start_slot << TAS2563_TDMConfigurationReg1_RXOFFSET51_Shift));

	pTAS2563->mnASIFormat = fmt;

	return 0;
}


static int tas2563_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_info(pTAS2563->dev, "%s, format=0x%x\n", __func__, fmt);

	mutex_lock(&pTAS2563->codec_lock);

	ret = tas2563_set_fmt(pTAS2563, fmt);

	mutex_unlock(&pTAS2563->codec_lock);
	return ret;
}

static int tas2563_set_dai_tdm_slot(struct snd_soc_dai *dai,
		unsigned int tx_mask, unsigned int rx_mask,
		int slots, int slot_width)
{
	int ret = 0;
	struct snd_soc_codec *codec = dai->codec;
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

	dev_info(pTAS2563->dev, "%s, tx_mask:%d, rx_mask:%d, slots:%d, slot_width:%d",
			__func__, tx_mask, rx_mask, slots, slot_width);

	ret = tas2563_set_slot(pTAS2563, slot_width);

	return ret;
}

static int tas2563_program_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2563->codec_lock);

	pValue->value.integer.value[0] = pTAS2563->mnCurrentProgram;
	dev_info(pTAS2563->dev, "tas2563_program_get = %d\n",
		pTAS2563->mnCurrentProgram);

	mutex_unlock(&pTAS2563->codec_lock);
	return 0;
}

static int tas2563_program_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	unsigned int nProgram = pValue->value.integer.value[0];
	int ret = 0, nConfiguration = -1;

	mutex_lock(&pTAS2563->codec_lock);

	if (nProgram == pTAS2563->mnCurrentProgram)
		nConfiguration = pTAS2563->mnCurrentConfiguration;
	ret = tas2563_set_program(pTAS2563, nProgram, nConfiguration);

	mutex_unlock(&pTAS2563->codec_lock);
	return ret;
}

static int tas2563_calibration_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2563->codec_lock);

	pValue->value.integer.value[0] = pTAS2563->mnCurrentCalibration;
	dev_info(pTAS2563->dev,
		"tas2563_calibration_get = %d\n",
		pTAS2563->mnCurrentCalibration);

	mutex_unlock(&pTAS2563->codec_lock);
	return 0;
}

static int tas2563_calibration_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);
	unsigned int nCalibration = pValue->value.integer.value[0];
	int ret = 0;

	mutex_lock(&pTAS2563->codec_lock);

	ret = tas2563_set_calibration(pTAS2563, nCalibration);

	mutex_unlock(&pTAS2563->codec_lock);
	return ret;
}

static struct snd_soc_dai_ops tas2563_dai_ops = {
	.digital_mute = tas2563_mute,
	.hw_params  = tas2563_hw_params,
	.set_fmt    = tas2563_set_dai_fmt,
	.set_tdm_slot = tas2563_set_dai_tdm_slot,
};

#define TAS2563_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define TAS2563_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 \
						SNDRV_PCM_RATE_88200 |\
						SNDRV_PCM_RATE_96000 |\
						SNDRV_PCM_RATE_176400 |\
						SNDRV_PCM_RATE_192000\
						)

static struct snd_soc_dai_driver tas2563_dai_driver[] = {
	{
		.name = "tas2563 ASI1",
		.id = 0,
		.playback = {
			.stream_name    = "ASI1 Playback",
			.channels_min   = 2,
			.channels_max   = 2,
			.rates      = SNDRV_PCM_RATE_8000_192000,
			.formats    = TAS2563_FORMATS,
		},
		.capture = {
			.stream_name    = "ASI1 Capture",
			.channels_min   = 0,
			.channels_max   = 2,
			.rates          = SNDRV_PCM_RATE_8000_192000,
			.formats    = TAS2563_FORMATS,
		},
		.ops = &tas2563_dai_ops,
		.symmetric_rates = 1,
	},
};

static int tas2563_codec_probe(struct snd_soc_codec *codec)
{
	struct tas2563_priv *pTAS2563 = snd_soc_codec_get_drvdata(codec);

	pTAS2563->codec = codec;
	pTAS2563->set_calibration = tas2563_set_calibration;
	pTAS2563->set_config = tas2563_set_config;

	dev_err(pTAS2563->dev, "%s\n", __func__);

	return 0;
}

static int tas2563_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

/*static DECLARE_TLV_DB_SCALE(dac_tlv, 0, 100, 0);*/
static DECLARE_TLV_DB_SCALE(tas2563_digital_tlv, 1100, 50, 0);

static const struct snd_kcontrol_new tas2563_snd_controls[] = {
	SOC_SINGLE_TLV("Amp Output Level", TAS2563_PlaybackConfigurationReg0,
		0, 0x16, 0,
		tas2563_digital_tlv),
	SOC_SINGLE_EXT("Program", SND_SOC_NOPM, 0, 0x00FF, 0, tas2563_program_get,
		tas2563_program_put),
	SOC_SINGLE_EXT("Configuration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2563_configuration_get, tas2563_configuration_put),
	SOC_SINGLE_EXT("Calibration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2563_calibration_get, tas2563_calibration_put),
};

static struct snd_soc_codec_driver soc_codec_driver_tas2563 = {
	.probe			= tas2563_codec_probe,
	.remove			= tas2563_codec_remove,
	.read			= tas2563_codec_read,
	.write			= tas2563_codec_write,
	.suspend		= tas2563_codec_suspend,
	.resume			= tas2563_codec_resume,
#ifdef KCONTROL_CODEC
	.component_driver = {
#endif
		.controls		= tas2563_snd_controls,
		.num_controls		= ARRAY_SIZE(tas2563_snd_controls),
		.dapm_widgets		= tas2563_dapm_widgets,
		.num_dapm_widgets	= ARRAY_SIZE(tas2563_dapm_widgets),
		.dapm_routes		= tas2563_audio_map,
		.num_dapm_routes	= ARRAY_SIZE(tas2563_audio_map),
#ifdef KCONTROL_CODEC
	},
#endif
};

int tas2563_register_codec(struct tas2563_priv *pTAS2563)
{
	int nResult = 0;

	dev_info(pTAS2563->dev, "%s, enter\n", __func__);
	nResult = snd_soc_register_codec(pTAS2563->dev,
		&soc_codec_driver_tas2563,
		tas2563_dai_driver, ARRAY_SIZE(tas2563_dai_driver));
	return nResult;
}

int tas2563_deregister_codec(struct tas2563_priv *pTAS2563)
{
	snd_soc_unregister_codec(pTAS2563->dev);

	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2563 ALSA SOC Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif /* CONFIG_TAS2563_CODEC */
