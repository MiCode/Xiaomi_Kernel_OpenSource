/*  Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mhi.h>
#include <linux/mutex.h>
#include <dsp/voice_mhi.h>
#include <dsp/msm_audio_ion.h>
#include <dsp/audio_notifier.h>
#include <dsp/q6core.h>
#include <dsp/audio_notifier.h>
#include <ipc/apr.h>
#include "adsp_err.h"

#define VSS_IPKTEXG_CMD_SET_MAILBOX_MEMORY_CONFIG	0x0001333B

#define VOICE_MHI_STATE_SET(a, b) ((a) |= (1UL<<(b)))
#define VOICE_MHI_STATE_RESET(a, b) ((a) &= ~(1UL<<(b)))
#define VOICE_MHI_STATE_CHECK(a, b) (1UL & (a >> b))

#define CMD_STATUS_SUCCESS 0
#define CMD_STATUS_FAIL 1
#define TIMEOUT_MS 500
#define PORT_NUM 0x01
#define PORT_MASK 0x03
#define CONVERT_PORT_APR(x, y) (x << 8 | y)

enum voice_states {
	VOICE_MHI_INIT = 0,
	VOICE_MHI_PROBED = VOICE_MHI_INIT,
	VOICE_MHI_ADSP_UP,
	VOICE_MHI_SDX_UP,
	VOICE_MHI_INCALL
};

struct voice_mhi_addr {
	dma_addr_t base;
	uint32_t size;
};

struct voice_mhi_dev_info {
	struct platform_device *pdev;
	struct voice_mhi_addr phys_addr;
	struct voice_mhi_addr iova_pcie;
	struct voice_mhi_addr iova_adsp;
};

struct voice_mhi {
	struct voice_mhi_dev_info dev_info;
	struct mhi_device *mhi_dev;
	uint32_t vote_count;
	struct mutex mutex;
	enum voice_states voice_mhi_state;
	bool vote_enable;
	bool pcie_enabled;
	void *apr_mvm_handle;
	struct work_struct voice_mhi_work_pcie;
	struct work_struct voice_mhi_work_adsp;
	wait_queue_head_t voice_mhi_wait;
	u32 mvm_state;
	u32 async_err;
};

struct vss_ipktexg_cmd_set_mailbox_memory_config_t {
	struct apr_hdr hdr;
	uint64_t mailbox_mem_address_adsp;
	/*
	 * IOVA of mailbox memory for ADSP access
	 */
	uint64_t mailbox_mem_address_pcie;
	/*
	 * IOVA of mailbox memory for PCIe access
	 */
	uint32_t mem_size;
	/*
	 * Size of mailbox memory allocated
	 */
} __packed;

static struct voice_mhi voice_mhi_lcl;

static int voice_mhi_pcie_up_callback(struct mhi_device *,
					const struct mhi_device_id *);
static void voice_mhi_pcie_down_callback(struct mhi_device *);
static void voice_mhi_pcie_status_callback(struct mhi_device *, enum MHI_CB);
static int32_t voice_mhi_apr_callback(struct apr_client_data *data, void *priv);
static int voice_mhi_notifier_service_cb(struct notifier_block *nb,
					 unsigned long opcode, void *ptr);
static int voice_mhi_apr_register(void);

static struct notifier_block voice_mhi_service_nb = {
	.notifier_call  = voice_mhi_notifier_service_cb,
	.priority = -INT_MAX,
};

static const struct mhi_device_id voice_mhi_match_table[] = {
	{ .chan = "AUDIO_VOICE_0", .driver_data = 0 },
	{},
};

static struct mhi_driver voice_mhi_driver = {
	.id_table = voice_mhi_match_table,
	.probe = voice_mhi_pcie_up_callback,
	.remove = voice_mhi_pcie_down_callback,
	.status_cb = voice_mhi_pcie_status_callback,
	.driver = {
		.name = "voice_mhi_audio",
		.owner = THIS_MODULE,
	},
};

static int voice_mhi_notifier_service_cb(struct notifier_block *nb,
					 unsigned long opcode, void *ptr)
{
	pr_debug("%s: opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		if (voice_mhi_lcl.apr_mvm_handle) {
			apr_reset(voice_mhi_lcl.apr_mvm_handle);
			voice_mhi_lcl.apr_mvm_handle = NULL;
			VOICE_MHI_STATE_RESET(voice_mhi_lcl.voice_mhi_state,
					VOICE_MHI_ADSP_UP);
		}
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (!VOICE_MHI_STATE_CHECK(voice_mhi_lcl.voice_mhi_state,
				VOICE_MHI_ADSP_UP)) {
			VOICE_MHI_STATE_SET(voice_mhi_lcl.voice_mhi_state,
					VOICE_MHI_ADSP_UP);
			schedule_work(&voice_mhi_lcl.voice_mhi_work_adsp);
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;

}

static int32_t voice_mhi_apr_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *ptr1;

	if (data == NULL) {
		pr_err("%s: data is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: Payload Length = %d, opcode=%x\n", __func__,
		data->payload_size, data->opcode);

	switch (data->opcode) {

	case APR_BASIC_RSP_RESULT:
		if (data->payload_size < 2 * sizeof(uint32_t)) {
			pr_err("%s: APR_BASIC_RSP_RESULT payload less than expected\n",
					__func__);
			return 0;
		}
		ptr1 = data->payload;
		switch (ptr1[0]) {
		case VSS_IPKTEXG_CMD_SET_MAILBOX_MEMORY_CONFIG:
			pr_debug("%s: cmd VSS_IPKTEXG_CMD_SET_MAILBOX_MEMORY_CONFIG\n",
					 __func__);
			voice_mhi_lcl.mvm_state = CMD_STATUS_SUCCESS;
			voice_mhi_lcl.async_err = ptr1[1];
			wake_up(&voice_mhi_lcl.voice_mhi_wait);
			break;
		default:
			pr_err("%s: Invalid cmd response 0x%x 0x%x\n", __func__,
				ptr1[0], ptr1[1]);
			break;
		}
		break;

	case APR_RSP_ACCEPTED:
		if (data->payload_size < sizeof(uint32_t)) {
			pr_err("%s: APR_RSP_ACCEPTED payload less than expected\n",
					__func__);
			return 0;
		}
		ptr1 = data->payload;
		if (ptr1[0])
			pr_debug("%s: APR_RSP_ACCEPTED for 0x%x:\n",
				 __func__, ptr1[0]);
		break;

	case RESET_EVENTS:
		//Should we handle here or audio notifier down?
		if (voice_mhi_lcl.apr_mvm_handle) {
			apr_reset(voice_mhi_lcl.apr_mvm_handle);
			voice_mhi_lcl.apr_mvm_handle = NULL;
			VOICE_MHI_STATE_RESET(voice_mhi_lcl.voice_mhi_state,
					VOICE_MHI_ADSP_UP);
		}
		break;

	default:
		pr_err("%s: Invalid opcode %d\n", __func__,
				data->opcode);
		break;

	}
	return 0;
}

/**
 * voice_mhi_start -
 *        Start vote for MHI/PCIe clock
 *
 * Returns 0 on success or error on failure
 */
int voice_mhi_start(void)
{
	int ret = 0;

	mutex_lock(&voice_mhi_lcl.mutex);
	if (voice_mhi_lcl.pcie_enabled) {
		if (!voice_mhi_lcl.mhi_dev) {
			pr_err("%s: NULL device found\n", __func__);
			ret = -EINVAL;
			goto done;
		}
		if (voice_mhi_lcl.vote_count == 0) {
			ret = mhi_device_get_sync(voice_mhi_lcl.mhi_dev);
			if (ret) {
				pr_err("%s: mhi_device_get_sync failed\n",
					   __func__);
				ret = -EINVAL;
				goto done;
			}
			pr_debug("%s: mhi_device_get_sync success\n", __func__);
		} else {
			/* For DSDA, no additional voting is needed */
			pr_debug("%s: mhi is already voted\n", __func__);
		}
		voice_mhi_lcl.vote_count++;
	} else {
		/* PCIe not supported - return success*/
		goto done;
	}
done:
	mutex_unlock(&voice_mhi_lcl.mutex);

	return ret;
}
EXPORT_SYMBOL(voice_mhi_start);

/**
 * voice_mhi_end -
 *        End vote for MHI/PCIe clock
 *
 * Returns 0 on success or error on failure
 */
int voice_mhi_end(void)
{
	mutex_lock(&voice_mhi_lcl.mutex);
	if (voice_mhi_lcl.pcie_enabled) {
		if (!voice_mhi_lcl.mhi_dev || voice_mhi_lcl.vote_count == 0) {
			pr_err("%s: NULL device found\n", __func__);
			mutex_unlock(&voice_mhi_lcl.mutex);
			return -EINVAL;
		}

		if (voice_mhi_lcl.vote_count == 1)
			mhi_device_put(voice_mhi_lcl.mhi_dev);
		voice_mhi_lcl.vote_count--;
	}
	mutex_unlock(&voice_mhi_lcl.mutex);

	return 0;
}
EXPORT_SYMBOL(voice_mhi_end);

static int voice_mhi_set_mailbox_memory_config(void)
{
	struct vss_ipktexg_cmd_set_mailbox_memory_config_t mb_memory_config;
	int ret = 0;
	void *apr_mvm;

	if (!voice_mhi_lcl.apr_mvm_handle) {
		pr_err("%s: APR handle is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&mb_memory_config, 0, sizeof(mb_memory_config));
	mb_memory_config.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	mb_memory_config.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(mb_memory_config) - APR_HDR_SIZE);
	pr_debug("%s: pkt size = %d\n", __func__,
			 mb_memory_config.hdr.pkt_size);

	mutex_lock(&voice_mhi_lcl.mutex);
	apr_mvm = voice_mhi_lcl.apr_mvm_handle;

	/*
	 * Handle can be NULL as it is not tied to any session
	 */
	mb_memory_config.hdr.src_port = CONVERT_PORT_APR(PORT_NUM, PORT_MASK);
	mb_memory_config.hdr.dest_port = 0;
	mb_memory_config.hdr.token = 0;
	mb_memory_config.hdr.opcode = VSS_IPKTEXG_CMD_SET_MAILBOX_MEMORY_CONFIG;
	mb_memory_config.mailbox_mem_address_pcie =
			voice_mhi_lcl.dev_info.iova_pcie.base;
	mb_memory_config.mailbox_mem_address_adsp =
			voice_mhi_lcl.dev_info.iova_adsp.base;
	mb_memory_config.mem_size = voice_mhi_lcl.dev_info.iova_adsp.size;
	voice_mhi_lcl.mvm_state = CMD_STATUS_FAIL;
	voice_mhi_lcl.async_err = 0;

	ret = apr_send_pkt(apr_mvm, (uint32_t *) &mb_memory_config);
	if (ret < 0) {
		pr_err("%s: Set mailbox memory config failed ret=%d\n",
				__func__, ret);
		goto unlock;
	}

	ret = wait_event_timeout(voice_mhi_lcl.voice_mhi_wait,
				 (voice_mhi_lcl.mvm_state ==
				 CMD_STATUS_SUCCESS),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -ETIME;
		goto unlock;
	}
	if (voice_mhi_lcl.async_err > 0) {
		pr_err("%s: DSP returned error[%d]\n",
				__func__, voice_mhi_lcl.async_err);
		ret = voice_mhi_lcl.async_err;
		goto unlock;
	}
	ret = 0;
unlock:
	mutex_unlock(&voice_mhi_lcl.mutex);
	return ret;
}

static void voice_mhi_map_pcie_and_send(struct work_struct *work)
{
	dma_addr_t iova, phys_addr;
	uint32_t mem_size;
	struct device *md;

	mutex_lock(&voice_mhi_lcl.mutex);
	if (voice_mhi_lcl.mhi_dev) {
		md = &voice_mhi_lcl.mhi_dev->dev;
	} else {
		pr_err("%s: MHI device handle is NULL\n", __func__);
		goto err;
	}

	phys_addr = voice_mhi_lcl.dev_info.phys_addr.base;
	mem_size = voice_mhi_lcl.dev_info.iova_pcie.size;
	if (md) {
		iova = dma_map_resource(md->parent, phys_addr, mem_size,
					DMA_BIDIRECTIONAL, 0);
		if (dma_mapping_error(md->parent, iova)) {
			pr_err("%s: dma_mapping_error\n", __func__);
			goto err;
		}
		pr_debug("%s: dma_mapping_success iova:0x%lx\n",
				 __func__, (unsigned long)iova);
		voice_mhi_lcl.dev_info.iova_pcie.base = iova;

		if (q6core_is_adsp_ready()) {
			if (VOICE_MHI_STATE_CHECK(voice_mhi_lcl.voice_mhi_state,
					VOICE_MHI_SDX_UP)) {
				mutex_unlock(&voice_mhi_lcl.mutex);
				voice_mhi_set_mailbox_memory_config();
				return;
			}
		}
	}

err:
	mutex_unlock(&voice_mhi_lcl.mutex);
}

static void voice_mhi_register_apr_and_send(struct work_struct *work)
{
	int ret = 0;

	ret = voice_mhi_apr_register();
	if (ret) {
		pr_err("%s: APR registration failed %d\n", __func__, ret);
		return;
	}
	mutex_lock(&voice_mhi_lcl.mutex);
	if (q6core_is_adsp_ready()) {
		if (VOICE_MHI_STATE_CHECK(voice_mhi_lcl.voice_mhi_state,
				VOICE_MHI_SDX_UP)) {
			mutex_unlock(&voice_mhi_lcl.mutex);
			voice_mhi_set_mailbox_memory_config();
			return;
		}
	}
	mutex_unlock(&voice_mhi_lcl.mutex);
}

static int voice_mhi_pcie_up_callback(struct mhi_device *voice_mhi_dev,
				const struct mhi_device_id *id)
{

	if ((!voice_mhi_dev) || (id != &voice_mhi_match_table[0])) {
		pr_err("%s: Invalid device or table received\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: MHI PCIe UP callback\n", __func__);
	mutex_lock(&voice_mhi_lcl.mutex);
	voice_mhi_lcl.mhi_dev = voice_mhi_dev;
	VOICE_MHI_STATE_SET(voice_mhi_lcl.voice_mhi_state, VOICE_MHI_SDX_UP);
	mutex_unlock(&voice_mhi_lcl.mutex);
	schedule_work(&voice_mhi_lcl.voice_mhi_work_pcie);
	return 0;
}

static void voice_mhi_pcie_down_callback(struct mhi_device *voice_mhi_dev)
{
	dma_addr_t iova;
	struct device *md;

	mutex_lock(&voice_mhi_lcl.mutex);

	if (voice_mhi_lcl.mhi_dev)
		md = &voice_mhi_lcl.mhi_dev->dev;

	VOICE_MHI_STATE_RESET(voice_mhi_lcl.voice_mhi_state, VOICE_MHI_SDX_UP);
	iova = voice_mhi_lcl.dev_info.iova_pcie.base;

	if (md)
		dma_unmap_resource(md->parent, iova, PAGE_SIZE,
				   DMA_BIDIRECTIONAL, 0);

	voice_mhi_lcl.mhi_dev = NULL;
	voice_mhi_lcl.vote_count = 0;
	mutex_unlock(&voice_mhi_lcl.mutex);
}

static void voice_mhi_pcie_status_callback(struct mhi_device *voice_mhi_dev,
					enum MHI_CB mhi_cb)
{

}

static int voice_mhi_apr_register(void)
{
	int ret = 0;

	mutex_lock(&voice_mhi_lcl.mutex);
	voice_mhi_lcl.apr_mvm_handle = apr_register("ADSP", "MVM",
						(apr_fn)voice_mhi_apr_callback,
						CONVERT_PORT_APR(PORT_NUM,
							PORT_MASK),
						&voice_mhi_lcl);
	if (voice_mhi_lcl.apr_mvm_handle == NULL) {
		pr_err("%s: error in APR register\n", __func__);
		ret = -ENODEV;
	}
	mutex_unlock(&voice_mhi_lcl.mutex);

	return ret;
}

static int voice_mhi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node;
	uint32_t mem_size = 0;
	void *ptr;
	dma_addr_t phys_addr, iova;
	const __be32 *cell;

	pr_debug("%s:\n", __func__);

	INIT_WORK(&voice_mhi_lcl.voice_mhi_work_pcie,
				voice_mhi_map_pcie_and_send);
	INIT_WORK(&voice_mhi_lcl.voice_mhi_work_adsp,
				voice_mhi_register_apr_and_send);
	init_waitqueue_head(&voice_mhi_lcl.voice_mhi_wait);

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (node) {
		cell = of_get_property(node, "size", NULL);
		if (cell)
			mem_size = of_read_number(cell, 2);
		else {
			pr_err("%s: cell not found\n", __func__);
			ret = -EINVAL;
			goto done;
		}
	} else {
		pr_err("%s: Node read failed\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	pr_debug("%s: mem_size = %d\n", __func__, mem_size);

	if (mem_size) {
		ptr = dma_alloc_attrs(&pdev->dev, mem_size, &phys_addr,
					GFP_KERNEL, DMA_ATTR_NO_KERNEL_MAPPING);
		if (IS_ERR_OR_NULL(ptr)) {
			pr_err("%s: Memory alloc failed\n", __func__);
			ret = -ENOMEM;
			goto done;
		} else {
			pr_debug("%s: Memory alloc success phys_addr:0x%lx\n",
					 __func__, (unsigned long)phys_addr);
		}

		ret = msm_audio_ion_dma_map(&phys_addr, &iova, mem_size,
					DMA_BIDIRECTIONAL);
		if (ret) {
			pr_err("%s: dma mapping failed %d\n", __func__, ret);
			goto err_free;
		}
		pr_debug("%s: dma_mapping_success iova:0x%lx\n",
				 __func__, (unsigned long)iova);

		voice_mhi_lcl.dev_info.phys_addr.base = phys_addr;
		voice_mhi_lcl.dev_info.iova_adsp.base = iova;
		voice_mhi_lcl.dev_info.iova_adsp.size = mem_size;
		voice_mhi_lcl.dev_info.iova_pcie.size = mem_size;
		VOICE_MHI_STATE_SET(voice_mhi_lcl.voice_mhi_state,
				VOICE_MHI_ADSP_UP);

		ret = voice_mhi_apr_register();
		/* If fails register during audio notifier UP event */
		if (ret)
			pr_err("%s: APR register failed %d\n", __func__, ret);
		ret = mhi_driver_register(&voice_mhi_driver);
		if (ret) {
			pr_err("%s: mhi register failed %d\n", __func__, ret);
			goto done;
		}

		ret = audio_notifier_register("voice_mhi",
				AUDIO_NOTIFIER_ADSP_DOMAIN,
				&voice_mhi_service_nb);
		if (ret < 0)
			pr_err("%s: Audio notifier register failed ret = %d\n",
				__func__, ret);

		mutex_lock(&voice_mhi_lcl.mutex);
		voice_mhi_lcl.dev_info.pdev = pdev;
		voice_mhi_lcl.pcie_enabled = true;
		VOICE_MHI_STATE_SET(voice_mhi_lcl.voice_mhi_state,
				VOICE_MHI_PROBED);
		mutex_unlock(&voice_mhi_lcl.mutex);
	} else {
		pr_err("%s: Memory size can't be zero\n", __func__);
		ret = -ENOMEM;
		goto done;
	}

done:
	return ret;
err_free:
	dma_free_attrs(&pdev->dev, mem_size, ptr, phys_addr,
						   DMA_ATTR_NO_KERNEL_MAPPING);
	return 0;

}

static int voice_mhi_remove(struct platform_device *pdev)
{
	mhi_driver_unregister(&voice_mhi_driver);
	return 0;
}
static const struct of_device_id voice_mhi_of_match[]  = {
	{ .compatible = "qcom,voice-mhi-audio", },
	{},
};
static struct platform_driver voice_mhi_platform_driver = {
	.probe = voice_mhi_probe,
	.remove = voice_mhi_remove,
	.driver = {
		.name = "voice_mhi_audio",
		.owner = THIS_MODULE,
		.of_match_table = voice_mhi_of_match,
	}
};

int __init voice_mhi_init(void)
{
	int ret = 0;

	memset(&voice_mhi_lcl, 0, sizeof(voice_mhi_lcl));
	mutex_init(&voice_mhi_lcl.mutex);

	//Add remaining init here
	voice_mhi_lcl.pcie_enabled = false;
	voice_mhi_lcl.voice_mhi_state = VOICE_MHI_INIT;
	voice_mhi_lcl.vote_count = 0;
	voice_mhi_lcl.apr_mvm_handle = NULL;
	ret = platform_driver_register(&voice_mhi_platform_driver);

	return ret;
}

void __exit voice_mhi_exit(void)
{
	mutex_destroy(&voice_mhi_lcl.mutex);
	platform_driver_unregister(&voice_mhi_platform_driver);
}

MODULE_DESCRIPTION("Voice MHI module");
MODULE_LICENSE("GPL v2");
