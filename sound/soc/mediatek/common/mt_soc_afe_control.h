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

/*
DO NOT USER DIRECTLY, use irq manager
bool SetIrqMcuCounter(uint32 Irqmode, uint32 Counter);
bool SetIrqEnable(uint32 Irqmode, bool bEnable);
bool SetIrqMcuSampleRate(uint32 Irqmode, uint32 SampleRate);
*/
bool SetConnectionFormat(uint32 ConnectionFormat, uint32 Aud_block);
bool SetConnection(uint32 ConnectionState, uint32 Input, uint32 Output);
bool SetIntfConnection(uint32 ConnectionState, uint32 Aud_block_In, uint32 Aud_block_Out);
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

int setConnsysI2SIn(AudioDigtalI2S *DigtalI2S);
int setConnsysI2SInEnable(bool enable);
int setConnsysI2SAsrc(bool bIsUseASRC, unsigned int dToSampleRate);
int setConnsysI2SEnable(bool enable);

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

bool EnableSineGen(uint32 connection, bool direction, bool Enable);
bool SetSineGenSampleRate(uint32 SampleRate);
bool SetSineGenAmplitude(uint32 ampDivide);

bool SetModemPcmEnable(int modem_index, bool modem_pcm_on);
bool SetModemPcmConfig(int modem_index, AudioDigitalPCM p_modem_pcm_attribute);

bool Set2ndI2SIn(AudioDigtalI2S *mDigitalI2S);
bool Set2ndI2SInConfig(unsigned int sampleRate, bool bIsSlaveMode);
bool Set2ndI2SInEnable(bool bEnable);

bool checkDllinkMEMIfStatus(void);
bool checkUplinkMEMIfStatus(void);
bool SetMemIfFetchFormatPerSample(uint32 InterfaceType, uint32 eFetchFormat);
bool SetMemIfFormatReg(uint32 InterfaceType, uint32 eFetchFormat);
bool SetoutputConnectionFormat(uint32 ConnectionFormat, uint32 Output);

bool SetChipI2SAdcIn(AudioDigtalI2S *DigtalI2S, bool audioAdcI2SStatus);
bool setChipDmicPath(bool _enable, uint32 sample_rate);

/* Sample Rate Transform */
uint32 SampleRateTransform(uint32 sampleRate, Soc_Aud_Digital_Block audBlock);

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
bool GetOffloadSWMode(void);

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

/* IRQ Register Control Table and Handler Function Table*/
void RunIRQHandler(enum Soc_Aud_IRQ_MCU_MODE irqIndex);
const struct Aud_IRQ_CTRL_REG *GetIRQCtrlReg(enum Soc_Aud_IRQ_MCU_MODE irqIndex);
const struct Aud_RegBitsInfo *GetIRQPurposeReg(enum Soc_Aud_IRQ_PURPOSE sIrqPurpose);

bool SetHighAddr(Soc_Aud_Digital_Block MemBlock, bool usingdram, dma_addr_t addr);

/* GetEnableAudioBlockRegOffset */
enum MEM_BLOCK_ENABLE_REG_INDEX {
	MEM_BLOCK_ENABLE_REG_INDEX_AUDIO_BLOCK = 0,
	MEM_BLOCK_ENABLE_REG_INDEX_REG,
	MEM_BLOCK_ENABLE_REG_INDEX_OFFSET,
	MEM_BLOCK_ENABLE_REG_INDEX_NUM
};
uint32 GetEnableAudioBlockRegOffset(uint32 Aud_block);
uint32 GetEnableAudioBlockRegAddr(uint32 Aud_block);

/* FM AP Dependent */
bool SetFmI2sConnection(uint32 ConnectionState);
bool SetFmAwbConnection(uint32 ConnectionState);
int SetFmI2sInEnable(bool enable);
int SetFmI2sIn(AudioDigtalI2S *mDigitalI2S);
bool GetFmI2sInPathEnable(void);
bool SetFmI2sInPathEnable(bool bEnable);
int SetFmI2sAsrcEnable(bool bEnable);
int SetFmI2sAsrcConfig(bool bIsUseASRC, unsigned int dToSampleRate);

/* ANC AP Dependent */
bool SetAncRecordReg(uint32 value, uint32 mask);

/* irq from other module */
bool is_irq_from_ext_module(void);
int start_ext_sync_signal(void);
int stop_ext_sync_signal(void);
int ext_sync_signal(void);
void ext_sync_signal_lock(void);
void ext_sync_signal_unlock(void);


/* for vcore dvfs */
int vcore_dvfs(bool *enable, bool reset);
void set_screen_state(bool state);

#endif
