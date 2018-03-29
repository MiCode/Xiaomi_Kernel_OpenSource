/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
*This program is free software; you can redistribute it and/or
*modify it under the terms of the GNU General Public License as
*published by the Free Software Foundation version 2.
*This program is distributed AS-IS WITHOUT ANY WARRANTY of any
*kind, whether express or implied; INCLUDING without the implied warranty
*of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
*See the GNU General Public License for more details at
*http://www.gnu.org/licenses/gpl-2.0.html.
*/

#include <Wrap.h>
#include "si_time.h"
#include "si_usbpd_regs.h"
#include "si_usbpd_main.h"
/*#define NOT_DEFINED 0*/

#include <typec.h>

#define DRP 0
#define DFP 1
#define UFP 2

#define RESET_STATE_SET		1
#define RESET_STATE_RELEASE	0

#define DFP_CONFIG 1

#define RESET_PULSE_WIDTH 100

#ifdef NOT_DEFINED
static uint8_t sii_usbpd_get_src_cap(struct sii_usbp_policy_engine *pdev,
				     uint32_t *src_pdo, enum pdo_type type_sup);
#endif

/**************************************************************************/
int sii_usbpd_req_exit_mode(struct sii70xx_drv_context *drv_context, uint8_t portnum)
{
	int status = -EINVAL;
	struct sii_usbp_policy_engine *pUsbpd = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;

	if (drv_context->drp_config == USBPD_DFP) {
		status = sii_usbpd_src_exit_mode_req(pUsbpd, portnum);
	} else {
		pr_info("Not Supported in UFP\n");
		return -EINVAL;
	}
	return status;
}


int sii_usbpd_req_alt_mode(struct sii70xx_drv_context *drv_context,
			   uint8_t portnum, uint8_t svid_mode)
{
	int status = -EINVAL;
	struct sii_usbp_policy_engine *pUsbpd = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;
	if (drv_context->drp_config == USBPD_DFP) {
		pUsbpd->svid_mode = svid_mode;
		status = sii_usbpd_src_alt_mode_req(pUsbpd, portnum);
	} else {
		pr_info("Alternate mode cann't be issued from UFP\n");
		return -EINVAL;
	}
	return status;
}

int sii_usbpd_req_power_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum)
{
	int status = -EINVAL;
	struct sii_usbp_policy_engine *pUsbpd = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;

	if (drv_context->drp_config == USBPD_DFP)
		status = sii_usbpd_src_pwr_swap(pUsbpd, portnum);
	else
		status = sii_usbpd_snk_pwr_swap(pUsbpd, portnum);

	return status;
}

int sii_usbpd_req_data_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum)
{
	int status = -EINVAL;
	struct sii_usbp_policy_engine *pUsbpd = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;

	if (drv_context->drp_config == USBPD_DFP)
		status = sii_usbpd_src_data_swap(pUsbpd, portnum);
	else if (drv_context->drp_config == USBPD_UFP)
		status = sii_usbpd_snk_data_swap(pUsbpd, portnum);

	return status;
}

int sii_usbpd_req_custom_data(struct sii70xx_drv_context *drv_context)
{
	struct sii_usbp_policy_engine *pUsbpd = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;
	pUsbpd->custom_msg = true;
	return true;
}

int sii_usbpd_req_src_cap(struct sii70xx_drv_context *drv_context, uint8_t portnum)
{
	int status = -EINVAL;
	struct sii_usbp_policy_engine *pUsbpd = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;

	if (drv_context->drp_config == USBPD_UFP)
		status = sii_usbpd_give_src_cap(pUsbpd, portnum);
	else if (drv_context->drp_config == USBPD_DFP) {
		pr_info("Not Supported in DFP\n");
		return -EINVAL;
	}

	return status;
}

bool sii_usbpd_req_vconn_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum)
{
	/*int status = -EINVAL;
	   struct sii_usbp_policy_engine *pUsbpd =
	   (struct sii_usbp_policy_engine *)
	   drv_context->pusbpd_policy; */

	/*if (drv_context->drp_config == USBPD_DFP)
	   status = sii_usbpd_req_src_vconn_swap(pUsbpd, portnum);
	   else if (drv_context->drp_config == USBPD_UFP) {
	   pr_info(
	   "\n**** Not Supported in UFP***\n");
	   return -EINVAL;
	   } */
	pr_info("Not Supported\n");
	return -EINVAL;
}

bool sii_drv_set_custom_msg(struct sii70xx_drv_context *drv_context,
			    uint8_t bus_id, uint8_t data, bool enable)
{
	struct sii_usbp_policy_engine *pUsbpd = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;
	bool result;

	if (enable) {
		if (drv_context->drp_config == USBPD_UFP) {
			result = sii_usbpd_req_custom_data(drv_context);

			result = sii_usbpd_req_data_swap(drv_context, bus_id);

			if (result < 0)
				return -EINVAL;
			if (pUsbpd->dr_swap.done)
				pr_info("DR_SWAP DONE\n");
			else {
				pr_info("Failed\n");
				return -EINVAL;
			}

			msleep(7000);
			return true;
		}
		pr_info("Not Supported in DFP\n");
		return -EINVAL;
	} else {
		return -EINVAL;
	}
}


#ifdef NOT_DEFINED
static void assert_rp(struct sii_usbp_policy_engine *udev, bool is_rp)
{
	struct sii_usbp_policy_engine *pdev = container_of(udev,
							   struct sii_usbp_policy_engine, pUsbpd);

	if (!pdev) {
		pr_err("%s:Asserting Ra is failed\n", __func__);
		return;
	}

	if (is_rp) {
		set_bit(FEAT_PR_SWP, &pdev->ptypec_dev->inputs);
		sii_platform_wr_reg8(REG_ADDR__CCCTR12, 0x01);
	}
}

static void pwr_supply_enable(struct sii_usbp_policy_engine *pdev, bool is_enable)
{
	if (is_enable)
		pr_debug("pwr supply set to on\n");
	else
		pr_debug("pwr supply set to off\n");
}
#endif

void set_pd_reset(struct sii70xx_drv_context *drv_context, bool is_set)
{
	if (is_set)
		sii_platform_set_bit8(REG_ADDR__PDCCSRST, BIT_MSK__PDCCSRST__REG_PD24_SRST);
	else
		sii_platform_clr_bit8(REG_ADDR__PDCCSRST, BIT_MSK__PDCCSRST__REG_PD24_SRST);
}

void set_cc_reset(struct sii70xx_drv_context *drv_context, bool is_set)
{
	if (is_set)
		sii_platform_set_bit8(REG_ADDR__PDCCSRST, BIT_MSK__PDCCSRST__REG_CC24_SRST);
	else
		sii_platform_clr_bit8(REG_ADDR__PDCCSRST, BIT_MSK__PDCCSRST__REG_CC24_SRST);
}

void sii70xx_pd_reset_variables(struct sii_usbp_policy_engine *pUsbpd)
{
	pUsbpd->pd_connected = false;
	pUsbpd->hard_reset_in_progress = false;
	pUsbpd->pr_swap.in_progress = false;
	pUsbpd->prot_timeout = false;
	pUsbpd->svid_mode = true;
	pUsbpd->busy_flag = false;
	pUsbpd->crc_timer_inpt = false;
}

void sii70xx_pd_sm_reset(struct sii_usbp_policy_engine *pUsbpd)
{
	pUsbpd->caps_counter = 0;
	pUsbpd->api_dr_swap = false;
	pUsbpd->pr_swap.req_send = false;
	pUsbpd->hard_reset_counter = 0;

	pUsbpd->intf.param.sm_cmd_inputs = 0;
	pUsbpd->intf.param.svdm_sm_inputs = 0;
	pUsbpd->intf.param.uvdm_sm_inputs = 0;
	pUsbpd->intf.param.count = 0;

	if (pUsbpd->drv_context->drp_config == USBPD_UFP)
		sii_reset_ufp(pUsbpd);
	else
		sii_reset_dfp(pUsbpd);

}

static void sii70xx_src_caps(struct sii_usbp_policy_engine *pUsbpd, enum pdo_type supply_type)
{
	if (supply_type == VARIABLE_SUPPLY) {
		/*Yet to Define the caps */
		pr_debug("Variable Supply has to define\n");
	} else if (supply_type == BATTERY_SUPPLY) {
		/*Yet to Define the caps */
		pr_debug("Battery Supply has to define\n");
	} else {
		/*As per power profiles defined in the spec */
		pUsbpd->pd_src_caps[PDO_IDX_5V] = PDO_FIXED(5000, 900, PDO_FIXED_FLAGS);
		pUsbpd->pd_src_cap_cnt = PDO_IDX_5V + 1;
	}
}

static void sii70xx_snk_caps(struct sii_usbp_policy_engine *pUsbpd, enum pdo_type supply_type)
{
	if (supply_type == VARIABLE_SUPPLY) {
		/*Yet to Define the caps */
	} else if (supply_type == BATTERY_SUPPLY) {
		/*Yet to Define the caps */
	} else {
		/*As per power profiles defined in the spec */
		pUsbpd->pd_snk_caps[PDO_IDX_5V] = PDO_FIXED(5000, 900, PDO_FIXED_FLAGS);
		pUsbpd->pd_src_cap_cnt = PDO_IDX_5V + 1;
	}
}

uint8_t sii_usbpd_get_snk_cap(struct sii_usbp_policy_engine *pUsbpd,
			      uint32_t *snk_pdo, enum pdo_type type_sup)
{
	memcpy(snk_pdo, pUsbpd->pd_snk_caps, (pUsbpd->pd_snk_cap_cnt * 4));
	return pUsbpd->pd_snk_cap_cnt;
}

static void pd_extract_pdo_power(struct sii_usbp_policy_engine *pUsbpd,
				 uint32_t pdo, uint32_t *ma, uint32_t *mv)
{
	int max_ma, uw;

	*mv = ((pdo >> 10) & 0x3FF) * 50;

	/*pr_debug("imv => %X\n", *mv); */

	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
		uw = 250000 * (pdo & 0x3FF);
		/*pr_debug("uw => %X\n", uw); */
		max_ma = 1000 * MIN(1000 * uw, PD_MAX_POWER_MW) / *mv;
		/*pr_debug("max_ma => %X\n", max_ma); */
	} else {
		max_ma = 10 * (pdo & 0x3FF);
		/*pr_debug("max_ma => %X\n", max_ma); */
		max_ma = MIN(max_ma, PD_MAX_POWER_MW * 1000 / *mv);
		/*pr_debug("2 --> max_ma => %X\n", max_ma); */
	}

	*ma = MIN(max_ma, PD_MAX_CURRENT_MA);
}

static int pd_find_pdo_index(struct sii_usbp_policy_engine *pUsbpd,
			     int cnt, uint32_t *src_caps, int max_mv)
{
	int i, uw, max_uw = 0, mv, ma;
	int ret = -1;
	int cur_mv = 0;

	if (max_mv == -1)
		max_mv = PD_MAX_VOLTAGE_MV;

	max_mv = MIN(max_mv, PD_MAX_VOLTAGE_MV);

	for (i = 0; i < cnt; i++) {
		mv = ((src_caps[i] >> 10) & 0x3FF) * 50;

		pr_debug("index: mv is %d\t", mv);

		if ((src_caps[i] & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
			uw = 250000 * (src_caps[i] & 0x3FF);
			pr_debug("index: 1uw => %d\n", uw);
		} else {
			ma = (src_caps[i] & 0x3FF) * 10;
			pr_debug("index : ma => %d\n", ma);
			uw = ma * mv;
			pr_debug("index: 2uw => %d\n", uw);
		}

		if (mv > max_mv) {
			pr_debug("index : mv =>%d\n", mv);
			continue;
		}

		uw = MIN(uw, PD_MAX_POWER_MW * 1000);

		pr_debug("index : uw =>%d\n", uw);

		if ((uw > max_uw) || ((uw == max_uw) && mv < cur_mv)) {
			ret = i;
			max_uw = uw;
			cur_mv = mv;
			pr_debug("1 max_uw => %d , %d, %d\n", max_uw, ret, cur_mv);
		}

		if ((uw > max_uw) && (mv <= max_mv)) {
			ret = i;
			max_uw = uw;
			pr_debug("2 max_uw => %d , %d\n", max_uw, ret);
		}
	}

	return ret;
}

int build_request(struct sii_usbp_policy_engine *pUsbpd,
		  int cnt, uint32_t *src_caps, uint32_t *rdo,
		  uint32_t *ma, uint32_t *mv, enum request_type req_type)
{
	int pdo_index = -1, flags = 0;
	int uw;

	if (req_type == REQUEST_VSAFE5V) {
		pdo_index = 0;
	} else {
		pdo_index = pd_find_pdo_index(pUsbpd, cnt, src_caps, 5);
		pr_debug("pdo_index => %d\n", pdo_index);
	}

	if (pdo_index == -1)
		return -EPERM;

	pd_extract_pdo_power(pUsbpd, src_caps[pdo_index], ma, mv);

	uw = *ma * *mv;

	if (uw < (1000 * PD_OPERATING_POWER_MW))
		flags |= RDO_CAP_MISMATCH;

	if ((src_caps[pdo_index] & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
		int mw = uw / 1000;
		*rdo = RDO_BATT(pdo_index + 1, mw, mw, flags);
	} else {
		flags = 0;
		*rdo = RDO_FIXED(pdo_index + 1, *ma, *ma, flags);
	}
	return 0;
}

void send_request_msg(struct sii_usbp_policy_engine *pUsbpd)
{
	uint32_t curr_limit, supply_voltage;
	int res;

	res = build_request(pUsbpd, pUsbpd->pd_src_cap_cnt,
			    pUsbpd->pd_src_caps,
			    &pUsbpd->rdo, &curr_limit, &supply_voltage, REQUEST_VSAFE5V);

	if (res != 0)
		return;

	pr_debug("Req [%u] %u mV %u mA\n",
		 (unsigned int)RDO_POS(pUsbpd->rdo),
		 (unsigned int)supply_voltage, (unsigned int)curr_limit);

	if (pUsbpd->rdo & RDO_CAP_MISMATCH) {
		set_bit(CAN_NOT_BE_MET, &pUsbpd->intf.param.sm_cmd_inputs);
		pr_debug("Mismatch\n");
		return;
	}

	set_bit(VALID_REQ, &pUsbpd->intf.param.sm_cmd_inputs);
	/*pr_debug("\n"); */
}

void sii70xx_dpm_init(struct sii_usbp_policy_engine *pUsbpd)
{
	sii70xx_src_caps(pUsbpd, FIXED_SUPPLY);
	sii70xx_snk_caps(pUsbpd, FIXED_SUPPLY);
}

bool process_sysfs_commands(uint8_t bus_id,
			    enum ctrl_msg ctrl_msg_type, uint32_t *msg_data, uint8_t msg_size)
{
	return true;
}

bool send_hardreset(struct sii_usbp_policy_engine *pdev)
{
	if (!sii_check_tx_busy(pdev->drv_context)) {

		sii_platform_wr_reg8(REG_ADDR__PDTXBC, 0x60);
		sii_platform_set_bit8(REG_ADDR__PDTXCS, BIT_MSK__PDTXCS__RI_PDTXTRANSMIT_WP);
		return true;
	}
	return false;
}

bool set_pwr_params_default(struct sii_usbp_policy_engine *pUsbpd)
{
	bool result = true;

	pr_debug("Setting default values for the voltage and current\n");

	return result;
}

void si_update_pd_status(struct sii_usbp_policy_engine *pUsbpd)
{
	if (pUsbpd->pr_swap.done) {
		if (pUsbpd->evnt_notify_fn)
			pUsbpd->evnt_notify_fn(pUsbpd->drv_context, FEAT_PR_SWP);
	} else if (pUsbpd->dr_swap.done) {
		if (pUsbpd->evnt_notify_fn)
			pUsbpd->evnt_notify_fn(pUsbpd->drv_context, FEAT_DR_SWP);
	} else if (pUsbpd->vconn_swap.done) {
		if (pUsbpd->evnt_notify_fn)
			pUsbpd->evnt_notify_fn(pUsbpd->drv_context, FEAT_VCONN_SWP);
	}
}


void change_drp_data_role(struct sii_usbp_policy_engine *pUsbpd)
{
	if (sii_check_data_role_status(pUsbpd->drv_context))
		sii_update_data_role(pUsbpd->drv_context, false);
	else
		sii_update_data_role(pUsbpd->drv_context, true);
}

void change_drp_pwr_role(struct sii_usbp_policy_engine *pUsbpd)
{
	if (sii_check_power_role_status(pUsbpd->drv_context)) {

		sii_update_power_role(pUsbpd->drv_context, false);
		set_70xx_mode(pUsbpd->drv_context, TYPEC_DRP_UFP);
		sii70xx_vbus_enable(pUsbpd->drv_context, VBUS_SNK);
		trigger_driver(g_exttypec, HOST_TYPE, DISABLE, DONT_CARE);
		update_pwr_role(pUsbpd->drv_context, USBPD_ROLE_SINK);
	} else {
		/*set power role one */
		sii_update_power_role(pUsbpd->drv_context, true);
		set_70xx_mode(pUsbpd->drv_context, TYPEC_DRP_DFP);
		sii70xx_vbus_enable(pUsbpd->drv_context, VBUS_SRC);
		trigger_driver(g_exttypec, HOST_TYPE, ENABLE, DONT_CARE);
		update_pwr_role(pUsbpd->drv_context, USBPD_ROLE_SOURCE);
	}
}

uint8_t sii_usbpd_get_src_cap(struct sii_usbp_policy_engine *pUsbpd,
			      uint32_t *src_pdo, enum pdo_type type_sup)
{
	memcpy(src_pdo, pUsbpd->pd_src_caps, (pUsbpd->pd_src_cap_cnt * sizeof(*src_pdo)));

	return pUsbpd->pd_src_cap_cnt;
}

static void no_response_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *usbpd_dev = (struct sii_usbp_policy_engine *)context;
	struct sii70xx_drv_context *drv_context =
	    (struct sii70xx_drv_context *)usbpd_dev->drv_context;

	pr_debug("NO_RESP_TIMER\n");

	if (!down_interruptible(&usbpd_dev->drv_context->isr_lock)) {
		if (drv_context->drp_config == USBPD_DFP) {

			/*usbpd_dev->state = PE_SRC_Startup;
			   usbpd_dev->pd_connected = true;
			   sii70xx_pd_sm_reset(usbpd_dev); */
			if (usbpd_dev->hard_reset_counter > N_HARDRESETCOUNT)
				usbpd_dev->state = PE_SRC_Disabled;
			else if (usbpd_dev->hard_reset_counter <= N_HARDRESETCOUNT)
				usbpd_dev->state = PE_SRC_Hard_Reset;

			goto success;
		}
		if (drv_context->drp_config == USBPD_UFP) {
			if (usbpd_dev->hard_reset_counter > N_HARDRESETCOUNT)
				usbpd_dev->state = ErrorRecovery;
			else if (usbpd_dev->hard_reset_counter <= N_HARDRESETCOUNT)
				usbpd_dev->state = PE_SNK_Hard_Reset;

			goto success;
		}
		goto exit;
success:	wakeup_pd_queues(drv_context);
exit:		up(&usbpd_dev->drv_context->isr_lock);
	}

}

bool usbpd_timer_create(struct sii_usbp_policy_engine *pUsbpd)
{
	int rc = 0;

	if (!pUsbpd) {
		pr_warn("%s: failed in timer init.\n", __func__);
		return false;
	}
	if (!pUsbpd->usbpd_inst_no_resp_tmr) {
		rc = sii_timer_create(no_response_timer_handler,
				      pUsbpd, &pUsbpd->usbpd_inst_no_resp_tmr,
				      NoResponseTimer, true);

		if (rc != 0) {
			pr_warn("Failed to register NoResponseTimer timer!\n");
			return false;
		}
	}

	return true;
}

#ifdef NOT_DEFINED
static void assert_rd(struct sii_usbp_policy_engine *udev, bool is_rd)
{
	struct sii_usbp_policy_engine *pdev = container_of(udev,
							   struct sii_usbp_policy_engine, pUsbpd);

	if (!pdev) {
		pr_debug("%s:Asserting Rd is failed\n", __func__);
		return;
	}

	if (is_rd) {
		set_bit(FEAT_PR_SWP, &pdev->ptypec_dev->inputs);
		sii_platform_wr_reg8(REG_ADDR__CCCTR12, 0x00);
	}
}
#endif

void set_default_drp_config(struct sii_usbp_policy_engine *pusbpd)
{
	pusbpd->drv_context->pUsbpd_dp_mngr->dr_swap_supp = true;
	pusbpd->drv_context->pUsbpd_dp_mngr->pr_swap_supp = true;

	pusbpd->drv_context->pUsbpd_dp_mngr->identity_request = true;
	pusbpd->drv_context->pUsbpd_dp_mngr->entry_request = true;
	pusbpd->drv_context->pUsbpd_dp_mngr->modes_request = true;

	pusbpd->drv_context->pUsbpd_dp_mngr->svid_request = true;
	pusbpd->drv_context->pUsbpd_dp_mngr->identity_request = true;
	pusbpd->drv_context->pUsbpd_dp_mngr->vconn_swap = true;
}

void usbpd_setup_gpio(enum usbpd_mode state)
{
	if (state == USB)
		pr_debug("Setting GPIO's for USB\n");
	else
		pr_debug("Setting GPIO's for MHL\n");
}

void process_hard_reset(struct sii70xx_drv_context *drv_context)
{
	struct sii_usbp_policy_engine *usbpd_dev =
	    (struct sii_usbp_policy_engine *)drv_context->pusbpd_policy;

	usbpd_dev->hard_reset_in_progress = true;
	sii_platform_set_bit8(REG_ADDR__PDCTR0, BIT_MSK__PDCTR0__RI_PRL_HCRESET_DONE_BY_PE_WP);

	if (!usbpd_dev->pd_connected)
		return;
	/*sii70xx_vbus_enable(drv_context, VBUS_DEFAULT); */

	if (sii_check_tx_busy(drv_context)
	    & BIT_MSK__PDTXCS__RI_PDTXBUSY) {
		pr_info("busy in event thread\n");
		/*send_device_softreset(drv_context); */
	}
	if (drv_context->drp_config == USBPD_DFP) {
		pr_debug("HR DFP\n");
		usbpd_dev->state = PE_SRC_Startup;
		sii70xx_pd_sm_reset(usbpd_dev);
		sii70xx_vbus_enable(usbpd_dev->drv_context, VBUS_SRC);
	} else {
		pr_debug("HR: UFP\n");
		usbpd_dev->state = PE_SNK_Startup;
		sii70xx_pd_sm_reset(usbpd_dev);
		sii70xx_vbus_enable(usbpd_dev->drv_context, VBUS_SNK);
	}
	/*sii_timer_start(&(usbpd_dev->usbpd_inst_no_resp_tmr));*/
	wakeup_pd_queues(drv_context);
}

bool usbpd_delete_timer_func(struct sii_usbp_policy_engine *pdev)
{
	int rc = 0;

	if (!pdev) {
		pr_warn("%s: failed to delete timer.\n", __func__);
		return false;
	}

	if (pdev->usbpd_inst_no_resp_tmr) {
		sii_timer_stop(&pdev->usbpd_inst_no_resp_tmr);
		rc = sii_timer_delete(&pdev->usbpd_inst_no_resp_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_no_resp_tmr!\n");
			return false;
		}
	}
	return true;
}

void sii_update_inf_params(struct sii_usbpd_protocol *pusbpd_protlyr,
			   struct pd_cb_params *sm_inputs)
{
	struct sii70xx_drv_context *drv_context =
	    (struct sii70xx_drv_context *)pusbpd_protlyr->drv_context;
	struct sii_usbp_policy_engine *pUsbpd =
	    (struct sii_usbp_policy_engine *)drv_context->pusbpd_policy;

	pUsbpd->intf.param.sm_cmd_inputs |= sm_inputs->sm_cmd_inputs;
	pUsbpd->intf.param.svdm_sm_inputs |= sm_inputs->svdm_sm_inputs;
	pUsbpd->intf.param.uvdm_sm_inputs |= sm_inputs->uvdm_sm_inputs;
	pUsbpd->intf.param.count = sm_inputs->count;
	pUsbpd->intf.param.data = sm_inputs->data;
	wakeup_pd_queues(drv_context);
}

void wakeup_pd_queues(struct sii70xx_drv_context *drv_context)
{
	if (drv_context->drp_config == USBPD_UFP)
		wakeup_ufp_queue(drv_context);
	else if (drv_context->drp_config == USBPD_DFP)
		wakeup_dfp_queue(drv_context);
}

void *usbpd_init(struct sii70xx_drv_context *drv_context, bool(*event_notify_fn) (void *, uint32_t))
{
	struct sii_usbp_policy_engine *usbpd_dev;

	usbpd_dev = kzalloc(sizeof(struct sii_usbp_policy_engine), GFP_KERNEL);

	if (!usbpd_dev)
		return NULL;

	usbpd_dev->drv_context = drv_context;
	usbpd_dev->evnt_notify_fn = event_notify_fn;
	sii70xx_pd_reset_variables(usbpd_dev);
	set_default_drp_config(usbpd_dev);

	if (!usbpd_timer_create(usbpd_dev)) {
		pr_debug("%s: Failed in timer create.\n", __func__);
		goto exit;
	}

	memset(&usbpd_dev->intf, 0, sizeof(struct sii_usbpd_intf));

	sii70xx_dpm_init(usbpd_dev);

	usbpd_dev->dfp_work_queue = sii_create_single_thread_workqueue(SII_DRIVER_NAME,
								       src_pe_sm_work,
								       &usbpd_dev->pd_dfp_work,
								       &usbpd_dev->dfp_lock);

	usbpd_dev->ufp_work_queue =
	    sii_create_single_thread_workqueue(SII_DRIVER_NAME,
					       sink_pe_sm_work,
					       &usbpd_dev->pd_ufp_work, &usbpd_dev->ufp_lock);

	return usbpd_dev;
exit:
	usbpd_delete_timer_func(usbpd_dev);
	kfree(usbpd_dev);
	return NULL;
}

void usbpd_exit(void *context)
{
	struct sii_usbp_policy_engine *usbpd_dev = (struct sii_usbp_policy_engine *)context;

	cancel_work_sync(&usbpd_dev->pd_ufp_work);
	destroy_workqueue(usbpd_dev->ufp_work_queue);

	cancel_work_sync(&usbpd_dev->pd_dfp_work);
	destroy_workqueue(usbpd_dev->dfp_work_queue);
	usbpd_dfp_exit(usbpd_dev);
	usbpd_ufp_exit(usbpd_dev);
	usbpd_delete_timer_func(usbpd_dev);
	kfree(usbpd_dev);
	pr_info("usbpd_exit\n");
}
