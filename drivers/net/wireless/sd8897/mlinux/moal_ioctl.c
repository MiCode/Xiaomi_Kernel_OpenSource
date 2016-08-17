/** @file moal_ioctl.c
  *
  * @brief This file contains ioctl function to MLAN
  *
  * Copyright (C) 2008-2012, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

/********************************************************
Change log:
    10/21/2008: initial version
********************************************************/

#include    "moal_main.h"
#include    "moal_eth_ioctl.h"
#include    "moal_sdio.h"
#ifdef UAP_SUPPORT
#include    "moal_uap.h"
#endif

#if defined(STA_CFG80211) || defined(UAP_CFG80211)
#include "moal_cfg80211.h"
#endif

/********************************************************
			Local Variables
********************************************************/
/* CAC Measure report default time 60 seconds */
#define MEAS_REPORT_TIME (60 * HZ)
#define MRVL_TLV_HEADER_SIZE            4
/* Marvell Channel config TLV ID */
#define MRVL_CHANNELCONFIG_TLV_ID       (0x0100 + 0x2a)	/* 0x012a */

typedef struct _hostcmd_header
{
    /** Command Header : Command */
	t_u16 command;
    /** Command Header : Size */
	t_u16 size;
    /** Command Header : Sequence number */
	t_u16 seq_num;
    /** Command Header : Result */
	t_u16 result;
    /** Command action */
	t_u16 action;
} hostcmd_header, *phostcmd_header;

#ifdef STA_SUPPORT
/** Region code mapping */
typedef struct _region_code_mapping_t
{
    /** Region */
	t_u8 region[COUNTRY_CODE_LEN];
    /** Code */
	t_u8 code;
} region_code_mapping_t;

/** Region code mapping table */
static region_code_mapping_t region_code_mapping[] = {
	{"US ", 0x10},		/* US FCC */
	{"CA ", 0x20},		/* IC Canada */
	{"SG ", 0x10},		/* Singapore */
	{"EU ", 0x30},		/* ETSI */
	{"AU ", 0x30},		/* Australia */
	{"KR ", 0x30},		/* Republic Of Korea */
	{"FR ", 0x32},		/* France */
	{"CN ", 0x50},		/* China */
	{"JP ", 0xFF},		/* Japan special */
};

/** EEPROM Region code mapping table */
static region_code_mapping_t hw_region_code_mapping[] = {
	{"US ", 0x10},		/* US FCC */
	{"CA ", 0x20},		/* IC Canada */
	{"KR ", 0x30},		/* Korea */
	{"CN ", 0x50},		/* China */
	{"ES ", 0x31},		/* Spain */
	{"FR ", 0x32},		/* France */
	{"JP ", 0x40},		/* Japan */
	{"JP ", 0x41},		/* Japan */
};
#endif

/********************************************************
			Global Variables
********************************************************/

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29)
#ifdef UAP_SUPPORT
/** Network device handlers for uAP */
extern const struct net_device_ops woal_uap_netdev_ops;
#endif
#ifdef STA_SUPPORT
/** Network device handlers for STA */
extern const struct net_device_ops woal_netdev_ops;
#endif
#endif
extern int cfg80211_wext;

/********************************************************
			Local Functions
********************************************************/
#ifdef STA_SUPPORT
/**
 *  @brief This function converts region string to region code
 *
 *  @param region_string    Region string
 *
 *  @return                 Region code
 */
static t_u8
region_string_2_region_code(char *region_string)
{
	t_u8 i;
	t_u8 size = sizeof(region_code_mapping) / sizeof(region_code_mapping_t);

	ENTER();
	for (i = 0; i < size; i++) {
		if (!memcmp(region_string,
			    region_code_mapping[i].region,
			    strlen(region_string))) {
			LEAVE();
			return (region_code_mapping[i].code);
		}
	}
	/* Default is US */
	LEAVE();
	return region_code_mapping[0].code;
}

/**
 *  @brief This function converts region string to region code
 *
 *  @param region_code      region code
 *
 *  @return                 Region string or NULL
 */
char *
region_code_2_string(t_u8 region_code)
{
	t_u8 i;
	t_u8 size =
		sizeof(hw_region_code_mapping) / sizeof(region_code_mapping_t);

	ENTER();
	for (i = 0; i < size; i++) {
		if (hw_region_code_mapping[i].code == region_code) {
			LEAVE();
			return hw_region_code_mapping[i].region;
		}
	}
	LEAVE();
	return NULL;
}

t_u8
woal_is_valid_alpha2(char *alpha2)
{
	if (!alpha2 || strlen(alpha2) < 2)
		return MFALSE;
	if (isalpha(alpha2[0]) && isalpha(alpha2[1]))
		return MTRUE;
	return MFALSE;
}
#endif

/**
 *  @brief Copy mc address to the mlist
 *
 *  @param mlist    A pointer to mlan_multicast_list structure
 *  @param mac      mc address
 *
 *  @return         N/A
 */
static inline void
woal_copy_mc_addr(mlan_multicast_list * mlist, mlan_802_11_mac_addr mac)
{
	int i = 0;
	for (i = 0; i < mlist->num_multicast_addr; i++) {
		if (!memcmp(&mlist->mac_list[i], mac, ETH_ALEN))
			return;
	}
	if (mlist->num_multicast_addr < MLAN_MAX_MULTICAST_LIST_SIZE)
		memcpy(&mlist->mac_list[mlist->num_multicast_addr], mac,
		       ETH_ALEN);
	mlist->num_multicast_addr++;
	return;
}

/**
 *  @brief Copy multicast table
 *
 *  @param mlist    A pointer to mlan_multicast_list structure
 *  @param dev      A pointer to net_device structure
 *
 *  @return         Number of multicast addresses
 */
static inline int
woal_copy_mcast_addr(mlan_multicast_list * mlist, struct net_device *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
	struct dev_mc_list *mcptr = dev->mc_list;
	int i = 0;
#else
	struct netdev_hw_addr *mcptr = NULL;
#endif /* < 2.6.35 */

	ENTER();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
	for (i = 0; i < dev->mc_count && mcptr; i++) {
		woal_copy_mc_addr(mlist, mcptr->dmi_addr);
		mcptr = mcptr->next;
	}
#else
	netdev_for_each_mc_addr(mcptr, dev)
		woal_copy_mc_addr(mlist, mcptr->addr);
#endif /* < 2.6.35 */
	LEAVE();
	return mlist->num_multicast_addr;
}

/**
 *  @brief copy mc list from all the active interface
 *
 *  @param handle  A pointer to moal_handle
 *  @param mlist  A pointer to multicast list
 *
 *
 *  @return       total_mc_count
 */
static int
woal_copy_all_mc_list(moal_handle * handle, mlan_multicast_list * mlist)
{
	int i;
	moal_private *priv = NULL;
#ifdef STA_SUPPORT
	int mc_count = 0;
#endif
	ENTER();
	for (i = 0; i < handle->priv_num && (priv = handle->priv[i]); i++) {
#ifdef STA_SUPPORT
		if (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_STA) {
			if (handle->priv[i]->media_connected == MTRUE) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
				mc_count = priv->netdev->mc_count;
#else
				mc_count = netdev_mc_count(priv->netdev);
#endif
				if (mc_count)
					woal_copy_mcast_addr(mlist,
							     priv->netdev);
			}
		}
#endif
	}
	PRINTM(MIOCTL, "total mc_count=%d\n", mlist->num_multicast_addr);
	LEAVE();
	return mlist->num_multicast_addr;
}

/**
 *  @brief Fill in wait queue
 *
 *  @param priv         A pointer to moal_private structure
 *  @param wait         A pointer to wait_queue structure
 *  @param wait_option  Wait option
 *
 *  @return             N/A
 */
static inline void
woal_fill_wait_queue(moal_private * priv, wait_queue * wait, t_u8 wait_option)
{
	ENTER();
	wait->start_time = jiffies;
	wait->condition = MFALSE;
	switch (wait_option) {
	case MOAL_NO_WAIT:
		break;
	case MOAL_IOCTL_WAIT:
		wait->wait = &priv->ioctl_wait_q;
		break;
	case MOAL_CMD_WAIT:
		wait->wait = &priv->cmd_wait_q;
		break;
#ifdef CONFIG_PROC_FS
	case MOAL_PROC_WAIT:
		wait->wait = &priv->proc_wait_q;
		break;
#endif
#if defined(STA_WEXT) || defined(UAP_WEXT)
	case MOAL_WSTATS_WAIT:
		if (IS_STA_OR_UAP_WEXT(cfg80211_wext))
			wait->wait = &priv->w_stats_wait_q;
		break;
#endif
	}
	LEAVE();
	return;
}

/**
 *  @brief Wait mlan ioctl complete
 *
 *  @param priv         A pointer to moal_private structure
 *  @param req          A pointer to mlan_ioctl_req structure
 *  @param wait_option  Wait option
 *
 *  @return             N/A
 */
static inline void
woal_wait_ioctl_complete(moal_private * priv, mlan_ioctl_req * req,
			 t_u8 wait_option)
{
	mlan_status status;
	wait_queue *wait = (wait_queue *) req->reserved_1;

	ENTER();

	switch (wait_option) {
	case MOAL_NO_WAIT:
		break;
	case MOAL_IOCTL_WAIT:
		wait_event_interruptible_exclusive(priv->ioctl_wait_q,
						   wait->condition);
		break;
	case MOAL_CMD_WAIT:
		wait_event_interruptible_exclusive(priv->cmd_wait_q,
						   wait->condition);
		break;
#ifdef CONFIG_PROC_FS
	case MOAL_PROC_WAIT:
		wait_event_interruptible_exclusive(priv->proc_wait_q,
						   wait->condition);
		break;
#endif
#if defined(STA_WEXT) || defined(UAP_WEXT)
	case MOAL_WSTATS_WAIT:
		if (IS_STA_OR_UAP_WEXT(cfg80211_wext))
			wait_event_interruptible_exclusive(priv->w_stats_wait_q,
							   wait->condition);
		break;
#endif
	}
	if (wait->condition == MFALSE) {
		req->action = MLAN_ACT_CANCEL;
		status = mlan_ioctl(priv->phandle->pmlan_adapter, req);
		PRINTM(MMSG,
		       "wlan: IOCTL cancel %p id=0x%x, sub_id=0x%x, wait_option=%d, action=%d, status=%d\n",
		       req, req->req_id, (*(t_u32 *) req->pbuf), wait_option,
		       (int)req->action, status);
	}
	LEAVE();
	return;
}

/**
 *  @brief CAC period block cmd handler
 *
 *  @param priv     A pointer to moal_private structure
 *  @param req      A pointer to mlan_ioctl_req buffer
 *
 *  @return         MTRUE/MFALSE
 */
static inline t_bool
woal_cac_period_block_cmd(moal_private * priv, pmlan_ioctl_req req)
{
	mlan_status ret = MFALSE;
	t_u32 sub_command;

	ENTER();
	if (req == NULL || req->pbuf == NULL) {
		goto done;
	}

	sub_command = *(t_u32 *) req->pbuf;

	switch (req->req_id) {
	case MLAN_IOCTL_SCAN:
		if (sub_command == MLAN_OID_SCAN_NORMAL ||
		    sub_command == MLAN_OID_SCAN_SPECIFIC_SSID ||
		    sub_command == MLAN_OID_SCAN_USER_CONFIG) {
			ret = MTRUE;
		}
		break;
	case MLAN_IOCTL_BSS:
		if (sub_command == MLAN_OID_BSS_STOP ||
#ifdef UAP_SUPPORT
		    sub_command == MLAN_OID_UAP_BSS_CONFIG ||
#endif
		    sub_command == MLAN_OID_BSS_CHANNEL
		    /* sub_command == MLAN_OID_BSS_ROLE */ ) {
			ret = MTRUE;
		}
		break;
	case MLAN_IOCTL_RADIO_CFG:
		if (sub_command == MLAN_OID_BAND_CFG) {
			ret = MTRUE;
		}
		break;
#if defined(UAP_SUPPORT)
	case MLAN_IOCTL_SNMP_MIB:
		if (sub_command == MLAN_OID_SNMP_MIB_DOT11D ||
		    sub_command == MLAN_OID_SNMP_MIB_DOT11H) {
			ret = MTRUE;
		}
		break;
#endif
	case MLAN_IOCTL_11D_CFG:
#ifdef STA_SUPPORT
		if (sub_command == MLAN_OID_11D_CFG_ENABLE) {
			ret = MTRUE;
		}
#endif
		if (sub_command == MLAN_OID_11D_DOMAIN_INFO) {
			ret = MTRUE;
		}
		break;
	case MLAN_IOCTL_MISC_CFG:
		if (sub_command == MLAN_OID_MISC_REGION) {
			ret = MTRUE;
		}
		if (sub_command == MLAN_OID_MISC_HOST_CMD) {
			phostcmd_header phostcmd;
			t_u8 *ptlv_buf;
			t_u16 tag, length;

			phostcmd =
				(phostcmd_header) ((pmlan_ds_misc_cfg) req->
						   pbuf)->param.hostcmd.cmd;
			ptlv_buf = (t_u8 *) phostcmd + sizeof(hostcmd_header);
			if (phostcmd->action == MLAN_ACT_SET) {
				while (ptlv_buf <
				       (t_u8 *) phostcmd + phostcmd->size) {
					tag = *(t_u16 *) ptlv_buf;
					length = *(t_u16 *) (ptlv_buf + 2);
					/* Check Blocking TLV here, should add
					   more... */
					if (tag == MRVL_CHANNELCONFIG_TLV_ID) {
						ret = MTRUE;
						break;
					}
					ptlv_buf +=
						(length + MRVL_TLV_HEADER_SIZE);
				}
			}
		}
		break;
	case MLAN_IOCTL_11H_CFG:
		/* Prevent execute more than once */
		if (sub_command == MLAN_OID_11H_CHANNEL_CHECK) {
			ret = MTRUE;
		}
		break;
	default:
		ret = MFALSE;
		break;
	}

done:
	LEAVE();
	return ret;
}

/********************************************************
			Global Functions
********************************************************/

/**
 *  @brief Send ioctl request to MLAN
 *
 *  @param priv          A pointer to moal_private structure
 *  @param req           A pointer to mlan_ioctl_req buffer
 *  @param wait_option   Wait option (MOAL_WAIT or MOAL_NO_WAIT)
 *
 *  @return              MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_request_ioctl(moal_private * priv, mlan_ioctl_req * req, t_u8 wait_option)
{
	wait_queue *wait;
	mlan_status status;
	unsigned long flags;

	ENTER();

	if (priv->phandle->surprise_removed == MTRUE) {
		PRINTM(MERROR,
		       "IOCTL is not allowed while the device is not present\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
#if defined(SDIO_SUSPEND_RESUME)
	if (priv->phandle->is_suspended == MTRUE) {
		PRINTM(MERROR, "IOCTL is not allowed while suspended\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
#endif

	/* For MLAN_OID_MISC_HOST_CMD, action is 0, "action set" is checked
	   later */
	if ((req->action == MLAN_ACT_SET || req->action == 0) &&
	    priv->phandle->cac_period == MTRUE) {
		t_u32 sub_command;
		/* CAC checking period left to complete jiffies */
		long cac_left_jiffies;

		sub_command = *(t_u32 *) req->pbuf;

		/* cac_left_jiffies will be negative if and only if * event
		   MLAN_EVENT_ID_DRV_MEAS_REPORT recieved from FW * after CAC
		   measure period ends, * usually this could be considered as a
		   FW bug */
		cac_left_jiffies = MEAS_REPORT_TIME -
			(jiffies - priv->phandle->meas_start_jiffies);
#ifdef DFS_TESTING_SUPPORT
		if (priv->phandle->cac_period_jiffies) {
			cac_left_jiffies = priv->phandle->cac_period_jiffies -
				(jiffies - priv->phandle->meas_start_jiffies);
		}
#endif
		if (cac_left_jiffies < 0) {
			/* Avoid driver hang in FW died during CAC measure
			   period */
			priv->phandle->cac_period = MFALSE;
			PRINTM(MERROR,
			       "CAC measure period spends longer than scheduled time "
			       "or meas done event never received\n");
			status = MLAN_STATUS_FAILURE;
			goto done;
		}

		/* Check BSS START first */
		if (sub_command == MLAN_OID_BSS_START) {
			mlan_ds_bss *bss;
			bss = (mlan_ds_bss *) req->pbuf;
			/*
			 * Bss delay start after channel report received,
			 * not block the driver by delay executing. This is
			 * because a BSS_START cmd is always executed right
			 * after channel check issued.
			 */
			if (priv->phandle->delay_bss_start == MFALSE) {
				PRINTM(MMSG,
				       "Received BSS Start command during CAC period, "
				       "delay executing %ld seconds\n",
				       cac_left_jiffies / HZ);
				priv->phandle->delay_bss_start = MTRUE;
				memcpy(&priv->phandle->delay_ssid_bssid,
				       &bss->param.ssid_bssid,
				       sizeof(mlan_ssid_bssid));
				/* TODO: return success to allow the half below
				   of routines of which calling BSS start to
				   execute */
				status = MLAN_STATUS_SUCCESS;
				goto done;
			} else {
				/* TODO: not blocking it, just return failure */
				PRINTM(MMSG,
				       "Only one BSS Start command allowed for delay "
				       "executing!\n");
				status = MLAN_STATUS_FAILURE;
				goto done;
			}
		}
		if (woal_cac_period_block_cmd(priv, req)) {
			priv->phandle->meas_wait_q_woken = MFALSE;
			PRINTM(MMSG, "CAC check is on going... Blocking Command"
			       " %ld seconds\n", cac_left_jiffies / HZ);
			/* blocking timeout set to 1.5 * CAC checking period
			   left time */
			wait_event_interruptible_timeout(priv->phandle->
							 meas_wait_q,
							 priv->phandle->
							 meas_wait_q_woken,
							 cac_left_jiffies * 3 /
							 2);
		}
	} else if (priv->phandle->cac_period) {
		PRINTM(MINFO, "Operation during CAC check period.\n");
	}
	wait = (wait_queue *) req->reserved_1;
	req->bss_index = priv->bss_index;
	if (wait_option)
		woal_fill_wait_queue(priv, wait, wait_option);
	else
		req->reserved_1 = 0;

	/* Call MLAN ioctl handle */
	atomic_inc(&priv->phandle->ioctl_pending);
	spin_lock_irqsave(&priv->phandle->driver_lock, flags);
	status = mlan_ioctl(priv->phandle->pmlan_adapter, req);
	spin_unlock_irqrestore(&priv->phandle->driver_lock, flags);
	switch (status) {
	case MLAN_STATUS_PENDING:
		PRINTM(MIOCTL,
		       "IOCTL pending: %p id=0x%x, sub_id=0x%x wait_option=%d, action=%d\n",
		       req, req->req_id, (*(t_u32 *) req->pbuf), wait_option,
		       (int)req->action);
		/* Status pending, wake up main process */
		queue_work(priv->phandle->workqueue, &priv->phandle->main_work);

		/* Wait for completion */
		if (wait_option) {
			woal_wait_ioctl_complete(priv, req, wait_option);
			status = wait->status;
		}
		break;
	case MLAN_STATUS_SUCCESS:
	case MLAN_STATUS_FAILURE:
	case MLAN_STATUS_RESOURCE:
		PRINTM(MIOCTL,
		       "IOCTL: %p id=0x%x, sub_id=0x%x wait_option=%d, action=%d status=%d\n",
		       req, req->req_id, (*(t_u32 *) req->pbuf), wait_option,
		       (int)req->action, status);
	default:
		atomic_dec(&priv->phandle->ioctl_pending);
		break;
	}

done:
	LEAVE();
	return status;
}

/**
 *  @brief Send set MAC address request to MLAN
 *
 *  @param priv   A pointer to moal_private structure
 *
 *  @return       MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_request_set_mac_address(moal_private * priv)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_bss *bss = NULL;
	mlan_status status;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = (mlan_ioctl_req *) woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		status = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_BSS_MAC_ADDR;
	memcpy(&bss->param.mac_addr, priv->current_addr,
	       sizeof(mlan_802_11_mac_addr));
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_SET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, MOAL_CMD_WAIT);
	if (status == MLAN_STATUS_SUCCESS) {
		memcpy(priv->netdev->dev_addr, priv->current_addr, ETH_ALEN);
		HEXDUMP("priv->MacAddr:", priv->current_addr, ETH_ALEN);
	} else {
		PRINTM(MERROR,
		       "set mac address failed! status=%d, error_code=0x%x\n",
		       status, req->status_code);
	}
done:
	if (req)
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Send multicast list request to MLAN
 *
 *  @param priv   A pointer to moal_private structure
 *  @param dev    A pointer to net_device structure
 *
 *  @return       N/A
 */
void
woal_request_set_multicast_list(moal_private * priv, struct net_device *dev)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_bss *bss = NULL;
	mlan_status status;
	int mc_count = 0;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = (mlan_ioctl_req *) woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		PRINTM(MERROR, "%s:Fail to allocate ioctl req buffer\n",
		       __FUNCTION__);
		goto done;
	}

	/* Fill request buffer */
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_BSS_MULTICAST_LIST;
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_SET;
	if (dev->flags & IFF_PROMISC) {
		bss->param.multicast_list.mode = MLAN_PROMISC_MODE;
	} else if (dev->flags & IFF_ALLMULTI) {
		bss->param.multicast_list.mode = MLAN_ALL_MULTI_MODE;
	} else {
		bss->param.multicast_list.mode = MLAN_MULTICAST_MODE;
		mc_count =
			woal_copy_all_mc_list(priv->phandle,
					      &bss->param.multicast_list);
		if (mc_count > MLAN_MAX_MULTICAST_LIST_SIZE)
			bss->param.multicast_list.mode = MLAN_ALL_MULTI_MODE;
	}

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, MOAL_NO_WAIT);
	if (status != MLAN_STATUS_PENDING)
		kfree(req);
done:
	LEAVE();
	return;
}

/**
 *  @brief Send deauth command to MLAN
 *
 *  @param priv          A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param mac           MAC address to deauthenticate
 *
 *  @return              MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_disconnect(moal_private * priv, t_u8 wait_option, t_u8 * mac)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_bss *bss = NULL;
	mlan_status status;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = (mlan_ioctl_req *) woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		status = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_BSS_STOP;
	if (mac)
		memcpy((t_u8 *) & bss->param.bssid, mac,
		       sizeof(mlan_802_11_mac_addr));
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_SET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);

done:
	if (req && (wait_option || status != MLAN_STATUS_PENDING))
		kfree(req);
#ifdef REASSOCIATION
	priv->reassoc_required = MFALSE;
#endif /* REASSOCIATION */
	LEAVE();
	return status;
}

/**
 *  @brief Send bss_start command to MLAN
 *
 *  @param priv          A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param ssid_bssid    A point to mlan_ssid_bssid structure
 *
 *  @return              MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_bss_start(moal_private * priv, t_u8 wait_option,
	       mlan_ssid_bssid * ssid_bssid)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_bss *bss = NULL;
	mlan_status status;

	ENTER();

	/* Stop the O.S. TX queue When we are roaming */
	woal_stop_queue(priv->netdev);
	if (priv->media_connected == MFALSE) {
		if (netif_carrier_ok(priv->netdev))
			netif_carrier_off(priv->netdev);
	}

	/* Allocate an IOCTL request buffer */
	req = (mlan_ioctl_req *) woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		status = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_BSS_START;
	if (ssid_bssid)
		memcpy(&bss->param.ssid_bssid, ssid_bssid,
		       sizeof(mlan_ssid_bssid));
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_SET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
#ifdef STA_CFG80211
#ifdef STA_SUPPORT
	priv->assoc_status = req->status_code;
#endif
#endif
done:
	if (req)
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Get BSS info
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param bss_info             A pointer to mlan_bss_info structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_bss_info(moal_private * priv, t_u8 wait_option,
		  mlan_bss_info * bss_info)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_get_info *info = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_get_info));
	if (req == NULL) {
		PRINTM(MERROR,
		       "Fail to allocate the buffer for get bss_info\n");
		status = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	info = (mlan_ds_get_info *) req->pbuf;
	info->sub_command = MLAN_OID_GET_BSS_INFO;
	req->req_id = MLAN_IOCTL_GET_INFO;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS) {
		if (bss_info)
			memcpy(bss_info, &info->param.bss_info,
			       sizeof(mlan_bss_info));
	}
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

#ifdef STA_SUPPORT
/**
 *  @brief Set/Get retry count
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param action               Action set or get
 *  @param wait_option          Wait option
 *  @param value                Retry value
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_get_retry(moal_private * priv, t_u32 action,
		   t_u8 wait_option, int *value)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_snmp_mib *mib = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_snmp_mib));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	mib = (mlan_ds_snmp_mib *) req->pbuf;
	mib->sub_command = MLAN_OID_SNMP_MIB_RETRY_COUNT;
	req->req_id = MLAN_IOCTL_SNMP_MIB;
	req->action = action;

	if (action == MLAN_ACT_SET) {
		if (*value < MLAN_TX_RETRY_MIN || *value > MLAN_TX_RETRY_MAX) {
			ret = MLAN_STATUS_FAILURE;
			goto done;
		}
		mib->param.retry_count = *value;
	}

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS && action == MLAN_ACT_GET) {
		*value = mib->param.retry_count;
	}
#ifdef STA_CFG80211
	/* If set is invoked from other than iw i.e iwconfig, wiphy retry count
	   should be updated as well */
	if (IS_STA_CFG80211(cfg80211_wext) && priv->wdev && priv->wdev->wiphy &&
	    (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) &&
	    (action == MLAN_ACT_SET)) {
		priv->wdev->wiphy->retry_long = (t_u8) * value;
		priv->wdev->wiphy->retry_short = (t_u8) * value;
	}
#endif

done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Set/Get RTS threshold
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param action               Action set or get
 *  @param wait_option          Wait option
 *  @param value                RTS threshold value
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_get_rts(moal_private * priv, t_u32 action,
		 t_u8 wait_option, int *value)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_snmp_mib *mib = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_snmp_mib));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	mib = (mlan_ds_snmp_mib *) req->pbuf;
	mib->sub_command = MLAN_OID_SNMP_MIB_RTS_THRESHOLD;
	req->req_id = MLAN_IOCTL_SNMP_MIB;
	req->action = action;

	if (action == MLAN_ACT_SET) {
		if (*value < MLAN_RTS_MIN_VALUE || *value > MLAN_RTS_MAX_VALUE) {
			ret = MLAN_STATUS_FAILURE;
			goto done;
		}
		mib->param.rts_threshold = *value;
	}

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS && action == MLAN_ACT_GET) {
		*value = mib->param.rts_threshold;
	}
#ifdef STA_CFG80211
	/* If set is invoked from other than iw i.e iwconfig, wiphy RTS
	   threshold should be updated as well */
	if (IS_STA_CFG80211(cfg80211_wext) && priv->wdev && priv->wdev->wiphy &&
	    (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) &&
	    (action == MLAN_ACT_SET))
		priv->wdev->wiphy->rts_threshold = *value;
#endif

done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Set/Get Fragment threshold
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param action               Action set or get
 *  @param wait_option          Wait option
 *  @param value                Fragment threshold value
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_get_frag(moal_private * priv, t_u32 action,
		  t_u8 wait_option, int *value)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_snmp_mib *mib = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_snmp_mib));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	mib = (mlan_ds_snmp_mib *) req->pbuf;
	mib->sub_command = MLAN_OID_SNMP_MIB_FRAG_THRESHOLD;
	req->req_id = MLAN_IOCTL_SNMP_MIB;
	req->action = action;

	if (action == MLAN_ACT_SET) {
		if (*value < MLAN_FRAG_MIN_VALUE ||
		    *value > MLAN_FRAG_MAX_VALUE) {
			ret = MLAN_STATUS_FAILURE;
			goto done;
		}
		mib->param.frag_threshold = *value;
	}

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS && action == MLAN_ACT_GET) {
		*value = mib->param.frag_threshold;
	}
#ifdef STA_CFG80211
	/* If set is invoked from other than iw i.e iwconfig, wiphy fragment
	   threshold should be updated as well */
	if (IS_STA_CFG80211(cfg80211_wext) && priv->wdev && priv->wdev->wiphy &&
	    (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) &&
	    (action == MLAN_ACT_SET))
		priv->wdev->wiphy->frag_threshold = *value;
#endif

done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Set/Get generic IE
 *
 *  @param priv         A pointer to moal_private structure
 *  @param action       Action set or get
 *  @param ie           Information element
 *  @param ie_len       Length of the IE
 *
 *  @return             MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_get_gen_ie(moal_private * priv, t_u32 action, t_u8 * ie, int *ie_len)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	if ((action == MLAN_ACT_GET) && (ie == NULL || ie_len == NULL)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (action == MLAN_ACT_SET && *ie_len > MAX_IE_SIZE) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	misc = (mlan_ds_misc_cfg *) req->pbuf;
	misc->sub_command = MLAN_OID_MISC_GEN_IE;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	req->action = action;
	misc->param.gen_ie.type = MLAN_IE_TYPE_GEN_IE;

	if (action == MLAN_ACT_SET) {
		misc->param.gen_ie.len = *ie_len;
		if (*ie_len)
			memcpy(misc->param.gen_ie.ie_data, ie, *ie_len);
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (action == MLAN_ACT_GET) {
		*ie_len = misc->param.gen_ie.len;
		if (*ie_len)
			memcpy(ie, misc->param.gen_ie.ie_data, *ie_len);
	}

done:
	if (req)
		kfree(req);

	LEAVE();
	return ret;
}

/**
 *  @brief Set/Get TX power
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param action               Action set or get
 *  @param power_cfg            A pinter to mlan_power_cfg_t structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_get_tx_power(moal_private * priv,
		      t_u32 action, mlan_power_cfg_t * power_cfg)
{
	mlan_ds_power_cfg *pcfg = NULL;
	mlan_ioctl_req *req = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_power_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	pcfg = (mlan_ds_power_cfg *) req->pbuf;
	pcfg->sub_command = MLAN_OID_POWER_CFG;
	req->req_id = MLAN_IOCTL_POWER_CFG;
	req->action = action;
	if (action == MLAN_ACT_SET && power_cfg)
		memcpy(&pcfg->param.power_cfg, power_cfg,
		       sizeof(mlan_power_cfg_t));

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT))
		ret = MLAN_STATUS_FAILURE;

	if (ret == MLAN_STATUS_SUCCESS && power_cfg)
		memcpy(power_cfg, &pcfg->param.power_cfg,
		       sizeof(mlan_power_cfg_t));

done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Set/Get IEEE power management
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param action               Action set or get
 *  @param disabled             A pointer to disabled flag
 *  @param power_type           IEEE power type
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_get_power_mgmt(moal_private * priv,
			t_u32 action, int *disabled, int power_type)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_pm_cfg *pm_cfg = NULL;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	pm_cfg = (mlan_ds_pm_cfg *) req->pbuf;
	pm_cfg->sub_command = MLAN_OID_PM_CFG_IEEE_PS;
	req->req_id = MLAN_IOCTL_PM_CFG;
	req->action = action;

	if (action == MLAN_ACT_SET) {
		PRINTM(MINFO, "PS_MODE set power disabled=%d power type=%#x\n",
		       *disabled, power_type);
		if (*disabled)
			pm_cfg->param.ps_mode = 0;
		else {
			/* Check not support case only (vwrq->disabled ==
			   FALSE) */
			if ((power_type & MW_POWER_TYPE) == MW_POWER_TIMEOUT) {
				PRINTM(MERROR,
				       "Setting power timeout is not supported\n");
				ret = MLAN_STATUS_FAILURE;
				goto done;
			} else if ((power_type & MW_POWER_TYPE) ==
				   MW_POWER_PERIOD) {
				PRINTM(MERROR,
				       "Setting power period is not supported\n");
				ret = MLAN_STATUS_FAILURE;
				goto done;
			}
			pm_cfg->param.ps_mode = 1;
		}
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (ret == MLAN_STATUS_SUCCESS && action == MLAN_ACT_GET)
		*disabled = pm_cfg->param.ps_mode;

#ifdef STA_CFG80211
	/* If set is invoked from other than iw i.e iwconfig, wiphy IEEE power
	   save mode should be updated */
	if (IS_STA_CFG80211(cfg80211_wext) && priv->wdev &&
	    (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) &&
	    (action == MLAN_ACT_SET)) {
		if (*disabled)
			priv->wdev->ps = MFALSE;
		else
			priv->wdev->ps = MTRUE;
	}
#endif

done:
	if (req)
		kfree(req);

	LEAVE();
	return ret;
}

/**
 * @brief Set region code
 *
 * @param priv     A pointer to moal_private structure
 * @param region   A pointer to region string
 *
 * @return         MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING --success, otherwise fail
 */
mlan_status
woal_set_region_code(moal_private * priv, char *region)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_misc_cfg *cfg = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	cfg = (mlan_ds_misc_cfg *) req->pbuf;
	cfg->sub_command = MLAN_OID_MISC_REGION;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	req->action = MLAN_ACT_SET;
	cfg->param.region_code = region_string_2_region_code(region);
	ret = woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT);

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Set/Get data rate
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param action               Action set or get
 *  @param datarate             A pointer to mlan_rate_cfg_t structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_get_data_rate(moal_private * priv,
		       t_u8 action, mlan_rate_cfg_t * datarate)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_rate *rate = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_rate));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	rate = (mlan_ds_rate *) req->pbuf;
	rate->param.rate_cfg.rate_type = MLAN_RATE_VALUE;
	rate->sub_command = MLAN_OID_RATE_CFG;
	req->req_id = MLAN_IOCTL_RATE;
	req->action = action;

	if (datarate && (action == MLAN_ACT_SET))
		memcpy(&rate->param.rate_cfg, datarate,
		       sizeof(mlan_rate_cfg_t));

	if (MLAN_STATUS_SUCCESS ==
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		if (datarate && (action == MLAN_ACT_GET))
			memcpy(datarate, &rate->param.rate_cfg,
			       sizeof(mlan_rate_cfg_t));
	} else {
		ret = MLAN_STATUS_FAILURE;
	}

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Get assoc_resp buffer
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param assoc_rsp            A pointer to mlan_ds_misc_assoc_rsp structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_get_assoc_rsp(moal_private * priv, mlan_ds_misc_assoc_rsp * assoc_rsp)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		PRINTM(MERROR, "Fail to allocate buffer for get assoc resp\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	req->req_id = MLAN_IOCTL_MISC_CFG;
	misc = (pmlan_ds_misc_cfg) req->pbuf;
	misc->sub_command = MLAN_OID_MISC_ASSOC_RSP;
	req->action = MLAN_ACT_GET;

	if (MLAN_STATUS_SUCCESS ==
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		if (assoc_rsp)
			memcpy(assoc_rsp, &misc->param.assoc_resp,
			       sizeof(mlan_ds_misc_assoc_rsp));
	} else {
		ret = MLAN_STATUS_FAILURE;
	}
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}
#endif

/**
 *  @brief Send get FW info request to MLAN
 *
 *  @param priv             A pointer to moal_private structure
 *  @param wait_option      Wait option
 *  @param fw_info          FW information
 *
 *  @return                 MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_request_get_fw_info(moal_private * priv, t_u8 wait_option,
			 mlan_fw_info * fw_info)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_get_info *info;
	mlan_status status;
	ENTER();
	memset(priv->current_addr, 0xff, ETH_ALEN);

	/* Allocate an IOCTL request buffer */
	req = (mlan_ioctl_req *) woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		status = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	info = (mlan_ds_get_info *) req->pbuf;
	req->req_id = MLAN_IOCTL_GET_INFO;
	req->action = MLAN_ACT_GET;
	info->sub_command = MLAN_OID_GET_FW_INFO;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS) {
		priv->phandle->fw_release_number = info->param.fw_info.fw_ver;
		if (priv->current_addr[0] == 0xff)
			memcpy(priv->current_addr,
			       &info->param.fw_info.mac_addr,
			       sizeof(mlan_802_11_mac_addr));
		memcpy(priv->netdev->dev_addr, priv->current_addr, ETH_ALEN);
		if (fw_info)
			memcpy(fw_info, &info->param.fw_info,
			       sizeof(mlan_fw_info));
		DBG_HEXDUMP(MCMD_D, "mac", priv->current_addr, 6);
	} else
		PRINTM(MERROR,
		       "get fw info failed! status=%d, error_code=0x%x\n",
		       status, req->status_code);
done:
	if (req)
		kfree(req);
	LEAVE();
	return status;
}

#ifdef PROC_DEBUG
/**
 *  @brief Get debug info
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param debug_info           A pointer to mlan_debug_info structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_debug_info(moal_private * priv, t_u8 wait_option,
		    mlan_debug_info * debug_info)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_get_info *info = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_get_info));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	info = (mlan_ds_get_info *) req->pbuf;
	info->sub_command = MLAN_OID_GET_DEBUG_INFO;
	req->req_id = MLAN_IOCTL_GET_INFO;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS) {
		if (debug_info) {
			memcpy(debug_info, &info->param.debug_info,
			       sizeof(mlan_debug_info));
		}
	}
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Set debug info
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param debug_info           A pointer to mlan_debug_info structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_debug_info(moal_private * priv, t_u8 wait_option,
		    mlan_debug_info * debug_info)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_get_info *info = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;

	ENTER();

	if (!debug_info) {
		ret = -EINVAL;
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_get_info));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	info = (mlan_ds_get_info *) req->pbuf;
	info->sub_command = MLAN_OID_GET_DEBUG_INFO;
	memcpy(&info->param.debug_info, debug_info, sizeof(mlan_debug_info));
	req->req_id = MLAN_IOCTL_GET_INFO;
	req->action = MLAN_ACT_SET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}
#endif /* PROC_DEBUG */

#if defined(STA_WEXT) || defined(UAP_WEXT)
/**
 *  @brief host command ioctl function
 *
 *  @param priv		A pointer to moal_private structure
 *  @param wrq 		A pointer to iwreq structure
 *  @return    		0 --success, otherwise fail
 */
int
woal_host_command(moal_private * priv, struct iwreq *wrq)
{
	HostCmd_Header cmd_header;
	int ret = 0;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	/* Sanity check */
	if (wrq->u.data.pointer == NULL) {
		PRINTM(MERROR, "hostcmd IOCTL corrupt data\n");
		ret = -EINVAL;
		goto done;
	}
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	misc = (mlan_ds_misc_cfg *) req->pbuf;
	memset(&cmd_header, 0, sizeof(cmd_header));

	/* get command header */
	if (copy_from_user
	    (&cmd_header, wrq->u.data.pointer, sizeof(HostCmd_Header))) {
		PRINTM(MERROR, "copy from user failed: Host command header\n");
		ret = -EFAULT;
		goto done;
	}
	misc->param.hostcmd.len = woal_le16_to_cpu(cmd_header.size);

	PRINTM(MINFO, "Host command len = %u\n", misc->param.hostcmd.len);

	if (!misc->param.hostcmd.len ||
	    misc->param.hostcmd.len > MLAN_SIZE_OF_CMD_BUFFER) {
		PRINTM(MERROR, "Invalid data buffer length\n");
		ret = -EINVAL;
		goto done;
	}

	/* get the whole command from user */
	if (copy_from_user
	    (misc->param.hostcmd.cmd, wrq->u.data.pointer,
	     woal_le16_to_cpu(cmd_header.size))) {
		PRINTM(MERROR, "copy from user failed\n");
		ret = -EFAULT;
		goto done;
	}
	misc->sub_command = MLAN_OID_MISC_HOST_CMD;
	req->req_id = MLAN_IOCTL_MISC_CFG;

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}
	if (copy_to_user
	    (wrq->u.data.pointer, (t_u8 *) misc->param.hostcmd.cmd,
	     misc->param.hostcmd.len)) {
		ret = -EFAULT;
		goto done;
	}
	wrq->u.data.length = misc->param.hostcmd.len;
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}
#endif

#if defined(WIFI_DIRECT_SUPPORT) || defined(UAP_SUPPORT)
/**
 *  @brief host command ioctl function
 *
 *  @param dev      A pointer to net_device structure
 *  @param req      A pointer to ifreq structure
 *  @return         0 --success, otherwise fail
 */
/*********  format of ifr_data *************/
/*    buf_len + Hostcmd_body 		   */
/*    buf_len: 4 bytes                     */
/*             the length of the buf which */
/*             can be used to return data  */
/*             to application		   */
/*    Hostcmd_body       	           */
/*******************************************/
int
woal_hostcmd_ioctl(struct net_device *dev, struct ifreq *req)
{
	moal_private *priv = (moal_private *) netdev_priv(dev);
	t_u32 buf_len = 0;
	HostCmd_Header cmd_header;
	int ret = 0;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ioctl_req *ioctl_req = NULL;

	ENTER();

	/* Sanity check */
	if (req->ifr_data == NULL) {
		PRINTM(MERROR, "uap_hostcmd_ioctl() corrupt data\n");
		ret = -EFAULT;
		goto done;
	}

	if (copy_from_user(&buf_len, req->ifr_data, sizeof(buf_len))) {
		PRINTM(MERROR, "Copy from user failed\n");
		ret = -EFAULT;
		goto done;
	}

	memset(&cmd_header, 0, sizeof(cmd_header));

	/* get command header */
	if (copy_from_user
	    (&cmd_header, req->ifr_data + sizeof(buf_len),
	     sizeof(HostCmd_Header))) {
		PRINTM(MERROR, "Copy from user failed\n");
		ret = -EFAULT;
		goto done;
	}

	PRINTM(MINFO, "Host command len = %d\n",
	       woal_le16_to_cpu(cmd_header.size));

	if (woal_le16_to_cpu(cmd_header.size) > MLAN_SIZE_OF_CMD_BUFFER) {
		ret = -EINVAL;
		goto done;
	}

	ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (ioctl_req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	misc = (mlan_ds_misc_cfg *) ioctl_req->pbuf;

	misc->param.hostcmd.len = woal_le16_to_cpu(cmd_header.size);

	/* get the whole command from user */
	if (copy_from_user
	    (misc->param.hostcmd.cmd, req->ifr_data + sizeof(buf_len),
	     misc->param.hostcmd.len)) {
		PRINTM(MERROR, "copy from user failed\n");
		ret = -EFAULT;
		goto done;
	}
	misc->sub_command = MLAN_OID_MISC_HOST_CMD;
	ioctl_req->req_id = MLAN_IOCTL_MISC_CFG;

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, ioctl_req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}
	if (misc->param.hostcmd.len > buf_len) {
		PRINTM(MERROR,
		       "buf_len is too small, resp_len=%d, buf_len=%d\n",
		       (int)misc->param.hostcmd.len, (int)buf_len);
		ret = -EFAULT;
		goto done;
	}
	if (copy_to_user
	    (req->ifr_data + sizeof(buf_len), (t_u8 *) misc->param.hostcmd.cmd,
	     misc->param.hostcmd.len)) {
		ret = -EFAULT;
		goto done;
	}
done:
	if (ioctl_req)
		kfree(ioctl_req);
	LEAVE();
	return ret;
}
#endif

/**
 *  @brief CUSTOM_IE ioctl handler
 *
 *  @param dev      A pointer to net_device structure
 *  @param req      A pointer to ifreq structure
 *  @return         0 --success, otherwise fail
 */
int
woal_custom_ie_ioctl(struct net_device *dev, struct ifreq *req)
{
	moal_private *priv = (moal_private *) netdev_priv(dev);
	mlan_ioctl_req *ioctl_req = NULL;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ds_misc_custom_ie *custom_ie = NULL;
	int ret = 0;

	ENTER();

	/* Sanity check */
	if (req->ifr_data == NULL) {
		PRINTM(MERROR, "woal_custom_ie_ioctl() corrupt data\n");
		ret = -EFAULT;
		goto done;
	}

	custom_ie = kmalloc(sizeof(mlan_ds_misc_custom_ie), GFP_KERNEL);
	if (!custom_ie) {
		ret = -ENOMEM;
		goto done;
	}
	memset(custom_ie, 0, sizeof(mlan_ds_misc_custom_ie));

	if (copy_from_user
	    (custom_ie, req->ifr_data, sizeof(mlan_ds_misc_custom_ie))) {
		PRINTM(MERROR, "Copy from user failed\n");
		ret = -EFAULT;
		goto done;
	}

	ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (ioctl_req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	misc = (mlan_ds_misc_cfg *) ioctl_req->pbuf;
	misc->sub_command = MLAN_OID_MISC_CUSTOM_IE;
	ioctl_req->req_id = MLAN_IOCTL_MISC_CFG;
	if ((custom_ie->len == 0) ||
	    (custom_ie->len == sizeof(custom_ie->ie_data_list[0].ie_index)))
		ioctl_req->action = MLAN_ACT_GET;
	else
		ioctl_req->action = MLAN_ACT_SET;

	memcpy(&misc->param.cust_ie, custom_ie, sizeof(mlan_ds_misc_custom_ie));

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, ioctl_req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

	if (ioctl_req->action == MLAN_ACT_GET) {
		if (copy_to_user
		    (req->ifr_data, &misc->param.cust_ie,
		     sizeof(mlan_ds_misc_custom_ie))) {
			PRINTM(MERROR, "Copy to user failed!\n");
			ret = -EFAULT;
			goto done;
		}
	} else if (ioctl_req->status_code == MLAN_ERROR_IOCTL_FAIL) {
		/* send a separate error code to indicate error from driver */
		ret = EFAULT;
	}

done:
	if (ioctl_req)
		kfree(ioctl_req);
	if (custom_ie)
		kfree(custom_ie);
	LEAVE();
	return ret;
}

/**
 *  @brief send raw data packet ioctl function
 *
 *  @param dev      A pointer to net_device structure
 *  @param req      A pointer to ifreq structure
 *  @return         0 --success, otherwise fail
 */
int
woal_send_host_packet(struct net_device *dev, struct ifreq *req)
{
	moal_private *priv = (moal_private *) netdev_priv(dev);
	t_u32 packet_len = 0;
	int ret = 0;
	pmlan_buffer pmbuf = NULL;
	mlan_status status;

	ENTER();

	/* Sanity check */
	if (req->ifr_data == NULL) {
		PRINTM(MERROR, "woal_send_host_packet() corrupt data\n");
		ret = -EFAULT;
		goto done;
	}

	if (copy_from_user(&packet_len, req->ifr_data, sizeof(packet_len))) {
		PRINTM(MERROR, "Copy from user failed\n");
		ret = -EFAULT;
		goto done;
	}
#define PACKET_HEADER_LEN        8
	pmbuf = woal_alloc_mlan_buffer(priv->phandle,
				       MLAN_MIN_DATA_HEADER_LEN + packet_len +
				       PACKET_HEADER_LEN);
	if (!pmbuf) {
		PRINTM(MERROR, "Fail to allocate mlan_buffer\n");
		ret = -ENOMEM;
		goto done;
	}
	pmbuf->data_offset = MLAN_MIN_DATA_HEADER_LEN;

	/* get whole packet and header */
	if (copy_from_user
	    (pmbuf->pbuf + pmbuf->data_offset,
	     req->ifr_data + sizeof(packet_len),
	     PACKET_HEADER_LEN + packet_len)) {
		PRINTM(MERROR, "Copy from user failed\n");
		ret = -EFAULT;
		woal_free_mlan_buffer(priv->phandle, pmbuf);
		goto done;
	}
	pmbuf->data_len = PACKET_HEADER_LEN + packet_len;
	pmbuf->buf_type = MLAN_BUF_TYPE_RAW_DATA;
	pmbuf->bss_index = priv->bss_index;
	status = mlan_send_packet(priv->phandle->pmlan_adapter, pmbuf);
	switch (status) {
	case MLAN_STATUS_PENDING:
		atomic_inc(&priv->phandle->tx_pending);
		queue_work(priv->phandle->workqueue, &priv->phandle->main_work);
		break;
	case MLAN_STATUS_SUCCESS:
		woal_free_mlan_buffer(priv->phandle, pmbuf);
		break;
	case MLAN_STATUS_FAILURE:
	default:
		woal_free_mlan_buffer(priv->phandle, pmbuf);
		ret = -EFAULT;
		break;
	}
done:
	LEAVE();
	return ret;
}

#if defined(UAP_WEXT)
/**
 *  @brief Set/Get CUSTOM_IE ioctl handler
 *
 *  @param priv         A pointer to moal_private structure
 *  @param mask         Mask to set or clear from caller
 *  @param ie           IE buffer to set for beacon
 *  @param ie_len       Length of the IE
 *
 *  @return         0 --success, otherwise fail
 */
int
woal_set_get_custom_ie(moal_private * priv, t_u16 mask, t_u8 * ie, int ie_len)
{
	mlan_ioctl_req *ioctl_req = NULL;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ds_misc_custom_ie *misc_ie = NULL;
	int ret = 0;
	custom_ie *pcust_bcn_ie = NULL;

	ENTER();

	ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (ioctl_req == NULL) {
		LEAVE();
		return -ENOMEM;
	}

	misc = (mlan_ds_misc_cfg *) ioctl_req->pbuf;
	misc->sub_command = MLAN_OID_MISC_CUSTOM_IE;
	ioctl_req->req_id = MLAN_IOCTL_MISC_CFG;
	ioctl_req->action = MLAN_ACT_SET;
	misc_ie = &misc->param.cust_ie;

#ifndef TLV_TYPE_MGMT_IE
#define TLV_TYPE_MGMT_IE            (0x169)
#endif
	misc_ie->type = TLV_TYPE_MGMT_IE;
	misc_ie->len = (sizeof(custom_ie) - MAX_IE_SIZE) + ie_len;
	pcust_bcn_ie = misc_ie->ie_data_list;
	pcust_bcn_ie->ie_index = 0xffff;
	pcust_bcn_ie->mgmt_subtype_mask = mask;
	pcust_bcn_ie->ie_length = ie_len;
	memcpy(pcust_bcn_ie->ie_buffer, ie, ie_len);

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, ioctl_req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
	}

	if (ioctl_req)
		kfree(ioctl_req);
	LEAVE();
	return ret;
}
#endif /* defined(HOST_TXRX_MGMT_FRAME) && defined(UAP_WEXT) */

/**
 *  @brief ioctl function get BSS type
 *
 *  @param dev      A pointer to net_device structure
 *  @param req      A pointer to ifreq structure
 *  @return         0 --success, otherwise fail
 */
int
woal_get_bss_type(struct net_device *dev, struct ifreq *req)
{
	int ret = 0;
	moal_private *priv = (moal_private *) netdev_priv(dev);
	int bss_type;

	ENTER();

	bss_type = (int)priv->bss_type;
	if (copy_to_user(req->ifr_data, &bss_type, sizeof(int))) {
		PRINTM(MINFO, "Copy to user failed!\n");
		ret = -EFAULT;
	}

	LEAVE();
	return ret;
}

#if defined(STA_SUPPORT) && defined(UAP_SUPPORT)
/**
 * @brief Swithces BSS role of interface
 *
 * @param priv          A pointer to moal_private structure
 * @param action        Action: set or get
 * @param wait_option   Wait option (MOAL_WAIT or MOAL_NO_WAIT)
 * @param bss_role      A pointer to bss role
 *
 * @return         0 --success, otherwise fail
 */
mlan_status
woal_bss_role_cfg(moal_private * priv, t_u8 action,
		  t_u8 wait_option, t_u8 * bss_role)
{
	int ret = 0;
	mlan_ds_bss *bss = NULL;
	mlan_ioctl_req *req = NULL;
	struct net_device *dev = priv->netdev;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_BSS_ROLE;
	req->req_id = MLAN_IOCTL_BSS;
	req->action = action;
	if (action == MLAN_ACT_SET) {
		if (priv->bss_role == *bss_role) {
			PRINTM(MWARN, "BSS is in desired role already\n");
			goto done;
		} else {
			bss->param.bss_role = *bss_role;
		}
	}
	if (MLAN_STATUS_SUCCESS != woal_request_ioctl(priv, req, wait_option)) {
		ret = -EFAULT;
		goto done;
	}

	if (action == MLAN_ACT_GET) {
		*bss_role = bss->param.bss_role;
	} else {
		/* Update moal_private */
		priv->bss_role = *bss_role;
		if (priv->bss_type == MLAN_BSS_TYPE_UAP)
			priv->bss_type = MLAN_BSS_TYPE_STA;
		else if (priv->bss_type == MLAN_BSS_TYPE_STA)
			priv->bss_type = MLAN_BSS_TYPE_UAP;

		if (*bss_role == MLAN_BSS_ROLE_UAP) {
			/* Switch: STA -> uAP */
			/* Setup the OS Interface to our functions */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 29)
			dev->do_ioctl = woal_uap_do_ioctl;
			dev->set_multicast_list = woal_uap_set_multicast_list;
#else
			dev->netdev_ops = &woal_uap_netdev_ops;
#endif
#ifdef UAP_WEXT
			if (IS_UAP_WEXT(cfg80211_wext)) {
#if WIRELESS_EXT < 21
				dev->get_wireless_stats =
					woal_get_uap_wireless_stats;
#endif
				dev->wireless_handlers =
					(struct iw_handler_def *)
					&woal_uap_handler_def;
				init_waitqueue_head(&priv->w_stats_wait_q);
			}
#endif /* UAP_WEXT */
		} else if (*bss_role == MLAN_BSS_ROLE_STA) {
			/* Switch: uAP -> STA */
			/* Setup the OS Interface to our functions */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 29)
			dev->do_ioctl = woal_do_ioctl;
			dev->set_multicast_list = woal_set_multicast_list;
#else
			dev->netdev_ops = &woal_netdev_ops;
#endif
#ifdef STA_WEXT
			if (IS_STA_WEXT(cfg80211_wext)) {
#if WIRELESS_EXT < 21
				dev->get_wireless_stats =
					woal_get_wireless_stats;
#endif
				dev->wireless_handlers =
					(struct iw_handler_def *)
					&woal_handler_def;
				init_waitqueue_head(&priv->w_stats_wait_q);
			}
#endif /* STA_WEXT */
		}
	}

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

#if defined(STA_WEXT) || defined(UAP_WEXT)
/**
 * @brief Set/Get BSS role
 *
 * @param priv     A pointer to moal_private structure
 * @param wrq      A pointer to iwreq structure
 *
 * @return         0 --success, otherwise fail
 */
int
woal_set_get_bss_role(moal_private * priv, struct iwreq *wrq)
{
	int ret = 0;
	int bss_role = 0;
	t_u8 action = MLAN_ACT_GET;

	ENTER();

	if (wrq->u.data.length) {
		if (copy_from_user(&bss_role, wrq->u.data.pointer, sizeof(int))) {
			PRINTM(MERROR, "Copy from user failed\n");
			ret = -EFAULT;
			goto done;
		}
		if ((bss_role != MLAN_BSS_ROLE_STA &&
		     bss_role != MLAN_BSS_ROLE_UAP)
#if defined(WIFI_DIRECT_SUPPORT)
		    || (priv->bss_type != MLAN_BSS_TYPE_WIFIDIRECT)
#endif
			) {
			PRINTM(MWARN, "Invalid BSS role\n");
			ret = -EINVAL;
			goto done;
		}
		if (bss_role == GET_BSS_ROLE(priv)) {
			PRINTM(MWARN, "Already BSS is in desired role\n");
			ret = -EINVAL;
			goto done;
		}
		action = MLAN_ACT_SET;
		/* Reset interface */
		woal_reset_intf(priv, MOAL_IOCTL_WAIT, MFALSE);
	}

	if (MLAN_STATUS_SUCCESS != woal_bss_role_cfg(priv,
						     action, MOAL_IOCTL_WAIT,
						     (t_u8 *) & bss_role)) {
		ret = -EFAULT;
		goto done;
	}

	if (!wrq->u.data.length) {
		if (copy_to_user(wrq->u.data.pointer, &bss_role, sizeof(int))) {
			ret = -EFAULT;
			goto done;
		}
		wrq->u.data.length = 1;
	} else {
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext))
			woal_clear_all_mgmt_ies(priv);
#endif
		/* Initialize private structures */
		woal_init_priv(priv, MOAL_IOCTL_WAIT);

		/* Enable interfaces */
		netif_device_attach(priv->netdev);
		woal_start_queue(priv->netdev);
	}

done:
	LEAVE();
	return ret;
}
#endif /* STA_WEXT || UAP_WEXT */
#endif /* STA_SUPPORT && UAP_SUPPORT */

/**
 *  @brief Set auto arp resp
 *
 *  @param handle         A pointer to moal_handle structure
 *  @param enable         enable/disable
 *
 *  @return               MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
static mlan_status
woal_set_auto_arp(moal_handle * handle, t_u8 enable)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	int i = 0;
	moal_private *priv = NULL;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ioctl_req *req = NULL;
	mlan_ds_misc_ipaddr_cfg ipaddr_cfg;

	ENTER();

	memset(&ipaddr_cfg, 0, sizeof(ipaddr_cfg));
	for (i = 0; i < handle->priv_num && (priv = handle->priv[i]); i++) {
		if (priv->ip_addr_type != IPADDR_TYPE_NONE) {
			memcpy(ipaddr_cfg.ip_addr[ipaddr_cfg.ip_addr_num],
			       priv->ip_addr, IPADDR_LEN);
			ipaddr_cfg.ip_addr_num++;
		}
	}
	if (ipaddr_cfg.ip_addr_num == 0) {
		PRINTM(MIOCTL, "No IP addr configured.\n");
		goto done;
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		PRINTM(MIOCTL, "IOCTL req allocated failed!\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	misc = (mlan_ds_misc_cfg *) req->pbuf;
	misc->sub_command = MLAN_OID_MISC_IP_ADDR;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	req->action = MLAN_ACT_SET;
	memcpy(&misc->param.ipaddr_cfg, &ipaddr_cfg, sizeof(ipaddr_cfg));
	if (enable) {
		misc->param.ipaddr_cfg.op_code = MLAN_IPADDR_OP_ARP_FILTER |
			MLAN_IPADDR_OP_AUTO_ARP_RESP;
		misc->param.ipaddr_cfg.ip_addr_type = IPADDR_TYPE_IPV4;
	} else {
	/** remove ip */
		misc->param.ipaddr_cfg.op_code = MLAN_IPADDR_OP_IP_REMOVE;
	}
	ret = woal_request_ioctl(woal_get_priv(handle, MLAN_BSS_ROLE_ANY), req,
				 MOAL_NO_WAIT);
	if (ret != MLAN_STATUS_SUCCESS && ret != MLAN_STATUS_PENDING) {
		PRINTM(MIOCTL, "Set auto arp IOCTL failed!\n");
	}
done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Get Host Sleep parameters
 *
 *  @param priv         A pointer to moal_private structure
 *  @param action       Action: set or get
 *  @param wait_option  Wait option (MOAL_WAIT or MOAL_NO_WAIT)
 *  @param hscfg        A pointer to mlan_ds_hs_cfg structure
 *
 *  @return             MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_get_hs_params(moal_private * priv, t_u16 action, t_u8 wait_option,
		       mlan_ds_hs_cfg * hscfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_pm_cfg *pmcfg = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	pmcfg = (mlan_ds_pm_cfg *) req->pbuf;
	pmcfg->sub_command = MLAN_OID_PM_CFG_HS_CFG;
	req->req_id = MLAN_IOCTL_PM_CFG;
	req->action = action;
	if (action == MLAN_ACT_SET)
		memcpy(&pmcfg->param.hs_cfg, hscfg, sizeof(mlan_ds_hs_cfg));

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS) {
		if (hscfg && action == MLAN_ACT_GET) {
			memcpy(hscfg, &pmcfg->param.hs_cfg,
			       sizeof(mlan_ds_hs_cfg));
		}
	}
done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Cancel Host Sleep configuration
 *
 *  @param priv             A pointer to moal_private structure
 *  @param wait_option      wait option
 *
 *  @return      MLAN_STATUS_SUCCESS, MLAN_STATUS_PENDING,
 *                      or MLAN_STATUS_FAILURE
 */
mlan_status
woal_cancel_hs(moal_private * priv, t_u8 wait_option)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_hs_cfg hscfg;

	ENTER();

	/* Cancel Host Sleep */
	hscfg.conditions = HOST_SLEEP_CFG_CANCEL;
	hscfg.is_invoke_hostcmd = MTRUE;
	ret = woal_set_get_hs_params(priv, MLAN_ACT_SET, wait_option, &hscfg);

	/* remove auto arp from FW */
	woal_set_auto_arp(priv->phandle, MFALSE);

	LEAVE();
	return ret;
}

#if defined(SDIO_SUSPEND_RESUME)
/**  @brief This function enables the host sleep
 *
 *  @param priv   A Pointer to the moal_private structure
 *  @return 	  MTRUE or MFALSE
 */
int
woal_enable_hs(moal_private * priv)
{
	mlan_ds_hs_cfg hscfg;
	moal_handle *handle = NULL;
	int hs_actived = MFALSE;
	int timeout = 0;
#ifdef SDIO_SUSPEND_RESUME
	mlan_ds_ps_info pm_info;
#endif

	ENTER();

	if (priv == NULL) {
		PRINTM(MERROR, "Invalid priv\n");
		goto done;
	}
	handle = priv->phandle;
	if (handle->hs_activated == MTRUE) {
		PRINTM(MIOCTL, "HS Already actived\n");
		hs_actived = MTRUE;
		goto done;
	}
#if defined(WIFI_DIRECT_SUPPORT)
#if defined(STA_CFG80211) && defined(UAP_CFG80211)
	/* cancel pending remain on channel */
	if (priv->phandle->remain_on_channel) {
		t_u8 channel_status;
		moal_private *remain_priv =
			priv->phandle->priv[priv->phandle->remain_bss_index];
		if (remain_priv) {
			woal_cfg80211_remain_on_channel_cfg(remain_priv,
							    MOAL_NO_WAIT, MTRUE,
							    &channel_status,
							    NULL, 0, 0);
			if (priv->phandle->cookie) {
				cfg80211_remain_on_channel_expired(
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
									  remain_priv->
									  netdev,
#else
									  remain_priv->
									  wdev,
#endif
									  priv->
									  phandle->
									  cookie,
									  &priv->
									  phandle->
									  chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
									  priv->
									  phandle->
									  channel_type,
#endif
									  GFP_ATOMIC);
				priv->phandle->cookie = 0;
			}
		}
		priv->phandle->remain_on_channel = MFALSE;
	}
#endif
#endif

#ifdef STA_SUPPORT
	woal_reconfig_bgscan(priv->phandle);
#endif

	/* Set auto arp response configuration to Fw */
	woal_set_auto_arp(handle, MTRUE);
	/* Enable Host Sleep */
	handle->hs_activate_wait_q_woken = MFALSE;
	memset(&hscfg, 0, sizeof(mlan_ds_hs_cfg));
	hscfg.is_invoke_hostcmd = MTRUE;
	if (woal_set_get_hs_params(priv, MLAN_ACT_SET, MOAL_NO_WAIT, &hscfg) ==
	    MLAN_STATUS_FAILURE) {
		PRINTM(MIOCTL, "IOCTL request HS enable failed\n");
		goto done;
	}
	timeout = wait_event_interruptible_timeout(handle->hs_activate_wait_q,
						   handle->
						   hs_activate_wait_q_woken,
						   HS_ACTIVE_TIMEOUT);
	sdio_claim_host(((struct sdio_mmc_card *)handle->card)->func);
	if ((handle->hs_activated == MTRUE) || (handle->is_suspended == MTRUE)) {
		PRINTM(MCMND, "suspend success! force=%u skip=%u\n",
		       handle->hs_force_count, handle->hs_skip_count);
		hs_actived = MTRUE;
	}
#ifdef SDIO_SUSPEND_RESUME
	else {
		handle->suspend_fail = MTRUE;
		woal_get_pm_info(priv, &pm_info);
		if (pm_info.is_suspend_allowed == MTRUE) {
#ifdef MMC_PM_FUNC_SUSPENDED
			woal_wlan_is_suspended(priv->phandle);
#endif
			handle->hs_force_count++;
			PRINTM(MCMND, "suspend allowed! force=%u skip=%u\n",
			       handle->hs_force_count, handle->hs_skip_count);
			hs_actived = MTRUE;
		}
	}
#endif /* SDIO_SUSPEND_RESUME */
	sdio_release_host(((struct sdio_mmc_card *)handle->card)->func);
	if (hs_actived != MTRUE) {
		handle->hs_skip_count++;
#ifdef SDIO_SUSPEND_RESUME
		PRINTM(MCMND,
		       "suspend skipped! timeout=%d allow=%d force=%u skip=%u\n",
		       timeout, (int)pm_info.is_suspend_allowed,
		       handle->hs_force_count, handle->hs_skip_count);
#else
		PRINTM(MCMND, "suspend skipped! timeout=%d skip=%u\n",
		       timeout, handle->hs_skip_count);
#endif
		woal_cancel_hs(priv, MOAL_NO_WAIT);
	}
done:
	LEAVE();
	return hs_actived;
}
#endif

#ifdef CONFIG_PROC_FS
/**
 *  @brief This function send soft_reset command to firmware
 *
 *  @param handle   A pointer to moal_handle structure
 *  @return 	    MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING on success, otherwise failure code
 */
mlan_status
woal_request_soft_reset(moal_handle * handle)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_misc_cfg *misc = NULL;

	ENTER();
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req) {
		misc = (mlan_ds_misc_cfg *) req->pbuf;
		misc->sub_command = MLAN_OID_MISC_SOFT_RESET;
		req->req_id = MLAN_IOCTL_MISC_CFG;
		req->action = MLAN_ACT_SET;
		ret = woal_request_ioctl(woal_get_priv
					 (handle, MLAN_BSS_ROLE_ANY), req,
					 MOAL_PROC_WAIT);
	}

	handle->surprise_removed = MTRUE;
	woal_sched_timeout(5);
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}
#endif /* CONFIG_PROC_FS */

/**
 *  @brief Set wapi enable
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param enable               MTRUE or MFALSE
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_wapi_enable(moal_private * priv, t_u8 wait_option, t_u32 enable)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_WAPI_ENABLED;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_SET;
	sec->param.wapi_enabled = enable;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Get version
 *
 *  @param handle 		A pointer to moal_handle structure
 *  @param version		A pointer to version buffer
 *  @param max_len		max length of version buffer
 *
 *  @return 	   		N/A
 */
void
woal_get_version(moal_handle * handle, char *version, int max_len)
{
	union
	{
		t_u32 l;
		t_u8 c[4];
	} ver;
	char fw_ver[32];

	ENTER();

	ver.l = handle->fw_release_number;
	snprintf(fw_ver, sizeof(fw_ver), "%u.%u.%u.p%u",
		 ver.c[2], ver.c[1], ver.c[0], ver.c[3]);

	snprintf(version, max_len, handle->driver_version, fw_ver);

	LEAVE();
}

#if defined(STA_WEXT) || defined(UAP_WEXT)
/**
 *  @brief Get Driver Version
 *
 *  @param priv         A pointer to moal_private structure
 *  @param req          A pointer to ifreq structure
 *
 *  @return             0 --success, otherwise fail
 */
int
woal_get_driver_version(moal_private * priv, struct ifreq *req)
{
	struct iwreq *wrq = (struct iwreq *)req;
	int len;
	char buf[MLAN_MAX_VER_STR_LEN];
	ENTER();

	woal_get_version(priv->phandle, buf, sizeof(buf) - 1);

	len = strlen(buf);
	if (wrq->u.data.pointer) {
		if (copy_to_user(wrq->u.data.pointer, buf, len)) {
			PRINTM(MERROR, "Copy to user failed\n");
			LEAVE();
			return -EFAULT;
		}
		wrq->u.data.length = len;
	}
	PRINTM(MINFO, "MOAL VERSION: %s\n", buf);
	LEAVE();
	return 0;
}

/**
 *  @brief Get extended driver version
 *
 *  @param priv         A pointer to moal_private structure
 *  @param ireq         A pointer to ifreq structure
 *
 *  @return             0 --success, otherwise fail
 */
int
woal_get_driver_verext(moal_private * priv, struct ifreq *ireq)
{
	struct iwreq *wrq = (struct iwreq *)ireq;
	mlan_ds_get_info *info = NULL;
	mlan_ioctl_req *req = NULL;
	int ret = 0;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_get_info));
	if (req == NULL) {
		LEAVE();
		return -ENOMEM;
	}

	info = (mlan_ds_get_info *) req->pbuf;
	info->sub_command = MLAN_OID_GET_VER_EXT;
	req->req_id = MLAN_IOCTL_GET_INFO;
	req->action = MLAN_ACT_GET;

	if (!wrq->u.data.flags) {
		info->param.ver_ext.version_str_sel =
			*((int *)(wrq->u.name + SUBCMD_OFFSET));
	} else {
		if (copy_from_user
		    (&info->param.ver_ext.version_str_sel, wrq->u.data.pointer,
		     sizeof(info->param.ver_ext.version_str_sel))) {
			PRINTM(MERROR, "Copy from user failed\n");
			ret = -EFAULT;
			goto done;
		} else {
			if (((t_s32) (info->param.ver_ext.version_str_sel)) < 0) {
				PRINTM(MERROR, "Invalid arguments!\n");
				ret = -EINVAL;
				goto done;
			}
		}
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

	if (wrq->u.data.pointer) {
		if (copy_to_user
		    (wrq->u.data.pointer, info->param.ver_ext.version_str,
		     strlen(info->param.ver_ext.version_str))) {
			PRINTM(MERROR, "Copy to user failed\n");
			ret = -EFAULT;
			goto done;
		}
		wrq->u.data.length = strlen(info->param.ver_ext.version_str);
	}

	PRINTM(MINFO, "MOAL EXTENDED VERSION: %s\n",
	       info->param.ver_ext.version_str);
done:
	if (req)
		kfree(req);

	LEAVE();
	return ret;
}
#endif

#ifdef DEBUG_LEVEL1
/**
 *  @brief Set driver debug bit masks to mlan in order to enhance performance
 *
 *  @param priv         A pointer to moal_private structure
 *  @param drvdbg		Driver debug level
 *
 *  @return             0 --success, otherwise fail
 */
int
woal_set_drvdbg(moal_private * priv, t_u32 drvdbg)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_misc_cfg *misc = NULL;
	int ret = 0;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		LEAVE();
		return -ENOMEM;
	}

	misc = (mlan_ds_misc_cfg *) req->pbuf;
	misc->sub_command = MLAN_OID_MISC_DRVDBG;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	req->action = MLAN_ACT_SET;
	misc->param.drvdbg = drvdbg;

	ret = woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT);

	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);

	LEAVE();
	return ret;
}
#endif

/**
 *  @brief Mgmt frame forward registration
 *
 *  @param priv         A pointer to moal_private structure
 *  @param action       Action: set or get
 *  @param pmgmt_subtype_mask   A Pointer to mgmt frame subtype mask
 *  @param wait_option  wait option (MOAL_WAIT or MOAL_NO_WAIT)
 *
 *  @return             0 --success, otherwise fail
 */
int
woal_reg_rx_mgmt_ind(moal_private * priv, t_u16 action,
		     t_u32 * pmgmt_subtype_mask, t_u8 wait_option)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_misc_cfg *misc = NULL;
	int ret = 0;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		LEAVE();
		return -ENOMEM;
	}

	misc = (mlan_ds_misc_cfg *) req->pbuf;
	misc->sub_command = MLAN_OID_MISC_RX_MGMT_IND;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	req->action = action;
	misc->param.mgmt_subtype_mask = *pmgmt_subtype_mask;
	if (req->action == MLAN_ACT_SET)
		memcpy(&misc->param.mgmt_subtype_mask,
		       pmgmt_subtype_mask,
		       sizeof(misc->param.mgmt_subtype_mask));

	ret = woal_request_ioctl(priv, req, wait_option);

	if (req->action == MLAN_ACT_GET)
		memcpy(pmgmt_subtype_mask, &misc->param.mgmt_subtype_mask,
		       sizeof(misc->param.mgmt_subtype_mask));

	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);

	LEAVE();
	return ret;
}

/**
 *  @brief Set/Get Transmit beamforming configuration
 *
 *  @param priv         A pointer to moal_private structure
 *  @param action       Action: set or get
 *  @param tx_bf_cfg    A pointer to tx_bf_cfg structure
 *
 *  @return         MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_get_tx_bf_cfg(moal_private * priv, t_u16 action,
		       mlan_ds_11n_tx_bf_cfg * tx_bf_cfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_11n_cfg *bf_cfg = NULL;

	ENTER();

	/* Sanity test */
	if (tx_bf_cfg == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_11n_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	bf_cfg = (mlan_ds_11n_cfg *) req->pbuf;
	req->req_id = MLAN_IOCTL_11N_CFG;
	bf_cfg->sub_command = MLAN_OID_11N_CFG_TX_BF_CFG;

	req->action = action;
	memcpy(&bf_cfg->param.tx_bf, tx_bf_cfg, sizeof(mlan_ds_11n_tx_bf_cfg));

	/* Send IOCTL request to MLAN */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (action == MLAN_ACT_GET)
		memcpy(tx_bf_cfg, &bf_cfg->param.tx_bf,
		       sizeof(mlan_ds_11n_tx_bf_cfg));

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Handle ioctl resp
 *
 *  @param priv 	Pointer to moal_private structure
 *  @param req		Pointer to mlan_ioctl_req structure
 *
 *  @return    		N/A
 */
void
woal_process_ioctl_resp(moal_private * priv, mlan_ioctl_req * req)
{
	ENTER();

	if (priv == NULL) {
		LEAVE();
		return;
	}
	switch (req->req_id) {
	case MLAN_IOCTL_GET_INFO:
#ifdef STA_WEXT
#ifdef STA_SUPPORT
		if (IS_STA_WEXT(cfg80211_wext) &&
		    GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA)
			woal_ioctl_get_info_resp(priv,
						 (mlan_ds_get_info *) req->
						 pbuf);
#endif
#endif
#ifdef UAP_WEXT
#ifdef UAP_SUPPORT
		if (IS_UAP_WEXT(cfg80211_wext) &&
		    GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP)
			woal_ioctl_get_uap_info_resp(priv,
						     (mlan_ds_get_info *) req->
						     pbuf);
#endif
#endif
		break;
#ifdef STA_WEXT
#ifdef STA_SUPPORT
	case MLAN_IOCTL_BSS:
		if (IS_STA_WEXT(cfg80211_wext) &&
		    GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA)
			woal_ioctl_get_bss_resp(priv,
						(mlan_ds_bss *) req->pbuf);
		break;
#endif
#endif
	default:
		break;
	}

	LEAVE();
	return;
}

/**
 *  @brief Get PM info
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param pm_info              A pointer to mlan_ds_ps_info structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_pm_info(moal_private * priv, mlan_ds_ps_info * pm_info)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_pm_cfg *pmcfg = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		PRINTM(MERROR, "Fail to alloc mlan_ds_pm_cfg buffer\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	pmcfg = (mlan_ds_pm_cfg *) req->pbuf;
	pmcfg->sub_command = MLAN_OID_PM_INFO;
	req->req_id = MLAN_IOCTL_PM_CFG;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, MOAL_CMD_WAIT);
	if (ret == MLAN_STATUS_SUCCESS) {
		if (pm_info) {
			memcpy(pm_info, &pmcfg->param.ps_info,
			       sizeof(mlan_ds_ps_info));
		}
	}
done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Get Deep Sleep
 *
 *  @param priv      Pointer to the moal_private driver data struct
 *  @param data      Pointer to return deep_sleep setting
 *
 *  @return          0 --success, otherwise fail
 */
int
woal_get_deep_sleep(moal_private * priv, t_u32 * data)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_pm_cfg *pm = NULL;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		LEAVE();
		return -ENOMEM;
	}
	pm = (mlan_ds_pm_cfg *) req->pbuf;
	pm->sub_command = MLAN_OID_PM_CFG_DEEP_SLEEP;
	req->req_id = MLAN_IOCTL_PM_CFG;

	req->action = MLAN_ACT_GET;
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}
	*data = pm->param.auto_deep_sleep.auto_ds;
	*(data + 1) = pm->param.auto_deep_sleep.idletime;

done:
	if (req)
		kfree(req);

	LEAVE();
	return ret;
}

/**
 *  @brief Set Deep Sleep
 *
 *  @param priv         Pointer to the moal_private driver data struct
 *  @param wait_option  wait option
 *  @param bdeep_sleep  TRUE--enalbe deepsleep, FALSE--disable deepsleep
 *  @param idletime     Idle time for optimized PS API
 *
 *  @return             0 --success, otherwise fail
 */
int
woal_set_deep_sleep(moal_private * priv, t_u8 wait_option, BOOLEAN bdeep_sleep,
		    t_u16 idletime)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_pm_cfg *pm = NULL;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		LEAVE();
		return -ENOMEM;
	}
	pm = (mlan_ds_pm_cfg *) req->pbuf;
	pm->sub_command = MLAN_OID_PM_CFG_DEEP_SLEEP;
	req->req_id = MLAN_IOCTL_PM_CFG;

	req->action = MLAN_ACT_SET;
	if (bdeep_sleep == MTRUE) {
		PRINTM(MIOCTL, "Deep Sleep: sleep\n");
		pm->param.auto_deep_sleep.auto_ds = DEEP_SLEEP_ON;
		if (idletime) {
			pm->param.auto_deep_sleep.idletime = idletime;
		}
		ret = woal_request_ioctl(priv, req, wait_option);
		if (ret != MLAN_STATUS_SUCCESS && ret != MLAN_STATUS_PENDING) {
			ret = -EFAULT;
			goto done;
		}
	} else {
		PRINTM(MIOCTL, "%lu : Deep Sleep: wakeup\n", jiffies);
		pm->param.auto_deep_sleep.auto_ds = DEEP_SLEEP_OFF;
		ret = woal_request_ioctl(priv, req, wait_option);
		if (ret != MLAN_STATUS_SUCCESS && ret != MLAN_STATUS_PENDING) {
			ret = -EFAULT;
			goto done;
		}
	}
done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);

	LEAVE();
	return ret;
}

/**
 *  @brief Cancel CAC period block
 *
 *  @param priv     A pointer to moal_private structure
 *
 *  @return         N/A
 */
void
woal_cancel_cac_block(moal_private * priv)
{
	ENTER();
	/* if during CAC period, wake up wait queue */
	if (priv->phandle->cac_period == MTRUE) {
		priv->phandle->cac_period = MFALSE;
		priv->phandle->meas_start_jiffies = 0;
		if (priv->phandle->delay_bss_start == MTRUE) {
			priv->phandle->delay_bss_start = MFALSE;
		}
		if (priv->phandle->meas_wait_q_woken == MFALSE) {
			priv->phandle->meas_wait_q_woken = MTRUE;
			wake_up_interruptible(&priv->phandle->meas_wait_q);
		}
	}
	LEAVE();
}

/** MEAS report timeout value in seconds */

/**
 *  @brief Issue MLAN_OID_11H_CHANNEL_CHECK ioctl
 *
 *  @param priv     Pointer to the moal_private driver data struct
 *
 *  @return         0 --success, otherwise fail
 */
int
woal_11h_channel_check_ioctl(moal_private * priv)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_11h_cfg *ds_11hcfg = NULL;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_11h_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	ds_11hcfg = (mlan_ds_11h_cfg *) req->pbuf;

	ds_11hcfg->sub_command = MLAN_OID_11H_CHANNEL_CHECK;
	req->req_id = MLAN_IOCTL_11H_CFG;
	req->action = MLAN_ACT_SET;
	/* Send Channel Check command and wait until the report is ready */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

	/* set flag from here */
	priv->phandle->cac_period = MTRUE;
	priv->phandle->meas_start_jiffies = jiffies;

done:
	if (req)
		kfree(req);

	LEAVE();
	return ret;
}

#if defined(WIFI_DIRECT_SUPPORT)
/**
 *  @brief set/get wifi direct mode
 *
 *  @param priv            A pointer to moal_private structure
 *  @param action     	   set or get
 *  @param mode            A pointer to wifi direct mode
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_wifi_direct_mode_cfg(moal_private * priv, t_u16 action, t_u16 * mode)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_bss *bss = NULL;

	ENTER();
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_WIFI_DIRECT_MODE;
	req->req_id = MLAN_IOCTL_BSS;

	req->action = action;
	if (action == MLAN_ACT_SET)
		bss->param.wfd_mode = *mode;
	ret = woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT);
	if (ret == MLAN_STATUS_SUCCESS) {
		*mode = bss->param.wfd_mode;
		PRINTM(MIOCTL, "ACT=%d, wifi_direct_mode=%d\n", action, *mode);
	}
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief set remain channel
 *
 *  @param priv            A pointer to moal_private structure
 *  @param wait_option  Wait option
 *  @param pchan           A pointer to mlan_ds_remain_chan structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_remain_channel_ioctl(moal_private * priv, t_u8 wait_option,
			      mlan_ds_remain_chan * pchan)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_radio_cfg *radio_cfg = NULL;

	ENTER();
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_radio_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	radio_cfg = (mlan_ds_radio_cfg *) req->pbuf;
	radio_cfg->sub_command = MLAN_OID_REMAIN_CHAN_CFG;
	req->req_id = MLAN_IOCTL_RADIO_CFG;

	req->action = MLAN_ACT_SET;
	memcpy(&radio_cfg->param.remain_chan, pchan,
	       sizeof(mlan_ds_remain_chan));
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS) {
		memcpy(pchan, &radio_cfg->param.remain_chan,
		       sizeof(mlan_ds_remain_chan));
	}
done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}
#endif /* WIFI_DIRECT_SUPPORT */

#ifdef STA_SUPPORT
/**
 *  @brief Get RSSI info
 *
 *  @param priv         A pointer to moal_private structure
 *  @param wait_option  Wait option
 *  @param signal       A pointer tp mlan_ds_get_signal structure
 *
 *  @return             MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_signal_info(moal_private * priv, t_u8 wait_option,
		     mlan_ds_get_signal * signal)
{
	int ret = 0;
	mlan_ds_get_info *info = NULL;
	mlan_ioctl_req *req = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_get_info));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	info = (mlan_ds_get_info *) req->pbuf;
	info->sub_command = MLAN_OID_GET_SIGNAL;
	info->param.signal.selector = ALL_RSSI_INFO_MASK;
	req->req_id = MLAN_IOCTL_GET_INFO;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS) {
		if (signal)
			memcpy(signal, &info->param.signal,
			       sizeof(mlan_ds_get_signal));
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext)) {
			if (info->param.signal.selector & BCN_RSSI_AVG_MASK)
				priv->w_stats.qual.level =
					info->param.signal.bcn_rssi_avg;
			if (info->param.signal.selector & BCN_NF_AVG_MASK)
				priv->w_stats.qual.noise =
					info->param.signal.bcn_nf_avg;
		}
#endif
	}
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Get scan table
 *
 *  @param priv         A pointer to moal_private structure
 *  @param wait_option  Wait option
 *  @param scan_resp    A pointer to mlan_scan_resp structure
 *
 *  @return             MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_scan_table(moal_private * priv, t_u8 wait_option,
		    mlan_scan_resp * scan_resp)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_scan *scan = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	scan = (mlan_ds_scan *) req->pbuf;
	scan->sub_command = MLAN_OID_SCAN_NORMAL;
	req->req_id = MLAN_IOCTL_SCAN;
	req->action = MLAN_ACT_GET;
	memcpy((void *)&scan->param.scan_resp, (void *)scan_resp,
	       sizeof(mlan_scan_resp));

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS) {
		if (scan_resp) {
			memcpy(scan_resp, &scan->param.scan_resp,
			       sizeof(mlan_scan_resp));
		}
	}

done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Request a scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param req_ssid             A pointer to mlan_802_11_ssid structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_request_scan(moal_private * priv,
		  t_u8 wait_option, mlan_802_11_ssid * req_ssid)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_handle *handle = priv->phandle;
	mlan_ioctl_req *ioctl_req = NULL;
	mlan_ds_scan *scan = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	if (MOAL_ACQ_SEMAPHORE_BLOCK(&handle->async_sem)) {
		PRINTM(MERROR, "Acquire semaphore error, request_scan\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	handle->scan_pending_on_block = MTRUE;

	/* Allocate an IOCTL request buffer */
	ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
	if (ioctl_req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	scan = (mlan_ds_scan *) ioctl_req->pbuf;

	if (req_ssid && req_ssid->ssid_len != 0) {
		/* Specific SSID scan */
		ioctl_req->req_id = MLAN_IOCTL_SCAN;
		ioctl_req->action = MLAN_ACT_SET;

		scan->sub_command = MLAN_OID_SCAN_SPECIFIC_SSID;

		memcpy(scan->param.scan_req.scan_ssid.ssid,
		       req_ssid->ssid,
		       MIN(MLAN_MAX_SSID_LENGTH, req_ssid->ssid_len));
		scan->param.scan_req.scan_ssid.ssid_len =
			MIN(MLAN_MAX_SSID_LENGTH, req_ssid->ssid_len);
	} else {
		/* Normal scan */
		ioctl_req->req_id = MLAN_IOCTL_SCAN;
		ioctl_req->action = MLAN_ACT_SET;

		scan->sub_command = MLAN_OID_SCAN_NORMAL;
	}
	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, ioctl_req, wait_option);

	if (status == MLAN_STATUS_FAILURE) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
done:
	if ((ioctl_req) && (status != MLAN_STATUS_PENDING))
		kfree(ioctl_req);

	if (ret == MLAN_STATUS_FAILURE) {
		handle->scan_pending_on_block = MFALSE;
		MOAL_REL_SEMAPHORE(&handle->async_sem);
	}
	LEAVE();
	return ret;
}

/**
 *  @brief Change Adhoc Channel
 *
 *  @param priv 		A pointer to moal_private structure
 *  @param channel		The channel to be set.
 *
 *  @return 	   		MLAN_STATUS_SUCCESS--success, MLAN_STATUS_FAILURE--fail
 */
mlan_status
woal_change_adhoc_chan(moal_private * priv, int channel)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_bss_info bss_info;
	mlan_ds_bss *bss = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	memset(&bss_info, 0, sizeof(bss_info));

	/* Get BSS information */
	if (MLAN_STATUS_SUCCESS !=
	    woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	if (bss_info.bss_mode == MLAN_BSS_MODE_INFRA) {
		ret = MLAN_STATUS_SUCCESS;
		goto done;
	}

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Get current channel */
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_IBSS_CHANNEL;
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	if (bss->param.bss_chan.channel == (unsigned int)channel) {
		ret = MLAN_STATUS_SUCCESS;
		goto done;
	}
	PRINTM(MINFO, "Updating Channel from %d to %d\n",
	       (int)bss->param.bss_chan.channel, channel);

	if (bss_info.media_connected != MTRUE) {
		ret = MLAN_STATUS_SUCCESS;
		goto done;
	}

	/* Do disonnect */
	bss->sub_command = MLAN_OID_BSS_STOP;
	memset((t_u8 *) & bss->param.bssid, 0, ETH_ALEN);

	/* Send IOCTL request to MLAN */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Do specific SSID scanning */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_scan(priv, MOAL_IOCTL_WAIT, &bss_info.ssid)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	/* Start/Join Adhoc network */
	bss->sub_command = MLAN_OID_BSS_START;
	memset(&bss->param.ssid_bssid, 0, sizeof(mlan_ssid_bssid));
	memcpy(&bss->param.ssid_bssid.ssid, &bss_info.ssid,
	       sizeof(mlan_802_11_ssid));

	/* Send IOCTL request to MLAN */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
	}

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Find the best network to associate
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param ssid_bssid           A pointer to mlan_ssid_bssid structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_find_best_network(moal_private * priv, t_u8 wait_option,
		       mlan_ssid_bssid * ssid_bssid)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_bss *bss = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	t_u8 *mac = 0;

	ENTER();

	if (!ssid_bssid) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	bss = (mlan_ds_bss *) req->pbuf;
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_GET;
	bss->sub_command = MLAN_OID_BSS_FIND_BSS;

	memcpy(&bss->param.ssid_bssid, ssid_bssid, sizeof(mlan_ssid_bssid));

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS) {
		memcpy(ssid_bssid, &bss->param.ssid_bssid,
		       sizeof(mlan_ssid_bssid));
		mac = (t_u8 *) & ssid_bssid->bssid;
		PRINTM(MINFO, "Find network: ssid=%s, " MACSTR ", idx=%d\n",
		       ssid_bssid->ssid.ssid, MAC2STR(mac),
		       (int)ssid_bssid->idx);
	}

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Get authentication mode
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param auth_mode            A pointer to authentication mode
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_auth_mode(moal_private * priv, t_u8 wait_option, t_u32 * auth_mode)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_AUTH_MODE;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS && auth_mode) {
		*auth_mode = sec->param.auth_mode;
	}
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Get encrypt mode
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param encrypt_mode         A pointer to encrypt mode
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_encrypt_mode(moal_private * priv, t_u8 wait_option,
		      t_u32 * encrypt_mode)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_ENCRYPT_MODE;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS && encrypt_mode) {
		*encrypt_mode = sec->param.encrypt_mode;
	}
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Get WPA enable
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param enable               A pointer to wpa enable status
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_get_wpa_enable(moal_private * priv, t_u8 wait_option, t_u32 * enable)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_WPA_ENABLED;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status == MLAN_STATUS_SUCCESS && enable) {
		*enable = sec->param.wpa_enabled;
	}
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Set authentication mode
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param auth_mode            Authentication mode
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_auth_mode(moal_private * priv, t_u8 wait_option, t_u32 auth_mode)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_AUTH_MODE;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_SET;
	sec->param.auth_mode = auth_mode;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Set encrypt mode
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param encrypt_mode         Encryption mode
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_encrypt_mode(moal_private * priv, t_u8 wait_option, t_u32 encrypt_mode)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_ENCRYPT_MODE;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_SET;
	sec->param.encrypt_mode = encrypt_mode;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Set wpa enable
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param enable               MTRUE or MFALSE
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_wpa_enable(moal_private * priv, t_u8 wait_option, t_u32 enable)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_WPA_ENABLED;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_SET;
	sec->param.wpa_enabled = enable;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief enable wep key
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_enable_wep_key(moal_private * priv, t_u8 wait_option)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_ENCRYPT_KEY;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_SET;
	sec->param.encrypt_key.key_disable = MFALSE;
	sec->param.encrypt_key.key_index = MLAN_KEY_INDEX_DEFAULT;
	sec->param.encrypt_key.is_current_wep_key = MTRUE;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief Request user scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param scan_cfg             A pointer to wlan_user_scan_config structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_request_userscan(moal_private * priv,
		      t_u8 wait_option, wlan_user_scan_cfg * scan_cfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *ioctl_req = NULL;
	mlan_ds_scan *scan = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	moal_handle *handle = priv->phandle;
	ENTER();

	if (MOAL_ACQ_SEMAPHORE_BLOCK(&handle->async_sem)) {
		PRINTM(MERROR, "Acquire semaphore error, request_scan\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	handle->scan_pending_on_block = MTRUE;

	/* Allocate an IOCTL request buffer */
	ioctl_req =
		woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan) +
					  sizeof(wlan_user_scan_cfg));
	if (ioctl_req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	scan = (mlan_ds_scan *) ioctl_req->pbuf;
	scan->sub_command = MLAN_OID_SCAN_USER_CONFIG;
	ioctl_req->req_id = MLAN_IOCTL_SCAN;
	ioctl_req->action = MLAN_ACT_SET;
	memcpy(scan->param.user_scan.scan_cfg_buf, scan_cfg,
	       sizeof(wlan_user_scan_cfg));
	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, ioctl_req, wait_option);
	if (status == MLAN_STATUS_FAILURE) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

done:
	if ((ioctl_req) && (status != MLAN_STATUS_PENDING))
		kfree(ioctl_req);

	if (ret == MLAN_STATUS_FAILURE) {
		handle->scan_pending_on_block = MFALSE;
		MOAL_REL_SEMAPHORE(&handle->async_sem);
	}
	LEAVE();
	return ret;
}

/**
 *  @brief woal_get_scan_config
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param scan_cfg             A pointer to scan_cfg structure
 *
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_get_scan_config(moal_private * priv, mlan_scan_cfg * scan_cfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_scan *scan = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	scan = (mlan_ds_scan *) req->pbuf;
	scan->sub_command = MLAN_OID_SCAN_CONFIG;
	req->req_id = MLAN_IOCTL_SCAN;
	req->action = MLAN_ACT_GET;
	memset(&scan->param.scan_cfg, 0, sizeof(mlan_scan_cfg));
	if (MLAN_STATUS_SUCCESS ==
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		if (scan_cfg) {
			memcpy(scan_cfg, &scan->param.scan_cfg,
			       sizeof(mlan_scan_cfg));
		}
	} else {
		ret = MLAN_STATUS_FAILURE;
	}
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief set scan time
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param active_scan_time     Active scan time
 *  @param passive_scan_time    Passive scan time
 *  @param specific_scan_time   Specific scan time
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_scan_time(moal_private * priv, t_u16 active_scan_time,
		   t_u16 passive_scan_time, t_u16 specific_scan_time)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_scan *scan = NULL;
	mlan_ioctl_req *req = NULL;
	mlan_scan_cfg scan_cfg;

	ENTER();

	memset(&scan_cfg, 0, sizeof(scan_cfg));
	if (MLAN_STATUS_SUCCESS != woal_get_scan_config(priv, &scan_cfg)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	scan = (mlan_ds_scan *) req->pbuf;
	scan->sub_command = MLAN_OID_SCAN_CONFIG;
	req->req_id = MLAN_IOCTL_SCAN;
	req->action = MLAN_ACT_SET;
	memset(&scan->param.scan_cfg, 0, sizeof(mlan_scan_cfg));
	scan_cfg.scan_time.active_scan_time = active_scan_time;
	scan_cfg.scan_time.specific_scan_time = specific_scan_time;
	scan_cfg.scan_time.passive_scan_time = passive_scan_time;
	PRINTM(MIOCTL, "Set specific=%d, active=%d, passive=%d\n",
	       (int)active_scan_time, (int)passive_scan_time,
	       (int)specific_scan_time);
	memcpy(&scan->param.scan_cfg, &scan_cfg, sizeof(mlan_scan_cfg));
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT))
		ret = MLAN_STATUS_FAILURE;
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief request scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param scan_cfg             A pointer to wlan_user_scan_cfg structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_do_scan(moal_private * priv, wlan_user_scan_cfg * scan_cfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_handle *handle = priv->phandle;

	ENTER();
	if (handle->scan_pending_on_block == MTRUE) {
		PRINTM(MINFO, "scan already in processing...\n");
		LEAVE();
		return ret;
	}
#ifdef REASSOCIATION
	if (MOAL_ACQ_SEMAPHORE_BLOCK(&handle->reassoc_sem)) {
		PRINTM(MERROR, "Acquire semaphore error, woal_do_combo_scan\n");
		LEAVE();
		return -EBUSY;
	}
#endif /* REASSOCIATION */
	priv->report_scan_result = MTRUE;
	if (!scan_cfg) {
		ret = woal_request_scan(priv, MOAL_NO_WAIT, NULL);
	} else {
		ret = woal_request_userscan(priv, MOAL_NO_WAIT, scan_cfg);
	}
#ifdef REASSOCIATION
	MOAL_REL_SEMAPHORE(&handle->reassoc_sem);
#endif
	LEAVE();
	return ret;
}

/**
 *  @brief Cancel pending scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_cancel_scan(moal_private * priv, t_u8 wait_option)
{
	mlan_ioctl_req *req = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_handle *handle = priv->phandle;
#ifdef STA_CFG80211
	int i = 0;
#endif

	/* If scan is in process, cancel the scan command */
	if (!handle->scan_pending_on_block)
		return ret;
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	req->req_id = MLAN_IOCTL_SCAN;
	req->action = MLAN_ACT_SET;
	((mlan_ds_scan *) req->pbuf)->sub_command = MLAN_OID_SCAN_CANCEL;
	ret = woal_request_ioctl(priv, req, wait_option);
	handle->scan_pending_on_block = MFALSE;
	MOAL_REL_SEMAPHORE(&handle->async_sem);
#ifdef STA_CFG80211
	for (i = 0; i < handle->priv_num; i++) {
		if (IS_STA_CFG80211(cfg80211_wext) &&
		    handle->priv[i]->scan_request) {
			cfg80211_scan_done(handle->priv[i]->scan_request,
					   MTRUE);
			handle->priv[i]->scan_request = NULL;
		}
	}
#endif
done:
	if (req)
		kfree(req);
	return ret;
}

/**
 *  @brief find ssid in scan_table
 *
 *  @param priv         A pointer to moal_private
 *  @ssid_bssid         A pointer to mlan_ssid_bssid structure
 *  @return             MLAN_STATUS_SUCCESS/MLAN_STATUS_FAILURE
 */
int
woal_find_essid(moal_private * priv, mlan_ssid_bssid * ssid_bssid)
{
	int ret = 0;
	mlan_scan_resp scan_resp;
	struct timeval t;
	ENTER();

	if (MLAN_STATUS_SUCCESS !=
	    woal_get_scan_table(priv, MOAL_IOCTL_WAIT, &scan_resp)) {
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	do_gettimeofday(&t);
/** scan result timeout value */
#define SCAN_RESULT_AGEOUT      10
	if (t.tv_sec > (scan_resp.age_in_secs + SCAN_RESULT_AGEOUT)) {
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	ret = woal_find_best_network(priv, MOAL_IOCTL_WAIT, ssid_bssid);
	LEAVE();
	return ret;
}

/**
 *  @brief Request user scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param scan_cfg             A pointer to wlan_bgscan_cfg structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_request_bgscan(moal_private * priv,
		    t_u8 wait_option, wlan_bgscan_cfg * scan_cfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *ioctl_req = NULL;
	mlan_ds_scan *scan = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	ENTER();

	/* Allocate an IOCTL request buffer */
	ioctl_req =
		woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan) +
					  sizeof(wlan_bgscan_cfg));
	if (ioctl_req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	scan = (mlan_ds_scan *) ioctl_req->pbuf;
	scan->sub_command = MLAN_OID_SCAN_BGSCAN_CONFIG;
	ioctl_req->req_id = MLAN_IOCTL_SCAN;
	ioctl_req->action = MLAN_ACT_SET;
	memcpy(scan->param.user_scan.scan_cfg_buf, scan_cfg,
	       sizeof(wlan_bgscan_cfg));

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, ioctl_req, wait_option);
	if (status == MLAN_STATUS_FAILURE) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
done:
	if ((ioctl_req) && (status != MLAN_STATUS_PENDING))
		kfree(ioctl_req);

	LEAVE();
	return ret;
}

/**
 *  @brief set bgscan config
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param buf                  A pointer to scan command buf
 *  @param length               buf length
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_bg_scan(moal_private * priv, char *buf, int length)
{
	t_u8 *ptr = buf + strlen("BGSCAN-CONFIG") + 1;
	int buf_left = length - (strlen("BGSCAN-CONFIG") + 1);
	int band = 0;
	int num_ssid = 0;
	int ssid_len = 0;
	mlan_status ret = MLAN_STATUS_FAILURE;

	ENTER();
	memset(&priv->scan_cfg, 0, sizeof(priv->scan_cfg));
	priv->scan_cfg.report_condition = BG_SCAN_SSID_MATCH;
	while (buf_left >= 2) {
		switch (*ptr) {
		case WEXT_CSCAN_SSID_SECTION:
			ssid_len = *(ptr + 1);
			if ((buf_left < (ssid_len + 2)) ||
			    (ssid_len > MLAN_MAX_SSID_LENGTH)) {
				PRINTM(MERROR,
				       "Invalid ssid, buf_left=%d, ssid_len=%d\n",
				       buf_left, ssid_len);
				buf_left = 0;
				break;
			}
			if (ssid_len &&
			    (num_ssid < (MRVDRV_MAX_SSID_LIST_LENGTH - 1))) {
				strncpy(priv->scan_cfg.ssid_list[num_ssid].ssid,
					ptr + 2, ssid_len);
				priv->scan_cfg.ssid_list[num_ssid].max_len = 0;
				PRINTM(MIOCTL, "BG scan: ssid=%s\n",
				       priv->scan_cfg.ssid_list[num_ssid].ssid);
				num_ssid++;
			}
			buf_left -= ssid_len + 2;
			ptr += ssid_len + 2;
			break;
		case WEXT_BGSCAN_RSSI_SECTION:
			priv->scan_cfg.report_condition =
				BG_SCAN_SSID_RSSI_MATCH;
			priv->scan_cfg.rssi_threshold = ptr[1];
			PRINTM(MIOCTL, "BG scan: rssi_threshold=%d\n",
			       (int)priv->scan_cfg.rssi_threshold);
			ptr += 2;
			buf_left -= 2;
			break;
		case WEXT_BGSCAN_REPEAT_SECTION:
			priv->scan_cfg.repeat_count = (t_u16) ptr[1];
			PRINTM(MIOCTL, "BG scan: repeat_count=%d\n",
			       (int)priv->scan_cfg.repeat_count);
			ptr += 2;
			buf_left -= 2;
			break;
		case WEXT_BGSCAN_INTERVAL_SECTION:
			priv->scan_cfg.scan_interval =
				(ptr[2] << 8 | ptr[1]) * 1000;
			PRINTM(MIOCTL, "BG scan: scan_interval=%d\n",
			       (int)priv->scan_cfg.scan_interval);
			ptr += 3;
			buf_left -= 3;
			break;
		default:
			buf_left = 0;
			break;
		}
	}
    /** set bgscan when ssid_num > 0 */
	if (num_ssid) {
		if (MLAN_STATUS_SUCCESS != woal_get_band(priv, &band)) {
			ret = MLAN_STATUS_FAILURE;
			goto done;
		}
		switch (band) {
		case WIFI_FREQUENCY_BAND_2GHZ:
			priv->scan_cfg.chan_list[0].radio_type =
				0 | BAND_SPECIFIED;
			break;
		case WIFI_FREQUENCY_BAND_5GHZ:
			priv->scan_cfg.chan_list[0].radio_type =
				1 | BAND_SPECIFIED;
			break;
		default:
			break;
		}
		priv->scan_cfg.bss_type = MLAN_BSS_MODE_INFRA;
		priv->scan_cfg.action = BG_SCAN_ACT_SET;
		priv->scan_cfg.enable = MTRUE;
		ret = woal_request_bgscan(priv, MOAL_IOCTL_WAIT,
					  &priv->scan_cfg);
	}
done:
	LEAVE();
	return ret;
}

#ifdef STA_CFG80211
/**
 *  @brief set bgscan and new rssi_low_threshold
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param set_rssi             flag for set rssi_low_threshold
 *
 *  @return                     N/A
 */
void
woal_config_bgscan_and_rssi(moal_private * priv, t_u8 set_rssi)
{
	char rssi_low[10];
	mlan_bss_info bss_info;
	int band = 0;

	ENTER();
	memset(&bss_info, 0, sizeof(bss_info));
	woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info);
	if (!bss_info.media_connected) {
		PRINTM(MIOCTL, "We already lost connection\n");
		LEAVE();
		return;
	}
	memset(&priv->scan_cfg, 0, sizeof(priv->scan_cfg));
	strncpy(priv->scan_cfg.ssid_list[0].ssid, bss_info.ssid.ssid,
		bss_info.ssid.ssid_len);
	priv->scan_cfg.ssid_list[0].max_len = 0;

	priv->scan_cfg.report_condition = BG_SCAN_SSID_RSSI_MATCH;
	priv->scan_cfg.rssi_threshold = priv->rssi_low - RSSI_HYSTERESIS;
	priv->scan_cfg.repeat_count = DEF_REPEAT_COUNT;
	priv->scan_cfg.scan_interval = MIN_BGSCAN_INTERVAL;
	woal_get_band(priv, &band);
	switch (band) {
	case WIFI_FREQUENCY_BAND_2GHZ:
		priv->scan_cfg.chan_list[0].radio_type = 0 | BAND_SPECIFIED;
		break;
	case WIFI_FREQUENCY_BAND_5GHZ:
		priv->scan_cfg.chan_list[0].radio_type = 1 | BAND_SPECIFIED;
		break;
	default:
		break;
	}
	priv->scan_cfg.bss_type = MLAN_BSS_MODE_INFRA;
	priv->scan_cfg.action = BG_SCAN_ACT_SET;
	priv->scan_cfg.enable = MTRUE;
	woal_request_bgscan(priv, MOAL_NO_WAIT, &priv->scan_cfg);
	if (set_rssi &&
	    ((priv->rssi_low + RSSI_HYSTERESIS) <= LOWEST_RSSI_THRESHOLD)) {
		priv->rssi_low += RSSI_HYSTERESIS;
		sprintf(rssi_low, "%d", priv->rssi_low);
		woal_set_rssi_low_threshold(priv, rssi_low, MOAL_NO_WAIT);
	}
	LEAVE();
}
#endif

/**
 *  @brief stop bg scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          wait option
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_stop_bg_scan(moal_private * priv, t_u8 wait_option)
{
	wlan_bgscan_cfg scan_cfg;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	ENTER();

	memset(&scan_cfg, 0, sizeof(scan_cfg));
	scan_cfg.action = BG_SCAN_ACT_SET;
	scan_cfg.enable = MFALSE;
	ret = woal_request_bgscan(priv, wait_option, &scan_cfg);

	LEAVE();
	return ret;
}

/**
 *  @brief set bgscan config
 *
 *  @param handle               A pointer to moal_handle structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
void
woal_reconfig_bgscan(moal_handle * handle)
{
	int i;
	for (i = 0; i < MIN(handle->priv_num, MLAN_MAX_BSS_NUM); i++) {
		if (handle->priv[i] &&
		    (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_STA)) {
			if (handle->priv[i]->bg_scan_start &&
			    handle->priv[i]->bg_scan_reported) {
				PRINTM(MIOCTL, "Reconfig BGSCAN\n");
				woal_request_bgscan(handle->priv[i],
						    MOAL_NO_WAIT,
						    &handle->priv[i]->scan_cfg);
				handle->priv[i]->bg_scan_reported = MFALSE;
			}
		}
	}
}

/**
 *  @brief set rssi low threshold
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param rssi 	            A pointer to low rssi
 *  @param wait_option          Wait option
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_rssi_low_threshold(moal_private * priv, char *rssi, t_u8 wait_option)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_misc_cfg *misc = NULL;
	int low_rssi = 0;

	ENTER();
	if (priv->media_connected == MFALSE)
		goto done;

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	misc = (mlan_ds_misc_cfg *) req->pbuf;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	misc->sub_command = MLAN_OID_MISC_SUBSCRIBE_EVENT;
	req->action = MLAN_ACT_SET;
	misc->param.subscribe_event.evt_action = SUBSCRIBE_EVT_ACT_BITWISE_SET;
	misc->param.subscribe_event.evt_bitmap = SUBSCRIBE_EVT_RSSI_LOW;
	misc->param.subscribe_event.evt_bitmap |= SUBSCRIBE_EVT_PRE_BEACON_LOST;
	misc->param.subscribe_event.pre_beacon_miss = DEFAULT_PRE_BEACON_MISS;

	if (MLAN_STATUS_SUCCESS != woal_atoi(&low_rssi, rssi)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
#ifdef STA_CFG80211
	priv->mrvl_rssi_low = low_rssi;
#endif
	misc->param.subscribe_event.low_rssi = low_rssi;
	misc->param.subscribe_event.low_rssi_freq = 0;
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_FAILURE) {
		PRINTM(MERROR, "request set rssi_low_threshold fail!\n");
		goto done;
	}
done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

#ifdef STA_CFG80211
/**
 *  @brief set rssi low threshold
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param event_id				event id.
 *  @param wait_option 			wait option
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_rssi_threshold(moal_private * priv, t_u32 event_id, t_u8 wait_option)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_misc_cfg *misc = NULL;

	ENTER();
	if (priv->media_connected == MFALSE)
		goto done;
	if (priv->mrvl_rssi_low)
		goto done;
	if (event_id == MLAN_EVENT_ID_FW_BCN_RSSI_LOW) {
		if (priv->last_rssi_low < 100)
			priv->last_rssi_low += priv->cqm_rssi_hyst;
		priv->last_rssi_high = abs(priv->cqm_rssi_thold);
	} else if (event_id == MLAN_EVENT_ID_FW_BCN_RSSI_HIGH) {
		priv->last_rssi_low = abs(priv->cqm_rssi_thold);
		if (priv->last_rssi_high > priv->cqm_rssi_hyst)
			priv->last_rssi_high -= priv->cqm_rssi_hyst;
	} else {
		priv->last_rssi_low = abs(priv->cqm_rssi_thold);
		priv->last_rssi_high = abs(priv->cqm_rssi_thold);
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	misc = (mlan_ds_misc_cfg *) req->pbuf;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	misc->sub_command = MLAN_OID_MISC_SUBSCRIBE_EVENT;
	req->action = MLAN_ACT_SET;
	if (!event_id && !priv->cqm_rssi_thold && !priv->cqm_rssi_hyst)
		misc->param.subscribe_event.evt_action =
			SUBSCRIBE_EVT_ACT_BITWISE_CLR;
	else
		misc->param.subscribe_event.evt_action =
			SUBSCRIBE_EVT_ACT_BITWISE_SET;
	misc->param.subscribe_event.evt_bitmap =
		SUBSCRIBE_EVT_RSSI_LOW | SUBSCRIBE_EVT_RSSI_HIGH;
	misc->param.subscribe_event.low_rssi_freq = 0;
	misc->param.subscribe_event.low_rssi = priv->last_rssi_low;
	misc->param.subscribe_event.high_rssi_freq = 0;
	misc->param.subscribe_event.high_rssi = priv->last_rssi_high;
	PRINTM(MIOCTL, "rssi_low=%d, rssi_high=%d action=%d\n",
	       (int)priv->last_rssi_low, (int)priv->last_rssi_high,
	       misc->param.subscribe_event.evt_action);
	ret = woal_request_ioctl(priv, req, wait_option);
done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}
#endif

/**
 *  @brief  Get power mode
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param powermode            A pointer to powermode buf
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_get_powermode(moal_private * priv, int *powermode)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	int ps_mode;

	ENTER();

	if (MLAN_STATUS_SUCCESS !=
	    woal_set_get_power_mgmt(priv, MLAN_ACT_GET, &ps_mode, 0)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (ps_mode)
		*powermode = MFALSE;
	else
		*powermode = MTRUE;

done:
	LEAVE();
	return ret;
}

/**
 *  @brief set scan type
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param scan_type            MLAN_SCAN_TYPE_ACTIVE/MLAN_SCAN_TYPE_PASSIVE
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_scan_type(moal_private * priv, t_u32 scan_type)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_scan *scan = NULL;
	mlan_ioctl_req *req = NULL;
	mlan_scan_cfg scan_cfg;

	ENTER();
	memset(&scan_cfg, 0, sizeof(scan_cfg));
	if (MLAN_STATUS_SUCCESS != woal_get_scan_config(priv, &scan_cfg)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	scan = (mlan_ds_scan *) req->pbuf;
	scan->sub_command = MLAN_OID_SCAN_CONFIG;
	req->req_id = MLAN_IOCTL_SCAN;
	req->action = MLAN_ACT_SET;
	memset(&scan->param.scan_cfg, 0, sizeof(mlan_scan_cfg));
	scan_cfg.scan_type = scan_type;
	PRINTM(MIOCTL, "Set scan_type=%d\n", (int)scan_type);
	memcpy(&scan->param.scan_cfg, &scan_cfg, sizeof(mlan_scan_cfg));
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT))
		ret = MLAN_STATUS_FAILURE;
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief enable/disable ext_scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param enable               MTRUE -- enable, MFALSE --disable
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_enable_ext_scan(moal_private * priv, t_u8 enable)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_scan *scan = NULL;
	mlan_ioctl_req *req = NULL;
	mlan_scan_cfg scan_cfg;

	ENTER();
	memset(&scan_cfg, 0, sizeof(scan_cfg));
	if (MLAN_STATUS_SUCCESS != woal_get_scan_config(priv, &scan_cfg)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	scan = (mlan_ds_scan *) req->pbuf;
	scan->sub_command = MLAN_OID_SCAN_CONFIG;
	req->req_id = MLAN_IOCTL_SCAN;
	req->action = MLAN_ACT_SET;
	memset(&scan->param.scan_cfg, 0, sizeof(mlan_scan_cfg));
	scan_cfg.ext_scan = enable;
	PRINTM(MIOCTL, "Set ext_scan=%d\n", (int)enable);
	memcpy(&scan->param.scan_cfg, &scan_cfg, sizeof(mlan_scan_cfg));
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT))
		ret = MLAN_STATUS_FAILURE;
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief set power mode
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param powermode            A pointer to powermode string.
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_powermode(moal_private * priv, char *powermode)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	int disabled;

	ENTER();

	if (*powermode == '1') {
		PRINTM(MIOCTL, "Disable power save\n");
		disabled = 1;
	} else if (*powermode == '0') {
		PRINTM(MIOCTL, "Enable power save\n");
		disabled = 0;
	} else {
		PRINTM(MERROR, "unsupported power mode\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_set_get_power_mgmt(priv, MLAN_ACT_SET, &disabled, 0))
		ret = MLAN_STATUS_FAILURE;

done:
	LEAVE();
	return ret;
}

/**
 *  @brief set combo scan
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param buf                  A pointer to scan command buf
 *  @param length               buf length
 *
 *  @return                     0 -- success, otherwise fail
 */
int
woal_set_combo_scan(moal_private * priv, char *buf, int length)
{
	int ret = 0;
	wlan_user_scan_cfg scan_cfg;
	t_u8 *ptr = buf + WEXT_CSCAN_HEADER_SIZE;
	int buf_left = length - WEXT_CSCAN_HEADER_SIZE;
	int num_ssid = 0;
	int num_chan = 0;
	int ssid_len = 0;
	int i = 0;
	t_u16 passive_scan_time = 0;
	t_u16 specific_scan_time = 0;

	ENTER();
	memset(&scan_cfg, 0, sizeof(scan_cfg));
	while (buf_left >= 2) {
		switch (*ptr) {
		case WEXT_CSCAN_SSID_SECTION:
			ssid_len = *(ptr + 1);
			if ((buf_left < (ssid_len + 2)) ||
			    (ssid_len > MLAN_MAX_SSID_LENGTH)) {
				PRINTM(MERROR,
				       "Invalid ssid, buf_left=%d, ssid_len=%d\n",
				       buf_left, ssid_len);
				buf_left = 0;
				break;
			}
			if (ssid_len &&
			    (num_ssid < (MRVDRV_MAX_SSID_LIST_LENGTH - 1))) {
				strncpy(scan_cfg.ssid_list[num_ssid].ssid,
					ptr + 2, ssid_len);
				scan_cfg.ssid_list[num_ssid].max_len = 0;
				PRINTM(MIOCTL, "Combo scan: ssid=%s\n",
				       scan_cfg.ssid_list[num_ssid].ssid);
				num_ssid++;
			}
			buf_left -= ssid_len + 2;
			ptr += ssid_len + 2;
			break;
		case WEXT_CSCAN_CHANNEL_SECTION:
			num_chan = ptr[1];
			if ((buf_left < (num_chan + 2)) ||
			    (num_chan > WLAN_USER_SCAN_CHAN_MAX)) {
				PRINTM(MERROR,
				       "Invalid channel list, buf_left=%d, num_chan=%d\n",
				       buf_left, num_chan);
				buf_left = 0;
				break;
			}
			for (i = 0; i < num_chan; i++) {
				scan_cfg.chan_list[i].chan_number = ptr[2 + i];
				PRINTM(MIOCTL, "Combo scan: chan=%d\n",
				       scan_cfg.chan_list[i].chan_number);
			}
			buf_left -= 2 + num_chan;
			ptr += 2 + num_chan;
			break;
		case WEXT_CSCAN_PASV_DWELL_SECTION:
			if (buf_left < 3) {
				PRINTM(MERROR,
				       "Invalid PASV_DWELL_SECTION, buf_left=%d\n",
				       buf_left);
				buf_left = 0;
				break;
			}
			passive_scan_time = ptr[2] << 8 | ptr[1];
			ptr += 3;
			buf_left -= 3;
			break;
		case WEXT_CSCAN_HOME_DWELL_SECTION:
			if (buf_left < 3) {
				PRINTM(MERROR,
				       "Invalid HOME_DWELL_SECTION, buf_left=%d\n",
				       buf_left);
				buf_left = 0;
				break;
			}
			specific_scan_time = ptr[2] << 8 | ptr[1];
			ptr += 3;
			buf_left -= 3;
			break;
		default:
			buf_left = 0;
			break;
		}
	}
	if (passive_scan_time || specific_scan_time) {
		PRINTM(MIOCTL,
		       "Set passive_scan_time=%d specific_scan_time=%d\n",
		       passive_scan_time, specific_scan_time);
		if (MLAN_STATUS_FAILURE ==
		    woal_set_scan_time(priv, 0, passive_scan_time,
				       specific_scan_time)) {
			ret = -EFAULT;
			goto done;
		}
	}
	if (num_ssid || num_chan) {
		if (num_ssid) {
			/* Add broadcast scan to ssid_list */
			scan_cfg.ssid_list[num_ssid].max_len = 0xff;
			if (priv->scan_type == MLAN_SCAN_TYPE_PASSIVE)
				woal_set_scan_type(priv, MLAN_SCAN_TYPE_ACTIVE);
		}
		if (MLAN_STATUS_FAILURE == woal_do_scan(priv, &scan_cfg))
			ret = -EFAULT;
		if (num_ssid && (priv->scan_type == MLAN_SCAN_TYPE_PASSIVE))
			woal_set_scan_type(priv, MLAN_SCAN_TYPE_PASSIVE);
	} else {
		/* request broadcast scan */
		if (MLAN_STATUS_FAILURE == woal_do_scan(priv, NULL))
			ret = -EFAULT;
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief  Get band
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param band                 A pointer to band buf
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_get_band(moal_private * priv, int *band)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ioctl_req *req = NULL;
	mlan_ds_radio_cfg *radio_cfg = NULL;
	int support_band = 0;

	ENTER();

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_radio_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	radio_cfg = (mlan_ds_radio_cfg *) req->pbuf;
	radio_cfg->sub_command = MLAN_OID_BAND_CFG;
	req->req_id = MLAN_IOCTL_RADIO_CFG;
	/* Get config_bands, adhoc_start_band and adhoc_channel values from
	   MLAN */
	req->action = MLAN_ACT_GET;
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	if (radio_cfg->param.band_cfg.
	    config_bands & (BAND_B | BAND_G | BAND_GN))
		support_band |= WIFI_FREQUENCY_BAND_2GHZ;
	if (radio_cfg->param.band_cfg.config_bands & (BAND_A | BAND_AN))
		support_band |= WIFI_FREQUENCY_BAND_5GHZ;
	*band = support_band;
	if (support_band == WIFI_FREQUENCY_ALL_BAND)
		*band = WIFI_FREQUENCY_BAND_AUTO;
done:
	if (req)
		kfree(req);

	LEAVE();
	return ret;
}

/**
 *  @brief set band
 *
 *  @param priv            A pointer to moal_private structure
 *  @param pband            A pointer to band string.
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_set_band(moal_private * priv, char *pband)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	int band = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_radio_cfg *radio_cfg = NULL;

	ENTER();
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_radio_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	radio_cfg = (mlan_ds_radio_cfg *) req->pbuf;
	radio_cfg->sub_command = MLAN_OID_BAND_CFG;
	req->req_id = MLAN_IOCTL_RADIO_CFG;

	/* Get fw supported values from MLAN */
	req->action = MLAN_ACT_GET;
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	if (*pband == '0') {
		PRINTM(MIOCTL, "Set band to AUTO\n");
		band = radio_cfg->param.band_cfg.fw_bands;
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext) && priv->wdev &&
		    priv->wdev->wiphy) {
			if (radio_cfg->param.band_cfg.fw_bands & BAND_A)
				priv->wdev->wiphy->bands[IEEE80211_BAND_5GHZ] =
					&cfg80211_band_5ghz;
			priv->wdev->wiphy->bands[IEEE80211_BAND_2GHZ] =
				&cfg80211_band_2ghz;
		}
#endif
	} else if (*pband == '1') {
		PRINTM(MIOCTL, "Set band to 5G\n");
		if (!(radio_cfg->param.band_cfg.fw_bands & BAND_A)) {
			PRINTM(MERROR, "Don't support 5G band\n");
			ret = MLAN_STATUS_FAILURE;
			goto done;
		}
		band = BAND_A;
		band |= BAND_AN;
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext) && priv->wdev &&
		    priv->wdev->wiphy) {
			priv->wdev->wiphy->bands[IEEE80211_BAND_5GHZ] =
				&cfg80211_band_5ghz;
			priv->wdev->wiphy->bands[IEEE80211_BAND_2GHZ] = NULL;
		}
#endif
	} else if (*pband == '2') {
		PRINTM(MIOCTL, "Set band to 2G\n");
		band = BAND_B | BAND_G;
		band |= BAND_GN;
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext) && priv->wdev &&
		    priv->wdev->wiphy) {
			priv->wdev->wiphy->bands[IEEE80211_BAND_5GHZ] = NULL;
			priv->wdev->wiphy->bands[IEEE80211_BAND_2GHZ] =
				&cfg80211_band_2ghz;
		}
#endif
	} else {
		PRINTM(MERROR, "unsupported band\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	/* Set config_bands to MLAN */
	req->action = MLAN_ACT_SET;
	memset(&radio_cfg->param.band_cfg, 0, sizeof(mlan_ds_band_cfg));
	radio_cfg->param.band_cfg.config_bands = band;
	radio_cfg->param.band_cfg.adhoc_start_band = band;
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 *  @brief Add RX Filter
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param rxfilter             A pointer to rxfilter string.
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_add_rxfilter(moal_private * priv, char *rxfilter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	ENTER();
	/* Android command: "DRIVER RXFILTER-ADD 0" "DRIVER RXFILTER-ADD 1"
	   "DRIVER RXFILTER-ADD 3" */
	if (*rxfilter == '0') {
		PRINTM(MIOCTL, "Add IPV4 multicast filter\n");
		priv->rx_filter |= RX_FILTER_IPV4_MULTICAST;
	} else if (*rxfilter == '1') {
		PRINTM(MIOCTL, "Add broadcast filter\n");
		priv->rx_filter |= RX_FILTER_BROADCAST;
	} else if (*rxfilter == '2') {
		PRINTM(MIOCTL, "Add unicast filter\n");
		priv->rx_filter |= RX_FILTER_UNICAST;
	} else if (*rxfilter == '3') {
		PRINTM(MIOCTL, "Add IPV6 multicast fitler\n");
		priv->rx_filter |= RX_FILTER_IPV6_MULTICAST;
	} else {
		PRINTM(MERROR, "unsupported rx fitler\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief Remove RX Filter
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param rxfilter             A pointer to rxfilter string.
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_remove_rxfilter(moal_private * priv, char *rxfilter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	ENTER();
	if (*rxfilter == '0') {
		PRINTM(MIOCTL, "Remove IPV4 multicast filter\n");
		priv->rx_filter &= ~RX_FILTER_IPV4_MULTICAST;
	} else if (*rxfilter == '1') {
		PRINTM(MIOCTL, "Remove broadcast filter\n");
		priv->rx_filter &= ~RX_FILTER_BROADCAST;
	} else if (*rxfilter == '2') {
		PRINTM(MIOCTL, "Remove unicast filter\n");
		priv->rx_filter &= ~RX_FILTER_UNICAST;
	} else if (*rxfilter == '3') {
		PRINTM(MIOCTL, "Remove IPV6 multicast fitler\n");
		priv->rx_filter &= ~RX_FILTER_IPV6_MULTICAST;
	} else {
		PRINTM(MERROR, "unsupported rx fitler\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
done:
	LEAVE();
	return ret;
}

/**
 * @brief Set/Get WMM IE QoS configuration
 *
 * @param priv     A pointer to moal_private structure
 *  @param action  Action set or get
 * @param qos_cfg  A pointer to QoS configuration structure
 *
 * @return         MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_priv_qos_cfg(moal_private * priv, t_u32 action, char *qos_cfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_wmm_cfg *cfg = NULL;
	mlan_ioctl_req *req = NULL;
	int qosinfo = 0;

	ENTER();

	if (qos_cfg == NULL) {
		PRINTM(MERROR, "QOS info buffer is null\n");
		return MLAN_STATUS_FAILURE;
	}
	if ((action == MLAN_ACT_SET) &&
	    (MLAN_STATUS_SUCCESS != woal_atoi(&qosinfo, qos_cfg))) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_wmm_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	cfg = (mlan_ds_wmm_cfg *) req->pbuf;
	cfg->sub_command = MLAN_OID_WMM_CFG_QOS;
	req->req_id = MLAN_IOCTL_WMM_CFG;
	req->action = action;
	if (action == MLAN_ACT_SET) {
		cfg->param.qos_cfg = (t_u8) qosinfo;
		PRINTM(MIOCTL, "set qosinfo=%d\n", qosinfo);
	}
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT))
		ret = MLAN_STATUS_FAILURE;
	if (action == MLAN_ACT_GET)
		*qos_cfg = cfg->param.qos_cfg;
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 * @brief Set sleep period
 *
 * @param priv     A pointer to moal_private structure
 * @param psleeppd A pointer to sleep period configuration structure
 *
 * @return         MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
int
woal_set_sleeppd(moal_private * priv, char *psleeppd)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_pm_cfg *pm_cfg = NULL;
	mlan_ioctl_req *req = NULL;
	int sleeppd = 0;

	ENTER();

	if (MLAN_STATUS_SUCCESS != woal_atoi(&sleeppd, psleeppd)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	PRINTM(MIOCTL, "set sleeppd=%d\n", sleeppd);
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	pm_cfg = (mlan_ds_pm_cfg *) req->pbuf;
	pm_cfg->sub_command = MLAN_OID_PM_CFG_SLEEP_PD;
	req->req_id = MLAN_IOCTL_PM_CFG;
	if ((sleeppd <= MAX_SLEEP_PERIOD && sleeppd >= MIN_SLEEP_PERIOD) ||
	    (sleeppd == 0)
	    || (sleeppd == SLEEP_PERIOD_RESERVED_FF)
		) {
		req->action = MLAN_ACT_SET;
		pm_cfg->param.sleep_period = sleeppd;
	} else {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 * @brief  Set scan period function
 *
 * @param priv     A pointer to moal_private structure
 *  @param buf                  A pointer to scan command buf
 *  @param length               buf length
 *
 * @return         MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
int
woal_set_scan_cfg(moal_private * priv, char *buf, int length)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	t_u8 *ptr = buf + NL80211_SCANCFG_HEADER_SIZE;
	int buf_left = length - NL80211_SCANCFG_HEADER_SIZE;
	t_u16 active_scan_time = 0;
	t_u16 passive_scan_time = 0;
	t_u16 specific_scan_time = 0;

	ENTER();
	while (buf_left >= 2) {
		switch (*ptr) {
		case NL80211_SCANCFG_ACTV_DWELL_SECTION:
			if (buf_left < 3) {
				PRINTM(MERROR,
				       "Invalid ACTV_DWELL_SECTION, buf_left=%d\n",
				       buf_left);
				buf_left = 0;
				break;
			}
			active_scan_time = ptr[2] << 8 | ptr[1];
			ptr += 3;
			buf_left -= 3;
			break;
		case NL80211_SCANCFG_PASV_DWELL_SECTION:
			if (buf_left < 3) {
				PRINTM(MERROR,
				       "Invalid PASV_DWELL_SECTION, buf_left=%d\n",
				       buf_left);
				buf_left = 0;
				break;
			}
			passive_scan_time = ptr[2] << 8 | ptr[1];
			ptr += 3;
			buf_left -= 3;
			break;
		case NL80211_SCANCFG_SPCF_DWELL_SECTION:
			if (buf_left < 3) {
				PRINTM(MERROR,
				       "Invalid SPCF_DWELL_SECTION, buf_left=%d\n",
				       buf_left);
				buf_left = 0;
				break;
			}
			specific_scan_time = ptr[2] << 8 | ptr[1];
			ptr += 3;
			buf_left -= 3;
			break;
		default:
			buf_left = 0;
			break;
		}
	}

	if (active_scan_time || passive_scan_time || specific_scan_time) {
		PRINTM(MIOCTL, "Set active_scan_time= %d passive_scan_time=%d "
		       "specific_scan_time=%d\n", active_scan_time,
		       passive_scan_time, specific_scan_time);
		if (MLAN_STATUS_FAILURE == woal_set_scan_time(priv,
							      active_scan_time,
							      passive_scan_time,
							      specific_scan_time))
		{
			ret = -EFAULT;
		}
	}

	LEAVE();
	return ret;
}

#endif /* STA_SUPPORT */
