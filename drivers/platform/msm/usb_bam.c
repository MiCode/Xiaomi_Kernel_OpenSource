/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/msm-sps.h>
#include <linux/ipa.h>
#include <linux/usb_bam.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>

#define USB_THRESHOLD 512
#define USB_BAM_MAX_STR_LEN 50
#define USB_BAM_TIMEOUT (10*HZ)
#define DBG_MAX_MSG   512UL
#define DBG_MSG_LEN   160UL
#define TIME_BUF_LEN  17
#define DBG_EVENT_LEN  143

#define USB_BAM_NR_PORTS	4

/* Additional memory to be allocated than required data fifo size */
#define DATA_FIFO_EXTRA_MEM_ALLOC_SIZE 512

#define ARRAY_INDEX_FROM_ADDR(base, addr) ((addr) - (base))

#define ENABLE_EVENT_LOG 1
static unsigned int enable_event_log = ENABLE_EVENT_LOG;
module_param(enable_event_log, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_event_log, "enable event logging in debug buffer");

#define LOGLEVEL_NONE 8
#define LOGLEVEL_DEBUG 7
#define LOGLEVEL_ERR 3

#define log_event(log_level, x...)					\
do {									\
	unsigned long flags;						\
	char *buf;							\
	if (log_level == LOGLEVEL_DEBUG)				\
		pr_debug(x);						\
	else if (log_level == LOGLEVEL_ERR)				\
		pr_err(x);						\
	if (enable_event_log) {						\
		write_lock_irqsave(&usb_bam_dbg.lck, flags);		\
		buf = usb_bam_dbg.buf[usb_bam_dbg.idx];			\
		put_timestamp(buf);					\
		snprintf(&buf[TIME_BUF_LEN - 1], DBG_EVENT_LEN, x);	\
		usb_bam_dbg.idx = (usb_bam_dbg.idx + 1) % DBG_MAX_MSG;	\
		write_unlock_irqrestore(&usb_bam_dbg.lck, flags);	\
	}								\
} while (0)

#define log_event_none(x, ...) log_event(LOGLEVEL_NONE, x, ##__VA_ARGS__)
#define log_event_dbg(x, ...) log_event(LOGLEVEL_DEBUG, x, ##__VA_ARGS__)
#define log_event_err(x, ...) log_event(LOGLEVEL_ERR, x, ##__VA_ARGS__)

struct usb_bam_sps_type {
	struct sps_pipe **sps_pipes;
	struct sps_connect *sps_connections;
};

/**
* struct usb_bam_ctx_type - represents the usb bam driver entity
* @usb_bam_sps: holds the sps pipes the usb bam driver holds
*	against the sps driver.
* @usb_bam_pdev: the platfrom device that represents the usb bam.
* @usb_bam_wq: Worqueue used for managing states of reset against
*	a peer bam.
* @max_connections: The maximum number of pipes that are configured
*	in the platform data.
* @h_bam: This array stores for each BAM ("ssusb", "hsusb" or "hsic") the
*	handle/device of the sps driver.
* @pipes_enabled_per_bam: This array stores for each BAM
*	("ssusb", "hsusb" or "hsic") the number of pipes currently enabled.
* @inactivity_timer_ms: The timeout configuration per each bam for inactivity
*	timer feature.
* @is_bam_inactivity: Is there no activity on all pipes belongs to a
*	specific bam. (no activity = no data is pulled or pushed
*	from/into ones of the pipes).
* @usb_bam_connections: array (allocated on probe) having all BAM connections
* @usb_bam_lock: to protect fields of ctx or usb_bam_connections
*/
struct usb_bam_ctx_type {
	struct usb_bam_sps_type		usb_bam_sps;
	struct resource			*io_res;
	void __iomem			*regs;
	int				irq;
	struct platform_device		*usb_bam_pdev;
	struct workqueue_struct		*usb_bam_wq;
	u8				max_connections;
	unsigned long			h_bam;
	u8				pipes_enabled_per_bam;
	u32				inactivity_timer_ms;
	bool				is_bam_inactivity;
	struct usb_bam_pipe_connect	*usb_bam_connections;
	spinlock_t		usb_bam_lock;
};

static char *bam_enable_strings[MAX_BAMS] = {
	[DWC3_CTRL] = "ssusb",
	[CI_CTRL] = "hsusb",
	[HSIC_CTRL]  = "hsic",
};

/*
 * CI_CTRL & DWC3_CTRL shouldn't be used simultaneously
 * since both share the same prod & cons rm resourses
 */
static enum ipa_client_type ipa_rm_resource_prod[MAX_BAMS] = {
	[CI_CTRL] = IPA_RM_RESOURCE_USB_PROD,
	[HSIC_CTRL]  = IPA_RM_RESOURCE_HSIC_PROD,
	[DWC3_CTRL] = IPA_RM_RESOURCE_USB_PROD,
};

static enum ipa_client_type ipa_rm_resource_cons[MAX_BAMS] = {
	[CI_CTRL] = IPA_RM_RESOURCE_USB_CONS,
	[HSIC_CTRL]  = IPA_RM_RESOURCE_HSIC_CONS,
	[DWC3_CTRL] = IPA_RM_RESOURCE_USB_CONS,
};

static int usb_cons_request_resource(void);
static int usb_cons_release_resource(void);
static int ss_usb_cons_request_resource(void);
static int ss_usb_cons_release_resource(void);
static int hsic_cons_request_resource(void);
static int hsic_cons_release_resource(void);

static int (*request_resource_cb[MAX_BAMS])(void) = {
	[CI_CTRL] = usb_cons_request_resource,
	[HSIC_CTRL]  = hsic_cons_request_resource,
	[DWC3_CTRL] = ss_usb_cons_request_resource,
};

static int (*release_resource_cb[MAX_BAMS])(void)  = {
	[CI_CTRL] = usb_cons_release_resource,
	[HSIC_CTRL]  = hsic_cons_release_resource,
	[DWC3_CTRL] = ss_usb_cons_release_resource,
};

struct usb_bam_ipa_handshake_info {
	enum ipa_rm_event cur_prod_state;
	enum ipa_rm_event cur_cons_state;

	enum usb_bam_mode cur_bam_mode;
	enum usb_ctrl bam_type;
	int connect_complete;
	int bus_suspend;
	bool disconnected;
	bool in_lpm;
	u8 prod_pipes_enabled_per_bam;

	int (*wake_cb)(void *);
	void *wake_param;

	u32 suspend_src_idx[USB_BAM_NR_PORTS];
	u32 suspend_dst_idx[USB_BAM_NR_PORTS];
	u32 resume_src_idx[USB_BAM_NR_PORTS];
	u32 resume_dst_idx[USB_BAM_NR_PORTS];

	u32 pipes_to_suspend;
	u32 pipes_suspended;
	u32 pipes_resumed;

	struct completion prod_avail;
	struct completion prod_released;

	struct mutex suspend_resume_mutex;
	struct work_struct resume_work;
	struct work_struct finish_suspend_work;
};

struct usb_bam_host_info {
	struct device *dev;
	bool in_lpm;
};

static spinlock_t usb_bam_ipa_handshake_info_lock;
static struct usb_bam_ipa_handshake_info info[MAX_BAMS];

static struct usb_bam_ctx_type msm_usb_bam[MAX_BAMS];
/* USB bam type used as a peer of the qdss in bam2bam mode */
static enum usb_ctrl qdss_usb_bam_type;

static struct usb_bam_host_info host_info[MAX_BAMS];

static int __usb_bam_register_wake_cb(enum usb_ctrl bam_type, int idx,
				      int (*callback)(void *user),
	void *param, bool trigger_cb_per_pipe);
static void wait_for_prod_release(enum usb_ctrl cur_bam);
static void usb_bam_start_suspend(struct usb_bam_ipa_handshake_info *info_ptr);

static struct {
	char buf[DBG_MAX_MSG][DBG_MSG_LEN];   /* buffer */
	unsigned idx;   /* index */
	rwlock_t lck;   /* lock */
} __maybe_unused usb_bam_dbg = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

/*put_timestamp - writes time stamp to buffer */
static void __maybe_unused put_timestamp(char *tbuf)
{
	unsigned long long t;
	unsigned long nanosec_rem;

	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000)/1000;
	snprintf(tbuf, TIME_BUF_LEN, "[%5lu.%06lu]: ", (unsigned long)t,
		nanosec_rem);
}

void msm_bam_set_hsic_host_dev(struct device *dev)
{
	if (dev) {
		/* Hold the device until allowing lpm */
		info[HSIC_CTRL].in_lpm = false;
		log_event_dbg("%s: Getting hsic device %pK\n", __func__, dev);
		pm_runtime_get(dev);
	} else if (host_info[HSIC_CTRL].dev) {
		log_event_dbg("%s: Try Putting hsic device %pK, lpm:%d\n",
				__func__, host_info[HSIC_CTRL].dev,
				info[HSIC_CTRL].in_lpm);
		/* Just release previous device if not already done */
		if (!info[HSIC_CTRL].in_lpm) {
			info[HSIC_CTRL].in_lpm = true;
			pm_runtime_put(host_info[HSIC_CTRL].dev);
		}
	}

	host_info[HSIC_CTRL].dev = dev;
	host_info[HSIC_CTRL].in_lpm = false;
}

void msm_bam_set_usb_host_dev(struct device *dev)
{
	host_info[CI_CTRL].dev = dev;
	host_info[CI_CTRL].in_lpm = false;
}

static int get_bam_type_from_core_name(const char *name)
{
	if (strnstr(name, bam_enable_strings[DWC3_CTRL],
			USB_BAM_MAX_STR_LEN) ||
		strnstr(name, "dwc3", USB_BAM_MAX_STR_LEN))
		return DWC3_CTRL;
	else if (strnstr(name, bam_enable_strings[HSIC_CTRL],
			USB_BAM_MAX_STR_LEN) ||
		strnstr(name, "ci13xxx_msm_hsic", USB_BAM_MAX_STR_LEN))
		return HSIC_CTRL;
	else if (strnstr(name, bam_enable_strings[CI_CTRL],
			USB_BAM_MAX_STR_LEN) ||
		strnstr(name, "ci", USB_BAM_MAX_STR_LEN))
		return CI_CTRL;

	log_event_err("%s: invalid BAM name(%s)\n", __func__, name);
	return -EINVAL;
}

static void usb_bam_set_inactivity_timer(enum usb_ctrl bam)
{
	struct sps_timer_ctrl timer_ctrl;
	struct usb_bam_pipe_connect *pipe_connect;
	struct sps_pipe *pipe = NULL;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam];
	int i;

	log_event_dbg("%s: enter\n", __func__);

	/*
	 * Since we configure global incativity timer for all pipes
	 * and not per each pipe, it is enough to use some pipe
	 * handle associated with this bam, so just find the first one.
	 * This pipe handle is required due to SPS driver API we use below.
	 */
	for (i = 0; i < ctx->max_connections; i++) {
		pipe_connect = &ctx->usb_bam_connections[i];
		if (pipe_connect->bam_type == bam && pipe_connect->enabled) {
			pipe = ctx->usb_bam_sps.sps_pipes[i];
			break;
		}
	}

	if (!pipe) {
		pr_warning("%s: Bam %s has no connected pipes\n", __func__,
			bam_enable_strings[bam]);
		return;
	}

	timer_ctrl.op = SPS_TIMER_OP_CONFIG;
	timer_ctrl.mode = SPS_TIMER_MODE_ONESHOT;
	timer_ctrl.timeout_msec = ctx->inactivity_timer_ms;
	sps_timer_ctrl(pipe, &timer_ctrl, NULL);

	timer_ctrl.op = SPS_TIMER_OP_RESET;
	sps_timer_ctrl(pipe, &timer_ctrl, NULL);
}

static int usb_bam_alloc_buffer(struct usb_bam_pipe_connect *pipe_connect)
{
	int ret = 0;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[pipe_connect->bam_type];
	struct msm_usb_bam_platform_data *pdata =
				ctx->usb_bam_pdev->dev.platform_data;
	struct sps_mem_buffer *data_buf = &(pipe_connect->data_mem_buf);
	struct sps_mem_buffer *desc_buf = &(pipe_connect->desc_mem_buf);

	pr_debug("%s: data_fifo size:%x desc_fifo_size:%x\n",
				__func__, pipe_connect->data_fifo_size,
				pipe_connect->desc_fifo_size);
	switch (pipe_connect->mem_type) {
	case SPS_PIPE_MEM:
		log_event_dbg("%s: USB BAM using SPS pipe memory\n", __func__);
		ret = sps_setup_bam2bam_fifo(data_buf,
				pipe_connect->data_fifo_base_offset,
				pipe_connect->data_fifo_size, 1);
		if (ret) {
			log_event_err("%s: data fifo setup failure %d\n",
					__func__, ret);
			goto err_exit;
		}

		ret = sps_setup_bam2bam_fifo(desc_buf,
				pipe_connect->desc_fifo_base_offset,
				pipe_connect->desc_fifo_size, 1);
		if (ret) {
			log_event_err("%s: desc. fifo setup failure %d\n",
					__func__, ret);
			goto err_exit;
		}
		break;
	case OCI_MEM:
		if (pipe_connect->mem_type == OCI_MEM)
			log_event_dbg("%s: USB BAM using ocimem\n", __func__);

		if (data_buf->base) {
			log_event_err("%s: Already allocated OCI Memory\n",
								__func__);
			break;
		}

		data_buf->phys_base = pipe_connect->data_fifo_base_offset +
						pdata->usb_bam_fifo_baseaddr;
		data_buf->size = pipe_connect->data_fifo_size;
		data_buf->base = ioremap(data_buf->phys_base, data_buf->size);
		if (!data_buf->base) {
			log_event_err("%s: ioremap failed for data fifo\n",
					__func__);
			ret = -ENOMEM;
			goto err_exit;
		}
		memset_io(data_buf->base, 0, data_buf->size);

		desc_buf->phys_base = pipe_connect->desc_fifo_base_offset +
						pdata->usb_bam_fifo_baseaddr;
		desc_buf->size = pipe_connect->desc_fifo_size;
		desc_buf->base = ioremap(desc_buf->phys_base, desc_buf->size);
		if (!desc_buf->base) {
			log_event_err("%s: ioremap failed for desc fifo\n",
					__func__);
			iounmap(data_buf->base);
			ret = -ENOMEM;
			goto err_exit;
		}
		memset_io(desc_buf->base, 0, desc_buf->size);
		break;
	case SYSTEM_MEM:
		log_event_dbg("%s: USB BAM using system memory\n", __func__);

		if (data_buf->base) {
			log_event_err("%s: Already allocated memory\n",
								__func__);
			break;
		}

		/* BAM would use system memory, allocate FIFOs */
		data_buf->size = pipe_connect->data_fifo_size;
		/* On platforms which use CI controller, USB HW can fetch
		 * additional 128 bytes at the end of circular buffer when
		 * AXI prefetch is enabled and hence requirement is to
		 * allocate 512 bytes more than required length.
		 */
		if (pipe_connect->bam_type == CI_CTRL)
			data_buf->base =
				dma_alloc_coherent(&ctx->usb_bam_pdev->dev,
				(pipe_connect->data_fifo_size +
					DATA_FIFO_EXTRA_MEM_ALLOC_SIZE),
				&(data_buf->phys_base),
				GFP_KERNEL);
		else
			data_buf->base =
				dma_alloc_coherent(&ctx->usb_bam_pdev->dev,
				pipe_connect->data_fifo_size,
				&(data_buf->phys_base),
				GFP_KERNEL);
		if (!data_buf->base) {
			log_event_err("%s: dma_alloc_coherent failed for data fifo\n",
								__func__);
			ret = -ENOMEM;
			goto err_exit;
		}
		memset(data_buf->base, 0, pipe_connect->data_fifo_size);

		desc_buf->size = pipe_connect->desc_fifo_size;
		desc_buf->base = dma_alloc_coherent(&ctx->usb_bam_pdev->dev,
			pipe_connect->desc_fifo_size,
			&(desc_buf->phys_base),
			GFP_KERNEL);
		if (!desc_buf->base) {
			log_event_err("%s: dma_alloc_coherent failed for desc fifo\n",
								__func__);
			if (pipe_connect->bam_type == CI_CTRL)
				dma_free_coherent(&ctx->usb_bam_pdev->dev,
				(pipe_connect->data_fifo_size +
					DATA_FIFO_EXTRA_MEM_ALLOC_SIZE),
				data_buf->base,
				data_buf->phys_base);
			else
				dma_free_coherent(&ctx->usb_bam_pdev->dev,
				pipe_connect->data_fifo_size,
				data_buf->base,
				data_buf->phys_base);
			ret = -ENOMEM;
			goto err_exit;
		}
		memset(desc_buf->base, 0, pipe_connect->desc_fifo_size);
		break;
	default:
		log_event_err("%s: invalid mem type\n", __func__);
		ret = -EINVAL;
	}

	return ret;

err_exit:
	return ret;
}

int usb_bam_alloc_fifos(enum usb_ctrl cur_bam, u8 idx)
{
	int ret;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect =
					&ctx->usb_bam_connections[idx];

	ret = usb_bam_alloc_buffer(pipe_connect);
	if (ret) {
		log_event_err("%s(): Error(%d) allocating buffer\n",
				__func__, ret);
		return ret;
	}
	return 0;
}

int usb_bam_free_fifos(enum usb_ctrl cur_bam, u8 idx)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect =
				&ctx->usb_bam_connections[idx];
	struct sps_connect *sps_connection =
				&ctx->usb_bam_sps.sps_connections[idx];

	pr_debug("%s(): data size:%x desc size:%x\n",
			__func__, sps_connection->data.size,
			sps_connection->desc.size);

	switch (pipe_connect->mem_type) {
	case SYSTEM_MEM:
		log_event_dbg("%s: Freeing system memory used by PIPE\n",
				__func__);
		if (sps_connection->data.phys_base) {
			if (cur_bam == CI_CTRL)
				dma_free_coherent(&ctx->usb_bam_pdev->dev,
					(sps_connection->data.size +
						DATA_FIFO_EXTRA_MEM_ALLOC_SIZE),
					sps_connection->data.base,
					sps_connection->data.phys_base);
			else
				dma_free_coherent(&ctx->usb_bam_pdev->dev,
					sps_connection->data.size,
					sps_connection->data.base,
					sps_connection->data.phys_base);
			sps_connection->data.phys_base = 0;
			pipe_connect->data_mem_buf.base = NULL;
		}
		if (sps_connection->desc.phys_base) {
			dma_free_coherent(&ctx->usb_bam_pdev->dev,
					sps_connection->desc.size,
					sps_connection->desc.base,
					sps_connection->desc.phys_base);
			sps_connection->desc.phys_base = 0;
			pipe_connect->desc_mem_buf.base = NULL;
		}
		break;
	case OCI_MEM:
		log_event_dbg("Freeing oci memory used by BAM PIPE\n");
		if (sps_connection->data.base) {
			iounmap(sps_connection->data.base);
			sps_connection->data.base = NULL;
			pipe_connect->data_mem_buf.base = NULL;
		}
		if (sps_connection->desc.base) {
			iounmap(sps_connection->desc.base);
			sps_connection->desc.base = NULL;
			pipe_connect->desc_mem_buf.base = NULL;
		}
		break;
	case SPS_PIPE_MEM:
		log_event_dbg("%s: nothing to be be\n", __func__);
		break;
	}

	return 0;
}

static int connect_pipe(enum usb_ctrl cur_bam, u8 idx, u32 *usb_pipe_idx)
{
	int ret;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_sps_type usb_bam_sps = ctx->usb_bam_sps;
	struct sps_pipe **pipe = &(usb_bam_sps.sps_pipes[idx]);
	struct sps_connect *sps_connection = &usb_bam_sps.sps_connections[idx];
	struct usb_bam_pipe_connect *pipe_connect =
					&ctx->usb_bam_connections[idx];
	enum usb_bam_pipe_dir dir = pipe_connect->dir;
	struct sps_mem_buffer *data_buf = &(pipe_connect->data_mem_buf);
	struct sps_mem_buffer *desc_buf = &(pipe_connect->desc_mem_buf);

	*pipe = sps_alloc_endpoint();
	if (*pipe == NULL) {
		log_event_err("%s: sps_alloc_endpoint failed\n", __func__);
		return -ENOMEM;
	}

	ret = sps_get_config(*pipe, sps_connection);
	if (ret) {
		log_event_err("%s: tx get config failed %d\n", __func__, ret);
		goto free_sps_endpoint;
	}

	ret = sps_phy2h(pipe_connect->src_phy_addr, &(sps_connection->source));
	if (ret) {
		log_event_err("%s: sps_phy2h failed (src BAM) %d\n",
				__func__, ret);
		goto free_sps_endpoint;
	}

	sps_connection->src_pipe_index = pipe_connect->src_pipe_index;
	ret = sps_phy2h(pipe_connect->dst_phy_addr,
		&(sps_connection->destination));
	if (ret) {
		log_event_err("%s: sps_phy2h failed (dst BAM) %d\n",
				__func__, ret);
		goto free_sps_endpoint;
	}
	sps_connection->dest_pipe_index = pipe_connect->dst_pipe_index;

	if (dir == USB_TO_PEER_PERIPHERAL) {
		sps_connection->mode = SPS_MODE_SRC;
		*usb_pipe_idx = pipe_connect->src_pipe_index;
	} else {
		sps_connection->mode = SPS_MODE_DEST;
		*usb_pipe_idx = pipe_connect->dst_pipe_index;
	}

	sps_connection->data = *data_buf;
	sps_connection->desc = *desc_buf;
	sps_connection->event_thresh = 16;
	sps_connection->options = SPS_O_AUTO_ENABLE;

	ret = sps_connect(*pipe, sps_connection);
	if (ret < 0) {
		log_event_err("%s: sps_connect failed %d\n", __func__, ret);
		goto error;
	}

	return 0;

error:
	sps_disconnect(*pipe);
free_sps_endpoint:
	sps_free_endpoint(*pipe);
	return ret;
}

static int connect_pipe_sys2bam_ipa(enum usb_ctrl cur_bam, u8 idx,
			struct usb_bam_connect_ipa_params *ipa_params)
{
	int ret;
	enum usb_bam_pipe_dir dir = ipa_params->dir;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect =
				&ctx->usb_bam_connections[idx];
	struct ipa_sys_connect_params sys_in_params;
	unsigned long usb_handle;
	phys_addr_t usb_phy_addr;
	u32 clnt_hdl = 0;

	memset(&sys_in_params, 0, sizeof(sys_in_params));

	if (dir == USB_TO_PEER_PERIPHERAL) {
		usb_phy_addr = pipe_connect->src_phy_addr;
		sys_in_params.client = ipa_params->src_client;
		ipa_params->ipa_cons_ep_idx =
			ipa_get_ep_mapping(sys_in_params.client);
	} else {
		usb_phy_addr = pipe_connect->dst_phy_addr;
		sys_in_params.client = ipa_params->dst_client;
		ipa_params->ipa_prod_ep_idx =
			ipa_get_ep_mapping(sys_in_params.client);
	}

	log_event_dbg("%s(): ipa_prod_ep_idx:%d ipa_cons_ep_idx:%d\n",
			__func__, ipa_params->ipa_prod_ep_idx,
			ipa_params->ipa_cons_ep_idx);

	/* Get HSUSB / HSIC bam handle */
	ret = sps_phy2h(usb_phy_addr, &usb_handle);
	if (ret) {
		log_event_err("%s: sps_phy2h failed (HSUSB/HSIC BAM) %d\n",
			__func__, ret);
		return ret;
	}

	pipe_connect->activity_notify = ipa_params->activity_notify;
	pipe_connect->inactivity_notify = ipa_params->inactivity_notify;
	pipe_connect->priv = ipa_params->priv;

	/* IPA sys connection params */
	sys_in_params.desc_fifo_sz = pipe_connect->desc_fifo_size;
	sys_in_params.priv = ipa_params->priv;
	sys_in_params.notify = ipa_params->notify;
	sys_in_params.skip_ep_cfg = ipa_params->skip_ep_cfg;
	sys_in_params.keep_ipa_awake = ipa_params->keep_ipa_awake;
	memcpy(&sys_in_params.ipa_ep_cfg, &ipa_params->ipa_ep_cfg,
		   sizeof(struct ipa_ep_cfg));

	ret = ipa_setup_sys_pipe(&sys_in_params, &clnt_hdl);
	if (ret) {
		log_event_err("%s: ipa_connect failed\n", __func__);
		return ret;
	}
	pipe_connect->ipa_clnt_hdl = clnt_hdl;
	if (dir == USB_TO_PEER_PERIPHERAL)
		ipa_params->cons_clnt_hdl = clnt_hdl;
	else
		ipa_params->prod_clnt_hdl = clnt_hdl;

	return 0;
}

static int connect_pipe_bam2bam_ipa(enum usb_ctrl cur_bam, u8 idx,
			struct usb_bam_connect_ipa_params *ipa_params)
{
	int ret;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_sps_type usb_bam_sps = ctx->usb_bam_sps;
	enum usb_bam_pipe_dir dir = ipa_params->dir;
	struct sps_pipe **pipe = &(usb_bam_sps.sps_pipes[idx]);
	struct sps_connect *sps_connection = &usb_bam_sps.sps_connections[idx];
	struct usb_bam_pipe_connect *pipe_connect =
					&ctx->usb_bam_connections[idx];
	struct sps_mem_buffer *data_buf = &(pipe_connect->data_mem_buf);
	struct sps_mem_buffer *desc_buf = &(pipe_connect->desc_mem_buf);
	struct ipa_connect_params ipa_in_params;
	struct ipa_sps_params sps_out_params;
	u32 usb_phy_addr;
	unsigned long usb_handle;
	u32 clnt_hdl = 0;

	memset(&ipa_in_params, 0, sizeof(ipa_in_params));
	memset(&sps_out_params, 0, sizeof(sps_out_params));

	if (dir == USB_TO_PEER_PERIPHERAL) {
		usb_phy_addr = pipe_connect->src_phy_addr;
		ipa_in_params.client_ep_idx = pipe_connect->src_pipe_index;
		ipa_in_params.client = ipa_params->src_client;
	} else {
		usb_phy_addr = pipe_connect->dst_phy_addr;
		ipa_in_params.client_ep_idx = pipe_connect->dst_pipe_index;
		ipa_in_params.client = ipa_params->dst_client;
	}
	/* Get HSUSB / HSIC bam handle */
	ret = sps_phy2h(usb_phy_addr, &usb_handle);
	if (ret) {
		log_event_err("%s: sps_phy2h failed (HSUSB/HSIC BAM) %d\n",
			__func__, ret);
		return ret;
	}

	pipe_connect->activity_notify = ipa_params->activity_notify;
	pipe_connect->inactivity_notify = ipa_params->inactivity_notify;
	pipe_connect->priv = ipa_params->priv;
	pipe_connect->reset_pipe_after_lpm = ipa_params->reset_pipe_after_lpm;

	/* IPA input parameters */
	ipa_in_params.client_bam_hdl = usb_handle;
	ipa_in_params.desc_fifo_sz = pipe_connect->desc_fifo_size;
	ipa_in_params.data_fifo_sz = pipe_connect->data_fifo_size;
	ipa_in_params.notify = ipa_params->notify;
	ipa_in_params.priv = ipa_params->priv;
	ipa_in_params.skip_ep_cfg = ipa_params->skip_ep_cfg;
	ipa_in_params.keep_ipa_awake = ipa_params->keep_ipa_awake;

	ipa_in_params.desc = pipe_connect->desc_mem_buf;
	ipa_in_params.data = pipe_connect->data_mem_buf;

	memcpy(&ipa_in_params.ipa_ep_cfg, &ipa_params->ipa_ep_cfg,
		   sizeof(struct ipa_ep_cfg));

	ret = ipa_connect(&ipa_in_params, &sps_out_params, &clnt_hdl);
	if (ret) {
		log_event_err("%s: ipa_connect failed\n", __func__);
		return ret;
	}
	pipe_connect->ipa_clnt_hdl = clnt_hdl;

	*pipe = sps_alloc_endpoint();
	if (*pipe == NULL) {
		log_event_err("%s: sps_alloc_endpoint failed\n", __func__);
		ret = -ENOMEM;
		goto disconnect_ipa;
	}

	ret = sps_get_config(*pipe, sps_connection);
	if (ret) {
		log_event_err("%s: tx get config failed %d\n", __func__ , ret);
		goto free_sps_endpoints;
	}

	if (dir == USB_TO_PEER_PERIPHERAL) {
		/* USB src IPA dest */
		sps_connection->mode = SPS_MODE_SRC;
		ipa_params->cons_clnt_hdl = clnt_hdl;
		sps_connection->source = usb_handle;
		sps_connection->destination = sps_out_params.ipa_bam_hdl;
		sps_connection->src_pipe_index = pipe_connect->src_pipe_index;
		sps_connection->dest_pipe_index = sps_out_params.ipa_ep_idx;
		ipa_params->ipa_cons_ep_idx = sps_out_params.ipa_ep_idx;
		*(ipa_params->src_pipe) = sps_connection->src_pipe_index;
		pipe_connect->dst_pipe_index = sps_out_params.ipa_ep_idx;
		log_event_dbg("%s: BAM pipe usb[%x]->ipa[%x] connection\n",
			__func__,
			pipe_connect->src_pipe_index,
			pipe_connect->dst_pipe_index);
		sps_connection->options = SPS_O_NO_DISABLE;
	} else {
		/* IPA src, USB dest */
		sps_connection->mode = SPS_MODE_DEST;
		ipa_params->prod_clnt_hdl = clnt_hdl;
		sps_connection->source = sps_out_params.ipa_bam_hdl;
		sps_connection->destination = usb_handle;
		sps_connection->src_pipe_index = sps_out_params.ipa_ep_idx;
		ipa_params->ipa_prod_ep_idx = sps_out_params.ipa_ep_idx;
		sps_connection->dest_pipe_index = pipe_connect->dst_pipe_index;
		*(ipa_params->dst_pipe) = sps_connection->dest_pipe_index;
		pipe_connect->src_pipe_index = sps_out_params.ipa_ep_idx;
		log_event_dbg("%s: BAM pipe ipa[%x]->usb[%x] connection\n",
			__func__,
			pipe_connect->src_pipe_index,
			pipe_connect->dst_pipe_index);
		sps_connection->options = 0;
	}

	sps_connection->data = *data_buf;
	sps_connection->desc = *desc_buf;
	sps_connection->event_thresh = 16;
	sps_connection->options |= SPS_O_AUTO_ENABLE;

	ret = sps_connect(*pipe, sps_connection);
	if (ret < 0) {
		log_event_err("%s: sps_connect failed %d\n", __func__, ret);
		goto error;
	}

	return 0;

error:
	sps_disconnect(*pipe);
free_sps_endpoints:
	sps_free_endpoint(*pipe);
disconnect_ipa:
	ipa_disconnect(clnt_hdl);
	return ret;
}

static int disconnect_pipe(enum usb_ctrl cur_bam, u8 idx)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct sps_pipe *pipe = ctx->usb_bam_sps.sps_pipes[idx];
	struct sps_connect *sps_connection =
				&ctx->usb_bam_sps.sps_connections[idx];

	sps_disconnect(pipe);
	sps_free_endpoint(pipe);
	ctx->usb_bam_sps.sps_pipes[idx] = NULL;
	sps_connection->options &= ~SPS_O_AUTO_ENABLE;

	return 0;
}

static bool _hsic_host_bam_resume_core(void)
{
	log_event_dbg("%s: enter\n", __func__);

	/* Exit from "full suspend" in case of hsic host */
	if (host_info[HSIC_CTRL].dev && info[HSIC_CTRL].in_lpm) {
		log_event_dbg("%s: Getting hsic device %pK\n", __func__,
			host_info[HSIC_CTRL].dev);
		pm_runtime_get(host_info[HSIC_CTRL].dev);
		info[HSIC_CTRL].in_lpm = false;
		return true;
	}
	return false;
}

static void _hsic_host_bam_suspend_core(void)
{
	log_event_dbg("%s: enter\n", __func__);

	if (host_info[HSIC_CTRL].dev && !info[HSIC_CTRL].in_lpm) {
		log_event_dbg("%s: Putting hsic host device %pK\n", __func__,
			host_info[HSIC_CTRL].dev);
		pm_runtime_put(host_info[HSIC_CTRL].dev);
		info[HSIC_CTRL].in_lpm = true;
	}
}

static void usb_bam_suspend_core(enum usb_ctrl bam_type,
	enum usb_bam_mode bam_mode,
	bool disconnect)
{
	log_event_dbg("%s: enter bam=%s\n", __func__,
			bam_enable_strings[bam_type]);

	if ((bam_mode == USB_BAM_DEVICE) || (bam_type != HSIC_CTRL)) {
		log_event_err("%s: Invalid BAM type %d\n", __func__, bam_type);
		return;
	}

	_hsic_host_bam_suspend_core();
	return;
}

static bool usb_bam_resume_core(enum usb_ctrl bam_type,
	enum usb_bam_mode bam_mode)
{
	log_event_dbg("%s: enter bam=%s\n", __func__,
			bam_enable_strings[bam_type]);

	if ((bam_mode == USB_BAM_DEVICE) || (bam_type != HSIC_CTRL)) {
		log_event_err("%s: Invalid BAM type %d\n", __func__, bam_type);
		return false;
	}

	return _hsic_host_bam_resume_core();
}

/**
 * usb_bam_disconnect_ipa_prod() - disconnects the USB consumer i.e. IPA producer.
 * @ipa_params: USB IPA related parameters
 * @cur_bam: USB controller used for BAM functionality
 *
 * It performs disconnect with IPA driver for IPA producer pipe and
 * with SPS driver for USB BAM consumer pipe. This API also takes care
 * of SYS2BAM and BAM2BAM IPA disconnect functionality.
 *
 * Return: 0 in case of success, errno otherwise.
 */
static int usb_bam_disconnect_ipa_prod(
		struct usb_bam_connect_ipa_params *ipa_params,
		enum usb_ctrl cur_bam)
{
	int ret;
	u8 idx = 0;
	struct usb_bam_pipe_connect *pipe_connect;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];

	idx = ipa_params->dst_idx;
	pipe_connect = &ctx->usb_bam_connections[idx];
	pipe_connect->activity_notify = NULL;
	pipe_connect->inactivity_notify = NULL;
	pipe_connect->priv = NULL;

	/* close IPA -> USB pipe */
	if (pipe_connect->pipe_type == USB_BAM_PIPE_BAM2BAM) {
		ret = ipa_disconnect(ipa_params->prod_clnt_hdl);
		if (ret) {
			log_event_err("%s: dst pipe disconnection failure\n",
					__func__);
			return ret;
		}

		ret = usb_bam_disconnect_pipe(cur_bam, idx);
		if (ret) {
			log_event_err("%s: failure to disconnect pipe %d\n",
				   __func__, idx);
			return ret;
		}
	} else {
		ret = ipa_teardown_sys_pipe(ipa_params->prod_clnt_hdl);
		if (ret) {
			log_event_err("%s: dst pipe disconnection failure\n",
				  __func__);
			return ret;
		}

		pipe_connect->enabled = false;
		spin_lock(&ctx->usb_bam_lock);
		if (ctx->pipes_enabled_per_bam == 0)
			log_event_err("%s: wrong pipes enabled counter for bam=%d\n",
				__func__, pipe_connect->bam_type);
		else
			ctx->pipes_enabled_per_bam -= 1;
		spin_unlock(&ctx->usb_bam_lock);
	}

	return 0;
}

/**
 * usb_bam_disconnect_ipa_cons() - disconnects the USB producer i.e. IPA consumer.
 * @ipa_params: USB IPA related parameters
 * @cur_bam: USB controller used for BAM functionality
 *
 * It performs disconnect with IPA driver for IPA consumer pipe and
 * with SPS driver for USB BAM producer pipe. This API also takes care
 * of SYS2BAM and BAM2BAM IPA disconnect functionality.
 *
 * Return: 0 in case of success, errno otherwise.
 */
static int usb_bam_disconnect_ipa_cons(
		struct usb_bam_connect_ipa_params *ipa_params,
		enum usb_ctrl cur_bam)
{
	int ret;
	u8 idx = 0;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect;
	struct sps_pipe *pipe;
	u32 timeout = 50, pipe_empty;
	struct usb_bam_sps_type usb_bam_sps = ctx->usb_bam_sps;
	struct sps_connect *sps_connection;
	bool inject_zlt = true;

	idx = ipa_params->src_idx;
	pipe = ctx->usb_bam_sps.sps_pipes[idx];
	pipe_connect = &ctx->usb_bam_connections[idx];
	sps_connection = &usb_bam_sps.sps_connections[idx];

	pipe_connect->activity_notify = NULL;
	pipe_connect->inactivity_notify = NULL;
	pipe_connect->priv = NULL;

	/*
	 * On some platforms, there is a chance that flow control
	 * is disabled from IPA side, due to this IPA core may not
	 * consume data from USB. Hence notify IPA to enable flow
	 * control and then check sps pipe is empty or not before
	 * processing USB->IPA pipes disconnect.
	 */
	ipa_clear_endpoint_delay(ipa_params->cons_clnt_hdl);
retry:
	/* Make sure pipe is empty before disconnecting it */
	while (1) {
		ret = sps_is_pipe_empty(pipe, &pipe_empty);
		if (ret) {
			log_event_err("%s: sps_is_pipe_empty failed with %d\n",
			       __func__, ret);
			return ret;
		}
		if (pipe_empty || !--timeout)
			break;

		/* Check again */
		usleep_range(1000, 2000);
	}

	if (!pipe_empty) {
		if (inject_zlt) {
			pr_debug("%s: Inject ZLT\n", __func__);
			log_event_dbg("%s: Inject ZLT\n", __func__);
			inject_zlt = false;
			sps_pipe_inject_zlt(sps_connection->destination,
					sps_connection->dest_pipe_index);
			timeout = 10;
			goto retry;
		}
		log_event_err("%s: src pipe(USB) not empty, wait timed out!\n",
								__func__);
		sps_get_bam_debug_info(ctx->h_bam, 93,
				(SPS_BAM_PIPE(0) | SPS_BAM_PIPE(1)), 0, 2);
		ipa_bam_reg_dump();
		panic("%s:SPS pipe not empty for USB->IPA\n", __func__);
	}

	/* Do the release handshake with the IPA via RM */
	spin_lock(&usb_bam_ipa_handshake_info_lock);
	info[cur_bam].connect_complete = 0;
	info[cur_bam].disconnected = 1;
	spin_unlock(&usb_bam_ipa_handshake_info_lock);

	/* Start release handshake on the last USB BAM producer pipe */
	if (info[cur_bam].prod_pipes_enabled_per_bam == 1)
		wait_for_prod_release(cur_bam);

	/* close USB -> IPA pipe */
	if (pipe_connect->pipe_type == USB_BAM_PIPE_BAM2BAM) {
		ret = ipa_disconnect(ipa_params->cons_clnt_hdl);
		if (ret) {
			log_event_err("%s: src pipe disconnection failure\n",
					__func__);
			return ret;
		}

		ret = usb_bam_disconnect_pipe(cur_bam, idx);
		if (ret) {
			log_event_err("%s: failure to disconnect pipe %d\n",
				__func__, idx);
			return ret;
		}
	} else {
		ret = ipa_teardown_sys_pipe(ipa_params->cons_clnt_hdl);
		if (ret) {
			log_event_err("%s: src pipe disconnection failure\n",
					__func__);
			return ret;
		}

		pipe_connect->enabled = false;
		spin_lock(&ctx->usb_bam_lock);
		if (ctx->pipes_enabled_per_bam == 0)
			log_event_err("%s: wrong pipes enabled counter for bam=%d\n",
				__func__, pipe_connect->bam_type);
		else
			ctx->pipes_enabled_per_bam -= 1;
		spin_unlock(&ctx->usb_bam_lock);
	}

	pipe_connect->ipa_clnt_hdl = -1;
	info[cur_bam].prod_pipes_enabled_per_bam -= 1;

	return 0;
}

int usb_bam_connect(enum usb_ctrl cur_bam, int idx, u32 *bam_pipe_idx)
{
	int ret;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect =
				&ctx->usb_bam_connections[idx];
	struct device *bam_dev = &ctx->usb_bam_pdev->dev;
	struct msm_usb_bam_platform_data *pdata = bam_dev->platform_data;
	enum usb_bam_mode cur_mode;

	if (pipe_connect->enabled) {
		pr_warning("%s: connection %d was already established\n"
				   , __func__, idx);
		return 0;
	}

	if (!bam_pipe_idx) {
		log_event_err("%s: invalid bam_pipe_idx\n", __func__);
		return -EINVAL;
	}
	if (idx < 0 || idx > ctx->max_connections) {
		log_event_err("idx is wrong %d\n", idx);
		return -EINVAL;
	}

	cur_mode = pipe_connect->bam_mode;

	log_event_dbg("%s: PM Runtime GET %d, count: %d\n",
			__func__, idx, get_pm_runtime_counter(bam_dev));
	pm_runtime_get_sync(bam_dev);

	spin_lock(&ctx->usb_bam_lock);
	/* Check if BAM requires RESET before connect and reset of first pipe */
	if ((pdata->reset_on_connect == true) &&
			    (ctx->pipes_enabled_per_bam == 0)) {
		spin_unlock(&ctx->usb_bam_lock);

		if (cur_bam == CI_CTRL)
			msm_hw_bam_disable(1);

		sps_device_reset(ctx->h_bam);

		if (cur_bam == CI_CTRL)
			msm_hw_bam_disable(0);

		spin_lock(&ctx->usb_bam_lock);
	}
	spin_unlock(&ctx->usb_bam_lock);

	/* Set the BAM mode (host/device) according to connected pipe */
	info[cur_bam].cur_bam_mode = pipe_connect->bam_mode;

	ret = connect_pipe(cur_bam, idx, bam_pipe_idx);
	if (ret) {
		log_event_err("%s: pipe connection[%d] failure\n",
				__func__, idx);
		log_event_dbg("%s: err, PM RT PUT %d, count: %d\n",
			__func__, idx, get_pm_runtime_counter(bam_dev));
		pm_runtime_put_sync(bam_dev);
		return ret;
	}
	log_event_dbg("%s: pipe connection[%d] success\n", __func__, idx);
	pipe_connect->enabled = 1;
	spin_lock(&ctx->usb_bam_lock);
	ctx->pipes_enabled_per_bam += 1;
	spin_unlock(&ctx->usb_bam_lock);

	return 0;
}

static int __sps_reset_pipe(enum usb_ctrl bam_type,
			    struct sps_pipe *pipe, u32 idx)
{
	int ret;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct sps_connect *sps_connection =
		&ctx->usb_bam_sps.sps_connections[idx];

	ret = sps_disconnect(pipe);
	if (ret) {
		log_event_err("%s: sps_disconnect() failed %d\n",
				__func__, ret);
		return ret;
	}

	ret = sps_connect(pipe, sps_connection);
	if (ret < 0) {
		log_event_err("%s: sps_connect() failed %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void reset_pipe_for_resume(struct usb_bam_pipe_connect *pipe_connect)
{
	int ret;
	enum usb_ctrl bam_type = pipe_connect->bam_type;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	u32 idx = ARRAY_INDEX_FROM_ADDR(ctx->usb_bam_connections, pipe_connect);
	struct sps_pipe *pipe = ctx->usb_bam_sps.sps_pipes[idx];

	if (!pipe_connect->reset_pipe_after_lpm ||
			pipe_connect->pipe_type != USB_BAM_PIPE_BAM2BAM) {
		log_event_dbg("No need to reset pipe %d\n", idx);
		return;
	}

	ret = __sps_reset_pipe(bam_type, pipe, idx);
	if (ret) {
		log_event_err("%s failed to reset the USB sps pipe\n",
				__func__);
		return;
	}

	ret = ipa_reset_endpoint(pipe_connect->ipa_clnt_hdl);
	if (ret) {
		log_event_err("%s failed to reset the IPA pipe\n", __func__);
		return;
	}
	log_event_dbg("%s: USB/IPA pipes reset after resume\n", __func__);
}

/* Stop PROD transfers in case they were started */
static void stop_prod_transfers(struct usb_bam_pipe_connect *pipe_connect)
{
	if (pipe_connect->stop && !pipe_connect->prod_stopped) {
		log_event_dbg("%s: Stop PROD transfers on\n", __func__);
		pipe_connect->stop(pipe_connect->start_stop_param,
				  USB_TO_PEER_PERIPHERAL);
		pipe_connect->prod_stopped = true;
	}
}

static void start_prod_transfers(struct usb_bam_pipe_connect *pipe_connect)
{
	log_event_err("%s: Starting PROD\n", __func__);
	if (pipe_connect->start && pipe_connect->prod_stopped) {
		log_event_dbg("%s: Enqueue PROD transfer\n", __func__);
		pipe_connect->start(pipe_connect->start_stop_param,
			  USB_TO_PEER_PERIPHERAL);
		pipe_connect->prod_stopped = false;
	}
}

static void start_cons_transfers(struct usb_bam_pipe_connect *pipe_connect)
{
	/* Start CONS transfer */
	if (pipe_connect->start && pipe_connect->cons_stopped) {
		log_event_dbg("%s: Enqueue CONS transfer\n", __func__);
		pipe_connect->start(pipe_connect->start_stop_param,
					PEER_PERIPHERAL_TO_USB);
		pipe_connect->cons_stopped = 0;
	}
}

/* Stop CONS transfers in case they were started */
static void stop_cons_transfers(struct usb_bam_pipe_connect *pipe_connect)
{
	if (pipe_connect->stop && !pipe_connect->cons_stopped) {
		log_event_dbg("%s: Stop CONS transfers\n", __func__);
		pipe_connect->stop(pipe_connect->start_stop_param,
				  PEER_PERIPHERAL_TO_USB);
		pipe_connect->cons_stopped = 1;
	}
}

static void resume_suspended_pipes(enum usb_ctrl cur_bam)
{
	u32 idx, dst_idx;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect;

	log_event_dbg("Resuming: suspend pipes =%d\n",
			info[cur_bam].pipes_suspended);

	while (info[cur_bam].pipes_suspended >= 1) {
		idx = info[cur_bam].pipes_suspended - 1;
		dst_idx = info[cur_bam].resume_dst_idx[idx];
		pipe_connect = &ctx->usb_bam_connections[dst_idx];
		if (pipe_connect->cons_stopped) {
			log_event_dbg("%s: Starting CONS on %d\n", __func__,
					dst_idx);
			start_cons_transfers(pipe_connect);
		}

		log_event_dbg("%s: Starting PROD on %d\n", __func__, dst_idx);
		start_prod_transfers(pipe_connect);
		info[cur_bam].pipes_suspended--;
		info[cur_bam].pipes_resumed++;
		/* Suspend was aborted, renew pm_runtime vote */
		log_event_dbg("%s: PM Runtime GET %d, count: %d\n", __func__,
			  idx, get_pm_runtime_counter(&ctx->usb_bam_pdev->dev));
		pm_runtime_get(&ctx->usb_bam_pdev->dev);
	}
}

static inline int all_pipes_suspended(enum usb_ctrl cur_bam)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];

	log_event_dbg("%s: pipes_suspended=%d pipes_enabled_per_bam=%d\n",
			__func__, info[cur_bam].pipes_suspended,
			ctx->pipes_enabled_per_bam);

	return info[cur_bam].pipes_suspended == ctx->pipes_enabled_per_bam;
}

static void usb_bam_finish_suspend(enum usb_ctrl cur_bam)
{
	int ret, bam2bam;
	u32 cons_empty, idx, dst_idx;
	struct sps_pipe *cons_pipe;
	struct usb_bam_pipe_connect *pipe_connect;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct device *bam_dev = &ctx->usb_bam_pdev->dev;

	mutex_lock(&info[cur_bam].suspend_resume_mutex);

	spin_lock(&usb_bam_ipa_handshake_info_lock);
	/* If cable was disconnected, let disconnection seq do everything */
	if (info[cur_bam].disconnected || all_pipes_suspended(cur_bam)) {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		mutex_unlock(&info[cur_bam].suspend_resume_mutex);
		log_event_dbg("%s: Cable disconnected\n", __func__);
		return;
	}
	log_event_dbg("%s: bam:%s RT GET: %d\n", __func__,
		  bam_enable_strings[cur_bam], get_pm_runtime_counter(bam_dev));
	pm_runtime_get(bam_dev);

	/* If resume was called don't finish this work */
	if (!info[cur_bam].bus_suspend) {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		log_event_dbg("%s: Bus resume in progress\n", __func__);
		goto no_lpm;
	}

	/* Go over all pipes, stop and suspend them, and go to lpm */
	while (!all_pipes_suspended(cur_bam)) {
		idx = info[cur_bam].pipes_suspended;
		dst_idx = info[cur_bam].suspend_dst_idx[idx];
		cons_pipe = ctx->usb_bam_sps.sps_pipes[dst_idx];
		pipe_connect = &ctx->usb_bam_connections[dst_idx];

		log_event_dbg("pipes_suspended=%d pipes_to_suspend=%d\n",
			info[cur_bam].pipes_suspended,
			info[cur_bam].pipes_to_suspend);

		bam2bam = (pipe_connect->pipe_type == USB_BAM_PIPE_BAM2BAM);

		spin_unlock(&usb_bam_ipa_handshake_info_lock);

		if (bam2bam) {
			ret = sps_is_pipe_empty(cons_pipe, &cons_empty);
			if (ret) {
				log_event_err("%s: sps_is_pipe_empty failed with %d\n",
					__func__, ret);
				goto no_lpm;
			}
		} else {
			log_event_err("%s: pipe type is not B2B\n", __func__);
			cons_empty = true;
		}

		spin_lock(&usb_bam_ipa_handshake_info_lock);
		/* Stop CONS transfers and go to lpm if no more data in the */
		/* pipes */
		if (cons_empty) {
			log_event_dbg("%s: Stopping CONS transfers on dst_idx=%d\n"
				, __func__, dst_idx);
			stop_cons_transfers(pipe_connect);

			spin_unlock(&usb_bam_ipa_handshake_info_lock);
			log_event_dbg("%s: Suspending pipe\n", __func__);
			spin_lock(&usb_bam_ipa_handshake_info_lock);
			info[cur_bam].resume_src_idx[idx] =
				info[cur_bam].suspend_src_idx[idx];
			info[cur_bam].resume_dst_idx[idx] =
				info[cur_bam].suspend_dst_idx[idx];
			info[cur_bam].pipes_suspended++;

			log_event_dbg("%s: PM Runtime PUT %d, count: %d\n",
				__func__, idx, get_pm_runtime_counter(bam_dev));
			pm_runtime_put(&ctx->usb_bam_pdev->dev);
		} else {
			log_event_dbg("%s: Pipe is not empty, not going to LPM\n",
				 __func__);
			spin_unlock(&usb_bam_ipa_handshake_info_lock);
			goto no_lpm;
		}
	}
	info[cur_bam].pipes_to_suspend = 0;
	info[cur_bam].pipes_resumed = 0;
	spin_unlock(&usb_bam_ipa_handshake_info_lock);

	/* ACK on the last pipe */
	if (info[cur_bam].pipes_suspended == ctx->pipes_enabled_per_bam &&
	     info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_RELEASED) {
		ipa_rm_notify_completion(
			IPA_RM_RESOURCE_RELEASED,
			ipa_rm_resource_cons[cur_bam]);
	}

	log_event_dbg("%s: Starting LPM on Bus Suspend, RT PUT:%d\n", __func__,
			get_pm_runtime_counter(bam_dev));
	/* Put to match _get at the beginning of this routine */
	pm_runtime_put_sync(bam_dev);

	mutex_unlock(&info[cur_bam].suspend_resume_mutex);

	return;

no_lpm:

	spin_lock(&usb_bam_ipa_handshake_info_lock);
	resume_suspended_pipes(cur_bam);
	info[cur_bam].pipes_resumed = 0;
	info[cur_bam].pipes_to_suspend = 0;
	info[cur_bam].pipes_suspended = 0;
	spin_unlock(&usb_bam_ipa_handshake_info_lock);
	/* Finish the handshake. Resume Sequence will start automatically
	   by the data in the pipes */
	if (info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_RELEASED)
		ipa_rm_notify_completion(IPA_RM_RESOURCE_RELEASED,
				ipa_rm_resource_cons[cur_bam]);

	/* Put to match _get at the beginning of this routine */
	pm_runtime_put(bam_dev);

	mutex_unlock(&info[cur_bam].suspend_resume_mutex);
}

void usb_bam_finish_suspend_(struct work_struct *w)
{
	enum usb_ctrl cur_bam;
	struct usb_bam_ipa_handshake_info *info_ptr;

	info_ptr = container_of(w, struct usb_bam_ipa_handshake_info,
			finish_suspend_work);
	cur_bam = info_ptr->cur_bam_mode;

	log_event_dbg("%s: Finishing suspend sequence(BAM=%s)\n", __func__,
			bam_enable_strings[cur_bam]);
	usb_bam_finish_suspend(cur_bam);
}

static void usb_prod_notify_cb(void *user_data, enum ipa_rm_event event,
	unsigned long data)
{
	enum usb_ctrl *cur_bam = (void *)user_data;

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		log_event_dbg("%s: %s_PROD resource granted\n",
			__func__, bam_enable_strings[*cur_bam]);
		info[*cur_bam].cur_prod_state = IPA_RM_RESOURCE_GRANTED;
		complete_all(&info[*cur_bam].prod_avail);
		break;
	case IPA_RM_RESOURCE_RELEASED:
		log_event_dbg("%s: %s_PROD resource released\n",
			__func__, bam_enable_strings[*cur_bam]);
		info[*cur_bam].cur_prod_state = IPA_RM_RESOURCE_RELEASED;
		complete_all(&info[*cur_bam].prod_released);
		break;
	default:
		break;
	}
	return;
}

/**
 * usb_bam_resume_host: vote for hsic host core resume.
 *
 * NOTE: This function should be called in a context that hold
 *	 usb_bam_lock.
 */
static void usb_bam_resume_host(enum usb_ctrl bam_type)
{
	int i;
	struct usb_bam_pipe_connect *pipe_iter;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];

	log_event_dbg("%s: enter bam=%s\n", __func__,
			bam_enable_strings[bam_type]);

	if (usb_bam_resume_core(bam_type, USB_BAM_HOST)) {
		for (i = 0; i < ctx->max_connections; i++) {
			pipe_iter = &ctx->usb_bam_connections[i];
			if (pipe_iter->enabled && pipe_iter->suspended)
				pipe_iter->suspended = false;
		}
	}
}

static int cons_request_resource(enum usb_ctrl cur_bam)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	int ret = -EINPROGRESS;

	log_event_dbg("%s: Request %s_CONS resource\n",
			__func__, bam_enable_strings[cur_bam]);

	spin_lock(&ctx->usb_bam_lock);
	spin_lock(&usb_bam_ipa_handshake_info_lock);
	info[cur_bam].cur_cons_state = IPA_RM_RESOURCE_GRANTED;

	switch (info[cur_bam].cur_bam_mode) {
	case USB_BAM_DEVICE:
		if (ctx->pipes_enabled_per_bam &&
		    info[cur_bam].connect_complete) {
			if (!all_pipes_suspended(cur_bam) &&
				!info[cur_bam].bus_suspend) {
				log_event_dbg("%s: ACK on cons_request\n",
						__func__);
				ret = 0;
			} else if (info[cur_bam].bus_suspend) {
				info[cur_bam].bus_suspend = 0;
				log_event_dbg("%s: Wake up host\n", __func__);
				if (info[cur_bam].wake_cb)
					info[cur_bam].wake_cb(
						info[cur_bam].wake_param);
			}
		}

		break;
	case USB_BAM_HOST:
		/*
		 * Vote for hsic resume, however the core
		 * resume may not be completed yet or on the other hand
		 * hsic core might already be resumed, due to a vote
		 * by other driver, in this case we will just renew our
		 * vote here.
		 */
		usb_bam_resume_host(cur_bam);

		/*
		 * Return sucess if there are pipes connected
		 * and hsic core is actually not in lpm.
		 * If in lpm, grant will occur on resume
		 * finish (see msm_bam_hsic_notify_on_resume)
		 */
		if (ctx->pipes_enabled_per_bam && !host_info[cur_bam].in_lpm)
			ret = 0;

		break;

	default:
		break;
	}

	spin_unlock(&usb_bam_ipa_handshake_info_lock);
	spin_unlock(&ctx->usb_bam_lock);

	if (ret == -EINPROGRESS)
		log_event_dbg("%s: EINPROGRESS on cons_request\n", __func__);

	return ret;
}

static int ss_usb_cons_request_resource(void)
{
	return cons_request_resource(DWC3_CTRL);
}


static int usb_cons_request_resource(void)
{
	return cons_request_resource(CI_CTRL);
}

static int hsic_cons_request_resource(void)
{
	return cons_request_resource(HSIC_CTRL);
}

static int cons_release_resource(enum usb_ctrl cur_bam)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];

	log_event_dbg("%s: Release %s_CONS resource\n",
			__func__, bam_enable_strings[cur_bam]);

	info[cur_bam].cur_cons_state = IPA_RM_RESOURCE_RELEASED;

	spin_lock(&ctx->usb_bam_lock);
	if (!ctx->pipes_enabled_per_bam) {
		spin_unlock(&ctx->usb_bam_lock);
		log_event_dbg("%s: ACK on cons_release\n", __func__);
		return 0;
	}
	spin_unlock(&ctx->usb_bam_lock);

	if (info[cur_bam].cur_bam_mode == USB_BAM_DEVICE) {
		spin_lock(&usb_bam_ipa_handshake_info_lock);
		if (info[cur_bam].bus_suspend) {
			queue_work(ctx->usb_bam_wq,
				   &info[cur_bam].finish_suspend_work);
		}
		spin_unlock(&usb_bam_ipa_handshake_info_lock);

		log_event_dbg("%s: EINPROGRESS cons_release\n", __func__);
		return -EINPROGRESS;
	} else if (info[cur_bam].cur_bam_mode == USB_BAM_HOST) {
		/*
		 * Allow to go to lpm for now. Actual state will be checked
		 * in msm_bam_hsic_lpm_ok() / msm_bam_lpm_ok() just before
		 * going to lpm.
		 */
		usb_bam_suspend_core(cur_bam, info[cur_bam].cur_bam_mode, 1);
	}

	return 0;
}

static int hsic_cons_release_resource(void)
{
	return cons_release_resource(HSIC_CTRL);
}

static int usb_cons_release_resource(void)
{
	return cons_release_resource(CI_CTRL);
}

static int ss_usb_cons_release_resource(void)
{
	return cons_release_resource(DWC3_CTRL);
}

static void usb_bam_ipa_create_resources(enum usb_ctrl cur_bam)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct msm_usb_bam_platform_data *pdata =
					ctx->usb_bam_pdev->dev.platform_data;
	struct ipa_rm_create_params usb_prod_create_params;
	struct ipa_rm_create_params usb_cons_create_params;
	int ret;

	/* Create USB/HSIC_PROD entity */
	memset(&usb_prod_create_params, 0, sizeof(usb_prod_create_params));
	usb_prod_create_params.name = ipa_rm_resource_prod[cur_bam];
	usb_prod_create_params.reg_params.notify_cb = usb_prod_notify_cb;
	usb_prod_create_params.reg_params.user_data = &pdata->bam_type;
	usb_prod_create_params.floor_voltage = IPA_VOLTAGE_SVS;
	ret = ipa_rm_create_resource(&usb_prod_create_params);
	if (ret) {
		log_event_err("%s: Failed to create USB_PROD resource\n",
							__func__);
		return;
	}

	/* Create USB_CONS entity */
	memset(&usb_cons_create_params, 0, sizeof(usb_cons_create_params));
	usb_cons_create_params.name = ipa_rm_resource_cons[cur_bam];
	usb_cons_create_params.request_resource = request_resource_cb[cur_bam];
	usb_cons_create_params.release_resource = release_resource_cb[cur_bam];
	usb_cons_create_params.floor_voltage = IPA_VOLTAGE_SVS;
	ret = ipa_rm_create_resource(&usb_cons_create_params);
	if (ret) {
		log_event_err("%s: Failed to create USB_CONS resource\n",
				__func__);
		return;
	}
}

static void wait_for_prod_granted(enum usb_ctrl cur_bam)
{
	int ret;

	log_event_dbg("%s Request %s_PROD_RES\n", __func__,
		bam_enable_strings[cur_bam]);
	if (info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_GRANTED)
		log_event_dbg("%s: CONS already granted for some reason\n",
			__func__);
	if (info[cur_bam].cur_prod_state == IPA_RM_RESOURCE_GRANTED)
		log_event_dbg("%s: PROD already granted for some reason\n",
			__func__);

	init_completion(&info[cur_bam].prod_avail);

	ret = ipa_rm_request_resource(ipa_rm_resource_prod[cur_bam]);
	if (!ret) {
		info[cur_bam].cur_prod_state = IPA_RM_RESOURCE_GRANTED;
		complete_all(&info[cur_bam].prod_avail);
		log_event_dbg("%s: PROD_GRANTED without wait\n", __func__);
	} else if (ret == -EINPROGRESS) {
		log_event_dbg("%s: Waiting for PROD_GRANTED\n", __func__);
		if (!wait_for_completion_timeout(&info[cur_bam].prod_avail,
			USB_BAM_TIMEOUT))
			log_event_err("%s: Timeout wainting for PROD_GRANTED\n",
				__func__);
	} else
		log_event_err("%s: ipa_rm_request_resource ret =%d\n",
				__func__, ret);
}

void notify_usb_connected(enum usb_ctrl cur_bam)
{
	log_event_dbg("%s: enter\n", __func__);

	spin_lock(&usb_bam_ipa_handshake_info_lock);
	if (info[cur_bam].cur_bam_mode == USB_BAM_DEVICE)
		info[cur_bam].connect_complete = 1;
	spin_unlock(&usb_bam_ipa_handshake_info_lock);

	if (info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_GRANTED) {
		log_event_dbg("%s: Notify %s CONS_GRANTED\n", __func__,
				bam_enable_strings[cur_bam]);
		ipa_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
				 ipa_rm_resource_cons[cur_bam]);
	}
}

static void wait_for_prod_release(enum usb_ctrl cur_bam)
{
	int ret;

	if (info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_RELEASED)
		log_event_dbg("%s consumer already released\n", __func__);
	 if (info[cur_bam].cur_prod_state == IPA_RM_RESOURCE_RELEASED)
		log_event_dbg("%s producer already released\n", __func__);

	init_completion(&info[cur_bam].prod_released);
	log_event_dbg("%s: Releasing %s_PROD\n", __func__,
				bam_enable_strings[cur_bam]);
	ret = ipa_rm_release_resource(ipa_rm_resource_prod[cur_bam]);
	if (!ret) {
		log_event_dbg("%s: Released without waiting\n", __func__);
		info[cur_bam].cur_prod_state = IPA_RM_RESOURCE_RELEASED;
		complete_all(&info[cur_bam].prod_released);
	} else if (ret == -EINPROGRESS) {
		log_event_dbg("%s: Waiting for PROD_RELEASED\n", __func__);
		if (!wait_for_completion_timeout(&info[cur_bam].prod_released,
						USB_BAM_TIMEOUT))
			log_event_err("%s: Timeout waiting for PROD_RELEASED\n",
			__func__);
	} else {
		log_event_err("%s: ipa_rm_request_resource ret =%d\n",
				__func__, ret);
	}
}

static bool check_pipes_empty(enum usb_ctrl bam_type, u8 src_idx, u8 dst_idx)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct sps_pipe *prod_pipe, *cons_pipe;
	struct usb_bam_pipe_connect *prod_pipe_connect, *cons_pipe_connect;
	u32 prod_empty, cons_empty;

	prod_pipe_connect = &ctx->usb_bam_connections[src_idx];
	cons_pipe_connect = &ctx->usb_bam_connections[dst_idx];
	if (!prod_pipe_connect->enabled || !cons_pipe_connect->enabled) {
		log_event_err("%s: pipes are not enabled dst=%d src=%d\n",
				__func__, prod_pipe_connect->enabled,
				cons_pipe_connect->enabled);
	}

	/* If we have any remaints in the pipes we don't go to sleep */
	prod_pipe = ctx->usb_bam_sps.sps_pipes[src_idx];
	cons_pipe = ctx->usb_bam_sps.sps_pipes[dst_idx];
	log_event_dbg("prod_pipe=%pK, cons_pipe=%pK\n", prod_pipe, cons_pipe);

	if (!cons_pipe || (!prod_pipe &&
			prod_pipe_connect->pipe_type == USB_BAM_PIPE_BAM2BAM)) {
		log_event_err("Missing a pipe!\n");
		return false;
	}

	if (prod_pipe && sps_is_pipe_empty(prod_pipe, &prod_empty)) {
		log_event_err("sps_is_pipe_empty(prod) failed\n");
		return false;
	} else {
		prod_empty = true;
	}

	if (sps_is_pipe_empty(cons_pipe, &cons_empty)) {
		log_event_err("sps_is_pipe_empty(cons) failed\n");
		return false;
	}

	if (!prod_empty || !cons_empty) {
		log_event_err("pipes not empty prod=%d cond=%d\n",
			prod_empty, cons_empty);
		return false;
	}

	return true;

}

void usb_bam_suspend(enum usb_ctrl cur_bam,
		     struct usb_bam_connect_ipa_params *ipa_params)
{
	struct usb_bam_pipe_connect *pipe_connect;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	enum usb_bam_mode bam_mode;
	u8 src_idx, dst_idx;

	log_event_dbg("%s: enter\n", __func__);

	if (!ipa_params) {
		log_event_err("%s: Invalid ipa params\n", __func__);
		return;
	}

	src_idx = ipa_params->src_idx;
	dst_idx = ipa_params->dst_idx;

	if (src_idx >= ctx->max_connections ||
				dst_idx >= ctx->max_connections) {
		log_event_err("%s: Invalid connection index src=%d dst=%d\n",
			__func__, src_idx, dst_idx);
	}

	pipe_connect = &ctx->usb_bam_connections[src_idx];
	bam_mode = pipe_connect->bam_mode;
	if (bam_mode != USB_BAM_DEVICE)
		return;

	log_event_dbg("%s: Starting suspend sequence(BAM=%s)\n", __func__,
			bam_enable_strings[cur_bam]);

	spin_lock(&usb_bam_ipa_handshake_info_lock);
	info[cur_bam].bus_suspend = 1;

	/* If cable was disconnected, let disconnection seq do everything */
	if (info[cur_bam].disconnected) {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		log_event_dbg("%s: Cable disconnected\n", __func__);
		return;
	}

	log_event_dbg("%s: Adding src=%d dst=%d in pipes_to_suspend=%d\n",
			__func__, src_idx,
			dst_idx, info[cur_bam].pipes_to_suspend);
	info[cur_bam].suspend_src_idx[info[cur_bam].pipes_to_suspend] = src_idx;
	info[cur_bam].suspend_dst_idx[info[cur_bam].pipes_to_suspend] = dst_idx;
	info[cur_bam].pipes_to_suspend++;

	spin_unlock(&usb_bam_ipa_handshake_info_lock);

	usb_bam_start_suspend(&info[cur_bam]);
}

static void usb_bam_start_suspend(struct usb_bam_ipa_handshake_info *info_ptr)
{
	struct usb_bam_pipe_connect *pipe_connect;
	enum usb_ctrl cur_bam = info_ptr->bam_type;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	u8 src_idx, dst_idx;
	int pipes_to_suspend;

	cur_bam = info_ptr->bam_type;
	log_event_dbg("%s: Starting suspend sequence(BAM=%s)\n", __func__,
			bam_enable_strings[cur_bam]);

	mutex_lock(&info[cur_bam].suspend_resume_mutex);

	spin_lock(&usb_bam_ipa_handshake_info_lock);
	/* If cable was disconnected, let disconnection seq do everything */
	if (info[cur_bam].disconnected) {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		mutex_unlock(&info[cur_bam].suspend_resume_mutex);
		log_event_dbg("%s: Cable disconnected\n", __func__);
		return;
	}

	pipes_to_suspend = info[cur_bam].pipes_to_suspend;
	if (!info[cur_bam].bus_suspend || !pipes_to_suspend) {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		log_event_dbg("%s: Resume started, not suspending\n", __func__);
		mutex_unlock(&info[cur_bam].suspend_resume_mutex);
		return;
	}

	src_idx = info[cur_bam].suspend_src_idx[pipes_to_suspend - 1];
	dst_idx = info[cur_bam].suspend_dst_idx[pipes_to_suspend - 1];

	pipe_connect = &ctx->usb_bam_connections[dst_idx];
	stop_prod_transfers(pipe_connect);

	spin_unlock(&usb_bam_ipa_handshake_info_lock);

	/* Don't start LPM seq if data in the pipes */
	if (!check_pipes_empty(cur_bam, src_idx, dst_idx)) {
		start_prod_transfers(pipe_connect);
		info[cur_bam].pipes_to_suspend = 0;
		info[cur_bam].bus_suspend = 0;
		mutex_unlock(&info[cur_bam].suspend_resume_mutex);
		return;
	}

	spin_lock(&usb_bam_ipa_handshake_info_lock);

	/* Start release handshake on the last pipe */
	if (info[cur_bam].pipes_to_suspend * 2 == ctx->pipes_enabled_per_bam) {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		wait_for_prod_release(cur_bam);
	} else {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
	}

	mutex_unlock(&info[cur_bam].suspend_resume_mutex);
	if (info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_RELEASED)
		usb_bam_finish_suspend(cur_bam);
	else
		log_event_dbg("Consumer not released yet\n");
}

static void usb_bam_finish_resume(struct work_struct *w)
{
	/* TODO: Change this when HSIC device support is introduced */
	enum usb_ctrl cur_bam;
	struct usb_bam_ipa_handshake_info *info_ptr;
	struct usb_bam_pipe_connect *pipe_connect;
	struct usb_bam_ctx_type *ctx;
	struct device *bam_dev;
	u32 idx, dst_idx, suspended;

	info_ptr = container_of(w, struct usb_bam_ipa_handshake_info,
			resume_work);
	cur_bam = info_ptr->bam_type;
	ctx = &msm_usb_bam[cur_bam];
	bam_dev = &ctx->usb_bam_pdev->dev;

	log_event_dbg("%s: enter bam=%s, RT GET: %d\n", __func__,
		  bam_enable_strings[cur_bam], get_pm_runtime_counter(bam_dev));

	pm_runtime_get_sync(bam_dev);

	mutex_lock(&info[cur_bam].suspend_resume_mutex);

	/* Suspend or disconnect happened in the meantime */
	spin_lock(&usb_bam_ipa_handshake_info_lock);
	if (info[cur_bam].bus_suspend || info[cur_bam].disconnected) {
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		log_event_dbg("%s: Bus suspended, not resuming, RT PUT: %d\n",
				__func__, get_pm_runtime_counter(bam_dev));
		mutex_unlock(&info[cur_bam].suspend_resume_mutex);
		pm_runtime_put_sync(bam_dev);
		return;
	}
	info[cur_bam].pipes_to_suspend = 0;

	log_event_dbg("Resuming: pipes_suspended =%d\n",
		 info[cur_bam].pipes_suspended);

	suspended = info[cur_bam].pipes_suspended;
	while (suspended >= 1) {
		idx = suspended - 1;
		dst_idx = info[cur_bam].resume_dst_idx[idx];
		pipe_connect = &ctx->usb_bam_connections[dst_idx];
		spin_unlock(&usb_bam_ipa_handshake_info_lock);
		reset_pipe_for_resume(pipe_connect);
		spin_lock(&usb_bam_ipa_handshake_info_lock);
		if (pipe_connect->cons_stopped) {
			log_event_dbg("%s: Starting CONS on %d\n", __func__,
					dst_idx);
			start_cons_transfers(pipe_connect);
		}
		suspended--;
	}
	if (info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_GRANTED) {
		log_event_dbg("%s: Notify CONS_GRANTED\n", __func__);
		ipa_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
				 ipa_rm_resource_cons[cur_bam]);
	}
	spin_unlock(&usb_bam_ipa_handshake_info_lock);

	/* Start handshake for the first pipe resumed */
	if (info[cur_bam].pipes_resumed == 0)
		wait_for_prod_granted(cur_bam);

	spin_lock(&usb_bam_ipa_handshake_info_lock);
	while (info[cur_bam].pipes_suspended >= 1) {
		idx = info[cur_bam].pipes_suspended - 1;
		dst_idx = info[cur_bam].resume_dst_idx[idx];
		pipe_connect = &ctx->usb_bam_connections[dst_idx];
		log_event_dbg("%s: Starting PROD on %d\n", __func__, dst_idx);
		start_prod_transfers(pipe_connect);
		info[cur_bam].pipes_suspended--;
		info[cur_bam].pipes_resumed++;
		log_event_dbg("%s: PM Runtime GET %d, count: %d\n",
			  __func__, idx, get_pm_runtime_counter(bam_dev));
		pm_runtime_get(&ctx->usb_bam_pdev->dev);
	}

	if (info[cur_bam].pipes_resumed == ctx->pipes_enabled_per_bam) {
		info[cur_bam].pipes_resumed = 0;
		if (info[cur_bam].cur_cons_state == IPA_RM_RESOURCE_GRANTED) {
			log_event_dbg("%s: Notify CONS_GRANTED\n", __func__);
			ipa_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
						 ipa_rm_resource_cons[cur_bam]);
		}
	}

	spin_unlock(&usb_bam_ipa_handshake_info_lock);
	mutex_unlock(&info[cur_bam].suspend_resume_mutex);
	log_event_dbg("%s: done..PM Runtime PUT :%d\n",
			  __func__, get_pm_runtime_counter(bam_dev));
	/* Put to match _get at the beginning of this routine */
	pm_runtime_put(&ctx->usb_bam_pdev->dev);
}

void usb_bam_resume(enum usb_ctrl cur_bam,
		    struct usb_bam_connect_ipa_params *ipa_params)
{
	u8 src_idx, dst_idx;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect;

	log_event_dbg("%s: Resuming\n", __func__);

	if (!ipa_params) {
		log_event_err("%s: Invalid ipa params\n", __func__);
		return;
	}

	src_idx = ipa_params->src_idx;
	dst_idx = ipa_params->dst_idx;

	if (src_idx >= ctx->max_connections ||
			dst_idx >= ctx->max_connections) {
		log_event_err("%s: Invalid connection index src=%d dst=%d\n",
			__func__, src_idx, dst_idx);
		return;
	}

	pipe_connect = &ctx->usb_bam_connections[src_idx];
	log_event_dbg("%s: bam=%s mode =%d\n", __func__,
		bam_enable_strings[cur_bam], pipe_connect->bam_mode);
	if (pipe_connect->bam_mode != USB_BAM_DEVICE)
		return;

	info[cur_bam].in_lpm = false;
	spin_lock(&usb_bam_ipa_handshake_info_lock);
	info[cur_bam].bus_suspend = 0;
	spin_unlock(&usb_bam_ipa_handshake_info_lock);
	queue_work(ctx->usb_bam_wq, &info[cur_bam].resume_work);
}

static void _msm_bam_wait_for_host_prod_granted(enum usb_ctrl bam_type)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];

	spin_lock(&ctx->usb_bam_lock);

	log_event_dbg("%s: enter bam=%s\n", __func__,
			bam_enable_strings[bam_type]);
	ctx->is_bam_inactivity = false;

	/* Get back to resume state including wakeup ipa */
	usb_bam_resume_core(bam_type, USB_BAM_HOST);

	/* Ensure getting the producer resource */
	wait_for_prod_granted(bam_type);

	spin_unlock(&ctx->usb_bam_lock);

}

void msm_bam_wait_for_hsic_host_prod_granted(void)
{
	log_event_dbg("%s: start\n", __func__);
	_msm_bam_wait_for_host_prod_granted(HSIC_CTRL);
}

static void _msm_bam_host_notify_on_resume(enum usb_ctrl bam_type)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];

	spin_lock(&ctx->usb_bam_lock);
	log_event_dbg("%s: enter bam=%s\n", __func__,
			bam_enable_strings[bam_type]);

	host_info[bam_type].in_lpm = false;

	/* HSIC resume completed. Notify CONS grant if CONS was requested */
	notify_usb_connected(bam_type);

	/*
	 * This function is called to notify the usb bam driver
	 * that the hsic core and hsic bam hw are fully resumed
	 * and clocked on. Therefore we can now set the inactivity
	 * timer to the hsic bam hw.
	 */
	if (ctx->inactivity_timer_ms)
		usb_bam_set_inactivity_timer(bam_type);

	spin_unlock(&ctx->usb_bam_lock);
}

static bool msm_bam_host_lpm_ok(enum usb_ctrl bam_type)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct usb_bam_pipe_connect *pipe_iter;
	int i;

	log_event_dbg("%s: enter bam=%s\n", __func__,
			bam_enable_strings[bam_type]);

	if (!host_info[bam_type].dev)
		return true;

	log_event_dbg("%s: Starting hsic full suspend sequence\n",
		__func__);

	/*
	 * Start low power mode by releasing the device only if the resources
	 * were indeed released and we are still in inactivity state (wakeup
	 * event has not occurred while we were waiting for resources release
	 */
	spin_lock(&ctx->usb_bam_lock);

	if (info[bam_type].cur_cons_state == IPA_RM_RESOURCE_RELEASED &&
	    info[bam_type].cur_prod_state == IPA_RM_RESOURCE_RELEASED &&
	    ctx->is_bam_inactivity && info[bam_type].in_lpm) {

		pr_debug("%s(): checking HSIC Host pipe state\n", __func__);
		if (!msm_bam_hsic_host_pipe_empty()) {
			log_event_err("%s(): HSIC HOST Pipe is not empty\n",
					__func__);
			spin_unlock(&ctx->usb_bam_lock);
			return false;
		}

		/* HSIC host will go now to lpm */
		log_event_dbg("%s: vote for suspend hsic %pK\n",
			__func__, host_info[bam_type].dev);

		for (i = 0; i < ctx->max_connections; i++) {
			pipe_iter = &ctx->usb_bam_connections[i];
			if (pipe_iter->bam_type == bam_type &&
			    pipe_iter->enabled && !pipe_iter->suspended)
				pipe_iter->suspended = true;
		}

		host_info[bam_type].in_lpm = true;
		spin_unlock(&ctx->usb_bam_lock);

		return true;
	}

	/* We don't allow lpm, therefore renew our vote here */
	if (info[bam_type].in_lpm) {
		log_event_dbg("%s: Not allow lpm while ref count=0\n",
				__func__);
		log_event_dbg("%s: inactivity=%d, c_s=%d p_s=%d\n", __func__,
			  ctx->is_bam_inactivity, info[bam_type].cur_cons_state,
			  info[bam_type].cur_prod_state);
		pm_runtime_get(host_info[bam_type].dev);
		info[bam_type].in_lpm = false;
		spin_unlock(&ctx->usb_bam_lock);
	} else {
		spin_unlock(&ctx->usb_bam_lock);
	}

	return false;
}

void msm_bam_hsic_host_notify_on_resume(void)
{
	_msm_bam_host_notify_on_resume(HSIC_CTRL);
}

static int usb_bam_set_ipa_perf(enum usb_ctrl cur_bam,
			      enum usb_bam_pipe_dir dir,
			      enum usb_device_speed usb_connection_speed)
{
	int ret;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct ipa_rm_perf_profile ipa_rm_perf_prof;
	struct msm_usb_bam_platform_data *pdata =
					ctx->usb_bam_pdev->dev.platform_data;

	if (usb_connection_speed == USB_SPEED_SUPER)
		ipa_rm_perf_prof.max_supported_bandwidth_mbps =
			pdata->max_mbps_superspeed;
	else
		/* Bam2Bam is supported only for SS and HS (HW limitation) */
		ipa_rm_perf_prof.max_supported_bandwidth_mbps =
			pdata->max_mbps_highspeed;

	/*
	 * Having a max mbps property in dtsi file is a must
	 * for target with IPA capability.
	 */
	if (!ipa_rm_perf_prof.max_supported_bandwidth_mbps) {
		log_event_err("%s: Max mbps is required for speed %d\n",
				__func__, usb_connection_speed);
		return -EINVAL;
	}

	if (dir == USB_TO_PEER_PERIPHERAL) {
		log_event_dbg("%s: vote ipa_perf resource=%d perf=%d mbps\n",
			__func__, ipa_rm_resource_prod[cur_bam],
			ipa_rm_perf_prof.max_supported_bandwidth_mbps);
		ret = ipa_rm_set_perf_profile(ipa_rm_resource_prod[cur_bam],
					&ipa_rm_perf_prof);
	} else {
		log_event_dbg("%s: vote ipa_perf resource=%d perf=%d mbps\n",
			__func__, ipa_rm_resource_cons[cur_bam],
			ipa_rm_perf_prof.max_supported_bandwidth_mbps);
		ret = ipa_rm_set_perf_profile(ipa_rm_resource_cons[cur_bam],
					&ipa_rm_perf_prof);
	}

	return ret;
}

int usb_bam_connect_ipa(enum usb_ctrl cur_bam,
			struct usb_bam_connect_ipa_params *ipa_params)
{
	u8 idx;
	enum usb_bam_mode cur_mode;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect;
	struct device *bam_dev = &ctx->usb_bam_pdev->dev;
	struct msm_usb_bam_platform_data *pdata = bam_dev->platform_data;
	int ret;
	bool bam2bam, is_dpl;

	log_event_dbg("%s: start\n", __func__);

	if (!ipa_params) {
		log_event_err("%s: Invalid ipa params\n",
			__func__);
		return -EINVAL;
	}

	if (ipa_params->dir == USB_TO_PEER_PERIPHERAL)
		idx = ipa_params->src_idx;
	else
		idx = ipa_params->dst_idx;

	if (idx >= ctx->max_connections) {
		log_event_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}
	pipe_connect = &ctx->usb_bam_connections[idx];

	if (pipe_connect->enabled) {
		log_event_err("%s: connection %d was already established\n",
			__func__, idx);
		return 0;
	}

	ret = usb_bam_set_ipa_perf(pipe_connect->bam_type, ipa_params->dir,
			     ipa_params->usb_connection_speed);
	if (ret) {
		log_event_err("%s: call to usb_bam_set_ipa_perf failed %d\n",
			__func__, ret);
		return ret;
	}

	log_event_dbg("%s: enter\n", __func__);

	cur_mode = pipe_connect->bam_mode;
	bam2bam = (pipe_connect->pipe_type == USB_BAM_PIPE_BAM2BAM);

	if (ipa_params->dst_client == IPA_CLIENT_USB_DPL_CONS)
		is_dpl = true;
	else
		is_dpl = false;

	/* Set the BAM mode (host/device) according to connected pipe */
	info[cur_bam].cur_bam_mode = pipe_connect->bam_mode;

	if (cur_mode == USB_BAM_DEVICE) {
		mutex_lock(&info[cur_bam].suspend_resume_mutex);

		spin_lock(&ctx->usb_bam_lock);
		if (ctx->pipes_enabled_per_bam == 0) {
			spin_unlock(&ctx->usb_bam_lock);
			spin_lock(&usb_bam_ipa_handshake_info_lock);
			info[cur_bam].connect_complete = 0;
			info[cur_bam].disconnected = 0;
			info[cur_bam].bus_suspend = 0;
			info[cur_bam].pipes_suspended = 0;
			info[cur_bam].pipes_to_suspend = 0;
			info[cur_bam].pipes_resumed = 0;
			spin_unlock(&usb_bam_ipa_handshake_info_lock);
		} else {
			spin_unlock(&ctx->usb_bam_lock);
		}
		pipe_connect->cons_stopped = 0;
		pipe_connect->prod_stopped = 0;
	}

	log_event_dbg("%s: PM Runtime GET %d, count: %d\n",
		  __func__, idx, get_pm_runtime_counter(bam_dev));
	pm_runtime_get_sync(bam_dev);

	 /* Check if BAM requires RESET before connect and reset first pipe */
	 spin_lock(&ctx->usb_bam_lock);
	 if (pdata->reset_on_connect && !ctx->pipes_enabled_per_bam) {
		spin_unlock(&ctx->usb_bam_lock);

		if (cur_bam == CI_CTRL)
			msm_hw_bam_disable(1);

		sps_device_reset(ctx->h_bam);

		if (cur_bam == CI_CTRL)
			msm_hw_bam_disable(0);

		/* On re-connect assume out from lpm for HOST BAM */
		if (cur_mode == USB_BAM_HOST)
			usb_bam_resume_core(cur_bam, cur_mode);

		/* On re-connect assume out from lpm for all BAMs */
		info[cur_bam].in_lpm = false;
	} else {
		spin_unlock(&ctx->usb_bam_lock);
		if (!ctx->pipes_enabled_per_bam)
			pr_debug("No BAM reset on connect, just pipe reset\n");
	}

	if (ipa_params->dir == USB_TO_PEER_PERIPHERAL) {
		if (info[cur_bam].prod_pipes_enabled_per_bam == 0)
			wait_for_prod_granted(cur_bam);
		info[cur_bam].prod_pipes_enabled_per_bam += 1;
	}

	if (bam2bam)
		ret = connect_pipe_bam2bam_ipa(cur_bam, idx, ipa_params);
	else
		ret = connect_pipe_sys2bam_ipa(cur_bam, idx, ipa_params);
	if (ret) {
		log_event_err("%s: pipe connection failure RT PUT: %d\n",
				__func__, get_pm_runtime_counter(bam_dev));
		pm_runtime_put_sync(bam_dev);
		if (cur_mode == USB_BAM_DEVICE)
			mutex_unlock(&info[cur_bam].suspend_resume_mutex);
		return ret;
	}
	log_event_dbg("%s: pipe connection success\n", __func__);
	spin_lock(&ctx->usb_bam_lock);
	pipe_connect->enabled = 1;
	pipe_connect->suspended = 0;

	/* Set global inactivity timer upon first pipe connection */
	if (!ctx->pipes_enabled_per_bam && ctx->inactivity_timer_ms &&
			    pipe_connect->inactivity_notify && bam2bam)
		usb_bam_set_inactivity_timer(cur_bam);

	ctx->pipes_enabled_per_bam += 1;

	/*
	 * Notify USB connected on the first two pipes connected for
	 * tethered function's producer and consumer only. Current
	 * understanding is that there won't be more than 3 pipes used
	 * in USB BAM2BAM IPA mode i.e. 2 consumers and 1 producer.
	 * If more producer and consumer pipe are being used, this
	 * logic is required to be revisited here.
	 */
	if (ctx->pipes_enabled_per_bam >= 2 &&
			ipa_params->dir == PEER_PERIPHERAL_TO_USB && !is_dpl)
		notify_usb_connected(cur_bam);
	spin_unlock(&ctx->usb_bam_lock);

	if (cur_mode == USB_BAM_DEVICE)
		mutex_unlock(&info[cur_bam].suspend_resume_mutex);

	log_event_dbg("%s: done\n", __func__);

	return 0;
}
EXPORT_SYMBOL(usb_bam_connect_ipa);

int usb_bam_get_pipe_type(enum usb_ctrl bam_type, u8 idx,
			  enum usb_bam_pipe_type *type)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct usb_bam_pipe_connect *pipe_connect =
			 &ctx->usb_bam_connections[idx];

	if (idx >= ctx->max_connections) {
		log_event_err("%s: Invalid connection index\n", __func__);
		return -EINVAL;
	}
	if (!type) {
		log_event_err("%s: null pointer provided for type\n", __func__);
		return -EINVAL;
	} else {
		*type = pipe_connect->pipe_type;
	}

	return 0;
}
EXPORT_SYMBOL(usb_bam_get_pipe_type);

static void usb_bam_work(struct work_struct *w)
{
	int i;
	struct usb_bam_event_info *event_info =
		container_of(w, struct usb_bam_event_info, event_w);
	struct usb_bam_pipe_connect *pipe_connect =
		container_of(event_info, struct usb_bam_pipe_connect, event);
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[pipe_connect->bam_type];
	struct usb_bam_pipe_connect *pipe_iter;
	int (*callback)(void *priv);
	void *param = NULL;

	switch (event_info->type) {
	case USB_BAM_EVENT_WAKEUP:
	case USB_BAM_EVENT_WAKEUP_PIPE:

		log_event_dbg("%s received USB_BAM_EVENT_WAKEUP\n", __func__);

		/*
		 * Make sure the PROD resource is granted before
		 * wakeup hsic host class driver (done by the callback below)
		 */
		if (pipe_connect->peer_bam == IPA_P_BAM &&
			pipe_connect->bam_mode == USB_BAM_HOST &&
		    info[pipe_connect->bam_type].cur_prod_state
				!= IPA_RM_RESOURCE_GRANTED) {
			wait_for_prod_granted(pipe_connect->bam_type);
		}

		/*
		 * Check if need to resume the hsic host.
		 * On one hand, since we got the wakeup interrupt
		 * the hsic bam clocks are already enabled, so no need
		 * to actualluy resume the hardware... However, we still need
		 * to update the usb bam driver state (to set in_lpm=false),
		 * and to wake ipa and to hold again the hsic host
		 * device again to avoid it going to low poer mode next time
		 * until we complete releasing the hsic consumer and producer
		 * resources against the ipa resource manager.
		 */
		spin_lock(&ctx->usb_bam_lock);
		if (pipe_connect->bam_mode == USB_BAM_HOST)
			usb_bam_resume_host(pipe_connect->bam_type);
		spin_unlock(&ctx->usb_bam_lock);

		/* Notify about wakeup / activity of the bam */
		if (event_info->callback)
			event_info->callback(event_info->param);

		/*
		 * Reset inactivity timer counter if this pipe's bam
		 * has inactivity timeout.
		 */
		spin_lock(&ctx->usb_bam_lock);
		if (ctx->inactivity_timer_ms)
			usb_bam_set_inactivity_timer(pipe_connect->bam_type);
		spin_unlock(&ctx->usb_bam_lock);

		if (pipe_connect->bam_mode == USB_BAM_DEVICE) {
			/* A2 wakeup not from LPM (CONS was up) */
			wait_for_prod_granted(pipe_connect->bam_type);
			if (pipe_connect->start) {
				log_event_dbg("%s: Enqueue PROD transfer\n",
						__func__);
				pipe_connect->start(
					pipe_connect->start_stop_param,
					USB_TO_PEER_PERIPHERAL);
			}
		}

		break;

	case USB_BAM_EVENT_INACTIVITY:

		log_event_dbg("%s received USB_BAM_EVENT_INACTIVITY\n",
				__func__);

		/*
		 * Since event info is one structure per pipe, it might be
		 * overriden when we will register the wakeup events below,
		 * and still we want ot register the wakeup events before we
		 * notify on the inactivity in order to identify the next
		 * activity as soon as possible.
		 */
		callback = event_info->callback;
		param = event_info->param;

		/*
		 * Upon inactivity, configure wakeup irq for all pipes
		 * that are into the usb bam.
		 */
		spin_lock(&ctx->usb_bam_lock);
		for (i = 0; i < ctx->max_connections; i++) {
			pipe_iter = &ctx->usb_bam_connections[i];
			if (pipe_iter->bam_type == pipe_connect->bam_type &&
			    pipe_iter->dir == PEER_PERIPHERAL_TO_USB &&
			    pipe_iter->enabled) {
				log_event_dbg("%s: Register wakeup on pipe %pK\n",
					__func__, pipe_iter);
				__usb_bam_register_wake_cb(
					pipe_connect->bam_type, i,
					pipe_iter->activity_notify,
					pipe_iter->priv,
					false);
			}
		}
		spin_unlock(&ctx->usb_bam_lock);

		/* Notify about the inactivity to the USB class driver */
		if (callback)
			callback(param);

		wait_for_prod_release(pipe_connect->bam_type);
		log_event_dbg("%s: complete wait on hsic producer s=%d\n",
			__func__, info[pipe_connect->bam_type].cur_prod_state);

		/*
		 * Allow to go to lpm for now if also consumer is down.
		 * If consumer is up, we will wait to the release consumer
		 * notification.
		 */
		if (host_info[pipe_connect->bam_type].dev &&
		    info[pipe_connect->bam_type].cur_cons_state ==
		    IPA_RM_RESOURCE_RELEASED &&
				!info[pipe_connect->bam_type].in_lpm) {
			usb_bam_suspend_core(pipe_connect->bam_type,
				pipe_connect->bam_mode, 1);
		}

		break;
	default:
		log_event_err("%s: unknown usb bam event type %d\n", __func__,
			event_info->type);
	}
}

static void usb_bam_wake_cb(struct sps_event_notify *notify)
{
	struct usb_bam_event_info *event_info =
		(struct usb_bam_event_info *)notify->user;
	struct usb_bam_pipe_connect *pipe_connect =
		container_of(event_info,
			     struct usb_bam_pipe_connect,
			     event);
	enum usb_ctrl bam = pipe_connect->bam_type;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam];

	spin_lock(&ctx->usb_bam_lock);

	if (event_info->type == USB_BAM_EVENT_WAKEUP_PIPE)
		queue_work(ctx->usb_bam_wq, &event_info->event_w);
	else if (event_info->type == USB_BAM_EVENT_WAKEUP &&
			ctx->is_bam_inactivity) {

		/*
		 * Sps wake event is per pipe, so usb_bam_wake_cb is
		 * called per pipe. However, we want to filter the wake
		 * event to be wake event per all the pipes.
		 * Therefore, the first pipe that awaked will be considered
		 * as global bam wake event.
		 */
		ctx->is_bam_inactivity = false;

		queue_work(ctx->usb_bam_wq, &event_info->event_w);
	}

	spin_unlock(&ctx->usb_bam_lock);
}

static int __usb_bam_register_wake_cb(enum usb_ctrl bam_type, int idx,
				int (*callback)(void *user), void *param,
				bool trigger_cb_per_pipe)
{
	struct sps_pipe *pipe;
	struct sps_connect *sps_connection;
	struct usb_bam_pipe_connect *pipe_connect;
	struct usb_bam_event_info *wake_event_info;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	int ret;

	if (idx < 0 || idx > ctx->max_connections) {
		log_event_err("%s:idx is wrong %d\n", __func__, idx);
		return -EINVAL;
	}
	pipe = ctx->usb_bam_sps.sps_pipes[idx];
	sps_connection = &ctx->usb_bam_sps.sps_connections[idx];
	pipe_connect = &ctx->usb_bam_connections[idx];
	wake_event_info = &pipe_connect->event;

	wake_event_info->type = (trigger_cb_per_pipe ?
				USB_BAM_EVENT_WAKEUP_PIPE :
				USB_BAM_EVENT_WAKEUP);
	wake_event_info->param = param;
	wake_event_info->callback = callback;
	wake_event_info->event.mode = SPS_TRIGGER_CALLBACK;
	wake_event_info->event.xfer_done = NULL;
	wake_event_info->event.callback = callback ? usb_bam_wake_cb : NULL;
	wake_event_info->event.user = wake_event_info;
	wake_event_info->event.options = SPS_O_WAKEUP;
	ret = sps_register_event(pipe, &wake_event_info->event);
	if (ret) {
		log_event_err("%s: sps_register_event() failed %d\n",
				__func__, ret);
		return ret;
	}

	sps_connection->options = callback ?
		(SPS_O_AUTO_ENABLE | SPS_O_WAKEUP | SPS_O_WAKEUP_IS_ONESHOT) :
		SPS_O_AUTO_ENABLE;
	ret = sps_set_config(pipe, sps_connection);
	if (ret) {
		log_event_err("%s: sps_set_config() failed %d\n",
				__func__, ret);
		return ret;
	}
	log_event_dbg("%s: success\n", __func__);
	return 0;
}

int usb_bam_register_wake_cb(enum usb_ctrl bam_type, u8 idx,
			     int (*callback)(void *user), void *param)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct usb_bam_pipe_connect *pipe_connect =
				&ctx->usb_bam_connections[idx];

	info[pipe_connect->bam_type].wake_cb = callback;
	info[pipe_connect->bam_type].wake_param = param;
	return __usb_bam_register_wake_cb(bam_type, idx, callback, param, true);
}

int usb_bam_register_start_stop_cbs(enum usb_ctrl bam_type, u8 dst_idx,
	void (*start)(void *, enum usb_bam_pipe_dir),
	void (*stop)(void *, enum usb_bam_pipe_dir), void *param)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct usb_bam_pipe_connect *pipe_connect =
				&ctx->usb_bam_connections[dst_idx];

	log_event_dbg("%s: Register for %d\n", __func__, dst_idx);
	pipe_connect->start = start;
	pipe_connect->stop = stop;
	pipe_connect->start_stop_param = param;

	return 0;
}

int usb_bam_disconnect_pipe(enum usb_ctrl bam_type, u8 idx)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct usb_bam_pipe_connect *pipe_connect;
	struct device *bam_dev = &ctx->usb_bam_pdev->dev;
	int ret;
	struct msm_usb_bam_platform_data *pdata =
				ctx->usb_bam_pdev->dev.platform_data;

	pipe_connect = &ctx->usb_bam_connections[idx];

	if (!pipe_connect->enabled) {
		log_event_err("%s: connection %d isn't enabled\n",
			__func__, idx);
		return 0;
	}

	ret = disconnect_pipe(bam_type, idx);
	if (ret) {
		log_event_err("%s: src pipe disconnection failure\n", __func__);
		return ret;
	}

	pipe_connect->enabled = 0;
	spin_lock(&ctx->usb_bam_lock);
	if (!ctx->pipes_enabled_per_bam) {
		log_event_err("%s: wrong pipes enabled counter for bam_type=%d\n",
			__func__, bam_type);
	} else {
		ctx->pipes_enabled_per_bam -= 1;
	}
	spin_unlock(&ctx->usb_bam_lock);

	log_event_dbg("%s: success disconnecting pipe %d\n", __func__, idx);

	if (pdata->reset_on_disconnect && !ctx->pipes_enabled_per_bam) {
		if (bam_type == CI_CTRL)
			msm_hw_bam_disable(1);

		sps_device_reset(ctx->h_bam);

		if (bam_type == CI_CTRL)
			msm_hw_bam_disable(0);
	}
	/* Enable usb irq here which is disabled in function drivers
	 * during disconnect after BAM reset.
	 */
	if (!ctx->pipes_enabled_per_bam && (bam_type == CI_CTRL))
		msm_usb_irq_disable(false);
	/* This function is directly called by USB Transport drivers
	 * to disconnect pipes. Drop runtime usage count here. For
	 * IPA, caller takes care of it
	 */
	if (pipe_connect->peer_bam != IPA_P_BAM) {
		log_event_dbg("%s: PM Runtime PUT %d, count: %d\n",
			  __func__, idx, get_pm_runtime_counter(bam_dev));
		pm_runtime_put_sync(bam_dev);
	}

	return 0;
}

/**
 * is_ipa_hanlde_valid: Check if ipa_handle is valid or not
 * @ipa_handle: IPA Handle for producer or consumer
 *
 * Returns true is ipa handle is valid.
 */
static bool is_ipa_handle_valid(u32 ipa_handle)
{

	return (ipa_handle != -1);
}

int usb_bam_disconnect_ipa(enum usb_ctrl cur_bam,
			   struct usb_bam_connect_ipa_params *ipa_params)
{
	int ret = 0, pipes_disconncted = 0;
	u8 idx = 0;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[cur_bam];
	struct usb_bam_pipe_connect *pipe_connect;
	struct device *bam_dev = &ctx->usb_bam_pdev->dev;
	enum usb_bam_mode bam_mode;

	if (!is_ipa_handle_valid(ipa_params->prod_clnt_hdl) &&
			!is_ipa_handle_valid(ipa_params->cons_clnt_hdl)) {
		log_event_err("%s: Both IPA handles are invalid.\n", __func__);
		return -EINVAL;
	}

	log_event_dbg("%s: Starting disconnect sequence\n", __func__);
	log_event_dbg("%s(): prod_clnt_hdl:%d cons_clnt_hdl:%d\n", __func__,
			ipa_params->prod_clnt_hdl, ipa_params->cons_clnt_hdl);
	if (is_ipa_handle_valid(ipa_params->prod_clnt_hdl))
		idx = ipa_params->dst_idx;
	if (is_ipa_handle_valid(ipa_params->cons_clnt_hdl))
		idx = ipa_params->src_idx;
	pipe_connect = &ctx->usb_bam_connections[idx];
	bam_mode = pipe_connect->bam_mode;

	if (bam_mode != USB_BAM_DEVICE)
		return -EINVAL;

	mutex_lock(&info[cur_bam].suspend_resume_mutex);
	/* Delay USB core to go into lpm before we finish our handshake */
	if (is_ipa_handle_valid(ipa_params->prod_clnt_hdl)) {
		ret = usb_bam_disconnect_ipa_prod(ipa_params, cur_bam);
		if (ret)
			goto out;
		pipes_disconncted++;
	}

	if (is_ipa_handle_valid(ipa_params->cons_clnt_hdl)) {
		ret = usb_bam_disconnect_ipa_cons(ipa_params, cur_bam);
		if (ret)
			goto out;
		pipes_disconncted++;
	}

	/* Notify CONS release on the last cons pipe released */
	if (!ctx->pipes_enabled_per_bam) {
		if (info[cur_bam].cur_cons_state ==
				IPA_RM_RESOURCE_RELEASED) {
			log_event_dbg("%s: Notify CONS_RELEASED\n", __func__);
			ipa_rm_notify_completion(
				IPA_RM_RESOURCE_RELEASED,
				ipa_rm_resource_cons[cur_bam]);
		}
	}

out:
	/* Pipes are connected one by one, but can get disconnected in pairs */
	while (pipes_disconncted--) {
		if (!info[cur_bam].pipes_suspended) {
			log_event_dbg("%s: PM Runtime PUT %d, count: %d\n",
					__func__, pipes_disconncted,
					get_pm_runtime_counter(bam_dev));
			pm_runtime_put_sync(&ctx->usb_bam_pdev->dev);
		}
	}

	mutex_unlock(&info[cur_bam].suspend_resume_mutex);

	return ret;
}
EXPORT_SYMBOL(usb_bam_disconnect_ipa);

static void usb_bam_sps_events(enum sps_callback_case sps_cb_case, void *user)
{
	int i;
	int bam;
	struct usb_bam_ctx_type *ctx;
	struct usb_bam_pipe_connect *pipe_connect;
	struct usb_bam_event_info *event_info;

	switch (sps_cb_case) {
	case SPS_CALLBACK_BAM_TIMER_IRQ:

		log_event_dbg("%s: received SPS_CALLBACK_BAM_TIMER_IRQ\n",
				__func__);

		bam = get_bam_type_from_core_name((char *)user);
		if (bam < 0 || bam >= MAX_BAMS) {
			log_event_err("%s: Invalid bam, type=%d ,name=%s\n",
				__func__, bam, (char *)user);
			return;
		}
		ctx = &msm_usb_bam[bam];
		spin_lock(&ctx->usb_bam_lock);

		ctx->is_bam_inactivity = true;
		log_event_dbg("%s: Inactivity happened on bam=%s,%d\n",
				__func__, (char *)user, bam);

		for (i = 0; i < ctx->max_connections; i++) {
			pipe_connect = &ctx->usb_bam_connections[i];

			/*
			 * Notify inactivity once, Since it is global
			 * for all pipes on bam. Notify only if we have
			 * connected pipes.
			 */
			if (pipe_connect->enabled) {
				event_info = &pipe_connect->event;
				event_info->type = USB_BAM_EVENT_INACTIVITY;
				event_info->param = pipe_connect->priv;
				event_info->callback =
					pipe_connect->inactivity_notify;
				queue_work(ctx->usb_bam_wq,
						&event_info->event_w);
				break;
			}
		}

		spin_unlock(&ctx->usb_bam_lock);

		break;
	default:
		log_event_dbg("%s: received sps_cb_case=%d\n", __func__,
			(int)sps_cb_case);
	}
}

static struct msm_usb_bam_platform_data *usb_bam_dt_to_pdata(
	struct platform_device *pdev, u32 usb_addr)
{
	struct msm_usb_bam_platform_data *pdata;
	struct device_node *node = pdev->dev.of_node;
	int rc = 0;
	u8 i = 0;
	u32 bam, bam_mode;
	u32 addr;
	u32 threshold, max_connections = 0;
	static struct usb_bam_pipe_connect *usb_bam_connections;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,bam-type", &bam);
	if (rc) {
		log_event_err("%s: bam type is missing in device tree\n",
			__func__);
		return NULL;
	}
	if (bam >= MAX_BAMS) {
		log_event_err("%s: Invalid bam type %d in device tree\n",
			__func__, bam);
		return NULL;
	}
	pdata->bam_type = bam;

	rc = of_property_read_u32(node, "qcom,bam-mode", &bam_mode);
	if (rc) {
		pr_debug("%s: bam mode is missing in device tree\n",
			__func__);
		/* Default to DEVICE if bam_mode is not specified */
		bam_mode = USB_BAM_DEVICE;
	}

	pdata->reset_on_connect = of_property_read_bool(node,
					"qcom,reset-bam-on-connect");

	pdata->reset_on_disconnect = of_property_read_bool(node,
					"qcom,reset-bam-on-disconnect");

	rc = of_property_read_u32(node, "qcom,usb-bam-num-pipes",
		&pdata->usb_bam_num_pipes);
	if (rc) {
		log_event_err("Invalid usb bam num pipes property\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-bam-max-mbps-highspeed",
		&pdata->max_mbps_highspeed);
	if (rc)
		pdata->max_mbps_highspeed = 0;

	rc = of_property_read_u32(node, "qcom,usb-bam-max-mbps-superspeed",
		&pdata->max_mbps_superspeed);
	if (rc)
		pdata->max_mbps_superspeed = 0;

	rc = of_property_read_u32(node, "qcom,usb-bam-fifo-baseaddr", &addr);
	if (rc)
		pr_debug("%s: Invalid usb base address property\n", __func__);
	else
		pdata->usb_bam_fifo_baseaddr = addr;

	pdata->ignore_core_reset_ack = of_property_read_bool(node,
		"qcom,ignore-core-reset-ack");

	pdata->disable_clk_gating = of_property_read_bool(node,
		"qcom,disable-clk-gating");

	rc = of_property_read_u32(node, "qcom,usb-bam-override-threshold",
			&threshold);
	if (rc)
		pdata->override_threshold = USB_THRESHOLD;
	else
		pdata->override_threshold = threshold;

	for_each_child_of_node(pdev->dev.of_node, node)
		max_connections++;

	if (!max_connections) {
		log_event_err("%s: error: max_connections is zero\n", __func__);
		goto err;
	}

	usb_bam_connections = devm_kzalloc(&pdev->dev, max_connections *
		sizeof(struct usb_bam_pipe_connect), GFP_KERNEL);

	if (!usb_bam_connections) {
		log_event_err("%s: devm_kzalloc failed(%d)\n",
				__func__,  __LINE__);
		return NULL;
	}

	/* retrieve device tree parameters */
	for_each_child_of_node(pdev->dev.of_node, node) {
		usb_bam_connections[i].bam_type = bam;
		usb_bam_connections[i].bam_mode = bam_mode;

		rc = of_property_read_string(node, "label",
			&usb_bam_connections[i].name);
		if (rc)
			goto err;

		rc = of_property_read_u32(node, "qcom,usb-bam-mem-type",
			&usb_bam_connections[i].mem_type);
		if (rc)
			goto err;

		if (usb_bam_connections[i].mem_type == OCI_MEM) {
			if (!pdata->usb_bam_fifo_baseaddr) {
				log_event_err("%s: base address is missing\n",
					__func__);
				goto err;
			}
		}
		rc = of_property_read_u32(node, "qcom,peer-bam",
			&usb_bam_connections[i].peer_bam);
		if (rc) {
			log_event_err("%s: peer bam is missing in device tree\n",
				__func__);
			goto err;
		}
		/*
		 * Store USB bam_type to be used with QDSS. As only one device
		 * bam is currently supported, check the same in DT connections
		 */
		if (usb_bam_connections[i].peer_bam == QDSS_P_BAM) {
			if (qdss_usb_bam_type) {
				log_event_err("%s: overriding QDSS pipe!, update DT\n",
					__func__);
			}
			qdss_usb_bam_type = usb_bam_connections[i].bam_type;
		}

		rc = of_property_read_u32(node, "qcom,dir",
			&usb_bam_connections[i].dir);
		if (rc) {
			log_event_err("%s: direction is missing in device tree\n",
				__func__);
			goto err;
		}

		rc = of_property_read_u32(node, "qcom,pipe-num",
			&usb_bam_connections[i].pipe_num);
		if (rc) {
			log_event_err("%s: pipe num is missing in device tree\n",
				__func__);
			goto err;
		}

		rc = of_property_read_u32(node, "qcom,pipe-connection-type",
			&usb_bam_connections[i].pipe_type);
		if (rc)
			pr_debug("%s: pipe type is defaulting to bam2bam\n",
					__func__);

		of_property_read_u32(node, "qcom,peer-bam-physical-address",
						&addr);
		if (usb_bam_connections[i].dir == USB_TO_PEER_PERIPHERAL) {
			usb_bam_connections[i].src_phy_addr = usb_addr;
			usb_bam_connections[i].dst_phy_addr = addr;
		} else {
			usb_bam_connections[i].src_phy_addr = addr;
			usb_bam_connections[i].dst_phy_addr = usb_addr;
		}

		of_property_read_u32(node, "qcom,src-bam-pipe-index",
			&usb_bam_connections[i].src_pipe_index);

		of_property_read_u32(node, "qcom,dst-bam-pipe-index",
			&usb_bam_connections[i].dst_pipe_index);

		of_property_read_u32(node, "qcom,data-fifo-offset",
			&usb_bam_connections[i].data_fifo_base_offset);

		rc = of_property_read_u32(node, "qcom,data-fifo-size",
			&usb_bam_connections[i].data_fifo_size);
		if (rc)
			goto err;

		of_property_read_u32(node, "qcom,descriptor-fifo-offset",
			&usb_bam_connections[i].desc_fifo_base_offset);

		rc = of_property_read_u32(node, "qcom,descriptor-fifo-size",
			&usb_bam_connections[i].desc_fifo_size);
		if (rc)
			goto err;
		i++;
	}

	msm_usb_bam[bam].usb_bam_connections = usb_bam_connections;
	msm_usb_bam[bam].max_connections = max_connections;

	return pdata;
err:
	log_event_err("%s: failed\n", __func__);
	return NULL;
}

static void msm_usb_bam_update_props(struct sps_bam_props *props,
				struct platform_device *pdev)
{
	struct msm_usb_bam_platform_data *pdata = pdev->dev.platform_data;
	enum usb_ctrl bam_type = pdata->bam_type;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];


	props->phys_addr = ctx->io_res->start;
	props->virt_addr = NULL;
	props->virt_size = resource_size(ctx->io_res);
	props->irq = ctx->irq;
	props->summing_threshold = pdata->override_threshold;
	props->event_threshold = pdata->override_threshold;
	props->num_pipes = pdata->usb_bam_num_pipes;
	props->callback = usb_bam_sps_events;
	props->user = bam_enable_strings[bam_type];

	/*
	* HSUSB and HSIC Cores don't support RESET ACK signal to BAMs
	* Hence, let BAM to ignore acknowledge from USB while resetting PIPE
	*/
	if (pdata->ignore_core_reset_ack && bam_type != DWC3_CTRL)
		props->options = SPS_BAM_NO_EXT_P_RST;

	if (pdata->disable_clk_gating)
		props->options |= SPS_BAM_NO_LOCAL_CLK_GATING;

	/*
	 * HSUSB BAM is not NDP BAM and it must be enabled before
	 * starting peripheral controller to avoid switching USB core mode
	 * from legacy to BAM with ongoing data transfers.
	 */
	if (bam_type == CI_CTRL) {
		log_event_dbg("Register and enable HSUSB BAM\n");
		props->options |= SPS_BAM_OPT_ENABLE_AT_BOOT;
		props->options |= SPS_BAM_FORCE_RESET;
	}
}

static int usb_bam_init(struct platform_device *pdev)
{
	int ret;
	struct msm_usb_bam_platform_data *pdata = pdev->dev.platform_data;
	enum usb_ctrl bam_type = pdata->bam_type;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct sps_bam_props props;


	pr_debug("%s: usb_bam_init - %s\n", __func__,
		bam_enable_strings[bam_type]);

	/*
	 * CI USB2 BAM is registered before starting controller
	 * and only if bam2bam function is present in composition
	 */
	if (bam_type == CI_CTRL)
		return 0;

	memset(&props, 0, sizeof(props));
	msm_usb_bam_update_props(&props, pdev);
	ret = sps_register_bam_device(&props, &ctx->h_bam);

	if (ret < 0) {
		log_event_err("%s: register bam error %d\n",
				__func__, ret);
		return -EFAULT;
	}

	return 0;
}

static int enable_usb_bam(struct platform_device *pdev)
{
	int ret;
	struct msm_usb_bam_platform_data *pdata = pdev->dev.platform_data;
	enum usb_ctrl bam_type = pdata->bam_type;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];

	ret = usb_bam_init(pdev);
	if (ret) {
		log_event_err("failed to init bam %s\n",
				bam_enable_strings[bam_type]);
		return ret;
	}

	ctx->usb_bam_sps.sps_pipes = devm_kzalloc(&pdev->dev,
		ctx->max_connections * sizeof(struct sps_pipe *),
		GFP_KERNEL);

	if (!ctx->usb_bam_sps.sps_pipes) {
		log_event_err("%s: failed to allocate sps_pipes\n", __func__);
		return -ENOMEM;
	}

	ctx->usb_bam_sps.sps_connections = devm_kzalloc(&pdev->dev,
		ctx->max_connections * sizeof(struct sps_connect),
		GFP_KERNEL);
	if (!ctx->usb_bam_sps.sps_connections) {
		log_event_err("%s: failed to allocate sps_connections\n",
				__func__);
		return -ENOMEM;
	}

	return 0;
}

static int usb_bam_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	int i;
	struct usb_bam_ctx_type *ctx;

	for (i = 0; i < MAX_BAMS; i++) {
		ctx = &msm_usb_bam[i];
		if (ctx->h_bam)
			break;
	}

	if (i == MAX_BAMS)
		goto fail;

	if (!ctx->pipes_enabled_per_bam || info[i].pipes_suspended)
		goto fail;

	pr_err("%s: dump usb bam registers here in call back!\n",
								__func__);
	sps_get_bam_debug_info(ctx->h_bam, 93,
			(SPS_BAM_PIPE(0) | SPS_BAM_PIPE(1)), 0, 2);

fail:
	return NOTIFY_DONE;
}

static struct notifier_block usb_bam_panic_blk = {
	.notifier_call  = usb_bam_panic_notifier,
};

void usb_bam_register_panic_hdlr(void)
{
	atomic_notifier_chain_register(&panic_notifier_list,
			&usb_bam_panic_blk);
}

static int usb_bam_probe(struct platform_device *pdev)
{
	int ret, i, irq;
	struct resource *io_res;
	void __iomem *regs;
	enum usb_ctrl bam_type;
	struct usb_bam_ctx_type *ctx;
	struct msm_usb_bam_platform_data *pdata;

	dev_dbg(&pdev->dev, "usb_bam_probe\n");

	io_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!io_res) {
		dev_err(&pdev->dev, "missing BAM memory resource\n");
		return -ENODEV;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Unable to get IRQ resource\n");
		return irq;
	}

	regs = devm_ioremap(&pdev->dev, io_res->start, resource_size(io_res));
	if (!regs) {
		log_event_err("%s: ioremap failed\n", __func__);
		return -ENOMEM;
	}

	/* specify BAM physical address to be filled in BAM connections */
	pdata = usb_bam_dt_to_pdata(pdev, io_res->start);
	if (!pdata)
		return -EINVAL;

	pdev->dev.platform_data = pdata;
	bam_type = pdata->bam_type;
	ctx = &msm_usb_bam[bam_type];

	ctx->usb_bam_pdev = pdev;
	ctx->irq = irq;
	ctx->regs = regs;
	ctx->io_res = io_res;

	for (i = 0; i < ctx->max_connections; i++) {
		ctx->usb_bam_connections[i].enabled = 0;
		INIT_WORK(&ctx->usb_bam_connections[i].event.event_w,
			usb_bam_work);
	}

	init_completion(&info[bam_type].prod_avail);
	complete(&info[bam_type].prod_avail);
	init_completion(&info[bam_type].prod_released);
	complete(&info[bam_type].prod_released);
	info[bam_type].cur_prod_state = IPA_RM_RESOURCE_RELEASED;
	info[bam_type].cur_cons_state = IPA_RM_RESOURCE_RELEASED;
	info[bam_type].bam_type = bam_type;
	INIT_WORK(&info[bam_type].resume_work, usb_bam_finish_resume);
	INIT_WORK(&info[bam_type].finish_suspend_work, usb_bam_finish_suspend_);
	mutex_init(&info[bam_type].suspend_resume_mutex);

	ctx->usb_bam_wq = alloc_workqueue("usb_bam_wq",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!ctx->usb_bam_wq) {
		log_event_err("unable to create workqueue usb_bam_wq\n");
		return -ENOMEM;
	}

	ret = enable_usb_bam(pdev);
	if (ret) {
		destroy_workqueue(ctx->usb_bam_wq);
		return ret;
	}

	pm_runtime_no_callbacks(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	spin_lock_init(&usb_bam_ipa_handshake_info_lock);
	if (ipa_get_transport_type() == IPA_TRANSPORT_TYPE_SPS &&
			ipa_is_ready())
		usb_bam_ipa_create_resources(bam_type);
	spin_lock_init(&ctx->usb_bam_lock);

	usb_bam_register_panic_hdlr();
	return ret;
}

bool usb_bam_get_prod_granted(enum usb_ctrl bam_type, u8 idx)
{
	return (info[bam_type].cur_prod_state == IPA_RM_RESOURCE_GRANTED);
}
EXPORT_SYMBOL(usb_bam_get_prod_granted);

int get_bam2bam_connection_info(enum usb_ctrl bam_type, u8 idx,
	u32 *usb_bam_pipe_idx, struct sps_mem_buffer *desc_fifo,
	struct sps_mem_buffer *data_fifo, enum usb_pipe_mem_type *mem_type)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	struct usb_bam_pipe_connect *pipe_connect =
			&ctx->usb_bam_connections[idx];
	enum usb_bam_pipe_dir dir = pipe_connect->dir;

	if (dir == USB_TO_PEER_PERIPHERAL)
		*usb_bam_pipe_idx = pipe_connect->src_pipe_index;
	else
		*usb_bam_pipe_idx = pipe_connect->dst_pipe_index;

	if (data_fifo)
		memcpy(data_fifo, &pipe_connect->data_mem_buf,
		sizeof(struct sps_mem_buffer));
	if (desc_fifo)
		memcpy(desc_fifo, &pipe_connect->desc_mem_buf,
		sizeof(struct sps_mem_buffer));
	if (mem_type)
		*mem_type = pipe_connect->mem_type;

	return 0;
}
EXPORT_SYMBOL(get_bam2bam_connection_info);

int get_qdss_bam_connection_info(unsigned long *usb_bam_handle,
	u32 *usb_bam_pipe_idx, u32 *peer_pipe_idx,
	struct sps_mem_buffer *desc_fifo, struct sps_mem_buffer *data_fifo,
	enum usb_pipe_mem_type *mem_type)
{
	u8 idx;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[qdss_usb_bam_type];
	struct sps_connect *sps_connection;

	/* QDSS uses only one pipe */
	idx = usb_bam_get_connection_idx(qdss_usb_bam_type, QDSS_P_BAM,
		PEER_PERIPHERAL_TO_USB, USB_BAM_DEVICE, 0);

	get_bam2bam_connection_info(qdss_usb_bam_type, idx, usb_bam_pipe_idx,
						desc_fifo, data_fifo, mem_type);


	sps_connection = &ctx->usb_bam_sps.sps_connections[idx];
	*usb_bam_handle = sps_connection->destination;
	*peer_pipe_idx = sps_connection->src_pipe_index;

	return 0;
}
EXPORT_SYMBOL(get_qdss_bam_connection_info);

int usb_bam_get_connection_idx(enum usb_ctrl bam_type, enum peer_bam client,
	enum usb_bam_pipe_dir dir, enum usb_bam_mode bam_mode, u32 num)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];
	u8 i;

	for (i = 0; i < ctx->max_connections; i++) {
		if (ctx->usb_bam_connections[i].peer_bam == client &&
		    ctx->usb_bam_connections[i].dir == dir &&
		    ctx->usb_bam_connections[i].bam_mode == bam_mode &&
		    ctx->usb_bam_connections[i].pipe_num == num) {
			log_event_dbg("%s: index %d was found\n", __func__, i);
			return i;
		}
	}

	log_event_err("%s: failed for %d\n", __func__, bam_type);
	return -ENODEV;
}
EXPORT_SYMBOL(usb_bam_get_connection_idx);

int usb_bam_get_bam_type(const char *core_name)
{
	int bam_type = get_bam_type_from_core_name(core_name);

	if (bam_type < 0 || bam_type >= MAX_BAMS) {
		log_event_err("%s: Invalid bam, type=%d, name=%s\n",
			__func__, bam_type, core_name);
		return -EINVAL;
	}

	return bam_type;
}
EXPORT_SYMBOL(usb_bam_get_bam_type);

/*
 * This function makes sure ipa endpoints are disabled for both USB->IPA
 * and IPA->USB pipes before USB bam reset. USB BAM reset is required to
 * to avoid EP flush issues while disabling USB endpoints on disconnect.
 */
int msm_do_bam_disable_enable(enum usb_ctrl bam)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam];
	struct sps_pipe *pipe;
	u32 timeout = 10, pipe_empty;
	int ret = 0, i;
	struct sps_connect *sps_connection;
	struct usb_bam_sps_type usb_bam_sps = ctx->usb_bam_sps;
	struct usb_bam_pipe_connect *pipe_connect;
	int qdss_idx;
	struct msm_usb_bam_platform_data *pdata;

	if (!ctx->usb_bam_pdev)
		return 0;

	pdata = ctx->usb_bam_pdev->dev.platform_data;
	if (bam != CI_CTRL)
		return 0;

	if (!ctx->pipes_enabled_per_bam || info[bam].pipes_suspended)
		return 0;

	if (in_interrupt()) {
		pr_err("%s:API called in interrupt context\n", __func__);
		return 0;
	}

	mutex_lock(&info[bam].suspend_resume_mutex);
	log_event_dbg("%s: Perform USB BAM reset\n", __func__);
	/* Get QDSS pipe index to avoid pipe reset */
	qdss_idx = usb_bam_get_connection_idx(qdss_usb_bam_type, QDSS_P_BAM,
		PEER_PERIPHERAL_TO_USB, USB_BAM_DEVICE, 0);

	for (i = 0; i < ctx->max_connections; i++) {
		pipe_connect = &ctx->usb_bam_connections[i];
		if (pipe_connect->enabled &&
				(pipe_connect->dir == PEER_PERIPHERAL_TO_USB) &&
							(qdss_idx != i)) {
			/* Call to disable IPA producer endpoint */
			ipa_disable_endpoint(pipe_connect->ipa_clnt_hdl);
			sps_pipe_reset(ctx->h_bam,
						pipe_connect->dst_pipe_index);
		}
	}

	for (i = 0; i < ctx->max_connections; i++) {
		pipe_connect = &ctx->usb_bam_connections[i];
		if (pipe_connect->enabled &&
				(pipe_connect->dir == USB_TO_PEER_PERIPHERAL) &&
							(qdss_idx != i)) {
			pipe = ctx->usb_bam_sps.sps_pipes[i];
			sps_connection = &usb_bam_sps.sps_connections[i];
			timeout = 10;
			/*
			 * On some platforms, there is a chance that flow
			 * control is disabled from IPA side, due to this IPA
			 * core may not consume data from USB. Hence notify IPA
			 * to enable flow control and then check sps pipe is
			 * empty or not before processing USB->IPA disconnect.
			 */
			ipa_clear_endpoint_delay(pipe_connect->ipa_clnt_hdl);

			/* Make sure pipes are empty before disconnecting it */
			while (1) {
				ret = sps_is_pipe_empty(pipe, &pipe_empty);
				if (ret) {
					log_event_err("%s: pipeempty fail %d\n",
								__func__, ret);
					goto err;
				}
				if (pipe_empty || !--timeout)
					break;

				/* Check again */
				usleep_range(1000, 2000);
			}
			if (!pipe_empty) {
				log_event_dbg("%s: Inject ZLT\n", __func__);
				sps_pipe_inject_zlt(sps_connection->destination,
					sps_connection->dest_pipe_index);

				timeout = 0;
				while (1) {
					ret = sps_is_pipe_empty(pipe,
								&pipe_empty);
					if (ret)
						goto err;

					if (pipe_empty)
						break;

					timeout++;
					/* Check again */
					usleep_range(1000, 2000);
				}
			}
			/* Call to disable IPA consumer endpoint */
			ipa_disable_endpoint(pipe_connect->ipa_clnt_hdl);
			sps_pipe_reset(ctx->h_bam,
						pipe_connect->src_pipe_index);
		}
	}

	/* Perform USB BAM reset */
	msm_hw_bam_disable(1);
	sps_device_reset(ctx->h_bam);
	msm_hw_bam_disable(0);
	log_event_dbg("%s: USB BAM reset done\n", __func__);
	ret = 0;

err:
	mutex_unlock(&info[bam].suspend_resume_mutex);
	return ret;
}
EXPORT_SYMBOL(msm_do_bam_disable_enable);

bool msm_usb_bam_enable(enum usb_ctrl bam, bool bam_enable)
{
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam];
	static bool bam_enabled;
	int ret;

	if (!ctx->usb_bam_pdev)
		return 0;

	if (bam != CI_CTRL)
		return 0;

	if (bam_enabled == bam_enable) {
		log_event_dbg("%s: USB BAM is already %s\n", __func__,
				bam_enable ? "Registered" : "De-registered");
		return 0;
	}

	if (bam_enable) {
		struct sps_bam_props props;

		memset(&props, 0, sizeof(props));
		msm_usb_bam_update_props(&props, ctx->usb_bam_pdev);
		msm_hw_bam_disable(1);
		ret = sps_register_bam_device(&props, &ctx->h_bam);
		bam_enabled = true;
		if (ret < 0) {
			log_event_err("%s: register bam error %d\n",
					__func__, ret);
			return -EFAULT;
		}
		log_event_dbg("%s: USB BAM Registered\n", __func__);
		msm_hw_bam_disable(0);
	} else {
		msm_hw_soft_reset();
		msm_hw_bam_disable(1);
		sps_device_reset(ctx->h_bam);
		sps_deregister_bam_device(ctx->h_bam);
		log_event_dbg("%s: USB BAM De-registered\n", __func__);
		bam_enabled = false;
	}

	return 0;
}
EXPORT_SYMBOL(msm_usb_bam_enable);

/**
 * msm_bam_hsic_host_pipe_empty - Check all HSIC host BAM pipe state
 *
 * return true if all BAM pipe used for HSIC Host mode is empty.
 */
bool msm_bam_hsic_host_pipe_empty(void)
{
	struct usb_bam_pipe_connect *pipe_connect;
	struct sps_pipe *pipe = NULL;
	enum usb_ctrl bam = HSIC_CTRL;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam];
	int i, ret;
	u32 status;

	pr_debug("%s: enter\n", __func__);

	for (i = 0; i < ctx->max_connections; i++) {
		pipe_connect = &ctx->usb_bam_connections[i];
		if (pipe_connect->enabled) {
			pipe = ctx->usb_bam_sps.sps_pipes[i];
			ret = sps_is_pipe_empty(pipe, &status);
			if (ret) {
				log_event_err("%s(): sps_is_pipe_empty() failed\n",
								__func__);
				log_event_err("%s(): SRC index(%d), DEST index(%d):\n",
						__func__,
						pipe_connect->src_pipe_index,
						pipe_connect->dst_pipe_index);
				WARN_ON(1);
			}

			if (!status) {
				log_event_err("%s(): pipe is not empty.\n",
						__func__);
				log_event_err("%s(): SRC index(%d), DEST index(%d):\n",
						__func__,
						pipe_connect->src_pipe_index,
						pipe_connect->dst_pipe_index);
				return false;
			} else {
				pr_debug("%s(): SRC index(%d), DEST index(%d):\n",
						__func__,
						pipe_connect->src_pipe_index,
						pipe_connect->dst_pipe_index);
			}
		}

	}

	if (!pipe)
		log_event_err("%s: Bam %s has no connected pipes\n", __func__,
						bam_enable_strings[bam]);

	return true;
}
EXPORT_SYMBOL(msm_bam_hsic_host_pipe_empty);

bool msm_bam_hsic_lpm_ok(void)
{
	log_event_dbg("%s: enter\n", __func__);

	if (info[HSIC_CTRL].cur_bam_mode == USB_BAM_HOST)
		return msm_bam_host_lpm_ok(HSIC_CTRL);

	/* RuntimPM is used to manage device mode LPM */
	return 0;
}
EXPORT_SYMBOL(msm_bam_hsic_lpm_ok);

static int usb_bam_remove(struct platform_device *pdev)
{
	struct msm_usb_bam_platform_data *pdata = pdev->dev.platform_data;
	enum usb_ctrl bam_type = pdata->bam_type;
	struct usb_bam_ctx_type *ctx = &msm_usb_bam[bam_type];

	destroy_workqueue(ctx->usb_bam_wq);

	return 0;
}

static const struct of_device_id usb_bam_dt_match[] = {
	{ .compatible = "qcom,usb-bam-msm",
	},
	{}
};
MODULE_DEVICE_TABLE(of, usb_bam_dt_match);

static struct platform_driver usb_bam_driver = {
	.probe = usb_bam_probe,
	.remove = usb_bam_remove,
	.driver		= {
		.name	= "usb_bam",
		.of_match_table = usb_bam_dt_match,
	},
};

static int __init init(void)
{
	return platform_driver_register(&usb_bam_driver);
}
module_init(init);

static void __exit cleanup(void)
{
	platform_driver_unregister(&usb_bam_driver);
}
module_exit(cleanup);

MODULE_DESCRIPTION("MSM USB BAM DRIVER");
MODULE_LICENSE("GPL v2");
