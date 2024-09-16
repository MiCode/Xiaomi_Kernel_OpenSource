/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/interrupt.h>
#include <linux/irqreturn.h>

#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>

#include "btmtk_chip_if.h"
#include "btmtk_mt66xx_reg.h"
#include "conninfra.h"
#include "connsys_debug_utility.h"

/*******************************************************************************
*				C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*			       D A T A	 T Y P E S
********************************************************************************
*/

/*******************************************************************************
*			      P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*			     P R I V A T E   D A T A
********************************************************************************
*/
extern struct btmtk_dev *g_bdev;
static struct bt_irq_ctrl bgf2ap_btif_wakeup_irq = {.name = "BTIF_WAKEUP_IRQ"};
static struct bt_irq_ctrl bgf2ap_sw_irq = {.name = "BGF_SW_IRQ"};
static struct bt_irq_ctrl *bt_irq_table[BGF2AP_IRQ_MAX];
static struct work_struct rst_trigger_work;


/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*			       F U N C T I O N S
********************************************************************************
*/
#if (USE_DEVICE_NODE == 0)
/* bt_report_hw_error()
 *
 *    Insert an event to stack to report error while recovering from chip reset
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     0 if success, otherwise error code
 *
 */
void bt_report_hw_error()
{
	const uint8_t HCI_EVT_HW_ERROR[] = {0x04, 0x10, 0x01, 0x00};
	btmtk_recv(g_bdev->hdev, HCI_EVT_HW_ERROR, sizeof(HCI_EVT_HW_ERROR));
}
#endif

/* bt_reset_work
 *
 *    A work thread that handles BT subsys reset request
 *
 * Arguments:
 *    [IN] work
 *
 * Return Value:
 *    N/A
 *
 */
static void bt_reset_work(struct work_struct *work)
{
	BTMTK_INFO("Trigger subsys reset");
	bt_chip_reset_flow(RESET_LEVEL_0_5, CONNDRV_TYPE_BT, "Subsys reset");
}

/* bt_trigger_reset
 *
 *    Trigger reset (could be subsys or whole chip reset)
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *    N/A
 *
 */
void bt_trigger_reset(void)
{
	int32_t ret = conninfra_is_bus_hang();

	BTMTK_INFO("%s: conninfra_is_bus_hang ret = %d", __func__, ret);

	if (ret > 0)
		conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "bus hang");
	else if (ret == CONNINFRA_ERR_RST_ONGOING)
		BTMTK_INFO("whole chip reset is onging, skip subsys reset");
	else
		schedule_work(&rst_trigger_work);
}

/* bt_bgf2ap_irq_handler
 *
 *    Handling BGF2AP_SW_IRQ, include FW log & chip reset
 *    Please be noticed this handler is running in bt thread
 *    not interrupt thread
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *    N/A
 *
 */
void bt_bgf2ap_irq_handler(void)
{
	int32_t ret, mailbox_status = 0, bgf_status = 0;

	g_bdev->bgf2ap_ind = FALSE;
	/* 1. Check conninfra bus before accessing BGF's CR */
	if (!conninfra_reg_readable()) {
		ret = conninfra_is_bus_hang();
		if (ret > 0) {
			BTMTK_ERR("conninfra bus is hang, needs reset");
			conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "bus hang");
			return;
		}
		BTMTK_ERR("conninfra not readable, but not bus hang ret = %d", ret);
	}

	/* 2. Check bgf bus status */
	if (bt_is_bgf_bus_timeout()) {
		bt_dump_bgfsys_all();
		return;
	}

	/* 3. Read IRQ status CR to identify what happens */
	bgf_status = REG_READL(BGF_SW_IRQ_STATUS);
	if (!(bgf_status & BGF_FW_LOG_NOTIFY)) {
		BTMTK_INFO("bgf_status = 0x%08x", bgf_status);
	}

	if (bgf_status == 0xDEADFEED) {
		bt_dump_bgfsys_all();
		bt_enable_irq(BGF2AP_SW_IRQ);
	} else if (bgf_status & BGF_SUBSYS_CHIP_RESET) {
		SET_BIT(BGF_SW_IRQ_RESET_ADDR, BGF_SUBSYS_CHIP_RESET);
		if (g_bdev->rst_level != RESET_LEVEL_NONE)
			complete(&g_bdev->rst_comp);
		else
			schedule_work(&rst_trigger_work);
	} else if (bgf_status & BGF_FW_LOG_NOTIFY) {
		/* FW notify host to get FW log */
		SET_BIT(BGF_SW_IRQ_RESET_ADDR, BGF_FW_LOG_NOTIFY);
		connsys_log_irq_handler(CONN_DEBUG_TYPE_BT);
		bt_enable_irq(BGF2AP_SW_IRQ);
	} else if (bgf_status &  BGF_WHOLE_CHIP_RESET) {
		conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "FW trigger");
	} else {
		BTMTK_WARN("uknown case");
		bt_enable_irq(BGF2AP_SW_IRQ);
	}
}


/* btmtk_reset_init()
 *
 *    Inint work thread for subsys chip reset
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     N/A
 *
 */
void btmtk_reset_init(void)
{
	INIT_WORK(&rst_trigger_work, bt_reset_work);
}

/* btmtk_irq_handler()
 *
 *    IRQ handler, process following types IRQ
 *    BGF2AP_BTIF_WAKEUP_IRQ   - this IRQ indicates that FW has data to transmit
 *    BGF2AP_SW_IRQ            - this indicates that fw assert / fw log
 *
 * Arguments:
 *    [IN] irq - IRQ number
 *    [IN] arg -
 *
 * Return Value:
 *    returns IRQ_HANDLED for handled IRQ, IRQ_NONE otherwise
 *
 */
static irqreturn_t btmtk_irq_handler(int irq, void * arg)
{
	if (irq == bgf2ap_btif_wakeup_irq.irq_num) {
		if (g_bdev->rst_level == RESET_LEVEL_NONE) {
			bt_disable_irq(BGF2AP_BTIF_WAKEUP_IRQ);
			g_bdev->rx_ind = TRUE;
			g_bdev->psm.sleep_flag = FALSE;
			wake_up_interruptible(&g_bdev->tx_waitq);
		}
		return IRQ_HANDLED;
	} else if (irq == bgf2ap_sw_irq.irq_num) {
		bt_disable_irq(BGF2AP_SW_IRQ);
		g_bdev->bgf2ap_ind = TRUE;
		wake_up_interruptible(&g_bdev->tx_waitq);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

/* bt_request_irq()
 *
 *    Request IRQ
 *
 * Arguments:
 *    [IN] irq_type - IRQ type
 *
 * Return Value:
 *    returns 0 for success, fail otherwise
 *
 */
int32_t bt_request_irq(enum bt_irq_type irq_type)
{
	uint32_t irq_num = 0;
	int32_t ret = 0;
	unsigned long irq_flags = 0;
	struct bt_irq_ctrl *pirq = NULL;
	struct device_node *node = NULL;

	switch (irq_type) {
	case BGF2AP_BTIF_WAKEUP_IRQ:
		node = of_find_compatible_node(NULL, NULL, "mediatek,bt");
		if (node) {
			irq_num = irq_of_parse_and_map(node, 0);
			BTMTK_INFO("irqNum of BGF2AP_BTIF_WAKEUP_IRQ = %d", irq_num);
		}
		else
			BTMTK_ERR("WIFI-OF: get bt device node fail");

		irq_flags = IRQF_TRIGGER_HIGH | IRQF_SHARED;
		pirq = &bgf2ap_btif_wakeup_irq;
		break;
	case BGF2AP_SW_IRQ:
		node = of_find_compatible_node(NULL, NULL, "mediatek,bt");
		if (node) {
			irq_num = irq_of_parse_and_map(node, 1);
			BTMTK_INFO("irqNum of BGF2AP_SW_IRQ = %d", irq_num);
		}
		else
			BTMTK_ERR("WIFI-OF: get bt device node fail");
		irq_flags = IRQF_TRIGGER_HIGH | IRQF_SHARED;
		pirq = &bgf2ap_sw_irq;
		break;
	default:
		BTMTK_ERR("Invalid irq_type %d!", irq_type);
		return -EINVAL;
	}

	BTMTK_INFO("pirq = %p, flag = 0x%08x", pirq, irq_flags);
	ret = request_irq(irq_num, btmtk_irq_handler, irq_flags,
			  pirq->name, pirq);
	if (ret) {
		BTMTK_ERR("Request %s (%u) failed! ret(%d)", pirq->name, irq_num, ret);
		return ret;
	}

	BTMTK_INFO("Request %s (%u) succeed", pirq->name, irq_num);
	bt_irq_table[irq_type] = pirq;
	pirq->irq_num = irq_num;
	pirq->active = TRUE;
	spin_lock_init(&pirq->lock);
	return 0;
}

/* bt_enable_irq()
 *
 *    Enable IRQ
 *
 * Arguments:
 *    [IN] irq_type - IRQ type
 *
 * Return Value:
 *    N/A
 *
 */
void bt_enable_irq(enum bt_irq_type irq_type)
{
	struct bt_irq_ctrl *pirq;

	if (irq_type >= BGF2AP_IRQ_MAX) {
		BTMTK_ERR("Invalid irq_type %d!", irq_type);
		return;
	}

	pirq = bt_irq_table[irq_type];
	if (pirq) {
		spin_lock_irqsave(&pirq->lock, pirq->flags);
		if (!pirq->active) {
			enable_irq(pirq->irq_num);
			pirq->active = TRUE;
		}
		spin_unlock_irqrestore(&pirq->lock, pirq->flags);
	}
}

/* bt_disable_irq()
 *
 *    Disable IRQ
 *
 * Arguments:
 *    [IN] irq_type - IRQ type
 *
 * Return Value:
 *    N/A
 *
 */
void bt_disable_irq(enum bt_irq_type irq_type)
{
	struct bt_irq_ctrl *pirq;

	if (irq_type >= BGF2AP_IRQ_MAX) {
		BTMTK_ERR("Invalid irq_type %d!", irq_type);
		return;
	}

	pirq = bt_irq_table[irq_type];
	if (pirq) {
		spin_lock_irqsave(&pirq->lock, pirq->flags);
		if (pirq->active) {
			disable_irq_nosync(pirq->irq_num);
			pirq->active = FALSE;
		}
		spin_unlock_irqrestore(&pirq->lock, pirq->flags);
	}
}


/* bt_disable_irq()
 *
 *    Release IRQ and de-register IRQ
 *
 * Arguments:
 *    [IN] irq_type - IRQ type
 *
 * Return Value:
 *    N/A
 *
 */
void bt_free_irq(enum bt_irq_type irq_type)
{
	struct bt_irq_ctrl *pirq;

	if (irq_type >= BGF2AP_IRQ_MAX) {
		BTMTK_ERR("Invalid irq_type %d!", irq_type);
		return;
	}

	pirq = bt_irq_table[irq_type];
	if (pirq) {
		free_irq(pirq->irq_num, pirq);
		pirq->active = FALSE;
		bt_irq_table[irq_type] = NULL;
	}
}
