/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/platform.c#3
*/

/*! \file   "platform.c"
    \brief  This file including the protocol layer privacy function.

    This file provided the macros and functions library support for the
    protocol layer security setting from wlan_oid.c and for parse.c and
    rsn.c and nic_privacy.c

*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/version.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fs.h>

#include <linux/uaccess.h>

#include "gl_os.h"

#if CFG_ENABLE_EARLY_SUSPEND
#include <linux/earlysuspend.h>
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define WIFI_NVRAM_FILE_NAME   "/data/nvram/APCFG/APRDEB/WIFI"
#define WIFI_NVRAM_CUSTOM_NAME "/data/nvram/APCFG/APRDEB/WIFI_CUSTOM"

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

static int netdev_event(struct notifier_block *nb, unsigned long notification, void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
	struct net_device *prDev = ifa->ifa_dev->dev;
	P_GLUE_INFO_T prGlueInfo = NULL;

	if (prDev == NULL) {
		/* DBGLOG(REQ, INFO, ("netdev_event: device is empty.\n")); */
		return NOTIFY_DONE;
	}

	if ((strncmp(prDev->name, "p2p", 3) != 0) && (strncmp(prDev->name, "wlan", 4) != 0)) {
		/* DBGLOG(REQ, INFO, ("netdev_event: xxx\n")); */
		return NOTIFY_DONE;
	}
#if 0				/* CFG_SUPPORT_PASSPOINT */
	{
		/* DBGLOG(REQ, INFO, "[netdev_event] IPV4_DAD is unlock now!!\n"); */
		prGlueInfo->fgIsDad = FALSE;
	}
#endif /* CFG_SUPPORT_PASSPOINT */

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	if (prGlueInfo == NULL) {
		DBGLOG(REQ, INFO, "netdev_event: prGlueInfo is empty.\n");
		return NOTIFY_DONE;
	}

	if (prGlueInfo->fgIsInSuspendMode == FALSE) {
		/* DBGLOG(REQ, INFO,
		("netdev_event: PARAM_MEDIA_STATE_DISCONNECTED. (%d)\n", prGlueInfo->eParamMediaStateIndicated)); */
		return NOTIFY_DONE;
	}

	kalSetNetAddressFromInterface(prGlueInfo, prDev, TRUE);

	return NOTIFY_DONE;

}

#if 0				/* CFG_SUPPORT_PASSPOINT */
static int net6dev_event(struct notifier_block *nb, unsigned long notification, void *ptr)
{
	struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct net_device *prDev = ifa->idev->dev;
	P_GLUE_INFO_T prGlueInfo = NULL;

	if (prDev == NULL) {
		DBGLOG(REQ, INFO, "net6dev_event: device is empty.\n");
		return NOTIFY_DONE;
	}

	if ((strncmp(prDev->name, "p2p", 3) != 0) && (strncmp(prDev->name, "wlan", 4) != 0)) {
		DBGLOG(REQ, INFO, "net6dev_event: xxx\n");
		return NOTIFY_DONE;
	}

	if (strncmp(prDev->name, "p2p", 3) == 0) {
		/* because we store the address of prGlueInfo in p2p's private date of net device */
		/* *((P_GLUE_INFO_T *) netdev_priv(prGlueInfo->prP2PInfo->prDevHandler)) = prGlueInfo; */
		prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	} else {		/* wlan0 */
		prGlueInfo = (P_GLUE_INFO_T) netdev_priv(prDev);
	}

	if (prGlueInfo == NULL) {
		DBGLOG(REQ, INFO, "netdev_event: prGlueInfo is empty.\n");
		return NOTIFY_DONE;
	}
	/* DBGLOG(REQ, INFO, "[net6dev_event] IPV6_DAD is unlock now!!\n"); */
	prGlueInfo->fgIs6Dad = FALSE;

	return NOTIFY_DONE;
}
#endif /* CFG_SUPPORT_PASSPOINT */

static struct notifier_block inetaddr_notifier = {
	.notifier_call = netdev_event,
};

#if 0				/* CFG_SUPPORT_PASSPOINT */
static struct notifier_block inet6addr_notifier = {
	.notifier_call = net6dev_event,
};
#endif /* CFG_SUPPORT_PASSPOINT */

void wlanRegisterNotifier(void)
{
	register_inetaddr_notifier(&inetaddr_notifier);
#if 0				/* CFG_SUPPORT_PASSPOINT */
	register_inet6addr_notifier(&inet6addr_notifier);
#endif /* CFG_SUPPORT_PASSPOINT */
}

void wlanUnregisterNotifier(void)
{
	unregister_inetaddr_notifier(&inetaddr_notifier);
#if 0				/* CFG_SUPPORT_PASSPOINT */
	unregister_inetaddr_notifier(&inet6addr_notifier);
#endif /* CFG_SUPPORT_PASSPOINT */
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
			   early_suspend_callback wlanSuspend, late_resume_callback wlanResume)
{
	int ret = 0;

	if (NULL != wlanSuspend)
		prDesc->suspend = wlanSuspend;
	else {
		DBGLOG(REQ, INFO, "glRegisterEarlySuspend wlanSuspend ERROR.\n");
		ret = -1;
	}

	if (NULL != wlanResume)
		prDesc->resume = wlanResume;
	else {
		DBGLOG(REQ, INFO, "glRegisterEarlySuspend wlanResume ERROR.\n");
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
static int nvram_read(char *filename, char *buf, ssize_t len, int offset)
{
#if CFG_SUPPORT_NVRAM
	struct file *fd;
	int retLen = -1;

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	fd = filp_open(filename, O_RDONLY, 0644);

	if (IS_ERR(fd)) {
		DBGLOG(INIT, INFO, "[nvram_read] : failed to open!!\n");
		return -1;
	}

	do {
		if ((fd->f_op == NULL) || (fd->f_op->read == NULL)) {
			DBGLOG(INIT, INFO, "[nvram_read] : file can not be read!!\n");
			break;
		}

		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					DBGLOG(INIT, INFO, "[nvram_read] : failed to seek!!\n");
					break;
				}
			} else {
				fd->f_pos = offset;
			}
		}

		retLen = fd->f_op->read(fd, buf, len, &fd->f_pos);

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
static int nvram_write(char *filename, char *buf, ssize_t len, int offset)
{
#if CFG_SUPPORT_NVRAM
	struct file *fd;
	int retLen = -1;

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	fd = filp_open(filename, O_WRONLY | O_CREAT, 0644);

	if (IS_ERR(fd)) {
		DBGLOG(INIT, INFO, "[nvram_write] : failed to open!!\n");
		return -1;
	}

	do {
		if ((fd->f_op == NULL) || (fd->f_op->write == NULL)) {
			DBGLOG(INIT, INFO, "[nvram_write] : file can not be write!!\n");
			break;
		}
		/* End of if */
		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					DBGLOG(INIT, INFO, "[nvram_write] : failed to seek!!\n");
					break;
				}
			} else {
				fd->f_pos = offset;
			}
		}

		retLen = fd->f_op->write(fd, buf, len, &fd->f_pos);

	} while (FALSE);

	filp_close(fd, NULL);

	set_fs(old_fs);

	return retLen;

#else /* !CFG_SUPPORT_NVRAMS */

	return -EIO;

#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief API for reading data on NVRAM
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
BOOLEAN kalCfgDataRead16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, OUT PUINT_16 pu2Data)
{
	if (pu2Data == NULL)
		return FALSE;

	if (nvram_read(WIFI_NVRAM_FILE_NAME,
		       (char *)pu2Data, sizeof(unsigned short), u4Offset) != sizeof(unsigned short)) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief API for writing data on NVRAM
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
BOOLEAN kalCfgDataWrite16(IN P_GLUE_INFO_T prGlueInfo, UINT_32 u4Offset, UINT_16 u2Data)
{
	if (nvram_write(WIFI_NVRAM_FILE_NAME,
			(char *)&u2Data, sizeof(unsigned short), u4Offset) != sizeof(unsigned short)) {
		return FALSE;
	} else {
		return TRUE;
	}
}
