/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>

/* #include <linux/xlog.h> */

#include <linux/io.h>
/* ============================================================ */

/* #include <linux/uaccess.h> */
/* #include <linux/module.h> */
/* #include <linux/fs.h> */
/* #include <linux/platform_device.h> */
/* #include <linux/cdev.h> */
#include <linux/interrupt.h>
/* #include <asm/io.h> */
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
/* #include <linux/earlysuspend.h> */
/* #include <linux/mm.h> */
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
/* #include <linux/slab.h> */
/* #include <linux/gfp.h> */
/* #include <linux/aee.h> */
#include <linux/timer.h>
/* #include <linux/disp_assert_layer.h> */
/* #include <linux/xlog.h> */
/* #include <linux/fs.h> */

/* Arch dependent files */
/* #include <asm/mach/map.h> */
/* #include <mach/mt6577_pll.h> */
/* #include <mach/mt_irq.h> */
/* #include <mach/irqs.h> */

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif				/* defined(CONFIG_MTK_LEGACY) */

/* #include <asm/tcm.h> */
#include <asm/cacheflush.h>
/* #include <asm/system.h> */
/* #include <linux/mm.h> */
#include <linux/pagemap.h>

#ifndef FPGA_VERSION
/* #include <mach/mt_boot.h> */
#endif

#ifdef MTK_JPEG_CMDQ_SUPPORT
#include <cmdq_core.h>
#include <cmdq_record.h>
#endif

#ifndef JPEG_DEV
#include <linux/proc_fs.h>
#endif

#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/of_device.h>
#endif
#ifdef JPEG_PM_DOMAIN_ENABLE
/*#include <linux/pm_runtime.h>*/
#include "mtk_smi.h"
#endif
/* ========================================================== */

#include "jpeg_drv.h"
#include "jpeg_drv_common.h"

#ifdef MTK_JPEG_CMDQ_SUPPORT
#include "jpeg_cmdq.h"
#endif
/* #define USE_SYSRAM */

#define JPEG_DEVNAME "mtk_jpeg"
#define JDEC_DEVNAME "mtk_jdec"

#define TABLE_SIZE 4096

#define JPEG_DEC_PROCESS 0x1
#define JPEG_ENC_PROCESS 0x2

/* #define FPGA_VERSION */
#include "jpeg_drv_reg.h"

#include "smi_public.h"

/* Support QoS */
#include <linux/pm_qos.h>

/* -------------------------------------------------------------------------- */
/*  */
/* -------------------------------------------------------------------------- */

#ifdef JPEG_DEV
/* device and driver */
static dev_t jenc_devno;
static struct cdev *jenc_cdev;
static struct class *jenc_class;

#ifdef JPEG_PM_DOMAIN_ENABLE
static dev_t jdec_devno;
static struct cdev *jdec_cdev;
static struct class *jdec_class;
#endif

#endif

static struct JpegDeviceStruct gJpegqDev;
#ifdef CONFIG_OF
static struct JpegDeviceStruct *gJpegqDevs;
static int nrJpegDevs;

static const struct of_device_id venc_jpg_of_ids[] = {
	{.compatible = "mediatek,venc_jpg",},
	{}
};

#ifdef JPEG_PM_DOMAIN_ENABLE
static const struct of_device_id jdec_of_ids[] = {
	{.compatible = "mediatek,jpgdec",},
	{}
};
#endif

#endif

#ifndef CONFIG_MTK_CLKMGR
static struct JpegClk gJpegClk;
#endif
/* decoder */
#ifdef JPEG_DEC_DRIVER
static wait_queue_head_t dec_wait_queue;
static spinlock_t jpeg_dec_lock;
static int dec_status;
static int dec_ready;

#endif

#ifdef JPEG_PM_DOMAIN_ENABLE
struct platform_device *pjdec_dev;
struct platform_device *pjenc_dev;
#endif

/* encoder */
static wait_queue_head_t enc_wait_queue;
static spinlock_t jpeg_enc_lock;
static int enc_status;
static int enc_ready;
static DEFINE_MUTEX(jpeg_enc_power_lock);
static DEFINE_MUTEX(DriverOpenCountLock);
static int Driver_Open_Count;

/* Support QoS */
struct pm_qos_request jpgenc_qos_request;

/* ========================================== */
/* CMDQ */

/* static cmdqRecStruct jpegCMDQ_handle; */
#ifdef JPEG_DEC_DRIVER
/* static cmdqRecHandle jpegCMDQ_handle; */
#endif

/* -------------------------------------------------------------------------- */
/* JPEG Common Function */
/* -------------------------------------------------------------------------- */

#ifdef FPGA_VERSION

void jpeg_drv_dec_power_on(void)
{
	JPEG_MSG("JPEG Decoder Power On\n");
}

void jpeg_drv_dec_power_off(void)
{
	JPEG_MSG("JPEG Decoder Power Off\n");
}

void jpeg_drv_enc_power_on(void)
{
#ifdef FPGA_VERSION
	IMG_REG_WRITE((0), JPEG_EARLY_MM_BASE);
	JPEG_MSG("JPEG Encoder RESET_MM_BASE!!\n");
#endif
	JPEG_MSG("JPEG Encoder Power On\n");
}

void jpeg_drv_enc_power_off(void)
{
	JPEG_MSG("JPEG Encoder Power Off\n");
}

#else

static irqreturn_t jpeg_drv_enc_isr(int irq, void *dev_id)
{
	/* JPEG_MSG("JPEG Encoder Interrupt\n"); */
	if (enc_status == 0) {
		JPEG_ERR("interrupt without power on");
		return IRQ_HANDLED;
	}

	if (irq == gJpegqDev.encIrqId) {
		if (jpeg_isr_enc_lisr() == 0)
			wake_up_interruptible(&enc_wait_queue);
	}

	return IRQ_HANDLED;
}

#ifdef JPEG_DEC_DRIVER
static irqreturn_t jpeg_drv_dec_isr(int irq, void *dev_id)
{
	/* JPEG_MSG("JPEG Decoder Interrupt\n"); */

	if (irq == gJpegqDev.decIrqId) {
		/* mt65xx_irq_mask(MT6575_JPEG_CODEC_IRQ_ID); */

		if (jpeg_isr_dec_lisr() == 0)
			wake_up_interruptible(&dec_wait_queue);
	}

	return IRQ_HANDLED;
}

void jpeg_drv_dec_power_on(void)
{
#ifdef CONFIG_MTK_CLKMGR
	enable_clock(MT_CG_DISP0_SMI_COMMON, "JPEG");
	enable_clock(MT_CG_VENC_LARB, "JPEG");
	enable_clock(MT_CG_VENC_JPGDEC, "JPEG");
#else
	#ifdef JPEG_PM_DOMAIN_ENABLE
		mtk_smi_larb_clock_on(3, true);
		if (clk_prepare_enable(gJpegClk.clk_venc_jpgDec))
			JPEG_ERR("enable clk_venc_jpgDec fail!");
	#else
		#ifdef CONFIG_MTK_SMI_EXT
			smi_bus_prepare_enable(SMI_LARB7_REG_INDX, "JPEG", true);
			if (clk_prepare_enable(gJpegClk.clk_venc_jpgDec))
				JPEG_ERR("enable clk_venc_jpgDec fail!");
		#else
			if (clk_prepare_enable(gJpegClk.clk_scp_sys_mm0))
			JPEG_ERR("enable clk_scp_sys_mm0 fail!");

			if (clk_prepare_enable(gJpegClk.clk_smi_common))
				JPEG_ERR("enable clk_smi_common fail!");

			if (clk_prepare_enable(gJpegClk.clk_scp_sys_ven))
				JPEG_ERR("enable clk_scp_sys_ven fail!");

			if (clk_prepare_enable(gJpegClk.clk_venc_jpgDec))
				JPEG_ERR("enable clk_venc_jpgDec fail!");

			if (clk_prepare_enable(gJpegClk.clk_venc_larb))
				JPEG_ERR("enable clk_venc_larb fail!");
		#endif
	#endif
#endif
}

void jpeg_drv_dec_power_off(void)
{
#ifdef CONFIG_MTK_CLKMGR
	disable_clock(MT_CG_VENC_JPGDEC, "JPEG");
	disable_clock(MT_CG_VENC_LARB, "JPEG");
	disable_clock(MT_CG_DISP0_SMI_COMMON, "JPEG");
#else
	#ifdef JPEG_PM_DOMAIN_ENABLE
		clk_disable_unprepare(gJpegClk.clk_venc_jpgDec);
		mtk_smi_larb_clock_off(3, true);
	#else
		#ifdef CONFIG_MTK_SMI_EXT
			clk_disable_unprepare(gJpegClk.clk_venc_jpgDec);
			smi_bus_disable_unprepare(SMI_LARB7_REG_INDX, "JPEG", true);
		#else
			clk_disable_unprepare(gJpegClk.clk_venc_larb);
			clk_disable_unprepare(gJpegClk.clk_venc_jpgDec);
			clk_disable_unprepare(gJpegClk.clk_scp_sys_ven);
			clk_disable_unprepare(gJpegClk.clk_smi_common);
			clk_disable_unprepare(gJpegClk.clk_scp_sys_mm0);
		#endif
	#endif
#endif
}
#endif

void jpeg_drv_enc_power_on(void)
{
#ifdef CONFIG_MTK_CLKMGR
	/* REG_JPEG_MM_REG_MASK  = 0; */
	enable_clock(MT_CG_DISP0_SMI_COMMON, "JPEG");
	#ifdef CONFIG_ARCH_MT6735M
		enable_clock(MT_CG_IMAGE_JPGENC, "JPEG");
	#else
		enable_clock(MT_CG_VENC_LARB, "JPEG");
		enable_clock(MT_CG_VENC_JPGENC, "JPEG");
#endif
#else
	#ifdef JPEG_PM_DOMAIN_ENABLE
		mtk_smi_larb_clock_on(3, true);
		if (clk_prepare_enable(gJpegClk.clk_venc_jpgEnc))
			JPEG_ERR("enable clk_venc_jpgEnc fail!");
	#else
		#ifdef CONFIG_MTK_SMI_EXT
			#if defined(PLATFORM_MT6779)
			smi_bus_prepare_enable(SMI_LARB3_REG_INDX,
				"JPEG", true);

			if (clk_prepare_enable(gJpegClk.clk_venc_jpgEnc))
				JPEG_ERR("enable clk_venc_jpgDec fail!");

			#elif defined(PLATFORM_MT6757)


			if (clk_prepare_enable(gJpegClk.clk_scp_sys_mm0))
				JPEG_ERR("enable clk_scp_sys_mm0 fail!");


			if (clk_prepare_enable(gJpegClk.clk_smi_common))
				JPEG_ERR("enable clk_smi_common fail!");


			if (clk_prepare_enable(gJpegClk.clk_scp_sys_ven))
				JPEG_ERR("enable clk_scp_sys_ven clk fail!");


			if (clk_prepare_enable(gJpegClk.clk_venc_jpgEnc))
				JPEG_ERR("enable clk_venc_jpgEnc fail!");


			if (clk_prepare_enable(gJpegClk.clk_venc_larb))
				JPEG_ERR("enable clk_venc_larb fail!");

			#else
			smi_bus_prepare_enable(SMI_LARB1_REG_INDX,
				"JPEG", true);

			if (clk_prepare_enable(gJpegClk.clk_venc_jpgEnc))
				JPEG_ERR("enable clk_venc_jpgDec fail!");
			#endif

		#else
			if (clk_prepare_enable(gJpegClk.clk_scp_sys_mm0))
				JPEG_ERR("enable clk_scp_sys_mm0 fail!");

			if (clk_prepare_enable(gJpegClk.clk_smi_common))
				JPEG_ERR("enable clk_smi_common fail!");

			if (clk_prepare_enable(gJpegClk.clk_scp_sys_ven))
				JPEG_ERR("enable clk_scp_sys_ven clk fail!");

			if (clk_prepare_enable(gJpegClk.clk_venc_jpgEnc))
				JPEG_ERR("enable clk_venc_jpgEnc fail!");

			#ifndef CONFIG_ARCH_MT6735M
				if (clk_prepare_enable(gJpegClk.clk_venc_larb))
					JPEG_ERR("enable clk_venc_larb fail!");
			#endif
		#endif
	#endif
#endif
}

void jpeg_drv_enc_power_off(void)
{
#ifdef CONFIG_MTK_CLKMGR
	#ifdef CONFIG_ARCH_MT6735M
		disable_clock(MT_CG_IMAGE_JPGENC, "JPEG");
	#else
		disable_clock(MT_CG_VENC_JPGENC, "JPEG");
		disable_clock(MT_CG_VENC_LARB, "JPEG");
	#endif
	disable_clock(MT_CG_DISP0_SMI_COMMON, "JPEG");
#else
	#ifdef JPEG_PM_DOMAIN_ENABLE
		clk_disable_unprepare(gJpegClk.clk_venc_jpgEnc);
		mtk_smi_larb_clock_off(3, true);
	#else
		#ifdef CONFIG_MTK_SMI_EXT

			#if defined(PLATFORM_MT6779)
			clk_disable_unprepare(gJpegClk.clk_venc_jpgEnc);
			smi_bus_disable_unprepare(SMI_LARB3_REG_INDX,
				"JPEG", true);
			#elif defined(PLATFORM_MT6757)
			clk_disable_unprepare(gJpegClk.clk_venc_larb);
			clk_disable_unprepare(gJpegClk.clk_venc_jpgEnc);
			clk_disable_unprepare(gJpegClk.clk_scp_sys_ven);
			clk_disable_unprepare(gJpegClk.clk_smi_common);
			clk_disable_unprepare(gJpegClk.clk_scp_sys_mm0);

			#else
			clk_disable_unprepare(gJpegClk.clk_venc_jpgEnc);
			smi_bus_disable_unprepare(SMI_LARB1_REG_INDX,
				"JPEG", true);
			#endif
		#else
			#ifndef CONFIG_ARCH_MT6735M
				clk_disable_unprepare(gJpegClk.clk_venc_larb);
			#endif
			clk_disable_unprepare(gJpegClk.clk_venc_jpgEnc);
			clk_disable_unprepare(gJpegClk.clk_scp_sys_ven);
			clk_disable_unprepare(gJpegClk.clk_smi_common);
			clk_disable_unprepare(gJpegClk.clk_scp_sys_mm0);
		#endif
	#endif
#endif
}
#endif


#ifdef JPEG_DEC_DRIVER
static int jpeg_drv_dec_init(void)
{
	int retValue;

	spin_lock(&jpeg_dec_lock);
	if (dec_status != 0) {
		JPEG_WRN("jpeg_drv_dec_init HW is busy\n");
		retValue = -EBUSY;
	} else {
		dec_status = 1;
		dec_ready = 0;
		retValue = 0;
	}
	spin_unlock(&jpeg_dec_lock);

	if (retValue == 0) {
		jpeg_drv_dec_power_on();
		jpeg_drv_dec_verify_state_and_reset();
	}

	return retValue;
}

static void jpeg_drv_dec_deinit(void)
{
	if (dec_status != 0) {

		spin_lock(&jpeg_dec_lock);
		dec_status = 0;
		dec_ready = 0;
		spin_unlock(&jpeg_dec_lock);

		jpeg_drv_dec_reset();

		jpeg_drv_dec_power_off();
	}
}
#endif

static int jpeg_drv_enc_init(void)
{
	int retValue;

	spin_lock(&jpeg_enc_lock);
	if (enc_status != 0) {
		JPEG_WRN("jpeg_drv_enc_init HW is busy\n");
		retValue = -EBUSY;
	} else {
		enc_status = 1;
		enc_ready = 0;
		retValue = 0;
	}
	spin_unlock(&jpeg_enc_lock);

	mutex_lock(&jpeg_enc_power_lock);
	if (retValue == 0) {
		jpeg_drv_enc_power_on();
		jpeg_drv_enc_verify_state_and_reset();
	}
	mutex_unlock(&jpeg_enc_power_lock);

	return retValue;
}

static void jpeg_drv_enc_deinit(void)
{
	if (enc_status != 0) {
		spin_lock(&jpeg_enc_lock);
		enc_status = 0;
		enc_ready = 0;
		spin_unlock(&jpeg_enc_lock);

		mutex_lock(&jpeg_enc_power_lock);
		jpeg_drv_enc_reset();
		jpeg_drv_enc_power_off();
		mutex_unlock(&jpeg_enc_power_lock);
	}
}


/* -------------------------------------------------------------------------- */
/* JPEG REG DUMP FUNCTION */
/* -------------------------------------------------------------------------- */
void jpeg_reg_dump(void)
{
	unsigned int reg_value = 0;
	unsigned int index = 0;

	JPEG_WRN("JPEG REG:\n ********************\n");
	for (index = 0; index < 0x168; index += 4) {
		/* reg_value = ioread32(JPEG_DEC_BASE + index); */
		IMG_REG_READ(reg_value, JPEG_DEC_BASE + index);
		JPEG_WRN("+0x%x 0x%x\n", index, reg_value);
	}
}

/* -------------------------------------------------------------------------- */
/* JPEG DECODER IOCTRL FUNCTION */
/* -------------------------------------------------------------------------- */

#ifdef JPEG_DEC_DRIVER
static int jpeg_dec_ioctl(unsigned int cmd, unsigned long arg,
									struct file *file)
{
	unsigned int *pStatus;
	unsigned int decResult;
	long timeout_jiff;
	JPEG_DEC_DRV_IN dec_params;
	JPEG_DEC_CONFIG_ROW dec_row_params;
	/* JPEG_DEC_CONFIG_CMDQ cfg_cmdq_params; */

	unsigned int irq_st = 0;
	/* unsigned int timeout = 0x1FFFFF; */

	JPEG_DEC_DRV_OUT outParams;

	pStatus = (unsigned int *)file->private_data;

	if (pStatus == NULL) {
		JPEG_MSG
		    ("[JPEGDRV]Decoder: Private data is null in flush operation\n");
		return -EFAULT;
	}
	switch (cmd) {
		/* initial and reset JPEG encoder */
	case JPEG_DEC_IOCTL_INIT:	/* OT:OK */
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Init!!\n");*/
		if (jpeg_drv_dec_init() == 0)
			*pStatus = JPEG_DEC_PROCESS;
		break;

	case JPEG_DEC_IOCTL_RESET:	/* OT:OK */
		JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Reset!!\n");
		jpeg_drv_dec_reset();
		break;

	case JPEG_DEC_IOCTL_CONFIG:
		/* JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Configration!!\n"); */
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_MSG
			    ("[JPEGDRV]Permission Denied! This process can not access decoder\n");
			return -EFAULT;
		}
		if (dec_status == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&dec_params, (void *)arg, sizeof(JPEG_DEC_DRV_IN))) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder : Copy from user error\n");
			return -EFAULT;
		}
		/* _jpeg_dec_dump_reg_en = dec_params.regDecDumpEn; */
		if (dec_params.decodeMode == JPEG_DEC_MODE_MCU_ROW)
			_jpeg_dec_mode = 1;
		else
			_jpeg_dec_mode = 0;

		if (jpeg_drv_dec_set_config_data(&dec_params) == 0)
			return -EFAULT;

		spin_lock(&jpeg_dec_lock);
		dec_ready = 1;
		spin_unlock(&jpeg_dec_lock);

		break;

	case JPEG_DEC_IOCTL_FLUSH_CMDQ:

	#if 0 /* currently no use */
		JPEG_MSG("[JPEGDRV]enter JPEG BUILD CMDQ !!\n");
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_MSG
			    ("[JPEGDRV]Permission Denied! This process can not access decoder\n");
			return -EFAULT;
		}
		if (dec_status == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&cfg_cmdq_params, (void *)arg, sizeof(JPEG_DEC_CONFIG_CMDQ))) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder : Copy from user error\n");
			return -EFAULT;
		}
		JPEG_MSG("[JPEGDRV]JPEG CREATE CMDQ !!\n");

		cmdqRecCreate(CMDQ_SCENARIO_JPEG_DEC, &jpegCMDQ_handle);

		cmdqRecWait(jpegCMDQ_handle, CMDQ_EVENT_JPEG_DEC_EOF);

		for (i = 0; i < cfg_cmdq_params.goNum; i++) {
			JPEG_MSG("[JPEGDRV]JPEG gen CMDQ %x %x %x %x  !!\n",
				 cfg_cmdq_params.pauseMCUidx[i], cfg_cmdq_params.decRowBuf0[i],
				 cfg_cmdq_params.decRowBuf1[i], cfg_cmdq_params.decRowBuf2[i]);
			cmdqRecWrite(jpegCMDQ_handle,
				     0x18004170 /*REG_ADDR_JPGDEC_PAUSE_MCU_NUM */,
				     cfg_cmdq_params.pauseMCUidx[i] - 1, 0xFFFFFFFF);
			cmdqRecWrite(jpegCMDQ_handle, 0x18004140 /*REG_ADDR_JPGDEC_DEST_ADDR0_Y */,
				     cfg_cmdq_params.decRowBuf0[i], 0xFFFFFFFF);
			cmdqRecWrite(jpegCMDQ_handle, 0x18004144 /*REG_ADDR_JPGDEC_DEST_ADDR0_U */,
				     cfg_cmdq_params.decRowBuf1[i], 0xFFFFFFFF);
			cmdqRecWrite(jpegCMDQ_handle, 0x18004148 /*REG_ADDR_JPGDEC_DEST_ADDR0_V */,
				     cfg_cmdq_params.decRowBuf2[i], 0xFFFFFFFF);

			JPEG_MSG("[JPEGDRV]JPEG gen CMDQ go!!\n");
			/* trigger */
			cmdqRecWrite(jpegCMDQ_handle,
				     0x18004274 /*REG_ADDR_JPGDEC_INTERRUPT_STATUS */,
				     BIT_INQST_MASK_PAUSE, 0xFFFFFFFF);

			JPEG_MSG("[JPEGDRV]JPEG gen CMDQ wait!!\n");
			/* wait frame done */
			cmdqRecWait(jpegCMDQ_handle, CMDQ_EVENT_JPEG_DEC_EOF);
			/* cmdqRecPoll(jpegCMDQ_handle, 0x18004274 */
			/*    , BIT_INQST_MASK_PAUSE, 0x0010); */
		}

		JPEG_MSG("[JPEGDRV]JPEG flush CMDQ start!!\n");
		cmdqRecFlush(jpegCMDQ_handle);
		JPEG_MSG("[JPEGDRV]JPEG flush CMDQ end!!\n");

		cmdqRecDestroy(jpegCMDQ_handle);
		JPEG_MSG("[JPEGDRV]JPEG destroy CMDQ end!!\n");
	#endif
		break;

	case JPEG_DEC_IOCTL_RESUME:
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_MSG
			    ("[JPEGDRV]Permission Denied! This process can not access decoder\n");
			return -EFAULT;
		}
		if (dec_status == 0 || dec_ready == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&dec_row_params, (void *)arg, sizeof(JPEG_DEC_CONFIG_ROW))) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder : Copy from user error\n");
			return -EFAULT;
		}

		JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Resume, [%d] %x %x %x !!\n",
			 dec_row_params.pauseMCU - 1, dec_row_params.decRowBuf[0],
			 dec_row_params.decRowBuf[1], dec_row_params.decRowBuf[2]);

		if (!jpeg_drv_dec_set_dst_bank0(dec_row_params.decRowBuf[0], dec_row_params.decRowBuf[1],
					   dec_row_params.decRowBuf[2])) {
			return -EFAULT;
		}

		if (!jpeg_drv_dec_set_pause_mcu_idx(dec_row_params.pauseMCU - 1))
			return -EFAULT;


		/* lock CPU to ensure irq is enabled after trigger HW */
		spin_lock(&jpeg_dec_lock);
		jpeg_drv_dec_resume(BIT_INQST_MASK_PAUSE);
		spin_unlock(&jpeg_dec_lock);
		break;

	case JPEG_DEC_IOCTL_START:	/* OT:OK */
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_WRN("Permission Denied! This process can not access decoder");
			return -EFAULT;
		}
		if (dec_status == 0 || dec_ready == 0) {
			JPEG_WRN("Decoder status is available, HOW COULD THIS HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Start!!\n");*/

		jpeg_drv_dec_start();
		break;

	case JPEG_DEC_IOCTL_WAIT:
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_WRN("Permission Denied! This process can not access decoder");
			return -EFAULT;
		}
		if (dec_status == 0 || dec_ready == 0) {
			JPEG_WRN("Decoder status is available, HOW COULD THIS HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&outParams, (void *)arg, sizeof(JPEG_DEC_DRV_OUT))) {
			JPEG_WRN("JPEG Decoder : Copy from user error\n");
			return -EFAULT;
		}

		/* set timeout */
		timeout_jiff = outParams.timeout * HZ / 1000;
		/* JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Wait Resume Time Jiffies : %ld\n", timeout_jiff); */
	#ifdef FPGA_VERSION
		JPEG_MSG("[JPEGDRV]Polling JPEG Status");

		do {
			_jpeg_dec_int_status = REG_JPGDEC_INTERRUPT_STATUS;
		} while (_jpeg_dec_int_status == 0);
	#else
		if (jpeg_isr_dec_lisr() < 0) {
			long ret = 0;

			do {
				/* JPEG_MSG("wait JPEG irq\n"); */
				ret = wait_event_interruptible_timeout(dec_wait_queue, _jpeg_dec_int_status,
							 timeout_jiff);
				if (ret == 0)
					JPEG_MSG("[JPEGDRV]JPEG Decoder Wait timeout !!\n");
			} while (ret < 0);
		} else
			JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Enter IRQ Wait Already Done!!\n");
	#endif

		decResult = jpeg_drv_dec_get_result();

		if (decResult >= 2) {
			JPEG_MSG("[JPEGDRV]Decode Result : %d, status %x!\n", decResult,
				 _jpeg_dec_int_status);

			jpeg_drv_dec_dump_key_reg();

#ifndef JPEG_PM_DOMAIN_ENABLE
#ifdef CONFIG_MTK_SMI_EXT
		/* need to dump smi for the case that no irq coming from HW */
		if (decResult == 5)
			smi_debug_bus_hang_detect(SMI_PARAM_BUS_OPTIMIZATION, 1, 0, 1);
#endif
#endif
			jpeg_drv_dec_warm_reset();
		}
		irq_st = _jpeg_dec_int_status;
		decResult = decResult | (irq_st << 8);
		_jpeg_dec_int_status = 0;
		if (copy_to_user(outParams.result, &decResult, sizeof(unsigned int))) {
			JPEG_WRN("JPEG Decoder : Copy to user error (result)\n");
			return -EFAULT;
		}

		break;

	case JPEG_DEC_IOCTL_BREAK:
		if (jpeg_drv_dec_break() < 0)
			return -EFAULT;
		break;

	case JPEG_DEC_IOCTL_DUMP_REG:
		JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder DUMP REGISTER !!\n");
		jpeg_drv_dec_dump_reg();
		break;

	case JPEG_DEC_IOCTL_DEINIT:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Deinit !!\n");*/
		/* copy input parameters */
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_ERR("Permission Denied! This process can not access encoder");
			return -EFAULT;
		}

		if (dec_status == 0) {
			JPEG_ERR("Encoder status is available, HOW COULD THIS HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}
		jpeg_drv_dec_deinit();
		*pStatus = 0;
		break;
	default:
		JPEG_ERR("JPEG DEC IOCTL NO THIS COMMAND\n");
		break;
	}
	return 0;
}
#endif /* JPEG_DEC_DRIVER */

static int jpeg_enc_ioctl(unsigned int cmd, unsigned long arg, struct file *file)
{
	int retValue;
	/* unsigned int decResult; */

	long timeout_jiff;
	unsigned int file_size, enc_result_code;

	/* unsigned int _jpeg_enc_int_status; */
	unsigned int jpeg_enc_wait_timeout = 0;
	unsigned int cycle_count;
	unsigned int ret;
	/* No spec, considering [picture size] x [target fps] */
	unsigned int cshot_spec = 0xffffffff;
	/* limiting FPS, Upper Bound FPS = 20 */
	unsigned int target_fps = 20;

	unsigned int *pStatus;

	/* Support QoS */
	unsigned int emi_bw = 0;
	unsigned int picSize = 0;
	unsigned int picCost = 0;

	/* JpegDrvEncParam cfgEnc; */
	JPEG_ENC_DRV_IN cfgEnc;

	JPEG_ENC_DRV_OUT enc_result;

	 pStatus = (unsigned int *)file->private_data;

	if (pStatus == NULL) {
		JPEG_WRN("Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
		return -EFAULT;
	}

	switch (cmd) {
	case JPEG_ENC_IOCTL_RW_REG:
		/* jpeg_drv_enc_rw_reg(); */
		break;

		/* initial and reset JPEG encoder */
	case JPEG_ENC_IOCTL_INIT:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Init!!\n");*/

		retValue = jpeg_drv_enc_init();

		if (retValue == 0)
			*pStatus = JPEG_ENC_PROCESS;

		return retValue;

		break;

	case JPEG_ENC_IOCTL_WARM_RESET:
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("Permission Denied!");
			return -EFAULT;
		}
		if (enc_status == 0) {
			JPEG_WRN("Encoder status is available");
			*pStatus = 0;
			return -EFAULT;
		}

		JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Warm Reset\n");
		enc_result_code = jpeg_drv_enc_warm_reset();
		if (enc_result_code == 0)
			return -EFAULT;
		break;

		/* configure the register */
	case JPEG_ENC_IOCTL_CONFIG:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Configure Hardware\n");*/
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("Permission Denied! This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0) {
			JPEG_WRN("Encoder status is available, HOW COULD THIS HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}

		/* copy input parameters */
		if (copy_from_user(&cfgEnc, (void *)arg, sizeof(JPEG_ENC_DRV_IN))) {
			JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy from user error\n");
			return -EFAULT;
		}

		/* 0. reset */
		jpeg_drv_enc_reset();

		/* 1. set src config */
		/* memset(&src_cfg, 0, sizeof(JpegDrvEncSrcCfg)); */

		/* src_cfg.luma_addr = cfgEnc.srcBufferAddr; */
		/* if (cfgEnc.encFormat == NV12 || cfgEnc.encFormat == NV21) */
		/* { */
		/* unsigned int srcChromaAddr = cfgEnc.srcChromaAddr; */
		/* srcChromaAddr = TO_CEIL(srcChromaAddr, 128);    //((srcChromaAddr+127)&~127); */
		/* src_cfg.chroma_addr = srcChromaAddr; */
		/* } */
		/*  */
		/* src_cfg.width = cfgEnc.encWidth; */
		/* src_cfg.height = cfgEnc.encHeight; */
		/* src_cfg.yuv_format = cfgEnc.encFormat; */

		/* Support QoS */
		picSize = (cfgEnc.encWidth * cfgEnc.encHeight) / 1000000;
		/* BW = encode width x height x bpp x 1.6 */
		/* Assume compress ratio is 0.6 */
		if (cfgEnc.encFormat == 0x0 || cfgEnc.encFormat == 0x1)
			picCost = ((picSize * 2) * 8/5) + 1;
		else
			picCost = ((picSize * 3/2) * 8/5) + 1;

#ifdef QOS_MT6765_SUPPORT
		/* on mt6765, 16MP = 14.5 FPS */
		cshot_spec = 232;
#endif
#ifdef QOS_MT6761_SUPPORT
		/* on mt6761, lpddr4: 26MP = 5 FPS , lpddr3: 26MP = 1 FPS*/
		cshot_spec = 26;
#endif

		if ((picCost * target_fps) < cshot_spec) {
			emi_bw = picCost * target_fps;
		} else {
			emi_bw = cshot_spec / picCost;
			emi_bw = (emi_bw + 1) * picCost;
		}

		/* QoS requires Occupied BW */
		/* Data BW x 1.33 */
		emi_bw = emi_bw * 4/3;

		/* update BW request before trigger HW */
		pm_qos_update_request(&jpgenc_qos_request, emi_bw);

		/* 1. set src config */
		JPEG_MSG("[JPEGDRV]SRC_IMG:%x %x, DU:%x, fmt:%x\n",
			 cfgEnc.encWidth, cfgEnc.encHeight,
			 cfgEnc.totalEncDU, cfgEnc.encFormat);

		JPEG_MSG("[JPEGDRV]picSize:%d, BW:%d\n", picSize, emi_bw);

		ret =
		    jpeg_drv_enc_set_src_image(cfgEnc.encWidth, cfgEnc.encHeight, cfgEnc.encFormat,
					       cfgEnc.totalEncDU);
		if (ret == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Encoder set srouce image failed\n");
			return -EFAULT;
		}

		/* 2. set src buffer info */
		JPEG_MSG("[JPEGDRV]SRC_BUF: addr %x, %x, stride %x, %x!!\n", cfgEnc.srcBufferAddr,
			 cfgEnc.srcChromaAddr, cfgEnc.imgStride, cfgEnc.memStride);

		ret =
		    jpeg_drv_enc_set_src_buf(cfgEnc.encFormat, cfgEnc.imgStride, cfgEnc.memStride,
					     cfgEnc.srcBufferAddr, cfgEnc.srcChromaAddr);
		if (ret == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Encoder set srouce buffer failed\n");
			return -EFAULT;
		}

		/* if (0 == jpeg_drv_enc_src_cfg(src_cfg)) */
		/* { */
		/* JPEG_MSG("JPEG Encoder src cfg failed\n"); */
		/* return -EFAULT; */
		/* } */

		/* 3. set dst buffer info */
		JPEG_MSG("[JPEGDRV]DST_BUF: addr:%x, size:%x, ofs:%x, mask:%x!!\n",
			 cfgEnc.dstBufferAddr, cfgEnc.dstBufferSize, cfgEnc.dstBufAddrOffset,
			 cfgEnc.dstBufAddrOffsetMask);

		ret =
		    jpeg_drv_enc_set_dst_buff(cfgEnc.dstBufferAddr, cfgEnc.dstBufferSize,
					      cfgEnc.dstBufAddrOffset, cfgEnc.dstBufAddrOffsetMask);
		if (ret == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Encoder set dst buffer failed\n");
			return -EFAULT;
		}

		/* 4 .set ctrl config */
		JPEG_MSG("[JPEGDRV]ENC_CFG: exif:%d, q:%d, DRI:%d !!\n", cfgEnc.enableEXIF,
			 cfgEnc.encQuality, cfgEnc.restartInterval);

		jpeg_drv_enc_ctrl_cfg(cfgEnc.enableEXIF, cfgEnc.encQuality, cfgEnc.restartInterval);

		spin_lock(&jpeg_enc_lock);
		enc_ready = 1;
		spin_unlock(&jpeg_enc_lock);
		break;

	case JPEG_ENC_IOCTL_START:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Start!!\n");*/
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("Permission Denied! This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0 || enc_ready == 0) {
			JPEG_WRN("Encoder status is unavailable, HOW COULD THIS HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}
		jpeg_drv_enc_start();
		break;

	case JPEG_ENC_IOCTL_WAIT:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Wait!!\n");*/
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("Permission Denied! This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0 || enc_ready == 0) {
			JPEG_WRN("Encoder status is unavailable, HOW COULD THIS HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&enc_result, (void *)arg, sizeof(JPEG_ENC_DRV_OUT))) {
			JPEG_WRN("JPEG Encoder : Copy from user error\n");
			return -EFAULT;
		}

		/* TODO:    ENC_DONE in REG_JPEG_ENC_INTERRUPT_STATUS need to set to 0 after read. */
		jpeg_enc_wait_timeout = 0xFFFFFF;

	#ifdef FPGA_VERSION
		do {
			_jpeg_enc_int_status = REG_JPEG_ENC_INTERRUPT_STATUS;
			jpeg_enc_wait_timeout--;
		} while (_jpeg_enc_int_status == 0 && jpeg_enc_wait_timeout > 0);

		if (jpeg_enc_wait_timeout == 0)
			JPEG_MSG("JPEG Encoder timeout\n");

		ret = jpeg_drv_enc_get_result(&file_size);

		JPEG_MSG("Result : %d, Size : %u, address : 0x%x\n", ret, file_size,
			 ioread32(JPEG_ENC_BASE + 0x120));

		if (_jpeg_enc_int_status != 1)
			jpeg_drv_enc_dump_reg();
	#else
		/* set timeout */
		timeout_jiff = enc_result.timeout * HZ / 1000;
		JPEG_MSG("[JPEGDRV]JPEG Encoder Time Jiffies : %ld\n", timeout_jiff);

		if (jpeg_isr_enc_lisr() < 0) {
			do {
				ret = wait_event_interruptible_timeout(enc_wait_queue, _jpeg_enc_int_status,
								 timeout_jiff);
				if (ret > 0)
					JPEG_MSG("[JPEGDRV]JPEG Encoder Wait done !!\n");
				else if (ret == 0)
					JPEG_MSG("[JPEGDRV]JPEG Encoder Wait timeout !!\n");
			} while (ret < 0);
		} else
			JPEG_MSG("[JPEGDRV]JPEG Encoder already done !!\n");

		/* Support QoS: remove BW request after HW done */
		pm_qos_update_request(&jpgenc_qos_request, 0);

		ret = jpeg_drv_enc_get_result(&file_size);

		JPEG_MSG("[JPEGDRV]Result : %d, Size : %u!!\n", ret, file_size);
		if (ret != 0) {
			jpeg_drv_enc_dump_reg();

#ifndef JPEG_PM_DOMAIN_ENABLE
#ifdef CONFIG_MTK_SMI_EXT
		/* need to dump smi for the case that no irq coming from HW */
		if (ret == 3)
			smi_debug_bus_hang_detect(SMI_PARAM_BUS_OPTIMIZATION, 1, 0, 1);
#endif
#endif
			jpeg_drv_enc_warm_reset();

			return -EFAULT;
		}
	#endif
		cycle_count = jpeg_drv_enc_get_cycle_count();

		if (copy_to_user(enc_result.fileSize, &file_size, sizeof(unsigned int))) {
			JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy to user error (file size)\n");
			return -EFAULT;
		}
		if (copy_to_user(enc_result.result, &ret, sizeof(unsigned int))) {
			JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy to user error (status)\n");
			return -EFAULT;
		}
		if (copy_to_user(enc_result.cycleCount, &cycle_count, sizeof(unsigned int))) {
			JPEG_MSG("[JPEGDRV]JPEG Encoder : Copy to user error (cycle)\n");
			return -EFAULT;
		}
		break;

	case JPEG_ENC_IOCTL_DEINIT:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Deinit!!\n");*/
		/* copy input parameters */
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("Permission Denied! This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0) {
			JPEG_WRN("Encoder status is available, HOW COULD THIS HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}
		jpeg_drv_enc_deinit();
		*pStatus = 0;
		break;

	case JPEG_ENC_IOCTL_DUMP_REG:
		jpeg_drv_enc_dump_reg();
		break;

	default:
		JPEG_MSG("[JPEGDRV]JPEG ENC IOCTL NO THIS COMMAND\n");

	}
	return 0;
}


/* -------------------------------------------------------------------------- */
/*  */
/* -------------------------------------------------------------------------- */

#ifdef CONFIG_COMPAT

static int compat_get_jpeg_dec_ioctl_wait_data(compat_JPEG_DEC_DRV_OUT __user *data32,
					       JPEG_DEC_DRV_OUT __user *data) {
	compat_long_t timeout;
	compat_uptr_t result;
	int err;

	err = get_user(timeout, &data32->timeout);
	err |= put_user(timeout, &data->timeout);
	err |= get_user(result, &data32->result);
	err |= put_user(compat_ptr(result), &data->result);
	return err;
}

static int compat_put_jpeg_dec_ioctl_wait_data(compat_JPEG_DEC_DRV_OUT __user *data32,
					       JPEG_DEC_DRV_OUT __user *data) {
	compat_long_t timeout;
	/* compat_uptr_t result; */
	int err;

	err = get_user(timeout, &data->timeout);
	err |= put_user(timeout, &data32->timeout);
	/* err |= get_user(result, &data->result); */
	/* err |= put_user(result, &data32->result); */
	return err;
}

static int compat_get_jpeg_dec_ioctl_chksum_data(compat_JpegDrvDecResult __user *data32,
						 JpegDrvDecResult __user *data) {
	compat_uptr_t pChksum;
	int err;

	err = get_user(pChksum, &data32->pChksum);
	err |= put_user(compat_ptr(pChksum), &data->pChksum);
	return err;
}

static int compat_put_jpeg_dec_ioctl_chksum_data(compat_JpegDrvDecResult __user *data32,
						 JpegDrvDecResult __user *data) {
	/* compat_uptr_t pChksum; */
	/* int err; */

	/* err = get_user(pChksum, &data->pChksum); */
	/* err |= put_user(pChksum, &data32->pChksum); */
	return 0;
}

static int compat_get_jpeg_enc_ioctl_wait_data(compat_JPEG_ENC_DRV_OUT __user *data32,
					       JPEG_ENC_DRV_OUT __user *data) {
	compat_long_t timeout;
	compat_uptr_t fileSize;
	compat_uptr_t result;
	compat_uptr_t cycleCount;
	int err;

	err = get_user(timeout, &data32->timeout);
	err |= put_user(timeout, &data->timeout);
	err |= get_user(fileSize, &data32->fileSize);
	err |= put_user(compat_ptr(fileSize), &data->fileSize);
	err |= get_user(result, &data32->result);
	err |= put_user(compat_ptr(result), &data->result);
	err |= get_user(cycleCount, &data32->cycleCount);
	err |= put_user(compat_ptr(cycleCount), &data->cycleCount);
	return err;
}

static int compat_put_jpeg_enc_ioctl_wait_data(compat_JPEG_ENC_DRV_OUT __user *data32,
					       JPEG_ENC_DRV_OUT __user *data) {
	compat_long_t timeout;
	/* compat_uptr_t fileSize; */
	/* compat_uptr_t result; */
	/* compat_uptr_t cycleCount; */
	int err;

	err = get_user(timeout, &data->timeout);
	err |= put_user(timeout, &data32->timeout);
	/* err |= get_user(fileSize, &data->fileSize); */
	/* err |= put_user(fileSize, &data32->fileSize); */
	/* err |= get_user(result, &data->result); */
	/* err |= put_user(result, &data32->result); */
	/* err |= get_user(cycleCount, &data->cycleCount); */
	/* err |= put_user(cycleCount, &data32->cycleCount); */
	return err;
}

static long compat_jpeg_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_JPEG_DEC_IOCTL_WAIT:
		{
			compat_JPEG_DEC_DRV_OUT __user *data32;
			JPEG_DEC_DRV_OUT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_jpeg_dec_ioctl_wait_data(data32, data);
			if (err)
				return err;
			ret =
				filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_WAIT,
						(unsigned long)data);
			err = compat_put_jpeg_dec_ioctl_wait_data(data32, data);
			return ret ? ret : err;
		} case COMPAT_JPEG_DEC_IOCTL_CHKSUM: {
			compat_JpegDrvDecResult __user *data32;
			JpegDrvDecResult __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_jpeg_dec_ioctl_chksum_data(data32, data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_CHKSUM,
						       (unsigned long)data);
			err = compat_put_jpeg_dec_ioctl_chksum_data(data32, data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_ENC_IOCTL_WAIT:
		{
			compat_JPEG_ENC_DRV_OUT __user *data32;
			JPEG_ENC_DRV_OUT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_jpeg_enc_ioctl_wait_data(data32, data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(filp, JPEG_ENC_IOCTL_WAIT,
						       (unsigned long)data);
			err = compat_put_jpeg_enc_ioctl_wait_data(data32, data);
			return ret ? ret : err;
		}
	case JPEG_DEC_IOCTL_INIT:
	case JPEG_DEC_IOCTL_START:
	case JPEG_DEC_IOCTL_DEINIT:
	case JPEG_DEC_IOCTL_DUMP_REG:
	case JPEG_ENC_IOCTL_INIT:
	case JPEG_ENC_IOCTL_DEINIT:
	case JPEG_ENC_IOCTL_START:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);

	case JPEG_DEC_IOCTL_CONFIG:
	case JPEG_DEC_IOCTL_RESUME:
	case JPEG_DEC_IOCTL_FLUSH_CMDQ:
	case JPEG_ENC_IOCTL_CONFIG:
		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));

	default:
		return -ENOIOCTLCMD;
	}
}
#endif

/* static int jpeg_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) */
static long jpeg_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
#ifdef JPEG_DEC_DRIVER
	case JPEG_DEC_IOCTL_INIT:
	case JPEG_DEC_IOCTL_CONFIG:
	case JPEG_DEC_IOCTL_START:
	case JPEG_DEC_IOCTL_RESUME:
	case JPEG_DEC_IOCTL_WAIT:
	case JPEG_DEC_IOCTL_DEINIT:
	case JPEG_DEC_IOCTL_DUMP_REG:
	case JPEG_DEC_IOCTL_FLUSH_CMDQ:
		return jpeg_dec_ioctl(cmd, arg, file);
#endif
	case JPEG_ENC_IOCTL_INIT:
	case JPEG_ENC_IOCTL_CONFIG:
	case JPEG_ENC_IOCTL_WAIT:
	case JPEG_ENC_IOCTL_DEINIT:
	case JPEG_ENC_IOCTL_START:
		return jpeg_enc_ioctl(cmd, arg, file);
	default:
		break;
	}
	return -EINVAL;
}

static int jpeg_open(struct inode *inode, struct file *file)
{
	unsigned int *pStatus;
	mutex_lock(&DriverOpenCountLock);
	Driver_Open_Count++;
	mutex_unlock(&DriverOpenCountLock);

	/* Allocate and initialize private data */
	 file->private_data = kmalloc(sizeof(unsigned int), GFP_ATOMIC);

	if (file->private_data == NULL) {
		JPEG_WRN("Not enough entry for JPEG open operation\n");
		return -ENOMEM;
	}

	pStatus = (unsigned int *)file->private_data;
	*pStatus = 0;

	return 0;
}

static ssize_t jpeg_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	JPEG_MSG("jpeg driver read\n");
	return 0;
}

static int jpeg_release(struct inode *inode, struct file *file)
{
	mutex_lock(&DriverOpenCountLock);
	Driver_Open_Count--;
	if (Driver_Open_Count == 0) {
		if (enc_status != 0) {
			JPEG_WRN("error handling for encoder");
			jpeg_drv_enc_deinit();
		}

#ifdef JPEG_DEC_DRIVER
		if (dec_status != 0) {
			JPEG_WRN("error handling for decoder");
			jpeg_drv_dec_deinit();
		}
#endif
	}
	mutex_unlock(&DriverOpenCountLock);



	if (file->private_data != NULL) {
		kfree(file->private_data);
		file->private_data = NULL;
	}
	return 0;
}

static int jpeg_flush(struct file *a_pstFile, fl_owner_t a_id)
{
	unsigned int *pStatus;

	 pStatus = (unsigned int *)a_pstFile->private_data;

	if (pStatus == NULL) {
		JPEG_WRN("Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
		return -EFAULT;
	}

	if (*pStatus == JPEG_ENC_PROCESS) {
		if (enc_status != 0) {
			JPEG_WRN("Error! Enable error handling for jpeg encoder");
			jpeg_drv_enc_deinit();
		}
	}
#ifdef JPEG_DEC_DRIVER
	else if (*pStatus == JPEG_DEC_PROCESS) {
		if (dec_status != 0) {
			JPEG_WRN("Error! Enable error handling for jpeg decoder");
			jpeg_drv_dec_deinit();
		}
	}
#endif

	return 0;
}

/* Kernel interface */
static struct file_operations const jpeg_fops = {
	.owner = THIS_MODULE,
	/* .ioctl          = jpeg_ioctl, */
	.unlocked_ioctl = jpeg_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_jpeg_ioctl,
#endif
	.open = jpeg_open,
	.release = jpeg_release,
	.flush = jpeg_flush,
	.read = jpeg_read,
};


const long jpeg_dev_get_encoder_base_VA(void)
{
	return gJpegqDev.encRegBaseVA;
}


const long jpeg_dev_get_decoder_base_VA(void)
{
	return gJpegqDev.decRegBaseVA;
}


static int jpeg_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	int new_count;
	struct JpegDeviceStruct *jpegDev;
	struct device_node *node = NULL;
	void *tmpPtr;

	new_count = nrJpegDevs + 1;
	tmpPtr = krealloc(gJpegqDevs, sizeof(struct JpegDeviceStruct) * new_count, GFP_KERNEL);
	if (!tmpPtr) {
		JPEG_ERR("Unable to allocate JpegDeviceStruct\n");
		return -ENOMEM;
	}
	gJpegqDevs = (JpegDeviceStruct *)tmpPtr;

	jpegDev = &(gJpegqDevs[nrJpegDevs]);
	jpegDev->pDev = &pdev->dev;
	memset(&gJpegqDev, 0x0, sizeof(JpegDeviceStruct));

	node = pdev->dev.of_node;
	jpegDev->encRegBaseVA = (unsigned long)of_iomap(node, 0);
	jpegDev->encIrqId = irq_of_parse_and_map(node, 0);
	#ifdef CONFIG_MTK_CLKMGR
	#else
		#ifdef JPEG_PM_DOMAIN_ENABLE
			pjenc_dev = pdev;
		#else
			/* venc-mtcmos lead to disp power scpsys SCP_SYS_DISP */
			gJpegClk.clk_scp_sys_mm0 =
				of_clk_get_by_name(node, "MT_CG_SCP_SYS_MM0");
			if (IS_ERR(gJpegClk.clk_scp_sys_mm0))
				JPEG_ERR("get MT_CG_SCP_SYS_MM0 clk error!");
			/* venc-mtcmos lead to venc power scpsys SCP_SYS_VEN */
			gJpegClk.clk_scp_sys_ven =
				of_clk_get_by_name(node, "MT_CG_SCP_SYS_VEN");
			if (IS_ERR(gJpegClk.clk_scp_sys_ven))
				JPEG_ERR("get MT_CG_SCP_SYS_VEN clk error!");

			gJpegClk.clk_smi_common =
				of_clk_get_by_name(node, "MT_CG_SMI_COMMON");
			if (IS_ERR(gJpegClk.clk_smi_common))
				JPEG_ERR("get MT_CG_SMI_COMMON clk error!");
			gJpegClk.clk_venc_larb =
				of_clk_get_by_name(node, "MT_CG_VENC_LARB");
			if (IS_ERR(gJpegClk.clk_venc_larb))
				JPEG_ERR("get MT_CG_VENC_LARB clk error!");
		#endif
		gJpegClk.clk_venc_jpgEnc = of_clk_get_by_name(node, "MT_CG_VENC_JPGENC");
		if (IS_ERR(gJpegClk.clk_venc_jpgEnc))
			JPEG_ERR("get MT_CG_VENC_JPGENC clk error!");
	#endif
	#ifdef JPEG_DEC_DRIVER
		jpegDev->decRegBaseVA = (unsigned long)of_iomap(node, 1);
		jpegDev->decIrqId = irq_of_parse_and_map(node, 1);
	    #ifdef CONFIG_MTK_CLKMGR
	    #else
			gJpegClk.clk_venc_jpgDec = of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC");
			if (IS_ERR(gJpegClk.clk_venc_jpgDec))
				JPEG_ERR("get MT_CG_VENC_JPGDEC clk error!");
		#endif
	#endif
	gJpegqDev = *jpegDev;

	/* Support QoS */
	pm_qos_add_request(&jpgenc_qos_request,
		PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);

#else
	gJpegqDev.encRegBaseVA = (0L | 0xF7003000);
	gJpegqDev.decRegBaseVA = (0L | 0xF7004000);
	gJpegqDev.encIrqId = JPGENC_IRQ_BIT_ID;
	gJpegqDev.decIrqId = JPGDEC_IRQ_BIT_ID;

	gJpegqDev.pDev = &pdev->dev;
#endif

{
#ifdef JPEG_DEV
	int ret;
	struct class_device *class_dev = NULL;

	JPEG_MSG("-------------jpeg driver probe-------\n");
	ret = alloc_chrdev_region(&jenc_devno, 0, 1, JPEG_DEVNAME);

	if (ret)
		JPEG_ERR("Error: Can't Get Major number for JPEG Device\n");
	else
		JPEG_MSG("Get JPEG Device Major number (%d)\n", jenc_devno);

	jenc_cdev = cdev_alloc();
	jenc_cdev->owner = THIS_MODULE;
	jenc_cdev->ops = &jpeg_fops;

	ret = cdev_add(jenc_cdev, jenc_devno, 1);

	jenc_class = class_create(THIS_MODULE, JPEG_DEVNAME);
	class_dev =
	    (struct class_device *)device_create(jenc_class, NULL, jenc_devno, NULL, JPEG_DEVNAME);
#else
	proc_create("mtk_jpeg", 0664, NULL, &jpeg_fops);
#endif
}

	spin_lock_init(&jpeg_enc_lock);

	/* initial codec, register codec ISR */
	enc_status = 0;
	_jpeg_enc_int_status = 0;

#ifdef JPEG_DEC_DRIVER
	spin_lock_init(&jpeg_dec_lock);

	dec_status = 0;
	_jpeg_dec_int_status = 0;
	_jpeg_dec_mode = 0;
#endif

#ifndef FPGA_VERSION
	#ifdef JPEG_DEC_DRIVER
		init_waitqueue_head(&dec_wait_queue);
	#endif
	init_waitqueue_head(&enc_wait_queue);

	/* mt6575_irq_set_sens(MT6575_JPEG_CODEC_IRQ_ID, MT65xx_LEVEL_SENSITIVE); */
	/* mt6575_irq_set_polarity(MT6575_JPEG_CODEC_IRQ_ID, MT65xx_POLARITY_LOW); */
	/* mt6575_irq_unmask(MT6575_JPEG_CODEC_IRQ_ID); */
	JPEG_MSG("request JPEG Encoder IRQ\n");
	enable_irq(gJpegqDev.encIrqId);
	if (request_irq(gJpegqDev.encIrqId, jpeg_drv_enc_isr, IRQF_TRIGGER_LOW, "jpeg_enc_driver", NULL))
		JPEG_ERR("JPEG ENC Driver request irq failed\n");
	#ifdef JPEG_DEC_DRIVER
		enable_irq(gJpegqDev.decIrqId);
		JPEG_MSG("request JPEG Decoder IRQ\n");
		if (request_irq(gJpegqDev.decIrqId, jpeg_drv_dec_isr, IRQF_TRIGGER_FALLING, "jpeg_dec_driver", NULL))
			JPEG_ERR("JPEG DEC Driver request irq failed\n");
	#endif
#endif
	JPEG_MSG("JPEG Probe Done\n");

	return 0;
}

static int jpeg_remove(struct platform_device *pdev)
{
	JPEG_MSG("JPEG Codec remove\n");
	/* unregister_chrdev(JPEGDEC_MAJOR, JPEGDEC_DEVNAME); */
#ifndef FPGA_VERSION
	free_irq(gJpegqDev.encIrqId, NULL);
  #ifdef JPEG_DEC_DRIVER
	free_irq(gJpegqDev.decIrqId, NULL);
  #endif
#endif
	/* Support QoS */
	pm_qos_remove_request(&jpgenc_qos_request);

	JPEG_MSG("Done\n");
	return 0;
}

static void jpeg_shutdown(struct platform_device *pdev)
{
	JPEG_MSG("JPEG Codec shutdown\n");
	/* Nothing yet */
}

/* PM suspend */
static int jpeg_suspend(struct platform_device *pdev, pm_message_t mesg)
{
#ifdef JPEG_DEC_DRIVER
	if (dec_status != 0)
		jpeg_drv_dec_deinit();
#endif

	if (enc_status != 0)
		jpeg_drv_enc_deinit();
	return 0;
}

/* PM resume */
static int jpeg_resume(struct platform_device *pdev)
{
	return 0;
}


static int jpeg_pm_suspend(struct device *pDevice)
{
	struct platform_device *pdev = to_platform_device(pDevice);

	WARN_ON(pdev == NULL);

	return jpeg_suspend(pdev, PMSG_SUSPEND);
}

static int jpeg_pm_resume(struct device *pDevice)
{
	struct platform_device *pdev = to_platform_device(pDevice);

	WARN_ON(pdev == NULL);

	return jpeg_resume(pdev);
}

static int jpeg_pm_restore_noirq(struct device *pDevice)
{
	return 0;
}


static struct dev_pm_ops const jpeg_pm_ops = {
	.suspend = jpeg_pm_suspend,
	.resume = jpeg_pm_resume,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = jpeg_pm_restore_noirq,
};

static struct platform_driver jpeg_driver = {
	.probe = jpeg_probe,
	.remove = jpeg_remove,
	.shutdown = jpeg_shutdown,
	.suspend = jpeg_suspend,
	.resume = jpeg_resume,
	.driver = {
		.name = JPEG_DEVNAME,
		.pm = &jpeg_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = venc_jpg_of_ids,
#endif
		},
};


static void jpeg_device_release(struct device *dev)
{
	/* Nothing to release? */
}

static u64 jpegdec_dmamask = ~(u32) 0;

static struct platform_device jpeg_device = {
	.name = JPEG_DEVNAME,
	.id = 0,
	.dev = {
		.release = jpeg_device_release,
		.dma_mask = &jpegdec_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
		.num_resources = 0,
};

#if 0 /*def JPEG_PM_DOMAIN_ENABLE*/

/* Kernel interface */
static struct file_operations const jdec_fops = {
	.owner = THIS_MODULE,
	/* .ioctl          = jpeg_ioctl, */
	.unlocked_ioctl = NULL,
#ifdef CONFIG_COMPAT
	.compat_ioctl = NULL,
#endif
	.open = NULL,
	.release = NULL,
	.flush = NULL,
	.read = NULL,
};

static int jdec_probe(struct platform_device *pdev)
{
#ifdef JPEG_DEV
	int ret;
	struct class_device *class_dev = NULL;
#endif
	JPEG_MSG("+jdec_probe\n");

	pjdec_dev = pdev;
#ifdef JPEG_DEV
	ret = alloc_chrdev_region(&jdec_devno, 0, 1, JDEC_DEVNAME);
	if (ret)
		JPEG_MSG("Error: Can't Get Major number for %s Device\n", JDEC_DEVNAME);
	else
		JPEG_MSG("Get %s Device Major number (%d)\n", JDEC_DEVNAME, jdec_devno);

	jdec_cdev = cdev_alloc();
	jdec_cdev->owner = THIS_MODULE;
	jdec_cdev->ops = NULL;

	ret = cdev_add(jdec_cdev, jdec_devno, 1);

	jdec_class = class_create(THIS_MODULE, JDEC_DEVNAME);
	class_dev =
		(struct class_device *)device_create(jdec_class, NULL, jdec_devno, NULL, JDEC_DEVNAME);
#else
	proc_create(JDEC_DEVNAME, 0664, NULL, &jdec_fops);
#endif
	/* venc_power_on(); */
	return 0;
}



static struct platform_driver jdec_driver = {
	probe = jdec_probe,
	*.remove = vcodec_venc_remove, */
	driver = {
		.name = JDEC_DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = jdec_of_ids,
	}
};

#endif

static int __init jpeg_init(void)
{
	int ret;

	JPEG_MSG("JPEG Codec initialize\n");

#if 0
	JPEG_MSG("Register the JPEG Codec device\n");
	if (platform_device_register(&jpeg_device)) {
		JPEG_ERR("failed to register jpeg codec device\n");
		ret = -ENODEV;
		return ret;
	}
#endif

	JPEG_MSG("Register the JPEG Codec driver\n");
	if (platform_driver_register(&jpeg_driver)) {
		JPEG_ERR("failed to register jpeg codec driver\n");
		platform_device_unregister(&jpeg_device);
		ret = -ENODEV;
		return ret;
	}
#if 0 /*def JPEG_PM_DOMAIN_ENABLE*/
	if (platform_driver_register(&jdec_driver)) {
		JPEG_ERR("failed to register jdec_driver codec driver\n");
		platform_device_unregister(pjdec_dev);
		ret = -ENODEV;
		return ret;
	}
#endif
#ifdef MTK_JPEG_CMDQ_SUPPORT
	cmdqCoreRegisterCB(CMDQ_GROUP_JPEG,
			   cmdqJpegClockOn, cmdqJpegDumpInfo, cmdqJpegResetEng, cmdqJpegClockOff);
#endif
	Driver_Open_Count = 0;
	return 0;
}

static void __exit jpeg_exit(void)
{
	JPEG_MSG("jpeg_exit +\n");
#ifdef JPEG_DEV
	cdev_del(jenc_cdev);
	unregister_chrdev_region(jenc_devno, 1);
	device_destroy(jenc_class, jenc_devno);
	class_destroy(jenc_class);
#else
	remove_proc_entry("mtk_jpeg", NULL);
#endif
#ifdef MTK_JPEG_CMDQ_SUPPORT
	cmdqCoreRegisterCB(CMDQ_GROUP_JPEG, NULL, NULL, NULL, NULL);
#endif
	/* JPEG_MSG("Unregistering driver\n"); */
	platform_driver_unregister(&jpeg_driver);
	platform_device_unregister(&jpeg_device);
#ifdef JPEG_PM_DOMAIN_ENABLE
  #ifdef JPEG_DEV
	/* cdev_del(jdec_cdev); */
	/* unregister_chrdev_region(jdec_devno, 1); */
	/* device_destroy(jdec_class, jdec_devno); */
	/* class_destroy(jdec_class);*/
  #else
	remove_proc_entry("mtk_jenc", NULL);
  #endif
	/*platform_driver_unregister(&jdec_driver);*/
	platform_device_unregister(pjenc_dev);
	JPEG_MSG("jpeg_exit jdec remove\n");
#endif
	JPEG_MSG("jpeg_exit -\n");
}
module_init(jpeg_init);
module_exit(jpeg_exit);
MODULE_AUTHOR("Hao-Ting Huang <otis.huang@mediatek.com>");
MODULE_DESCRIPTION("MT6582 JPEG Codec Driver");
MODULE_LICENSE("GPL");
