// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifdef CCCI_KMODULE_ENABLE
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/clk.h> /* for clk_prepare/un* */

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_fsm_internal.h"
#include "md_sys1_platform.h"
#include "modem_secure_base.h"

#ifdef FEATURE_SCP_CCCI_SUPPORT
#include "scp_ipi.h"

#ifdef CCCI_KMODULE_ENABLE
void ccci_scp_md_state_sync(int md_state);

struct ccci_fsm_scp ccci_scp_ctl = {
	.md_id = 0,
	.md_state_sync = &ccci_scp_md_state_sync,
};

static struct ccci_clk_node scp_clk_table[] = {
	{ NULL, "infra-ccif2-ap"},
	{ NULL, "infra-ccif2-md"},
};

void ccci_scp_md_state_sync(int md_state)
{
	schedule_work(&ccci_scp_ctl.scp_md_state_sync_work);
}


/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;
#endif

static atomic_t scp_state = ATOMIC_INIT(SCP_CCCI_STATE_INVALID);
static struct ccci_ipi_msg scp_ipi_tx_msg;
static struct mutex scp_ipi_tx_mutex;
static struct work_struct scp_ipi_rx_work;
static wait_queue_head_t scp_ipi_rx_wq;
static struct ccci_skb_queue scp_ipi_rx_skb_list;
static unsigned int init_work_done;
static unsigned int scp_clk_last_state;
static struct ccci_ipi_msg scp_ipi_rx_msg;

static int ccci_scp_ipi_send(int md_id, int op_id, void *data)
{
	int ret = 0;
	int ipi_status = 0;
	unsigned int cnt = 0;

	if (atomic_read(&scp_state) == SCP_CCCI_STATE_INVALID) {
		CCCI_ERROR_LOG(md_id, FSM,
			"ignore IPI %d, SCP state %d!\n",
			op_id, atomic_read(&scp_state));
		return -CCCI_ERR_MD_NOT_READY;
	}

	mutex_lock(&scp_ipi_tx_mutex);
	memset(&scp_ipi_tx_msg, 0, sizeof(scp_ipi_tx_msg));
	scp_ipi_tx_msg.md_id = md_id;
	scp_ipi_tx_msg.op_id = op_id;
	scp_ipi_tx_msg.data[0] = *((u32 *)data);
	CCCI_NORMAL_LOG(scp_ipi_tx_msg.md_id, FSM,
		"IPI send op_id=%d/data=0x%x, size=%d\n",
		scp_ipi_tx_msg.op_id, scp_ipi_tx_msg.data[0],
		(int)sizeof(struct ccci_ipi_msg));

	while (1) {
		ipi_status = mtk_ipi_send(&scp_ipidev, IPI_OUT_APCCCI_0,
		0, &scp_ipi_tx_msg, (sizeof(scp_ipi_tx_msg) / 4), 1);
		if (ipi_status != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			CCCI_ERROR_LOG(md_id, FSM, "IPI send 10 times!\n");
			/* aee_kernel_warning("ccci", "ipi:tx busy");*/
			break;
		}
	}
	if (ipi_status != IPI_ACTION_DONE) {
		CCCI_ERROR_LOG(md_id, FSM, "IPI send fail!\n");
		ret = -CCCI_ERR_MD_NOT_READY;
	}

	mutex_unlock(&scp_ipi_tx_mutex);
	return ret;
}

static int scp_set_clk_cg(unsigned int on)
{
	int idx, ret;

	if (!(on == 0 || on == 1)) {
		CCCI_ERROR_LOG(MD_SYS1, FSM,
			"%s:on=%u is invalid\n", __func__, on);
		return -1;
	}

	if (on == scp_clk_last_state) {
		CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s:on=%u skip set scp clk!\n",
			__func__, on);
		return 0;
	}

	/* Before OFF CCIF2 clk, set the ACK register to 1 */
	if (on == 0) {
		if (!ccci_scp_ctl.ccif2_ap_base || !ccci_scp_ctl.ccif2_md_base) {
			CCCI_ERROR_LOG(MD_SYS1, FSM, "%s can't ack ccif2\n",
				       __func__);
		} else {
			ccci_write32(ccci_scp_ctl.ccif2_ap_base, APCCIF_ACK, 0xFFFF);
			ccci_write32(ccci_scp_ctl.ccif2_md_base, APCCIF_ACK, 0xFFFF);
			CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s, ack ccif2 reg done!\n",
					__func__);
		}
	}

	for (idx = 0; idx < ARRAY_SIZE(scp_clk_table); idx++) {
		if (on) {
			ret = clk_prepare_enable(scp_clk_table[idx].clk_ref);
			if (ret) {
				CCCI_ERROR_LOG(MD_SYS1, FSM,
					"open scp clk fail:%s,ret=%d\n",
					scp_clk_table[idx].clk_name, ret);
				return -1;
			}
		} else
			clk_disable_unprepare(scp_clk_table[idx].clk_ref);
	}

	CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s:on=%u set done!\n",
		__func__, on);
	scp_clk_last_state = on;

	return 0;
}

static void ccci_notify_atf_set_scpmem(void)
{
	struct arm_smccc_res res = {0};

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, SCP_CLK_SET_DONE,
		0, 0, 0, 0, 0, 0, &res);
	CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s [done]\n", __func__);
}


static void ccci_scp_md_state_sync_work(struct work_struct *work)
{
	struct ccci_fsm_scp *scp_ctl = container_of(work,
		struct ccci_fsm_scp, scp_md_state_sync_work);
	int ret;
	enum MD_STATE_FOR_USER state =
		ccci_fsm_get_md_state_for_user(scp_ctl->md_id);
	int count = 0;

	switch (state) {
	case MD_STATE_READY:
		if (scp_ctl->md_id == MD_SYS1) {
			while (count < SCP_BOOT_TIMEOUT/EVENT_POLL_INTEVAL) {
				if (atomic_read(&scp_state) ==
					SCP_CCCI_STATE_BOOTING
					|| atomic_read(&scp_state)
					== SCP_CCCI_STATE_RBREADY
					|| atomic_read(&scp_state)
					== SCP_CCCI_STATE_STOP)
					break;
				count++;
				msleep(EVENT_POLL_INTEVAL);
			}
			if (count == SCP_BOOT_TIMEOUT/EVENT_POLL_INTEVAL)
				CCCI_ERROR_LOG(scp_ctl->md_id, FSM,
					"SCP init not ready!\n");
			else {
				ret = scp_set_clk_cg(1);
				if (ret) {
					CCCI_ERROR_LOG(scp_ctl->md_id, FSM,
						"fail to set scp clk, ret = %d\n", ret);
					break;
				}
				ccci_notify_atf_set_scpmem();
				ret = ccci_port_send_msg_to_md(scp_ctl->md_id,
					CCCI_SYSTEM_TX, CCISM_SHM_INIT, 0, 1);
				if (ret < 0)
					CCCI_ERROR_LOG(scp_ctl->md_id, FSM,
						"fail to send CCISM_SHM_INIT %d\n",
						ret);
			}
		} else
			break;
		break;
	case MD_STATE_EXCEPTION:
	case MD_STATE_INVALID:
		ccci_scp_ipi_send(scp_ctl->md_id,
			CCCI_OP_MD_STATE, &state);
		break;
	default:
		break;
	};
}

static void ccci_scp_ipi_rx_work(struct work_struct *work)
{
	struct ccci_ipi_msg *ipi_msg_ptr = NULL;
	struct sk_buff *skb = NULL;
	int data, ret;

	while (!skb_queue_empty(&scp_ipi_rx_skb_list.skb_list)) {
		skb = ccci_skb_dequeue(&scp_ipi_rx_skb_list);
		if (skb == NULL) {
			CCCI_ERROR_LOG(-1, CORE,
				"ccci_skb_dequeue fail\n");
			return;
		}
		ipi_msg_ptr = (struct ccci_ipi_msg *)skb->data;
		if (!get_modem_is_enabled(ipi_msg_ptr->md_id)) {
			CCCI_ERROR_LOG(ipi_msg_ptr->md_id,
				CORE, "MD not exist\n");
			return;
		}
		switch (ipi_msg_ptr->op_id) {
		case CCCI_OP_SCP_STATE:
			switch (ipi_msg_ptr->data[0]) {
			case SCP_CCCI_STATE_BOOTING:
				if (atomic_read(&scp_state) ==
					SCP_CCCI_STATE_RBREADY) {
					CCCI_NORMAL_LOG(ipi_msg_ptr->md_id, FSM,
						"SCP reset detected\n");
					ccci_port_send_msg_to_md(MD_SYS1,
					CCCI_SYSTEM_TX, CCISM_SHM_INIT, 0, 1);
					ccci_port_send_msg_to_md(MD_SYS3,
					CCCI_CONTROL_TX,
					C2K_CCISM_SHM_INIT, 0, 1);
				} else {
					CCCI_NORMAL_LOG(ipi_msg_ptr->md_id, FSM,
						"SCP boot up\n");
				}
				/* too early to init share memory here,
				 * EMI MPU may not be ready yet
				 */
				break;
			case SCP_CCCI_STATE_RBREADY:
				switch (ipi_msg_ptr->md_id) {
				case MD_SYS1:
					ccci_port_send_msg_to_md(MD_SYS1,
					CCCI_SYSTEM_TX,
					CCISM_SHM_INIT_DONE, 0, 1);
					break;
				case MD_SYS3:
					ccci_port_send_msg_to_md(MD_SYS3,
					CCCI_CONTROL_TX,
					C2K_CCISM_SHM_INIT_DONE, 0, 1);
					break;
				};
				data =
				ccci_fsm_get_md_state_for_user(
					ipi_msg_ptr->md_id);
				ccci_scp_ipi_send(ipi_msg_ptr->md_id,
					CCCI_OP_MD_STATE, &data);
				break;
			case SCP_CCCI_STATE_STOP:
				CCCI_NORMAL_LOG(ipi_msg_ptr->md_id, FSM,
						"MD INVALID,scp send ack to ap\n");
				ret = scp_set_clk_cg(0);
				if (ret)
					CCCI_ERROR_LOG(ipi_msg_ptr->md_id, FSM,
						"fail to set scp clk, ret = %d\n", ret);
				break;
			default:
				break;
			};
			atomic_set(&scp_state, ipi_msg_ptr->data[0]);
			break;
		default:
			break;
		};
		ccci_free_skb(skb);
	}
}

/*
 * IPI for logger init
 * @param id:   IPI id
 * @param prdata: callback function parameter
 * @param data:  IPI data
 * @param len: IPI data length
 */
static int ccci_scp_ipi_handler(unsigned int id, void *prdata, void *data,
			unsigned int len)
{
	struct sk_buff *skb = NULL;

	if (len != sizeof(struct ccci_ipi_msg)) {
		CCCI_ERROR_LOG(-1, CORE,
		"IPI handler, data length wrong %d vs. %d\n",
		len, (int)sizeof(struct ccci_ipi_msg));
		return -1;
	}

	skb = ccci_alloc_skb(len, 0, 0);
	if (!skb)
		return -1;

	memcpy(skb_put(skb, len), data, len);
	ccci_skb_enqueue(&scp_ipi_rx_skb_list, skb);
	/* ipi_send use mutex, can not be called from ISR context */
	schedule_work(&scp_ipi_rx_work);

	return 0;
}
#endif

int fsm_ccism_init_ack_handler(int md_id, int data)
{
#ifdef FEATURE_SCP_CCCI_SUPPORT
	struct ccci_smem_region *ccism_scp =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_CCISM_SCP);

	memset_io(ccism_scp->base_ap_view_vir, 0, ccism_scp->size);
	ccci_scp_ipi_send(md_id, CCCI_OP_SHM_INIT,
		&ccism_scp->base_ap_view_phy);
#endif
	return 0;
}

static int fsm_sim_type_handler(int md_id, int data)
{
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(md_id);

	per_md_data->sim_type = data;
	return 0;
}

#ifdef CCCI_KMODULE_ENABLE
#ifdef FEATURE_SCP_CCCI_SUPPORT
void fsm_scp_init0(void)
{
	enum MD_STATE_FOR_USER state =
		ccci_fsm_get_md_state_for_user(ccci_scp_ctl.md_id);
	mutex_init(&scp_ipi_tx_mutex);

	if (!init_work_done) {
		INIT_WORK(&scp_ipi_rx_work, ccci_scp_ipi_rx_work);
		init_work_done = 1;
	}
	init_waitqueue_head(&scp_ipi_rx_wq);
	ccci_skb_queue_init(&scp_ipi_rx_skb_list, 16, 16, 0);

	CCCI_NORMAL_LOG(-1, FSM, "register IPI\n");

	if (mtk_ipi_register(&scp_ipidev, IPI_IN_APCCCI_0,
		(void *)ccci_scp_ipi_handler, NULL,
		&scp_ipi_rx_msg) != IPI_ACTION_DONE)
		CCCI_ERROR_LOG(-1, FSM, "register IPI fail!\n");

	atomic_set(&scp_state, SCP_CCCI_STATE_BOOTING);

	if (state != MD_STATE_INVALID)
		ccci_scp_md_state_sync(state);
}

static int apsync_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY:
		fsm_scp_init0();
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block apsync_notifier = {
	.notifier_call = apsync_event,
};
#endif
#endif

static int ccif_scp_clk_init(struct device *dev)
{
	int idx = 0;

	for (idx = 0; idx < ARRAY_SIZE(scp_clk_table); idx++) {
		scp_clk_table[idx].clk_ref = devm_clk_get(dev,
			scp_clk_table[idx].clk_name);
		if (IS_ERR(scp_clk_table[idx].clk_ref)) {
			CCCI_ERROR_LOG(-1, FSM,
				"%s:scp get %s failed\n",
				__func__, scp_clk_table[idx].clk_name);
			scp_clk_table[idx].clk_ref = NULL;
			return -1;
		}
	}

	return 0;
}

static int fsm_scp_hw_init(struct ccci_fsm_scp *scp_ctl, struct device *dev)
{
	scp_ctl->ccif2_ap_base = of_iomap(dev->of_node, 0);
	scp_ctl->ccif2_md_base = of_iomap(dev->of_node, 1);

	if (!scp_ctl->ccif2_ap_base || !scp_ctl->ccif2_md_base) {
		CCCI_ERROR_LOG(-1, FSM,
			"ccif2_ap_base=NULL or ccif2_md_base=NULL\n");
		return -1;
	}

	return 0;
}

int fsm_scp_init(struct ccci_fsm_scp *scp_ctl, struct device *dev)
{
	int ret = 0;
#ifndef CCCI_KMODULE_ENABLE
	struct ccci_fsm_ctl *ctl =
		container_of(scp_ctl, struct ccci_fsm_ctl, scp_ctl);
#endif

	ret = fsm_scp_hw_init(scp_ctl, dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, FSM, "ccci scp hw init fail\n");
		return ret;
	}

	ret = ccif_scp_clk_init(dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, FSM, "ccif scp clk init fail\n");
		return ret;
	}

#ifdef FEATURE_SCP_CCCI_SUPPORT
	scp_A_register_notify(&apsync_notifier);
#endif
#ifndef CCCI_KMODULE_ENABLE
	scp_ctl->md_id = ctl->md_id;
#endif
#ifdef FEATURE_SCP_CCCI_SUPPORT
	INIT_WORK(&scp_ctl->scp_md_state_sync_work,
		ccci_scp_md_state_sync_work);
	register_ccci_sys_call_back(scp_ctl->md_id, CCISM_SHM_INIT_ACK,
		fsm_ccism_init_ack_handler);
#endif

	register_ccci_sys_call_back(scp_ctl->md_id, MD_SIM_TYPE,
		fsm_sim_type_handler);

	return ret;
}

#ifdef CCCI_KMODULE_ENABLE
#ifdef FEATURE_SCP_CCCI_SUPPORT
int ccci_scp_probe(struct platform_device *pdev)
{
	int ret;

	ret = fsm_scp_init(&ccci_scp_ctl, &pdev->dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, FSM, "ccci get scp info fail");
		return ret;
	}

	ccci_fsm_scp_register(0, &ccci_scp_ctl);

	return 0;
}

static const struct of_device_id ccci_scp_of_ids[] = {
	{.compatible = "mediatek,ccci_md_scp"},
	{}
};

static struct platform_driver ccci_scp_driver = {

	.driver = {
		.name = "ccci_md_scp",
		.of_match_table = ccci_scp_of_ids,
	},

	.probe = ccci_scp_probe,
};

static int __init ccci_scp_init(void)
{
	int ret;

	CCCI_NORMAL_LOG(-1, FSM, "ccci scp driver init start\n");

	ret = platform_driver_register(&ccci_scp_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, FSM, "ccci scp driver init fail %d", ret);
		return ret;
	}
	CCCI_NORMAL_LOG(-1, FSM, "ccci scp driver init end\n");
	return 0;
}

module_init(ccci_scp_init);
#endif
MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci scp driver");
MODULE_LICENSE("GPL");

#endif
