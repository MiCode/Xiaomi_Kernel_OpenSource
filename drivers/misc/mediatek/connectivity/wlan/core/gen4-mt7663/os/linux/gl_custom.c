/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>
#include "precomp.h"

#if CFG_SUPPORT_CUSTOM_NETLINK

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define MAX_BIND_PROCESS           (1)
#define GL_CUSTOM_FAMILY_NAME      "WIFI_NL_CUSTOM"
#define MAX_CUSTOM_PKT_LENGTH      (2048)
enum {
	__GL_CUSTOM_ATTR_INVALID,
	GL_CUSTOM_ATTR_MSG,	/* message */
	__GL_CUSTOM_ATTR_MAX,
};
#define GL_CUSTOM_ATTR_MAX       (__GL_CUSTOM_ATTR_MAX - 1)

enum {
	__GL_CUSTOM_COMMAND_INVALID,
	GL_CUSTOM_COMMAND_BIND,	/* bind */
	GL_CUSTOM_COMMAND_SEND,	/* user -> kernel */
	GL_CUSTOM_COMMAND_RECV,	/* kernel -> user */
	__GL_CUSTOM_COMMAND_MAX,
};
#define GL_CUSTOM_COMMAND_MAX    (__GL_CUSTOM_COMMAND_MAX - 1)

#if CFG_SUPPORT_TX_BEACON_STA_MODE
#define CUSTOM_BEACON_TYPE (0x80)
#endif
/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int32_t glCustomGenlBind(
	struct sk_buff *skb, struct genl_info *info);
static int32_t glCustomRecvFromUplayer(
	struct sk_buff *skb, struct genl_info *info);

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static struct genl_family glCustomGenlFamily = {
	.id = GENL_ID_GENERATE,
	.hdrsize = 0,
	.name = GL_CUSTOM_FAMILY_NAME,
	.version = 1,
	.maxattr = GL_CUSTOM_ATTR_MAX,
};

/* attribute policy */
static struct nla_policy glCustomGenlPolicy[GL_CUSTOM_ATTR_MAX + 1] = {
	[GL_CUSTOM_ATTR_MSG] = {.type = NLA_NUL_STRING},
};

/* operation definition */
static struct genl_ops glCustomGenlOpsArray[] = {
	{
		.cmd = GL_CUSTOM_COMMAND_BIND,
		.flags = 0,
		.policy = glCustomGenlPolicy,
		.doit = glCustomGenlBind,
		.dumpit = NULL,
	},
	{
		.cmd = GL_CUSTOM_COMMAND_SEND,
		.flags = 0,
		.policy = glCustomGenlPolicy,
		.doit = glCustomRecvFromUplayer,
		.dumpit = NULL,
	},
};

int32_t gBindProcessNum;
pid_t gBindPid;

uint8_t gScanIEBuf[MAX_IE_LENGTH];
uint32_t gScanIELen;

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
void glCustomGenlInit(void)
{
	if (genl_register_family_with_ops(&glCustomGenlFamily,
			glCustomGenlOpsArray) != 0)
		DBGLOG(INIT, ERROR,
			"%s(): GE_NELINK family registration fail\n",
			__func__);
}

void glCustomGenlDeinit(void)
{
	genl_unregister_family(&glCustomGenlFamily);
}

static int32_t glCustomGenlBind(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *na;
	int8_t *pData = NULL;

	DBGLOG(INIT, ERROR, "%s(): ->\n", __func__);
	if (info == NULL)
		return -1;

	na = info->attrs[GL_CUSTOM_ATTR_MSG];
	if (na)
		pData = (int8_t *) nla_data(na);

	if (strcmp(pData, "BIND") == 0) {
		if (gBindProcessNum < MAX_BIND_PROCESS) {
			gBindPid = info->snd_portid;
			gBindProcessNum++;
			DBGLOG(INIT, ERROR,
				"%s():-> pid  = %d\n",
				__func__, info->snd_portid);
		} else {
			gBindPid = info->snd_portid;
			DBGLOG(INIT, ERROR,
				"%s(): exceeding binding limit %d\n",
				__func__, MAX_BIND_PROCESS);
		}
	} else if (strcmp(pData, "UNBIND") == 0) {
		if (gBindProcessNum == 1) {
			gBindPid = 0;
			gBindProcessNum--;
			DBGLOG(INIT, ERROR,
				"%s():-> pid  = %d unbind\n",
				__func__, info->snd_portid);
		} else {
			DBGLOG(INIT, ERROR,
				"%s(): unbinding error %d\n",
				__func__, gBindProcessNum);
		}
	} else {
		DBGLOG(INIT, ERROR,
			"%s(): Unknown cmd %s\n",
			__func__, pData);
	}

	return 0;
}

#if CFG_SUPPORT_TX_BEACON_STA_MODE
static uint32_t glCustomBeaconTxDone(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		DBGLOG(TX, WARN,
			"glCustomBeaconTxDone: Status: %d, seq NO. %d\n",
			rTxDoneStatus, prMsduInfo->ucTxSeqNum);

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;
}

static uint32_t glCustomSendBeacon(void *probe_resp, uint32_t resp_len)
{
	struct ADAPTER *prAdapter;
	struct MSDU_INFO *prMsduInfo;
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = wlanGetGlueInfo();

	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "No glue info\n");
		return WLAN_STATUS_FAILURE;
	}

	prAdapter = prGlueInfo->prAdapter;

	/* 4 <1> Allocate a PKT_INFO_T for Probe Response Frame */
	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, resp_len);
	if (prMsduInfo == NULL) {
		DBGLOG(BSS, WARN, "No PKT_INFO_T for sending Custom Frame\n");
		return WLAN_STATUS_RESOURCES;
	}

	/* 4 <2> Compose Probe Response frame header */
	/* and fixed fields in MSDU_INfO_T. */
	/* Compose Header and Fixed Field */
	memcpy((int8_t *)prMsduInfo->prPacket, (int8_t *)probe_resp, resp_len);

	/* 4 <3> Update information of MSDU_INFO_T */
	nicTxSetMngPacket(prAdapter, prMsduInfo, 0,
				0xFF, WLAN_MAC_MGMT_HEADER_LEN,
				resp_len, glCustomBeaconTxDone,
				MSDU_RATE_MODE_LOWEST_RATE);
	nicTxConfigPktControlFlag(prMsduInfo,
				MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

	DBGLOG(TX, WARN, "glCustomSendBeacon seq NO %d\n",
		prMsduInfo->ucTxSeqNum);

	/* 4 <6> Inform TXM  to send this Beacon /Probe Response frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}
#endif

static int32_t glCustomRecvFromUplayer(
	struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *na;
	int8_t *pData = NULL;
	int32_t u4DataLen = 0;

	if (info == NULL)
		goto out;

	na = info->attrs[GL_CUSTOM_ATTR_MSG];

	if (na) {
		pData = (int8_t *) nla_data(na);
		u4DataLen = nla_len(na);
	}

	DBGLOG(INIT, TRACE, "glCustomRecvFromUplayer len=%d, data[0]=0x%2x\n",
		u4DataLen, (uint8_t)pData[0]);


#if CFG_SUPPORT_TX_BEACON_STA_MODE
	if ((uint8_t)pData[0] == CUSTOM_BEACON_TYPE)
		glCustomSendBeacon(pData, u4DataLen);
#endif

out:
	return 0;
}
#endif /* CFG_CUSTOM_NETLINK_SUPPORT */
