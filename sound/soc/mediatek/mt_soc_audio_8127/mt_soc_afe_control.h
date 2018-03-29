/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#ifndef _AUDIO_AFE_CONTROL_H
#define _AUDIO_AFE_CONTROL_H

#include "mt_soc_afe_common.h"
#include "mt_soc_afe_connection.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_afe_def.h"
#include <sound/memalloc.h>

#define GIC_PRIVATE_SIGNALS          (32)
#define AUDDRV_DL1_MAX_BUFFER_LENGTH (0x6000)
#define MT8163_AFE_MCU_IRQ_LINE (GIC_PRIVATE_SIGNALS + 142)
#define MASK_ALL          (0xFFFFFFFF)
#define AFE_MASK_ALL  (0xffffffff)

/*bool InitAfeControl(void);*/
bool ResetAfeControl(void);
void Auddrv_Reg_map(void);
void mt_afe_apb_bus_init(void);

int mt_afe_platform_init(void *dev);
void mt_afe_init_control(void *dev);

void mt_afe_platform_deinit(void *dev);

bool mt_afe_set_sample_rate(uint32_t Aud_block, uint32_t SampleRate);
bool mt_afe_set_channels(uint32_t Memory_Interface, uint32_t channel);

bool mt_afe_set_irq_counter(uint32_t Irqmode, uint32_t Counter);
bool mt_afe_set_irq_state(uint32_t Irqmode, bool bEnable);
bool mt_afe_set_irq_rate(uint32_t  Irqmode, uint32_t SampleRate);
uint32_t mt_afe_rate_to_idx(uint32_t sample_rate);
uint32_t mt_afe_sinegen_rate_to_idx(uint32_t sample_rate);
void mt_afe_enable_afe(bool enable);
int mt_afe_enable_memory_path(uint32_t block);
int mt_afe_disable_memory_path(uint32_t block);
bool mt_afe_get_memory_path_state(uint32_t block);
void mt_afe_set_i2s_dac_out(uint32_t sample_rate);
int mt_afe_enable_i2s_dac(void);
int mt_afe_disable_i2s_dac(void);
int mt_afe_enable_i2s_adc(void);
int mt_afe_disable_i2s_adc(void);

void mt_afe_set_2nd_i2s_out(uint32_t sample_rate, uint32_t clock_mode);
int mt_afe_enable_2nd_i2s_out(void);
int mt_afe_disable_2nd_i2s_out(void);
void mt_afe_set_2nd_i2s_in(struct AudioDigtalI2S *mDigitalI2S);

int mt_afe_enable_2nd_i2s_in(void);
int mt_afe_disable_2nd_i2s_in(void);

unsigned int get_sample_rate_index(unsigned int sample_rate);

void mt_afe_enable_afe(bool bEnable);
bool Set2ndI2SOutAttribute(uint32_t sampleRate);
bool Set2ndI2SOut(struct AudioDigtalI2S *DigtalI2S);
bool Set2ndI2SOutEnable(bool benable);
bool SetI2SAdcIn(struct AudioDigtalI2S *DigtalI2S);
bool Set2ndI2SAdcIn(struct AudioDigtalI2S *DigtalI2S);
bool SetDLSrc2(uint32_t SampleRate);

bool GetMrgI2SEnable(void);
bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate);
bool SetDaiBt(struct AudioDigitalDAIBT *mAudioDaiBt);
bool SetDaiBtEnable(bool bEanble);

bool SetI2SAdcEnable(bool bEnable);
bool Set2ndI2SAdcEnable(bool bEnable);
bool SetI2SDacOut(uint32_t SampleRate, bool Lowgitter, bool I2SWLen);
bool SetHwDigitalGainMode(uint32_t GainType, uint32_t SampleRate, uint32_t SamplePerStep);
bool SetHwDigitalGainEnable(int GainType, bool Enable);
bool SetHwDigitalGain(uint32_t Gain , int GainType);

bool SetMemDuplicateWrite(uint32_t InterfaceType, int dupwrite);
bool mt_afe_enable_sinegen_hw(uint32_t connection , bool direction);
int mt_afe_disable_sinegen_hw(void);
bool SetSideGenSampleRate(uint32_t SampleRate);
bool CleanPreDistortion(void);
bool EnableSideToneFilter(bool stf_on);
bool SetModemPcmEnable(int modem_index, bool modem_pcm_on);
bool SetModemPcmConfig(int modem_index, struct AudioDigitalPCM p_modem_pcm_attribute);

bool Set2ndI2SIn(struct AudioDigtalI2S *mDigitalI2S);
bool Set2ndI2SInEnable(bool bEnable);

bool mt_afe_set_i2s_asrc_config(bool bIsUseASRC, unsigned int dToSampleRate);
int mt_afe_enable_i2s_asrc(void);
int mt_afe_disable_i2s_asrc(void);

bool checkUplinkMEMIfStatus(void);
bool SetoutputConnectionFormat(uint32_t ConnectionFormat, uint32_t  Output);


uint32_t GetHDMIApLLSource(void);
/*bool SetHDMIMCLK(void);*/
bool SetHDMIBCLK(void);
bool SetHDMIdatalength(uint32_t length);
#if 0
bool SetHDMIApLL(uint32_t ApllSource);
bool SetHDMIsamplerate(uint32_t samplerate);
void EnableHDMIDivPower(uint32_t Diveder_name, bool bEnable);
void EnableSpdifDivPower(uint32_t Diveder_name, bool bEnable);
void EnableSpdif2DivPower(uint32_t Diveder_name, bool bEnable);
#endif
bool SetHDMIConnection(uint32_t ConnectionState, uint32_t Input , uint32_t Output);
bool SetHDMIChannels(uint32_t Channels);
bool SetHDMIEnable(bool bEnable);
uint32_t  SetCLkHdmiBclk(uint32_t MckDiv, uint32_t SampleRate, uint32_t Channels , uint32_t bitDepth);



void SetHdmiPcmInterConnection(unsigned int connection_state, unsigned int channels);
/* Soc_Aud_I2S_WLEN_WLEN_16BITS or Soc_Aud_I2S_WLEN_WLEN_32BITS */

/* APLL , low jitter mode setting */
void EnableApll(uint32_t SampleRate, bool bEnable);
uint32_t SetCLkMclk(uint32_t I2snum, uint32_t SampleRate);
void EnableI2SDivPower(uint32_t Diveder_name, bool bEnable);
#if 0
void EnableApll1(bool bEnable);
void EnableApll2(bool bEnable);
#endif
void  SetCLkBclk(uint32_t MckDiv, uint32_t SampleRate, uint32_t Channels , uint32_t Wlength);

int afe_allocate_mem_buffer(struct device *pDev, enum Soc_Aud_Digital_Block MemBlock, uint32_t Buffer_length);
struct AFE_MEM_CONTROL_T  *get_mem_control_t(enum Soc_Aud_Digital_Block MemBlock);
bool set_memif_substream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream);
bool RemoveMemifSubStream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream);
bool ClearMemBlock(enum Soc_Aud_Digital_Block MemBlock);

/* interrupt handler */

void afe_dl1_spinlock_lock(void);
void afe_dl1_spinlock_unlock(void);
void afe_dl1_interrupt_handler(void);
void afe_dl2_interrupt_handler(void);
void afe_ul1_interrupt_handler(void);
void afe_ul1_spinlock_lock(void);
void afe_ul1_spinlock_unlock(void);
void afe_awb_interrupt_handler(void);
void afe_dai_interrupt_handler(void);
void afe_hdmi_interrupt_handler(void);
uint32_t get_mem_copysizebystream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream);
void set_mem_copysizebystream(enum Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream, uint32_t size);

struct snd_dma_buffer *afe_get_mem_buffer(enum Soc_Aud_Digital_Block MemBlock);
int afe_allocate_dl1_buffer(struct device *pDev, uint32_t Afe_Buf_Length);


bool backup_audio_register(void);
bool afe_restore_audio_register(void);

void afe_control_mutex_lock(void);
void afe_control_mutex_unlock(void);

/* Sram management function */
void afe_control_sram_lock(void);
void afe_control_sram_unlock(void);
size_t GetCaptureSramSize(void);
unsigned int get_sramstate(void);
void clear_sramstate(unsigned int State);
void set_sramstate(unsigned int State);
unsigned int get_playback_sram_fullsize(void);
unsigned int GetPLaybackSramPartial(void);
unsigned int GetPLaybackDramSize(void);
size_t GetCaptureDramSize(void);

/* offsetTrimming */
void OpenAfeDigitaldl1(bool bEnable);
void SetExternalModemStatus(const bool bEnable);

/* set VOW status for AFE GPIO control */
void SetVOWStatus(bool bEnable);
bool ConditionEnterSuspend(void);
void SetFMEnableFlag(bool bEnable);
void SetOffloadEnableFlag(bool bEnable);
void SetOffloadSWMode(bool bEnable);

bool set_offload_cbk(enum Soc_Aud_Digital_Block block, void *offloadstream, void (*offloadCbk)(void *stream));
bool clr_offload_cbk(enum Soc_Aud_Digital_Block block, void *offloadstream);

unsigned int align64bytesize(unsigned int insize);


void set_hdmi_out_control(unsigned int channels);

void set_hdmi_out_control_enable(bool enable);

void set_hdmi_i2s(void);

void set_hdmi_i2s_enable(bool enable);

void set_hdmi_clock_source(unsigned int sample_rate);

void set_hdmi_bck_div(unsigned int sample_rate);


#ifdef CONFIG_OF
int GetGPIO_Info(int type, int *pin, int *pinmode);
#endif

#endif
