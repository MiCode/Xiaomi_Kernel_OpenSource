/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/slab.h>
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_reserved_mem.h"
#include "adsp_core.h"
#include "adsp_logger.h"

#define PLT_LOG_ENABLE              0x504C5402 /* magic */
#define MINIMUM_LOG_BUF_SIZE        0x10000 /* 64k */

unsigned int adsp_log_poll(struct log_ctrl_s *ctrl)
{
	struct log_info_s *log_info;
	struct buffer_info_s *buf_info;

	if (!ctrl || !ctrl->inited)
		return 0;

	log_info = (struct log_info_s *)ctrl->priv;
	buf_info = (struct buffer_info_s *)(ctrl->priv + log_info->info_ofs);

	if (buf_info->r_pos != buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	return 0;
}

ssize_t adsp_log_read(struct log_ctrl_s *ctrl, char __user *userbuf, size_t len)
{
	unsigned int w_pos, r_pos, datalen = 0;
	unsigned int data_len[2];
	void *addr;
	void *tmp_area;
	struct log_info_s *log_info;
	struct buffer_info_s *buf_info;

	if (!ctrl || !ctrl->inited)
		return 0;

	addr = ctrl->priv;
	log_info = (struct log_info_s *)ctrl->priv;
	buf_info =(struct buffer_info_s *)(ctrl->priv + log_info->info_ofs);
	mutex_lock(&ctrl->lock);

	memcpy_fromio(&r_pos, &buf_info->r_pos, sizeof(r_pos));
	memcpy_fromio(&w_pos, &buf_info->w_pos, sizeof(w_pos));

	if (r_pos == w_pos)
		goto error;
	else if (r_pos < w_pos)
		datalen = w_pos - r_pos;
	else
		datalen = log_info->buff_size - r_pos + w_pos;

	if (datalen > len)
		datalen = len;

	if (r_pos + datalen > log_info->buff_size) {
		data_len[0] = log_info->buff_size - r_pos;
		data_len[1] = r_pos + datalen - log_info->buff_size;
	} else {
		data_len[0] = datalen;
		data_len[1] = 0;
	}

	tmp_area = vmalloc(datalen);
	if (tmp_area) {
		addr += log_info->buff_ofs;
		memcpy_fromio(tmp_area, addr + r_pos, data_len[0]);
		memcpy_fromio(tmp_area + data_len[0], addr, data_len[1]);

		if (copy_to_user(userbuf, tmp_area, datalen))
			pr_info("%s, copy to user buf failed\n", __func__);

		vfree(tmp_area);
	}

	r_pos += datalen;
	if (r_pos >= log_info->buff_size)
		r_pos -= log_info->buff_size;

#ifdef CONFIG_MTK_AEE_FEATURE
	if (r_pos >= log_info->buff_size) {
		aee_kernel_exception("ADSP", "logger overflow r_pos:%u >= %u\n",
				     r_pos, log_info->buff_size);
	}
#endif
	memcpy_toio(&buf_info->r_pos, &r_pos, sizeof(r_pos));
error:
	mutex_unlock(&ctrl->lock);
	return datalen;
}

/*
 * ipi send to enable adsp logger flag
 */
ssize_t adsp_log_enable(struct log_ctrl_s *ctrl, int cid, u32 enable)
{
	int ret = 0;
	struct log_info_s *log_info;

	mutex_lock(&ctrl->lock);
	if (ctrl->inited) {
		enable = (enable) ? 1 : 0;

		_adsp_register_feature(cid, ADSP_LOGGER_FEATURE_ID, 0);

		ret = adsp_push_message(ADSP_IPI_LOGGER_ENABLE, &enable,
				    sizeof(enable), 20, cid);

		_adsp_deregister_feature(cid, ADSP_LOGGER_FEATURE_ID, 0);

		if (ret != ADSP_IPI_DONE) {
			pr_err("%s(), logger enable fail ret=%d\n",
			       __func__, ret);
			goto error;
		} else {
		        log_info = (struct log_info_s *)ctrl->priv;
		        log_info->enable = enable;
                }
	}
error:
	mutex_unlock(&ctrl->lock);
	return 0;
}

/*
 * IPI for logger init
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void adsp_logger_init_handler(int id, void *data, unsigned int len)
{
	unsigned int *ptr = (unsigned int *)data;
	unsigned int delay = 0;

	struct adsp_priv *pdata = get_adsp_core_by_id(*ptr);

	if (!pdata) {
		pr_info("%s, pdata is NULL\n", __func__);
		return;
	}
	if (len > sizeof(unsigned int)) {
		/* sync error, show error message, add delay for re-sync */
		delay = msecs_to_jiffies(100);
		pr_info("%s, resync fail msg, id(%u), addr(0x%x), size(0x%x), check:[0x%x, 0x%x, 0x%x, 0x%x, 0x%x]\n",
			__func__,
			*(ptr + 0), *(ptr + 1),
			*(ptr + 2), *(ptr + 3),
			*(ptr + 4), *(ptr + 5),
			*(ptr + 6), *(ptr + 7));
	}

	/* send work to initialize logger*/
	schedule_delayed_work(&pdata->log_ctrl->work, delay);
}

/*
 * init adsp logger dram ctrl structure
 * @return:     0: success, otherwise: fail
 */
struct log_ctrl_s *adsp_logger_init(int mem_id, void (*work_cb)(struct work_struct *ws))
{
	struct log_ctrl_s *ctrl = NULL;
	struct log_info_s *log_info;
	struct buffer_info_s *buf_info;
	int last_ofs;
	size_t size;

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		goto DONE;
	ctrl->priv = adsp_get_reserve_mem_virt(mem_id);
	size = adsp_get_reserve_mem_size(mem_id);

	if (!ctrl->priv || size < MINIMUM_LOG_BUF_SIZE) {
		pr_info("%s(), failed addr=%p, size=%zu\n", __func__,
			ctrl->priv, size);
		kfree(ctrl);
		ctrl = NULL;
		goto DONE;
	}
	memset(ctrl->priv, 0, size);

	/* init dram ctrl table */
	last_ofs = 0;

	log_info = (struct log_info_s *)ctrl->priv;
	log_info->base = PLT_LOG_ENABLE; /* magic */
	log_info->enable = 0;
	log_info->size = sizeof(struct log_info_s);
	last_ofs += ALIGN(sizeof(struct log_info_s), 128);

	log_info->info_ofs = last_ofs;
	buf_info = (struct buffer_info_s *)(ctrl->priv + log_info->info_ofs);
	buf_info->r_pos = 0;
	buf_info->w_pos = 0;
	last_ofs += sizeof(struct buffer_info_s);
	last_ofs = ALIGN(last_ofs, 128);

	log_info->buff_ofs = last_ofs;
	log_info->buff_size = size - last_ofs;

	/* register logger ini IPI */
	adsp_ipi_registration(ADSP_IPI_LOGGER_INIT, adsp_logger_init_handler,
			      "logger_init");
	INIT_DELAYED_WORK(&ctrl->work, work_cb);
	/* init ap use struct */
	mutex_init(&ctrl->lock);
	ctrl->inited = true;

	pr_debug("%s, init done, check:[0x%x, 0x%x, 0x%x, 0x%x]", __func__,
		 log_info->base, log_info->size, log_info->info_ofs, log_info->buff_ofs);

DONE:
	return ctrl;
}

ssize_t adsp_dump_log_state(struct log_ctrl_s *ctrl, char *buf, int size)
{
	int n = 0;
	unsigned int w_pos, r_pos;
	struct log_info_s *log_info = (struct log_info_s *)ctrl->priv;
	struct buffer_info_s *info = ctrl->priv + log_info->info_ofs;

	n +=  scnprintf(buf + n, size - n, "log_control\n");
	n +=  scnprintf(buf + n, size - n, "base:%X, size: %u\n",
			log_info->base, log_info->size);
	n +=  scnprintf(buf + n, size - n, "init:%u, enable:%u\n",
			ctrl->inited, log_info->enable);
	n +=  scnprintf(buf + n, size - n, "\nbuffer_info\n");
	n +=  scnprintf(buf + n, size - n,
			"info_ofs:%u, buff_ofs:%u, buff_size:%u\n",
			log_info->info_ofs, log_info->buff_ofs, log_info->buff_size);

	memcpy_fromio(&r_pos, &info->r_pos, sizeof(r_pos));
	memcpy_fromio(&w_pos, &info->w_pos, sizeof(w_pos));

	n +=  scnprintf(buf + n, size - n, "r_pos:%u, w_pos:%u\n",
			r_pos, w_pos);
	return n;
}

