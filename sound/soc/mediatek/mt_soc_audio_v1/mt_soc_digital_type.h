/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_digital_type.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *
 *******************************************************************************/

#ifndef _AUDIO_DIGITAL_TYPE_H
#define _AUDIO_DIGITAL_TYPE_H


/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/


typedef enum
{
    // memmory interfrace
    Soc_Aud_Digital_Block_MEM_DL1 = 0,
    Soc_Aud_Digital_Block_MEM_DL2,
    Soc_Aud_Digital_Block_MEM_VUL,
    Soc_Aud_Digital_Block_MEM_DAI,
    Soc_Aud_Digital_Block_MEM_I2S,  // currently no use
    Soc_Aud_Digital_Block_MEM_AWB,
    Soc_Aud_Digital_Block_MEM_MOD_DAI,
    Soc_Aud_Digital_Block_MEM_DL1_DATA2 ,
    Soc_Aud_Digital_Block_MEM_VUL_DATA2,
    Soc_Aud_Digital_Block_MEM_HDMI,
    // connection to int main modem
    Soc_Aud_Digital_Block_MODEM_PCM_1_O ,
    // connection to extrt/int modem
    Soc_Aud_Digital_Block_MODEM_PCM_2_O ,
    // 1st I2S for DAC and ADC
    Soc_Aud_Digital_Block_I2S_OUT_DAC ,
    Soc_Aud_Digital_Block_I2S_OUT_DAC_2 , // 4 channel
    Soc_Aud_Digital_Block_I2S_IN_ADC ,
    Soc_Aud_Digital_Block_I2S_IN_ADC_2,  //  4 channel
    // 2nd I2S
    Soc_Aud_Digital_Block_I2S_OUT_2 ,
    Soc_Aud_Digital_Block_I2S_IN_2 ,
    // HW gain contorl
    Soc_Aud_Digital_Block_HW_GAIN1,
    Soc_Aud_Digital_Block_HW_GAIN2,
    // megrge interface
    Soc_Aud_Digital_Block_MRG_I2S_OUT,
    Soc_Aud_Digital_Block_MRG_I2S_IN,
    Soc_Aud_Digital_Block_DAI_BT,
    Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK,
    Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE = Soc_Aud_Digital_Block_MEM_HDMI + 1
} Soc_Aud_Digital_Block;

typedef enum
{
    Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT,
    Soc_Aud_MemIF_Direction_DIRECTION_INPUT
} Soc_Aud_MemIF_Direction;

typedef enum
{
    Soc_Aud_InterConnectionInput_I00,
    Soc_Aud_InterConnectionInput_I01,
    Soc_Aud_InterConnectionInput_I02,
    Soc_Aud_InterConnectionInput_I03,
    Soc_Aud_InterConnectionInput_I04,
    Soc_Aud_InterConnectionInput_I05,
    Soc_Aud_InterConnectionInput_I06,
    Soc_Aud_InterConnectionInput_I07,
    Soc_Aud_InterConnectionInput_I08,
    Soc_Aud_InterConnectionInput_I09,
    Soc_Aud_InterConnectionInput_I10,
    Soc_Aud_InterConnectionInput_I11,
    Soc_Aud_InterConnectionInput_I12,
    Soc_Aud_InterConnectionInput_I13,
    Soc_Aud_InterConnectionInput_I14,
    Soc_Aud_InterConnectionInput_I15,
    Soc_Aud_InterConnectionInput_I16,
    Soc_Aud_InterConnectionInput_I17,
    Soc_Aud_InterConnectionInput_I18,
    Soc_Aud_InterConnectionInput_I19,
    Soc_Aud_InterConnectionInput_I20,
    Soc_Aud_InterConnectionInput_I21,
    Soc_Aud_InterConnectionInput_I22,
    Soc_Aud_InterConnectionInput_Num_Input
} Soc_Aud_InterConnectionInput;

typedef enum
{
    Soc_Aud_InterConnectionOutput_O00,
    Soc_Aud_InterConnectionOutput_O01,
    Soc_Aud_InterConnectionOutput_O02,
    Soc_Aud_InterConnectionOutput_O03,
    Soc_Aud_InterConnectionOutput_O04,
    Soc_Aud_InterConnectionOutput_O05,
    Soc_Aud_InterConnectionOutput_O06,
    Soc_Aud_InterConnectionOutput_O07,
    Soc_Aud_InterConnectionOutput_O08,
    Soc_Aud_InterConnectionOutput_O09,
    Soc_Aud_InterConnectionOutput_O10,
    Soc_Aud_InterConnectionOutput_O11,
    Soc_Aud_InterConnectionOutput_O12,
    Soc_Aud_InterConnectionOutput_O13,
    Soc_Aud_InterConnectionOutput_O14,
    Soc_Aud_InterConnectionOutput_O15,
    Soc_Aud_InterConnectionOutput_O16,
    Soc_Aud_InterConnectionOutput_O17,
    Soc_Aud_InterConnectionOutput_O18,
    Soc_Aud_InterConnectionOutput_O19,
    Soc_Aud_InterConnectionOutput_O20,
    Soc_Aud_InterConnectionOutput_O21,
    Soc_Aud_InterConnectionOutput_O22,
    Soc_Aud_InterConnectionOutput_O23,
    Soc_Aud_InterConnectionOutput_O24,
    Soc_Aud_InterConnectionOutput_O25,
    Soc_Aud_InterConnectionOutput_Num_Output
} Soc_Aud_InterConnectionOutput;

typedef enum
{
    Soc_Aud_InterConnectionInput_I30,
    Soc_Aud_InterConnectionInput_I31,
    Soc_Aud_InterConnectionInput_I32,
    Soc_Aud_InterConnectionInput_I33,
    Soc_Aud_InterConnectionInput_I34,
    Soc_Aud_InterConnectionInput_I35,
    Soc_Aud_InterConnectionInput_I36,
    Soc_Aud_InterConnectionInput_I37,
} Soc_Aud_Hdmi_InterConnectionInput;


typedef enum
{
    Soc_Aud_InterConnectionOutput_O30,
    Soc_Aud_InterConnectionOutput_O31,
    Soc_Aud_InterConnectionOutput_O32,
    Soc_Aud_InterConnectionOutput_O33,
    Soc_Aud_InterConnectionOutput_O34,
    Soc_Aud_InterConnectionOutput_O35,
    Soc_Aud_InterConnectionOutput_O36,
    Soc_Aud_InterConnectionOutput_O37,
} Soc_Aud_Hdmi_InterConnectionOutput;


typedef enum
{
    Soc_Aud_InterCon_DisConnect = 0x0,
    Soc_Aud_InterCon_Connection = 0x1,
    Soc_Aud_InterCon_ConnectionShift = 0x2
} Soc_Aud_InterConnectionState;


typedef enum
{
    STREAMSTATUS_STATE_FREE = -1,     // memory is not allocate
    STREAMSTATUS_STATE_STANDBY,  // memory allocate and ready
    STREAMSTATUS_STATE_EXECUTING, // stream is running
} STREAMSTATUS;

enum Soc_Aud_TopClockType
{
    Soc_Aud_TopClockType_APB_CLOCK = 1,
    Soc_Aud_TopClockType_APB_AFE_CLOCK = 2,
    Soc_Aud_TopClockType_APB_I2S_INPUT_CLOCK = 6,
    Soc_Aud_TopClockType_APB_AFE_CK_DIV_RRST = 16,
    Soc_Aud_TopClockType_APB_PDN_APLL_TUNER  = 19,
    Soc_Aud_TopClockType_APB_PDN_HDMI_CK     = 20,
    Soc_Aud_TopClockType_APB_PDN_SPDIF_CK    = 21
};

enum Soc_Aud_AFEClockType
{
    Soc_Aud_AFEClockType_AFE_ON = 0,
    Soc_Aud_AFEClockType_DL1_ON = 1,
    Soc_Aud_AFEClockType_DL2_ON = 2,
    Soc_Aud_AFEClockType_VUL_ON = 3,
    Soc_Aud_AFEClockType_DAI_ON = 4,
    Soc_Aud_AFEClockType_I2S_ON = 5,
    Soc_Aud_AFEClockType_AWB_ON = 6,
    Soc_Aud_AFEClockType_MOD_PCM_ON = 7,
    Soc_Aud_AFEClockType_DL1Data2 = 8 ,
    Soc_Aud_AFEClockType_VULdata2 = 9,
    Soc_Aud_AFEClockType_VUL2data = 10,
};

enum Soc_Aud_IRQ_MCU_MODE
{
    Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE = 0,
    Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
    Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE,
    Soc_Aud_IRQ_MCU_MODE_IRQ4_MCU_MODE,
    Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE,
    Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE
};

enum Soc_Aud_Hw_Digital_Gain
{
    Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1,
    Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2
};

enum Soc_Aud_BCK_INV
{
    Soc_Aud_INV_BCK_NO_INVERSE = 0,
    Soc_Aud_INV_BCK_INVESE = 1
};

enum Soc_Aud_I2S_IN_PAD_SEL
{
    Soc_Aud_I2S_IN_PAD_SEL_I2S_IN_FROM_CONNSYS = 0,
    Soc_Aud_I2S_IN_PAD_SEL_I2S_IN_FROM_IO_MUX = 1
};

enum Soc_Aud_LR_SWAP
{
    Soc_Aud_LR_SWAP_NO_SWAP = 0,
    Soc_Aud_LR_SWAP_LR_DATASWAP = 1
};

enum Soc_Aud_INV_LRCK
{
    Soc_Aud_INV_LRCK_NO_INVERSE = 0,
    Soc_Aud_INV_LRCK_INVESE_LRCK = 1
};

enum Soc_Aud_I2S_FORMAT
{
    Soc_Aud_I2S_FORMAT_EIAJ = 0,
    Soc_Aud_I2S_FORMAT_I2S  = 1
};

enum Soc_Aud_I2S_SRC
{
    Soc_Aud_I2S_SRC_MASTER_MODE = 0,
    Soc_Aud_I2S_SRC_SLAVE_MODE = 1
};

enum  Soc_Aud_I2S_HD_EN
{
    Soc_Aud_NORMAL_CLOCK = 0,
    Soc_Aud_LOW_JITTER_CLOCK = 1
};

enum Soc_Aud_I2S_WLEN
{
    Soc_Aud_I2S_WLEN_WLEN_16BITS = 0 ,
    Soc_Aud_I2S_WLEN_WLEN_32BITS = 1
};

enum Soc_Aud_I2S_SAMPLERATE
{
    Soc_Aud_I2S_SAMPLERATE_I2S_8K = 0,
    Soc_Aud_I2S_SAMPLERATE_I2S_11K = 1,
    Soc_Aud_I2S_SAMPLERATE_I2S_12K = 2,
    Soc_Aud_I2S_SAMPLERATE_I2S_16K = 4,
    Soc_Aud_I2S_SAMPLERATE_I2S_22K = 5,
    Soc_Aud_I2S_SAMPLERATE_I2S_24K = 6,
    Soc_Aud_I2S_SAMPLERATE_I2S_32K = 8,
    Soc_Aud_I2S_SAMPLERATE_I2S_44K = 9,
    Soc_Aud_I2S_SAMPLERATE_I2S_48K = 10,
    Soc_Aud_I2S_SAMPLERATE_I2S_88K = 11,
    Soc_Aud_I2S_SAMPLERATE_I2S_96K = 12,
    Soc_Aud_I2S_SAMPLERATE_I2S_174K = 13,
    Soc_Aud_I2S_SAMPLERATE_I2S_192K = 14,
    Soc_Aud_I2S_SAMPLERATE_I2S_260K = 15,
};

enum Soc_Aud_I2S
{
    Soc_Aud_I2S0 = 0,
    Soc_Aud_I2S1,
    Soc_Aud_I2S2,
    Soc_Aud_I2S3,
};

enum Soc_Aud_APLL_SOURCE
{
    Soc_Aud_APLL1 = 0,  // 44.1K base
    Soc_Aud_APLL2,        // 48base
};

typedef struct
{
    bool mLR_SWAP;
    bool mI2S_SLAVE;
    uint32 mI2S_SAMPLERATE;
    bool mINV_LRCK;
    bool mI2S_FMT;
    bool mI2S_WLEN;
    bool mI2S_EN;
    bool mI2S_HDEN;
    bool mI2S_IN_PAD_SEL;

    // her for ADC usage , DAC will not use this
    int mBuffer_Update_word;
    bool mloopback;
    bool mFpga_bit;
    bool mFpga_bit_test;
} AudioDigtalI2S;

enum Soc_Aud_TX_LCH_RPT
{
    Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT = 0,
    Soc_Aud_TX_LCH_RPT_TX_LCH_REPEAT    = 1
};

enum Soc_Aud_VBT_16K_MODE
{
    Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE = 0,
    Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_ENABLE  = 1
};

enum Soc_Aud_EXT_MODEM
{
    Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM = 0,
    Soc_Aud_EXT_MODEM_MODEM_2_USE_EXTERNAL_MODEM = 1
};

enum Soc_Aud_PCM_SYNC_TYPE
{
    Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC        = 0, // bck sync length = 1
    Soc_Aud_PCM_SYNC_TYPE_EXTEND_BCK_CYCLE_SYNC = 1  // bck sync length = PCM_INTF_CON[9:13]
};

enum Soc_Aud_BT_MODE
{
    Soc_Aud_BT_MODE_DUAL_MIC_ON_TX   = 0,
    Soc_Aud_BT_MODE_SINGLE_MIC_ON_TX = 1
};

enum Soc_Aud_BYPASS_SRC
{
    Soc_Aud_BYPASS_SRC_SLAVE_USE_ASRC       = 0, // slave mode & external modem uses different crystal
    Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO = 1  // slave mode & external modem uses the same crystal
};

enum Soc_Aud_PCM_CLOCK_SOURCE
{
    Soc_Aud_PCM_CLOCK_SOURCE_MASTER_MODE = 0,
    Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE  = 1
};

enum Soc_Aud_PCM_WLEN_LEN
{
    Soc_Aud_PCM_WLEN_LEN_PCM_16BIT = 0,
    Soc_Aud_PCM_WLEN_LEN_PCM_32BIT = 1
};

enum Soc_Aud_PCM_MODE
{
    Soc_Aud_PCM_MODE_PCM_MODE_8K  = 0,
    Soc_Aud_PCM_MODE_PCM_MODE_16K = 1 ,
    Soc_Aud_PCM_MODE_PCM_MODE_32K = 2 ,
};

enum Soc_Aud_PCM_FMT
{
    Soc_Aud_PCM_FMT_PCM_I2S    = 0,
    Soc_Aud_PCM_FMT_PCM_EIAJ   = 1,
    Soc_Aud_PCM_FMT_PCM_MODE_A = 2,
    Soc_Aud_PCM_FMT_PCM_MODE_B = 3
};

typedef struct
{
    uint32	mBclkInInv;
    uint32          mTxLchRepeatSel;
    uint32        mVbt16kModeSel;
    uint32           mExtModemSel;
    uint8             mExtendBckSyncLength;
    uint32       mExtendBckSyncTypeSel;
    uint32             mSingelMicSel;
    uint32          mAsyncFifoSel;
    uint32    mSlaveModeSel;
    uint32        mPcmWordLength;
    uint32            mPcmModeWidebandSel;
    uint32            mPcmFormat;
    uint8              mModemPcmOn;
} AudioDigitalPCM;

enum Soc_Aud_BT_DAI_INPUT
{
    Soc_Aud_BT_DAI_INPUT_FROM_BT,
    Soc_Aud_BT_DAI_INPUT_FROM_MGRIF
};

enum Soc_Aud_DATBT_MODE
{
    Soc_Aud_DATBT_MODE_Mode8K,
    Soc_Aud_DATBT_MODE_Mode16K
};

enum Soc_Aud_DAI_DEL
{
    Soc_Aud_DAI_DEL_HighWord,
    Soc_Aud_DAI_DEL_LowWord
};

enum Soc_Aud_BTSYNC
{
    Soc_Aud_BTSYNC_Short_Sync,
    Soc_Aud_BTSYNC_Long_Sync
};

typedef struct
{
    bool mUSE_MRGIF_INPUT;
    bool mDAI_BT_MODE;
    bool mDAI_DEL;
    int mBT_LEN;
    bool mDATA_RDY;
    bool mBT_SYNC;
    bool mBT_ON;
    bool mDAIBT_ON;
} AudioDigitalDAIBT;

enum Soc_Aud_MRFIF_I2S_SAMPLERATE
{
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_8K = 0,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_11K = 1,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_12K = 2,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_16K = 4,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_22K = 5,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_24K = 6,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_32K = 8,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_44K = 9,
    Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_48K = 10
};

typedef struct
{
    bool Mergeif_I2S_Enable;
    bool Merge_cnt_Clear;
    int Mrg_I2S_SampleRate;
    int Mrg_Sync_Dly;
    int Mrg_Clk_Edge_Dly;
    int Mrg_Clk_Dly;
    bool MrgIf_En;
} AudioMrgIf;

// class for irq mode and counter.
typedef struct
{
    unsigned int mStatus;  // on,off
    unsigned int mIrqMcuCounter;
    unsigned int mSampleRate;
} AudioIrqMcuMode;

typedef struct
{
    int mFormat;
    int mDirection;
    unsigned int mSampleRate;
    unsigned int mChannels;
    unsigned int mBufferSize;
    unsigned int mInterruptSample;
    unsigned int mMemoryInterFaceType;
    unsigned int mClockInverse;
    unsigned int mMonoSel;
    unsigned int mdupwrite;
    unsigned int mState;
    unsigned int mFetchFormatPerSample;
    int mUserCount;
    void *privatedata;
} AudioMemIFAttribute;

typedef struct
{
    unsigned int  offset;
    unsigned int  value;
    unsigned int  mask;
} Register_Control;

typedef struct
{
    int bSpeechFlag;
    int bBgsFlag;
    int bRecordFlag;
    int bTtyFlag;
    int bVT;
    int bAudioPlay;
} SPH_Control;

typedef struct
{
    int SampleRate;
    int ClkApllSel; // 0-5
} Hdmi_Clock_Control;

enum SPEAKER_CHANNEL
{
    Channel_None = 0 ,
    Channel_Right,
    Channel_Left,
    Channel_Stereo
};

enum SOUND_PATH
{
    DEFAULT_PATH = 0,
    IN1_PATH,
    IN2_PATH,
    IN3_PATH,
    IN1_IN2_MIX,
};

enum MIC_ANALOG_SWICTH
{
    MIC_ANA_DEFAULT_PATH = 0,
    MIC_ANA_SWITCH1_HIGH
};
enum PolicyParameters
{
    POLICY_LOAD_VOLUME =         0,
    POLICY_SET_FM_SPEAKER,
    POLICY_CHECK_FM_PRIMARY_KEY_ROUTING,
    POLICY_SET_FM_PRESTOP,
};

enum modem_index_t
{
    MODEM_1   = 0,
    MODEM_2   = 1,
    MODEM_EXTERNAL   = 2,
    NUM_MODEM
};

typedef enum
{
    AFE_WLEN_16_BIT = 0,
    AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA = 1,
    AFE_WLEN_32_BIT_ALIGN_24BIT_DATA_8BIT_0 = 3,
} FETCHFORMATPERSAMPLE;

typedef enum
{
    OUTPUT_DATA_FORMAT_16BIT = 0,
    OUTPUT_DATA_FORMAT_24BIT
} OUTPUT_DATA_FORMAT;


typedef enum
{
    APLL_SOURCE_24576 = 0,
    APLL_SOURCE_225792 = 1
} APLL_SOURCE_SEL;

typedef enum
{
    HDMI_SDATA0 = 0,
    HDMI_SDATA1,
    HDMI_SDATA2,
    HDMI_SDATA3,
} HDMI_SDATA_CHANNEL;

typedef enum
{
    HDMI_8_CHANNELS = 0,
    HDMI_6_CHANNELS,
    HDMI_4_CHANNELS,
    HDMI_2_CHANNELS,
} HDMI_SDATA_SEQUENCE;


typedef struct
{
    bool mLR_SWAP;
    bool mI2S_SLAVE;
    uint32 mSampleRate;
    bool mINV_LRCK;
    bool mI2S_FMT;
    bool mI2S_WLEN;
    bool mI2S_EN;

    uint32 mChannels;
    uint32 mApllSource;
    uint32 mApllSamplerate;
    uint32 mHdmiMckDiv;
    uint32 mMckSamplerate;
    uint32 mHdmiBckDiv;
    uint32 mBckSamplerate;

    // her for ADC usage , DAC will not use this
    int mBuffer_Update_word;
    bool mloopback;
    bool mFpga_bit;
    bool mFpga_bit_test;
} AudioHdmi;

typedef enum
{
    SRAM_STATE_FREE =0,
    SRAM_STATE_PLAYBACKFULL = 0x1,
    SRAM_STATE_PLAYBACKPARTIAL =0x2 ,
    SRAM_STATE_CAPTURE  =0x4,
    SRAM_STATE_PLAYBACKDRAM  =0x8,
    SRAM_STATE_SPH_SPK_MNTR_CAPTURE_DL = 0x10,
    SRAM_STATE_SPH_SPK_MNTR_PROCESS_DL = 0x20,
    SRAM_STATE_SPH_SPK_MNTR_SENSE_IV = 0x40,
} AUDIO_SRAM_STATE;

typedef struct
{
    unsigned int mMemoryState;
    bool mPlaybackAllocated;
    bool mPlaybackAllocateSize;
    bool mCaptureAllocated;
    bool mCaptureAllocateSize;
} AudioSramManager;

typedef enum
{
    AUDIO_ANC_ON = 0,
    AUDIO_ANC_OFF ,
} AUDIO_ANC_MODE;


typedef enum
{
    AUDIO_APLL1_DIV0 =8 ,
    AUDIO_APLL1_DIV1 =9,
    AUDIO_APLL1_DIV2 =10,
    AUDIO_APLL1_DIV3 =11,
    AUDIO_APLL1_DIV4 =12,
    AUDIO_APLL1_DIV5 =13,
    AUDIO_APLL2_DIV0 =16,
    AUDIO_APLL2_DIV1 =17,
    AUDIO_APLL2_DIV2 =18,
    AUDIO_APLL2_DIV3 =19,
    AUDIO_APLL2_DIV4 =20,
    AUDIO_APLL2_DIV5 =21
} AUDIO_APLL_DIVIDER_GROUP;

typedef enum
{
    AUDIO_APLL1_DIV0_INDEX =0 ,
    AUDIO_APLL1_DIV1_INDEX ,
    AUDIO_APLL1_DIV2_INDEX ,
    AUDIO_APLL1_DIV3_INDEX ,
    AUDIO_APLL1_DIV4_INDEX ,
    AUDIO_APLL1_DIV5_INDEX ,
    AUDIO_APLL1_DIV_NUM ,
} AUDIO_APLL1_DIVIDER_INDEX;

typedef enum
{
    AUDIO_APLL2_DIV0_INDEX  = 0,
    AUDIO_APLL2_DIV1_INDEX ,
    AUDIO_APLL2_DIV2_INDEX ,
    AUDIO_APLL2_DIV3_INDEX ,
    AUDIO_APLL2_DIV4_INDEX ,
    AUDIO_APLL2_DIV5_INDEX ,
    AUDIO_APLL2_DIV_NUM ,
} AUDIO_APLL2_DIVIDER_INDEX;

typedef enum
{
    AUDIO_APLL1_GROUP1 = 0 ,
    AUDIO_APLL2_GROUP2 = 1,
} AUDIO_APLL_GROUP_DEFINE;

typedef enum
{
    AUDIO_MODE_NORMAL =0 ,
    AUDIO_MODE_RINGTONE,
    AUDIO_MODE_INCALL,
    AUDIO_MODE_INCALL2 ,
    AUDIO_MODE_INCALL_EXTERNAL ,
} AUDIO_MODE;

typedef struct
{
    uint32 REG_AUDIO_TOP_CON1;
    uint32 REG_AUDIO_TOP_CON2;
    uint32 REG_AUDIO_TOP_CON3;
    uint32 REG_AFE_DAC_CON0;
    uint32 REG_AFE_DAC_CON1;
    uint32 REG_AFE_I2S_CON;
    uint32 REG_AFE_DAIBT_CON0;
    uint32 REG_AFE_CONN0;
    uint32 REG_AFE_CONN1;
    uint32 REG_AFE_CONN2;
    uint32 REG_AFE_CONN3;
    uint32 REG_AFE_CONN4;
    uint32 REG_AFE_I2S_CON1;
    uint32 REG_AFE_I2S_CON2;
    uint32 REG_AFE_MRGIF_CON;
    uint32 REG_AFE_DL1_BASE;
    uint32 REG_AFE_DL1_CUR;
    uint32 REG_AFE_DL1_END;
    uint32 REG_AFE_DL1_D2_BASE;
    uint32 REG_AFE_DL1_D2_CUR;
    uint32 REG_AFE_DL1_D2_END;
    uint32 REG_AFE_VUL_D2_BASE;
    uint32 REG_AFE_VUL_D2_END;
    uint32 REG_AFE_VUL_D2_CUR;
    uint32 REG_AFE_I2S_CON3;
    uint32 REG_AFE_DL2_BASE;
    uint32 REG_AFE_DL2_CUR;
    uint32 REG_AFE_DL2_END;
    uint32 REG_AFE_CONN5;
    uint32 REG_AFE_CONN_24BIT;
    uint32 REG_AFE_AWB_BASE;
    uint32 REG_AFE_AWB_END;
    uint32 REG_AFE_AWB_CUR;
    uint32 REG_AFE_VUL_BASE;
    uint32 REG_AFE_VUL_END;
    uint32 REG_AFE_VUL_CUR;
    uint32 REG_AFE_DAI_BASE;
    uint32 REG_AFE_DAI_END;
    uint32 REG_AFE_DAI_CUR;
    uint32 REG_AFE_CONN6;
    uint32 REG_AFE_MEMIF_MSB;
    uint32 REG_AFE_MEMIF_MON0;
    uint32 REG_AFE_MEMIF_MON1;
    uint32 REG_AFE_MEMIF_MON2;
    uint32 REG_AFE_MEMIF_MON3;
    uint32 REG_AFE_MEMIF_MON4;
    uint32 REG_AFE_ADDA_DL_SRC2_CON0;
    uint32 REG_AFE_ADDA_DL_SRC2_CON1;
    uint32 REG_AFE_ADDA_UL_SRC_CON0;
    uint32 REG_AFE_ADDA_UL_SRC_CON1;
    uint32 REG_AFE_ADDA_TOP_CON0;
    uint32 REG_AFE_ADDA_UL_DL_CON0;
    uint32 REG_AFE_ADDA_SRC_DEBUG;
    uint32 REG_AFE_ADDA_SRC_DEBUG_MON0;
    uint32 REG_AFE_ADDA_SRC_DEBUG_MON1;
    uint32 REG_AFE_ADDA_NEWIF_CFG0;
    uint32 REG_AFE_ADDA_NEWIF_CFG1;
    uint32 REG_AFE_SIDETONE_DEBUG;
    uint32 REG_AFE_SIDETONE_MON;
    uint32 REG_AFE_SIDETONE_CON0;
    uint32 REG_AFE_SIDETONE_COEFF;
    uint32 REG_AFE_SIDETONE_CON1;
    uint32 REG_AFE_SIDETONE_GAIN;
    uint32 REG_AFE_SGEN_CON0;
    uint32 REG_AFE_SGEN_CON1;
    uint32 REG_AFE_TOP_CON0;
    uint32 REG_AFE_ADDA_PREDIS_CON0;
    uint32 REG_AFE_ADDA_PREDIS_CON1;
    uint32 REG_AFE_MRGIF_MON0;
    uint32 REG_AFE_MRGIF_MON1;
    uint32 REG_AFE_MRGIF_MON2;
    uint32 REG_AFE_MOD_DAI_BASE;
    uint32 REG_AFE_MOD_DAI_END;
    uint32 REG_AFE_MOD_DAI_CUR;
    uint32 REG_AFE_HDMI_OUT_CON0;
    uint32 REG_AFE_HDMI_BASE;
    uint32 REG_AFE_HDMI_CUR;
    uint32 REG_AFE_HDMI_END;
    uint32 REG_AFE_HDMI_CONN0;
    uint32 REG_AFE_IRQ_MCU_CON;
    uint32 REG_AFE_IRQ_MCU_STATUS;
    uint32 REG_AFE_IRQ_MCU_CLR;
    uint32 REG_AFE_IRQ_MCU_CNT1;
    uint32 REG_AFE_IRQ_MCU_CNT2;
    uint32 REG_AFE_IRQ_MCU_EN;
    uint32 REG_AFE_IRQ_MCU_MON2;
    uint32 REG_AFE_IRQ1_MCU_CNT_MON;
    uint32 REG_AFE_IRQ2_MCU_CNT_MON;
    uint32 REG_AFE_IRQ1_MCU_EN_CNT_MON;
    uint32 REG_AFE_MEMIF_MAXLEN;
    uint32 REG_AFE_MEMIF_PBUF_SIZE;
    uint32 REG_AFE_APLL1_TUNER_CFG;
    uint32 REG_AFE_APLL2_TUNER_CFG;
    uint32 REG_AFE_GAIN1_CON0;
    uint32 REG_AFE_GAIN1_CON1;
    uint32 REG_AFE_GAIN1_CON2;
    uint32 REG_AFE_GAIN1_CON3;
    uint32 REG_AFE_GAIN1_CONN;
    uint32 REG_AFE_GAIN1_CUR;
    uint32 REG_AFE_GAIN2_CON0;
    uint32 REG_AFE_GAIN2_CON1;
    uint32 REG_AFE_GAIN2_CON2;
    uint32 REG_AFE_GAIN2_CON3;
    uint32 REG_AFE_GAIN2_CONN;
    uint32 REG_AFE_GAIN2_CUR;
    uint32 REG_AFE_GAIN2_CONN2;
    uint32 REG_AFE_GAIN2_CONN3;
    uint32 REG_AFE_GAIN1_CONN2;
    uint32 REG_AFE_GAIN1_CONN3;
    uint32 REG_AFE_CONN7;
    uint32 REG_AFE_CONN8;
    uint32 REG_AFE_CONN9;
    uint32 REG_FPGA_CFG2;
    uint32 REG_FPGA_CFG3;;
    uint32 REG_FPGA_CFG0;
    uint32 REG_FPGA_CFG1;
    uint32 REG_FPGA_VER;
    uint32 REG_FPGA_STC;
    uint32 REG_AFE_ASRC_CON0;
    uint32 REG_AFE_ASRC_CON1;
    uint32 REG_AFE_ASRC_CON2;
    uint32 REG_AFE_ASRC_CON3;
    uint32 REG_AFE_ASRC_CON4;
    uint32 REG_AFE_ASRC_CON5;
    uint32 REG_AFE_ASRC_CON6;
    uint32 REG_AFE_ASRC_CON7;
    uint32 REG_AFE_ASRC_CON8;
    uint32 REG_AFE_ASRC_CON9;
    uint32 REG_AFE_ASRC_CON10;
    uint32 REG_AFE_ASRC_CON11;
    uint32 REG_PCM_INTF_CON;
    uint32 REG_PCM_INTF_CON2;
    uint32 REG_PCM2_INTF_CON;
    uint32 REG_AFE_TDM_CON1;
    uint32 REG_AFE_TDM_CON2;
    uint32 REG_AFE_ASRC_CON13;
    uint32 REG_AFE_ASRC_CON14;
    uint32 REG_AFE_ASRC_CON15;
    uint32 REG_AFE_ASRC_CON16;
    uint32 REG_AFE_ASRC_CON17;
    uint32 REG_AFE_ASRC_CON18;
    uint32 REG_AFE_ASRC_CON19;
    uint32 REG_AFE_ASRC_CON20;
    uint32 REG_AFE_ASRC_CON21;
    uint32 REG_AFE_ADDA2_TOP_CON0;
    uint32 REG_AFE_ADDA2_UL_SRC_CON0;
    uint32 REG_AFE_ADDA2_UL_SRC_CON1;
    uint32 REG_AFE_ADDA2_SRC_DEBUG;
    uint32 REG_AFE_ADDA2_SRC_DEBUG_MON0;
    uint32 REG_AFE_ADDA2_SRC_DEBUG_MON1;
    uint32 REG_AFE_ADDA2_NEWIF_CFG0;
    uint32 REG_AFE_ADDA2_NEWIF_CFG1;
    uint32 REG_AFE_ADDA2_ULCF_CFG_02_01;
    uint32 REG_AFE_ADDA2_ULCF_CFG_04_03;
    uint32 REG_AFE_ADDA2_ULCF_CFG_06_05;
    uint32 REG_AFE_ADDA2_ULCF_CFG_08_07;
    uint32 REG_AFE_ADDA2_ULCF_CFG_10_09;
    uint32 REG_AFE_ADDA2_ULCF_CFG_12_11;
    uint32 REG_AFE_ADDA2_ULCF_CFG_14_13;
    uint32 REG_AFE_ADDA2_ULCF_CFG_16_15;
    uint32 REG_AFE_ADDA2_ULCF_CFG_18_17;
    uint32 REG_AFE_ADDA2_ULCF_CFG_20_19;
    uint32 REG_AFE_ADDA2_ULCF_CFG_22_21;
    uint32 REG_AFE_ADDA2_ULCF_CFG_24_23;
    uint32 REG_AFE_ADDA2_ULCF_CFG_26_25;
    uint32 REG_AFE_ADDA2_ULCF_CFG_28_27;
    uint32 REG_AFE_ADDA2_ULCF_CFG_30_29;
    uint32 REG_AFE_ADDA3_UL_SRC_CON0;
    uint32 REG_AFE_ADDA3_UL_SRC_CON1;
    uint32 REG_AFE_ADDA3_SRC_DEBUG;
    uint32 REG_AFE_ADDA3_SRC_DEBUG_MON0;
    uint32 REG_AFE_ADDA3_ULCF_CFG_02_01;
    uint32 REG_AFE_ADDA3_ULCF_CFG_04_03;
    uint32 REG_AFE_ADDA3_ULCF_CFG_06_05;
    uint32 REG_AFE_ADDA3_ULCF_CFG_08_07;
    uint32 REG_AFE_ADDA3_ULCF_CFG_10_09;
    uint32 REG_AFE_ADDA3_ULCF_CFG_12_11;
    uint32 REG_AFE_ADDA3_ULCF_CFG_14_13;
    uint32 REG_AFE_ADDA3_ULCF_CFG_16_15;
    uint32 REG_AFE_ADDA3_ULCF_CFG_18_17;
    uint32 REG_AFE_ADDA3_ULCF_CFG_20_19;
    uint32 REG_AFE_ADDA3_ULCF_CFG_22_21;
    uint32 REG_AFE_ADDA3_ULCF_CFG_24_23;
    uint32 REG_AFE_ADDA3_ULCF_CFG_26_25;
    uint32 REG_AFE_ADDA3_ULCF_CFG_28_27;
    uint32 REG_AFE_ADDA3_ULCF_CFG_30_29;
    uint32 REG_AFE_ASRC2_CON0;
    uint32 REG_AFE_ASRC2_CON1;
    uint32 REG_AFE_ASRC2_CON2;
    uint32 REG_AFE_ASRC2_CON3;
    uint32 REG_AFE_ASRC2_CON4;
    uint32 REG_AFE_ASRC2_CON5;
    uint32 REG_AFE_ASRC2_CON6;
    uint32 REG_AFE_ASRC2_CON7;
    uint32 REG_AFE_ASRC2_CON8;
    uint32 REG_AFE_ASRC2_CON9;
    uint32 REG_AFE_ASRC2_CON10;
    uint32 REG_AFE_ASRC2_CON11;
    uint32 REG_AFE_ASRC2_CON12;
    uint32 REG_AFE_ASRC2_CON13;
    uint32 REG_AFE_ASRC2_CON14;
    uint32 REG_AFE_ASRC3_CON0;
    uint32 REG_AFE_ASRC3_CON1;
    uint32 REG_AFE_ASRC3_CON2;
    uint32 REG_AFE_ASRC3_CON3;
    uint32 REG_AFE_ASRC3_CON4;
    uint32 REG_AFE_ASRC3_CON5;
    uint32 REG_AFE_ASRC3_CON6;
    uint32 REG_AFE_ASRC3_CON7;
    uint32 REG_AFE_ASRC3_CON8;
    uint32 REG_AFE_ASRC3_CON9;
    uint32 REG_AFE_ASRC3_CON10;
    uint32 REG_AFE_ASRC3_CON11;
    uint32 REG_AFE_ASRC3_CON12;
    uint32 REG_AFE_ASRC3_CON13;
    uint32 REG_AFE_ASRC3_CON14;
} AudioAfeRegCache;

#endif

