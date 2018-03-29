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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/gl_kal.c#10
*/

/*! \file   gl_kal.c
    \brief  GLUE Layer will export the required procedures here for internal driver stack.

    This file contains all routines which are exported from GLUE Layer to internal
    driver stack.
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include "gl_kal.h"
#include "gl_wext.h"
#include "precomp.h"
#if CFG_SUPPORT_AGPS_ASSIST
#include <net/netlink.h>
#endif
#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
#include <mt_sleep.h>
#endif


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
#if DBG
int allocatedMemSize = 0;
#endif

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static PVOID pvIoBuffer;
static UINT_32 pvIoBufferSize;
static UINT_32 pvIoBufferUsage;
static struct KAL_HALT_CTRL_T rHaltCtrl = {
	.lock = __SEMAPHORE_INITIALIZER(rHaltCtrl.lock, 1),
	.owner = NULL,
	.fgHalt = TRUE,
	.fgHeldByKalIoctl = FALSE,
	.u4HoldStart = 0,
};

/* framebuffer callback related variable and status flag */
static struct notifier_block wlan_fb_notifier;
void *wlan_fb_notifier_priv_data = NULL;
BOOLEAN wlan_fb_power_down = FALSE;
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#if CFG_ENABLE_FW_DOWNLOAD

static struct file *filp;
static uid_t orgfsuid;
static gid_t orgfsgid;
static mm_segment_t orgfs;

static PUINT_8 apucFwPath[] = {
	(PUINT_8) "/storage/sdcard0/",
	(PUINT_8) "/vendor/firmware/",
#if !CONFIG_ANDROID
	(PUINT_8) "/lib/firmware/",
#endif
	NULL
};

#if defined(MT6797)

static PUINT_8 apucFwName[] = {
	(PUINT_8) CFG_FW_FILENAME "_6797",
	NULL
};

static PPUINT_8 appucFwNameTable[] = {
	apucFwName
};

#else
/* E2 */
static PUINT_8 apucFwNameE2[] = {
	(PUINT_8) CFG_FW_FILENAME "_MT6630_E2",
	(PUINT_8) CFG_FW_FILENAME "_MT6630",
	NULL
};

/* E3 */
static PUINT_8 apucFwNameE3[] = {
	(PUINT_8) CFG_FW_FILENAME "_MT6630_E3",
	(PUINT_8) CFG_FW_FILENAME "_MT6630",
	(PUINT_8) CFG_FW_FILENAME "_MT6630_E2",
	NULL
};

/* Default */
static PUINT_8 apucFwName[] = {
	(PUINT_8) CFG_FW_FILENAME "_MT6630",
	(PUINT_8) CFG_FW_FILENAME "_MT6630_E2",
	(PUINT_8) CFG_FW_FILENAME "_MT6630_E3",
	NULL
};

static PPUINT_8 appucFwNameTable[] = {
	apucFwName,
	apucFwNameE2,
	apucFwNameE3,
	apucFwNameE3,
};
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to
*        open firmware image in kernel space
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
*
* \retval WLAN_STATUS_SUCCESS.
* \retval WLAN_STATUS_FAILURE.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalFirmwareOpen(IN P_GLUE_INFO_T prGlueInfo)
{
	UINT_8 ucPathIdx, ucNameIdx;
	PPUINT_8 apucNameTable;
	UINT_8 ucMaxEcoVer = (sizeof(appucFwNameTable) / sizeof(PPUINT_8));
	UINT_8 ucCurEcoVer = wlanGetEcoVersion(prGlueInfo->prAdapter);
	UINT_8 aucFwName[128];
	BOOLEAN fgResult = FALSE;

	/* FIX ME: since we don't have hotplug script in the filesystem
	 * , so the request_firmware() KAPI can not work properly
	 */

	/* save uid and gid used for filesystem access.
	 * set user and group to 0(root) */
	struct cred *cred = (struct cred *)get_current_cred();

	orgfsuid = cred->fsuid.val;
	orgfsgid = cred->fsgid.val;
	cred->fsuid.val = cred->fsgid.val = 0;

	ASSERT(prGlueInfo);

	orgfs = get_fs();
	set_fs(get_ds());

	/* Get FW name table */
	if (ucMaxEcoVer < ucCurEcoVer)
		apucNameTable = apucFwName;
	else
		apucNameTable = appucFwNameTable[ucCurEcoVer - 1];

	/* Try to open FW binary */
	for (ucPathIdx = 0; apucFwPath[ucPathIdx]; ucPathIdx++) {
		for (ucNameIdx = 0; apucNameTable[ucNameIdx]; ucNameIdx++) {

			kalSprintf(aucFwName, "%s%s", apucFwPath[ucPathIdx], apucNameTable[ucNameIdx]);

			filp = filp_open(aucFwName, O_RDONLY, 0);
			if (IS_ERR(filp)) {
				DBGLOG(INIT, TRACE, "Open FW image: %s failed, errno[%p]\n",
						     aucFwName, ERR_PTR((LONG) filp));
				continue;
			} else {
				DBGLOG(INIT, TRACE, "Open FW image: %s done\n", aucFwName);
				fgResult = TRUE;
				break;
			}
		}

		if (fgResult)
			break;
	}

	/* Check result */
	if (fgResult) {
		DBGLOG(INIT, INFO, "Open FW image: %s done\n", aucFwName);
	} else {
		DBGLOG(INIT, ERROR, "Open FW image failed! Cur/Max ECO Ver[E%u/E%u]\n", ucCurEcoVer, ucMaxEcoVer);

		/* Dump tried FW path/name */
		for (ucPathIdx = 0; apucFwPath[ucPathIdx]; ucPathIdx++) {
			for (ucNameIdx = 0; apucNameTable[ucNameIdx]; ucNameIdx++) {

				kalSprintf(aucFwName, "%s%s", apucFwPath[ucPathIdx], apucNameTable[ucNameIdx]);

				filp = filp_open(aucFwName, O_RDONLY, 0);
				if (IS_ERR(filp)) {
					DBGLOG(INIT, INFO, "Open FW image: %s failed, errno[%p]\n",
							    aucFwName, ERR_PTR((LONG) filp));
				} else {
					DBGLOG(INIT, INFO, "Open FW image: %s done\n", aucFwName);
				}
			}
		}
		goto error_open;
	}

	return WLAN_STATUS_SUCCESS;

error_open:
	/* restore */
	set_fs(orgfs);
	cred->fsuid.val = orgfsuid;
	cred->fsgid.val = orgfsgid;
	put_cred(cred);
	return WLAN_STATUS_FAILURE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to
*        release firmware image in kernel space
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
*
* \retval WLAN_STATUS_SUCCESS.
* \retval WLAN_STATUS_FAILURE.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalFirmwareClose(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	if ((filp != NULL) && !IS_ERR(filp)) {
		/* close firmware file */
		filp_close(filp, NULL);

		/* restore */
		set_fs(orgfs);
		{
			struct cred *cred = (struct cred *)get_current_cred();

			cred->fsuid.val = orgfsuid;
			cred->fsgid.val = orgfsgid;
			put_cred(cred);
		}
		filp = NULL;
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to
*        load firmware image in kernel space
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
*
* \retval WLAN_STATUS_SUCCESS.
* \retval WLAN_STATUS_FAILURE.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalFirmwareLoad(IN P_GLUE_INFO_T prGlueInfo, OUT PVOID prBuf, IN UINT_32 u4Offset, OUT PUINT_32 pu4Size)
{
	ASSERT(prGlueInfo);
	ASSERT(pu4Size);
	ASSERT(prBuf);

	/* l = filp->f_path.dentry->d_inode->i_size; */

	/* the object must have a read method */
	if ((filp == NULL) || IS_ERR(filp) || (filp->f_op == NULL) || (filp->f_op->read == NULL)) {
		goto error_read;
	} else {
		filp->f_pos = u4Offset;
		*pu4Size = filp->f_op->read(filp, prBuf, *pu4Size, &filp->f_pos);
	}

	return WLAN_STATUS_SUCCESS;

error_read:
	return WLAN_STATUS_FAILURE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to
*        query firmware image size in kernel space
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
*
* \retval WLAN_STATUS_SUCCESS.
* \retval WLAN_STATUS_FAILURE.
*
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS kalFirmwareSize(IN P_GLUE_INFO_T prGlueInfo, OUT PUINT_32 pu4Size)
{
	ASSERT(prGlueInfo);
	ASSERT(pu4Size);

	*pu4Size = filp->f_path.dentry->d_inode->i_size;

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to load firmware image
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
* \param ppvMapFileBuf  Pointer of pointer to memory-mapped firmware image
* \param pu4FileLength  File length and memory mapped length as well

* \retval Map File Handle, used for unammping
*/
/*----------------------------------------------------------------------------*/

PVOID kalFirmwareImageMapping(IN P_GLUE_INFO_T prGlueInfo, OUT PPVOID ppvMapFileBuf, OUT PUINT_32 pu4FileLength)
{
	DEBUGFUNC("kalFirmwareImageMapping");

	ASSERT(prGlueInfo);
	ASSERT(ppvMapFileBuf);
	ASSERT(pu4FileLength);

	do {
		UINT_32 u4FwSize = 0;
		PVOID prFwBuffer = NULL;

		/* <1> Open firmware */
		if (kalFirmwareOpen(prGlueInfo) != WLAN_STATUS_SUCCESS)
			break;

		/* <2> Query firmare size */
		kalFirmwareSize(prGlueInfo, &u4FwSize);
		/* <3> Use vmalloc for allocating large memory trunk */
		prFwBuffer = vmalloc(ALIGN_4(u4FwSize));
		/* <4> Load image binary into buffer */
		if (kalFirmwareLoad(prGlueInfo, prFwBuffer, 0, &u4FwSize) != WLAN_STATUS_SUCCESS) {
			vfree(prFwBuffer);
			kalFirmwareClose(prGlueInfo);
			break;
		}
		/* <5> write back info */
		*pu4FileLength = u4FwSize;
		*ppvMapFileBuf = prFwBuffer;

		return prFwBuffer;

	} while (FALSE);

	return NULL;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to unload firmware image mapped memory
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
* \param pvFwHandle     Pointer to mapping handle
* \param pvMapFileBuf   Pointer to memory-mapped firmware image
*
* \retval none
*/
/*----------------------------------------------------------------------------*/

VOID kalFirmwareImageUnmapping(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prFwHandle, IN PVOID pvMapFileBuf)
{
	DEBUGFUNC("kalFirmwareImageUnmapping");

	ASSERT(prGlueInfo);

	/* pvMapFileBuf might be NULL when file doesn't exist */
	if (pvMapFileBuf)
		vfree(pvMapFileBuf);

	kalFirmwareClose(prGlueInfo);
}

#endif

#if 0

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to load firmware image
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
* \param ppvMapFileBuf  Pointer of pointer to memory-mapped firmware image
* \param pu4FileLength  File length and memory mapped length as well

* \retval Map File Handle, used for unammping
*/
/*----------------------------------------------------------------------------*/

PVOID kalFirmwareImageMapping(IN P_GLUE_INFO_T prGlueInfo, OUT PPVOID ppvMapFileBuf, OUT PUINT_32 pu4FileLength)
{
	INT_32 i4Ret = 0;

	DEBUGFUNC("kalFirmwareImageMapping");

	ASSERT(prGlueInfo);
	ASSERT(ppvMapFileBuf);
	ASSERT(pu4FileLength);

	do {
		GL_HIF_INFO_T *prHifInfo = &prGlueInfo->rHifInfo;

		prGlueInfo->prFw = NULL;

		/* <1> Open firmware */
		i4Ret = request_firmware(&prGlueInfo->prFw, CFG_FW_FILENAME, &prHifInfo->func->dev);

		if (i4Ret) {
			DBGLOG(INIT, INFO, "fw %s:request failed %d\n", CFG_FW_FILENAME, i4Ret);
		} else {
			*pu4FileLength = prGlueInfo->prFw->size;
			*ppvMapFileBuf = prGlueInfo->prFw->data;
			return prGlueInfo->prFw->data;
		}

	} while (FALSE);

	return NULL;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to unload firmware image mapped memory
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
* \param pvFwHandle     Pointer to mapping handle
* \param pvMapFileBuf   Pointer to memory-mapped firmware image
*
* \retval none
*/
/*----------------------------------------------------------------------------*/

VOID kalFirmwareImageUnmapping(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prFwHandle, IN PVOID pvMapFileBuf)
{
	DEBUGFUNC("kalFirmwareImageUnmapping");

	ASSERT(prGlueInfo);
	ASSERT(pvMapFileBuf);

	release_firmware(prGlueInfo->prFw);

}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to acquire
*        OS SPIN_LOCK.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] rLockCategory  Specify which SPIN_LOCK
* \param[out] pu4Flags      Pointer of a variable for saving IRQ flags
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalAcquireSpinLock(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_SPIN_LOCK_CATEGORY_E rLockCategory, OUT PULONG plFlags)
{
	ULONG ulFlags = 0;

	ASSERT(prGlueInfo);
	ASSERT(plFlags);

	if (rLockCategory < SPIN_LOCK_NUM) {

#if CFG_USE_SPIN_LOCK_BOTTOM_HALF
		spin_lock_bh(&prGlueInfo->rSpinLock[rLockCategory]);
#else /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
		spin_lock_irqsave(&prGlueInfo->rSpinLock[rLockCategory], ulFlags);
#endif /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */

		*plFlags = ulFlags;
	}

}				/* end of kalAcquireSpinLock() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to release
*        OS SPIN_LOCK.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] rLockCategory  Specify which SPIN_LOCK
* \param[in] u4Flags        Saved IRQ flags
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalReleaseSpinLock(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_SPIN_LOCK_CATEGORY_E rLockCategory, IN ULONG ulFlags)
{
	ASSERT(prGlueInfo);

	if (rLockCategory < SPIN_LOCK_NUM) {

#if CFG_USE_SPIN_LOCK_BOTTOM_HALF
		spin_unlock_bh(&prGlueInfo->rSpinLock[rLockCategory]);
#else /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
		spin_unlock_irqrestore(&prGlueInfo->rSpinLock[rLockCategory], ulFlags);
#endif /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */

	}

}				/* end of kalReleaseSpinLock() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to acquire
*        OS MUTEX.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] rMutexCategory  Specify which MUTEX
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalAcquireMutex(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_MUTEX_CATEGORY_E rMutexCategory)
{
	ASSERT(prGlueInfo);

	if (rMutexCategory < MUTEX_NUM) {
		DBGLOG(INIT, TRACE, "MUTEX_LOCK[%u] Try to acquire\n", rMutexCategory);
		mutex_lock(&prGlueInfo->arMutex[rMutexCategory]);
		DBGLOG(INIT, TRACE, "MUTEX_LOCK[%u] Acquired\n", rMutexCategory);
	}

}				/* end of kalAcquireMutex() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to release
*        OS MUTEX.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] rMutexCategory  Specify which MUTEX
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalReleaseMutex(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_MUTEX_CATEGORY_E rMutexCategory)
{
	ASSERT(prGlueInfo);

	if (rMutexCategory < MUTEX_NUM) {
		mutex_unlock(&prGlueInfo->arMutex[rMutexCategory]);
		DBGLOG(INIT, TRACE, "MUTEX_UNLOCK[%u]\n", rMutexCategory);
	}

}				/* end of kalReleaseMutex() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is provided by GLUE Layer for internal driver stack to update
*        current MAC address.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pucMacAddr     Pointer of current MAC address
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalUpdateMACAddress(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucMacAddr)
{
	ASSERT(prGlueInfo);
	ASSERT(pucMacAddr);

	if (UNEQUAL_MAC_ADDR(prGlueInfo->prDevHandler->dev_addr, pucMacAddr))
		memcpy(prGlueInfo->prDevHandler->dev_addr, pucMacAddr, PARAM_MAC_ADDR_LEN);

}

#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
* \brief To query the packet information for offload related parameters.
*
* \param[in] pvPacket Pointer to the packet descriptor.
* \param[in] pucFlag  Points to the offload related parameter.
*
* \return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID kalQueryTxChksumOffloadParam(IN PVOID pvPacket, OUT PUINT_8 pucFlag)
{
	struct sk_buff *skb = (struct sk_buff *)pvPacket;
	UINT_8 ucFlag = 0;

	ASSERT(pvPacket);
	ASSERT(pucFlag);

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
#if DBG
		/* Kevin: do double check, we can remove this part in Normal Driver.
		 * Because we register NIC feature with NETIF_F_IP_CSUM for MT5912B MAC, so
		 * we'll process IP packet only.
		 */
		if (skb->protocol != htons(ETH_P_IP)) {
			/* DBGLOG(INIT, INFO, "Wrong skb->protocol( = %08x)
				 for TX Checksum Offload.\n", skb->protocol); */
		} else
#endif
			ucFlag |= (TX_CS_IP_GEN | TX_CS_TCP_UDP_GEN);
	}

	*pucFlag = ucFlag;

}				/* kalQueryChksumOffloadParam */

/* 4 2007/10/8, mikewu, this is rewritten by Mike */
/*----------------------------------------------------------------------------*/
/*!
* \brief To update the checksum offload status to the packet to be indicated to OS.
*
* \param[in] pvPacket Pointer to the packet descriptor.
* \param[in] pucFlag  Points to the offload related parameter.
*
* \return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID kalUpdateRxCSUMOffloadParam(IN PVOID pvPacket, IN ENUM_CSUM_RESULT_T aeCSUM[])
{
	struct sk_buff *skb = (struct sk_buff *)pvPacket;

	ASSERT(pvPacket);

	if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_SUCCESS || aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_SUCCESS)
	    && ((aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_SUCCESS)
		|| (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_SUCCESS))) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else {
		skb->ip_summed = CHECKSUM_NONE;
#if DBG
		if (aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_NONE && aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_NONE)
			DBGLOG(RX, TRACE, "RX: \"non-IPv4/IPv6\" Packet\n");
		else if (aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_FAILED)
			DBGLOG(RX, TRACE, "RX: \"bad IP Checksum\" Packet\n");
		else if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_FAILED)
			DBGLOG(RX, TRACE, "RX: \"bad TCP Checksum\" Packet\n");
		else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_FAILED)
			DBGLOG(RX, TRACE, "RX: \"bad UDP Checksum\" Packet\n");

#endif
	}

}				/* kalUpdateRxCSUMOffloadParam */
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to free packet allocated from kalPacketAlloc.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of the packet descriptor
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalPacketFree(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket)
{
	dev_kfree_skb((struct sk_buff *)pvPacket);
	if (prGlueInfo)
		prGlueInfo->u8SkbFreed++;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Only handles driver own creating packet (coalescing buffer).
*
* \param prGlueInfo   Pointer of GLUE Data Structure
* \param u4Size       Pointer of Packet Handle
* \param ppucData     Status Code for OS upper layer
*
* \return NULL: Failed to allocate skb, Not NULL get skb
*/
/*----------------------------------------------------------------------------*/
PVOID kalPacketAlloc(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Size, OUT PUINT_8 *ppucData)
{
	struct sk_buff *prSkb = dev_alloc_skb(u4Size);

	if (prSkb) {
		*ppucData = (PUINT_8) (prSkb->data);

		kalResetPacket(prGlueInfo, (P_NATIVE_PACKET) prSkb);
	}
#if DBG
	{
		PUINT_32 pu4Head = (PUINT_32) &prSkb->cb[0];
		*pu4Head = (UINT_32) prSkb->head;
		DBGLOG(RX, TRACE, "prSkb->head = %#lx, prSkb->cb = %#lx\n", (UINT_32) prSkb->head, *pu4Head);
	}
#endif
	return (PVOID) prSkb;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Process the received packet for indicating to OS.
*
* \param[in] prGlueInfo     Pointer to the Adapter structure.
* \param[in] pvPacket       Pointer of the packet descriptor
* \param[in] pucPacketStart The starting address of the buffer of Rx packet.
* \param[in] u4PacketLen    The packet length.
* \param[in] pfgIsRetain    Is the packet to be retained.
* \param[in] aerCSUM        The result of TCP/ IP checksum offload.
*
* \retval WLAN_STATUS_SUCCESS.
* \retval WLAN_STATUS_FAILURE.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
kalProcessRxPacket(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN PUINT_8 pucPacketStart, IN UINT_32 u4PacketLen,
		   /* IN PBOOLEAN           pfgIsRetain, */
		   IN BOOLEAN fgIsRetain, IN ENUM_CSUM_RESULT_T aerCSUM[])
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	struct sk_buff *skb = (struct sk_buff *)pvPacket;

	skb->data = (unsigned char *)pucPacketStart;

	/* Reset skb */
	skb_reset_tail_pointer(skb);
	skb_trim(skb, 0);

	/* Put data */
	skb_put(skb, u4PacketLen);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	kalUpdateRxCSUMOffloadParam(skb, aerCSUM);
#endif

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief To indicate an array of received packets is available for higher
*        level protocol uses.
*
* \param[in] prGlueInfo Pointer to the Adapter structure.
* \param[in] apvPkts The packet array to be indicated
* \param[in] ucPktNum The number of packets to be indicated
*
* \retval TRUE Success.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalRxIndicatePkts(IN P_GLUE_INFO_T prGlueInfo, IN PVOID apvPkts[], IN UINT_8 ucPktNum)
{
	UINT_8 ucIdx = 0;
	struct net_device *prNetDev = NULL;
	ASSERT(prGlueInfo);
	ASSERT(apvPkts);

	prNetDev = prGlueInfo->prDevHandler;
	for (ucIdx = 0; ucIdx < ucPktNum; ucIdx++)
		kalRxIndicateOnePkt(prGlueInfo, apvPkts[ucIdx]);
	if (netif_carrier_ok(prNetDev))
		kalPerMonStart(prGlueInfo);

	KAL_WAKE_LOCK_TIMEOUT(prGlueInfo->prAdapter, &prGlueInfo->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(WAKE_LOCK_RX_TIMEOUT));

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief To indicate one received packets is available for higher
*        level protocol uses.
*
* \param[in] prGlueInfo Pointer to the Adapter structure.
* \param[in] pvPkt The packet to be indicated
*
* \retval TRUE Success.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalRxIndicateOnePkt(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPkt)
{
	struct net_device *prNetDev = prGlueInfo->prDevHandler;
	struct sk_buff *prSkb = NULL;
	UINT_8 bssIdx = 0;

	ASSERT(prGlueInfo);
	ASSERT(pvPkt);

	prSkb = pvPkt;
#if DBG && 0
	do {
		PUINT_8 pu4Head = (PUINT_8) &prSkb->cb[0];
		UINT_32 u4HeadValue = 0;

		kalMemCopy(&u4HeadValue, pu4Head, sizeof(u4HeadValue));
		DBGLOG(RX, TRACE, "prSkb->head = 0x%p, prSkb->cb = 0x%x\n", pu4Head, u4HeadValue);
	} while (0);
#endif

#if 1
	bssIdx = GLUE_GET_PKT_BSS_IDX(prSkb);
	prNetDev = (struct net_device *)wlanGetNetInterfaceByBssIdx(prGlueInfo, bssIdx);
	if (!prNetDev) {
		prNetDev = prGlueInfo->prDevHandler;
	} else if (bssIdx == NET_DEV_P2P_IDX) {
		if (prGlueInfo->prAdapter->rP2PNetRegState == ENUM_NET_REG_STATE_UNREGISTERED) {
			DBGLOG(RX, INFO, "bssIdx = %d P2PNetRegState=%d\n",
				bssIdx, prGlueInfo->prAdapter->rP2PNetRegState);
			prNetDev = prGlueInfo->prDevHandler;
		}
	}
#if CFG_SUPPORT_SNIFFER
	if (prGlueInfo->fgIsEnableMon)
		prNetDev = prGlueInfo->prMonDevHandler;
#endif
	prNetDev->stats.rx_bytes += prSkb->len;
	prNetDev->stats.rx_packets++;
#else
	if (GLUE_GET_PKT_IS_P2P(prSkb)) {
		/* P2P */
#if CFG_ENABLE_WIFI_DIRECT
		if (prGlueInfo->prAdapter->fgIsP2PRegistered)
			prNetDev = kalP2PGetDevHdlr(prGlueInfo);
		/* prNetDev->stats.rx_bytes += prSkb->len; */
		/* prNetDev->stats.rx_packets++; */
		prGlueInfo->prP2PInfo->rNetDevStats.rx_bytes += prSkb->len;
		prGlueInfo->prP2PInfo->rNetDevStats.rx_packets++;

#else
		prNetDev = prGlueInfo->prDevHandler;
#endif
	} else if (GLUE_GET_PKT_IS_PAL(prSkb)) {
		/* BOW */
#if CFG_ENABLE_BT_OVER_WIFI && CFG_BOW_SEPARATE_DATA_PATH
		if (prGlueInfo->rBowInfo.fgIsNetRegistered)
			prNetDev = prGlueInfo->rBowInfo.prDevHandler;
#else
		prNetDev = prGlueInfo->prDevHandler;
#endif
	} else {
		/* AIS */
		prNetDev = prGlueInfo->prDevHandler;
		prGlueInfo->rNetDevStats.rx_bytes += prSkb->len;
		prGlueInfo->rNetDevStats.rx_packets++;

	}
#endif
	STATS_RX_PKT_INFO_DISPLAY(prSkb->data);
	prNetDev->last_rx = jiffies;
#if CFG_SUPPORT_SNIFFER
	if (prGlueInfo->fgIsEnableMon) {
		skb_reset_mac_header(prSkb);
		prSkb->ip_summed = CHECKSUM_UNNECESSARY;
		prSkb->pkt_type = PACKET_OTHERHOST;
		prSkb->protocol = htons(ETH_P_802_2);
	} else {
		prSkb->protocol = eth_type_trans(prSkb, prNetDev);
	}
#else
	prSkb->protocol = eth_type_trans(prSkb, prNetDev);
#endif
	prSkb->dev = prNetDev;
	/* DBGLOG_MEM32(RX, TRACE, (PUINT_32)prSkb->data, prSkb->len); */
	/* DBGLOG(RX, EVENT, ("kalRxIndicatePkts len = %d\n", prSkb->len)); */
	if (prSkb->tail > prSkb->end) {
		DBGLOG(RX, ERROR,
		       "kalRxIndicateOnePkt [prSkb = 0x%p][prSkb->len = %u][prSkb->protocol = 0x%02X] %p,%p\n",
			(PUINT_8) prSkb, prSkb->len, prSkb->protocol, prSkb->tail, prSkb->end);
		DBGLOG_MEM32(RX, ERROR, (PUINT_32) prSkb->data, prSkb->len);
	}
	if (!in_interrupt())
		netif_rx_ni(prSkb);	/* only in non-interrupt context */
	else
		netif_rx(prSkb);

	wlanReturnPacket(prGlueInfo->prAdapter, NULL);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Called by driver to indicate event to upper layer, for example, the wpa
*        supplicant or wireless tools.
*
* \param[in] pvAdapter Pointer to the adapter descriptor.
* \param[in] eStatus Indicated status.
* \param[in] pvBuf Indicated message buffer.
* \param[in] u4BufLen Indicated message buffer size.
*
* \return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID
kalIndicateStatusAndComplete(IN P_GLUE_INFO_T prGlueInfo, IN WLAN_STATUS eStatus, IN PVOID pvBuf, IN UINT_32 u4BufLen)
{

	UINT_32 bufLen;
	P_PARAM_STATUS_INDICATION_T pStatus = (P_PARAM_STATUS_INDICATION_T) pvBuf;
	P_PARAM_AUTH_EVENT_T pAuth = (P_PARAM_AUTH_EVENT_T) pStatus;
	P_PARAM_PMKID_CANDIDATE_LIST_T pPmkid = (P_PARAM_PMKID_CANDIDATE_LIST_T) (pStatus + 1);
	PARAM_MAC_ADDRESS arBssid;
	struct cfg80211_scan_request *prScanRequest = NULL;
	PARAM_SSID_T ssid;
	struct ieee80211_channel *prChannel = NULL;
	struct cfg80211_bss *bss;
	UINT_8 ucChannelNum;
	P_BSS_DESC_T prBssDesc = NULL;
	WLAN_STATUS scanStatus = WLAN_STATUS_SUCCESS;

	GLUE_SPIN_LOCK_DECLARATION();

	kalMemZero(arBssid, MAC_ADDR_LEN);

	ASSERT(prGlueInfo);

	switch (eStatus) {
	case WLAN_STATUS_ROAM_OUT_FIND_BEST:
	case WLAN_STATUS_MEDIA_CONNECT:

		prGlueInfo->eParamMediaStateIndicated = PARAM_MEDIA_STATE_CONNECTED;

		/* indicate assoc event */
		wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &bufLen);
		wext_indicate_wext_event(prGlueInfo, SIOCGIWAP, arBssid, bufLen);

		/* switch netif on */
		netif_carrier_on(prGlueInfo->prDevHandler);

		do {
			/* print message on console */
			wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQuerySsid, &ssid, sizeof(ssid), &bufLen);

			ssid.aucSsid[(ssid.u4SsidLen >= PARAM_MAX_LEN_SSID) ?
				     (PARAM_MAX_LEN_SSID - 1) : ssid.u4SsidLen] = '\0';
			DBGLOG(INIT, INFO, "[wifi] %s netif_carrier_on [ssid:%s " MACSTR "]\n",
					    prGlueInfo->prDevHandler->name, ssid.aucSsid, MAC2STR(arBssid));
		} while (0);

		if (prGlueInfo->fgIsRegistered == TRUE) {
			struct cfg80211_bss *bss_others = NULL;
			UINT_8 ucLoopCnt = 15; /* only loop 15 times to avoid dead loop */

			/* retrieve channel */
			ucChannelNum =
			    wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter,
							  prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);
			if (ucChannelNum <= 14) {
				prChannel =
				    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
							  ieee80211_channel_to_frequency
							  (ucChannelNum, IEEE80211_BAND_2GHZ));
			} else {
				prChannel =
				    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
							  ieee80211_channel_to_frequency
							  (ucChannelNum, IEEE80211_BAND_5GHZ));
			}

			if (!prChannel)
				DBGLOG(SCN, ERROR, "prChannel is NULL and ucChannelNum is %d\n", ucChannelNum);

			/* ensure BSS exists */
			bss = cfg80211_get_bss(priv_to_wiphy(prGlueInfo), prChannel, arBssid,
					       ssid.aucSsid, ssid.u4SsidLen, WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);

			if (bss == NULL) {
				/* create BSS on-the-fly */
				prBssDesc = ((P_AIS_FSM_INFO_T)
					     (&(prGlueInfo->prAdapter->rWifiVar.rAisFsmInfo)))->prTargetBssDesc;

				if (prBssDesc != NULL && prChannel) {
					bss = cfg80211_inform_bss(priv_to_wiphy(prGlueInfo),
								prChannel,
								CFG80211_BSS_FTYPE_PRESP,
								arBssid,
								0,	/* TSF */
								WLAN_CAPABILITY_ESS,
								prBssDesc->u2BeaconInterval,	/* beacon interval */
								prBssDesc->aucIEBuf,	/* IE */
								prBssDesc->u2IELength,	/* IE Length */
								RCPI_TO_dBm(prBssDesc->ucRCPI) * 100,	/* MBM */
								GFP_KERNEL);
				}
			}
			/* remove all bsses that before and only channel different with the current connected one
				if without this patch, UI will show channel A is connected even if AP has change channel
				from A to B */
			while (ucLoopCnt--) {
				bss_others = cfg80211_get_bss(priv_to_wiphy(prGlueInfo), NULL, arBssid,
						ssid.aucSsid, ssid.u4SsidLen, WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);
				if (bss && bss_others && bss_others != bss) {
					DBGLOG(SCN, INFO, "remove BSSes that only channel different\n");
					cfg80211_unlink_bss(priv_to_wiphy(prGlueInfo), bss_others);
				} else
					break;
			}

			/* CFG80211 Indication */
			if (eStatus == WLAN_STATUS_ROAM_OUT_FIND_BEST) {
				cfg80211_roamed_bss(prGlueInfo->prDevHandler,
						    bss,
						    prGlueInfo->aucReqIe,
						    prGlueInfo->u4ReqIeLength,
						    prGlueInfo->aucRspIe, prGlueInfo->u4RspIeLength, GFP_KERNEL);
			} else {
				cfg80211_connect_result(prGlueInfo->prDevHandler, arBssid,
							prGlueInfo->aucReqIe,
							prGlueInfo->u4ReqIeLength,
							prGlueInfo->aucRspIe,
							prGlueInfo->u4RspIeLength, WLAN_STATUS_SUCCESS, GFP_KERNEL);
			}
		}

		break;

	case WLAN_STATUS_MEDIA_DISCONNECT:
	case WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY:
		/* indicate disassoc event */
		wext_indicate_wext_event(prGlueInfo, SIOCGIWAP, NULL, 0);
		/* For CR 90 and CR99, While supplicant do reassociate, driver will do netif_carrier_off first,
		   after associated success, at joinComplete(), do netif_carier_on,
		   but for unknown reason, the supplicant 1x pkt will not called the driver
		   hardStartXmit, for template workaround these bugs, add this compiling flag
		 */
		/* switch netif off */

#if 1				/* CONSOLE_MESSAGE */
		DBGLOG(INIT, INFO, "[wifi] %s netif_carrier_off\n", prGlueInfo->prDevHandler->name);
#endif

		netif_carrier_off(prGlueInfo->prDevHandler);

		/*Full2Partial*/
		DBGLOG(INIT, TRACE, "Full2Partial disconenct reset value\n");
		prGlueInfo->u4LastFullScanTime = 0;
		prGlueInfo->ucTrScanType = 0;
		kalMemSet(prGlueInfo->ucChannelNum, 0, FULL_SCAN_MAX_CHANNEL_NUM);
		if (prGlueInfo->puFullScan2PartialChannel != NULL) {
			kalMemFree(prGlueInfo->puFullScan2PartialChannel,
			VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));
			prGlueInfo->puFullScan2PartialChannel = NULL;
		}

		if (prGlueInfo->fgIsRegistered == TRUE) {
			P_BSS_INFO_T prBssInfo = prGlueInfo->prAdapter->prAisBssInfo;
			UINT_16 u2DeauthReason = 0;

			if (prBssInfo)
				u2DeauthReason = prBssInfo->u2DeauthReason;
			/* CFG80211 Indication */
			cfg80211_disconnected(prGlueInfo->prDevHandler, u2DeauthReason, NULL, 0, GFP_KERNEL);
		}

		prGlueInfo->eParamMediaStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED;

		break;

	case WLAN_STATUS_SCAN_COMPLETE:
		if (pvBuf)
			scanStatus = *(WLAN_STATUS *)pvBuf;

		/* indicate scan complete event */
		wext_indicate_wext_event(prGlueInfo, SIOCGIWSCAN, NULL, 0);

		DBGLOG(SCN, EVENT, "scan complete, cfg80211 scan request is %p\n", prGlueInfo->prScanRequest);
		/* 1. reset first for newly incoming request */
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		if (prGlueInfo->prScanRequest != NULL) {
			prScanRequest = prGlueInfo->prScanRequest;
			prGlueInfo->prScanRequest = NULL;
		} else
			DBGLOG(SCN, WARN, "scan complete but cfg80211 scan request is NULL\n");
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

		/* 2. then CFG80211 Indication */
		if (prScanRequest != NULL) {
			if (scanStatus == WLAN_STATUS_SUCCESS)
				cfg80211_scan_done(prScanRequest, FALSE);
			else
				cfg80211_scan_done(prScanRequest, TRUE);
		}

		break;

#if 0
	case WLAN_STATUS_MSDU_OK:
		if (netif_running(prGlueInfo->prDevHandler))
			netif_wake_queue(prGlueInfo->prDevHandler);
		break;
#endif

	case WLAN_STATUS_MEDIA_SPECIFIC_INDICATION:
		if (pStatus) {
			switch (pStatus->eStatusType) {
			case ENUM_STATUS_TYPE_AUTHENTICATION:
				/* indicate (UC/GC) MIC ERROR event only */
				if ((pAuth->arRequest[0].u4Flags ==
				     PARAM_AUTH_REQUEST_PAIRWISE_ERROR) ||
				    (pAuth->arRequest[0].u4Flags == PARAM_AUTH_REQUEST_GROUP_ERROR)) {
					cfg80211_michael_mic_failure(prGlueInfo->prDevHandler, NULL,
								     (pAuth->arRequest[0].u4Flags ==
								      PARAM_AUTH_REQUEST_PAIRWISE_ERROR)
								     ? NL80211_KEYTYPE_PAIRWISE :
								     NL80211_KEYTYPE_GROUP, 0, NULL, GFP_KERNEL);
					wext_indicate_wext_event(prGlueInfo, IWEVMICHAELMICFAILURE,
								 (unsigned char *)&pAuth->arRequest[0],
								 pAuth->arRequest[0].u4Length);
				}
				break;

			case ENUM_STATUS_TYPE_CANDIDATE_LIST:
				{
					UINT_32 i = 0;

					P_PARAM_PMKID_CANDIDATE_T prPmkidCand =
					    (P_PARAM_PMKID_CANDIDATE_T) &pPmkid->arCandidateList[0];

					for (i = 0; i < pPmkid->u4NumCandidates; i++) {
						wext_indicate_wext_event(prGlueInfo,
									 IWEVPMKIDCAND,
									 (unsigned char *)&pPmkid->arCandidateList[i],
									 pPmkid->u4NumCandidates);
						prPmkidCand += sizeof(PARAM_PMKID_CANDIDATE_T);
					}
				}
				break;

			default:
				/* case ENUM_STATUS_TYPE_MEDIA_STREAM_MODE */
				/*
				 */
				break;
			}
		}
		break;

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	case WLAN_STATUS_BWCS_UPDATE:
		{
			wext_indicate_wext_event(prGlueInfo, IWEVCUSTOM, pvBuf, sizeof(PTA_IPC_T));
		}

		break;

#endif
	case WLAN_STATUS_JOIN_TIMEOUT:
		{
			P_BSS_DESC_T prBssDesc = prGlueInfo->prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;

			if (prBssDesc)
				COPY_MAC_ADDR(arBssid, prBssDesc->aucBSSID);
			else
				COPY_MAC_ADDR(arBssid, prGlueInfo->prAdapter->rWifiVar.rConnSettings.aucBSSID);
			cfg80211_connect_result(prGlueInfo->prDevHandler,
						arBssid,
						prGlueInfo->aucReqIe,
						prGlueInfo->u4ReqIeLength,
						prGlueInfo->aucRspIe,
						prGlueInfo->u4RspIeLength, WLAN_STATUS_AUTH_TIMEOUT, GFP_KERNEL);
			break;
		}
	default:
		break;
	}
}				/* kalIndicateStatusAndComplete */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to update the (re)association request
*        information to the structure used to query and set
*        OID_802_11_ASSOCIATION_INFORMATION.
*
* \param[in] prGlueInfo Pointer to the Glue structure.
* \param[in] pucFrameBody Pointer to the frame body of the last (Re)Association
*                         Request frame from the AP.
* \param[in] u4FrameBodyLen The length of the frame body of the last
*                           (Re)Association Request frame.
* \param[in] fgReassocRequest TRUE, if it is a Reassociation Request frame.
*
* \return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID
kalUpdateReAssocReqInfo(IN P_GLUE_INFO_T prGlueInfo,
			IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest)
{
	PUINT_8 cp;

	ASSERT(prGlueInfo);

	/* reset */
	prGlueInfo->u4ReqIeLength = 0;

	if (fgReassocRequest) {
		if (u4FrameBodyLen < 15)
			return;
	} else {
		if (u4FrameBodyLen < 9)
			return;
	}

	cp = pucFrameBody;

	if (fgReassocRequest) {
		/* Capability information field 2 */
		/* Listen interval field 2 */
		/* Current AP address 6 */
		cp += 10;
		u4FrameBodyLen -= 10;
	} else {
		/* Capability information field 2 */
		/* Listen interval field 2 */
		cp += 4;
		u4FrameBodyLen -= 4;
	}

	wext_indicate_wext_event(prGlueInfo, IWEVASSOCREQIE, cp, u4FrameBodyLen);

	if (u4FrameBodyLen <= CFG_CFG80211_IE_BUF_LEN) {
		prGlueInfo->u4ReqIeLength = u4FrameBodyLen;
		kalMemCopy(prGlueInfo->aucReqIe, cp, u4FrameBodyLen);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is called to update the (re)association
*        response information to the structure used to reply with
*        cfg80211_connect_result
*
* @param prGlueInfo      Pointer to adapter descriptor
* @param pucFrameBody    Pointer to the frame body of the last (Re)Association
*                         Response frame from the AP
* @param u4FrameBodyLen  The length of the frame body of the last
*                          (Re)Association Response frame
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalUpdateReAssocRspInfo(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen)
{
	UINT_32 u4IEOffset = 6;	/* cap_info, status_code & assoc_id */
	UINT_32 u4IELength = u4FrameBodyLen - u4IEOffset;

	ASSERT(prGlueInfo);

	/* reset */
	prGlueInfo->u4RspIeLength = 0;

	if (u4IELength <= CFG_CFG80211_IE_BUF_LEN) {
		prGlueInfo->u4RspIeLength = u4IELength;
		kalMemCopy(prGlueInfo->aucRspIe, pucFrameBody + u4IEOffset, u4IELength);
	}

}				/* kalUpdateReAssocRspInfo */

VOID kalResetPacket(IN P_GLUE_INFO_T prGlueInfo, IN P_NATIVE_PACKET prPacket)
{
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;

	/* Reset cb */
	kalMemZero(prSkb->cb, sizeof(prSkb->cb));
}

/*----------------------------------------------------------------------------*/
/*
* \brief This function is TX entry point of NET DEVICE.
*
* \param[in] prSkb  Pointer of the sk_buff to be sent
* \param[in] prDev  Pointer to struct net_device
* \param[in] prGlueInfo  Pointer of prGlueInfo
* \param[in] ucBssIndex  BSS index of this net device
*
* \retval WLAN_STATUS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
kalHardStartXmit(struct sk_buff *prSkb, IN struct net_device *prDev, P_GLUE_INFO_T prGlueInfo, UINT_8 ucBssIndex)
{
	P_QUE_ENTRY_T prQueueEntry = NULL;
	P_QUE_T prTxQueue = NULL;
	UINT_16 u2QueueIdx = 0;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prSkb);
	ASSERT(prGlueInfo);

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		DBGLOG(INIT, INFO, "GLUE_FLAG_HALT skip tx\n");
		dev_kfree_skb(prSkb);
		prGlueInfo->u8SkbFreed++;
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	prQueueEntry = (P_QUE_ENTRY_T) GLUE_GET_PKT_QUEUE_ENTRY(prSkb);
	prTxQueue = &prGlueInfo->rTxQueue;

	GLUE_SET_PKT_BSS_IDX(prSkb, ucBssIndex);

#if CFG_DBG_GPIO_PINS
	/* TX request from OS */
	mtk_wcn_stp_debug_gpio_assert(IDX_TX_REQ, DBG_TIE_LOW);
	kalUdelay(1);
	mtk_wcn_stp_debug_gpio_assert(IDX_TX_REQ, DBG_TIE_HIGH);
#endif

	/* Parsing frame info */
	if (!wlanProcessTxFrame(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prSkb)) {
		/* Cannot extract packet */
		DBGLOG(INIT, INFO, "Cannot extract content, skip this frame\n");
		dev_kfree_skb(prSkb);
		prGlueInfo->u8SkbFreed++;
		return WLAN_STATUS_INVALID_PACKET;
	}

	/* Tx profiling */
	wlanTxProfilingTagPacket(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prSkb, TX_PROF_TAG_OS_TO_DRV);

	/* Handle normal data frame */
	u2QueueIdx = skb_get_queue_mapping(prSkb);

	if (u2QueueIdx >= CFG_MAX_TXQ_NUM) {
		DBGLOG(INIT, INFO, "Incorrect queue index, skip this frame\n");
		dev_kfree_skb(prSkb);
		prGlueInfo->u8SkbFreed++;
		return WLAN_STATUS_INVALID_PACKET;
	}
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
	QUEUE_INSERT_TAIL(prTxQueue, prQueueEntry);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

	GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
	GLUE_INC_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex][u2QueueIdx]);

	if (GLUE_GET_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex][u2QueueIdx])
	    >= prGlueInfo->prAdapter->rWifiVar.u4NetifStopTh) {
		netif_stop_subqueue(prDev, u2QueueIdx);

		DBGLOG(TX, INFO,
		       "Stop subqueue for BSS[%d] QIDX[%d] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
			ucBssIndex, u2QueueIdx, prSkb->len,
			GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
			GLUE_GET_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex]
					 [u2QueueIdx]));
	}

	/* Update NetDev statisitcs */
	prDev->stats.tx_bytes += prSkb->len;
	prDev->stats.tx_packets++;

	DBGLOG(TX, LOUD,
	       "Enqueue frame for BSS[%d] QIDX[%d] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
		ucBssIndex, u2QueueIdx, prSkb->len,
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
		GLUE_GET_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex][u2QueueIdx]));

	kalSetEvent(prGlueInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of kalHardStartXmit() */

WLAN_STATUS kalResetStats(IN struct net_device *prDev)
{
	DBGLOG(QM, LOUD, "Reset NetDev[0x%p] statistics\n", prDev);

	kalMemZero(kalGetStats(prDev), sizeof(struct net_device_stats));

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief A method of struct net_device, to get the network interface statistical
*        information.
*
* Whenever an application needs to get statistics for the interface, this method
* is called. This happens, for example, when ifconfig or netstat -i is run.
*
* \param[in] prDev      Pointer to struct net_device.
*
* \return net_device_stats buffer pointer.
*/
/*----------------------------------------------------------------------------*/
PVOID kalGetStats(IN struct net_device *prDev)
{
	return (PVOID) &prDev->stats;
}				/* end of wlanGetStats() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Notify OS with SendComplete event of the specific packet. Linux should
*        free packets here.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
* \param[in] status         Status Code for OS upper layer
*
* \return -
*/
/*----------------------------------------------------------------------------*/
VOID kalSendCompleteAndAwakeQueue(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket)
{
	struct net_device *prDev = NULL;
	struct sk_buff *prSkb = NULL;
	UINT_16 u2QueueIdx = 0;
	UINT_8 ucBssIndex = 0;
	BOOLEAN fgIsValidDevice = TRUE;

	ASSERT(pvPacket);
	/* ASSERT(prGlueInfo->i4TxPendingFrameNum); */

	prSkb = (struct sk_buff *)pvPacket;
	u2QueueIdx = skb_get_queue_mapping(prSkb);
	ASSERT(u2QueueIdx < CFG_MAX_TXQ_NUM);

	ucBssIndex = GLUE_GET_PKT_BSS_IDX(pvPacket);

#if CFG_ENABLE_WIFI_DIRECT
	{
		P_BSS_INFO_T prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, ucBssIndex);

		/* in case packet was sent after P2P device is unregistered */
		if ((prBssInfo->eNetworkType == NETWORK_TYPE_P2P) &&
		    (prGlueInfo->prAdapter->fgIsP2PRegistered == FALSE)) {
			fgIsValidDevice = FALSE;
		}
	}
#endif

#if 0
	if ((GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum) <= 0)) {
		UINT_8 ucBssIdx;
		UINT_16 u2QIdx;

		DBGLOG(INIT, INFO, "TxPendingFrameNum[%u] CurFrameId[%u]\n", prGlueInfo->i4TxPendingFrameNum,
		       GLUE_GET_PKT_ARRIVAL_TIME(pvPacket));

		for (ucBssIdx = 0; ucBssIdx < HW_BSSID_NUM; ucBssIdx++) {
			for (u2QIdx = 0; u2QIdx < CFG_MAX_TXQ_NUM; u2QIdx++) {
				DBGLOG(INIT, INFO, "BSS[%u] Q[%u] TxPendingFrameNum[%u]\n",
				       ucBssIdx, u2QIdx, prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIdx][u2QIdx]);
			}
		}
	}

	ASSERT((GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum) > 0));
#endif

	GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
	GLUE_DEC_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex][u2QueueIdx]);

	DBGLOG(TX, LOUD,
	       "Release frame for BSS[%d] QIDX[%d] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
		ucBssIndex, u2QueueIdx, prSkb->len,
		GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
		GLUE_GET_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex][u2QueueIdx]));

	prDev = prSkb->dev;

	ASSERT(prDev);

	if (fgIsValidDevice == TRUE) {
		UINT_32 u4StartTh = prGlueInfo->prAdapter->rWifiVar.u4NetifStartTh;

		if (netif_subqueue_stopped(prDev, prSkb) &&
		    prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex][u2QueueIdx] <= u4StartTh) {
			netif_wake_subqueue(prDev, u2QueueIdx);
			DBGLOG(TX, INFO,
			       "WakeUp Queue BSS[%d] QIDX[%d] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
				ucBssIndex, u2QueueIdx, prSkb->len,
				GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
				GLUE_GET_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex]
						 [u2QueueIdx]));
		}
	}

	dev_kfree_skb((struct sk_buff *)pvPacket);
	prGlueInfo->u8SkbFreed++;
	DBGLOG(TX, LOUD, "----- pending frame %d -----\n", prGlueInfo->i4TxPendingFrameNum);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Copy Mac Address setting from registry. It's All Zeros in Linux.
*
* \param[in] prAdapter Pointer to the Adapter structure
*
* \param[out] paucMacAddr Pointer to the Mac Address buffer
*
* \retval WLAN_STATUS_SUCCESS
*
* \note
*/
/*----------------------------------------------------------------------------*/
VOID kalQueryRegistryMacAddr(IN P_GLUE_INFO_T prGlueInfo, OUT PUINT_8 paucMacAddr)
{
	UINT_8 aucZeroMac[MAC_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 }

	DEBUGFUNC("kalQueryRegistryMacAddr");

	ASSERT(prGlueInfo);
	ASSERT(paucMacAddr);

	kalMemCopy((PVOID) paucMacAddr, (PVOID) aucZeroMac, MAC_ADDR_LEN);

}				/* end of kalQueryRegistryMacAddr() */

#if CFG_SUPPORT_EXT_CONFIG
/*----------------------------------------------------------------------------*/
/*!
* \brief Read external configuration, ex. NVRAM or file
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
*
* \return none
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalReadExtCfg(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* External data is given from user space by ioctl or /proc, not read by
	   driver.
	 */
	if (0 != prGlueInfo->u4ExtCfgLength)
		DBGLOG(INIT, TRACE, "Read external configuration data -- OK\n");
	else
		DBGLOG(INIT, TRACE, "Read external configuration data -- fail\n");

	return prGlueInfo->u4ExtCfgLength;
}
#endif

BOOLEAN
kalIPv4FrameClassifier(IN P_GLUE_INFO_T prGlueInfo,
		       IN P_NATIVE_PACKET prPacket, IN PUINT_8 pucIpHdr,
		       OUT P_TX_PACKET_INFO prTxPktInfo)
{
	UINT_8 ucIpVersion, ucIcmpType;
	UINT_8 ucIpProto;
	UINT_8 ucSeqNo;
	PUINT_8 pucUdpHdr, pucIcmp;
	UINT_16 u2DstPort, u2IcmpId, u2IcmpSeq;
	P_BOOTP_PROTOCOL_T prBootp;
	UINT_32 u4DhcpMagicCode;

	/* UINT_16 u2IpId; */

	/* IPv4 version check */
	ucIpVersion = (pucIpHdr[0] & IP_VERSION_MASK) >> IP_VERSION_OFFSET;
	if (ucIpVersion != IP_VERSION_4) {
		DBGLOG(TX, WARN, "Invalid IPv4 packet version: %u\n", ucIpVersion);
		return FALSE;
	}
	/* WLAN_GET_FIELD_16(&pucIpHdr[IPV4_HDR_IP_IDENTIFICATION_OFFSET], &u2IpId); */

	ucIpProto = pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET];

	if (ucIpProto == IP_PRO_UDP) {
		pucUdpHdr = &pucIpHdr[IPV4_HDR_LEN];

		/* DBGLOG_MEM8(INIT, INFO, pucUdpHdr, 256); */

		/* Get UDP DST port */
		WLAN_GET_FIELD_BE16(&pucUdpHdr[UDP_HDR_DST_PORT_OFFSET], &u2DstPort);

		/* DBGLOG(INIT, INFO, ("UDP DST[%u]\n", u2DstPort)); */

		/* Get UDP SRC port */
		/* WLAN_GET_FIELD_BE16(&pucUdpHdr[UDP_HDR_SRC_PORT_OFFSET], &u2SrcPort); */

		/* BOOTP/DHCP protocol */
		if ((u2DstPort == IP_PORT_BOOTP_SERVER) ||
			(u2DstPort == IP_PORT_BOOTP_CLIENT)) {

			prBootp = (P_BOOTP_PROTOCOL_T) &pucUdpHdr[UDP_HDR_LEN];

			WLAN_GET_FIELD_BE32(&prBootp->aucOptions[0], &u4DhcpMagicCode);

			if (u4DhcpMagicCode == DHCP_MAGIC_NUMBER) {
				UINT_32 u4Xid;

				WLAN_GET_FIELD_BE32(&prBootp->u4TransId, &u4Xid);

				ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
				GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

				DBGLOG(TX, INFO, "DHCP PKT[0x%p] XID[0x%08x] OPT[%u] TYPE[%u], SeqNo: %d\n",
						   prPacket, u4Xid, prBootp->aucOptions[4], prBootp->aucOptions[6],
						   ucSeqNo);

				prTxPktInfo->u2Flag |= BIT(ENUM_PKT_DHCP);
			}
		}
	} else if (ucIpProto == IP_PRO_ICMP) {
			/* the number of ICMP packets is seldom so we print log here */
			pucIcmp = &pucIpHdr[20];

			ucIcmpType = pucIcmp[0];
			if (ucIcmpType == 3) /* don't log network unreachable packet */
				return FALSE;
			u2IcmpId = *(UINT_16 *) &pucIcmp[4];
			u2IcmpSeq = *(UINT_16 *) &pucIcmp[6];

			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
			DBGLOG(SW4, INFO, "<TX> ICMP: Type %d, Id 0x04%x, Seq BE 0x%04x, SeqNo: %d\n",
					ucIcmpType, u2IcmpId, u2IcmpSeq, ucSeqNo);
			prTxPktInfo->u2Flag |= BIT(ENUM_PKT_ICMP);
		}

	return TRUE;
}

BOOLEAN
kalArpFrameClassifier(IN P_GLUE_INFO_T prGlueInfo,
		       IN P_NATIVE_PACKET prPacket, IN PUINT_8 pucIpHdr, OUT P_TX_PACKET_INFO prTxPktInfo)
{
	UINT_16 u2ArpOp;
	UINT_8 ucSeqNo;

	ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
	WLAN_GET_FIELD_BE16(&pucIpHdr[ARP_OPERATION_OFFSET], &u2ArpOp);

	DBGLOG(TX, INFO, "ARP %s PKT[0x%p] TAR MAC/IP[" MACSTR "]/[" IPV4STR "], SeqNo: %d\n",
			   u2ArpOp == ARP_OPERATION_REQUEST ? "REQ" : "RSP",
			   prPacket, MAC2STR(&pucIpHdr[ARP_TARGET_MAC_OFFSET]),
			   IPV4TOSTR(&pucIpHdr[ARP_TARGET_IP_OFFSET]), ucSeqNo);

	GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

	prTxPktInfo->u2Flag |= BIT(ENUM_PKT_ARP);
	return TRUE;
}

BOOLEAN
kalTdlsFrameClassifier(IN P_GLUE_INFO_T prGlueInfo,
		       IN P_NATIVE_PACKET prPacket, IN PUINT_8 pucIpHdr, OUT P_TX_PACKET_INFO prTxPktInfo)
{
	UINT_8 ucSeqNo;
	UINT_8 ucActionCode;

	ucActionCode = pucIpHdr[TDLS_ACTION_CODE_OFFSET];

	DBGLOG(TX, INFO, "TDLS action code: %d\n", ucActionCode);

	ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);

	GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

	prTxPktInfo->u2Flag |= BIT(ENUM_PKT_TDLS);

	return TRUE;
}

BOOLEAN
kalSecurityFrameClassifier(IN P_GLUE_INFO_T prGlueInfo,
		       IN P_NATIVE_PACKET prPacket, IN PUINT_8 pucIpHdr,
		       IN UINT_16 u2EthType, OUT P_TX_PACKET_INFO prTxPktInfo)
{
	PUINT_8 pucEapol;
	UINT_8 ucEapolType;
	UINT_8 ucSeqNo;

	UINT_8 ucSubType; /* sub type filed*/
	UINT_16 u2Length;
	UINT_16 u2Seq;

	pucEapol = pucIpHdr;

	if (u2EthType == ETH_P_1X) {

		ucEapolType = pucEapol[1];

		switch (ucEapolType) {
		case 0: /* eap packet */

			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

			DBGLOG(SW4, INFO, "<TX> EAP Packet: code %d, id %d, type %d, SeqNo: %d\n",
					pucEapol[4], pucEapol[5], pucEapol[7], ucSeqNo);
			break;
		case 1: /* eapol start */
			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

			DBGLOG(SW4, INFO, "<TX> EAPOL: start, SeqNo: %d\n", ucSeqNo);
			break;
		case 3: /* key */

			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

			DBGLOG(SW4, INFO, "<TX> EAPOL: key, KeyInfo 0x%04x, SeqNo: %d\n",
					*((PUINT_16)(&pucEapol[5])), ucSeqNo);
			break;
		}

	} else if (u2EthType == ETH_WPI_1X) {

		ucSubType = pucEapol[3]; /* sub type filed*/
		u2Length = *(PUINT_16)&pucEapol[6];
		u2Seq = *(PUINT_16)&pucEapol[8];
		ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
		GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

		DBGLOG(SW4, INFO, "<TX> WAPI: subType %d, Len %d, Seq %d, SeqNo: %d\n",
				ucSubType, u2Length, u2Seq, ucSeqNo);

	}
	prTxPktInfo->u2Flag |= BIT(ENUM_PKT_1X);
	return TRUE;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This inline function is to extract some packet information, including
*        user priority, packet length, destination address, 802.1x and BT over Wi-Fi
*        or not.
*
* @param prGlueInfo         Pointer to the glue structure
* @param prPacket           Packet descriptor
* @param prTxPktInfo        Extracted packet info
*
* @retval TRUE      Success to extract information
* @retval FALSE     Fail to extract correct information
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalQoSFrameClassifierAndPacketInfo(IN P_GLUE_INFO_T prGlueInfo,
				   IN P_NATIVE_PACKET prPacket, OUT P_TX_PACKET_INFO prTxPktInfo)
{
	UINT_32 u4PacketLen;
	UINT_16 u2EtherType;
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	PUINT_8 aucLookAheadBuf = NULL;
	UINT_8 ucEthTypeLenOffset = ETHER_HEADER_LEN - ETHER_TYPE_LEN;
	PUINT_8 pucNextProtocol = NULL;

	u4PacketLen = prSkb->len;

	if (u4PacketLen < ETHER_HEADER_LEN) {
		DBGLOG(INIT, WARN, "Invalid Ether packet length: %u\n", u4PacketLen);
		return FALSE;
	}

	aucLookAheadBuf = prSkb->data;

	/* Reset Packet Info */
	kalMemZero(prTxPktInfo, sizeof(TX_PACKET_INFO));

	/* 4 <0> Obtain Ether Type/Len */
	WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset], &u2EtherType);

	/* 4 <1> Skip 802.1Q header (VLAN Tagging) */
	if (u2EtherType == ETH_P_VLAN) {
		prTxPktInfo->u2Flag |= BIT(ENUM_PKT_VLAN_EXIST);
		ucEthTypeLenOffset += ETH_802_1Q_HEADER_LEN;
		WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset], &u2EtherType);
	}
	/* 4 <2> Obtain next protocol pointer */
	pucNextProtocol = &aucLookAheadBuf[ucEthTypeLenOffset + ETHER_TYPE_LEN];

	/* 4 <3> Handle ethernet format */
	switch (u2EtherType) {

	case ETH_P_IPV4:
		/* IPv4 header length check */
		if (u4PacketLen < (ucEthTypeLenOffset + ETHER_TYPE_LEN + IPV4_HDR_LEN)) {
			DBGLOG(INIT, WARN, "Invalid IPv4 packet length: %u\n", u4PacketLen);
			break;
		}

		kalIPv4FrameClassifier(prGlueInfo, prPacket, pucNextProtocol, prTxPktInfo);
		break;

	case ETH_P_ARP:

		kalArpFrameClassifier(prGlueInfo, prPacket, pucNextProtocol, prTxPktInfo);
		break;

	case ETH_P_1X:
	case ETH_P_PRE_1X:
#if CFG_SUPPORT_WAPI
	case ETH_WPI_1X:
#endif
		kalSecurityFrameClassifier(prGlueInfo, prPacket, pucNextProtocol,
			u2EtherType, prTxPktInfo);
		break;

	case ETH_PRO_TDLS:
		kalTdlsFrameClassifier(prGlueInfo, prPacket, pucNextProtocol, prTxPktInfo);
		break;
	default:
		/* 4 <4> Handle 802.3 format if LEN <= 1500 */
		if (u2EtherType <= ETH_802_3_MAX_LEN)
			prTxPktInfo->u2Flag |= BIT(ENUM_PKT_802_3);
		break;
	}

	/* 4 <4.1> Check for PAL (BT over Wi-Fi) */
	/* Move to kalBowFrameClassifier */

	/* 4 <5> Return the value of Priority Parameter. */
	/* prSkb->priority is assigned by Linux wireless utility function(cfg80211_classify8021d) */
	/* at net_dev selection callback (ndo_select_queue) */
	prTxPktInfo->ucPriorityParam = prSkb->priority;

	/* 4 <6> Retrieve Packet Information - DA */
	/* Packet Length/ Destination Address */
	prTxPktInfo->u4PacketLen = u4PacketLen;

	kalMemCopy(prTxPktInfo->aucEthDestAddr, aucLookAheadBuf, PARAM_MAC_ADDR_LEN);

	return TRUE;
}				/* end of kalQoSFrameClassifier() */

BOOLEAN kalGetEthDestAddr(IN P_GLUE_INFO_T prGlueInfo, IN P_NATIVE_PACKET prPacket, OUT PUINT_8 pucEthDestAddr)
{
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	PUINT_8 aucLookAheadBuf = NULL;

	/* Sanity Check */
	if (!prPacket || !prGlueInfo)
		return FALSE;

	aucLookAheadBuf = prSkb->data;

	kalMemCopy(pucEthDestAddr, aucLookAheadBuf, PARAM_MAC_ADDR_LEN);

	return TRUE;
}

VOID
kalOidComplete(IN P_GLUE_INFO_T prGlueInfo,
	       IN BOOLEAN fgSetQuery, IN UINT_32 u4SetQueryInfoLen, IN WLAN_STATUS rOidStatus)
{
	ASSERT(prGlueInfo);
	/* remove timeout check timer */
	wlanoidClearTimeoutCheck(prGlueInfo->prAdapter);

	prGlueInfo->rPendStatus = rOidStatus;

	prGlueInfo->u4OidCompleteFlag = 1;
	/* complete ONLY if there are waiters */
	if (!completion_done(&prGlueInfo->rPendComp)) {
		complete(&prGlueInfo->rPendComp);
	} else {
		DBGLOG(INIT, WARN, "SKIP multiple OID complete!\n");
		WARN_ON(TRUE);
	}

	/* else let it timeout on kalIoctl entry */
}

VOID kalOidClearance(IN P_GLUE_INFO_T prGlueInfo)
{

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to transfer linux ioctl to OID, and  we
* need to specify the behavior of the OID by ourself
*
* @param prGlueInfo         Pointer to the glue structure
* @param pvInfoBuf          Data buffer
* @param u4InfoBufLen       Data buffer length
* @param fgRead             Is this a read OID
* @param fgWaitResp         does this OID need to wait for values
* @param fgCmd              does this OID compose command packet
* @param pu4QryInfoLen      The data length of the return values
*
* @retval TRUE      Success to extract information
* @retval FALSE     Fail to extract correct information
*/
/*----------------------------------------------------------------------------*/

/* todo: enqueue the i/o requests for multiple processes access */
/*  */
/* currently, return -1 */
/*  */

/* static GL_IO_REQ_T OidEntry; */

WLAN_STATUS
kalIoctl(IN P_GLUE_INFO_T prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN PVOID pvInfoBuf,
	 IN UINT_32 u4InfoBufLen, IN BOOL fgRead, IN BOOL fgWaitResp, IN BOOL fgCmd, OUT PUINT_32 pu4QryInfoLen)
{
	P_GL_IO_REQ_T prIoReq = NULL;
	WLAN_STATUS ret = WLAN_STATUS_SUCCESS;

	if (fgIsResetting == TRUE)
		return WLAN_STATUS_SUCCESS;

	/* GLUE_SPIN_LOCK_DECLARATION(); */
	ASSERT(prGlueInfo);

	/* if wait longer than double OID timeout timer, then will show backtrace who held halt lock.
		at this case, we will return kalIoctl failure because tx_thread may be hung */
	if (kalHaltLock(2 * WLAN_OID_TIMEOUT_THRESHOLD))
		return WLAN_STATUS_FAILURE;

	if (kalIsHalted()) {
		kalHaltUnlock();
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (down_interruptible(&prGlueInfo->ioctl_sem)) {
		kalHaltUnlock();
		return WLAN_STATUS_FAILURE;
	}
	rHaltCtrl.fgHeldByKalIoctl = TRUE;
	/* <2> TODO: thread-safe */

	/* <3> point to the OidEntry of Glue layer */

	prIoReq = &(prGlueInfo->OidEntry);

	ASSERT(prIoReq);

	/* <4> Compose the I/O request */
	prIoReq->prAdapter = prGlueInfo->prAdapter;
	prIoReq->pfnOidHandler = pfnOidHandler;
	prIoReq->pvInfoBuf = pvInfoBuf;
	prIoReq->u4InfoBufLen = u4InfoBufLen;
	prIoReq->pu4QryInfoLen = pu4QryInfoLen;
	prIoReq->fgRead = fgRead;
	prIoReq->fgWaitResp = fgWaitResp;
	prIoReq->rStatus = WLAN_STATUS_FAILURE;

	/* <5> Reset the status of pending OID */
	prGlueInfo->rPendStatus = WLAN_STATUS_FAILURE;
	/* prGlueInfo->u4TimeoutFlag = 0; */
	prGlueInfo->u4OidCompleteFlag = 0;

	/* <6> Check if we use the command queue */
	prIoReq->u4Flag = fgCmd;

	/* <7> schedule the OID bit */
	set_bit(GLUE_FLAG_OID_BIT, &prGlueInfo->ulFlag);

	/* <7.1> Hold wakelock to ensure OS won't be suspended */
	KAL_WAKE_LOCK_TIMEOUT(prGlueInfo->prAdapter, &prGlueInfo->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(WAKE_LOCK_THREAD_WAKEUP_TIMEOUT));

	/* <8> Wake up tx thread to handle kick start the I/O request */
	wake_up_interruptible(&prGlueInfo->waitq);

	/* <9> Block and wait for event or timeout, current the timeout is 2 secs */
	wait_for_completion(&prGlueInfo->rPendComp);
	{
		/* Case 1: No timeout. */
		/* if return WLAN_STATUS_PENDING, the status of cmd is stored in prGlueInfo  */
		if (prIoReq->rStatus == WLAN_STATUS_PENDING)
			ret = prGlueInfo->rPendStatus;
		else
			ret = prIoReq->rStatus;
	}
#if 0
	else {
		/* Case 2: timeout */
		/* clear pending OID's cmd in CMD queue */
		if (fgCmd) {
			prGlueInfo->u4TimeoutFlag = 1;
			wlanReleasePendingOid(prGlueInfo->prAdapter, 0);
		}
		ret = WLAN_STATUS_FAILURE;
	}
#endif

	/* <10> Clear bit for error handling */
	clear_bit(GLUE_FLAG_OID_BIT, &prGlueInfo->ulFlag);

	up(&prGlueInfo->ioctl_sem);
	rHaltCtrl.fgHeldByKalIoctl = FALSE;
	kalHaltUnlock();
	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all pending security frames
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearSecurityFrames(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	QUE_T rReturnCmdQue;
	P_QUE_T prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;

	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending security frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
			if (prCmdInfo->pfCmdTimeoutHandler)
				prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter, prCmdInfo);
			else
				wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear pending security frames
*        belongs to dedicated network type
*
* \param prGlueInfo         Pointer of GLUE Data Structure
* \param eNetworkTypeIdx    Network Type Index
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearSecurityFramesByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	QUE_T rReturnCmdQue;
	P_QUE_T prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;

	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending security frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME && prCmdInfo->ucBssIndex == ucBssIndex) {
			if (prCmdInfo->pfCmdTimeoutHandler)
				prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter, prCmdInfo);
			else
				wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all pending management frames
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearMgmtFrames(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	QUE_T rReturnCmdQue;
	P_QUE_T prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending management frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
			wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all pending management frames
*           belongs to dedicated network type
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearMgmtFramesByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	QUE_T rReturnCmdQue;
	P_QUE_T prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending management frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME && prCmdInfo->ucBssIndex == ucBssIndex) {
			wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}				/* kalClearMgmtFramesByBssIdx */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all commands in command queue
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearCommandQueue(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	QUE_T rReturnCmdQue;
	P_QUE_T prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);

	/* Clear ALL in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter, prCmdInfo);
		else
			wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);

		cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}
}

UINT_32 kalProcessTxPacket(P_GLUE_INFO_T prGlueInfo, struct sk_buff *prSkb)
{
	UINT_32 u4Status = WLAN_STATUS_SUCCESS;

	if (NULL == prSkb) {
		DBGLOG(INIT, WARN, "prSkb == NULL in tx\n");
		return u4Status;
	}

	/* Handle security frame */
	if (GLUE_TEST_PKT_FLAG(prSkb, ENUM_PKT_1X)) {
		if (wlanProcessSecurityFrame(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prSkb)) {
			u4Status = WLAN_STATUS_SUCCESS;
			GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);
		} else {
			u4Status = WLAN_STATUS_RESOURCES;
		}
	}
	/* Handle normal frame */
	else
		u4Status = wlanEnqueueTxPacket(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prSkb);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to process Tx request to tx_thread
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalProcessTxReq(P_GLUE_INFO_T prGlueInfo, PBOOLEAN pfgNeedHwAccess)
{
	P_QUE_T prCmdQue = NULL;
	P_QUE_T prTxQueue = NULL;
	QUE_T rTempQue;
	P_QUE_T prTempQue = &rTempQue;
	QUE_T rTempReturnQue;
	P_QUE_T prTempReturnQue = &rTempReturnQue;
	P_QUE_ENTRY_T prQueueEntry = NULL;
	/* struct sk_buff      *prSkb = NULL; */
	UINT_32 u4Status;
#if CFG_SUPPORT_MULTITHREAD
	UINT_32 u4CmdCount = 0;
#endif
	UINT_32 u4TxLoopCount;

	/* for spin lock acquire and release */
	GLUE_SPIN_LOCK_DECLARATION();

	prTxQueue = &prGlueInfo->rTxQueue;
	prCmdQue = &prGlueInfo->rCmdQueue;

	QUEUE_INITIALIZE(prTempQue);
	QUEUE_INITIALIZE(prTempReturnQue);

	u4TxLoopCount = prGlueInfo->prAdapter->rWifiVar.u4TxFromOsLoopCount;

	/* Process Mailbox Messages */
	wlanProcessMboxMessage(prGlueInfo->prAdapter);

	/* Process CMD request */
#if CFG_SUPPORT_MULTITHREAD
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	u4CmdCount = prCmdQue->u4NumElem;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	if (u4CmdCount > 0)
		wlanProcessCommandQueue(prGlueInfo->prAdapter, prCmdQue);
#else
	if (prCmdQue->u4NumElem > 0) {
		if (*pfgNeedHwAccess == FALSE) {
			*pfgNeedHwAccess = TRUE;

			wlanAcquirePowerControl(prGlueInfo->prAdapter);
		}
		wlanProcessCommandQueue(prGlueInfo->prAdapter, prCmdQue);
	}
#endif


	while (u4TxLoopCount--) {
		while (QUEUE_IS_NOT_EMPTY(prTxQueue)) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
			QUEUE_MOVE_ALL(prTempQue, prTxQueue);
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

			/* Handle Packet Tx */
			while (QUEUE_IS_NOT_EMPTY(prTempQue)) {
				QUEUE_REMOVE_HEAD(prTempQue, prQueueEntry, P_QUE_ENTRY_T);

				if (NULL == prQueueEntry)
					break;

				u4Status =
				    kalProcessTxPacket(prGlueInfo,
						       (struct sk_buff *)GLUE_GET_PKT_DESCRIPTOR(prQueueEntry));
#if 0
				prSkb = (struct sk_buff *)GLUE_GET_PKT_DESCRIPTOR(prQueueEntry);
				ASSERT(prSkb);
				if (NULL == prSkb) {
					DBGLOG(INIT, WARN, "prSkb == NULL in tx\n");
					continue;
				}

				/* Handle security frame */
				if (GLUE_GET_PKT_IS_1X(prSkb)) {
					if (wlanProcessSecurityFrame(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prSkb)) {
						u4Status = WLAN_STATUS_SUCCESS;
						GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);
					} else {
						u4Status = WLAN_STATUS_RESOURCES;
					}
				}
				/* Handle normal frame */
				else
					u4Status = wlanEnqueueTxPacket(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prSkb);
#endif
				/* Enqueue packet back into TxQueue if resource is not enough */
				if (u4Status == WLAN_STATUS_RESOURCES) {
					QUEUE_INSERT_TAIL(prTempReturnQue, prQueueEntry);
					break;
				}
			}

			if (wlanGetTxPendingFrameCount(prGlueInfo->prAdapter) > 0)
				wlanTxPendingPackets(prGlueInfo->prAdapter, pfgNeedHwAccess);

			/* Enqueue packet back into TxQueue if resource is not enough */
			if (QUEUE_IS_NOT_EMPTY(prTempReturnQue)) {
				QUEUE_CONCATENATE_QUEUES(prTempReturnQue, prTempQue);

				GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
				QUEUE_CONCATENATE_QUEUES_HEAD(prTxQueue, prTempReturnQue);
				GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

				break;
			}
		}

		if (wlanGetTxPendingFrameCount(prGlueInfo->prAdapter) > 0)
			wlanTxPendingPackets(prGlueInfo->prAdapter, pfgNeedHwAccess);
	}

}

#if CFG_SUPPORT_MULTITHREAD
/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param data       data pointer to private data of hif_thread
*
* @retval           If the function succeeds, the return value is 0.
* Otherwise, an error code is returned.
*
*/
/*----------------------------------------------------------------------------*/

int hif_thread(void *data)
{
	struct net_device *dev = data;
	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(dev));
	int ret = 0;
	KAL_WAKELOCK_DECLARE(rHifThreadWakeLock);
	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter, &rHifThreadWakeLock, "WLAN hif_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rHifThreadWakeLock);

	DBGLOG(INIT, INFO, "hif_thread starts running ID=%d\n", KAL_GET_CURRENT_THREAD_ID());
	prGlueInfo->u4HifThreadPid = KAL_GET_CURRENT_THREAD_ID();

	set_user_nice(current, prGlueInfo->prAdapter->rWifiVar.cThreadNice);

	while (TRUE) {

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
			DBGLOG(INIT, INFO, "hif_thread should stop now...\n");
			break;
		}

		/* Unlock wakelock if hif_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_HIF_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rHifThreadWakeLock);

		/*
		 * sleep on waitqueue if no events occurred. Event contain (1) GLUE_FLAG_INT
		 * (2) GLUE_FLAG_OID (3) GLUE_FLAG_TXREQ (4) GLUE_FLAG_HALT
		 *
		 */
		do {
			ret = wait_event_interruptible(prGlueInfo->waitq_hif,
						       ((prGlueInfo->ulFlag & GLUE_FLAG_HIF_PROCESS) != 0));
		} while (ret != 0);

#if 0 /*defined(MT6797)*/
		nicDisableInterrupt(prGlueInfo->prAdapter);
#endif
		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter, &rHifThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rHifThreadWakeLock);

		wlanAcquirePowerControl(prGlueInfo->prAdapter);

		/* Handle Interrupt */
		if (test_and_clear_bit(GLUE_FLAG_INT_BIT, &prGlueInfo->ulFlag)) {
			/* the Wi-Fi interrupt is already disabled in mmc thread,
			   so we set the flag only to enable the interrupt later  */
			prGlueInfo->prAdapter->fgIsIntEnable = FALSE;
			if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
				/* Should stop now... skip pending interrupt */
				DBGLOG(INIT, INFO, "ignore pending interrupt\n");
			} else {
				/* DBGLOG(INIT, INFO, ("HIF Interrupt!\n")); */
				prGlueInfo->TaskIsrCnt++;
				wlanIST(prGlueInfo->prAdapter);
			}
		}

		/* TX Commands */
		if (test_and_clear_bit(GLUE_FLAG_HIF_TX_CMD_BIT, &prGlueInfo->ulFlag))
			wlanTxCmdMthread(prGlueInfo->prAdapter);

		/* Process TX data packet to SDIO request */
		if (test_and_clear_bit(GLUE_FLAG_HIF_TX_BIT, &prGlueInfo->ulFlag))
			nicTxMsduQueueMthread(prGlueInfo->prAdapter);

		/* Set FW own */
		if (test_and_clear_bit(GLUE_FLAG_HIF_FW_OWN_BIT, &prGlueInfo->ulFlag))
			prGlueInfo->prAdapter->fgWiFiInSleepyState = TRUE;

		if (test_and_clear_bit(GLUE_FLAG_HAL_MCR_RD_BIT, &prGlueInfo->ulFlag)) {
			HAL_MCR_HIF_RD(prGlueInfo->prAdapter, prGlueInfo->u4Register,
							prGlueInfo->prRegValue);
			complete(&prGlueInfo->rHalRDMCRComp);
		}

		if (test_and_clear_bit(GLUE_FLAG_HAL_MCR_WR_BIT, &prGlueInfo->ulFlag)) {
			HAL_MCR_HIF_WR(prGlueInfo->prAdapter, prGlueInfo->u4Register,
							prGlueInfo->u4RegValue);
			complete(&prGlueInfo->rHalWRMCRComp);
		}

		if (test_and_clear_bit(GLUE_FLAG_HAL_MCR_RD_BIT, &prGlueInfo->ulFlag)) {
			HAL_MCR_HIF_RD(prGlueInfo->prAdapter, prGlueInfo->u4Register,
							prGlueInfo->prRegValue);
			complete(&prGlueInfo->rHalRDMCRComp);
		}

		if (test_and_clear_bit(GLUE_FLAG_HAL_MCR_WR_BIT, &prGlueInfo->ulFlag)) {
			HAL_MCR_HIF_WR(prGlueInfo->prAdapter, prGlueInfo->u4Register,
							prGlueInfo->u4RegValue);
			complete(&prGlueInfo->rHalWRMCRComp);
		}

		/* Release to FW own */
		wlanReleasePowerControl(prGlueInfo->prAdapter);
#if defined(MT6797)
		nicEnableInterrupt(prGlueInfo->prAdapter);
#endif
	}

	complete(&prGlueInfo->rHifHaltComp);

	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter, &rHifThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rHifThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter, &rHifThreadWakeLock);

	return 0;
}

int rx_thread(void *data)
{
	struct net_device *dev = data;
	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(dev));

	QUE_T rTempRxQue;
	P_QUE_T prTempRxQue = NULL;
	P_QUE_ENTRY_T prQueueEntry = NULL;
	int ret = 0;
	UINT_32 u4LoopCount;

	/* for spin lock acquire and release */
	KAL_SPIN_LOCK_DECLARATION();
	KAL_WAKELOCK_DECLARE(rRxThreadWakeLock);
	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter, &rRxThreadWakeLock, "WLAN rx_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rRxThreadWakeLock);

	DBGLOG(INIT, INFO, "rx_thread starts running ID=%d\n", KAL_GET_CURRENT_THREAD_ID());
	prGlueInfo->u4RxThreadPid = KAL_GET_CURRENT_THREAD_ID();

	set_user_nice(current, prGlueInfo->prAdapter->rWifiVar.cThreadNice);

	prTempRxQue = &rTempRxQue;

	while (TRUE) {

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
			DBGLOG(INIT, INFO, "rx_thread should stop now...\n");
			break;
		}

		/* Unlock wakelock if rx_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_RX_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rRxThreadWakeLock);

		/*
		 * sleep on waitqueue if no events occurred.
		 */
		do {
			ret = wait_event_interruptible(prGlueInfo->waitq_rx,
						       ((prGlueInfo->ulFlag & GLUE_FLAG_RX_PROCESS) != 0));
		} while (ret != 0);

		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter, &rRxThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rRxThreadWakeLock);

		if (test_and_clear_bit(GLUE_FLAG_RX_TO_OS_BIT, &prGlueInfo->ulFlag)) {
			u4LoopCount = prGlueInfo->prAdapter->rWifiVar.u4Rx2OsLoopCount;

			while (u4LoopCount--) {
				while (QUEUE_IS_NOT_EMPTY(&prGlueInfo->prAdapter->rRxQueue)) {
					QUEUE_INITIALIZE(prTempRxQue);

					GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_TO_OS_QUE);
					QUEUE_MOVE_ALL(prTempRxQue, &prGlueInfo->prAdapter->rRxQueue);
					GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_TO_OS_QUE);

					while (QUEUE_IS_NOT_EMPTY(prTempRxQue)) {
						QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry, P_QUE_ENTRY_T);
						kalRxIndicateOnePkt(prGlueInfo,
								    (PVOID) GLUE_GET_PKT_DESCRIPTOR(prQueueEntry));
					}

					KAL_WAKE_LOCK_TIMEOUT(prGlueInfo->prAdapter, &prGlueInfo->rTimeoutWakeLock,
							      MSEC_TO_JIFFIES(WAKE_LOCK_RX_TIMEOUT));
				}
			}
		}
	}

	complete(&prGlueInfo->rRxHaltComp);

	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter, &rRxThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rRxThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter, &rRxThreadWakeLock);

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is a kernel thread function for handling command packets
* Tx requests and interrupt events
*
* @param data       data pointer to private data of tx_thread
*
* @retval           If the function succeeds, the return value is 0.
* Otherwise, an error code is returned.
*
*/
/*----------------------------------------------------------------------------*/

int tx_thread(void *data)
{
	struct net_device *dev = data;
	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(dev));
	P_GL_IO_REQ_T prIoReq = NULL;
	int ret = 0;
	BOOLEAN fgNeedHwAccess = FALSE;
	KAL_WAKELOCK_DECLARE(rTxThreadWakeLock);

#if CFG_SUPPORT_MULTITHREAD
	prGlueInfo->u4TxThreadPid = KAL_GET_CURRENT_THREAD_ID();
#endif

	current->flags |= PF_NOFREEZE;
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);
	set_user_nice(current, prGlueInfo->prAdapter->rWifiVar.cThreadNice);

	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter, &rTxThreadWakeLock, "WLAN tx_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rTxThreadWakeLock);

	DBGLOG(INIT, INFO, "tx_thread starts running...\n");
	while (TRUE) {

#if CFG_ENABLE_WIFI_DIRECT
		/*run p2p multicast list work. */
		if (test_and_clear_bit(GLUE_FLAG_SUB_MOD_MULTICAST_BIT, &prGlueInfo->ulFlag))
			p2pSetMulticastListWorkQueueWrapper(prGlueInfo);
#endif

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
			DBGLOG(INIT, INFO, "tx_thread should stop now...\n");
			break;
		}

		/* Unlock wakelock if tx_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_TX_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rTxThreadWakeLock);

		/*
		 * sleep on waitqueue if no events occurred. Event contain (1) GLUE_FLAG_INT
		 * (2) GLUE_FLAG_OID (3) GLUE_FLAG_TXREQ (4) GLUE_FLAG_HALT
		 *
		 */
		do {
			ret = wait_event_interruptible(prGlueInfo->waitq,
						       ((prGlueInfo->ulFlag & GLUE_FLAG_TX_PROCESS) != 0));
		} while (ret != 0);

		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter, &rTxThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rTxThreadWakeLock);
#if CFG_DBG_GPIO_PINS
		/* TX thread Wake up */
		mtk_wcn_stp_debug_gpio_assert(IDX_TX_THREAD, DBG_TIE_LOW);
#endif

#if CFG_ENABLE_WIFI_DIRECT
		/*run p2p multicast list work. */
		if (test_and_clear_bit(GLUE_FLAG_SUB_MOD_MULTICAST_BIT, &prGlueInfo->ulFlag))
			p2pSetMulticastListWorkQueueWrapper(prGlueInfo);

		if (test_and_clear_bit(GLUE_FLAG_FRAME_FILTER_BIT, &prGlueInfo->ulFlag)) {
			p2pFuncUpdateMgmtFrameRegister(prGlueInfo->prAdapter,
						       prGlueInfo->prP2PInfo->u4OsMgmtFrameFilter);
		}
#endif
		if (test_and_clear_bit(GLUE_FLAG_FRAME_FILTER_AIS_BIT, &prGlueInfo->ulFlag)) {
			P_AIS_FSM_INFO_T prAisFsmInfo = (P_AIS_FSM_INFO_T) NULL;

			prAisFsmInfo = &(prGlueInfo->prAdapter->rWifiVar.rAisFsmInfo);
			prAisFsmInfo->u4AisPacketFilter = prGlueInfo->u4OsMgmtFrameFilter;
		}

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
			DBGLOG(INIT, INFO, "<1>tx_thread should stop now...\n");
			break;
		}

		fgNeedHwAccess = FALSE;

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		if (prGlueInfo->fgEnSdioTestPattern == TRUE) {
			if (fgNeedHwAccess == FALSE) {
				fgNeedHwAccess = TRUE;

				wlanAcquirePowerControl(prGlueInfo->prAdapter);
			}

			if (prGlueInfo->fgIsSdioTestInitialized == FALSE) {
				/* enable PRBS mode */
				kalDevRegWrite(prGlueInfo, MCR_WTMCR, 0x00080002);
				prGlueInfo->fgIsSdioTestInitialized = TRUE;
			}

			if (prGlueInfo->fgSdioReadWriteMode == TRUE) {
				/* read test */
				kalDevPortRead(prGlueInfo,
					       MCR_WTMDR,
					       256,
					       prGlueInfo->aucSdioTestBuffer, sizeof(prGlueInfo->aucSdioTestBuffer));
			} else {
				/* write test */
				kalDevPortWrite(prGlueInfo,
						MCR_WTMDR,
						172,
						prGlueInfo->aucSdioTestBuffer, sizeof(prGlueInfo->aucSdioTestBuffer));
			}
		}
#endif
#if CFG_SUPPORT_MULTITHREAD
#else
		/* Handle Interrupt */
		if (test_and_clear_bit(GLUE_FLAG_INT_BIT, &prGlueInfo->ulFlag)) {

			if (fgNeedHwAccess == FALSE) {
				fgNeedHwAccess = TRUE;

				wlanAcquirePowerControl(prGlueInfo->prAdapter);
			}

			/* the Wi-Fi interrupt is already disabled in mmc thread,
			   so we set the flag only to enable the interrupt later  */
			prGlueInfo->prAdapter->fgIsIntEnable = FALSE;
			/* wlanISR(prGlueInfo->prAdapter, TRUE); */

			if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
				/* Should stop now... skip pending interrupt */
				DBGLOG(INIT, INFO, "ignore pending interrupt\n");
			} else {
				prGlueInfo->TaskIsrCnt++;
				wlanIST(prGlueInfo->prAdapter);
			}
		}
#endif
		/* transfer ioctl to OID request */
#if 0
		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
			DBGLOG(INIT, INFO, "<2>tx_thread should stop now...\n");
			break;
		}
#endif

		do {
			if (test_and_clear_bit(GLUE_FLAG_OID_BIT, &prGlueInfo->ulFlag)) {
				/* get current prIoReq */
				prIoReq = &(prGlueInfo->OidEntry);
				if (FALSE == prIoReq->fgRead) {
					prIoReq->rStatus = wlanSetInformation(prIoReq->prAdapter,
									      prIoReq->pfnOidHandler,
									      prIoReq->pvInfoBuf,
									      prIoReq->u4InfoBufLen,
									      prIoReq->pu4QryInfoLen);
				} else {
					prIoReq->rStatus = wlanQueryInformation(prIoReq->prAdapter,
										prIoReq->pfnOidHandler,
										prIoReq->pvInfoBuf,
										prIoReq->u4InfoBufLen,
										prIoReq->pu4QryInfoLen);
				}

				if (prIoReq->rStatus != WLAN_STATUS_PENDING) {
					/* complete ONLY if there are waiters */
					if (!completion_done(&prGlueInfo->rPendComp)) {
						complete(&prGlueInfo->rPendComp);
					} else {
						DBGLOG(INIT, WARN,
							"SKIP multiple OID complete!\n");
					}
				} else {
					wlanoidTimeoutCheck(prGlueInfo->prAdapter, prIoReq->pfnOidHandler);
				}
			}

		} while (FALSE);

		/*
		 *
		 * if TX request, clear the TXREQ flag. TXREQ set by kalSetEvent/GlueSetEvent
		 * indicates the following requests occur
		 *
		 */
#if 0
		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
			DBGLOG(INIT, INFO, "<3>tx_thread should stop now...\n");
			break;
		}
#endif

		if (test_and_clear_bit(GLUE_FLAG_TXREQ_BIT, &prGlueInfo->ulFlag))
			kalProcessTxReq(prGlueInfo, &fgNeedHwAccess);
#if CFG_SUPPORT_MULTITHREAD
		/* Process RX */
		if (test_and_clear_bit(GLUE_FLAG_RX_BIT, &prGlueInfo->ulFlag))
			nicRxProcessRFBs(prGlueInfo->prAdapter);
		if (test_and_clear_bit(GLUE_FLAG_TX_CMD_DONE_BIT, &prGlueInfo->ulFlag))
			wlanTxCmdDoneMthread(prGlueInfo->prAdapter);
#endif

		/* Process RX, In linux, we don't need to free sk_buff by ourself */

		/* In linux, we don't need to free sk_buff by ourself */

		/* In linux, we don't do reset */
#if CFG_SUPPORT_MULTITHREAD
#else
		if (fgNeedHwAccess == TRUE)
			wlanReleasePowerControl(prGlueInfo->prAdapter);
#endif
		/* handle cnmTimer time out */
		if (test_and_clear_bit(GLUE_FLAG_TIMEOUT_BIT, &prGlueInfo->ulFlag))
			wlanTimerTimeoutCheck(prGlueInfo->prAdapter);
#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		if (prGlueInfo->fgEnSdioTestPattern == TRUE)
			kalSetEvent(prGlueInfo);
#endif

#if CFG_DBG_GPIO_PINS
		/* TX thread go to sleep */
		if (!prGlueInfo->ulFlag)
			mtk_wcn_stp_debug_gpio_assert(IDX_TX_THREAD, DBG_TIE_HIGH);
#endif
	}

#if 0
	if (fgNeedHwAccess == TRUE)
		wlanReleasePowerControl(prGlueInfo->prAdapter);
#endif

	/* flush the pending TX packets */
	if (GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum) > 0)
		kalFlushPendingTxPackets(prGlueInfo);

	/* flush pending security frames */
	if (GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum) > 0)
		kalClearSecurityFrames(prGlueInfo);

	/* remove pending oid */
	wlanReleasePendingOid(prGlueInfo->prAdapter, 0);

	/* In linux, we don't need to free sk_buff by ourself */

	DBGLOG(INIT, INFO, "mtk_sdiod stops\n");
	complete(&prGlueInfo->rHaltComp);
	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter, &rTxThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rTxThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter, &rTxThreadWakeLock);

	return 0;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to check if card is removed
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
*
* \retval TRUE:     card is removed
*         FALSE:    card is still attached
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsCardRemoved(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return FALSE;
	/* Linux MMC doesn't have removal notification yet */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to send command to firmware for overriding netweork address
 *
 * \param pvGlueInfo Pointer of GLUE Data Structure

 * \retval TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
BOOLEAN kalRetrieveNetworkAddress(IN P_GLUE_INFO_T prGlueInfo, IN OUT PARAM_MAC_ADDRESS *prMacAddr)
{
	ASSERT(prGlueInfo);

	if (prGlueInfo->fgIsMacAddrOverride == FALSE) {
#if !defined(CONFIG_X86)
		UINT_32 i;
		BOOLEAN fgIsReadError = FALSE;

		for (i = 0; i < MAC_ADDR_LEN; i += 2) {
			if (kalCfgDataRead16(prGlueInfo,
					     OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucMacAddress) + i,
					     (PUINT_16) (((PUINT_8) prMacAddr) + i)) == FALSE) {
				fgIsReadError = TRUE;
				break;
			}
		}

		if (fgIsReadError == TRUE)
			return FALSE;
		else
			return TRUE;
#else
		/* x86 Linux doesn't need to override network address so far */
		return FALSE;
#endif
	} else {
		COPY_MAC_ADDR(prMacAddr, prGlueInfo->rMacAddrOverride);

		return TRUE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to flush pending TX packets in glue layer
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalFlushPendingTxPackets(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prTxQue;
	P_QUE_ENTRY_T prQueueEntry;
	PVOID prPacket;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	prTxQue = &(prGlueInfo->rTxQueue);

	if (GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum)) {
		while (TRUE) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
			QUEUE_REMOVE_HEAD(prTxQue, prQueueEntry, P_QUE_ENTRY_T);
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

			if (prQueueEntry == NULL)
				break;

			prPacket = GLUE_GET_PKT_DESCRIPTOR(prQueueEntry);

			kalSendComplete(prGlueInfo, prPacket, WLAN_STATUS_NOT_ACCEPTED);
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is get indicated media state
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalGetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->eParamMediaStateIndicated;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set indicated media state
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalSetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicate)
{
	ASSERT(prGlueInfo);

	prGlueInfo->eParamMediaStateIndicated = eParamMediaStateIndicate;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear pending OID staying in command queue
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalOidCmdClearance(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	QUE_T rReturnCmdQue;
	P_QUE_T prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);

	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {

		if (((P_CMD_INFO_T) prQueueEntry)->fgIsOid) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;
			break;
		}

		QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	if (prCmdInfo) {
		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter, prCmdInfo);
		else
			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_NOT_ACCEPTED);

		prGlueInfo->u4OidCompleteFlag = 1;
		cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to insert command into prCmdQueue
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*        prQueueEntry   Pointer of queue entry to be inserted
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalEnqueueCommand(IN P_GLUE_INFO_T prGlueInfo, IN P_QUE_ENTRY_T prQueueEntry)
{
	P_QUE_T prCmdQue;
	P_CMD_INFO_T prCmdInfo;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);
	ASSERT(prQueueEntry);

	prCmdQue = &prGlueInfo->rCmdQueue;

	prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

	DBGLOG(TX, LOUD, "EN-Q CMD TYPE[%u] ID[0x%02X] SEQ[%u] to CMD Q\n",
			    prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Handle EVENT_ID_ASSOC_INFO event packet by indicating to OS with
*        proper information
*
* @param pvGlueInfo     Pointer of GLUE Data Structure
* @param prAssocInfo    Pointer of EVENT_ID_ASSOC_INFO Packet
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID kalHandleAssocInfo(IN P_GLUE_INFO_T prGlueInfo, IN P_EVENT_ASSOC_INFO prAssocInfo)
{
	/* to do */
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to get firmware load address from registry
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetFwLoadAddress(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->rRegInfo.u4LoadAddress;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to get firmware start address from registry
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetFwStartAddress(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->rRegInfo.u4StartAddress;
}

/*----------------------------------------------------------------------------*/
/*!
 * * @brief Notify OS with SendComplete event of the specific packet. Linux should
 * *        free packets here.
 * *
 * * @param pvGlueInfo     Pointer of GLUE Data Structure
 * * @param pvPacket       Pointer of Packet Handle
 * * @param status         Status Code for OS upper layer
 * *
 * * @return none
 * */
/*----------------------------------------------------------------------------*/

/* / Todo */
VOID kalSecurityFrameSendComplete(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN WLAN_STATUS rStatus)
{
	ASSERT(pvPacket);

	/* dev_kfree_skb((struct sk_buff *) pvPacket); */
	kalSendCompleteAndAwakeQueue(prGlueInfo, pvPacket);
	GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);
}

UINT_32 kalGetTxPendingFrameCount(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return (UINT_32) (GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to retrieve the number of pending commands
*        (including MMPDU, 802.1X and command packets)
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetTxPendingCmdCount(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;

	ASSERT(prGlueInfo);
	prCmdQue = &prGlueInfo->rCmdQueue;

	return prCmdQue->u4NumElem;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Timer Initialization Procedure
*
* \param[in] prGlueInfo     Pointer to GLUE Data Structure
* \param[in] prTimerHandler Pointer to timer handling function, whose only
*                           argument is "prAdapter"
*
* \retval none
*
*/
/*----------------------------------------------------------------------------*/

/* static struct timer_list tickfn; */

VOID kalOsTimerInitialize(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prTimerHandler)
{

	ASSERT(prGlueInfo);

	init_timer(&(prGlueInfo->tickfn));
	prGlueInfo->tickfn.function = prTimerHandler;
	prGlueInfo->tickfn.data = (unsigned long)prGlueInfo;
}

/* Todo */
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the time to do the time out check.
*
* \param[in] prGlueInfo Pointer to GLUE Data Structure
* \param[in] rInterval  Time out interval from current time.
*
* \retval TRUE Success.
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalSetTimer(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Interval)
{
	ASSERT(prGlueInfo);
	del_timer_sync(&(prGlueInfo->tickfn));

	prGlueInfo->tickfn.expires = jiffies + u4Interval * HZ / MSEC_PER_SEC;
	add_timer(&(prGlueInfo->tickfn));

	return TRUE;		/* success */
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to cancel
*
* \param[in] prGlueInfo Pointer to GLUE Data Structure
*
* \retval TRUE  :   Timer has been canceled
*         FALAE :   Timer doens't exist
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalCancelTimer(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	clear_bit(GLUE_FLAG_TIMEOUT_BIT, &prGlueInfo->ulFlag);

	if (del_timer_sync(&(prGlueInfo->tickfn)) >= 0)
		return TRUE;
	else
		return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is a callback function for scanning done
*
* \param[in] prGlueInfo Pointer to GLUE Data Structure
*
* \retval none
*
*/
/*----------------------------------------------------------------------------*/
VOID kalScanDone(IN P_GLUE_INFO_T prGlueInfo, IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN WLAN_STATUS status)
{
	ASSERT(prGlueInfo);
	scanLogEssResult(prGlueInfo->prAdapter);

	scanReportBss2Cfg80211(prGlueInfo->prAdapter, BSS_TYPE_INFRASTRUCTURE, NULL);

	/* check for system configuration for generating error message on scan list */
	wlanCheckSystemConfiguration(prGlueInfo->prAdapter);

	kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_SCAN_COMPLETE, &status, sizeof(WLAN_STATUS));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to generate a random number
*
* \param none
*
* \retval UINT_32
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalRandomNumber(VOID)
{
	UINT_32 number = 0;

	get_random_bytes(&number, 4);

	return number;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief command timeout call-back function
 *
 * \param[in] prGlueInfo Pointer to the GLUE data structure.
 *
 * \retval (none)
 */
/*----------------------------------------------------------------------------*/
VOID kalTimeoutHandler(unsigned long arg)
{

	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) arg;

	ASSERT(prGlueInfo);

	/* Notify tx thread  for timeout event */
	set_bit(GLUE_FLAG_TIMEOUT_BIT, &prGlueInfo->ulFlag);
	wake_up_interruptible(&prGlueInfo->waitq);

}

VOID kalSetEvent(P_GLUE_INFO_T pr)
{
	set_bit(GLUE_FLAG_TXREQ_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq);
}

#if CFG_SUPPORT_MULTITHREAD
VOID kalSetTxEvent2Hif(P_GLUE_INFO_T pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock, MSEC_TO_JIFFIES(WAKE_LOCK_THREAD_WAKEUP_TIMEOUT));

	set_bit(GLUE_FLAG_HIF_TX_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}

VOID kalSetFwOwnEvent2Hif(P_GLUE_INFO_T pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock, MSEC_TO_JIFFIES(WAKE_LOCK_THREAD_WAKEUP_TIMEOUT));

	set_bit(GLUE_FLAG_HIF_FW_OWN_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}

VOID kalSetTxEvent2Rx(P_GLUE_INFO_T pr)
{
	if (!pr->rx_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock, MSEC_TO_JIFFIES(WAKE_LOCK_THREAD_WAKEUP_TIMEOUT));

	set_bit(GLUE_FLAG_RX_TO_OS_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_rx);
}

VOID kalSetTxCmdEvent2Hif(P_GLUE_INFO_T pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock, MSEC_TO_JIFFIES(WAKE_LOCK_THREAD_WAKEUP_TIMEOUT));

	set_bit(GLUE_FLAG_HIF_TX_CMD_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief to check if configuration file (NVRAM/Registry) exists
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsConfigurationExist(IN P_GLUE_INFO_T prGlueInfo)
{
#if !defined(CONFIG_X86)
	ASSERT(prGlueInfo);

	return prGlueInfo->fgNvramAvailable;
#else
	/* there is no configuration data for x86-linux */
	return FALSE;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Registry information
*
* \param[in]
*           prGlueInfo
*
* \return
*           Pointer of REG_INFO_T
*/
/*----------------------------------------------------------------------------*/
P_REG_INFO_T kalGetConfiguration(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return &(prGlueInfo->rRegInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve version information of corresponding configuration file
*
* \param[in]
*           prGlueInfo
*
* \param[out]
*           pu2Part1CfgOwnVersion
*           pu2Part1CfgPeerVersion
*           pu2Part2CfgOwnVersion
*           pu2Part2CfgPeerVersion
*
* \return
*           NONE
*/
/*----------------------------------------------------------------------------*/
VOID
kalGetConfigurationVersion(IN P_GLUE_INFO_T prGlueInfo,
			   OUT PUINT_16 pu2Part1CfgOwnVersion,
			   OUT PUINT_16 pu2Part1CfgPeerVersion,
			   OUT PUINT_16 pu2Part2CfgOwnVersion, OUT PUINT_16 pu2Part2CfgPeerVersion)
{
	ASSERT(prGlueInfo);

	ASSERT(pu2Part1CfgOwnVersion);
	ASSERT(pu2Part1CfgPeerVersion);
	ASSERT(pu2Part2CfgOwnVersion);
	ASSERT(pu2Part2CfgPeerVersion);

	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2Part1OwnVersion), pu2Part1CfgOwnVersion);

	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2Part1PeerVersion), pu2Part1CfgPeerVersion);

	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2Part2OwnVersion), pu2Part2CfgOwnVersion);

	kalCfgDataRead16(prGlueInfo, OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2Part2PeerVersion), pu2Part2CfgPeerVersion);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief to check if the WPS is active or not
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalWSCGetActiveState(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->fgWpsActive;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief update RSSI and LinkQuality to GLUE layer
*
* \param[in]
*           prGlueInfo
*           eNetTypeIdx
*           cRssi
*           cLinkQuality
*
* \return
*           None
*/
/*----------------------------------------------------------------------------*/
VOID
kalUpdateRSSI(IN P_GLUE_INFO_T prGlueInfo,
	      IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN INT_8 cRssi, IN INT_8 cLinkQuality)
{
	struct iw_statistics *pStats = (struct iw_statistics *)NULL;

	ASSERT(prGlueInfo);

	switch (eNetTypeIdx) {
	case KAL_NETWORK_TYPE_AIS_INDEX:
		pStats = (struct iw_statistics *)(&(prGlueInfo->rIwStats));
		break;
#if CFG_ENABLE_WIFI_DIRECT
#if CFG_SUPPORT_P2P_RSSI_QUERY
	case KAL_NETWORK_TYPE_P2P_INDEX:
		pStats = (struct iw_statistics *)(&(prGlueInfo->rP2pIwStats));
		break;
#endif
#endif
	default:
		break;

	}

	if (pStats) {
		pStats->qual.qual = cLinkQuality;
		pStats->qual.noise = 0;
		pStats->qual.updated = IW_QUAL_QUAL_UPDATED | IW_QUAL_NOISE_UPDATED | IW_QUAL_DBM;
		pStats->qual.level = 0x100 + cRssi;
		pStats->qual.updated |= IW_QUAL_LEVEL_UPDATED;
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Pre-allocate I/O buffer
*
* \param[in]
*           none
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalInitIOBuffer(VOID)
{
	UINT_32 u4Size;

	if (CFG_COALESCING_BUFFER_SIZE >= CFG_RX_COALESCING_BUFFER_SIZE)
		u4Size = CFG_COALESCING_BUFFER_SIZE + sizeof(ENHANCE_MODE_DATA_STRUCT_T);
	else
		u4Size = CFG_RX_COALESCING_BUFFER_SIZE + sizeof(ENHANCE_MODE_DATA_STRUCT_T);

	pvIoBuffer = kmalloc(u4Size, GFP_KERNEL);
	if (pvIoBuffer) {
		pvIoBufferSize = u4Size;
		pvIoBufferUsage = 0;

		return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Free pre-allocated I/O buffer
*
* \param[in]
*           none
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID kalUninitIOBuffer(VOID)
{
	kfree(pvIoBuffer);

	pvIoBuffer = (PVOID) NULL;
	pvIoBufferSize = 0;
	pvIoBufferUsage = 0;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief Dispatch pre-allocated I/O buffer
*
* \param[in]
*           u4AllocSize
*
* \return
*           PVOID for pointer of pre-allocated I/O buffer
*/
/*----------------------------------------------------------------------------*/
PVOID kalAllocateIOBuffer(IN UINT_32 u4AllocSize)
{
	PVOID ret = (PVOID) NULL;

	if (pvIoBuffer) {
		if (u4AllocSize <= (pvIoBufferSize - pvIoBufferUsage)) {
			ret = (PVOID) &(((PUINT_8) (pvIoBuffer))[pvIoBufferUsage]);
			pvIoBufferUsage += u4AllocSize;
		}
	} else {
		/* fault tolerance */
		ret = (PVOID) kalMemAlloc(u4AllocSize, PHY_MEM_TYPE);
	}

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Release all dispatched I/O buffer
*
* \param[in]
*           none
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID kalReleaseIOBuffer(IN PVOID pvAddr, IN UINT_32 u4Size)
{
	if (pvIoBuffer) {
		pvIoBufferUsage -= u4Size;
	} else {
		/* fault tolerance */
		kalMemFree(pvAddr, PHY_MEM_TYPE, u4Size);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
kalGetChannelList(IN P_GLUE_INFO_T prGlueInfo,
		  IN ENUM_BAND_T eSpecificBand,
		  IN UINT_8 ucMaxChannelNum, IN PUINT_8 pucNumOfChannel, IN P_RF_CHANNEL_INFO_T paucChannelList)
{
	rlmDomainGetChnlList(prGlueInfo->prAdapter, eSpecificBand, FALSE, ucMaxChannelNum,
			     pucNumOfChannel, paucChannelList);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOL kalIsAPmode(IN P_GLUE_INFO_T prGlueInfo)
{
#if 0				/* Marked for MT6630 (New ucBssIndex) */
#if CFG_ENABLE_WIFI_DIRECT
	if (IS_NET_ACTIVE(prGlueInfo->prAdapter, NETWORK_TYPE_P2P_INDEX) &&
	    p2pFuncIsAPMode(prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo))
		return TRUE;
#endif
#endif

	return FALSE;
}

#if CFG_SUPPORT_802_11W
/*----------------------------------------------------------------------------*/
/*!
* \brief to check if the MFP is active or not
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetMfpSetting(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->rWpaInfo.u4Mfp;
}
#endif

struct file *kalFileOpen(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}

VOID kalFileClose(struct file *file)
{
	filp_close(file, NULL);
}

UINT_32 kalFileRead(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

UINT_32 kalFileWrite(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

UINT_32 kalWriteToFile(const PUINT_8 pucPath, BOOLEAN fgDoAppend, PUINT_8 pucData, UINT_32 u4Size)
{
	struct file *file = NULL;
	UINT_32 ret;
	UINT_32 u4Flags = 0;

	if (fgDoAppend)
		u4Flags = O_APPEND;

	file = kalFileOpen(pucPath, O_WRONLY | O_CREAT | u4Flags, S_IRWXU);
	ret = kalFileWrite(file, 0, pucData, u4Size);
	kalFileClose(file);

	return ret;
}

INT_32 kalReadToFile(const PUINT_8 pucPath, PUINT_8 pucData, UINT_32 u4Size, PUINT_32 pu4ReadSize)
{
	struct file *file = NULL;
	INT_32 ret = -1;
	UINT_32 u4ReadSize = 0;

	DBGLOG(INIT, TRACE, "kalReadToFile() path %s\n", pucPath);

	file = kalFileOpen(pucPath, O_RDONLY, 0);

	if ((file != NULL) && !IS_ERR(file)) {
		u4ReadSize = kalFileRead(file, 0, pucData, u4Size);
		kalFileClose(file);
		if (pu4ReadSize)
			*pu4ReadSize = u4ReadSize;
		ret = 0;
	}
	return ret;
}

UINT_32 kalCheckPath(const PUINT_8 pucPath)
{
	struct file *file = NULL;
	UINT_32 u4Flags = 0;

	file = kalFileOpen(pucPath, O_WRONLY | O_CREAT | u4Flags, S_IRWXU);
	if (!file)
		return -1;

	kalFileClose(file);
	return 1;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief    To indicate BSS-INFO to NL80211 as scanning result
*
* \param[in]
*           prGlueInfo
*           pucBeaconProbeResp
*           u4FrameLen
*
*
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalIndicateBssInfo(IN P_GLUE_INFO_T prGlueInfo,
		   IN PUINT_8 pucBeaconProbeResp,
		   IN UINT_32 u4FrameLen, IN UINT_8 ucChannelNum, IN INT_32 i4SignalStrength)
{
	struct wiphy *wiphy;
	struct ieee80211_channel *prChannel = NULL;

	ASSERT(prGlueInfo);
	wiphy = priv_to_wiphy(prGlueInfo);

	/* search through channel entries */
	if (ucChannelNum <= 14) {
		prChannel =
		    ieee80211_get_channel(wiphy, ieee80211_channel_to_frequency(ucChannelNum, IEEE80211_BAND_2GHZ));
	} else {
		prChannel =
		    ieee80211_get_channel(wiphy, ieee80211_channel_to_frequency(ucChannelNum, IEEE80211_BAND_5GHZ));
	}

	if (prChannel != NULL && prGlueInfo->fgIsRegistered == TRUE) {
		struct cfg80211_bss *bss;
#if CFG_SUPPORT_TSF_USING_BOOTTIME
		struct ieee80211_mgmt *prMgmtFrame = (struct ieee80211_mgmt *)pucBeaconProbeResp;

		prMgmtFrame->u.beacon.timestamp = kalGetBootTime();
#endif

		/* indicate to NL80211 subsystem */
		bss = cfg80211_inform_bss_frame(wiphy,
						prChannel,
						(struct ieee80211_mgmt *)pucBeaconProbeResp,
						u4FrameLen, i4SignalStrength * 100, GFP_KERNEL);

		if (!bss) {
			/* ToDo:: DBGLOG */
			DBGLOG(REQ, WARN, "cfg80211_inform_bss_frame() returned with NULL\n");
		} else
			cfg80211_put_bss(wiphy, bss);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief    To indicate channel ready
*
* \param[in]
*           prGlueInfo
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalReadyOnChannel(IN P_GLUE_INFO_T prGlueInfo,
		  IN UINT_64 u8Cookie,
		  IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum, IN UINT_32 u4DurationMs)
{
	struct ieee80211_channel *prChannel = NULL;
	enum nl80211_channel_type rChannelType;

	/* ucChannelNum = wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter, NETWORK_TYPE_AIS_INDEX); */

	if (prGlueInfo->fgIsRegistered == TRUE) {
		if (ucChannelNum <= 14) {
			prChannel =
			    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
						  ieee80211_channel_to_frequency(ucChannelNum, IEEE80211_BAND_2GHZ));
		} else {
			prChannel =
			    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
						  ieee80211_channel_to_frequency(ucChannelNum, IEEE80211_BAND_5GHZ));
		}

		switch (eSco) {
		case CHNL_EXT_SCN:
			rChannelType = NL80211_CHAN_NO_HT;
			break;

		case CHNL_EXT_SCA:
			rChannelType = NL80211_CHAN_HT40MINUS;
			break;

		case CHNL_EXT_SCB:
			rChannelType = NL80211_CHAN_HT40PLUS;
			break;

		case CHNL_EXT_RES:
		default:
			rChannelType = NL80211_CHAN_HT20;
			break;
		}

		cfg80211_ready_on_channel(
						 prGlueInfo->prDevHandler->ieee80211_ptr,
						 u8Cookie, prChannel,
						 u4DurationMs, GFP_KERNEL);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief    To indicate channel expiration
*
* \param[in]
*           prGlueInfo
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalRemainOnChannelExpired(IN P_GLUE_INFO_T prGlueInfo,
			  IN UINT_64 u8Cookie, IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum)
{
	struct ieee80211_channel *prChannel = NULL;
	enum nl80211_channel_type rChannelType;

	ucChannelNum =
	    wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter, prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);

	if (prGlueInfo->fgIsRegistered == TRUE) {
		if (ucChannelNum <= 14) {
			prChannel =
			    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
						  ieee80211_channel_to_frequency(ucChannelNum, IEEE80211_BAND_2GHZ));
		} else {
			prChannel =
			    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
						  ieee80211_channel_to_frequency(ucChannelNum, IEEE80211_BAND_5GHZ));
		}

		switch (eSco) {
		case CHNL_EXT_SCN:
			rChannelType = NL80211_CHAN_NO_HT;
			break;

		case CHNL_EXT_SCA:
			rChannelType = NL80211_CHAN_HT40MINUS;
			break;

		case CHNL_EXT_SCB:
			rChannelType = NL80211_CHAN_HT40PLUS;
			break;

		case CHNL_EXT_RES:
		default:
			rChannelType = NL80211_CHAN_HT20;
			break;
		}

		cfg80211_remain_on_channel_expired(
							  prGlueInfo->prDevHandler->ieee80211_ptr,
							  u8Cookie, prChannel,
							  GFP_KERNEL);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief    To indicate Mgmt tx status
*
* \param[in]
*           prGlueInfo
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo,
			IN UINT_64 u8Cookie, IN BOOLEAN fgIsAck, IN PUINT_8 pucFrameBuf, IN UINT_32 u4FrameLen)
{

	do {
		if ((prGlueInfo == NULL)
		    || (pucFrameBuf == NULL)
		    || (u4FrameLen == 0)) {
			DBGLOG(AIS, TRACE,
			       "Unexpected pointer PARAM. 0x%p, 0x%p, %u.", prGlueInfo, pucFrameBuf, u4FrameLen);
			ASSERT(FALSE);
			break;
		}

		cfg80211_mgmt_tx_status(
					       prGlueInfo->prDevHandler->ieee80211_ptr,
					       u8Cookie, pucFrameBuf, u4FrameLen, fgIsAck, GFP_KERNEL);

	} while (FALSE);

}				/* kalIndicateMgmtTxStatus */

VOID kalIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo, IN P_SW_RFB_T prSwRfb)
{
	INT_32 i4Freq = 0;
	UINT_8 ucChnlNum = 0;

	do {
		if ((prGlueInfo == NULL) || (prSwRfb == NULL)) {
			ASSERT(FALSE);
			break;
		}

		ucChnlNum = (UINT_8) HAL_RX_STATUS_GET_CHNL_NUM(prSwRfb->prRxStatus);

		i4Freq = nicChannelNum2Freq(ucChnlNum) / 1000;

		cfg80211_rx_mgmt(
					prGlueInfo->prDevHandler->ieee80211_ptr,
					i4Freq,	/* in MHz */
					RCPI_TO_dBm((UINT_8)
						    HAL_RX_STATUS_GET_RCPI(prSwRfb->prRxStatusGroup3)),
					prSwRfb->pvHeader, prSwRfb->u2PacketLen, GFP_ATOMIC);
	} while (FALSE);

}				/* kalIndicateRxMgmtFrame */

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
/*----------------------------------------------------------------------------*/
/*!
* \brief    To configure SDIO test pattern mode
*
* \param[in]
*           prGlueInfo
*           fgEn
*           fgRead
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalSetSdioTestPattern(IN P_GLUE_INFO_T prGlueInfo, IN BOOLEAN fgEn, IN BOOLEAN fgRead)
{
	const UINT_8 aucPattern[] = {
		0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,
		0xaa, 0x55, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80,
		0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f,
		0x40, 0x40, 0x40, 0xbf, 0x40, 0x40, 0x40, 0xbf,
		0xbf, 0xbf, 0x40, 0xbf, 0xbf, 0xbf, 0x20, 0x20,
		0x20, 0xdf, 0x20, 0x20, 0x20, 0xdf, 0xdf, 0xdf,
		0x20, 0xdf, 0xdf, 0xdf, 0x10, 0x10, 0x10, 0xef,
		0x10, 0x10, 0x10, 0xef, 0xef, 0xef, 0x10, 0xef,
		0xef, 0xef, 0x08, 0x08, 0x08, 0xf7, 0x08, 0x08,
		0x08, 0xf7, 0xf7, 0xf7, 0x08, 0xf7, 0xf7, 0xf7,
		0x04, 0x04, 0x04, 0xfb, 0x04, 0x04, 0x04, 0xfb,
		0xfb, 0xfb, 0x04, 0xfb, 0xfb, 0xfb, 0x02, 0x02,
		0x02, 0xfd, 0x02, 0x02, 0x02, 0xfd, 0xfd, 0xfd,
		0x02, 0xfd, 0xfd, 0xfd, 0x01, 0x01, 0x01, 0xfe,
		0x01, 0x01, 0x01, 0xfe, 0xfe, 0xfe, 0x01, 0xfe,
		0xfe, 0xfe, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
		0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff,
		0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
		0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
		0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
		0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff,
		0x00, 0x00, 0x00, 0xff
	};
	UINT_32 i;

	ASSERT(prGlueInfo);

	/* access to MCR_WTMCR to engage PRBS mode */
	prGlueInfo->fgEnSdioTestPattern = fgEn;
	prGlueInfo->fgSdioReadWriteMode = fgRead;

	if (fgRead == FALSE) {
		/* fill buffer for data to be written */
		for (i = 0; i < sizeof(aucPattern); i++)
			prGlueInfo->aucSdioTestBuffer[i] = aucPattern[i];
	}

	return TRUE;
}
#endif

#if (CFG_MET_PACKET_TRACE_SUPPORT == 1)
#define PROC_MET_PROF_CTRL                 "met_ctrl"
#define PROC_MET_PROF_PORT                 "met_port"

struct proc_dir_entry *pMetProcDir;
void *pMetGlobalData = NULL;

#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief    To indicate scheduled scan results are avilable
*
* \param[in]
*           prGlueInfo
*
* \return
*           None
*/
/*----------------------------------------------------------------------------*/
VOID kalSchedScanResults(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);
	scanReportBss2Cfg80211(prGlueInfo->prAdapter, BSS_TYPE_INFRASTRUCTURE, NULL);
	cfg80211_sched_scan_results(priv_to_wiphy(prGlueInfo));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief    To indicate scheduled scan has been stopped
*
* \param[in]
*           prGlueInfo
*
* \return
*           None
*/
/*----------------------------------------------------------------------------*/
VOID kalSchedScanStopped(IN P_GLUE_INFO_T prGlueInfo, BOOLEAN fgDriverTriggerd)
{
	/* DBGLOG(SCN, INFO, ("-->kalSchedScanStopped\n" )); */

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

#if 1
	/* 1. reset first for newly incoming request */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if (prGlueInfo->prSchedScanRequest != NULL)
		prGlueInfo->prSchedScanRequest = NULL;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
#endif
	DBGLOG(SCN, INFO, "Driver triggerd %d\n", fgDriverTriggerd);

	/* 2. indication to cfg80211 */
	/* 20150205 change cfg80211_sched_scan_stopped to work queue to use K thread to send event instead of Tx thread
	   due to sched_scan_mtx dead lock issue by Tx thread serves oid cmds and send event in the same time  */
	if (fgDriverTriggerd)
		schedule_delayed_work(&sched_workq, 0);
}

BOOLEAN
kalGetIPv4Address(IN struct net_device *prDev,
		  IN UINT_32 u4MaxNumOfAddr, OUT PUINT_8 pucIpv4Addrs, OUT PUINT_32 pu4NumOfIpv4Addr)
{
	UINT_32 u4NumIPv4 = 0;
	UINT_32 u4AddrLen = IPV4_ADDR_LEN;
	struct in_ifaddr *prIfa;

	/* 4 <1> Sanity check of netDevice */
	if (!prDev || !(prDev->ip_ptr) || !((struct in_device *)(prDev->ip_ptr))->ifa_list) {
		DBGLOG(INIT, INFO, "IPv4 address is not available for dev(0x%p)\n", prDev);

		*pu4NumOfIpv4Addr = 0;
		return FALSE;
	}

	prIfa = ((struct in_device *)(prDev->ip_ptr))->ifa_list;

	/* 4 <2> copy the IPv4 address */
	while ((u4NumIPv4 < u4MaxNumOfAddr) && prIfa) {
		kalMemCopy(&pucIpv4Addrs[u4NumIPv4 * u4AddrLen], &prIfa->ifa_local, u4AddrLen);
		prIfa = prIfa->ifa_next;

		DBGLOG(INIT, INFO,
		       "IPv4 addr [%u][" IPV4STR "]\n", u4NumIPv4, IPV4TOSTR(&pucIpv4Addrs[u4NumIPv4 * u4AddrLen]));

		u4NumIPv4++;
	}

	*pu4NumOfIpv4Addr = u4NumIPv4;

	return TRUE;
}

BOOLEAN
kalGetIPv6Address(IN struct net_device *prDev,
		  IN UINT_32 u4MaxNumOfAddr, OUT PUINT_8 pucIpv6Addrs, OUT PUINT_32 pu4NumOfIpv6Addr)
{
	UINT_32 u4NumIPv6 = 0;
	UINT_32 u4AddrLen = IPV6_ADDR_LEN;
	struct inet6_ifaddr *prIfa;
	struct list_head *prAddrList;

	/* 4 <1> Sanity check of netDevice */
	if (!prDev || !(prDev->ip6_ptr)) {
		DBGLOG(INIT, INFO, "IPv6 address is not available for dev(0x%p)\n", prDev);

		*pu4NumOfIpv6Addr = 0;
		return FALSE;
	}

	prAddrList = &((struct inet6_dev *)(prDev->ip6_ptr))->addr_list;

	/* 4 <2> copy the IPv6 address */
	list_for_each_entry(prIfa, prAddrList, if_list) {
		kalMemCopy(&pucIpv6Addrs[u4NumIPv6 * u4AddrLen], &prIfa->addr, u4AddrLen);

		DBGLOG(INIT, INFO,
		       "IPv6 addr [%u][" IPV6STR "]\n", u4NumIPv6, IPV6TOSTR(&pucIpv6Addrs[u4NumIPv6 * u4AddrLen]));

		if ((u4NumIPv6 + 1) >= u4MaxNumOfAddr)
			break;
		u4NumIPv6++;
	}

	*pu4NumOfIpv6Addr = u4NumIPv6;

	return TRUE;
}

static void wlanNotifyFwSuspend(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgSuspend)
{
	WLAN_STATUS rStatus;
	UINT_32 u4SetInfoLen;

	rStatus = kalIoctl(prGlueInfo,
			wlanoidNotifyFwSuspend,
			(PVOID)&fgSuspend,
			sizeof(fgSuspend),
			FALSE,
			FALSE,
			TRUE,
			&u4SetInfoLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "wlanNotifyFwSuspend fail\n");
}

VOID
kalSetNetAddress(IN P_GLUE_INFO_T prGlueInfo,
		 IN UINT_8 ucBssIdx,
		 IN PUINT_8 pucIPv4Addr, IN UINT_32 u4NumIPv4Addr, IN PUINT_8 pucIPv6Addr, IN UINT_32 u4NumIPv6Addr)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_32 u4SetInfoLen = 0;
	UINT_32 u4Len = OFFSET_OF(PARAM_NETWORK_ADDRESS_LIST, arAddress);
	P_PARAM_NETWORK_ADDRESS_LIST prParamNetAddrList;
	P_PARAM_NETWORK_ADDRESS prParamNetAddr;
	UINT_32 i, u4AddrLen;

	/* 4 <1> Calculate buffer size */
	/* IPv4 */
	u4Len += (((sizeof(PARAM_NETWORK_ADDRESS) - 1) + IPV4_ADDR_LEN) * u4NumIPv4Addr);
	/* IPv6 */
	u4Len += (((sizeof(PARAM_NETWORK_ADDRESS) - 1) + IPV6_ADDR_LEN) * u4NumIPv6Addr);

	/* 4 <2> Allocate buffer */
	prParamNetAddrList = (P_PARAM_NETWORK_ADDRESS_LIST) kalMemAlloc(u4Len, VIR_MEM_TYPE);

	if (!prParamNetAddrList) {
		DBGLOG(INIT, WARN, "Fail to alloc buffer for setting BSS[%u] network address!\n", ucBssIdx);
		return;
	}
	/* 4 <3> Fill up network address */
	prParamNetAddrList->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
	prParamNetAddrList->u4AddressCount = 0;
	prParamNetAddrList->ucBssIdx = ucBssIdx;

	/* 4 <3.1> Fill up IPv4 address */
	u4AddrLen = IPV4_ADDR_LEN;
	prParamNetAddr = prParamNetAddrList->arAddress;
	for (i = 0; i < u4NumIPv4Addr; i++) {
		prParamNetAddr->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
		prParamNetAddr->u2AddressLength = u4AddrLen;
		kalMemCopy(prParamNetAddr->aucAddress, &pucIPv4Addr[i * u4AddrLen], u4AddrLen);

		prParamNetAddr = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prParamNetAddr +
							    (ULONG) (u4AddrLen +
								     OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress)));
	}
	prParamNetAddrList->u4AddressCount += u4NumIPv4Addr;

	/* 4 <3.2> Fill up IPv6 address */
	u4AddrLen = IPV6_ADDR_LEN;
	for (i = 0; i < u4NumIPv6Addr; i++) {
		prParamNetAddr->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
		prParamNetAddr->u2AddressLength = u4AddrLen;
		kalMemCopy(prParamNetAddr->aucAddress, &pucIPv6Addr[i * u4AddrLen], u4AddrLen);

		prParamNetAddr = (P_PARAM_NETWORK_ADDRESS) ((ULONG) prParamNetAddr +
							    (ULONG) (u4AddrLen +
								     OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress)));
	}
	prParamNetAddrList->u4AddressCount += u4NumIPv6Addr;

	/* 4 <4> IOCTL to tx_thread */
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetNetworkAddress,
			   (PVOID) prParamNetAddrList, u4Len, FALSE, FALSE, TRUE, &u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "%s: Fail to set network address\n", __func__);

	kalMemFree(prParamNetAddrList, VIR_MEM_TYPE, u4Len);

}

VOID kalSetNetAddressFromInterface(IN P_GLUE_INFO_T prGlueInfo, IN struct net_device *prDev, IN BOOLEAN fgSet)
{
	UINT_32 u4NumIPv4, u4NumIPv6;
	UINT_8 pucIPv4Addr[IPV4_ADDR_LEN * CFG_PF_ARP_NS_MAX_NUM], pucIPv6Addr[IPV6_ADDR_LEN * CFG_PF_ARP_NS_MAX_NUM];
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;

	prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(prDev);

	if (prNetDevPrivate->prGlueInfo != prGlueInfo)
		DBGLOG(REQ, WARN, "%s: unexpected prGlueInfo(0x%p)!\n", __func__, prNetDevPrivate->prGlueInfo);

	u4NumIPv4 = 0;
	u4NumIPv6 = 0;

	if (fgSet) {
		kalGetIPv4Address(prDev, CFG_PF_ARP_NS_MAX_NUM, pucIPv4Addr, &u4NumIPv4);
		kalGetIPv6Address(prDev, CFG_PF_ARP_NS_MAX_NUM, pucIPv6Addr, &u4NumIPv6);
		wlanNotifyFwSuspend(prGlueInfo, TRUE);
	} else {
		wlanNotifyFwSuspend(prGlueInfo, FALSE);
	}

	if (u4NumIPv4 + u4NumIPv6 > CFG_PF_ARP_NS_MAX_NUM) {
		if (u4NumIPv4 >= CFG_PF_ARP_NS_MAX_NUM) {
			u4NumIPv4 = CFG_PF_ARP_NS_MAX_NUM;
			u4NumIPv6 = 0;
		} else {
			u4NumIPv6 = CFG_PF_ARP_NS_MAX_NUM - u4NumIPv4;
		}
	}

	kalSetNetAddress(prGlueInfo, prNetDevPrivate->ucBssIdx, pucIPv4Addr, u4NumIPv4, pucIPv6Addr, u4NumIPv6);
}

#if CFG_MET_PACKET_TRACE_SUPPORT

BOOLEAN kalMetCheckProfilingPacket(IN P_GLUE_INFO_T prGlueInfo, IN P_NATIVE_PACKET prPacket)
{
	UINT_32 u4PacketLen;
	UINT_16 u2EtherTypeLen;
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	PUINT_8 aucLookAheadBuf = NULL;
	UINT_8 ucEthTypeLenOffset = ETHER_HEADER_LEN - ETHER_TYPE_LEN;
	PUINT_8 pucNextProtocol = NULL;

	u4PacketLen = prSkb->len;

	if (u4PacketLen < ETHER_HEADER_LEN) {
		DBGLOG(INIT, WARN, "Invalid Ether packet length: %u\n", u4PacketLen);
		return FALSE;
	}

	aucLookAheadBuf = prSkb->data;

	/* 4 <0> Obtain Ether Type/Len */
	WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset], &u2EtherTypeLen);

	/* 4 <1> Skip 802.1Q header (VLAN Tagging) */
	if (u2EtherTypeLen == ETH_P_VLAN) {
		ucEthTypeLenOffset += ETH_802_1Q_HEADER_LEN;
		WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset], &u2EtherTypeLen);
	}
	/* 4 <2> Obtain next protocol pointer */
	pucNextProtocol = &aucLookAheadBuf[ucEthTypeLenOffset + ETHER_TYPE_LEN];

	/* 4 <3> Handle ethernet format */
	switch (u2EtherTypeLen) {

		/* IPv4 */
	case ETH_P_IPV4:
		{
			PUINT_8 pucIpHdr = pucNextProtocol;
			UINT_8 ucIpVersion;

			/* IPv4 header length check */
			if (u4PacketLen < (ucEthTypeLenOffset + ETHER_TYPE_LEN + IPV4_HDR_LEN)) {
				DBGLOG(INIT, WARN, "Invalid IPv4 packet length: %u\n", u4PacketLen);
				return FALSE;
			}

			/* IPv4 version check */
			ucIpVersion = (pucIpHdr[0] & IP_VERSION_MASK) >> IP_VERSION_OFFSET;
			if (ucIpVersion != IP_VERSION_4) {
				DBGLOG(INIT, WARN, "Invalid IPv4 packet version: %u\n", ucIpVersion);
				return FALSE;
			}

			if (pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET] == IP_PROTOCOL_UDP) {
				PUINT_8 pucUdpHdr = &pucIpHdr[IPV4_HDR_LEN];
				UINT_16 u2UdpDstPort;
				UINT_16 u2UdpSrcPort;

				/* Get UDP DST port */
				WLAN_GET_FIELD_BE16(&pucUdpHdr[UDP_HDR_DST_PORT_OFFSET], &u2UdpDstPort);

				/* Get UDP SRC port */
				WLAN_GET_FIELD_BE16(&pucUdpHdr[UDP_HDR_SRC_PORT_OFFSET], &u2UdpSrcPort);

				if (u2UdpSrcPort == prGlueInfo->u2MetUdpPort) {
					UINT_16 u2IpId;

					/* Store IP ID for Tag */
					WLAN_GET_FIELD_BE16(&pucIpHdr[IPV4_HDR_IP_IDENTIFICATION_OFFSET], &u2IpId);
#if 0
					DBGLOG(INIT, INFO, "TX PKT PROTOCOL[0x%x] UDP DST port[%u] IP_ID[%u]\n",
							    pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET], u2UdpDstPort,
							    u2IpId);
#endif
					GLUE_SET_PKT_IP_ID(prPacket, u2IpId);

					return TRUE;
				}
			}
		}
		break;

	default:
		break;
	}

	return FALSE;
}

static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(0 == tracing_mark_write_addr))
		tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");
}

VOID kalMetTagPacket(IN P_GLUE_INFO_T prGlueInfo, IN P_NATIVE_PACKET prPacket, IN ENUM_TX_PROFILING_TAG_T eTag)
{
	if (!prGlueInfo->fgMetProfilingEn)
		return;

	switch (eTag) {
	case TX_PROF_TAG_OS_TO_DRV:
		if (kalMetCheckProfilingPacket(prGlueInfo, prPacket)) {
			__mt_update_tracing_mark_write_addr();
#ifdef CONFIG_TRACING		/* #if CFG_MET_PACKET_TRACE_SUPPORT */
			event_trace_printk(tracing_mark_write_addr, "S|%d|%s|%d\n", current->tgid, "WIFI-CHIP",
					   GLUE_GET_PKT_IP_ID(prPacket));
#endif
			GLUE_SET_PKT_FLAG_PROF_MET(prPacket);
		}
		break;

	case TX_PROF_TAG_DRV_TX_DONE:
		if (GLUE_GET_PKT_IS_PROF_MET(prPacket)) {
			__mt_update_tracing_mark_write_addr();
#ifdef CONFIG_TRACING		/* #if CFG_MET_PACKET_TRACE_SUPPORT */
			event_trace_printk(tracing_mark_write_addr, "F|%d|%s|%d\n", current->tgid, "WIFI-CHIP",
					   GLUE_GET_PKT_IP_ID(prPacket));
#endif
		}
		break;

	case TX_PROF_TAG_MAC_TX_DONE:
		break;

	default:
		break;
	}
}

VOID kalMetInit(IN P_GLUE_INFO_T prGlueInfo)
{
	prGlueInfo->fgMetProfilingEn = FALSE;
	prGlueInfo->u2MetUdpPort = 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for adjusting Debug Level to turn on/off debugging message.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
#if 0
static ssize_t kalMetWriteProcfs(struct file *file, const char __user *buffer, size_t count, loff_t *off)
{
	char acBuf[128 + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	int u16MetUdpPort;
	int u8MetProfEnable;

	IN P_GLUE_INFO_T prGlueInfo;
	ssize_t result;

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	result = copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (sscanf(acBuf, " %d %d", &u8MetProfEnable, &u16MetUdpPort) == 2) {
		DBGLOG(INIT, INFO,
			"MET_PROF: Write MET PROC Enable=%d UDP_PORT=%d\n", u8MetProfEnable, u16MetUdpPort);
	}
	if (pMetGlobalData != NULL) {
		prGlueInfo = (P_GLUE_INFO_T) pMetGlobalData;
		prGlueInfo->fgMetProfilingEn = (BOOLEAN) u8MetProfEnable;
		prGlueInfo->u2MetUdpPort = (UINT_16) u16MetUdpPort;
	}
	return count;
}
#endif
static ssize_t kalMetCtrlWriteProcfs(struct file *file, const char __user *buffer, size_t count, loff_t *off)
{
	char acBuf[128 + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	int u8MetProfEnable;
	ssize_t result;

	IN P_GLUE_INFO_T prGlueInfo;

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	result = copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (sscanf(acBuf, " %d", &u8MetProfEnable) == 1) {
		DBGLOG(INIT, INFO,
			"MET_PROF: Write MET PROC Enable=%d\n", u8MetProfEnable);
	}
	if (pMetGlobalData != NULL) {
		prGlueInfo = (P_GLUE_INFO_T) pMetGlobalData;
		prGlueInfo->fgMetProfilingEn = (UINT_8) u8MetProfEnable;
	}
	return count;
}

static ssize_t kalMetPortWriteProcfs(struct file *file, const char __user *buffer, size_t count, loff_t *off)
{
	char acBuf[128 + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	int u16MetUdpPort;
	ssize_t result;

	IN P_GLUE_INFO_T prGlueInfo;

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	result = copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (sscanf(acBuf, " %d", &u16MetUdpPort) == 1) {
		DBGLOG(INIT, INFO,
			"MET_PROF: Write MET PROC UDP_PORT=%d\n", u16MetUdpPort);
	}
	if (pMetGlobalData != NULL) {
		prGlueInfo = (P_GLUE_INFO_T) pMetGlobalData;
		prGlueInfo->u2MetUdpPort = (UINT_16) u16MetUdpPort;
	}
	return count;
}

#if 0
const struct file_operations rMetProcFops = {
.write = kalMetWriteProcfs
};
#endif
const struct file_operations rMetProcCtrlFops = {
.write = kalMetCtrlWriteProcfs
};

const struct file_operations rMetProcPortFops = {
.write = kalMetPortWriteProcfs
};

int kalMetInitProcfs(IN P_GLUE_INFO_T prGlueInfo)
{
	/* struct proc_dir_entry *pMetProcDir; */
	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		DBGLOG(INIT, ERROR, "init proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}
	/*
	 * Directory: Root (/proc/net/wlan0)
	 */
	pMetProcDir = proc_mkdir("wlan0", init_net.proc_net);
	if (pMetProcDir == NULL)
		return -ENOENT;
	/*
	   /proc/net/wlan0
	   |-- met_ctrl         (PROC_MET_PROF_CTRL)
	 */
	/* proc_create(PROC_MET_PROF_CTRL, 0x0644, pMetProcDir, &rMetProcFops); */
	proc_create(PROC_MET_PROF_CTRL, 0, pMetProcDir, &rMetProcCtrlFops);
	proc_create(PROC_MET_PROF_PORT, 0, pMetProcDir, &rMetProcPortFops);

	pMetGlobalData = (void *)prGlueInfo;

	return 0;
}

int kalMetRemoveProcfs(void)
{

	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		DBGLOG(INIT, WARN, "remove proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}
	/* remove_proc_entry(PROC_MET_PROF_CTRL, pMetProcDir);
	remove_proc_entry(PROC_MET_PROF_PORT, pMetProcDir); */
	/* remove root directory (proc/net/wlan0) */
	remove_proc_subtree("wlan0", init_net.proc_net);
	/* clear MetGlobalData */
	pMetGlobalData = NULL;

	return 0;
}

#endif
#if CFG_SUPPORT_AGPS_ASSIST
BOOLEAN kalIndicateAgpsNotify(P_ADAPTER_T prAdapter, UINT_8 cmd, PUINT_8 data, UINT_16 dataLen)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;

	struct sk_buff *skb = cfg80211_testmode_alloc_event_skb(priv_to_wiphy(prGlueInfo),
								dataLen, GFP_KERNEL);
	if (!skb) {
		DBGLOG(AIS, TRACE, "kalIndicateAgpsNotify: alloc skb failed\n");
		return FALSE;
	}
	/* DBGLOG(CCX, INFO, ("WLAN_STATUS_AGPS_NOTIFY, cmd=%d\n", cmd)); */
	if (unlikely(nla_put(skb, MTK_ATTR_AGPS_CMD, sizeof(cmd), &cmd) < 0))
		goto nla_put_failure;
	if (dataLen > 0 && data && unlikely(nla_put(skb, MTK_ATTR_AGPS_DATA, dataLen, data) < 0))
		goto nla_put_failure;
	if (unlikely(nla_put(skb, MTK_ATTR_AGPS_IFINDEX, sizeof(UINT_32), &prGlueInfo->prDevHandler->ifindex) < 0))
		goto nla_put_failure;
	/* currently, the ifname maybe wlan0, p2p0, so the maximum name length will be 5 bytes */
	if (unlikely(nla_put(skb, MTK_ATTR_AGPS_IFNAME, 5, prGlueInfo->prDevHandler->name) < 0))
		goto nla_put_failure;
	cfg80211_testmode_event(skb, GFP_KERNEL);
	return TRUE;

nla_put_failure:
	kfree_skb(skb);
	return FALSE;
}
#endif

UINT_64 kalGetBootTime(void)
{
	struct timespec ts;
	UINT_64 bootTime = 0;

	get_monotonic_boottime(&ts);
	bootTime = ts.tv_sec;
	bootTime *= USEC_PER_SEC;
	bootTime += ts.tv_nsec / NSEC_PER_USEC;
	return bootTime;
}

#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
/* if SPM is not implement this function, we will use this default one */
wake_reason_t __weak slp_get_wake_reason(VOID)
{
	DBGLOG(INIT, WARN, "SPM didn't define this function!\n");
	return WR_NONE;
}

#ifdef MT6630
/* if SPM is not implement this function, we will use this default one */
bool __weak spm_read_eint_status(UINT_32 u4EintNum)
{
	DBGLOG(INIT, WARN, "SPM didn't define this function!\n");
	return FALSE;
}
static inline BOOLEAN spm_check_wakesrc(VOID)
{
	return spm_read_eint_status(4);
}
#else
/* if SPM is not implement this function, we will use this default one */
UINT_32 __weak spm_get_last_wakeup_src(VOID)
{
	return 0;
}
static inline BOOLEAN spm_check_wakesrc(VOID)
{
	return !!(spm_get_last_wakeup_src() & WAKE_SRC_CONN2AP);
}
#endif
BOOLEAN kalIsWakeupByWlan(P_ADAPTER_T  prAdapter)
{
	/* SUSPEND_FLAG_FOR_WAKEUP_REASON is set means system has suspended, but may be failed
		duo to some driver suspend failed. so we need help of function slp_get_wake_reason */
	if (test_and_clear_bit(SUSPEND_FLAG_FOR_WAKEUP_REASON, &prAdapter->ulSuspendFlag) == 0)
		return FALSE;
	/* if slp_get_wake_reason or spm_get_last_wakeup_src is NULL, it means SPM module didn't implement
		it. then we should return FALSE always. otherwise,  if slp_get_wake_reason returns WR_WAKE_SRC,
		then it means the host is suspend successfully. */
	if (slp_get_wake_reason() != WR_WAKE_SRC)
		return FALSE;
	/* spm_get_last_wakeup_src will returns the last wakeup source,
		WAKE_SRC_CONN2AP is connsys */
	return spm_check_wakesrc();
}
#endif

INT_32 kalHaltLock(UINT_32 waitMs)
{
	INT_32 i4Ret = 0;

	if (waitMs) {
		i4Ret = down_timeout(&rHaltCtrl.lock, MSEC_TO_JIFFIES(waitMs));
		if (!i4Ret)
			goto success;
		if (i4Ret != -ETIME)
			return i4Ret;
		if (rHaltCtrl.fgHeldByKalIoctl) {
			P_GLUE_INFO_T prGlueInfo = wlanGetGlueInfo();

			DBGLOG(INIT, ERROR,
				"kalIoctl was executed longer than %u ms, show backtrace of tx_thread!\n",
				kalGetTimeTick() - rHaltCtrl.u4HoldStart);
			if (prGlueInfo)
				show_stack(prGlueInfo->main_thread, NULL);
		} else {
			DBGLOG(INIT, ERROR, "halt lock held by %s pid %d longer than %u ms!\n",
				rHaltCtrl.owner->comm, rHaltCtrl.owner->pid,
				kalGetTimeTick() - rHaltCtrl.u4HoldStart);
			show_stack(rHaltCtrl.owner, NULL);
		}
		return i4Ret;
	}
	down(&rHaltCtrl.lock);
success:
	rHaltCtrl.owner = current;
	rHaltCtrl.u4HoldStart = kalGetTimeTick();
	return 0;
}

INT_32 kalHaltTryLock(VOID)
{
	INT_32 i4Ret = 0;

	i4Ret = down_trylock(&rHaltCtrl.lock);
	if (i4Ret)
		return i4Ret;
	rHaltCtrl.owner = current;
	rHaltCtrl.u4HoldStart = kalGetTimeTick();
	return 0;
}

VOID kalHaltUnlock(VOID)
{
	if (kalGetTimeTick() - rHaltCtrl.u4HoldStart > WLAN_OID_TIMEOUT_THRESHOLD * 2 &&
		rHaltCtrl.owner)
		DBGLOG(INIT, ERROR, "process %s pid %d hold halt lock longer than 4s!\n",
			rHaltCtrl.owner->comm, rHaltCtrl.owner->pid);
	rHaltCtrl.owner = NULL;
	up(&rHaltCtrl.lock);
}

VOID kalSetHalted(BOOLEAN fgHalt)
{
	rHaltCtrl.fgHalt = fgHalt;
}

BOOLEAN kalIsHalted(VOID)
{
	return rHaltCtrl.fgHalt;
}
#ifdef MT6797
VOID kalDumpWTBL(P_ADAPTER_T prAdapter)
{
	UINT_8 i = 0;
	UINT_32 au4WTBL[4] = {0,};
	P_WLAN_TABLE_T prWtbl = prAdapter->rWifiVar.arWtbl;
	PUINT_8 pucWtblBaseAddr = NULL;

	pucWtblBaseAddr = glRemapConnsysAddr(prAdapter->prGlueInfo, 0x60320000, 1024);
	if (!pucWtblBaseAddr)
		return;

	for (i = 0; i < WTBL_SIZE; i++)
		if (prWtbl[i].ucUsed) {
			au4WTBL[0] = CONNSYS_REG_READ(pucWtblBaseAddr, i*0x20);
			au4WTBL[1] = CONNSYS_REG_READ(pucWtblBaseAddr, i*0x20+4);
			au4WTBL[2] = CONNSYS_REG_READ(pucWtblBaseAddr, i*0x20+8);
			au4WTBL[3] = CONNSYS_REG_READ(pucWtblBaseAddr, i*0x20+12);
			DBGLOG(RX, ERROR, "content of wlan index %d:0x%08x%08x%08x%08x\n",
				i, au4WTBL[0], au4WTBL[1], au4WTBL[2], au4WTBL[3]);
		}
	glUnmapConnsysAddr(prAdapter->prGlueInfo, pucWtblBaseAddr, 0x60320000);
}
#endif

#if 0
VOID kalPerMonDump(IN P_GLUE_INFO_T prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, WARN, "ulPerfMonFlag:0x%lx\n", prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, WARN, "ulLastTxBytes:%ld\n", prPerMonitor->ulLastTxBytes);
	DBGLOG(SW4, WARN, "ulLastRxBytes:%ld\n", prPerMonitor->ulLastRxBytes);
	DBGLOG(SW4, WARN, "ulP2PLastTxBytes:%ld\n", prPerMonitor->ulP2PLastTxBytes);
	DBGLOG(SW4, WARN, "ulP2PLastRxBytes:%ld\n", prPerMonitor->ulP2PLastRxBytes);
	DBGLOG(SW4, WARN, "ulThroughput:%ld\n", prPerMonitor->ulThroughput);
	DBGLOG(SW4, WARN, "u4UpdatePeriod:%d\n", prPerMonitor->u4UpdatePeriod);
	DBGLOG(SW4, WARN, "u4TarPerfLevel:%d\n", prPerMonitor->u4TarPerfLevel);
	DBGLOG(SW4, WARN, "u4CurrPerfLevel:%d\n", prPerMonitor->u4CurrPerfLevel);
	DBGLOG(SW4, WARN, "netStats tx_bytes:%ld\n", prGlueInfo->prDevHandler->stats.tx_bytes);
	DBGLOG(SW4, WARN, "netStats tx_bytes:%ld\n", prGlueInfo->prDevHandler->stats.rx_bytes);
	DBGLOG(SW4, WARN, "p2p netStats tx_bytes:%ld\n", prGlueInfo->prP2PInfo->prDevHandler->stats.tx_bytes);
	DBGLOG(SW4, WARN, "p2p netStats tx_bytes:%ld\n", prGlueInfo->prP2PInfo->prDevHandler->stats.rx_bytes);
}
#endif

inline INT_32 kalPerMonInit(IN P_GLUE_INFO_T prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, INFO, "enter %s\n", __func__);
	if (KAL_TEST_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag))
		DBGLOG(SW4, WARN, "abnormal, perf monitory already running\n");
	KAL_CLR_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag);
	KAL_CLR_BIT(PERF_MON_DISABLE_BIT, prPerMonitor->ulPerfMonFlag);
	KAL_SET_BIT(PERF_MON_STOP_BIT, prPerMonitor->ulPerfMonFlag);
	prPerMonitor->u4UpdatePeriod = 1000;
	cnmTimerInitTimer(prGlueInfo->prAdapter,
		&prPerMonitor->rPerfMonTimer,
		(PFN_MGMT_TIMEOUT_FUNC) kalPerMonHandler, (ULONG) NULL);
	DBGLOG(SW4, INFO, "exit %s\n", __func__);
	return 0;
}

inline INT_32 kalPerMonDisable(IN P_GLUE_INFO_T prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;

	DBGLOG(SW4, INFO, "enter %s\n", __func__);
	if (KAL_TEST_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag)) {
		DBGLOG(SW4, TRACE, "need to stop before disable\n");
		kalPerMonStop(prGlueInfo);
	}
	KAL_SET_BIT(PERF_MON_DISABLE_BIT, prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, TRACE, "exit %s\n", __func__);
	return 0;
}

inline INT_32 kalPerMonEnable(IN P_GLUE_INFO_T prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;

	DBGLOG(SW4, INFO, "enter %s\n", __func__);
	KAL_CLR_BIT(PERF_MON_DISABLE_BIT, prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, TRACE, "exit %s\n", __func__);
	return 0;
}

inline INT_32 kalPerMonStart(IN P_GLUE_INFO_T prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, TRACE, "enter %s\n", __func__);

	if ((wlan_fb_power_down || prGlueInfo->fgIsInSuspendMode) &&
		!KAL_TEST_BIT(PERF_MON_DISABLE_BIT, prPerMonitor->ulPerfMonFlag)) {
		/* Remove this to prevent KE, kalPerMonStart might be called in soft irq
		   kalBoostCpu might call flush_work which will use wait_for_completion
		   then KE will happen in this case
		   Simply don't start performance monitor here
		*/
		/*kalPerMonDisable(prGlueInfo);*/
		return 0;
	}
	if (KAL_TEST_BIT(PERF_MON_DISABLE_BIT, prPerMonitor->ulPerfMonFlag) ||
		KAL_TEST_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag))
		return 0;

	prPerMonitor->ulLastRxBytes = 0;
	prPerMonitor->ulLastTxBytes = 0;
	prPerMonitor->ulP2PLastRxBytes = 0;
	prPerMonitor->ulP2PLastTxBytes = 0;
	prPerMonitor->ulThroughput = 0;
	prPerMonitor->u4CurrPerfLevel = 0;
	prPerMonitor->u4TarPerfLevel = 0;

	cnmTimerStartTimer(prGlueInfo->prAdapter, &prPerMonitor->rPerfMonTimer, prPerMonitor->u4UpdatePeriod);
	KAL_SET_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag);
	KAL_CLR_BIT(PERF_MON_STOP_BIT, prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, INFO, "perf monitor started\n");
	return 0;
}

inline INT_32 kalPerMonStop(IN P_GLUE_INFO_T prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, TRACE, "enter %s\n", __func__);

	if (KAL_TEST_BIT(PERF_MON_DISABLE_BIT, prPerMonitor->ulPerfMonFlag)) {
		DBGLOG(SW4, TRACE, "perf monitory disabled\n");
		return 0;
	}

	if (KAL_TEST_BIT(PERF_MON_STOP_BIT, prPerMonitor->ulPerfMonFlag)) {
		DBGLOG(SW4, TRACE, "perf monitory already stopped\n");
		return 0;
	}

	KAL_SET_BIT(PERF_MON_STOP_BIT, prPerMonitor->ulPerfMonFlag);
	if (KAL_TEST_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag)) {
		cnmTimerStopTimer(prGlueInfo->prAdapter, &prPerMonitor->rPerfMonTimer);
		KAL_CLR_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag);
		prPerMonitor->ulLastRxBytes = 0;
		prPerMonitor->ulLastTxBytes = 0;
		prPerMonitor->ulP2PLastRxBytes = 0;
		prPerMonitor->ulP2PLastTxBytes = 0;
		prPerMonitor->ulThroughput = 0;
		prPerMonitor->u4CurrPerfLevel = 0;
		prPerMonitor->u4TarPerfLevel = 0;
		/*Cancel CPU performance mode request*/
		kalBoostCpu(0);
	}
	DBGLOG(SW4, TRACE, "exit %s\n", __func__);
	return 0;
}

inline INT_32 kalPerMonDestroy(IN P_GLUE_INFO_T prGlueInfo)
{
	kalPerMonDisable(prGlueInfo);
	return 0;
}

VOID kalPerMonHandler(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	/*Calculate current throughput*/
	struct PERF_MONITOR_T *prPerMonitor;
	struct net_device *prNetDev = NULL;
	UINT_32 u4Idx = 0;
	P_BSS_INFO_T prP2pBssInfo = (P_BSS_INFO_T) NULL;

	LONG latestTxBytes, latestRxBytes, txDiffBytes, rxDiffBytes;
	LONG p2pLatestTxBytes, p2pLatestRxBytes, p2pTxDiffBytes, p2pRxDiffBytes;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;

	if ((prGlueInfo->ulFlag & GLUE_FLAG_HALT) || (!prAdapter->fgIsP2PRegistered))
		return;

	for (u4Idx = 0; u4Idx < BSS_INFO_NUM; u4Idx++) {
		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, (UINT_8) u4Idx);
		if (prP2pBssInfo->eNetworkType == NETWORK_TYPE_P2P)
			break;
	}
	prNetDev = prGlueInfo->prDevHandler;

	prPerMonitor = &prAdapter->rPerMonitor;
	DBGLOG(SW4, TRACE, "enter kalPerMonHandler\n");

	latestTxBytes = prGlueInfo->prDevHandler->stats.tx_bytes;
	latestRxBytes = prGlueInfo->prDevHandler->stats.rx_bytes;
	p2pLatestTxBytes = prGlueInfo->prP2PInfo->prDevHandler->stats.tx_bytes;
	p2pLatestRxBytes = prGlueInfo->prP2PInfo->prDevHandler->stats.rx_bytes;
	if (0 == prPerMonitor->ulLastRxBytes &&
		0 == prPerMonitor->ulLastTxBytes &&
		0 == prPerMonitor->ulP2PLastRxBytes &&
		0 == prPerMonitor->ulP2PLastTxBytes) {
		prPerMonitor->ulThroughput = 0;
	} else {
		txDiffBytes = latestTxBytes - prPerMonitor->ulLastTxBytes;
		rxDiffBytes = latestRxBytes - prPerMonitor->ulLastRxBytes;
		if (0 > txDiffBytes)
			txDiffBytes = -(txDiffBytes);
		if (0 > rxDiffBytes)
			rxDiffBytes = -(rxDiffBytes);

		p2pTxDiffBytes = p2pLatestTxBytes - prPerMonitor->ulP2PLastTxBytes;
		p2pRxDiffBytes = p2pLatestRxBytes - prPerMonitor->ulP2PLastRxBytes;
		if (0 > p2pTxDiffBytes)
			p2pTxDiffBytes = -(p2pTxDiffBytes);
		if (0 > p2pRxDiffBytes)
			p2pRxDiffBytes = -(p2pRxDiffBytes);

		prPerMonitor->ulThroughput = txDiffBytes + rxDiffBytes + p2pTxDiffBytes + p2pRxDiffBytes;
		prPerMonitor->ulThroughput *= 1000;
		prPerMonitor->ulThroughput /= prPerMonitor->u4UpdatePeriod;
		prPerMonitor->ulThroughput <<= 3;
	}

	prPerMonitor->ulLastTxBytes = latestTxBytes;
	prPerMonitor->ulLastRxBytes = latestRxBytes;
	prPerMonitor->ulP2PLastTxBytes = p2pLatestTxBytes;
	prPerMonitor->ulP2PLastRxBytes = p2pLatestRxBytes;

	if (prPerMonitor->ulThroughput < THROUGHPUT_L1_THRESHOLD)
		prPerMonitor->u4TarPerfLevel = 0;
	else if (prPerMonitor->ulThroughput < THROUGHPUT_L2_THRESHOLD)
		prPerMonitor->u4TarPerfLevel = 1;
	else if (prPerMonitor->ulThroughput < THROUGHPUT_L3_THRESHOLD)
		prPerMonitor->u4TarPerfLevel = 2;
	else if (prPerMonitor->ulThroughput < THROUGHPUT_L4_THRESHOLD)
		prPerMonitor->u4TarPerfLevel = 3;
	else
		prPerMonitor->u4TarPerfLevel = 9;

	if (wlan_fb_power_down ||
		prGlueInfo->fgIsInSuspendMode ||
		!(netif_carrier_ok(prNetDev) ||
		(prP2pBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) ||
		(prP2pBssInfo->rStaRecOfClientList.u4NumElem > 0)))
		kalPerMonStop(prGlueInfo);
	else {
		DBGLOG(SW4, TRACE, "throughput:%ld bps\n", prPerMonitor->ulThroughput);
		if (prPerMonitor->u4TarPerfLevel != prPerMonitor->u4CurrPerfLevel) {
			/* if tar level = 0; core_number=prPerMonitor->u4TarPerfLevel+1*/
			if (prPerMonitor->u4TarPerfLevel)
				kalBoostCpu(prPerMonitor->u4TarPerfLevel+1);
			else
				kalBoostCpu(0);
		}
		cnmTimerStartTimer(prGlueInfo->prAdapter, &prPerMonitor->rPerfMonTimer, prPerMonitor->u4UpdatePeriod);
	}
	prPerMonitor->u4CurrPerfLevel = prPerMonitor->u4TarPerfLevel;

	DBGLOG(SW4, TRACE, "exit kalPerMonHandler\n");
}

INT_32 __weak kalBoostCpu(UINT_32 core_num)
{
	DBGLOG(SW4, WARN, "enter weak kalBoostCpu, core_num:%d\n", core_num);
	return 0;
}

static int wlan_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	INT_32 blank;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T)wlan_fb_notifier_priv_data;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK || !prGlueInfo)
		return 0;

	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted()) {
		kalHaltUnlock();
		return 0;
	}

	blank = *(INT_32 *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		kalPerMonEnable(prGlueInfo);
		wlan_fb_power_down = FALSE;
		break;
	case FB_BLANK_POWERDOWN:
		wlan_fb_power_down = TRUE;
		kalPerMonDisable(prGlueInfo);
		break;
	default:
		break;
	}

	kalHaltUnlock();
	return 0;
}

INT_32 kalFbNotifierReg(IN P_GLUE_INFO_T prGlueInfo)
{
	INT_32 i4Ret;

	wlan_fb_notifier_priv_data = prGlueInfo;
	wlan_fb_notifier.notifier_call = wlan_fb_notifier_callback;
	i4Ret = fb_register_client(&wlan_fb_notifier);
	if (i4Ret)
		DBGLOG(SW4, WARN, "Register wlan_fb_notifier failed:%d\n", i4Ret);
	else
		DBGLOG(SW4, TRACE, "Register wlan_fb_notifier succeed\n");

	return i4Ret;
}

VOID kalFbNotifierUnReg(VOID)
{
	fb_unregister_client(&wlan_fb_notifier);
	wlan_fb_notifier_priv_data = NULL;
}

UINT_8 kalGetEapolKeyType(P_NATIVE_PACKET prPacket)
{
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;

	if (!prSkb)
		return EAPOL_KEY_NOT_KEY;
	return (UINT_8)secGetEapolKeyType(prSkb->data);
}
