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

#ifdef JPEG_HYBRID_DEC_DRIVER
#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#endif
#endif

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
#include <linux/suspend.h>
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
#include <linux/pm_runtime.h>
#include <linux/clk.h>
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

#if ENABLE_MMQOS || defined(JPEG_HYBRID_DEC_DRIVER)
#include "smi_port.h"
#include "mmdvfs_pmqos.h"
#endif

#include <ion_drv.h>

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

#ifdef JPEG_ENC_DRIVER
static const struct of_device_id venc_jpg_of_ids[] = {
	{.compatible = "mediatek,venc_jpg",},
	{}
};
#endif

#ifdef JPEG_HYBRID_DEC_DRIVER
static const struct of_device_id jdec_hybrid_of_ids[] = {
	{.compatible = "mediatek,jpgdec",},
	{}
};
#endif

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

/* hybrid decoder */
#ifdef JPEG_HYBRID_DEC_DRIVER
static wait_queue_head_t hybrid_dec_wait_queue[HW_CORE_NUMBER];
static DEFINE_MUTEX(jpeg_hybrid_dec_lock);
#define JPGDEC_BASE_0 0x17040000
#define JPGDEC_BASE_1 0x17050000
#define JPGDEC_BASE_2 0x17840000
#define JPGDEC_REGION 0x1000

static struct JPEG_DEC_DRV_HWINFO dec_hwinfo[HW_CORE_NUMBER] = {
	{false, JPGDEC_BASE_0},
	{false, JPGDEC_BASE_1},
	{false, JPGDEC_BASE_2}
};

#endif

#ifdef JPEG_PM_DOMAIN_ENABLE
struct platform_device *pjdec_dev;
struct platform_device *pjenc_dev;
#endif

/* encoder */
#ifdef JPEG_ENC_DRIVER
static wait_queue_head_t enc_wait_queue;
static spinlock_t jpeg_enc_lock;
static int enc_status;
static int enc_ready;
#endif
static DEFINE_MUTEX(jpeg_enc_power_lock);
static DEFINE_MUTEX(DriverOpenCountLock);
static int Driver_Open_Count;

#if ENABLE_MMQOS
static struct plist_head jpegenc_rlist;
static struct mm_qos_request jpeg_y_rdma;
static struct mm_qos_request jpeg_c_rdma;
static struct mm_qos_request jpeg_qtbl;
static struct mm_qos_request jpeg_bsdma;
static unsigned int cshot_spec_dts;
#endif

/* Support QoS */
struct pm_qos_request jpgenc_qos_request;
#ifdef JPEG_HYBRID_DEC_DRIVER
static struct pm_qos_request jpgdec_qos_request;
static u64 g_freq_steps[MAX_FREQ_STEP];  //index 0 is max
static u32 freq_step_size;
#endif

static struct ion_client *g_jpeg_ion_client;


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

#ifdef JPEG_ENC_DRIVER
void jpeg_drv_enc_prepare_bw_request(void)
{
#if ENABLE_MMQOS
	plist_head_init(&jpegenc_rlist);
	mm_qos_add_request(&jpegenc_rlist, &jpeg_y_rdma, SMI_JPGENC_Y_RDMA);
	mm_qos_add_request(&jpegenc_rlist, &jpeg_c_rdma, SMI_JPGENC_C_RDMA);
	mm_qos_add_request(&jpegenc_rlist, &jpeg_qtbl, SMI_JPGENC_Q_TABLE);
	mm_qos_add_request(&jpegenc_rlist, &jpeg_bsdma, SMI_JPGENC_BSDMA);
#endif

}

void jpegenc_drv_enc_update_bw_request(struct JPEG_ENC_DRV_IN cfgEnc)
{
#if ENABLE_MMQOS
	/* No spec, considering [picture size] x [target fps] */
	unsigned int cshot_spec = 0xffffffff;
	/* limiting FPS, Upper Bound FPS = 20 */
	unsigned int target_fps = 20;

	/* Support QoS */
	unsigned int emi_bw = 0;
	unsigned int picSize = 0;
	unsigned int limitedFPS = 0;


	/* Support QoS */
	picSize = (cfgEnc.encWidth * cfgEnc.encHeight) / 1000000;
	/* BW = encode width x height x bpp x 1.6 */
	/* Assume compress ratio is 0.6 */
	#if 0
	if (cfgEnc.encFormat == 0x0 || cfgEnc.encFormat == 0x1)
		picCost = ((picSize * 2) * 8/5) + 1;
	else
		picCost = ((picSize * 3/2) * 8/5) + 1;
	#endif


	cshot_spec = cshot_spec_dts;

	if ((picSize * target_fps) < cshot_spec) {
		emi_bw = picSize * target_fps;
	} else {
		limitedFPS = cshot_spec / picSize;
		emi_bw = (limitedFPS + 1) * picSize;
	}

	/* QoS requires Occupied BW */
	/* Data BW x 1.33 */
	emi_bw = emi_bw * 4/3;

	JPEG_MSG("Width %d Height %d emi_bw %d cshot_spec %d\n",
		 cfgEnc.encWidth, cfgEnc.encHeight, emi_bw, cshot_spec);

	mm_qos_set_request(&jpeg_y_rdma, emi_bw, 0, BW_COMP_NONE);

	if (cfgEnc.encFormat == 0x0 || cfgEnc.encFormat == 0x1)
		mm_qos_set_request(&jpeg_c_rdma, emi_bw, 0, BW_COMP_NONE);
	else
		mm_qos_set_request(&jpeg_c_rdma, emi_bw * 1/2, 0, BW_COMP_NONE);


	mm_qos_set_request(&jpeg_qtbl, 10, 0, BW_COMP_NONE);
	mm_qos_set_request(&jpeg_bsdma, emi_bw, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&jpegenc_rlist);


#endif
}



void jpegenc_drv_enc_remove_bw_request(void)
{
#if ENABLE_MMQOS
	mm_qos_remove_all_request(&jpegenc_rlist);
#endif
}


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
#endif

#ifdef JPEG_HYBRID_DEC_DRIVER
static int jpeg_drv_hybrid_dec_suspend(void)
{
	int i;

	JPEG_MSG("%s\n", __func__);
	for (i = 0 ; i < HW_CORE_NUMBER; i++) {
		JPEG_MSG("jpeg dec suspend core %d\n", i);
		if (dec_hwinfo[i].locked) {
			JPEG_MSG("jpeg dec suspend core %d fail\n", i);
			return -EBUSY;
		}
	}
	return 0;
}

static int jpeg_drv_hybrid_dec_suspend_notifier(
					struct notifier_block *nb,
					unsigned long action, void *data)
{
	int i;
	int wait_cnt = 0;

	JPEG_MSG("%s, action:%ld\n", __func__, action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		gJpegqDev.is_suspending = 1;
		for (i = 0 ; i < HW_CORE_NUMBER; i++) {
			JPEG_MSG("jpeg dec sn wait core %d\n", i);
			while (dec_hwinfo[i].locked) {
				JPEG_MSG("jpeg dec sn core %d locked. wait...\n", i);
				usleep_range(10000, 20000);
				wait_cnt++;
				if (wait_cnt > 5) {
					JPEG_MSG("jpeg dec sn wait core %d fail\n", i);
					return NOTIFY_DONE;
				}
			}
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		gJpegqDev.is_suspending = 0;
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static irqreturn_t jpeg_drv_hybrid_dec_isr(int irq, void *dev_id)
{
	int ret = 0;
	int i;

	JPEG_MSG("JPEG Hybrid Decoder Interrupt %d\n", irq);
	for (i = 0 ; i < HW_CORE_NUMBER; i++) {
		if (irq == gJpegqDev.hybriddecIrqId[i]) {
			if (!dec_hwinfo[i].locked) {
				JPEG_ERR("JPEG isr from unlocked HW %d",
						i);
				return IRQ_HANDLED;
			}
			ret = jpeg_isr_hybrid_dec_lisr(i);
			if (ret == 0)
				wake_up_interruptible(
				&(hybrid_dec_wait_queue[i]));
			JPEG_MSG("JPEG Hybrid Dec clear Interrupt %d ret %d\n"
					, irq, ret);
			break;
		}
	}

	return IRQ_HANDLED;
}

void jpeg_drv_hybrid_dec_prepare_dvfs(void)
{
	int ret;
	int i;

	if (!freq_step_size) {
		pm_qos_add_request(&jpgdec_qos_request, PM_QOS_VENC_FREQ,
				 PM_QOS_DEFAULT_VALUE);
		ret = mmdvfs_qos_get_freq_steps(PM_QOS_VENC_FREQ, g_freq_steps,
					&freq_step_size);
		if (ret < 0)
			JPEG_MSG("Failed to get venc freq steps (%d)\n", ret);

		for (i = 0 ; i < freq_step_size ; i++)
			JPEG_MSG("freq %d  %lx", i, g_freq_steps[i]);
	}

}

void jpeg_drv_hybrid_dec_unprepare_dvfs(void)
{
	pm_qos_update_request(&jpgdec_qos_request,  0);
	pm_qos_remove_request(&jpgdec_qos_request);
}

void jpeg_drv_hybrid_dec_start_dvfs(void)
{
	if (g_freq_steps[0] != 0) {
		JPEG_MSG("highest freq 0x%x", g_freq_steps[0]);
		pm_qos_update_request(&jpgdec_qos_request,  g_freq_steps[0]);
	}
}

void jpeg_drv_hybrid_dec_end_dvfs(void)
{
	pm_qos_update_request(&jpgdec_qos_request,  0);
}

void jpeg_drv_hybrid_dec_power_on(int id)
{
	int ret;
#ifdef CONFIG_MTK_PSEUDO_M4U
	int i, larb_port_num, larb_id;
	struct M4U_PORT_STRUCT port;
#endif

	if (id <= 1) {
		if (!dec_hwinfo[(id+1)%2].locked) {
			smi_bus_prepare_enable(SMI_LARB7, "JPEG0");
			ret = clk_prepare_enable(gJpegClk.clk_venc_jpgDec);
			if (ret)
				JPEG_ERR("clk MT_CG_VENC_JPGDEC failed %d",
						ret);
			ret = clk_prepare_enable(gJpegClk.clk_venc_jpgDec_c1);
			if (ret)
				JPEG_ERR("clk MT_CG_VENC_JPGDEC_C1 failed %d",
						ret);
		}
	} else {
		smi_bus_prepare_enable(SMI_LARB8, "JPEG1");
		ret = clk_prepare_enable(gJpegClk.clk_venc_c1_jpgDec);
		if (ret)
			JPEG_ERR("clk enable MT_CG_VENC_C1_JPGDEC failed %d",
					ret);
	}

	jpeg_drv_hybrid_dec_start_dvfs();

#ifdef CONFIG_MTK_PSEUDO_M4U
	if (id <= 1) {
		larb_port_num = SMI_LARB7_PORT_NUM;
		larb_id = 7;
	} else {
		larb_port_num = SMI_LARB8_PORT_NUM;
		larb_id = 8;
	}

	//enable 34bits port configs & sram settings
	for (i = 0; i < larb_port_num; i++) {
		if (i == 11 || i == 12 ||
			(i >= 23 && i <= 26)) {
			port.ePortID = MTK_M4U_ID(larb_id, i);
			port.Direction = 0;
			port.Distance = 1;
			port.domain = 0;
			port.Security = 0;
			port.Virtuality = 1;
			m4u_config_port(&port);
		}
	}
#endif
	JPEG_MSG("JPEG Hybrid Decoder Power On %d\n", id);
}

void jpeg_drv_hybrid_dec_power_off(int id)
{
	jpeg_drv_hybrid_dec_end_dvfs();
	if (id <= 1) {
		if (!dec_hwinfo[(id+1)%2].locked) {
			clk_disable_unprepare(gJpegClk.clk_venc_jpgDec);
			clk_disable_unprepare(gJpegClk.clk_venc_jpgDec_c1);
			smi_bus_disable_unprepare(SMI_LARB7, "JPEG0");
		}
	} else {
		clk_disable_unprepare(gJpegClk.clk_venc_c1_jpgDec);
		smi_bus_disable_unprepare(SMI_LARB8, "JPEG1");
	}
	JPEG_MSG("JPEG Hybrid Decoder Power Off %d\n", id);
}

#endif

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
		mtk_smi_larb_clock_on(1, true);
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
		mtk_smi_larb_clock_off(1, true);
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

#ifdef JPEG_ENC_DRIVER
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
	//mtk_smi_larb_clock_on(3, true);

	if (pm_runtime_get_sync(gJpegqDev.pDev) < 0)
		JPEG_ERR("pm_runtime_get sync jpeg fail\n");

	if (pm_runtime_get_sync(gJpegqDev.larbjpeg) < 0)
		JPEG_ERR("pm_runtime_get larbjpeg fail\n");

	if (clk_prepare_enable(gJpegClk.clk_venc_jpgEnc) < 0)
		JPEG_ERR("enable clk_venc_jpgEnc fail!");
	#else
		#ifdef CONFIG_MTK_SMI_EXT
		#if defined(PLATFORM_MT6779)
		smi_bus_prepare_enable(SMI_LARB3_REG_INDX,
			"JPEG", true);
		#elif defined(PLATFORM_MT6785)

		smi_bus_prepare_enable(SMI_LARB3, "JPEG");

		#elif defined(PLATFORM_MT6768)

		smi_bus_prepare_enable(SMI_LARB4, "JPEG");

		#elif defined(PLATFORM_MT6767)

		smi_bus_prepare_enable(SMI_LARB4, "JPEG");

		#elif defined(PLATFORM_MT6765)

		smi_bus_prepare_enable(SMI_LARB1, "JPEG");
		#elif defined(PLATFORM_MT6761)

		smi_bus_prepare_enable(SMI_LARB1, "JPEG");
		#elif defined(PLATFORM_MT6739)

		smi_bus_prepare_enable(SMI_LARB1, "JPEG");

		#elif defined(PLATFORM_MT6771)

		smi_bus_prepare_enable(SMI_LARB4, "JPEG");

		#endif
		if (clk_prepare_enable(gJpegClk.clk_venc_jpgEnc))
			JPEG_ERR("enable clk_venc_jpgDec fail!");
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
	enable_irq(gJpegqDev.encIrqId);
}

void jpeg_drv_enc_power_off(void)
{
	disable_irq(gJpegqDev.encIrqId);
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
	pm_runtime_put_sync(gJpegqDev.larbjpeg);
	pm_runtime_put_sync(gJpegqDev.pDev);
	#else
		#ifdef CONFIG_MTK_SMI_EXT
		clk_disable_unprepare(gJpegClk.clk_venc_jpgEnc);
		#if defined(PLATFORM_MT6779)

		smi_bus_disable_unprepare(SMI_LARB3_REG_INDX,
		"JPEG", true);

		#elif defined(PLATFORM_MT6785)

		smi_bus_disable_unprepare(SMI_LARB3, "JPEG");

		#elif defined(PLATFORM_MT6768)

		smi_bus_disable_unprepare(SMI_LARB4, "JPEG");

		#elif defined(PLATFORM_MT6767)

		smi_bus_disable_unprepare(SMI_LARB4, "JPEG");

		#elif defined(PLATFORM_MT6765)

		smi_bus_disable_unprepare(SMI_LARB1, "JPEG");

		#elif defined(PLATFORM_MT6761)

		smi_bus_disable_unprepare(SMI_LARB1, "JPEG");

		#elif defined(PLATFORM_MT6739)

		smi_bus_disable_unprepare(SMI_LARB1, "JPEG");


		#elif defined(PLATFORM_MT6771)

		smi_bus_disable_unprepare(SMI_LARB4, "JPEG");

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
#endif

#ifdef JPEG_HYBRID_DEC_DRIVER
static int jpeg_drv_dec_hybrid_lock(unsigned int *pa)
{
	int retValue = 0;
	int id = 0;

	if (gJpegqDev.is_suspending) {
		JPEG_WRN("jpeg dec is suspending");
		return -EBUSY;
	}

	mutex_lock(&jpeg_hybrid_dec_lock);
	for (id = 0; id < HW_CORE_NUMBER; id++) {
		if (dec_hwinfo[id].locked) {
			JPEG_WRN("jpeg dec HW core %d is busy", id);
			continue;
		} else {
			*pa = dec_hwinfo[id].hwpa;
			dec_hwinfo[id].locked = true;
			JPEG_WRN("jpeg dec get %d HW core pa 0x%x", id, *pa);
			_jpeg_hybrid_dec_int_status[id] = 0;
			jpeg_drv_hybrid_dec_power_on(id);
			enable_irq(gJpegqDev.hybriddecIrqId[id]);
			break;
		}
	}

	mutex_unlock(&jpeg_hybrid_dec_lock);
	if (id == HW_CORE_NUMBER) {
		JPEG_WRN("jpeg dec HW core all busy");
		retValue = -EBUSY;
		// TODO: keep waiting until there's an available HW core
	}

	return retValue;
}

static void jpeg_drv_dec_hybrid_unlock(unsigned int *pa)
{
	int id = 0;

	mutex_lock(&jpeg_hybrid_dec_lock);
	for (id = 0; id < HW_CORE_NUMBER; id++) {
		if (*pa == dec_hwinfo[id].hwpa) {
			if (!dec_hwinfo[id].locked) {
				JPEG_WRN("try to unlock a free core %d", id);
			} else {
				dec_hwinfo[id].locked = false;
				JPEG_WRN("jpeg dec HW core %d is unlocked", id);
				jpeg_drv_hybrid_dec_power_off(id);
				disable_irq(gJpegqDev.hybriddecIrqId[id]);
			}
			break;
		}
	}
	mutex_unlock(&jpeg_hybrid_dec_lock);
}
#endif

#ifdef JPEG_DEC_DRIVER
static int jpeg_drv_dec_init(void)
{
	int retValue;

	spin_lock(&jpeg_dec_lock);
	if (dec_status != 0) {
		JPEG_WRN("%s HW is busy\n", __func__);
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

#ifdef JPEG_ENC_DRIVER
static int jpeg_drv_enc_init(void)
{
	int retValue;
	spin_lock(&jpeg_enc_lock);
	if (enc_status != 0) {
		JPEG_WRN("%s HW is busy\n", __func__);
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
#endif

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
		reg_value = IMG_REG_READ(JPEG_DEC_BASE + index);
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
	unsigned int index = 0;
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
		("[JPGDRV]Dec: Private data is null in flush operation\n");
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
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_MSG
			("[JPGDRV]This process can not access decoder\n");
			return -EFAULT;
		}
		if (dec_status == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&dec_params,
				 (void *)arg,
				 sizeof(JPEG_DEC_DRV_IN))) {
			JPEG_MSG("[JPGDRV]JPEG Dec:Copy from user error\n");
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
		("[JPGDRV]This process can not access decoder\n");
		return -EFAULT;
	}
	if (dec_status == 0) {
		JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
		*pStatus = 0;
		return -EFAULT;
	}
	if (copy_from_user(&cfg_cmdq_params,
				 (void *)arg,
				 sizeof(JPEG_DEC_CONFIG_CMDQ))) {
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
		cmdqRecWrite(jpegCMDQ_handle, 0x18004140 /*DEC_DEST_ADDR0_Y*/,
			     cfg_cmdq_params.decRowBuf0[i], 0xFFFFFFFF);
		cmdqRecWrite(jpegCMDQ_handle, 0x18004144 /*DEC_DEST_ADDR0_U*/,
			     cfg_cmdq_params.decRowBuf1[i], 0xFFFFFFFF);
		cmdqRecWrite(jpegCMDQ_handle, 0x18004148 /*DEC_DEST_ADDR0_V*/,
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
			 ("[JPGDRV]This process can not access decoder\n");
			return -EFAULT;
		}
		if (dec_status == 0 || dec_ready == 0) {
			JPEG_MSG("[JPEGDRV]JPEG Decoder is unlocked!!");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&dec_row_params,
				 (void *)arg,
				 sizeof(JPEG_DEC_CONFIG_ROW))) {
			JPEG_MSG("[JPGDRV]JPEG Dec:Copy from user error\n");
			return -EFAULT;
		}

		JPEG_MSG("[JPGDRV]JPEG Decoder Resume, [%d] %x %x %x !!\n",
		dec_row_params.pauseMCU - 1, dec_row_params.decRowBuf[0],
		dec_row_params.decRowBuf[1], dec_row_params.decRowBuf[2]);

		if (!jpeg_drv_dec_set_dst_bank0(dec_row_params.decRowBuf[0],
			 dec_row_params.decRowBuf[1],
			 dec_row_params.decRowBuf[2])) {
			return -EFAULT;
		}
		index = dec_row_params.pauseMCU - 1;
		if (!jpeg_drv_dec_set_pause_mcu_idx(index))
			return -EFAULT;


		/* lock CPU to ensure irq is enabled after trigger HW */
		spin_lock(&jpeg_dec_lock);
		jpeg_drv_dec_resume(BIT_INQST_MASK_PAUSE);
		spin_unlock(&jpeg_dec_lock);
	break;

	case JPEG_DEC_IOCTL_START:	/* OT:OK */
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_WRN("This process can not access decoder");
			return -EFAULT;
		}
		if (dec_status == 0 || dec_ready == 0) {
			JPEG_WRN("Dec status is available,it can't HAPPEN");
			*pStatus = 0;
			return -EFAULT;
		}
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Decoder Start!!\n");*/

		jpeg_drv_dec_start();
		break;

	case JPEG_DEC_IOCTL_WAIT:
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_WRN("This process can not access decoder");
			return -EFAULT;
		}
		if (dec_status == 0 || dec_ready == 0) {
			JPEG_WRN("Dec status is available,it can't HAPPEN ??");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&outParams,
				 (void *)arg,
				 sizeof(JPEG_DEC_DRV_OUT))) {
			JPEG_WRN("JPEG Decoder : Copy from user error\n");
			return -EFAULT;
		}

		/* set timeout */
		timeout_jiff = outParams.timeout * HZ / 1000;
	#ifdef FPGA_VERSION
		JPEG_MSG("[JPEGDRV]Polling JPEG Status");

		do {
			_jpeg_dec_int_status =
				 IMG_REG_READ(REG_ADDR_JPGDEC_STATUS);
		} while (_jpeg_dec_int_status == 0);
	#else
		if (jpeg_isr_dec_lisr() < 0) {
			long ret = 0;

			do {
				/* JPEG_MSG("wait JPEG irq\n"); */
				ret = wait_event_interruptible_timeout(
						dec_wait_queue,
						 _jpeg_dec_int_status,
						 timeout_jiff);
				if (ret == 0)
					JPEG_MSG("[JPGDRV]Dec isr timeout\n");
			} while (ret < 0);
		} else
			JPEG_MSG("[JPGDRV] JPG Dec IRQ Wait Already Done!!\n");
	#endif

		decResult = jpeg_drv_dec_get_result();

		if (decResult >= 2) {
			JPEG_MSG("[JPEGDRV]Dec Result : %d, status %x!\n",
				 decResult,
				 _jpeg_dec_int_status);

			jpeg_drv_dec_dump_key_reg();

#ifndef JPEG_PM_DOMAIN_ENABLE
#ifdef CONFIG_MTK_SMI_EXT
		/* need to dump smi for the case that no irq coming from HW */
		if (decResult == 5)
			smi_debug_bus_hang_detect(0, "JPEG");
#endif
#endif
			jpeg_drv_dec_warm_reset();
		}
		irq_st = _jpeg_dec_int_status;
		decResult = decResult | (irq_st << 8);
		_jpeg_dec_int_status = 0;
		if (copy_to_user(outParams.result,
				 &decResult,
				 sizeof(unsigned int))) {
			JPEG_WRN("JPEG Dec: Copy to user error\n");
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
			JPEG_ERR("This process can not access encoder");
			return -EFAULT;
		}

		if (dec_status == 0) {
			JPEG_ERR("Enc status is available,can't HAPPEN");
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

#ifdef JPEG_ENC_DRIVER
static int jpeg_enc_ioctl(unsigned int cmd, unsigned long arg,
					 struct file *file)
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
	struct JPEG_ENC_DRV_IN cfgEnc;

	struct JPEG_ENC_DRV_OUT enc_result;

	 pStatus = (unsigned int *)file->private_data;

	if (pStatus == NULL) {
		JPEG_WRN("Private data null in flush,can't HAPPEN ??\n");
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
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0) {
			JPEG_WRN("Enc status is available,can't HAPPEN");
			*pStatus = 0;
			return -EFAULT;
		}

		/* copy input parameters */
		if (copy_from_user(&cfgEnc, (void *)arg,
				 sizeof(struct JPEG_ENC_DRV_IN))) {
			JPEG_MSG("[JPGDRV]Encoder : Copy from user error\n");
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
		/* srcChromaAddr = TO_CEIL(srcChromaAddr, 128); */
		/* //((srcChromaAddr+127)&~127); */
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


#if ENABLE_MMQOS
		jpegenc_drv_enc_update_bw_request(cfgEnc);
#else
		/* update BW request before trigger HW */
		pm_qos_update_request(&jpgenc_qos_request, emi_bw);
#endif
		/* 1. set src config */
		JPEG_MSG("[JPEGDRV]SRC_IMG:%x %x, DU:%x, fmt:%x\n",
			 cfgEnc.encWidth, cfgEnc.encHeight,
			 cfgEnc.totalEncDU, cfgEnc.encFormat);

		JPEG_MSG("[JPEGDRV]picSize:%d, BW:%d\n", picSize, emi_bw);

		ret =
		    jpeg_drv_enc_set_src_image(cfgEnc.encWidth,
					       cfgEnc.encHeight,
					       cfgEnc.encFormat,
					       cfgEnc.totalEncDU);
		if (ret == 0) {
			JPEG_MSG("[JPGDRV]Enc set srouce image failed\n");
			return -EFAULT;
		}

		/* 2. set src buffer info */
		JPEG_MSG("[JPEGDRV]SRC_BUF: addr %x, %x, stride %x, %x fd %d %d!!\n",
			 cfgEnc.srcBufferAddr,
			 cfgEnc.srcChromaAddr,
			 cfgEnc.imgStride,
			 cfgEnc.memStride,
			 cfgEnc.srcFd,
			 cfgEnc.srcFd2);

		ret =
		    jpeg_drv_enc_set_src_buf(g_jpeg_ion_client,
						 cfgEnc.encFormat,
					     cfgEnc.imgStride,
					     cfgEnc.memStride,
					     cfgEnc.memHeight,
					     cfgEnc.srcFd,
					     cfgEnc.srcFd2);
		if (ret == 0) {
			JPEG_MSG("[JPGDRV]Enc set srouce buffer failed\n");
			return -EFAULT;
		}

		/* if (0 == jpeg_drv_enc_src_cfg(src_cfg)) */
		/* { */
		/* JPEG_MSG("JPEG Encoder src cfg failed\n"); */
		/* return -EFAULT; */
		/* } */

		/* 3. set dst buffer info */
		JPEG_MSG("[JPGDRV]DST_BUF: addr:%x, size:%x, ofs:%x, mask:%x Fd 0x%x\n",
			 cfgEnc.dstBufferAddr,
			 cfgEnc.dstBufferSize,
			 cfgEnc.dstBufAddrOffset,
			 cfgEnc.dstBufAddrOffsetMask,
			 cfgEnc.dstFd);


		ret =
		    jpeg_drv_enc_set_dst_buff(g_jpeg_ion_client,
						  cfgEnc.dstFd,
					      cfgEnc.dstBufferSize,
					      cfgEnc.dstBufAddrOffset,
					      cfgEnc.dstBufAddrOffsetMask);
		if (ret == 0) {
			JPEG_MSG("[JPGDRV]Enc set dst buffer failed\n");
			return -EFAULT;
		}

		/* 4 .set ctrl config */
		JPEG_MSG("[JPEGDRV]ENC_CFG: exif:%d, q:%d, DRI:%d !!\n",
			 cfgEnc.enableEXIF,
			 cfgEnc.encQuality,
			 cfgEnc.restartInterval);

		jpeg_drv_enc_ctrl_cfg(cfgEnc.enableEXIF,
					 cfgEnc.encQuality,
					 cfgEnc.restartInterval);

		/* 5 . request IRQ*/
#ifdef CONFIG_MTK_SEC_JPEG_SUPPORT
		if (!cfgEnc.bSecure) {
			init_waitqueue_head(&enc_wait_queue);

			JPEG_MSG("request JPEG Encoder IRQ\n");
			enable_irq(gJpegqDev.encIrqId);
			if (request_irq(gJpegqDev.encIrqId, jpeg_drv_enc_isr,
				IRQF_TRIGGER_LOW, "jpeg_enc_driver", NULL))
				JPEG_ERR
				("JPEG ENC Driver request irq failed\n");
		}
#endif

		spin_lock(&jpeg_enc_lock);
		enc_ready = 1;
		spin_unlock(&jpeg_enc_lock);
		break;

	case JPEG_ENC_IOCTL_START:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Start!!\n");*/
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0 || enc_ready == 0) {
			JPEG_WRN("Enc status is unavailable,can't HAPPEN");
			*pStatus = 0;
			return -EFAULT;
		}
		jpeg_drv_enc_start();
		break;

	case JPEG_ENC_IOCTL_WAIT:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Wait!!\n");*/
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0 || enc_ready == 0) {
			JPEG_WRN("Enc status is unavailable, can't HAPPEN");
			*pStatus = 0;
			return -EFAULT;
		}
		if (copy_from_user(&enc_result,
				 (void *)arg,
				 sizeof(struct JPEG_ENC_DRV_OUT))) {
			JPEG_WRN("JPEG Encoder : Copy from user error\n");
			return -EFAULT;
		}

		/* TODO:    ENC_DONE in REG_JPEG_ENC_INTERRUPT_STATUS*/
		/*need to set to 0 after read. */
		jpeg_enc_wait_timeout = 0xFFFFFF;

	#ifdef FPGA_VERSION
		do {
			_jpeg_enc_int_status = IMG_REG_READ(
				REG_ADDR_JPEG_ENC_INTERRUPT_STATUS);
			jpeg_enc_wait_timeout--;
		} while (_jpeg_enc_int_status == 0 &&
			 jpeg_enc_wait_timeout > 0);

		if (jpeg_enc_wait_timeout == 0)
			JPEG_MSG("JPEG Encoder timeout\n");

		ret = jpeg_drv_enc_get_result(&file_size);

		JPEG_MSG("Result : %d, Size : %u, address : 0x%x\n",
			 ret,
			 file_size,
			 ioread32(JPEG_ENC_BASE + 0x120));

		if (_jpeg_enc_int_status != 1)
			jpeg_drv_enc_dump_reg();
	#else
		/* set timeout */
		timeout_jiff = enc_result.timeout * HZ / 1000;
		JPEG_MSG("[JPEGDRV]JPEG Encoder Time Jiffies : %ld\n",
			 timeout_jiff);

		if (jpeg_isr_enc_lisr() < 0) {
			do {
				ret = wait_event_interruptible_timeout(
					 enc_wait_queue,
					 _jpeg_enc_int_status,
					 timeout_jiff);
			if (ret > 0)
				JPEG_MSG("[JPGDRV]Enc Wait done\n");
			else if (ret == 0)
				JPEG_MSG("[JPGDRV]Enc Wait timeout\n");
			} while (ret < 0);
		} else
			JPEG_MSG("[JPGDRV]Enc already done !!\n");

#ifdef CONFIG_MTK_SEC_JPEG_SUPPORT
		if (!enc_result.bSecure)
			free_irq(gJpegqDev.encIrqId, NULL);
#endif

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
			smi_debug_bus_hang_detect(0, "JPEG");
#endif
#endif
			jpeg_drv_enc_warm_reset();

			return -EFAULT;
		}
	#endif
		cycle_count = jpeg_drv_enc_get_cycle_count();

		if (copy_to_user(enc_result.fileSize,
				 &file_size,
				 sizeof(unsigned int))) {
			JPEG_MSG("[JPGDRV]Enc:Copy to user error\n");
			return -EFAULT;
		}
		if (copy_to_user(enc_result.result,
				 &ret,
				 sizeof(unsigned int))) {
			JPEG_MSG("[JPGDRV]Enc:Copy to user error\n");
			return -EFAULT;
		}
		if (copy_to_user(enc_result.cycleCount,
				 &cycle_count,
				 sizeof(unsigned int))) {
			JPEG_MSG("[JPGDRV]Enc:Copy to user error\n");
			return -EFAULT;
		}
		break;

	case JPEG_ENC_IOCTL_DEINIT:
		/*JPEG_MSG("[JPEGDRV][IOCTL] JPEG Encoder Deinit!!\n");*/
		/* copy input parameters */
		if (*pStatus != JPEG_ENC_PROCESS) {
			JPEG_WRN("This process can not access encoder");
			return -EFAULT;
		}

		if (enc_status == 0) {
			JPEG_WRN("Enc status is available,can't HAPPEN");
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
#endif

#ifdef JPEG_HYBRID_DEC_DRIVER
static int jpeg_hybrid_dec_ioctl(unsigned int cmd, unsigned long arg,
			struct file *file)
{
	unsigned int *pStatus;
	unsigned int hwpa;
	int hwid;
	long timeout_jiff;

	struct JPEG_DEC_DRV_HYBRID_OUT outParams;
	struct JPEG_DEC_DRV_HYBRID_IN inParams;

	pStatus = (unsigned int *)file->private_data;

	if (pStatus == NULL) {
		JPEG_MSG
		("[JPGDRV]Hybrid Dec: Private data is null\n");
		return -EFAULT;
	}
	switch (cmd) {
	case JPEG_DEC_IOCTL_LOCK:
		JPEG_ERR("JPEG DEC IOCTL LOCK\n");
		if (copy_from_user(
			&outParams, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_OUT))) {
			JPEG_WRN("JPEG Decoder: Copy from user error\n");
			return -EFAULT;
		}
		if (outParams.timeout != 3000) // JPEG oal magic number
			return -EFAULT;
		if (jpeg_drv_dec_hybrid_lock(&hwpa) == 0) {
			*pStatus = JPEG_DEC_PROCESS;
			if (copy_to_user(
				outParams.hwpa, &hwpa, sizeof(unsigned int))) {
				JPEG_WRN("JPEG Decoder: Copy to user error\n");
				return -EFAULT;
			}
		} else {
			JPEG_WRN("jpeg_drv_dec_hybrid_init failed (hw busy)\n");
			return -EBUSY;
		}
		break;
	case JPEG_DEC_IOCTL_HYBRID_WAIT:
		JPEG_ERR("JPEG DEC IOCTL HYBRID WAIT\n");

		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_WRN(
			"Permission Denied! This process cannot access decoder");
			return -EFAULT;
		}

		if (copy_from_user(
			&inParams, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_IN))) {
			JPEG_WRN("JPEG Decoder : Copy from user error\n");
			return -EFAULT;
		}

		/* set timeout */
		timeout_jiff = 3000 * HZ / 1000;
		// TODO: set timeout
		JPEG_MSG("JPEG Hybrid Decoder Wait Resume Time: %ld\n",
				timeout_jiff);
		hwpa = inParams.hwpa;
		hwid = jpeg_dev_get_hybrid_decoder_id(hwpa);
		if (hwid < 0) {
			JPEG_ERR("JPEG Decoder: get hybrid dec id failed\n");
			return -EFAULT;
		}
	#ifdef FPGA_VERSION
		JPEG_MSG("Polling JPEG Hybrid Dec Status hwpa: 0x%x\n",
				inParams.hwpa);

		do {
			_jpeg_hybrid_dec_int_status[hwid] =
			IMG_REG_READ(REG_JPGDEC_HYBRID_INT_STATUS(hwid));
			JPEG_MSG("Hybrid Polling status %d\n",
			_jpeg_hybrid_dec_int_status[hwid]);
		} while (_jpeg_hybrid_dec_int_status[hwid] == 0);

	#else
		if (jpeg_isr_hybrid_dec_lisr(hwid) < 0) {
			long ret = 0;
			int waitfailcnt = 0;

			do {
				ret = wait_event_interruptible_timeout(
					hybrid_dec_wait_queue[hwid],
					_jpeg_hybrid_dec_int_status[hwid],
					timeout_jiff);
				if (ret == 0)
					JPEG_MSG(
					"JPEG Hybrid Dec Wait timeout!\n");
				if (ret < 0) {
					waitfailcnt++;
					JPEG_ERR(
					"JPEG Hybrid Dec Wait Error %d",
					waitfailcnt);
				}
			} while (ret < 0 && waitfailcnt < 5);
		} else
			JPEG_MSG("JPEG Hybrid Dec IRQ Wait Already Done!\n");
		_jpeg_hybrid_dec_int_status[hwid] = 0;
	#endif
		break;
	case JPEG_DEC_IOCTL_UNLOCK:
		JPEG_ERR("JPEG DEC IOCTL UNLOCK\n");
		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_WRN(
			"Permission Denied! This process cannot access decoder");
			return -EFAULT;
		}
		if (copy_from_user(
			&inParams, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_IN))) {
			JPEG_WRN("JPEG Decoder : Copy from user error\n");
			return -EFAULT;
		}
		JPEG_ERR("JPEG DEC IOCTL UNLOCK %d\n", inParams.hwpa);
		hwpa = inParams.hwpa;
		jpeg_drv_dec_hybrid_unlock(&hwpa);

		break;
	default:
		JPEG_ERR("JPEG DEC IOCTL NO THIS COMMAND\n");
		break;
	}
	return 0;
}
#endif /* JPEG_HYBRID_DEC_DRIVER */


/* -------------------------------------------------------------------------- */
/*  */
/* -------------------------------------------------------------------------- */

#ifdef CONFIG_COMPAT
static int compat_get_jpeg_hybrid_out_data(
		 struct compat_JPEG_DEC_DRV_HYBRID_OUT __user *data32,
		 struct JPEG_DEC_DRV_HYBRID_OUT __user *data)
{
	compat_long_t timeout;
	compat_uptr_t hwpa;
	int err;

	err = get_user(timeout, &data32->timeout);
	err |= put_user(timeout, &data->timeout);
	err |= get_user(hwpa, &data32->hwpa);
	err |= put_user(compat_ptr(hwpa), &data->hwpa);
	return err;
}

static int compat_get_jpeg_hybrid_in_data(
		 struct compat_JPEG_DEC_DRV_HYBRID_IN __user *data32,
		 struct JPEG_DEC_DRV_HYBRID_IN __user *data)
{
	compat_long_t timeout;
	unsigned int hwpa;
	int err;

	err = get_user(timeout, &data32->timeout);
	err |= put_user(timeout, &data->timeout);
	err |= get_user(hwpa, &data32->hwpa);
	err |= put_user(hwpa, &data->hwpa);
	return err;
}

static int compat_get_jpeg_dec_ioctl_wait_data(
		 struct compat_JPEG_DEC_DRV_OUT __user *data32,
		 struct JPEG_DEC_DRV_OUT __user *data)
{
	compat_long_t timeout;
	compat_uptr_t result;
	int err;

	err = get_user(timeout, &data32->timeout);
	err |= put_user(timeout, &data->timeout);
	err |= get_user(result, &data32->result);
	err |= put_user(compat_ptr(result), &data->result);
	return err;
}

static int compat_put_jpeg_dec_ioctl_wait_data(
		 struct compat_JPEG_DEC_DRV_OUT __user *data32,
		 struct JPEG_DEC_DRV_OUT __user *data)
{
	compat_long_t timeout;
	/* compat_uptr_t result; */
	int err;

	err = get_user(timeout, &data->timeout);
	err |= put_user(timeout, &data32->timeout);
	/* err |= get_user(result, &data->result); */
	/* err |= put_user(result, &data32->result); */
	return err;
}

static int compat_get_jpeg_dec_ioctl_chksum_data(
		 struct compat_JpegDrvDecResult __user *data32,
		 struct JpegDrvDecResult __user *data)
{
	compat_uptr_t pChksum;
	int err;

	err = get_user(pChksum, &data32->pChksum);
	err |= put_user(compat_ptr(pChksum), &data->pChksum);
	return err;
}

static int compat_put_jpeg_dec_ioctl_chksum_data(
		 struct compat_JpegDrvDecResult __user *data32,
		 struct JpegDrvDecResult __user *data)
{
	/* compat_uptr_t pChksum; */
	/* int err; */

	/* err = get_user(pChksum, &data->pChksum); */
	/* err |= put_user(pChksum, &data32->pChksum); */
	return 0;
}

static int compat_get_jpeg_enc_ioctl_wait_data(
		 struct compat_JPEG_ENC_DRV_OUT __user *data32,
		 struct JPEG_ENC_DRV_OUT __user *data)
{
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

static int compat_put_jpeg_enc_ioctl_wait_data(
		 struct compat_JPEG_ENC_DRV_OUT __user *data32,
		 struct JPEG_ENC_DRV_OUT __user *data)
{
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

static long compat_jpeg_ioctl(
		 struct file *filp,
		 unsigned int cmd,
		 unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_JPEG_DEC_IOCTL_WAIT:
		{
			struct compat_JPEG_DEC_DRV_OUT __user *data32;
			struct JPEG_DEC_DRV_OUT __user *data;
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
			struct compat_JpegDrvDecResult __user *data32;
			struct JpegDrvDecResult __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_jpeg_dec_ioctl_chksum_data(
				 data32,
				 data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(filp,
						       JPEG_DEC_IOCTL_CHKSUM,
						       (unsigned long)data);
			err = compat_put_jpeg_dec_ioctl_chksum_data(
				 data32,
				 data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_ENC_IOCTL_WAIT:
		{
			struct compat_JPEG_ENC_DRV_OUT __user *data32;
			struct JPEG_ENC_DRV_OUT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_jpeg_enc_ioctl_wait_data(data32, data);
			if (err)
				return err;
			ret =
			    filp->f_op->unlocked_ioctl(
				filp,
				 JPEG_ENC_IOCTL_WAIT,
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
		return filp->f_op->unlocked_ioctl(
			filp, cmd,
			 (unsigned long)compat_ptr(arg));
	case COMPAT_JPEG_DEC_IOCTL_LOCK:
		{
			struct compat_JPEG_DEC_DRV_HYBRID_OUT __user *data32;
			struct JPEG_DEC_DRV_HYBRID_OUT __user *data;
			int err;

			JPEG_MSG("COMPAT_JPEG_DEC_IOCTL_LOCK\n");
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;
			err = compat_get_jpeg_hybrid_out_data(data32, data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_LOCK,
					(unsigned long)data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT:
		{
			struct compat_JPEG_DEC_DRV_HYBRID_IN __user *data32;
			struct JPEG_DEC_DRV_HYBRID_IN __user *data;
			int err;

			JPEG_MSG("COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT\n");
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;
			err = compat_get_jpeg_hybrid_in_data(data32, data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(
					filp, JPEG_DEC_IOCTL_HYBRID_WAIT,
					(unsigned long)data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_DEC_IOCTL_UNLOCK:
		{
			struct compat_JPEG_DEC_DRV_HYBRID_IN __user *data32;
			struct JPEG_DEC_DRV_HYBRID_IN __user *data;
			int err;

			JPEG_MSG("COMPAT_JPEG_DEC_IOCTL_UNLOCK\n");
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;
			err = compat_get_jpeg_hybrid_in_data(data32, data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_UNLOCK,
					(unsigned long)data);
			return ret ? ret : err;
		}
	default:
		return -ENOIOCTLCMD;
	}
}
#endif


static long jpeg_unlocked_ioctl(
	struct file *file,
	 unsigned int cmd,
	 unsigned long arg)
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
#ifdef JPEG_HYBRID_DEC_DRIVER
	case JPEG_DEC_IOCTL_LOCK:
	case JPEG_DEC_IOCTL_HYBRID_WAIT:
	case JPEG_DEC_IOCTL_UNLOCK:
		return jpeg_hybrid_dec_ioctl(cmd, arg, file);
#endif
#ifdef JPEG_ENC_DRIVER
	case JPEG_ENC_IOCTL_INIT:
	case JPEG_ENC_IOCTL_CONFIG:
	case JPEG_ENC_IOCTL_WAIT:
	case JPEG_ENC_IOCTL_DEINIT:
	case JPEG_ENC_IOCTL_START:
		return jpeg_enc_ioctl(cmd, arg, file);
#endif
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

static ssize_t jpeg_read(
	struct file *file,
	 char __user *data,
	 size_t len, loff_t *ppos)
{
	JPEG_MSG("jpeg driver read\n");
	return 0;
}

static int jpeg_release(struct inode *inode, struct file *file)
{
	mutex_lock(&DriverOpenCountLock);
	Driver_Open_Count--;
	if (Driver_Open_Count == 0) {
#ifdef JPEG_ENC_DRIVER
		if (enc_status != 0) {
			JPEG_WRN("Enable error handle for enc");
			jpeg_drv_enc_deinit();
		}
#endif

#ifdef JPEG_DEC_DRIVER
		if (dec_status != 0) {
			JPEG_WRN("Enable error handle for dec");
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
		JPEG_WRN("Private data null in flush, can't HAPPEN\n");
		return -EFAULT;
	}
#ifdef JPEG_ENC_DRIVER
	if (*pStatus == JPEG_ENC_PROCESS) {
		if (enc_status != 0) {
			JPEG_WRN("Enable error handle for enc");
			jpeg_drv_enc_deinit();
		}
	}
#endif
#ifdef JPEG_DEC_DRIVER
	else if (*pStatus == JPEG_DEC_PROCESS) {
		if (dec_status != 0) {
			JPEG_WRN("Enable error handle for dec");
			jpeg_drv_dec_deinit();
		}
	}
#endif

	return 0;
}
#ifdef JPEG_HYBRID_DEC_DRIVER
void jpeg_vma_open(struct vm_area_struct *vma)
{
	JPEG_WRN("jpeg VMA open, virt %lx, phys %lx\n",
			vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void jpeg_vma_close(struct vm_area_struct *vma)
{
	JPEG_WRN("jpeg VMA close, virt %lx, phys %lx\n",
			vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

static const struct vm_operations_struct
jpeg_remap_vm_ops = {
	.open = jpeg_vma_open,
	.close = jpeg_vma_close,
};

static int jpeg_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long length;
	unsigned long pfn;

	length = vma->vm_end - vma->vm_start;
	pfn = vma->vm_pgoff<<PAGE_SHIFT;

	if (length > JPGDEC_REGION || (pfn != JPGDEC_BASE_0
		&& pfn != JPGDEC_BASE_1 && pfn != JPGDEC_BASE_2)) {
		JPEG_WRN("[JPEG] illegal mmap pfn 0x%lx, len 0x%lx\n",
			pfn, length);
		return -EAGAIN;
	}
	JPEG_WRN("[JPEG] start 0x%lx, end 0x%lx, pgoff 0x%lx\n",
			(unsigned long)vma->vm_start,
			(unsigned long)vma->vm_end,
			(unsigned long)vma->vm_pgoff);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops = &jpeg_remap_vm_ops;
	jpeg_vma_open(vma);

	return 0;
}
#endif

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
#ifdef JPEG_HYBRID_DEC_DRIVER
	.mmap = jpeg_mmap,
#endif
};


const long jpeg_dev_get_encoder_base_VA(void)
{
	return gJpegqDev.encRegBaseVA;
}


const long jpeg_dev_get_decoder_base_VA(void)
{
	return gJpegqDev.decRegBaseVA;
}

#ifdef JPEG_HYBRID_DEC_DRIVER
const long jpeg_dev_get_hybrid_decoder_base_VA(int id)
{
	return gJpegqDev.hybriddecRegBaseVA[id];
}

const int jpeg_dev_get_hybrid_decoder_id(unsigned int pa)
{
	int id = 0;

	for (id = 0; id < HW_CORE_NUMBER; id++) {
		if (dec_hwinfo[id].hwpa == pa) {
			JPEG_MSG("jpeg dec HW id of pa 0x%x is %d\n", pa, id);
			return id;
		}
	}
	JPEG_ERR("jpeg dec HW pa 0x%x unknown\n", pa);
	return -1;
}
#endif

static int jpeg_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	int new_count;
	struct JpegDeviceStruct *jpegDev;
	struct device_node *node = NULL;
#ifdef JPEG_PM_DOMAIN_ENABLE
	struct platform_device *pJpeg = NULL;
#endif
	void *tmpPtr;
#ifdef JPEG_HYBRID_DEC_DRIVER
	int i;
#endif

	new_count = nrJpegDevs + 1;
	tmpPtr = krealloc(gJpegqDevs,
		 sizeof(struct JpegDeviceStruct) * new_count,
		 GFP_KERNEL);
	if (!tmpPtr) {
		JPEG_ERR("Unable to allocate JpegDeviceStruct\n");
		return -ENOMEM;
	}
	gJpegqDevs = (struct JpegDeviceStruct *)tmpPtr;

	jpegDev = &(gJpegqDevs[nrJpegDevs]);
	jpegDev->pDev = &pdev->dev;
	memset(&gJpegqDev, 0x0, sizeof(struct JpegDeviceStruct));

	node = pdev->dev.of_node;
#ifdef JPEG_ENC_DRIVER
	jpegDev->encRegBaseVA = (unsigned long)of_iomap(node, 0);
	jpegDev->encIrqId = irq_of_parse_and_map(node, 0);
	#ifdef CONFIG_MTK_CLKMGR
	#else
		#ifdef JPEG_PM_DOMAIN_ENABLE
		node = of_find_compatible_node(NULL, NULL, "mediatek,venc_gcon");

		pJpeg = of_find_device_by_node(node);
		if (WARN_ON(!pJpeg)) {
			of_node_put(node);
			return -EINVAL;
		}
		#if 0
		gJpegClk.clk_venc_jpgEnc =
			devm_clk_get(&pJpeg->dev, "MT_CG_VENC_JPGENC");
		if (IS_ERR(gJpegClk.clk_venc_jpgEnc))
			JPEG_ERR("get MT_CG_VENC_JPGENC clk error!");
		#else
		gJpegClk.clk_venc_jpgEnc =
			of_clk_get_by_name(node, "MT_CG_VENC_JPGENC");
		if (IS_ERR(gJpegClk.clk_venc_jpgEnc))
			JPEG_ERR("get MT_CG_VENC_JPGENC clk error!");
		#endif
		node = of_parse_phandle(pJpeg->dev.of_node, "mediatek,larb", 0);
		if (!node)
			return -EINVAL;

		pJpeg = of_find_device_by_node(node);
		if (WARN_ON(!pJpeg)) {
			of_node_put(node);
			return -EINVAL;
		}
		jpegDev->larbjpeg = &pJpeg->dev;

		pm_runtime_enable(&pdev->dev);
		node = pdev->dev.of_node;
		#else
			#ifndef CONFIG_MTK_SMI_EXT
				/* venc-mtcmos lead to disp */
				/* power scpsys SCP_SYS_DISP */
				gJpegClk.clk_scp_sys_mm0 =
				of_clk_get_by_name(node, "MT_CG_SCP_SYS_MM0");
				if (IS_ERR(gJpegClk.clk_scp_sys_mm0))
					JPEG_ERR("get MT_CG_SCP_SYS_MM0 err");
				/* venc-mtcmos lead to venc */
				/* power scpsys SCP_SYS_VEN */
				gJpegClk.clk_scp_sys_ven =
				of_clk_get_by_name(node, "MT_CG_SCP_SYS_VEN");
				if (IS_ERR(gJpegClk.clk_scp_sys_ven))
					JPEG_ERR("get MT_CG_SCP_SYS_VEN err");

				gJpegClk.clk_smi_common =
				of_clk_get_by_name(node, "MT_CG_SMI_COMMON");
				if (IS_ERR(gJpegClk.clk_smi_common))
					JPEG_ERR("get MT_CG_SMI_COMMON err");
				gJpegClk.clk_venc_larb =
				of_clk_get_by_name(node, "MT_CG_VENC_LARB");
				if (IS_ERR(gJpegClk.clk_venc_larb))
					JPEG_ERR("get MT_CG_VENC_LARB err");
			#endif//#ifndef CONFIG_MTK_SMI_EXT
			gJpegClk.clk_venc_jpgEnc =
			of_clk_get_by_name(node, "MT_CG_VENC_JPGENC");
			if (IS_ERR(gJpegClk.clk_venc_jpgEnc))
				JPEG_ERR("get MT_CG_VENC_JPGENC clk error!");
		#endif//#ifdef JPEG_PM_DOMAIN_ENABLE
	#endif
#endif
#ifdef JPEG_HYBRID_DEC_DRIVER
	node = of_find_compatible_node(NULL, NULL, "mediatek,jpgdec");
	for (i = 0; i < HW_CORE_NUMBER; i++) {
		jpegDev->hybriddecRegBaseVA[i] =
					(unsigned long)of_iomap(node, i);
		jpegDev->hybriddecIrqId[i] = irq_of_parse_and_map(node, i);
		JPEG_ERR("Jpeg Hybrid Dec Probe %d base va 0x%x irqid %d",
				i, jpegDev->hybriddecRegBaseVA[i],
				jpegDev->hybriddecIrqId[i]);
	}
	jpeg_drv_hybrid_dec_prepare_dvfs();
	JPEG_MSG("get JPGDEC clk!");
	gJpegClk.clk_venc_jpgDec =
			 of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC");
	if (IS_ERR(gJpegClk.clk_venc_jpgDec))
		JPEG_ERR("get MT_CG_VENC_JPGDEC clk error!");
	gJpegClk.clk_venc_jpgDec_c1 =
			 of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC_C1");
	if (IS_ERR(gJpegClk.clk_venc_jpgDec_c1))
		JPEG_ERR("get MT_CG_VENC_JPGDEC_C1 clk error!");
	gJpegClk.clk_venc_c1_jpgDec =
			 of_clk_get_by_name(node, "MT_CG_VENC_C1_JPGDEC");
	if (IS_ERR(gJpegClk.clk_venc_c1_jpgDec))
		JPEG_ERR("get MT_CG_VENC_C1_JPGDEC clk error!");
	JPEG_MSG("get JPGDEC clk done!");
	jpegDev->pm_notifier.notifier_call = jpeg_drv_hybrid_dec_suspend_notifier;
	register_pm_notifier(&jpegDev->pm_notifier);
	jpegDev->is_suspending = 0;
#endif
	#ifdef JPEG_DEC_DRIVER
		jpegDev->decRegBaseVA = (unsigned long)of_iomap(node, 1);
		jpegDev->decIrqId = irq_of_parse_and_map(node, 1);
	    #ifdef CONFIG_MTK_CLKMGR
	    #else
			gJpegClk.clk_venc_jpgDec =
			 of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC");
			if (IS_ERR(gJpegClk.clk_venc_jpgDec))
				JPEG_ERR("get MT_CG_VENC_JPGDEC err");
		#endif
	#endif
#ifdef JPEG_ENC_DRIVER
	#if ENABLE_MMQOS
	if (of_property_read_u32(node, "cshot-spec", &cshot_spec_dts)) {
		JPEG_ERR("cshot spec read failed\n");
		JPEG_ERR("init cshot spec as 0xFFFFFFFF\n");
		cshot_spec_dts = 0xFFFFFFFF;
	}
	#endif
#endif

	gJpegqDev = *jpegDev;

#ifdef JPEG_ENC_DRIVER
	/* Support QoS */
	pm_qos_add_request(&jpgenc_qos_request,
		PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
#endif

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
		JPEG_ERR("Can't Get Major number for JPEG Device\n");
	else
		JPEG_MSG("Get JPEG Device Major number (%d)\n", jenc_devno);

	jenc_cdev = cdev_alloc();
	jenc_cdev->owner = THIS_MODULE;
	jenc_cdev->ops = &jpeg_fops;

	ret = cdev_add(jenc_cdev, jenc_devno, 1);

	jenc_class = class_create(THIS_MODULE, JPEG_DEVNAME);
	class_dev =
	    (struct class_device *)device_create(jenc_class,
			 NULL, jenc_devno, NULL, JPEG_DEVNAME);
#else
	proc_create("mtk_jpeg", 0x644, NULL, &jpeg_fops);
#endif
}
#ifdef JPEG_ENC_DRIVER
	spin_lock_init(&jpeg_enc_lock);

	/* initial codec, register codec ISR */
	enc_status = 0;
	_jpeg_enc_int_status = 0;
#endif
#ifdef JPEG_DEC_DRIVER
	spin_lock_init(&jpeg_dec_lock);

	dec_status = 0;
	_jpeg_dec_int_status = 0;
	_jpeg_dec_mode = 0;
#endif

#ifndef CONFIG_MTK_SEC_JPEG_SUPPORT
#ifndef FPGA_VERSION
	#ifdef JPEG_DEC_DRIVER
		init_waitqueue_head(&dec_wait_queue);
	#endif
    #ifdef JPEG_ENC_DRIVER
	init_waitqueue_head(&enc_wait_queue);

	/* mt6575_irq_unmask(MT6575_JPEG_CODEC_IRQ_ID); */
	JPEG_MSG("request JPEG Encoder IRQ\n");
	if (request_irq(gJpegqDev.encIrqId,
		 jpeg_drv_enc_isr, IRQF_TRIGGER_LOW,
		 "jpeg_enc_driver", NULL))
		JPEG_ERR("JPEG ENC Driver request irq failed\n");
    #endif
	#ifdef JPEG_DEC_DRIVER
		enable_irq(gJpegqDev.decIrqId);
		JPEG_MSG("request JPEG Decoder IRQ\n");
		if (request_irq(gJpegqDev.decIrqId,
				 jpeg_drv_dec_isr, IRQF_TRIGGER_FALLING,
				 "jpeg_dec_driver", NULL))
			JPEG_ERR("JPEG DEC Driver request irq failed\n");
	#endif
#endif
#ifdef JPEG_HYBRID_DEC_DRIVER

	memset(_jpeg_hybrid_dec_int_status, 0, HW_CORE_NUMBER);
	for (i = 0; i < HW_CORE_NUMBER; i++) {
		JPEG_MSG("Request irq %d\n", gJpegqDev.hybriddecIrqId[i]);
		init_waitqueue_head(&(hybrid_dec_wait_queue[i]));
		if (request_irq(gJpegqDev.hybriddecIrqId[i],
				 jpeg_drv_hybrid_dec_isr, IRQF_TRIGGER_HIGH,
				 "jpeg_dec_driver", NULL))
			JPEG_ERR("JPEG Hybrid DEC requestirq %d failed\n", i);
		disable_irq(gJpegqDev.hybriddecIrqId[i]);
	}
#endif
#endif //#ifndef CONFIG_MTK_SEC_JPEG_SUPPORT
	JPEG_MSG("JPEG Probe Done\n");

	return 0;
}

static int jpeg_remove(struct platform_device *pdev)
{
#ifdef JPEG_HYBRID_DEC_DRIVER
	int i;
#endif
	JPEG_MSG("JPEG Codec remove\n");
	/* unregister_chrdev(JPEGDEC_MAJOR, JPEGDEC_DEVNAME); */
#ifndef CONFIG_MTK_SEC_JPEG_SUPPORT
#ifndef FPGA_VERSION
	#ifdef JPEG_ENC_DRIVER
	free_irq(gJpegqDev.encIrqId, NULL);
	#endif
	#ifdef JPEG_DEC_DRIVER
	free_irq(gJpegqDev.decIrqId, NULL);
	#endif
	#ifdef JPEG_HYBRID_DEC_DRIVER
	for (i = 0; i < HW_CORE_NUMBER; i++)
		free_irq(gJpegqDev.hybriddecIrqId[i], NULL);
	jpeg_drv_hybrid_dec_unprepare_dvfs();
	#endif
#endif
#endif
    #ifdef JPEG_ENC_DRIVER
	/* Support QoS */
	pm_qos_remove_request(&jpgenc_qos_request);
    #endif
#ifdef JPEG_PM_DOMAIN_ENABLE
	pm_runtime_disable(&pdev->dev);
#endif
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
#ifdef JPEG_HYBRID_DEC_DRIVER
	int ret;

	ret = jpeg_drv_hybrid_dec_suspend();
	if (ret != 0)
		return ret;
#endif
#ifdef JPEG_ENC_DRIVER
	if (enc_status != 0)
		jpeg_drv_enc_deinit();
#endif
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
#ifdef JPEG_ENC_DRIVER
		.of_match_table = venc_jpg_of_ids,
#endif
#ifdef JPEG_HYBRID_DEC_DRIVER
		.of_match_table = jdec_hybrid_of_ids,
#endif
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
	JPEG_MSG("+%s\n", __func__);

	pjdec_dev = pdev;
#ifdef JPEG_DEV
	ret = alloc_chrdev_region(&jdec_devno, 0, 1, JDEC_DEVNAME);
	if (ret)
		JPEG_MSG("Can't Get Major number for %s\n",
		JDEC_DEVNAME);
	else
		JPEG_MSG("Get %s Device Major number (%d)\n",
		 JDEC_DEVNAME, jdec_devno);

	jdec_cdev = cdev_alloc();
	jdec_cdev->owner = THIS_MODULE;
	jdec_cdev->ops = NULL;

	ret = cdev_add(jdec_cdev, jdec_devno, 1);

	jdec_class = class_create(THIS_MODULE, JDEC_DEVNAME);
	class_dev =
		(struct class_device *)device_create(
		jdec_class, NULL, jdec_devno, NULL, JDEC_DEVNAME);
#else
	proc_create(JDEC_DEVNAME, 0x644, NULL, &jdec_fops);
#endif
	/* venc_power_on(); */
	return 0;
}



static struct platform_driver jdec_driver = {
	probe = jdec_probe,
	/*.remove = vcodec_venc_remove, */
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
			 cmdqJpegClockOn,
			 cmdqJpegDumpInfo,
			 cmdqJpegResetEng,
			 cmdqJpegClockOff);
#endif
	Driver_Open_Count = 0;
#ifdef JPEG_ENC_DRIVER
	jpeg_drv_enc_prepare_bw_request();
#endif

	if (!g_jpeg_ion_client && g_ion_device) {
		JPEG_MSG("create ion client\n");
		g_jpeg_ion_client = ion_client_create(g_ion_device, "jpegenc");
	}
	return 0;
}

static void __exit jpeg_exit(void)
{
	JPEG_MSG("%s +\n", __func__);
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
	JPEG_MSG("%s jdec remove\n", __func__);
#endif
#ifdef JPEG_ENC_DRIVER
	jpegenc_drv_enc_remove_bw_request();
#endif
	if (g_jpeg_ion_client)
		ion_client_destroy(g_jpeg_ion_client);
	g_jpeg_ion_client = NULL;
	JPEG_MSG("%s -\n", __func__);
}
module_init(jpeg_init);
module_exit(jpeg_exit);
MODULE_AUTHOR("Hao-Ting Huang <otis.huang@mediatek.com>");
MODULE_DESCRIPTION("MT6582 JPEG Codec Driver");
MODULE_LICENSE("GPL");
