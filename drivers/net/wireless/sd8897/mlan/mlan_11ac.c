/** @file mlan_11ac.c
 *
 *  @brief This file contains the functions for station ioctl.
 *
 *  Copyright (C) 2011-2012, Marvell International Ltd.
 *
 *  This software file (the "File") is distributed by Marvell International
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991
 *  (the "License").  You may use, redistribute and/or modify this File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available by writing to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 *  worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
 */

#include "mlan.h"
#include "mlan_join.h"
#include "mlan_util.h"
#include "mlan_fw.h"
#include "mlan_main.h"
#include "mlan_wmm.h"
#include "mlan_11n.h"

/********************************************************
    Local Variables
********************************************************/

/********************************************************
    Global Variables
********************************************************/

/********************************************************
    Local Functions
********************************************************/

/**
 *  @brief determine the center frquency center index for bandwidth
 *         of 80 MHz and 160 MHz
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param band         band
 *  @param pri_chan     primary channel
 *  @param chan_bw      channel bandwidth
 *
 *  @return             channel center frequency center, if found; O, otherwise
 */

t_u8
wlan_get_center_freq_idx(IN mlan_private * pmpriv,
			 IN t_u8 band, IN t_u32 pri_chan, IN t_u8 chan_bw)
{
	t_u8 center_freq_idx = 0;

	if (band & BAND_AAC) {
		switch (pri_chan) {
		case 36:
		case 40:
		case 44:
		case 48:
			if (chan_bw == CHANNEL_BW_80MHZ) {
				center_freq_idx = 42;
				break;
			}
		case 52:
		case 56:
		case 60:
		case 64:
			if (chan_bw == CHANNEL_BW_80MHZ) {
				center_freq_idx = 58;
				break;
			} else if (chan_bw == CHANNEL_BW_160MHZ) {
				center_freq_idx = 50;
				break;
			}
		case 100:
		case 104:
		case 108:
		case 112:
			if (chan_bw == CHANNEL_BW_80MHZ) {
				center_freq_idx = 106;
				break;
			}
		case 116:
		case 120:
		case 124:
		case 128:
			if (chan_bw == CHANNEL_BW_80MHZ) {
				center_freq_idx = 122;
				break;
			} else if (chan_bw == CHANNEL_BW_160MHZ) {
				center_freq_idx = 114;
				break;
			}
		case 132:
		case 136:
		case 140:
		case 144:
			if (chan_bw == CHANNEL_BW_80MHZ) {
				center_freq_idx = 138;
				break;
			}
		case 149:
		case 153:
		case 157:
		case 161:
			if (chan_bw == CHANNEL_BW_80MHZ) {
				center_freq_idx = 155;
				break;
			}
		default:	/* error. go to the default */
			center_freq_idx = 42;
		}
	}
	return center_freq_idx;
}

/**
 *  @brief This function fills the cap info
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param pht_cap      A pointer to MrvlIETypes_HTCap_t structure
 *  @param bands        Band configuration
 *
 *  @return             N/A
 */
static void
wlan_fill_cap_info(mlan_private * priv,
		   MrvlIETypes_VHTCap_t * pvht_cap, t_u8 bands)
{
	mlan_adapter *pmadapter = priv->adapter;
	t_u32 usr_dot_11ac_dev_cap;

	ENTER();

	if (bands & BAND_A)
		usr_dot_11ac_dev_cap = pmadapter->usr_dot_11ac_dev_cap_a;
	else
		usr_dot_11ac_dev_cap = pmadapter->usr_dot_11ac_dev_cap_bg;

	pvht_cap->vht_cap.vht_cap_info = usr_dot_11ac_dev_cap;

	LEAVE();
}

/**
 *  @brief Set/get 11ac configuration
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param pioctl_req   A pointer to ioctl request buffer
 *
 *  @return     MLAN_STATUS_PENDING --success, otherwise fail
 */
static mlan_status
wlan_11ac_ioctl_vhtcfg(IN pmlan_adapter pmadapter,
		       IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11ac_cfg *cfg = MNULL;
	t_u16 cmd_action = 0;
	t_u32 usr_vht_cap_info = 0;
	t_u32 cfg_value = 0;
	t_u32 hw_value = 0;
	t_u8 nss = 0;

	ENTER();

#define VHT_CAP_INFO_BIT_FIELDS   (MBIT(4)|MBIT(5)|MBIT(6)|MBIT(7)|\
                                   MBIT(11)|MBIT(12)|MBIT(19)|MBIT(20)|\
                                   MBIT(21)|MBIT(22)|MBIT(28)|MBIT(29))

	cfg = (mlan_ds_11ac_cfg *) pioctl_req->pbuf;

	if (pioctl_req->action == MLAN_ACT_SET) {
	/** SET operation */
	/** validate the user input and correct it if necessary */
	/** set bit fileds */
		usr_vht_cap_info = VHT_CAP_INFO_BIT_FIELDS &
			cfg->param.vht_cfg.vht_cap_info &
			pmadapter->hw_dot_11ac_dev_cap;
	/** set MAX MPDU LEN field (bit 0 - bit 1) */
		cfg_value =
			GET_VHTCAP_MAXMPDULEN(cfg->param.vht_cfg.vht_cap_info);
		hw_value =
			GET_VHTCAP_MAXMPDULEN(pmadapter->hw_dot_11ac_dev_cap);
		SET_VHTCAP_MAXMPDULEN(usr_vht_cap_info,
				      MIN(cfg_value, hw_value));
	/** set CHAN Width Set field (bit 2 - bit 3) */
		cfg_value = GET_VHTCAP_CHWDSET(cfg->param.vht_cfg.vht_cap_info);
		hw_value = GET_VHTCAP_CHWDSET(pmadapter->hw_dot_11ac_dev_cap);
		SET_VHTCAP_CHWDSET(usr_vht_cap_info, MIN(cfg_value, hw_value));
	/** set Rx STBC field (bit 8 - bit 10) */
		cfg_value = GET_VHTCAP_RXSTBC(cfg->param.vht_cfg.vht_cap_info);
		hw_value = GET_VHTCAP_RXSTBC(pmadapter->hw_dot_11ac_dev_cap);
		SET_VHTCAP_RXSTBC(usr_vht_cap_info, MIN(cfg_value, hw_value));
	/** set Steering Number of BFer Ant (bit 13 - bit 15) */
		cfg_value =
			GET_VHTCAP_SNBFERANT(cfg->param.vht_cfg.vht_cap_info);
		hw_value = GET_VHTCAP_SNBFERANT(pmadapter->hw_dot_11ac_dev_cap);
		SET_VHTCAP_SNBFERANT(usr_vht_cap_info,
				     MIN(cfg_value, hw_value));
	/** set Number of Sounding Dimension (bit 16 - bit 18) */
		cfg_value =
			GET_VHTCAP_NUMSNDDM(cfg->param.vht_cfg.vht_cap_info);
		hw_value = GET_VHTCAP_NUMSNDDM(pmadapter->hw_dot_11ac_dev_cap);
		SET_VHTCAP_NUMSNDDM(usr_vht_cap_info, MIN(cfg_value, hw_value));
	/** set Number of Max AMPDU Length Exponent (bit 23 - bit 25) */
		cfg_value =
			GET_VHTCAP_MAXAMPDULENEXP(cfg->param.vht_cfg.
						  vht_cap_info);
		hw_value =
			GET_VHTCAP_MAXAMPDULENEXP(pmadapter->
						  hw_dot_11ac_dev_cap);
		SET_VHTCAP_MAXAMPDULENEXP(usr_vht_cap_info,
					  MIN(cfg_value, hw_value));
	/** set VHT Link Adaptation Capable (bit 26 - bit 27) */
		cfg_value =
			GET_VHTCAP_LINKADPCAP(cfg->param.vht_cfg.vht_cap_info);
		hw_value =
			GET_VHTCAP_LINKADPCAP(pmadapter->hw_dot_11ac_dev_cap);
		SET_VHTCAP_LINKADPCAP(usr_vht_cap_info,
				      MIN(cfg_value, hw_value));
	/** update the user setting if it is beyond the hw capabiliteis */
		cfg->param.vht_cfg.vht_cap_info = usr_vht_cap_info;
		PRINTM(MINFO, "Set: vht cap info  0x%x\n", usr_vht_cap_info);

	/** update the RX MCS map */
#define NO_NSS_SUPPORT 0x3
		if (cfg->param.vht_cfg.txrx & MLAN_RADIO_RX) {
			/* use the previous user value */
			if (cfg->param.vht_cfg.vht_rx_mcs == 0xffffffff)
				cfg->param.vht_cfg.vht_rx_mcs =
					GET_VHTMCS(pmadapter->
						   usr_dot_11ac_mcs_support);
			for (nss = 1; nss <= 8; nss++) {
				cfg_value =
					GET_VHTNSSMCS(cfg->param.vht_cfg.
						      vht_rx_mcs, nss);
				hw_value =
					GET_DEVNSSRXMCS(pmadapter->
							hw_dot_11ac_mcs_support,
							nss);
				if ((hw_value == NO_NSS_SUPPORT) ||
				    (cfg_value == NO_NSS_SUPPORT))
					SET_VHTNSSMCS(cfg->param.vht_cfg.
						      vht_rx_mcs, nss,
						      NO_NSS_SUPPORT);
				else
					SET_VHTNSSMCS(cfg->param.vht_cfg.
						      vht_rx_mcs, nss,
						      MIN(cfg_value, hw_value));
			}
			PRINTM(MINFO, "Set: vht rx mcs set 0x%08x\n",
			       cfg->param.vht_cfg.vht_rx_mcs);
			/* use the previous user value */
			if (cfg->param.vht_cfg.vht_tx_mcs == 0xffffffff)
				cfg->param.vht_cfg.vht_tx_mcs =
					GET_VHTMCS(pmadapter->
						   usr_dot_11ac_mcs_support >>
						   16);
			for (nss = 1; nss <= 8; nss++) {
				cfg_value =
					GET_VHTNSSMCS(cfg->param.vht_cfg.
						      vht_tx_mcs, nss);
				hw_value =
					GET_DEVNSSTXMCS(pmadapter->
							hw_dot_11ac_mcs_support,
							nss);
				if ((hw_value == NO_NSS_SUPPORT) ||
				    (cfg_value == NO_NSS_SUPPORT))
					SET_VHTNSSMCS(cfg->param.vht_cfg.
						      vht_tx_mcs, nss,
						      NO_NSS_SUPPORT);
				else
					SET_VHTNSSMCS(cfg->param.vht_cfg.
						      vht_tx_mcs, nss,
						      MIN(cfg_value, hw_value));
			}
			PRINTM(MINFO, "Set: vht tx mcs set 0x%08x\n",
			       cfg->param.vht_cfg.vht_tx_mcs);

			RESET_DEVRXMCSMAP(pmadapter->usr_dot_11ac_mcs_support);
			pmadapter->usr_dot_11ac_mcs_support |=
				GET_VHTMCS(cfg->param.vht_cfg.vht_rx_mcs);
			RESET_DEVTXMCSMAP(pmadapter->usr_dot_11ac_mcs_support);
			pmadapter->usr_dot_11ac_mcs_support |=
				(GET_VHTMCS(cfg->param.vht_cfg.vht_tx_mcs) <<
				 16);
			PRINTM(MINFO, "Set: vht mcs set 0x%08x\n",
			       pmadapter->usr_dot_11ac_mcs_support);
		}
	}

	if (pmpriv->bss_role == MLAN_BSS_ROLE_STA) {
		if (cfg->param.vht_cfg.txrx & MLAN_RADIO_RX) {
			/* maximum VHT configuration used in association */
			if (pioctl_req->action == MLAN_ACT_SET) {
				if (cfg->param.vht_cfg.band == BAND_SELECT_BG)
					pmadapter->usr_dot_11ac_dev_cap_bg =
						usr_vht_cap_info;
				else if (cfg->param.vht_cfg.band ==
					 BAND_SELECT_A)
					pmadapter->usr_dot_11ac_dev_cap_a =
						usr_vht_cap_info;
				else {
					pmadapter->usr_dot_11ac_dev_cap_bg =
						usr_vht_cap_info;
					pmadapter->usr_dot_11ac_dev_cap_a =
						usr_vht_cap_info;
				}

			} else {
		/** GET operation */
				if (cfg->param.vht_cfg.band == BAND_SELECT_BG) {
					cfg->param.vht_cfg.vht_cap_info =
						pmadapter->
						usr_dot_11ac_dev_cap_bg;
					PRINTM(MINFO,
					       "Get: vht cap info for 2.4GHz 0x%x\n",
					       pmadapter->
					       usr_dot_11ac_dev_cap_bg);
				} else if (cfg->param.vht_cfg.band ==
					   BAND_SELECT_A) {
					cfg->param.vht_cfg.vht_cap_info =
						pmadapter->
						usr_dot_11ac_dev_cap_a;
					PRINTM(MINFO,
					       "Get: vht cap info for 5GHz 0x%x\n",
					       pmadapter->
					       usr_dot_11ac_dev_cap_a);
				} else {
					PRINTM(MINFO,
					       "Get: invalid band selection for vht cap info\n");
					ret = MLAN_STATUS_FAILURE;
				}
				cfg->param.vht_cfg.vht_rx_mcs =
					GET_DEVRXMCSMAP(pmadapter->
							usr_dot_11ac_mcs_support);
				cfg->param.vht_cfg.vht_tx_mcs =
					GET_DEVTXMCSMAP(pmadapter->
							usr_dot_11ac_mcs_support);
			}
			LEAVE();
			return ret;
		}
	}

	if (pioctl_req->action == MLAN_ACT_SET)
		cmd_action = HostCmd_ACT_GEN_SET;
	else
		cmd_action = HostCmd_ACT_GEN_GET;

	/* Send request to firmware */
	ret = wlan_prepare_cmd(pmpriv,
			       HostCmd_CMD_11AC_CFG,
			       cmd_action,
			       0,
			       (t_void *) pioctl_req,
			       (t_void *) & cfg->param.vht_cfg);
	if (ret == MLAN_STATUS_SUCCESS)
		ret = MLAN_STATUS_PENDING;

	LEAVE();
	return ret;
}

/**
 *  @brief Get supported MCS set
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param pioctl_req   A pointer to ioctl request buffer
 *
 *  @return     MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11ac_ioctl_supported_mcs_set(IN pmlan_adapter pmadapter,
				  IN pmlan_ioctl_req pioctl_req)
{
	/* mlan_ds_11ac_cfg *cfg= MNULL; */
	/* int rx_mcs_supp; */
	/* t_u8 mcs_set[NUM_MCS_SUPP]; */

	ENTER();
#if 0
	if (pioctl_req->action == MLAN_ACT_SET) {
		PRINTM(MERROR, "Set operation is not supported\n");
		pioctl_req->status_code = MLAN_ERROR_IOCTL_INVALID;
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	rx_mcs_supp = GET_11ACRXMCSSUPP(pmadapter->usr_dot_11ac_mcs_support);
	/* Set MCS */
	memset(pmadapter, (t_u8 *) mcs_set, 0xff, rx_mcs_supp);
	/* Clear all the other values */
	memset(pmadapter, (t_u8 *) & mcs_set[rx_mcs_supp], 0,
	       NUM_MCS_FIELD - rx_mcs_supp);
	/* Set MCS32 with 40MHz support */
	if (ISSUPP_CHANWIDTH80(pmadapter->usr_dot_11ac_dev_cap_bg)
	    || ISSUPP_CHANWIDTH80(pmadapter->usr_dot_11ac_dev_cap_a)
		)
		SETHT_MCS32(mcs_set);

	cfg = (mlan_ds_11ac_cfg *) pioctl_req->pbuf;
	memcpy(pmadapter, cfg->param.supported_mcs_set, mcs_set, NUM_MCS_SUPP);

#endif
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/********************************************************
    Global Functions
********************************************************/

/**
 *  @brief This function prints the 802.11ac device capability
 *
 *  @param pmadapter     A pointer to mlan_adapter structure
 *  @param cap           Capability value
 *
 *  @return        N/A
 */
void
wlan_show_dot11acdevcap(pmlan_adapter pmadapter, t_u32 cap)
{
	ENTER();

	switch (GET_VHTCAP_MAXMPDULEN(cap)) {
	case 0x00:
		PRINTM(MINFO,
		       "GET_HW_SPEC: Maximum MSDU length = 3895 octets\n");
		break;
	case 0x01:
		PRINTM(MINFO,
		       "GET_HW_SPEC: Maximum MSDU length = 7991 octets\n");
		break;
	case 0x10:
		PRINTM(MINFO,
		       "GET_HW_SPEC: Maximum MSDU length = 11454 octets\n");
		break;
	default:
		PRINTM(MINFO, "Unsupport value\n");
		break;
	}

	PRINTM(MINFO, "GET_HW_SPEC: HTC-VHT %s\n",
	       (ISSUPP_11ACVHTHTCVHT(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: VHT TXOP PS %s\n",
	       (ISSUPP_11ACVHTTXOPPS(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: MU RX beamformee %s\n",
	       (ISSUPP_11ACMURXBEAMFORMEE(cap) ? "supported" :
		"not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: MU TX beamformee %s\n",
	       (ISSUPP_11ACMUTXBEAMFORMEE(cap) ? "supported" :
		"not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: SU RX Beamformee %s\n",
	       (ISSUPP_11ACSUBEAMFORMEE(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: SU TX Beamformer %s\n",
	       (ISSUPP_11ACSUBEAMFORMER(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Rx STBC %s\n",
	       (ISSUPP_11ACRXSTBC(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Tx STBC %s\n",
	       (ISSUPP_11ACTXSTBC(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Short GI %s for 160MHz BW\n",
	       (ISSUPP_11ACSGI160(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Short GI %s for 80MHz BW\n",
	       (ISSUPP_11ACSGI80(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: LDPC coding %s\n",
	       (ISSUPP_11ACLDPC(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Channel BW 20/40/80/160/80+80 MHz %s\n",
	       (ISSUPP_11ACBW8080(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Channel BW 20/40/80/160 MHz %s\n",
	       (ISSUPP_11ACBW160(cap) ? "supported" : "not supported"));

	LEAVE();
	return;
}

/**
 *  @brief This function prints the 802.11ac device MCS
 *
 *  @param pmadapter A pointer to mlan_adapter structure
 *  @param support   Support value
 *
 *  @return        N/A
 */
void
wlan_show_dot11acmcssupport(pmlan_adapter pmadapter, t_u32 support)
{
	ENTER();

	PRINTM(MINFO, "GET_HW_SPEC: MCSs for %2dx%2d MIMO\n",
	       GET_DEVRXMCSMAP(support), GET_DEVTXMCSMAP(support));

	LEAVE();
	return;
}

/**
 *  @brief This function converts the 2-bit MCS map to the highest long GI
 *  VHT PPDU data rate
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param bands        Supported bands
 *  @param mcs_map      2-bit MCS map
 *
 *  @return             the max data rate for long GI
 */
t_u16
wlan_convert_mcsmap_to_maxrate(mlan_private * priv, t_u8 bands, t_u16 mcs_map)
{
	t_u8 i;
	t_u8 nss;
	t_u8 max_mcs;
	t_u16 max_rate = 0;
	t_u32 usr_vht_cap_info = 0;
	mlan_adapter *pmadapter = priv->adapter;

	/* tables of the MCS map to the highest data rate (in Mbps) supported
	   for long GI */
	t_u16 max_rate_lgi_80MHZ[8][3] = {
		{0x124, 0x15F, 0x186}
		,		/* NSS = 1 */
		{0x249, 0x2BE, 0x30C}
		,		/* NSS = 2 */
		{0x36D, 0x41D, 0x492}
		,		/* NSS = 3 */
		{0x492, 0x57C, 0x618}
		,		/* NSS = 4 */
		{0x5B6, 0x6DB, 0x79E}
		,		/* NSS = 5 */
		{0x6DB, 0x83A, 0x0}
		,		/* NSS = 6 */
		{0x7FF, 0x999, 0xAAA}
		,		/* NSS = 7 */
		{0x924, 0xAF8, 0xC30}	/* NSS = 8 */
	};
	t_u16 max_rate_lgi_160MHZ[8][3] = {
		{0x249, 0x2BE, 0x30C}
		,		/* NSS = 1 */
		{0x492, 0x57C, 0x618}
		,		/* NSS = 2 */
		{0x6DB, 0x83A, 0x0}
		,		/* NSS = 3 */
		{0x924, 0xAF8, 0xC30}
		,		/* NSS = 4 */
		{0xB6D, 0xDB6, 0xF3C}
		,		/* NSS = 5 */
		{0xDB6, 0x1074, 0x1248}
		,		/* NSS = 6 */
		{0xFFF, 0x1332, 0x1554}
		,		/* NSS = 7 */
		{0x1248, 0x15F0, 0x1860}	/* NSS = 8 */
	};

	if (bands & BAND_AAC)
		usr_vht_cap_info = pmadapter->usr_dot_11ac_dev_cap_a;
	else
		usr_vht_cap_info = pmadapter->usr_dot_11ac_dev_cap_bg;

	/* find the max NSS supported */
	nss = 0;
	for (i = 0; i < 8; i++) {
		max_mcs = (mcs_map >> (2 * i)) & 0x3;
		if (max_mcs < 3) {
			nss = i;
		}
	}

	max_mcs = (mcs_map >> (2 * nss)) & 0x3;
	/* if max_mcs is 3, nss must be 0 (SS = 1). Thus, max mcs is MCS 9 */
	if (max_mcs >= 3)
		max_mcs = 2;

	if (GET_VHTCAP_CHWDSET(usr_vht_cap_info)) {
		/* support 160 MHz */
		max_rate = max_rate_lgi_160MHZ[nss][max_mcs];
		if (max_rate == 0) {
			/* MCS9 is not supported in NSS6 */
			max_rate = max_rate_lgi_160MHZ[nss][max_mcs - 1];
		}

	} else {
		max_rate = max_rate_lgi_80MHZ[nss][max_mcs];
		if (max_rate == 0) {
			/* MCS9 is not supported in NSS3 */
			max_rate = max_rate_lgi_80MHZ[nss][max_mcs - 1];
		}
	}

	return max_rate;
}

/**
 *  @brief This function fills the VHT cap tlv out put format is LE, not CPU
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param pvht_cap      A pointer to MrvlIETypes_HTCap_t structure
 *  @param bands        Band configuration
 *
 *  @return             N/A
 */
void
wlan_fill_vht_cap_tlv(mlan_private * priv,
		      MrvlIETypes_VHTCap_t * pvht_cap, t_u8 bands)
{
	mlan_adapter *pmadapter = priv->adapter;
	t_u16 mcs_map_user = 0;
	t_u16 mcs_map_resp = 0;
	t_u16 mcs_map_result = 0;
	t_u16 mcs_user = 0;
	t_u16 mcs_resp = 0;
	t_u16 nss;

	ENTER();

	/* Fill VHT cap info */
	wlan_fill_cap_info(priv, pvht_cap, bands);
	pvht_cap->vht_cap.vht_cap_info =
		wlan_cpu_to_le32(pvht_cap->vht_cap.vht_cap_info);

	/* Fill VHT MCS Set */
	/* rx MCS Set, find the minimum of the user rx mcs and ap rx mcs */
	mcs_map_user = GET_DEVRXMCSMAP(pmadapter->usr_dot_11ac_mcs_support);
	mcs_map_resp = wlan_le16_to_cpu(pvht_cap->vht_cap.mcs_sets.rx_mcs_map);
	mcs_map_result = 0;
	for (nss = 1; nss <= 8; nss++) {
		mcs_user = GET_VHTNSSMCS(mcs_map_user, nss);
		mcs_resp = GET_VHTNSSMCS(mcs_map_resp, nss);
		if ((mcs_user == NO_NSS_SUPPORT) ||
		    (mcs_resp == NO_NSS_SUPPORT))
			SET_VHTNSSMCS(mcs_map_result, nss, NO_NSS_SUPPORT);
		else
			SET_VHTNSSMCS(mcs_map_result, nss,
				      MIN(mcs_user, mcs_resp));
	}
	/* rx MCS map */
	pvht_cap->vht_cap.mcs_sets.rx_mcs_map =
		wlan_cpu_to_le16(mcs_map_result);

	/* rx highest rate */
	pvht_cap->vht_cap.mcs_sets.rx_max_rate =
		wlan_convert_mcsmap_to_maxrate(priv, bands, mcs_map_result);
	pvht_cap->vht_cap.mcs_sets.rx_max_rate =
		wlan_cpu_to_le16(pvht_cap->vht_cap.mcs_sets.rx_max_rate);

	/* tx MCS Set find the minimum of the user tx mcs and ap tx mcs */
	mcs_map_user = GET_DEVTXMCSMAP(pmadapter->usr_dot_11ac_mcs_support);
	mcs_map_resp = wlan_le16_to_cpu(pvht_cap->vht_cap.mcs_sets.tx_mcs_map);
	mcs_map_result = 0;
	for (nss = 1; nss <= 8; nss++) {
		mcs_user = GET_VHTNSSMCS(mcs_map_user, nss);
		mcs_resp = GET_VHTNSSMCS(mcs_map_resp, nss);
		if ((mcs_user == NO_NSS_SUPPORT) ||
		    (mcs_resp == NO_NSS_SUPPORT))
			SET_VHTNSSMCS(mcs_map_result, nss, NO_NSS_SUPPORT);
		else
			SET_VHTNSSMCS(mcs_map_result, nss,
				      MIN(mcs_user, mcs_resp));
	}

	/* tx MCS map */
	pvht_cap->vht_cap.mcs_sets.tx_mcs_map =
		wlan_cpu_to_le16(mcs_map_result);
	/* tx highest rate */
	pvht_cap->vht_cap.mcs_sets.tx_max_rate =
		wlan_convert_mcsmap_to_maxrate(priv, bands, mcs_map_result);
	pvht_cap->vht_cap.mcs_sets.tx_max_rate =
		wlan_cpu_to_le16(pvht_cap->vht_cap.mcs_sets.tx_max_rate);

	LEAVE();
	return;
}

/**
 *  @brief This function append the 802_11N tlv
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param pbss_desc    A pointer to BSSDescriptor_t structure
 *  @param ppbuffer     A Pointer to command buffer pointer
 *
 *  @return bytes added to the buffer
 */
int
wlan_cmd_append_11ac_tlv(mlan_private * pmpriv, BSSDescriptor_t * pbss_desc,
			 t_u8 ** ppbuffer)
{
	pmlan_adapter pmadapter = pmpriv->adapter;
	MrvlIETypes_VHTCap_t *pvht_cap;
	MrvlIETypes_OperModeNtf_t *pmrvl_oper_mode;
	IEEEtypes_OperModeNtf_t *pieee_oper_mode;
	MrvlIETypes_VHTOprat_t *pvht_op;
	t_u8 supp_chwd_set;
	t_u32 usr_vht_cap_info;
	int ret_len = 0;

	ENTER();

	/* Null Checks */
	if (ppbuffer == MNULL) {
		LEAVE();
		return 0;
	}
	if (*ppbuffer == MNULL) {
		LEAVE();
		return 0;
	}
	if (pbss_desc->bss_band & BAND_A)
		usr_vht_cap_info = pmadapter->usr_dot_11ac_dev_cap_a;
	else
		usr_vht_cap_info = pmadapter->usr_dot_11ac_dev_cap_bg;

	/* VHT Capabilities IE */
	if (pbss_desc->pvht_cap) {
		pvht_cap = (MrvlIETypes_VHTCap_t *) * ppbuffer;
		memset(pmadapter, pvht_cap, 0, sizeof(MrvlIETypes_VHTCap_t));
		pvht_cap->header.type = wlan_cpu_to_le16(VHT_CAPABILITY);
		pvht_cap->header.len = sizeof(VHT_capa_t);
		memcpy(pmadapter,
		       (t_u8 *) pvht_cap + sizeof(MrvlIEtypesHeader_t),
		       (t_u8 *) pbss_desc->pvht_cap +
		       sizeof(IEEEtypes_Header_t), pvht_cap->header.len);

		wlan_fill_vht_cap_tlv(pmpriv, pvht_cap, pbss_desc->bss_band);

		HEXDUMP("VHT_CAPABILITIES IE", (t_u8 *) pvht_cap,
			sizeof(MrvlIETypes_VHTCap_t));
		*ppbuffer += sizeof(MrvlIETypes_VHTCap_t);
		ret_len += sizeof(MrvlIETypes_VHTCap_t);
		pvht_cap->header.len = wlan_cpu_to_le16(pvht_cap->header.len);
	}

	/* VHT Operation IE */
	if (pbss_desc->pvht_oprat) {
		if (pmpriv->bss_mode == MLAN_BSS_MODE_IBSS) {
			pvht_op = (MrvlIETypes_VHTOprat_t *) * ppbuffer;
			memset(pmadapter, pvht_op, 0,
			       sizeof(MrvlIETypes_VHTOprat_t));
			pvht_op->header.type = wlan_cpu_to_le16(VHT_OPERATION);
			pvht_op->header.len = sizeof(MrvlIETypes_VHTOprat_t) -
				sizeof(MrvlIEtypesHeader_t);
			memcpy(pmadapter,
			       (t_u8 *) pvht_op + sizeof(MrvlIEtypesHeader_t),
			       (t_u8 *) pbss_desc->pvht_oprat +
			       sizeof(IEEEtypes_Header_t), pvht_op->header.len);

			/* negotiate the channel width and central freq */
			/* and keep the central freq as the peer suggests */
			supp_chwd_set = GET_VHTCAP_CHWDSET(usr_vht_cap_info);
			if (supp_chwd_set == VHT_CAP_CHWD_80MHZ)
				pvht_op->chan_width = MIN(VHT_OPER_CHWD_80MHZ,
							  pbss_desc->
							  pvht_oprat->
							  chan_width);
			else if (supp_chwd_set == VHT_CAP_CHWD_160MHZ)
				pvht_op->chan_width = MIN(VHT_OPER_CHWD_160MHZ,
							  pbss_desc->
							  pvht_oprat->
							  chan_width);
			else if (supp_chwd_set == VHT_CAP_CHWD_80_80MHZ)
				pvht_op->chan_width =
					MIN(VHT_OPER_CHWD_80_80MHZ,
					    pbss_desc->pvht_oprat->chan_width);
			else
				pvht_op->chan_width = VHT_OPER_CHWD_20_40MHZ;

			HEXDUMP("VHT_OPERATION IE", (t_u8 *) pvht_op,
				sizeof(MrvlIETypes_VHTOprat_t));
			*ppbuffer += sizeof(MrvlIETypes_VHTOprat_t);
			ret_len += sizeof(MrvlIETypes_VHTOprat_t);
			pvht_op->header.len =
				wlan_cpu_to_le16(pvht_op->header.len);
		}
	}
	/* Operating Mode Notification IE */
	if (pbss_desc->poper_mode) {
		pieee_oper_mode = pbss_desc->poper_mode;
		pmrvl_oper_mode = (MrvlIETypes_OperModeNtf_t *) * ppbuffer;
		memset(pmadapter, pmrvl_oper_mode, 0,
		       sizeof(MrvlIETypes_OperModeNtf_t));
		pmrvl_oper_mode->header.type = wlan_cpu_to_le16(OPER_MODE_NTF);
		pmrvl_oper_mode->header.len = sizeof(t_u8);
		pmrvl_oper_mode->oper_mode = pieee_oper_mode->oper_mode;

		HEXDUMP("OPER MODE NTF IE", (t_u8 *) pmrvl_oper_mode,
			sizeof(MrvlIETypes_OperModeNtf_t));
		*ppbuffer += sizeof(MrvlIETypes_OperModeNtf_t);
		ret_len += sizeof(MrvlIETypes_OperModeNtf_t);
		pmrvl_oper_mode->header.len =
			wlan_cpu_to_le16(pmrvl_oper_mode->header.len);
	}

	LEAVE();
	return ret_len;
}

/**
 *  @brief 11ac configuration handler
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param pioctl_req   A pointer to ioctl request buffer
 *
 *  @return     MLAN_STATUS_SUCCESS --success, otherwise fail
 */
mlan_status
wlan_11ac_cfg_ioctl(pmlan_adapter pmadapter, pmlan_ioctl_req pioctl_req)
{
	mlan_status status = MLAN_STATUS_SUCCESS;
	mlan_ds_11ac_cfg *cfg = MNULL;

	ENTER();

	if (pioctl_req->buf_len < sizeof(mlan_ds_11ac_cfg)) {
		PRINTM(MINFO, "MLAN bss IOCTL length is too short.\n");
		pioctl_req->data_read_written = 0;
		pioctl_req->buf_len_needed = sizeof(mlan_ds_11ac_cfg);
		pioctl_req->status_code = MLAN_ERROR_INVALID_PARAMETER;
		LEAVE();
		return MLAN_STATUS_RESOURCE;
	}
	cfg = (mlan_ds_11ac_cfg *) pioctl_req->pbuf;
	switch (cfg->sub_command) {
	case MLAN_OID_11AC_VHT_CFG:
		status = wlan_11ac_ioctl_vhtcfg(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11AC_CFG_SUPPORTED_MCS_SET:
		status = wlan_11ac_ioctl_supported_mcs_set(pmadapter,
							   pioctl_req);
		break;
	default:
		pioctl_req->status_code = MLAN_ERROR_IOCTL_INVALID;
		status = MLAN_STATUS_FAILURE;
		break;
	}
	LEAVE();
	return status;
}

/**
 *  @brief This function prepares 11ac cfg command
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param cmd      A pointer to HostCmd_DS_COMMAND structure
 *  @param cmd_action   the action: GET or SET
 *  @param pdata_buf    A pointer to data buffer
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_cmd_11ac_cfg(IN pmlan_private pmpriv,
		  IN HostCmd_DS_COMMAND * cmd,
		  IN t_u16 cmd_action, IN t_void * pdata_buf)
{
	pmlan_adapter pmadapter = pmpriv->adapter;
	HostCmd_DS_11AC_CFG *vhtcfg = &cmd->params.vhtcfg;
	mlan_ds_11ac_vht_cfg *vht_cfg = (mlan_ds_11ac_vht_cfg *) pdata_buf;

	ENTER();
	cmd->command = wlan_cpu_to_le16(HostCmd_CMD_11AC_CFG);
	cmd->size = wlan_cpu_to_le16(sizeof(HostCmd_DS_11AC_CFG) + S_DS_GEN);
	vhtcfg->action = wlan_cpu_to_le16(cmd_action);
	vhtcfg->band_config = vht_cfg->band & 0xFF;
	vhtcfg->misc_config = vht_cfg->txrx & 0x3;
	if (vhtcfg->misc_config != 2)
		vhtcfg->misc_config |= (vht_cfg->bwcfg << 2);

	vhtcfg->vht_cap_info = wlan_cpu_to_le32(vht_cfg->vht_cap_info);
	vht_cfg->vht_rx_mcs = wlan_cpu_to_le32(vht_cfg->vht_rx_mcs);
	memcpy(pmadapter, &vhtcfg->vht_supp_mcs_set[0],
	       &vht_cfg->vht_rx_mcs, sizeof(t_u32));
	vht_cfg->vht_tx_mcs = wlan_cpu_to_le32(vht_cfg->vht_tx_mcs);
	memcpy(pmadapter, &vhtcfg->vht_supp_mcs_set[4],
	       &vht_cfg->vht_tx_mcs, sizeof(t_u32));
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the command response of 11accfg
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param resp         A pointer to HostCmd_DS_COMMAND
 *  @param pioctl_buf   A pointer to mlan_ioctl_req structure
 *
 *  @return        MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_ret_11ac_cfg(IN pmlan_private pmpriv,
		  IN HostCmd_DS_COMMAND * resp, IN mlan_ioctl_req * pioctl_buf)
{
	pmlan_adapter pmadapter = pmpriv->adapter;
	mlan_ds_11ac_cfg *cfg = MNULL;
	HostCmd_DS_11AC_CFG *vhtcfg = &resp->params.vhtcfg;

	ENTER();
	cfg = (mlan_ds_11ac_cfg *) pioctl_buf->pbuf;
	if (pioctl_buf &&
	    (wlan_le16_to_cpu(vhtcfg->action) == HostCmd_ACT_GEN_GET)) {
		cfg->param.vht_cfg.band = vhtcfg->band_config;
		cfg->param.vht_cfg.txrx = vhtcfg->misc_config & 0x03;
		if (cfg->param.vht_cfg.txrx & 0x1)
			cfg->param.vht_cfg.bwcfg =
				(vhtcfg->misc_config & 0x04) >> 2;
		else
			cfg->param.vht_cfg.bwcfg = 0;

		cfg->param.vht_cfg.vht_cap_info =
			wlan_le32_to_cpu(vhtcfg->vht_cap_info);
		memcpy(pmadapter, &cfg->param.vht_cfg.vht_rx_mcs,
		       &vhtcfg->vht_supp_mcs_set[0], sizeof(t_u32));
		cfg->param.vht_cfg.vht_rx_mcs =
			wlan_le32_to_cpu(cfg->param.vht_cfg.vht_rx_mcs);
		memcpy(pmadapter, &cfg->param.vht_cfg.vht_tx_mcs,
		       &vhtcfg->vht_supp_mcs_set[4], sizeof(t_u32));
		cfg->param.vht_cfg.vht_tx_mcs =
			wlan_le32_to_cpu(cfg->param.vht_cfg.vht_tx_mcs);
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}
