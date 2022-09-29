// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <mtk_heap.h>
#include <linux/delay.h>
#include <linux/rpmsg.h>

#include <gz-trusty/smcall.h>

#include "sapu_driver.h"

struct apusys_sapu_data *get_apusys_sapu_data(struct file *filep)
{
	struct apusys_sapu_data *data;
	struct miscdevice *miscdev = filep->private_data;

	if (filep->private_data == NULL) {
		pr_info("filep->private_data == NULL\n");
		return NULL;
	}

	data = container_of(miscdev, struct apusys_sapu_data, mdev);
	return data;
}

static void free_ref_cnt(struct kref *kref)
{
	pr_info("%s : kref ref_count reset\n", __func__);
	kref_init(kref);
}

static int sapu_lock_ipi_send(uint32_t lock, struct kref *ref_cnt)
{
	int i, ret = 0, retry_cnt = 50;
	unsigned int sapu_lock_ref_cnt;
	struct PWRarg data;
	struct sapu_lock_rpmsg_device *sapu_lock_rpm_dev;
	unsigned long long jiffies_start = 0, jiffies_end = 0;

	sapu_lock_rpm_dev = get_rpm_dev();

	if (!sapu_lock_rpm_dev->ept) {
		pr_info("%s: sapu_lock_rpm_dev.ept == NULL\n", __func__);
		return -ENXIO;
	}

	mutex_lock(get_rpm_mtx());

	if (lock) {
		kref_get(ref_cnt);
	} else {
		if (kref_put(ref_cnt, free_ref_cnt)) {
			ret = -EPERM;
			goto unlock_and_ret;
		}
	}

	sapu_lock_ref_cnt = kref_read(ref_cnt);
	pr_info("%s: lock = %d, sapu_lock_ref_cnt = %d\n",
			__func__, lock, sapu_lock_ref_cnt);

	/**
	 * sapu_lock_ref_cnt = 0 : X
	 * sapu_lock_ref_cnt = 1 : power unlock
	 * sapu_lock_ref_cnt = 2 : power lock
	 */
	if ((lock == 0 && sapu_lock_ref_cnt == 1)
		|| (lock != 0 && sapu_lock_ref_cnt == 2)) {

		data.lock = sapu_lock_ref_cnt - 1;
		pr_info("%s: data.lock = %d\n", __func__, data.lock);

		for (i = 0; i < retry_cnt; i++) {
			ret = rpmsg_send(sapu_lock_rpm_dev->ept,
							&data, sizeof(data));

			/* send busy, retry */
			if (ret == -EBUSY || ret == -EAGAIN) {
				pr_info("%s: re-send ipi(retry_cnt = %d)\n",
							__func__, retry_cnt);
				if (ret == -EBUSY)
					usleep_range(10000, 11000);
				else if (ret == -EAGAIN && i < 10)
					usleep_range(200, 500);
				else if (ret == -EAGAIN && i < 25)
					usleep_range(1000, 2000);
				else if (ret == -EAGAIN)
					usleep_range(10000, 11000);
				continue;
			}
			break;
		}

		/* only need to wait when ipi send success */
		if (ret == 0) {
			/* wait for receiving ack
			 * to ensure uP clear irq status done
			 */
			jiffies_start = get_jiffies_64();

			ret = wait_for_completion_timeout(
					&sapu_lock_rpm_dev->ack,
					msecs_to_jiffies(10000));

			jiffies_end = get_jiffies_64();

			if (likely(jiffies_end > jiffies_start)) {
				if (unlikely((jiffies_end-jiffies_start) > msecs_to_jiffies(100)))
					pr_info("completion ack is overtime => %u ms\n",
						jiffies_to_msecs(jiffies_end-jiffies_start));
			} else {
				pr_info("tick timer's value overflow\n");
			}

			if (ret == 0) {
				pr_info("%s: wait for completion timeout\n",
						__func__);
				ret = -EBUSY;
			} else {
				ret = 0;
			}
		}
	}

unlock_and_ret:
	mutex_unlock(get_rpm_mtx());
	return ret;
}

int sapu_ha_bridge(struct dmArg *ioDmArg, struct haArg *ioHaArg)
{
	union MTEEC_PARAM p[4];
	TZ_RESULT ret;

	KREE_SESSION_HANDLE ha_sn = 0;

	pr_info("[SAPU_LOG] KREE_CreateSession\n");
	ret = KREE_CreateSession(ioDmArg->haSrvName, &ha_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]CreateSession ha_sn fail(%d)\n", __func__, ret);
		return -EPIPE;
	}

	p[0].mem.size = sizeof(*ioHaArg);
	p[0].mem.buffer = (void *)ioHaArg;

	pr_info("[SAPU_LOG] KREE_TeeServiceCall\n");
	ret = KREE_TeeServiceCall(ha_sn, ioDmArg->command,
						TZ_ParamTypes2(TZPT_MEM_INPUT,
						TZPT_VALUE_OUTPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]TeeServiceCall fail(%d)\n", __func__, ret);
		return -EPIPE;
	}

	// return code
	pr_info("[SAPU_LOG] p[1].value.a = %x", p[1].value.a);

	pr_info("[SAPU_LOG] KREE_CloseSession\n");
	ret = KREE_CloseSession(ha_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]ha_sn(0x%x) CloseSession fail(%d)\n",
					__func__, ha_sn, ret);
		return -EPIPE;
	}

	return 0;
}

long apusys_sapu_internal_ioctl(struct file *filep, unsigned int cmd, void __user *arg,
	unsigned int compat)
{
	int ret;
	struct apusys_sapu_data *data;

	struct dma_buf *dmem_dmabuf = NULL;
	struct dma_buf_attachment *dmem_attach;
	struct device *dmem_device;
	struct sg_table *dmem_sgt;
	struct platform_device *pdev;

	struct dmArg ioDmArg;
	struct haArg ioHaArg;
	struct PWRarg ioPWRarg;

	(void)compat;

	data = get_apusys_sapu_data(filep);
	if (data == NULL) {
		pr_info("%s %d\n", __func__, __LINE__);
		return 0;
	}

	if (_IOC_TYPE(cmd) != APUSYS_SAPU_IOC_MAGIC) {
		pr_info("%s %d -EINVAL\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
	case APUSYS_SAPU_DATAMEM:
		pr_info("%s APUSYS_SAPU_DATAMEM 0x%x\n", __func__, cmd);
		ret = copy_from_user(&ioDmArg, arg, sizeof(ioDmArg));
		if (ret) {
			pr_info("[%s]copy_from_user fail(0x%x)\n",
					__func__, ret);
			return ret;
		}
		// pr_info("%s fd=%d\n", __func__, ioDmArg.fd);

		/* get handle */
		dmem_dmabuf = dma_buf_get(ioDmArg.fd);
		if (!dmem_dmabuf || IS_ERR(dmem_dmabuf)) {
			pr_info("dma_buf_get error %d\n", __LINE__);
			return -EINVAL;
		}

		ioHaArg.handle = dmabuf_to_secure_handle(dmem_dmabuf);
		if (!ioHaArg.handle) {
			pr_info("dmabuf_to_secure_handle failed!\n");
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		/* get iova */
		pdev = data->pdev;
		if (pdev == NULL) {
			pr_info("%s %d\n", __func__, __LINE__);
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		dmem_device = &pdev->dev;
		if (dmem_device == NULL) {
			pr_info("%s %d\n", __func__, __LINE__);
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		dmem_attach = dma_buf_attach(dmem_dmabuf, dmem_device);
		if (IS_ERR(dmem_attach)) {
			pr_info("dmem_attach fail\n");
			ret = -EINVAL;
			goto datamem_dmabuf_put;
		}

		dmem_sgt = dma_buf_map_attachment(dmem_attach,
					DMA_BIDIRECTIONAL);
		if (IS_ERR(dmem_sgt)) {
			pr_info("map failed, detach and return\n");
			ret = -EINVAL;
			goto datamem_dmabuf_detach;
		}

		ioHaArg.dma_addr = sg_dma_address(dmem_sgt->sgl);
		// pr_info("dma_addr=%xad\n", ioHaArg.dma_addr);

		ioHaArg.model_hd_ha = ioDmArg.model_hd_ha;
		/* Call to HA with params */
		ret = sapu_ha_bridge(&ioDmArg, &ioHaArg);
		if (ret)
			pr_info("call to HA failed, ret = %d\n", ret);

		dma_buf_unmap_attachment(dmem_attach,
					dmem_sgt, DMA_BIDIRECTIONAL);
datamem_dmabuf_detach:
		dma_buf_detach(dmem_dmabuf, dmem_attach);
datamem_dmabuf_put:
		dma_buf_put(dmem_dmabuf);
	break;

	case APUSYS_POWER_CONTROL:
		pr_info("%s APUSYS_POWER_CONTROL 0x%x\n", __func__, cmd);
		ret = copy_from_user(&ioPWRarg, arg, sizeof(ioPWRarg));
		if (ret) {
			pr_info("[%s]copy_from_user fail(0x%x)\n",
						__func__, ret);
			return ret;
		}

		pdev = data->pdev;

		/**
		 * lock : ipi_send first, then smc to change state
		 * unlock : smc to change state first, then ipi_send
		 */
		if (ioPWRarg.lock > 0) {
			ret = sapu_lock_ipi_send(1, &data->lock_ref_cnt);
			if (ret) {
				pr_info("[%s]sapu_lock_ipi_send return(0x%x)\n",
								__func__, ret);
				return ret;
			}

			ret = trusty_std_call32(pdev->dev.parent,
					MTEE_SMCNR(MT_SMCF_SC_VPU,
					pdev->dev.parent),
					0, 1, 0);
			if (ret) {
				pr_info("[%s]trusty_std_call32 fail(0x%x), reset the power lock\n",
					 __func__, ret);

				ret = sapu_lock_ipi_send(0, &data->lock_ref_cnt);
				if (ret) {
					pr_info("[%s]sapu_lock_ipi_send return(0x%x), reset failed\n",
						 __func__, ret);
				}
				return ret;
			}
		} else {
			ret = trusty_std_call32(pdev->dev.parent,
					MTEE_SMCNR(MT_SMCF_SC_VPU,
					pdev->dev.parent),
					0, 0, 0);
			if (ret) {
				pr_info("[%s]trusty_std_call32 fail(0x%x)\n",
					__func__, ret);
				return ret;
			}

			ret = sapu_lock_ipi_send(0, &data->lock_ref_cnt);
			if (ret) {
				pr_info("[%s]sapu_lock_ipi_send return(0x%x)\n",
					__func__, ret);
				return ret;
			}
		}
	break;

	default:
		pr_info("%s unknown 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	break;
	}

	return ret;
}

