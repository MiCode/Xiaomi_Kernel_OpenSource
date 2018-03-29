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
 *   mt6583.c
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
 * Ship Hsu
 *
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *					 C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *				E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Gpio.h"
#include "AudDrv_Clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_codec_63xx.h"
#include "mt_soc_pcm_common.h"
#include <linux/mutex.h>
#include <linux/wakelock.h>

/*
 *	function implementation
 */

#define GIC_PRIVATE_SIGNALS		  (32)
#define AUDDRV_DL1_MAX_BUFFER_LENGTH (0x6000)
#define MT6595_AFE_MCU_IRQ_LINE (GIC_PRIVATE_SIGNALS + 0x86)
#define MT6595_AFE_MCU_ANC_TO_AP_LINE (0xC6)
#define ANC_DEVNAME "ancservice"

/*#define ANC_MD32_TO_HOST_IPC (ANC_MD32_BASE + 0x04)
#define ANC_HOST_TO_MD32_IPC (ANC_MD32_BASE + 0x08)
#define ANC_SEMAPHORE		(ANC_MD32_BASE + 0x50)
*/

#define ANC_MD32_BASE   0x11025000
#define ANC_MD32_PTCM   0x11020000
#define ANC_MD32_DTCM   0x11022000
#define ANC_A2M_IPC_DATA   (ANC_MD32_DTCM+0x3800)
#define ANC_M2A_IPC_DATA   (ANC_MD32_DTCM+0x3000)
#define ANC_MD32_SRAM   0x11025000

#define MD32_PTCM_SIZE 0x4000
#define MD32_DTCM_SIZE 0x2800
#define MD32_CFGREG_SIZE 0x100

#define MD32_BASE_REG		   (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR))
#define MD32_TO_HOST_REG		(*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0004))
#define HOST_TO_MD32_REG		(*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0008))

#define MD32_GENERAL_REG0	   (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x000C))
#define MD32_GENERAL_REG1	   (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0010))
#define MD32_GENERAL_REG2	   (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0014))
#define MD32_GENERAL_REG3	   (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0018))

#define MD32_DEBUG_PC_REG	   (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0030))
#define MD32_DEBUG_R14_REG	  (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0034))
#define MD32_DEBUG_R15_REG	  (*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0038))
#define MD32_WDT_REG			(*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0040))

#define MD32_AUD_MD32_D1		(*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0020))
#define MD32_AUD_MD32_D2		(*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0024))
#define MD32_MD32_AUD_D1		(*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x0028))
#define MD32_MD32_AUD_D2		(*(volatile unsigned int *)(MD32_REG_VIRTUAL_ADDR + 0x002C))


#define ReadREG_ANC(_addr) (*(volatile unsigned int *)(_addr))
#define WriteREG_ANC(_addr, _value) (*(volatile unsigned int *)(_addr) = (_value))
#define ReadREG16_ANC(_addr, _value) ((_value) = *(volatile unsigned short *)(_addr))
#define WriteREG16_ANC(_addr, _value) (*(volatile unsigned short *)(_addr) = (_value))

static int mtk_anc_probe(struct platform_device *pdev);
static int mtk_anc_close(struct snd_pcm_substream *substream);
static int mtk_anc_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_anc_platform_probe(struct snd_soc_platform *platform);

static DEFINE_MUTEX(to_md32_ipc_lock);

#define AUD_DRV_ANC_IOC_MAGIC 'C'
/* ANC Control */
#define SET_ANC_CONTROL		  _IOW(AUD_DRV_ANC_IOC_MAGIC, 0x1, int)
#define SET_ANC_PARAMETER		_IOW(AUD_DRV_ANC_IOC_MAGIC, 0x2, int)
#define GET_ANC_PARAMETER		_IOW(AUD_DRV_ANC_IOC_MAGIC, 0x3, int)

#define MD32_DATA_IMAGE_PATH	"/system/etc/firmware/md32_d.bin"
#define MD32_PROGRAM_IMAGE_PATH "/system/etc/firmware/md32_p.bin"

#define MD32TCM_DM_BASE   0xF0028000
#define MD32TCM_PM_BASE   0xF0020000
#define MD32_BASE		 0xf0050000
#define ANC_PC			 (MD32_BASE + 0x60)
#define ANC_PWR			 (MD32_BASE)
#define ANC_HANDSHAKE_ID_ADDR			 (MD32TCM_DM_BASE + 0x3FF0)
#define ANC_HANDSHAKE_VAL_ADDR			(MD32TCM_DM_BASE + 0x3FF4)
#define MD32_COEF_OFFSET 0x1200

static kal_uint32 dump_analog;
static kal_uint32 IPC_wait_queue_flag;

#define MAX_TABS 68
int md32_coef[MAX_TABS];


DECLARE_WAIT_QUEUE_HEAD(IPC_Wait_Queue);

void *MD32_REG_VIRTUAL_ADDR;
void *MD32_DTCM_VIRTUAL_ADDR;
void *MD32_PTCM_VIRTUAL_ADDR;
/*void *SPM_AUDIO_PWR_CON;
void *SPM_AUDIO_ACCESS;
#define WriteREG(_addr, _value) (*(volatile uint32 *)(_addr) = (uint32)(_value))
#define ReadREG(_addr)		  (*(volatile uint32 *)(_addr))
*/

typedef enum {
	M2A_DumpReady,
	M2A_DebugMsgReady,

	A2M_SetStatus,
	M2A_SetStatusAck,

	A2M_UpdateParameter,
	M2A_UpdateParameterAck,

	A2M_EnableDump,
	M2A_EnableDumpAck,

	A2M_EnableDebug,
	M2A_EnableDebugAck,

} MD32_IPC_MSG;

static struct snd_soc_pcm_runtime *pruntimepcm;

static void memcpy_md32(void __iomem *trg, const void *src, int size)
{
	int i;

	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < (size >> 2); i++)
		*t++ = *s++;
}

int get_md32_img_sz(const char *IMAGE_PATH)
{
	struct file *filp = NULL;
	struct inode *inode;

	off_t fsize = 0;

	filp = filp_open(IMAGE_PATH, O_RDONLY, 0644);

	if (!IS_ERR(filp)) {
		inode = filp->f_dentry->d_inode;
		fsize = inode->i_size;
	} else {
		pr_err("Open MD32 image %s FAIL!\n", IMAGE_PATH);
		return -1;
	}

	filp_close(filp, NULL);
	return fsize;
}

void upload_coef(void)
{
	pr_debug("upload_coef\n");
	memcpy((void *)(MD32_DTCM_VIRTUAL_ADDR + MD32_COEF_OFFSET), md32_coef,
		   MAX_TABS * 4);
}

void update_coef(void)
{
	upload_coef();
	MD32_GENERAL_REG1 = 0x1;

	while (MD32_GENERAL_REG1 != 0)
		;

	pr_debug("update coff success\n");
}

int load_md32(const char *IMAGE_PATH, void *dst)
{
	struct file *filp = NULL;
	unsigned char *buf = NULL;
	unsigned char *ptr;
	struct inode *inode;

	off_t fsize;
	mm_segment_t fs;

	ptr = buf;

	filp = filp_open(IMAGE_PATH, O_RDONLY, 0644);

	if (IS_ERR(filp)) {
		pr_debug("[ANC_MD32] Open MD32 image %s FAIL!\n", IMAGE_PATH);
		goto error;
	} else {
		inode = filp->f_dentry->d_inode;
		fsize = inode->i_size;
		pr_debug("[ANC_MD32] file %s size: %i\n", IMAGE_PATH, (int)fsize);
		buf = kmalloc((size_t)fsize + 1 , GFP_KERNEL);
		fs = get_fs();
		set_fs(KERNEL_DS);
		filp->f_op->read(filp, buf, fsize, &(filp->f_pos));
		set_fs(fs);
		buf[fsize] = '\0';
		/*pr_debug("<1>The File Content is:\n");
		pr_debug("<1>%s",buf);
		memcpy(dst, buf, fsize);*/
		memcpy_md32(dst, buf, fsize);
	}

	filp_close(filp, NULL);
	kfree(buf);
	return fsize;

error:
	if (filp != NULL)
		filp_close(filp, NULL);

	kfree(buf);
	return -1;

}


#define IRQ3_FS_POS		 16
#define IRQ3_FS_LEN		 4
#define IRQ3_ON		 2

#define ADDA_adda_afe_on_POS	0
#define ADDA_adda_afe_on_LEN	1

#define ADDA_afe_ul_dl_con0_reserved_POS	1
#define ADDA_afe_ul_dl_con0_reserved_LEN	14

void get_io_remap(void)
{
	MD32_REG_VIRTUAL_ADDR = ioremap_nocache(ANC_MD32_BASE, MD32_CFGREG_SIZE);
	MD32_PTCM_VIRTUAL_ADDR = ioremap_nocache(ANC_MD32_PTCM, MD32_PTCM_SIZE);
	MD32_DTCM_VIRTUAL_ADDR = ioremap_nocache(ANC_MD32_DTCM, MD32_DTCM_SIZE);
}

void setDebugDump(bool enable)
{
	if (enable) {
		/* bConnect IO_0/1 to O21/22 */
		Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x1 << 11, 0x1 << 11);
		/* bConnect IO_2/3 to O5/6 */
		Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x1 << 12, 0x1 << 12);
	} else {
		/* bConnect IO_0/1 to O21/22 */
		Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0 << 11, 0x1 << 11);
		/* bConnect IO_2/3 to O5/6 */
		Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0 << 12, 0x1 << 12);
	}
}

void preset(void)
{
	volatile uint32 *AFE_Register;
	volatile uint32 *SPM_Register;

	AFE_Register = (volatile uint32 *)Get_Afe_Powertop_Pointer();
	SPM_Register = ioremap_nocache(0x10006000, 0x1000);

	pr_debug("ANCService_ioctl test(before) AFE_Register: %x", *AFE_Register);
	*SPM_Register = 0x0B160001L;
	/* AFE_Register |= 0xDL; */
	*AFE_Register &= 0x00FFFFFFL;
	*AFE_Register |= 0x00330000L;
	/* AFE_Register |= 0x0003000DL; */

	get_io_remap();

	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0 , 0x3 << 1);
	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0, 0x7 << 4); /* adda2_anc_dl_input_mode:260k */
	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x1 << 3, 0x1 << 3); /* afe adda2_dl_src_on */
	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x1 << 7, 0x1 << 7); /* ul_dn25_sel */

	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0 << 1, 0x1 << 1); /* use md32, not sgen */
	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0 << 2, 0x1 << 2); /* use md32, not sgen */

    /* anc_up8x_rxif_adc_voice_mode:8: time slot1 = 78, time slot2 = 24 @ 260K interval */
	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x8 << 28, 0xf << 28);

	Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x3f << ADDA_afe_ul_dl_con0_reserved_POS,
				((2^ADDA_afe_ul_dl_con0_reserved_LEN)-1) << ADDA_afe_ul_dl_con0_reserved_POS);

	SetADDAEnable(true); /* adda_afe_on:  1: enable */

	AudDrv_GPIO_Request(true, Soc_Aud_Digital_Block_ADDA_ANC);

	pr_debug("ANCService_ioctl test(after) AFE_Register: %x", *AFE_Register);
}

void enable_uplink_path(void)
{
	/* mtkif rx rg_voice mode set to 260k */
	Afe_Set_Reg(AFE_ADDA_NEWIF_CFG2, 0x8 << 28, 0xf << 28);

	SetULSrcEnable(true); /* UL SRC on which will enable mtk if rx */

	AudDrv_ADC_Clk_On();
}

void disable_uplink_path(void)
{
	AudDrv_ADC_Clk_Off();
	SetULSrcEnable(false);
	/* anc_tx off */
	Afe_Set_Reg(AFE_ADDA2_TOP_CON0, 0x0 << 1, 0x1 << 1);
}

void md32_write_processed(void)
{
	MD32_GENERAL_REG0 = 0x0;
}

void md32_write_mic_data(void)
{
	MD32_GENERAL_REG0 = 0x1;
}

void md32_write_tone(void)
{
	MD32_GENERAL_REG0 = 0x2;
}

void md32_write_seq_data(void)
{
	MD32_GENERAL_REG0 = 0x3;
}

void md32_write_silence(void)
{
	MD32_GENERAL_REG0 = 0x4;
}

void download_md32_binary(void)
{
	int p_sz, d_sz, ret;

	msleep(50);

	/* unsigned char *md32_data_image={0}; */
	/* unsigned char *md32_program_image={0}; */
	/* unsigned char *d_buf, *p_buf; */
	p_sz = get_md32_img_sz(MD32_PROGRAM_IMAGE_PATH);
	d_sz = get_md32_img_sz(MD32_DATA_IMAGE_PATH);


	/* (*(volatile unsigned int *)(SPM_AUDIO_ACCESS)) = 0x0b160001; */
	/* *AFE_Register |= 0x00330F0D */

	pr_debug("ANC_Service %s p_sz:%d d_sz:%d\n", __func__, p_sz, d_sz);

	MD32_BASE_REG = 0x0;	/* turn off md32 */
	do {
		if (p_sz > 0)
			ret = load_md32(MD32_PROGRAM_IMAGE_PATH, MD32_PTCM_VIRTUAL_ADDR);

		if (d_sz > 0)
			ret = load_md32(MD32_DATA_IMAGE_PATH, MD32_DTCM_VIRTUAL_ADDR);
		MD32_TO_HOST_REG = 0x11220000;
		upload_coef();
		MD32_BASE_REG = 0x1;
		pr_debug("[ANC_MD32] MD32 download success and bootup\n");
		return;

	} while (0);
	pr_debug("[ANC_MD32] boot up failed!!! free images\n");
	/*
	if(md32_data_image)
		kfree(md32_data_image);
	if(md32_program_image)
		kfree(md32_program_image);
		*/
}

/* =============write file============= */
mm_segment_t oldfs;

void InitKernelEnv(void)
{
	oldfs = get_fs();
	set_fs(KERNEL_DS);
}

void DinitKernelEnv(void)
{
	set_fs(oldfs);
}

struct file *OpenFile(char *path, int flag, int mode)
{
	struct file *fp;

	fp = filp_open(path, flag, 0);
	if (fp)
		return fp;
	else
		return NULL;
}

int WriteFile(struct file *fp, char *buf, int readlen)
{
	if (fp->f_op && fp->f_op->read)
		return fp->f_op->write(fp, buf, readlen, &fp->f_pos);
	else
		return -1;
}

int ReadFile(struct file *fp, char *buf, int readlen)
{
	if (fp->f_op && fp->f_op->read)
		return fp->f_op->read(fp, buf, readlen, &fp->f_pos);
	else
		return -1;
}

int CloseFile(struct file *fp)
{
	filp_close(fp, NULL);
	return 0;
}

/* ============================== */


static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};
static int ANCService_open(struct inode *inode, struct file *fp)
{
	pr_debug("%s inode:%p, file:%p\n", __func__, inode, fp);
	return 0;
}

static int ANCService_release(struct inode *inode, struct file *fp)
{
	pr_debug("%s inode:%p, file:%p\n", __func__, inode, fp);

	if (!(fp->f_mode & FMODE_WRITE || fp->f_mode & FMODE_READ))
		return -ENODEV;
	return 0;
}

void md32_dump_memory(void)
{
	struct file *fp;

	InitKernelEnv();

	/* write to file */
	fp = OpenFile("/sdcard/mtklog/md32_mem.dat", O_CREAT | O_WRONLY, 0);
	if (fp != NULL) {
		WriteFile(fp, (char *)ANC_MD32_DTCM, 16384);
		CloseFile(fp);
	}

	DinitKernelEnv();

}

void md32_dump_reg(void)
{
	/*
		pr_debug("===DUMP MD32 Memory===\n");
		pr_debug("MD32_TO_HOST_REG  = 0x%x\n", MD32_TO_HOST_REG);
		pr_debug("MD32_MD32_TO_SPM_REG  = 0x%x\n", MD32_MD32_TO_SPM_REG);
		pr_debug("HOST_TO_MD32_REG  = 0x%x\n", HOST_TO_MD32_REG);
		pr_debug("MD32_SPM_TO_MD32_REG  = 0x%x\n", MD32_SPM_TO_MD32_REG);
		pr_debug("MD32_DEBUG_PC_REG  = 0x%x\n", MD32_DEBUG_PC_REG);
		pr_debug("MD32_DEBUG_R14_REG  = 0x%x\n", MD32_DEBUG_R14_REG);
		pr_debug("MD32_DEBUG_R15_REG  = 0x%x\n", MD32_DEBUG_R15_REG);
		pr_debug("MD32_WDT_REG  = 0x%x\n", MD32_WDT_REG);
		pr_debug("MD32_TO_SPM_REG  = 0x%x\n", MD32_TO_SPM_REG);
		*/
	/* pr_debug("AFE_GENERAL_REG10  = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG10)); */
	md32_dump_memory();
}


void wait_ipc_ack(void)
{
	int ret;

	pr_debug("ANC wait_event_interruptible_timeout\n");
	ret = wait_event_interruptible_timeout(IPC_Wait_Queue, IPC_wait_queue_flag,
					       msecs_to_jiffies(1000));
	if (ret < 0)
		pr_debug("ANC md32 irq failed, ret=%d", ret);
}

void trigger_ipc_l(void)
{
	pr_debug("ANC trigger_ipc_l\n");
	IPC_wait_queue_flag = 0;
	HOST_TO_MD32_REG = 0x1;
	/*	ANC_M2A_IPC_DATA */
}

void trigger_ipc_wait_l(void)
{
	trigger_ipc_l();
	wait_ipc_ack();
}

void wake_up_ipc_wait(void)
{
	pr_debug("ANC wake_up_ipc_wait\n");
	IPC_wait_queue_flag = 1;
	wake_up_interruptible(&IPC_Wait_Queue);
}

void require_ipc_lock(void)
{
	pr_debug("ANC require_ipc_lock\n");
	mutex_lock(&to_md32_ipc_lock);
	pr_debug("ANC require_ipc_lock get\n");
}

void release_ipc_lock(void)
{
	mutex_unlock(&to_md32_ipc_lock);
}

void notify_md32_update_parameter(void)
{
	pr_debug("notify_md32_update_parameter\n");
	require_ipc_lock();
	WriteREG_ANC(ANC_A2M_IPC_DATA, A2M_UpdateParameter);
	trigger_ipc_wait_l();
	release_ipc_lock();
}

void notify_md32_set_status(bool Enable)
{
	pr_debug("notify_md32_enable_debug\n");
	require_ipc_lock();
	WriteREG_ANC(ANC_A2M_IPC_DATA, A2M_SetStatus);
	WriteREG_ANC(ANC_A2M_IPC_DATA + 0x4, Enable ? 0x1 : 0x0);
	trigger_ipc_wait_l();
	release_ipc_lock();
}

void notify_md32_enable_dump(bool Enable)
{
	pr_debug("notify_md32_enable_dump\n");
	require_ipc_lock();
	WriteREG_ANC(ANC_A2M_IPC_DATA, A2M_EnableDump);
	WriteREG_ANC(ANC_A2M_IPC_DATA + 0x4, Enable ? 0x1 : 0x0);
	trigger_ipc_wait_l();
	release_ipc_lock();
}

void notify_md32_enable_debug(bool Enable)
{
	pr_debug("notify_md32_enable_debug\n");
	require_ipc_lock();
	WriteREG_ANC(ANC_A2M_IPC_DATA, A2M_EnableDebug);
	WriteREG_ANC(ANC_A2M_IPC_DATA + 0x4, Enable ? 0x1 : 0x0);
	trigger_ipc_wait_l();
	release_ipc_lock();
}

void on_md32_debug_message_ready(void)
{

}

void on_md32_dump_data_ready(void)
{

}

void on_md32_ipc_trigger(void)
{
	/* ANC_M2A_IPC_DATA */
	int irq_type = ReadREG_ANC(ANC_M2A_IPC_DATA);

	switch (irq_type) {
	/* MD32 notify ap actively. */
	case M2A_DumpReady:
		pr_debug("ANC M2A_DumpReady\n");
		on_md32_dump_data_ready();
		break;

	case M2A_DebugMsgReady:
		pr_debug("ANC M2A_DebugMsgReady\n");
		on_md32_debug_message_ready();
		break;


	/* Types of Ack */
	case M2A_SetStatusAck:
		pr_debug("ANC M2A_SetStatusAck\n");
		wake_up_ipc_wait();
		break;
	case M2A_UpdateParameterAck:
		pr_debug("ANC M2A_UpdateParameterAck\n");
		wake_up_ipc_wait();
		break;
	case M2A_EnableDumpAck:
		pr_debug("ANC M2A_StartDumpAck\n");
		wake_up_ipc_wait();
		break;
	case M2A_EnableDebugAck:
		pr_debug("ANC M2A_StartDebugAck\n");
		wake_up_ipc_wait();
		break;
	}
}

static long ANCService_ioctl(struct file *fp, unsigned int cmd,
				 unsigned long arg)
{
	int  ret = 0;

	pr_debug("ANCService_ioctl cmd = %u arg = %lu\n", cmd, arg);

	switch (cmd) {
	case SET_ANC_CONTROL: {
		pr_debug("SET_ANC_CONTROL(%lu)", arg);
		switch (arg) {
		case 1:
			md32_dump_reg();
			break;
		case 2:
			download_md32_binary();
			break;
		case 30:
			md32_write_processed();
			break;
		case 31:
			md32_write_silence();
			break;
		case 33:
			md32_write_tone();
			break;
		case 34:
			md32_write_seq_data();
			break;
		case 35:
			md32_write_mic_data();
			break;
		case 40:
			Afe_Log_Print();
			break;
		case 81:
			AudDrv_Clk_On();
			AudDrv_ANC_Clk_On();
			SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_ANC, true);
			EnableAfe(true);
			preset();
			download_md32_binary();
			enable_uplink_path();
			break;
		case 82:
			SetADDAEnable(false);
			SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_ANC, false);
			EnableAfe(false);
			AudDrv_GPIO_Request(false, Soc_Aud_Digital_Block_ADDA_ANC);
			disable_uplink_path();
			AudDrv_ANC_Clk_Off();
			AudDrv_Clk_Off();
			break;
		case 91:
			update_coef();
			break;
		case 21:
			pr_debug("send ipc from ap to md32");
			trigger_ipc_l();
			break;
		case 22:
			pr_debug("ANC notify_md32_set_status(true);");
			notify_md32_set_status(true);
			break;
		case 23:
			pr_debug("ANC notify_md32_set_status(false);");
			notify_md32_set_status(false);
			break;
		default:
			pr_debug("SET_ANC_CONTROL no such command = %lu", arg);
			break;
		}
		break;
	}
	case SET_ANC_PARAMETER: {
		pr_debug("SET_ANC_CONTROL(%lu)", arg);
		switch (arg) {
		default:
			pr_debug("SET_ANC_CONTROL no such command = %lu", arg);
			break;
		}
		break;
	}
	case GET_ANC_PARAMETER: {
		pr_debug("SET_ANC_CONTROL(%lu)", arg);
		switch (arg) {
		default:
			pr_debug("SET_ANC_CONTROL no such command = %lu", arg);
			break;
		}
		break;
	}
	}
	return ret;
}

/*
 * ioctl32 compat
 */
#ifdef CONFIG_COMPAT

static long ANCService_ioctl_compat(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	file->f_op->unlocked_ioctl(file, cmd, arg);
	return 0;
}

#else
#define ANCService_ioctl_compat   NULL
#endif

static ssize_t ANCService_write(struct file *fp, const char __user *data,
				size_t count, loff_t *offset)
{
	char temp_str[MAX_TABS * 4];

	pr_debug("ANCService_write write count %d", (int)count);

	if (count > MAX_TABS * 4)
		return -EINVAL;

	if (copy_from_user(temp_str, data, count) != 0)
		return -EFAULT;

	memcpy((void *)md32_coef, (void *)temp_str, count);

	pr_debug("ANCService_write write done");
	return 0;
}

static ssize_t ANCService_read(struct file *fp,  char __user *data,
				   size_t count, loff_t *offset)
{
	char buffer[1380] = {0};

	int n = 0;
	char *cur = buffer;
	int retval;
	int i;

	volatile uint32 *AFE_Register = (volatile uint32 *)Get_Afe_Powertop_Pointer();

	if (!dump_analog) {
		cur += sprintf((char *)cur, "===DUMP MD32 Memory===\n");
		cur += sprintf((char *)cur, "MD32_BASE_REG  = 0x%x\n", MD32_BASE_REG);
		cur += sprintf((char *)cur, "MD32_TO_HOST_REG  = 0x%x\n", MD32_TO_HOST_REG);
		cur += sprintf((char *)cur, "HOST_TO_MD32_REG  = 0x%x\n", HOST_TO_MD32_REG);
		cur += sprintf((char *)cur, "MD32_DEBUG_PC_REG  = 0x%x\n", MD32_DEBUG_PC_REG);
		cur += sprintf((char *)cur, "MD32_DEBUG_R14_REG  = 0x%x\n", MD32_DEBUG_R14_REG);
		cur += sprintf((char *)cur, "MD32_DEBUG_R15_REG  = 0x%x\n", MD32_DEBUG_R15_REG);
		cur += sprintf((char *)cur, "MD32_WDT_REG  = 0x%x\n", MD32_WDT_REG);
		cur += sprintf((char *)cur, "MD32_AUD_MD32_D1  = 0x%x\n", MD32_AUD_MD32_D1);
		cur += sprintf((char *)cur, "MD32_AUD_MD32_D2  = 0x%x\n", MD32_AUD_MD32_D2);
		cur += sprintf((char *)cur, "MD32_MD32_AUD_D1  = 0x%x\n", MD32_MD32_AUD_D1);
		cur += sprintf((char *)cur, "MD32_MD32_AUD_D2  = 0x%x\n", MD32_MD32_AUD_D2);
		cur += sprintf((char *)cur, "MD32_GENERAL_REG0  = 0x%x\n", MD32_GENERAL_REG0);
		cur += sprintf((char *)cur, "MD32_GENERAL_REG1  = 0x%x\n", MD32_GENERAL_REG1);
		cur += sprintf((char *)cur, "MD32_GENERAL_REG2  = 0x%x\n", MD32_GENERAL_REG2);
		cur += sprintf((char *)cur, "MD32_GENERAL_REG3  = 0x%x\n", MD32_GENERAL_REG3);

		cur += sprintf((char *)cur, "AFE_GENERAL_REG0 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG0));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG1 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG1));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG2 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG2));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG3 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG3));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG4 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG4));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG5 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG5));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG6 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG6));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG7 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG7));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG8 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG8));
		cur += sprintf((char *)cur, "AFE_GENERAL_REG9 = 0x%x\n",
				   Afe_Get_Reg(AFE_GENERAL_REG9));

		cur += sprintf((char *)cur, "AFE_Register(SPM)  = 0x%x\n", *AFE_Register);

		cur += sprintf((char *)cur, "MD32_Coef:\n");
		for (i = 0; i < MAX_TABS; i++)
			cur += sprintf((char *)cur, "%d,", md32_coef[i]);
		cur += sprintf((char *)cur, "\n");

	}

	n = (int)(cur - (char *)buffer);
	if (n > count)
		n = count;
	retval = copy_to_user(data, buffer, n);

	return n;
}

static int ANCService_flush(struct file *flip, fl_owner_t id)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int ANCService_fasync(int fd, struct file *flip, int mode)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int ANCService_remap_mmap(struct file *flip, struct vm_area_struct *vma)
{
	pr_debug("%s\n", __func__);
	return -1;
}

static const struct file_operations ANCService_fops = {
	.owner		  = THIS_MODULE,
	.open		   = ANCService_open,
	.release		= ANCService_release,
	.unlocked_ioctl = ANCService_ioctl,
	.compat_ioctl  =  ANCService_ioctl_compat,
	.write		  = ANCService_write,
	.read		   = ANCService_read,
	.flush		  = ANCService_flush,
	.fasync		 = ANCService_fasync,
	.mmap		   = ANCService_remap_mmap
};

static struct miscdevice ANCService_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = ANC_DEVNAME,
	.fops = &ANCService_fops,
};

static int mtk_anc_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("mtk_anc_pcm_open\n");

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);


	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_debug("mtk_anc_pcm_open runtime rate = %d channels = %d\n", runtime->rate,
		   runtime->channels);
	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	if (substream->pcm->device & 2) {
		runtime->hw.info &= ~(SNDRV_PCM_INFO_MMAP |
					  SNDRV_PCM_INFO_MMAP_VALID);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_playback_constraints\n");

	if (ret < 0) {
		pr_debug("mtk_anc_close\n");
		mtk_anc_close(substream);
		return ret;
	}
	pr_debug("mtk_anc_pcm_open return\n");
	return 0;
}

static int mtk_anc_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_anc_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_anc_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return -EINVAL;
}

static int mtk_anc_copy(struct snd_pcm_substream *substream,
			int channel, snd_pcm_uframes_t pos,
			void __user *dst, snd_pcm_uframes_t count)
{
	pr_debug("mtk_anc_copy pos = %lu count = %lu\n ", pos, count);
	return 0;
}

static int mtk_anc_silence(struct snd_pcm_substream *substream,
			   int channel, snd_pcm_uframes_t pos,
			   snd_pcm_uframes_t count)
{
	pr_debug("mtk_anc_silence\n");
	return 0; /* do nothing */
}


static void *anc_page[2];

static struct page *mtk_anc_page(struct snd_pcm_substream *substream,
				 unsigned long offset)
{
	pr_debug("mtk_anc_page\n");
	return virt_to_page(anc_page[substream->stream]); /* the same page */
}

static int mtk_anc_prepare(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_alsa_prepare\n");
	return 0;
}

static int mtk_anc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	pr_debug("mtk_anc_hw_params\n");
	return ret;
}

static int mtk_anc_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_anc_hw_free\n");
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_afe_ops = {
	.open =	 mtk_anc_pcm_open,
	.close =	mtk_anc_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	mtk_anc_hw_params,
	.hw_free =  mtk_anc_hw_free,
	.prepare =  mtk_anc_prepare,
	.trigger =  mtk_anc_trigger,
	.copy =	 mtk_anc_copy,
	.silence =  mtk_anc_silence,
	.page =	 mtk_anc_page,
};

static struct snd_soc_platform_driver mtk_soc_anc_platform = {
	.ops		= &mtk_afe_ops,
	.pcm_new	= mtk_anc_new,
	.probe	  = mtk_anc_platform_probe,
};

irqreturn_t AudDrv_ANC_IRQ_handler(int irq, void *dev_id)
{
	pr_debug("AudDrv_ANC_IRQ_handler %d\n", irq);
	/*MD32_TO_SPM_REG = 0x0;
	on_md32_ipc_trigger();
	MD32_TO_HOST_REG =  0x0; */
	return IRQ_HANDLED;
}

bool Register_Aud_ANC_Irq(void *dev)
{
	int ret = 0;

	pr_debug("Register_Aud_ANC_Irq %s dev name =%s\n", __func__, dev_name(dev));

	/* ret = request_irq(MT6595_AFE_MCU_ANC_TO_AP_LINE,
	AudDrv_ANC_IRQ_handler, IRQF_TRIGGER_NONE, "MD32 IPC_MD2HOST", NULL); */
	if (ret)
		pr_debug("[anc md32] request irq failed : %d\n", ret);
	else
		pr_debug("[anc md32] request irq successful\n");

	return ret;
}

static int mtk_anc_probe(struct platform_device *pdev)
{
	int ret;

	dump_analog = 0;
	IPC_wait_queue_flag = 0;
	pruntimepcm = NULL;

	pr_debug("mtk_anc_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_ANC_PCM);
	Register_Aud_ANC_Irq(&pdev->dev);
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	ret = misc_register(&ANCService_misc_device);
	/* register MISC device */
	if (ret) {
		pr_debug("ANCService misc_register Fail:%d\n", ret);
		return ret;
	}

	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_anc_platform);
}

static int mtk_anc_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pruntimepcm  = rtd;
	pr_debug("%s\n", __func__);
	return ret;
}

static int mtk_anc_platform_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_anc_platform_probe\n");
	return 0;
}


static int mtk_afeanc_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_anc_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_anc", },
	{}
};
#endif

static struct platform_driver mtk_anc_driver = {
	.driver = {
		.name = MT_SOC_ANC_PCM,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_anc_of_ids,
#endif
	},
	.probe = mtk_anc_probe,
	.remove = mtk_afeanc_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_anc_dev;
#endif

static int __init mtk_soc_anc_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkafe_anc_dev = platform_device_alloc(MT_SOC_ANC_PCM , -1);
	if (!soc_mtkafe_anc_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkafe_anc_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_anc_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_anc_driver);

	return ret;

}

module_init(mtk_soc_anc_platform_init);

static void __exit mtk_soc_anc_platform_exit(void)
{

	pr_debug("%s\n", __func__);
	platform_driver_unregister(&mtk_anc_driver);
}
module_exit(mtk_soc_anc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
