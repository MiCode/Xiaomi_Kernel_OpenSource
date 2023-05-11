/*
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mfd/max77729-private.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec/maxim/max77729_usbc.h>
#include <linux/usb/typec/maxim/max77729_alternate.h>

extern void stop_usb_host(void *data);
extern void stop_usb_peripheral(void *data);
extern void start_usb_host(void *data, bool ss);
extern void start_usb_peripheral(void *data);

extern struct max77729_usbc_platform_data *g_usbc_data;

void max77729_dp_detach(void *data)
{
	struct max77729_usbc_platform_data *usbpd_data = data;
	pr_info("%s: dp_is_connect %d\n", __func__, usbpd_data->dp_is_connect);
	usbpd_data->dp_is_connect = 0;
	usbpd_data->dp_hs_connect = 0;
	usbpd_data->is_sent_pin_configuration = 0;
}

static irqreturn_t max77729_vconncop_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_cc_data *cc_data = usbc_data->cc_data;
	max77729_read_reg(usbc_data->muic, REG_CC_STATUS1, &cc_data->cc_status1);
	cc_data->vconnocp = (cc_data->cc_status1 & BIT_VCONNOCPI)
			>> FFS(BIT_VCONNOCPI);
	msg_maxim("New VCONNOCP Status Interrupt (%d)",
		cc_data->vconnocp);
	return IRQ_HANDLED;
}

static irqreturn_t max77729_vsafe0v_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_cc_data *cc_data = usbc_data->cc_data;
	u8 ccpinstat = 0;
	/* pr_debug("%s: IRQ(%d)_IN\n", __func__, irq); */
	max77729_read_reg(usbc_data->muic, REG_BC_STATUS, &cc_data->bc_status);
	max77729_read_reg(usbc_data->muic, REG_CC_STATUS0, &cc_data->cc_status0);
	max77729_read_reg(usbc_data->muic, REG_CC_STATUS1, &cc_data->cc_status1);
	ccpinstat = (cc_data->cc_status0 & BIT_CCPinStat)
				>> FFS(BIT_CCPinStat);
	cc_data->vsafe0v = (cc_data->cc_status1 & BIT_VSAFE0V)
				>> FFS(BIT_VSAFE0V);

	msg_maxim("New VSAFE0V Status Interrupt (%d)",
		cc_data->vsafe0v);
	/* pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq); */

	return IRQ_HANDLED;
}


static irqreturn_t max77729_ccpinstat_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_cc_data *cc_data = usbc_data->cc_data;
	/* union power_supply_propval val; */
	u8 ccpinstat = 0;

	max77729_read_reg(usbc_data->muic, REG_CC_STATUS0, &cc_data->cc_status0);

	/* pr_debug("%s: IRQ(%d)_IN\n", __func__, irq); */
	ccpinstat = (cc_data->cc_status0 & BIT_CCPinStat)
		>> FFS(BIT_CCPinStat);

	switch (ccpinstat) {
	case NO_DETERMINATION:
			msg_maxim("CCPINSTAT (NO_DETERMINATION)");
			break;
	case CC1_ACTIVE:
			msg_maxim("CCPINSTAT (CC1_ACTIVE)");
			break;

	case CC2_ACTVIE:
			msg_maxim("CCPINSTAT (CC2_ACTIVE)");
			break;

	case AUDIO_ACCESSORY:
			msg_maxim("CCPINSTAT (AUDIO_ACCESSORY)");
			break;

	default:
			msg_maxim("CCPINSTAT [%d]", ccpinstat);
			break;

	}
	cc_data->ccpinstat = ccpinstat;
	usbc_data->cc_pin_status  = ccpinstat;

	/* val.intval = ccpinstat; */
	/* psy_do_property("usb", set, POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION, val); */
	/* pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq); */

	return IRQ_HANDLED;
}

static irqreturn_t max77729_ccistat_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_cc_data *cc_data = usbc_data->cc_data;
	union extcon_property_value eval;
	u8 ccistat = 0;

	max77729_read_reg(usbc_data->muic, REG_CC_STATUS0, &cc_data->cc_status0);
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	ccistat = (cc_data->cc_status0 & BIT_CCIStat) >> FFS(BIT_CCIStat);
	switch (ccistat) {
	case NOT_IN_UFP_MODE:
		msg_maxim("Not in UFP");
		break;

	case CCI_500mA:
		msg_maxim("Vbus Current is 500mA!");
		break;

	case CCI_1_5A:
		msg_maxim("Vbus Current is 1.5A!");
		break;

	case CCI_3_0A:
		msg_maxim("Vbus Current is 3.0A!");
		break;

	default:
		msg_maxim("CCINSTAT(Never Call this routine) !");
		break;

	}
	cc_data->ccistat = ccistat;
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);

	eval.intval = cc_data->ccistat > CCI_500mA ? 1 : 0;
	extcon_set_property(usbc_data->extcon, EXTCON_USB, EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT, eval);
	max77729_notify_rp_current_level(usbc_data);

	return IRQ_HANDLED;
}


static irqreturn_t max77729_ccvnstat_irq(int irq, void *data)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_cc_data *cc_data = usbc_data->cc_data;
	u8 ccvcnstat = 0;

	max77729_read_reg(usbc_data->muic, REG_CC_STATUS0, &cc_data->cc_status0);

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	ccvcnstat = (cc_data->cc_status0 & BIT_CCVcnStat) >> FFS(BIT_CCVcnStat);

	switch (ccvcnstat) {
	case 0:
		msg_maxim("Vconn Disabled");
		if (cc_data->current_vcon != OFF) {
			cc_data->previous_vcon = cc_data->current_vcon;
			cc_data->current_vcon = OFF;
		}
		break;

	case 1:
		msg_maxim("Vconn Enabled");
		if (cc_data->current_vcon != ON) {
			cc_data->previous_vcon = cc_data->current_vcon;
			cc_data->current_vcon = ON;
		}
		break;

	default:
		msg_maxim("ccvnstat(Never Call this routine) !");
		break;

	}
	cc_data->ccvcnstat = ccvcnstat;
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);


	return IRQ_HANDLED;
}

static void max77729_ccstat_irq_handler(void *data, int irq)
{
	struct max77729_usbc_platform_data *usbc_data = data;
	struct max77729_cc_data *cc_data = usbc_data->cc_data;
	u8 ccstat = 0;

	/* int prev_power_role = usbc_data->typec_power_role; */
	/* union power_supply_propval val; */

	max77729_read_reg(usbc_data->muic, REG_CC_STATUS0, &cc_data->cc_status0);
	ccstat =  (cc_data->cc_status0 & BIT_CCStat) >> FFS(BIT_CCStat);
	if (irq == CCIC_IRQ_INIT_DETECT) {
		if (ccstat == cc_SINK)
			msg_maxim("initial time : SNK");
		else
			return;
	}
	if (ccstat == cc_No_Connection)
		usbc_data->pd_state = max77729_State_PE_Initial_detach;
	else if (ccstat == cc_SOURCE)
		usbc_data->pd_state = max77729_State_PE_SRC_Send_Capabilities;
	else if (ccstat == cc_SINK)
		usbc_data->pd_state = max77729_State_PE_SNK_Wait_for_Capabilities;


	if (!ccstat) {
		if (usbc_data->plug_attach_done) {
			msg_maxim("PLUG_DETACHED ---");
				usbc_data->typec_power_role = TYPEC_SINK;
				usbc_data->typec_data_role = TYPEC_DEVICE;
				usbc_data->pwr_opmode = TYPEC_PWR_MODE_USB;
			if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_PR ||
				usbc_data->typec_try_state_change == TRY_ROLE_SWAP_DR) {
				/* Role change try and new mode detected */
				msg_maxim("typec_reverse_completion, detached while pd_swap");
				usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
				complete(&usbc_data->typec_reverse_completion);
			}

			/*if (usbc_data->pd_data->current_dr == UFP)*/
				stop_usb_peripheral(usbc_data);
			/*else if (usbc_data->pd_data->current_dr == DFP)*/
				stop_usb_host(usbc_data);

			usbc_data->plug_attach_done = 0;
			usbc_data->is_hvdcp = false;
			usbc_data->cc_data->current_pr = 0xFF;
			usbc_data->pd_data->current_dr = 0xFF;
			usbc_data->cc_data->current_vcon = 0xFF;
			usbc_data->detach_done_wait = 1;
		}
	} else {
		if (!usbc_data->plug_attach_done) {
			msg_maxim("PLUG_ATTACHED +++");
			usbc_data->plug_attach_done = 1;
		}
	}

	switch (ccstat) {
	case cc_No_Connection:
			msg_maxim("ccstat : cc_No_Connection");
			usbc_data->adapter_svid	= 0x0;
			usbc_data->adapter_id	= 0x0;
			usbc_data->xid	= 0x0;
			usbc_data->send_vdm_identity = 0;
			usbc_data->sink_Ready = false;
			usbc_data->source_Ready = false;
			usbc_data->verifed = 0;
			if (usbc_data->typec_try_pps_enable == TRY_PPS_ENABLE){
				usbc_data->typec_try_pps_enable = TRY_PPS_NONE;
				complete(&usbc_data->pps_in_wait);
			}

			usbc_data->acc_type = 0;
			usbc_data->src_cap_flag = 0x0;
			usbc_data->pd_data->cc_status = CC_NO_CONN;
			usbc_data->is_samsung_accessory_enter_mode = 0;
			usbc_data->pn_flag = false;
			usbc_data->pd_support = false;
			usbc_data->srcccap_request_retry = false;

			if (!usbc_data->typec_try_state_change)
				max77729_usbc_clear_queue(usbc_data);

			usbc_data->typec_power_role = TYPEC_SINK;

			max77729_detach_pd(usbc_data);
			usbc_data->pd_pr_swap = cc_No_Connection;

			if (irq != CCIC_IRQ_INIT_DETECT)
				max77729_vbus_turn_on_ctrl(usbc_data, OFF, false);
			cancel_delayed_work(&usbc_data->vbus_hard_reset_work);
			/* maxim_pmic */
			/* val.intval = 0; */
			/* psy_do_property("usb", set, */
					/* POWER_SUPPLY_PROP_PRESENT, val); */

			break;
	case cc_SINK:
			msg_maxim("ccstat : cc_SINK");
			usbc_data->pd_data->cc_status = CC_SNK;
			usbc_data->pn_flag = false;

			usbc_data->typec_power_role = TYPEC_SINK;
			if (cc_data->current_pr != SNK) {
				cc_data->previous_pr = cc_data->current_pr;
				cc_data->current_pr = SNK;
				/* if (prev_power_role == TYPEC_SOURCE) */

			}

 			if (irq != CCIC_IRQ_INIT_DETECT)
				max77729_vbus_turn_on_ctrl(usbc_data, OFF, true);
			/* start_usb_peripheral(usbc_data); */
			max77729_notify_rp_current_level(usbc_data);
			break;
	case cc_SOURCE:
			msg_maxim("ccstat : cc_SOURCE");
			usbc_data->pd_data->cc_status = CC_SRC;
			usbc_data->pn_flag = false;
			usbc_data->srcccap_request_retry = false;

			usbc_data->typec_power_role = TYPEC_SOURCE;
			if (cc_data->current_pr != SRC) {
				cc_data->previous_pr = cc_data->current_pr;
				cc_data->current_pr = SRC;

				/* if (prev_power_role == TYPEC_SINK) */
			}

  			if (irq != CCIC_IRQ_INIT_DETECT)
				max77729_vbus_turn_on_ctrl(usbc_data, ON, false);
			/* start_usb_host(usbc_data, true); */
			break;
	case cc_Audio_Accessory:
			msg_maxim("ccstat : cc_Audio_Accessory");
			usbc_data->acc_type = 1;
			max77729_process_check_accessory(usbc_data);
			break;
	case cc_Debug_Accessory:
			msg_maxim("ccstat : cc_Debug_Accessory");
			break;
	case cc_Error:
			msg_maxim("ccstat : cc_Error");
			break;
	case cc_Disabled:
			msg_maxim("ccstat : cc_Disabled");
			break;
	case cc_RFU:
			msg_maxim("ccstat : cc_RFU");
			break;
	default:
			break;
	}
}

static irqreturn_t max77729_ccstat_irq(int irq, void *data)
{
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77729_ccstat_irq_handler(data, irq);
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

int max77729_cc_init(struct max77729_usbc_platform_data *usbc_data)
{
	struct max77729_cc_data *cc_data = NULL;
	int ret;

	msg_maxim("IN");

	cc_data = usbc_data->cc_data;

	cc_data->irq_vconncop = usbc_data->irq_base + MAX77729_CC_IRQ_VCONNCOP_INT;
	if (cc_data->irq_vconncop) {
		ret = request_threaded_irq(cc_data->irq_vconncop,
			   NULL, max77729_vconncop_irq,
			   0,
			   "cc-vconncop-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	cc_data->irq_vsafe0v = usbc_data->irq_base + MAX77729_CC_IRQ_VSAFE0V_INT;
	if (cc_data->irq_vsafe0v) {
		ret = request_threaded_irq(cc_data->irq_vsafe0v,
			   NULL, max77729_vsafe0v_irq,
			   0,
			   "cc-vsafe0v-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	cc_data->irq_ccpinstat = usbc_data->irq_base + MAX77729_CC_IRQ_CCPINSTAT_INT;
	if (cc_data->irq_ccpinstat) {
		ret = request_threaded_irq(cc_data->irq_ccpinstat,
			   NULL, max77729_ccpinstat_irq,
			   0,
			   "cc-ccpinstat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	cc_data->irq_ccistat = usbc_data->irq_base + MAX77729_CC_IRQ_CCISTAT_INT;
	if (cc_data->irq_ccistat) {
		ret = request_threaded_irq(cc_data->irq_ccistat,
			   NULL, max77729_ccistat_irq,
			   0,
			   "cc-ccistat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	cc_data->irq_ccvcnstat = usbc_data->irq_base + MAX77729_CC_IRQ_CCVCNSTAT_INT;
	if (cc_data->irq_ccvcnstat) {
		ret = request_threaded_irq(cc_data->irq_ccvcnstat,
			   NULL, max77729_ccvnstat_irq,
			   0,
			   "cc-ccvcnstat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	cc_data->irq_ccstat = usbc_data->irq_base + MAX77729_CC_IRQ_CCSTAT_INT;
	if (cc_data->irq_ccstat) {
		ret = request_threaded_irq(cc_data->irq_ccstat,
			   NULL, max77729_ccstat_irq,
			   0,
			   "cc-ccstat-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	/* check CC Pin state for cable attach booting scenario */
	max77729_ccstat_irq_handler(usbc_data, CCIC_IRQ_INIT_DETECT);
	msg_maxim("OUT");

	return 0;

err_irq:
	kfree(cc_data);
	return ret;

}
