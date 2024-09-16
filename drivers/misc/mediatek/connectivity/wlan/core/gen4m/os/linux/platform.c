/*******************************************************************************
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
 ******************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux
 *      /platform.c#3
 */

/*! \file   "platform.c"
 *    \brief  This file including the protocol layer privacy function.
 *
 *    This file provided the macros and functions library support for the
 *    protocol layer security setting from wlan_oid.c and for parse.c and
 *    rsn.c and nic_privacy.c
 *
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include <linux/version.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fs.h>

#include <linux/uaccess.h>
#include "precomp.h"
#include "gl_os.h"

#if CFG_ENABLE_EARLY_SUSPEND
#include <linux/earlysuspend.h>
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define WIFI_NVRAM_FILE_NAME   "/data/nvram/APCFG/APRDEB/WIFI"
#define WIFI_NVRAM_CUSTOM_NAME "/data/nvram/APCFG/APRDEB/WIFI_CUSTOM"

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static const uint8_t *apucDebugNetdevState[] = {
	(uint8_t *) DISP_STRING("NETDEV_UNKNOWN"),
	(uint8_t *) DISP_STRING("NETDEV_UP"),
	(uint8_t *) DISP_STRING("NETDEV_DOWN"),
	(uint8_t *) DISP_STRING("NETDEV_REBOOT"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGE"),
	(uint8_t *) DISP_STRING("NETDEV_REGISTER"),
	(uint8_t *) DISP_STRING("NETDEV_UNREGISTER"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGEMTU"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGEADDR"),
	(uint8_t *) DISP_STRING("NETDEV_GOING_DOWN"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGENAME"),
	(uint8_t *) DISP_STRING("NETDEV_FEAT_CHANGE"),
	(uint8_t *) DISP_STRING("NETDEV_BONDING_FAILOVER"),
	(uint8_t *) DISP_STRING("NETDEV_PRE_UP"),
	(uint8_t *) DISP_STRING("NETDEV_PRE_TYPE_CHANGE"),
	(uint8_t *) DISP_STRING("NETDEV_POST_TYPE_CHANGE"),
	(uint8_t *) DISP_STRING("NETDEV_POST_INIT"),
	(uint8_t *) DISP_STRING("NETDEV_UNREGISTER_FINAL"),
	(uint8_t *) DISP_STRING("NETDEV_RELEASE"),
	(uint8_t *) DISP_STRING("NETDEV_NOTIFY_PEERS"),
	(uint8_t *) DISP_STRING("NETDEV_JOIN"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGEUPPER"),
	(uint8_t *) DISP_STRING("NETDEV_RESEND_IGMP"),
	(uint8_t *) DISP_STRING("NETDEV_PRECHANGEMTU"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGEINFODATA"),
	(uint8_t *) DISP_STRING("NETDEV_BONDING_INFO"),
	(uint8_t *) DISP_STRING("NETDEV_PRECHANGEUPPER"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGELOWERSTATE"),
	(uint8_t *) DISP_STRING("NETDEV_UDP_TUNNEL_PUSH_INFO"),
	(uint8_t *) DISP_STRING("NETDEV_UNKNOWN"),
	(uint8_t *) DISP_STRING("NETDEV_CHANGE_TX_QUEUE_LEN"),
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static int wlan_netdev_notifier_call(struct notifier_block *nb,
		unsigned long state, void *ndev);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if 1
static int netdev_event(struct notifier_block *nb,
			unsigned long notification, void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
	struct net_device *prDev = ifa->ifa_dev->dev;
	struct GLUE_INFO *prGlueInfo = NULL;

	if (prDev == NULL) {
		/* DBGLOG(REQ, INFO, ("netdev_event: device is empty.\n")); */
		return NOTIFY_DONE;
	}

	if ((strncmp(prDev->name, "p2p", 3) != 0)
	    && (strncmp(prDev->name, "wlan", 4) != 0)) {
		/* DBGLOG(REQ, INFO, ("netdev_event: xxx\n")); */
		return NOTIFY_DONE;
	}
#if 0				/* CFG_SUPPORT_PASSPOINT */
	{
		/* printk(KERN_INFO
		 *        "[netdev_event] IPV4_DAD is unlock now!!\n");
		 */
		prGlueInfo->fgIsDad = FALSE;
	}
#endif /* CFG_SUPPORT_PASSPOINT */

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	if (prGlueInfo == NULL) {
		DBGLOG(REQ, INFO, "netdev_event: prGlueInfo is empty.\n");
		return NOTIFY_DONE;
	}

	if (prGlueInfo->fgIsInSuspendMode == FALSE) {
		/* DBGLOG(REQ, INFO,
		 *  ("netdev_event: MEDIA_STATE_DISCONNECTED. (%d)\n",
		 * prGlueInfo->eParamMediaStateIndicated));
		 */
		return NOTIFY_DONE;
	}

	kalSetNetAddressFromInterface(prGlueInfo, prDev, TRUE);

	return NOTIFY_DONE;

}
#endif
#if 0				/* CFG_SUPPORT_PASSPOINT */
static int net6dev_event(struct notifier_block *nb,
			 unsigned long notification, void *ptr)
{
	struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct net_device *prDev = ifa->idev->dev;
	struct GLUE_INFO *prGlueInfo = NULL;

	if (prDev == NULL) {
		DBGLOG(REQ, INFO, "net6dev_event: device is empty.\n");
		return NOTIFY_DONE;
	}

	if ((strncmp(prDev->name, "p2p", 3) != 0)
	    && (strncmp(prDev->name, "wlan", 4) != 0)) {
		DBGLOG(REQ, INFO, "net6dev_event: xxx\n");
		return NOTIFY_DONE;
	}

	if (strncmp(prDev->name, "p2p", 3) == 0) {
		/* because we store the address of prGlueInfo in p2p's private
		 * date of net device
		 */
		/* *((P_GLUE_INFO_T *) netdev_priv(
		 *        prGlueInfo->prP2PInfo[0]->prDevHandler)) = prGlueInfo;
		 */
		prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	} else {		/* wlan0 */
		prGlueInfo = (struct GLUE_INFO *) netdev_priv(prDev);
	}

	if (prGlueInfo == NULL) {
		DBGLOG(REQ, INFO, "netdev_event: prGlueInfo is empty.\n");
		return NOTIFY_DONE;
	}
	/* printk(KERN_INFO "[net6dev_event] IPV6_DAD is unlock now!!\n"); */
	prGlueInfo->fgIs6Dad = FALSE;

	return NOTIFY_DONE;
}
#endif /* CFG_SUPPORT_PASSPOINT */

#if 1       /* unused  */
static struct notifier_block inetaddr_notifier = {
	.notifier_call = netdev_event,
};
#endif

#if 0				/* CFG_SUPPORT_PASSPOINT */
static struct notifier_block inet6addr_notifier = {
	.notifier_call = net6dev_event,
};
#endif /* CFG_SUPPORT_PASSPOINT */

void wlanRegisterInetAddrNotifier(void)
{
#if CFG_ENABLE_NET_DEV_NOTIFY

	register_inetaddr_notifier(&inetaddr_notifier);
#if 0				/* CFG_SUPPORT_PASSPOINT */
	register_inet6addr_notifier(&inet6addr_notifier);
#endif /* CFG_SUPPORT_PASSPOINT */

#endif
}

void wlanUnregisterInetAddrNotifier(void)
{
#if CFG_ENABLE_NET_DEV_NOTIFY

	unregister_inetaddr_notifier(&inetaddr_notifier);
#if 0				/* CFG_SUPPORT_PASSPOINT */
	unregister_inetaddr_notifier(&inet6addr_notifier);
#endif /* CFG_SUPPORT_PASSPOINT */

#endif
}

#if CFG_ENABLE_EARLY_SUSPEND
/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will register platform driver to os
 *
 * \param[in] wlanSuspend    Function pointer to platform suspend function
 * \param[in] wlanResume   Function pointer to platform resume   function
 *
 * \return The result of registering earlysuspend
 */
/*----------------------------------------------------------------------------*/

int glRegisterEarlySuspend(struct early_suspend *prDesc,
			   early_suspend_callback wlanSuspend,
			   late_resume_callback wlanResume)
{
	int ret = 0;

	if (wlanSuspend != NULL)
		prDesc->suspend = wlanSuspend;
	else {
		DBGLOG(REQ, INFO,
		       "glRegisterEarlySuspend wlanSuspend ERROR.\n");
		ret = -1;
	}

	if (wlanResume != NULL)
		prDesc->resume = wlanResume;
	else {
		DBGLOG(REQ, INFO,
		       "glRegisterEarlySuspend wlanResume ERROR.\n");
		ret = -1;
	}

	register_early_suspend(prDesc);
	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will un-register platform driver to os
 *
 * \return The result of un-registering earlysuspend
 */
/*----------------------------------------------------------------------------*/

int glUnregisterEarlySuspend(struct early_suspend *prDesc)
{
	int ret = 0;

	unregister_early_suspend(prDesc);

	prDesc->suspend = NULL;
	prDesc->resume = NULL;

	return ret;
}
#endif

#if 0
/*----------------------------------------------------------------------------*/
/*!
 * \brief Utility function for reading data from files on NVRAM-FS
 *
 * \param[in]
 *           filename
 *           len
 *           offset
 * \param[out]
 *           buf
 * \return
 *           actual length of data being read
 */
/*----------------------------------------------------------------------------*/
static int nvram_read(char *filename, char *buf,
		      ssize_t len, int offset)
{
#if CFG_SUPPORT_NVRAM
	struct file *fd;
	int retLen = -1;
#if KERNEL_VERSION(4, 4, 0) <= LINUX_VERSION_CODE
	loff_t pos;
	char __user *p;
#endif

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	fd = filp_open(filename, O_RDONLY, 0644);

	if (IS_ERR(fd)) {
		DBGLOG(INIT, INFO, "[nvram_read] : failed to open!!\n");
		set_fs(old_fs);
		return -1;
	}

	do {
		if (fd->f_op == NULL) {
			DBGLOG(INIT, INFO, "[nvram_read] : f_op is NULL!!\n");
			break;
		}

		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					DBGLOG(INIT, INFO,
					       "[nvram_read] : failed to seek!!\n");
					break;
				}
			} else {
				fd->f_pos = offset;
			}
		}

#if KERNEL_VERSION(4, 4, 0) <= LINUX_VERSION_CODE
		p = (__force char __user *)buf;
		pos = (loff_t)offset;

		retLen = __vfs_read(fd, p, len, &pos);
#else
		retLen = fd->f_op->read(fd, buf, len, &fd->f_pos);
#endif
		if (retLen < 0)
			DBGLOG(INIT, ERROR,
			       "[nvram_read] : read failed!! Error code: %d\n",
			       retLen);
	} while (FALSE);

	filp_close(fd, NULL);

	set_fs(old_fs);

	return retLen;

#else /* !CFG_SUPPORT_NVRAM */

	return -EIO;

#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Utility function for writing data to files on NVRAM-FS
 *
 * \param[in]
 *           filename
 *           buf
 *           len
 *           offset
 * \return
 *           actual length of data being written
 */
/*----------------------------------------------------------------------------*/
static int nvram_write(char *filename, char *buf,
		       ssize_t len, int offset)
{
#if CFG_SUPPORT_NVRAM
	struct file *fd;
	int retLen = -1;
#if KERNEL_VERSION(4, 4, 0) <= LINUX_VERSION_CODE
	loff_t pos;
	char __user *p;
#endif

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	fd = filp_open(filename, O_WRONLY | O_CREAT, 0644);

	if (IS_ERR(fd)) {
		DBGLOG(INIT, INFO, "[nvram_write] : failed to open!!\n");
		set_fs(old_fs);
		return -1;
	}

	do {
		if (fd->f_op == NULL) {
			DBGLOG(INIT, INFO, "[nvram_write] : f_op is NULL!!\n");
			break;
		}
		/* End of if */
		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					DBGLOG(INIT, INFO,
					       "[nvram_write] : failed to seek!!\n");
					break;
				}
			} else {
				fd->f_pos = offset;
			}
		}

#if KERNEL_VERSION(4, 4, 0) <= LINUX_VERSION_CODE
		p = (__force char __user *)buf;
		pos = (loff_t)offset;

		retLen = __vfs_write(fd, p, len, &pos);
#else
		retLen = fd->f_op->write(fd, buf, len, &fd->f_pos);
#endif
		if (retLen < 0)
			DBGLOG(INIT, ERROR,
			       "[nvram_write] : write failed!! Error code: %d\n",
			       retLen);
	} while (FALSE);

	filp_close(fd, NULL);

	set_fs(old_fs);

	return retLen;

#else /* !CFG_SUPPORT_NVRAMS */

	return -EIO;

#endif
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief API for reading data on NVRAM with flexible length.
 *
 * \param[in]
 *           prGlueInfo
 *           u4Offset
 *           len
 * \param[out]
 *           pu2Data
 * \return
 *           TRUE
 *           FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t kalCfgDataRead(IN struct GLUE_INFO *prGlueInfo,
			IN uint32_t u4Offset,
			IN ssize_t len, OUT uint16_t *pu2Data)
{
	if (pu2Data == NULL)
		return FALSE;

	if (u4Offset + len > MAX_CFG_FILE_WIFI_REC_SIZE)
		return FALSE;

	kalMemCopy(pu2Data, &g_aucNvram[u4Offset], len);
	return TRUE;
#if 0
	if (nvram_read(WIFI_NVRAM_FILE_NAME,
		       (char *)pu2Data, len, u4Offset) != len) {
		return FALSE;
	} else {
		return TRUE;
	}
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief API for reading data on NVRAM with 2 bytes fixed length.
 *
 * \param[in]
 *           prGlueInfo
 *           u4Offset
 * \param[out]
 *           pu2Data
 * \return
 *           TRUE
 *           FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t kalCfgDataRead16(IN struct GLUE_INFO *prGlueInfo,
			  IN uint32_t u4Offset, OUT uint16_t *pu2Data)
{
	if (pu2Data == NULL)
		return FALSE;

	if (u4Offset + sizeof(unsigned short) > MAX_CFG_FILE_WIFI_REC_SIZE)
		return FALSE;

	kalMemCopy(pu2Data, &g_aucNvram[u4Offset],
		sizeof(unsigned short));
	return TRUE;
#if 0
	if (nvram_read(WIFI_NVRAM_FILE_NAME,
		       (char *)pu2Data, sizeof(unsigned short),
		       u4Offset) != sizeof(unsigned short)) {
		return FALSE;
	} else {
		return TRUE;
	}
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief API for writing data on NVRAM with 2 bytes fixed length.
 *
 * \param[in]
 *           prGlueInfo
 *           u4Offset
 *           u2Data
 * \return
 *           TRUE
 *           FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t kalCfgDataWrite16(IN struct GLUE_INFO *prGlueInfo,
			   uint32_t u4Offset, uint16_t u2Data)
{
	if (u4Offset + sizeof(unsigned short) > MAX_CFG_FILE_WIFI_REC_SIZE)
		return FALSE;

	kalMemCopy(&g_aucNvram[u4Offset], &u2Data,
		sizeof(unsigned short));
	return TRUE;
#if 0
	if (nvram_write(WIFI_NVRAM_FILE_NAME,
			(char *)&u2Data, sizeof(unsigned short),
			u4Offset) != sizeof(unsigned short)) {
		return FALSE;
	} else {
		return TRUE;
	}
#endif
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief API for writing data on NVRAM with 1 bytes fixed length.
 *
 * \param[in]
 *           prGlueInfo
 *           u4Offset
 *           u1Data
 * \return
 *           TRUE
 *           FALSE
 */
/*----------------------------------------------------------------------------*/

u_int8_t kalCfgDataWrite8(IN struct GLUE_INFO *prGlueInfo,
			   IN uint32_t u4Offset, IN uint8_t u1Data)
{
	if (u4Offset + sizeof(unsigned char) > MAX_CFG_FILE_WIFI_REC_SIZE)
		return FALSE;

	kalMemCopy(&g_aucNvram[u4Offset], &u1Data,
		sizeof(unsigned char));
	return TRUE;

}

static int wlan_netdev_notifier_call(struct notifier_block *nb,
		unsigned long state, void *ndev)
{
#if KERNEL_VERSION(3, 11, 0) <= CFG80211_VERSION_CODE
	struct netdev_notifier_info *dev_notif_info = ndev;
	struct net_device *dev = dev_notif_info != NULL ?
			dev_notif_info->dev : NULL;
#else
	struct net_device *dev = ndev;
#endif

	if (!dev)
		return NOTIFY_DONE;

	if ((strncmp(dev->name, "wlan", 4) != 0) &&
			(strncmp(dev->name, "p2p", 3) != 0) &&
			(strncmp(dev->name, "ap", 2) != 0)) {
		return NOTIFY_DONE;
	}

	DBGLOG(REQ, TRACE, "%s's new state: %lu %s.\n",
			dev->name, state, apucDebugNetdevState[state]);

	return NOTIFY_DONE;
}

static struct notifier_block wlan_netdev_notifier = {
	.notifier_call = wlan_netdev_notifier_call,
};

void wlanRegisterNetdevNotifier(void)
{
	register_netdevice_notifier(&wlan_netdev_notifier);
}

void wlanUnregisterNetdevNotifier(void)
{
	unregister_netdevice_notifier(&wlan_netdev_notifier);
}
