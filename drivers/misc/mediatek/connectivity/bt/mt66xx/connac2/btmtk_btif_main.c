/*
 *  Copyright (c) 2016,2017 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/spinlock_types.h>
#include <linux/kthread.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/timer.h>

#include "conninfra.h"
#include "mtk_btif_exp.h"
#include "connectivity_build_in_adapter.h"
#include "btmtk_config.h"
#include "btmtk_define.h"
#include "btmtk_main.h"
#include "btmtk_btif.h"
#include "btmtk_mt66xx_reg.h"

#if SUPPORT_COREDUMP
#include "connsys_debug_utility.h"
#endif


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define LOG TRUE
#define MTKBT_BTIF_NAME			"mtkbt_btif"
#define BTIF_OWNER_NAME 		"CONSYS_BT"

#define LPCR_POLLING_RTY_LMT		4096 /* 16*n */
#define LPCR_MASS_DUMP_LMT		4000
#define BTIF_IDLE_WAIT_TIME		32 /* ms */

#define MAX_RESET_COUNT			(3)
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
uint8_t *p_conn_infra_base_addr;
uint8_t *p_bgfsys_base_addr;
struct btmtk_dev *g_bdev;
static unsigned long g_btif_id; /* The user identifier to operate btif */
struct task_struct *g_btif_rxd_thread;

static struct platform_driver mtkbt_btif_driver = {
	.driver = {
		.name = MTKBT_BTIF_NAME,
		.owner = THIS_MODULE,
	},
	.probe = NULL,
	.remove = NULL,
};

#if (USE_DEVICE_NODE == 0)
static struct platform_device *mtkbt_btif_device;
#endif

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/* bt_reg_init
 *
 *    Remapping control registers
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bt_reg_init(void)
{
	int32_t ret = -1;
	struct device_node *node = NULL;
	struct consys_reg_base_addr *base_addr = NULL;
	struct resource res;
	int32_t flag, i = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,bt");
	if (node) {
		for (i = 0; i < CONSYS_BASE_ADDR_MAX; i++) {
			base_addr = &bt_reg.reg_base_addr[i];

			ret = of_address_to_resource(node, i, &res);
			if (ret) {
				BTMTK_ERR("Get Reg Index(%d) failed", i);
				return -1;
			}

			base_addr->phy_addr = res.start;
			base_addr->vir_addr =
				(unsigned long) of_iomap(node, i);
			of_get_address(node, i, &(base_addr->size), &flag);

			BTMTK_DBG("Get Index(%d) phy(0x%zx) baseAddr=(0x%zx) size=(0x%zx)",
				i, base_addr->phy_addr, base_addr->vir_addr,
				base_addr->size);
		}

	} else {
		BTMTK_ERR("[%s] can't find CONSYS compatible node\n", __func__);
		return -1;
	}
	return 0;
}

/* bt_reg_deinit
 *
 *    Release the memory of remapping address
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *    N/A
 *
 */
int32_t bt_reg_deinit(void)
{
	int i = 0;

	for (i = 0; i < CONSYS_BASE_ADDR_MAX; i++) {
		if (bt_reg.reg_base_addr[i].vir_addr) {
			iounmap((void __iomem*)bt_reg.reg_base_addr[i].vir_addr);
			bt_reg.reg_base_addr[i].vir_addr = 0;
		}
	}
	return 0;
}

static struct notifier_block bt_fb_notifier;
static int btmtk_fb_notifier_callback(struct notifier_block
				*self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int32_t blank = 0;
	int32_t val = 0;

	if ((event != FB_EVENT_BLANK))
		return 0;

	blank = *(int32_t *)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_POWERDOWN:
		if(g_bdev->bt_state == FUNC_ON) {
			if (conninfra_reg_readable()) {
				val = REG_READL(CON_REG_SPM_BASE_ADDR + 0x26C);
				BTMTK_INFO("%s: HOST_MAILBOX_BT_ADDR[0x1806026C] read[0x%08x]", __func__, val);
			}

			BTMTK_INFO("%s: blank state [%ld]->[%ld], and send cmd", __func__, g_bdev->blank_state, blank);
			g_bdev->blank_state = blank;
			btmtk_send_blank_status_cmd(g_bdev->hdev, blank);
		} else {
			BTMTK_INFO("%s: blank state [%ld]->[%ld]", __func__, g_bdev->blank_state, blank);
			g_bdev->blank_state = blank;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int btmtk_fb_notify_register(void)
{
	int32_t ret;

	bt_fb_notifier.notifier_call = btmtk_fb_notifier_callback;

	ret = fb_register_client(&bt_fb_notifier);
	if (ret)
		BTMTK_ERR("Register wlan_fb_notifier failed:%d\n", ret);
	else
		BTMTK_DBG("Register wlan_fb_notifier succeed\n");

	return ret;
}

static void btmtk_fb_notify_unregister(void)
{
	fb_unregister_client(&bt_fb_notifier);
}

/* btmtk_cif_dump_fw_no_rsp
 *
 *    Dump fw/driver own cr and btif cr if
 *    fw response timeout
 *
 * Arguments:
 *
 *
 * Return Value:
 *     N/A
 *
 */
void btmtk_cif_dump_fw_no_rsp(unsigned int flag)
{
	BTMTK_WARN("%s! [%u]", __func__, flag);
	if (!g_btif_id) {
		BTMTK_ERR("NULL BTIF ID reference!");
	} else {
		if (flag & BT_BTIF_DUMP_OWN_CR)
			bt_dump_cif_own_cr();

		if (flag & BT_BTIF_DUMP_REG)
			mtk_wcn_btif_dbg_ctrl(g_btif_id, BTIF_DUMP_BTIF_REG);

		if (flag & BT_BTIF_DUMP_LOG)
			mtk_wcn_btif_dbg_ctrl(g_btif_id, BTIF_DUMP_LOG);

		if (flag & BT_BTIF_DUMP_DMA)
			mtk_wcn_btif_dbg_ctrl(g_btif_id, BTIF_DUMP_DMA_VFIFO);

	}
}


void btmtk_cif_dump_rxd_backtrace(void)
{
	if (!g_btif_rxd_thread)
		BTMTK_ERR("g_btif_rxd_thread == NULL");
	else
		KERNEL_show_stack(g_btif_rxd_thread, NULL);
}


/* btmtk_cif_dump_btif_tx_no_rsp
 *
 *    Dump btif cr if fw no response yet
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     N/A
 *
 */
void btmtk_cif_dump_btif_tx_no_rsp(void)
{
	BTMTK_INFO("%s", __func__);
	if (!g_btif_id) {
		BTMTK_ERR("NULL BTIF ID reference!");
	} else {
		mtk_wcn_btif_dbg_ctrl(g_btif_id, BTIF_DUMP_BTIF_IRQ);
	}
}

/* btmtk_cif_fw_own_clr()
 *
 *    Ask FW to wakeup from sleep state, driver should invoke this function
 *    before communitcate with FW/HW
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 */
static int32_t btmtk_cif_fw_own_clr(void)
{
	uint32_t lpctl_cr;
	int32_t retry = LPCR_POLLING_RTY_LMT;

	do {
		/* assume wait interval 0.5ms each time,
		 * wait maximum total 7ms to query status
		 */
		if ((retry & 0xF) == 0) { /* retry % 16 == 0 */
			if (((retry < LPCR_POLLING_RTY_LMT && retry >= LPCR_MASS_DUMP_LMT) || (retry == 2048) || (retry == 32)) &&
				((retry & 0x1F) == 0)) {
				BTMTK_WARN("FW own clear failed in %d ms, retry[%d]", (LPCR_POLLING_RTY_LMT - retry) / 2, retry);
				bt_dump_cif_own_cr();
				REG_WRITEL(BGF_LPCTL, BGF_HOST_CLR_FW_OWN_B);
				BTMTK_WARN("FW own clear dump after write:");
				bt_dump_cif_own_cr();
			} else
				REG_WRITEL(BGF_LPCTL, BGF_HOST_CLR_FW_OWN_B);
		}

		lpctl_cr = REG_READL(BGF_LPCTL);
		if (!(lpctl_cr & BGF_OWNER_STATE_SYNC_B)) {
			REG_WRITEL(BGF_IRQ_STAT, BGF_IRQ_FW_OWN_CLR_B);
			break;
		}

		usleep_range(500, 550);
		retry --;
	} while (retry > 0);

	if (retry == 0) {
		BTMTK_ERR("FW own clear (Wakeup) failed!");
		bt_dump_cif_own_cr();

		/* dump cpupcr, 10 times with 1ms interval */
		bt_dump_cpupcr(10 , 5);
		return -1;
	}

	BTMTK_DBG("FW own clear (Wakeup) succeed");
	return 0;
}

/* btmtk_cif_fw_own_set()
 *
 *    Ask FW to sleep for power saving
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t btmtk_cif_fw_own_set(void)
{
	uint32_t irqstat_cr;
	int32_t retry = LPCR_POLLING_RTY_LMT;

	do {
		if ((retry & 0xF) == 0) { /* retry % 16 == 0 */
			if (((retry < LPCR_POLLING_RTY_LMT && retry >= LPCR_MASS_DUMP_LMT) || (retry == 2048) || (retry == 32)) &&
				((retry & 0x1F) == 0)) {
				BTMTK_WARN("FW own set failed in %d ms, retry[%d]", (LPCR_POLLING_RTY_LMT - retry) / 2, retry);
				bt_dump_cif_own_cr();
				REG_WRITEL(BGF_LPCTL, BGF_HOST_SET_FW_OWN_B);
				BTMTK_WARN("FW own set dump after write:");
				bt_dump_cif_own_cr();
			} else
				REG_WRITEL(BGF_LPCTL, BGF_HOST_SET_FW_OWN_B);
		}

		/*
		 * As current H/W design, BGF_LPCTL.OWNER_STATE_SYNC bit will be automatically
		 * asserted by H/W once driver set FW own, not waiting for F/W's ack,
		 * which means F/W may still transmit data to host at that time.
		 * So we cannot check this bit for FW own done identity, use BGF_IRQ_FW_OWN_SET
		 * bit for instead, even on polling mode and the interrupt is masked.
		 *
		 * This is a work-around, H/W will change design on ECO.
		 */
		irqstat_cr = REG_READL(BGF_IRQ_STAT2);
		if (irqstat_cr & BGF_IRQ_FW_OWN_SET_B) {
			/* Write 1 to clear IRQ */
			REG_WRITEL(BGF_IRQ_STAT2, BGF_IRQ_FW_OWN_SET_B);
			break;
		}

		usleep_range(500, 550);
		retry --;
	} while (retry > 0);

	if (retry == 0) {
		BTMTK_ERR("FW own set (Sleep) failed!");
		bt_dump_cif_own_cr();

		/* dump cpupcr, 10 times with 1ms interval */
		bt_dump_cpupcr(10 , 5);
		return -1;
	}

	BTMTK_DBG("FW own set (Sleep) succeed");
	return 0;
}

int bt_chip_reset_flow(enum bt_reset_level rst_level,
			     enum consys_drv_type drv,
			     char *reason)
{
	u_int8_t is_1st_reset = (g_bdev->rst_count == 0) ? TRUE : FALSE;
	uint8_t retry = 15;
	int32_t dump_property = 0; /* default = 0 */
	int32_t ret = 0;

	/* dump debug message */
	show_all_dump_packet();
	btmtk_cif_dump_fw_no_rsp(BT_BTIF_DUMP_ALL);

	g_bdev->rst_count++;
	while (g_bdev->rst_level != RESET_LEVEL_NONE && retry > 0) {
		msleep(200);
		retry--;
	}

	if (retry == 0) {
		BTMTK_ERR("Unable to trigger pre_chip_rst due to unfinish previous reset");
		return -1;
	}

	g_bdev->rst_level = rst_level;

	if (is_1st_reset) {
		/*
		 * 1. Trigger BT PAD_EINT to force FW coredump,
		 *    trigger coredump only if the first time reset
		 *    (compare with the case of subsys reset fail)
		 */
		CLR_BIT(BGF_PAD_EINT, BIT(9));
		bt_enable_irq(BGF2AP_SW_IRQ);

		/* 2. Wait for IRQ */
		if (!wait_for_completion_timeout(&g_bdev->rst_comp, msecs_to_jiffies(2000))) {
#if SUPPORT_COREDUMP
			dump_property = CONNSYS_DUMP_PROPERTY_NO_WAIT;
#endif
			BTMTK_ERR("uanble to get reset IRQ in 2000ms");
		}

		/* 3. Reset PAD_INT CR */
		SET_BIT(BGF_PAD_EINT, BIT(9));
#if SUPPORT_COREDUMP
		/* 4. Do coredump, only do this while BT is on */
		down(&g_bdev->halt_sem);
		if (g_bdev->bt_state == FUNC_ON) {
			bt_dump_bgfsys_debug_cr();
			connsys_coredump_start(g_bdev->coredump_handle, dump_property, drv, reason);
		} else
			BTMTK_WARN("BT is not at on state, skip coredump: [%d]",
					g_bdev->bt_state);
		up(&g_bdev->halt_sem);
#endif
	}

	/* Dump assert reason, only for subsys reset */
	if (rst_level == RESET_LEVEL_0_5) {
		phys_addr_t emi_ap_phy_base;
		uint8_t *dump_msg_addr;
		uint8_t msg[256] = {0};

		conninfra_get_phy_addr((uint32_t*)&emi_ap_phy_base, NULL);
		emi_ap_phy_base &= 0xFFFFFFFF;

		dump_msg_addr = ioremap_nocache(emi_ap_phy_base + 0x3B000, 0x100);
		if (dump_msg_addr) {
			memcpy(msg, dump_msg_addr, 0xFF);
			iounmap(dump_msg_addr);
			msg[0xFF] = 0;
			BTMTK_INFO("FW Coredump Msg: [%s]", msg);
		} else {
			BTMTK_ERR("uanble to remap dump msg addr");
		}
	}

	/* directly return if following cases
	 * 1. BT is already off
	 * 2. another reset is already done whole procdure and back to normal
	 */
	if (g_bdev->bt_state == FUNC_OFF || g_bdev->rst_level == RESET_LEVEL_NONE) {
		BTMTK_INFO("BT is already off or reset success, skip power off [%d, %d]",
			g_bdev->bt_state, g_bdev->rst_level);
		return 0;
	}

	g_bdev->bt_state = RESET_START;
	bt_notify_state(g_bdev);

	/* 5. Turn off BT */
	ret = g_bdev->hdev->close(g_bdev->hdev);
#if (USE_DEVICE_NODE == 0)
	bt_report_hw_error();
#endif
	return ret;
}


/*******************************************************************************
*                   C A L L   B A C K   F U N C T I O N S
********************************************************************************
*/
/* bt_pre_chip_rst_handler()
 *
 *    Pre-action of chip reset (before HW power off), driver should ask
 *    conninfra to do coredump and then turn off bt driver
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int bt_pre_chip_rst_handler(enum consys_drv_type drv, char *reason)
{
	// skip reset flow if bt is not on
	if (g_bdev->bt_state == FUNC_OFF)
		return 0;
	else
		return bt_chip_reset_flow(RESET_LEVEL_0, drv, reason);
}

/* bt_post_chip_rst_handler()
 *
 *    Post-action of chip reset (after HW power on), turn on BT
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int bt_post_chip_rst_handler(void)
{
	bt_notify_state(g_bdev);
	return 0;
}

/* bt_do_pre_power_on()
 *
 * Only set the flag to pre-calibration mode here
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 always (for success)
 *
 */
static int bt_do_pre_power_on(void)
{
	return btmtk_set_power_on(g_bdev->hdev, TRUE);
}

/* bt_do_calibration()
 *
 *    calibration flow is control by conninfra driver, driver should implement
 *    the function of calibration callback, here what driver do is send cmd to
 *    FW to get calibration data (BT calibration is done in BT func on) and
 *    backup the calibration data
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bt_do_calibration(void)
{
	int32_t ret = 0;

	if(g_bdev->bt_state != FUNC_ON) {
		BTMTK_ERR("%s: bt is not on, skip calibration", __func__);
		return CONNINFRA_CB_RET_CAL_FAIL_POWER_OFF;
	}

	ret = btmtk_calibration_flow(g_bdev->hdev);
	/* return -1 means driver unable to recv calibration event and reseting
	 * In such case, we don't have to turn off bt, it will be handled by 
	 * reset thread */
	if (ret != -1 && g_bdev->bt_precal_state == FUNC_OFF)
		btmtk_set_power_off(g_bdev->hdev, TRUE);

	if (ret == -1) {
		BTMTK_ERR("%s: error return in recving calibration event, reset", __func__);
		return CONNINFRA_CB_RET_CAL_FAIL_POWER_ON;
	} else if (ret) {
		BTMTK_ERR("%s: error return in sent calibration cmd", __func__);
		return (g_bdev->bt_precal_state) ? CONNINFRA_CB_RET_CAL_FAIL_POWER_ON :
						   CONNINFRA_CB_RET_CAL_FAIL_POWER_OFF;
	} else
		return (g_bdev->bt_precal_state) ? CONNINFRA_CB_RET_CAL_PASS_POWER_ON :
						   CONNINFRA_CB_RET_CAL_PASS_POWER_OFF;

}

/* btmtk_send_thermal_query_cmd
 *
 *    Send query thermal (a-die) command to FW
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *    Thermal value
 *
 */
static int32_t bt_query_thermal(void)
{
	uint8_t cmd[] = { 0x01, 0x91, 0xFD, 0x00 };
	struct bt_internal_cmd *p_inter_cmd = &g_bdev->internal_cmd;
	struct wmt_pkt_param *evt = &p_inter_cmd->wmt_event_params;
	uint8_t *evtbuf = NULL;
	int32_t ret = 0;
	int32_t thermal = 0;
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0x0E, 0x08, 0x01, 0x91, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00 }; */

	BTMTK_INFO("[InternalCmd] %s", __func__);
	down(&g_bdev->internal_cmd_sem);
	g_bdev->event_intercept = TRUE;

	p_inter_cmd->waiting_event = 0xE4;
	p_inter_cmd->pending_cmd_opcode = 0xFD91;
	p_inter_cmd->wmt_opcode = WMT_OPCODE_RF_CAL;
	p_inter_cmd->result = WMT_EVT_INVALID;

	ret = btmtk_main_send_cmd(g_bdev->hdev, cmd, sizeof(cmd),
						BTMTKUART_TX_WAIT_VND_EVT);

	if (ret <= 0) {
		BTMTK_ERR("Unable to send thermal cmd");
		return -1;
	}

	if (p_inter_cmd->result == WMT_EVT_SUCCESS) {
		evtbuf = &evt->u.evt_buf[6];
		thermal =  evtbuf[3] << 24 |
			   evtbuf[2] << 16 |
			   evtbuf[1] << 8 |
			   evtbuf[0];
	}

	g_bdev->event_intercept = FALSE;
	up(&g_bdev->internal_cmd_sem);
	BTMTK_INFO("[InternalCmd] %s done, result = %s, thermal = %d",
		__func__,  _internal_evt_result(p_inter_cmd->result), thermal);
	return thermal;
}

/* sub_drv_ops_cb
 *
 *    All callbacks needs by conninfra driver, 3 types of callback functions
 *    1. Chip reset functions
 *    2. Calibration functions
 *    3. Thermal query functions (todo)
 *
 */
static struct sub_drv_ops_cb bt_drv_cbs =
{
	.rst_cb.pre_whole_chip_rst = bt_pre_chip_rst_handler,
	.rst_cb.post_whole_chip_rst = bt_post_chip_rst_handler,
	.pre_cal_cb.pwr_on_cb = bt_do_pre_power_on,
	.pre_cal_cb.do_cal_cb = bt_do_calibration,
	.thermal_qry = bt_query_thermal,
};

/* bt_receive_data_cb
 *
 *    Callback function register to BTIF while BTIF receiving data from FW
 *
 * Arguments:
 *     [IN] buf    pointer to incoming data buffer
 *     [IN] count  length of incoming data
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
static int32_t bt_receive_data_cb(uint8_t *buf, uint32_t count)
{
	BTMTK_DBG_RAW(buf, count, "%s, len = %d rx data: ", __func__, count);
	add_dump_packet(buf, count, RX);
	g_bdev->psm.sleep_flag = FALSE;
	return btmtk_recv(g_bdev->hdev, buf, count);
}

#if SUPPORT_COREDUMP
/* bt_coredump_cb
 *
 *    coredump API to check bt cr status
 *    Access bgf bus won't hang, so checking conninfra bus
 *
 */
static struct coredump_event_cb bt_coredump_cb =
{
	.reg_readable = conninfra_reg_readable,
	.poll_cpupcr = bt_dump_cpupcr,
};
#endif


/*******************************************************************************
*                        B T I F  F U N C T I O N S
********************************************************************************
*/
#if SUPPORT_BT_THREAD
static void btmtk_btif_enter_deep_idle(struct work_struct *pwork)
{
	int32_t ret = 0;
	struct btif_deepidle_ctrl *idle_ctrl = &g_bdev->btif_dpidle_ctrl;

	down(&idle_ctrl->sem);
	BTMTK_DBG("%s idle = [%d]", __func__, idle_ctrl->is_dpidle);
	if (idle_ctrl->is_dpidle) {
		ret = mtk_wcn_btif_dpidle_ctrl(g_btif_id, BTIF_DPIDLE_ENABLE);
		bt_release_wake_lock(&g_bdev->psm.wake_lock);
		idle_ctrl->is_dpidle = (ret) ? FALSE : TRUE;

		if (ret)
			BTMTK_ERR("BTIF enter dpidle failed(%d)", ret);
		else
			BTMTK_DBG("BTIF enter dpidle succeed");
	} else
		BTMTK_DBG("Deep idle is set to false, skip this time");
	up(&idle_ctrl->sem);
}
#endif

/* btmtk_btif_open

 *
 *     Called when line discipline changed to HCI_UART.
 *
 * Arguments:
 *     tty    pointer to tty info structure
 * Return Value:
 *     0 if success, otherwise error code
 */
int32_t btmtk_btif_open(void)
{
	int32_t ret = 0;
	struct btif_deepidle_ctrl *idle_ctrl = &g_bdev->btif_dpidle_ctrl;

	/* 1. Open BTIF */
	ret = mtk_wcn_btif_open(BTIF_OWNER_NAME, &g_btif_id);
	if (ret) {
		BTMTK_ERR("BT open BTIF failed(%d)", ret);
		return -1;
	}

#if SUPPORT_BT_THREAD
	idle_ctrl->is_dpidle = FALSE;
	idle_ctrl->task = create_singlethread_workqueue("dpidle_task");
	if (!idle_ctrl->task){
		BTMTK_ERR("fail to idle_ctrl->task ");
		return -1;
	}
	INIT_DELAYED_WORK(&idle_ctrl->work, btmtk_btif_enter_deep_idle);
#endif

	/* 2. Register rx callback */
	ret = mtk_wcn_btif_rx_cb_register(g_btif_id, (MTK_WCN_BTIF_RX_CB)bt_receive_data_cb);
	if (ret) {
		BTMTK_ERR("Register rx cb to BTIF failed(%d)", ret);
		mtk_wcn_btif_close(g_btif_id);
		return -1;
	}

	g_btif_rxd_thread = mtk_btif_exp_rx_thread_get(g_btif_id);

#if SUPPORT_BT_THREAD
	/* 3. Enter deeple idle */
	ret = mtk_wcn_btif_dpidle_ctrl(g_btif_id, TRUE);
	if (ret) {
		BTMTK_ERR("BTIF enter dpidle failed(%d)", ret);
		mtk_wcn_btif_close(g_btif_id);
		return -1;
	}
#endif

	BTMTK_DBG("BT open BTIF OK");
	return 0;
}

/* btmtk_btif_close()
 *
 *    Close btif
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 */
int32_t btmtk_btif_close()
{
	int32_t ret = 0;

	if (!g_btif_id) {
		BTMTK_ERR("NULL BTIF ID reference!");
		return 0;
	}

#if SUPPORT_BT_THREAD
	if(&g_bdev->btif_dpidle_ctrl.task != NULL) {
		cancel_delayed_work(&g_bdev->btif_dpidle_ctrl.work);
		flush_workqueue(g_bdev->btif_dpidle_ctrl.task);
		down(&g_bdev->btif_dpidle_ctrl.sem);
		destroy_workqueue(g_bdev->btif_dpidle_ctrl.task);
		up(&g_bdev->btif_dpidle_ctrl.sem);
		g_bdev->btif_dpidle_ctrl.task = NULL;
	}
#endif

	ret = mtk_wcn_btif_close(g_btif_id);
	g_btif_id = 0;
	bt_release_wake_lock(&g_bdev->psm.wake_lock);

	if (ret) {
		BTMTK_ERR("BT close BTIF failed(%d)", ret);
		return -1;
	}
	BTMTK_DBG("BT close BTIF OK");
	return 0;

}

#if SUPPORT_BT_THREAD
/* btmtk_btif_dpidle_ctrl
 *
 *
 *     Ask btif to wakeup / sleep
 *
 * Arguments:
 *     enable    0 to wakeup BTIF, sleep otherwise
 *
 * Return Value:
 *     0 if success, otherwise error code
 */
static int32_t btmtk_btif_dpidle_ctrl(u_int8_t enable)
{
	int32_t ret = 0;
	struct btif_deepidle_ctrl *idle_ctrl = &g_bdev->btif_dpidle_ctrl;
	down(&idle_ctrl->sem);

	if (!g_btif_id) {
		BTMTK_ERR("NULL BTIF ID reference!");
		return -1;
	}

	if(idle_ctrl->task != NULL) {
		BTMTK_DBG("%s enable = %d", __func__, enable);
		/* 1. Remove active timer, and remove a unschedule timer does no harm */
		cancel_delayed_work(&idle_ctrl->work);

		/* 2. Check enable or disable */
		if (!enable) {
			/* disable deep idle, call BTIF api directly */
			bt_hold_wake_lock(&g_bdev->psm.wake_lock);
			ret = mtk_wcn_btif_dpidle_ctrl(g_btif_id, BTIF_DPIDLE_DISABLE);
			idle_ctrl->is_dpidle = (ret) ? TRUE : FALSE;

			if (ret)
				BTMTK_ERR("BTIF exit dpidle failed(%d)", ret);
			else
				BTMTK_DBG("BTIF exit dpidle succeed");
		} else {
			BTMTK_DBG("create timer for enable deep idle");
			idle_ctrl->is_dpidle = TRUE;
			/* enable deep idle, schedule a timer */
			queue_delayed_work(idle_ctrl->task, &idle_ctrl->work,
						(BTIF_IDLE_WAIT_TIME * HZ) >> 10);
		}
	} else {
		BTMTK_INFO("idle_ctrl->task already cancelled!");
	}

	up(&idle_ctrl->sem);
	return ret;
}
#endif

/* btmtk_btif_probe
 *
 *     Probe function of BT driver with BTIF HAL, initialize variable after
 *     driver installed
 *
 * Arguments:
 *     [IN] pdev    platform device pointer after driver installed
 *
 * Return Value:
 *     0 if success, otherwise error code
 */
static int btmtk_btif_probe(struct platform_device *pdev)
{
	/* 1. allocate global context data */
	if (g_bdev == NULL) {
		g_bdev = kzalloc(sizeof(struct btmtk_dev), GFP_KERNEL);
		if (!g_bdev) {
			BTMTK_ERR("%s: alloc memory fail (g_data)", __func__);
			return -1;
		}
	}

	g_bdev->pdev = pdev;

	if (bt_reg_init()) {
		BTMTK_ERR("%s: Error allocating memory remap");
		return -1;
	}

	/* 2. Init HCI device */
	btmtk_allocate_hci_device(g_bdev, HCI_UART);

	/* 3. Init power manager */
	bt_psm_init(&g_bdev->psm);

	/* 4. Init reset completion */
	init_completion(&g_bdev->rst_comp);

	/* 5. Init fw log */
	fw_log_bt_init(g_bdev->hdev);

	/* 6. Init semaphore */
	sema_init(&g_bdev->halt_sem, 1);
	sema_init(&g_bdev->internal_cmd_sem, 1);
	sema_init(&g_bdev->btif_dpidle_ctrl.sem, 1);

#if SUPPORT_COREDUMP
	/* 7. Init coredump */
	g_bdev->coredump_handle = connsys_coredump_init(CONN_DEBUG_TYPE_BT, &bt_coredump_cb);
#endif

	/* 8. Register screen on/off notify callback */
	g_bdev->blank_state = FB_BLANK_UNBLANK;
	btmtk_fb_notify_register();

	/* Finally register callbacks to conninfra driver */
	conninfra_sub_drv_ops_register(CONNDRV_TYPE_BT, &bt_drv_cbs);

	BTMTK_INFO("%s: Done", __func__);
	return 0;
}

/* btmtk_btif_remove
 *
 *     Remove function of BT driver with BTIF HAL, de-initialize variable after
 *     driver being removed
 *
 * Arguments:
 *     [IN] pdev    platform device pointer
 *
 * Return Value:
 *     0 if success, otherwise error code
 */
static int btmtk_btif_remove(struct platform_device *pdev)
{
	conninfra_sub_drv_ops_unregister(CONNDRV_TYPE_BT);

	btmtk_fb_notify_unregister();

	fw_log_bt_exit();

	bt_psm_deinit(&g_bdev->psm);
#if (USE_DEVICE_NODE == 0)
	btmtk_free_hci_device(g_bdev, HCI_UART);
#endif
#if SUPPORT_COREDUMP
	if (!g_bdev->coredump_handle)
		connsys_coredump_deinit(g_bdev->coredump_handle);
#endif

	bt_reg_deinit();
	kfree(g_bdev);
	return 0;
}

/* btmtk_cif_register
 *
 *    BT driver with BTIF hal has its own device, this API should be invoked
 *    by BT main driver when installed
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 */
int btmtk_cif_register(void)
{
	int ret = -1;

	mtkbt_btif_driver.probe = btmtk_btif_probe;
	mtkbt_btif_driver.remove = btmtk_btif_remove;

#if (USE_DEVICE_NODE == 1)
	ret = btmtk_btif_probe(NULL);
	rx_queue_initialize();
#else
	ret = platform_driver_register(&mtkbt_btif_driver);
	BTMTK_INFO("platform_driver_register ret = %d", ret);

	mtkbt_btif_device = platform_device_alloc(MTKBT_BTIF_NAME, 0);
	if (mtkbt_btif_device == NULL) {
		platform_driver_unregister(&mtkbt_btif_driver);
		BTMTK_ERR("platform_device_alloc device fail");
		return -1;
	}
	ret = platform_device_add(mtkbt_btif_device);
	if (ret) {
		platform_driver_unregister(&mtkbt_btif_driver);
		BTMTK_ERR("platform_device_add fail");

		return -1;
	}
#endif
	BTMTK_INFO("%s: Done", __func__);
	return 0;
}

/* btmtk_cif_register
 *
 *    BT driver with BTIF hal has its own device, this API should be invoked
 *    by BT main driver when removed
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0
 */
int btmtk_cif_deregister(void)
{
	btmtk_btif_close();
#if (USE_DEVICE_NODE == 1)
	rx_queue_destroy();
#else
	platform_driver_unregister(&mtkbt_btif_driver);
	platform_device_unregister(mtkbt_btif_device);
#endif

	return 0;
}

/* btmtk_cif_send_cmd
 *
 *    Public API of BT TX
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *    [IN] cmd      - sending data buffer
 *    [IN] cmd_len  - data length
 *    [IN] retry_limit - retry count
 *    [IN] endpoint    - NOT USE in BTIF driver (ignore)
 *    [IN] tx_state    - NOT USE in BTIF driver (ignore)
 *
 * Return Value:
 *    write length of TX
 */
int32_t btmtk_cif_send_cmd(struct hci_dev *hdev, const uint8_t *cmd,
		const int32_t cmd_len, int32_t retry_limit, int32_t endpoint, uint64_t tx_state)
{
	int32_t ret = -1;
	uint32_t wr_count = 0;
	int32_t tx_len = cmd_len;
	int32_t retry = 0;
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	BTMTK_DBG_RAW(cmd, cmd_len, "%s, len = %d, send cmd: ", __func__, cmd_len);

	if (!g_btif_id) {
		BTMTK_ERR("NULL BTIF ID reference!");
		return -1;
	}

	add_dump_packet(cmd, cmd_len, TX);

#if (DRIVER_CMD_CHECK == 1)
		// To do: need to check if list_append will affect throughput
		if (cmd[0] == 0x01) {
			if ((cmd[1] == 0x5D && cmd[2] == 0xFC) ||
				(cmd[1] == 0x6F && cmd[2] == 0xFC) ||
				bdev->cmd_timeout_check == FALSE ) {
				/* Skip these cmd:
				fw will not return response, or response with other format */
			} else {
				u16 cmd_opcode = (cmd[1] << 8) | cmd[2];
				BTMTK_DBG("%s add opcode %4X in cmd queue", __func__,cmd_opcode);
				cmd_list_append(cmd_opcode);
				update_command_response_workqueue();
			}
		}
#endif

	while (tx_len > 0 && retry < retry_limit) {
		if (retry++ > 0)
			usleep_range(USLEEP_5MS_L, USLEEP_5MS_H);

		ret = mtk_wcn_btif_write(g_btif_id, cmd, tx_len);
		if (ret < 0) {
			BTMTK_ERR("BTIF write failed(%d) on retry(%d)", ret, retry-1);
			return -1;
		}

		tx_len -= ret;
		wr_count += ret;
		cmd += ret;
	}
	return ret;
}

/* _check_wmt_evt_over_hci
 *
 *    Check incoming event (RX data) if it's a WMT(vendor) event
 *    (won't send to stack)
 *
 * Arguments:
 *    [IN] buffer   - event buffer
 *    [IN] len      - event length
 *    [IN] expected_op - OPCODE that driver is waiting for (calle assigned)
 *    [IN] p_evt_params - event parameter
 *
 * Return Value:
 *    return check
 *    WMT_EVT_SUCCESS - get expected event
 *    WMT_EVT_FAIL    - otherwise
 */
static int32_t _check_wmt_evt_over_hci(
		uint8_t *buffer,
		uint16_t len,
		uint8_t  expected_op,
		struct wmt_pkt_param *p_evt_params)
{
	struct wmt_pkt *p_wmt_evt;
	uint8_t opcode, sub_opcode;
	uint8_t status = 0xFF; /* reserved value for check error */
	uint16_t param_len = 0;

	/* Sanity check */
	if (len < (HCI_EVT_HDR_LEN + WMT_EVT_HDR_LEN)) {
		BTMTK_ERR("Incomplete packet len %d for WMT event!", len);
		goto check_error;
	}

	p_wmt_evt = (struct wmt_pkt *)&buffer[WMT_EVT_OFFSET];
	if (p_wmt_evt->hdr.dir != WMT_PKT_DIR_CHIP_TO_HOST) {
		BTMTK_ERR("WMT direction %x error!", p_wmt_evt->hdr.dir);
		goto check_error;
	}

	opcode = p_wmt_evt->hdr.opcode;
	if (opcode != expected_op) {
		BTMTK_ERR("WMT OpCode is unexpected! opcode[0x%02X], expect[0x%02X]", opcode, expected_op);
		goto check_error;
	}

	param_len = (p_wmt_evt->hdr.param_len[1] << 8) | (p_wmt_evt->hdr.param_len[0]);
	/* Sanity check */
	if (len < (HCI_EVT_HDR_LEN + WMT_EVT_HDR_LEN + param_len)) {
		BTMTK_ERR("Incomplete packet len %d for WMT event!", len);
		goto check_error;
	}

	switch (opcode) {
	case WMT_OPCODE_FUNC_CTRL:
		if (param_len != sizeof(p_wmt_evt->params.u.func_ctrl_evt)) {
			BTMTK_ERR("Unexpected event param len %d for WMT OpCode 0x%x!",
				      param_len, opcode);
			break;
		}
		status = p_wmt_evt->params.u.func_ctrl_evt.status;
		break;

	case WMT_OPCODE_RF_CAL:
		sub_opcode = p_wmt_evt->params.u.rf_cal_evt.subop;

		if (sub_opcode != 0x03) {
			BTMTK_ERR("Unexpected subop 0x%x for WMT OpCode 0x%x!",
				      sub_opcode, opcode);
			break;
		}

		if (param_len != sizeof(p_wmt_evt->params.u.rf_cal_evt)) {
			BTMTK_ERR("Unexpected event param len %d for WMT OpCode 0x%x!",
				      param_len, opcode);
			break;
		}
		status = p_wmt_evt->params.u.rf_cal_evt.status;
		break;
	case WMT_OPCODE_0XF0:
		status = 0x00; // todo: need more check?
		break;
	default:
		BTMTK_ERR("Unknown WMT OpCode 0x%x!", opcode);
		break;
	}

	if (status != 0xFF) {
		memcpy(p_evt_params, &p_wmt_evt->params, param_len);
		return (status == 0) ? WMT_EVT_SUCCESS : WMT_EVT_FAIL;
	}

check_error:
	BTMTK_DBG_RAW(buffer, len, "Dump data: ");
	return WMT_EVT_INVALID;
}

/* btmtk_cif_dispatch_event
 *
 *    Handler of vendor event
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *    [IN] skb      - packet that carries vendor event
 *
 * Return Value:
 *    return check  - 0 for checking success, -1 otherwise
 *
 */
int32_t btmtk_cif_dispatch_event(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);
	struct bt_internal_cmd *p_inter_cmd = &bdev->internal_cmd;
	uint8_t event_code = skb->data[0];

	/* WMT event */
	BTMTK_DBG("%s: event code=[0x%02x], length = %d", __func__, event_code, skb->len);
	if (event_code == 0xE4) {
		p_inter_cmd->result = _check_wmt_evt_over_hci(skb->data,
					      skb->len,
					      p_inter_cmd->wmt_opcode,
					      &p_inter_cmd->wmt_event_params);

		//p_inter_cmd->result = (ret == WMT_EVT_SUCCESS) ? 1: 0;
	} else if(event_code == 0x0E && skb->len == 10 &&
		  skb->data[3] == 0x91 && skb->data[4] == 0xFD) {
		/* Sepcial case for thermal event, currently FW thermal event
		 * is not a typical WMT event, we have to do it separately
		 */
		memcpy((void*)&p_inter_cmd->wmt_event_params.u, skb->data, skb->len);
		p_inter_cmd->result = WMT_EVT_SUCCESS;
	} else {
		/* Not WMT event */
		p_inter_cmd->result = WMT_EVT_SKIP;
	}

	//return (p_inter_cmd->result == 1) ? 0 : -1;
	return p_inter_cmd->result;
}

#if SUPPORT_BT_THREAD
/* bt_tx_wait_for_msg
 *
 *    Check needing action of current bt status to wake up bt thread
 *
 * Arguments:
 *    [IN] bdev     - bt driver control strcuture
 *
 * Return Value:
 *    return check  - 1 for waking up bt thread, 0 otherwise
 *
 */
static u_int8_t bt_tx_wait_for_msg(struct btmtk_dev *bdev)
{
	/* Ignore other cases for reset situation, and wait for close */
	if (bdev->bt_state == RESET_START)
		return kthread_should_stop();
	else {
		BTMTK_DBG("skb [%d], rx_ind [%d], bgf2ap_ind [%d], sleep_flag [%d], wakeup_flag [%d], force_on [%d]",
				skb_queue_empty(&bdev->tx_queue), bdev->rx_ind,
				bdev->bgf2ap_ind, bdev->psm.sleep_flag,
				bdev->psm.wakeup_flag,
				bdev->psm.force_on);
		return (!skb_queue_empty(&bdev->tx_queue)
			|| bdev->rx_ind
			|| bdev->bgf2ap_ind
			|| (!bdev->psm.force_on && bdev->psm.sleep_flag) // only check sleep_flag if force_on is FALSE
			|| bdev->psm.wakeup_flag
			|| kthread_should_stop());
	}
}

/* btmtk_tx_thread
 *
 *    Internal bt thread handles driver's TX / wakeup / sleep flow
 *
 * Arguments:
 *    [IN] arg
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_tx_thread(void * arg)
{
	struct btmtk_dev *bdev = (struct btmtk_dev *)arg;
	struct bt_psm_ctrl *psm = &bdev->psm;
	int32_t sleep_ret = 0, wakeup_ret = 0, ret, ii;
	struct sk_buff *skb;
	int skb_len, btif_pending_data;
	char state_tag[20] = "";

	BTMTK_INFO("btmtk_tx_thread start running...");
	do {
		BTMTK_DBG("entering  wait_event_interruptible");
		wait_event_interruptible(bdev->tx_waitq, bt_tx_wait_for_msg(bdev));
		BTMTK_DBG("btmtk_tx_thread wakeup");
		if (kthread_should_stop()) {
			BTMTK_INFO("btmtk_tx_thread should stop now...");
			if (psm->state == PSM_ST_NORMAL_TR)
				bt_release_wake_lock(&psm->wake_lock);
			break;
		}

		/* handling SW IRQ */
		if(bdev->bgf2ap_ind) {
			bt_bgf2ap_irq_handler();
			/* reset bgf2ap_ind flag move into bt_bgf2ap_irq_handler */
			continue;
		}

		switch (psm->state) {
		case PSM_ST_SLEEP:
			if (psm->sleep_flag) {
				psm->sleep_flag = FALSE;
				complete(&psm->comp);
				psm->result = sleep_ret;
				continue;
			}
			/*
			 *  TX queue has pending data to send,
			 *    or F/W pull interrupt to indicate there's data to host,
			 *    or there's a explicit wakeup request.
			 *
			 *  All need to execute the Wakeup procedure.
			 */
			btmtk_btif_dpidle_ctrl(FALSE);

			bt_disable_irq(BGF2AP_BTIF_WAKEUP_IRQ);
			wakeup_ret = btmtk_cif_fw_own_clr();
			if (wakeup_ret) {
				/*
				 * Special case handling:
				 * if FW is asserted, FW OWN clr must fail,
				 * so we can assume that FW is asserted from driver view
				 * and trigger reset directly
				 */
				bt_enable_irq(BGF2AP_BTIF_WAKEUP_IRQ);
				btmtk_btif_dpidle_ctrl(TRUE);
				/* check current bt_state to prevent from conflict
				 * resetting b/w subsys reset & whole chip reset
				 */
				if (bdev->bt_state == FUNC_ON) {
					BTMTK_ERR("(PSM_ST_SLEEP) FATAL: btmtk_cif_fw_own_clr error!! going to reset");
					bt_trigger_reset();
				} else
					BTMTK_WARN("(PSM_ST_SLEEP) bt_state [%d] is not FUNC_ON, skip reset", bdev->bt_state);
				break;
			} else {
				/* BGFSYS is awake and ready for data transmission */
				psm->state = PSM_ST_NORMAL_TR;
			}
			break;

		case PSM_ST_NORMAL_TR:
			if (psm->wakeup_flag) {
				psm->wakeup_flag = FALSE;
				complete(&psm->comp);
				psm->result = wakeup_ret;
			}

			if (bdev->rx_ind) {
				BTMTK_DBG("wakeup by BTIF_WAKEUP_IRQ");
				/* Just reset the flag, F/W will send data to host after FW own clear */
				bdev->rx_ind = FALSE;
			}

			/*
			 *  Dequeue and send TX pending packets to bus
			 */
			 while(!skb_queue_empty(&bdev->tx_queue)) {
				skb = skb_dequeue(&bdev->tx_queue);
				if(skb == NULL)
					continue;

				/*
				 * make a copy of skb->len ot prevent skb being
				 * free after sending and recv event from FW
				 */
				skb_len = skb->len;
				// dump cr if it is fw_assert cmd
				if (skb_len >= 4 && skb->data[0] == 0x01 && skb->data[1] == 0x5B &&
				    skb->data[2] == 0xFD && skb->data[3] == 0x00) {
					kfree_skb(skb);
					skb_queue_purge(&bdev->tx_queue);
					bt_trigger_reset();
					break;
				}
#if (DRIVER_CMD_CHECK == 1)
				// enable driver check command timeout mechanism
				if (skb_len >= 3 && skb->data[0] == 0x01 && skb->data[1] == 0x1B &&
					skb->data[2] == 0xFD ) {
					kfree_skb(skb);
					BTMTK_INFO("enable check command timeout");
					bdev->cmd_timeout_check = TRUE;
					continue;
				}
#endif
				ret = btmtk_cif_send_cmd(bdev->hdev, skb->data, skb->len, 5, 0, 0);
				kfree_skb(skb);
				if ((ret < 0) || (ret != skb_len)) {
					BTMTK_ERR("FATAL: TX packet error!! (%u/%d)", skb_len, ret);
					break;
				}

				if (psm->sleep_flag) {
					BTMTK_DBG("more data to send and wait for event, ignore sleep");
					psm->sleep_flag = FALSE;
				}
			}

			/*
			 *  If Quick PS mode is enabled,
			 *    or there's a explicit sleep request.
			 *
			 *  We need to excecute the Sleep procedure.
			 *
			 *  For performance consideration, donot try to enter sleep during BT func on.
			 *
			 *  [20191108]
			 *  - do not sleep if there's pending cmd waiting for event
			 *  [20191119]
			 *  - add check if btif has pending data
			 */
			if (bdev->bt_state == FUNC_ON && cmd_list_isempty() &&
			   psm->sleep_flag && !psm->force_on) {
				// wait if btif tx is not finish yet
				for (ii = 0; ii < 5; ii++) {
					if (mtk_btif_is_tx_complete(g_btif_id) > 0)
						break;
					else
						usleep_range(USLEEP_1MS_L, USLEEP_1MS_H);
					if (ii == 4)
						BTMTK_INFO("%s mtk_btif_is_tx_complete run 5 times", state_tag);
				}

				sleep_ret = btmtk_cif_fw_own_set();
				if (sleep_ret) {
					if (bdev->bt_state == FUNC_ON) {
						BTMTK_ERR("(PSM_ST_NORMAL_TR) FATAL: btmtk_cif_fw_own_set error!! going to reset");
						bt_trigger_reset();
					} else
						BTMTK_WARN("(PSM_ST_NORMAL_TR) bt_state [%d] is not FUNC_ON, skip reset", bdev->bt_state);
					break;
				} else {
					bt_enable_irq(BGF2AP_BTIF_WAKEUP_IRQ);
					btmtk_btif_dpidle_ctrl(TRUE);
					psm->state = PSM_ST_SLEEP;
				}
			}
			break;

		default:
			BTMTK_ERR("FATAL: Unknown state %d!!", psm->state);
			break;
		}
	} while(1);

	return 0;
}
#endif
