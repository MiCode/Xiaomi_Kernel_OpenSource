/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2018 XiaoMi, Inc.
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
 *   mtk_soc_codec_63xx
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Audio codec stub file
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


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_analog_type.h"
#include <mach/mt_clkbuf_ctl.h>
#include <sound/mt_soc_audio.h>
#include <mach/vow_api.h>
#ifdef CONFIG_MTK_SPEAKER
#include "mt_soc_codec_speaker_63xx.h"
#endif

#include "mt_soc_pcm_common.h"
#include <mach/mt_gpio.h>
//#define VOW_TONE_TEST

#ifndef CONFIG_MTK_FPGA
extern void mt6331_upmu_set_rg_audmicbias1lowpen(kal_uint32 val);
extern void mt6331_upmu_set_rg_audmicbias1dcswnen(kal_uint32 val);
extern void mt6331_upmu_set_rg_audmicbias1dcswpen(kal_uint32 val);
extern void mt6331_upmu_set_rg_audpwdbmicbias1(kal_uint32 val);

extern void mt6331_upmu_set_rg_audmicbias0lowpen(kal_uint32 val);
extern void mt6331_upmu_set_rg_audmicbias0dcswnen(kal_uint32 val);
extern void mt6331_upmu_set_rg_audmicbias0dcswpen(kal_uint32 val);
extern void mt6331_upmu_set_rg_audpwdbmicbias0(kal_uint32 val);
#endif

// static function declaration
static void HeadsetRVolumeSet(void);
static void HeadsetLVolumeSet(void);
static bool AudioPreAmp1_Sel(int Mul_Sel);
static bool GetAdcStatus(void);
static void Apply_Speaker_Gain(void);
static bool TurnOnVOWDigitalHW(bool enable);
static void TurnOffDacPower(void);
static void TurnOnDacPower(void);
static void OpenClassH(void);
static void OpenClassAB(void);
static void SetDcCompenSation(void);
static bool GetDLStatus(void);
static void EnableMicBias(uint32_t Mic, bool bEnable);
static void OpenMicbias0(bool bEanble);
static void OpenMicbias1(bool bEanble);
static void OpenMicbias2(bool bEanble);
static void OpenMicbias3(bool bEanble);
static void Audio_ADC1_Set_Input(int Input);
static void Audio_ADC2_Set_Input(int Input);
static void Audio_ADC3_Set_Input(int Input);
static void Audio_ADC4_Set_Input(int Input);
static void Voice_Amp_Change(bool enable);
static void Speaker_Amp_Change(bool enable);
static bool TurnOnVOWADcPowerACC(int ADCType, bool enable);
static void Audio_Amp_Change(int channels , bool enable);
void OpenAnalogHeadphone(bool bEnable);


#ifndef CONFIG_MTK_FPGA
extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);
#endif

static mt6331_Codec_Data_Priv *mCodec_data = NULL;
static uint32 mBlockSampleRate[AUDIO_ANALOG_DEVICE_INOUT_MAX] = {48000, 48000, 48000};
static DEFINE_MUTEX(Ana_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_buf_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_Clk_Mutex);
static DEFINE_MUTEX(Ana_Power_Mutex);
static DEFINE_MUTEX(AudAna_lock);

static int mAudio_Analog_Mic1_mode  = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic2_mode  = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic3_mode  = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic4_mode  = AUDIO_ANALOGUL_MODE_ACC;

static int mAudio_Vow_Analog_Func_Enable = false;
static int mAudio_Vow_Digital_Func_Enable = false;
static int TrimOffset = 2048;
//static const int DC1unit_in_uv = 21576*2; // in uv
//static const int DC1unit_in_uv = 19184 ; // in uv with 0DB
static const int DC1unit_in_uv = 17265 ; // in uv with 0DB
static const int DC1devider = 8; // in uv

#ifdef CONFIG_MTK_SPEAKER
static int Speaker_mode = AUDIO_SPEAKER_MODE_AB;
static unsigned int Speaker_pga_gain = 1 ; // default 0Db.
static bool mSpeaker_Ocflag = false;
static int mEnableAuxAdc = 0;
#endif
static int mAdc_Power_Mode = 0;
static unsigned int dAuxAdcChannel = 16;
static const int mDcOffsetTrimChannel = 9;
static bool mInitCodec = false;

static int reg_AFE_VOW_CFG0 = 0xffff;   //bias removing reference amp
static int reg_AFE_VOW_CFG1 = 0x0200;   //watch dog timer initial value
static int reg_AFE_VOW_CFG2 = 0x2424;   //VOW A/B value
static int reg_AFE_VOW_CFG3 = 0x8767;   //VOW alpha/beta value
static int reg_AFE_VOW_CFG4 = 0x000c;   //VOW ADC test config
static int reg_AFE_VOW_CFG5 = 0x0000;   //N min value

static void  SavePowerState(void)
{
    int i = 0;
    for (i = 0; i < AUDIO_ANALOG_VOLUME_TYPE_MAX ; i++)
    {
        mCodec_data->mAudio_BackUpAna_DevicePower[i] = mCodec_data->mAudio_Ana_DevicePower[i];
    }
}

static void  RestorePowerState(void)
{
    int i = 0;
    for (i = 0; i < AUDIO_ANALOG_VOLUME_TYPE_MAX ; i++)
    {
        mCodec_data->mAudio_Ana_DevicePower[i] = mCodec_data->mAudio_BackUpAna_DevicePower[i];
    }
}


static bool mAnaSuspend = false;
void SetAnalogSuspend(bool bEnable)
{
    printk("+%s bEnable ==%d mAnaSuspend = %d \n", __func__, bEnable, mAnaSuspend);
    if ((bEnable == true) && (mAnaSuspend == false))
    {
        //Ana_Log_Print();
        SavePowerState();
        if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == true)
        {
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = false;
            Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1 , false);
        }
        if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] == true)
        {
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] = false;
            Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1 , false);
        }
        if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL] == true)
        {
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL] = false;
            Voice_Amp_Change(false);
        }
        if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] == true)
        {
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] = false;
            Speaker_Amp_Change(false);
        }
        //Ana_Log_Print();
        mAnaSuspend = true;
    }
    else if ((bEnable == false) && (mAnaSuspend == true))
    {
        //Ana_Log_Print();
        if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == true)
        {
            Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1 , true);
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = true;
        }
        if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] == true)
        {
            Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1 , true);
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] = false;
        }
        if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL] == true)
        {
            Voice_Amp_Change(true);
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL] = false;
        }
        if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] == true)
        {
            Speaker_Amp_Change(true);
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] = false;
        }
        RestorePowerState();
        //Ana_Log_Print();
        mAnaSuspend = false;
    }
    printk("-%s bEnable ==%d mAnaSuspend = %d \n", __func__, bEnable, mAnaSuspend);
}


static int audck_buf_Count = 0;
void audckbufEnable(bool enable)
{
    printk("audckbufEnable audck_buf_Count = %d enable = %d \n", audck_buf_Count, enable);
    mutex_lock(&Ana_buf_Ctrl_Mutex);
    if (enable)
    {
        if (audck_buf_Count == 0)
        {
            printk("+clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
            clk_buf_ctrl(CLK_BUF_AUDIO, true);
            printk("-clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
        }
        audck_buf_Count++;
    }
    else
    {
        audck_buf_Count--;
        if (audck_buf_Count == 0)
        {
            printk("+clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
            clk_buf_ctrl(CLK_BUF_AUDIO, false);
            printk("-clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
        }
        if (audck_buf_Count < 0)
        {
            printk("audck_buf_Count count <0 \n");
            audck_buf_Count = 0;
        }
    }
    mutex_unlock(&Ana_buf_Ctrl_Mutex);
}

static int ClsqAuxCount = 0;
static void ClsqAuxEnable(bool enable)
{
    printk("ClsqAuxEnable ClsqAuxCount = %d enable = %d \n", ClsqAuxCount, enable);
    mutex_lock(& AudAna_lock);
    if (enable)
    {
        if (ClsqAuxCount == 0)
        {
            Ana_Set_Reg(TOP_CLKSQ, 0x0002, 0x0002); //CKSQ Enable
        }
        ClsqAuxCount++;
    }
    else
    {
        ClsqAuxCount--;
        if (ClsqAuxCount < 0)
        {
            printk("ClsqAuxEnable count <0 \n");
            ClsqAuxCount = 0;
        }
        if (ClsqAuxCount == 0)
        {
            Ana_Set_Reg(TOP_CLKSQ, 0x0000, 0x0002);
        }
    }
    mutex_unlock(& AudAna_lock);
}

static int ClsqCount = 0;
static void ClsqEnable(bool enable)
{
    printk("ClsqEnable ClsqAuxCount = %d enable = %d \n", ClsqCount, enable);
    mutex_lock(& AudAna_lock);
    if (enable)
    {
        if (ClsqCount == 0)
        {
            Ana_Set_Reg(TOP_CLKSQ, 0x0001, 0x0001); //CKSQ Enable
        }
        ClsqCount++;
    }
    else
    {
        ClsqCount--;
        if (ClsqCount < 0)
        {
            printk("ClsqEnable count <0 \n");
            ClsqCount = 0;
        }
        if (ClsqCount == 0)
        {
            Ana_Set_Reg(TOP_CLKSQ, 0x0000, 0x0001);
        }
    }
    mutex_unlock(& AudAna_lock);
}

static int TopCkCount = 0;
static void Topck_Enable(bool enable)
{
    printk("Topck_Enable enable = %d TopCkCount = %d \n", enable, TopCkCount);
    mutex_lock(&Ana_Clk_Mutex);
    if (enable == true)
    {
        if (TopCkCount == 0)
        {
            Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x3000, 0x3000);  //AUD clock power down released
        }
        TopCkCount++;
    }
    else
    {
        TopCkCount--;
        if (TopCkCount == 0)
        {
            Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x3000, 0x3000);
        }
        if (TopCkCount <= 0)
        {
            printk("TopCkCount <0 =%d\n ", TopCkCount);
            TopCkCount = 0;
        }
    }
    mutex_unlock(&Ana_Clk_Mutex);
}

static int NvRegCount = 0;
static void NvregEnable(bool enable)
{
    printk("NvregEnable NvRegCount == %d enable = %d \n", NvRegCount, enable);
    mutex_lock(&Ana_Clk_Mutex);
    if (enable == true)
    {
        if (NvRegCount == 0)
        {
            Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0000, 0xffff);  //AUD clock power down released
        }
        NvRegCount++;
    }
    else
    {
        NvRegCount--;
        if (NvRegCount == 0)
        {
            Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0001, 0xffff);
        }
        if (NvRegCount < 0)
        {
            printk("NvRegCount <0 =%d\n ", NvRegCount);
            NvRegCount = 0;
        }
    }
    mutex_unlock(&Ana_Clk_Mutex);
}

static int AdcClockCount = 0;
static void AdcClockEnable(bool enable)
{
    mutex_lock(&Ana_Clk_Mutex);
    if (enable == true)
    {
        if (AdcClockCount == 0)
        {
            Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x3000, 0xffff);  //AUD clock power down released
        }
        AdcClockCount++;
    }
    else
    {
        AdcClockCount--;
        if (AdcClockCount == 0)
        {
            Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x3000, 0xffff);
        }
        if (AdcClockCount <= 0)
        {
            printk("TopCkCount <0 =%d\n ", AdcClockCount);
            AdcClockCount = 0;
        }
    }
    mutex_unlock(&Ana_Clk_Mutex);
}

#if defined (MTK_VOW_SUPPORT)
void vow_irq_handler(void)
{
    printk("audio irq event....\n");
    //TurnOnVOWADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
    TurnOnVOWDigitalHW(false);
#if defined(VOW_TONE_TEST)
    EnableSideGenHw(Soc_Aud_InterConnectionOutput_O03, Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
#endif
    VowDrv_ChangeStatus();
}
#endif

static int LowPowerAdcClockCount = 0;
static void LowPowerAdcClockEnable(bool enable)
{
    mutex_lock(&Ana_Clk_Mutex);
    if (enable == true)
    {
        if (LowPowerAdcClockCount == 0)
        {
            Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x8000, 0xffff);  //Lowpower AUD clock power down released
        }
        LowPowerAdcClockCount++;
    }
    else
    {
        LowPowerAdcClockCount--;
        if (LowPowerAdcClockCount == 0)
        {
            Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x8000, 0xffff);
        }
        if (LowPowerAdcClockCount < 0)
        {
            printk("LowPowerAdcClockCount <0 =%d\n ", LowPowerAdcClockCount);
            LowPowerAdcClockCount = 0;
        }
    }
    mutex_unlock(&Ana_Clk_Mutex);
}


#ifdef CONFIG_MTK_SPEAKER
static void Apply_Speaker_Gain(void)
{
    Ana_Set_Reg(SPK_CON9,  Speaker_pga_gain << 8, 0xf << 8);
}
#else
static void Apply_Speaker_Gain(void)
{

}
#endif

void setOffsetTrimMux(unsigned int Mux)
{
    printk("%s Mux = %d\n", __func__, Mux);
    Ana_Set_Reg(AUDBUF_CFG7 , Mux << 12, 0xf << 12); // buffer mux select
}

void setOffsetTrimBufferGain(unsigned int gain)
{
    Ana_Set_Reg(AUDBUF_CFG7 , gain << 10, 0x3 << 10); // buffer mux select
}

static int mHplTrimOffset = 2048;
static int mHprTrimOffset = 2048;

void SetHplTrimOffset(int Offset)
{
    printk("%s Offset = %d\n",__func__,Offset);
    mHplTrimOffset = Offset;
    if ((Offset > 2098) || (Offset < 1998))
    {
        mHplTrimOffset = 2048;
        printk("SetHplTrimOffset offset may be invalid offset = %d\n", Offset);
    }
}

void SetHprTrimOffset(int Offset)
{
    printk("%s Offset = %d\n",__func__,Offset);
    mHprTrimOffset = Offset;
    if ((Offset > 2098) || (Offset < 1998))
    {
        mHprTrimOffset = 2048;
        printk("SetHplTrimOffset offset may be invalid offset = %d\n", Offset);
    }
}

void EnableTrimbuffer(bool benable)
{
    if (benable == true)
    {
        Ana_Set_Reg(AUDBUF_CFG8 , 0x1 << 13, 0x1 << 13); // trim buffer enable
    }
    else
    {
        Ana_Set_Reg(AUDBUF_CFG8 , 0x0 << 13, 0x1 << 13); // trim buffer enable
    }
}

void OpenTrimBufferHardware(bool enable)
{
    if (enable)
    {
        TurnOnDacPower();
        printk("%s \n", __func__);
        Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0000, 0xffff); // Enable AUDGLB
        OpenClassAB();
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)
        Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
        msleep(1);
        Ana_Set_Reg(ZCD_CON0,   0x0000, 0xffff); // Disable AUD_ZCD_CFG0
        Ana_Set_Reg(AUDBUF_CFG0,   0xE008, 0xffff); // Disable headphone, voice and short-ckt protection.
        Ana_Set_Reg(IBIASDIST_CFG0,   0x0092, 0xffff); //Enable IBIST
        Ana_Set_Reg(ZCD_CON2,  0x0489 , 0xffff); // Set HPR/HPL gain as 0dB, step by step
        Ana_Set_Reg(ZCD_CON3,  0x001F , 0xffff); //Set voice gain as minimum (~ -40dB)
        Ana_Set_Reg(AUDBUF_CFG1,  0x0900 , 0xffff); //De_OSC of HP and enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG0,  0xE009 , 0xffff); //Enable voice driver
        Ana_Set_Reg(AUDBUF_CFG1,  0x0940 , 0xffff); //Enable pre-charge buffer
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501 , 0xffff); //Enable AUD_CLK
        Ana_Set_Reg(AUDDAC_CFG0, 0x000c , 0x000c); //Enable Audio DAC
        Ana_Set_Reg(AUDDAC_CFG0, 0x0003 , 0x0003); //Enable Audio DAC
    }
    else
    {
        printk("Audio_Amp_Change off amp\n");
        Ana_Set_Reg(AUDDAC_CFG0, 0x0000, 0xffff); // Disable Audio DAC
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5500, 0xffff); // Disable AUD_CLK
        Ana_Set_Reg(IBIASDIST_CFG0, 0x0192, 0xffff); // Disable IBIST
        Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0028, 0xffff); // Disable NV regulator (-1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0000, 0xffff); // Disable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDDAC_CFG0, 0x000e, 0xffff); // enable audio bias. enable audio DAC, HP buffers
        Ana_Set_Reg(AUDDAC_CFG0, 0x000d, 0xffff); // enable audio bias. enable audio DAC, HP buffers
        Ana_Set_Reg(AFE_DL_SRC2_CON0_L , 0x1800 , 0xffffffff);
        Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
        TurnOffDacPower();
    }
}


void OpenAnalogTrimHardware(bool enable)
{
    if (enable)
    {
        TurnOnDacPower();
        printk("%s \n", __func__);
        //Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0000, 0xffff); // Enable AUDGLB
        OpenClassAB();
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)
        Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
        msleep(1);
        Ana_Set_Reg(ZCD_CON0,   0x0000, 0xffff); // Disable AUD_ZCD_CFG0
        Ana_Set_Reg(AUDBUF_CFG0,   0xE008, 0xffff); // Disable headphone, voice and short-ckt protection.
        Ana_Set_Reg(IBIASDIST_CFG0,   0x0092, 0xffff); //Enable IBIST
        Ana_Set_Reg(ZCD_CON2,  0x0489 , 0xffff); // Set HPR/HPL gain as 0dB, step by step
        Ana_Set_Reg(ZCD_CON3,  0x001F , 0xffff); //Set voice gain as minimum (~ -40dB)
        Ana_Set_Reg(AUDBUF_CFG1,  0x0900 , 0xffff); //De_OSC of HP and enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG0,  0xE009 , 0xffff); //Enable voice driver
        Ana_Set_Reg(AUDBUF_CFG1,  0x0940 , 0xffff); //Enable pre-charge buffer
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501 , 0xffff); //Enable AUD_CLK
        Ana_Set_Reg(AUDDAC_CFG0, 0x000c , 0x000c); //Enable Audio DAC
        Ana_Set_Reg(AUDDAC_CFG0, 0x0003 , 0x0003); //Enable Audio DAC
    }
    else
    {
        printk("Audio_Amp_Change off amp\n");
        Ana_Set_Reg(AUDDAC_CFG0, 0x0000, 0xffff); // Disable Audio DAC
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5500, 0xffff); // Disable AUD_CLK
        Ana_Set_Reg(IBIASDIST_CFG0, 0x0192, 0xffff); // Disable IBIST
        Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0028, 0xffff); // Disable NV regulator (-1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0000, 0xffff); // Disable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDDAC_CFG0, 0x000e, 0xffff); // enable audio bias. enable audio DAC, HP buffers
        Ana_Set_Reg(AUDDAC_CFG0, 0x000d, 0xffff); // enable audio bias. enable audio DAC, HP buffers
        Ana_Set_Reg(AFE_DL_SRC2_CON0_L , 0x1800 , 0xffffffff);
        Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
        TurnOffDacPower();
    }
}

void OpenAnalogHeadphone(bool bEnable)
{
    printk("OpenAnalogHeadphone bEnable = %d", bEnable);
    if (bEnable)
    {
        SetHplTrimOffset(2048);
        SetHprTrimOffset(2048);
        mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 44100;
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1 , true);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = true;
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1 , true);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] = true;
    }
    else
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = false;
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1 , false);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] = false;
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1 , false);
    }
}

bool OpenHeadPhoneImpedanceSetting(bool bEnable)
{
    printk("%s benable = %d\n", __func__, bEnable);
    if (GetDLStatus() == true)
    {
        return false;
    }

    if (bEnable == true)
    {
        TurnOnDacPower();
        OpenClassAB();
        Ana_Set_Reg(AUDBUF_CFG5, 0x0003, 0x0003); // enable HP bias circuits
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)
        Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
        Ana_Set_Reg(AUDBUF_CFG0, 0xE000, 0xffff);
        Ana_Set_Reg(AUDBUF_CFG1, 0x0000, 0xffff);
        Ana_Set_Reg(AUDBUF_CFG8, 0x4000, 0xffff);
        Ana_Set_Reg(IBIASDIST_CFG0, 0x0092, 0xffff);
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501, 0xffff);
        Ana_Set_Reg(AUDDAC_CFG0, 0x0009, 0xffff);
        Ana_Set_Reg(AUDBUF_CFG6, 0x4800, 0xffff);
    }
    else
    {
        Ana_Set_Reg(AUDDAC_CFG0, 0x0000, 0xffff); // Disable Audio DAC
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5500, 0xffff); // Disable AUD_CLK
        Ana_Set_Reg(IBIASDIST_CFG0, 0x0192, 0xffff); // Disable IBIST
        Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0028, 0xffff); // Disable NV regulator (-1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0000, 0xffff); // Disable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDBUF_CFG8, 0x0000, 0xffff);
        Ana_Set_Reg(AUDBUF_CFG5, 0x0000, 0x0003); // disable HP bias circuits
        TurnOffDacPower();
        Ana_Set_Reg(AUDBUF_CFG6, 0x0000, 0xffff);
    }
    return true;
}

void setHpGainZero(void)
{
    Ana_Set_Reg(ZCD_CON2, 0x8 << 7, 0x0f80);
    Ana_Set_Reg(ZCD_CON2, 0x8 , 0x001f);
}

void SetSdmLevel(unsigned int level)
{
    Ana_Set_Reg(AFE_DL_SDM_CON1, level, 0xffffffff);
}


static void SetHprOffset(int OffsetTrimming)
{
    short Dccompsentation = 0;
    int DCoffsetValue = 0;
    unsigned short RegValue = 0;
    printk("%s OffsetTrimming = %d \n",__func__,OffsetTrimming);
    DCoffsetValue = OffsetTrimming *  1000000;
    DCoffsetValue = (DCoffsetValue / DC1devider);  // in uv
    printk("%s DCoffsetValue = %d \n",__func__,DCoffsetValue);
    DCoffsetValue = (DCoffsetValue / DC1unit_in_uv);
    printk("%s DCoffsetValue = %d \n",__func__,DCoffsetValue);
    Dccompsentation = DCoffsetValue;
    RegValue = Dccompsentation;
    printk("%s RegValue = 0x%x\n",__func__,RegValue);
    Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, RegValue , 0xffff);
}

static void SetHplOffset(int OffsetTrimming)
{
    short Dccompsentation = 0;
    int DCoffsetValue = 0;
    unsigned short RegValue = 0;
    printk("%s OffsetTrimming = %d \n",__func__,OffsetTrimming);
    DCoffsetValue = OffsetTrimming *1000000;
    DCoffsetValue = (DCoffsetValue / DC1devider);  // in uv
    printk("%s DCoffsetValue = %d \n",__func__,DCoffsetValue);
    DCoffsetValue = (DCoffsetValue / DC1unit_in_uv);
    printk("%s DCoffsetValue = %d \n",__func__,DCoffsetValue);
    Dccompsentation = DCoffsetValue;
    RegValue = Dccompsentation;
    printk("%s RegValue = 0x%x\n",__func__,RegValue);
    Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, RegValue  , 0xffff);
}

static void EnableDcCompensation(bool bEnable)
{
    if(bEnable == true)
    {
        Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, bEnable , 0x1);
    }
    else
    {
        Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, bEnable , 0x1);
    }
}

static void SetHprOffsetTrim(void)
{
    int OffsetTrimming = mHprTrimOffset - TrimOffset;
    printk("%s mHprTrimOffset = %d TrimOffset = %d \n",__func__,mHprTrimOffset,TrimOffset);
    SetHprOffset(OffsetTrimming);
}

static void SetHpLOffsetTrim(void)
{
    int OffsetTrimming = mHplTrimOffset - TrimOffset;
    printk("%s mHprTrimOffset = %d TrimOffset = %d \n",__func__,mHplTrimOffset,TrimOffset);
    SetHplOffset(OffsetTrimming);
}

static void SetDcCompenSation(void)
{
    SetHprOffsetTrim();
    SetHpLOffsetTrim();
    EnableDcCompensation(true);
}

static void OpenClassH(void)
{
    Ana_Set_Reg(AFE_CLASSH_CFG7, 0xAD2D, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG8, 0x1313, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG9, 0x132d, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG10, 0x2d13, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG11, 0x1315, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG12, 0x6464, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG13, 0x2a2a, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG14, 0x009c, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG26, 0x9313, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG27, 0x1313, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG28, 0x1315, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG29, 0x1515, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG30, 0x1515, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG1, 0xBF04, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG2, 0x5fbf, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG3, 0x1104, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG4, 0x2412, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG5, 0x0201, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG6, 0x2800, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG14, 0x009c, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG21, 0x2108, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG22, 0x06db, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG23, 0xffff , 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG24, 0x0bd6, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG25, 0x1740, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG0,   0xd608, 0xffff); // Classh CK fix 591KHz
    udelay(100);
    Ana_Set_Reg(AFE_CLASSH_CFG0,   0xd20d, 0xffff); // Classh CK fix 591KHz
    udelay(100);
}

static void OpenClassAB(void)
{
    Ana_Set_Reg(AFE_CLASSH_CFG7, 0x8909, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG8, 0x0909, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG9, 0x1309, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG10, 0x0909, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG11, 0x0915, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG12, 0x1414, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG13, 0x1414, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG14, 0x009c, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG26, 0x9313, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG27, 0x1313, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG28, 0x1315, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG29, 0x1515, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG30, 0x1515, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG1, 0x0024, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG2, 0x2f90, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG3, 0x1104, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG4, 0x2412, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG5, 0x0201, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG6, 0x2800, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG21, 0xa108, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG22, 0x06db, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG23, 0x0bd6, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG24, 0x1492, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG25, 0x1740, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG0,   0xd419, 0xffff); // Classh CK fix 591KHz
    Ana_Set_Reg(AFE_CLASSH_CFG1,   0x0021, 0xffff); // Classh CK fix 591KHz
}

static void SetDCcoupleNP(int ADCType, int mode)
{
    printk("%s ADCType= %d mode = %d\n", __func__, ADCType, mode);
#ifndef CONFIG_MTK_FPGA
    switch (mode)
    {
        case AUDIO_ANALOGUL_MODE_ACC:
        case AUDIO_ANALOGUL_MODE_DCC:
        case AUDIO_ANALOGUL_MODE_DMIC:
        {
            if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
            {
                mt6331_upmu_set_rg_audmicbias0dcswnen(false); // mic0 DC N external
                mt6331_upmu_set_rg_audmicbias0dcswpen(false); // mic0 DC P external
            }
            else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
            {
                mt6331_upmu_set_rg_audmicbias1dcswnen(false); // mic0 DC N external
                mt6331_upmu_set_rg_audmicbias1dcswpen(false); // mic0 DC P external
            }
        }
        break;
        case AUDIO_ANALOGUL_MODE_DCCECMDIFF:
        {
            if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
            {
                mt6331_upmu_set_rg_audmicbias0dcswnen(true); // mic0 DC N internal
                mt6331_upmu_set_rg_audmicbias0dcswpen(true); // mic0 DC P internal
            }
            else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
            {
                mt6331_upmu_set_rg_audmicbias1dcswnen(true); // mic0 DC N internal
                mt6331_upmu_set_rg_audmicbias1dcswpen(true); // mic0 DC P internal
            }
        }
        break;
        case AUDIO_ANALOGUL_MODE_DCCECMSINGLE:
        {
            if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
            {
                mt6331_upmu_set_rg_audmicbias0dcswnen(false); // mic0 DC N internal
                mt6331_upmu_set_rg_audmicbias0dcswpen(true); // mic0 DC P internal
            }
            else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
            {
                mt6331_upmu_set_rg_audmicbias1dcswnen(true); // mic0 DC N internal
                mt6331_upmu_set_rg_audmicbias1dcswpen(false); // mic0 DC P internal
            }
        }
        break;
        default:
            break;
    }
#endif
}

static void OpenMicbias3(bool bEnable)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, (bEnable << 7), (0x1 << 7));
}

static void OpenMicbias2(bool bEnable)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, (bEnable << 0), (0x1 << 0));
}

static void SetMicVref2(uint32_t vRef)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, vRef << 1, 0x7 << 1);
}

static void SetMicVref3(uint32_t vRef)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, vRef << 8, 0x7 << 8);
}

static void EnableMicBias(uint32_t Mic, bool bEnable)
{
    if (bEnable == true)
    {
        switch (Mic)
        {
            case AUDIO_ANALOG_DEVICE_IN_ADC1:
                OpenMicbias0(true);
                OpenMicbias1(true);
                break;
            case AUDIO_ANALOG_DEVICE_IN_ADC2:
                OpenMicbias0(true);
                OpenMicbias1(true);
                OpenMicbias2(true);
                break;
            case AUDIO_ANALOG_DEVICE_IN_ADC3:
            case AUDIO_ANALOG_DEVICE_IN_ADC4:
                OpenMicbias3(true);
                break;
        }
    }
    else
    {
        switch (Mic)
        {
            case AUDIO_ANALOG_DEVICE_IN_ADC1:
                OpenMicbias0(false);
                OpenMicbias1(false);
                break;
            case AUDIO_ANALOG_DEVICE_IN_ADC2:
                OpenMicbias0(false);
                OpenMicbias1(false);
                OpenMicbias2(false);
                break;
            case AUDIO_ANALOG_DEVICE_IN_ADC3:
            case AUDIO_ANALOG_DEVICE_IN_ADC4:
                OpenMicbias3(false);
                break;
        }
    }
}

static void SetMic2DCcoupleSwitch(bool internal)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, internal << 4, 0x1 << 4);
    Ana_Set_Reg(AUDMICBIAS_CFG1, internal << 5, 0x1 << 5);
}

static void SetMic3DCcoupleSwitch(bool internal)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, internal << 11, 0x1 << 4);
    Ana_Set_Reg(AUDMICBIAS_CFG1, internal << 12, 0x1 << 5);
}

/*static void SetMic2DCcoupleSwitchSingle(bool internal)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, 1 << 4, 0x1 << 4);
    Ana_Set_Reg(AUDMICBIAS_CFG1, 0 << 5, 0x1 << 5);
}

static void SetMic3DCcoupleSwitchSingle(bool internal)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, 1 << 11, 0x1 << 4);
    Ana_Set_Reg(AUDMICBIAS_CFG1, 0 << 12, 0x1 << 5);
}*/

static void SetMic2powermode(bool lowpower)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, lowpower << 6, 0x1 << 6);
}

static void SetMic3powermode(bool lowpower)
{
    Ana_Set_Reg(AUDMICBIAS_CFG1, lowpower << 13, 0x1 << 13);
}


static void OpenMicbias1(bool bEnable)
{
    printk("%s bEnable = %d \n", __func__, bEnable);
#ifndef CONFIG_MTK_FPGA
    if (bEnable == true)
    {
        mt6331_upmu_set_rg_audpwdbmicbias1(true); // mic bias power 1 on
    }
    else
    {
        mt6331_upmu_set_rg_audmicbias1lowpen(true); // mic 1 low power mode
        mt6331_upmu_set_rg_audpwdbmicbias1(false); // mic bias power 1 off
    }
#endif
}

static void SetMicbias1lowpower(bool benable)
{
#ifndef CONFIG_MTK_FPGA
    mt6331_upmu_set_rg_audmicbias1lowpen(benable); // mic 1 power mode
#endif
}

static void OpenMicbias0(bool bEanble)
{
    printk("%s bEanble = %d\n", __func__, bEanble);
#ifndef CONFIG_MTK_FPGA
    if (bEanble == true)
    {
        mt6331_upmu_set_rg_audpwdbmicbias0(true); // mic bias power 0 on
        mt6331_upmu_set_rg_audmicbias0vref(0x2); // set to 1.9V
    }
    else
    {
        mt6331_upmu_set_rg_audmicbias0lowpen(true); // mic 0 low power mode
        mt6331_upmu_set_rg_audpwdbmicbias0(false); // mic bias power 0 off
    }
#endif
}

static void SetMicbias0lowpower(bool benable)
{
#ifndef CONFIG_MTK_FPGA
    mt6331_upmu_set_rg_audmicbias0lowpen(benable); // mic 1 power mode
#endif
}



/*static bool Dl_Hpdet_impedence(void)
{
    ClsqAuxEnable(true);
    ClsqEnable(true);
    Topck_Enable(true);
    NvregEnable(true);
    Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock
    OpenClassH();
    Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0028, 0xffff); //Enable cap-less LDOs (1.6V)
    Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0068, 0xffff); //Enable NV regulator (-1.6V)
    Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
    Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff); //Disable AUD_ZCD
    Ana_Set_Reg(AUDBUF_CFG0, 0xE000, 0xffff); //Disable headphone, voice and short-ckt protection.
    Ana_Set_Reg(AUDBUF_CFG1, 0x0000, 0xffff); //De_OSC of HP and output STBENH disable
    Ana_Set_Reg(AUDBUF_CFG8, 0x4000, 0xffff); //HPDET circuit enable
    Ana_Set_Reg(IBIASDIST_CFG0, 0x0092, 0xffff); //Enable IBIST
    Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501, 0xffff); //Enable AUD_CLK
    Ana_Set_Reg(AUDDAC_CFG0, 0x0009, 0xffff); //Enable Audio DAC
    Ana_Set_Reg(AUDCLKGEN_CFG0, 0x4800, 0xffff); //Select HPR as HPDET output and select DACLP as HPDET circuit input
    //Hereafter, use AUXADC for HP impedance detection , start ADC....


    ClsqAuxEnable(false);
    ClsqEnable(false);
    Topck_Enable(false);
    NvregEnable(false);
    return true;
}*/


static uint32 GetULNewIFFrequency(uint32 frequency)
{
    uint32 Reg_value = 0;
    switch (frequency)
    {
        case 8000:
        case 16000:
        case 32000:
            Reg_value = 1;
            break;
        case 48000:
            Reg_value = 3;
        default:
            break;
    }
    return Reg_value;
}

uint32 GetULFrequency(uint32 frequency)
{
    uint32 Reg_value = 0;
    printk("%s frequency =%d\n", __func__, frequency);
    switch (frequency)
    {
        case 8000:
        case 16000:
        case 32000:
            Reg_value = 0x0;
            break;
        case 48000:
            Reg_value = 0x1;
        default:
            break;

    }
    return Reg_value;
}


uint32 ULSampleRateTransform(uint32 SampleRate)
{
    switch (SampleRate)
    {
        case 8000:
            return 0;
        case 16000:
            return 1;
        case 32000:
            return 2;
        case 48000:
            return 3;
        default:
            break;
    }
    return 0;
}


static int mt63xx_codec_startup(struct snd_pcm_substream *substream , struct snd_soc_dai *Daiport)
{
    //printk("+mt63xx_codec_startup name = %s number = %d\n", substream->name, substream->number);
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && substream->runtime->rate)
    {
        //printk("mt63xx_codec_startup set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n", substream->runtime->rate);
        mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

    }
    else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && substream->runtime->rate)
    {
        //printk("mt63xx_codec_startup set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n", substream->runtime->rate);
        mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
    }
    //printk("-mt63xx_codec_startup name = %s number = %d\n", substream->name, substream->number);
    return 0;
}

static int mt63xx_codec_prepare(struct snd_pcm_substream *substream , struct snd_soc_dai *Daiport)
{
    //printk("mt63xx_codec_prepare set up  rate = %d\n", substream->runtime->rate);
    if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        printk("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n", substream->runtime->rate);
        mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

    }
    else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        printk("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n", substream->runtime->rate);
        mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
    }
    return 0;
}

static int mt6323_codec_trigger(struct snd_pcm_substream *substream , int command , struct snd_soc_dai *Daiport)
{
    switch (command)
    {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_SUSPEND:
            break;
    }

    //printk("mt6323_codec_trigger command = %d\n ", command);
    return 0;
}

static const struct snd_soc_dai_ops mt6323_aif1_dai_ops =
{
    .startup    = mt63xx_codec_startup,
    .prepare   = mt63xx_codec_prepare,
    .trigger     = mt6323_codec_trigger,
};

static struct snd_soc_dai_driver mtk_6331_dai_codecs[] =
{
    {
        .name = MT_SOC_CODEC_TXDAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_DL1_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_RXDAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .capture = {
            .stream_name = MT_SOC_UL1_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_TDMRX_DAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .capture = {
            .stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_192000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_I2S0TXDAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_I2SDL1_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_192000,
            .formats = SND_SOC_ADV_MT_FMTS,
        }
    },
    {
        .name = MT_SOC_CODEC_PCMTXDAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_PCM2_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
        .capture = {
            .stream_name = MT_SOC_PCM2_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_PCMRXDAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_PCM1_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
        .capture = {
            .stream_name = MT_SOC_PCM1_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_VOICECALLEXTINTDAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_VOICE_EXTINT_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SNDRV_PCM_FMTBIT_S16_LE,
        },
        .capture = {
            .stream_name = MT_SOC_VOICE_EXTINT_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SNDRV_PCM_FMTBIT_S16_LE,
        },
    },
    {
        .name = MT_SOC_CODEC_FMI2S2RXDAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_FM_I2S2_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
        .capture = {
            .stream_name = MT_SOC_FM_I2S2_RECORD_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_44100,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_ULDLLOOPBACK_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
        .capture = {
            .stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_STUB_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_ROUTING_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_RXDAI2_NAME,
        .capture = {
            .stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_MRGRX_DAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_MRGRX_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 8,
            .rates = SNDRV_PCM_RATE_8000_192000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
        .capture = {
            .stream_name = MT_SOC_MRGRX_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 8,
            .rates = SNDRV_PCM_RATE_8000_192000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
    {
        .name = MT_SOC_CODEC_HP_IMPEDANCE_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .playback = {
            .stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
   {
        .name = MT_SOC_CODEC_MOD_DAI_NAME,
        .ops = &mt6323_aif1_dai_ops,
        .capture = {
            .stream_name = MT_SOC_MODDAI_STREAM_NAME,
            .channels_min = 1,
            .channels_max = 2,
            .rates = SNDRV_PCM_RATE_8000_48000,
            .formats = SND_SOC_ADV_MT_FMTS,
        },
    },
};


uint32 GetDLNewIFFrequency(unsigned int frequency)
{
    uint32 Reg_value = 0;
    //printk("AudioPlatformDevice ApplyDLNewIFFrequency ApplyDLNewIFFrequency = %d", frequency);
    switch (frequency)
    {
        case 8000:
            Reg_value = 0;
            break;
        case 11025:
            Reg_value = 1;
            break;
        case 12000:
            Reg_value = 2;
            break;
        case 16000:
            Reg_value = 3;
            break;
        case 22050:
            Reg_value = 4;
            break;
        case 24000:
            Reg_value = 5;
            break;
        case 32000:
            Reg_value = 6;
            break;
        case 44100:
            Reg_value = 7;
            break;
        case 48000:
            Reg_value = 8;
        default:
            printk("ApplyDLNewIFFrequency with frequency = %d", frequency);
    }
    return Reg_value;
}

static bool GetDLStatus(void)
{
    int i = 0;
    for (i = 0; i < AUDIO_ANALOG_DEVICE_2IN1_SPK ; i++)
    {
        if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
        {
            return true;
        }
    }
    return false;
}

static void TurnOnDacPower(void)
{
    printk("TurnOnDacPower\n");
    audckbufEnable(true);
    ClsqEnable(true);
    Topck_Enable(true);
    NvregEnable(true);
    if ((GetAdcStatus() == false))
    {
        Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x003a, 0xffff);   //power on clock
    }
    else
    {
        Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock
    }
    Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffffffff);
    Ana_Set_Reg(AFUNC_AUD_CON0, 0xc3a1, 0xffffffff); //sdm audio fifo clock power on
    Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffffffff); //sdm power on
    Ana_Set_Reg(AFUNC_AUD_CON2, 0x000b, 0xffffffff); //sdm fifo enable
    Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffffffff); //set attenuation gainQuant
    Ana_Set_Reg(AFE_UL_DL_CON0 , 0x0001, 0xffffffff); //[0] afe enable

    Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0 , GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12 | 0x330 , 0xffffffff);
    Ana_Set_Reg(AFE_DL_SRC2_CON0_H , GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12 | 0x330 , 0xffffffff);

    Ana_Set_Reg(AFE_DL_SRC2_CON0_L , 0x1801 , 0xffffffff); //turn off mute function and turn on dl
    Ana_Set_Reg(PMIC_AFE_TOP_CON0 , 0x0000 , 0xffffffff); //set DL in normal path, not from sine gen table

}

static void TurnOffDacPower(void)
{
    printk("TurnOffDacPower\n");

    Ana_Set_Reg(AFE_DL_SRC2_CON0_L , 0x0000 , 0xffff); //bit0, Turn off down-link
    if (GetAdcStatus() == false)
    {
        Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
    }
    udelay(250);

    Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0x0040); //down-link power down

    Ana_Set_Reg(AFE_CLASSH_CFG1, 0x24, 0xffff);
    Ana_Set_Reg(AFE_CLASSH_CFG0, 0xd518, 0xffff); // ClassH off
    Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0, 0xffff); // NCP off
    ClsqEnable(false);
    Topck_Enable(false);
    NvregEnable(false);
    audckbufEnable(false);
}

static void HeadsetVoloumeRestore(void)
{
    int index = 0,oldindex = 0,offset =0,count =1;
    printk("%s\n", __func__);
    index =   8 ;
    oldindex = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
    if(index  > oldindex)
    {
        printk("index = %d oldindex = %d \n",index,oldindex);
        offset = index - oldindex;
        while(offset >0)
        {
            Ana_Set_Reg(ZCD_CON2, ((oldindex + count)<<7 )|(oldindex + count) , 0xf9f);
            offset--;
            count++;
            udelay(100);
        }
    }
    else
    {
        printk("index = %d oldindex = %d \n",index,oldindex);
        offset = oldindex - index;
        while(offset >0)
        {
            Ana_Set_Reg(ZCD_CON2, ((oldindex-count)<<7)|(oldindex-count) , 0xf9f);
            offset--;
            count++;
            udelay(100);
        }
    }
    Ana_Set_Reg(ZCD_CON2, 0x489, 0xf9f);
}

static void HeadsetVoloumeSet(void)
{
    int index = 0,oldindex = 0,offset =0, count =1;
    printk("%s\n", __func__);
    index =   mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
    oldindex = 8;
    if(index  > oldindex)
    {
        printk("index = %d oldindex = %d \n",index,oldindex);
        offset = index - oldindex;
        while(offset >0)
        {
            Ana_Set_Reg(ZCD_CON2, ((oldindex + count)<<7 )|(oldindex + count) , 0xf9f);
            offset--;
            count++;
            udelay(200);
        }
    }
    else
    {
        printk("index = %d oldindex = %d \n",index,oldindex);
        offset = oldindex - index;
        while(offset >0)
        {
            Ana_Set_Reg(ZCD_CON2, ((oldindex-count)<<7)|(oldindex-count) , 0xf9f);
            offset--;
            count++;
            udelay(200);
        }
    }
    Ana_Set_Reg(ZCD_CON2, (index << 7) | (index), 0xf9f);
}

static void Audio_Amp_Change(int channels , bool enable)
{
    if (enable)
    {
        if (GetDLStatus() == false)
        {
            TurnOnDacPower();
        }
        // here pmic analog control
        if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == false &&
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] == false)
        {
            printk("%s \n", __func__);

            OpenClassH();

            Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
            Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)

            Ana_Set_Reg(ZCD_CON0,   0x0000, 0xffff); // Disable AUD_ZCD_CFG0
            Ana_Set_Reg(AUDBUF_CFG0,   0xE000, 0xffff); // Disable headphone, voice and short-ckt protection.
            Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x8068, 0xffff); // Enable AU_REFN short circuit
            Ana_Set_Reg(IBIASDIST_CFG0,   0x0092, 0xffff); //Enable IBIST
            Ana_Set_Reg(AUDBUF_CFG5, 0x0007, 0x0007); // enable HP bias circuits

            Ana_Set_Reg(ZCD_CON2,  0x0489 , 0xffff); // Set HPR/HPL gain as 0dB, step by step
            Ana_Set_Reg(ZCD_CON3,  0x001F , 0xffff); //Set voice gain as minimum (~ -40dB)
            Ana_Set_Reg(AUDBUF_CFG1,  0x900 , 0xffff); //De_OSC of HP and enable output STBENH
            Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
            Ana_Set_Reg(AUDBUF_CFG0,  0xE001 , 0xffff); //Enable voice driver
            Ana_Set_Reg(AUDBUF_CFG1,  0x940 , 0xffff); //Enable pre-charge buffer
            Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501 , 0xffff); //Enable AUD_CLK
            Ana_Set_Reg(AUDDAC_CFG0, 0x000f , 0x000f); //Enable Audio DAC_DL_PGA_Handset_GAIN
            udelay(100);
            Ana_Set_Reg(AUDBUF_CFG0, 0xE141 , 0xffff);
            Ana_Set_Reg(ZCD_CON2, 0x0489 , 0xffff); //Set HPR/HPL gain as 0dB gain
            SetDcCompenSation();
            udelay(100);
            // here may cause pop
            Ana_Set_Reg(AUDBUF_CFG0, 0xe147 , 0xffff); // Enable HPR/HPL
            Ana_Set_Reg(AUDBUF_CFG0, 0xE14e , 0xffff); // Enable HPR/HPL
            Ana_Set_Reg(AUDBUF_CFG5, 0x0003, 0x0007); // enable HP bias circuits
			#ifdef CONFIG_CM865_MAINBOARD
            Ana_Set_Reg(AUDBUF_CFG1, 0x0100 , 0xffff); // Disable pre-charge buffer
			#else
            Ana_Set_Reg(AUDBUF_CFG1, 0x0900 , 0xffff); // Disable pre-charge buffer
			#endif
            Ana_Set_Reg(AUDBUF_CFG2, 0x0020 , 0xffff); // Disable De_OSC of voice
            Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff);//Disable AU_REFN short circuit

            // apply volume setting
            HeadsetVoloumeSet();
        }
        else if (channels == AUDIO_ANALOG_CHANNELS_LEFT1)
        {
            //Ana_Set_Reg(AUDDAC_CFG0, 0x000f, 0xffff); // enable audio bias. enable audio DAC, HP buffers

        }
        else if (channels == AUDIO_ANALOG_CHANNELS_RIGHT1)
        {
            //Ana_Set_Reg(AUDDAC_CFG0, 0x000f, 0xffff); // enable audio bias. enable audio DAC, HP buffers
        }
    }
    else
    {

        if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == false &&
            mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] == false)
        {
            printk("Audio_Amp_Change off amp\n");
            HeadsetVoloumeRestore();// Set HPR/HPL gain as 0dB, step by step
            //Ana_Set_Reg(ZCD_CON2,  0x0489 , 0xffff);
            Ana_Set_Reg(AUDBUF_CFG0, 0xE148, 0xffff); // Disable HPR/HPL
            Ana_Set_Reg(AUDDAC_CFG0, 0x0000, 0xffff); // Disable Audio DAC
            Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5500, 0xffff); // Disable AUD_CLK
            Ana_Set_Reg(IBIASDIST_CFG0, 0x0192, 0xffff); // Disable IBIST
            Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0028, 0xffff); // Disable NV regulator (-1.6V)
            Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0000, 0xffff); // Disable cap-less LDOs (1.6V)
            Ana_Set_Reg(AUDBUF_CFG5, 0x0000, 0x0003); // enable HP bias circuits
            EnableDcCompensation(false);
        }
        else if (channels == AUDIO_ANALOG_CHANNELS_LEFT1)
        {
            //Ana_Set_Reg(AUDDAC_CFG0, 0x000e, 0xffff); // enable audio bias. enable audio DAC, HP buffers
        }
        else if (channels == AUDIO_ANALOG_CHANNELS_RIGHT1)
        {
            //Ana_Set_Reg(AUDDAC_CFG0, 0x000d, 0xffff); // enable audio bias. enable audio DAC, HP buffers
        }
        if (GetDLStatus() == false)
        {
            Ana_Set_Reg(AFE_DL_SRC2_CON0_L , 0x1800 , 0xffffffff);
            if (GetAdcStatus() == false)
            {
                Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
            }
            TurnOffDacPower();
        }
    }
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_AmpL_Get = %d\n", mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL];
    return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    mutex_lock(&Ana_Ctrl_Mutex);

    printk("%s() gain = %ld \n ", __func__, ucontrol->value.integer.value[0]);
    if ((ucontrol->value.integer.value[0]  == true) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL]  == false))
    {
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1 , true);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = ucontrol->value.integer.value[0];
    }
    else if ((ucontrol->value.integer.value[0]  == false) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL]  == true))
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = ucontrol->value.integer.value[0];
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1 , false);
    }
    mutex_unlock(&Ana_Ctrl_Mutex);
    return 0;
}

static int Audio_AmpR_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_AmpR_Get = %d\n", mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR];
    return 0;
}

static int Audio_AmpR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    mutex_lock(&Ana_Ctrl_Mutex);

    printk("%s()\n", __func__);
    if ((ucontrol->value.integer.value[0]  == true) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR]  == false))
    {
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1 , true);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] = ucontrol->value.integer.value[0];
    }
    else if ((ucontrol->value.integer.value[0]  == false) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR]  == true))
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] = ucontrol->value.integer.value[0];
        Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1 , false);
    }
    mutex_unlock(&Ana_Ctrl_Mutex);
    return 0;
}

/*static void  SetVoiceAmpVolume(void)
{
    int index;
    printk("%s\n", __func__);
    index =  mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
    Ana_Set_Reg(ZCD_CON3, index , 0x001f);
}*/

static void Voice_Amp_Change(bool enable)
{
    if (enable)
    {
        printk("turn on ampL\n");
        if (GetDLStatus() == false)
        {
            Ana_Set_Reg(0x0680, 0x0000, 0xffff); // Enable AUDGLB
            Ana_Set_Reg(TOP_CKSEL_CON_CLR, 0x0001, 0x0001); //use internal 26M
            TurnOnDacPower();
            printk("Voice_Amp_Change on amp\n");
            OpenClassAB();
            Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
            Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)
            Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
            Ana_Set_Reg(ZCD_CON0,   0x0700, 0xffff); // Disable AUD_ZCD
            Ana_Set_Reg(AUDBUF_CFG0,   0xE008, 0xffff); // Disable headphone, voice and short-ckt protection.
            Ana_Set_Reg(IBIASDIST_CFG0,   0x0092, 0xffff); //Enable IBIST
            Ana_Set_Reg(ZCD_CON3,  0x001F , 0xffff); //Set voice gain as minimum (~ -40dB)
            Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
            Ana_Set_Reg(AUDCLKGEN_CFG0,  0x5501 , 0xffff); //Enable voice driver
            Ana_Set_Reg(AUDDAC_CFG0,  0x0009 , 0xffff); //Switch voice MUX to audio DAC
            Ana_Set_Reg(AUDBUF_CFG0,  0xE010 , 0xffff); //Enable voice driver
            Ana_Set_Reg(AUDBUF_CFG0,  0xE011 , 0xffff); //Enable voice driver
            Ana_Set_Reg(ZCD_CON3,  0x0009 , 0xffff); //Set voice gain as 0dB
        }
    }
    else
    {
        printk("turn off ampL\n");
        if (GetDLStatus() == false)
        {
            TurnOffDacPower();
            Ana_Set_Reg(AUDBUF_CFG0,  0xE010 , 0xffff); //Disable voice driver
            Ana_Set_Reg(AUDDAC_CFG0,  0x0000 , 0xffff); //Disable L-ch Audio DAC
            Ana_Set_Reg(AUDCLKGEN_CFG0,  0x5500 , 0xffff); //Disable AUD_CLK
            Ana_Set_Reg(IBIASDIST_CFG0,  0x0192 , 0xffff); //Disable IBIST
            Ana_Set_Reg(AUDLDO_NVREG_CFG0,  0x0028 , 0xffff); //Disable NV regulator (-1.6V)
            Ana_Set_Reg(AUDLDO_NVREG_CFG0,  0x0000 , 0xffff); //Disable cap-less LDOs (1.6V)
        }
    }
}

static int Voice_Amp_Get(struct snd_kcontrol *kcontrol,
                         struct snd_ctl_elem_value *ucontrol)
{
    printk("Voice_Amp_Get = %d\n", mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL];
    return 0;
}

static int Voice_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    mutex_lock(&Ana_Ctrl_Mutex);
    printk("%s()\n", __func__);
    if ((ucontrol->value.integer.value[0]  == true) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL]  == false))
    {
        Voice_Amp_Change(true);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL] = ucontrol->value.integer.value[0];
    }
    else if ((ucontrol->value.integer.value[0]  == false) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL]  == true))
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HSOUTL] = ucontrol->value.integer.value[0];
        Voice_Amp_Change(false);
    }
    mutex_unlock(&Ana_Ctrl_Mutex);
    return 0;
}

static void Speaker_Amp_Change(bool enable)
{
    if (enable)
    {
        if (GetDLStatus() == false)
        {
            TurnOnDacPower();
        }
        printk("turn on Speaker_Amp_Change \n");
            mt_set_gpio_dir(GPIO_SPEAKER_EN_PIN,GPIO_DIR_OUT); // output
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // high
    msleep(10); 
   mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
       mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
      mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
      mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
      mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high

        // here pmic analog control
        //Ana_Set_Reg(AUDNVREGGLB_CFG0  , 0x0000 , 0xffffffff);
        OpenClassAB();
        Ana_Set_Reg(AUDLDO_NVREG_CFG0 , 0x0028 , 0xffffffff); //Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0 , 0x0068 , 0xffffffff); //Enable NV regulator (-1.6V)
        Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
        Ana_Set_Reg(ZCD_CON0  , 0x0700 , 0xffffffff); //Disable AUD_ZCD
        Ana_Set_Reg(AUDBUF_CFG6  , 0x00C0 , 0xffffffff); // Disable line-out and short-ckt protection. LO MUX is opened
        Ana_Set_Reg(IBIASDIST_CFG0  , 0x0092 , 0xffffffff); // Enable IBIST
        Ana_Set_Reg(ZCD_CON1  , 0x0F9F , 0xffffffff); // Set LOR/LOL gain as minimum (~ -40dB)
        Ana_Set_Reg(AUDBUF_CFG7  , 0x0102 , 0xffffffff); // De_OSC of LO and enable output STBENH
        Ana_Set_Reg(AUDCLKGEN_CFG0  , 0x5501 , 0xffffffff); // Enable AUD_CLK
        Ana_Set_Reg(AUDDAC_CFG0  , 0x000F , 0xffffffff); //Enable Audio DAC
        SetDcCompenSation();
        Ana_Set_Reg(AUDBUF_CFG6  , 0x00E8 , 0xffffffff); //Switch LO MUX to audio DAC
        Ana_Set_Reg(AUDBUF_CFG6  , 0x00EB , 0xffffffff); //Enable LOR/LOL
        Ana_Set_Reg(ZCD_CON1  , 0x0489 , 0xffffffff); // Set LOR/LOL gain as 0dB
#ifdef CONFIG_MTK_SPEAKER
        if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
        {
            Speaker_ClassD_Open();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
        {
            Speaker_ClassAB_Open();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
        {
            Speaker_ReveiverMode_Open();
        }
#endif
        Apply_Speaker_Gain();
 	msleep(100); //dengbing add 增加这一句延时 
    }
    else
    {
        printk("turn off1 Speaker_Amp_Change \n");
    mt_set_gpio_dir(GPIO_SPEAKER_EN_PIN,GPIO_DIR_OUT); // output
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // high
#ifdef CONFIG_MTK_SPEAKER
        if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
        {
            Speaker_ClassD_close();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
        {
            Speaker_ClassAB_close();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
        {
            Speaker_ReveiverMode_close();
        }
#endif
        if (GetDLStatus() == false)
        {
            TurnOffDacPower();
        }
        Ana_Set_Reg(AUDBUF_CFG6  , 0x00E8 , 0xffffffff); // Disable LOR/LOL
        Ana_Set_Reg(AUDBUF_CFG6  , 0x0000 , 0xffffffff);
        Ana_Set_Reg(AUDBUF_CFG7  , 0x0900 , 0xffffffff);
        Ana_Set_Reg(AUDDAC_CFG0  , 0x0000 , 0xffffffff); // Disable Audio DAC
        Ana_Set_Reg(AUDCLKGEN_CFG0  , 0x5500 , 0xffffffff); // Disable AUD_CLK
        Ana_Set_Reg(IBIASDIST_CFG0  , 0x0192 , 0xffffffff); // Disable IBIST
        Ana_Set_Reg(AUDLDO_NVREG_CFG0  , 0x0028 , 0xffffffff); // Disable NV regulator (-1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0  , 0x0000 , 0xffffffff); // Disable cap-less LDOs (1.6V)
    }
}

static int Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
                           struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] ;
    return 0;
}

static int Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

    printk("%s() value = %ld \n ", __func__, ucontrol->value.integer.value[0]);
    if ((ucontrol->value.integer.value[0] == true) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL]  == false))
    {
        Speaker_Amp_Change(true);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] = ucontrol->value.integer.value[0];
    }
    else if ((ucontrol->value.integer.value[0] == false) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL]  == true))
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] = ucontrol->value.integer.value[0];
        Speaker_Amp_Change(false);
    }
    return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
    if (enable)
    {
        if (GetDLStatus() == false)
        {
            TurnOnDacPower();
        }
        printk("turn on Speaker_Amp_Change \n");
		//add by dingyin 0714
		//printk("turn on Speaker_Amp_Change \n");
            mt_set_gpio_dir(GPIO_SPEAKER_EN_PIN,GPIO_DIR_OUT); // output
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // high
    msleep(10); 
   mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
       mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
      mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
      mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
 udelay(2);
      mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // low
 udelay(2);
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ONE); // high
    //end by dingyin 0714
        // here pmic analog control
        //Ana_Set_Reg(AUDNVREGGLB_CFG0  , 0x0000 , 0xffffffff);
        OpenClassAB();

        Ana_Set_Reg(AUDLDO_NVREG_CFG0 , 0x0028 , 0xffffffff); //Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0 , 0x0068 , 0xffffffff); //Enable NV regulator (-1.6V)
        Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
        Ana_Set_Reg(ZCD_CON0  , 0x0700 , 0xffffffff); //Disable AUD_ZCD

        Ana_Set_Reg(AUDBUF_CFG0  , 0xE008 , 0xffffffff); //Disable headphone, voice and short-ckt protection.
        Ana_Set_Reg(AUDBUF_CFG6  , 0x00C0 , 0xffffffff); // Disable line-out and short-ckt protection. LO MUX is opened

        Ana_Set_Reg(IBIASDIST_CFG0  , 0x0092 , 0xffffffff); // Enable IBIST
        Ana_Set_Reg(ZCD_CON2  , 0x0489 , 0xffffffff); // Set LOR/LOL gain as minimum (~ -40dB)
        Ana_Set_Reg(ZCD_CON1  , 0x0489 , 0xffffffff); // Set LOR/LOL gain as minimum (~ -40dB)
        Ana_Set_Reg(ZCD_CON3 , 0x001F , 0xffffffff); // Set voice gain as minimum (~ -40dB)

        Ana_Set_Reg(AUDBUF_CFG1  , 0x0900 , 0xffffffff); // De_OSC of HP and enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG7  , 0x0102 , 0xffffffff); // De_OSC of LO and enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG2  , 0x0022 , 0xffffffff); // De_OSC of voice, enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG0  , 0xE009 , 0xffffffff); // Enable voice driver
        Ana_Set_Reg(AUDBUF_CFG1  , 0x0940 , 0xffffffff); // Enable pre-charge buffer_map_state
        msleep(1);

        Ana_Set_Reg(AUDCLKGEN_CFG0  , 0x5501 , 0xffffffff); // Enable AUD_CLK
        Ana_Set_Reg(AUDDAC_CFG0  , 0x000F , 0xffffffff); //Enable Audio DAC
        SetDcCompenSation();
        Ana_Set_Reg(AUDBUF_CFG6  , 0x00E8 , 0xffffffff); //Switch LO MUX to audio DAC
        Ana_Set_Reg(AUDBUF_CFG6  , 0x00EB , 0xffffffff); //Enable LOR/LOL
        Ana_Set_Reg(ZCD_CON1  , 0x0489 , 0xffffffff); // Set LOR/LOL gain as 0dB

        Ana_Set_Reg(AUDBUF_CFG0  , 0xE0A9 , 0xffffffff); // Switch HP MUX to audio DAC
        Ana_Set_Reg(AUDBUF_CFG0  , 0xE0AF , 0xffffffff); // Enable HPR/HPL
        #ifdef CONFIG_CM865_MAINBOARD
        Ana_Set_Reg(AUDBUF_CFG1, 0x0100 , 0xffff); // Disable pre-charge buffer
		#else
        Ana_Set_Reg(AUDBUF_CFG1, 0x0900 , 0xffff); // Disable pre-charge buffer
		#endif
        Ana_Set_Reg(AUDBUF_CFG2  , 0x0020 , 0xffffffff); // Disable De_OSC of voice
        Ana_Set_Reg(AUDBUF_CFG0  , 0xE0AE , 0xffffffff); // Disable voice buffer
        Ana_Set_Reg(AUDBUF_CFG2  , 0x0489 , 0xffffffff); // Set HPR/HPL gain as 0dB

#ifdef CONFIG_MTK_SPEAKER
        if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
        {
            Speaker_ClassD_Open();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
        {
            Speaker_ClassAB_Open();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
        {
            Speaker_ReveiverMode_Open();
        }
#endif
        HeadsetRVolumeSet();
        HeadsetLVolumeSet();
        Apply_Speaker_Gain();
    }
    else
    {

#ifdef CONFIG_MTK_SPEAKER
        if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
        {
            Speaker_ClassD_close();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
        {
            Speaker_ClassAB_close();
        }
        else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
        {
            Speaker_ReveiverMode_close();
        }
#endif

        Ana_Set_Reg(AUDBUF_CFG0  , 0xE149 , 0xffff); // Disable HPR/HPL
        Ana_Set_Reg(AUDDAC_CFG0  , 0x0000 , 0xffff); // Disable Audio DAC
        Ana_Set_Reg(AUDBUF_CFG0  , 0xE148 , 0xffff);
        Ana_Set_Reg(AUDBUF_CFG2  , 0x0020 , 0xffff);
        Ana_Set_Reg(AUDBUF_CFG6  , 0x0000 , 0xffff);
        Ana_Set_Reg(AUDBUF_CFG7  , 0x0902 , 0xffff);
        Ana_Set_Reg(AUDCLKGEN_CFG0  , 0x5500 , 0xffff); // Disable AUD_CLK
        Ana_Set_Reg(IBIASDIST_CFG0  , 0x0192 , 0xffff); // Disable IBIST
        Ana_Set_Reg(AUDLDO_NVREG_CFG0  , 0x0028 , 0xffff); // Disable NV regulator (-1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0  , 0x0000 , 0xffff); // Disable cap-less LDOs (1.6V)

        printk("turn off Speaker_Amp_Change \n");
		//add by dingyin 0714
	mt_set_gpio_dir(GPIO_SPEAKER_EN_PIN,GPIO_DIR_OUT); // output  
    mt_set_gpio_out(GPIO_SPEAKER_EN_PIN,GPIO_OUT_ZERO); // high
            //end by dingyin 0714
        if (GetDLStatus() == false)
        {
            Ana_Set_Reg(AFE_DL_SRC2_CON0_L , 0x1800 , 0xffff);
            if (GetAdcStatus() == false)
            {
                Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
            }
            TurnOffDacPower();
        }
    }

}


static int Headset_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
                                   struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_R] ;
    return 0;
}

static int Headset_Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    //struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

    printk("%s() gain = %lu \n ", __func__, ucontrol->value.integer.value[0]);
    if ((ucontrol->value.integer.value[0]  == true) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_R]  == false))
    {
        Headset_Speaker_Amp_Change(true);
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_R] = ucontrol->value.integer.value[0];
    }
    else if ((ucontrol->value.integer.value[0]  == false) && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_R]  == true))
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_R] = ucontrol->value.integer.value[0];
        Headset_Speaker_Amp_Change(false);
    }
    return 0;
}

static bool Ext_Speaker_Mode = false;
static int Audio_Ext_Speaker_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    // add fpr ext speaker control , ex: GPIO pull high / low
    Ext_Speaker_Mode = ucontrol->value.integer.value[0];
    printk("%s Ext_Speaker_Mode = %d",__func__,Ext_Speaker_Mode);
    if(Ext_Speaker_Mode == true) // on
    {

    }
    else // off
    {

    }
    return 0;
}

static int Audio_Ext_Speaker_Get(struct snd_kcontrol *kcontrol,
                                   struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = Ext_Speaker_Mode ;
    return 0;
}

static const char *Ext_speaker_amp_function[] = {"Off", "On"};
static const struct soc_enum Audio_Ext_dev_Enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Ext_speaker_amp_function), Ext_speaker_amp_function),
};

static const struct snd_kcontrol_new mt_ext_dev_controls[] =
{
    SOC_ENUM_EXT("Ext_Speaker_Amp_Switch", Audio_Ext_dev_Enum[0], Audio_Ext_Speaker_Get, Audio_Ext_Speaker_Set),
};
#ifdef CONFIG_MTK_SPEAKER
static const char *speaker_amp_function[] = {"CALSSD", "CLASSAB", "RECEIVER"};
static const char *speaker_PGA_function[] = {"MUTE", "0Db", "4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
                                             "11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
                                            };
static const char *speaker_OC_function[] = {"Off", "On"};
static const char *speaker_CS_function[] = {"Off", "On"};
static const char *speaker_CSPeakDetecReset_function[] = {"Off", "On"};

static int Audio_Speaker_Class_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    mutex_lock(&Ana_Ctrl_Mutex);
    Speaker_mode = ucontrol->value.integer.value[0];
    mutex_unlock(&Ana_Ctrl_Mutex);
    return 0;
}

static int Audio_Speaker_Class_Get(struct snd_kcontrol *kcontrol,
                                   struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = Speaker_mode ;
    return 0;
}

static int Audio_Speaker_Pga_Gain_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    Speaker_pga_gain = ucontrol->value.integer.value[0];
    Ana_Set_Reg(SPK_CON9,  (Speaker_pga_gain +1)<< 8, 0xf << 8);
    return 0;
}

static int Audio_Speaker_OcFlag_Get(struct snd_kcontrol *kcontrol,
                                    struct snd_ctl_elem_value *ucontrol)
{
    mSpeaker_Ocflag =  GetSpeakerOcFlag();
    ucontrol->value.integer.value[0] = mSpeaker_Ocflag ;
    return 0;
}

static int Audio_Speaker_OcFlag_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s is not support setting \n", __func__);
    return 0;
}

static int Audio_Speaker_Pga_Gain_Get(struct snd_kcontrol *kcontrol,
                                      struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = Speaker_pga_gain ;
    return 0;
}

static int Audio_Speaker_Current_Sensing_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    static int dauxadc1_tempval = 0;

    if (ucontrol->value.integer.value[0])
    {
        printk("enable Audio_Speaker_Current_Sensing_Set\n");
        //Ana_Set_Reg (SPK_CON12,  0x9300, 0xffff);CON12 invalid @ 6332
        Ana_Set_Reg(SPK_CON16,  0x8000, 0x8000); //[15]ISENSE enable
        Ana_Set_Reg(SPK_CON14,  0x0050, 0x0050); //[6]VSENSE enable
        mEnableAuxAdc = 1;
        dauxadc1_tempval = Ana_Get_Reg(0x886a);
        Ana_Set_Reg(0x886a, 0x00b0, 0xFFFF);    // Important
    }
    else
    {
        printk("disable Audio_Speaker_Current_Sensing_Set\n");
        //Ana_Set_Reg (SPK_CON12,  0x1300, 0xffff); CON12 invalid @ 6332
        Ana_Set_Reg(SPK_CON16,  0x0, 0x8000); //[15]ISENSE disable
        Ana_Set_Reg(SPK_CON14,  0x0, 0x0050); //[6]VSENSE enable
        Ana_Set_Reg(0x80be, 0x02, 0x02);
        Ana_Set_Reg(0x80c0, 0x02, 0x02);
        Ana_Set_Reg(MT6332_AUXADC_CON12, 0, 0x8000); //[15]Set Speaker mode
        Ana_Set_Reg(MT6332_AUXADC_CON0, 0x0000, 0x8000);
        Ana_Set_Reg(0x886a, dauxadc1_tempval, 0xFFFF);    // Important
        mEnableAuxAdc = 0;
    }
    return 0;
}

static int Audio_Speaker_Current_Sensing_Get(struct snd_kcontrol *kcontrol,
                                             struct snd_ctl_elem_value *ucontrol)
{
    //ucontrol->value.integer.value[0] = (Ana_Get_Reg (SPK_CON12)>>15)&0x01;
    ucontrol->value.integer.value[0] = (Ana_Get_Reg(SPK_CON16) >> 15) & 0x01; //[15]ISENSE
    return 0;
}

static int Audio_Speaker_Current_Sensing_Peak_Detector_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    /*
    if (ucontrol->value.integer.value[0])
        Ana_Set_Reg (SPK_CON12,  1<<14, 1<<14);
    else
        Ana_Set_Reg (SPK_CON12,  0, 1<<14);
        */
    return 0;
}

static int Audio_Speaker_Current_Sensing_Peak_Detector_Get(struct snd_kcontrol *kcontrol,
                                                           struct snd_ctl_elem_value *ucontrol)
{
    /*
    ucontrol->value.integer.value[0] = (Ana_Get_Reg (SPK_CON12)>>14)&0x01;
    */
    return 0;
}


static const struct soc_enum Audio_Speaker_Enum[] =
{
    // speaker class setting
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_amp_function), speaker_amp_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_PGA_function), speaker_PGA_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_OC_function), speaker_OC_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CS_function), speaker_CS_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CSPeakDetecReset_function), speaker_CSPeakDetecReset_function),
};

static const struct snd_kcontrol_new mt6331_snd_Speaker_controls[] =
{
    SOC_ENUM_EXT("Audio_Speaker_class_Switch", Audio_Speaker_Enum[0], Audio_Speaker_Class_Get, Audio_Speaker_Class_Set),
    SOC_ENUM_EXT("Audio_Speaker_PGA_gain", Audio_Speaker_Enum[1], Audio_Speaker_Pga_Gain_Get, Audio_Speaker_Pga_Gain_Set),
    SOC_ENUM_EXT("Audio_Speaker_OC_Falg", Audio_Speaker_Enum[2], Audio_Speaker_OcFlag_Get, Audio_Speaker_OcFlag_Set),
    SOC_ENUM_EXT("Audio_Speaker_CurrentSensing", Audio_Speaker_Enum[3], Audio_Speaker_Current_Sensing_Get, Audio_Speaker_Current_Sensing_Set),
    SOC_ENUM_EXT("Audio_Speaker_CurrentPeakDetector", Audio_Speaker_Enum[4], Audio_Speaker_Current_Sensing_Peak_Detector_Get, Audio_Speaker_Current_Sensing_Peak_Detector_Set),
};

#define HW_BUFFER_LENGTH 21
const uint32 V_Buffer_Table[HW_BUFFER_LENGTH] =
{
    MT6332_AUXADC_ADC19,
    MT6332_AUXADC_ADC20,
    MT6332_AUXADC_ADC21,
    MT6332_AUXADC_ADC22,
    MT6332_AUXADC_ADC23,
    MT6332_AUXADC_ADC24,
    MT6332_AUXADC_ADC25,
    MT6332_AUXADC_ADC26,
    MT6332_AUXADC_ADC27,
    MT6332_AUXADC_ADC28,
    MT6332_AUXADC_ADC29,
    MT6332_AUXADC_ADC30,
    MT6332_AUXADC_ADC0,
    MT6332_AUXADC_ADC1,
    MT6332_AUXADC_ADC2,
    MT6332_AUXADC_ADC3,
    MT6332_AUXADC_ADC4,
    MT6332_AUXADC_ADC5,
    MT6332_AUXADC_ADC6,
    MT6332_AUXADC_ADC7,
    MT6332_AUXADC_ADC8
};

const uint32 I_Buffer_Table[HW_BUFFER_LENGTH] =
{
    MT6332_AUXADC_ADC31,
    MT6332_AUXADC_ADC32,
    MT6332_AUXADC_ADC33,
    MT6332_AUXADC_ADC34,
    MT6332_AUXADC_ADC35,
    MT6332_AUXADC_ADC36,
    MT6332_AUXADC_ADC37,
    MT6332_AUXADC_ADC38,
    MT6332_AUXADC_ADC39,
    MT6332_AUXADC_ADC40,
    MT6332_AUXADC_ADC41,
    MT6332_AUXADC_ADC42,
    MT6332_AUXADC_ADC9,
    MT6332_AUXADC_ADC10,
    MT6332_AUXADC_ADC11,
    MT6332_AUXADC_ADC12,
    MT6332_AUXADC_ADC13,
    MT6332_AUXADC_ADC14,
    MT6332_AUXADC_ADC15,
    MT6332_AUXADC_ADC16,
    MT6332_AUXADC_ADC17
};

int Audio_AuxAdcData_Get_ext(void)
{
    const int dRecCount = 1024;
    int dRecReadIndex = 0;
    int dValidCount = 0;
    int dCheckCount = 0;
    int cnt1, cnt2, iv_queue, v_cnt, i_cnt, cnt, i, hw_read_idx, iov_flag, iov_cnt;
    int dMax, dCurValue;
    int output_freq, freq_meter_data;

    printk("vibspk auxadc in+\n");

    if (mEnableAuxAdc == 0)
    {
        return 0;
    }

    if (mEnableAuxAdc == 1)
    {
        Ana_Set_Reg(0x80be, 0x02, 0x02);
        Ana_Set_Reg(0x80c0, 0x02, 0x02);
        Ana_Set_Reg(MT6332_AUXADC_CON0, 0x8000, 0x8000);
        Ana_Set_Reg(0x8094, 0, 0xFFFF);
        Ana_Set_Reg(0x809a, 0x6023, 0xFFFF);
        Ana_Set_Reg(0x80a0, 0x004a, 0xFFFF);
        Ana_Set_Reg(0x80b2, 0x01ff, 0xFFFF);
        Ana_Set_Reg(0x80c2, 0x0005, 0xFFFF);
        //Ana_Set_Reg(0x886a, 0x00b0, 0xFFFF);    // Important
    }

#if 1
    if (mEnableAuxAdc == 1)
    {
        Ana_Set_Reg(0x0680, 0x0000, 0xFFFF);
        Ana_Set_Reg(0x015A, 0x0001, 0xFFFF);
        Ana_Set_Reg(0x015C, 0x0003, 0xFFFF);
        Ana_Set_Reg(0x013C, 0x3000, 0xFFFF);
        Ana_Set_Reg(0x809A, 0x6025, 0xFFFF);
        Ana_Set_Reg(0x8552, 0x0004, 0xFFFF);
        Ana_Set_Reg(0x853E, 0x0021, 0xFFFF);
        Ana_Set_Reg(0x8534, 0x0300, 0xFFFF);
        Ana_Set_Reg(0x853A, 0x0000, 0xFFFF);
        Ana_Set_Reg(0x8542, 0x8542, 0xFFFF);
        Ana_Set_Reg(0x8094, 0x0000, 0xFFFF);
        Ana_Set_Reg(0x809E, 0x0004, 0xFFFF);
        while ((Ana_Get_Reg(0x809A) & 0x4) != 0)
        {
            printk("WAITING clock\n");
        }

        Ana_Set_Reg(0x809A, 0x6023, 0xFFFF); //0x6027
        //TOP_CKSEL_CON0[1] = 1?b1swtich to SMPS_CK, 1'b0 switch to 32KHz
        Ana_Set_Reg(0x80A6, 0x0100, 0xFFFF);
        Ana_Set_Reg(0x80BC, 0x0040, 0xFFFF);
        Ana_Set_Reg(0x80BC, 0x0000, 0xFFFF);
        //FQMTR_CON1[15:0] =  Frequency meter window setting (= numbers of  auxadc_12M_CK cycles)
        Ana_Set_Reg(0x8CD0, 175, 0xFFFF); // setting window value = 175
        // FQMTR_CON0[2:0]=1 ,select meter clock = 12M
        // FQMTR_CON0[15]=1 enable fqmtr
        Ana_Set_Reg(0x8CCE, 0x8000, 0xFFFF); //0 :turn on FQMTR
        msleep(2);
        while ((Ana_Get_Reg(0x8CCE) & 0x8) != 0)
        {
            printk("WAITING FQMTR_CON0[3]\n");
            //Do nothing;
        }
        //delay 1ms ，ensure Busy =1
        // FQMTR_CON0[3] =1?b0 ,wait FQMTR busy bit become to 0
        //Use 32KHz to meter variable 12MHz
        freq_meter_data = Ana_Get_Reg(0x8CD2); // check if [3] becomes to 0
        //read FQMTR data
        output_freq = (32768 * freq_meter_data) / (175);
        //output_freq = (32000*freq_meter_data)/(366*33);
        //output_freq = (12000000*freq_meter_data)/64;
        printk("freq_meter_data %d %d\n", freq_meter_data, output_freq);
        output_freq = output_freq / 96000;
        printk("freq divider %d\n", output_freq);
    }
#endif
    dMax = dCurValue = 0;
    hw_read_idx = 0;
    if (mEnableAuxAdc == 1)
    {
        Ana_Set_Reg(MT6332_AUXADC_CON13, 20, 0x01FF); //[0:8]: period
        Ana_Set_Reg(MT6332_AUXADC_CON12, 21, 0x007F); //Set Buffer Length
    }
    Ana_Set_Reg(MT6332_AUXADC_CON12, 0x0040, 0x0040);
    Ana_Set_Reg(MT6332_AUXADC_CON12, 0x0080, 0x0080);
    Ana_Set_Reg(MT6332_AUXADC_CON12, 0x0100, 0x0100);
    msleep(1);
    Ana_Set_Reg(MT6332_AUXADC_CON12, 0, 0x0040);
    Ana_Set_Reg(MT6332_AUXADC_CON12, 0, 0x0080);
    Ana_Set_Reg(MT6332_AUXADC_CON12, 0, 0x0100);
    if (mEnableAuxAdc == 1)
    {
        Ana_Set_Reg(MT6332_AUXADC_CON12, 21, 0x007F); //Set Buffer Length
        Ana_Set_Reg(MT6332_AUXADC_CON36, 0xAB, 0x01FF); //Set Channel 10 & 11 for V and I
        Ana_Set_Reg(MT6332_AUXADC_CON12, 0x8000, 0x8000); //[15]Set Speaker mode
    }

    //        Ana_Set_Reg(MT6332_AUXADC_CON33, 0, 0xFFFF);

    //        printk("hochi CON13 [%x] CON12 [%x]\n", Ana_Get_Reg(MT6332_AUXADC_CON13), Ana_Get_Reg(MT6332_AUXADC_CON12));
    Ana_Set_Reg(MT6332_AUXADC_CON13, 0x0200, 0x0200);     //[9]: enable

    //        printk("hochi while+ [%x]\n", Ana_Get_Reg(MT6332_AUXADC_CON33));

    do
    {
        iv_queue = Ana_Get_Reg(MT6332_AUXADC_CON33);
        v_cnt = (iv_queue >> 8) & 0x3F;
        i_cnt = iv_queue & 0x3F;
        //ov_flag = iv_queue & 0x8000;
        //            printk("hochi o [%d] v [%d] i [%d] t [%d]\n",ov_flag,v_cnt,i_cnt,dRecReadIndex);
        /*
            if (ov_flag != 0)
            {
                printk("hochi %s overflow \n", __func__);
                printk("hochi ov MT6332_AUXADC_CON33 [%x]\n", iv_queue);
                break;
            }
            else
        */
        {
            dCheckCount++;
        }
        //printk("o [%d] v [%d] i [%d]",ov_flag,v_cnt,i_cnt);

        if (/*(v_cnt > 0) ||*/ (i_cnt > 0))
        {
            dValidCount++;
            /*
            if (v_cnt > i_cnt)
            {
                cnt = i_cnt;
            }
            else
            {
                cnt = v_cnt;
            }
            */
            if (i_cnt > HW_BUFFER_LENGTH)
            {
                iov_cnt = i_cnt;
                iov_flag = 1;
                i_cnt = HW_BUFFER_LENGTH;
            }
            else
            {
                iov_flag = 0;
            }

            cnt = i_cnt;
            if (cnt + hw_read_idx >= HW_BUFFER_LENGTH)
            {
                cnt1 = HW_BUFFER_LENGTH - hw_read_idx;
                cnt2 = cnt - cnt1;
            }
            else
            {
                cnt1 = cnt;
                cnt2 = 0;
            }

            for (i = 0; i < cnt1; i++)
            {
                int /*v_tmp, */i_tmp;

                i_tmp = Ana_Get_Reg(I_Buffer_Table[hw_read_idx]);
                //v_tmp = Ana_Get_Reg(V_Buffer_Table[hw_read_idx]);
                /*
                if( hw_read_idx == 19)
                {
                    bufferBase[ring_write_idx++] = ((v_tmp >> 3) & 0xFFF); //LSB 15 bits
                }
                else
                {
                    bufferBase[ring_write_idx++] = (v_tmp & 0xFFF); //LSB 12 bits
                }
                */
                if (hw_read_idx == 17 || hw_read_idx == 18 || hw_read_idx == 19)
                {
                    dCurValue = ((i_tmp >> 3) & 0xFFF); //LSB 15 bits
                }
                else
                {
                    dCurValue = (i_tmp & 0xFFF); //LSB 12 bits
                }

                //              if (/*(v_tmp & 0x8000) == 0 || */(i_tmp & 0x8000) == 0)
                //             {
                //must_print("AUXADC_CON33=0x%x at %d\n\n", iv_queue, hw_read_idx);
                //must_print("v_tmp=0x%x i_tmp= 0x%x, hw_read_idx %d, V_Addr 0x%x, I_Addr 0x%x\n\n", v_tmp, i_tmp, hw_read_idx, I_Buffer_Table[hw_read_idx], V_Buffer_Table[hw_read_idx]);
                //           }
                if (dCurValue > dMax)
                {
                    dMax = dCurValue;
                }
                hw_read_idx++;

                if (hw_read_idx >= HW_BUFFER_LENGTH)
                {
                    hw_read_idx = 0;
                }
            }

            //If warp to head, do second round
            for (i = 0; i < cnt2; i++)
            {
                int /*v_tmp, */i_tmp;

                i_tmp = Ana_Get_Reg(I_Buffer_Table[hw_read_idx]);
                //v_tmp = Ana_Get_Reg(V_Buffer_Table[hw_read_idx]);
                /*
                if( hw_read_idx == 19)
                {
                    bufferBase[ring_write_idx++] = ((v_tmp >> 3)& 0xFFF); //LSB 15 bits
                }
                else
                {
                    bufferBase[ring_write_idx++] = (v_tmp & 0xFFF); //LSB 12 bits
                }
                */
                if (hw_read_idx == 17 || hw_read_idx == 18 || hw_read_idx == 19)
                {
                    dCurValue = ((i_tmp >> 3) & 0xFFF); //LSB 15 bits
                }
                else
                {
                    dCurValue = (i_tmp & 0xFFF); //LSB 12 bits
                }
                /*
                if ((v_tmp & 0x8000) == 0 || (i_tmp & 0x8000) == 0)
                {
                    printk("hochi AUXADC_CON33=0x%x at %d\n\n", iv_queue, hw_read_idx);
                    printk("hochi v_tmp=0x%x i_tmp= 0x%x, hw_read_idx %d, V_Addr 0x%x, I_Addr 0x%x\n\n", v_tmp, i_tmp, hw_read_idx, I_Buffer_Table[hw_read_idx], V_Buffer_Table[hw_read_idx]);
                }
                */
                if (dCurValue > dMax)
                {
                    dMax = dCurValue;
                }
                hw_read_idx++;
                if (hw_read_idx >= HW_BUFFER_LENGTH)
                {
                    hw_read_idx = 0;
                }
            }

            dRecReadIndex += cnt;

            if (iov_flag)
            {
                Ana_Set_Reg(MT6332_AUXADC_CON12, 0x0080, 0x0080);
                printk("vibspk auxadc skip io [%d] i [%d]\n", iov_flag, iov_cnt);
                Ana_Set_Reg(MT6332_AUXADC_CON12, 0, 0x0080);
            }
        }


    }
    while (dRecCount > dRecReadIndex/* && dValidCount > 0*/);

    // if (ov_flag)
    //     printk("hochi : overflow dRecReadIndex [%d] dValidCount [%d] dCheckCount [%d]\n",dRecReadIndex,dValidCount,dCheckCount);
    // else
    printk("vibspk auxadc- : dMax = %d dRecReadIndex [%d] dValidCount [%d] dCheckCount [%d]\n", dMax, dRecReadIndex, dValidCount, dCheckCount);
#if 0//0519
    Ana_Set_Reg(0x80be, 0x02, 0x02);
    Ana_Set_Reg(0x80c0, 0x02, 0x02);

    Ana_Set_Reg(MT6332_AUXADC_CON12, 0, 0x8000); //[15]Set Speaker mode
    Ana_Set_Reg(MT6332_AUXADC_CON13, 0, 0x0200);     //[9]: enable
    Ana_Set_Reg(MT6332_AUXADC_CON0, 0x0000, 0x8000);
#endif
    Ana_Set_Reg(MT6332_AUXADC_CON13, 0, 0x0200);     //[9]: enable
    mEnableAuxAdc = 2;
    printk("vibspk auxadc-\n");
    return dMax;
}


#endif
//int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);

static int Audio_AuxAdcData_Get(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
#ifdef CONFIG_MTK_SPEAKER
    ucontrol->value.integer.value[0] = Audio_AuxAdcData_Get_ext();//PMIC_IMM_GetSPK_THR_IOneChannelValue(0x001B, 1, 0);
#else
    ucontrol->value.integer.value[0] = 0;
#endif
    return 0;
}

static int Audio_AuxAdcData_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    dAuxAdcChannel = ucontrol->value.integer.value[0];
    printk("%s dAuxAdcChannel = 0x%x \n", __func__, dAuxAdcChannel);
    return 0;
}


static const struct snd_kcontrol_new Audio_snd_auxadc_controls[] =
{
    SOC_SINGLE_EXT("Audio AUXADC Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_AuxAdcData_Get, Audio_AuxAdcData_Set),
};


static const char *amp_function[] = {"Off", "On"};
static const char *aud_clk_buf_function[] = {"Off", "On"};

//static const char *DAC_SampleRate_function[] = {"8000", "11025", "16000", "24000", "32000", "44100", "48000"};
static const char *DAC_DL_PGA_Headset_GAIN[] = {"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db", "-2Db", "-3Db",
                                                "-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db" , "-40Db"
                                               };
static const char *DAC_DL_PGA_Handset_GAIN[] = {"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db", "-2Db", "-3Db",
                                                "-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db" , "-40Db"
                                               };

static const char *DAC_DL_PGA_Speaker_GAIN[] = {"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db", "-2Db", "-3Db",
                                                "-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db" , "-40Db"
                                               };

//static const char *Voice_Mux_function[] = {"Voice", "Speaker"};

static int Lineout_PGAL_Get(struct snd_kcontrol *kcontrol,
                            struct snd_ctl_elem_value *ucontrol)
{
    printk("Speaker_PGA_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL];
    return 0;
}

static int Lineout_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
    {
        index = 0x1f;
    }
    Ana_Set_Reg(ZCD_CON1, index , 0x001f);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] = ucontrol->value.integer.value[0];
    return 0;
}

static int Lineout_PGAR_Get(struct snd_kcontrol *kcontrol,
                            struct snd_ctl_elem_value *ucontrol)
{
    printk("%s  = %d\n", __func__, mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR];
    return 0;
}

static int Lineout_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    //    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int index = 0;
    printk("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
    {
        index = 0x1f;
    }
    Ana_Set_Reg(ZCD_CON1, index << 7 , 0x0f10);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR] = ucontrol->value.integer.value[0];
    return 0;
}

static int Handset_PGA_Get(struct snd_kcontrol *kcontrol,
                           struct snd_ctl_elem_value *ucontrol)
{
    printk("Handset_PGA_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
    return 0;
}

static int Handset_PGA_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    //    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int index = 0;

    printk("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN) - 1))
    {
        index = 0x1f;
    }
    Ana_Set_Reg(ZCD_CON3, index , 0x001f);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] = ucontrol->value.integer.value[0];
    return 0;
}


static void HeadsetLVolumeSet(void)
{
    int index = 0;
    printk("%s\n", __func__);
    index =   mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
    Ana_Set_Reg(ZCD_CON2, index , 0x001f);
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol,
                            struct snd_ctl_elem_value *ucontrol)
{
    printk("Headset_PGAL_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
    return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    //    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int index = 0;
    int offset = 0 , step = 0;

    //printk("%s(), index = %d arraysize = %lu \n", __func__, ucontrol->value.enumerated.item[0], ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN));

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
    {
        index = 0x1f;
    }
    offset =  mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] - ucontrol->value.integer.value[0];

    if (offset > 0)
    {
        step = -1;
    }
    else if (offset == 0)
    {
        printk("return for gain is the same");
        return 0;
    }
    else
    {
        step = 1;
    }
    //remove while due to headset gain update too late for user experience, trade off: headset pop hw limitation
#if 0
    while (offset != 0)
    {
        mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] += step;
        Ana_Set_Reg(ZCD_CON2, (mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]), 0x001f);
        msleep(1);
        offset += step;
    }
#endif
    msleep(1);
    Ana_Set_Reg(ZCD_CON2, index , 0x001f);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] = ucontrol->value.integer.value[0];
    return 0;
}

static void HeadsetRVolumeSet(void)
{
    int index = 0;
    printk("%s\n", __func__);
    index =   mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
    Ana_Set_Reg(ZCD_CON2, index << 7, 0x0f80);
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol,
                            struct snd_ctl_elem_value *ucontrol)
{
    printk("Headset_PGAR_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
    return 0;
}

static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    //    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int index = 0;
    int offset = 0 , step = 0;

    printk("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
    {
        index = 0x1f;
    }

    offset =  mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] - ucontrol->value.integer.value[0];
    if (offset > 0)
    {
        step = -1;
    }
    else if (offset == 0)
    {
        printk("return for gain is the same");
        return 0;
    }
    else
    {
        step = 1;
    }
//remove while due to headset gain update too late for user experience, trade off: headset pop hw limitation
#if 0
    while (offset != 0)
    {
        mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] += step;
        Ana_Set_Reg(ZCD_CON2, (mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR]) << 7, 0x0f80);
        msleep(1);
        offset += step;
    }
#endif
    msleep(1);
    Ana_Set_Reg(ZCD_CON2, index << 7, 0x0f80);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = ucontrol->value.integer.value[0];
    return 0;
}


/*static int Voice_Mux_Get(struct snd_kcontrol *kcontrol,
                         struct snd_ctl_elem_value *ucontrol)
{
    printk("Voice_Mux_Get = %d\n", mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_VOICE]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_VOICE];
    return 0;
}

static int Voice_Mux_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    printk("%s()\n", __func__);
    if (ucontrol->value.integer.value[0])
    {
        printk("%s()\n", __func__);
        snd_soc_dapm_disable_pin(&codec->dapm, "SPEAKER");
        snd_soc_dapm_disable_pin(&codec->dapm, "RX_BIAS");
        snd_soc_dapm_sync(&codec->dapm);
    }
    else
    {
        printk("%s()\n", __func__);
        snd_soc_dapm_enable_pin(&codec->dapm, "SPEAKER");
        snd_soc_dapm_enable_pin(&codec->dapm, "RX_BIAS");
        snd_soc_dapm_sync(&codec->dapm);
    }

    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_VOICE] = ucontrol->value.integer.value[0];
    return 0;
}*/

static uint32 mHp_Impedance = 32;

static int Audio_Hp_Impedance_Get(struct snd_kcontrol *kcontrol,
                                  struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_Hp_Impedance_Get = %d\n", mHp_Impedance);
    ucontrol->value.integer.value[0] = mHp_Impedance;
    return 0;

}

static int Audio_Hp_Impedance_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    mHp_Impedance = ucontrol->value.integer.value[0];
    printk("%s Audio_Hp_Impedance_Set = 0x%x \n", __func__, mHp_Impedance);
    return 0;
}

static int Aud_Clk_Buf_Get(struct snd_kcontrol *kcontrol,
                           struct snd_ctl_elem_value *ucontrol)
{
    printk("\%s n", __func__);
    ucontrol->value.integer.value[0] = audck_buf_Count;
    return 0;
}

static int Aud_Clk_Buf_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    //int index = 0;
    printk("%s(), value = %d\n", __func__, ucontrol->value.enumerated.item[0]);
    if (ucontrol->value.integer.value[0])
    {
        audckbufEnable(true);
    }
    else
    {
        audckbufEnable(false);
    }
    return 0;
}


static const struct soc_enum Audio_DL_Enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
    // here comes pga gain setting
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN), DAC_DL_PGA_Headset_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN), DAC_DL_PGA_Headset_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN), DAC_DL_PGA_Handset_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN), DAC_DL_PGA_Speaker_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN), DAC_DL_PGA_Speaker_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aud_clk_buf_function), aud_clk_buf_function),
};

static const struct snd_kcontrol_new mt6331_snd_controls[] =
{
    SOC_ENUM_EXT("Audio_Amp_R_Switch", Audio_DL_Enum[0], Audio_AmpR_Get, Audio_AmpR_Set),
    SOC_ENUM_EXT("Audio_Amp_L_Switch", Audio_DL_Enum[1], Audio_AmpL_Get, Audio_AmpL_Set),
    SOC_ENUM_EXT("Voice_Amp_Switch", Audio_DL_Enum[2], Voice_Amp_Get, Voice_Amp_Set),
    SOC_ENUM_EXT("Speaker_Amp_Switch", Audio_DL_Enum[3], Speaker_Amp_Get, Speaker_Amp_Set),
    SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", Audio_DL_Enum[4], Headset_Speaker_Amp_Get, Headset_Speaker_Amp_Set),
    SOC_ENUM_EXT("Headset_PGAL_GAIN", Audio_DL_Enum[5], Headset_PGAL_Get, Headset_PGAL_Set),
    SOC_ENUM_EXT("Headset_PGAR_GAIN", Audio_DL_Enum[6], Headset_PGAR_Get, Headset_PGAR_Set),
    SOC_ENUM_EXT("Handset_PGA_GAIN", Audio_DL_Enum[7], Handset_PGA_Get, Handset_PGA_Set),
    SOC_ENUM_EXT("Lineout_PGAR_GAIN", Audio_DL_Enum[8], Lineout_PGAR_Get, Lineout_PGAR_Set),
    SOC_ENUM_EXT("Lineout_PGAL_GAIN", Audio_DL_Enum[9], Lineout_PGAL_Get, Lineout_PGAL_Set),
    SOC_ENUM_EXT("AUD_CLK_BUF_Switch", Audio_DL_Enum[10], Aud_Clk_Buf_Get, Aud_Clk_Buf_Set),
    SOC_SINGLE_EXT("Audio HP Impedance", SND_SOC_NOPM, 0, 512, 0, Audio_Hp_Impedance_Get, Audio_Hp_Impedance_Set),
};

static const struct snd_kcontrol_new mt6331_Voice_Switch[] =
{
    //SOC_DAPM_ENUM_EXT("Voice Mux", Audio_DL_Enum[10], Voice_Mux_Get, Voice_Mux_Set),
};

void SetMicPGAGain(void)
{
    int index = 0;
    index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index , 0x0007);
    index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index << 4, 0x0070);
    index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index << 8, 0x0700);
    index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index << 12, 0x7000);
}

static bool GetAdcStatus(void)
{
    int i = 0;
    for (i = AUDIO_ANALOG_DEVICE_IN_ADC1 ; i < AUDIO_ANALOG_DEVICE_MAX ; i++)
    {
        if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
        {
            return true;
        }
    }
    return false;
}

static bool GetDacStatus(void)
{
    int i = 0;
    for (i = AUDIO_ANALOG_DEVICE_OUT_EARPIECER ; i < AUDIO_ANALOG_DEVICE_2IN1_SPK ; i++)
    {
        if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
        {
            return true;
        }
    }
    return false;
}


static bool TurnOnADcPowerACC(int ADCType, bool enable)
{
    printk("%s ADCType = %d enable = %d \n", __func__, ADCType, enable);
    if (enable)
    {
        //uint32 ULIndex = GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
        uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
        uint32 SampleRate_VUL2 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC_2];
        if (GetAdcStatus() == false)
        {
            audckbufEnable(true);
            if (mAdc_Power_Mode == true)
            {
                Ana_Set_Reg(AUDBUF_CFG4, 0x0001, 0x0001);  // Set AVDD32_AUD lowpower mode
            }
            else
            {
                Ana_Set_Reg(AUDBUF_CFG4, 0x0000, 0x0001);  // Set AVDD32_AUD lowpower mode
            }
            NvregEnable(true);
            ClsqAuxEnable(true);
            ClsqEnable(true);
            if (mAdc_Power_Mode == true)
            {
                Ana_Set_Reg(AUDADC_CFG0, 0x4400, 0xffff);      // Enable ADC CLK
            }
            else
            {
                Ana_Set_Reg(AUDADC_CFG0, 0x0400, 0xffff);      // Enable ADC CLK
            }
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC1, mAudio_Analog_Mic1_mode);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC2, mAudio_Analog_Mic2_mode);

            //OpenMicbias1();
            //OpenMicbias0();
            if (mAdc_Power_Mode == false)
            {
                SetMicbias1lowpower(false);
                SetMicbias0lowpower(false);
            }
            else
            {
                SetMicbias1lowpower(true);
                SetMicbias0lowpower(true);
            }

            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x78F, 0xffff);   //Enable MICBIAS0,1 (2.7V)
            SetMicVref2(0x2);// 1.9V
            SetMicVref3(0x2); // 1.9V
            SetMic2DCcoupleSwitch(false);
            SetMic3DCcoupleSwitch(false);
            if (mAdc_Power_Mode == false)
            {
                SetMic2powermode(false);
                SetMic3powermode(false);
            }
            else
            {
                SetMic2powermode(true);
                SetMic3powermode(true);
            }
            //OpenMicbias3(true);
            //OpenMicbias2(true);
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0007, 0xffff);   //Enable LCLDO18_ENC (1.8V), Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0000, 0x8888);   //Enable  LCLDO19_ADCCH0_1, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x000f, 0xffff);   //Enable LCLDO18_ENC (1.8V), Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x8888, 0x8888);   //Enable  LCLDO19_ADCCH0_1, Remote-Sense
            }
            //Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x3333, 0xffff);   //Set PGA CH0_1 gain = 18dB
            SetMicPGAGain();
            //Ana_Set_Reg(AUDPREAMP_CFG0, 0x0051, 0x001f);   //Enable PGA CH0_1 (CH0 in)
            //Ana_Set_Reg(AUDPREAMP_CFG1, 0x16d5, 0xffff);   //Enable ADC CH0_1 (PGA in)

            //here to set digital part
            Topck_Enable(true);
            AdcClockEnable(true);
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff);   //power on ADC clk
            if ((GetDacStatus() == false))
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x005a, 0xffff);   //power on clock
            }
            else
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock
            }
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff);   //power on ADC clk
            Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);   //configure ADC setting

            Ana_Set_Reg(AFE_MIC_ARRAY_CFG, 0x44e4, 0xffff);   //AFE_MIC_ARRAY_CFG
            Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff);   //turn on afe

            Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F | (GetULNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]) << 10), 0xffff); // config UL up8x_rxif adc voice mode
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1) << 3 | ULSampleRateTransform(SampleRate_VUL1) << 1) , 0x001f);// ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);

            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0041, 0xffff);   //power on uplink
        }

        //  open ADC indivisaully
        EnableMicBias(ADCType, enable);
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0007, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-SENSEINFOBYTES
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0070, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x000f, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x00f0, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0051, 0x001f);   //Enable PGA CH0_1
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0x007f);   //Enable ADC CH0_1 (PGA in)
            AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
            Audio_ADC1_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1]);

            Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F | (GetULNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]) << 10), 0xffff); // config UL up8x_rxif adc voice mode
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1) << 3 | ULSampleRateTransform(SampleRate_VUL1) << 1) , 0x001f);// ULsampling rate

            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC1, mAudio_Analog_Mic1_mode);
        }
        else   if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0007, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-SENSEINFOBYTES
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0070, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x000f, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x00f0, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }

            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0040, 0x03c0);   //Enable PGA CH2
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0x007f);   //Enable ADC CH0_1 (PGA in)
            Audio_ADC2_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2]);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC2, mAudio_Analog_Mic2_mode);

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0700, 0x0f00);   //Enable LCLDO19_ADCCH3, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0f00, 0x0f00);   //Enable LCLDO19_ADCCH3, Remote-Sense
            }
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0280, 0x0380);   //Enable ADC CH3 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0001, 0x000f);   //Enable preamp CH3
            Audio_ADC3_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3]);
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x7000, 0xf000);   //Enable LCLDO19_ADCCH4, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0xf000, 0xf000);   //Enable LCLDO19_ADCCH4, Remote-Sense
            }
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x1400, 0x1c00);   //Enable ADC CH4 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0010, 0x00f0);   //Enable preamp CH4
            Audio_ADC4_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4]);
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);
        }
        else
        {
            printk("\n");
        }
    }
    else
    {
        if ((GetAdcStatus() == false))
        {
            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0xa000, 0xffff);   //power off

            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0000, 0xffff);   //Disable ADC CH0_1 (PGA in) Disable ADC CH_2 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0000, 0xffff);   //Disable PGA CH0_1 (CH0 in) Disable PGA CH_2
            Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x0000, 0xffff);   //Set PGA CH0_1 gain = 0dB  Set PGA CH_2 gain = 0dB

            Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x2222, 0xffff);   //disable LCLDO19_ADCCH0_1, Remote-Sense
            Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0002, 0xffff);   //disable LCLDO18_ENC (1.8V), Remote-Sense

            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x2020, 0xffff);   //power on clock
            SetMic2powermode(true);
            SetMic3powermode(true);

            Ana_Set_Reg(AUDADC_CFG0, 0x0000, 0xffff);   //configure ADC setting
            ClsqAuxEnable(false);
            ClsqEnable(false);
            NvregEnable(false);
            Topck_Enable(false);
            audckbufEnable(false);
            if (GetDLStatus() == false)
            {
                // check for if DL/UL will share same register

            }
            else
            {

            }

        }
        //todo , close analog
        EnableMicBias(ADCType, enable);
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {
        }
    }
    return true;
}

static bool TurnOnADcPowerDmic(int ADCType, bool enable)
{
    printk("%s ADCType = %d enable = %d \n", __func__, ADCType, enable);
    if (enable)
    {
        uint32 ULIndex = GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
        uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
        uint32 SampleRate_VUL2 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC_2];
        if (GetAdcStatus() == false)
        {
            audckbufEnable(true);
            NvregEnable(true);
            ClsqAuxEnable(true);
            ClsqEnable(true);
            Ana_Set_Reg(AUDDIGMI_CFG0, 0x0041, 0xffff);    //Enable DMIC0 (BIAS=10)
            Ana_Set_Reg(AUDDIGMI_CFG1, 0x0041, 0xffff);    //Enable DMIC1 (BIAS=10)

            Ana_Set_Reg(AUDADC_CFG0, 0x0400, 0xffff);      // Enable ADC CLK
            //Ana_Set_Reg(AUDMICBIAS_CFG0, 0x78F, 0xffff);   //Enable MICBIAS0,1 (2.7V)
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC1, AUDIO_ANALOGUL_MODE_DMIC);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC2, AUDIO_ANALOGUL_MODE_DMIC);
            SetMicbias1lowpower(false);
            SetMicbias0lowpower(false);

            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x285, 0xffff);   //Enable MICBIAS2,3 (2.7V)
            SetMicVref2(0x2);// 1.9V
            SetMicVref3(0x2); // 1.9V
            SetMic2DCcoupleSwitch(false);
            SetMic3DCcoupleSwitch(false);
            SetMic2powermode(false);
            SetMic3powermode(false);


            //here to set digital part
            Topck_Enable(true);
            AdcClockEnable(true);

            if ((GetDacStatus() == false))
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x005a, 0xffff);   //power on clock
            }
            else
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock
            }

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff);   //power on ADC clk
            Ana_Set_Reg(PMIC_AFE_TOP_CON0, (ULIndex << 7) | (ULIndex << 6), 0xffff); //configure ADC setting
            Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff);   //turn on afe
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1) << 3 | ULSampleRateTransform(SampleRate_VUL1) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (1 << 7),(1<<7) ); // dmic open
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (1 << 5) | (1 << 6), (1 << 5) | (1 << 6)); // dmic open

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (1 << 5) | (1 << 6), (1 << 5) | (1 << 6)); // dmic open
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0043, 0xffff);

            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0003, 0xffff);   //power on uplink

        }
        // todo , open ADC indivisaully
        EnableMicBias(ADCType, enable);
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {
        }
        else   if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {

        }
        else
        {
            printk("\n");
        }
    }
    else
    {
        if (GetAdcStatus() == false)
        {

            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x2020, 0xffff);   //Enable MICBIAS2,3 (2.7V)
            SetMic2powermode(true);
            SetMic3powermode(true);
            OpenMicbias3(false);
            OpenMicbias2(false);
            SetMic2powermode(true);
            SetMic3powermode(true);
            //EnableMicBias(ADCType, enable);

            Ana_Set_Reg(AUDDIGMI_CFG0, 0x0040, 0xffff);    //Disable DMIC0 (BIAS=10)
            Ana_Set_Reg(AUDDIGMI_CFG1, 0x0040, 0xffff);    //Disable DMIC1 (BIAS=10)

            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (0 << 7),(1<<7) ); // dmic close
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (0 << 5) | (0 << 6), (1 << 5) | (1 << 6)); // dmic close

            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, 0x0000, 0xffe0);   //ch1 and ch2 digital mic OFF
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0xa000, 0xffff);   //power off

            ClsqAuxEnable(false);
            ClsqEnable(false);
            NvregEnable(false);
            Topck_Enable(false);
            audckbufEnable(false);
            if (GetDLStatus() == false)
            {
                // check for if DL/UL will share same register
                Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
            }
            else
            {

            }

        }
        //todo , close analog
        EnableMicBias(ADCType, enable);
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {

        }
    }
    return true;
}

static bool TurnOnADcPowerDCC(int ADCType, bool enable)
{
    printk("%s ADCType = %d enable = %d \n", __func__, ADCType, enable);
    if (enable)
    {
        //uint32 ULIndex = GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
        uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
        uint32 SampleRate_VUL2 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC_2];
        if (GetAdcStatus() == false)
        {
            audckbufEnable(true);
            if (mAdc_Power_Mode == true)
            {
                Ana_Set_Reg(AUDBUF_CFG4, 0x0001, 0x0001);  // Set AVDD32_AUD lowpower mode
            }
            else
            {
                Ana_Set_Reg(AUDBUF_CFG4, 0x0000, 0x0001);  // Set AVDD32_AUD lowpower mode
            }
            NvregEnable(true);    //Enable AUDGLB
            ClsqAuxEnable(true);    //Enable ADC CLK
            ClsqEnable(true);

            //here to set digital part
            Topck_Enable(true);
            AdcClockEnable(true);

            if (mAdc_Power_Mode == true)
            {
                Ana_Set_Reg(AUDADC_CFG0, 0x4400, 0xffff);      // Enable ADC CLK
            }
            else
            {
                Ana_Set_Reg(AUDADC_CFG0, 0x0400, 0xffff);      // Enable ADC CLK
            }
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC1, mAudio_Analog_Mic1_mode);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC2, mAudio_Analog_Mic2_mode);
            //OpenMicbias1();
            //OpenMicbias0();
            //EnableMicBias(ADCType, enable);
            if (mAdc_Power_Mode == false)
            {
                SetMicbias1lowpower(false);
                SetMicbias0lowpower(false);
            }
            else
            {
                SetMicbias1lowpower(true);
                SetMicbias0lowpower(true);
            }

            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x1ab5, 0xffff);   //Enable MICBIAS2,3
            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x78F, 0xffff);   //Enable MICBIAS0,1 (2.7V)
            SetMicVref2(0x2);// 1.9V
            SetMicVref3(0x2); // 1.9V
            SetMic2DCcoupleSwitch(false);
            SetMic3DCcoupleSwitch(false);
            if (mAdc_Power_Mode == false)
            {
                SetMic2powermode(false);
                SetMic3powermode(false);
            }
            else
            {
                SetMic2powermode(true);
                SetMic3powermode(true);
            }

            //OpenMicbias3(true);
            //OpenMicbias2(true);
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0007, 0xffff);   //Enable LCLDO18_ENC (1.8V), Remote-SENSEINFOBYTES
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0000, 0x8888);   //Enable  LCLDO19_ADCCH0_1, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x000f, 0xffff);   //Enable LCLDO18_ENC (1.8V), Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x8888, 0x8888);   //Enable  LCLDO19_ADCCH0_1, Remote-Sense
            }

            Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);   //DC_26M_50K_EN
            SetMicPGAGain();

            Ana_Set_Reg(AUDPREAMP_CFG0, 0x01c7, 0xffff);   //Enable PGA CH0_1 (CH0 in)
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0xffff);   //Enable ADC CH0_1 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x00d3, 0xffff);   //Enable PGA CH0_1 (CH0 in)

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff);   //power on ADC clk

            if ((GetDacStatus() == false))
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x005a, 0xffff);   //power on clock
            }
            else
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock
            }

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff);   //power on ADC clk
            Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);   //configure ADC setting

            Ana_Set_Reg(AFE_MIC_ARRAY_CFG, 0x44e4, 0xffff);   //AFE_MIC_ARRAY_CFG
            Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff);   //turn on afe

            Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F | (GetULNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]) << 10), 0xffff); // config UL up8x_rxif adc voice mode
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1) << 3 | ULSampleRateTransform(SampleRate_VUL1) << 1) , 0x001f); // ULsampling rate

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);

            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0041, 0xffff);   //power on uplink

        }
        // todo , open ADC indivisaully
        EnableMicBias(ADCType, enable);
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0007, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-SENSEINFOBYTES
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0070, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x000f, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x00f0, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }

            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0007, 0x003f);   //Enable PGA CH0_1
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0x007f);   //Enable ADC CH0_1 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0013, 0x003f);   //Enable PGA CH0_1
            Audio_ADC1_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1]);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC1, mAudio_Analog_Mic1_mode);
            AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);

            Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F | (GetULNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]) << 10), 0xffff); // config UL up8x_rxif adc voice mode
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1) << 3 | ULSampleRateTransform(SampleRate_VUL1) << 1) , 0x001f);// ULsampling rate

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0007, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-SENSEINFOBYTES
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0070, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x000f, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x00f0, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }

            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC2, mAudio_Analog_Mic2_mode);
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x01c0, 0x03c0);   //Enable PGA CH2
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0x007f);   //Enable ADC CH0_1 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x00c0, 0x03c0);   //Enable PGA CH2
            Audio_ADC2_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2]);
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0700, 0x0f00);   //Enable LCLDO19_ADCCH3, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0f00, 0x0f00);   //Enable LCLDO19_ADCCH3, Remote-Sense
            }
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0007, 0x000f);   //Enable preamp CH3
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0280, 0x0380);   //Enable ADC CH3 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0003, 0x000f);   //Enable preamp CH3
            Audio_ADC3_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3]);

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x7000, 0xf000);   //Enable LCLDO19_ADCCH4, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0xf000, 0xf000);   //Enable LCLDO19_ADCCH4, Remote-Sense
            }
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0070, 0x00f0);   //Enable preamp CH4
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x1400, 0x1c00);   //Enable ADC CH4 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0030, 0x00f0);   //Enable preamp CH4
            Audio_ADC4_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4]);
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);
        }
        else
        {
            printk("\n");
        }
    }
    else
    {
        if ((GetAdcStatus() == false))
        {
            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0xa000, 0xffff);   //power off

            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0000, 0xffff);   //Disable ADC CH0_1 (PGA in) Disable ADC CH_2 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0000, 0xffff);   //Disable PGA CH0_1 (CH0 in) Disable PGA CH_2
            Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x0000, 0xffff);   //Set PGA CH0_1 gain = 0dB  Set PGA CH_2 gain = 0dB
            Ana_Set_Reg(AFE_DCCLK_CFG0, 0x0, 0xffff);   //DC_26M_50K_ off

            Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x2222, 0xffff);   //disable LCLDO19_ADCCH0_1, Remote-Sense
            Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0002, 0xffff);   //disable LCLDO18_ENC (1.8V), Remote-Sense

            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x2020, 0xffff);   //power on clock
            SetMic2powermode(true);
            SetMic3powermode(true);
            //OpenMicbias3(false);
            //penMicbias2(false);
            //Ana_Set_Reg(AUDMICBIAS_CFG0, 0x0000, 0xffff);   //power on ADC clk
            //CloseMicbias1();
            //CloseMicbias0();
            //EnableMicBias(ADCType, enable);

            Ana_Set_Reg(AUDADC_CFG0, 0x0000, 0xffff);   //configure ADC setting

            ClsqAuxEnable(false);
            ClsqEnable(false);
            NvregEnable(false);
            Topck_Enable(false);
            audckbufEnable(false);
            if (GetDLStatus() == false)
            {
                Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
            }
            else
            {

            }

        }
        EnableMicBias(ADCType, enable);
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {

        }
    }
    return true;
}


static bool TurnOnADcPowerDCCECM(int ADCType, bool enable)
{
    printk("%s ADCType = %d enable = %d \n", __func__, ADCType, enable);
    if (enable)
    {
        //uint32 ULIndex = GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
        uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
        uint32 SampleRate_VUL2 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC_2];
        if (GetAdcStatus() == false)
        {
            audckbufEnable(true);
            if (mAdc_Power_Mode == true)
            {
                Ana_Set_Reg(AUDBUF_CFG4, 0x0001, 0x0001);  // Set AVDD32_AUD lowpower mode
            }
            else
            {
                Ana_Set_Reg(AUDBUF_CFG4, 0x0000, 0x0001);  // Set AVDD32_AUD lowpower mode
            }
            NvregEnable(true);    //Enable AUDGLB
            ClsqAuxEnable(true);    //Enable ADC CLK
            ClsqEnable(true);

            //here to set digital part
            Topck_Enable(true);
            AdcClockEnable(true);

            if (mAdc_Power_Mode == true)
            {
                Ana_Set_Reg(AUDADC_CFG0, 0x4400, 0xffff);      // Enable ADC CLK
            }
            else
            {
                Ana_Set_Reg(AUDADC_CFG0, 0x0400, 0xffff);      // Enable ADC CLK
            }

            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC1, mAudio_Analog_Mic1_mode);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC2, mAudio_Analog_Mic2_mode);

            //OpenMicbias1();
            //OpenMicbias0();
            if (mAdc_Power_Mode == false)
            {
                SetMicbias1lowpower(false);
                SetMicbias0lowpower(false);
            }
            else
            {
                SetMicbias1lowpower(true);
                SetMicbias0lowpower(true);
            }
            //EnableMicBias(ADCType, enable);


            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x1ab5, 0xffff);   //Enable MICBIAS2,3
            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x78F, 0xffff);   //Enable MICBIAS0,1 (2.7V)
            SetMicVref2(0x2);// 1.9V
            SetMicVref3(0x2); // 1.9V
            SetMic2DCcoupleSwitch(true);
            SetMic3DCcoupleSwitch(true);
            if (mAdc_Power_Mode == false)
            {
                SetMic2powermode(false);
                SetMic3powermode(false);
            }
            else
            {
                SetMic2powermode(true);
                SetMic3powermode(true);
            }

            //OpenMicbias3(true);
            //OpenMicbias2(true);

            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0007, 0xffff);   //Enable LCLDO18_ENC (1.8V), Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0000, 0x8888);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x000f, 0xffff);   //Enable LCLDO18_ENC (1.8V), Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x8888, 0x8888);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
            }

            Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);   //DC_26M_50K_EN

            Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x3333, 0xffff);   //Set PGA CH0_1 gain = 18dB
            SetMicPGAGain();

            Ana_Set_Reg(AUDPREAMP_CFG0, 0x01c7, 0xffff);   //Enable PGA CH0_1 (CH0 in)
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0xffff);   //Enable ADC CH0_1 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x00d3, 0xffff);   //Enable PGA CH0_1 (CH0 in)


            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff);   //power on ADC clk

            if ((GetDacStatus() == false))
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x005a, 0xffff);   //power on clock
            }
            else
            {
                Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock
            }

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff);   //power on ADC clk
            Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);   //configure ADC setting

            Ana_Set_Reg(AFE_MIC_ARRAY_CFG, 0x44e4, 0xffff);   //AFE_MIC_ARRAY_CFG
            Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff);   //turn on afe

            Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F | (GetULNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]) << 10), 0xffff); // config UL up8x_rxif adc voice mode
            Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1) << 3 | ULSampleRateTransform(SampleRate_VUL1) << 1) , 0x001f); // ULsampling rate

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);

            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0041, 0xffff);   //power on uplink

        }
        EnableMicBias(ADCType, enable);
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0007, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-SENSEINFOBYTES
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0070, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x000f, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x00f0, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0007, 0x003f);   //Enable PGA CH0_1
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0x007f);   //Enable ADC CH0_1 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0013, 0x003f);   //Enable PGA CH0_1
            AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
            Audio_ADC1_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1]);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC1, mAudio_Analog_Mic1_mode);

        }
        else   if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0007, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-SENSEINFOBYTES
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0070, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x000f, 0x000f);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x00f0, 0x00f0);   //Enable LCLDO19_ADCCH2, Remote-Sensen)
            }
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x01c0, 0x03c0);   //Enable PGA CH2
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0x007f);   //Enable ADC CH0_1 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x00c0, 0x03c0);   //Enable PGA CH2
            Audio_ADC2_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2]);
            SetDCcoupleNP(AUDIO_ANALOG_DEVICE_IN_ADC2, mAudio_Analog_Mic2_mode);
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0700, 0x0f00);   //Enable LCLDO19_ADCCH3, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x0f00, 0x0f00);   //Enable LCLDO19_ADCCH3, Remote-Sense
            }

            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0007, 0x000f);   //Enable preamp CH3
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0280, 0x0380);   //Enable ADC CH3 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0003, 0x000f);   //Enable preamp CH3
            Audio_ADC3_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3]);

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);
        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {
            if (mAdc_Power_Mode == false)
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x7000, 0xf000);   //Enable LCLDO19_ADCCH4, Remote-Sense
            }
            else
            {
                Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0xf000, 0xf000);   //Enable LCLDO19_ADCCH4, Remote-Sense
            }

            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0070, 0x00f0);   //Enable preamp CH4
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x1400, 0x1c00);   //Enable ADC CH4 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG2, 0x0030, 0x00f0);   //Enable preamp CH4
            Audio_ADC4_Set_Input(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4]);

            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL2) << 3 | ULSampleRateTransform(SampleRate_VUL2) << 1) , 0x001f); // ULsampling rate
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0041, 0xffff);
        }
        else
        {
            printk("\n");
        }
    }
    else
    {
        if ((GetAdcStatus() == false))
        {
            Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON0_L, 0x0000, 0xffff);   //power on uplink
            Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0xa000, 0xffff);   //power off

            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0000, 0xffff);   //Disable ADC CH0_1 (PGA in) Disable ADC CH_2 (PGA in)
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0000, 0xffff);   //Disable PGA CH0_1 (CH0 in) Disable PGA CH_2
            Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x0000, 0xffff);   //Set PGA CH0_1 gain = 0dB  Set PGA CH_2 gain = 0dB
            Ana_Set_Reg(AFE_DCCLK_CFG0, 0x0, 0xffff);   //DC_26M_50K_ off

            Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x2222, 0xffff);   //disable LCLDO19_ADCCH0_1, Remote-Sense
            Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0002, 0xffff);   //disable LCLDO18_ENC (1.8V), Remote-Sense

            //Ana_Set_Reg(AUDMICBIAS_CFG1, 0x2020, 0xffff);   //power on clock
            SetMic2powermode(true);
            SetMic3powermode(true);
            //OpenMicbias3(false);
            //OpenMicbias2(false);
            //EnableMicBias(ADCType, enable);

            //Ana_Set_Reg(AUDMICBIAS_CFG0, 0x0000, 0xffff);   //power on ADC clk
            //CloseMicbias1();
            //CloseMicbias0();

            Ana_Set_Reg(AUDADC_CFG0, 0x0000, 0xffff);   //configure ADC setting

            ClsqAuxEnable(false);
            ClsqEnable(false);
            NvregEnable(false);
            Topck_Enable(false);
            audckbufEnable(false);
            if (GetDLStatus() == false)
            {
                Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);   //turn off afe
            }
            else
            {

            }

        }
        EnableMicBias(ADCType, enable);
        //todo , close analog
        if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC3)
        {

        }
        else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC4)
        {

        }
    }
    return true;
}

static bool TurnOnVOWDigitalHW(bool enable)
{
    printk("%s enable = %d \n", __func__, enable);
    if (enable)
    {
        Ana_Set_Reg(AFE_VOW_TOP, 0x0810, 0xffff);   //VOW control (window mode)
    }
    else
    {
        Ana_Set_Reg(AFE_VOW_TOP, 0x0012, 0xffff);   //VOW control
        Ana_Set_Reg(AFE_VOW_TOP, 0x8012, 0xffff);   //VOW control
    }
    return true;
}

static bool TurnOnVOWADcPowerACC(int ADCType, bool enable)
{
    printk("%s ADCType = %d enable = %d \n", __func__, ADCType, enable);
    if (enable)
    {
#if defined(VOW_TONE_TEST)
        OpenAfeDigitaldl1(false);
        OpenAnalogHeadphone(false);
        EnableSideGenHw(Soc_Aud_InterConnectionOutput_O03, Soc_Aud_InterConnectionOutput_Num_Output, false);
        AudDrv_Clk_Off();
#endif
#if defined (MTK_VOW_SUPPORT)
        //Set VOW driver status first
        VowDrv_EnableHW(true);
#endif
        //uint32 ULIndex = GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
        //uint32 SampleRate = 8000;//mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
        //if (GetAdcStatus() == false) //[Todo]Implement concurrncy test
        {
            NvregEnable(true); // Enable AUDGLB
            //Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0000, 0xffff);
            Ana_Set_Reg(ANALDO_CON3, 0xc542, 0xffff);   //Enable AVDD32_AUD lowpower mode
            Ana_Set_Reg(AUDBUF_CFG4, 0x1, 0xffff);   //Set AUDGLB lowpower mode
            Ana_Set_Reg(AUDMICBIAS_CFG0, 0x004f, 0xffff);   //Set MICBIAS0 lowpower mode - Enable MICBIAS0 (2.7v)
            Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x000f, 0xffff);   //Set LCLDO18_ENC low power mode
            Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x222f, 0xffff);   //Set LCLDO19_ADCCH0_1 low power mode

            Ana_Set_Reg(AUDVOWPLL_CFG2, 0x002b, 0xffff);   //enable PLL relatch
            Ana_Set_Reg(AUDVOWPLL_CFG0, 0x00bd, 0xffff);   //enable PLL

            Ana_Set_Reg(AUDADC_CFG0, 0x3400, 0xffff);   //Set CLK from MADPLL and Enable ADCCLK (12.84MHz/4)

            Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x0002, 0xffff);   //Set PGA CH0_1 gain=12dB
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0011, 0xffff); //Enable PGA CH0_1 (CH0 in)

            Ana_Set_Reg(AUDADC_CFG1, 0x6400, 0xffff);   //Set UP GLB lowpower mode, set ADC low CLK rate mode

            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0005, 0xffff);   //Enable ADC CH0_1 (PGA in)

            //here to set digital part
            //Ana_Set_Reg(TOP_CLKSQ_SET, 0x0003, 0xffff);  //Enable CLKSQ [Todo]Modify ClsqEnable
            ClsqEnable(true);
            ClsqAuxEnable(true);
            //Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x8000, 0xffff);  //Enable CLKSQ [Todo]Modify ClsqEnable


            //Topck_Enable(true);
            LowPowerAdcClockEnable(true);
            //Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x8000, 0xffff);
            //[Todo]Enable VOW INT (has alredy done in pmic.c)
            //enable VOW INT in pmic driver
            //~
#if 1  //Set by HAL
            Ana_Set_Reg(AFE_VOW_CFG0, reg_AFE_VOW_CFG0, 0xffff);   //bias removing reference amp
            Ana_Set_Reg(AFE_VOW_CFG1, reg_AFE_VOW_CFG1, 0xffff);   //watch dog timer initial value
            Ana_Set_Reg(AFE_VOW_CFG2, reg_AFE_VOW_CFG2, 0xffff);   //VOW A/B value
            Ana_Set_Reg(AFE_VOW_CFG3, reg_AFE_VOW_CFG3, 0xffff);   //VOW alpha/beta value
            Ana_Set_Reg(AFE_VOW_CFG4, reg_AFE_VOW_CFG4, 0xffff);   //VOW ADC test config
            Ana_Set_Reg(AFE_VOW_CFG5, reg_AFE_VOW_CFG5, 0xffff);   //N min value
#endif
            //Ana_Set_Reg(AFE_VOW_TOP, 0x0810, 0xffff);   //VOW control (window mode)
            //TurnOnVOWDigitalHW(true);//move to another digital control
#if defined(VOW_TONE_TEST)
            //test output
            AudDrv_Clk_On();
            OpenAfeDigitaldl1(true);
            OpenAnalogHeadphone(true);
#endif
        }

    }
    else
    {
#if defined (MTK_VOW_SUPPORT)
        //Set VOW driver status first
        VowDrv_EnableHW(false);
#endif
        //if (GetAdcStatus() == false)
        {
            // turn off digital first
            //Ana_Set_Reg(AFE_VOW_TOP, 0x0012, 0xffff);   //MAD control
            //Ana_Set_Reg(AFE_VOW_TOP, 0x8012, 0xffff);   //MAD control
            //TurnOnVOWDigitalHW(false);//move to another digital control
            LowPowerAdcClockEnable(false);
            //Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x8000, 0xffff);
            ClsqEnable(false);
            ClsqAuxEnable(false);
            // turn off analog
            Ana_Set_Reg(AUDPREAMP_CFG1, 0x0000, 0xffff);         //Disable ADC CH0_1 (PGA in)
            Ana_Set_Reg(AUDADC_CFG1, 0x0400, 0xffff);            //"Disable UP GLB lowpower mode Disable ADC low CLK rate mode"
            Ana_Set_Reg(AUDPREAMP_CFG0, 0x0000, 0xffff);         //Disable PGA CH0_1 (CH0 in)
            Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x0000, 0xffff);     //Set PGA CH0_1 gain = 0dB
            Ana_Set_Reg(AUDADC_CFG0, 0x0000, 0xffff);            //"Set CLK from CLKSQ Disable ADC CLK (12.84MHz/4)"
            Ana_Set_Reg(AUDVOWPLL_CFG0, 0x00BC, 0xffff);         //Disable PLL (F=32768*(47+2)*8)
            Ana_Set_Reg(AUDVOWPLL_CFG2, 0x0023, 0xffff);         //Disable PLL Relatch
            Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x2222, 0xffff);      //Disable LCLDO19_ADCCH0_1, Remote-Sense
            Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0002, 0xffff);      //"Disable LCLDO18_ENC (1.8V), Remote-Sense  Set LCLDO19_ADC voltage 1.9V"
            Ana_Set_Reg(AUDMICBIAS_CFG0, 0x0000, 0xffff);        //"Disable MICBIAS0 lowpower modeDisable MICBIAS0 (2.7V)"
            Ana_Set_Reg(AUDBUF_CFG4, 0x0000, 0xffff);            //Disable AUDGLB lowpower mode
            Ana_Set_Reg(ANALDO_CON3, 0xC540, 0xffff);            //Disable AVDD32_AUD lowpower mode
            //Ana_Set_Reg( AUDNVREGGLB_CFG0, 0x0001, 0xffff);      //Disable AUDGLB
            //ClsqEnable(false);
            NvregEnable(false);
            //Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0001, 0xffff);
            //Topck_Enable(false);
        }
    }
    return true;
}



// here start uplink power function
static const char *ADC_function[] = {"Off", "On"};
static const char *ADC_power_mode[] = {"normal", "lowpower"};
static const char *PreAmp_Mux_function[] = {"OPEN", "IN_ADC1", "IN_ADC2"};
static const char *ADC_UL_PGA_GAIN[] = { "0Db", "6Db", "12Db", "18Db", "24Db", "30Db"};
static const char *Pmic_Digital_Mux[] = { "ADC1", "ADC2", "ADC3", "ADC4"};
static const char *Adc_Input_Sel[] = { "idle", "AIN", "Preamp"};
static const char *Audio_AnalogMic_Mode[] = { "ACCMODE", "DCCMODE", "DMIC", "DCCECMDIFFMODE", "DCCECMSINGLEMODE"};
static const char *Audio_VOW_ADC_Function[] = {"Off", "On"};
static const char *Audio_VOW_Digital_Function[] = {"Off", "On"};

static const struct soc_enum Audio_UL_Enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function), PreAmp_Mux_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_power_mode), ADC_power_mode),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_ADC_Function), Audio_VOW_ADC_Function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_Digital_Function), Audio_VOW_Digital_Function),
};

static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_ADC1_Get = %d\n", mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1];
    return 0;
}

static int Audio_ADC1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    mutex_lock(&Ana_Power_Mutex);
    if (ucontrol->value.integer.value[0])
    {
        if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1 , true);
        }
        else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1 , true);
        }
        else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
        }
        else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC1 , true);
        }
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] = ucontrol->value.integer.value[0];
    }
    else
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] = ucontrol->value.integer.value[0];
        if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1 , false);
        }
        else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1 , false);
        }
        else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
        }
        else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
        }
    }
    mutex_unlock(&Ana_Power_Mutex);
    return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_ADC2_Get = %d\n", mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2];
    return 0;
}

static int Audio_ADC2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    mutex_lock(&Ana_Power_Mutex);
    if (ucontrol->value.integer.value[0])
    {
        if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2 , true);
        }
        else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2 , true);
        }
        else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2 , true);
        }
        else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC2 , true);
        }
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] = ucontrol->value.integer.value[0];
    }
    else
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] = ucontrol->value.integer.value[0];
        if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2 , false);
        }
        else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2 , false);
        }
        else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2 , false);
        }
        else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC2 , false);
        }
    }
    mutex_unlock(&Ana_Power_Mutex);
    return 0;
}

static int Audio_ADC3_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_ADC3_Get = %d\n", mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC3]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC3];
    return 0;
}

static int Audio_ADC3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    mutex_lock(&Ana_Power_Mutex);
    if (ucontrol->value.integer.value[0])
    {
        if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC3 , true);
        }
        else if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC3 , true);
        }
        else if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC3 , true);
        }
        else if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC3 , true);
        }
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC3] = ucontrol->value.integer.value[0];
    }
    else
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC3] = ucontrol->value.integer.value[0];

        if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC3 , false);
        }
        else if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC3 , false);
        }
        else if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC3 , false);
        }
        else if (mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic3_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC3 , false);
        }
    }
    mutex_unlock(&Ana_Power_Mutex);
    return 0;
}

static int Audio_ADC4_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_ADC4_Get = %d\n", mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC4]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC4];
    return 0;
}

static int Audio_ADC4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    mutex_lock(&Ana_Power_Mutex);
    if (ucontrol->value.integer.value[0])
    {
        if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC4 , true);
        }
        else if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC4 , true);
        }
        else if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC4 , true);
        }
        else if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC4 , true);
        }
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC4] = ucontrol->value.integer.value[0];
    }
    else
    {
        mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC4] = ucontrol->value.integer.value[0];
        if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_ACC)
        {
            TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC4 , false);
        }
        else if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DCC)
        {
            TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC4 , false);
        }
        else if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DMIC)
        {
            TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC4 , false);
        }
        else if (mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF || mAudio_Analog_Mic4_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
        {
            TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC4 , false);
        }
    }
    mutex_unlock(&Ana_Power_Mutex);
    return 0;
}

static int Audio_ADC1_Sel_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1];
    return 0;
}

static void Audio_ADC1_Set_Input(int Input)
{
    if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_OPEN)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0000 << 1), 0x0006);  // pinumx sel
    }
    else if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_ADC)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0001 << 1), 0x0006);
    }
    // ADC2
    else if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0002 << 1), 0x0006);
    }
    else
    {
        printk("%s() warning \n ", __func__);
    }
    printk("%s() done \n", __func__);
}

static int Audio_ADC1_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int input = ucontrol->value.integer.value[0] ;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    Audio_ADC1_Set_Input(input);
    printk("%s() done \n", __func__);
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1] = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_ADC2_Sel_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2];
    return 0;
}

static void Audio_ADC2_Set_Input(int Input)
{
    if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_OPEN)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0000 << 5), 0x0060);  // pinumx sel
    }
    else if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_ADC)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0001 << 5), 0x0060);
    }
    else if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0002 << 5), 0x0060);
    }
    else
    {
        printk("%s() warning \n ", __func__);
    }
}


static int Audio_ADC2_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int input = ucontrol->value.integer.value[0];
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    Audio_ADC2_Set_Input(input);
    printk("%s() done \n", __func__);
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] = ucontrol->value.integer.value[0];
    return 0;
}

static void Audio_ADC3_Set_Input(int Input)
{
    if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_OPEN)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0000 << 8), 0x0300);  // pinumx sel
    }
    else if (Input  == AUDIO_ANALOG_AUDIOANALOG_INPUT_ADC)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0001 << 8), 0x0300);
    }
    else if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0002 << 8), 0x0300);
    }
    else
    {
        printk("%s() warning \n ", __func__);
    }
}

static int Audio_ADC3_Sel_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3];
    return 0;
}

static int Audio_ADC3_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int input = ucontrol->value.integer.value[0];
    printk("%s()\n", __func__);

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    Audio_ADC3_Set_Input(input);
    printk("%s() done \n", __func__);
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3] = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_ADC4_Sel_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4];
    return 0;
}

static void Audio_ADC4_Set_Input(int Input)
{
    if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_OPEN)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0000 << 11), 0x1800);  // pinumx sel
    }
    else if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_ADC)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0001 << 11), 0x1800);
    }
    else if (Input == AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP)
    {
        Ana_Set_Reg(AUDPREAMP_CFG1, (0x0002 << 11), 0x1800);
    }
    else
    {
        printk("%s() warning \n ", __func__);
    }
}


static int Audio_ADC4_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int input = ucontrol->value.integer.value[0];
    printk("%s()\n", __func__);

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    Audio_ADC4_Set_Input(input);
    printk("%s() done \n", __func__);
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4] = ucontrol->value.integer.value[0];
    return 0;
}

static bool AudioPreAmp1_Sel(int Mul_Sel)
{
    printk("%s Mul_Sel = %d ", __func__, Mul_Sel);
    if (Mul_Sel == 0)
    {
        Ana_Set_Reg(AUDPREAMP_CFG0, 0x0000, 0x0030);    // pinumx open
    }
    else if (Mul_Sel == 1)
    {
        Ana_Set_Reg(AUDPREAMP_CFG0, 0x0010, 0x0030);       // ADC 0
    }
    // ADC2
    else if (Mul_Sel == 2)
    {
        Ana_Set_Reg(AUDPREAMP_CFG0, 0x0020, 0x0030);    // ADC 1
    }
    else
    {
        printk("AnalogSetMux warning");
    }

    return true;
}


static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol,
                             struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]; = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1];
    return 0;
}

static int Audio_PreAmp1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);

    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] = ucontrol->value.integer.value[0];
    AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
    printk("%s() done \n", __func__);
    return 0;
}

static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_AmpR_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
    return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index , 0x0007);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = ucontrol->value.integer.value[0];
    return 0;
}


static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_PGA2_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
    return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index << 4, 0x0070);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = ucontrol->value.integer.value[0];
    return 0;
}


static int Audio_PGA3_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_AmpR_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3];
    return 0;
}

static int Audio_PGA3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index << 8, 0x0700);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3] = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_PGA4_Get(struct snd_kcontrol *kcontrol,
                          struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_AmpR_Get = %d \n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4];
    return 0;
}

static int Audio_PGA4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    Ana_Set_Reg(AUDPREAMPGAIN_CFG0, index << 12, 0x7000);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4] = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_MicSource1_Get(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
    printk("Audio_MicSource1_Get = %d\n", mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1];
    return 0;
}

static int Audio_MicSource1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    printk("%s() index = %d done \n", __func__, index);
    Ana_Set_Reg(AFE_MIC_ARRAY_CFG, index | index << 8, 0x0303);
    mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_MicSource2_Get(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2];
    return 0;
}

static int Audio_MicSource2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    printk("%s() done \n", __func__);
    Ana_Set_Reg(AFE_MIC_ARRAY_CFG, index << 2 | index << 10, 0x0c0c);
    mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_MicSource3_Get(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_3]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_3];
    return 0;
}

static int Audio_MicSource3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    printk("%s() done \n", __func__);
    Ana_Set_Reg(AFE_MIC_ARRAY_CFG, index << 4 | index << 12, 0x3030);
    mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_3] = ucontrol->value.integer.value[0];
    return 0;
}


static int Audio_MicSource4_Get(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_4]);
    ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_4];
    return 0;
}

static int Audio_MicSource4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    int index = 0;
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    index = ucontrol->value.integer.value[0];
    printk("%s() done \n", __func__);
    Ana_Set_Reg(AFE_MIC_ARRAY_CFG, index << 6 | index << 14, 0xc0c0);
    mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_4] = ucontrol->value.integer.value[0];
    return 0;
}

// Mic ACC/DCC Mode Setting
static int Audio_Mic1_Mode_Select_Get(struct snd_kcontrol *kcontrol,
                                      struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() mAudio_Analog_Mic1_mode = %d\n", __func__, mAudio_Analog_Mic1_mode);
    ucontrol->value.integer.value[0] = mAudio_Analog_Mic1_mode;
    return 0;
}

static int Audio_Mic1_Mode_Select_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    mAudio_Analog_Mic1_mode = ucontrol->value.integer.value[0];
    printk("%s() mAudio_Analog_Mic1_mode = %d \n", __func__, mAudio_Analog_Mic1_mode);
    return 0;
}

static int Audio_Mic2_Mode_Select_Get(struct snd_kcontrol *kcontrol,
                                      struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %d\n", __func__, mAudio_Analog_Mic2_mode);
    ucontrol->value.integer.value[0] = mAudio_Analog_Mic2_mode;
    return 0;
}

static int Audio_Mic2_Mode_Select_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    mAudio_Analog_Mic2_mode = ucontrol->value.integer.value[0];
    printk("%s() mAudio_Analog_Mic1_mode = %d \n", __func__, mAudio_Analog_Mic2_mode);
    return 0;
}


static int Audio_Mic3_Mode_Select_Get(struct snd_kcontrol *kcontrol,
                                      struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %d\n", __func__, mAudio_Analog_Mic3_mode);
    ucontrol->value.integer.value[0] = mAudio_Analog_Mic3_mode;
    return 0;
}

static int Audio_Mic3_Mode_Select_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    mAudio_Analog_Mic3_mode = ucontrol->value.integer.value[0];
    printk("%s() mAudio_Analog_Mic1_mode = %d \n", __func__, mAudio_Analog_Mic3_mode);
    return 0;
}

static int Audio_Mic4_Mode_Select_Get(struct snd_kcontrol *kcontrol,
                                      struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %d\n", __func__, mAudio_Analog_Mic4_mode);
    ucontrol->value.integer.value[0] = mAudio_Analog_Mic4_mode;
    return 0;
}

static int Audio_Mic4_Mode_Select_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    mAudio_Analog_Mic4_mode = ucontrol->value.integer.value[0];
    printk("%s() mAudio_Analog_Mic1_mode = %d \n", __func__, mAudio_Analog_Mic4_mode);
    return 0;
}

static int Audio_Adc_Power_Mode_Get(struct snd_kcontrol *kcontrol,
                                    struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %d\n", __func__, mAdc_Power_Mode);
    ucontrol->value.integer.value[0] = mAdc_Power_Mode;
    return 0;
}

static int Audio_Adc_Power_Mode_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_power_mode))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    mAdc_Power_Mode = ucontrol->value.integer.value[0];
    printk("%s() mAudio_Analog_Mic1_mode = %d \n", __func__, mAdc_Power_Mode);
    return 0;
}


static int Audio_Vow_ADC_Func_Switch_Get(struct snd_kcontrol *kcontrol,
                                         struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %d\n", __func__, mAudio_Vow_Analog_Func_Enable);
    ucontrol->value.integer.value[0] = mAudio_Vow_Analog_Func_Enable;
    return 0;
}

static int Audio_Vow_ADC_Func_Switch_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_ADC_Function))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }

    if (ucontrol->value.integer.value[0])
    {
        TurnOnVOWADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
    }
    else
    {
        TurnOnVOWADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
    }

    mAudio_Vow_Analog_Func_Enable = ucontrol->value.integer.value[0];
    printk("%s() mAudio_Analog_Mic1_mode = %d \n", __func__, mAudio_Vow_Analog_Func_Enable);
    return 0;
}

static int Audio_Vow_Digital_Func_Switch_Get(struct snd_kcontrol *kcontrol,
                                             struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %d\n", __func__, mAudio_Vow_Digital_Func_Enable);
    ucontrol->value.integer.value[0] = mAudio_Vow_Digital_Func_Enable;
    return 0;
}

static int Audio_Vow_Digital_Func_Switch_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_Digital_Function))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }

    if (ucontrol->value.integer.value[0])
    {
        TurnOnVOWDigitalHW(true);
    }
    else
    {
        TurnOnVOWDigitalHW(false);
    }

    mAudio_Vow_Digital_Func_Enable = ucontrol->value.integer.value[0];
    printk("%s() mAudio_Analog_Mic1_mode = %d \n", __func__, mAudio_Vow_Digital_Func_Enable);
    return 0;
}


static int Audio_Vow_Cfg0_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    int value = /*Ana_Get_Reg(AFE_MAD_CFG0)*/reg_AFE_VOW_CFG0;
    printk("%s()  = %d\n", __func__, value);
    ucontrol->value.integer.value[0] = value;
    return 0;
}

static int Audio_Vow_Cfg0_Set(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
    //Ana_Set_Reg(AFE_MAD_CFG0, ucontrol->value.integer.value[0], 0xffff);
    reg_AFE_VOW_CFG0 = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_Vow_Cfg1_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    int value = /*Ana_Get_Reg(AFE_MAD_CFG1)*/reg_AFE_VOW_CFG1;
    printk("%s()  = %d\n", __func__, value);
    ucontrol->value.integer.value[0] = value;
    return 0;
}

static int Audio_Vow_Cfg1_Set(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
    //Ana_Set_Reg(AFE_MAD_CFG1, ucontrol->value.integer.value[0], 0xffff);
    reg_AFE_VOW_CFG1 = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_Vow_Cfg2_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    int value = /*Ana_Get_Reg(AFE_MAD_CFG2)*/reg_AFE_VOW_CFG2;
    printk("%s()  = %d\n", __func__, value);
    ucontrol->value.integer.value[0] = value;
    return 0;
}

static int Audio_Vow_Cfg2_Set(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
    //Ana_Set_Reg(AFE_MAD_CFG2, ucontrol->value.integer.value[0], 0xffff);
    reg_AFE_VOW_CFG2 = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_Vow_Cfg3_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    int value = /*Ana_Get_Reg(AFE_MAD_CFG3)*/reg_AFE_VOW_CFG3;
    printk("%s()  = %d\n", __func__, value);
    ucontrol->value.integer.value[0] = value;
    return 0;
}

static int Audio_Vow_Cfg3_Set(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
    //Ana_Set_Reg(AFE_MAD_CFG3, ucontrol->value.integer.value[0], 0xffff);
    reg_AFE_VOW_CFG3 = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_Vow_Cfg4_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    int value = /*Ana_Get_Reg(AFE_MAD_CFG4)*/reg_AFE_VOW_CFG4;
    printk("%s()  = %d\n", __func__, value);
    ucontrol->value.integer.value[0] = value;
    return 0;
}

static int Audio_Vow_Cfg4_Set(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
    //Ana_Set_Reg(AFE_MAD_CFG4, ucontrol->value.integer.value[0], 0xffff);
    reg_AFE_VOW_CFG4 = ucontrol->value.integer.value[0];
    return 0;
}

static int Audio_Vow_Cfg5_Get(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    int value = /*Ana_Get_Reg(AFE_MAD_CFG5)*/reg_AFE_VOW_CFG5;
    printk("%s()  = %d\n", __func__, value);
    ucontrol->value.integer.value[0] = value;
    return 0;
}

static int Audio_Vow_Cfg5_Set(struct snd_kcontrol *kcontrol,
                              struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
    //Ana_Set_Reg(AFE_MAD_CFG5, ucontrol->value.integer.value[0], 0xffff);
    reg_AFE_VOW_CFG5 = ucontrol->value.integer.value[0];
    return 0;
}


static bool SineTable_DAC_HP_flag = false;
static bool SineTable_UL2_flag = false;

static int SineTable_UL2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    if (ucontrol->value.integer.value[0])
    {
        Ana_Set_Reg(PMIC_AFE_TOP_CON0 , 0x0002 , 0x2); //set DL sine gen table
        Ana_Set_Reg(AFE_SGEN_CFG0 , 0x0080 , 0xffffffff);
        Ana_Set_Reg(AFE_SGEN_CFG1 , 0x0101 , 0xffffffff);
    }
    else
    {
        Ana_Set_Reg(PMIC_AFE_TOP_CON0 , 0x0002 , 0x2); //set DL sine gen table
        Ana_Set_Reg(AFE_SGEN_CFG0 , 0x0000 , 0xffffffff);
        Ana_Set_Reg(AFE_SGEN_CFG1 , 0x0101 , 0xffffffff);
    }
    return 0;
}

static int SineTable_UL2_Get(struct snd_kcontrol *kcontrol,
                             struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    ucontrol->value.integer.value[0] = SineTable_UL2_flag;
    return 0;
}

static int SineTable_DAC_HP_Get(struct snd_kcontrol *kcontrol,
                                struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    ucontrol->value.integer.value[0] = SineTable_DAC_HP_flag;
    return 0;
}

static int SineTable_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.integer.value[0])
    {
        SineTable_DAC_HP_flag = ucontrol->value.integer.value[0];
        printk("TurnOnDacPower\n");
        audckbufEnable(true);
        ClsqEnable(true);
        Topck_Enable(true);
        NvregEnable(true);
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffffffff);
        Ana_Set_Reg(AFUNC_AUD_CON0, 0xc3a1, 0xffffffff); //sdm audio fifo clock power on
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffffffff); //sdm power on
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x000b, 0xffffffff); //sdm fifo enable
        Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffffffff); //set attenuation gain
        Ana_Set_Reg(AFE_UL_DL_CON0 , 0x0001, 0xffffffff); //[0] afe enable

        Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0 , 0x8330 , 0xffffffff);
        Ana_Set_Reg(AFE_DL_SRC2_CON0_H , 0x8330, 0xffff000f);

        Ana_Set_Reg(AFE_DL_SRC2_CON0_L , 0x1801 , 0xffffffff); //turn off mute function and turn on dl
        Ana_Set_Reg(PMIC_AFE_TOP_CON0 , 0x0001 , 0xffffffff); //set DL  sine gen table
        Ana_Set_Reg(AFE_SGEN_CFG0 , 0x0080 , 0xffffffff);
        Ana_Set_Reg(AFE_SGEN_CFG1 , 0x0101 , 0xffffffff);

        Ana_Set_Reg(0x0680, 0x0000, 0xffff); // Enable AUDGLB
        OpenClassAB();
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)
        Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
        Ana_Set_Reg(ZCD_CON0,   0x0700, 0xffff); // Disable AUD_ZCD
        Ana_Set_Reg(AUDBUF_CFG0,   0xE008, 0xffff); // Disable headphone, voice and short-ckt protection.
        Ana_Set_Reg(IBIASDIST_CFG0,   0x0092, 0xffff); //Enable IBIST
        Ana_Set_Reg(ZCD_CON2,  0x0F9F , 0xffff); //Set HPR/HPL gain as minimum (~ -40dB)
        Ana_Set_Reg(ZCD_CON3,  0x001F , 0xffff); //Set voice gain as minimum (~ -40dB)
        Ana_Set_Reg(AUDBUF_CFG1,  0x0900 , 0xffff); //De_OSC of HP and enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG0,  0xE009 , 0xffff); //Enable voice driver
        Ana_Set_Reg(AUDBUF_CFG1,  0x0940 , 0xffff); //Enable pre-charge buffer
        msleep(1);
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501 , 0xffff); //Enable AUD_CLK
        Ana_Set_Reg(AUDDAC_CFG0, 0x000f , 0xffff); //Enable Audio DAC
        SetDcCompenSation();

        Ana_Set_Reg(AUDBUF_CFG0, 0xE149 , 0xffff); // Switch HP MUX to audio DAC
        Ana_Set_Reg(AUDBUF_CFG0, 0xE14F , 0xffff); // Enable HPR/HPL
        #ifdef CONFIG_CM865_MAINBOARD
        Ana_Set_Reg(AUDBUF_CFG1, 0x0100 , 0xffff); // Disable pre-charge buffer
		#else
        Ana_Set_Reg(AUDBUF_CFG1, 0x0900 , 0xffff); // Disable pre-charge buffer
		#endif
        Ana_Set_Reg(AUDBUF_CFG2, 0x0020 , 0xffff); // Disable De_OSC of voice
        Ana_Set_Reg(AUDBUF_CFG0, 0xE14E , 0xffff); // Disable voice buffer
        Ana_Set_Reg(ZCD_CON2,       0x0489 , 0xffff); // Set HPR/HPL gain as 0dB, step by step

    }
    else
    {
        SineTable_DAC_HP_flag = ucontrol->value.integer.value[0];
        if (GetDLStatus() == false)
        {
            Ana_Set_Reg(AUDBUF_CFG0, 0xE149, 0xffff); // Disable HPR/HPL
            Ana_Set_Reg(AUDDAC_CFG0, 0x0000, 0xffff); // Disable Audio DAC
            Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5500, 0xffff); // Disable AUD_CLK
            Ana_Set_Reg(IBIASDIST_CFG0, 0x0192, 0xffff); // Disable IBIST
            Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0028, 0xffff); // Disable NV regulator (-1.6V)
            Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0000, 0xffff); // Disable cap-less LDOs (1.6V)
            Ana_Set_Reg(AFE_CLASSH_CFG0, 0xd518, 0xffff); // ClassH off
            Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0518, 0xffff); // NCP offset
            Ana_Set_Reg(PMIC_AFE_TOP_CON0 , 0x0000 , 0xffffffff); //set DL normal
        }
    }
    return 0;
}

static void ADC_LOOP_DAC_Func(int command)
{
    if (command == AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON || command == AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON)
    {
        audckbufEnable(true);
        ClsqEnable(true);
        Topck_Enable(true);
        NvregEnable(true);
        Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock
        Ana_Set_Reg(AUDADC_CFG0, 0x0400, 0xffff);      // Enable ADC CLK

        //Ana_Set_Reg(AUDMICBIAS_CFG0, 0x78F, 0xffff);   //Enable MICBIAS0,1 (2.7V)
        OpenMicbias1(true);
        SetMicbias1lowpower(false);
        OpenMicbias0(true);
        SetMicbias0lowpower(false);

        Ana_Set_Reg(AUDMICBIAS_CFG1, 0x285, 0xffff);   //Enable MICBIAS2,3 (2.7V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG1, 0x0007, 0xffff);   //Enable LCLDO18_ENC (1.8V), Remote-Sense
        Ana_Set_Reg(AUDLDO_NVREG_CFG2, 0x2277, 0xffff);   //Enable LCLDO19_ADCCH0_1, Remote-Sense
        Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x0022, 0xffff);   //Set PGA CH0_1 gain = 12dB
        SetMicPGAGain();
        Ana_Set_Reg(AUDPREAMP_CFG0, 0x0051, 0xffff);   //Enable PGA CH0_1 (CH0 in)
        Ana_Set_Reg(AUDPREAMP_CFG1, 0x0055, 0xffff);   //Enable ADC CH0_1 (PGA in)

        Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffffffff); //power on ADC clk
        Ana_Set_Reg(AFE_TOP_CON0, 0x4000, 0xffffffff); //AFE[14] loopback test1 ( UL tx sdata to DL rx)
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffffffff);
        Ana_Set_Reg(AFUNC_AUD_CON0, 0xc3a1, 0xffffffff); //sdm audio fifo clock power on
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffffffff); //sdm power on
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x000b, 0xffffffff); //sdm fifo enable
        Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffffffff); //set attenuation gain
        Ana_Set_Reg(AFE_UL_DL_CON0 , 0x0001, 0xffffffff); //[0] afe enable

        Ana_Set_Reg(AFE_UL_SRC0_CON0_H, 0x0000 , 0x0010); // UL1

        Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0001, 0xffff);   //power on uplink
        Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0, 0x0380, 0xffff); //MTKIF
        Ana_Set_Reg(AFE_DL_SRC2_CON0_H, 0x0800, 0xffff);   //DL
        Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0001, 0xffff); //DL

        // here to start analog part
        //Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0000, 0xffff); //Enable AUDGLB
        OpenClassAB();

        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)
        Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
        Ana_Set_Reg(ZCD_CON0,   0x0700, 0xffff); // Disable AUD_ZCD
        Ana_Set_Reg(AUDBUF_CFG0,   0xE008, 0xffff); // Disable headphone, voice and short-ckt protection.
        Ana_Set_Reg(IBIASDIST_CFG0,   0x0092, 0xffff); //Enable IBIST
        if (command == AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON)
        {
            Ana_Set_Reg(ZCD_CON3,  0x001f , 0xffff); //Set voice gain as minimum (~ -40dB)
            Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
            Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501 , 0xffff); //Enable AUD_CLK
            Ana_Set_Reg(AUDDAC_CFG0, 0x0009 , 0xffff); //Enable Audio DAC
            SetDcCompenSation();

            Ana_Set_Reg(AUDBUF_CFG0, 0xE010 , 0xffff); // Switch HP MUX to audio DAC
            Ana_Set_Reg(AUDBUF_CFG0, 0xE011 , 0xffff); // Enable HPR/HPL
            Ana_Set_Reg(ZCD_CON3,  0x0009 , 0xffff); // Set HPR/HPL gain as 0dB, step by step
        }
        else if (command == AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON)
        {
            Ana_Set_Reg(ZCD_CON2,  0x0F9F , 0xffff); //Set HPR/HPL gain as minimum (~ -40dB)
            Ana_Set_Reg(ZCD_CON3,  0x001f , 0xffff); //Set voice gain as minimum (~ -40dB)
            Ana_Set_Reg(AUDBUF_CFG1,  0x0900 , 0xffff); //De_OSC of HP and enable output STBENH
            Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
            Ana_Set_Reg(AUDBUF_CFG0,  0xE009 , 0xffff); //Enable voice driver
            Ana_Set_Reg(AUDBUF_CFG1,  0x0940 , 0xffff); //De_OSC of HP and enable output STBENH
            Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501 , 0xffff); //Enable AUD_CLK
            Ana_Set_Reg(AUDDAC_CFG0, 0x000F , 0xffff); //Enable Audio DAC
            SetDcCompenSation();

            Ana_Set_Reg(AUDBUF_CFG0, 0xE149 , 0xffff); // Switch HP MUX to audio DAC
            Ana_Set_Reg(AUDBUF_CFG0, 0xE14F , 0xffff); // Enable HPR/HPL
			#ifdef CONFIG_CM865_MAINBOARD
            Ana_Set_Reg(AUDBUF_CFG1, 0x0100 , 0xffff); // Enable HPR/HPL
			#else
            Ana_Set_Reg(AUDBUF_CFG1, 0x0900 , 0xffff); // Enable HPR/HPL
			#endif
            Ana_Set_Reg(AUDBUF_CFG2, 0x0020 , 0xffff); // Enable HPR/HPL
            Ana_Set_Reg(AUDBUF_CFG0, 0xE14E , 0xffff); // Enable HPR/HPL
            Ana_Set_Reg(ZCD_CON2,  0x0489 , 0xffff); // Set HPR/HPL gain as 0dB, step by step
        }
    }
    else
    {
        if (command == AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON)
        {
            Ana_Set_Reg(AUDBUF_CFG0,  0xe010 , 0xffff); // Disable voice driver
            Ana_Set_Reg(AUDDAC_CFG0,  0x0000, 0xffff); // Disable L-ch Audio DAC
        }
        else if (command == AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON)
        {
            Ana_Set_Reg(AUDBUF_CFG0,  0xE149 , 0xffff); // Disable voice DRIVERMODE_CODEC_ONLY
            Ana_Set_Reg(AUDDAC_CFG0,  0x0000, 0xffff); // Disable L-ch Audio DAC
        }
        Ana_Set_Reg(AUDCLKGEN_CFG0,  0x5500, 0xffff); // Disable AUD_CLK
        Ana_Set_Reg(IBIASDIST_CFG0,  0x0192, 0xffff); //Disable IBIST
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,  0x0028, 0xffff); //Disable NV regulator (-1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,  0x0000, 0xffff); //Disable cap-less LDOs (1.6V)
        Ana_Set_Reg(AFE_CLASSH_CFG0, 0xd518, 0xffff); // ClassH offset
        Ana_Set_Reg(AUDLDO_NVREG_CFG0, 0x0518, 0xffff); // NCP offset
    }
}

static bool DAC_LOOP_DAC_HS_flag = false;
static int ADC_LOOP_DAC_HS_Get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HS_flag;
    return 0;
}

static int ADC_LOOP_DAC_HS_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.integer.value[0])
    {
        DAC_LOOP_DAC_HS_flag = ucontrol->value.integer.value[0];
        ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON);
    }
    else
    {
        DAC_LOOP_DAC_HS_flag = ucontrol->value.integer.value[0];
        ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HS_OFF);
    }
    return 0;
}

static bool DAC_LOOP_DAC_HP_flag = false;
static int ADC_LOOP_DAC_HP_Get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HP_flag;
    return 0;
}

static int ADC_LOOP_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

    printk("%s()\n", __func__);
    if (ucontrol->value.integer.value[0])
    {
        DAC_LOOP_DAC_HP_flag = ucontrol->value.integer.value[0];
        ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON);
    }
    else
    {
        DAC_LOOP_DAC_HP_flag = ucontrol->value.integer.value[0];
        ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HP_OFF);
    }
    return 0;
}

static bool Voice_Call_DAC_DAC_HS_flag = false;
static int Voice_Call_DAC_DAC_HS_Get(struct snd_kcontrol *kcontrol,
                                     struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    ucontrol->value.integer.value[0] = Voice_Call_DAC_DAC_HS_flag;
    return 0;
}

static int Voice_Call_DAC_DAC_HS_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s()\n", __func__);
    if (ucontrol->value.integer.value[0])
    {
        Voice_Call_DAC_DAC_HS_flag = ucontrol->value.integer.value[0];
        // here to set voice call 16L setting...
        Ana_Set_Reg(AUDNVREGGLB_CFG0,  0x0000 , 0xffff); //  RG_AUDGLB_PWRDN_VA32 = 1'b0
        Ana_Set_Reg(TOP_CLKSQ,  0x0001, 0xffff); // CKSQ Enable
        Ana_Set_Reg(AUDADC_CFG0,  0x0400, 0xffff); // Enable ADC CLK26CALI
        //Ana_Set_Reg(AUDMICBIAS_CFG0,  0x78f, 0xffff); //  Enable MICBIAS0 (2.7V)
        OpenMicbias1(true);
        SetMicbias1lowpower(false);
        OpenMicbias0(true);
        SetMicbias0lowpower(false);

        Ana_Set_Reg(AUDMICBIAS_CFG1,  0x285, 0xffff); //  Enable MICBIAS2 (2.7V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG1,  0x0007, 0xffff); //   Enable LCLDO18_ENC (1.8V), Remote-Sense ; Set LCLDO19_ADC voltage 1.9V
        Ana_Set_Reg(AUDLDO_NVREG_CFG2,  0x2277, 0xffff); // Enable LCLDO19_ADCCH0_1, Remote-Sense ; Enable LCLDO19_ADCCH_2, Remote-Sense
        Ana_Set_Reg(AUDPREAMPGAIN_CFG0, 0x033, 0xffff); // Set PGA CH0_1 gain = 18dB ; Set PGA CH_2 gain = 18dB
        SetMicPGAGain();
        Ana_Set_Reg(AUDPREAMP_CFG0, 0x051, 0xffff); // Enable PGA CH0_1 (CH0 in) ; Enable PGA CH_2
        Ana_Set_Reg(AUDPREAMP_CFG1, 0x055, 0xffff); // Enable ADC CH0_1 (PGA in) ; Enable ADC CH_2 (PGA in)

        Ana_Set_Reg(TOP_CLKSQ_SET, 0x0003, 0xffff); //CKSQ Enable
        Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x3000, 0xffff); //AUD clock power down released
        Ana_Set_Reg(TOP_CKSEL_CON_CLR, 0x0001, 0x0001); //use internal 26M

        Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   //power on clock

        Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0x0000, 0xffff); // power on ADC clk
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff); // sdm audio fifo clock power on
        Ana_Set_Reg(AFUNC_AUD_CON0, 0xc3a1, 0xffff); // scrambler clock on enable
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff); // sdm power on
        Ana_Set_Reg(AFUNC_AUD_CON2, 0x000b, 0xffff); // sdm fifo enable
        Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffff); // set attenuation gain
        Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff); // afe enable
        Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0, 0x3330, 0xffff); //time slot1= 47, time slot2=24 @ 384K interval.
        Ana_Set_Reg(AFE_DL_SRC2_CON0_H, 0x3330, 0xffff); //16k samplerate
        Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x1801, 0xffff); //turn off mute function and turn on dl
        Ana_Set_Reg(AFE_UL_SRC0_CON0_H, 0x000a, 0xffff); //UL1
        Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0001, 0xffff); //power on uplink

        //============sine gen table============
        Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff); //no loopback
        Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff); //L/R-ch @ sample rate = 8*8K for tone = 0dB of 1K Hz example.
        Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff); //L/R-ch @ sample rate = 8*8K for tone = 0dB of 1K Hz example.

        // ======================here set analog part (audio HP playback)=========================
        Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0000, 0xffff); // [0] RG_AUDGLB_PWRDN_VA32 = 1'b0

        Ana_Set_Reg(AFE_CLASSH_CFG7, 0x8909, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG8, 0x0d0d, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG9, 0x0d10, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG10, 0x1010, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG11, 0x1010, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG12, 0x0000, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG13, 0x0000, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG14, 0x009c, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG26, 0x8d0d, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG27, 0x0d0d, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG28, 0x0d10, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG29, 0x1010, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG30, 0x1010, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG1, 0x0024, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG2, 0x2f90, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG3, 0x1104, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG4, 0x2412, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG5, 0x0201, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG6, 0x2800, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG21, 0xa108, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG22, 0x06db, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG23, 0x0bd6, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG24, 0x1492, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG25, 0x1740, 0xffff); // Classh CK fix 591KHz
        Ana_Set_Reg(AFE_CLASSH_CFG0,   0xd518, 0xffff); // Classh CK fix 591KHz
        msleep(1);
        Ana_Set_Reg(AFE_CLASSH_CFG0,   0xd419, 0xffff); // Classh CK fix 591KHz
        msleep(1);
        Ana_Set_Reg(AFE_CLASSH_CFG1,   0x0021, 0xffff); // Classh CK fix 591KHz
        msleep(1);

        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0028, 0xffff); // Enable cap-less LDOs (1.6V)
        Ana_Set_Reg(AUDLDO_NVREG_CFG0,   0x0068, 0xffff); // Enable NV regulator (-1.6V)
        Ana_Set_Reg(AUDBUF_CFG5, 0x001f, 0xffff); // enable HP bias circuits
        Ana_Set_Reg(ZCD_CON0,   0x0700, 0xffff); // Disable AUD_ZCD
        Ana_Set_Reg(AUDBUF_CFG0,   0xE008, 0xffff); // Disable headphone, voice and short-ckt protection.
        Ana_Set_Reg(IBIASDIST_CFG0,   0x0092, 0xffff); //Enable IBIST

        Ana_Set_Reg(ZCD_CON2,  0x0F9F , 0xffff); //Set HPR/HPL gain as minimum (~ -40dB)
        Ana_Set_Reg(ZCD_CON3,  0x001f , 0xffff); //Set voice gain as minimum (~ -40dB)
        Ana_Set_Reg(AUDBUF_CFG1,  0x0900 , 0xffff); //De_OSC of HP and enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG2,  0x0022 , 0xffff); //De_OSC of voice, enable output STBENH
        Ana_Set_Reg(AUDBUF_CFG0,  0xE009 , 0xffff); //Enable voice driver
        Ana_Set_Reg(AUDBUF_CFG1,  0x0940 , 0xffff); //De_OSC of HP and enable output STBENH
        Ana_Set_Reg(AUDCLKGEN_CFG0, 0x5501 , 0xffff); //Enable AUD_CLK
        Ana_Set_Reg(AUDDAC_CFG0, 0x000F , 0xffff); //Enable Audio DAC
        SetDcCompenSation();

        Ana_Set_Reg(AUDBUF_CFG0, 0xE010 , 0xffff); // Switch HP MUX to audio DAC
        Ana_Set_Reg(AUDBUF_CFG0, 0xE011 , 0xffff);
		#ifdef CONFIG_CM865_MAINBOARD
        Ana_Set_Reg(AUDBUF_CFG1, 0x0100 , 0xffff);
		#else
        Ana_Set_Reg(AUDBUF_CFG1, 0x0900 , 0xffff);
		#endif
        Ana_Set_Reg(AUDBUF_CFG2, 0x0020 , 0xffff);
        //Ana_Set_Reg(AUDBUF_CFG0, 0xE146 , 0xffff); // Enable HPR/HPL
        Ana_Set_Reg(ZCD_CON2,  0x0489 , 0xffff); // Set HPR/HPL gain as 0dB, step by step
        Ana_Set_Reg(ZCD_CON3,  0x0489 , 0xffff); // Set HPR/HPL gain as 0dB, step by step

        //Phone_Call_16k_Vioce_mode_DL_UL

    }
    else
    {
        Voice_Call_DAC_DAC_HS_flag = ucontrol->value.integer.value[0];
    }
    return 0;
}


/*static bool GetLoopbackStatus(void)
{
    printk("%s DAC_LOOP_DAC_HP_flag = %d DAC_LOOP_DAC_HP_flag = %d \n", __func__, DAC_LOOP_DAC_HP_flag, DAC_LOOP_DAC_HP_flag);
    return (DAC_LOOP_DAC_HP_flag || DAC_LOOP_DAC_HP_flag);
}*/


// here start uplink power function
static const char *Pmic_Test_function[] = {"Off", "On"};

static const struct soc_enum Pmic_Test_Enum[] =
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
};

static const struct snd_kcontrol_new mt6331_pmic_Test_controls[] =
{
    SOC_ENUM_EXT("SineTable_DAC_HP", Pmic_Test_Enum[0], SineTable_DAC_HP_Get, SineTable_DAC_HP_Set),
    SOC_ENUM_EXT("DAC_LOOP_DAC_HS", Pmic_Test_Enum[1], ADC_LOOP_DAC_HS_Get, ADC_LOOP_DAC_HS_Set),
    SOC_ENUM_EXT("DAC_LOOP_DAC_HP", Pmic_Test_Enum[2], ADC_LOOP_DAC_HP_Get, ADC_LOOP_DAC_HP_Set),
    SOC_ENUM_EXT("Voice_Call_DAC_DAC_HS", Pmic_Test_Enum[3], Voice_Call_DAC_DAC_HS_Get, Voice_Call_DAC_DAC_HS_Set),
    SOC_ENUM_EXT("SineTable_UL2", Pmic_Test_Enum[4], SineTable_UL2_Get, SineTable_UL2_Set),
};

static const struct snd_kcontrol_new mt6331_UL_Codec_controls[] =
{
    SOC_ENUM_EXT("Audio_ADC_1_Switch", Audio_UL_Enum[0], Audio_ADC1_Get, Audio_ADC1_Set),
    SOC_ENUM_EXT("Audio_ADC_2_Switch", Audio_UL_Enum[1], Audio_ADC2_Get, Audio_ADC2_Set),
    SOC_ENUM_EXT("Audio_ADC_3_Switch", Audio_UL_Enum[2], Audio_ADC3_Get, Audio_ADC3_Set),
    SOC_ENUM_EXT("Audio_ADC_4_Switch", Audio_UL_Enum[3], Audio_ADC4_Get, Audio_ADC4_Set),
    SOC_ENUM_EXT("Audio_Preamp1_Switch", Audio_UL_Enum[4], Audio_PreAmp1_Get, Audio_PreAmp1_Set),
    SOC_ENUM_EXT("Audio_ADC_1_Sel", Audio_UL_Enum[5], Audio_ADC1_Sel_Get, Audio_ADC1_Sel_Set),
    SOC_ENUM_EXT("Audio_ADC_2_Sel", Audio_UL_Enum[6], Audio_ADC2_Sel_Get, Audio_ADC2_Sel_Set),
    SOC_ENUM_EXT("Audio_ADC_3_Sel", Audio_UL_Enum[7], Audio_ADC3_Sel_Get, Audio_ADC3_Sel_Set),
    SOC_ENUM_EXT("Audio_ADC_4_Sel", Audio_UL_Enum[8], Audio_ADC4_Sel_Get, Audio_ADC4_Sel_Set),
    SOC_ENUM_EXT("Audio_PGA1_Setting", Audio_UL_Enum[9], Audio_PGA1_Get, Audio_PGA1_Set),
    SOC_ENUM_EXT("Audio_PGA2_Setting", Audio_UL_Enum[10], Audio_PGA2_Get, Audio_PGA2_Set),
    SOC_ENUM_EXT("Audio_PGA3_Setting", Audio_UL_Enum[11], Audio_PGA3_Get, Audio_PGA3_Set),
    SOC_ENUM_EXT("Audio_PGA4_Setting", Audio_UL_Enum[12], Audio_PGA4_Get, Audio_PGA4_Set),
    SOC_ENUM_EXT("Audio_MicSource1_Setting", Audio_UL_Enum[13], Audio_MicSource1_Get, Audio_MicSource1_Set),
    SOC_ENUM_EXT("Audio_MicSource2_Setting", Audio_UL_Enum[14], Audio_MicSource2_Get, Audio_MicSource2_Set),
    SOC_ENUM_EXT("Audio_MicSource3_Setting", Audio_UL_Enum[15], Audio_MicSource3_Get, Audio_MicSource3_Set),
    SOC_ENUM_EXT("Audio_MicSource4_Setting", Audio_UL_Enum[16], Audio_MicSource4_Get, Audio_MicSource4_Set),
    SOC_ENUM_EXT("Audio_MIC1_Mode_Select", Audio_UL_Enum[17], Audio_Mic1_Mode_Select_Get, Audio_Mic1_Mode_Select_Set),
    SOC_ENUM_EXT("Audio_MIC2_Mode_Select", Audio_UL_Enum[18], Audio_Mic2_Mode_Select_Get, Audio_Mic2_Mode_Select_Set),
    SOC_ENUM_EXT("Audio_MIC3_Mode_Select", Audio_UL_Enum[19], Audio_Mic3_Mode_Select_Get, Audio_Mic3_Mode_Select_Set),
    SOC_ENUM_EXT("Audio_MIC4_Mode_Select", Audio_UL_Enum[20], Audio_Mic4_Mode_Select_Get, Audio_Mic4_Mode_Select_Set),
    SOC_ENUM_EXT("Audio_Mic_Power_Mode", Audio_UL_Enum[21], Audio_Adc_Power_Mode_Get, Audio_Adc_Power_Mode_Set),
    SOC_ENUM_EXT("Audio_Vow_ADC_Func_Switch", Audio_UL_Enum[22], Audio_Vow_ADC_Func_Switch_Get, Audio_Vow_ADC_Func_Switch_Set),
    SOC_ENUM_EXT("Audio_Vow_Digital_Func_Switch", Audio_UL_Enum[23], Audio_Vow_Digital_Func_Switch_Get, Audio_Vow_Digital_Func_Switch_Set),
    SOC_SINGLE_EXT("Audio VOWCFG0 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg0_Get, Audio_Vow_Cfg0_Set),
    SOC_SINGLE_EXT("Audio VOWCFG1 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg1_Get, Audio_Vow_Cfg1_Set),
    SOC_SINGLE_EXT("Audio VOWCFG2 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg2_Get, Audio_Vow_Cfg2_Set),
    SOC_SINGLE_EXT("Audio VOWCFG3 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg3_Get, Audio_Vow_Cfg3_Set),
    SOC_SINGLE_EXT("Audio VOWCFG4 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg4_Get, Audio_Vow_Cfg4_Set),
    SOC_SINGLE_EXT("Audio VOWCFG5 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg5_Get, Audio_Vow_Cfg5_Set),
};

/*static void speaker_event(struct snd_soc_dapm_widget *w,
                          struct snd_kcontrol *kcontrol, int event)
{
    printk("speaker_event = %d\n", event);
    switch (event)
    {
        case SND_SOC_DAPM_PRE_PMU:
            printk("%s SND_SOC_DAPM_PRE_PMU", __func__);
            break;
        case SND_SOC_DAPM_POST_PMU:
            printk("%s SND_SOC_DAPM_POST_PMU", __func__);
            break;
        case SND_SOC_DAPM_PRE_PMD:
            printk("%s SND_SOC_DAPM_PRE_PMD", __func__);
            break;
        case SND_SOC_DAPM_POST_PMD:
            printk("%s SND_SOC_DAPM_POST_PMD", __func__);
        case SND_SOC_DAPM_PRE_REG:
            printk("%s SND_SOC_DAPM_PRE_REG", __func__);
        case SND_SOC_DAPM_POST_REG:
            printk("%s SND_SOC_DAPM_POST_REG", __func__);
            break;
    }
}*/


/* The register address is the same as other codec so it can use resmgr */
/*static int codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
                                struct snd_kcontrol *kcontrol, int event)
{
    printk("codec_enable_rx_bias = %d\n", event);
    switch (event)
    {
        case SND_SOC_DAPM_PRE_PMU:
            printk("%s SND_SOC_DAPM_PRE_PMU", __func__);
            break;
        case SND_SOC_DAPM_POST_PMU:
            printk("%s SND_SOC_DAPM_POST_PMU", __func__);
            break;
        case SND_SOC_DAPM_PRE_PMD:
            printk("%s SND_SOC_DAPM_PRE_PMD", __func__);
            break;
        case SND_SOC_DAPM_POST_PMD:
            printk("%s SND_SOC_DAPM_POST_PMD", __func__);
        case SND_SOC_DAPM_PRE_REG:
            printk("%s SND_SOC_DAPM_PRE_REG", __func__);
        case SND_SOC_DAPM_POST_REG:
            printk("%s SND_SOC_DAPM_POST_REG", __func__);
            break;
    }
    return 0;
}*/

static const struct snd_soc_dapm_widget mt6331_dapm_widgets[] =
{
    /* Outputs */
    SND_SOC_DAPM_OUTPUT("EARPIECE"),
    SND_SOC_DAPM_OUTPUT("HEADSET"),
    SND_SOC_DAPM_OUTPUT("SPEAKER"),
    /*
    SND_SOC_DAPM_MUX_E("VOICE_Mux_E", SND_SOC_NOPM, 0, 0  , &mt6331_Voice_Switch, codec_enable_rx_bias,
    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
    SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
    */

};

static const struct snd_soc_dapm_route mtk_audio_map[] =
{
    {"VOICE_Mux_E", "Voice Mux", "SPEAKER PGA"},
};

static void mt6331_codec_init_reg(struct snd_soc_codec *codec)
{
    printk("%s  \n", __func__);
    Ana_Set_Reg(TOP_CLKSQ, 0x0 , 0xffff);
    Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0001, 0xffff);
    Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x3000, 0x3000);
    Ana_Set_Reg(AUDBUF_CFG0,  0xE000 , 0xe000); //Disable voice DriverVer_type
    // set to lowe power mode
#ifndef CONFIG_MTK_FPGA
    mt6331_upmu_set_rg_audmicbias1lowpen(true); // mic 1 low power mode
    mt6331_upmu_set_rg_audmicbias0lowpen(true); // mic 1 low power mode
#endif
    Ana_Set_Reg(AUDMICBIAS_CFG1, 0x2020, 0xffff);   //power on clock
    Ana_Set_Reg(AFE_ADDA2_UL_SRC_CON1_L, 0xA000, 0xffff);   //power on clock
}

void InitCodecDefault(void)
{
    printk("%s\n", __func__);
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 3;
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 3;
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3] = 3;
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4] = 3;
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;
    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;

    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1] = AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] = AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3] = AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4] = AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
}

static int mt6331_codec_probe(struct snd_soc_codec *codec)
{
    struct snd_soc_dapm_context *dapm = &codec->dapm;
    printk("%s()\n", __func__);
    if (mInitCodec == true)
    {
        return 0;
    }

    snd_soc_dapm_new_controls(dapm, mt6331_dapm_widgets,
                              ARRAY_SIZE(mt6331_dapm_widgets));
    snd_soc_dapm_add_routes(dapm, mtk_audio_map,
                            ARRAY_SIZE(mtk_audio_map));

    //add codec controls
    snd_soc_add_codec_controls(codec, mt6331_snd_controls,
                               ARRAY_SIZE(mt6331_snd_controls));
    snd_soc_add_codec_controls(codec, mt6331_UL_Codec_controls,
                               ARRAY_SIZE(mt6331_UL_Codec_controls));
    snd_soc_add_codec_controls(codec, mt6331_Voice_Switch,
                               ARRAY_SIZE(mt6331_Voice_Switch));
    snd_soc_add_codec_controls(codec, mt6331_pmic_Test_controls,
                               ARRAY_SIZE(mt6331_pmic_Test_controls));

#ifdef CONFIG_MTK_SPEAKER
    snd_soc_add_codec_controls(codec, mt6331_snd_Speaker_controls,
                               ARRAY_SIZE(mt6331_snd_Speaker_controls));
#endif
    snd_soc_add_codec_controls(codec,mt_ext_dev_controls,
                               ARRAY_SIZE(mt_ext_dev_controls));

    snd_soc_add_codec_controls(codec, Audio_snd_auxadc_controls,
                               ARRAY_SIZE(Audio_snd_auxadc_controls));

    // here to set  private data
    mCodec_data = kzalloc(sizeof(mt6331_Codec_Data_Priv), GFP_KERNEL);
    if (!mCodec_data)
    {
        printk("Failed to allocate private data\n");
        return -ENOMEM;
    }
    snd_soc_codec_set_drvdata(codec, mCodec_data);

    memset((void *)mCodec_data , 0 , sizeof(mt6331_Codec_Data_Priv));
    mt6331_codec_init_reg(codec);
    InitCodecDefault();
    mInitCodec = true;

    return 0;
}

static int mt6331_codec_remove(struct snd_soc_codec *codec)
{
    printk("%s()\n", __func__);
    return 0;
}

static unsigned int mt6331_read(struct snd_soc_codec *codec,
                                unsigned int reg)
{
    printk("mt6331_read reg = 0x%x", reg);
    Ana_Get_Reg(reg);
    return 0;
}

static int mt6331_write(struct snd_soc_codec *codec, unsigned int reg,
                        unsigned int value)
{
    printk("mt6331_write reg = 0x%x  value= 0x%x\n", reg, value);
    Ana_Set_Reg(reg , value , 0xffffffff);
    return 0;
}

static int mt6331_Readable_registers(struct snd_soc_codec *codec,
                                     unsigned int reg)
{
    return 1;
}

static int mt6331_volatile_registers(struct snd_soc_codec *codec,
                                     unsigned int reg)
{
    return 1;
}

static struct snd_soc_codec_driver soc_mtk_codec =
{
    .probe    = mt6331_codec_probe,
    .remove = mt6331_codec_remove,

    .read = mt6331_read,
    .write = mt6331_write,

    .readable_register = mt6331_Readable_registers,
    .volatile_register = mt6331_volatile_registers,

    // use add control to replace
    //.controls = mt6331_snd_controls,
    //.num_controls = ARRAY_SIZE(mt6331_snd_controls),

    .dapm_widgets = mt6331_dapm_widgets,
    .num_dapm_widgets = ARRAY_SIZE(mt6331_dapm_widgets),
    .dapm_routes = mtk_audio_map,
    .num_dapm_routes = ARRAY_SIZE(mtk_audio_map),

};

static int mtk_mt6331_codec_dev_probe(struct platform_device *pdev)
{
    pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    if (pdev->dev.dma_mask == NULL)
    {
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
    }

    if (pdev->dev.of_node)
    {
        dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_NAME);
    }

    printk("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
    return snd_soc_register_codec(&pdev->dev,
                                  &soc_mtk_codec, mtk_6331_dai_codecs, ARRAY_SIZE(mtk_6331_dai_codecs));
}

static int mtk_mt6331_codec_dev_remove(struct platform_device *pdev)
{
    printk("%s:\n", __func__);

    snd_soc_unregister_codec(&pdev->dev);
    return 0;

}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_codec_63xx_of_ids[] =
{
    { .compatible = "mediatek,mt_soc_codec_63xx", },
    {}
};
#endif

static struct platform_driver mtk_codec_6331_driver =
{
    .driver = {
        .name = MT_SOC_CODEC_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = mt_soc_codec_63xx_of_ids,
#endif
    },
    .probe  = mtk_mt6331_codec_dev_probe,
    .remove = mtk_mt6331_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec6331_dev;
#endif

static int __init mtk_mt6331_codec_init(void)
{
    int ret = 0;
    printk("%s:\n", __func__);
#ifndef CONFIG_OF
    soc_mtk_codec6331_dev = platform_device_alloc(MT_SOC_CODEC_NAME, -1);
    if (!soc_mtk_codec6331_dev)
    {
        return -ENOMEM;
    }

    ret = platform_device_add(soc_mtk_codec6331_dev);
    if (ret != 0)
    {
        platform_device_put(soc_mtk_codec6331_dev);
        return ret;
    }
#endif
    ret = platform_driver_register(&mtk_codec_6331_driver);
    return ret;
}
module_init(mtk_mt6331_codec_init);

static void __exit mtk_mt6331_codec_exit(void)
{
    printk("%s:\n", __func__);

    platform_driver_unregister(&mtk_codec_6331_driver);
}

module_exit(mtk_mt6331_codec_exit);

/* Module information */
MODULE_DESCRIPTION("MTK  codec driver");
MODULE_LICENSE("GPL v2");

