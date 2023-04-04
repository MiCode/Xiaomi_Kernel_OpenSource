// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <soc/mediatek/smi.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>

#include "mtk-interconnect.h"


#include "jpeg_drv.h"
#include "jpeg_drv_reg.h"
#include "jpeg_drv_common.h"


#define JPEG_DEVNAME "mtk_jpeg"

#define JPEG_DEC_PROCESS 0x1
#define JPEG_ENC_PROCESS 0x2

static struct JpegDeviceStruct gJpegqDev;
static atomic_t nodeCount;

static const struct of_device_id jpeg_of_ids[] = {
	{.compatible = "mediatek,jpgdec0",},
	{.compatible = "mediatek,jpgdec1",},
	{.compatible = "mediatek,venc_jpg",},
	{}
};

/* hybrid decoder */
static wait_queue_head_t hybrid_dec_wait_queue[HW_CORE_NUMBER];
static DEFINE_MUTEX(jpeg_hybrid_dec_lock);

static bool dec_hwlocked[HW_CORE_NUMBER] = {false, false/*, false*/};
static unsigned int _jpeg_hybrid_dec_int_status[HW_CORE_NUMBER];
static struct dmabuf_info bufInfo[HW_CORE_NUMBER];

/* jpeg encoder */
static wait_queue_head_t enc_wait_queue;
static DEFINE_MUTEX(jpeg_enc_power_lock);
static unsigned int enc_status;
static unsigned int enc_ready;
const char *platform_string;

int jpg_dbg_level;
module_param(jpg_dbg_level, int, 0644);

static int jpeg_isr_hybrid_dec_lisr(int id)
{
	unsigned int tmp = 0;

	if (dec_hwlocked[id]) {
		tmp = IMG_REG_READ(REG_JPGDEC_HYBRID_274(id));
		if (tmp) {
			_jpeg_hybrid_dec_int_status[id] = tmp;
			IMG_REG_WRITE(tmp, REG_JPGDEC_HYBRID_274(id));
			JPEG_LOG(1, "return 0");
			return 0;
		}
	}
	JPEG_LOG(1, "return -1");
	return -1;
}

static int jpeg_drv_hybrid_dec_start(unsigned int data[],
										unsigned int id,
										int *index_buf_fd)
{
	u64 ibuf_iova, obuf_iova;
	int ret;
	void *ptr;
	unsigned int node_id;

	JPEG_LOG(1, "+ id:%d", id);
	ret = 0;
	ibuf_iova = 0;
	obuf_iova = 0;
	node_id = id / 2;

	mutex_lock(&jpeg_hybrid_dec_lock);
	bufInfo[id].o_dbuf = jpg_dmabuf_alloc(data[20], 128, 0);
	bufInfo[id].o_attach = NULL;
	bufInfo[id].o_sgt = NULL;

	bufInfo[id].i_dbuf = jpg_dmabuf_get(data[7]);
	bufInfo[id].i_attach = NULL;
	bufInfo[id].i_sgt = NULL;

	if (!bufInfo[id].o_dbuf) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "o_dbuf alloc failed");
		return -1;
	}

	if (!bufInfo[id].i_dbuf) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "i_dbuf null error");
		return -1;
	}

	ret = jpg_dmabuf_get_iova(bufInfo[id].o_dbuf, &obuf_iova, gJpegqDev.pDev[node_id], &bufInfo[id].o_attach, &bufInfo[id].o_sgt);
	JPEG_LOG(1, "obuf_iova:0x%llx lsb:0x%lx msb:0x%lx", obuf_iova,
		(unsigned long)(unsigned char*)obuf_iova,
		(unsigned long)(unsigned char*)(obuf_iova>>32));

	ptr = jpg_dmabuf_vmap(bufInfo[id].o_dbuf);
	if (ptr != NULL && data[20] > 0)
		memset(ptr, 0, data[20]);
	jpg_dmabuf_vunmap(bufInfo[id].o_dbuf, ptr);
	jpg_get_dmabuf(bufInfo[id].o_dbuf);
	// get obuf for adding reference count, avoid early release in userspace.
	*index_buf_fd = jpg_dmabuf_fd(bufInfo[id].o_dbuf);

	ret = jpg_dmabuf_get_iova(bufInfo[id].i_dbuf, &ibuf_iova, gJpegqDev.pDev[node_id], &bufInfo[id].i_attach, &bufInfo[id].i_sgt);
	JPEG_LOG(1, "ibuf_iova 0x%llx lsb:0x%lx msb:0x%lx", ibuf_iova,
		(unsigned long)(unsigned char*)ibuf_iova,
		(unsigned long)(unsigned char*)(ibuf_iova>>32));

	if (ret != 0) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "get iova fail i:0x%llx o:0x%llx", ibuf_iova, obuf_iova);
		return ret;
	}

	if (!dec_hwlocked[id]) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "hw %d unlocked, start fail", id);
		return -1;
	}

	IMG_REG_WRITE(data[0], REG_JPGDEC_HYBRID_090(id));
	IMG_REG_WRITE(data[1], REG_JPGDEC_HYBRID_090(id));
	IMG_REG_WRITE(data[2], REG_JPGDEC_HYBRID_0FC(id));
	IMG_REG_WRITE(data[3], REG_JPGDEC_HYBRID_14C(id));
	IMG_REG_WRITE(data[4], REG_JPGDEC_HYBRID_150(id));
	IMG_REG_WRITE(data[5], REG_JPGDEC_HYBRID_154(id));
	IMG_REG_WRITE(data[6], REG_JPGDEC_HYBRID_17C(id));
	IMG_REG_WRITE((unsigned long)(unsigned char*)ibuf_iova, REG_JPGDEC_HYBRID_200(id));
	IMG_REG_WRITE((unsigned long)(unsigned char*)(ibuf_iova>>32), REG_JPGDEC_HYBRID_378(id));
	IMG_REG_WRITE(data[8], REG_JPGDEC_HYBRID_20C(id));
	IMG_REG_WRITE(data[9], REG_JPGDEC_HYBRID_210(id));
	IMG_REG_WRITE(data[10], REG_JPGDEC_HYBRID_224(id));
	IMG_REG_WRITE(data[11], REG_JPGDEC_HYBRID_23C(id));
	IMG_REG_WRITE(data[12], REG_JPGDEC_HYBRID_24C(id));
	IMG_REG_WRITE(data[13], REG_JPGDEC_HYBRID_270(id));
	IMG_REG_WRITE(data[14], REG_JPGDEC_HYBRID_31C(id));
	IMG_REG_WRITE(data[15], REG_JPGDEC_HYBRID_330(id));
	IMG_REG_WRITE(data[16], REG_JPGDEC_HYBRID_334(id));
	IMG_REG_WRITE((unsigned long)(unsigned char*)obuf_iova, REG_JPGDEC_HYBRID_338(id));
	IMG_REG_WRITE((unsigned long)(unsigned char*)(obuf_iova>>32), REG_JPGDEC_HYBRID_384(id));
	IMG_REG_WRITE((unsigned long)(unsigned char*)obuf_iova, REG_JPGDEC_HYBRID_36C(id));
	IMG_REG_WRITE((unsigned long)(unsigned char*)obuf_iova + data[20], REG_JPGDEC_HYBRID_370(id));
	IMG_REG_WRITE((unsigned long)(unsigned char*)obuf_iova + data[20]*2, REG_JPGDEC_HYBRID_374(id));
	IMG_REG_WRITE(data[17], REG_JPGDEC_HYBRID_33C(id));
	IMG_REG_WRITE(data[18], REG_JPGDEC_HYBRID_344(id));
	IMG_REG_WRITE(data[19], REG_JPGDEC_HYBRID_240(id));

	mutex_unlock(&jpeg_hybrid_dec_lock);

	JPEG_LOG(1, "-");
	return ret;
}

static void jpeg_drv_hybrid_dec_get_p_n_s(
										unsigned int id,
										int *progress_n_status)
{
	int progress, status;
	progress = IMG_REG_READ(REG_JPGDEC_HYBRID_340(id)) - 1;
	status = IMG_REG_READ(REG_JPGDEC_HYBRID_348(id));
	*progress_n_status = progress << 4 | status;
	JPEG_LOG(1, "progress_n_status %d", *progress_n_status);
}

static irqreturn_t jpeg_drv_hybrid_dec_isr(int irq, void *dev_id)
{
	int ret = 0;
	int i;

	JPEG_LOG(1, "JPEG Hybrid Decoder Interrupt %d", irq);
	for (i = 0 ; i < HW_CORE_NUMBER; i++) {
		if (irq == gJpegqDev.hybriddecIrqId[i]) {
			if (!dec_hwlocked[i]) {
				JPEG_LOG(0, "JPEG isr from unlocked HW %d",
						i);
				return IRQ_HANDLED;
			}
			ret = jpeg_isr_hybrid_dec_lisr(i);
			if (ret == 0)
				wake_up_interruptible(
				&(hybrid_dec_wait_queue[i]));
			JPEG_LOG(1, "JPEG Hybrid Dec clear Interrupt %d ret %d"
					, irq, ret);
			break;
		}
	}

	return IRQ_HANDLED;
}

void jpeg_drv_hybrid_dec_prepare_dvfs(unsigned int id)
{
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(gJpegqDev.pDev[id]);
	if (ret < 0) {
		JPEG_LOG(0, "Failed to get opp table (%d)", ret);
		return;
	}

	gJpegqDev.jpeg_reg[id] = devm_regulator_get(gJpegqDev.pDev[id],
						"dvfsrc-vcore");
	if (gJpegqDev.jpeg_reg[id] == 0) {
		JPEG_LOG(0, "Failed to get regulator");
		return;
	}

	gJpegqDev.jpeg_freq_cnt[id] = dev_pm_opp_get_opp_count(gJpegqDev.pDev[id]);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(gJpegqDev.pDev[id], &freq))) {
		gJpegqDev.jpeg_freqs[id][i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
}

void jpeg_drv_hybrid_dec_unprepare_dvfs(void)
{
}

void jpeg_drv_hybrid_dec_start_dvfs(unsigned int id)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	if (gJpegqDev.jpeg_reg[id] != 0) {
		JPEG_LOG(1, "request freq %lu",
				gJpegqDev.jpeg_freqs[id][gJpegqDev.jpeg_freq_cnt[id]-1]);
		opp = dev_pm_opp_find_freq_ceil(gJpegqDev.pDev[id],
		&gJpegqDev.jpeg_freqs[id][gJpegqDev.jpeg_freq_cnt[id]-1]);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(gJpegqDev.jpeg_reg[id], volt, INT_MAX);
		if (ret) {
			JPEG_LOG(0 ,"Failed to set regulator voltage %d",
			volt);
		}
	}

}

void jpeg_drv_hybrid_dec_end_dvfs(unsigned int id)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	if (gJpegqDev.jpeg_reg[id] != 0) {
		JPEG_LOG(1, "request freq %lu", gJpegqDev.jpeg_freqs[id][0]);
		opp = dev_pm_opp_find_freq_ceil(gJpegqDev.pDev[id],
					&gJpegqDev.jpeg_freqs[id][0]);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(gJpegqDev.jpeg_reg[id], volt, INT_MAX);
		if (ret) {
			JPEG_LOG(0 ,"Failed to set regulator voltage %d",
			volt);
		}
	}
}

void jpeg_drv_hybrid_dec_power_on(int id)
{
	int ret;

	if (!dec_hwlocked[(id+1)%HW_CORE_NUMBER]/*&& !dec_hwlocked[(id+2)%HW_CORE_NUM]*/) {
		if (gJpegqDev.jpegLarb[0]) {
			JPEG_LOG(1, "power on larb7");
			ret = mtk_smi_larb_get(gJpegqDev.jpegLarb[0]);
			if (ret)
				JPEG_LOG(0, "mtk_smi_larb_get failed %d",
					ret);
		}
		ret = clk_prepare_enable(gJpegqDev.jpegClk.clk_venc_jpgDec);
		if (ret)
			JPEG_LOG(0, "clk MT_CG_VENC_JPGDEC failed %d",
					ret);
		ret = clk_prepare_enable(gJpegqDev.jpegClk.clk_venc_jpgDec_c1);
		if (ret)
			JPEG_LOG(0, "clk MT_CG_VENC_JPGDEC_C1 failed %d",
					ret);
	}

	if (id == 2) {
		if (gJpegqDev.jpegLarb[1]) {
			JPEG_LOG(1, "power on larb8");
			ret = mtk_smi_larb_get(gJpegqDev.jpegLarb[1]);
			if (ret)
				JPEG_LOG(0, "mtk_smi_larb_get failed %d",
					ret);
		}
		ret = clk_prepare_enable(gJpegqDev.jpegClk.clk_venc_c1_jpgDec);
		if (ret)
			JPEG_LOG(0, "clk enable MT_CG_VENC_C1_JPGDEC failed %d",
					ret);
		jpeg_drv_hybrid_dec_start_dvfs(1);
	} else
		jpeg_drv_hybrid_dec_start_dvfs(0);

	JPEG_LOG(1, "JPEG Hybrid Decoder Power On %d", id);
}

void jpeg_drv_hybrid_dec_power_off(int id)
{
	if (id == 2) {
		jpeg_drv_hybrid_dec_end_dvfs(1);
		clk_disable_unprepare(gJpegqDev.jpegClk.clk_venc_c1_jpgDec);
		if (gJpegqDev.jpegLarb[1])
			mtk_smi_larb_put(gJpegqDev.jpegLarb[1]);
	} else
		jpeg_drv_hybrid_dec_end_dvfs(0);

	if (!dec_hwlocked[(id+1)%HW_CORE_NUMBER]/* && !dec_hwlocked[(id+2)%3]*/) {
		clk_disable_unprepare(gJpegqDev.jpegClk.clk_venc_jpgDec);
		clk_disable_unprepare(gJpegqDev.jpegClk.clk_venc_jpgDec_c1);
		if (gJpegqDev.jpegLarb[0])
			mtk_smi_larb_put(gJpegqDev.jpegLarb[0]);
	}

	JPEG_LOG(1, "JPEG Hybrid Decoder Power Off %d", id);
}

static int jpeg_drv_hybrid_dec_lock(int *hwid)
{
	int retValue = 0;
	int id = 0;

	if (gJpegqDev.is_suspending) {
		JPEG_LOG(0, "jpeg dec is suspending");
		*hwid = -1;
		return -EBUSY;
	}

	mutex_lock(&jpeg_hybrid_dec_lock);
	for (id = 0; id < HW_CORE_NUMBER; id++) {
		if (dec_hwlocked[id]) {
			JPEG_LOG(1, "jpeg dec HW core %d is busy", id);
			continue;
		} else {
			*hwid = id;
			dec_hwlocked[id] = true;
			JPEG_LOG(1, "jpeg dec get %d HW core", id);
			_jpeg_hybrid_dec_int_status[id] = 0;
			jpeg_drv_hybrid_dec_power_on(id);
			enable_irq(gJpegqDev.hybriddecIrqId[id]);
			break;
		}
	}

	mutex_unlock(&jpeg_hybrid_dec_lock);
	if (id == HW_CORE_NUMBER) {
		JPEG_LOG(1, "jpeg dec HW core all busy");
		*hwid = -1;
		retValue = -EBUSY;
	}

	return retValue;
}

static void jpeg_drv_hybrid_dec_unlock(unsigned int hwid)
{
	mutex_lock(&jpeg_hybrid_dec_lock);
	if (!dec_hwlocked[hwid]) {
		JPEG_LOG(0, "try to unlock a free core %d", hwid);
	} else {
		dec_hwlocked[hwid] = false;
		JPEG_LOG(1, "jpeg dec HW core %d is unlocked", hwid);
		jpeg_drv_hybrid_dec_power_off(hwid);
		disable_irq(gJpegqDev.hybriddecIrqId[hwid]);
		jpg_dmabuf_free_iova(bufInfo[hwid].i_dbuf,
			bufInfo[hwid].i_attach,
			bufInfo[hwid].i_sgt);
		jpg_dmabuf_free_iova(bufInfo[hwid].o_dbuf,
			bufInfo[hwid].o_attach,
			bufInfo[hwid].o_sgt);
		jpg_dmabuf_put(bufInfo[hwid].i_dbuf);
		jpg_dmabuf_put(bufInfo[hwid].o_dbuf);
		bufInfo[hwid].i_dbuf = NULL;
		bufInfo[hwid].o_dbuf = NULL;
		// we manually add 1 ref count, need to put it.
	}
	mutex_unlock(&jpeg_hybrid_dec_lock);
}

static int jpeg_drv_hybrid_dec_suspend_notifier(
					struct notifier_block *nb,
					unsigned long action, void *data)
{
	int i;
	int wait_cnt = 0;

	JPEG_LOG(0, "action:%ld", action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		gJpegqDev.is_suspending = 1;
		for (i = 0 ; i < HW_CORE_NUMBER; i++) {
			JPEG_LOG(1, "jpeg dec sn wait core %d", i);
			while (dec_hwlocked[i]) {
				JPEG_LOG(1, "jpeg dec sn core %d locked. wait...", i);
				usleep_range(10000, 20000);
				wait_cnt++;
				if (wait_cnt > 5) {
					JPEG_LOG(0, "jpeg dec sn unlock core %d", i);
					jpeg_drv_hybrid_dec_unlock(i);
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

static int jpeg_drv_hybrid_dec_suspend(void)
{
	int i;

	JPEG_LOG(0, "+");
	for (i = 0 ; i < HW_CORE_NUMBER; i++) {
		JPEG_LOG(1, "jpeg dec suspend core %d", i);
		if (dec_hwlocked[i]) {
			JPEG_LOG(0, "suspend unlock core %d\n", i);
			jpeg_drv_hybrid_dec_unlock(i);
		}
	}
	return 0;
}

static int jpeg_drv_hybrid_dec_get_status(int hwid)
{
	int p_n_s;

	p_n_s = -1;
	mutex_lock(&jpeg_hybrid_dec_lock);
	if (!dec_hwlocked[hwid]) {
		JPEG_LOG(1, "hw %d unlocked, return -1 status", hwid);
	} else {
		JPEG_LOG(1, "get p_n_s @ hw %d", hwid);
		jpeg_drv_hybrid_dec_get_p_n_s(hwid, &p_n_s);
	}
	mutex_unlock(&jpeg_hybrid_dec_lock);

	return p_n_s;
}

static unsigned int jpeg_get_node_index(const char *name)
{
	if (strncmp(name, "jpgdec0", strlen("jpgdec0")) == 0) {
		JPEG_LOG(0, "name %s", name);
		return 0;
	} else if (strncmp(name, "jpgdec1", strlen("jpgdec1")) == 0) {
		JPEG_LOG(0, "name %s", name);
		return 1;
	} else if (strncmp(name, "venc_jpg", strlen("venc_jpg")) == 0) {
		JPEG_LOG(0, "name %s", name);
		return 2;
	}
	JPEG_LOG(0, "name not found %s", name);
	return 0;

}

static int jpeg_hybrid_dec_ioctl(unsigned int cmd, unsigned long arg,
			struct file *file)
{
	unsigned int *pStatus;
	int hwid;
	int index_buf_fd;
	long timeout_jiff;
	int progress_n_status;

	struct JPEG_DEC_DRV_HYBRID_TASK taskParams;
	struct JPEG_DEC_DRV_HYBRID_P_N_S pnsParmas;

	pStatus = (unsigned int *)file->private_data;

	if (pStatus == NULL) {
		JPEG_LOG
		(0, "Private data is null");
		return -EFAULT;
	}
	switch (cmd) {
	case JPEG_DEC_IOCTL_HYBRID_START:
		JPEG_LOG(1, "JPEG DEC IOCTL HYBRID START");
		if (copy_from_user(
			&taskParams, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_TASK))) {
			JPEG_LOG(0, "Copy from user error");
			return -EFAULT;
		}
		if (taskParams.timeout != 3000) // JPEG oal magic number
			return -EFAULT;
		if (jpeg_drv_hybrid_dec_lock(&hwid) == 0) {
			*pStatus = JPEG_DEC_PROCESS;
		} else {
			JPEG_LOG(1, "jpeg_drv_hybrid_dec_lock failed (hw busy)");
			return -EBUSY;
		}

		if (jpeg_drv_hybrid_dec_start(taskParams.data, hwid, &index_buf_fd) == 0) {
			JPEG_LOG(1, "jpeg_drv_hybrid_dec_start success %u index buf fd:%d", hwid, index_buf_fd);
			if (copy_to_user(
				taskParams.hwid, &hwid, sizeof(int))) {
				JPEG_LOG(0, "Copy to user error");
				return -EFAULT;
			}
			if (copy_to_user(
				taskParams.index_buf_fd, &index_buf_fd, sizeof(int))) {
				JPEG_LOG(0, "Copy to user error");
				return -EFAULT;
			}
		} else {
			JPEG_LOG(0, "jpeg_drv_dec_hybrid_start failed");
			jpeg_drv_hybrid_dec_unlock(hwid);
			return -EFAULT;
		}
		break;
	case JPEG_DEC_IOCTL_HYBRID_WAIT:
		JPEG_LOG(1,"JPEG DEC IOCTL HYBRID WAIT");

		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_LOG(0,
			"Permission Denied! This process cannot access decoder");
			return -EFAULT;
		}

		if (copy_from_user(
			&pnsParmas, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_P_N_S))) {
			JPEG_LOG(0, "Copy from user error");
			return -EFAULT;
		}

		/* set timeout */
		timeout_jiff = msecs_to_jiffies(3000);
		JPEG_LOG(1, "JPEG Hybrid Decoder Wait Resume Time: %ld",
				timeout_jiff);
		hwid = pnsParmas.hwid;
		if (hwid < 0 || hwid >= HW_CORE_NUMBER) {
			JPEG_LOG(0, "get hybrid dec id failed");
			return -EFAULT;
		}
	#ifdef FPGA_VERSION
		JPEG_LOG(1, "Polling JPEG Hybrid Dec Status hwid: %d",
				hwid);

		do {
			_jpeg_hybrid_dec_int_status[hwid] =
			IMG_REG_READ(REG_JPGDEC_HYBRID_INT_STATUS(hwid));
			JPEG_LOG(1, "Hybrid Polling status %d",
			_jpeg_hybrid_dec_int_status[hwid]);
		} while (_jpeg_hybrid_dec_int_status[hwid] == 0);

	#else
		if (!dec_hwlocked[hwid]) {
			JPEG_LOG(0, "wait on unlock core %d\n", hwid);
			return -EFAULT;
		}
		if (jpeg_isr_hybrid_dec_lisr(hwid) < 0) {
			long ret = 0;
			int waitfailcnt = 0;

			do {
				ret = wait_event_interruptible_timeout(
					hybrid_dec_wait_queue[hwid],
					_jpeg_hybrid_dec_int_status[hwid],
					timeout_jiff);
				if (ret == 0)
					JPEG_LOG(0,
					"JPEG Hybrid Dec Wait timeout!");
				if (ret < 0) {
					waitfailcnt++;
					JPEG_LOG(0,
					"JPEG Hybrid Dec Wait Error %d",
					waitfailcnt);
					usleep_range(10000, 20000);
				}
			} while (ret < 0 && waitfailcnt < 500);
		} else
			JPEG_LOG(1, "JPEG Hybrid Dec IRQ Wait Already Done!");
		_jpeg_hybrid_dec_int_status[hwid] = 0;
	#endif
		progress_n_status = jpeg_drv_hybrid_dec_get_status(hwid);
		JPEG_LOG(1, "jpeg_drv_hybrid_dec_get_status %d", progress_n_status);

		if (copy_to_user(
			pnsParmas.progress_n_status, &progress_n_status, sizeof(int))) {
			JPEG_LOG(0, "Copy to user error");
			return -EFAULT;
		}

		mutex_lock(&jpeg_hybrid_dec_lock);
		if (dec_hwlocked[hwid]) {
			IMG_REG_WRITE(0x0, REG_JPGDEC_HYBRID_090(hwid));
			IMG_REG_WRITE(0x00000010, REG_JPGDEC_HYBRID_090(hwid));
		}
		mutex_unlock(&jpeg_hybrid_dec_lock);

		jpeg_drv_hybrid_dec_unlock(hwid);
		break;
	case JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS:
		JPEG_LOG(1,"JPEG DEC IOCTL HYBRID GET PROGRESS N STATUS");

		if (*pStatus != JPEG_DEC_PROCESS) {
			JPEG_LOG(0,
			"Permission Denied! This process cannot access decoder");
			return -EFAULT;
		}

		if (copy_from_user(
			&pnsParmas, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_P_N_S))) {
			JPEG_LOG(0, "JPEG Decoder : Copy from user error");
			return -EFAULT;
		}

		hwid = pnsParmas.hwid;
		if (hwid < 0 || hwid >= HW_CORE_NUMBER) {
			JPEG_LOG(0, "get P_N_S hwid invalid");
			return -EFAULT;
		}
		progress_n_status = jpeg_drv_hybrid_dec_get_status(hwid);

		if (copy_to_user(
			pnsParmas.progress_n_status, &progress_n_status, sizeof(int))) {
			JPEG_LOG(0, "JPEG Decoder: Copy to user error");
			return -EFAULT;
		}
		break;
	default:
		JPEG_LOG(0, "JPEG DEC IOCTL NO THIS COMMAND");
		break;
	}
	return 0;
}

void jpeg_drv_enc_power_on(void)
{
	int ret;
	unsigned int node_index = 2;

	if (gJpegqDev.jpegLarb[node_index]) {
		JPEG_LOG(1, "power on larb1");
		ret = mtk_smi_larb_get(gJpegqDev.jpegLarb[node_index]);
		if (ret)
			JPEG_LOG(0, "mtk_smi_larb_get failed %d",
				ret);
	}
	ret = clk_prepare_enable(gJpegqDev.jpegClk.clk_venc_jpgEnc);
	if (ret)
		JPEG_LOG(0, "clk MT_CG_VENC_JPGENC failed %d",
				ret);
	enable_irq(gJpegqDev.encIrqId);

	JPEG_LOG(1, "JPEG Encoder Power On %d", node_index);
}

void jpeg_drv_enc_power_off(void)
{
	unsigned int node_index = 2;

	disable_irq(gJpegqDev.encIrqId);
	clk_disable_unprepare(gJpegqDev.jpegClk.clk_venc_jpgEnc);
	if (gJpegqDev.jpegLarb[2])
		mtk_smi_larb_put(gJpegqDev.jpegLarb[node_index]);
	JPEG_LOG(1, "JPEG Encoder Power Off %d", node_index);
}


static int jpeg_drv_enc_init(void)
{
	int retValue;

	mutex_lock(&jpeg_enc_power_lock);
	if (enc_status != 0) {
		JPEG_WRN("%s HW is busy\n", __func__);
		retValue = -EBUSY;
	} else {
		enc_status = 1;
		enc_ready = 0;
		retValue = 0;
	}

	if (retValue == 0) {
		jpeg_drv_enc_power_on();
		jpeg_drv_enc_verify_state_and_reset();
	}

	return retValue;
}

static void jpeg_drv_enc_deinit(void)
{
	if (enc_status != 0) {
		enc_status = 0;
		enc_ready = 0;
		jpeg_drv_enc_reset();
		jpeg_drv_enc_power_off();
		mutex_unlock(&jpeg_enc_power_lock);
	}
}
void set_jpeg_enc_bw(u64 target_bw)
{
	if (gJpegqDev.jpeg_enc_qos_req != 0) {
		mtk_icc_set_bw(gJpegqDev.jpeg_enc_qos_req, MBps_to_icc((u32)target_bw), 0);
		pr_info("[JPEG][ENC] set larb %u bw",
		target_bw);
	}
}


static int jpeg_drv_enc_flush_buff(int fd)
{
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	pr_info("%s fd: %d+\n", __func__, fd);
	if (fd <= 0) {
		pr_info("%s, fd: %d can't be <= 0, please check\n",
		__func__, fd);
		return -1;
	}
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_info("dma_buf_get fail");
		return -1;
	}
	attach = dma_buf_attach(dmabuf, gJpegqDev.pDev[2]);
	if (IS_ERR(attach)) {
		pr_info("[%s] attach fail, return\n", __func__);
		return -EFAULT;
	}
	sgt = dma_buf_map_attachment(attach, DMA_TO_DEVICE);
	if (IS_ERR(sgt)) {
		pr_info("map failed, detach and return\n");
		dma_buf_detach(dmabuf, attach);
		return -EFAULT;
	}
	dma_sync_sg_for_device(gJpegqDev.pDev[2], sgt->sgl, sgt->orig_nents, DMA_TO_DEVICE);
	dma_buf_unmap_attachment(attach, sgt, DMA_TO_DEVICE);
	dma_buf_detach(dmabuf, attach);
	pr_info("%s -\n", __func__);
	return 0;
}


static int jpeg_drv_enc_invalidate_buff(int fd)
{
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	pr_info("%s fd : %d+\n", __func__, fd);
	if (fd <= 0) {
		pr_info("%s, fd: %d can't be <= 0, please check\n",
		__func__, fd);
		return -1;
	}
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_info("dma_buf_get fail");
		return -1;
	}
	attach = dma_buf_attach(dmabuf, gJpegqDev.pDev[2]);
	if (IS_ERR(attach)) {
		pr_info("[%s] attach fail, return\n", __func__);
		return -EFAULT;
	}
	sgt = dma_buf_map_attachment(attach, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_info("map failed, detach and return\n");
		dma_buf_detach(dmabuf, attach);
		return -EFAULT;
	}
	dma_sync_sg_for_device(gJpegqDev.pDev[2], sgt->sgl, sgt->orig_nents, DMA_FROM_DEVICE);
	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
	dma_buf_detach(dmabuf, attach);
	pr_info("%s -\n", __func__);

	return 0;
}

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

	unsigned int fd;

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

		if (!strcmp(MTK_PLATFORM_MT6765, platform_string)) {
			/* on mt6765, 16MP = 14.5 FPS */
			cshot_spec = 232;
		} else if (!strcmp(MTK_PLATFORM_MT6761, platform_string)) {
			/* on mt6761, lpddr4: 26MP = 5 FPS , lpddr3: 26MP = 1 FPS*/
			cshot_spec = 26;
		}

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
		//mtk_pm_qos_update_request(&jpgenc_qos_request, emi_bw);
		set_jpeg_enc_bw(emi_bw);

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
		    jpeg_drv_enc_set_src_buf(gJpegqDev.pDev[2], cfgEnc.encFormat,
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
		    jpeg_drv_enc_set_dst_buff(gJpegqDev.pDev[2], cfgEnc.dstFd,
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

		enc_ready = 1;
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
			} while (0);
		} else
			JPEG_MSG("[JPGDRV]Enc already done !!\n");

		/* Support QoS: remove BW request after HW done */
		//mtk_pm_qos_update_request(&jpgenc_qos_request, 0);
		set_jpeg_enc_bw(0);
		ret = jpeg_drv_enc_get_result(&file_size);

		JPEG_MSG("[JPEGDRV]Result : %d, Size : %u!!\n", ret, file_size);
		if (ret != 0) {
			jpeg_drv_enc_dump_reg();
			if (ret == 3)
				mtk_smi_dbg_hang_detect("JPEG");

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

	case JPEG_ENC_IOCTL_FLUSH_BUFF:
				/* copy input parameters */
		if (copy_from_user(&fd, (void *)arg,
				 sizeof(unsigned int))) {
			JPEG_MSG("[JPGDRV]Encoder : Copy from user error\n");
			return -EFAULT;
		}
		jpeg_drv_enc_flush_buff(fd);
		break;

	case JPEG_ENC_IOCTL_INVALIDATE_BUFF:
		if (copy_from_user(&fd, (void *)arg,
				 sizeof(unsigned int))) {
			JPEG_MSG("[JPGDRV]Encoder : Copy from user error\n");
			return -EFAULT;
		}
		jpeg_drv_enc_invalidate_buff(fd);
		break;


	default:
		JPEG_MSG("[JPEGDRV]JPEG ENC IOCTL NO THIS COMMAND\n");

	}
	return 0;
}



#if IS_ENABLED(CONFIG_COMPAT)
static int compat_get_jpeg_hybrid_task_data(
		 struct compat_JPEG_DEC_DRV_HYBRID_TASK __user *data32,
		 struct JPEG_DEC_DRV_HYBRID_TASK __user *data)
{
	compat_long_t timeout;
	compat_uptr_t hwid;
	compat_uptr_t index_buf_fd;
	int err, i;
	unsigned int temp;

	err = get_user(timeout, &data32->timeout);
	err |= put_user(timeout, &data->timeout);
	err |= get_user(hwid, &data32->hwid);
	err |= put_user(compat_ptr(hwid), &data->hwid);
	err |= get_user(index_buf_fd, &data32->index_buf_fd);
	err |= put_user(compat_ptr(index_buf_fd), &data->index_buf_fd);

	for (i = 0; i < 21; i++) {
		err |= get_user(temp, &data32->data[i]);
		err |= put_user(temp, &data->data[i]);
	}

	return err;
}

static int compat_get_jpeg_hybrid_pns_data(
		 struct compat_JPEG_DEC_DRV_HYBRID_P_N_S __user *data32,
		 struct JPEG_DEC_DRV_HYBRID_P_N_S __user *data)
{
	int hwid;
	compat_uptr_t progress_n_status;
	int err;

	err = get_user(hwid, &data32->hwid);
	err |= put_user(hwid, &data->hwid);
	err |= get_user(progress_n_status, &data32->progress_n_status);
	err |= put_user(compat_ptr(progress_n_status), &data->progress_n_status);

	return err;
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
	case COMPAT_JPEG_DEC_IOCTL_HYBRID_START:
		{
			struct compat_JPEG_DEC_DRV_HYBRID_TASK __user *data32;
			struct JPEG_DEC_DRV_HYBRID_TASK __user *data;
			int err;

			JPEG_LOG(1, "COMPAT_JPEG_DEC_IOCTL_HYBRID_START");
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;
			err = compat_get_jpeg_hybrid_task_data(data32, data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_HYBRID_START,
					(unsigned long)data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT:
		{
			struct compat_JPEG_DEC_DRV_HYBRID_P_N_S __user *data32;
			struct JPEG_DEC_DRV_HYBRID_P_N_S __user *data;
			int err;

			JPEG_LOG(1, "COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT");
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;
			err = compat_get_jpeg_hybrid_pns_data(data32, data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(
					filp, JPEG_DEC_IOCTL_HYBRID_WAIT,
					(unsigned long)data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS:
		{
			struct compat_JPEG_DEC_DRV_HYBRID_P_N_S __user *data32;
			struct JPEG_DEC_DRV_HYBRID_P_N_S __user *data;
			int err;

			JPEG_LOG(1, "COMPAT_JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS");
			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;
			err = compat_get_jpeg_hybrid_pns_data(data32, data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS,
					(unsigned long)data);
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
	case JPEG_ENC_IOCTL_INIT:
	case JPEG_ENC_IOCTL_DEINIT:
	case JPEG_ENC_IOCTL_START:
	case JPEG_ENC_IOCTL_FLUSH_BUFF:
	case JPEG_ENC_IOCTL_INVALIDATE_BUFF:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);

	case JPEG_ENC_IOCTL_CONFIG:
		return filp->f_op->unlocked_ioctl(
			filp, cmd,
			 (unsigned long)compat_ptr(arg));
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
	case JPEG_DEC_IOCTL_HYBRID_START:
	case JPEG_DEC_IOCTL_HYBRID_WAIT:
	case JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS:
		return jpeg_hybrid_dec_ioctl(cmd, arg, file);

	case JPEG_ENC_IOCTL_INIT:
	case JPEG_ENC_IOCTL_CONFIG:
	case JPEG_ENC_IOCTL_WAIT:
	case JPEG_ENC_IOCTL_DEINIT:
	case JPEG_ENC_IOCTL_START:
	case JPEG_ENC_IOCTL_FLUSH_BUFF:
	case JPEG_ENC_IOCTL_INVALIDATE_BUFF:
		return jpeg_enc_ioctl(cmd, arg, file);
	default:
		break;
	}
	return -EINVAL;
}

static int jpeg_open(struct inode *inode, struct file *file)
{
	unsigned int *pStatus;

	/* Allocate and initialize private data */
	 file->private_data = kmalloc(sizeof(unsigned int), GFP_ATOMIC);

	if (file->private_data == NULL) {
		JPEG_LOG(0, "Not enough entry for JPEG open operation");
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
	JPEG_LOG(1, "jpeg driver read");
	return 0;
}

static int jpeg_release(struct inode *inode, struct file *file)
{
	if (file->private_data != NULL) {
		kfree(file->private_data);
		file->private_data = NULL;
	}
	return 0;
}

/* Kernel interface */
static const struct proc_ops jpeg_fops = {
	.proc_ioctl = jpeg_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = compat_jpeg_ioctl,
#endif
	.proc_open = jpeg_open,
	.proc_release = jpeg_release,
	.proc_read = jpeg_read,
};


const long jpeg_dev_get_encoder_base_VA(void)
{
	return gJpegqDev.encRegBaseVA;
}

const long jpeg_dev_get_hybrid_decoder_base_VA(int id)
{
	return gJpegqDev.hybriddecRegBaseVA[id];
}


static irqreturn_t jpeg_drv_enc_isr(int irq, void *dev_id)
{

	/* JPEG_MSG("JPEG Encoder Interrupt\n"); */
	if (enc_status == 0) {
		JPEG_LOG(0, "interrupt without power on");
		return IRQ_HANDLED;
	}

	if (irq == gJpegqDev.encIrqId) {
		if (jpeg_isr_enc_lisr() == 0)
			wake_up_interruptible(&enc_wait_queue);
	}

	return IRQ_HANDLED;

}

static void jpeg_drv_prepare_bw_request(int id)
{
	int i, ret;
	u32 port_num = 0;
	const char *path_strs[32];

	pr_info("%s id : %d", __func__, id);
	if (id == 2) {
		gJpegqDev.jpeg_enc_qos_req = 0;
		ret = of_property_read_u32(gJpegqDev.pDev[id]->of_node,
			"interconnect-num", &port_num);
		if (ret) {
			pr_info("[JPEG ENC] Cannot get interconnect num, skip");
			return;
		}
		ret = of_property_read_string_array(gJpegqDev.pDev[id]->of_node,
			"interconnect-names", path_strs, port_num);
		if (ret < 0) {
			pr_info("[JPEG ENC] Cannot get interconnect names, skip");
			return;
		} else if (ret != (int)port_num) {
			pr_info("[JPEG ENC] Interconnect name count not match %u %d",
				port_num, ret);
		}
		for (i = 0; i < port_num; i++) {
			gJpegqDev.jpeg_enc_qos_req =
				of_mtk_icc_get(gJpegqDev.pDev[id], path_strs[i]);
			pr_info("[JPEG ENC] qos port[%d] name %s gJpegqDev.jpeg_enc_qos_req : %p",
				i, path_strs[i], gJpegqDev.jpeg_enc_qos_req);
		}
	}
}


static int jpeg_probe(struct platform_device *pdev)
{
	struct device_node *node = NULL, *larbnode = NULL;
	struct platform_device *larbdev;
	int i, node_index, ret;

	JPEG_LOG(0, "JPEG Probe");
	atomic_inc(&nodeCount);

	for (i = 0; i < HW_CORE_NUMBER; i++) {
		bufInfo[i].o_dbuf = NULL;
		bufInfo[i].o_attach = NULL;
		bufInfo[i].o_sgt = NULL;

		bufInfo[i].i_dbuf = NULL;
		bufInfo[i].i_attach = NULL;
		bufInfo[i].i_sgt = NULL;
		JPEG_LOG(1, "initializing io dma buf for core id: %d", i);
	}

	node_index = jpeg_get_node_index(pdev->dev.of_node->name);
	dev_info(&pdev->dev, "node\n");
	pr_info("node_index : %d", node_index);
	if (atomic_read(&nodeCount) == 1)
		memset(&gJpegqDev, 0x0, sizeof(struct JpegDeviceStruct));
	gJpegqDev.pDev[node_index] = &pdev->dev;

	node = pdev->dev.of_node;

	if (node_index == 0) {
		for (i = 0; i < HW_CORE_NUMBER; i++) {
			gJpegqDev.hybriddecRegBaseVA[i] =
				(unsigned long)of_iomap(node, i);

			gJpegqDev.hybriddecIrqId[i] = irq_of_parse_and_map(node, i);
			JPEG_LOG(0, "Jpeg Hybrid Dec Probe %d base va 0x%lx irqid %d",
				i, gJpegqDev.hybriddecRegBaseVA[i],
				gJpegqDev.hybriddecIrqId[i]);

			JPEG_LOG(0, "Request irq %d", gJpegqDev.hybriddecIrqId[i]);
			init_waitqueue_head(&(hybrid_dec_wait_queue[i]));
			if (request_irq(gJpegqDev.hybriddecIrqId[i],
				jpeg_drv_hybrid_dec_isr, IRQF_TRIGGER_HIGH,
				"jpeg_dec_driver", NULL))
					JPEG_LOG(0, "JPEG Hybrid DEC requestirq %d failed", i);
			disable_irq(gJpegqDev.hybriddecIrqId[i]);
		}
		gJpegqDev.jpegClk.clk_venc_jpgDec =
			of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC");
		if (IS_ERR(gJpegqDev.jpegClk.clk_venc_jpgDec))
			JPEG_LOG(0, "get MT_CG_VENC_JPGDEC clk error!");

		gJpegqDev.jpegClk.clk_venc_jpgDec_c1 =
			of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC_C1");
		if (IS_ERR(gJpegqDev.jpegClk.clk_venc_jpgDec_c1))
			JPEG_LOG(0, "get MT_CG_VENC_JPGDEC_C1 clk error!");
	} else if (node_index == 2) {
		pr_info("node_index : %d", node_index);
		ret = of_property_read_string(node, "mediatek,platform", &platform_string);
		if (ret < 0)
			pr_info("[ERROR][ENC] platform entry missing in jpeg enc node\n");
		gJpegqDev.encRegBaseVA = (unsigned long)of_iomap(node, 0);
		gJpegqDev.encIrqId = irq_of_parse_and_map(node, 0);
		init_waitqueue_head(&enc_wait_queue);
		JPEG_LOG(0, "request JPEG Encoder IRQ\n");
		if (request_irq(gJpegqDev.encIrqId, jpeg_drv_enc_isr, IRQF_TRIGGER_HIGH,
			"jpeg_enc_driver", gJpegqDev.pDev[node_index]))
			JPEG_LOG(0, "JPEG ENC Driver request irq failed\n");
		disable_irq(gJpegqDev.encIrqId);
		gJpegqDev.jpegClk.clk_venc_jpgEnc =
			 of_clk_get_by_name(node, "MT_CG_VENC_JPGENC");
		if (IS_ERR(gJpegqDev.jpegClk.clk_venc_jpgEnc))
			JPEG_LOG(0, "get MT_CG_VENC_JPGENC clk error!");
	}

	larbnode = of_parse_phandle(node, "mediatek,larbs", 0);
	if (!larbnode) {
		JPEG_LOG(0, "fail to get larbnode %d", node_index);
		return -1;
	}
	larbdev = of_find_device_by_node(larbnode);
	if (WARN_ON(!larbdev)) {
		of_node_put(larbnode);
		JPEG_LOG(0, "fail to get larbdev %d", node_index);
		return -1;
	}
	gJpegqDev.jpegLarb[node_index] = &larbdev->dev;
	JPEG_LOG(0, "get larb from node gJpegqDev.jpegLarb[%d] =%p ",
		node_index, gJpegqDev.jpegLarb[node_index]);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret) {
		JPEG_LOG(0, "64-bit DMA enable failed");
		return ret;
	}
	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
		devm_kzalloc(&pdev->dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(&pdev->dev, (unsigned int)DMA_BIT_MASK(34));
		if (ret)
			JPEG_LOG(0, "Failed to set DMA segment size\n");
	}
	pm_runtime_enable(&pdev->dev);
	if (node_index == 1 || node_index == 0)
		jpeg_drv_hybrid_dec_prepare_dvfs(node_index);
	if (node_index == 2) {
		pr_info("node_index : %d", node_index);
		enc_status = 0;
		_jpeg_enc_int_status = 0;
		jpeg_drv_prepare_bw_request(node_index);
	}
	if (atomic_read(&nodeCount) == 1) {
		if (node_index == 1 || node_index == 0) {
			gJpegqDev.pm_notifier.notifier_call = jpeg_drv_hybrid_dec_suspend_notifier;
			register_pm_notifier(&gJpegqDev.pm_notifier);
			gJpegqDev.is_suspending = 0;
			memset(_jpeg_hybrid_dec_int_status, 0, HW_CORE_NUMBER);
		}
		proc_create("mtk_jpeg", 0x644, NULL, &jpeg_fops);
	}

	JPEG_LOG(0, "JPEG Probe Done");

	return 0;
}

static int jpeg_remove(struct platform_device *pdev)
{
	int i, node_index;

	JPEG_LOG(0, "JPEG Codec remove");
	atomic_dec(&nodeCount);

	node_index = jpeg_get_node_index(pdev->dev.of_node->name);

	if (node_index == 0)
		for (i = 0; i < HW_CORE_NUMBER - 1; i++)
			free_irq(gJpegqDev.hybriddecIrqId[i], NULL);
	else {
		i = HW_CORE_NUMBER - 1;
		free_irq(gJpegqDev.hybriddecIrqId[i], NULL);
	}
	jpeg_drv_hybrid_dec_unprepare_dvfs();

	return 0;
}

static void jpeg_shutdown(struct platform_device *pdev)
{
	JPEG_LOG(0, "JPEG Codec shutdown");
}

/* PM suspend */
static int jpeg_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	int ret;

	ret = jpeg_drv_hybrid_dec_suspend();
	if (ret != 0)
		return ret;
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
		.of_match_table = jpeg_of_ids,
		},
};

static int __init jpeg_init(void)
{
	int ret;

	JPEG_LOG(0, "Register the JPEG Codec driver");
	atomic_set(&nodeCount, 0);
	if (platform_driver_register(&jpeg_driver)) {
		JPEG_LOG(0, "failed to register jpeg codec driver");
		ret = -ENODEV;
		return ret;
	}

	return 0;
}

static void __exit jpeg_exit(void)
{
	remove_proc_entry("mtk_jpeg", NULL);
	platform_driver_unregister(&jpeg_driver);
}
module_init(jpeg_init);
module_exit(jpeg_exit);
MODULE_AUTHOR("Jason Hsu <yeong-cherng.hsu@mediatek.com>");
MODULE_DESCRIPTION("JPEG Dec Codec Driver");
MODULE_LICENSE("GPL");
