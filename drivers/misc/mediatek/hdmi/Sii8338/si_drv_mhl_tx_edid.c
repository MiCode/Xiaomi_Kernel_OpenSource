#ifndef __KERNEL__
#include <string.h>
#include "si_common.h"
#else
#include <linux/string.h>
#endif
#include "si_platform.h"
#include "si_cra.h"
#include "si_cra_cfg.h"
#include "si_8338_regs.h"
#include "si_drv_mhl_tx_edid.h"
#include "si_mhl_tx_api.h"

uint8_t VIDEO_CAPABILITY_D_BLOCK_found = false;
uint8_t prefertiming;

#define GLOBAL_BYTE_BUF_BLOCK_SIZE  131
uint8_t PLACE_IN_DATA_SEG g_CommData[GLOBAL_BYTE_BUF_BLOCK_SIZE];
static uint8_t ParseEDID(uint8_t *pEdid, uint8_t *numExt);
static uint8_t Parse861ShortDescriptors(uint8_t *Data);
static uint8_t Parse861LongDescriptors(uint8_t *Data);
static bool_t GetBIT2(uint8_t *reg_val);
static bool_t ReleaseDDC(uint8_t reg_val);
#define BITS_2_1                0x06
#define TWO_LSBITS              0x03
#define THREE_LSBITS            0x07
#define FOUR_LSBITS             0x0F
#define FIVE_LSBITS             0x1F
#define TWO_MSBITS              0xC0
#define T_BIT2    50
#define EDID_BLOCK_0_OFFSET 0x0000
#define EDID_BLOCK_1_OFFSET 0x0080
#define EDID_BLOCK_SIZE      128
#define EDID_HDR_NO_OF_FF   0x06
#define NUM_OF_EXTEN_ADDR   0x7E
#define EDID_TAG_ADDR       0x00
#define EDID_REV_ADDR       0x01
#define EDID_TAG_IDX        0x02
#define LONG_DESCR_PTR_IDX  0x02
#define MISC_SUPPORT_IDX    0x03
#define ESTABLISHED_TIMING_INDEX        35
#define NUM_OF_STANDARD_TIMINGS          8
#define STANDARD_TIMING_OFFSET          38
#define LONG_DESCR_LEN                  18
#define NUM_OF_DETAILED_DESCRIPTORS      4
#define DETAILED_TIMING_OFFSET        0x36
#define PIX_CLK_OFFSET                   0
#define H_ACTIVE_OFFSET                  2
#define H_BLANKING_OFFSET                3
#define V_ACTIVE_OFFSET                  5
#define V_BLANKING_OFFSET                6
#define H_SYNC_OFFSET                    8
#define H_SYNC_PW_OFFSET                 9
#define V_SYNC_OFFSET                   10
#define V_SYNC_PW_OFFSET                10
#define H_IMAGE_SIZE_OFFSET             12
#define V_IMAGE_SIZE_OFFSET             13
#define H_BORDER_OFFSET                 15
#define V_BORDER_OFFSET                 16
#define FLAGS_OFFSET                    17
#define AR16_10                          0
#define AR4_3                            1
#define AR5_4                            2
#define AR16_9                           3
#define AUDIO_D_BLOCK       0x01
#define VIDEO_D_BLOCK       0x02
#define VENDOR_SPEC_D_BLOCK 0x03
#define SPKR_ALLOC_D_BLOCK  0x04
#define USE_EXTENDED_TAG    0x07
#define COLORIMETRY_D_BLOCK 0x05
#define HDMI_SIGNATURE_LEN  0x03
#define CEC_PHYS_ADDR_LEN   0x02
#define EDID_EXTENSION_TAG  0x02
#define EDID_REV_THREE      0x03
#define EDID_DATA_START     0x04
#define EDID_BLOCK_0        0x00
#define EDID_BLOCK_2_3      0x01
#define VIDEO_CAPABILITY_D_BLOCK 0x00
#define ReadSegmentBlockEDID(a, b, c, d)   I2C_ReadSegmentBlock(0xA0, a, b, d, c)
Type_EDID_Descriptors EDID_Data = {
	{0},
	{{0}, {0}, {0} },
	{0},
	false, false, false, false, false, 0, 0, 0, 0, false
};

static bool_t DoEDID_Checksum(uint8_t *);
static bool_t CheckEDID_Header(uint8_t *);
static void ParseBlock_0_TimingDescripors(uint8_t *);
static uint8_t Parse861Extensions(uint8_t);
static uint8_t Parse861ShortDescriptors(uint8_t *);
static uint8_t Parse861LongDescriptors(uint8_t *);
#ifdef CONF__TPI_EDID_PRINT
static void ParseEstablishedTiming(uint8_t *);
static bool_t ParseDetailedTiming(uint8_t *, uint8_t, uint8_t);
static void ParseStandardTiming(uint8_t *);
#endif
uint8_t ParseEDID(uint8_t *pEdid, uint8_t *numExt)
{
	uint8_t i, j, k;
	/* uint16_t OldDebugFormat; */
	TX_EDID_PRINT(("EDID DATA (Segment = 0 Block = 0 Offset = %d):\n",
		       (int)EDID_BLOCK_0_OFFSET));
#if 1
	/* OldDebugFormat = SiiOsDebugGetConfig(); */
	/* SiiOsDebugSetConfig(SII_OS_DEBUG_FORMAT_SIMPLE); */
	for (j = 0, i = 0; j < 128; j++) {
		k = pEdid[j];
		TX_EDID_PRINT(("%02X ", (int)k));
		i++;
		if (i == 0x10) {
			TX_EDID_PRINT(("\n"));
			i = 0;
		}
	}
	TX_EDID_PRINT(("\n"));
	/* SiiOsDebugSetConfig(OldDebugFormat); */
#endif
	if (!CheckEDID_Header(pEdid)) {
		TX_DEBUG_PRINT(("EDID -> Incorrect Header\n"));
		return EDID_INCORRECT_HEADER;
	}
	if (!DoEDID_Checksum(pEdid)) {
		TX_DEBUG_PRINT(("EDID -> Checksum Error\n"));
		return EDID_CHECKSUM_ERROR;
	}
	ParseBlock_0_TimingDescripors(pEdid);
	*numExt = pEdid[NUM_OF_EXTEN_ADDR];
	TX_EDID_PRINT(("EDID -> Now 861 Extensions = %d\n", (int)*numExt));
	if (!(*numExt)) {
		return EDID_NO_861_EXTENSIONS;
	}
	return (EDID_OK);
}

uint8_t SiiDrvMhlTxReadEdid(void)
{
	uint8_t Result = EDID_OK;
	uint8_t NumOfExtensions;
	uint8_t reg_val;
	if (GetBIT2(&reg_val)) {
		memset(g_CommData, 0x00, GLOBAL_BYTE_BUF_BLOCK_SIZE);
		SiiRegEdidReadBlock(TX_PAGE_DDC_SEGM | 0x0000,
				    TX_PAGE_DDC_EDID | EDID_BLOCK_0_OFFSET, g_CommData,
				    EDID_BLOCK_SIZE);
		Result = ParseEDID(g_CommData, &NumOfExtensions);
		if (Result != EDID_OK) {
			if (Result == EDID_NO_861_EXTENSIONS) {
				EDID_Data.HDMI_Sink = false;
				EDID_Data.YCbCr_4_4_4 = false;
				EDID_Data.YCbCr_4_2_2 = false;
				EDID_Data.CEC_A_B = 0x00;
				EDID_Data.CEC_C_D = 0x00;
				TX_DEBUG_PRINT(("EDID -> No 861 Extensions, NOT HDMI Sink device\n"));
			} else {
				TX_DEBUG_PRINT(("EDID -> Parse FAILED\n"));
				EDID_Data.HDMI_Sink = true;
				EDID_Data.YCbCr_4_4_4 = false;
				EDID_Data.YCbCr_4_2_2 = false;
				EDID_Data.CEC_A_B = 0x00;
				EDID_Data.CEC_C_D = 0x00;
			}
		} else {
			TX_DEBUG_PRINT(("EDID -> Parse OK\n"));
			Result = Parse861Extensions(NumOfExtensions);
			if (Result != EDID_OK) {
				TX_DEBUG_PRINT(("EDID -> Extension Parse FAILED\n"));
				EDID_Data.HDMI_Sink = true;
				EDID_Data.YCbCr_4_4_4 = false;
				EDID_Data.YCbCr_4_2_2 = false;
				EDID_Data.CEC_A_B = 0x00;
				EDID_Data.CEC_C_D = 0x00;
			}
		}
		if (!ReleaseDDC(reg_val)) {
			TX_DEBUG_PRINT(("EDID -> DDC bus release failed\n"));
			return EDID_DDC_BUS_RELEASE_FAILURE;
		}
		TX_DEBUG_PRINT(("EDID_Data.HDMI_Sink = %d\n", (int)EDID_Data.HDMI_Sink));
		TX_DEBUG_PRINT(("EDID_Data.YCbCr_4_4_4 = %d\n", (int)EDID_Data.YCbCr_4_4_4));
		TX_DEBUG_PRINT(("EDID_Data.YCbCr_4_2_2 = %d\n", (int)EDID_Data.YCbCr_4_2_2));
		TX_DEBUG_PRINT(("EDID_Data.CEC_A_B = 0x%x\n", (int)EDID_Data.CEC_A_B));
		TX_DEBUG_PRINT(("EDID_Data.CEC_C_D = 0x%x\n", (int)EDID_Data.CEC_C_D));
	} else {
		TX_DEBUG_PRINT(("EDID -> DDC bus request failed\n"));
		EDID_Data.HDMI_Sink = true;
		EDID_Data.YCbCr_4_4_4 = false;
		EDID_Data.YCbCr_4_2_2 = false;
		EDID_Data.CEC_A_B = 0x00;
		EDID_Data.CEC_C_D = 0x00;
		return EDID_DDC_BUS_REQ_FAILURE;
	}
	return Result;
}

static void ParseBlock_0_TimingDescripors(uint8_t *Data)
{
#ifdef CONF__TPI_EDID_PRINT
	uint8_t i;
	uint8_t Offset;
	ParseEstablishedTiming(Data);
	ParseStandardTiming(Data);
	for (i = 0; i < NUM_OF_DETAILED_DESCRIPTORS; i++) {
		Offset = DETAILED_TIMING_OFFSET + (LONG_DESCR_LEN * i);
		ParseDetailedTiming(Data, Offset, EDID_BLOCK_0);
	}
#else
	Data = Data;
#endif
}

static uint8_t Parse861Extensions(uint8_t NumOfExtensions)
{
	uint8_t i, j, k;
	/* uint16_t OldDebugFormat; */
	uint8_t ErrCode;
	uint8_t Segment = 0;
	uint8_t Block = 0;
	uint8_t Offset = 0;
	EDID_Data.HDMI_Sink = false;
	do {
		Block++;
		Offset = 0;
		if ((Block % 2) > 0) {
			Offset = EDID_BLOCK_SIZE;
		}
		Segment = (uint8_t) (Block / 2);
		SiiRegEdidReadBlock(TX_PAGE_DDC_SEGM | Segment, TX_PAGE_DDC_EDID | Offset,
				    g_CommData, EDID_BLOCK_SIZE);
		TX_EDID_PRINT(("EDID DATA (Segment = %d Block = %d Offset = %d):\n", (int)Segment,
			       (int)Block, (int)Offset));
#if 1
		/* OldDebugFormat = SiiOsDebugGetConfig(); */
		/* SiiOsDebugSetConfig(SII_OS_DEBUG_FORMAT_SIMPLE); */
		for (j = 0, i = 0; j < 128; j++) {
			k = g_CommData[j];
			TX_EDID_PRINT(("%2.2X ", (int)k));
			i++;
			if (i == 0x10) {
				TX_EDID_PRINT(("\n"));
				i = 0;
			}
		}
		TX_EDID_PRINT(("\n"));
		/* SiiOsDebugSetConfig(OldDebugFormat); */
#endif
		if ((NumOfExtensions > 1) && (Block == 1)) {
			continue;
		}
		if (!DoEDID_Checksum(g_CommData)) {
			TX_DEBUG_PRINT(("EDID(%d) -> Checksum Error\n", (int)Block));
			return EDID_CHECKSUM_ERROR;
		}
		ErrCode = Parse861ShortDescriptors(g_CommData);
		if (ErrCode != EDID_SHORT_DESCRIPTORS_OK) {
			TX_EDID_PRINT((" not EDID_SHORT_DESCRIPTORS_OK"));
			return ErrCode;
		}
		ErrCode = Parse861LongDescriptors(g_CommData);
		if (ErrCode != EDID_LONG_DESCRIPTORS_OK) {
			TX_EDID_PRINT((" not EDID_LONG_DESCRIPTORS_OK"));
			return ErrCode;
		}
	} while (Block < NumOfExtensions);
	return EDID_OK;
}

uint8_t Parse861ShortDescriptors(uint8_t *Data)
{
	uint8_t LongDescriptorOffset;
	uint8_t DataBlockLength;
	uint8_t DataIndex;
	uint8_t ExtendedTagCode;
	uint8_t VSDB_BaseOffset = 0;
	uint8_t V_DescriptorIndex = 0;
	uint8_t A_DescriptorIndex = 0;
	uint8_t TagCode;
	uint8_t i;
	uint8_t j;

	VIDEO_CAPABILITY_D_BLOCK_found = false;

	if (Data[EDID_TAG_ADDR] != EDID_EXTENSION_TAG) {
		TX_EDID_PRINT(("EDID -> Extension Tag Error\n"));
		return EDID_EXT_TAG_ERROR;
	}
	if (Data[EDID_REV_ADDR] != EDID_REV_THREE) {
		TX_EDID_PRINT(("EDID -> Revision Error\n"));
		return EDID_REV_ADDR_ERROR;
	}
	LongDescriptorOffset = Data[LONG_DESCR_PTR_IDX];
	EDID_Data.UnderScan = ((Data[MISC_SUPPORT_IDX]) >> 7) & BIT0;
	EDID_Data.BasicAudio = ((Data[MISC_SUPPORT_IDX]) >> 6) & BIT0;
	EDID_Data.YCbCr_4_4_4 = ((Data[MISC_SUPPORT_IDX]) >> 5) & BIT0;
	EDID_Data.YCbCr_4_2_2 = ((Data[MISC_SUPPORT_IDX]) >> 4) & BIT0;
	DataIndex = EDID_DATA_START;
	while (DataIndex < LongDescriptorOffset) {
		TagCode = (Data[DataIndex] >> 5) & THREE_LSBITS;
		DataBlockLength = Data[DataIndex++] & FIVE_LSBITS;
		if ((DataIndex + DataBlockLength) > LongDescriptorOffset) {
			TX_EDID_PRINT(("EDID -> V Descriptor Overflow\n"));
			return EDID_V_DESCR_OVERFLOW;
		}
		i = 0;
		switch (TagCode) {
		case VIDEO_D_BLOCK:
			while ((i < DataBlockLength) && (i < MAX_V_DESCRIPTORS)) {
				EDID_Data.VideoDescriptor[V_DescriptorIndex++] = Data[DataIndex++];
				i++;
			}
			DataIndex += DataBlockLength - i;
			TX_EDID_PRINT(("EDID -> Short Descriptor Video Block\n"));
			prefertiming = EDID_Data.VideoDescriptor[0] && 0x7F;	/* use 1st mode supported by sink */
			break;
		case AUDIO_D_BLOCK:
			while (i < DataBlockLength / 3) {
				j = 0;
				while (j < AUDIO_DESCR_SIZE) {
					EDID_Data.AudioDescriptor[A_DescriptorIndex][j++] =
					    Data[DataIndex++];
				}
				A_DescriptorIndex++;
				i++;
			}
			TX_EDID_PRINT(("EDID -> Short Descriptor Audio Block\n"));
			break;
		case SPKR_ALLOC_D_BLOCK:
			EDID_Data.SpkrAlloc[i++] = Data[DataIndex++];
			DataIndex += 2;
			TX_EDID_PRINT(("EDID -> Short Descriptor Speaker Allocation Block\n"));
			break;
		case USE_EXTENDED_TAG:
			ExtendedTagCode = Data[DataIndex++];

			switch (ExtendedTagCode) {
			case VIDEO_CAPABILITY_D_BLOCK:
				VIDEO_CAPABILITY_D_BLOCK_found = true;
				TX_EDID_PRINT(("EDID -> Short Descriptor Video Capability Block, IDEO_CAPABILITY_D_BLOCK_found =true\n"));
				DataIndex += 1;
				break;
			case COLORIMETRY_D_BLOCK:
				EDID_Data.ColorimetrySupportFlags = Data[DataIndex++] & TWO_LSBITS;
				EDID_Data.MetadataProfile = Data[DataIndex++] & THREE_LSBITS;
				TX_EDID_PRINT(("EDID -> Short Descriptor Colorimetry Block\n"));
				break;
			}
			break;
		case VENDOR_SPEC_D_BLOCK:
			VSDB_BaseOffset = DataIndex - 1;
			if ((Data[DataIndex++] == 0x03) &&
			    (Data[DataIndex++] == 0x0C) && (Data[DataIndex++] == 0x00))
				EDID_Data.HDMI_Sink = true;
			else
				EDID_Data.HDMI_Sink = false;
			EDID_Data.CEC_A_B = Data[DataIndex++];
			EDID_Data.CEC_C_D = Data[DataIndex++];
#if 0
#if (IS_CEC == 1)
			{
				uint16_t phyAddr;
				phyAddr = (uint16_t) EDID_Data.CEC_C_D;
				phyAddr |= ((uint16_t) EDID_Data.CEC_A_B << 8);
				if (phyAddr != SI_CecGetDevicePA()) {
					SI_CecSetDevicePA(phyAddr);
#if (IS_CDC == 1)
					CpCdcInit();
#endif
				}
			}
#endif
#endif
			if ((DataIndex + 7) > VSDB_BaseOffset + DataBlockLength)
				EDID_Data._3D_Supported = false;
			else if (Data[DataIndex + 7] >> 7)
				EDID_Data._3D_Supported = true;
			else
				EDID_Data._3D_Supported = false;
			DataIndex += DataBlockLength - HDMI_SIGNATURE_LEN - CEC_PHYS_ADDR_LEN;
			TX_EDID_PRINT(("EDID -> Short Descriptor Vendor Block\n\n"));
			break;
		default:
			TX_EDID_PRINT(("EDID -> Unknown Tag Code\n"));
			return EDID_UNKNOWN_TAG_CODE;
		}
	}
	return EDID_SHORT_DESCRIPTORS_OK;
}

uint8_t Parse861LongDescriptors(uint8_t *Data)
{
	uint8_t LongDescriptorsOffset;
	uint8_t DescriptorNum = 1;
	LongDescriptorsOffset = Data[LONG_DESCR_PTR_IDX];
	if (!LongDescriptorsOffset) {
		TX_DEBUG_PRINT(("EDID -> No Detailed Descriptors\n"));
		return EDID_NO_DETAILED_DESCRIPTORS;
	}
	while (LongDescriptorsOffset + LONG_DESCR_LEN < EDID_BLOCK_SIZE) {
		TX_EDID_PRINT(("Parse Results - CEA-861 Long Descriptor #%d:\n",
			       (int)DescriptorNum));
		TX_EDID_PRINT(("===============================================================\n"));
#ifdef CONF__TPI_EDID_PRINT
		if (!ParseDetailedTiming(Data, LongDescriptorsOffset, EDID_BLOCK_2_3))
			break;
#endif
		LongDescriptorsOffset += LONG_DESCR_LEN;
		DescriptorNum++;
	}
	return EDID_LONG_DESCRIPTORS_OK;
}

bool_t DoEDID_Checksum(uint8_t *Block)
{
	uint8_t i;
	uint8_t CheckSum = 0;
	for (i = 0; i < EDID_BLOCK_SIZE; i++)
		CheckSum += Block[i];
	if (CheckSum)
		return false;
	return true;
}

static bool_t CheckEDID_Header(uint8_t *Block)
{
	uint8_t i = 0;
	if (Block[i])
		return false;
	for (i = 1; i < 1 + EDID_HDR_NO_OF_FF; i++) {
		if (Block[i] != 0xFF)
			return false;
	}
	if (Block[i])
		return false;
	return true;
}

#ifdef CONF__TPI_EDID_PRINT
static void ParseEstablishedTiming(uint8_t *Data)
{
	TX_EDID_PRINT((TPI_EDID_CHANNEL, "Parsing Established Timing:\n"));
	TX_EDID_PRINT((TPI_EDID_CHANNEL, "===========================\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT7)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "720 x 400 @ 70Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT6)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "720 x 400 @ 88Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT5)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "640 x 480 @ 60Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT4)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "640 x 480 @ 67Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT3)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "640 x 480 @ 72Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT2)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "640 x 480 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT1)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "800 x 600 @ 56Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT0)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "800 x 400 @ 60Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT7)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "800 x 600 @ 72Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT6)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "800 x 600 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT5)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "832 x 624 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT4)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "1024 x 768 @ 87Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT3)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "1024 x 768 @ 60Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT2)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "1024 x 768 @ 70Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT1)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "1024 x 768 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT0)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "1280 x 1024 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 2] & 0x80)
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "1152 x 870 @ 75Hz\n"));
	if ((!Data[0]) && (!Data[ESTABLISHED_TIMING_INDEX + 1]) && (!Data[2]))
		TX_EDID_PRINT((TPI_EDID_CHANNEL, "No established video modes\n"));
}

static void ParseStandardTiming(uint8_t *Data)
{
	uint8_t i;
	uint8_t AR_Code;
	TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Parsing Standard Timing:\n"));
	TPI_EDID_PRINT((TPI_EDID_CHANNEL, "========================\n"));
	for (i = 0; i < NUM_OF_STANDARD_TIMINGS; i += 2) {
		if ((Data[STANDARD_TIMING_OFFSET + i] == 0x01)
		    && ((Data[STANDARD_TIMING_OFFSET + i + 1]) == 1)) {
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Standard Timing Undefined\n"));
		} else {
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Horizontal Active pixels: %i\n",
					(int)((Data[STANDARD_TIMING_OFFSET + i] + 31) * 8)));
			AR_Code = (Data[STANDARD_TIMING_OFFSET + i + 1] & TWO_MSBITS) >> 6;
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Aspect Ratio: "));
			switch (AR_Code) {
			case AR16_10:
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "16:10\n"));
				break;
			case AR4_3:
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "4:3\n"));
				break;
			case AR5_4:
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "5:4\n"));
				break;
			case AR16_9:
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "16:9\n"));
				break;
			}
		}
	}
}

static bool_t ParseDetailedTiming(uint8_t *Data, uint8_t DetailedTimingOffset, uint8_t Block)
{
	uint8_t TmpByte;
	uint8_t i;
	uint16_t TmpWord;
	TmpWord = Data[DetailedTimingOffset + PIX_CLK_OFFSET] +
	    256 * Data[DetailedTimingOffset + PIX_CLK_OFFSET + 1];
	if (TmpWord == 0x00) {
		if (Block == EDID_BLOCK_0) {
			if (Data[DetailedTimingOffset + 3] == 0xFC) {
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Monitor Name: "));
				for (i = 0; i < 13; i++) {
					TPI_EDID_PRINT((TPI_EDID_CHANNEL, "%c",
							Data[DetailedTimingOffset + 5 + i]));
				}
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "\n"));
			} else if (Data[DetailedTimingOffset + 3] == 0xFD) {
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Monitor Range Limits:\n\n"));
				i = 0;
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Min Vertical Rate in Hz: %d\n",
						(int)Data[DetailedTimingOffset + 5 + i++]));
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Max Vertical Rate in Hz: %d\n",
						(int)Data[DetailedTimingOffset + 5 + i++]));
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Min Horizontal Rate in Hz: %d\n",
						(int)Data[DetailedTimingOffset + 5 + i++]));
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Max Horizontal Rate in Hz: %d\n",
						(int)Data[DetailedTimingOffset + 5 + i++]));
				TPI_EDID_PRINT((TPI_EDID_CHANNEL,
						"Max Supported pixel clock rate in MHz/10: %d\n",
						(int)Data[DetailedTimingOffset + 5 + i++]));
				TPI_EDID_PRINT((TPI_EDID_CHANNEL,
						"Tag for secondary timing formula (00h=not used): %d\n",
						(int)Data[DetailedTimingOffset + 5 + i++]));
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Min Vertical Rate in Hz %d\n",
						(int)Data[DetailedTimingOffset + 5 + i]));
				TPI_EDID_PRINT((TPI_EDID_CHANNEL, "\n"));
			}
		} else if (Block == EDID_BLOCK_2_3) {
			TPI_EDID_PRINT((TPI_EDID_CHANNEL,
					"No More Detailed descriptors in this block\n"));
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "\n"));
			return false;
		}
	} else {
		if ((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x36)) {
			TPI_EDID_PRINT((TPI_EDID_CHANNEL,
					"\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 1:\n"));
			TPI_EDID_PRINT((TPI_EDID_CHANNEL,
					"===========================================================\n\n"));
		} else if ((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x48)) {
			TPI_EDID_PRINT((TPI_EDID_CHANNEL,
					"\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 2:\n"));
			TPI_EDID_PRINT((TPI_EDID_CHANNEL,
					"===========================================================\n\n"));
		}
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Pixel Clock (MHz * 100): %d\n", (int)TmpWord));
		TmpWord = Data[DetailedTimingOffset + H_ACTIVE_OFFSET] +
		    256 * ((Data[DetailedTimingOffset + H_ACTIVE_OFFSET + 2] >> 4) & FOUR_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Horizontal Active Pixels: %d\n", (int)TmpWord));
		TmpWord = Data[DetailedTimingOffset + H_BLANKING_OFFSET] +
		    256 * (Data[DetailedTimingOffset + H_BLANKING_OFFSET + 1] & FOUR_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Horizontal Blanking (Pixels): %d\n",
				(int)TmpWord));
		TmpWord =
		    (Data[DetailedTimingOffset + V_ACTIVE_OFFSET]) +
		    256 * ((Data[DetailedTimingOffset + (V_ACTIVE_OFFSET) + 2] >> 4) & FOUR_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Vertical Active (Lines): %d\n", (int)TmpWord));
		TmpWord = Data[DetailedTimingOffset + V_BLANKING_OFFSET] +
		    256 * (Data[DetailedTimingOffset + V_BLANKING_OFFSET + 1] & FOUR_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Vertical Blanking (Lines): %d\n", (int)TmpWord));
		TmpWord = Data[DetailedTimingOffset + H_SYNC_OFFSET] +
		    256 * ((Data[DetailedTimingOffset + (H_SYNC_OFFSET + 3)] >> 6) & TWO_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Horizontal Sync Offset (Pixels): %d\n",
				(int)TmpWord));
		TmpWord =
		    Data[DetailedTimingOffset + H_SYNC_PW_OFFSET] +
		    256 * ((Data[DetailedTimingOffset + (H_SYNC_PW_OFFSET + 2)] >> 4) & TWO_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Horizontal Sync Pulse Width (Pixels): %d\n",
				(int)TmpWord));
		TmpWord =
		    (Data[DetailedTimingOffset + V_SYNC_OFFSET] >> 4) & FOUR_LSBITS +
		    256 * ((Data[DetailedTimingOffset + (V_SYNC_OFFSET + 1)] >> 2) & TWO_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Vertical Sync Offset (Lines): %d\n",
				(int)TmpWord));
		TmpWord =
		    (Data[DetailedTimingOffset + V_SYNC_PW_OFFSET]) & FOUR_LSBITS +
		    256 * (Data[DetailedTimingOffset + (V_SYNC_PW_OFFSET + 1)] & TWO_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Vertical Sync Pulse Width (Lines): %d\n",
				(int)TmpWord));
		TmpWord =
		    Data[DetailedTimingOffset + H_IMAGE_SIZE_OFFSET] +
		    256 *
		    (((Data[DetailedTimingOffset + (H_IMAGE_SIZE_OFFSET + 2)]) >> 4) & FOUR_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Horizontal Image Size (mm): %d\n",
				(int)TmpWord));
		TmpWord =
		    Data[DetailedTimingOffset + V_IMAGE_SIZE_OFFSET] +
		    256 * (Data[DetailedTimingOffset + (V_IMAGE_SIZE_OFFSET + 1)] & FOUR_LSBITS);
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Vertical Image Size (mm): %d\n", (int)TmpWord));
		TmpByte = Data[DetailedTimingOffset + H_BORDER_OFFSET];
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Horizontal Border (Pixels): %d\n",
				(int)TmpByte));
		TmpByte = Data[DetailedTimingOffset + V_BORDER_OFFSET];
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Vertical Border (Lines): %d\n", (int)TmpByte));
		TmpByte = Data[DetailedTimingOffset + FLAGS_OFFSET];
		if (TmpByte & BIT7)
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Interlaced\n"));
		else
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Non-Interlaced\n"));
		if (!(TmpByte & BIT5) && !(TmpByte & BIT6))
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Normal Display, No Stereo\n"));
		else
			TPI_EDID_PRINT((TPI_EDID_CHANNEL,
					"Refer to VESA E-EDID Release A, Revision 1, table 3.17\n"));
		if (!(TmpByte & BIT3) && !(TmpByte & BIT4))
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Analog Composite\n"));
		if ((TmpByte & BIT3) && !(TmpByte & BIT4))
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Bipolar Analog Composite\n"));
		else if (!(TmpByte & BIT3) && (TmpByte & BIT4))
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Digital Composite\n"));
		else if ((TmpByte & BIT3) && (TmpByte & BIT4))
			TPI_EDID_PRINT((TPI_EDID_CHANNEL, "Digital Separate\n"));
		TPI_EDID_PRINT((TPI_EDID_CHANNEL, "\n"));
	}
	return true;
}
#endif
static bool_t GetBIT2(uint8_t *reg_val)
{
	uint8_t DDCReqTimeout = T_BIT2;
	*reg_val = SiiRegRead(TX_PAGE_L0 | 0x00C7);
	SiiRegModify(TX_PAGE_L0 | 0x00C7, BIT0, SET_BITS);
	while (DDCReqTimeout--) {
		if (SiiRegRead(TX_PAGE_L0 | 0x00C7) & BIT1) {
			SiiRegModify(TX_PAGE_L0 | 0x00C7, BIT2, SET_BITS);
			TX_DEBUG_PRINT(("\nGetBIT2_sucessfully\n"));
			return true;
		}
		SiiRegModify(TX_PAGE_L0 | 0x00C7, BIT0, SET_BITS);
		HalTimerWait(20);
	}
	SiiRegModify(TX_PAGE_L0 | 0x00C7, BIT0, CLEAR_BITS);
	return false;
}

static bool_t ReleaseDDC(uint8_t reg_val)
{
	uint8_t DDCReqTimeout = T_BIT2;
	while (DDCReqTimeout--) {
		SiiRegWrite(TX_PAGE_L0 | 0x00C7, reg_val & ~(BIT0 | BIT0));
		if (!(SiiRegRead(TX_PAGE_L0 | 0x00C7) & BIT1)) {
			return true;
		}
	}
	return false;
}
