// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>

#include "inc/tcpci_typec.h"
#if IS_ENABLED(CONFIG_MTK_CHARGER)
#include <charger_class.h>
#endif /* CONFIG_MTK_CHARGER */

#define RT_PD_MANAGER_VERSION	"1.0.11"

#define GKI

#ifdef GKI
struct typec_port {
	unsigned int			id;
	struct device			dev;
	struct ida			mode_ids;

	int				prefer_role;
	enum typec_data_role		data_role;
	enum typec_role			pwr_role;
	enum typec_role			vconn_role;
	enum typec_pwr_opmode		pwr_opmode;
	enum typec_port_type		port_type;
	struct mutex			port_type_lock;

	enum typec_orientation		orientation;
	struct typec_switch		*sw;
	struct typec_mux		*mux;

	const struct typec_capability	*cap;
	const struct typec_operations   *ops;

	struct list_head		port_list;
	struct mutex			port_list_lock; /* Port list lock */

	void				*pld;

	ANDROID_KABI_RESERVE(1);
};
#endif /* GKI */

struct rpmd_notifier_block {
	struct notifier_block nb;
	struct rt_pd_manager_data *rpmd;
};

struct rt_pd_manager_data {
	struct device *dev;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	struct charger_device *chg_dev;
#endif /* CONFIG_MTK_CHARGER */	
	u32 nr_port;
	struct tcpc_device **tcpc;
	uint8_t *role_def;
	struct typec_capability *typec_caps;
	struct typec_port **typec_port;
	struct typec_partner **partner;
	struct typec_partner_desc *partner_desc;
	struct usb_pd_identity *partner_identity;
#ifndef GKI
	struct typec_mux **mux;
#endif /* GKI */
	bool *en_wd0;
	struct rpmd_notifier_block *pd_nb;
};

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct rpmd_notifier_block *pd_nb =
		container_of(nb, struct rpmd_notifier_block, nb);
	struct rt_pd_manager_data *rpmd = pd_nb->rpmd;
	int ret = 0, idx = pd_nb - rpmd->pd_nb;
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	enum typec_pwr_opmode opmode = TYPEC_PWR_MODE_USB;
#ifndef GKI
	uint16_t pd_revision = 0x0300;
#endif /* GKI */
	uint32_t partner_vdos[VDO_MAX_NR];
	struct typec_mux_state state = {.mode = event, .data = noti};

	dev_info(rpmd->dev, "%s event = %lu, idx = %d\n", __func__, event, idx);

	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		dev_info(rpmd->dev, "%s sink vbus %dmV %dmA type(0x%02X)\n",
				    __func__, noti->vbus_state.mv,
				    noti->vbus_state.ma, noti->vbus_state.type);
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		dev_info(rpmd->dev, "%s source vbus %dmV %dmA type(0x%02X)\n",
				    __func__, noti->vbus_state.mv,
				    noti->vbus_state.ma, noti->vbus_state.type);
		if(noti->vbus_state.mv == 0)
			charger_dev_enable_otg(rpmd->chg_dev, false);
			/*charger_dev_enable_discharge(rpmd->chg_dev, false);*/
		else
			charger_dev_enable_otg(rpmd->chg_dev, true);
			/*charger_dev_enable_discharge(rpmd->chg_dev, true);*/
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

#ifdef GKI
		typec_mux_set(rpmd->typec_port[idx]->mux, &state);
#else
		typec_mux_set(rpmd->mux[idx], &state);
#endif /* GKI */

		if (old_state == TYPEC_UNATTACHED &&
		    (new_state == TYPEC_ATTACHED_SNK ||
		     new_state == TYPEC_ATTACHED_NORP_SRC ||
		     new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		     new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			dev_info(rpmd->dev,
				 "%s Charger plug in, polarity = %d\n",
				 __func__, noti->typec_state.polarity);
			/*
			 * start charger type detection,
			 * and enable device connection
			 */

			typec_set_data_role(rpmd->typec_port[idx],
					    TYPEC_DEVICE);
			typec_set_pwr_role(rpmd->typec_port[idx], TYPEC_SINK);
			typec_set_pwr_opmode(rpmd->typec_port[idx],
					     noti->typec_state.rp_level -
					     TYPEC_CC_VOLT_SNK_DFT);
			typec_set_vconn_role(rpmd->typec_port[idx], TYPEC_SINK);
			/* set typec switch orientation */
			typec_set_orientation(rpmd->typec_port[idx],
					      noti->typec_state.polarity ?
					      TYPEC_ORIENTATION_REVERSE :
					      TYPEC_ORIENTATION_NORMAL);
		} else if ((old_state == TYPEC_ATTACHED_SNK ||
			    old_state == TYPEC_ATTACHED_NORP_SRC ||
			    old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			    old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
			    new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "%s Charger plug out\n", __func__);
			/*
			 * report charger plug-out,
			 * and disable device connection
			 */
		} else if (old_state == TYPEC_UNATTACHED &&
			   (new_state == TYPEC_ATTACHED_SRC ||
			    new_state == TYPEC_ATTACHED_DEBUG)) {
			dev_info(rpmd->dev,
				 "%s OTG plug in, polarity = %d\n",
				 __func__, noti->typec_state.polarity);
			/* enable host connection */

			typec_set_data_role(rpmd->typec_port[idx], TYPEC_HOST);
			typec_set_pwr_role(rpmd->typec_port[idx], TYPEC_SOURCE);
			switch (noti->typec_state.local_rp_level) {
			case TYPEC_RP_3_0:
				opmode = TYPEC_PWR_MODE_3_0A;
				break;
			case TYPEC_RP_1_5:
				opmode = TYPEC_PWR_MODE_1_5A;
				break;
			case TYPEC_RP_DFT:
			default:
				opmode = TYPEC_PWR_MODE_USB;
				break;
			}
			typec_set_pwr_opmode(rpmd->typec_port[idx], opmode);
			typec_set_vconn_role(rpmd->typec_port[idx],
					     TYPEC_SOURCE);
			/* set typec switch orientation */
			typec_set_orientation(rpmd->typec_port[idx],
					      noti->typec_state.polarity ?
					      TYPEC_ORIENTATION_REVERSE :
					      TYPEC_ORIENTATION_NORMAL);
		} else if ((old_state == TYPEC_ATTACHED_SRC ||
			    old_state == TYPEC_ATTACHED_DEBUG) &&
			    new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "%s OTG plug out\n", __func__);
			/* disable host connection */
		} else if (old_state == TYPEC_UNATTACHED &&
			   new_state == TYPEC_ATTACHED_AUDIO) {
			dev_info(rpmd->dev, "%s Audio plug in\n", __func__);
			/* enable AudioAccessory connection */
		} else if (old_state == TYPEC_ATTACHED_AUDIO &&
			   new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "%s Audio plug out\n", __func__);
			/* disable AudioAccessory connection */
		}

		if (new_state == TYPEC_UNATTACHED) {
			typec_unregister_partner(rpmd->partner[idx]);
			rpmd->partner[idx] = NULL;
			if (rpmd->typec_caps[idx].prefer_role == TYPEC_SOURCE) {
				typec_set_data_role(rpmd->typec_port[idx],
						    TYPEC_HOST);
				typec_set_pwr_role(rpmd->typec_port[idx],
						   TYPEC_SOURCE);
				typec_set_pwr_opmode(rpmd->typec_port[idx],
						     TYPEC_PWR_MODE_USB);
				typec_set_vconn_role(rpmd->typec_port[idx],
						     TYPEC_SOURCE);
			} else {
				typec_set_data_role(rpmd->typec_port[idx],
						    TYPEC_DEVICE);
				typec_set_pwr_role(rpmd->typec_port[idx],
						   TYPEC_SINK);
				typec_set_pwr_opmode(rpmd->typec_port[idx],
						     TYPEC_PWR_MODE_USB);
				typec_set_vconn_role(rpmd->typec_port[idx],
						     TYPEC_SINK);
			}
			/* set typec switch orientation */
			typec_set_orientation(rpmd->typec_port[idx],
					      TYPEC_ORIENTATION_NONE);
		} else if (!rpmd->partner[idx]) {
			memset(&rpmd->partner_identity[idx], 0,
			       sizeof(rpmd->partner_identity[idx]));
			rpmd->partner_desc[idx].usb_pd = false;
			switch (new_state) {
			case TYPEC_ATTACHED_AUDIO:
				rpmd->partner_desc[idx].accessory =
					TYPEC_ACCESSORY_AUDIO;
				break;
			case TYPEC_ATTACHED_DEBUG:
			case TYPEC_ATTACHED_DBGACC_SNK:
			case TYPEC_ATTACHED_CUSTOM_SRC:
				rpmd->partner_desc[idx].accessory =
					TYPEC_ACCESSORY_DEBUG;
				break;
			default:
				rpmd->partner_desc[idx].accessory =
					TYPEC_ACCESSORY_NONE;
				break;
			}
			rpmd->partner[idx] = typec_register_partner(
						rpmd->typec_port[idx],
						&rpmd->partner_desc[idx]);
			if (IS_ERR(rpmd->partner[idx])) {
				ret = PTR_ERR(rpmd->partner[idx]);
				dev_notice(rpmd->dev,
				"%s typec register partner fail(%d)\n",
					   __func__, ret);
			}
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		dev_info(rpmd->dev, "%s power role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_SINK) {
			dev_info(rpmd->dev, "%s swap power role to sink\n",
					    __func__);
			/*
			 * report charger plug-in without charger type detection
			 * to not interfering with USB2.0 communication
			 */

			typec_set_pwr_role(rpmd->typec_port[idx], TYPEC_SINK);
		} else if (noti->swap_state.new_role == PD_ROLE_SOURCE) {
			dev_info(rpmd->dev, "%s swap power role to source\n",
					    __func__);
			/* report charger plug-out */

			typec_set_pwr_role(rpmd->typec_port[idx], TYPEC_SOURCE);
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		dev_info(rpmd->dev, "%s data role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_UFP) {
			dev_info(rpmd->dev, "%s swap data role to device\n",
					    __func__);
			/*
			 * disable host connection,
			 * and enable device connection
			 */

			typec_set_data_role(rpmd->typec_port[idx],
					    TYPEC_DEVICE);
		} else if (noti->swap_state.new_role == PD_ROLE_DFP) {
			dev_info(rpmd->dev, "%s swap data role to host\n",
					    __func__);
			/*
			 * disable device connection,
			 * and enable host connection
			 */

			typec_set_data_role(rpmd->typec_port[idx], TYPEC_HOST);
		}
		break;
	case TCP_NOTIFY_VCONN_SWAP:
		dev_info(rpmd->dev, "%s vconn role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role) {
			dev_info(rpmd->dev, "%s swap vconn role to on\n",
					    __func__);
			typec_set_vconn_role(rpmd->typec_port[idx],
					     TYPEC_SOURCE);
		} else {
			dev_info(rpmd->dev, "%s swap vconn role to off\n",
					    __func__);
			typec_set_vconn_role(rpmd->typec_port[idx], TYPEC_SINK);
		}
		break;
	case TCP_NOTIFY_PD_STATE:
		dev_info(rpmd->dev, "%s pd state = %d\n",
				    __func__, noti->pd_state.connected);
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			break;
		case PD_CONNECT_HARD_RESET:
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
		case PD_CONNECT_PE_READY_SNK_APDO:
		case PD_CONNECT_PE_READY_SRC:
		case PD_CONNECT_PE_READY_SRC_PD30:
			typec_set_pwr_opmode(rpmd->typec_port[idx],
					     TYPEC_PWR_MODE_PD);
			if (!rpmd->partner[idx])
				break;
#ifndef GKI
			if (noti->pd_state.connected <= PD_CONNECT_PE_READY_SRC)
				pd_revision = 0x0200;
			typec_partner_set_pd_revision(rpmd->partner[idx],
						      pd_revision);
			typec_partner_set_svdm_version(rpmd->partner[idx],
						       pd_revision == 0x0300 ?
						       SVDM_VER_2_0 :
						       SVDM_VER_1_0);
#endif /* GKI */
			ret = tcpm_inquire_pd_partner_inform(rpmd->tcpc[idx],
							     partner_vdos);
			if (ret != TCPM_SUCCESS)
				break;
			rpmd->partner_identity[idx].id_header = partner_vdos[0];
			rpmd->partner_identity[idx].cert_stat = partner_vdos[1];
			rpmd->partner_identity[idx].product = partner_vdos[2];
			rpmd->partner_identity[idx].vdo[0] = partner_vdos[3];
			rpmd->partner_identity[idx].vdo[1] = partner_vdos[4];
			rpmd->partner_identity[idx].vdo[2] = partner_vdos[5];
			typec_partner_set_identity(rpmd->partner[idx]);
			break;
		};
		break;
	case TCP_NOTIFY_CABLE_TYPE:
		dev_info(rpmd->dev, "%s cable type = %d\n",
				    __func__, noti->cable_type.type);
		break;
	case TCP_NOTIFY_AMA_DP_HPD_STATE:
		dev_info(rpmd->dev, "%s irq = %u, state = %u\n",
				    __func__, noti->ama_dp_hpd_state.irq,
				    noti->ama_dp_hpd_state.state);
#ifdef GKI
		typec_mux_set(rpmd->typec_port[idx]->mux, &state);
#else
		typec_mux_set(rpmd->mux[idx], &state);
#endif /* GKI */
		break;
	case TCP_NOTIFY_AMA_DP_STATE:
		dev_info(rpmd->dev, "%s sel_config = %u, signal = %u\n",
				    __func__, noti->ama_dp_state.sel_config,
				    noti->ama_dp_state.signal);
		dev_info(rpmd->dev, "%s pin_assignment = %u, polarity = %u\n",
				    __func__, noti->ama_dp_state.pin_assignment,
				    noti->ama_dp_state.polarity);
		dev_info(rpmd->dev, "%s active = %u\n",
				    __func__, noti->ama_dp_state.active);
#ifdef GKI
		typec_mux_set(rpmd->typec_port[idx]->mux, &state);
#else
		typec_mux_set(rpmd->mux[idx], &state);
#endif /* GKI */
		break;
	case TCP_NOTIFY_WD0_STATE:
		if (!rpmd->en_wd0[idx])
			break;
		tcpm_typec_change_role_postpone(rpmd->tcpc[idx],
						noti->wd0_state.wd0 ?
						rpmd->role_def[idx] :
						TYPEC_ROLE_SNK, true);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int find_port_index(struct rt_pd_manager_data *rpmd,
			   struct typec_port *port)
{
	int i = 0;

	for (i = 0; i < rpmd->nr_port; i++) {
		if (rpmd->typec_port[i] == port)
			return i;
	}

	return 0;
}

static int tcpc_typec_try_role(struct typec_port *port, int role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
	int idx = find_port_index(rpmd, port);
	uint8_t typec_role = TYPEC_ROLE_UNKNOWN;

	dev_info(rpmd->dev, "%s role = %d, idx = %d\n", __func__, role, idx);

	switch (role) {
	case TYPEC_NO_PREFERRED_ROLE:
		typec_role = TYPEC_ROLE_DRP;
		break;
	case TYPEC_SINK:
		typec_role = TYPEC_ROLE_TRY_SNK;
		break;
	case TYPEC_SOURCE:
		typec_role = TYPEC_ROLE_TRY_SRC;
		break;
	default:
		return 0;
	}

	return tcpm_typec_change_role_postpone(rpmd->tcpc[idx], typec_role,
					       true);
}

static int tcpc_typec_dr_set(struct typec_port *port, enum typec_data_role role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
	int ret = 0, idx = find_port_index(rpmd, port);
	uint8_t data_role = tcpm_inquire_pd_data_role(rpmd->tcpc[idx]);
	bool do_swap = false;

	dev_info(rpmd->dev, "%s role = %d, idx = %d\n", __func__, role, idx);

	if (role == TYPEC_HOST) {
		if (data_role == PD_ROLE_UFP) {
			do_swap = true;
			data_role = PD_ROLE_DFP;
		}
	} else if (role == TYPEC_DEVICE) {
		if (data_role == PD_ROLE_DFP) {
			do_swap = true;
			data_role = PD_ROLE_UFP;
		}
	} else {
		dev_notice(rpmd->dev, "%s invalid role\n", __func__);
		return -EINVAL;
	}

	if (do_swap) {
		ret = tcpm_dpm_pd_data_swap(rpmd->tcpc[idx], data_role, NULL);
		if (ret != TCPM_SUCCESS) {
			dev_notice(rpmd->dev, "%s data role swap fail(%d)\n",
					      __func__, ret);
			return -EPERM;
		}
	}

	return 0;
}

static int tcpc_typec_pr_set(struct typec_port *port, enum typec_role role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
	int ret = 0, idx = find_port_index(rpmd, port);
	uint8_t power_role = tcpm_inquire_pd_power_role(rpmd->tcpc[idx]);
	bool do_swap = false;

	dev_info(rpmd->dev, "%s role = %d, idx = %d\n", __func__, role, idx);

	if (role == TYPEC_SOURCE) {
		if (power_role == PD_ROLE_SINK) {
			do_swap = true;
			power_role = PD_ROLE_SOURCE;
		}
	} else if (role == TYPEC_SINK) {
		if (power_role == PD_ROLE_SOURCE) {
			do_swap = true;
			power_role = PD_ROLE_SINK;
		}
	} else {
		dev_notice(rpmd->dev, "%s invalid role\n", __func__);
		return -EINVAL;
	}

	if (do_swap) {
		ret = tcpm_dpm_pd_power_swap(rpmd->tcpc[idx], power_role, NULL);
		if (ret == TCPM_ERROR_NO_PD_CONNECTED)
			ret = tcpm_typec_role_swap(rpmd->tcpc[idx]);
		if (ret != TCPM_SUCCESS) {
			dev_notice(rpmd->dev, "%s power role swap fail(%d)\n",
					      __func__, ret);
			return -EPERM;
		}
	}

	return 0;
}

static int tcpc_typec_vconn_set(struct typec_port *port, enum typec_role role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
	int ret = 0, idx = find_port_index(rpmd, port);
	uint8_t vconn_role = tcpm_inquire_pd_vconn_role(rpmd->tcpc[idx]);
	bool do_swap = false;

	dev_info(rpmd->dev, "%s role = %d, idx = %d\n", __func__, role, idx);

	if (role == TYPEC_SOURCE) {
		if (vconn_role == PD_ROLE_VCONN_OFF) {
			do_swap = true;
			vconn_role = PD_ROLE_VCONN_ON;
		}
	} else if (role == TYPEC_SINK) {
		if (vconn_role != PD_ROLE_VCONN_OFF) {
			do_swap = true;
			vconn_role = PD_ROLE_VCONN_OFF;
		}
	} else {
		dev_notice(rpmd->dev, "%s invalid role\n", __func__);
		return -EINVAL;
	}

	if (do_swap) {
		ret = tcpm_dpm_pd_vconn_swap(rpmd->tcpc[idx], vconn_role, NULL);
		if (ret != TCPM_SUCCESS) {
			dev_notice(rpmd->dev, "%s vconn role swap fail(%d)\n",
					      __func__, ret);
			return -EPERM;
		}
	}

	return 0;
}

static int tcpc_typec_port_type_set(struct typec_port *port,
				    enum typec_port_type type)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
	int idx = find_port_index(rpmd, port);
	bool as_sink = tcpc_typec_is_act_as_sink_role(rpmd->tcpc[idx]);
	uint8_t typec_role = TYPEC_ROLE_UNKNOWN;

	dev_info(rpmd->dev, "%s type = %d, as_sink = %d, idx = %d\n",
			    __func__, type, as_sink, idx);

	switch (type) {
	case TYPEC_PORT_SNK:
		if (as_sink)
			return 0;
		break;
	case TYPEC_PORT_SRC:
		if (!as_sink)
			return 0;
		break;
	case TYPEC_PORT_DRP:
		if (rpmd->typec_caps[idx].prefer_role == TYPEC_SOURCE)
			typec_role = TYPEC_ROLE_TRY_SRC;
		else if (rpmd->typec_caps[idx].prefer_role == TYPEC_SINK)
			typec_role = TYPEC_ROLE_TRY_SNK;
		else
			typec_role = TYPEC_ROLE_DRP;
		return tcpm_typec_change_role(rpmd->tcpc[idx], typec_role);
	default:
		return 0;
	}

	return tcpm_typec_role_swap(rpmd->tcpc[idx]);
}

static const struct typec_operations tcpc_typec_ops = {
	.try_role = tcpc_typec_try_role,
	.dr_set = tcpc_typec_dr_set,
	.pr_set = tcpc_typec_pr_set,
	.vconn_set = tcpc_typec_vconn_set,
	.port_type_set = tcpc_typec_port_type_set
};

static int typec_port_init(struct rt_pd_manager_data *rpmd, int idx)
{
	int ret = 0;

	rpmd->typec_caps[idx].type = TYPEC_PORT_DRP;
	rpmd->typec_caps[idx].data = TYPEC_PORT_DRD;
	rpmd->typec_caps[idx].revision = 0x0120;
	rpmd->typec_caps[idx].pd_revision = 0x0300;
	rpmd->typec_caps[idx].svdm_version = SVDM_VER_2_0;
	switch (rpmd->role_def[idx]) {
	case TYPEC_ROLE_SRC:
	case TYPEC_ROLE_TRY_SRC:
		rpmd->typec_caps[idx].prefer_role = TYPEC_SOURCE;
		break;
	case TYPEC_ROLE_SNK:
	case TYPEC_ROLE_TRY_SNK:
		rpmd->typec_caps[idx].prefer_role = TYPEC_SINK;
		break;
	default:
		rpmd->typec_caps[idx].prefer_role = TYPEC_NO_PREFERRED_ROLE;
		break;
	}
	rpmd->typec_caps[idx].accessory[0] = TYPEC_ACCESSORY_AUDIO;
	rpmd->typec_caps[idx].accessory[1] = TYPEC_ACCESSORY_DEBUG;
	rpmd->typec_caps[idx].fwnode = rpmd->tcpc[idx]->dev.parent->fwnode;
	rpmd->typec_caps[idx].driver_data = rpmd;
	rpmd->typec_caps[idx].ops = &tcpc_typec_ops;

	rpmd->typec_port[idx] = typec_register_port(rpmd->dev,
						    &rpmd->typec_caps[idx]);
	if (IS_ERR(rpmd->typec_port[idx])) {
		ret = PTR_ERR(rpmd->typec_port[idx]);
		dev_notice(rpmd->dev, "%s typec register port fail(%d)\n",
				      __func__, ret);
		goto out;
	}
	rpmd->partner_desc[idx].identity = &rpmd->partner_identity[idx];
#ifndef GKI
	rpmd->mux[idx] = typec_mux_get(rpmd->tcpc[idx]->dev.parent, NULL);
	if (IS_ERR(rpmd->mux[idx])) {
		ret = PTR_ERR(rpmd->mux[idx]);
		dev_notice(rpmd->dev, "%s typec mux get fail(%d)\n",
				      __func__, ret);
		goto out;
	}
#endif /* GKI */
out:
	return ret;
}

static void rt_pd_manager_remove_helper(struct rt_pd_manager_data *rpmd)
{
	int i = 0, ret = 0;

	for (i = 0; i < rpmd->nr_port; i++) {
		if (!rpmd->tcpc[i])
			break;
		if (IS_ERR(rpmd->typec_port[i]))
			break;
		typec_unregister_port(rpmd->typec_port[i]);
#ifndef GKI
		if (IS_ERR(rpmd->mux[i]))
			break;
		typec_mux_put(rpmd->mux[i]);
#endif /* GKI */
		ret = unregister_tcp_dev_notifier(rpmd->tcpc[i],
						  &rpmd->pd_nb[i].nb,
						  TCP_NOTIFY_TYPE_ALL);
		if (ret < 0)
			break;
	}
}

#define RPMD_DEVM_KCALLOC(member)					\
	(rpmd->member = devm_kcalloc(rpmd->dev, rpmd->nr_port,		\
				     sizeof(*rpmd->member), GFP_KERNEL))\

static int rt_pd_manager_probe(struct platform_device *pdev)
{
	struct rt_pd_manager_data *rpmd = NULL;
	int ret = 0, i = 0;
	char name[16];
	struct device_node *np = pdev->dev.of_node;

	dev_info(&pdev->dev, "%s (%s)\n", __func__, RT_PD_MANAGER_VERSION);

	rpmd = devm_kzalloc(&pdev->dev, sizeof(*rpmd), GFP_KERNEL);
	if (!rpmd)
		return -ENOMEM;

	rpmd->dev = &pdev->dev;

#if IS_ENABLED(CONFIG_MTK_CHARGER)
	rpmd->chg_dev = get_charger_by_name("primary_chg");
	if (!rpmd->chg_dev) {
		dev_notice(rpmd->dev, "%s get chg dev fail\n", __func__);
		ret = -ENODEV;
		goto err_get_chg_dev;
	}
#endif /* CONFIG_MTK_CHARGER */
	ret = of_property_read_u32(np, "nr_port", &rpmd->nr_port);
	if (ret < 0) {
		dev_notice(rpmd->dev, "%s read nr_port property fail(%d)\n",
				      __func__, ret);
		rpmd->nr_port = 1;
	}
	RPMD_DEVM_KCALLOC(tcpc);
	RPMD_DEVM_KCALLOC(role_def);
	RPMD_DEVM_KCALLOC(typec_caps);
	RPMD_DEVM_KCALLOC(typec_port);
	RPMD_DEVM_KCALLOC(partner);
	RPMD_DEVM_KCALLOC(partner_desc);
	RPMD_DEVM_KCALLOC(partner_identity);
#ifndef GKI
	RPMD_DEVM_KCALLOC(mux);
#endif /* GKI */
	RPMD_DEVM_KCALLOC(en_wd0);
	RPMD_DEVM_KCALLOC(pd_nb);
	if (!rpmd->tcpc || !rpmd->role_def || !rpmd->typec_caps ||
	    !rpmd->typec_port || !rpmd->partner || !rpmd->partner_desc ||
	    !rpmd->partner_identity ||
#ifndef GKI
	    !rpmd->mux ||
#endif /* GKI */
	    !rpmd->en_wd0 || !rpmd->pd_nb)
		return -ENOMEM;
	platform_set_drvdata(pdev, rpmd);

	for (i = 0; i < rpmd->nr_port; i++) {
		ret = snprintf(name, sizeof(name), "type_c_port%d", i);
		if (ret >= sizeof(name))
			dev_notice(rpmd->dev,
				   "%s type_c name is truncated\n", __func__);

		rpmd->tcpc[i] = tcpc_dev_get_by_name(name);
		if (!rpmd->tcpc[i]) {
			dev_notice(rpmd->dev, "%s get %s fail\n",
					      __func__, name);
			ret = -ENODEV;
			goto out;
		}

		rpmd->role_def[i] = tcpm_inquire_typec_role_def(rpmd->tcpc[i]);

		ret = typec_port_init(rpmd, i);
		if (ret < 0) {
			dev_notice(rpmd->dev, "%s typec port init fail(%d)\n",
					      __func__, ret);
			goto out;
		}

		ret = snprintf(name, sizeof(name), "en_wd0_port%d", i);
		if (ret >= sizeof(name))
			dev_notice(rpmd->dev,
				   "%s en_wd0 name is truncated\n", __func__);

		rpmd->en_wd0[i] = of_property_read_bool(np, name);

		rpmd->pd_nb[i].nb.notifier_call = pd_tcp_notifier_call;
		rpmd->pd_nb[i].rpmd = rpmd;
		ret = register_tcp_dev_notifier(rpmd->tcpc[i],
						&rpmd->pd_nb[i].nb,
						TCP_NOTIFY_TYPE_ALL);
		if (ret < 0) {
			dev_notice(rpmd->dev,
				   "%s register port%d notifier fail(%d)",
				   __func__, i, ret);
			goto out;
		}
	}
	dev_info(rpmd->dev, "%s OK!!\n", __func__);

	return 0;
out:
	rt_pd_manager_remove_helper(rpmd);
#if IS_ENABLED(CONFIG_MTK_CHARGER)	
err_get_chg_dev:
#endif /* CONFIG_MTK_CHARGER */
	return ret;
}

static int rt_pd_manager_remove(struct platform_device *pdev)
{
	struct rt_pd_manager_data *rpmd = platform_get_drvdata(pdev);

	dev_info(rpmd->dev, "%s ++\n", __func__);
	rt_pd_manager_remove_helper(rpmd);
	return 0;
}

static const struct of_device_id rt_pd_manager_of_match[] = {
	{ .compatible = "mediatek,rt-pd-manager" },
	{ }
};
MODULE_DEVICE_TABLE(of, rt_pd_manager_of_match);

static struct platform_driver rt_pd_manager_driver = {
	.driver = {
		.name = "rt-pd-manager",
		.of_match_table = of_match_ptr(rt_pd_manager_of_match),
	},
	.probe = rt_pd_manager_probe,
	.remove = rt_pd_manager_remove,
};

static int __init rt_pd_manager_init(void)
{
	return platform_driver_register(&rt_pd_manager_driver);
}
late_initcall(rt_pd_manager_init);

static void __exit rt_pd_manager_exit(void)
{
	platform_driver_unregister(&rt_pd_manager_driver);
}
module_exit(rt_pd_manager_exit);

MODULE_AUTHOR("Jeff Chang");
MODULE_DESCRIPTION("Richtek pd manager driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RT_PD_MANAGER_VERSION);

/*
 * Release Note
 * 1.0.11
 * (1) Add GKI #ifdef
 *
 * 1.0.10
 * (1) Add support for multiple ports
 *
 * 1.0.9
 * (1) Add more information for source vbus log
 *
 * 1.0.8
 * (1) Register typec_port
 * (2) Remove unused parts
 * (3) Add rt_pd_manager_remove()
 *
 * 1.0.7
 * (1) enable power path in sink vbus
 *
 * 1.0.6
 * (1) refactor data struct and remove unuse part
 * (2) move bc12 relative to charger ic driver
 */
