/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#include <Wrap.h>
#include "si_usbpd_main.h"
#include <typec.h>
#include "si_time.h"

void recovery_state_on_disconnect(struct sii_typec *ptypec_dev, bool is_dfp)
{
	if (is_dfp) {
		if (ptypec_dev->state != ATTACHED_DFP)
			ptypec_dev->state = DFP_UNATTACHED;
	} else {
		if (ptypec_dev->state != ATTACHED_UFP)
			ptypec_dev->state = UFP_UNATTACHED;
	}
}

void update_typec_status(struct sii_typec *ptypec_dev, bool is_dfp, bool status)
{
	if (status == DETTACH) {
		if (is_dfp) {

			ptypec_dev->ufp_attached = false;
			set_bit(UFP_DETACHED, &ptypec_dev->inputs);
			/*recovery_state_on_disconnect(ptypec_dev, is_dfp); */
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context, PD_UFP_DETACHED, 0x00, NULL);
#endif
		} else {
			set_bit(DFP_DETACHED, &ptypec_dev->inputs);
			ptypec_dev->dfp_attached = false;
			/*recovery_state_on_disconnect(ptypec_dev, is_dfp); */
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context, PD_DFP_DETACHED, 0x00, NULL);
#endif

		}
	} else if (status == ATTACH) {
		if (is_dfp) {
			/*sii_platform_clr_bit8(REG_ADDR__PDCC24INTM3,
			   BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK24); */
			ptypec_dev->dfp_attached = true;
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context, PD_DFP_ATTACHED, 0x00, NULL);
#endif
		} else {
			ptypec_dev->ufp_attached = true;
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(ptypec_dev->drv_context, PD_UFP_ATTACHED, 0x00, NULL);
#endif
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
		}
	}
}

void enable_power_mode(struct sii_typec *ptypec_dev, enum ufp_volsubstate cur_sel)
{
	if (!ptypec_dev)
		pr_warn("%s: Error\n", __func__);

	pr_info("CC Current Selection: ");

	switch (cur_sel) {
	case UFP_DEFAULT:
		ptypec_dev->pwr_ufp_sub_state = UFP_DEFAULT;
		pr_info("Default\n");
		break;
	case UFP_1P5V:
		sii_platform_wr_reg8(REG_ADDR__ANA_CCPD_MODE_CTRL, 0x1);
		ptypec_dev->pwr_ufp_sub_state = UFP_1P5V;
		pr_info("1.5A\n");
		break;
	case UFP_3V:
		sii_platform_wr_reg8(REG_ADDR__ANA_CCPD_MODE_CTRL, 0x2);
		ptypec_dev->pwr_ufp_sub_state = UFP_3V;
		pr_info("3A\n");
		break;
	default:
		pr_info("Not a valid case\n");
		break;
	}
}

void sii70xx_phy_sm_reset(struct sii_typec *ptypeC)
{
	ptypeC->ufp_attached = false;
	ptypeC->dfp_attached = false;
	ptypeC->dead_battery = false;
	ptypeC->is_flipped = TYPEC_UNDEFINED;
	ptypeC->pwr_ufp_sub_state = 0;
	ptypeC->pwr_dfp_sub_state = 0;
	ptypeC->inputs = 0;
}

void typec_sm_state_init(struct sii_typec *ptypec_dev, enum phy_drp_config drp_role)
{
	if (drp_role == TYPEC_DRP_TOGGLE_RD) {
		pr_info("DRP ROLE: set to TOGGLE_RD\n");
		ptypec_dev->cc_mode = DRP;
	} else if (drp_role == TYPEC_DRP_TOGGLE_RP) {
		pr_info("DRP ROLE: set to TOGGLE_RP\n");
		ptypec_dev->cc_mode = DRP;
	} else if (drp_role == TYPEC_DRP_DFP) {
		pr_info("DFP SET(F)\n");
		ptypec_dev->cc_mode = DFP;
	} else {
		pr_info("UFP SET(F)\n");
		ptypec_dev->cc_mode = UFP;
	}
	ptypec_dev->state = UNATTACHED;
}

/*
static void enable_vconn(enum typec_orientation cbl_orientation)
{
	uint8_t rcd_data1,rcd_data2;

	pr_info(" VCONN FLIP/NON-FLIP : ");

	rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) & 0x3F;
	rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) & 0x3F;

	if (cbl_orientation == TYPEC_CABLE_NOT_FLIPPED)	{
		if (((rcd_data2) >= DEFAULT_VCONN_MIN) &&
		((rcd_data2) <= DEFAULT_VCONN_MAX)) {
			pr_info(" TYPEC_CABLE_NOT_FLIPPED\n");
			goto vconn_enable;
		} else {
			pr_info(" NOT IN RANGE - FLIPPED\n");
		}
	} else {
		if(((rcd_data1) >= DEFAULT_VCONN_MIN) &&
		((rcd_data1) <= DEFAULT_VCONN_MAX)) {
			pr_info(" TYPEC_CABLE_FLIPPED\n");
			goto vconn_enable;
		} else {
			pr_info(" NOT IN RANGE - NOT FLIPPED\n");
		}
	}

vconn_enable:
	sii_platform_set_bit8(REG_ADDR__ANA_CCPD_DRV_CTRL,
			BIT_MSK__ANA_CCPD_DRV_CTRL__RI_VCONN_EN);
}
*/

int trigger_driver(struct usbtypc *typec, int type, int stat, int dir)
{
	pr_info("trigger_driver: type:%d, stat:%d, dir%d\n", type, stat, dir);

	if (type == DEVICE_TYPE && typec->device_driver) {
		if ((stat == DISABLE) && (typec->device_driver->disable)
		    && (typec->device_driver->on == ENABLE)) {
			typec->device_driver->disable(typec->device_driver->priv_data);
			typec->device_driver->on = DISABLE;

			usb_redriver_enter_dps(typec);
			pr_info("trigger_driver:[1]\n");
		} else if ((stat == ENABLE) && (typec->device_driver->enable)
			   && (typec->device_driver->on == DISABLE)) {
			typec->device_driver->enable(typec->device_driver->priv_data);
			typec->device_driver->on = ENABLE;

			usb_redriver_exit_dps(typec);
			pr_info("trigger_driver:[2]\n");
		} else {
			pr_info("%s No device driver to enable\n", __func__);
		}
	} else if (type == HOST_TYPE && typec->host_driver) {
		if ((stat == DISABLE) && (typec->host_driver->disable)
		    && (typec->host_driver->on == ENABLE)) {
			typec->host_driver->disable(typec->host_driver->priv_data);
			typec->host_driver->on = DISABLE;

			usb_redriver_enter_dps(typec);
			pr_info("trigger_driver:[3]\n");
		} else if ((stat == ENABLE) &&
			   (typec->host_driver->enable) && (typec->host_driver->on == DISABLE)) {
			typec->host_driver->enable(typec->host_driver->priv_data);
			typec->host_driver->on = ENABLE;

			usb_redriver_exit_dps(typec);
			pr_info("trigger_driver:[4]\n");
		} else {
			pr_info("%s No device driver to enable\n", __func__);
		}
	} else
		pr_info("trigger_driver:[5]\n");

	return 0;
}

static bool check_substrate_req_dfp(struct sii_typec *ptypec_dev)
{
	uint8_t rcd_data1, rcd_data2;
	enum typec_orientation cb_orietn = TYPEC_UNDEFINED;
	uint8_t vol_thres = 0;
	/*struct sii70xx_drv_context *drv_context =
	   (struct sii70xx_drv_context *)
	   ptypec_dev->drv_context; */

	if (!ptypec_dev) {
		pr_warn("%s: Error\n", __func__);
		return false;
	}

	rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) & 0x3F;
	rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) & 0x3F;
	vol_thres = sii_platform_rd_reg8(REG_ADDR__CCCTR8) & 0x3F;
	pr_info("CC1 voltage = %d and CC2 vol = %d %d\n",
		rcd_data1, rcd_data2, ptypec_dev->pwr_ufp_sub_state);
	if (((rcd_data1) > DEFAULT_MIN) && ((rcd_data1) <= vol_thres))
		cb_orietn = TYPEC_CABLE_NOT_FLIPPED;
	else if (((rcd_data2) > DEFAULT_MIN) && ((rcd_data2) <= vol_thres))
		cb_orietn = TYPEC_CABLE_FLIPPED;
	else
		cb_orietn = TYPEC_UNDEFINED;

	if (cb_orietn == TYPEC_CABLE_NOT_FLIPPED) {
		pr_info("CC Connection FLIP/NON-FLIP : NON-FLIP\n");
		sii_platform_clr_bit8(REG_ADDR__PDCTR12, BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped = TYPEC_CABLE_NOT_FLIPPED;
#ifndef CONFIG_USB_C_SWITCH_SII70XX_MHL_MODE
		trigger_driver(g_exttypec, HOST_TYPE, ENABLE, DONT_CARE);
#endif
	} else if (cb_orietn == TYPEC_CABLE_FLIPPED) {
		pr_info("CC Connection FLIP/NON-FLIP : FLIP\n");
		sii_platform_set_bit8(REG_ADDR__PDCTR12, BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped = TYPEC_CABLE_FLIPPED;
#ifndef CONFIG_USB_C_SWITCH_SII70XX_MHL_MODE
		trigger_driver(g_exttypec, HOST_TYPE, ENABLE, DONT_CARE);
#endif
	} else {
		ptypec_dev->is_flipped = TYPEC_UNDEFINED;
		pr_info("CC Connection FLIP/NON-FLIP : In-Valid\n");
	}
	/*substrate is not checked yet */
	return true;
}


void ufp_cc_adc_work(WORK_STRUCT *w)
{
	struct sii_typec *ptypec_dev = container_of(w,
						    struct sii_typec, adc_work);

	uint8_t rcd_data1, rcd_data2, vol_thres;

	rcd_data1 = 0;
	rcd_data2 = 0;
	vol_thres = 0;
	ptypec_dev->rcd_data1 = 0;
	ptypec_dev->rcd_data2 = 0;
	vol_thres = sii_platform_rd_reg8(REG_ADDR__CCCTR8) & 0x3F;
	pr_info("ufp_cc_adc_work\n");
	do {
		rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) & 0x3F;
		rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) & 0x3F;

		if (((rcd_data1 > DEFAULT_MIN) &&
		     (rcd_data1 <= vol_thres)) || ((rcd_data2 > DEFAULT_MIN)
						   && (rcd_data2 <= vol_thres)) ||
		    (ptypec_dev->state != ATTACHED_UFP)) {
			pr_info("Not ATTACHED_UFP\n");
			goto exit;
		}
		msleep(20);	/*min should me 20ms in linux is valid */
	} while (1);

exit:
	ptypec_dev->rcd_data1 = rcd_data1;
	ptypec_dev->rcd_data2 = rcd_data2;
	complete(&ptypec_dev->adc_read_done_complete);
}

static bool check_substrate_req_ufp(struct sii_typec *ptypec_dev)
{
	uint8_t rcd_data1, rcd_data2;
	uint8_t vol_thres;
	enum typec_orientation cb_orietn = TYPEC_UNDEFINED;

	if (!ptypec_dev) {
		pr_warn("%s: Error\n", __func__);
		return false;
	}

	rcd_data1 = sii_platform_rd_reg8(REG_ADDR__CC1VOL) & 0x3F;
	rcd_data2 = sii_platform_rd_reg8(REG_ADDR__CC2VOL) & 0x3F;
	vol_thres = sii_platform_rd_reg8(REG_ADDR__CCCTR8) & 0x3F;
	pr_info("CC1 voltage = %X and CC2 vol = %X\n", rcd_data1, rcd_data2);

	if (((rcd_data1 > DEFAULT_MIN) && (rcd_data1 <= vol_thres)) ||
	    ((rcd_data2 > DEFAULT_MIN) && (rcd_data2 <= vol_thres))) {
		pr_info("ADC checking\n");
		goto ufp_adc_done;
	} else {
		init_completion(&ptypec_dev->adc_read_done_complete);
#if defined(SII_LINUX_BUILD)
		schedule_work(&ptypec_dev->adc_work);
#else
		ptypec_dev->adc_vol_chng_event = true;
#endif
	}

	wait_for_completion_timeout(&ptypec_dev->adc_read_done_complete, msecs_to_jiffies(900));
	rcd_data1 = ptypec_dev->rcd_data1;
	rcd_data2 = ptypec_dev->rcd_data2;

ufp_adc_done:

	pr_info("Fianal CC1 = %X and CC2 = %X\n", rcd_data1, rcd_data2);
	if ((rcd_data1 > DEFAULT_MIN) && (rcd_data1 <= vol_thres))
		cb_orietn = TYPEC_CABLE_NOT_FLIPPED;
	else if ((rcd_data2 > DEFAULT_MIN) && (rcd_data2 <= vol_thres))
		cb_orietn = TYPEC_CABLE_FLIPPED;
	else
		cb_orietn = TYPEC_UNDEFINED;

	if ((rcd_data1 > DEFAULT_MIN) && (rcd_data1 <= DEFAULT_MAX))
		ptypec_dev->pwr_dfp_sub_state = UFP_DEFAULT;
	else if ((rcd_data2 > DEFAULT_MIN) && (rcd_data2 <= DEFAULT_MAX))
		ptypec_dev->pwr_dfp_sub_state = UFP_DEFAULT;
	else if ((rcd_data1 > MIN_1_5) && (rcd_data1 <= MAX_1_5))
		ptypec_dev->pwr_dfp_sub_state = UFP_1P5V;
	else if ((rcd_data2 > MIN_1_5) && (rcd_data2 <= MAX_1_5))
		ptypec_dev->pwr_dfp_sub_state = UFP_1P5V;
	else if ((rcd_data1 > MIN_3) && (rcd_data1 <= MAX_3))
		ptypec_dev->pwr_dfp_sub_state = UFP_3V;
	else if ((rcd_data2 > MIN_3) && (rcd_data2 <= MAX_3))
		ptypec_dev->pwr_dfp_sub_state = UFP_3V;
	else
		ptypec_dev->pwr_dfp_sub_state = UFP_DEFAULT;

	if (cb_orietn == TYPEC_CABLE_NOT_FLIPPED) {
		pr_info("UFP CC Connection FLIP/NON-FLIP : NON-FLIP\n");
		sii_platform_clr_bit8(REG_ADDR__PDCTR12, BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped = TYPEC_CABLE_NOT_FLIPPED;
		trigger_driver(g_exttypec, DEVICE_TYPE, ENABLE, DONT_CARE);
	} else if (cb_orietn == TYPEC_CABLE_FLIPPED) {
		pr_info("UFP CC Connection FLIP/NON-FLIP : FLIP\n");
		sii_platform_set_bit8(REG_ADDR__PDCTR12, BIT_MSK__PDCTR12__RI_MODE_FLIP);
		ptypec_dev->is_flipped = TYPEC_CABLE_FLIPPED;
		trigger_driver(g_exttypec, DEVICE_TYPE, ENABLE, DONT_CARE);
	} else {
		ptypec_dev->is_flipped = TYPEC_UNDEFINED;
		pr_info("UFP CC Connection FLIP/NON-FLIP : In-Valid\n");
	}

	return true;
}

void sii_typec_events(struct sii_typec *ptypec_dev, uint32_t event_flags)
{
	int work = 1;

	pr_info("Handle sii_typec_events :%x\n", event_flags);
	switch (ptypec_dev->state) {

	case ATTACHED_UFP:
		if (event_flags == FEAT_PR_SWP) {
			ptypec_dev->state = ATTACHED_DFP;
			set_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
			break;
		}

		if (event_flags == FEAT_DR_SWP) {
			set_bit(DR_SWAP_DONE, &ptypec_dev->inputs);
			work = 0;
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
			/*change to DFP */
		}
		break;
	case ATTACHED_DFP:
		if (event_flags == FEAT_PR_SWP) {
			ptypec_dev->is_vbus_detected = true;
			ptypec_dev->state = ATTACHED_UFP;
			set_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
			break;
		}

		if (event_flags == FEAT_DR_SWP) {
			clear_bit(FEAT_DR_SWP, &ptypec_dev->inputs);
			set_bit(DR_SWAP_DONE, &ptypec_dev->inputs);
			work = 0;
			/*ptypec_dev->state = ATTACHED_UFP; */
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
			break;
		}
	default:
		work = 0;
		break;
	}
}

void sii_check_vbus_status(struct sii_typec *ptypec_dev)
{
	uint8_t cc_status = 0;

	cc_status = sii_check_cc_toggle_status(ptypec_dev->drv_context);

	if (ptypec_dev->cc_mode == DRP) {
		if ((cc_status == 0x00) || (cc_status == 0x04)) {
			pr_info("Vbus Glitch\n");
			sii_update_70xx_mode(ptypec_dev->drv_context, TYPEC_DRP_TOGGLE_RD);
		} else if ((cc_status == 0x01) || (cc_status == 0x05)) {
			pr_info("CC Glitch\n");
			sii_update_70xx_mode(ptypec_dev->drv_context, TYPEC_DRP_TOGGLE_RP);
		}
	}
}

void typec_sm0_work(WORK_STRUCT *w)
{
	struct sii_typec *ptypec_dev = container_of(w,
						    struct sii_typec, sm_work);
	struct sii70xx_drv_context *drv_context =
	    (struct sii70xx_drv_context *)ptypec_dev->drv_context;
	/*struct sii_typec *phy = ptypec_dev->phy; */
	int work = 0;
	bool result;

	if (!down_interruptible(&drv_context->isr_lock)) {
		pr_info("Type-c State: %x ", ptypec_dev->state);
		switch (ptypec_dev->state) {
		case DISABLED:
		case ERROR_RECOVERY:
			pr_info("DISABLED\n");

			ptypec_dev->prev_state = ptypec_dev->state;
			ptypec_dev->is_flipped = TYPEC_UNDEFINED;
			ptypec_dev->state = UFP_UNATTACHED;
			msleep(25);
			break;

		case UNATTACHED:
			pr_info("UNATTACHED\n");
			/*chihhao
			   sii_platform_read_70xx_gpio(drv_context); */

			if (ptypec_dev->prev_state == UNATTACHED) {
				pr_info("PREV_STATE:UNATTACHED\n");
				if ((test_bit(DFP_DETACHED, &ptypec_dev->inputs)) || (test_bit(UFP_DETACHED,
					&ptypec_dev->inputs))) {
					if ((test_bit(DFP_ATTACHED, &ptypec_dev->inputs)) || (test_bit(UFP_ATTACHED,
						&ptypec_dev->inputs))) {
						pr_info("No Glitch\n");
					} else {
						clear_bit(DFP_DETACHED, &ptypec_dev->inputs);
						sii_check_vbus_status(ptypec_dev);
					}
				}
			} else if (ptypec_dev->prev_state == ATTACHED_UFP) {
				pr_info("PREV_STATE:ATTACHED_UFP\n");
				set_pd_reset(drv_context, true);
				if (ptypec_dev->cc_mode == DRP) {
					pr_info("toggle RD\n");
					sii_update_70xx_mode(ptypec_dev->drv_context,
							     TYPEC_DRP_TOGGLE_RD);
				} else if (ptypec_dev->cc_mode == DFP) {
					sii_update_70xx_mode(ptypec_dev->drv_context,
							     TYPEC_DRP_DFP);
				} else if (ptypec_dev->cc_mode == UFP) {
					sii_update_70xx_mode(ptypec_dev->drv_context,
							     TYPEC_DRP_UFP);
				}
				ptypec_dev->ufp_attached = false;
				/*to take new interrupt again if
				   unplug/plug happens */
			} else if (ptypec_dev->prev_state == LOCK_UFP) {
				pr_info("PREV_STATE:LOCK_UFP\n");
				set_pd_reset(drv_context, true);
				if (ptypec_dev->cc_mode == DRP) {
					pr_info("toggle RP\n");
					sii_update_70xx_mode(ptypec_dev->drv_context,
							     TYPEC_DRP_TOGGLE_RP);
				} else if (ptypec_dev->cc_mode == DFP) {
					sii_update_70xx_mode(ptypec_dev->drv_context,
							     TYPEC_DRP_DFP);
				}
				/*src= 0, snk = 0 */
				sii70xx_vbus_enable(drv_context, VBUS_SNK);
				ptypec_dev->dfp_attached = false;
			} else if (ptypec_dev->prev_state == ATTACHED_DFP) {
				pr_info("PREV_STATE:ATTACHED_DFP\n");
				set_pd_reset(drv_context, true);
				/*after pr swap this will happen */
				sii70xx_vbus_enable(drv_context, VBUS_SNK);
				if (ptypec_dev->cc_mode == DFP) {
					sii_update_70xx_mode(ptypec_dev->drv_context,
							     TYPEC_DRP_DFP);
				} else if (ptypec_dev->cc_mode == UFP) {
					sii_update_70xx_mode(ptypec_dev->drv_context,
							     TYPEC_DRP_UFP);
				}
				ptypec_dev->dfp_attached = false;
				/*to take new interrupt again if
				   unplug/plug happens */
			} else {
				pr_info("INVALID STATE\n");
			}

			if (test_bit(DFP_ATTACHED, &ptypec_dev->inputs)) {
				clear_bit(DFP_ATTACHED, &ptypec_dev->inputs);
				ptypec_dev->inputs = 0;
				ptypec_dev->is_vbus_detected = true;
				ptypec_dev->state = ATTACHED_UFP;
				work = 1;
				break;
			} else if (test_bit(UFP_ATTACHED, &ptypec_dev->inputs)) {
				clear_bit(UFP_ATTACHED, &ptypec_dev->inputs);
				ptypec_dev->state = DFP_DRP_WAIT;
				work = 1;
				break;
			}
			clear_bit(UFP_DETACHED, &ptypec_dev->inputs);
			clear_bit(DFP_DETACHED, &ptypec_dev->inputs);
			if (test_bit(PR_SWAP_DONE, &ptypec_dev->inputs))
				clear_bit(PR_SWAP_DONE, &ptypec_dev->inputs);

			ptypec_dev->prev_state = ptypec_dev->state;
			ptypec_dev->is_flipped = TYPEC_UNDEFINED;
			break;

		case LOCK_UFP:
			pr_info("LOCK_UFP\n");
			sii70xx_vbus_enable(drv_context, VBUS_DEFAULT);
			/*enable_vconn(cb_orietn); */
			set_70xx_mode(drv_context, TYPEC_DRP_UFP);
			/*as per mtk schematic and daniel suggestions */
			/* do not make CC pins to gnd by rd */
			ptypec_dev->prev_state = ptypec_dev->state;
			msleep(100);
			ptypec_dev->state = UNATTACHED;
			work = true;

			break;

		case ATTACHED_UFP:
			pr_info("ATTACHED UFP\n");
			ptypec_dev->prev_state = ptypec_dev->state;

			if (test_bit(DFP_DETACHED, &ptypec_dev->inputs)) {
				pr_info("DFP Detached\n");
				ptypec_dev->dfp_attached = false;
				clear_bit(DFP_DETACHED, &ptypec_dev->inputs);
				ptypec_dev->state = UNATTACHED;
				ptypec_dev->inputs = 0;
				work = 1;
				break;
			}
			if (test_bit(PR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
				pr_info("PR_SWAP\n");
				break;
			}
			if (test_bit(DR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(DR_SWAP_DONE, &ptypec_dev->inputs);
				pr_info("DR_SWAP_DONE\n");
				break;
			}
			if (test_bit(DFP_ATTACHED, &ptypec_dev->inputs)) {
				if (ptypec_dev->is_flipped != TYPEC_UNDEFINED) {
					clear_bit(DFP_ATTACHED, &ptypec_dev->inputs);
					/* for continuous interrupts in case of drp.
					   if we already processed this then we skip
					   this step */
					pr_info("ERROR Same Interrupt arraived\n");
					break;
				}
			}
			if (check_substrate_req_ufp(ptypec_dev)) {
				/*check CC voltages if ok then enable vbus */
				if (ptypec_dev->is_flipped == TYPEC_UNDEFINED) {
					pr_info("attached ufp: not In range\n");
				} else {
					/*cc voltages are fine */
					result =
					    usbpd_set_ufp_init
					    (ptypec_dev->drv_context->pusbpd_policy);
					if (!result) {
						pr_info("DFP:CONFIG FAILED\n");
						ptypec_dev->state = UNATTACHED;
						return;
					}
				}
			}
			work = false;
			break;

		case ATTACHED_DFP:
			pr_info("ATTACHED_DFP\n");
			ptypec_dev->prev_state = ptypec_dev->state;

			if (test_bit(UFP_DETACHED, &ptypec_dev->inputs)) {
				/*pr_debug("UFP DETACHED\n"); */
				clear_bit(UFP_DETACHED, &ptypec_dev->inputs);
				if (ptypec_dev->cc_mode == DRP) {
					ptypec_dev->state = LOCK_UFP;
					work = 1;
				} else {
					ptypec_dev->state = UNATTACHED;
					work = 1;
				}
				break;
			}

			if (test_bit(PR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(PR_SWAP_DONE, &ptypec_dev->inputs);
				break;
			}
			if (test_bit(DR_SWAP_DONE, &ptypec_dev->inputs)) {
				clear_bit(DR_SWAP_DONE, &ptypec_dev->inputs);
				break;
			}
			if (test_bit(UFP_ATTACHED, &ptypec_dev->inputs)) {
				if (ptypec_dev->is_flipped != TYPEC_UNDEFINED) {
					clear_bit(UFP_ATTACHED, &ptypec_dev->inputs);
					pr_info("ERROR Same Interrupt arraived\n");
					/* for continuous interrupts in case of drp.
					   if we already processed this then we skip
					   this step */
					break;
				}
			}
			if (check_substrate_req_dfp(ptypec_dev)) {
				/*check cc voltages */
				if (ptypec_dev->is_flipped == TYPEC_UNDEFINED) {
					pr_debug("DFP: not In range\n");
					sii70xx_vbus_enable(drv_context,
						VBUS_SRC);
					result = usbpd_set_dfp_init(ptypec_dev->
						drv_context->pusbpd_policy);
					if (!result) {
						pr_debug("Error: DFP Config\n");
						ptypec_dev->state = UNATTACHED;
						work = 1;
						break;
					}
				} else {
					sii70xx_vbus_enable(drv_context, VBUS_SRC);
					result =
					    usbpd_set_dfp_init(ptypec_dev->
							       drv_context->pusbpd_policy);
					if (!result) {
						pr_debug("Error: DFP Config\n");
						ptypec_dev->state = UNATTACHED;
						work = 1;
						break;
					}
				}
			}
			break;

		case DFP_DRP_WAIT:
			sii_mask_detach_interrupts(drv_context);
			ptypec_dev->prev_state = ptypec_dev->state;
			sii70xx_vbus_enable(drv_context,
				VBUS_SRC);
			pr_info("DFP_DRP_WAIT\n");
			msleep(100);
			/*enable_vconn(phy); */
			ptypec_dev->state = ATTACHED_DFP;
			work = true;
			sii_mask_attach_interrupts(ptypec_dev->drv_context);
			break;

		default:
			work = false;
			break;
		}

		up(&ptypec_dev->drv_context->isr_lock);

		if (work) {
			work = false;
			sii_wakeup_queues(ptypec_dev->typec_work_queue,
					  &ptypec_dev->sm_work, &ptypec_dev->typec_event, true);
		}
	}
}

void *phy_init(struct sii70xx_drv_context *drv_context)
{
	struct sii_typec *ptypeC = kzalloc(sizeof(struct sii_typec), GFP_KERNEL);

	if (!ptypeC)
		return NULL;

	ptypeC->drv_context = drv_context;
	ptypeC->prev_state = DISABLED;

	ptypeC->inputs = 0;
	ptypeC->pwr_dfp_sub_state = UFP_DEFAULT;
	ptypeC->pwr_ufp_sub_state = UFP_DEFAULT;

	sii70xx_phy_sm_reset(ptypeC);

	typec_sm_state_init(ptypeC, drv_context->phy_mode);

	ptypeC->typec_work_queue =
	    sii_create_single_thread_workqueue(SII_DRIVER_NAME,
					       typec_detection, &ptypeC->sm_work,
					       &ptypeC->typec_lock);

	if (ptypeC->typec_work_queue == NULL)
		goto exit;

	sii_create_workqueue(adc_voltage_detection,
			     &ptypeC->adc_work, "cc_adc_work", &ptypeC->typec_adc_lock);
	return ptypeC;
exit:
	phy_exit(ptypeC);
	return NULL;
}

void phy_exit(void *context)
{
	struct sii_typec *ptypec_dev = (struct sii_typec *)context;

	cancel_work_sync(&ptypec_dev->adc_work);

	cancel_work_sync(&ptypec_dev->sm_work);
	destroy_workqueue(ptypec_dev->typec_work_queue);

	kfree(ptypec_dev);
}
