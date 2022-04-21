// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <teei_id.h>
#include "tz_dcih.h"
#include <ut_drv.h>

#define IMSG_TAG "[tz_dcih]"
#include <imsg_log.h>

static LIST_HEAD(dcih_register_list);


static struct dcih_reg_info *find_dcih_reg_info(unsigned int driver_id)
{
	struct dcih_reg_info *info;

	list_for_each_entry(info, &dcih_register_list, list) {
		if (info->drv_info->driver_id == driver_id)
			return info;
	}

	return NULL;
}

static int register_share_buffer_to_driver(struct dcih_reg_info *info)
{
	struct TEEC_Operation op = {0};
	unsigned int returnOrigin;
	int res;
	struct ut_drv_entry *ut_drv = info->drv_info;
	struct ut_drv_param driver_param;

	driver_param.cmd_id = UT_DRV_REGISTER_DCI_BUFFER;
	driver_param.u.reg_dci_buf.phy_addr = info->phy_addr;
	driver_param.u.reg_dci_buf.buf_size = info->buf_size;

	IMSG_DEBUG("register dci buffer, phy_addr 0x%lx size 0x%x\n",
			info->phy_addr, info->buf_size);

	prepare_params(&op, (void *)&driver_param, sizeof(struct ut_drv_param));

	res = TEEC_InvokeCommand(&ut_drv->session, driver_param.cmd_id,
				&op, &returnOrigin);
	if (res != TEEC_SUCCESS) {
		int ret = get_result(&op);

		IMSG_ERROR("failed to register dci buffer, res 0x%x ret 0x%x\n",
				 res, ret);
		return ret;
	}

	return 0;
}

static int noitfy_driver_to_free_share_buffer(struct dcih_reg_info *info)
{
	struct TEEC_Operation op = {0};
	unsigned int returnOrigin;
	int res;
	struct ut_drv_entry *ut_drv = info->drv_info;
	struct ut_drv_param driver_param;

	driver_param.cmd_id = UT_DRV_FREE_DCI_BUFFER;
	driver_param.u.reg_dci_buf.phy_addr = info->phy_addr;
	driver_param.u.reg_dci_buf.buf_size = info->buf_size;

	IMSG_DEBUG("free dci buffer, phy_addr 0x%lx size 0x%x\n",
			info->phy_addr, info->buf_size);

	prepare_params(&op, (void *)&driver_param, sizeof(struct ut_drv_param));

	res = TEEC_InvokeCommand(&ut_drv->session, driver_param.cmd_id,
				&op, &returnOrigin);
	if (res != TEEC_SUCCESS) {
		int ret = get_result(&op);

		IMSG_ERROR("failed to free dci buffer, res 0x%x ret 0x%x\n",
				res, ret);
		return ret;
	}

	return 0;
}

int tz_create_share_buffer(unsigned int driver_id, unsigned int buff_size)
{
	struct dcih_reg_info *info;
	struct ut_drv_entry *drv_info;
	unsigned long tmp_addr;
	int ret;

	if (buff_size > MAX_DCIH_BUF_SIZE) {
		IMSG_ERROR("buffer size too large!\n");
		return -EINVAL;
	}

	info = find_dcih_reg_info(driver_id);
	if (info) {
		IMSG_INFO("share buffer already exist, driver_id 0x%x\n",
				driver_id);
		return 0;
	}

	drv_info = find_ut_drv_entry_by_driver_id(driver_id);
	if (!drv_info) {
		IMSG_ERROR("driver (0x%x) does not exist\n", driver_id);
		return -EINVAL;
	}

	info = kzalloc(sizeof(struct dcih_reg_info), GFP_KERNEL);
	if (!info) {
		IMSG_ERROR("failed to allocate memory for dcih_reg_info\n");
		return -ENOMEM;
	}

#ifdef UT_DMA_ZONE
	tmp_addr = __get_free_pages(GFP_KERNEL | GFP_DMA,
					get_order(ROUND_UP(buff_size, SZ_4K)));
#else
	tmp_addr = __get_free_pages(GFP_KERNEL,
					get_order(ROUND_UP(buff_size, SZ_4K)));
#endif
	if ((void *)tmp_addr == NULL) {
		IMSG_ERROR("failed to get free pages from linux kernel\n");
		ret = -ENOMEM;
		goto fail;
	}

	info->drv_info = drv_info;
	/* will dynamically assign this value by dcih behavior */
	info->mode = DCIH_MODE_INVALID;
	info->virt_addr = tmp_addr;
	info->phy_addr = virt_to_phys((void *)tmp_addr);
	info->buf_size = buff_size;
	init_completion(&info->wait_notify);
	init_completion(&info->wait_result);

	ret = register_share_buffer_to_driver(info);
	if (ret < 0)
		goto fail;

	list_add_tail(&info->list, &dcih_register_list);

	return 0;

fail:
	if (tmp_addr)
		free_pages(tmp_addr, get_order(ROUND_UP(buff_size, SZ_4K)));

	kfree(info);
	return ret;
}

int tz_free_share_buffer(unsigned int driver_id)
{
	struct dcih_reg_info *info;
	int ret;

	info = find_dcih_reg_info(driver_id);
	if (!info) {
		IMSG_ERROR("driver_id (0x%x) does not exist\n", driver_id);
		return -EINVAL;
	}

	ret = noitfy_driver_to_free_share_buffer(info);
	if (ret < 0)
		return ret;

	if (info->virt_addr)
		free_pages(info->virt_addr,
				get_order(ROUND_UP(info->buf_size, SZ_4K)));

	list_del(&info->list);
	kfree(info);

	return 0;
}

unsigned long tz_get_share_buffer(unsigned int driver_id)
{
	struct dcih_reg_info *info;

	info = find_dcih_reg_info(driver_id);
	if (!info) {
		IMSG_ERROR("driver_id (0x%x) does not exist\n", driver_id);
		return (unsigned long)NULL;
	}

	return info->virt_addr;
}

int tz_notify_driver(unsigned int driver_id)
{
	struct dcih_reg_info *info;

	info = find_dcih_reg_info(driver_id);
	if (!info) {
		IMSG_ERROR("driver_id (0x%x) does not exist\n", driver_id);
		return -EINVAL;
	}

	/*
	 * If DCI handler call tz_notify_driver()
	 * before tz_wait_for_notification(),
	 * we will treat it as the master of the DCI channel.
	 */
	if (info->mode == DCIH_MODE_INVALID)
		info->mode = DCIH_MODE_MASTER;

	if (info->mode == DCIH_MODE_MASTER) {
		struct TEEC_Operation op = {0};
		unsigned int returnOrigin;
		struct ut_drv_entry *ut_drv = info->drv_info;
		struct ut_drv_param driver_param;
		int res;

		driver_param.cmd_id = UT_DRV_NOTIFY_FROM_REE;
		/* TODO: parameter 'notify_from_ree' does not support yet */
		driver_param.u.notify_from_ree.cmd = 0;
		driver_param.u.notify_from_ree.data = 0;

		prepare_params(&op, (void *)&driver_param,
						sizeof(struct ut_drv_param));

		IMSG_DEBUG("dci master: start notify TEE driver\n");
		res = TEEC_InvokeCommand(&ut_drv->session, driver_param.cmd_id,
					&op, &returnOrigin);
		if (res != TEEC_SUCCESS) {
			int ret = get_result(&op);

			IMSG_ERROR("failed to notify TEE driver\n");
			IMSG_ERROR("(0x%x)res 0x%x ret 0x%x\n",
						driver_id, res, ret);
			return ret;
		}
	} else {
		IMSG_DEBUG("dci slave: handler is done");
		complete(&info->wait_result);
	}

	return 0;
}

int tz_wait_for_notification(unsigned int driver_id)
{
	struct dcih_reg_info *info;
	int ret;

	info = find_dcih_reg_info(driver_id);
	if (!info) {
		IMSG_ERROR("driver_id (0x%x) does not exist\n", driver_id);
		return -EINVAL;
	}

	IMSG_DEBUG("wait for notify by TEE driver\n");
	/*
	 * If DCI handler call tz_wait_for_notification()
	 * before tz_notify_driver(),
	 * we will treat it as the slave of the DCI channel.
	 */
	if (info->mode == DCIH_MODE_INVALID)
		info->mode = DCIH_MODE_SLAVE;

	if (info->mode == DCIH_MODE_SLAVE) {
		IMSG_DEBUG("dci slave: wait for TEE driver notification\n");
		ret = wait_for_completion_interruptible(&info->wait_notify);
		if (ret) {
			IMSG_INFO("interrupted by signal\n");
			return ret;
		}

		IMSG_DEBUG("dci slave:received notification from TEE driver\n");
		return 0;
	}

	IMSG_ERROR("dci master: no need to wait for notification\n");
	return -EINVAL;
}

int tz_notify_ree_handler(unsigned int driver_id)
{
	struct dcih_reg_info *info;
	int ret;

	info = find_dcih_reg_info(driver_id);
	if (!info) {
		IMSG_ERROR("driver_id (0x%x) does not exist\n", driver_id);
		return -EINVAL;
	}

	complete(&info->wait_notify);

	ret = wait_for_completion_interruptible_timeout(&info->wait_result,
			msecs_to_jiffies(5000));
	if (ret == -ERESTARTSYS) {
		IMSG_WARN("TEE driver: interrupted by signal\n");
		return ret;
	} else if (ret == 0) {
		IMSG_WARN("TEE driver: wait for result timeout\n");
		return -EAGAIN;
	}

	IMSG_DEBUG("TEE driver: dci handler is done\n");

	return 0;
}
