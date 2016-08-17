/** @file mlan_11ac.h
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

#ifndef _MLAN_11AC_H_
#define _MLAN_11AC_H_

#include "mlan_11n_aggr.h"
#include "mlan_11n_rxreorder.h"
#include "mlan_wmm.h"

void wlan_show_dot11acdevcap(pmlan_adapter pmadapter, t_u32 cap);
void wlan_show_dot11acmcssupport(pmlan_adapter pmadapter, t_u32 support);
t_u16 wlan_convert_mcsmap_to_maxrate(mlan_private * priv, t_u8 bands,
				     t_u16 mcs_map);
void wlan_fill_vht_cap_tlv(mlan_private * priv, MrvlIETypes_VHTCap_t * pvht_cap,
			   t_u8 bands);
int wlan_cmd_append_11ac_tlv(mlan_private * pmpriv, BSSDescriptor_t * pbss_desc,
			     t_u8 ** ppbuffer);
mlan_status wlan_11ac_cfg_ioctl(pmlan_adapter pmadapter,
				pmlan_ioctl_req pioctl_req);

mlan_status wlan_cmd_11ac_cfg(IN pmlan_private pmpriv,
			      IN HostCmd_DS_COMMAND * cmd,
			      IN t_u16 cmd_action, IN t_void * pdata_buf);

mlan_status wlan_ret_11ac_cfg(IN pmlan_private pmpriv,
			      IN HostCmd_DS_COMMAND * resp,
			      IN mlan_ioctl_req * pioctl_buf);

t_u8 wlan_get_center_freq_idx(IN mlan_private * pmpriv,
			      IN t_u8 band, IN t_u32 pri_chan, IN t_u8 chan_bw);

#endif /* _MLAN_11AC_H_ */
