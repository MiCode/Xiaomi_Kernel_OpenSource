/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_afe_control.h
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
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#ifndef _AUDIO_AFE_CONTROL_H
#define _AUDIO_AFE_CONTROL_H

#include "AudDrv_Type_Def.h"
#include "AudDrv_Common.h"
#include "mt_soc_digital_type.h"
#include "AudDrv_Def.h"
#include <sound/memalloc.h>

#define GIC_PRIVATE_SIGNALS          (32)
#define AUDDRV_DL1_MAX_BUFFER_LENGTH (0x6000)
#define MT6735_AFE_MCU_IRQ_LINE (GIC_PRIVATE_SIGNALS + 144)
#define MASK_ALL          (0xFFFFFFFF)
#define AFE_MASK_ALL  (0xffffffff)

bool InitAfeControl(struct device *pDev);
bool ResetAfeControl(void);
bool Register_Aud_Irq(void *dev, uint32 afe_irq_number);
void Auddrv_Reg_map(void);

bool SetSampleRate(uint32 Aud_block, uint32 SampleRate);
bool SetChannels(uint32 Memory_Interface, uint32 channel);
int SetMemifMonoSel(uint32 Memory_Interface, bool mono_use_r_ch);
/*
DO NOT USER DIRECTLY, use irq manager
bool SetIrqMcuCounter(uint32 Irqmode, uint32 Counter);
bool SetIrqEnable(uint32 Irqmode, bool bEnable);
bool SetIrqMcuSampleRate(uint32 Irqmode, uint32 SampleRate);
*/
bool SetConnection(uint32 ConnectionState, uint32 Input, uint32 Output);
bool SetMemoryPathEnable(uint32 Aud_block, bool bEnable);
bool GetMemoryPathEnable(uint32 Aud_block);
bool SetI2SDacEnable(bool bEnable);
bool GetI2SDacEnable(void);
void EnableAfe(bool bEnable);
bool Set2ndI2SOutAttribute(uint32_t sampleRate);
bool Set2ndI2SOut(AudioDigtalI2S *DigtalI2S);
bool Set2ndI2SOutEnable(bool benable);
bool SetI2SAdcIn(AudioDigtalI2S *DigtalI2S);
bool setDmicPath(bool _enable);

void SetULSrcEnable(bool bEnable);
void SetADDAEnable(bool bEnable);

bool SetExtI2SAdcIn(AudioDigtalI2S *DigtalI2S);
bool SetExtI2SAdcInEnable(bool bEnable);

bool Set2ndI2SAdcIn(AudioDigtalI2S *DigtalI2S);
bool SetDLSrc2(uint32 SampleRate);

bool GetMrgI2SEnable(void);
bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate);
bool SetDaiBt(AudioDigitalDAIBT *mAudioDaiBt);
bool SetDaiBtEnable(bool bEanble);

bool SetI2SAdcEnable(bool bEnable);
bool Set2ndI2SAdcEnable(bool bEnable);
bool SetI2SDacOut(uint32 SampleRate, bool Lowgitter, bool I2SWLen);
bool SetHwDigitalGainMode(uint32 GainType, uint32 SampleRate, uint32 SamplePerStep);
bool SetHwDigitalGainEnable(int GainType, bool Enable);
bool SetHwDigitalGain(uint32 Gain, int GainType);

bool SetMemDuplicateWrite(uint32 InterfaceType, int dupwrite);

bool EnableSineGen(uint32 connection, bool direction, bool Enable);
bool SetSineGenSampleRate(uint32 SampleRate);
bool SetSineGenAmplitude(uint32 ampDivide);

bool CleanPreDistortion(void);
bool EnableSideToneFilter(bool stf_on);
bool SetModemPcmEnable(int modem_index, bool modem_pcm_on);
bool SetModemPcmConfig(int modem_index, AudioDigitalPCM p_modem_pcm_attribute);

bool Set2ndI2SIn(AudioDigtalI2S *mDigitalI2S);
bool Set2ndI2SInConfig(unsigned int sampleRate, bool bIsSlaveMode);
bool Set2ndI2SInEnable(bool bEnable);

bool SetI2SASRCConfig(bool bIsUseASRC, unsigned int dToSampleRate);
bool SetI2SASRCEnable(bool bEnable);

bool checkDllinkMEMIfStatus(void);
bool checkUplinkMEMIfStatus(void);
bool SetMemIfFetchFormatPerSample(uint32 InterfaceType, uint32 eFetchFormat);
bool SetoutputConnectionFormat(uint32 ConnectionFormat, uint32 Output);

bool SetHDMIApLL(uint32 ApllSource);
uint32 GetHDMIApLLSource(void);
bool SetHDMIMCLK(void);
bool SetHDMIBCLK(void);
bool SetHDMIdatalength(uint32 length);
bool SetHDMIsamplerate(uint32 samplerate);
bool SetHDMIConnection(uint32 ConnectionState, uint32 Input, uint32 Output);
bool SetHDMIChannels(uint32 Channels);
bool SetHDMIEnable(bool bEnable);

bool SetTDMLrckWidth(uint32 cycles);
bool SetTDMbckcycle(uint32 cycles);
bool SetTDMChannelsSdata(uint32 channels);

/* Soc_Aud_I2S_WLEN_WLEN_16BITS or Soc_Aud_I2S_WLEN_WLEN_32BITS */

bool SetTDMDatalength(uint32 length);
bool SetTDMI2Smode(uint32 mode);
bool SetTDMLrckInverse(bool enable);
bool SetTDMBckInverse(bool enable);
bool SetTDMEnable(bool enable);

/* Sample Rate Transform */
uint32 SampleRateTransform(uint32 sampleRate, Soc_Aud_Digital_Block audBlock);

/* APLL , low jitter mode setting */
void EnableALLbySampleRate(uint32 SampleRate);
void DisableALLbySampleRate(uint32 SampleRate);
void EnableAPLLTunerbySampleRate(uint32 SampleRate);
void DisableAPLLTunerbySampleRate(uint32 SampleRate);
uint32 SetCLkMclk(uint32 I2snum, uint32 SampleRate);
void EnableI2SDivPower(uint32 Diveder_name, bool bEnable);
void EnableApll1(bool bEnable);
void EnableApll2(bool bEnable);
void SetCLkBclk(uint32 MckDiv, uint32 SampleRate, uint32 Channels, uint32 Wlength);

int AudDrv_Allocate_mem_Buffer(struct device *pDev, Soc_Aud_Digital_Block MemBlock,
			       uint32 Buffer_length);
AFE_MEM_CONTROL_T *Get_Mem_ControlT(Soc_Aud_Digital_Block MemBlock);
bool SetMemifSubStream(Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream);
bool RemoveMemifSubStream(Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream);
bool ClearMemBlock(Soc_Aud_Digital_Block MemBlock);

/* interrupt handler */

void Auddrv_Dl1_Spinlock_lock(void);
void Auddrv_Dl1_Spinlock_unlock(void);
void Auddrv_Dl2_Spinlock_lock(void);
void Auddrv_Dl2_Spinlock_unlock(void);
void Auddrv_Dl3_Spinlock_lock(void);
void Auddrv_Dl3_Spinlock_unlock(void);

void Auddrv_DL1_Interrupt_Handler(void);
void Auddrv_DL2_Interrupt_Handler(void);
void Auddrv_UL1_Interrupt_Handler(void);
void Auddrv_UL1_Spinlock_lock(void);
void Auddrv_UL1_Spinlock_unlock(void);
void Auddrv_UL2_Spinlock_lock(void);
void Auddrv_UL2_Spinlock_unlock(void);
void Auddrv_AWB_Interrupt_Handler(void);
void Auddrv_DAI_Interrupt_Handler(void);
void Auddrv_HDMI_Interrupt_Handler(void);
void Auddrv_UL2_Interrupt_Handler(void);
void Auddrv_MOD_DAI_Interrupt_Handler(void);
kal_uint32 Get_Mem_CopySizeByStream(Soc_Aud_Digital_Block MemBlock,
				    struct snd_pcm_substream *substream);
void Set_Mem_CopySizeByStream(Soc_Aud_Digital_Block MemBlock, struct snd_pcm_substream *substream,
			      uint32 size);

struct snd_dma_buffer *Get_Mem_Buffer(Soc_Aud_Digital_Block MemBlock);
int AudDrv_Allocate_DL1_Buffer(struct device *pDev, kal_uint32 Afe_Buf_Length,
	dma_addr_t dma_addr, unsigned char *dma_area);


bool BackUp_Audio_Register(void);
bool Restore_Audio_Register(void);

void AfeControlMutexLock(void);
void AfeControlMutexUnLock(void);

/* Sram management  function */
void AfeControlSramLock(void);
void AfeControlSramUnLock(void);
size_t GetCaptureSramSize(void);
unsigned int GetSramState(void);
void ClearSramState(unsigned int State);
void SetSramState(unsigned int State);
unsigned int GetPLaybackSramFullSize(void);
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

unsigned int Align64ByteSize(unsigned int insize);

void AudDrv_checkDLISRStatus(void);

/* sram mamager */
bool InitSramManager(struct device *pDev, unsigned int sramblocksize);
bool CheckSramAvail(unsigned int mSramLength, unsigned int *mSramBlockidx, unsigned int *mSramBlocknum);
int AllocateAudioSram(dma_addr_t *sram_phys_addr, unsigned char **msram_virt_addr,
	unsigned int mSramLength, void *user);
int freeAudioSram(void *user);

/* IRQ Manager */
int init_irq_manager(void);
int irq_add_user(const void *_user,
		 enum Soc_Aud_IRQ_MCU_MODE _irq,
		 unsigned int _rate,
		 unsigned int _count);
int irq_remove_user(const void *_user,
		    enum Soc_Aud_IRQ_MCU_MODE _irq);
int irq_update_user(const void *_user,
		    enum Soc_Aud_IRQ_MCU_MODE _irq,
		    unsigned int _rate,
		    unsigned int _count);

/* IRQ Manager */

bool SetHighAddr(Soc_Aud_Digital_Block MemBlock, bool usingdram);

#endif
