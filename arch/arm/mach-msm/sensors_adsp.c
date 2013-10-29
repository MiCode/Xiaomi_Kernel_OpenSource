/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/ctype.h>
#include <linux/of_device.h>
#include <linux/msm_dsps.h>
#include <linux/uaccess.h>
#include <asm/mach-types.h>
#include <asm/arch_timer.h>
#include <mach/subsystem_restart.h>
#include <mach/ocmem.h>
#include <mach/msm_smd.h>
#include <mach/sensors_adsp.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#define DRV_NAME	"sensors"
#define DRV_VERSION	"1.00"

#define SNS_OCMEM_SMD_CHANNEL	"SENSOR"
#define SNS_OCMEM_CLIENT_ID     OCMEM_SENSORS
#define SNS_OCMEM_SIZE		SZ_256K
#define SMD_BUF_SIZE		1024
#define SNS_TIMEOUT_MS		1000

#define SNS_OCMEM_ALLOC_GROW    0x00000001
#define SNS_OCMEM_ALLOC_SHRINK  0x00000002
#define SNS_OCMEM_MAP_DONE      0x00000004
#define SNS_OCMEM_MAP_FAIL      0x00000008
#define SNS_OCMEM_UNMAP_DONE    0x00000010
#define SNS_OCMEM_UNMAP_FAIL    0x00000020

#define DSPS_HAS_CLIENT         0x00000100
#define DSPS_HAS_NO_CLIENT      0x00000200
#define DSPS_BW_VOTE_ON         0x00000400
#define DSPS_BW_VOTE_OFF        0x00000800
#define DSPS_PHYS_ADDR_SET      0x00001000

/*
 *  Structure contains all state used by the sensors driver
 */
struct sns_adsp_control_s {
	wait_queue_head_t sns_wait;
	spinlock_t sns_lock;
	struct workqueue_struct *sns_workqueue;
	struct work_struct sns_work;
	struct workqueue_struct *smd_wq;
	struct work_struct smd_read_work;
	smd_channel_t *smd_ch;
	uint32_t sns_ocmem_status;
	uint32_t mem_segments_size;
	struct sns_mem_segment_s_v01 mem_segments[SNS_OCMEM_MAX_NUM_SEG_V01];
	struct ocmem_buf *buf;
	struct ocmem_map_list map_list;
	struct ocmem_notifier *ocmem_handle;
	bool ocmem_enabled;
	struct notifier_block ocmem_nb;
	uint32_t sns_ocmem_bus_client;
	struct platform_device *pdev;
	void *pil;
	struct class *dev_class;
	dev_t dev_num;
	struct device *dev;
	struct cdev *cdev;
};

static struct sns_adsp_control_s sns_ctl;

/*
 * All asynchronous responses from the OCMEM driver are received
 * by this function
 */
int sns_ocmem_drv_cb(struct notifier_block *self,
			unsigned long action,
			void *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&sns_ctl.sns_lock, flags);

	pr_debug("%s: Received OCMEM callback: action=%li\n",
		__func__, action);

	switch (action) {
	case OCMEM_MAP_DONE:
		sns_ctl.sns_ocmem_status |= SNS_OCMEM_MAP_DONE;
		sns_ctl.sns_ocmem_status &= (~OCMEM_MAP_FAIL &
						~SNS_OCMEM_UNMAP_DONE &
						~SNS_OCMEM_UNMAP_FAIL);
		break;
	case OCMEM_MAP_FAIL:
		sns_ctl.sns_ocmem_status |= SNS_OCMEM_MAP_FAIL;
		sns_ctl.sns_ocmem_status &= (~OCMEM_MAP_DONE &
						~SNS_OCMEM_UNMAP_DONE &
						~SNS_OCMEM_UNMAP_FAIL);
		break;
	case OCMEM_UNMAP_DONE:
		sns_ctl.sns_ocmem_status |= SNS_OCMEM_UNMAP_DONE;
		sns_ctl.sns_ocmem_status &= (~SNS_OCMEM_UNMAP_FAIL &
						~SNS_OCMEM_MAP_DONE &
						~OCMEM_MAP_FAIL);
		break;
	case OCMEM_UNMAP_FAIL:
		sns_ctl.sns_ocmem_status |= SNS_OCMEM_UNMAP_FAIL;
		sns_ctl.sns_ocmem_status &= (~SNS_OCMEM_UNMAP_DONE &
						~SNS_OCMEM_MAP_DONE &
						~OCMEM_MAP_FAIL);
		break;
	case OCMEM_ALLOC_GROW:
		sns_ctl.sns_ocmem_status |= SNS_OCMEM_ALLOC_GROW;
		sns_ctl.sns_ocmem_status &= ~SNS_OCMEM_ALLOC_SHRINK;
		break;
	case OCMEM_ALLOC_SHRINK:
		sns_ctl.sns_ocmem_status |= SNS_OCMEM_ALLOC_SHRINK;
		sns_ctl.sns_ocmem_status &= ~SNS_OCMEM_ALLOC_GROW;
		break;
	default:
		pr_err("%s: Unknown action received in OCMEM callback %lu\n",
						__func__, action);
		break;
	}

	spin_unlock_irqrestore(&sns_ctl.sns_lock, flags);
	wake_up(&sns_ctl.sns_wait);

	return 0;
}

/*
 * Processes messages received through SMD from the ADSP
 *
 * @param hdr The message header
 * @param msg Message pointer
 *
 */
void sns_ocmem_smd_process(struct sns_ocmem_hdr_s *hdr, void *msg)
{
	unsigned long flags;

	spin_lock_irqsave(&sns_ctl.sns_lock, flags);

	pr_debug("%s: Received message from ADSP; id: %i type: %i (%08x)\n",
		__func__, hdr->msg_id, hdr->msg_type,
		sns_ctl.sns_ocmem_status);

	if (hdr->msg_id == SNS_OCMEM_PHYS_ADDR_RESP_V01 &&
	    hdr->msg_type == SNS_OCMEM_MSG_TYPE_RESP) {
		struct sns_ocmem_phys_addr_resp_msg_v01 *msg_ptr =
				(struct sns_ocmem_phys_addr_resp_msg_v01 *)msg;
		pr_debug("%s: Received SNS_OCMEM_PHYS_ADDR_RESP_V01\n",
			__func__);
		pr_debug("%s: segments_valid=%d, segments_len=%d\n", __func__,
				msg_ptr->segments_valid, msg_ptr->segments_len);

		if (msg_ptr->segments_valid) {
			sns_ctl.mem_segments_size = msg_ptr->segments_len;
			memcpy(sns_ctl.mem_segments, msg_ptr->segments,
				sizeof(struct sns_mem_segment_s_v01) *
				msg_ptr->segments_len);

			sns_ctl.sns_ocmem_status |= DSPS_PHYS_ADDR_SET;
		} else {
			pr_err("%s: Received invalid segment list\n", __func__);
		}
	} else if (hdr->msg_id == SNS_OCMEM_HAS_CLIENT_IND_V01  &&
		   hdr->msg_type == SNS_OCMEM_MSG_TYPE_IND) {
		struct sns_ocmem_has_client_ind_msg_v01 *msg_ptr =
				(struct sns_ocmem_has_client_ind_msg_v01 *)msg;

		pr_debug("%s: Received SNS_OCMEM_HAS_CLIENT_IND_V01\n",
			__func__);
		pr_debug("%s: ADSP has %i client(s)\n", __func__,
			msg_ptr->num_clients);
		if (msg_ptr->num_clients > 0) {
			sns_ctl.sns_ocmem_status |= DSPS_HAS_CLIENT;
			sns_ctl.sns_ocmem_status &= ~DSPS_HAS_NO_CLIENT;
		} else {
			sns_ctl.sns_ocmem_status |= DSPS_HAS_NO_CLIENT;
			sns_ctl.sns_ocmem_status &= ~DSPS_HAS_CLIENT;
		}
	} else if (hdr->msg_id == SNS_OCMEM_BW_VOTE_RESP_V01 &&
		   hdr->msg_type == SNS_OCMEM_MSG_TYPE_RESP) {
		/* no need to handle this response msg, just return */
		pr_debug("%s: Received SNS_OCMEM_BW_VOTE_RESP_V01\n", __func__);
		spin_unlock_irqrestore(&sns_ctl.sns_lock, flags);
		return;
	} else if (hdr->msg_id == SNS_OCMEM_BW_VOTE_IND_V01 &&
		   hdr->msg_type == SNS_OCMEM_MSG_TYPE_IND) {
		struct sns_ocmem_bw_vote_ind_msg_v01 *msg_ptr =
			(struct sns_ocmem_bw_vote_ind_msg_v01 *)msg;
		pr_debug("%s: Received BW_VOTE_IND_V01, is_vote_on=%d\n",
						__func__, msg_ptr->is_vote_on);

		if (msg_ptr->is_vote_on) {
			sns_ctl.sns_ocmem_status |= DSPS_BW_VOTE_ON;
			sns_ctl.sns_ocmem_status &= ~DSPS_BW_VOTE_OFF;
		} else {
			sns_ctl.sns_ocmem_status |= DSPS_BW_VOTE_OFF;
			sns_ctl.sns_ocmem_status &= ~DSPS_BW_VOTE_ON;
		}
	} else {
		pr_err("%s: Unknown message type received. id: %i; type: %i\n",
					__func__, hdr->msg_id, hdr->msg_type);
	}

	spin_unlock_irqrestore(&sns_ctl.sns_lock, flags);

	wake_up(&sns_ctl.sns_wait);
}

static void sns_ocmem_smd_read(struct work_struct *ws)
{
	struct smd_channel *ch = sns_ctl.smd_ch;
	unsigned char *buf = NULL;
	int sz, len;

	for (;;) {
		sz = smd_cur_packet_size(ch);
		BUG_ON(sz > SMD_BUF_SIZE);
		len = smd_read_avail(ch);
		pr_debug("%s: sz=%d, len=%d\n", __func__, sz, len);
		if (len == 0 || len < sz)
			break;
		buf = kzalloc(SMD_BUF_SIZE, GFP_KERNEL);
		if (buf == NULL) {
			pr_err("%s: malloc failed", __func__);
			break;
		}

		if (smd_read(ch, buf, sz) != sz) {
			pr_err("%s: not enough data?!\n", __func__);
			kfree(buf);
			continue;
		}

		sns_ocmem_smd_process((struct sns_ocmem_hdr_s *)buf,
			(void *)((char *)buf +
			sizeof(struct sns_ocmem_hdr_s)));

		kfree(buf);

	}
}

/*
 * All SMD notifications and messages from Sensors on ADSP are
 * received by this function
 *
 */
void sns_ocmem_smd_notify_data(void *data, unsigned int event)
{
	if (event == SMD_EVENT_DATA) {
		int sz;
		pr_debug("%s: Received SMD event Data\n", __func__);
		sz = smd_cur_packet_size(sns_ctl.smd_ch);
		if ((sz > 0) && (sz <= smd_read_avail(sns_ctl.smd_ch)))
			queue_work(sns_ctl.smd_wq, &sns_ctl.smd_read_work);
	} else if (event == SMD_EVENT_OPEN) {
		pr_debug("%s: Received SMD event Open\n", __func__);
	} else if (event == SMD_EVENT_CLOSE) {
		pr_debug("%s: Received SMD event Close\n", __func__);
	}
}

static bool sns_ocmem_is_status_set(uint32_t sns_ocmem_status)
{
	unsigned long flags;
	bool is_set;

	spin_lock_irqsave(&sns_ctl.sns_lock, flags);
	is_set = sns_ctl.sns_ocmem_status & sns_ocmem_status;
	spin_unlock_irqrestore(&sns_ctl.sns_lock, flags);
	return is_set;
}

/*
 * Wait for a response from ADSP or OCMEM Driver, timeout if necessary
 *
 * @param sns_ocmem_status Status flags to wait for.
 * @param timeout_sec Seconds to wait before timeout
 * @param timeout_nsec Nanoseconds to wait.  Total timeout = nsec + sec
 *
 * @return 0 If any status flag is set at any time prior to a timeout.
 *	0 if success or timedout ; <0 for failures
 */
static int sns_ocmem_wait(uint32_t sns_ocmem_status,
			  uint32_t timeout_ms)
{
	int err;
	if (timeout_ms) {
		err = wait_event_interruptible_timeout(sns_ctl.sns_wait,
			sns_ocmem_is_status_set(sns_ocmem_status),
			msecs_to_jiffies(timeout_ms));

		if (err == 0)
			pr_err("%s: interruptible_timeout timeout err=%i\n",
							__func__, err);
		else if (err < 0)
			pr_err("%s: interruptible_timeout failed err=%i\n",
							__func__, err);
	} else { /* no timeout */
		err = wait_event_interruptible(sns_ctl.sns_wait,
			sns_ocmem_is_status_set(sns_ocmem_status));
		if (err < 0)
			pr_err("%s: wait_event_interruptible failed err=%i\n",
						__func__, err);
	}

	return err;
}

/*
 * Sends a message to the ADSP via SMD.
 *
 * @param hdr Specifies message type and other meta data
 * @param msg_ptr Pointer to the message contents.
 *                Must be freed within this function if no error is returned.
 *
 * @return 0 upon success; < 0 upon error
 */
static int
sns_ocmem_send_msg(struct sns_ocmem_hdr_s *hdr, void const *msg_ptr)
{
	int rv = 0;
	int err = 0;
	void *temp = NULL;
	int size = sizeof(struct sns_ocmem_hdr_s) + hdr->msg_size;

	temp = kzalloc(sizeof(struct sns_ocmem_hdr_s) + hdr->msg_size,
			GFP_KERNEL);
	if (temp == NULL) {
		pr_err("%s: allocation failure\n", __func__);
		rv = -ENOMEM;
		goto out;
	}

	hdr->dst_module = SNS_OCMEM_MODULE_ADSP;
	hdr->src_module = SNS_OCMEM_MODULE_KERNEL;

	memcpy(temp, hdr, sizeof(struct sns_ocmem_hdr_s));
	memcpy((char *)temp + sizeof(struct sns_ocmem_hdr_s),
		msg_ptr, hdr->msg_size);
	pr_debug("%s: send msg type: %i size: %i id: %i dst: %i src: %i\n",
				__func__, hdr->msg_type, hdr->msg_size,
				hdr->msg_id, hdr->dst_module, hdr->src_module);

	if (hdr == NULL) {
		pr_err("%s: NULL message header\n", __func__);
		rv = -EINVAL;
	} else {
		if (sns_ctl.smd_ch == NULL) {
			pr_err("%s: null smd_ch\n", __func__);
			rv = -EINVAL;
		}
		err = smd_write(sns_ctl.smd_ch, temp, size);
		if (err < 0) {
			pr_err("%s: smd_write failed %i\n", __func__, err);
			rv = -ECOMM;
		} else {
			pr_debug("%s smd_write successful ret=%d\n",
				__func__, err);
		}
	}

	kfree(temp);

out:
	return rv;
}

/*
 * Load ADSP Firmware.
 */

static int sns_load_adsp(void)
{
	sns_ctl.pil = subsystem_get("adsp");
	if (IS_ERR(sns_ctl.pil)) {
		pr_err("%s: fail to load ADSP firmware\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s: Q6/ADSP image is loaded\n", __func__);

	return 0;
}

static int sns_ocmem_platform_data_populate(struct platform_device *pdev)
{
	int ret;
	struct msm_bus_scale_pdata *sns_ocmem_bus_scale_pdata = NULL;
	struct msm_bus_vectors *sns_ocmem_bus_vectors = NULL;
	struct msm_bus_paths *ocmem_sns_bus_paths = NULL;
	u32 val;

	if (!pdev->dev.of_node) {
		pr_err("%s: device tree information missing\n", __func__);
		return -ENODEV;
	}

	sns_ocmem_bus_vectors = kzalloc(sizeof(struct msm_bus_vectors),
					GFP_KERNEL);
	if (!sns_ocmem_bus_vectors) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,src-id", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,src-id missing in DT node\n",
				__func__);
		goto fail1;
	}
	sns_ocmem_bus_vectors->src = val;
	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,dst-id", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,dst-id missing in DT node\n",
				__func__);
		goto fail1;
	}
	sns_ocmem_bus_vectors->dst = val;
	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,ab", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,ab missing in DT node\n",
					__func__);
		goto fail1;
	}
	sns_ocmem_bus_vectors->ab = val;
	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,ib", &val);
	if (ret) {
		dev_err(&pdev->dev, "%s: qcom,ib missing in DT node\n",
					__func__);
		goto fail1;
	}
	sns_ocmem_bus_vectors->ib = val;
	ocmem_sns_bus_paths = kzalloc(sizeof(struct msm_bus_paths),
					GFP_KERNEL);

	if (!ocmem_sns_bus_paths) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		goto fail1;
	}
	ocmem_sns_bus_paths->num_paths = 1;
	ocmem_sns_bus_paths->vectors = sns_ocmem_bus_vectors;

	sns_ocmem_bus_scale_pdata =
			kzalloc(sizeof(struct msm_bus_scale_pdata), GFP_KERNEL);
	if (!sns_ocmem_bus_scale_pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		goto fail2;
	}

	sns_ocmem_bus_scale_pdata->usecase = ocmem_sns_bus_paths;
	sns_ocmem_bus_scale_pdata->num_usecases = 1;
	sns_ocmem_bus_scale_pdata->name = "sensors-ocmem";

	dev_set_drvdata(&pdev->dev, sns_ocmem_bus_scale_pdata);
	return ret;

fail2:
	kfree(ocmem_sns_bus_paths);
fail1:
	kfree(sns_ocmem_bus_vectors);
	return ret;
}


/*
 * Initialize all sensors ocmem driver data fields and register with the
 * ocmem driver.
 *
 * @return 0 upon success; < 0 upon error
 */
static int sns_ocmem_init(void)
{
	int i, err, ret;
	struct sns_ocmem_hdr_s addr_req_hdr;
	struct msm_bus_scale_pdata *sns_ocmem_bus_scale_pdata = NULL;

	/* register from OCMEM callack */
	sns_ctl.ocmem_handle =
		ocmem_notifier_register(SNS_OCMEM_CLIENT_ID,
		&sns_ctl.ocmem_nb);
	if (sns_ctl.ocmem_handle == NULL) {
		pr_err("OCMEM notifier registration failed\n");
		return -EFAULT;
	}

	/* populate platform data */
	ret = sns_ocmem_platform_data_populate(sns_ctl.pdev);
	if (ret) {
		dev_err(&sns_ctl.pdev->dev,
			"%s: failed to populate platform data, rc = %d\n",
			__func__, ret);
		return -ENODEV;
	}
	sns_ocmem_bus_scale_pdata = dev_get_drvdata(&sns_ctl.pdev->dev);

	sns_ctl.sns_ocmem_bus_client =
		msm_bus_scale_register_client(sns_ocmem_bus_scale_pdata);

	if (!sns_ctl.sns_ocmem_bus_client) {
		pr_err("%s: msm_bus_scale_register_client() failed\n",
			__func__);
		return -EFAULT;
	}

	/* load ADSP first */
	if (sns_load_adsp() != 0) {
		pr_err("%s: sns_load_adsp failed\n", __func__);
		return -EFAULT;
	}

	/*
	 * wait before open SMD channel from kernel to ensure
	 * channel has been openned already from ADSP side
	 */
	msleep(1000);

	err = smd_named_open_on_edge(SNS_OCMEM_SMD_CHANNEL,
					SMD_APPS_QDSP,
					&sns_ctl.smd_ch,
					NULL,
					sns_ocmem_smd_notify_data);
	if (err != 0) {
		pr_err("%s: smd_named_open_on_edge failed %i\n", __func__, err);
		return -EFAULT;
	}

	pr_debug("%s: SMD channel openned successfuly!\n", __func__);
	/* wait for the channel ready before writing data */
	msleep(1000);
	addr_req_hdr.msg_id = SNS_OCMEM_PHYS_ADDR_REQ_V01;
	addr_req_hdr.msg_type = SNS_OCMEM_MSG_TYPE_REQ;
	addr_req_hdr.msg_size = 0;

	err = sns_ocmem_send_msg(&addr_req_hdr, NULL);
	if (err != 0) {
		pr_err("%s: sns_ocmem_send_msg failed %i\n", __func__, err);
		return -ECOMM;
	}

	err = sns_ocmem_wait(DSPS_PHYS_ADDR_SET, 0);
	if (err != 0) {
		pr_err("%s: sns_ocmem_wait failed %i\n", __func__, err);
		return -EFAULT;
	}

	sns_ctl.map_list.num_chunks = sns_ctl.mem_segments_size;
	for (i = 0; i < sns_ctl.mem_segments_size; i++) {
		sns_ctl.map_list.chunks[i].ro =
			sns_ctl.mem_segments[i].type == 1 ? true : false;
		sns_ctl.map_list.chunks[i].ddr_paddr =
			sns_ctl.mem_segments[i].start_address;
		sns_ctl.map_list.chunks[i].size =
			sns_ctl.mem_segments[i].size;

		pr_debug("%s: chunks[%d]: ro=%d, ddr_paddr=0x%lx, size=%li",
				__func__, i,
				sns_ctl.map_list.chunks[i].ro,
				sns_ctl.map_list.chunks[i].ddr_paddr,
				sns_ctl.map_list.chunks[i].size);
	}

	return 0;
}

/*
 * Unmaps memory in ocmem back to DDR, indicates to the ADSP its completion,
 * and waits for it to finish removing its bandwidth vote.
 */
static void sns_ocmem_unmap(void)
{
	unsigned long flags;
	int err = 0;

	ocmem_set_power_state(SNS_OCMEM_CLIENT_ID,
				sns_ctl.buf, OCMEM_ON);

	spin_lock_irqsave(&sns_ctl.sns_lock, flags);
	sns_ctl.sns_ocmem_status &= (~SNS_OCMEM_UNMAP_FAIL &
					~SNS_OCMEM_UNMAP_DONE);
	spin_unlock_irqrestore(&sns_ctl.sns_lock, flags);

	err = ocmem_unmap(SNS_OCMEM_CLIENT_ID,
				sns_ctl.buf,
				&sns_ctl.map_list);

	if (err != 0) {
		pr_err("ocmem_unmap failed %i\n", err);
	} else {
		err = sns_ocmem_wait(SNS_OCMEM_UNMAP_DONE |
					SNS_OCMEM_UNMAP_FAIL, 0);

		if (err == 0) {
			if (sns_ocmem_is_status_set(SNS_OCMEM_UNMAP_DONE))
				pr_debug("%s: OCMEM_UNMAP_DONE\n", __func__);
			else if (sns_ocmem_is_status_set(
							SNS_OCMEM_UNMAP_FAIL)) {
				pr_err("%s: OCMEM_UNMAP_FAIL\n", __func__);
				BUG_ON(true);
			} else
				pr_err("%s: status flag not set\n", __func__);
		} else {
			pr_err("%s: sns_ocmem_wait failed %i\n",
					__func__, err);
		}
	}

	ocmem_set_power_state(SNS_OCMEM_CLIENT_ID,
				sns_ctl.buf, OCMEM_OFF);
}

/*
 * Waits for allocation to succeed.  This may take considerable time if the device
 * is presently in a high-power use case.
 *
 * @return 0 on success; < 0 upon error
 */
static int sns_ocmem_wait_for_alloc(void)
{
	int err = 0;

	err = sns_ocmem_wait(SNS_OCMEM_ALLOC_GROW |
				DSPS_HAS_NO_CLIENT, 0);

	if (err == 0) {
		if (sns_ocmem_is_status_set(DSPS_HAS_NO_CLIENT)) {
			pr_debug("%s: Lost client while waiting for GROW\n",
				__func__);
			ocmem_free(SNS_OCMEM_CLIENT_ID, sns_ctl.buf);
			sns_ctl.buf = NULL;
			return -EPIPE;
		}
	} else {
		pr_err("sns_ocmem_wait failed %i\n", err);
		return -EFAULT;
	}

	return 0;
}

/*
 * Kicks-off the mapping of memory from DDR to ocmem.  Waits for the process
 * to complete, then indicates so to the ADSP.
 *
 * @return 0: Success; < 0: Other error
 */
static int sns_ocmem_map(void)
{
	int err = 0;
	unsigned long flags;

	spin_lock_irqsave(&sns_ctl.sns_lock, flags);
	sns_ctl.sns_ocmem_status &=
			(~SNS_OCMEM_MAP_FAIL & ~SNS_OCMEM_MAP_DONE);
	spin_unlock_irqrestore(&sns_ctl.sns_lock, flags);

	/* vote for ocmem bus bandwidth */
	err = msm_bus_scale_client_update_request(
				sns_ctl.sns_ocmem_bus_client,
				0);
	if (err)
		pr_err("%s: failed to vote for bus bandwidth\n", __func__);

	err = ocmem_map(SNS_OCMEM_CLIENT_ID,
			sns_ctl.buf,
			&sns_ctl.map_list);

	if (err != 0) {
		pr_debug("ocmem_map failed %i\n", err);
		ocmem_set_power_state(SNS_OCMEM_CLIENT_ID,
					sns_ctl.buf, OCMEM_OFF);
		ocmem_free(SNS_OCMEM_CLIENT_ID, sns_ctl.buf);
		sns_ctl.buf = NULL;
	} else {
		err = sns_ocmem_wait(SNS_OCMEM_ALLOC_SHRINK |
					DSPS_HAS_NO_CLIENT |
					SNS_OCMEM_MAP_DONE |
					SNS_OCMEM_MAP_FAIL, 0);

		if (err == 0) {
			if (sns_ocmem_is_status_set(SNS_OCMEM_MAP_DONE))
				pr_debug("%s: OCMEM mapping DONE\n", __func__);
			else if (sns_ocmem_is_status_set(DSPS_HAS_NO_CLIENT)) {
				pr_debug("%s: Lost client while waiting for MAP\n",
					__func__);
				sns_ocmem_unmap();
				ocmem_free(SNS_OCMEM_CLIENT_ID,
						sns_ctl.buf);
				sns_ctl.buf = NULL;
				err = -EPIPE;
			} else if (sns_ocmem_is_status_set(
						SNS_OCMEM_ALLOC_SHRINK)) {
				pr_debug("%s: SHRINK while wait for MAP\n",
					__func__);
				sns_ocmem_unmap();
				err = ocmem_shrink(SNS_OCMEM_CLIENT_ID,
						sns_ctl.buf, 0);
				BUG_ON(err != 0);
				err = -EFAULT;
			} else if (sns_ocmem_is_status_set(
						SNS_OCMEM_MAP_FAIL)) {
				pr_err("%s: OCMEM mapping fails\n", __func__);
				ocmem_set_power_state(SNS_OCMEM_CLIENT_ID,
							sns_ctl.buf,
							OCMEM_OFF);
				ocmem_free(SNS_OCMEM_CLIENT_ID,
						sns_ctl.buf);
				sns_ctl.buf = NULL;
			} else
				pr_err("%s: status flag not set\n", __func__);
		} else {
			pr_err("sns_ocmem_wait failed %i\n", err);
		}
	}

	return err;
}

/*
 * Allocates memory in ocmem and maps to it from DDR.
 *
 * @return 0 upon success; <0 upon failure;
 */
static int sns_ocmem_alloc(void)
{
	int err = 0;
	unsigned long flags;

	if (sns_ctl.buf == NULL) {
		spin_lock_irqsave(&sns_ctl.sns_lock, flags);
		sns_ctl.sns_ocmem_status &= ~SNS_OCMEM_ALLOC_GROW &
						~SNS_OCMEM_ALLOC_SHRINK;
		spin_unlock_irqrestore(&sns_ctl.sns_lock, flags);
		sns_ctl.buf = ocmem_allocate_nb(SNS_OCMEM_CLIENT_ID,
						SNS_OCMEM_SIZE);

		if (sns_ctl.buf == NULL) {
			pr_err("ocmem_allocate_nb returned NULL\n");
			sns_ctl.ocmem_enabled = false;
			err = -EFAULT;
		} else if (sns_ctl.buf->len != 0 &&
			SNS_OCMEM_SIZE > sns_ctl.buf->len) {
			pr_err("ocmem_allocate_nb: invalid len %li, Req: %i)\n",
				sns_ctl.buf->len, SNS_OCMEM_SIZE);
			sns_ctl.ocmem_enabled = false;
			err = -EFAULT;
		}
	}

	pr_debug("%s OCMEM buf=%lx, buffer len=%li\n", __func__,
			sns_ctl.buf->addr, sns_ctl.buf->len);

	while (sns_ctl.ocmem_enabled) {
		if (sns_ctl.buf->len == 0) {
			pr_debug("%s: Waiting for memory allocation\n",
				__func__);
			err = sns_ocmem_wait_for_alloc();
			if (err == -EPIPE) {
				pr_debug("%s:Lost client while wait for alloc\n",
					__func__);
				break;
			} else if (err != 0) {
				pr_err("sns_ocmem_wait_for_alloc failed %i\n",
					err);
				break;
			}
		}

		ocmem_set_power_state(SNS_OCMEM_CLIENT_ID,
					sns_ctl.buf,
					OCMEM_ON);

		err = sns_ocmem_map();

		if (err == -EPIPE) {
			pr_debug("%s: Lost client while waiting for mapping\n",
				__func__);
			break;
		} else if (err < 0) {
			pr_debug("%s: Mapping failed, will try again\n",
				__func__);
			break;
		} else if (err == 0) {
			pr_debug("%s: Mapping finished\n", __func__);
			break;
		}
	}

	return err;
}

/*
 * Indicate to the ADSP that unmapping has completed, and wait for the response
 * that its bandwidth vote has been removed.
 *
 * @return 0 Upon success; < 0 upon error
 */
static int sns_ocmem_unmap_send(void)
{
	int err;
	struct sns_ocmem_hdr_s msg_hdr;
	struct sns_ocmem_bw_vote_req_msg_v01 msg;

	memset(&msg, 0, sizeof(struct sns_ocmem_bw_vote_req_msg_v01));

	msg_hdr.msg_id = SNS_OCMEM_BW_VOTE_REQ_V01;
	msg_hdr.msg_type = SNS_OCMEM_MSG_TYPE_REQ;
	msg_hdr.msg_size = sizeof(struct sns_ocmem_bw_vote_req_msg_v01);
	msg.is_map = 0;
	msg.vectors_valid = 0;
	msg.vectors_len = 0;

	pr_debug("%s: send bw_vote OFF\n", __func__);
	err = sns_ocmem_send_msg(&msg_hdr, &msg);
	if (err != 0) {
		pr_err("%s: sns_ocmem_send_msg failed %i\n",
				__func__, err);
	} else {
		err = sns_ocmem_wait(DSPS_BW_VOTE_OFF, 0);
		if (err != 0)
			pr_err("%s: sns_ocmem_wait failed %i\n", __func__, err);
	}

	return err;
}

/*
 * Indicate to the ADSP that mapping has completed, and wait for the response
 * that its bandwidth vote has been made.
 *
 * @return 0 Upon success; < 0 upon error
 */
static int sns_ocmem_map_send(void)
{
	int err;
	struct sns_ocmem_hdr_s msg_hdr;
	struct sns_ocmem_bw_vote_req_msg_v01 msg;
	struct ocmem_vectors *vectors;

	memset(&msg, 0, sizeof(struct sns_ocmem_bw_vote_req_msg_v01));

	msg_hdr.msg_id = SNS_OCMEM_BW_VOTE_REQ_V01;
	msg_hdr.msg_type = SNS_OCMEM_MSG_TYPE_REQ;
	msg_hdr.msg_size = sizeof(struct sns_ocmem_bw_vote_req_msg_v01);
	msg.is_map = 1;

	vectors = ocmem_get_vectors(SNS_OCMEM_CLIENT_ID, sns_ctl.buf);
	if ((vectors != NULL)) {
		memcpy(&msg.vectors, vectors, sizeof(*vectors));
		/* TODO: set vectors_len */
		msg.vectors_valid = true;
		msg.vectors_len = 0;
	}

	pr_debug("%s: send bw_vote ON\n", __func__);
	err = sns_ocmem_send_msg(&msg_hdr, &msg);
	if (err != 0) {
		pr_err("%s: sns_ocmem_send_msg failed %i\n", __func__, err);
	} else {
		err = sns_ocmem_wait(DSPS_BW_VOTE_ON |
					SNS_OCMEM_ALLOC_SHRINK, 0);
		if (err != 0)
			pr_err("%s: sns_ocmem_wait failed %i\n", __func__, err);
	}

	return err;
}

/*
 * Perform the encessary operations to clean-up OCMEM after being notified that
 * there is no longer a client; if sensors was evicted; or if some error
 * has occurred.
 *
 * @param[i] do_free Whether the memory should be freed (true) or if shrink
 *                   should be called instead (false).
 */
static void sns_ocmem_evicted(bool do_free)
{
	int err = 0;

	sns_ocmem_unmap();
	if (do_free) {
		ocmem_free(SNS_OCMEM_CLIENT_ID, sns_ctl.buf);
		sns_ctl.buf = NULL;
	} else {
		err = ocmem_shrink(SNS_OCMEM_CLIENT_ID, sns_ctl.buf, 0);
		BUG_ON(err != 0);
	}

	err = sns_ocmem_unmap_send();
	if (err != 0)
		pr_err("sns_ocmem_unmap_send failed %i\n", err);
}

/*
 * After mapping has completed and the ADSP has reacted appropriately, wait
 * for a shrink command or word from the ADSP that it no longer has a client.
 *
 * @return 0 If no clients; < 0 upon error;
 */
static int sns_ocmem_map_done(void)
{
	int err = 0;
	unsigned long flags;

	err = sns_ocmem_map_send();
	if (err != 0) {
		pr_err("sns_ocmem_map_send failed %i\n", err);
		sns_ocmem_evicted(true);
	} else {
		ocmem_set_power_state(SNS_OCMEM_CLIENT_ID,
					sns_ctl.buf, OCMEM_OFF);

		pr_debug("%s: Waiting for shrink or 'no client' updates\n",
			__func__);
		err = sns_ocmem_wait(DSPS_HAS_NO_CLIENT |
					SNS_OCMEM_ALLOC_SHRINK, 0);
		if (err == 0) {
			if (sns_ocmem_is_status_set(DSPS_HAS_NO_CLIENT)) {
				pr_debug("%s: No longer have a client\n",
					__func__);
				sns_ocmem_evicted(true);
			} else if (sns_ocmem_is_status_set(
						SNS_OCMEM_ALLOC_SHRINK)) {
				pr_debug("%s: Received SHRINK\n", __func__);
				sns_ocmem_evicted(false);

				spin_lock_irqsave(&sns_ctl.sns_lock, flags);
				sns_ctl.sns_ocmem_status &=
						~SNS_OCMEM_ALLOC_SHRINK;
				spin_unlock_irqrestore(&sns_ctl.sns_lock,
							flags);
				err = -EFAULT;
			}
		} else {
			pr_err("sns_ocmem_wait failed %i\n", err);
		}
	}

	return err;
}

/*
 * Main function.
 * Initializes sensors ocmem feature, and waits for an ADSP client.
 */
static void sns_ocmem_main(struct work_struct *work)
{
	int err = 0;
	pr_debug("%s\n", __func__);

	err = sns_ocmem_init();
	if (err != 0) {
		pr_err("%s: sns_ocmem_init failed %i\n", __func__, err);
		return;
	}

	while (true) {
		pr_debug("%s: Waiting for sensor client\n", __func__);
		if (sns_ocmem_is_status_set(DSPS_HAS_CLIENT) ||
			!sns_ocmem_wait(DSPS_HAS_CLIENT, 0)) {
			pr_debug("%s: DSPS_HAS_CLIENT\n", __func__);

			err = sns_ocmem_alloc();
			if (err != 0) {
				pr_err("sns_ocmem_alloc failed %i\n", err);
				return;
			} else {
				err = sns_ocmem_map_done();
				if (err != 0) {
					pr_err("sns_ocmem_map_done failed %i",
						err);
					return;
				}
			}
		}
	}

	ocmem_notifier_unregister(sns_ctl.ocmem_handle,
					&sns_ctl.ocmem_nb);
}

static int sensors_adsp_open(struct inode *ip, struct file *fp)
{
	int ret = 0;
	return ret;
}

static int sensors_adsp_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * Read QTimer clock ticks and scale down to 32KHz clock as used
 * in DSPS
 */
static u32 sns_read_qtimer(void)
{
	u64 val;
	val = arch_counter_get_cntpct();
	/*
	 * To convert ticks from 19.2 Mhz clock to 32768 Hz clock:
	 * x = (value * 32768) / 19200000
	 * This is same as first left shift the value by 4 bits, i.e. mutiply
	 * by 16, and then divide by 9375. The latter is preferable since
	 * QTimer tick (value) is 56-bit, so (value * 32768) could overflow,
	 * while (value * 16) will never do
	 */
	val <<= 4;
	do_div(val, 9375);

	return (u32)val;
}

/*
 * IO Control - handle commands from client.
 */
static long sensors_adsp_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u32 val = 0;

	switch (cmd) {
	case DSPS_IOCTL_READ_SLOW_TIMER:
		val = sns_read_qtimer();
		ret = put_user(val, (u32 __user *) arg);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * platform driver
 */
const struct file_operations sensors_adsp_fops = {
	.owner = THIS_MODULE,
	.open = sensors_adsp_open,
	.release = sensors_adsp_release,
	.unlocked_ioctl = sensors_adsp_ioctl,
};

static int sensors_adsp_probe(struct platform_device *pdev)
{
	int ret = 0;
	sns_ctl.dev_class = class_create(THIS_MODULE, DRV_NAME);
	if (sns_ctl.dev_class == NULL) {
		pr_err("%s: class_create fail.\n", __func__);
		goto res_err;
	}

	ret = alloc_chrdev_region(&sns_ctl.dev_num, 0, 1, DRV_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region fail.\n", __func__);
		goto alloc_chrdev_region_err;
	}

	sns_ctl.dev = device_create(sns_ctl.dev_class, NULL,
				     sns_ctl.dev_num,
				     &sns_ctl, DRV_NAME);
	if (IS_ERR(sns_ctl.dev)) {
		pr_err("%s: device_create fail.\n", __func__);
		goto device_create_err;
	}

	sns_ctl.cdev = cdev_alloc();
	if (sns_ctl.cdev == NULL) {
		pr_err("%s: cdev_alloc fail.\n", __func__);
		goto cdev_alloc_err;
	}
	cdev_init(sns_ctl.cdev, &sensors_adsp_fops);
	sns_ctl.cdev->owner = THIS_MODULE;

	ret = cdev_add(sns_ctl.cdev, sns_ctl.dev_num, 1);
	if (ret) {
		pr_err("%s: cdev_add fail.\n", __func__);
		goto cdev_add_err;
	}

	sns_ctl.sns_workqueue =
			alloc_workqueue("sns_ocmem", WQ_NON_REENTRANT, 0);
	if (!sns_ctl.sns_workqueue) {
		pr_err("%s: Failed to create work queue\n",
			__func__);
		goto cdev_add_err;
	}

	sns_ctl.smd_wq =
			alloc_workqueue("smd_wq", WQ_NON_REENTRANT, 0);
	if (!sns_ctl.smd_wq) {
		pr_err("%s: Failed to create work queue\n",
			__func__);
		goto cdev_add_err;
	}

	init_waitqueue_head(&sns_ctl.sns_wait);
	spin_lock_init(&sns_ctl.sns_lock);

	sns_ctl.ocmem_handle = NULL;
	sns_ctl.buf = NULL;
	sns_ctl.sns_ocmem_status = 0;
	sns_ctl.ocmem_enabled = true;
	sns_ctl.ocmem_nb.notifier_call = sns_ocmem_drv_cb;
	sns_ctl.smd_ch = NULL;
	sns_ctl.pdev = pdev;

	INIT_WORK(&sns_ctl.sns_work, sns_ocmem_main);
	INIT_WORK(&sns_ctl.smd_read_work, sns_ocmem_smd_read);

	queue_work(sns_ctl.sns_workqueue, &sns_ctl.sns_work);

	return 0;

cdev_add_err:
	kfree(sns_ctl.cdev);
cdev_alloc_err:
	device_destroy(sns_ctl.dev_class, sns_ctl.dev_num);
device_create_err:
	unregister_chrdev_region(sns_ctl.dev_num, 1);
alloc_chrdev_region_err:
	class_destroy(sns_ctl.dev_class);
res_err:
	return -ENODEV;
}

static int sensors_adsp_remove(struct platform_device *pdev)
{
	struct msm_bus_scale_pdata *sns_ocmem_bus_scale_pdata = NULL;

	sns_ocmem_bus_scale_pdata = (struct msm_bus_scale_pdata *)
					dev_get_drvdata(&pdev->dev);

	kfree(sns_ocmem_bus_scale_pdata->usecase->vectors);
	kfree(sns_ocmem_bus_scale_pdata->usecase);
	kfree(sns_ocmem_bus_scale_pdata);

	ocmem_notifier_unregister(sns_ctl.ocmem_handle,
					&sns_ctl.ocmem_nb);
	destroy_workqueue(sns_ctl.sns_workqueue);
	destroy_workqueue(sns_ctl.smd_wq);

	cdev_del(sns_ctl.cdev);
	kfree(sns_ctl.cdev);
	sns_ctl.cdev = NULL;
	device_destroy(sns_ctl.dev_class, sns_ctl.dev_num);
	unregister_chrdev_region(sns_ctl.dev_num, 1);
	class_destroy(sns_ctl.dev_class);

	return 0;
}

static const struct of_device_id msm_adsp_sensors_dt_match[] = {
	{.compatible = "qcom,msm-adsp-sensors"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_adsp_sensors_dt_match);


static struct platform_driver sensors_adsp_driver = {
	.driver = {
		.name = "sensors-adsp",
		.owner = THIS_MODULE,
		.of_match_table = msm_adsp_sensors_dt_match,
	},
	.probe = sensors_adsp_probe,
	.remove = sensors_adsp_remove,
};

/*
 * Module Init.
 */
static int sensors_adsp_init(void)
{
	int rc;
	pr_debug("%s driver version %s.\n", DRV_NAME, DRV_VERSION);

	rc = platform_driver_register(&sensors_adsp_driver);

	if (rc) {
		pr_err("%s: Failed to register sensors adsp driver\n",
			__func__);
		return rc;
	}

	return 0;
}

/*
 * Module Exit.
 */
static void sensors_adsp_exit(void)
{
	platform_driver_unregister(&sensors_adsp_driver);
}

module_init(sensors_adsp_init);
module_exit(sensors_adsp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sensors ADSP driver");
