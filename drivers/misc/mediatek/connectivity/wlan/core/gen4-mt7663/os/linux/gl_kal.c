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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux
 *     /gl_kal.c#10
 */

/*! \file   gl_kal.c
 *    \brief  GLUE Layer will export the required procedures here for internal
 *            driver stack.
 *
 *    This file contains all routines which are exported from GLUE Layer to
 *    internal driver stack.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_os.h"
#include "gl_kal.h"
#include "gl_wext.h"
#include "precomp.h"
#if CFG_SUPPORT_AGPS_ASSIST
#include <net/netlink.h>
#endif
#if CFG_SUPPORT_MTK_CPU_SCHED
#include <linux/cpumask.h>
#include <mtk_sched_drv.h>
#endif
#ifdef CONFIG_IDME
#include <linux/ctype.h>
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* the maximum length of a file name */
#define FILE_NAME_MAX CFG_FW_NAME_MAX_LEN

/* the maximum number of all possible file name */
#define FILE_NAME_TOTAL 8

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
#if DBG
int allocatedMemSize;
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static void *pvIoBuffer;
static uint32_t pvIoBufferSize;
static uint32_t pvIoBufferUsage;

static struct KAL_HALT_CTRL_T rHaltCtrl = {
	.lock = __SEMAPHORE_INITIALIZER(rHaltCtrl.lock, 1),
	.owner = NULL,
	.fgHalt = TRUE,
	.fgHeldByKalIoctl = FALSE,
	.u4HoldStart = 0,
};
/* framebuffer callback related variable and status flag */
u_int8_t wlan_fb_power_down = FALSE;

#if CFG_FORCE_ENABLE_PERF_MONITOR
u_int8_t wlan_perf_monitor_force_enable = TRUE;
#else
u_int8_t wlan_perf_monitor_force_enable = FALSE;
#endif

#if CFG_ENABLE_WAKE_LOCK
static KAL_WAKE_LOCK_T rRxThreadWakeLock;
static KAL_WAKE_LOCK_T rHifThreadWakeLock;
static KAL_WAKE_LOCK_T rTxThreadWakeLock;
#endif

static struct notifier_block wlan_fb_notifier;
void *wlan_fb_notifier_priv_data;
u_int8_t g_fgIsOid = TRUE;
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if CFG_ENABLE_FW_DOWNLOAD

#if (defined(CONFIG_UIDGID_STRICT_TYPE_CHECKS) || \
	(KERNEL_VERSION(3, 14, 0) <= LINUX_VERSION_CODE))
#define  KUIDT_VALUE(v) (v.val)
#define  KGIDT_VALUE(v) (v.val)
#else
#define  KUIDT_VALUE(v) v
#define  KGIDT_VALUE(v) v
#endif

const struct firmware *fw_entry;

/* Default */
static uint8_t *apucFwName[] = {
	(uint8_t *) CFG_FW_FILENAME "_MT",
	NULL
};

static uint8_t *apucCr4FwName[] = {
	(uint8_t *) CFG_CR4_FW_FILENAME "_" HIF_NAME "_MT",
	(uint8_t *) CFG_CR4_FW_FILENAME "_MT",
	NULL
};

#if CFG_ASSERT_DUMP
/* Core dump debug usage */
#if MTK_WCN_HIF_SDIO
uint8_t *apucCorDumpN9FileName =
	"/data/misc/wifi/FW_DUMP_N9";
uint8_t *apucCorDumpCr4FileName =
	"/data/misc/wifi/FW_DUMP_Cr4";
#else
uint8_t *apucCorDumpN9FileName = "/tmp/FW_DUMP_N9";
uint8_t *apucCorDumpCr4FileName = "/tmp/FW_DUMP_Cr4";
#endif
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
uint32_t kalFirmwareOpen(IN struct GLUE_INFO *prGlueInfo,
			 IN uint8_t **apucNameTable)
{
	uint8_t ucNameIdx;
	/* PPUINT_8 apucNameTable; */
	uint8_t ucCurEcoVer = wlanGetEcoVersion(
				      prGlueInfo->prAdapter);
	u_int8_t fgResult = FALSE;
	int ret;

	/* Try to open FW binary */
	for (ucNameIdx = 0; apucNameTable[ucNameIdx]; ucNameIdx++) {
		/*
		 * Driver support request_firmware() to get files
		 * Android path: "/etc/firmware", "/vendor/firmware",
		 *               "/firmware/image"
		 * Linux path: "/lib/firmware", "/lib/firmware/update"
		 */
		ret = request_firmware(&fw_entry, apucNameTable[ucNameIdx],
				       prGlueInfo->prDev);

		if (ret) {
			DBGLOG(INIT, ERROR,
			       "Request FW image: %s failed, errno[%d]\n",
			       apucNameTable[ucNameIdx], fgResult);
			continue;
		} else {
			DBGLOG(INIT, TRACE, "Request FW image: %s done\n",
			       apucNameTable[ucNameIdx]);
			fgResult = TRUE;
			break;
		}
	}


	/* Check result */
	if (!fgResult)
		goto error_open;


	return WLAN_STATUS_SUCCESS;

error_open:
	DBGLOG(INIT, ERROR,
		"Request FW image failed! Cur ECO Ver[E%u]\n",
		ucCurEcoVer);

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
uint32_t kalFirmwareClose(IN struct GLUE_INFO *prGlueInfo)
{
	release_firmware(fw_entry);

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
uint32_t kalFirmwareLoad(IN struct GLUE_INFO *prGlueInfo,
			 OUT void *prBuf, IN uint32_t u4Offset,
			 OUT uint32_t *pu4Size)
{
	ASSERT(prGlueInfo);
	ASSERT(pu4Size);
	ASSERT(prBuf);

	if ((fw_entry == NULL) || (fw_entry->size == 0)
	    || (fw_entry->data == NULL)) {
		goto error_read;
	} else {
		memcpy(prBuf, fw_entry->data, fw_entry->size);
		*pu4Size = fw_entry->size;
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

uint32_t kalFirmwareSize(IN struct GLUE_INFO *prGlueInfo,
			 OUT uint32_t *pu4Size)
{
	ASSERT(prGlueInfo);
	ASSERT(pu4Size);

	*pu4Size = fw_entry->size;

	return WLAN_STATUS_SUCCESS;
}

void
kalConstructDefaultFirmwarePrio(struct GLUE_INFO
				*prGlueInfo, uint8_t **apucNameTable,
				uint8_t **apucName, uint8_t *pucNameIdx,
				uint8_t ucMaxNameIdx)
{
	struct mt66xx_chip_info *prChipInfo =
			prGlueInfo->prAdapter->chip_info;
	uint32_t chip_id = prChipInfo->chip_id;
	uint8_t sub_idx = 0;

	for (sub_idx = 0; apucNameTable[sub_idx]; sub_idx++) {
		if ((*pucNameIdx + 3) < ucMaxNameIdx) {
			/* Type 1. WIFI_RAM_CODE_MTxxxx */
			snprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
				"%s%x", apucNameTable[sub_idx], chip_id);
			(*pucNameIdx) += 1;

			/* Type 2. WIFI_RAM_CODE_MTxxxx.bin */
			snprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
				 "%s%x.bin",
				 apucNameTable[sub_idx], chip_id);
			(*pucNameIdx) += 1;

			/* Type 3. WIFI_RAM_CODE_MTxxxx_Ex */
			snprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
				 "%s%x_E%u",
				 apucNameTable[sub_idx], chip_id,
				 wlanGetEcoVersion(prGlueInfo->prAdapter));
			(*pucNameIdx) += 1;

			/* Type 4. WIFI_RAM_CODE_MTxxxx_Ex.bin */
			snprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
				 "%s%x_E%u.bin",
				 apucNameTable[sub_idx], chip_id,
				 wlanGetEcoVersion(prGlueInfo->prAdapter));
			(*pucNameIdx) += 1;
		} else {
			/* the table is not large enough */
			DBGLOG(INIT, ERROR,
			       "kalFirmwareImageMapping >> file name array is not enough.\n");
			ASSERT(0);
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to load firmware image
 *
 * \param pvGlueInfo     Pointer of GLUE Data Structure
 * \param ppvMapFileBuf  Pointer of pointer to memory-mapped firmware image
 * \param pu4FileLength  File length and memory mapped length as well
 *
 * \retval Map File Handle, used for unammping
 */
/*----------------------------------------------------------------------------*/

void *
kalFirmwareImageMapping(IN struct GLUE_INFO *prGlueInfo,
			OUT void **ppvMapFileBuf, OUT uint32_t *pu4FileLength,
			IN enum ENUM_IMG_DL_IDX_T eDlIdx)
{
	uint8_t **apucNameTable = NULL;
	uint8_t *apucName[FILE_NAME_TOTAL +
					  1]; /* extra +1, for the purpose of
					       * detecting the end of the array
					       */
	uint8_t idx = 0, max_idx, ucRomVer = 0,
		aucNameBody[FILE_NAME_TOTAL][FILE_NAME_MAX], sub_idx = 0;
	struct mt66xx_chip_info *prChipInfo =
			prGlueInfo->prAdapter->chip_info;
	uint32_t chip_id = prChipInfo->chip_id;

	DEBUGFUNC("kalFirmwareImageMapping");

	ASSERT(prGlueInfo);
	ASSERT(ppvMapFileBuf);
	ASSERT(pu4FileLength);

	*ppvMapFileBuf = NULL;
	*pu4FileLength = 0;

	do {
		/* <0.0> Get FW name prefix table */
		switch (eDlIdx) {
		case IMG_DL_IDX_N9_FW:
			apucNameTable = apucFwName;
			break;

		case IMG_DL_IDX_CR4_FW:
			apucNameTable = apucCr4FwName;
			break;

		case IMG_DL_IDX_PATCH:
			break;

		default:
			ASSERT(0);
			break;
		}

		/* <0.2> Construct FW name */
		memset(apucName, 0, sizeof(apucName));

		/* magic number 1: reservation for detection
		 * of the end of the array
		 */
		max_idx = (sizeof(apucName) / sizeof(uint8_t *)) - 1;

		idx = 0;
		apucName[idx] = (uint8_t *)(aucNameBody + idx);

		if (eDlIdx == IMG_DL_IDX_PATCH) {
			/* construct the file name for patch */

			/* mtxxxx_patch_ex_hdr.bin*/
			if (prChipInfo->fw_dl_ops->constructPatchName)
				prChipInfo->fw_dl_ops->constructPatchName(
					prGlueInfo, apucName, &idx);
			else {
				ucRomVer = wlanGetRomVersion(
						prGlueInfo->prAdapter) + 1;
				snprintf(apucName[idx], FILE_NAME_MAX,
					"mt%x_patch_e%u_hdr.bin", chip_id,
					ucRomVer);
			}
			idx += 1;
		} else {
			for (sub_idx = 0; sub_idx < max_idx; sub_idx++)
				apucName[sub_idx] =
					(uint8_t *)(aucNameBody + sub_idx);

			if (prChipInfo->fw_dl_ops->constructFirmwarePrio)
				prChipInfo->fw_dl_ops->constructFirmwarePrio(
					prGlueInfo, apucNameTable, apucName,
					&idx, max_idx);
			else
				kalConstructDefaultFirmwarePrio(
					prGlueInfo, apucNameTable, apucName,
					&idx, max_idx);
		}

		/* let the last pointer point to NULL
		 * so that we can detect the end of the array in
		 * kalFirmwareOpen().
		 */
		apucName[idx] = NULL;

		apucNameTable = apucName;

		/* <1> Open firmware */
		if (kalFirmwareOpen(prGlueInfo,
				    apucNameTable) != WLAN_STATUS_SUCCESS)
			break;
		{
			uint32_t u4FwSize = 0;
			void *prFwBuffer = NULL;
			/* <2> Query firmare size */
			kalFirmwareSize(prGlueInfo, &u4FwSize);
			/* <3> Use vmalloc for allocating large memory trunk */
			prFwBuffer = vmalloc(ALIGN_4(u4FwSize));
			/* <4> Load image binary into buffer */
			if (kalFirmwareLoad(prGlueInfo, prFwBuffer, 0,
					    &u4FwSize) != WLAN_STATUS_SUCCESS) {
				vfree(prFwBuffer);
				kalFirmwareClose(prGlueInfo);
				break;
			}
			/* <5> write back info */
			*pu4FileLength = u4FwSize;
			*ppvMapFileBuf = prFwBuffer;

			return prFwBuffer;
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

void kalFirmwareImageUnmapping(IN struct GLUE_INFO
		       *prGlueInfo, IN void *prFwHandle, IN void *pvMapFileBuf)
{
	DEBUGFUNC("kalFirmwareImageUnmapping");

	ASSERT(prGlueInfo);

	/* pvMapFileBuf might be NULL when file doesn't exist */
	if (pvMapFileBuf)
		vfree(pvMapFileBuf);

	kalFirmwareClose(prGlueInfo);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is provided by GLUE Layer for internal driver stack to
 *        acquire OS SPIN_LOCK.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] rLockCategory  Specify which SPIN_LOCK
 * \param[out] pu4Flags      Pointer of a variable for saving IRQ flags
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void kalAcquireSpinLock(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_SPIN_LOCK_CATEGORY_E rLockCategory,
			OUT unsigned long *plFlags)
{
	unsigned long ulFlags = 0;

	ASSERT(prGlueInfo);
	ASSERT(plFlags);

	if (rLockCategory < SPIN_LOCK_NUM) {
		DBGLOG(INIT, LOUD, "SPIN_LOCK[%u] Try to acquire\n",
		       rLockCategory);
#if CFG_USE_SPIN_LOCK_BOTTOM_HALF
		spin_lock_bh(&prGlueInfo->rSpinLock[rLockCategory]);
#else /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
		spin_lock_irqsave(&prGlueInfo->rSpinLock[rLockCategory],
				  ulFlags);
#endif /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */

		*plFlags = ulFlags;

		DBGLOG(INIT, LOUD, "SPIN_LOCK[%u] Acquired\n",
		       rLockCategory);
	}

}				/* end of kalAcquireSpinLock() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is provided by GLUE Layer for internal driver stack to
 *        release OS SPIN_LOCK.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] rLockCategory  Specify which SPIN_LOCK
 * \param[in] u4Flags        Saved IRQ flags
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void kalReleaseSpinLock(IN struct GLUE_INFO *prGlueInfo,
			IN enum ENUM_SPIN_LOCK_CATEGORY_E rLockCategory,
			IN unsigned long ulFlags)
{
	ASSERT(prGlueInfo);

	if (rLockCategory < SPIN_LOCK_NUM) {

#if CFG_USE_SPIN_LOCK_BOTTOM_HALF
		spin_unlock_bh(&prGlueInfo->rSpinLock[rLockCategory]);
#else /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
		spin_unlock_irqrestore(
			&prGlueInfo->rSpinLock[rLockCategory], ulFlags);
#endif /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
		DBGLOG(INIT, LOUD, "SPIN_UNLOCK[%u]\n", rLockCategory);
	}

}				/* end of kalReleaseSpinLock() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is provided by GLUE Layer for internal driver stack to
 *        acquire OS MUTEX.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] rMutexCategory  Specify which MUTEX
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void kalAcquireMutex(IN struct GLUE_INFO *prGlueInfo,
		     IN enum ENUM_MUTEX_CATEGORY_E rMutexCategory)
{
	ASSERT(prGlueInfo);

	if (rMutexCategory < MUTEX_NUM) {
		DBGLOG(INIT, TEMP,
			"MUTEX_LOCK[%u] Try to acquire\n", rMutexCategory);
		mutex_lock(&prGlueInfo->arMutex[rMutexCategory]);
		DBGLOG(INIT, TEMP, "MUTEX_LOCK[%u] Acquired\n", rMutexCategory);
	}

}				/* end of kalAcquireMutex() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is provided by GLUE Layer for internal driver stack to
 *        release OS MUTEX.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] rMutexCategory  Specify which MUTEX
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void kalReleaseMutex(IN struct GLUE_INFO *prGlueInfo,
		     IN enum ENUM_MUTEX_CATEGORY_E rMutexCategory)
{
	ASSERT(prGlueInfo);

	if (rMutexCategory < MUTEX_NUM) {
		mutex_unlock(&prGlueInfo->arMutex[rMutexCategory]);
		DBGLOG(INIT, TEMP, "MUTEX_UNLOCK[%u]\n", rMutexCategory);
	}

}				/* end of kalReleaseMutex() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is provided by GLUE Layer for internal driver stack to
 *        update current MAC address.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] pucMacAddr     Pointer of current MAC address
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void kalUpdateMACAddress(IN struct GLUE_INFO *prGlueInfo,
			 IN uint8_t *pucMacAddr)
{
	ASSERT(prGlueInfo);
	ASSERT(pucMacAddr);

	if (UNEQUAL_MAC_ADDR(prGlueInfo->prDevHandler->dev_addr,
			     pucMacAddr))
		memcpy(prGlueInfo->prDevHandler->dev_addr, pucMacAddr,
		       PARAM_MAC_ADDR_LEN);

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
void kalQueryTxChksumOffloadParam(IN void *pvPacket,
				  OUT uint8_t *pucFlag)
{
	struct sk_buff *skb = (struct sk_buff *)pvPacket;
	uint8_t ucFlag = 0;

	ASSERT(pvPacket);
	ASSERT(pucFlag);

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
#if DBG
		/* Kevin: do double check, we can remove this part in Normal
		 * Driver.
		 * Because we register NIC feature with NETIF_F_IP_CSUM for
		 * MT5912B MAC, so we'll process IP packet only.
		 */
		if (skb->protocol != htons(ETH_P_IP)) {

		} else
#endif
			ucFlag |= (TX_CS_IP_GEN | TX_CS_TCP_UDP_GEN);
	}

	*pucFlag = ucFlag;

}				/* kalQueryChksumOffloadParam */

/* 4 2007/10/8, mikewu, this is rewritten by Mike */
/*----------------------------------------------------------------------------*/
/*!
 * \brief To update the checksum offload status to the packet to be indicated to
 *        OS.
 *
 * \param[in] pvPacket Pointer to the packet descriptor.
 * \param[in] pucFlag  Points to the offload related parameter.
 *
 * \return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void kalUpdateRxCSUMOffloadParam(IN void *pvPacket,
				 IN enum ENUM_CSUM_RESULT aeCSUM[])
{
	struct sk_buff *skb = (struct sk_buff *)pvPacket;

	ASSERT(pvPacket);

	if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_SUCCESS
	     || aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_SUCCESS)
	    && ((aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_SUCCESS)
		|| (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_SUCCESS))) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else {
		skb->ip_summed = CHECKSUM_NONE;
#if DBG
		if (aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_NONE
		    && aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_NONE)
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
void kalPacketFree(IN struct GLUE_INFO *prGlueInfo,
		   IN void *pvPacket)
{
	dev_kfree_skb((struct sk_buff *)pvPacket);
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
void *kalPacketAlloc(IN struct GLUE_INFO *prGlueInfo,
		     IN uint32_t u4Size, OUT uint8_t **ppucData)
{
	struct mt66xx_chip_info *prChipInfo;
	struct sk_buff *prSkb;
	uint32_t u4TxHeadRoomSize;
	uint32_t u4flag = 0;

	prChipInfo = prGlueInfo->prAdapter->chip_info;
	u4TxHeadRoomSize = NIC_TX_DESC_AND_PADDING_LENGTH +
			   prChipInfo->txd_append_size;

	/* Enable __GFP_NORETRY to prevent oom issue when WiFi traffic */
	/* Only Disable __GFP_NORETRY for nicRxInitialize */
	if (prGlueInfo->prAdapter->fgIsFwDownloaded)
		u4flag = __GFP_NORETRY;

	if (in_interrupt())
		prSkb = __dev_alloc_skb(u4Size + u4TxHeadRoomSize,
					GFP_ATOMIC | __GFP_NOWARN |
					u4flag);
	else
		prSkb = __dev_alloc_skb(u4Size + u4TxHeadRoomSize,
					GFP_KERNEL | u4flag);

	if (prSkb) {
		skb_reserve(prSkb, u4TxHeadRoomSize);

		*ppucData = (uint8_t *) (prSkb->data);

		kalResetPacket(prGlueInfo, (void *) prSkb);
	}
#if DBG
	{
		uint32_t *pu4Head = (uint32_t *) &prSkb->cb[0];
		*pu4Head = (uint32_t) prSkb->head;
		DBGLOG(RX, TRACE, "prSkb->head = %#lx, prSkb->cb = %#lx\n",
		       (uint32_t) prSkb->head, *pu4Head);
	}
#endif
	return (void *) prSkb;
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
void *kalPacketAllocWithHeadroom(IN struct GLUE_INFO
		 *prGlueInfo, IN uint32_t u4Size, OUT uint8_t **ppucData)
{
	struct sk_buff *prSkb = dev_alloc_skb(u4Size);

	if (!prSkb) {
		DBGLOG(TX, WARN, "alloc skb failed\n");
		return NULL;
	}

	/*
	 * Reserve NIC_TX_HEAD_ROOM as this skb
	 * is allocated by driver instead of kernel.
	 */
	skb_reserve(prSkb, NIC_TX_HEAD_ROOM);

	*ppucData = (uint8_t *) (prSkb->data);

	kalResetPacket(prGlueInfo, (void *) prSkb);
#if DBG
	{
		uint32_t *pu4Head = (uint32_t *) &prSkb->cb[0];
		*pu4Head = (uint32_t) prSkb->head;
		DBGLOG(RX, TRACE, "prSkb->head = %#lx, prSkb->cb = %#lx\n",
		       (uint32_t) prSkb->head, *pu4Head);
	}
#endif
	return (void *) prSkb;
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
uint32_t
kalProcessRxPacket(IN struct GLUE_INFO *prGlueInfo,
		   IN void *pvPacket, IN uint8_t *pucPacketStart,
		   IN uint32_t u4PacketLen,
		   /* IN PBOOLEAN           pfgIsRetain, */
		   IN u_int8_t fgIsRetain, IN enum ENUM_CSUM_RESULT aerCSUM[])
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct sk_buff *skb = (struct sk_buff *)pvPacket;

	skb->data = (unsigned char *)pucPacketStart;

	/* Reset skb */
	skb_reset_tail_pointer(skb);
	skb_trim(skb, 0);

	if (skb->tail > skb->end) {
		DBGLOG(RX, ERROR,
#ifdef NET_SKBUFF_DATA_USES_OFFSET
			"[skb:0x%p][skb->len:%d][skb->protocol:0x%02X] tail:%u, end:%u, data:%p\n",
#else
			"[skb:0x%p][skb->len:%d][skb->protocol:0x%02X] tail:%p, end:%p, data:%p\n",
#endif
			(uint8_t *) skb,
			skb->len,
			skb->protocol,
			skb->tail,
			skb->end,
			skb->data);
		DBGLOG_MEM32(RX, ERROR, (uint32_t *) skb->data, skb->len);
		return WLAN_STATUS_FAILURE;
	}

	/* Put data */
	skb_put(skb, u4PacketLen);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	if (prGlueInfo->prAdapter->fgIsSupportCsumOffload)
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
uint32_t kalRxIndicatePkts(IN struct GLUE_INFO *prGlueInfo,
			   IN void *apvPkts[], IN uint8_t ucPktNum)
{
	uint8_t ucIdx = 0;

	ASSERT(prGlueInfo);
	ASSERT(apvPkts);

	for (ucIdx = 0; ucIdx < ucPktNum; ucIdx++)
		kalRxIndicateOnePkt(prGlueInfo, apvPkts[ucIdx]);

	KAL_WAKE_LOCK_TIMEOUT(prGlueInfo->prAdapter,
		&prGlueInfo->rTimeoutWakeLock, MSEC_TO_JIFFIES(
		prGlueInfo->prAdapter->rWifiVar.u4WakeLockRxTimeout));

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
uint32_t kalRxIndicateOnePkt(IN struct GLUE_INFO
			     *prGlueInfo, IN void *pvPkt)
{
	struct net_device *prNetDev = prGlueInfo->prDevHandler;
	struct sk_buff *prSkb = NULL;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prGlueInfo);
	ASSERT(pvPkt);

	prSkb = pvPkt;
	prChipInfo = prGlueInfo->prAdapter->chip_info;
#if DBG && 0
	do {
		uint8_t *pu4Head = (uint8_t *) &prSkb->cb[0];
		uint32_t u4HeadValue = 0;

		kalMemCopy(&u4HeadValue, pu4Head, sizeof(u4HeadValue));
		DBGLOG(RX, TRACE, "prSkb->head = 0x%p, prSkb->cb = 0x%lx\n",
		       pu4Head, u4HeadValue);
	} while (0);
#endif

#if 1

	prNetDev = (struct net_device *)wlanGetNetInterfaceByBssIdx(
			   prGlueInfo, GLUE_GET_PKT_BSS_IDX(prSkb));
	if (!prNetDev)
		prNetDev = prGlueInfo->prDevHandler;
#if CFG_SUPPORT_SNIFFER
	if (prGlueInfo->fgIsEnableMon)
		prNetDev = prGlueInfo->prMonDevHandler;
#endif
	if (prNetDev->dev_addr == NULL) {
		DBGLOG(RX, WARN, "dev_addr == NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prNetDev->stats.rx_bytes += prSkb->len;
	prNetDev->stats.rx_packets++;
#if (CFG_SUPPORT_PERF_IND == 1)
	if (GLUE_GET_PKT_BSS_IDX(prSkb) < BSS_DEFAULT_NUM) {
		/* update Performance Indicator statistics*/
		prGlueInfo->PerfIndCache.u4CurRxBytes
		[GLUE_GET_PKT_BSS_IDX(prSkb)] += prSkb->len;
	}
#endif
#else
	if (GLUE_GET_PKT_IS_P2P(prSkb)) {
		/* P2P */
#if CFG_ENABLE_WIFI_DIRECT
		if (prGlueInfo->prAdapter->fgIsP2PRegistered)
			prNetDev = kalP2PGetDevHdlr(prGlueInfo);
		/* prNetDev->stats.rx_bytes += prSkb->len; */
		/* prNetDev->stats.rx_packets++; */
		prGlueInfo->prP2PInfo[0]->rNetDevStats.rx_bytes +=
			prSkb->len;
		prGlueInfo->prP2PInfo[0]->rNetDevStats.rx_packets++;

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

	StatsEnvRxTime2Host(prGlueInfo->prAdapter, prSkb);

#if KERNEL_VERSION(4, 11, 0) <= CFG80211_VERSION_CODE
	/* ToDo jiffies assignment */
#else
	prNetDev->last_rx = jiffies;
#endif

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
#ifdef NET_SKBUFF_DATA_USES_OFFSET
		       "kalRxIndicateOnePkt [prSkb = 0x%p][prSkb->len = %d][prSkb->protocol = 0x%02x] %u,%u\n",
#else
		       "kalRxIndicateOnePkt [prSkb = 0x%p][prSkb->len = %d][prSkb->protocol = 0x%02x] %p,%p\n",
#endif
		       prSkb, prSkb->len, prSkb->protocol, prSkb->tail,
		       prSkb->end);
		DBGLOG_MEM32(RX, ERROR, (uint32_t *) prSkb->data,
			     prSkb->len);
	}

	if (prSkb->protocol == NTOHS(ETH_P_8021Q)
	    && !FEAT_SUP_LLC_VLAN_RX(prChipInfo)) {
		/*
		 * DA-MAC + SA-MAC + 0x8100 was removed in eth_type_trans()
		 * pkt format here is
		 * TCI(2-bytes) + Len(2-btyes) + payload-type(2-bytes) + payload
		 * Remove "Len" field inserted by RX VLAN header translation
		 * Note: TCI+payload-type is a standard 8021Q header
		 *
		 * This format update is based on RX VLAN HW header translation.
		 * If the setting was changed, you may need to change rules here
		 * as well.
		 */
		const uint8_t vlan_skb_mem_move = 2;

		/* Remove "Len" and shift data pointer 2 bytes */
		kalMemCopy(prSkb->data + vlan_skb_mem_move, prSkb->data,
			   vlan_skb_mem_move);
		skb_pull_rcsum(prSkb, vlan_skb_mem_move);

		/* Have to update MAC header properly. Otherwise, wrong MACs
		 * woud be passed up
		 */
		kalMemMove(prSkb->data - ETH_HLEN,
			   prSkb->data - ETH_HLEN - vlan_skb_mem_move,
			   ETH_HLEN);
		prSkb->mac_header += vlan_skb_mem_move;

		skb_reset_network_header(prSkb);
		skb_reset_transport_header(prSkb);
		kal_skb_reset_mac_len(prSkb);
	}

	if (prSkb->protocol == HTONS(ETH_P_1X))
		DBGLOG(RX, INFO, "Rx EAPOL Frame [Len: %d]\n", prSkb->len);

#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	if (kalRxNapiValidSkb(prGlueInfo, prSkb)) {
		skb_queue_tail(&prGlueInfo->rRxNapiSkbQ, prSkb);
		napi_schedule(&prGlueInfo->rNapi);
		return WLAN_STATUS_SUCCESS;
	}
#endif /* CFG_RX_NAPI_SUPPORT */

	if (!in_interrupt())
		netif_rx_ni(prSkb);	/* only in non-interrupt context */
	else
		netif_rx(prSkb);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Called by driver to indicate event to upper layer, for example, the
 *        wpa supplicant or wireless tools.
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
void
kalIndicateStatusAndComplete(IN struct GLUE_INFO
			     *prGlueInfo, IN uint32_t eStatus, IN void *pvBuf,
			     IN uint32_t u4BufLen)
{

	uint32_t bufLen;
	struct PARAM_STATUS_INDICATION *pStatus;
	struct PARAM_AUTH_EVENT *pAuth;
	struct PARAM_PMKID_CANDIDATE_LIST *pPmkid;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	struct PARAM_SSID ssid;
	struct ieee80211_channel *prChannel = NULL;
	struct cfg80211_bss *bss = NULL;
	uint8_t ucChannelNum;
	struct BSS_DESC *prBssDesc = NULL;
	uint8_t fgScanAborted = FALSE;
	struct wiphy *wiphy = NULL;

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	struct cfg80211_roam_info rRoamInfo = { 0 };
#endif

	GLUE_SPIN_LOCK_DECLARATION();

	kalMemZero(arBssid, MAC_ADDR_LEN);

	ASSERT(prGlueInfo);

	wiphy = priv_to_wiphy(prGlueInfo);

	pStatus = (struct PARAM_STATUS_INDICATION *)pvBuf;
	pAuth = (struct PARAM_AUTH_EVENT *)pStatus;
	pPmkid = (struct PARAM_PMKID_CANDIDATE_LIST *)(pStatus + 1);

	switch (eStatus) {
	case WLAN_STATUS_ROAM_OUT_FIND_BEST:
	case WLAN_STATUS_MEDIA_CONNECT:

		prGlueInfo->eParamMediaStateIndicated =
			PARAM_MEDIA_STATE_CONNECTED;

		/* indicate assoc event */
		wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
					&arBssid[0], sizeof(arBssid), &bufLen);
		wext_indicate_wext_event(prGlueInfo, SIOCGIWAP, arBssid,
					 bufLen);

		/* switch netif on */
		netif_carrier_on(prGlueInfo->prDevHandler);
#if CFG_SUPPORT_CFG80211_AUTH /* Report RX association response frame */
		DBGLOG(INIT, INFO,
			"Skip report CONNECTED when using supplicant SME\n");
#if CFG_SUPPORT_802_11R
		kalMemFree(prGlueInfo->rFtIeForTx.prMDIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prMDIE));
		kalMemFree(prGlueInfo->rFtIeForTx.prFTIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prFTIE));
		kalMemFree(prGlueInfo->rFtIeForTx.prRsnIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prRsnIE));
		kalMemFree(prGlueInfo->rFtIeForTx.prTIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prTIE));
		kalMemZero(&prGlueInfo->rFtIeForTx,
			   sizeof(prGlueInfo->rFtIeForTx));
#endif
		return;
#endif

		do {
			/* print message on console */
			wlanQueryInformation(prGlueInfo->prAdapter,
			     wlanoidQuerySsid, &ssid, sizeof(ssid), &bufLen);

			ssid.aucSsid[(ssid.u4SsidLen >= PARAM_MAX_LEN_SSID) ?
			    (PARAM_MAX_LEN_SSID - 1) : ssid.u4SsidLen] = '\0';
			DBGLOG(INIT, INFO,
				"[wifi] %s netif_carrier_on [ssid:%s " MACSTR
				"], Mac:" MACSTR "\n",
				prGlueInfo->prDevHandler->name, ssid.aucSsid,
				MAC2STR(arBssid),
				MAC2STR(
				prGlueInfo->prAdapter->rWifiVar.aucMacAddress));
		} while (0);

		if (prGlueInfo->fgIsRegistered == TRUE) {
			struct cfg80211_bss *bss_others = NULL;
			uint8_t ucLoopCnt =
				15; /* only loop 15 times to avoid dead loop */

			/* retrieve channel */
			ucChannelNum =
				wlanGetChannelNumberByNetwork(
					prGlueInfo->prAdapter,
					prGlueInfo->prAdapter->prAisBssInfo->
					ucBssIndex);
			if (ucChannelNum <= 14) {
				prChannel =
					ieee80211_get_channel(
						wiphy,
						ieee80211_channel_to_frequency
						(ucChannelNum, KAL_BAND_2GHZ));
			} else {
				prChannel =
					ieee80211_get_channel(
						wiphy,
						ieee80211_channel_to_frequency
						(ucChannelNum, KAL_BAND_5GHZ));
			}

			if (!prChannel) {
				DBGLOG(SCN, ERROR,
				       "prChannel is NULL and ucChannelNum is %d\n",
				       ucChannelNum);
				break;
			}

			/* ensure BSS exists */
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
			bss = cfg80211_get_bss(
				wiphy, prChannel, arBssid,
				ssid.aucSsid, ssid.u4SsidLen,
				IEEE80211_BSS_TYPE_ESS,
				IEEE80211_PRIVACY_ANY);
#else
			bss = cfg80211_get_bss(
				wiphy, prChannel, arBssid,
				ssid.aucSsid, ssid.u4SsidLen,
				WLAN_CAPABILITY_ESS,
				WLAN_CAPABILITY_ESS);
#endif
			if (bss == NULL) {
				/* create BSS on-the-fly */
				prBssDesc = ((struct AIS_FSM_INFO *)
					     (&(prGlueInfo->prAdapter->
					     rWifiVar.rAisFsmInfo)))->
					     prTargetBssDesc;

				if (prBssDesc != NULL) {
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
					bss = cfg80211_inform_bss(
			wiphy,
			prChannel,
			CFG80211_BSS_FTYPE_PRESP,
			arBssid,
			0, /* TSF */
			WLAN_CAPABILITY_ESS,
			prBssDesc->u2BeaconInterval, /* beacon interval */
			prBssDesc->aucIEBuf, /* IE */
			prBssDesc->u2IELength, /* IE Length */
			RCPI_TO_dBm(prBssDesc->ucRCPI) * 100, /* MBM */
			GFP_KERNEL);
#else
					bss = cfg80211_inform_bss(
			wiphy,
			prChannel,
			arBssid,
			0, /* TSF */
			WLAN_CAPABILITY_ESS,
			prBssDesc->u2BeaconInterval, /* beacon interval */
			prBssDesc->aucIEBuf, /* IE */
			prBssDesc->u2IELength, /* IE Length */
			RCPI_TO_dBm(prBssDesc->ucRCPI) * 100, /* MBM */
			GFP_KERNEL);
#endif
				}
			}

			StatsResetTxRx();

			/* remove all bsses that before and only channel
			 * different with the current connected one
			 * if without this patch, UI will show channel A is
			 * connected even if AP has change channel from A to B
			 */
			while (ucLoopCnt--) {
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
				bss_others = cfg80211_get_bss(
						wiphy,
						NULL, arBssid, ssid.aucSsid,
						ssid.u4SsidLen,
						IEEE80211_BSS_TYPE_ESS,
						IEEE80211_PRIVACY_ANY);
#else
				bss_others = cfg80211_get_bss(
						wiphy,
						NULL, arBssid, ssid.aucSsid,
						ssid.u4SsidLen,
						WLAN_CAPABILITY_ESS,
						WLAN_CAPABILITY_ESS);
#endif
				if (bss && bss_others && bss_others != bss) {
					DBGLOG(SCN, INFO,
					       "remove BSSes that only channel different\n");
					cfg80211_unlink_bss(
						wiphy,
						bss_others);

					cfg80211_put_bss(wiphy, bss_others);
				} else {
					if (bss_others) {
						cfg80211_put_bss(wiphy,
								 bss_others);
					}
					break;
				}
			}

			/* CFG80211 Indication */
			if (eStatus == WLAN_STATUS_ROAM_OUT_FIND_BEST) {
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
				rRoamInfo.bss = bss;
				rRoamInfo.req_ie = prGlueInfo->aucReqIe;
				rRoamInfo.req_ie_len =
				prGlueInfo->u4ReqIeLength;
				rRoamInfo.resp_ie = prGlueInfo->aucRspIe;
				rRoamInfo.resp_ie_len =
				prGlueInfo->u4RspIeLength;

				cfg80211_roamed(prGlueInfo->prDevHandler,
					&rRoamInfo, GFP_KERNEL);
#else
				cfg80211_roamed_bss(
					prGlueInfo->prDevHandler,
					bss,
					prGlueInfo->aucReqIe,
					prGlueInfo->u4ReqIeLength,
					prGlueInfo->aucRspIe,
					prGlueInfo->u4RspIeLength,
					GFP_KERNEL);
#endif
			} else {
				cfg80211_connect_result(
					prGlueInfo->prDevHandler,
					arBssid,
					prGlueInfo->aucReqIe,
					prGlueInfo->u4ReqIeLength,
					prGlueInfo->aucRspIe,
					prGlueInfo->u4RspIeLength,
					WLAN_STATUS_SUCCESS,
					GFP_KERNEL);
				if (bss)
					cfg80211_put_bss(wiphy, bss);
			}
		}

		break;

	case WLAN_STATUS_MEDIA_DISCONNECT:
	case WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY:
		/* indicate disassoc event */
		wext_indicate_wext_event(prGlueInfo, SIOCGIWAP, NULL, 0);
		/* For CR 90 and CR99, While supplicant do reassociate, driver
		 * will do netif_carrier_off first,
		 * after associated success, at joinComplete(),
		 * do netif_carier_on,
		 * but for unknown reason, the supplicant 1x pkt will not
		 * called the driver hardStartXmit, for template workaround
		 * these bugs, add this compiling flag
		 */
		/* switch netif off */

#if 1				/* CONSOLE_MESSAGE */
		DBGLOG(INIT, INFO, "[wifi] %s netif_carrier_off\n",
		       prGlueInfo->prDevHandler->name);
#endif

		netif_carrier_off(prGlueInfo->prDevHandler);
#if CFG_SUPPORT_CFG80211_AUTH
		/* Report T/RX deauth/disassociation frame */
		DBGLOG(INIT, INFO,
			"Skip report DISCONNECTED when using supplicant SME\n");
#else
		/* Full2Partial: reset */
		if (prGlueInfo->prAdapter) {
			struct SCAN_INFO *prScanInfo =
				&(prGlueInfo->prAdapter->rWifiVar.rScanInfo);
			prScanInfo->fgIsScanForFull2Partial = FALSE;
			prScanInfo->u4LastFullScanTime = 0;
		}

		if (prGlueInfo->fgIsRegistered == TRUE) {
			struct BSS_INFO *prBssInfo =
					prGlueInfo->prAdapter->prAisBssInfo;
			uint16_t u2DeauthReason = 0;
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)

			if (prBssInfo)
				u2DeauthReason = prBssInfo->u2DeauthReason;
			/* CFG80211 Indication */
			DBGLOG(INIT, INFO,
			    "[wifi]Indicate disconnection: Reason=%d Locally[%d]\n",
			    u2DeauthReason,
			    (eStatus ==
				WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY));
			cfg80211_disconnected(prGlueInfo->prDevHandler,
			    u2DeauthReason, NULL, 0,
			    eStatus == WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY,
			    GFP_KERNEL);

#else

#ifdef CONFIG_ANDROID
#if KERNEL_VERSION(3, 10, 0) == LINUX_VERSION_CODE
			/* Don't indicate disconnection to upper layer for
			 * ANDROID kernel 3.10
			 */
			/* since cfg80211 will indicate disconnection to
			 * wpa_supplicant for this kernel
			 */
			if (eStatus == WLAN_STATUS_MEDIA_DISCONNECT)
#endif
#endif
			{


				if (prBssInfo)
					u2DeauthReason =
						prBssInfo->u2DeauthReason;
				/* CFG80211 Indication */
				cfg80211_disconnected(prGlueInfo->prDevHandler,
						      u2DeauthReason, NULL, 0,
						      GFP_KERNEL);
			}


#endif
		}
#endif
		kalMemFree(prGlueInfo->rFtIeForTx.pucIEBuf, VIR_MEM_TYPE,
			   prGlueInfo->rFtIeForTx.u4IeLength);
#if CFG_SUPPORT_CFG80211_AUTH
		kalMemFree(prGlueInfo->rFtIeForTx.prMDIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prMDIE));
		kalMemFree(prGlueInfo->rFtIeForTx.prFTIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prFTIE));
		kalMemFree(prGlueInfo->rFtIeForTx.prRsnIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prRsnIE));
		kalMemFree(prGlueInfo->rFtIeForTx.prTIE, VIR_MEM_TYPE,
			   IE_SIZE(prGlueInfo->rFtIeForTx.prTIE));
#endif
		kalMemZero(&prGlueInfo->rFtIeForTx,
			   sizeof(prGlueInfo->rFtIeForTx));

		prGlueInfo->eParamMediaStateIndicated =
			PARAM_MEDIA_STATE_DISCONNECTED;

		break;

	case WLAN_STATUS_SCAN_COMPLETE:
		if (pvBuf && u4BufLen == sizeof(uint8_t))
			fgScanAborted = *(uint8_t *)pvBuf;

		/* indicate scan complete event */
		wext_indicate_wext_event(prGlueInfo, SIOCGIWSCAN, NULL, 0);

		if (fgScanAborted == FALSE) {
			kalScanLogCacheFlushBSS(prGlueInfo->prAdapter,
				SCAN_LOG_MSG_MAX_LEN);
			scanlog_dbg(LOG_SCAN_DONE_D2K, INFO, "Call cfg80211_scan_done (aborted=%u)\n",
				fgScanAborted);
		} else {
			scanlog_dbg(LOG_SCAN_ABORT_DONE_D2K, INFO, "Call cfg80211_scan_done (aborted=%u)\n",
				fgScanAborted);
		}

		/* 1. reset first for newly incoming request */
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
		if (prGlueInfo->prScanRequest != NULL) {
			kalCfg80211ScanDone(prGlueInfo->prScanRequest,
				fgScanAborted);
			prGlueInfo->prScanRequest = NULL;
		}
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

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
				    (pAuth->arRequest[0].u4Flags ==
				     PARAM_AUTH_REQUEST_GROUP_ERROR)) {
					cfg80211_michael_mic_failure(
					    prGlueInfo->prDevHandler, NULL,
					    (pAuth->arRequest[0].u4Flags ==
					    PARAM_AUTH_REQUEST_PAIRWISE_ERROR)
						? NL80211_KEYTYPE_PAIRWISE :
						NL80211_KEYTYPE_GROUP,
					    0, NULL, GFP_KERNEL);
					wext_indicate_wext_event(prGlueInfo,
					    IWEVMICHAELMICFAILURE,
					    (unsigned char *)
						&pAuth->arRequest[0],
					    pAuth->arRequest[0].u4Length);
				}
				break;

			case ENUM_STATUS_TYPE_CANDIDATE_LIST:
			  {
				uint32_t i = 0;

				struct PARAM_PMKID_CANDIDATE
					*prPmkidCand =
					    (struct PARAM_PMKID_CANDIDATE *)
						&pPmkid->arCandidateList[0];

				for (i = 0; i < pPmkid->u4NumCandidates; i++) {
					cfg80211_pmksa_candidate_notify(
						prGlueInfo->prDevHandler,
						1000,
						prPmkidCand[i].arBSSID,
						prPmkidCand[i].u4Flags,
						GFP_KERNEL);

					wext_indicate_wext_event(
						prGlueInfo,
						IWEVPMKIDCAND,
						(unsigned char *)
						&pPmkid->arCandidateList[i],
						pPmkid->u4NumCandidates);
				}
			  }
				break;
			case ENUM_STATUS_TYPE_FT_AUTH_STATUS:
				cfg80211_ft_event(prGlueInfo->prDevHandler,
						  &prGlueInfo->rFtEventParam);
				break;

			default:
				/* case ENUM_STATUS_TYPE_MEDIA_STREAM_MODE */
				break;
			}
		} else {

		}
		break;

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	case WLAN_STATUS_BWCS_UPDATE: {
		wext_indicate_wext_event(prGlueInfo, IWEVCUSTOM, pvBuf,
					 sizeof(struct PTA_IPC));
	}

	break;

#endif
	case WLAN_STATUS_JOIN_FAILURE: {
		struct BSS_DESC *prBssDesc = prGlueInfo->prAdapter->rWifiVar
						.rAisFsmInfo.prTargetBssDesc;
		if (prBssDesc) {
			DBGLOG(INIT, INFO, "JOIN Failure: u2JoinStatus=%d",
				prBssDesc->u2JoinStatus);
			COPY_MAC_ADDR(arBssid, prBssDesc->aucBSSID);
		} else {
			DBGLOG(INIT, INFO, "JOIN Failure: No TargetBssDesc");
			COPY_MAC_ADDR(arBssid,
				prGlueInfo->prAdapter->rWifiVar.rConnSettings
				.aucBSSID);
		}
		if (prBssDesc && prBssDesc->u2JoinStatus
		    && prBssDesc->u2JoinStatus != STATUS_CODE_AUTH_TIMEOUT
		    && prBssDesc->u2JoinStatus != STATUS_CODE_ASSOC_TIMEOUT)
			cfg80211_connect_result(prGlueInfo->prDevHandler,
				arBssid,
				prGlueInfo->aucReqIe,
				prGlueInfo->u4ReqIeLength,
				prGlueInfo->aucRspIe,
				prGlueInfo->u4RspIeLength,
				prBssDesc->u2JoinStatus,
				GFP_KERNEL);
		else
			cfg80211_connect_result(prGlueInfo->prDevHandler,
				arBssid,
				prGlueInfo->aucReqIe,
				prGlueInfo->u4ReqIeLength,
				prGlueInfo->aucRspIe,
				prGlueInfo->u4RspIeLength,
				WLAN_STATUS_AUTH_TIMEOUT,
				GFP_KERNEL);
		prGlueInfo->eParamMediaStateIndicated =
			PARAM_MEDIA_STATE_DISCONNECTED;
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
 * \param[in] pucFrameBody Pointer to the frame body of the last
 *                         (Re)Association Request frame from the AP.
 * \param[in] u4FrameBodyLen The length of the frame body of the last
 *                           (Re)Association Request frame.
 * \param[in] fgReassocRequest TRUE, if it is a Reassociation Request frame.
 *
 * \return (none)
 *
 */
/*----------------------------------------------------------------------------*/
void
kalUpdateReAssocReqInfo(IN struct GLUE_INFO *prGlueInfo,
			IN uint8_t *pucFrameBody, IN uint32_t u4FrameBodyLen,
			IN u_int8_t fgReassocRequest)
{
	uint8_t *cp;

	ASSERT(prGlueInfo);

	/* reset */
	prGlueInfo->u4ReqIeLength = 0;

	if (fgReassocRequest) {
		if (u4FrameBodyLen < 15) {
			return;
		}
	} else {
		if (u4FrameBodyLen < 9) {
			return;
		}
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

	wext_indicate_wext_event(prGlueInfo, IWEVASSOCREQIE, cp,
				 u4FrameBodyLen);

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
void kalUpdateReAssocRspInfo(IN struct GLUE_INFO
			     *prGlueInfo, IN uint8_t *pucFrameBody,
			     IN uint32_t u4FrameBodyLen)
{
	uint32_t u4IEOffset =
		6;	/* cap_info, status_code & assoc_id */
	uint32_t u4IELength = u4FrameBodyLen - u4IEOffset;

	ASSERT(prGlueInfo);

	/* reset */
	prGlueInfo->u4RspIeLength = 0;

	if (u4IELength <= CFG_CFG80211_IE_BUF_LEN) {
		prGlueInfo->u4RspIeLength = u4IELength;
		kalMemCopy(prGlueInfo->aucRspIe, pucFrameBody + u4IEOffset,
			   u4IELength);
	}

}				/* kalUpdateReAssocRspInfo */

void kalResetPacket(IN struct GLUE_INFO *prGlueInfo,
		    IN void *prPacket)
{
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;

	/* Reset cb */
	kalMemZero(prSkb->cb, sizeof(prSkb->cb));
}

/*----------------------------------------------------------------------------*/
/*
 * \brief This function is to check the pairwise eapol and wapi 1x.
 *
 * \param[in] prPacket  Pointer to struct net_device
 *
 * \retval WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
u_int8_t kalIsPairwiseEapolPacket(IN void *prPacket)
{
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	uint8_t *pucPacket = (uint8_t *)prSkb->data;
	uint16_t u2EthType = 0;
	uint16_t u2KeyInfo = 0;

	WLAN_GET_FIELD_BE16(&pucPacket[ETHER_HEADER_LEN - ETHER_TYPE_LEN],
			    &u2EthType);
#if CFG_SUPPORT_WAPI
	/* prBssInfo && prBssInfo->eNetworkType == NETWORK_TYPE_AIS &&
	 * wlanQueryWapiMode(prAdapter)
	 */
	if (u2EthType == ETH_WPI_1X)
		return TRUE;
#endif
	if (u2EthType != ETH_P_1X)
		return FALSE;
	u2KeyInfo = pucPacket[5 + ETHER_HEADER_LEN] << 8 |
		    pucPacket[6 + ETHER_HEADER_LEN];
#if 1
	/* BIT3 is pairwise key bit, and check SM is 0.  it means this is 4-way
	 * handshake frame
	 */
	DBGLOG(RSN, INFO, "u2KeyInfo=%d\n", u2KeyInfo);
	if ((u2KeyInfo & BIT(3)) && !(u2KeyInfo & BIT(13)))
		return TRUE;
#else
	/* BIT3 is pairwise key bit, bit 8 is key mic bit.
	 * only the two bits are set, it means this is 4-way handshake 4/4 or
	 * 2/4 frame
	 */
	DBGLOG(RSN, INFO, "u2KeyInfo=%d\n", u2KeyInfo);
	if ((u2KeyInfo & (BIT(3) | BIT(8))) == (BIT(3) | BIT(8)))
		return TRUE;
#endif
	return FALSE;
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
uint32_t
kalHardStartXmit(struct sk_buff *prOrgSkb,
		 IN struct net_device *prDev, struct GLUE_INFO *prGlueInfo,
		 uint8_t ucBssIndex)
{
	struct QUE_ENTRY *prQueueEntry = NULL;
	struct QUE *prTxQueue = NULL;
	uint16_t u2QueueIdx = 0;
	struct sk_buff *prSkbNew = NULL;
	struct sk_buff *prSkb = NULL;
	uint32_t u4SkbLen = 0;

	ASSERT(prOrgSkb);
	ASSERT(prGlueInfo);

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		DBGLOG(INIT, INFO, "GLUE_FLAG_HALT skip tx\n");
		dev_kfree_skb(prOrgSkb);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (prGlueInfo->prAdapter->fgIsEnableLpdvt) {
		DBGLOG(INIT, INFO, "LPDVT enable, skip this frame\n");
		dev_kfree_skb(prOrgSkb);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	if (skb_headroom(prOrgSkb) < NIC_TX_HEAD_ROOM) {
		/*
		 * Should not happen
		 * kernel crash may happen as skb shared info
		 * channged.
		 * offer an change for lucky anyway
		 */
		prSkbNew = skb_realloc_headroom(prOrgSkb, NIC_TX_HEAD_ROOM);
		ASSERT(prSkbNew);
		dev_kfree_skb(prOrgSkb);
		prSkb = prSkbNew;
	} else
		prSkb = prOrgSkb;

	prQueueEntry = (struct QUE_ENTRY *)
		       GLUE_GET_PKT_QUEUE_ENTRY(prSkb);
	prTxQueue = &prGlueInfo->rTxQueue;

	GLUE_SET_PKT_BSS_IDX(prSkb, ucBssIndex);

	/* Parsing frame info */
	if (!wlanProcessTxFrame(prGlueInfo->prAdapter,
				(void *) prSkb)) {
		/* Cannot extract packet */
		DBGLOG(INIT, INFO,
		       "Cannot extract content, skip this frame\n");
		dev_kfree_skb(prSkb);
		return WLAN_STATUS_INVALID_PACKET;
	}

	/* Tx profiling */
	wlanTxProfilingTagPacket(prGlueInfo->prAdapter,
				 (void *) prSkb, TX_PROF_TAG_OS_TO_DRV);

	/* Handle normal data frame */
	u2QueueIdx = skb_get_queue_mapping(prSkb);
	u4SkbLen = prSkb->len;

	if (u2QueueIdx >= CFG_MAX_TXQ_NUM) {
		DBGLOG(INIT, INFO,
		       "Incorrect queue index, skip this frame\n");
		dev_kfree_skb(prSkb);
		return WLAN_STATUS_INVALID_PACKET;
	}

	if (!HAL_IS_TX_DIRECT(prGlueInfo->prAdapter)) {
		GLUE_SPIN_LOCK_DECLARATION();

		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
		QUEUE_INSERT_TAIL(prTxQueue, prQueueEntry);
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
	}

	GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
	GLUE_INC_REF_CNT(
		prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex]
		[u2QueueIdx]);

	if (GLUE_GET_REF_CNT(prGlueInfo->ai4TxPendingFrameNumPerQueue
	    [ucBssIndex][u2QueueIdx]) >=
	    prGlueInfo->prAdapter->rWifiVar.u4NetifStopTh) {
		netif_stop_subqueue(prDev, u2QueueIdx);

		DBGLOG(TX, INFO,
		       "Stop subqueue for BSS[%u] QIDX[%u] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
		       ucBssIndex, u2QueueIdx, u4SkbLen,
		       GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
		       GLUE_GET_REF_CNT(
			       prGlueInfo->ai4TxPendingFrameNumPerQueue
			       [ucBssIndex][u2QueueIdx]));
	}

	/* Update NetDev statisitcs */
	prDev->stats.tx_bytes += u4SkbLen;
	prDev->stats.tx_packets++;

#if (CFG_SUPPORT_PERF_IND == 1)
	/* update Performance Indicator statistics*/
	prGlueInfo->PerfIndCache.u4CurTxBytes[ucBssIndex] += u4SkbLen;
#endif

	DBGLOG(TX, LOUD,
	       "Enqueue frame for BSS[%u] QIDX[%u] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
	       ucBssIndex, u2QueueIdx, u4SkbLen,
	       GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
	       GLUE_GET_REF_CNT(
		       prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex]
		       [u2QueueIdx]));

	if (HAL_IS_TX_DIRECT(prGlueInfo->prAdapter))
		return nicTxDirectStartXmit(prSkb, prGlueInfo);

	kalSetEvent(prGlueInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of kalHardStartXmit() */

uint32_t kalResetStats(IN struct net_device *prDev)
{
	DBGLOG(QM, LOUD, "Reset NetDev[0x%p] statistics\n", prDev);

	kalMemZero(kalGetStats(prDev),
		   sizeof(struct net_device_stats));

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief A method of struct net_device, to get the network interface
 *        statistical information.
 *
 * Whenever an application needs to get statistics for the interface, this
 * method is called. This happens, for example, when ifconfig or netstat -i is
 * run.
 *
 * \param[in] prDev      Pointer to struct net_device.
 *
 * \return net_device_stats buffer pointer.
 */
/*----------------------------------------------------------------------------*/
void *kalGetStats(IN struct net_device *prDev)
{
	return (void *) &prDev->stats;
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
void kalSendCompleteAndAwakeQueue(IN struct GLUE_INFO
				  *prGlueInfo, IN void *pvPacket)
{
	struct net_device *prDev = NULL;
	struct sk_buff *prSkb = NULL;
	uint16_t u2QueueIdx = 0;
	uint8_t ucBssIndex = 0;
	u_int8_t fgIsValidDevice = TRUE;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(pvPacket);
	/* ASSERT(prGlueInfo->i4TxPendingFrameNum); */

	prSkb = (struct sk_buff *)pvPacket;
	u2QueueIdx = skb_get_queue_mapping(prSkb);
	ASSERT(u2QueueIdx < CFG_MAX_TXQ_NUM);

	ucBssIndex = GLUE_GET_PKT_BSS_IDX(pvPacket);

#if 0
	if ((GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum) <=
	     0)) {
		uint8_t ucBssIdx;
		uint16_t u2QIdx;

		DBGLOG(INIT, INFO, "TxPendingFrameNum[%u] CurFrameId[%u]\n",
		       prGlueInfo->i4TxPendingFrameNum,
		       GLUE_GET_PKT_ARRIVAL_TIME(pvPacket));

		for (ucBssIdx = 0; ucBssIdx < prAdapter->ucHwBssIdNum;
		     ucBssIdx++) {
			for (u2QIdx = 0; u2QIdx < CFG_MAX_TXQ_NUM; u2QIdx++) {
				DBGLOG(INIT, INFO,
					"BSS[%u] Q[%u] TxPendingFrameNum[%u]\n",
					ucBssIdx, u2QIdx,
					prGlueInfo->ai4TxPendingFrameNumPerQueue
					[ucBssIdx][u2QIdx]);
			}
		}
	}

	ASSERT((GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum) >
		0));
#endif

	GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
	GLUE_DEC_REF_CNT(
		prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex]
		[u2QueueIdx]);

	DBGLOG(TX, LOUD,
	       "Release frame for BSS[%u] QIDX[%u] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
	       ucBssIndex, u2QueueIdx, prSkb->len,
	       GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
	       GLUE_GET_REF_CNT(
		       prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex]
		       [u2QueueIdx]));

	prDev = prSkb->dev;

	ASSERT(prDev);

#if CFG_ENABLE_WIFI_DIRECT
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	{
		struct BSS_INFO *prBssInfo = GET_BSS_INFO_BY_INDEX(
					     prGlueInfo->prAdapter, ucBssIndex);
		struct GL_P2P_INFO *prGlueP2pInfo = (struct GL_P2P_INFO *)
						    NULL;
		struct net_device *prNetdevice = NULL;

		/* in case packet was sent after P2P device is unregistered or
		 * the net_device was be free
		 */
		if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			if (prGlueInfo->prAdapter->fgIsP2PRegistered == FALSE)
				fgIsValidDevice = FALSE;
			else {
				ASSERT(prBssInfo->u4PrivateData < KAL_P2P_NUM);
				prGlueP2pInfo =
					prGlueInfo->prP2PInfo[
						prBssInfo->u4PrivateData];
				if (prGlueP2pInfo) {
					prNetdevice =
						prGlueP2pInfo->aprRoleHandler;
					/* The net_device may be free */
					if ((prDev != prNetdevice)
					    && (prDev !=
					    prGlueP2pInfo->prDevHandler)) {
						fgIsValidDevice = FALSE;
						DBGLOG(TX, LOUD,
						       "kalSendCompleteAndAwakeQueue net device deleted! ucBssIndex = %u\n",
						       ucBssIndex);
					}
				}
			}
		}
	}
#endif

	if (fgIsValidDevice == TRUE) {
		uint32_t u4StartTh =
			prGlueInfo->prAdapter->rWifiVar.u4NetifStartTh;

		if (netif_subqueue_stopped(prDev, prSkb) &&
		    prGlueInfo->ai4TxPendingFrameNumPerQueue[ucBssIndex]
		    [u2QueueIdx] <= u4StartTh) {
			netif_wake_subqueue(prDev, u2QueueIdx);
			DBGLOG(TX, TRACE,
				"WakeUp Queue BSS[%u] QIDX[%u] PKT_LEN[%u] TOT_CNT[%d] PER-Q_CNT[%d]\n",
				ucBssIndex, u2QueueIdx, prSkb->len,
				GLUE_GET_REF_CNT(
					prGlueInfo->i4TxPendingFrameNum),
				GLUE_GET_REF_CNT(
					prGlueInfo->
					ai4TxPendingFrameNumPerQueue
					[ucBssIndex][u2QueueIdx]));
		}
	}

#if CFG_ENABLE_WIFI_DIRECT
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
#endif

	dev_kfree_skb_any((struct sk_buff *)pvPacket);

	DBGLOG(TX, LOUD, "----- pending frame %d -----\n",
	       prGlueInfo->i4TxPendingFrameNum);

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
void kalQueryRegistryMacAddr(IN struct GLUE_INFO
			     *prGlueInfo, OUT uint8_t *paucMacAddr)
{
	uint8_t aucZeroMac[MAC_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 }

	DEBUGFUNC("kalQueryRegistryMacAddr");

	ASSERT(prGlueInfo);
	ASSERT(paucMacAddr);

	kalMemCopy((void *) paucMacAddr, (void *) aucZeroMac,
		   MAC_ADDR_LEN);

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
uint32_t kalReadExtCfg(IN struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* External data is given from user space by ioctl or /proc, not read by
	 *  driver.
	 */
	if (prGlueInfo->u4ExtCfgLength != 0)
		DBGLOG(INIT, TRACE,
		       "Read external configuration data -- OK\n");
	else
		DBGLOG(INIT, TRACE,
		       "Read external configuration data -- fail\n");

	return prGlueInfo->u4ExtCfgLength;
}
#endif

u_int8_t
kalIPv4FrameClassifier(IN struct GLUE_INFO *prGlueInfo,
		       IN void *prPacket, IN uint8_t *pucIpHdr,
		       OUT struct TX_PACKET_INFO *prTxPktInfo)
{
	uint8_t ucIpVersion, ucIcmpType;
	uint8_t ucIpProto;
	uint8_t ucSeqNo;
	uint8_t *pucUdpHdr, *pucIcmp;
	uint16_t u2DstPort, u2IcmpId, u2IcmpSeq;
	struct BOOTP_PROTOCOL *prBootp;
	uint32_t u4DhcpMagicCode;

	/* IPv4 version check */
	ucIpVersion = (pucIpHdr[0] & IP_VERSION_MASK) >>
		      IP_VERSION_OFFSET;
	if (ucIpVersion != IP_VERSION_4) {
		DBGLOG(TX, WARN, "Invalid IPv4 packet version: %u\n",
		       ucIpVersion);
		return FALSE;
	}

	ucIpProto = pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET];

	if (ucIpProto == IP_PRO_UDP) {
		pucUdpHdr = &pucIpHdr[IPV4_HDR_LEN];

		/* Get UDP DST port */
		WLAN_GET_FIELD_BE16(&pucUdpHdr[UDP_HDR_DST_PORT_OFFSET],
				    &u2DstPort);

		/* BOOTP/DHCP protocol */
		if ((u2DstPort == IP_PORT_BOOTP_SERVER) ||
		    (u2DstPort == IP_PORT_BOOTP_CLIENT)) {

			prBootp = (struct BOOTP_PROTOCOL *)
							&pucUdpHdr[UDP_HDR_LEN];

			WLAN_GET_FIELD_BE32(&prBootp->aucOptions[0],
					    &u4DhcpMagicCode);

			if (u4DhcpMagicCode == DHCP_MAGIC_NUMBER) {
				uint32_t u4Xid;

				WLAN_GET_FIELD_BE32(&prBootp->u4TransId,
						    &u4Xid);

				ucSeqNo = nicIncreaseTxSeqNum(
							prGlueInfo->prAdapter);
				GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

				DBGLOG_LIMITED(TX, INFO,
					"DHCP PKT[0x%p] XID[0x%08x] OPT[%u] TYPE[%u], SeqNo: %d\n",
					prPacket, u4Xid, prBootp->aucOptions[4],
					prBootp->aucOptions[6], ucSeqNo);
				prTxPktInfo->u2Flag |= BIT(ENUM_PKT_DHCP);
			}
		} else if (u2DstPort == UDP_PORT_DNS) {
			uint16_t u2IpId = *(uint16_t *)&pucIpHdr[IPV4_ADDR_LEN];
			uint8_t *pucUdpPayload = &pucUdpHdr[UDP_HDR_LEN];
			uint16_t u2TransId = (pucUdpPayload[0] << 8) |
					     pucUdpPayload[1];

			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
			DBGLOG_LIMITED(TX, INFO,
				"<TX> DNS: [0x%p] IPID[0x%02x] TransID[0x%04x] SeqNo[%d]\n",
				prPacket, u2IpId, u2TransId, ucSeqNo);
			prTxPktInfo->u2Flag |= BIT(ENUM_PKT_DNS);
		} else if ((u2DstPort == IP_PORT_MDNS)
					|| (u2DstPort == IP_PORT_MDNS)) {
			uint8_t *pucMdnsHdr = NULL;
			struct MDNS_RESP_INFO_T *respInfo;

			if (!prGlueInfo->prAdapter->mdns_offload_enable)
				return TRUE;

			respInfo = (struct MDNS_RESP_INFO_T *)kalMemAlloc(
					sizeof(struct MDNS_RESP_INFO_T),
					PHY_MEM_TYPE);
			if (respInfo == NULL)
				return TRUE;

			kalMemZero(respInfo, sizeof(struct MDNS_RESP_INFO_T));
			pucMdnsHdr = &pucIpHdr[IPV4_HDR_LEN + UDP_HDR_LEN];
			if (kalParseMdnsRespPkt(pucMdnsHdr, respInfo)
						== TRUE) {
				if (respInfo->ptrInfo.ttl != 0)
					kalAddMdnsCache(respInfo);
				else
					kalDelMdnsCache(respInfo);
			}
			kalMemFree(respInfo, PHY_MEM_TYPE,
				sizeof(struct MDNS_RESP_INFO_T));
		}
	} else if (ucIpProto == IP_PRO_ICMP) {
		/* the number of ICMP packets is seldom so we print log here */
		uint16_t u2IpId =
			(pucIpHdr[IPV4_ADDR_LEN] << 8) |
			pucIpHdr[IPV4_ADDR_LEN + 1];
		pucIcmp = &pucIpHdr[20];

		ucIcmpType = pucIcmp[0];
		if (ucIcmpType ==
		    3) /* don't log network unreachable packet */
			return FALSE;
		u2IcmpId = *(uint16_t *) &pucIcmp[4];
		u2IcmpSeq = *(uint16_t *) &pucIcmp[6];

		ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
		GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
		DBGLOG_LIMITED(TX, INFO,
			"<TX> ICMP: IPID[0x%04x] Type %d, Id 0x%04x, Seq BE 0x%04x, SeqNo: %d\n",
			u2IpId, ucIcmpType, u2IcmpId, u2IcmpSeq, ucSeqNo);
		prTxPktInfo->u2Flag |= BIT(ENUM_PKT_ICMP);
	}
	return TRUE;
}


u_int8_t
kalArpFrameClassifier(IN struct GLUE_INFO *prGlueInfo,
		      IN void *prPacket, IN uint8_t *pucIpHdr,
		      OUT struct TX_PACKET_INFO *prTxPktInfo)
{
	uint16_t u2ArpOp;
	uint8_t ucSeqNo;

	ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
	WLAN_GET_FIELD_BE16(&pucIpHdr[ARP_OPERATION_OFFSET],
			    &u2ArpOp);

	DBGLOG(TX, INFO, "ARP %s PKT[0x%p] TAR MAC/IP[" MACSTR "]/["
	       IPV4STR "], SeqNo: %d\n",
	       u2ArpOp == ARP_OPERATION_REQUEST ? "REQ" : "RSP",
	       prPacket, MAC2STR(&pucIpHdr[ARP_TARGET_MAC_OFFSET]),
	       IPV4TOSTR(&pucIpHdr[ARP_TARGET_IP_OFFSET]), ucSeqNo);

	GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

	prTxPktInfo->u2Flag |= BIT(ENUM_PKT_ARP);
	return TRUE;
}

u_int8_t
kalTdlsFrameClassifier(IN struct GLUE_INFO *prGlueInfo,
		       IN void *prPacket, IN uint8_t *pucIpHdr,
		       OUT struct TX_PACKET_INFO *prTxPktInfo)
{
	uint8_t ucSeqNo;
	uint8_t ucActionCode;

	ucActionCode = pucIpHdr[TDLS_ACTION_CODE_OFFSET];

	DBGLOG(TX, INFO, "TDLS action code: %d\n", ucActionCode);

	ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);

	GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

	prTxPktInfo->u2Flag |= BIT(ENUM_PKT_TDLS);

	return TRUE;
}

u_int8_t
kalSecurityFrameClassifier(IN struct GLUE_INFO *prGlueInfo,
			   IN void *prPacket, IN uint8_t *pucIpHdr,
			   IN uint16_t u2EthType, IN uint8_t *aucLookAheadBuf,
			   OUT struct TX_PACKET_INFO *prTxPktInfo)
{
	uint8_t *pucEapol;
	uint8_t ucEapolType;
	uint8_t ucSeqNo;
#if CFG_SUPPORT_WAPI
	uint8_t ucSubType; /* sub type filed*/
	uint16_t u2Length;
	uint16_t u2Seq;
#endif
	uint8_t	ucEAPoLKey = 0;
	uint8_t	ucEapOffset = ETHER_HEADER_LEN;
	uint16_t u2KeyInfo = 0;

	pucEapol = pucIpHdr;

	if (u2EthType == ETH_P_1X) {

		ucEapolType = pucEapol[1];

		/* Leave EAP to check */
		ucEAPoLKey = aucLookAheadBuf[1 + ucEapOffset];
		if (ucEAPoLKey != ETH_EAPOL_KEY)
			prTxPktInfo->u2Flag |= BIT(ENUM_PKT_NON_PROTECTED_1X);
		else {
			WLAN_GET_FIELD_BE16(&aucLookAheadBuf[5 + ucEapOffset],
					    &u2KeyInfo);
			/* BIT3 is pairwise key bit */
			DBGLOG(TX, INFO, "u2KeyInfo=%d\n", u2KeyInfo);
			if (u2KeyInfo & BIT(3))
				prTxPktInfo->u2Flag |=
					BIT(ENUM_PKT_NON_PROTECTED_1X);
		}


		switch (ucEapolType) {
		case 0: /* eap packet */

			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

			DBGLOG(TX, INFO,
			       "<TX> EAP Packet: code %d, id %d, type %d, PKT[0x%p], SeqNo: %d\n",
			       pucEapol[4], pucEapol[5], pucEapol[7], prPacket,
			       ucSeqNo);
			break;
		case 1: /* eapol start */
			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

			DBGLOG(TX, INFO,
			       "<TX> EAPOL: start, PKT[0x%p], SeqNo: %d\n",
			       prPacket, ucSeqNo);
			break;
		case 3: /* key */

			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);

			DBGLOG(TX, INFO,
			       "<TX> EAPOL: key, KeyInfo 0x%04x, PKT[0x%p], SeqNo: %d\n",
			       *((uint16_t *)(&pucEapol[5])), prPacket,
			       ucSeqNo);
			break;
		}
#if CFG_SUPPORT_WAPI
	} else if (u2EthType == ETH_WPI_1X) {

		ucSubType = pucEapol[3]; /* sub type filed*/
		u2Length = *(uint16_t *)&pucEapol[6];
		u2Seq = *(uint16_t *)&pucEapol[8];
		ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
		GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
		prTxPktInfo->u2Flag |= BIT(ENUM_PKT_NON_PROTECTED_1X);

		DBGLOG(TX, INFO,
		       "<TX> WAPI: subType %d, Len %d, Seq %d, PKT[0x%p], SeqNo: %d\n",
		       ucSubType, u2Length, u2Seq, prPacket, ucSeqNo);
#endif
	}
	prTxPktInfo->u2Flag |= BIT(ENUM_PKT_1X);
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This inline function is to extract some packet information, including
 *        user priority, packet length, destination address, 802.1x and BT over
 *        Wi-Fi or not.
 *
 * @param prGlueInfo         Pointer to the glue structure
 * @param prPacket           Packet descriptor
 * @param prTxPktInfo        Extracted packet info
 *
 * @retval TRUE      Success to extract information
 * @retval FALSE     Fail to extract correct information
 */
/*----------------------------------------------------------------------------*/
u_int8_t
kalQoSFrameClassifierAndPacketInfo(IN struct GLUE_INFO *prGlueInfo,
		IN void *prPacket, OUT struct TX_PACKET_INFO *prTxPktInfo)
{
	uint32_t u4PacketLen;
	uint16_t u2EtherTypeLen;
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	uint8_t *aucLookAheadBuf = NULL;
	uint8_t ucEthTypeLenOffset = ETHER_HEADER_LEN -
				     ETHER_TYPE_LEN;
	uint8_t *pucNextProtocol = NULL;
#if DSCP_SUPPORT
	uint8_t ucUserPriority;
#endif

	u4PacketLen = prSkb->len;

	if (u4PacketLen < ETHER_HEADER_LEN) {
		DBGLOG(INIT, WARN, "Invalid Ether packet length: %u\n",
		       u4PacketLen);
		return FALSE;
	}

	aucLookAheadBuf = prSkb->data;

	/* Reset Packet Info */
	kalMemZero(prTxPktInfo, sizeof(struct TX_PACKET_INFO));

	/* 4 <0> Obtain Ether Type/Len */
	WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset],
			    &u2EtherTypeLen);

	/* 4 <1> Skip 802.1Q header (VLAN Tagging) */
	if (u2EtherTypeLen == ETH_P_VLAN) {
		prTxPktInfo->u2Flag |= BIT(ENUM_PKT_VLAN_EXIST);
		ucEthTypeLenOffset += ETH_802_1Q_HEADER_LEN;
		WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset],
				    &u2EtherTypeLen);
	}
	/* 4 <2> Obtain next protocol pointer */
	pucNextProtocol = &aucLookAheadBuf[ucEthTypeLenOffset +
							      ETHER_TYPE_LEN];

	/* 4 <3> Handle ethernet format */
	switch (u2EtherTypeLen) {
	case ETH_P_IPV4:
		/* IPv4 header length check */
		if (u4PacketLen < (ucEthTypeLenOffset + ETHER_TYPE_LEN +
				   IPV4_HDR_LEN)) {
			DBGLOG(INIT, WARN, "Invalid IPv4 packet length: %u\n",
			       u4PacketLen);
			break;
		}
#if DSCP_SUPPORT
		if (GLUE_GET_PKT_BSS_IDX(prSkb) != P2P_DEV_BSS_INDEX) {
			ucUserPriority = getUpFromDscp(prGlueInfo,
				GLUE_GET_PKT_BSS_IDX(prSkb),
				(pucNextProtocol[1] & 0xFC) >> 2);
			if (ucUserPriority != 0xFF)
				prSkb->priority = ucUserPriority;
		}
#endif
		kalIPv4FrameClassifier(prGlueInfo, prPacket,
				       pucNextProtocol, prTxPktInfo);
		break;

	case ETH_P_ARP:
		kalArpFrameClassifier(prGlueInfo, prPacket, pucNextProtocol,
				      prTxPktInfo);
		break;

	case ETH_P_1X:
	case ETH_P_PRE_1X:
#if CFG_SUPPORT_WAPI
	case ETH_WPI_1X:
#endif
		kalSecurityFrameClassifier(prGlueInfo, prPacket,
			pucNextProtocol, u2EtherTypeLen, aucLookAheadBuf,
			prTxPktInfo);
		break;

	case ETH_PRO_TDLS:
		kalTdlsFrameClassifier(prGlueInfo, prPacket,
				       pucNextProtocol, prTxPktInfo);
		break;
	default:
		/* 4 <4> Handle 802.3 format if LEN <= 1500 */
		if (u2EtherTypeLen <= ETH_802_3_MAX_LEN)
			prTxPktInfo->u2Flag |= BIT(ENUM_PKT_802_3);
		break;
	}

	/* 4 <4.1> Check for PAL (BT over Wi-Fi) */
	/* Move to kalBowFrameClassifier */

	/* 4 <5> Return the value of Priority Parameter. */
	/* prSkb->priority is assigned by Linux wireless utility
	 * function(cfg80211_classify8021d)
	 */
	/* at net_dev selection callback (ndo_select_queue) */
	prTxPktInfo->ucPriorityParam = prSkb->priority;

	/* 4 <6> Retrieve Packet Information - DA */
	/* Packet Length/ Destination Address */
	prTxPktInfo->u4PacketLen = u4PacketLen;

	kalMemCopy(prTxPktInfo->aucEthDestAddr, aucLookAheadBuf,
		   PARAM_MAC_ADDR_LEN);

	return TRUE;
}				/* end of kalQoSFrameClassifier() */

u_int8_t kalGetEthDestAddr(IN struct GLUE_INFO *prGlueInfo,
			   IN void *prPacket, OUT uint8_t *pucEthDestAddr)
{
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	uint8_t *aucLookAheadBuf = NULL;

	/* Sanity Check */
	if (!prPacket || !prGlueInfo)
		return FALSE;

	aucLookAheadBuf = prSkb->data;

	kalMemCopy(pucEthDestAddr, aucLookAheadBuf,
		   PARAM_MAC_ADDR_LEN);

	return TRUE;
}

void
kalOidComplete(IN struct GLUE_INFO *prGlueInfo,
	       IN u_int8_t fgSetQuery, IN uint32_t u4SetQueryInfoLen,
	       IN uint32_t rOidStatus)
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
	}

	if (rOidStatus == WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, TRACE, "Complete OID, status:success\n");
	else
		DBGLOG(INIT, WARN, "Complete OID, status:0x%08x\n",
		       rOidStatus);

	/* else let it timeout on kalIoctl entry */
}

void kalOidClearance(IN struct GLUE_INFO *prGlueInfo)
{

}

void kalGetLocalTime(unsigned long long *sec, unsigned long *nsec)
{
	if (sec != NULL && nsec != NULL) {
		*sec = local_clock();
		*nsec = do_div(*sec, 1000000000)/1000;
	} else
		DBGLOG(INIT, ERROR,
			"The input parameters error when get local time\n");
}

/*
 * kalThreadSchedRetrieve
 * Retrieve thread's current scheduling statistics and
 * stored in output "sched".
 * Return value:
 *	 0 : Schedstats successfully retrieved
 *	-1 : Kernel's schedstats feature not enabled
 *	-2 : pThread not yet initialized or sched is a NULL pointer
 */
static int32_t kalThreadSchedRetrieve(struct task_struct *pThread,
					struct KAL_THREAD_SCHEDSTATS *pSched)
{
#ifdef CONFIG_SCHEDSTATS
	struct sched_entity se;
	unsigned long long sec;
	unsigned long usec;

	if (!pSched)
		return -2;

	/* always clear sched to simplify error handling at caller side */
	memset(pSched, 0, sizeof(struct KAL_THREAD_SCHEDSTATS));

	if (!pThread)
		return -2;

	memcpy(&se, &pThread->se, sizeof(struct sched_entity));
	kalGetLocalTime(&sec, &usec);

	pSched->time = sec*1000 + usec/1000;
	pSched->exec = se.sum_exec_runtime;
	pSched->runnable = se.statistics.wait_sum;
	pSched->iowait = se.statistics.iowait_sum;

	return 0;
#else
	/* always clear sched to simplify error handling at caller side */
	if (pSched)
		memset(pSched, 0, sizeof(struct KAL_THREAD_SCHEDSTATS));
	return -1;
#endif
}

/*
 * kalThreadSchedMark
 * Record the thread's current schedstats and stored in
 * output "schedstats" parameter for profiling at later time.
 * Return value:
 *	 0 : Schedstats successfully recorded
 *	-1 : Kernel's schedstats feature not enabled
 *	-2 : pThread not yet initialized or invalid parameters
 */
int32_t kalThreadSchedMark(struct task_struct *pThread,
				struct KAL_THREAD_SCHEDSTATS *pSchedstats)
{
	return kalThreadSchedRetrieve(pThread, pSchedstats);
}

/*
 * kalThreadSchedUnmark
 * Calculate scheduling statistics against the previously marked point.
 * The result will be filled back into the schedstats output parameter.
 * Return value:
 *	 0 : Schedstats successfully calculated
 *	-1 : Kernel's schedstats feature not enabled
 *	-2 : pThread not yet initialized or invalid parameters
 */
int32_t kalThreadSchedUnmark(struct task_struct *pThread,
				struct KAL_THREAD_SCHEDSTATS *pSchedstats)
{
	int32_t ret;
	struct KAL_THREAD_SCHEDSTATS sched_now;

	if (unlikely(!pSchedstats)) {
		ret = -2;
	} else {
		ret = kalThreadSchedRetrieve(pThread, &sched_now);
		if (ret == 0) {
			pSchedstats->time =
				sched_now.time - pSchedstats->time;
			pSchedstats->exec =
				sched_now.exec - pSchedstats->exec;
			pSchedstats->runnable =
				sched_now.runnable - pSchedstats->runnable;
			pSchedstats->iowait =
				sched_now.iowait - pSchedstats->iowait;
		}
	}
	return ret;
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

uint32_t
kalIoctl(IN struct GLUE_INFO *prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN void *pvInfoBuf,
	 IN uint32_t u4InfoBufLen, IN u_int8_t fgRead,
	 IN u_int8_t fgWaitResp, IN u_int8_t fgCmd,
	 OUT uint32_t *pu4QryInfoLen)
{
	struct GL_IO_REQ *prIoReq = NULL;
	struct KAL_THREAD_SCHEDSTATS schedstats;
	uint32_t ret = WLAN_STATUS_SUCCESS;
	uint32_t waitRet = 0;

#if CFG_CHIP_RESET_SUPPORT
	if (kalIsResetting())
		return WLAN_STATUS_SUCCESS;
#endif

	if (wlanIsChipAssert(prGlueInfo->prAdapter))
		return WLAN_STATUS_SUCCESS;

	/* GLUE_SPIN_LOCK_DECLARATION(); */
	ASSERT(prGlueInfo);

	/* Just direct function call if already
	*  in main_thread or interrupt context
	*/
	if (prGlueInfo->u4TxThreadPid == KAL_GET_CURRENT_THREAD_ID()
		|| in_interrupt()) {
		if (pfnOidHandler) {
			g_fgIsOid = FALSE;
			ret = pfnOidHandler(prGlueInfo->prAdapter, pvInfoBuf,
				u4InfoBufLen, pu4QryInfoLen);
		}
		return ret;
	}

	/* <1> Check if driver is halt */
	/* if (prGlueInfo->u4Flag & GLUE_FLAG_HALT) { */
	/* return WLAN_STATUS_ADAPTER_NOT_READY; */
	/* } */

	if (down_interruptible(&g_halt_sem))
		return WLAN_STATUS_FAILURE;

	if (g_u4HaltFlag) {
		up(&g_halt_sem);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (down_interruptible(&prGlueInfo->ioctl_sem)) {
		up(&g_halt_sem);
		return WLAN_STATUS_FAILURE;
	}

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
	KAL_WAKE_LOCK_TIMEOUT(prGlueInfo->prAdapter,
		&prGlueInfo->rTimeoutWakeLock, MSEC_TO_JIFFIES(
		prGlueInfo->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	/* <8> Wake up tx thread to handle kick start the I/O request */
	wake_up_interruptible(&prGlueInfo->waitq);

	/* <9> Block and wait for event or timeout,
	 * current the timeout is 30 secs
	 */
	kalThreadSchedMark(prGlueInfo->main_thread, &schedstats);
	waitRet = wait_for_completion_timeout(&prGlueInfo->rPendComp,
				MSEC_TO_JIFFIES(30*1000));
	kalThreadSchedUnmark(prGlueInfo->main_thread, &schedstats);

	if (waitRet > 0) {
		/* Case 1: No timeout. */
		/* if return WLAN_STATUS_PENDING, the status of cmd is stored
		 * in prGlueInfo
		 */
		if (prIoReq->rStatus == WLAN_STATUS_PENDING)
			ret = prGlueInfo->rPendStatus;
		else
			ret = prIoReq->rStatus;
		if (ret != WLAN_STATUS_SUCCESS)
			DBGLOG(OID, WARN, "kalIoctl: ret ErrCode: 0x%X\n", ret);
	} else {

#if 0
		/* Case 2: timeout */
		/* clear pending OID's cmd in CMD queue */
		if (fgCmd) {
			prGlueInfo->u4TimeoutFlag = 1;
			wlanReleasePendingOid(prGlueInfo->prAdapter, 0);
		}
#endif
		DBGLOG(OID, WARN,
			"duration:%llums, sched(x%llu/r%llu/i%llu)\n",
			schedstats.time, schedstats.exec,
			schedstats.runnable, schedstats.iowait);
		DBGLOG(OID, ERROR,
				"wait main_thread timeout, show backtrace:\n");

		kal_show_stack(prGlueInfo->prAdapter,
			prGlueInfo->main_thread, NULL);
		ret = WLAN_STATUS_FAILURE;
	}

	/* <10> Clear bit for error handling */
	clear_bit(GLUE_FLAG_OID_BIT, &prGlueInfo->ulFlag);

	up(&prGlueInfo->ioctl_sem);
	up(&g_halt_sem);

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
void kalClearSecurityFrames(IN struct GLUE_INFO *prGlueInfo)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE rReturnCmdQue;
	struct QUE *prReturnCmdQue = &rReturnCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;

	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending security frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
			if (prCmdInfo->pfCmdTimeoutHandler)
				prCmdInfo->pfCmdTimeoutHandler(
					prGlueInfo->prAdapter, prCmdInfo);
			else
				wlanReleaseCommand(prGlueInfo->prAdapter,
					prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
			GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
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
void kalClearSecurityFramesByBssIdx(IN struct GLUE_INFO
				    *prGlueInfo, IN uint8_t ucBssIndex)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE rReturnCmdQue;
	struct QUE *prReturnCmdQue = &rReturnCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;
	struct MSDU_INFO *prMsduInfo;
	u_int8_t fgFree;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending security frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;
		prMsduInfo = prCmdInfo->prMsduInfo;
		fgFree = FALSE;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME
		    && prMsduInfo) {
			if (prMsduInfo->ucBssIndex == ucBssIndex)
				fgFree = TRUE;
		}

		if (fgFree) {
			if (prCmdInfo->pfCmdTimeoutHandler)
				prCmdInfo->pfCmdTimeoutHandler(
					prGlueInfo->prAdapter, prCmdInfo);
			else
				wlanReleaseCommand(prGlueInfo->prAdapter,
					prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
			GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
		} else
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
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
void kalClearMgmtFrames(IN struct GLUE_INFO *prGlueInfo)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE rReturnCmdQue;
	struct QUE *prReturnCmdQue = &rReturnCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending management frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
			wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo,
					   TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
			GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
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
void kalClearMgmtFramesByBssIdx(IN struct GLUE_INFO
				*prGlueInfo, IN uint8_t ucBssIndex)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE rReturnCmdQue;
	struct QUE *prReturnCmdQue = &rReturnCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;
	struct MSDU_INFO *prMsduInfo;
	u_int8_t fgFree;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending management frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;
		prMsduInfo = prCmdInfo->prMsduInfo;
		fgFree = FALSE;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME
		    && prMsduInfo) {
			if (prMsduInfo->ucBssIndex == ucBssIndex)
				fgFree = TRUE;
		}

		if (fgFree) {
			wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo,
					   TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
			GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
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
void kalClearCommandQueue(IN struct GLUE_INFO *prGlueInfo)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE rReturnCmdQue;
	struct QUE *prReturnCmdQue = &rReturnCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);

	/* Clear ALL in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter,
						       prCmdInfo);
		else
			wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo,
					   TX_RESULT_QUEUE_CLEARANCE);

		cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingCmdNum);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}
}

uint32_t kalProcessTxPacket(struct GLUE_INFO *prGlueInfo,
			    struct sk_buff *prSkb)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	if (prSkb == NULL) {
		DBGLOG(INIT, WARN, "prSkb == NULL in tx\n");
		return u4Status;
	}

	/* Handle security frame */
	if (0 /* GLUE_TEST_PKT_FLAG(prSkb, ENUM_PKT_1X) */
	      /* No more sending via cmd */) {
		if (wlanProcessSecurityFrame(prGlueInfo->prAdapter,
					     (void *) prSkb)) {
			u4Status = WLAN_STATUS_SUCCESS;
			GLUE_INC_REF_CNT(
				prGlueInfo->i4TxPendingSecurityFrameNum);
		} else {
			u4Status = WLAN_STATUS_RESOURCES;
		}
	}
	/* Handle normal frame */
	else
		u4Status = wlanEnqueueTxPacket(prGlueInfo->prAdapter,
					       (void *) prSkb);

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to process Tx request to main_thread
 *
 * \param prGlueInfo     Pointer of GLUE Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void kalProcessTxReq(struct GLUE_INFO *prGlueInfo,
		     u_int8_t *pfgNeedHwAccess)
{
	struct QUE *prCmdQue = NULL;
	struct QUE *prTxQueue = NULL;
	struct QUE rTempQue;
	struct QUE *prTempQue = &rTempQue;
	struct QUE rTempReturnQue;
	struct QUE *prTempReturnQue = &rTempReturnQue;
	struct QUE_ENTRY *prQueueEntry = NULL;
	/* struct sk_buff      *prSkb = NULL; */
	uint32_t u4Status;
#if CFG_SUPPORT_MULTITHREAD
	uint32_t u4CmdCount = 0;
#endif
	uint32_t u4TxLoopCount;

	/* for spin lock acquire and release */
	GLUE_SPIN_LOCK_DECLARATION();

	prTxQueue = &prGlueInfo->rTxQueue;
	prCmdQue = &prGlueInfo->rCmdQueue;

	QUEUE_INITIALIZE(prTempQue);
	QUEUE_INITIALIZE(prTempReturnQue);

	u4TxLoopCount =
		prGlueInfo->prAdapter->rWifiVar.u4TxFromOsLoopCount;

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
				QUEUE_REMOVE_HEAD(prTempQue, prQueueEntry,
						  struct QUE_ENTRY *);

				if (prQueueEntry == NULL)
					break;

				u4Status = kalProcessTxPacket(prGlueInfo,
					      (struct sk_buff *)
					      GLUE_GET_PKT_DESCRIPTOR(
					      prQueueEntry));
#if 0
				prSkb =	(struct sk_buff *)
					GLUE_GET_PKT_DESCRIPTOR(prQueueEntry);
				ASSERT(prSkb);
				if (prSkb == NULL) {
					DBGLOG(INIT, WARN,
					       "prSkb == NULL in tx\n");
					continue;
				}

				/* Handle security frame */
				if (GLUE_GET_PKT_IS_1X(prSkb)) {
					if (wlanProcessSecurityFrame(
					    prGlueInfo->prAdapter,
					    (void *)prSkb)) {
						u4Status = WLAN_STATUS_SUCCESS;
						GLUE_INC_REF_CNT(prGlueInfo->
						i4TxPendingSecurityFrameNum);
					} else {
						u4Status =
							WLAN_STATUS_RESOURCES;
					}
				}
				/* Handle normal frame */
				else
					u4Status = wlanEnqueueTxPacket(
							prGlueInfo->prAdapter,
							(void *) prSkb);
#endif
				/* Enqueue packet back into TxQueue if resource
				 * is not enough
				 */
				if (u4Status == WLAN_STATUS_RESOURCES) {
					QUEUE_INSERT_TAIL(prTempReturnQue,
							  prQueueEntry);
					break;
				}
			}

			if (wlanGetTxPendingFrameCount(
			    prGlueInfo->prAdapter) > 0)
				wlanTxPendingPackets(prGlueInfo->prAdapter,
						     pfgNeedHwAccess);

			/* Enqueue packet back into TxQueue if resource is not
			 * enough
			 */
			if (QUEUE_IS_NOT_EMPTY(prTempReturnQue)) {
				QUEUE_CONCATENATE_QUEUES(prTempReturnQue,
							 prTempQue);

				GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo,
						       SPIN_LOCK_TX_QUE);
				QUEUE_CONCATENATE_QUEUES_HEAD(prTxQueue,
							      prTempReturnQue);
				GLUE_RELEASE_SPIN_LOCK(prGlueInfo,
						       SPIN_LOCK_TX_QUE);

				break;
			}
		}

		if (wlanGetTxPendingFrameCount(prGlueInfo->prAdapter) > 0)
			wlanTxPendingPackets(prGlueInfo->prAdapter,
					     pfgNeedHwAccess);
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
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
					 netdev_priv(dev));
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	int ret = 0;

	KAL_WAKE_LOCK_INIT(prAdapter, &rHifThreadWakeLock, "WLAN hif_thread");
	KAL_WAKE_LOCK(prAdapter, &rHifThreadWakeLock);

	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	prGlueInfo->u4HifThreadPid = KAL_GET_CURRENT_THREAD_ID();

	set_user_nice(current, prAdapter->rWifiVar.cThreadNice);

	while (TRUE) {

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
#if CFG_CHIP_RESET_SUPPORT
		    || kalIsResetting()
#endif
		    ) {
			DBGLOG(INIT, INFO, "hif_thread should stop now...\n");
			break;
		}

		/* Unlock wakelock if hif_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_HIF_PROCESS))
			KAL_WAKE_UNLOCK(prAdapter, &rHifThreadWakeLock);

		/*
		 * sleep on waitqueue if no events occurred. Event contain
		 * (1) GLUE_FLAG_INT (2) GLUE_FLAG_OID (3) GLUE_FLAG_TXREQ
		 * (4) GLUE_FLAG_HALT
		 *
		 */
		do {
			ret = wait_event_interruptible(prGlueInfo->waitq_hif,
				((prGlueInfo->ulFlag & GLUE_FLAG_HIF_PROCESS)
				!= 0));
		} while (ret != 0);
#if CFG_ENABLE_WAKE_LOCK
		if (!KAL_WAKE_LOCK_ACTIVE(prAdapter, &rHifThreadWakeLock))
			KAL_WAKE_LOCK(prAdapter, &rHifThreadWakeLock);
#endif
		if (prAdapter->fgIsFwOwn
		    && (prGlueInfo->ulFlag == GLUE_FLAG_HIF_FW_OWN)) {
			DBGLOG(INIT, INFO,
			       "Only FW OWN request, but now already done FW OWN\n");
			clear_bit(GLUE_FLAG_HIF_FW_OWN_BIT,
				  &prGlueInfo->ulFlag);
			continue;
		}
		wlanAcquirePowerControl(prAdapter);

		/* Handle Interrupt */
		if (test_and_clear_bit(GLUE_FLAG_INT_BIT,
				       &prGlueInfo->ulFlag)) {
			/* the Wi-Fi interrupt is already disabled in mmc
			 * thread, so we set the flag only to enable the
			 * interrupt later
			 */
			prAdapter->fgIsIntEnable = FALSE;
			if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
#if CFG_CHIP_RESET_SUPPORT
			    || kalIsResetting()
#endif
			   ) {
				/* Should stop now... skip pending interrupt */
				DBGLOG(INIT, INFO,
				       "ignore pending interrupt\n");
			} else {
				/* DBGLOG(INIT, INFO, ("HIF Interrupt!\n")); */
				prGlueInfo->TaskIsrCnt++;
				wlanIST(prAdapter);
			}
		}

#if CFG_SUPPORT_SER

		/* Skip Tx request if SER is operating */
		if ((prAdapter->fgIsFwOwn == FALSE) &&
		     !nicSerIsTxStop(prAdapter)) {

#else  /* !CFG_SUPPORT_SER */

		if (prAdapter->fgIsFwOwn == FALSE) {

#endif  /* CFG_SUPPORT_SER */
			/* TX Commands */
			if (test_and_clear_bit(GLUE_FLAG_HIF_TX_CMD_BIT,
					       &prGlueInfo->ulFlag))
				wlanTxCmdMthread(prAdapter);

			/* Process TX data packet to HIF */
			if (test_and_clear_bit(GLUE_FLAG_HIF_TX_BIT,
					       &prGlueInfo->ulFlag)) {
				if (HAL_HAS_AGG_THREAD(prGlueInfo->prAdapter)) {
					HAL_AGG_KICK_DATA
					(prGlueInfo->prAdapter);
				} else {
					nicTxMsduQueueMthread
					(prAdapter);
				}
			}
		}

		/* Read chip status when chip no response */
		if (test_and_clear_bit(GLUE_FLAG_HIF_PRT_HIF_DBG_INFO_BIT,
				       &prGlueInfo->ulFlag))
			halPrintHifDbgInfo(prAdapter);

		/* Set FW own */
		if (test_and_clear_bit(GLUE_FLAG_HIF_FW_OWN_BIT,
				       &prGlueInfo->ulFlag))
			prAdapter->fgWiFiInSleepyState = TRUE;

		/* Release to FW own */
		wlanReleasePowerControl(prAdapter);
	}

	complete(&prGlueInfo->rHifHaltComp);
#if CFG_ENABLE_WAKE_LOCK
	if (KAL_WAKE_LOCK_ACTIVE(prAdapter, &rHifThreadWakeLock))
		KAL_WAKE_UNLOCK(prAdapter, &rHifThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prAdapter, &rHifThreadWakeLock);
#endif

	DBGLOG(INIT, TRACE, "%s:%u stopped!\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

#if CFG_CHIP_RESET_HANG
	while (fgIsResetHangState == SER_L0_HANG_RST_HANG) {
		kalMsleep(SER_L0_HANG_LOG_TIME_INTERVAL);
		DBGLOG(INIT, STATE, "[SER][L0] SQC hang!\n");
	}
#endif

	return 0;
}

int rx_thread(void *data)
{
	struct net_device *dev = data;
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
					 netdev_priv(dev));

	struct QUE rTempRxQue;
	struct QUE *prTempRxQue = NULL;
	struct QUE_ENTRY *prQueueEntry = NULL;
#if CFG_SUPPORT_MTK_CPU_SCHED
	struct cpumask *mask = &current->cpus_allowed;
#endif

	int ret = 0;

	uint32_t u4LoopCount;

	/* for spin lock acquire and release */
	KAL_SPIN_LOCK_DECLARATION();

	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter,
			   &rRxThreadWakeLock, "WLAN rx_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rRxThreadWakeLock);

	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	prGlueInfo->u4RxThreadPid = KAL_GET_CURRENT_THREAD_ID();

	set_user_nice(current,
		      prGlueInfo->prAdapter->rWifiVar.cThreadNice);
#if CFG_SUPPORT_MTK_CPU_SCHED
	cpumask_clear_cpu(0, mask);
	cpumask_set_cpu(1, mask);
	cpumask_set_cpu(2, mask);
	cpumask_set_cpu(3, mask);
	mt_sched_setaffinity(0, mask);
#endif
	prTempRxQue = &rTempRxQue;

	while (TRUE) {

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
#if CFG_CHIP_RESET_SUPPORT
		    || kalIsResetting()
#endif
		   ) {
			DBGLOG(INIT, INFO, "rx_thread should stop now...\n");
			break;
		}

		/* Unlock wakelock if rx_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_RX_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter,
					&rRxThreadWakeLock);

		/*
		 * sleep on waitqueue if no events occurred.
		 */
		do {
			ret = wait_event_interruptible(prGlueInfo->waitq_rx,
			    ((prGlueInfo->ulFlag & GLUE_FLAG_RX_PROCESS) != 0));
		} while (ret != 0);
#if CFG_ENABLE_WAKE_LOCK
		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
					  &rRxThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter,
				      &rRxThreadWakeLock);
#endif
		if (test_and_clear_bit(GLUE_FLAG_RX_TO_OS_BIT,
				       &prGlueInfo->ulFlag)) {
			u4LoopCount =
			    prGlueInfo->prAdapter->rWifiVar.u4Rx2OsLoopCount;

			while (u4LoopCount--) {
				while (QUEUE_IS_NOT_EMPTY(
				       &prGlueInfo->prAdapter->rRxQueue)) {
					QUEUE_INITIALIZE(prTempRxQue);

					GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo,
					    SPIN_LOCK_RX_TO_OS_QUE);
					QUEUE_MOVE_ALL(prTempRxQue,
					    &prGlueInfo->prAdapter->rRxQueue);
					GLUE_RELEASE_SPIN_LOCK(prGlueInfo,
					    SPIN_LOCK_RX_TO_OS_QUE);

					while (QUEUE_IS_NOT_EMPTY(
						prTempRxQue)) {
						QUEUE_REMOVE_HEAD(prTempRxQue,
						    prQueueEntry,
						    struct QUE_ENTRY *);
						kalRxIndicateOnePkt(prGlueInfo,
						    (void *)
						    GLUE_GET_PKT_DESCRIPTOR(
						    prQueueEntry));
					}

				    KAL_WAKE_LOCK_TIMEOUT(prGlueInfo->prAdapter,
					&prGlueInfo->rTimeoutWakeLock,
					MSEC_TO_JIFFIES(prGlueInfo->prAdapter
					->rWifiVar.u4WakeLockRxTimeout));
				}
			}
		}
	}

	complete(&prGlueInfo->rRxHaltComp);
#if CFG_ENABLE_WAKE_LOCK
	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
				 &rRxThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rRxThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter,
			      &rRxThreadWakeLock);
#endif

	DBGLOG(INIT, TRACE, "%s:%u stopped!\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

#if CFG_CHIP_RESET_HANG
	while (fgIsResetHangState == SER_L0_HANG_RST_HANG) {
		kalMsleep(SER_L0_HANG_LOG_TIME_INTERVAL);
		DBGLOG(INIT, STATE, "[SER][L0] SQC hang!\n");
	}
#endif

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is a kernel thread function for handling command packets
 * Tx requests and interrupt events
 *
 * @param data       data pointer to private data of main_thread
 *
 * @retval           If the function succeeds, the return value is 0.
 * Otherwise, an error code is returned.
 *
 */
/*----------------------------------------------------------------------------*/

int main_thread(void *data)
{
	struct net_device *dev = data;
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
					 netdev_priv(dev));
	struct GL_IO_REQ *prIoReq = NULL;
	int ret = 0;
	u_int8_t fgNeedHwAccess = FALSE;

#if CFG_SUPPORT_MULTITHREAD
	prGlueInfo->u4TxThreadPid = KAL_GET_CURRENT_THREAD_ID();
#endif

	current->flags |= PF_NOFREEZE;
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);
	set_user_nice(current,
		      prGlueInfo->prAdapter->rWifiVar.cThreadNice);

	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter,
			   &rTxThreadWakeLock, "WLAN main_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, &rTxThreadWakeLock);

	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	while (TRUE) {

#if CFG_ENABLE_WIFI_DIRECT
		/*run p2p multicast list work. */
		if (test_and_clear_bit(GLUE_FLAG_SUB_MOD_MULTICAST_BIT,
				       &prGlueInfo->ulFlag))
			p2pSetMulticastListWorkQueueWrapper(prGlueInfo);
#endif

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT || kalIsResetting()) {
			DBGLOG(INIT, INFO, "%s should stop now...\n",
			       KAL_GET_CURRENT_THREAD_NAME());
			break;
		}

		/* Unlock wakelock if main_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_MAIN_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter,
					&rTxThreadWakeLock);

		/*
		 * sleep on waitqueue if no events occurred. Event contain
		 * (1) GLUE_FLAG_INT (2) GLUE_FLAG_OID (3) GLUE_FLAG_TXREQ
		 * (4) GLUE_FLAG_HALT
		 */
		do {
			ret = wait_event_interruptible(prGlueInfo->waitq,
				((prGlueInfo->ulFlag & GLUE_FLAG_MAIN_PROCESS)
				!= 0));
		} while (ret != 0);
#if CFG_ENABLE_WAKE_LOCK
		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
					  &rTxThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter,
				      &rTxThreadWakeLock);
#endif

#if CFG_ENABLE_WIFI_DIRECT
		/*run p2p multicast list work. */
		if (test_and_clear_bit(GLUE_FLAG_SUB_MOD_MULTICAST_BIT,
				       &prGlueInfo->ulFlag))
			p2pSetMulticastListWorkQueueWrapper(prGlueInfo);

		if (test_and_clear_bit(GLUE_FLAG_FRAME_FILTER_BIT,
				       &prGlueInfo->ulFlag)) {
			p2pFuncUpdateMgmtFrameRegister(prGlueInfo->prAdapter,
			       prGlueInfo->prP2PDevInfo->u4OsMgmtFrameFilter);
		}
#endif
		if (test_and_clear_bit(GLUE_FLAG_FRAME_FILTER_AIS_BIT,
				       &prGlueInfo->ulFlag)) {
			aisFuncUpdateMgmtFrameRegister(prGlueInfo->prAdapter,
					prGlueInfo->u4OsMgmtFrameFilter);
		}

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT || kalIsResetting()) {
			DBGLOG(INIT, INFO, "%s should stop now...\n",
			       KAL_GET_CURRENT_THREAD_NAME());
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
				kalDevRegWrite(prGlueInfo, MCR_WTMCR,
					       0x00080002);
				prGlueInfo->fgIsSdioTestInitialized = TRUE;
			}

			if (prGlueInfo->fgSdioReadWriteMode == TRUE) {
				/* read test */
				kalDevPortRead(prGlueInfo, MCR_WTMDR, 256,
				       prGlueInfo->aucSdioTestBuffer,
				       sizeof(prGlueInfo->aucSdioTestBuffer));
			} else {
				/* write test */
				kalDevPortWrite(prGlueInfo, MCR_WTMDR, 172,
					prGlueInfo->aucSdioTestBuffer,
					sizeof(prGlueInfo->aucSdioTestBuffer));
			}
		}
#endif
#if CFG_SUPPORT_MULTITHREAD
#else
		/* Handle Interrupt */
		if (test_and_clear_bit(GLUE_FLAG_INT_BIT,
				       &prGlueInfo->ulFlag)) {

			if (fgNeedHwAccess == FALSE) {
				fgNeedHwAccess = TRUE;

				wlanAcquirePowerControl(prGlueInfo->prAdapter);
			}

			/* the Wi-Fi interrupt is already disabled in mmc
			 * thread, so we set the flag only to enable the
			 * interrupt later
			 */
			prGlueInfo->prAdapter->fgIsIntEnable = FALSE;
			/* wlanISR(prGlueInfo->prAdapter, TRUE); */

			if (prGlueInfo->ulFlag & GLUE_FLAG_HALT ||
				kalIsResetting()) {
				/* Should stop now... skip pending interrupt */
				DBGLOG(INIT, INFO,
					"ignore pending interrupt\n");
			} else {
				prGlueInfo->TaskIsrCnt++;
				wlanIST(prGlueInfo->prAdapter);
			}
		}
#endif
		/* transfer ioctl to OID request */

		do {
			if (test_and_clear_bit(GLUE_FLAG_OID_BIT,
					       &prGlueInfo->ulFlag)) {
				g_fgIsOid = TRUE;
				/* get current prIoReq */
				prIoReq = &(prGlueInfo->OidEntry);
				if (prIoReq->fgRead == FALSE) {
					prIoReq->rStatus = wlanSetInformation(
							prIoReq->prAdapter,
							prIoReq->pfnOidHandler,
							prIoReq->pvInfoBuf,
							prIoReq->u4InfoBufLen,
							prIoReq->pu4QryInfoLen);
				} else {
					prIoReq->rStatus = wlanQueryInformation(
							prIoReq->prAdapter,
							prIoReq->pfnOidHandler,
							prIoReq->pvInfoBuf,
							prIoReq->u4InfoBufLen,
							prIoReq->pu4QryInfoLen);
				}

				if (prIoReq->rStatus != WLAN_STATUS_PENDING) {
					/* complete ONLY if there are waiters */
					if (!completion_done(
						&prGlueInfo->rPendComp))
						complete(
							&prGlueInfo->rPendComp);
					else
						DBGLOG(INIT, WARN,
							"SKIP multiple OID complete!\n"
						       );
				} else {
					wlanoidTimeoutCheck(
							prGlueInfo->prAdapter,
							prIoReq->pfnOidHandler);
				}
			}

		} while (FALSE);

		/*
		 *
		 * if TX request, clear the TXREQ flag. TXREQ set by
		 * kalSetEvent/GlueSetEvent
		 * indicates the following requests occur
		 *
		 */

		if (test_and_clear_bit(GLUE_FLAG_TXREQ_BIT,
				       &prGlueInfo->ulFlag))
			kalProcessTxReq(prGlueInfo, &fgNeedHwAccess);
#if CFG_SUPPORT_MULTITHREAD
		/* Process RX */
		if (test_and_clear_bit(GLUE_FLAG_RX_BIT,
				       &prGlueInfo->ulFlag))
			nicRxProcessRFBs(prGlueInfo->prAdapter);
		if (test_and_clear_bit(GLUE_FLAG_TX_CMD_DONE_BIT,
				       &prGlueInfo->ulFlag))
			wlanTxCmdDoneMthread(prGlueInfo->prAdapter);
#endif

		/* Process RX, In linux, we don't need to free sk_buff by
		 * ourself
		 */

		/* In linux, we don't need to free sk_buff by ourself */

		/* In linux, we don't do reset */
#if CFG_SUPPORT_MULTITHREAD
#else
		if (fgNeedHwAccess == TRUE)
			wlanReleasePowerControl(prGlueInfo->prAdapter);
#endif
		/* handle cnmTimer time out */
		if (test_and_clear_bit(GLUE_FLAG_TIMEOUT_BIT,
				       &prGlueInfo->ulFlag))
			wlanTimerTimeoutCheck(prGlueInfo->prAdapter);
#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		if (prGlueInfo->fgEnSdioTestPattern == TRUE)
			kalSetEvent(prGlueInfo);
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
	if (GLUE_GET_REF_CNT(
		    prGlueInfo->i4TxPendingSecurityFrameNum) > 0)
		kalClearSecurityFrames(prGlueInfo);

	/* remove pending oid */
	wlanReleasePendingOid(prGlueInfo->prAdapter, 0);

	complete(&prGlueInfo->rHaltComp);
#if CFG_ENABLE_WAKE_LOCK
	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
				 &rTxThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, &rTxThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter,
			      &rTxThreadWakeLock);
#endif

	DBGLOG(INIT, TRACE, "%s:%u stopped!\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

#if CFG_CHIP_RESET_HANG
	while (fgIsResetHangState == SER_L0_HANG_RST_HANG) {
		kalMsleep(SER_L0_HANG_LOG_TIME_INTERVAL);
		DBGLOG(INIT, STATE, "[SER][L0] SQC hang!\n");
	}
#endif

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
u_int8_t kalIsCardRemoved(IN struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return FALSE;
	/* Linux MMC doesn't have removal notification yet */
}
#ifdef CONFIG_IDME
#ifdef CFG_SUPPORT_DUAL_CARD_DUAL_DRIVER
/* Support dual NIC in IDME mode */
#define IDME_MACADDR "/proc/idme/mac_addr2"
#else
#define IDME_MACADDR "/proc/idme/mac_addr"
#endif
static int idme_get_mac_addr(unsigned char *mac_addr, size_t addr_len)
{
	unsigned char buf[IFHWADDRLEN * 2 + 1] = {""}, str[3] = {""};
	int i, mac[IFHWADDRLEN];
	struct file *f;
	size_t len;

	if (!mac_addr || addr_len < IFHWADDRLEN) {
		DBGLOG(INIT, ERROR, "invalid mac_addr ptr or buf\n");
		return -1;
	}

	f = filp_open(IDME_MACADDR, O_RDONLY, 0);
	if (IS_ERR(f)) {
		DBGLOG(INIT, ERROR, "can't open mac addr file\n");
		return -1;
	}

	kalFileRead(f, f->f_pos, buf, IFHWADDRLEN * 2);

	if (strlen(buf) != IFHWADDRLEN * 2)
		goto bailout;

	for (i = 0; i < IFHWADDRLEN; i++) {
		str[0] = buf[i * 2];
		str[1] = buf[i * 2 + 1];
		if (!isxdigit(str[0]) || !isxdigit(str[1]))
			goto bailout;
		len = sscanf(str, "%02x", &mac[i]);
		if (len != 1)
			goto bailout;
	}
	for (i = 0; i < IFHWADDRLEN; i++)
		mac_addr[i] = (unsigned char)mac[i];
	return 0;
bailout:
	DBGLOG(INIT, ERROR, "wrong mac addr %02x %02x\n", buf[0], buf[1]);
	return -1;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to send command to firmware for overriding
 *        netweork address
 *
 * \param pvGlueInfo Pointer of GLUE Data Structure
 *
 * \retval TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t kalRetrieveNetworkAddress(IN struct GLUE_INFO *prGlueInfo,
			   IN OUT uint8_t *prMacAddr)
{
	ASSERT(prGlueInfo);

	/* Get MAC address override from wlan feature option */
	prGlueInfo->fgIsMacAddrOverride =
		prGlueInfo->prAdapter->rWifiVar.ucMacAddrOverride;

	wlanHwAddrToBin(
		prGlueInfo->prAdapter->rWifiVar.aucMacAddrStr,
		prGlueInfo->rMacAddrOverride);

#ifdef CONFIG_IDME
	if (prMacAddr && 0 == idme_get_mac_addr((unsigned char *)prMacAddr,
		(sizeof(uint8_t) * PARAM_MAC_ADDR_LEN))) {
		DBGLOG(INIT, INFO, "use IDME mac addr\n");
		return TRUE;
	}
#endif

	if (prGlueInfo->fgIsMacAddrOverride == FALSE) {

#ifdef CFG_ENABLE_EFUSE_MAC_ADDR
		if (prGlueInfo->prAdapter->fgIsEmbbededMacAddrValid) {
			COPY_MAC_ADDR(prMacAddr,
			      prGlueInfo->prAdapter->rWifiVar.aucMacAddress);
			return TRUE;
		} else {
			return FALSE;
		}
#else
		if (prGlueInfo->fgNvramAvailable == FALSE) {
			DBGLOG(INIT, INFO, "glLoadNvram fail\n");
			return FALSE;
		}
		kalMemCopy(prMacAddr, prGlueInfo->rRegInfo.aucMacAddr,
			   PARAM_MAC_ADDR_LEN * sizeof(uint8_t));
		return TRUE;
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
void kalFlushPendingTxPackets(IN struct GLUE_INFO
			      *prGlueInfo)
{
	struct QUE *prTxQue;
	struct QUE_ENTRY *prQueueEntry;
	void *prPacket;

	ASSERT(prGlueInfo);

	prTxQue = &(prGlueInfo->rTxQueue);

	if (GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum) == 0)
		return;

	if (HAL_IS_TX_DIRECT()) {
		nicTxDirectClearSkbQ(prGlueInfo->prAdapter);
	} else {
		GLUE_SPIN_LOCK_DECLARATION();

		while (TRUE) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
			QUEUE_REMOVE_HEAD(prTxQue, prQueueEntry,
					  struct QUE_ENTRY *);
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

			if (prQueueEntry == NULL)
				break;

			prPacket = GLUE_GET_PKT_DESCRIPTOR(prQueueEntry);

			kalSendComplete(prGlueInfo, prPacket,
					WLAN_STATUS_NOT_ACCEPTED);
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
enum ENUM_PARAM_MEDIA_STATE kalGetMediaStateIndicated(
	IN struct GLUE_INFO *prGlueInfo)
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
void kalSetMediaStateIndicated(IN struct GLUE_INFO
			       *prGlueInfo, IN enum ENUM_PARAM_MEDIA_STATE
			       eParamMediaStateIndicate)
{
	ASSERT(prGlueInfo);

	prGlueInfo->eParamMediaStateIndicated =
		eParamMediaStateIndicate;
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
void kalOidCmdClearance(IN struct GLUE_INFO *prGlueInfo)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE rReturnCmdQue;
	struct QUE *prReturnCmdQue = &rReturnCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);

	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {

		if (((struct CMD_INFO *) prQueueEntry)->fgIsOid) {
			prCmdInfo = (struct CMD_INFO *) prQueueEntry;
			break;
		}
		QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prTempCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	if (prCmdInfo) {
		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter,
						       prCmdInfo);
		else
			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, 0,
				       WLAN_STATUS_NOT_ACCEPTED);

		prGlueInfo->u4OidCompleteFlag = 1;
		cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
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
void kalEnqueueCommand(IN struct GLUE_INFO *prGlueInfo,
		       IN struct QUE_ENTRY *prQueueEntry)
{
	struct QUE *prCmdQue;
	struct CMD_INFO *prCmdInfo;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);
	ASSERT(prQueueEntry);

	prCmdQue = &prGlueInfo->rCmdQueue;

	prCmdInfo = (struct CMD_INFO *) prQueueEntry;

	DBGLOG(INIT, INFO,
	       "EN-Q CMD TYPE[%u] ID[0x%02X] SEQ[%u] to CMD Q\n",
	       prCmdInfo->eCmdType, prCmdInfo->ucCID,
	       prCmdInfo->ucCmdSeqNum);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
	GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
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
void kalHandleAssocInfo(IN struct GLUE_INFO *prGlueInfo,
			IN struct EVENT_ASSOC_INFO *prAssocInfo)
{
	/* to do */
}

/*----------------------------------------------------------------------------*/
/*!
 * * @brief Notify OS with SendComplete event of the specific packet.
 * *        Linux should free packets here.
 * *
 * * @param pvGlueInfo     Pointer of GLUE Data Structure
 * * @param pvPacket       Pointer of Packet Handle
 * * @param status         Status Code for OS upper layer
 * *
 * * @return none
 */
/*----------------------------------------------------------------------------*/

/* / Todo */
void kalSecurityFrameSendComplete(IN struct GLUE_INFO
			  *prGlueInfo, IN void *pvPacket, IN uint32_t rStatus)
{
	ASSERT(pvPacket);

	/* dev_kfree_skb((struct sk_buff *) pvPacket); */
	kalSendCompleteAndAwakeQueue(prGlueInfo, pvPacket);
	GLUE_DEC_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);
}

uint32_t kalGetTxPendingFrameCount(IN struct GLUE_INFO
				   *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return (uint32_t) (GLUE_GET_REF_CNT(
				   prGlueInfo->i4TxPendingFrameNum));
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
uint32_t kalGetTxPendingCmdCount(IN struct GLUE_INFO
				 *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return (uint32_t)GLUE_GET_REF_CNT(
		       prGlueInfo->i4TxPendingCmdNum);
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

void kalOsTimerInitialize(IN struct GLUE_INFO *prGlueInfo,
			  IN void *prTimerHandler)
{

	ASSERT(prGlueInfo);
#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	timer_setup(&(prGlueInfo->tickfn), prTimerHandler, 0);
#else
	init_timer(&(prGlueInfo->tickfn));
	prGlueInfo->tickfn.function = prTimerHandler;
	prGlueInfo->tickfn.data = (unsigned long)prGlueInfo;
#endif
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
u_int8_t kalSetTimer(IN struct GLUE_INFO *prGlueInfo,
		     IN uint32_t u4Interval)
{
	ASSERT(prGlueInfo);

	if (HAL_IS_RX_DIRECT(prGlueInfo->prAdapter)) {
		mod_timer(&prGlueInfo->tickfn,
			  jiffies + u4Interval * HZ / MSEC_PER_SEC);
	} else {
		del_timer_sync(&(prGlueInfo->tickfn));

		prGlueInfo->tickfn.expires = jiffies + u4Interval * HZ /
					     MSEC_PER_SEC;
		add_timer(&(prGlueInfo->tickfn));
	}

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
u_int8_t kalCancelTimer(IN struct GLUE_INFO *prGlueInfo)
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
void kalScanDone(IN struct GLUE_INFO *prGlueInfo,
		 IN enum ENUM_KAL_NETWORK_TYPE_INDEX eNetTypeIdx,
		 IN uint32_t status)
{
	uint8_t fgAborted = (status != WLAN_STATUS_SUCCESS) ? TRUE : FALSE;
	ASSERT(prGlueInfo);

	scanLogEssResult(prGlueInfo->prAdapter);

	scanReportBss2Cfg80211(prGlueInfo->prAdapter,
			       BSS_TYPE_INFRASTRUCTURE, NULL);

	/* check for system configuration for generating error message on scan
	 * list
	 */
	wlanCheckSystemConfiguration(prGlueInfo->prAdapter);

	kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_SCAN_COMPLETE,
		&fgAborted, sizeof(fgAborted));
}

#if CFG_SUPPORT_SCAN_CACHE_RESULT
/*----------------------------------------------------------------------------*/
/*!
 * @brief update timestamp information of bss cache in kernel
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @return   status 0 if success, error code otherwise
 */
/*----------------------------------------------------------------------------*/
uint8_t kalUpdateBssTimestamp(IN struct GLUE_INFO *prGlueInfo)
{
	struct wiphy *wiphy;
	struct cfg80211_registered_device *rdev;
	struct cfg80211_internal_bss *bss = NULL;
	struct cfg80211_bss_ies *ies;
	uint64_t new_timestamp = kalGetBootTime();

	ASSERT(prGlueInfo);
	wiphy = priv_to_wiphy(prGlueInfo);
	if (!wiphy) {
		log_dbg(REQ, ERROR, "wiphy is null\n");
		return 1;
	}
	rdev = container_of(wiphy, struct cfg80211_registered_device, wiphy);

	log_dbg(REQ, INFO, "Update scan timestamp: %llu (%llu)\n",
		new_timestamp, le64_to_cpu(new_timestamp));

	/* add 1 ms to prevent scan time too short */
	new_timestamp += 1000;

	spin_lock_bh(&rdev->bss_lock);
	list_for_each_entry(bss, &rdev->bss_list, list) {
		const struct cfg80211_bss_ies *old;

		ies = kzalloc(sizeof(*ies) + bss->pub.ies->len, GFP_ATOMIC);
		if (!ies)
			continue;
		ies->len = bss->pub.ies->len;
		ies->tsf = le64_to_cpu(new_timestamp);
		ies->from_beacon = bss->pub.ies->from_beacon;
		memcpy(ies->data, bss->pub.ies->data, bss->pub.ies->len);
		if (ies->from_beacon) {
			old = rcu_access_pointer(bss->pub.beacon_ies);
			rcu_assign_pointer(bss->pub.beacon_ies, ies);
		} else { /* proberesp */
			old = rcu_access_pointer(bss->pub.proberesp_ies);
			rcu_assign_pointer(bss->pub.proberesp_ies, ies);
		}
		rcu_assign_pointer(bss->pub.ies, ies);
		if (old)
			kfree_rcu((struct cfg80211_bss_ies *)old, rcu_head);
	}
	spin_unlock_bh(&rdev->bss_lock);

	return 0;
}
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to generate a random number
 *
 * \param none
 *
 * \retval UINT_32
 */
/*----------------------------------------------------------------------------*/
uint32_t kalRandomNumber(void)
{
	uint32_t number = 0;

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
#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
void kalTimeoutHandler(struct timer_list *timer)
#else
void kalTimeoutHandler(unsigned long arg)
#endif
{

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	struct GLUE_INFO *prGlueInfo =
		from_timer(prGlueInfo, timer, tickfn);
#else
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) arg;
#endif

	ASSERT(prGlueInfo);

	/* Notify tx thread  for timeout event */
	set_bit(GLUE_FLAG_TIMEOUT_BIT, &prGlueInfo->ulFlag);
	wake_up_interruptible(&prGlueInfo->waitq);

}

void kalSetEvent(struct GLUE_INFO *pr)
{
	set_bit(GLUE_FLAG_TXREQ_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq);
}

void kalSetIntEvent(struct GLUE_INFO *pr)
{
	KAL_WAKE_LOCK(pr->prAdapter, &pr->rIntrWakeLock);

	set_bit(GLUE_FLAG_INT_BIT, &pr->ulFlag);

	/* when we got interrupt, we wake up servie thread */
#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&pr->waitq_hif);
#else
	wake_up_interruptible(&pr->waitq);
#endif
}

void kalSetHifDbgEvent(struct GLUE_INFO *pr)
{
	set_bit(GLUE_FLAG_HIF_PRT_HIF_DBG_INFO_BIT, &(pr->ulFlag));
#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&pr->waitq_hif);
#endif
}

#if CFG_SUPPORT_MULTITHREAD
void kalSetTxEvent2Hif(struct GLUE_INFO *pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));
	set_bit(GLUE_FLAG_HIF_TX_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}

void kalSetFwOwnEvent2Hif(struct GLUE_INFO *pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	set_bit(GLUE_FLAG_HIF_FW_OWN_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}

void kalSetTxEvent2Rx(struct GLUE_INFO *pr)
{
	if (!pr->rx_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	set_bit(GLUE_FLAG_RX_TO_OS_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_rx);
}

void kalSetTxCmdEvent2Hif(struct GLUE_INFO *pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, &pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

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
u_int8_t kalIsConfigurationExist(IN struct GLUE_INFO
				 *prGlueInfo)
{
#if !defined(CONFIG_X86)
	ASSERT(prGlueInfo);

	return prGlueInfo->fgNvramAvailable;
#else
	/* there is no configuration data for x86-linux */
	/*return FALSE;*/


	/*Modify for Linux PC support NVRAM Setting*/
	return prGlueInfo->fgNvramAvailable;
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
struct REG_INFO *kalGetConfiguration(IN struct GLUE_INFO
				     *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return &(prGlueInfo->rRegInfo);
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
u_int8_t kalWSCGetActiveState(IN struct GLUE_INFO
			      *prGlueInfo)
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
void
kalUpdateRSSI(IN struct GLUE_INFO *prGlueInfo,
	      IN enum ENUM_KAL_NETWORK_TYPE_INDEX eNetTypeIdx,
	      IN int8_t cRssi, IN int8_t cLinkQuality)
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
		pStats = (struct iw_statistics *)(&
						  (prGlueInfo->rP2pIwStats));
		break;
#endif
#endif
	default:
		break;

	}

	if (pStats) {
		pStats->qual.qual = cLinkQuality;
		pStats->qual.noise = 0;
		pStats->qual.updated = IW_QUAL_QUAL_UPDATED |
				       IW_QUAL_NOISE_UPDATED;
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
u_int8_t kalInitIOBuffer(u_int8_t is_pre_alloc)
{
	uint32_t u4Size;

	/* not pre-allocation for all memory usage */
	if (!is_pre_alloc) {
		pvIoBuffer = NULL;
		return FALSE;
	}

	/* pre-allocation for all memory usage */
	if (HIF_TX_COALESCING_BUFFER_SIZE >
	    HIF_RX_COALESCING_BUFFER_SIZE)
		u4Size = HIF_TX_COALESCING_BUFFER_SIZE;
	else
		u4Size = HIF_RX_COALESCING_BUFFER_SIZE;

	u4Size += HIF_EXTRA_IO_BUFFER_SIZE;

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
void kalUninitIOBuffer(void)
{
	kfree(pvIoBuffer);

	pvIoBuffer = (void *) NULL;
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
void *kalAllocateIOBuffer(IN uint32_t u4AllocSize)
{
	void *ret = (void *) NULL;

	if (pvIoBuffer) {
		if (u4AllocSize <= (pvIoBufferSize - pvIoBufferUsage)) {
			ret = (void *)
				&(((uint8_t *) (pvIoBuffer))[pvIoBufferUsage]);
			pvIoBufferUsage += u4AllocSize;
		}
	} else {
		/* fault tolerance */
		ret = (void *) kalMemAlloc(u4AllocSize, PHY_MEM_TYPE);
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
void kalReleaseIOBuffer(IN void *pvAddr, IN uint32_t u4Size)
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
void
kalGetChannelList(IN struct GLUE_INFO *prGlueInfo,
		  IN enum ENUM_BAND eSpecificBand,
		  IN uint8_t ucMaxChannelNum, IN uint8_t *pucNumOfChannel,
		  IN struct RF_CHANNEL_INFO *paucChannelList)
{
	rlmDomainGetChnlList(prGlueInfo->prAdapter, eSpecificBand,
			     FALSE,
			     ucMaxChannelNum, pucNumOfChannel, paucChannelList);
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
u_int8_t kalIsAPmode(IN struct GLUE_INFO *prGlueInfo)
{
#if 0				/* Marked for MT6630 (New ucBssIndex) */
#if CFG_ENABLE_WIFI_DIRECT
	if (IS_NET_ACTIVE(prGlueInfo->prAdapter,
			  NETWORK_TYPE_P2P_INDEX) &&
	    p2pFuncIsAPMode(
		    prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo))
		return TRUE;
#endif
#endif

	return FALSE;
}

#if CFG_SUPPORT_802_11W
/*----------------------------------------------------------------------------*/
/*!
 * \brief to check if the MFP is DISABLD/OPTIONAL/REQUIRED
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 *	 RSN_AUTH_MFP_DISABLED
 *	 RSN_AUTH_MFP_OPTIONAL
 *	 RSN_AUTH_MFP_DISABLED
 */
/*----------------------------------------------------------------------------*/
uint32_t kalGetMfpSetting(IN struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4RsnMfp = RSN_AUTH_MFP_DISABLED;

	ASSERT(prGlueInfo);

	switch (prGlueInfo->rWpaInfo.u4Mfp) {
	case IW_AUTH_MFP_DISABLED:
		u4RsnMfp = RSN_AUTH_MFP_DISABLED;
		break;
	case IW_AUTH_MFP_OPTIONAL:
		u4RsnMfp = RSN_AUTH_MFP_OPTIONAL;
		break;
	case IW_AUTH_MFP_REQUIRED:
		u4RsnMfp = RSN_AUTH_MFP_REQUIRED;
		break;
	default:
		u4RsnMfp = RSN_AUTH_MFP_DISABLED;
		break;
	}

	return u4RsnMfp;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief to check if the RSN IE CAP setting from supplicant
 *
 * \param[in]
 *           prGlueInfo
 *
 * \return
 *           TRUE
 *           FALSE
 */
/*----------------------------------------------------------------------------*/
uint8_t kalGetRsnIeMfpCap(IN struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->rWpaInfo.ucRSNMfpCap;
}
#endif

struct file *kalFileOpen(const char *path, int flags,
			 int rights)
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

void kalFileClose(struct file *file)
{
	filp_close(file, NULL);
}

uint32_t kalFileRead(struct file *file,
		     unsigned long long offset, unsigned char *data,
		     unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	ret = kernel_read(file, data, size, &offset);
#else
	ret = vfs_read(file, data, size, &offset);
#endif

	set_fs(oldfs);
	return ret;
}

uint32_t kalFileWrite(struct file *file,
		      unsigned long long offset, unsigned char *data,
		      unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	ret = kernel_write(file, data, size, &offset);
#else
	ret = vfs_write(file, data, size, &offset);
#endif

	set_fs(oldfs);
	return ret;
}

uint32_t kalWriteToFile(const uint8_t *pucPath,
			u_int8_t fgDoAppend, uint8_t *pucData, uint32_t u4Size)
{
	struct file *file = NULL;
	int32_t ret = -1;
	uint32_t u4Flags = 0;

	if (fgDoAppend)
		u4Flags = O_APPEND;

	file = kalFileOpen(pucPath, O_WRONLY | O_CREAT | u4Flags, 0700);
	if (file != NULL) {
		kalFileWrite(file, 0, pucData, u4Size);
		kalFileClose(file);
		ret = 0;
	}
	return ret;
}

int32_t kalReadToFile(const uint8_t *pucPath,
		      uint8_t *pucData, uint32_t u4Size, uint32_t *pu4ReadSize)
{
	struct file *file = NULL;
	int32_t ret = -1;
	uint32_t u4ReadSize = 0;

	DBGLOG(INIT, INFO, "kalReadToFile() path %s\n", pucPath);

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

uint32_t kalCheckPath(const uint8_t *pucPath)
{
	struct file *file = NULL;
	uint32_t u4Flags = 0;

	file = kalFileOpen(pucPath, O_WRONLY | O_CREAT | u4Flags, 0700);
	if (!file)
		return -1;

	kalFileClose(file);
	return 1;
}

uint32_t kalTrunkPath(const uint8_t *pucPath)
{
	struct file *file = NULL;
	uint32_t u4Flags = O_TRUNC;

	file = kalFileOpen(pucPath, O_WRONLY | O_CREAT | u4Flags, 0700);
	if (!file)
		return -1;

	kalFileClose(file);
	return 1;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief read request firmware file binary to pucData
 *
 * \param[in] pucPath  file name
 * \param[out] pucData  Request file output buffer
 * \param[in] u4Size  read size
 * \param[out] pu4ReadSize  real read size
 * \param[in] dev
 *
 * \return
 *           0 success
 *           >0 fail
 */
/*----------------------------------------------------------------------------*/
int32_t kalRequestFirmware(const uint8_t *pucPath,
			   uint8_t *pucData, uint32_t u4Size,
			   uint32_t *pu4ReadSize, struct device *dev)
{
	const struct firmware *fw;
	int ret = 0;

	/*
	 * Driver support request_firmware() to get files
	 * Android path: "/etc/firmware", "/vendor/firmware", "/firmware/image"
	 * Linux path: "/lib/firmware", "/lib/firmware/update"
	 */
	ret = request_firmware(&fw, pucPath, dev);

	if (ret != 0) {
		DBGLOG(INIT, INFO, "kalRequestFirmware %s Fail, errno[%d]!!\n",
		       pucPath, ret);
		pucData = NULL;
		*pu4ReadSize = 0;
		return ret;
	}

	DBGLOG(INIT, INFO, "kalRequestFirmware(): %s OK\n",
	       pucPath);

	if (fw->size < u4Size)
		u4Size = fw->size;

	memcpy(pucData, fw->data, u4Size);
	if (pu4ReadSize)
		*pu4ReadSize = u4Size;

	release_firmware(fw);

	return ret;
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
void
kalIndicateBssInfo(IN struct GLUE_INFO *prGlueInfo,
		   IN uint8_t *pucBeaconProbeResp,
		   IN uint32_t u4FrameLen, IN uint8_t ucChannelNum,
		   IN int32_t i4SignalStrength)
{
	struct wiphy *wiphy;
	struct ieee80211_channel *prChannel = NULL;

	ASSERT(prGlueInfo);
	wiphy = priv_to_wiphy(prGlueInfo);

	/* search through channel entries */
	if (ucChannelNum <= 14) {
		prChannel = ieee80211_get_channel(wiphy,
				ieee80211_channel_to_frequency(ucChannelNum,
								KAL_BAND_2GHZ));
	} else {
		prChannel = ieee80211_get_channel(wiphy,
				ieee80211_channel_to_frequency(ucChannelNum,
								KAL_BAND_5GHZ));
	}

	if (prChannel != NULL
	    && prGlueInfo->fgIsRegistered == TRUE) {
		struct cfg80211_bss *bss;
#if CFG_SUPPORT_TSF_USING_BOOTTIME
		struct ieee80211_mgmt *prMgmtFrame = (struct ieee80211_mgmt
						      *)pucBeaconProbeResp;

		prMgmtFrame->u.beacon.timestamp = kalGetBootTime();
#endif

		kalScanResultLog(prGlueInfo->prAdapter,
			(struct ieee80211_mgmt *)pucBeaconProbeResp);

		/* indicate to NL80211 subsystem */
		bss = cfg80211_inform_bss_frame(wiphy, prChannel,
				(struct ieee80211_mgmt *)pucBeaconProbeResp,
				u4FrameLen, i4SignalStrength * 100, GFP_KERNEL);

		if (!bss) {
			/* ToDo:: DBGLOG */
			DBGLOG(REQ, WARN,
			       "cfg80211_inform_bss_frame() returned with NULL\n");
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
void
kalReadyOnChannel(IN struct GLUE_INFO *prGlueInfo,
		  IN uint64_t u8Cookie,
		  IN enum ENUM_BAND eBand, IN enum ENUM_CHNL_EXT eSco,
		  IN uint8_t ucChannelNum, IN uint32_t u4DurationMs)
{
	struct ieee80211_channel *prChannel = NULL;
	enum nl80211_channel_type rChannelType;

	/* ucChannelNum = wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter,
	 *                NETWORK_TYPE_AIS_INDEX);
	 */

	if (prGlueInfo->fgIsRegistered == TRUE) {
		if (ucChannelNum <= 14) {
			prChannel =
				ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
				ieee80211_channel_to_frequency(ucChannelNum,
				KAL_BAND_2GHZ));
		} else {
			prChannel =
				ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
				ieee80211_channel_to_frequency(ucChannelNum,
				KAL_BAND_5GHZ));
		}

		if (prChannel == NULL) {
			DBGLOG(AIS, WARN,
			"prChannel is null");
			return;
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
			u8Cookie, prChannel, u4DurationMs, GFP_KERNEL);
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
void
kalRemainOnChannelExpired(IN struct GLUE_INFO *prGlueInfo,
			  IN uint64_t u8Cookie, IN enum ENUM_BAND eBand,
			  IN enum ENUM_CHNL_EXT eSco, IN uint8_t ucChannelNum)
{
	struct ieee80211_channel *prChannel = NULL;
	enum nl80211_channel_type rChannelType;

	ucChannelNum =
		wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter,
			prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex);

	if (prGlueInfo->fgIsRegistered == TRUE) {
		if (ucChannelNum <= 14) {
			prChannel =
				ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
				ieee80211_channel_to_frequency(ucChannelNum,
				KAL_BAND_2GHZ));
		} else {
			prChannel =
				ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
				ieee80211_channel_to_frequency(ucChannelNum,
				KAL_BAND_5GHZ));
		}

		if (prChannel == NULL) {
			DBGLOG(AIS, WARN,
			"prChannel is null");
			return;
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
			u8Cookie, prChannel, GFP_KERNEL);
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
void
kalIndicateMgmtTxStatus(IN struct GLUE_INFO *prGlueInfo,
			IN uint64_t u8Cookie, IN u_int8_t fgIsAck,
			IN uint8_t *pucFrameBuf, IN uint32_t u4FrameLen)
{

	do {
		if ((prGlueInfo == NULL)
		    || (pucFrameBuf == NULL)
		    || (u4FrameLen == 0)) {
			DBGLOG(AIS, TRACE,
			       "Unexpected pointer PARAM. 0x%lx, 0x%lx, %d.",
			       prGlueInfo, pucFrameBuf, u4FrameLen);
			ASSERT(FALSE);
			break;
		}

		cfg80211_mgmt_tx_status(
			prGlueInfo->prDevHandler->ieee80211_ptr,
			u8Cookie, pucFrameBuf, u4FrameLen, fgIsAck, GFP_KERNEL);

	} while (FALSE);

}				/* kalIndicateMgmtTxStatus */

void kalIndicateRxMgmtFrame(IN struct GLUE_INFO *prGlueInfo,
			    IN struct SW_RFB *prSwRfb)
{
	int32_t i4Freq = 0;
	uint8_t ucChnlNum = 0;

	do {
		if ((prGlueInfo == NULL) || (prSwRfb == NULL)) {
			ASSERT(FALSE);
			break;
		}

		ucChnlNum = (uint8_t) HAL_RX_STATUS_GET_CHNL_NUM(
							prSwRfb->prRxStatus);

		i4Freq = nicChannelNum2Freq(ucChnlNum) / 1000;

		if (prGlueInfo->prDevHandler->ieee80211_ptr == NULL) {
			DBGLOG(AIS, WARN,
				"ieee80211_ptr is NULL!\n");
			break;
		}

		if (prGlueInfo->u4OsMgmtFrameFilter == 0) {
			DBGLOG(AIS, WARN,
				"The cfg80211 hasn't do mgmt register!\n");
			break;
		}

#if (KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(prGlueInfo->prDevHandler->ieee80211_ptr,
			i4Freq,	/* in MHz */
			RCPI_TO_dBm((uint8_t) nicRxGetRcpiValueFromRxv(
			RCPI_MODE_MAX, prSwRfb)),
			prSwRfb->pvHeader, prSwRfb->u2PacketLen,
			NL80211_RXMGMT_FLAG_ANSWERED);

#elif (KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(prGlueInfo->prDevHandler->ieee80211_ptr,
			i4Freq,	/* in MHz */
			RCPI_TO_dBm((uint8_t)
			nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb)),
			prSwRfb->pvHeader, prSwRfb->u2PacketLen,
			NL80211_RXMGMT_FLAG_ANSWERED,
			GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(prGlueInfo->prDevHandler->ieee80211_ptr,
			i4Freq,	/* in MHz */
			RCPI_TO_dBm((uint8_t)
			nicRxGetRcpiValueFromRxv(RCPI_MODE_WF0, prSwRfb)),
			prSwRfb->pvHeader, prSwRfb->u2PacketLen,
			GFP_ATOMIC);
#endif

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
u_int8_t kalSetSdioTestPattern(IN struct GLUE_INFO
			*prGlueInfo, IN u_int8_t fgEn, IN u_int8_t fgRead)
{
	const uint8_t aucPattern[] = {
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
	uint32_t i;

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
void *pMetGlobalData;

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
void kalSchedScanResults(IN struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);
	scanReportBss2Cfg80211(prGlueInfo->prAdapter,
			       BSS_TYPE_INFRASTRUCTURE, NULL);

	scanlog_dbg(LOG_SCHED_SCAN_DONE_D2K, INFO, "Call cfg80211_sched_scan_results\n");
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	cfg80211_sched_scan_results(priv_to_wiphy(prGlueInfo), 0);
#else
	cfg80211_sched_scan_results(priv_to_wiphy(prGlueInfo));
#endif
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
void kalSchedScanStopped(IN struct GLUE_INFO *prGlueInfo,
			 u_int8_t fgDriverTriggerd)
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
	/* 20150205 change cfg80211_sched_scan_stopped to work queue to use K
	 * thread to send event instead of Tx thread
	 * due to sched_scan_mtx dead lock issue by Tx thread serves oid cmds
	 * and send event in the same time
	 */
	if (fgDriverTriggerd) {
		DBGLOG(SCN, INFO, "start work queue to send event\n");
		schedule_delayed_work(&sched_workq, 0);
		DBGLOG(SCN, INFO, "main_thread return from %s\n", __func__);
	}
}

#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
/*----------------------------------------------------------------------------*/
/*!
 * \brief    To check if device if wake up by wlan
 *
 * \param[in]
 *           prAdapter
 *
 * \return
 *           TRUE: wake up by wlan; otherwise, FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t kalIsWakeupByWlan(struct ADAPTER *prAdapter)
{
	/*
	 * SUSPEND_FLAG_FOR_WAKEUP_REASON is set means system has suspended,
	 * but may be failed duo to some driver suspend failed. so we need
	 * help of function slp_get_wake_reason
	 */
	if (test_and_clear_bit(SUSPEND_FLAG_FOR_WAKEUP_REASON,
			       &prAdapter->ulSuspendFlag) == 0)
		return FALSE;

	return TRUE;
}
#endif



u_int8_t
kalGetIPv4Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv4Addrs,
		  OUT uint32_t *pu4NumOfIpv4Addr)
{
	uint32_t u4NumIPv4 = 0;
	uint32_t u4AddrLen = IPV4_ADDR_LEN;
	struct in_ifaddr *prIfa;

	/* 4 <1> Sanity check of netDevice */
	if (!prDev || !(prDev->ip_ptr)
	    || !((struct in_device *)(prDev->ip_ptr))->ifa_list) {
		DBGLOG(INIT, INFO,
		       "IPv4 address is not available for dev(0x%p)\n", prDev);

		*pu4NumOfIpv4Addr = 0;
		return FALSE;
	}

	prIfa = ((struct in_device *)(prDev->ip_ptr))->ifa_list;

	/* 4 <2> copy the IPv4 address */
	while ((u4NumIPv4 < u4MaxNumOfAddr) && prIfa) {
		kalMemCopy(&pucIpv4Addrs[u4NumIPv4 * u4AddrLen],
			   &prIfa->ifa_local, u4AddrLen);
		prIfa = prIfa->ifa_next;

		DBGLOG(INIT, INFO,
		       "IPv4 addr [%u][" IPV4STR "]\n", u4NumIPv4,
		       IPV4TOSTR(&pucIpv4Addrs[u4NumIPv4 * u4AddrLen]));

		u4NumIPv4++;
	}

	*pu4NumOfIpv4Addr = u4NumIPv4;

	return TRUE;
}

#if IS_ENABLED(CONFIG_IPV6)
u_int8_t
kalGetIPv6Address(IN struct net_device *prDev,
		  IN uint32_t u4MaxNumOfAddr, OUT uint8_t *pucIpv6Addrs,
		  OUT uint32_t *pu4NumOfIpv6Addr)
{
	uint32_t u4NumIPv6 = 0;
	uint32_t u4AddrLen = IPV6_ADDR_LEN;
	struct inet6_ifaddr *prIfa;

	/* 4 <1> Sanity check of netDevice */
	if (!prDev || !(prDev->ip6_ptr)) {
		DBGLOG(INIT, INFO,
		       "IPv6 address is not available for dev(0x%p)\n", prDev);

		*pu4NumOfIpv6Addr = 0;
		return FALSE;
	}


	/* 4 <2> copy the IPv6 address */
	LIST_FOR_EACH_IPV6_ADDR(prIfa, prDev->ip6_ptr) {
		kalMemCopy(&pucIpv6Addrs[u4NumIPv6 * u4AddrLen],
			   &prIfa->addr, u4AddrLen);

		DBGLOG(INIT, INFO,
		       "IPv6 addr [%u][" IPV6STR "]\n", u4NumIPv6,
		       IPV6TOSTR(&pucIpv6Addrs[u4NumIPv6 * u4AddrLen]));

		if ((u4NumIPv6 + 1) >= u4MaxNumOfAddr)
			break;
		u4NumIPv6++;
	}

	*pu4NumOfIpv6Addr = u4NumIPv6;

	return TRUE;
}
#endif /* IS_ENABLED(CONFIG_IPV6) */

void
kalSetNetAddress(IN struct GLUE_INFO *prGlueInfo,
		 IN uint8_t ucBssIdx,
		 IN uint8_t *pucIPv4Addr, IN uint32_t u4NumIPv4Addr,
		 IN uint8_t *pucIPv6Addr, IN uint32_t u4NumIPv6Addr)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0;
	uint32_t u4Len = OFFSET_OF(struct
				   PARAM_NETWORK_ADDRESS_LIST, arAddress);
	struct PARAM_NETWORK_ADDRESS_LIST *prParamNetAddrList;
	struct PARAM_NETWORK_ADDRESS *prParamNetAddr;
	uint32_t i, u4AddrLen;

	/* 4 <1> Calculate buffer size */
	/* IPv4 */
	u4Len += (((sizeof(struct PARAM_NETWORK_ADDRESS) - 1) +
		   IPV4_ADDR_LEN) * u4NumIPv4Addr);
	/* IPv6 */
	u4Len += (((sizeof(struct PARAM_NETWORK_ADDRESS) - 1) +
		   IPV6_ADDR_LEN) * u4NumIPv6Addr);

	/* 4 <2> Allocate buffer */
	prParamNetAddrList = (struct PARAM_NETWORK_ADDRESS_LIST *)
			     kalMemAlloc(u4Len, VIR_MEM_TYPE);

	if (!prParamNetAddrList) {
		DBGLOG(INIT, WARN,
		       "Fail to alloc buffer for setting BSS[%u] network address!\n",
		       ucBssIdx);
		return;
	}
	/* 4 <3> Fill up network address */
	prParamNetAddrList->u2AddressType =
		PARAM_PROTOCOL_ID_TCP_IP;
	prParamNetAddrList->u4AddressCount = 0;
	prParamNetAddrList->ucBssIdx = ucBssIdx;

	/* 4 <3.1> Fill up IPv4 address */
	u4AddrLen = IPV4_ADDR_LEN;
	prParamNetAddr = prParamNetAddrList->arAddress;
	for (i = 0; i < u4NumIPv4Addr; i++) {
		prParamNetAddr->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
		prParamNetAddr->u2AddressLength = u4AddrLen;
		kalMemCopy(prParamNetAddr->aucAddress,
			   &pucIPv4Addr[i * u4AddrLen], u4AddrLen);

		prParamNetAddr = (struct PARAM_NETWORK_ADDRESS *) ((
			unsigned long) prParamNetAddr + (unsigned long) (
			u4AddrLen + OFFSET_OF(
			struct PARAM_NETWORK_ADDRESS, aucAddress)));
	}
	prParamNetAddrList->u4AddressCount += u4NumIPv4Addr;

	/* 4 <3.2> Fill up IPv6 address */
	u4AddrLen = IPV6_ADDR_LEN;
	for (i = 0; i < u4NumIPv6Addr; i++) {
		prParamNetAddr->u2AddressType = PARAM_PROTOCOL_ID_TCP_IP;
		prParamNetAddr->u2AddressLength = u4AddrLen;
		kalMemCopy(prParamNetAddr->aucAddress,
			   &pucIPv6Addr[i * u4AddrLen], u4AddrLen);

		prParamNetAddr = (struct PARAM_NETWORK_ADDRESS *) ((
			unsigned long) prParamNetAddr + (unsigned long) (
			u4AddrLen + OFFSET_OF(
			struct PARAM_NETWORK_ADDRESS, aucAddress)));
	}
	prParamNetAddrList->u4AddressCount += u4NumIPv6Addr;

	/* 4 <4> IOCTL to main_thread */
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetNetworkAddress,
			   (void *) prParamNetAddrList, u4Len,
			   FALSE, FALSE, TRUE, &u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "%s: Fail to set network address\n",
		       __func__);

	kalMemFree(prParamNetAddrList, VIR_MEM_TYPE, u4Len);

}

void kalSetNetAddressFromInterface(IN struct GLUE_INFO
		   *prGlueInfo, IN struct net_device *prDev, IN u_int8_t fgSet)
{
	uint32_t u4NumIPv4, u4NumIPv6;
	uint8_t pucIPv4Addr[IPV4_ADDR_LEN * CFG_PF_ARP_NS_MAX_NUM],
		pucIPv6Addr[IPV6_ADDR_LEN * CFG_PF_ARP_NS_MAX_NUM];
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			  netdev_priv(prDev);

	if (prNetDevPrivate->prGlueInfo != prGlueInfo)
		DBGLOG(REQ, WARN, "%s: unexpected prGlueInfo(0x%p)!\n",
		       __func__, prNetDevPrivate->prGlueInfo);

	u4NumIPv4 = 0;
	u4NumIPv6 = 0;

	if (fgSet) {
		kalGetIPv4Address(prDev, CFG_PF_ARP_NS_MAX_NUM, pucIPv4Addr,
				  &u4NumIPv4);
		kalGetIPv6Address(prDev, CFG_PF_ARP_NS_MAX_NUM, pucIPv6Addr,
				  &u4NumIPv6);
	}

	if (u4NumIPv4 + u4NumIPv6 > CFG_PF_ARP_NS_MAX_NUM) {
		if (u4NumIPv4 >= CFG_PF_ARP_NS_MAX_NUM) {
			u4NumIPv4 = CFG_PF_ARP_NS_MAX_NUM;
			u4NumIPv6 = 0;
		} else {
			u4NumIPv6 = CFG_PF_ARP_NS_MAX_NUM - u4NumIPv4;
		}
	}

	kalSetNetAddress(prGlueInfo, prNetDevPrivate->ucBssIdx,
			 pucIPv4Addr, u4NumIPv4, pucIPv6Addr, u4NumIPv6);
}

#if CFG_MET_PACKET_TRACE_SUPPORT

u_int8_t kalMetCheckProfilingPacket(IN struct GLUE_INFO
				    *prGlueInfo, IN void *prPacket)
{
	uint32_t u4PacketLen;
	uint16_t u2EtherTypeLen;
	struct sk_buff *prSkb = (struct sk_buff *)prPacket;
	uint8_t *aucLookAheadBuf = NULL;
	uint8_t ucEthTypeLenOffset = ETHER_HEADER_LEN -
				     ETHER_TYPE_LEN;
	uint8_t *pucNextProtocol = NULL;

	u4PacketLen = prSkb->len;

	if (u4PacketLen < ETHER_HEADER_LEN) {
		DBGLOG(INIT, WARN, "Invalid Ether packet length: %u\n",
		       u4PacketLen);
		return FALSE;
	}

	aucLookAheadBuf = prSkb->data;

	/* 4 <0> Obtain Ether Type/Len */
	WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset],
			    &u2EtherTypeLen);

	/* 4 <1> Skip 802.1Q header (VLAN Tagging) */
	if (u2EtherTypeLen == ETH_P_VLAN) {
		ucEthTypeLenOffset += ETH_802_1Q_HEADER_LEN;
		WLAN_GET_FIELD_BE16(&aucLookAheadBuf[ucEthTypeLenOffset],
				    &u2EtherTypeLen);
	}
	/* 4 <2> Obtain next protocol pointer */
	pucNextProtocol = &aucLookAheadBuf[ucEthTypeLenOffset +
							      ETHER_TYPE_LEN];

	/* 4 <3> Handle ethernet format */
	switch (u2EtherTypeLen) {

	/* IPv4 */
	case ETH_P_IPV4: {
		uint8_t *pucIpHdr = pucNextProtocol;
		uint8_t ucIpVersion;

		/* IPv4 header length check */
		if (u4PacketLen < (ucEthTypeLenOffset + ETHER_TYPE_LEN +
				   IPV4_HDR_LEN)) {
			DBGLOG(INIT, WARN, "Invalid IPv4 packet length: %u\n",
			       u4PacketLen);
			return FALSE;
		}

		/* IPv4 version check */
		ucIpVersion = (pucIpHdr[0] & IP_VERSION_MASK) >>
			      IP_VERSION_OFFSET;
		if (ucIpVersion != IP_VERSION_4) {
			DBGLOG(INIT, WARN, "Invalid IPv4 packet version: %u\n",
			       ucIpVersion);
			return FALSE;
		}

		if (pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET] == IP_PRO_UDP) {
			uint8_t *pucUdpHdr = &pucIpHdr[IPV4_HDR_LEN];
			uint16_t u2UdpDstPort;
			uint16_t u2UdpSrcPort;

			/* Get UDP DST port */
			WLAN_GET_FIELD_BE16(&pucUdpHdr[UDP_HDR_DST_PORT_OFFSET],
					    &u2UdpDstPort);

			/* Get UDP SRC port */
			WLAN_GET_FIELD_BE16(&pucUdpHdr[UDP_HDR_SRC_PORT_OFFSET],
					    &u2UdpSrcPort);

			if (u2UdpSrcPort == prGlueInfo->u2MetUdpPort) {
				uint16_t u2IpId;

				/* Store IP ID for Tag */
				WLAN_GET_FIELD_BE16(
				&pucIpHdr[IPV4_HDR_IP_IDENTIFICATION_OFFSET],
				&u2IpId);
#if 0
				DBGLOG(INIT, INFO,
				       "TX PKT PROTOCOL[0x%x] UDP DST port[%u] IP_ID[%u]\n",
				       pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET],
				       u2UdpDstPort,
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

static int __mt_find_tracing_mark_write_symbol_fn(
	void *prData, const char *pcNameBuf,
	struct module *prModule, unsigned long ulAddress)
{
	if (strcmp(pcNameBuf, "tracing_mark_write") == 0) {
		tracing_mark_write_addr = ulAddress;
		return 1;
	}
	return 0;
}

static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		kallsyms_on_each_symbol(
			__mt_find_tracing_mark_write_symbol_fn, NULL);
}

void kalMetTagPacket(IN struct GLUE_INFO *prGlueInfo,
		     IN void *prPacket, IN enum ENUM_TX_PROFILING_TAG eTag)
{
	if (!prGlueInfo->fgMetProfilingEn)
		return;

	switch (eTag) {
	case TX_PROF_TAG_OS_TO_DRV:
		if (kalMetCheckProfilingPacket(prGlueInfo, prPacket)) {
			/* trace_printk("S|%d|%s|%d\n", current->pid,
			 * "WIFI-CHIP", GLUE_GET_PKT_IP_ID(prPacket));
			 */
			__mt_update_tracing_mark_write_addr();
#if 0 /* #ifdef CONFIG_TRACING */ /* #if CFG_MET_PACKET_TRACE_SUPPORT */
			event_trace_printk(tracing_mark_write_addr,
					   "S|%d|%s|%d\n",
					   current->tgid, "WIFI-CHIP",
					   GLUE_GET_PKT_IP_ID(prPacket));
#endif
			GLUE_SET_PKT_FLAG_PROF_MET(prPacket);
		}
		break;

	case TX_PROF_TAG_DRV_TX_DONE:
		if (GLUE_GET_PKT_IS_PROF_MET(prPacket)) {
			/* trace_printk("F|%d|%s|%d\n", current->pid,
			 * "WIFI-CHIP", GLUE_GET_PKT_IP_ID(prPacket));
			 */
			__mt_update_tracing_mark_write_addr();
#if 0 /* #ifdef CONFIG_TRACING */ /* #if CFG_MET_PACKET_TRACE_SUPPORT */
			event_trace_printk(tracing_mark_write_addr,
					   "F|%d|%s|%d\n",
					   current->tgid, "WIFI-CHIP",
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

void kalMetInit(IN struct GLUE_INFO *prGlueInfo)
{
	prGlueInfo->fgMetProfilingEn = FALSE;
	prGlueInfo->u2MetUdpPort = 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief The PROC function for adjusting Debug Level to turn on/off debugging
 *	  message.
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
static ssize_t kalMetWriteProcfs(struct file *file,
			 const char __user *buffer, size_t count, loff_t *off)
{
	char acBuf[128 + 1];	/* + 1 for "\0" */
	uint32_t u4CopySize;
	int u16MetUdpPort;
	int u8MetProfEnable;

	IN struct GLUE_INFO *prGlueInfo;
	ssize_t result;

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count :
		     (sizeof(acBuf) - 1);
	result = copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (sscanf(acBuf, " %d %d", &u8MetProfEnable,
		   &u16MetUdpPort) == 2)
		DBGLOG(INIT, INFO,
		       "MET_PROF: Write MET PROC Enable=%d UDP_PORT=%d\n",
		       u8MetProfEnable, u16MetUdpPort);
	if (pMetGlobalData != NULL) {
		prGlueInfo = (struct GLUE_INFO *) pMetGlobalData;
		prGlueInfo->fgMetProfilingEn = (u_int8_t) u8MetProfEnable;
		prGlueInfo->u2MetUdpPort = (uint16_t) u16MetUdpPort;
	}
	return count;
}
#endif
static ssize_t kalMetCtrlWriteProcfs(struct file *file,
		     const char __user *buffer, size_t count, loff_t *off)
{
	char acBuf[128 + 1];	/* + 1 for "\0" */
	uint32_t u4CopySize;
	int u8MetProfEnable;
	ssize_t result;

	IN struct GLUE_INFO *prGlueInfo;

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count :
		     (sizeof(acBuf) - 1);
	result = copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (sscanf(acBuf, " %d", &u8MetProfEnable) == 1)
		DBGLOG(INIT, INFO, "MET_PROF: Write MET PROC Enable=%d\n",
		       u8MetProfEnable);
	if (pMetGlobalData != NULL) {
		prGlueInfo = (struct GLUE_INFO *) pMetGlobalData;
		prGlueInfo->fgMetProfilingEn = (uint8_t) u8MetProfEnable;
	}
	return count;
}

static ssize_t kalMetPortWriteProcfs(struct file *file,
		     const char __user *buffer, size_t count, loff_t *off)
{
	char acBuf[128 + 1];	/* + 1 for "\0" */
	uint32_t u4CopySize;
	int u16MetUdpPort;
	ssize_t result;

	IN struct GLUE_INFO *prGlueInfo;

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count :
		     (sizeof(acBuf) - 1);
	result = copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (sscanf(acBuf, " %d", &u16MetUdpPort) == 1)
		DBGLOG(INIT, INFO, "MET_PROF: Write MET PROC UDP_PORT=%d\n",
		       u16MetUdpPort);
	if (pMetGlobalData != NULL) {
		prGlueInfo = (struct GLUE_INFO *) pMetGlobalData;
		prGlueInfo->u2MetUdpPort = (uint16_t) u16MetUdpPort;
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

int kalMetInitProcfs(IN struct GLUE_INFO *prGlueInfo)
{
	/* struct proc_dir_entry *pMetProcDir; */
	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		DBGLOG(INIT, INFO, "init proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}
	/*
	 * Directory: Root (/proc/net/wlan0)
	 */
	pMetProcDir = proc_mkdir("wlan0", init_net.proc_net);
	if (pMetProcDir == NULL)
		return -ENOENT;
	/*
	 *  /proc/net/wlan0
	 *  |-- met_ctrl         (PROC_MET_PROF_CTRL)
	 */
	/* proc_create(PROC_MET_PROF_CTRL, 0x0644, pMetProcDir, &rMetProcFops);
	 */
	proc_create(PROC_MET_PROF_CTRL, 0000, pMetProcDir,
		    &rMetProcCtrlFops);
	proc_create(PROC_MET_PROF_PORT, 0000, pMetProcDir,
		    &rMetProcPortFops);

	pMetGlobalData = (void *)prGlueInfo;

	return 0;
}

int kalMetRemoveProcfs(void)
{

	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		DBGLOG(INIT, WARN,
		       "remove proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}
	remove_proc_entry(PROC_MET_PROF_CTRL, pMetProcDir);
	remove_proc_entry(PROC_MET_PROF_PORT, pMetProcDir);
	/* remove root directory (proc/net/wlan0) */
	remove_proc_entry("wlan0", init_net.proc_net);
	/* clear MetGlobalData */
	pMetGlobalData = NULL;

	return 0;
}

#endif
#if CFG_SUPPORT_AGPS_ASSIST
u_int8_t kalIndicateAgpsNotify(struct ADAPTER *prAdapter,
			       uint8_t cmd, uint8_t *data, uint16_t dataLen)
{
#ifdef CONFIG_NL80211_TESTMODE
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct sk_buff *skb = NULL;

	skb = cfg80211_testmode_alloc_event_skb(priv_to_wiphy(
			prGlueInfo),
						dataLen, GFP_KERNEL);

	/* DBGLOG(CCX, INFO, ("WLAN_STATUS_AGPS_NOTIFY, cmd=%d\n", cmd)); */
	if (unlikely(nla_put(skb, MTK_ATTR_AGPS_CMD, sizeof(cmd),
			     &cmd) < 0))
		goto nla_put_failure;
	if (dataLen > 0 && data
	    && unlikely(nla_put(skb, MTK_ATTR_AGPS_DATA, dataLen,
				data) < 0))
		goto nla_put_failure;
	if (unlikely(nla_put(skb, MTK_ATTR_AGPS_IFINDEX,
	    sizeof(uint32_t), &prGlueInfo->prDevHandler->ifindex) < 0))
		goto nla_put_failure;
	/* currently, the ifname maybe wlan0, p2p0, so the maximum name length
	 * will be 5 bytes
	 */
	if (unlikely(nla_put(skb, MTK_ATTR_AGPS_IFNAME, 5,
			     prGlueInfo->prDevHandler->name) < 0))
		goto nla_put_failure;

	cfg80211_testmode_event(skb, GFP_KERNEL);
	return TRUE;
nla_put_failure:
	kfree_skb(skb);
#else
	DBGLOG(INIT, WARN, "CONFIG_NL80211_TESTMODE not enabled\n");
#endif
	return FALSE;
}
#endif

uint64_t kalGetBootTime(void)
{
	struct timespec ts;
	uint64_t bootTime = 0;

#if KERNEL_VERSION(2, 6, 39) <= LINUX_VERSION_CODE
	get_monotonic_boottime(&ts);
#else
	ts = ktime_to_timespec(ktime_get());
#endif

	bootTime = ts.tv_sec;
	bootTime *= USEC_PER_SEC;
	bootTime += ts.tv_nsec / NSEC_PER_USEC;
	return bootTime;
}

#if CFG_ASSERT_DUMP
uint32_t kalOpenCorDumpFile(u_int8_t fgIsN9)
{
	/* Move open-op to kalWriteCorDumpFile(). Empty files only */
	uint32_t ret;
	uint8_t *apucFileName;

	if (fgIsN9)
		apucFileName = apucCorDumpN9FileName;
	else
		apucFileName = apucCorDumpCr4FileName;

	ret = kalTrunkPath(apucFileName);

	return (ret >= 0)?WLAN_STATUS_SUCCESS:WLAN_STATUS_FAILURE;
}

uint32_t kalWriteCorDumpFile(uint8_t *pucBuffer,
			     uint16_t u2Size, u_int8_t fgIsN9)
{
	uint32_t ret;
	uint8_t *apucFileName;

	if (fgIsN9)
		apucFileName = apucCorDumpN9FileName;
	else
		apucFileName = apucCorDumpCr4FileName;

	ret = kalWriteToFile(apucFileName, TRUE, pucBuffer, u2Size);

	return (ret >= 0)?WLAN_STATUS_SUCCESS:WLAN_STATUS_FAILURE;
}

uint32_t kalCloseCorDumpFile(u_int8_t fgIsN9)
{
	/* Move close-op to kalWriteCorDumpFile(). Do nothing here */

	return WLAN_STATUS_SUCCESS;
}
#endif

#if CFG_WOW_SUPPORT
void kalWowInit(IN struct GLUE_INFO *prGlueInfo)
{
	kalMemZero(&prGlueInfo->prAdapter->rWowCtrl.stWowPort,
		   sizeof(struct WOW_PORT));
	prGlueInfo->prAdapter->rWowCtrl.ucReason = INVALID_WOW_WAKE_UP_REASON;

	prGlueInfo->prAdapter->mdns_offload_enable = FALSE;
}

void kalWowCmdEventSetCb(IN struct ADAPTER *prAdapter,
			IN struct CMD_INFO *prCmdInfo,IN uint8_t *pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);


	if (prCmdInfo->ucCID == CMD_ID_SET_PF_CAPABILITY) {
		DBGLOG(INIT, STATE, "CMD_ID_SET_PF_CAPABILITY cmd done\n");
		prAdapter->fgSetPfCapabilityDone = TRUE;
	}

	if (prCmdInfo->ucCID == CMD_ID_SET_WOWLAN) {
		DBGLOG(INIT, STATE, "CMD_ID_SET_WOWLAN cmd done\n");
		prAdapter->fgSetWowDone = TRUE;
	}

}

void kalWowProcess(IN struct GLUE_INFO *prGlueInfo,
		   uint8_t enable)
{
	struct CMD_WOWLAN_PARAM rCmdWowlanParam;
	struct CMD_PACKET_FILTER_CAP rCmdPacket_Filter_Cap;
	struct WOW_CTRL *pWOW_CTRL =
			&prGlueInfo->prAdapter->rWowCtrl;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t ii, wait = 0;
	bool bWake = TRUE; /* to wake up host or just keep connection */

	kalMemZero(&rCmdWowlanParam,
		   sizeof(struct CMD_WOWLAN_PARAM));

	kalMemZero(&rCmdPacket_Filter_Cap,
		   sizeof(struct CMD_PACKET_FILTER_CAP));

	prGlueInfo->prAdapter->fgSetPfCapabilityDone = FALSE;
	prGlueInfo->prAdapter->fgSetWowDone = FALSE;

	if (prGlueInfo->prAdapter->rWifiVar.ucAdvPws == 1
		&& prGlueInfo->prAdapter->rWowCtrl.fgWowEnable == 0)
		bWake = FALSE;

	/* add mdns offload */
	if (enable)
		DBGLOG(PF, STATE, "Suspend: mDNS flag:%d\n",
			prGlueInfo->prAdapter->mdns_offload_enable);

	if (enable && prGlueInfo->prAdapter->mdns_offload_enable) {
		kalSendDelMdnsCacheToFw(prGlueInfo);
		kalSendAddMdnsCacheToFw(prGlueInfo);
	}

	DBGLOG(PF, INFO,
	       "PF, pAd ucBssIndex=%d, ucOwnMacIndex=%d\n",
	       prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex,
	       prGlueInfo->prAdapter->prAisBssInfo->ucOwnMacIndex);

	DBGLOG(PF, INFO, "profile wow=%d, bWake=%d, GpioInterval=%d\n",
	       prGlueInfo->prAdapter->rWifiVar.ucWow, bWake,
	       prGlueInfo->prAdapter->rWowCtrl.astWakeHif[0].u4GpioInterval);

/* Add this step in 0x4A wow command, then we can save 0x59 command */
#if 0
	rCmdPacket_Filter_Cap.packet_cap_type |=
		PACKETF_CAP_TYPE_MAGIC;
	/* 20160627 Bennett: if receive BMC magic, PF search by bssid index,
	 * which is different with OM index
	 */
	/* After discussion, enable all bssid bits */
	/* rCmdPacket_Filter_Cap.ucBssid |=
	 *		BIT(prGlueInfo->prAdapter->prAisBssInfo->ucOwnMacIndex);
	 */
	rCmdPacket_Filter_Cap.ucBssid |= BITS(0, 3);

	if (enable && bWake)
		rCmdPacket_Filter_Cap.usEnableBits |=
			PACKETF_CAP_TYPE_MAGIC;
	else
		rCmdPacket_Filter_Cap.usEnableBits &=
			~PACKETF_CAP_TYPE_MAGIC;

	rStatus = wlanSendSetQueryCmd(prGlueInfo->prAdapter,
				      CMD_ID_SET_PF_CAPABILITY,
				      TRUE,
				      FALSE,
				      FALSE,
				      kalWowCmdEventSetCb,
				      nicOidCmdTimeoutCommon,
				      sizeof(struct CMD_PACKET_FILTER_CAP),
				      (uint8_t *)&rCmdPacket_Filter_Cap,
				      NULL,
				      0);
#endif

	/* Set bssid bits in wowlan cmd to replace CMD_ID_SET_PF_CAPABILITY */
	/* After discussion, enable all bssid bits */
	rCmdWowlanParam.ucBssid |= BITS(0, 3);

	/* Let WOW enable/disable as last command, so we can back/restore DMA
	 * classify filter in FW
	 */
	rCmdWowlanParam.ucScenarioID = pWOW_CTRL->ucScenarioId;
	rCmdWowlanParam.ucBlockCount = pWOW_CTRL->ucBlockCount;
	rCmdWowlanParam.astWakeHif[0].ucWakeupHif =
		pWOW_CTRL->astWakeHif[0].ucWakeupHif;
	rCmdWowlanParam.astWakeHif[0].ucGpioPin =
		pWOW_CTRL->astWakeHif[0].ucGpioPin;
	rCmdWowlanParam.astWakeHif[0].ucTriggerLvl =
		pWOW_CTRL->astWakeHif[0].ucTriggerLvl;
	rCmdWowlanParam.astWakeHif[0].u4GpioInterval =
		pWOW_CTRL->astWakeHif[0].u4GpioInterval;

	DBGLOG(PF, INFO,
	       "Test XX source: IPV4/UDP=%d, IPV4/TCP=%d, IPV6/UDP=%d, IPV6/TCP=%d\n",
	       pWOW_CTRL->stWowPort.ucIPv4UdpPortCnt,
	       pWOW_CTRL->stWowPort.ucIPv4TcpPortCnt,
	       pWOW_CTRL->stWowPort.ucIPv6UdpPortCnt,
	       pWOW_CTRL->stWowPort.ucIPv6TcpPortCnt);

	/* copy UDP/TCP port setting */
	if (bWake)
		kalMemCopy(&rCmdWowlanParam.stWowPort,
			&prGlueInfo->prAdapter->rWowCtrl.stWowPort,
			sizeof(struct WOW_PORT));
	else
		kalMemZero(&prGlueInfo->prAdapter->rWowCtrl.stWowPort,
			sizeof(struct WOW_PORT));

	DBGLOG(PF, INFO,
	       "Cmd: IPV4/UDP=%d, IPV4/TCP=%d, IPV6/UDP=%d, IPV6/TCP=%d\n",
	       rCmdWowlanParam.stWowPort.ucIPv4UdpPortCnt,
	       rCmdWowlanParam.stWowPort.ucIPv4TcpPortCnt,
	       rCmdWowlanParam.stWowPort.ucIPv6UdpPortCnt,
	       rCmdWowlanParam.stWowPort.ucIPv6TcpPortCnt);

	for (ii = 0;
	     ii < rCmdWowlanParam.stWowPort.ucIPv4UdpPortCnt; ii++)
		DBGLOG(PF, INFO, "IPV4/UDP port[%d]=%d\n", ii,
		       rCmdWowlanParam.stWowPort.ausIPv4UdpPort[ii]);

	for (ii = 0;
	     ii < rCmdWowlanParam.stWowPort.ucIPv4TcpPortCnt; ii++)
		DBGLOG(PF, INFO, "IPV4/TCP port[%d]=%d\n", ii,
		       rCmdWowlanParam.stWowPort.ausIPv4TcpPort[ii]);

	for (ii = 0;
	     ii < rCmdWowlanParam.stWowPort.ucIPv6UdpPortCnt; ii++)
		DBGLOG(PF, INFO, "IPV6/UDP port[%d]=%d\n", ii,
		       rCmdWowlanParam.stWowPort.ausIPv6UdpPort[ii]);

	for (ii = 0;
	     ii < rCmdWowlanParam.stWowPort.ucIPv6TcpPortCnt; ii++)
		DBGLOG(PF, INFO, "IPV6/TCP port[%d]=%d\n", ii,
		       rCmdWowlanParam.stWowPort.ausIPv6TcpPort[ii]);


	/* GPIO parameter is necessary in suspend/resume */
	if (enable == 1) {
		rCmdWowlanParam.ucCmd = PM_WOWLAN_REQ_START;
		rCmdWowlanParam.ucDetectType = WOWLAN_DETECT_TYPE_MAGIC;
		rCmdWowlanParam.u2FilterFlag = WOWLAN_FF_DROP_ALL |
				       WOWLAN_FF_SEND_MAGIC_TO_HOST |
				       WOWLAN_FF_ALLOW_1X |
				       WOWLAN_FF_ALLOW_ARP_REQ2ME;
		if (!bWake)
			rCmdWowlanParam.ucDetectType = WOWLAN_DETECT_TYPE_NONE;

	} else {
		rCmdWowlanParam.ucCmd = PM_WOWLAN_REQ_STOP;
	}

	rStatus = wlanSendSetQueryCmd(prGlueInfo->prAdapter,
					CMD_ID_SET_WOWLAN,
					TRUE,
					FALSE,
					FALSE,
					kalWowCmdEventSetCb,
					nicOidCmdTimeoutCommon,
					sizeof(struct CMD_WOWLAN_PARAM),
					(uint8_t *)&rCmdWowlanParam,
					NULL,
					0);


	while (1) {
		kalUdelay(200);
		schedule();

		if (wait > 2500) {
			DBGLOG(INIT, ERROR, "WoW process timeout.PF:%d. WoW:%d\n",
				prGlueInfo->prAdapter->fgSetPfCapabilityDone,
				prGlueInfo->prAdapter->fgSetWowDone);
			break;
		}
		if (prGlueInfo->prAdapter->fgSetWowDone == TRUE) {
			DBGLOG(INIT, STATE, "WoW process done\n");
			break;
		}

		wait++;
	}

	/* ARP offload. move to last command */
	wlanSetSuspendMode(prGlueInfo, enable);

	wlanSendDummyCmd(prGlueInfo->prAdapter, TRUE);

}

struct MDNS_Template_Record gRspCacheRecord[MAX_MDNS_CACHE_NUM];

uint8_t gOldestIndex;

uint16_t kalMdsnParseName(uint8_t *pucMdnsHdr, uint16_t offset,
			uint8_t *name, uint16_t *name_len)
{
	uint16_t pos = 0;
	uint16_t i = 0;
	bool compress = FALSE;
	uint8_t len = 0;
	uint8_t limit = MDNS_NAME_MAX_LEN - 1;

	/* parse compress name */
	/* there may be compress name contain compress name */
	pos = offset;
	while (pucMdnsHdr[offset] != 0) {
		len = pucMdnsHdr[offset];
		switch (len & 0xc0) {
		case 0x00:
			/* uncompress code */
			if ((*name_len + len) > limit) {
				*name_len = 0;
				return 0;
			}

			for (i = 0; i <= len; i++) {
				name[*name_len] = pucMdnsHdr[offset + i];
				*name_len += 1;
			}
			offset += len + 1;
			if (compress == FALSE)
				pos = offset;
			break;

		case 0x40:
		case 0x80:
			*name_len = 0;
			return 0;

		case 0xc0:
			if (compress == FALSE) {
				pos = offset;
				compress = TRUE;
			}
			DBGLOG(PF, LOUD, "0x%02x 0x%02x.\n",
				pucMdnsHdr[offset], pucMdnsHdr[offset + 1]);
			offset = ((pucMdnsHdr[offset] & 0x3f) << 8)
						| pucMdnsHdr[offset + 1];
			break;
		}
	}

	/* for compress, c0xx 2byes, for uncompress 0 as the end of the name */
	if (compress == TRUE)
		pos += 2;
	else
		pos += 1;

	*name_len += 1;

	return pos;
}

bool kalParseMdnsRespPkt(uint8_t *pucMdnsHdr,
			struct MDNS_RESP_INFO_T *resp)
{
	uint16_t pos;
	uint8_t *ip;

	pos = 0;
	resp->usMdnsId = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->usMdnsFlags = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->usQuestionCnt = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->usAnswerCnt = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->usAuthCnt = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->usAddtionCnt = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	DBGLOG(SW4, LOUD, "MDNS PKT[ID %d] flags[0x%x]\n",
			resp->usMdnsId, resp->usMdnsFlags);
	DBGLOG(SW4, LOUD, "questionCnt[%d] answerCnt[%d]\n",
			resp->usQuestionCnt, resp->usAnswerCnt);
	DBGLOG(SW4, LOUD, "authcnt[%d] addCnt[%d]\n",
			resp->usAuthCnt, resp->usAddtionCnt);

	if ((resp->usMdnsFlags & 0x8000) == 0) {
		DBGLOG(SW4, LOUD, "MDNS is query packet\n");
		return FALSE;
	}

	if ((resp->usAnswerCnt == 1 && resp->usAddtionCnt == 3)
		|| (resp->usAnswerCnt == 4 && resp->usAddtionCnt == 0)) {
		DBGLOG(SW4, LOUD, "MDNS ans %d add %d\n",
			resp->usAnswerCnt, resp->usAddtionCnt);
	} else {
		DBGLOG(SW4, LOUD, "MDNS ans %d add %d\n",
			resp->usAnswerCnt, resp->usAddtionCnt);
		return FALSE;
	}

	/* parse ptr info */
	pos += 2;
	pos = kalMdsnParseName(pucMdnsHdr, pos,
						resp->ptrInfo.queryName,
						&resp->ptrInfo.queryNameLen);

	DBGLOG(SW4, LOUD, "qurey name: %s\n",
			resp->ptrInfo.queryName);
	DBGLOG(SW4, LOUD, "qurey name len: %d pos %d\n",
			resp->ptrInfo.queryNameLen, pos);

	resp->ptrInfo.type = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));
	if (resp->ptrInfo.type != 12) {
		DBGLOG(SW4, LOUD, "not ptr type %d\n",
				resp->ptrInfo.type);
		return FALSE;
	}

	pos += 2;
	resp->ptrInfo.cl = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->ptrInfo.ttl = NTOHL(*(uint32_t *)(pucMdnsHdr + pos));

	pos += 4;
	resp->ptrInfo.dataLen = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	DBGLOG(SW4, LOUD, "ptr type %d class %d ttl %d datalen %d\n",
			resp->ptrInfo.type, resp->ptrInfo.cl,
			resp->ptrInfo.ttl, resp->ptrInfo.dataLen);

	pos += 2;
	pos = kalMdsnParseName(pucMdnsHdr, pos,
						resp->ptrInfo.domName,
						&resp->ptrInfo.domNameLen);

	DBGLOG(SW4, LOUD, "ptr domain name: %s\n", resp->ptrInfo.domName);

	/* parse compress name, need update the datalen */
	resp->ptrInfo.dataLen = resp->ptrInfo.domNameLen;

	/* parse txt info */
	pos = kalMdsnParseName(pucMdnsHdr, pos, resp->txtInfo.domName,
					&resp->txtInfo.domNameLen);

	DBGLOG(SW4, LOUD, "txt domain name: %s\n", resp->txtInfo.domName);

	resp->txtInfo.type = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));
	if (resp->txtInfo.type != 16) {
		DBGLOG(SW4, LOUD, "not txt type %d\n", resp->txtInfo.type);
		return FALSE;
	}

	pos += 2;
	resp->txtInfo.cl = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->txtInfo.ttl = NTOHL(*(uint32_t *)(pucMdnsHdr + pos));

	pos += 4;
	resp->txtInfo.dataLen = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	DBGLOG(SW4, LOUD, "txt type %d class %d ttl %d datalen %d\n",
			resp->txtInfo.type, resp->txtInfo.cl,
			resp->txtInfo.ttl, resp->txtInfo.dataLen);

	pos += 2;
	kalMemCopy(resp->txtInfo.text,
				pucMdnsHdr + pos,
				resp->txtInfo.dataLen);

	DBGLOG(SW4, LOUD, "txt text: %s\n", resp->txtInfo.text);

	/* parse srv info */
	pos += resp->txtInfo.dataLen;
	pos = kalMdsnParseName(pucMdnsHdr, pos, resp->srvInfo.domName,
						&resp->srvInfo.domNameLen);

	DBGLOG(SW4, LOUD, "srv domain name: %s\n", resp->srvInfo.domName);

	resp->srvInfo.type = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));
	if (resp->srvInfo.type != 33) {
		DBGLOG(SW4, LOUD, "not srv type %d\n", resp->srvInfo.type);
		return FALSE;
	}

	pos += 2;
	resp->srvInfo.cl = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->srvInfo.ttl = NTOHL(*(uint32_t *)(pucMdnsHdr + pos));

	pos += 4;
	resp->srvInfo.dataLen = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->srvInfo.priority = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->srvInfo.weight = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->srvInfo.port = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	DBGLOG(SW4, LOUD, "srv type %d class %d ttl %d datalen %d.\n",
			resp->srvInfo.type, resp->srvInfo.cl,
			resp->srvInfo.ttl, resp->srvInfo.dataLen);

	DBGLOG(SW4, LOUD, "srv pri %d weight %d port %d.\n",
			resp->srvInfo.priority, resp->srvInfo.weight,
			resp->srvInfo.port);

	pos += 2;
	pos = kalMdsnParseName(pucMdnsHdr, pos, resp->srvInfo.target,
					&resp->srvInfo.targetLen);

	DBGLOG(SW4, LOUD, "srv target name: %s\n", resp->srvInfo.target);

	/* parse compress name, need update the datalen */
	resp->srvInfo.dataLen = 6 + resp->srvInfo.targetLen;

	/* parse a info */
	pos = kalMdsnParseName(pucMdnsHdr, pos, resp->aInfo.domName,
						&resp->aInfo.domNameLen);

	DBGLOG(SW4, LOUD, "a domain name: %s.\n", resp->aInfo.domName);

	resp->aInfo.type =  NTOHS(*(uint16_t *)(pucMdnsHdr + pos));
	if (resp->aInfo.type != 1) {
		DBGLOG(SW4, LOUD, "not a type %d.\n", resp->aInfo.type);
		return FALSE;
	}

	pos += 2;
	resp->aInfo.cl = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->aInfo.ttl = NTOHL(*(uint32_t *)(pucMdnsHdr + pos));

	pos += 4;
	resp->aInfo.dataLen = NTOHS(*(uint16_t *)(pucMdnsHdr + pos));

	pos += 2;
	resp->aInfo.ipAddr = NTOHL(*(uint32_t *)(pucMdnsHdr + pos));

	DBGLOG(SW4, LOUD, "a type %d class %d ttl %d datalen %d.\n",
			resp->aInfo.type, resp->aInfo.cl,
			resp->aInfo.ttl, resp->aInfo.dataLen);

	ip = (uint8_t *)&(resp->aInfo.ipAddr);
	DBGLOG(SW4, LOUD, "a host ip [%d.%d.%d.%d].\n",
			ip[0], ip[1], ip[2], ip[3]);

	return TRUE;
}

void kalPickMdnsParamFromResp(struct MDNS_RESP_INFO_T *respInfo,
		struct MDNS_Template_Record *respTemplate)
{
	uint16_t offset;
	struct MDNS_ANSWER_RR *prMdnsAnswerR = NULL;
	struct MDNS_TXT_RR *prMdnsTxtR = NULL;
	struct MDNS_SRV_RR *prMdnsSrvR = NULL;
	struct MDNS_QUESTION *prMdnsQuestion = NULL;
	struct MDNS_A_RR *prMdnsAR = NULL;

	prMdnsQuestion = &respTemplate->mdnsQuestionTemplate;

	/* store query info */
	kalMemCopy(prMdnsQuestion->name,
				respInfo->ptrInfo.queryName,
				MDNS_NAME_MAX_LEN);
	prMdnsQuestion->name_length = respInfo->ptrInfo.queryNameLen;
	prMdnsQuestion->type = respInfo->ptrInfo.type;
	prMdnsQuestion->class = respInfo->ptrInfo.cl;

	/* store answer RR */
	prMdnsAnswerR = &respTemplate->mdnsResponseRecord;
	offset = 0;
	kalMemCopy(&prMdnsAnswerR->data[offset],
				respInfo->ptrInfo.queryName,
				respInfo->ptrInfo.queryNameLen);

	offset += respInfo->ptrInfo.queryNameLen;
	*(uint16_t *)(&prMdnsAnswerR->data[offset])
					= HTONS(respInfo->ptrInfo.type);

	offset += sizeof(uint16_t);
	*(uint16_t *)(&prMdnsAnswerR->data[offset])
					= HTONS(respInfo->ptrInfo.cl);

	offset += sizeof(uint16_t);
	*(uint32_t *)(&prMdnsAnswerR->data[offset])
					= HTONL(respInfo->ptrInfo.ttl);

	offset += sizeof(uint32_t);
	*(uint16_t *)(&prMdnsAnswerR->data[offset])
					= HTONS(respInfo->ptrInfo.dataLen);

	offset += sizeof(uint16_t);
	kalMemCopy(&prMdnsAnswerR->data[offset],
				respInfo->ptrInfo.domName,
				respInfo->ptrInfo.domNameLen);
	offset += respInfo->ptrInfo.domNameLen;
	prMdnsAnswerR->data_length = offset;

	/* store server name (domain name) */
	kalMemCopy(respTemplate->ucServiceName,
				respInfo->ptrInfo.domName,
				respInfo->ptrInfo.domNameLen);
	respTemplate->ServiceNameLen = respInfo->ptrInfo.domNameLen;

	/* store SRV info */
	offset = 0;
	prMdnsSrvR = &respTemplate->mdnsSrvRecord;
	*(uint16_t *)(&prMdnsSrvR->data[offset])
					= HTONS(respInfo->srvInfo.type);

	offset += sizeof(uint16_t);
	*(uint16_t *)(&prMdnsSrvR->data[offset]) = HTONS(respInfo->srvInfo.cl);

	offset += sizeof(uint16_t);
	*(uint32_t *)(&prMdnsSrvR->data[offset]) = HTONL(respInfo->srvInfo.ttl);

	offset += sizeof(uint32_t);
	/* use target */
	*(uint16_t *)(&prMdnsSrvR->data[offset])
				= HTONS(respInfo->srvInfo.targetLen + 6);

	offset += sizeof(uint16_t);
	*(uint16_t *)(&prMdnsSrvR->data[offset])
				= HTONS(respInfo->srvInfo.priority);

	offset += sizeof(uint16_t);
	*(uint16_t *)(&prMdnsSrvR->data[offset])
				= HTONS(respInfo->srvInfo.weight);

	offset += sizeof(uint16_t);
	*(uint16_t *)(&prMdnsSrvR->data[offset])
				= HTONS(respInfo->srvInfo.port);

	offset += sizeof(uint16_t);
	prMdnsSrvR->data_length = offset;

	/* store TXT info */
	offset = 0;
	prMdnsTxtR = &respTemplate->mdnsTxtRecord;
	*(uint16_t *)(&prMdnsTxtR->data[offset])
				= HTONS(respInfo->txtInfo.type);

	offset += sizeof(uint16_t);
	*(uint16_t *)(&prMdnsTxtR->data[offset]) = HTONS(respInfo->txtInfo.cl);

	offset += sizeof(uint16_t);
	*(uint32_t *)(&prMdnsTxtR->data[offset]) = HTONL(respInfo->txtInfo.ttl);

	offset += sizeof(uint32_t);
	*(uint16_t *)(&prMdnsTxtR->data[offset])
				= HTONS(respInfo->txtInfo.dataLen);

	offset += sizeof(uint16_t);
	kalMemCopy(&prMdnsTxtR->data[offset],
				respInfo->txtInfo.text,
				respInfo->txtInfo.dataLen);
	offset += respInfo->txtInfo.dataLen;
	prMdnsTxtR->data_length = offset;

	/* store target name */
	kalMemCopy(respTemplate->ucTargetName,
				respInfo->srvInfo.target,
				respInfo->srvInfo.targetLen);
	respTemplate->TargetNameLen = respInfo->srvInfo.targetLen;

	/* store A info */
	offset = 0;
	prMdnsAR = &respTemplate->mdnsARecord;

	*(uint16_t *)(&prMdnsAR->data[offset])
				= HTONS(respInfo->aInfo.type);

	offset += sizeof(uint16_t);
	*(uint16_t *)(&prMdnsAR->data[offset]) = HTONS(respInfo->aInfo.cl);

	offset += sizeof(uint16_t);
	*(uint32_t *)(&prMdnsAR->data[offset]) = HTONL(respInfo->aInfo.ttl);

	offset += sizeof(uint32_t);
	*(uint16_t *)(&prMdnsAR->data[offset])
				= HTONS(respInfo->aInfo.dataLen);

	offset += sizeof(uint16_t);
	*(uint32_t *)(&prMdnsAR->data[offset]) = HTONL(respInfo->aInfo.ipAddr);
	offset += respInfo->aInfo.dataLen;
	prMdnsAR->data_length = offset;
}

void kalSendMdnsEventCb(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);


	if (prCmdInfo->ucCID == CMD_ID_SET_MDNS_RECORD) {
		DBGLOG(INIT, LOUD, "CMD_ID_SET_MDNS_RECORD cmd submitted\n");
		prAdapter->fgSetMdnsDone = TRUE;
	}
}

void kalSendDelMdnsCacheToFw(struct GLUE_INFO *prGlueInfo)
{
	struct CMD_MDNS_PARAM_T *cmdMdnsParam;
	uint8_t i;
	uint32_t wait;

	cmdMdnsParam = (struct CMD_MDNS_PARAM_T *)kalMemAlloc(
			sizeof(struct CMD_MDNS_PARAM_T), VIR_MEM_TYPE);
	if (!cmdMdnsParam)
		return;

	for (i = 0; i < MAX_MDNS_CACHE_NUM; i++) {
		kalMemZero(cmdMdnsParam, sizeof(struct CMD_MDNS_PARAM_T));

		cmdMdnsParam->ucCmd = MDNS_CMD_DEL_RECORD;
		cmdMdnsParam->u4RecordId = i;

		prGlueInfo->prAdapter->fgSetMdnsDone = FALSE;

		wlanSendSetQueryCmd(prGlueInfo->prAdapter,
				CMD_ID_SET_MDNS_RECORD,
				TRUE,
				FALSE,
				FALSE,
				kalSendMdnsEventCb,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_MDNS_PARAM_T),
				(uint8_t *)cmdMdnsParam,
				NULL,
				0);

		wait = 0;
		while (1) {
			kalMsleep(2);

			if (wait > 100) {
				DBGLOG(INIT, ERROR, "WoW process timeout\n\n");
				break;
			}
			if (prGlueInfo->prAdapter->fgSetMdnsDone == TRUE) {
				DBGLOG(INIT, LOUD,
					"send mdns record [%d] done\n", i);
				break;
			}
			wait++;
		}
	}

	kalMemFree(cmdMdnsParam, VIR_MEM_TYPE,
		   sizeof(struct CMD_MDNS_PARAM_T));
}

void kalSendAddMdnsCacheToFw(struct GLUE_INFO *prGlueInfo)
{
	struct CMD_MDNS_PARAM_T *cmdMdnsParam;
	uint8_t i;
	uint32_t wait;

	cmdMdnsParam = (struct CMD_MDNS_PARAM_T *)kalMemAlloc(
			sizeof(struct CMD_MDNS_PARAM_T), VIR_MEM_TYPE);
	if (!cmdMdnsParam)
		return;

	for (i = 0; i < MAX_MDNS_CACHE_NUM; i++) {
		if (gRspCacheRecord[i].bIsUsed == 0) {
			DBGLOG(SW4, LOUD, "null cache, don't send to fw.\n");
			continue;
		}

		kalMemZero(cmdMdnsParam, sizeof(struct CMD_MDNS_PARAM_T));

		prGlueInfo->prAdapter->fgSetMdnsDone = FALSE;

		cmdMdnsParam->ucCmd = MDNS_CMD_ADD_RECORD;
		cmdMdnsParam->u4RecordId = gRspCacheRecord[i].u4RecordId;
		kalMemCopy(&cmdMdnsParam->mdnsQueryRespTemplate,
				&gRspCacheRecord[i],
				sizeof(struct MDNS_Template_Record));
		DBGLOG(SW4, STATE, "mDNS cache: %d, recordid %d %s.\n",
			i, gRspCacheRecord[i].u4RecordId,
			gRspCacheRecord[i].mdnsQuestionTemplate.name);

		wlanSendSetQueryCmd(prGlueInfo->prAdapter,
				CMD_ID_SET_MDNS_RECORD,
				TRUE,
				FALSE,
				FALSE,
				kalSendMdnsEventCb,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_MDNS_PARAM_T),
				(uint8_t *)cmdMdnsParam,
				NULL,
				0);

		wait = 0;
		while (1) {
			kalMsleep(2);

			if (wait > 100) {
				DBGLOG(INIT, ERROR,
					"WoW process timeout\n\n");
				break;
			}
			if (prGlueInfo->prAdapter->fgSetMdnsDone == TRUE) {
				DBGLOG(INIT, LOUD,
					"send mdns record [%d] done\n", i);
				break;
			}
			wait++;
		}
	}

	kalMemFree(cmdMdnsParam, VIR_MEM_TYPE,
		sizeof(struct CMD_MDNS_PARAM_T));
}

void kalShowMdnsCache(void)
{
	uint8_t i;

	for (i = 0; i < MAX_MDNS_CACHE_NUM; i++) {
		DBGLOG(SW4, STATE, "mDNS cache: %d, %d [%s] [%s].\n",
			i, gRspCacheRecord[i].u4RecordId,
			gRspCacheRecord[i].mdnsQuestionTemplate.name,
			gRspCacheRecord[i].ucServiceName);
	}
}

void kalSendMdnsEnableToFw(struct GLUE_INFO *prGlueInfo)
{
	struct CMD_MDNS_PARAM_T *cmdMdnsParam;
	uint8_t *pucMdnsARecord;
	struct WLAN_MAC_HEADER_QoS_T *prMacHeader;
	uint8_t *pucIPHeader;
	uint8_t *pucUDPHeader;

	cmdMdnsParam = (struct CMD_MDNS_PARAM_T *)kalMemAlloc(
			sizeof(struct CMD_MDNS_PARAM_T), VIR_MEM_TYPE);
	if (!cmdMdnsParam)
		return;

	kalMemZero(cmdMdnsParam, sizeof(struct CMD_MDNS_PARAM_T));

	cmdMdnsParam->ucCmd = MDNS_CMD_ENABLE;

	DBGLOG(SW4, ERROR, "mDNS Enable.\n");
	/* Store 802.11 MAC header.*/

	prMacHeader = &cmdMdnsParam->aucMdnsMacHdr;
	prMacHeader->u2FrameCtrl = MAC_FRAME_QOS_DATA;
	prMacHeader->u2FrameCtrl |= MASK_FC_TO_DS;
	prMacHeader->u2FrameCtrl |= MASK_FC_PWR_MGT;

	/* SA, DA.*/
	COPY_MAC_ADDR(prMacHeader->aucAddr2,
					prGlueInfo->prDevHandler->dev_addr);
	prMacHeader->aucAddr3[0] = 1;
	prMacHeader->aucAddr3[1] = 0;
	prMacHeader->aucAddr3[2] = 0x5E;
	prMacHeader->aucAddr3[3] = 0;
	prMacHeader->aucAddr3[4] = 0;
	prMacHeader->aucAddr3[5] = 0xFB;

	prMacHeader->u2SeqCtrl = 0;

	prMacHeader->u2QosCtrl = 0;
	prMacHeader->u2QosCtrl |=
					(ACK_POLICY_NORMAL_ACK_IMPLICIT_BA_REQ
					<< MASK_QC_ACK_POLICY_OFFSET);

	/* IPv4 header.*/
	pucIPHeader = &cmdMdnsParam->aucMdnsIPHdr[0];
	/* Version: 4, Length:(5*4) */
	pucIPHeader[0] = 4<<4 | 5;
	/* Diff.Services */
	pucIPHeader[1] = 0;
	/* IP length:
	*(IPV4_HEADER_LENGTH + UDP_HEADER_LENGTH + u4MdnsLen)
	*/
	pucIPHeader[2] = 0;
	pucIPHeader[3] = 0;
	/* Identifier.*/
	pucIPHeader[4] = 0;
	pucIPHeader[5] = 0;
	/* Fragmentation Flags, Mask 0xE0.*/
	pucIPHeader[6] = 0;
	pucIPHeader[6] = 1 << 6;
	/* Fragment Offset, [6,7]Mask 0x1FFF.*/
	pucIPHeader[7] = 0;
	/* TTL.*/
	pucIPHeader[8] = 255;
	/* Protocol, UDP.*/
	pucIPHeader[9] = 17;
	/* Header Checksum.*/
	pucIPHeader[10] = 0;
	pucIPHeader[11] = 0;
	/* Source IP: FW could check and update SIP again.*/
	pucIPHeader[12] = 0;
	pucIPHeader[13] = 0;
	pucIPHeader[14] = 0;
	pucIPHeader[15] = 0;
	/* DIP:MC IP(224.0.0.251) */
	pucIPHeader[16] = 224;
	pucIPHeader[17] = 0;
	pucIPHeader[18] = 0;
	pucIPHeader[19] = 251;

	/*UDP header*/
	pucUDPHeader = &cmdMdnsParam->aucMdnsUdpHdr[0];
	/* Source port: 5353 */
	pucUDPHeader[0] = (5353 & 0xff00) >> 8;
	pucUDPHeader[1] = 5353 & 0x00ff;
	/* Destination port: 5353 */
	pucUDPHeader[2] = (5353 & 0xff00) >> 8;
	pucUDPHeader[3] = 5353 & 0x00ff;
	/* UDP length: Calculate by FW*/
	pucUDPHeader[4] = 0;
	pucUDPHeader[5] = 0;
	/* UDP CheckSum: Calculate by FW*/
	pucUDPHeader[6] = 0;
	pucUDPHeader[7] = 0;

	/* Store A RR.*/
	pucMdnsARecord = &cmdMdnsParam->mdnsARecord[0];
	/* Type.*/
	pucMdnsARecord[0] = 0;
	pucMdnsARecord[1] = 1;
	/* Class.*/
	pucMdnsARecord[2] = (32769 & 0xff00) >> 8;
	pucMdnsARecord[3] = 32769 & 0x00ff;
	/* TTL.*/
	pucMdnsARecord[4] = 0;
	pucMdnsARecord[5] = 0;
	pucMdnsARecord[6] = (4500 & 0xff00) >> 8;
	pucMdnsARecord[7] = 4500 & 0x00ff;
	/* Data Length.*/
	pucMdnsARecord[8] = 0;
	pucMdnsARecord[9] = 4;
	/* SIP Address.*/
	pucMdnsARecord[10] = 0;
	pucMdnsARecord[11] = 0;
	pucMdnsARecord[12] = 0;
	pucMdnsARecord[13] = 0;

	wlanSendSetQueryCmd(prGlueInfo->prAdapter,
				CMD_ID_SET_MDNS_RECORD,
				TRUE,
				FALSE,
				FALSE,
				NULL,
				NULL,
				sizeof(struct CMD_MDNS_PARAM_T),
				(uint8_t *)cmdMdnsParam,
				NULL,
				0);

	kalMemFree(cmdMdnsParam, VIR_MEM_TYPE,
		sizeof(struct CMD_MDNS_PARAM_T));
}

void kalSendMdnsDisableToFw(struct GLUE_INFO *prGlueInfo)
{
	struct CMD_MDNS_PARAM_T *cmdMdnsParam;

	cmdMdnsParam = (struct CMD_MDNS_PARAM_T *)kalMemAlloc(
			sizeof(struct CMD_MDNS_PARAM_T), VIR_MEM_TYPE);

	if (!cmdMdnsParam)
		return;

	kalMemZero(cmdMdnsParam, sizeof(struct CMD_MDNS_PARAM_T));

	cmdMdnsParam->ucCmd = MDNS_CMD_DISABLE;

	DBGLOG(SW4, ERROR, "mDNS Disable.\n");

	wlanSendSetQueryCmd(prGlueInfo->prAdapter,
				CMD_ID_SET_MDNS_RECORD,
				TRUE,
				FALSE,
				FALSE,
				NULL,
				NULL,
				sizeof(struct CMD_MDNS_PARAM_T),
				(uint8_t *)cmdMdnsParam,
				NULL,
				0);

	kalMemFree(cmdMdnsParam, VIR_MEM_TYPE,
		sizeof(struct CMD_MDNS_PARAM_T));
}

uint8_t kalAddMdnsCache(struct MDNS_RESP_INFO_T *respInfo)
{
	uint8_t i;
	bool flag = FALSE;
	struct MDNS_Template_Record *respTemplate;

	respTemplate = (struct MDNS_Template_Record *)kalMemAlloc(
			sizeof(struct MDNS_Template_Record), PHY_MEM_TYPE);

	if (respTemplate == NULL) {
		DBGLOG(SW4, ERROR, "kmalloc failed.\n");
		return 1;
	}

	kalMemZero(respTemplate, sizeof(struct MDNS_Template_Record));
	kalPickMdnsParamFromResp(respInfo, respTemplate);

	for (i = 0; i < MAX_MDNS_CACHE_NUM; i++) {
		/* if query name is same, newer one replace the older */
		if (kalMemCmp(&respTemplate->mdnsQuestionTemplate,
				&gRspCacheRecord[i].mdnsQuestionTemplate,
				sizeof(struct MDNS_QUESTION)) == 0 &&
			kalMemCmp(&respTemplate->mdnsResponseRecord,
				&gRspCacheRecord[i].mdnsResponseRecord,
				sizeof(struct MDNS_ANSWER_RR)) == 0 &&
			kalMemCmp(&respTemplate->mdnsTxtRecord,
				&gRspCacheRecord[i].mdnsTxtRecord,
				sizeof(struct MDNS_TXT_RR)) == 0 &&
			kalMemCmp(&respTemplate->mdnsSrvRecord,
				&gRspCacheRecord[i].mdnsSrvRecord,
				sizeof(struct MDNS_SRV_RR)) == 0) {
			respTemplate->bIsUsed = TRUE;
			respTemplate->u4RecordId = i;
			kalMemCopy(&gRspCacheRecord[i],
				respTemplate,
				sizeof(struct MDNS_Template_Record));
			flag = TRUE;
			break;
		}
	}

	if (flag == TRUE) {
		DBGLOG(SW4, LOUD, "already in the table, record %d.\n", i);
		goto exit;
	}

	for (i = 0; i <  MAX_MDNS_CACHE_NUM; i++) {
		/* find a unused one */
		if (gRspCacheRecord[i].bIsUsed == FALSE) {
			respTemplate->u4RecordId = i;
			respTemplate->bIsUsed = TRUE;
			kalMemCopy(&gRspCacheRecord[i],
				respTemplate,
				sizeof(struct MDNS_Template_Record));
			flag = TRUE;
			break;
		}
	}

	if (flag == TRUE) {
		DBGLOG(SW4, LOUD, "find a unused record %d.\n", i);
		goto exit;
	}

	DBGLOG(SW4, LOUD, "cache full, replace the oldest index %d.\n",
			gOldestIndex);

	respTemplate->u4RecordId = gOldestIndex;
	kalMemCopy(&gRspCacheRecord[gOldestIndex],
				respTemplate,
				sizeof(struct MDNS_Template_Record));
	gOldestIndex = (gOldestIndex + 1) % MAX_MDNS_CACHE_NUM;

exit:
	kalMemFree(respTemplate, PHY_MEM_TYPE,
		sizeof(struct MDNS_Template_Record));

	return 0;
}

void kalDelMdnsCache(struct MDNS_RESP_INFO_T *respInfo)
{
	uint8_t i, j;

	for (i = 0; i < MAX_MDNS_CACHE_NUM; i++) {
		if (kalMemCmp(respInfo->ptrInfo.queryName,
			gRspCacheRecord[i].mdnsQuestionTemplate.name,
				MDNS_QUESTION_NAME_MAX_LEN) == 0 &&
			kalMemCmp(respInfo->ptrInfo.domName,
				gRspCacheRecord[i].ucServiceName,
				MDNS_SERVICE_NAME_MAX_LEN) == 0) {
			DBGLOG(SW4, LOUD, "goodbye packet, delete %d.\n", i);
			kalMemZero(&gRspCacheRecord[i],
					sizeof(struct MDNS_Template_Record));
			break;
		}
	}

	/* move the other to fill the one */
	for (j = i + 1; j < MAX_MDNS_CACHE_NUM; j++) {
		if (gRspCacheRecord[j].bIsUsed == 0)
			continue;

		gRspCacheRecord[j].u4RecordId -= 1;
		kalMemCopy(&gRspCacheRecord[j - 1], &gRspCacheRecord[j],
				sizeof(struct MDNS_Template_Record));

	}

	kalMemZero(&gRspCacheRecord[MAX_MDNS_CACHE_NUM - 1],
			sizeof(struct MDNS_Template_Record));
}

void kalInitMdnsCache(void)
{
	kalMemZero(&gRspCacheRecord,
		sizeof(struct MDNS_Template_Record) * MAX_MDNS_CACHE_NUM);
}

#endif

void kalFreeTxMsduWorker(struct work_struct *work)
{
	struct GLUE_INFO *prGlueInfo;
	struct ADAPTER *prAdapter;
	struct QUE rTmpQue;
	struct QUE *prTmpQue = &rTmpQue;
	struct MSDU_INFO *prMsduInfo;

	if (g_u4HaltFlag)
		return;

	prGlueInfo = ENTRY_OF(work, struct GLUE_INFO,
			      rTxMsduFreeWork);
	prAdapter = prGlueInfo->prAdapter;

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT)
		return;

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
	QUEUE_MOVE_ALL(prTmpQue, &prAdapter->rTxDataDoneQueue);
	KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);

	while (QUEUE_IS_NOT_EMPTY(prTmpQue)) {
		QUEUE_REMOVE_HEAD(prTmpQue, prMsduInfo, struct MSDU_INFO *);

		wlanTxProfilingTagMsdu(prAdapter, prMsduInfo,
				       TX_PROF_TAG_DRV_FREE_MSDU);

		nicTxFreePacket(prAdapter, prMsduInfo, FALSE);
		nicTxReturnMsduInfo(prAdapter, prMsduInfo);
	}
}

void kalFreeTxMsdu(struct ADAPTER *prAdapter,
		   struct MSDU_INFO *prMsduInfo)
{

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
	QUEUE_INSERT_TAIL(&prAdapter->rTxDataDoneQueue,
			  (struct QUE_ENTRY *) prMsduInfo);
	KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);

	schedule_work(&prAdapter->prGlueInfo->rTxMsduFreeWork);
}

int32_t kalHaltLock(uint32_t waitMs)
{
	int32_t i4Ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	if (waitMs) {
		i4Ret = down_timeout(&rHaltCtrl.lock,
				     MSEC_TO_JIFFIES(waitMs));
		if (!i4Ret)
			goto success;
		if (i4Ret != -ETIME)
			return i4Ret;

		prGlueInfo = wlanGetGlueInfo();
		if (rHaltCtrl.fgHeldByKalIoctl) {
			DBGLOG(INIT, ERROR,
			       "kalIoctl was executed longer than %u ms, show backtrace of tx_thread!\n",
			       kalGetTimeTick() - rHaltCtrl.u4HoldStart);
			if (prGlueInfo)
				kal_show_stack(prGlueInfo->prAdapter,
					prGlueInfo->main_thread, NULL);
		} else {
			DBGLOG(INIT, ERROR,
			       "halt lock held by %s pid %d longer than %u ms!\n",
			       rHaltCtrl.owner->comm, rHaltCtrl.owner->pid,
			       kalGetTimeTick() - rHaltCtrl.u4HoldStart);
			if (prGlueInfo)
				kal_show_stack(prGlueInfo->prAdapter,
					rHaltCtrl.owner, NULL);
		}
		return i4Ret;
	}
	down(&rHaltCtrl.lock);
success:
	rHaltCtrl.owner = current;
	rHaltCtrl.u4HoldStart = kalGetTimeTick();
	return 0;
}

int32_t kalHaltTryLock(void)
{
	int32_t i4Ret = 0;

	i4Ret = down_trylock(&rHaltCtrl.lock);
	if (i4Ret)
		return i4Ret;
	rHaltCtrl.owner = current;
	rHaltCtrl.u4HoldStart = kalGetTimeTick();
	return 0;
}

void kalHaltUnlock(void)
{
	if (kalGetTimeTick() - rHaltCtrl.u4HoldStart >
	    WLAN_OID_TIMEOUT_THRESHOLD * 2 &&
	    rHaltCtrl.owner)
		DBGLOG(INIT, ERROR,
		       "process %s pid %d hold halt lock longer than 4s!\n",
		       rHaltCtrl.owner->comm, rHaltCtrl.owner->pid);
	rHaltCtrl.owner = NULL;
	up(&rHaltCtrl.lock);
}

void kalSetHalted(u_int8_t fgHalt)
{
	rHaltCtrl.fgHalt = fgHalt;
}

u_int8_t kalIsHalted(void)
{
	return rHaltCtrl.fgHalt;
}


#if 0
void kalPerMonDump(IN struct GLUE_INFO *prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, WARN, "ulPerfMonFlag:0x%lx\n",
	       prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, WARN, "ulLastTxBytes:%d\n",
	       prPerMonitor->ulLastTxBytes);
	DBGLOG(SW4, WARN, "ulLastRxBytes:%d\n",
	       prPerMonitor->ulLastRxBytes);
	DBGLOG(SW4, WARN, "ulP2PLastTxBytes:%d\n",
	       prPerMonitor->ulP2PLastTxBytes);
	DBGLOG(SW4, WARN, "ulP2PLastRxBytes:%d\n",
	       prPerMonitor->ulP2PLastRxBytes);
	DBGLOG(SW4, WARN, "ulThroughput:%d\n",
	       prPerMonitor->ulThroughput);
	DBGLOG(SW4, WARN, "u4UpdatePeriod:%d\n",
	       prPerMonitor->u4UpdatePeriod);
	DBGLOG(SW4, WARN, "u4TarPerfLevel:%d\n",
	       prPerMonitor->u4TarPerfLevel);
	DBGLOG(SW4, WARN, "u4CurrPerfLevel:%d\n",
	       prPerMonitor->u4CurrPerfLevel);
	DBGLOG(SW4, WARN, "netStats tx_bytes:%d\n",
	       prGlueInfo->prDevHandler->stats.tx_bytes);
	DBGLOG(SW4, WARN, "netStats tx_bytes:%d\n",
	       prGlueInfo->prDevHandler->stats.rx_bytes);
	DBGLOG(SW4, WARN, "p2p netStats tx_bytes:%d\n",
	       prGlueInfo->prP2PInfo->prDevHandler->stats.tx_bytes);
	DBGLOG(SW4, WARN, "p2p netStats tx_bytes:%d\n",
	       prGlueInfo->prP2PInfo->prDevHandler->stats.rx_bytes);
}
#endif

#if (CFG_SUPPORT_PERF_IND == 1)
#define PERF_UPDATE_PERIOD      1000 /* ms */
void kalPerfIndReset(IN struct ADAPTER *prAdapter)
{
	uint8_t ucIdx;

	for (ucIdx = 0; ucIdx < BSSID_NUM; ucIdx++) {
		prAdapter->prGlueInfo->PerfIndCache.u4CurTxBytes[ucIdx] = 0;
		prAdapter->prGlueInfo->PerfIndCache.u4CurRxBytes[ucIdx] = 0;
		prAdapter->prGlueInfo->PerfIndCache.u2CurRxRate[ucIdx] = 0;
		prAdapter->prGlueInfo->PerfIndCache.ucCurRxRCPI0[ucIdx] = 0;
		prAdapter->prGlueInfo->PerfIndCache.ucCurRxRCPI1[ucIdx] = 0;
	}
}

uint32_t kalPerfDisableDataRssiUpdate(IN struct ADAPTER *prAdapter)
{
	struct CMD_COEX_HANDLER rCmdCoexHandler;
	bool fgPerfDisableDataRssiUpdate = TRUE;

	ASSERT(prAdapter);

	rCmdCoexHandler.u4SubCmd =
	(enum ENUM_COEX_CMD_CTRL) COEX_CMD_SET_DISABLE_DATA_RSSI_UPDATE;

	kalMemCopy(&rCmdCoexHandler.aucBuffer,
		&fgPerfDisableDataRssiUpdate,
		sizeof(bool));

	return wlanSendSetQueryCmd(prAdapter,
		CMD_ID_COEX_CTRL,
		TRUE,
		FALSE,
		FALSE,
		NULL,
		NULL,
		sizeof(struct CMD_COEX_HANDLER),
		(uint8_t *) &rCmdCoexHandler,
		NULL,
		0);
}

void kalSetPerfReport(IN struct ADAPTER *prAdapter)
{
	struct CMD_PERF_IND *prCmdPerfReport;
	uint8_t i;
	uint32_t u4CurrentTp = 0;

	prCmdPerfReport = (struct CMD_PERF_IND *)
		cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct CMD_PERF_IND));

	if (!prCmdPerfReport)
		return;

	kalMemZero(prCmdPerfReport, sizeof(struct CMD_PERF_IND));

	prCmdPerfReport->ucCmdVer = 0;
	prCmdPerfReport->u2CmdLen = sizeof(struct CMD_PERF_IND);

	prCmdPerfReport->u4VaildPeriod = PERF_UPDATE_PERIOD;

	for (i = 0; i < BSS_DEFAULT_NUM; i++) {
		prCmdPerfReport->ulCurTxBytes[i] =
			prAdapter->prGlueInfo->PerfIndCache.u4CurTxBytes[i];
		prCmdPerfReport->ulCurRxBytes[i] =
			prAdapter->prGlueInfo->PerfIndCache.u4CurRxBytes[i];
		prCmdPerfReport->u2CurRxRate[i] =
			prAdapter->prGlueInfo->PerfIndCache.u2CurRxRate[i];
		prCmdPerfReport->ucCurRxRCPI0[i] =
			prAdapter->prGlueInfo->PerfIndCache.ucCurRxRCPI0[i];
		prCmdPerfReport->ucCurRxRCPI1[i] =
			prAdapter->prGlueInfo->PerfIndCache.ucCurRxRCPI1[i];
		prCmdPerfReport->ucCurRxNss[i] =
			prAdapter->prGlueInfo->PerfIndCache.ucCurRxNss[i];
		u4CurrentTp += (prCmdPerfReport->ulCurTxBytes[i] +
				prCmdPerfReport->ulCurRxBytes[i]);
	}

	if (u4CurrentTp != 0) {
		DBGLOG(SW4, INFO,
	"Total TP[%d] TX-Byte[%d][%d][%d][%d],RX-Byte[%d][%d][%d][%d]\n",
		u4CurrentTp,
		prCmdPerfReport->ulCurTxBytes[0],
		prCmdPerfReport->ulCurTxBytes[1],
		prCmdPerfReport->ulCurTxBytes[2],
		prCmdPerfReport->ulCurTxBytes[3],
		prCmdPerfReport->ulCurRxBytes[0],
		prCmdPerfReport->ulCurRxBytes[1],
		prCmdPerfReport->ulCurRxBytes[2],
		prCmdPerfReport->ulCurRxBytes[3]);
		DBGLOG(SW4, INFO,
	"Rate[%d][%d][%d][%d] RCPI[%d][%d][%d][%d]\n",
		prCmdPerfReport->u2CurRxRate[0],
		prCmdPerfReport->u2CurRxRate[1],
		prCmdPerfReport->u2CurRxRate[2],
		prCmdPerfReport->u2CurRxRate[3],
		prCmdPerfReport->ucCurRxRCPI0[0],
		prCmdPerfReport->ucCurRxRCPI0[1],
		prCmdPerfReport->ucCurRxRCPI0[2],
		prCmdPerfReport->ucCurRxRCPI0[3]);

	wlanSendSetQueryCmd(prAdapter,
		CMD_ID_PERF_IND,
		TRUE,
		FALSE,
		FALSE,
		NULL,
		NULL,
		sizeof(struct CMD_PERF_IND),
		(uint8_t *) prCmdPerfReport,
		NULL,
		0);
	}
	cnmMemFree(prAdapter, prCmdPerfReport);
}
#endif

inline int32_t kalPerMonInit(IN struct GLUE_INFO
			     *prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, TRACE, "enter %s\n", __func__);
	if (KAL_TEST_BIT(PERF_MON_RUNNING_BIT,
			 prPerMonitor->ulPerfMonFlag))
		DBGLOG(SW4, WARN,
		       "abnormal, perf monitor already running\n");
	KAL_CLR_BIT(PERF_MON_RUNNING_BIT,
		    prPerMonitor->ulPerfMonFlag);
	KAL_CLR_BIT(PERF_MON_DISABLE_BIT,
		    prPerMonitor->ulPerfMonFlag);
	KAL_SET_BIT(PERF_MON_STOP_BIT, prPerMonitor->ulPerfMonFlag);
	prPerMonitor->u4UpdatePeriod =
		prGlueInfo->prAdapter->rWifiVar.u4PerfMonUpdatePeriod;
	cnmTimerInitTimerOption(prGlueInfo->prAdapter,
				&prPerMonitor->rPerfMonTimer,
				(PFN_MGMT_TIMEOUT_FUNC) kalPerMonHandler,
				(unsigned long) NULL,
				TIMER_WAKELOCK_NONE);
#if (CFG_SUPPORT_PERF_IND == 1)
	kalPerfIndReset(prGlueInfo->prAdapter);
#endif
	DBGLOG(SW4, TRACE, "exit %s\n", __func__);
	return 0;
}

inline int32_t kalPerMonDisable(IN struct GLUE_INFO
				*prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;

	DBGLOG(SW4, INFO, "enter %s\n", __func__);
	if (KAL_TEST_BIT(PERF_MON_RUNNING_BIT,
			 prPerMonitor->ulPerfMonFlag)) {
		DBGLOG(SW4, TRACE, "need to stop before disable\n");
		kalPerMonStop(prGlueInfo);
	}
	KAL_SET_BIT(PERF_MON_DISABLE_BIT,
		    prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, TRACE, "exit %s\n", __func__);
	return 0;
}

inline int32_t kalPerMonEnable(IN struct GLUE_INFO
			       *prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;

	DBGLOG(SW4, INFO, "enter %s\n", __func__);
	KAL_CLR_BIT(PERF_MON_DISABLE_BIT,
		    prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, TRACE, "exit %s\n", __func__);
	return 0;
}

inline int32_t kalPerMonStart(IN struct GLUE_INFO
			      *prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, TEMP, "enter %s\n", __func__);

	if (!wlan_perf_monitor_force_enable &&
	    (wlan_fb_power_down || prGlueInfo->fgIsInSuspendMode))
		return 0;

	if (KAL_TEST_BIT(PERF_MON_DISABLE_BIT,
			 prPerMonitor->ulPerfMonFlag) ||
	    KAL_TEST_BIT(PERF_MON_RUNNING_BIT,
			 prPerMonitor->ulPerfMonFlag))
		return 0;

	prPerMonitor->ulLastRxBytes = 0;
	prPerMonitor->ulLastTxBytes = 0;
	prPerMonitor->ulP2PLastRxBytes = 0;
	prPerMonitor->ulP2PLastTxBytes = 0;
	prPerMonitor->ulThroughput = 0;
	prPerMonitor->u4CurrPerfLevel = 0;
	prPerMonitor->u4TarPerfLevel = 0;

	prPerMonitor->u4UpdatePeriod =
		prGlueInfo->prAdapter->rWifiVar.u4PerfMonUpdatePeriod;
	cnmTimerStartTimer(prGlueInfo->prAdapter,
		&prPerMonitor->rPerfMonTimer, prPerMonitor->u4UpdatePeriod);
	KAL_SET_BIT(PERF_MON_RUNNING_BIT,
		    prPerMonitor->ulPerfMonFlag);
	KAL_CLR_BIT(PERF_MON_STOP_BIT, prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, INFO, "perf monitor started\n");
	return 0;
}

inline int32_t kalPerMonStop(IN struct GLUE_INFO
			     *prGlueInfo)
{
	struct PERF_MONITOR_T *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, TRACE, "enter %s\n", __func__);

	if (KAL_TEST_BIT(PERF_MON_DISABLE_BIT,
			 prPerMonitor->ulPerfMonFlag)) {
		DBGLOG(SW4, TRACE, "perf monitor disabled\n");
		return 0;
	}

	if (KAL_TEST_BIT(PERF_MON_STOP_BIT,
			 prPerMonitor->ulPerfMonFlag)) {
		DBGLOG(SW4, TRACE, "perf monitor already stopped\n");
		return 0;
	}

	KAL_SET_BIT(PERF_MON_STOP_BIT, prPerMonitor->ulPerfMonFlag);
	if (KAL_TEST_BIT(PERF_MON_RUNNING_BIT,
			 prPerMonitor->ulPerfMonFlag)) {
		cnmTimerStopTimer(prGlueInfo->prAdapter,
				  &prPerMonitor->rPerfMonTimer);
		KAL_CLR_BIT(PERF_MON_RUNNING_BIT,
			    prPerMonitor->ulPerfMonFlag);
		prPerMonitor->ulLastRxBytes = 0;
		prPerMonitor->ulLastTxBytes = 0;
		prPerMonitor->ulP2PLastRxBytes = 0;
		prPerMonitor->ulP2PLastTxBytes = 0;
		prPerMonitor->ulThroughput = 0;
		prPerMonitor->ulWlanTxTp = 0;
		prPerMonitor->ulWlanRxTp = 0;
		prPerMonitor->ulP2PTxTp = 0;
		prPerMonitor->ulP2PRxTp = 0;
		prPerMonitor->u4CurrPerfLevel = 0;
		prPerMonitor->u4TarPerfLevel = 0;
		/*Cancel CPU performance mode request*/
		kalBoostCpu(prGlueInfo->prAdapter,
			    prPerMonitor->u4TarPerfLevel,
			    prGlueInfo->prAdapter->rWifiVar.u4BoostCpuTh);
	}
	DBGLOG(SW4, TRACE, "exit %s\n", __func__);
	return 0;
}

inline int32_t kalPerMonDestroy(IN struct GLUE_INFO
				*prGlueInfo)
{
	kalPerMonDisable(prGlueInfo);
	return 0;
}

void kalPerMonHandler(IN struct ADAPTER *prAdapter,
		      unsigned long ulParam)
{
	/*Calculate current throughput*/
	struct PERF_MONITOR_T *prPerMonitor;
	struct net_device *prNetDev = NULL;
	uint32_t u4Idx = 0;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	uint8_t	i =
		0; /*currently i will always be 0 because we only regist 0*/
	u_int8_t	fgIsP2PNoneUsed = FALSE;

	signed long latestTxBytes, latestRxBytes, txDiffBytes,
	       rxDiffBytes;
	signed long p2pLatestTxBytes, p2pLatestRxBytes,
	       p2pTxDiffBytes, p2pRxDiffBytes;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
#if (CFG_SUPPORT_PERF_IND == 1)
	uint8_t ucIdx;
	uint32_t u4CurrentTp = 0;
	/* TBD */
	/* struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar; */
#endif

	if ((prGlueInfo->ulFlag & GLUE_FLAG_HALT)
	    || (!prAdapter->fgIsP2PRegistered))
		return;

	for (u4Idx = 0; u4Idx < BSS_DEFAULT_NUM; u4Idx++) {
		prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						     (uint8_t) u4Idx);
		if (prP2pBssInfo->eNetworkType == NETWORK_TYPE_P2P)
			break;
	}
	prNetDev = prGlueInfo->prDevHandler;

	prPerMonitor = &prAdapter->rPerMonitor;
	DBGLOG(SW4, TRACE, "enter kalPerMonHandler\n");
#if (CFG_SUPPORT_PERF_IND == 1)
	/* TBD ==> if (prWifiVar->fgPerfIndicatorEn) */
	kalSetPerfReport(prAdapter);
	for (ucIdx = 0; ucIdx < BSS_DEFAULT_NUM; ucIdx++) {
		u4CurrentTp +=
		(prAdapter->prGlueInfo->PerfIndCache.u4CurTxBytes[ucIdx] +
		 prAdapter->prGlueInfo->PerfIndCache.u4CurRxBytes[ucIdx]);
	}
	kalPerfIndReset(prAdapter);
#endif

	latestTxBytes = prGlueInfo->prDevHandler->stats.tx_bytes;
	latestRxBytes = prGlueInfo->prDevHandler->stats.rx_bytes;
	if (prGlueInfo->prP2PInfo[i]) {
		if (prGlueInfo->prP2PInfo[i]->prDevHandler) {
			p2pLatestTxBytes =
			prGlueInfo->prP2PInfo[i]->prDevHandler->stats.tx_bytes;
			p2pLatestRxBytes =
			prGlueInfo->prP2PInfo[i]->prDevHandler->stats.rx_bytes;
		} else {
			fgIsP2PNoneUsed = TRUE;
			p2pLatestTxBytes = 0;
			p2pLatestRxBytes = 0;
		}
	} else {
		fgIsP2PNoneUsed = TRUE;
		p2pLatestTxBytes = 0;
		p2pLatestRxBytes = 0;
	}

	if (prPerMonitor->ulLastRxBytes == 0 &&
	    prPerMonitor->ulLastTxBytes == 0 &&
	    prPerMonitor->ulP2PLastRxBytes == 0 &&
	    prPerMonitor->ulP2PLastTxBytes == 0) {
		prPerMonitor->ulThroughput = 0;
		prPerMonitor->ulWlanTxTp = 0;
		prPerMonitor->ulWlanRxTp = 0;
		prPerMonitor->ulP2PTxTp = 0;
		prPerMonitor->ulP2PRxTp = 0;
	} else {
		txDiffBytes = latestTxBytes - prPerMonitor->ulLastTxBytes;
		rxDiffBytes = latestRxBytes - prPerMonitor->ulLastRxBytes;
		if (txDiffBytes < 0)
			txDiffBytes = -(txDiffBytes);
		if (rxDiffBytes < 0)
			rxDiffBytes = -(rxDiffBytes);

		if (!fgIsP2PNoneUsed) {
			p2pTxDiffBytes = p2pLatestTxBytes -
					 prPerMonitor->ulP2PLastTxBytes;
			p2pRxDiffBytes = p2pLatestRxBytes -
					 prPerMonitor->ulP2PLastRxBytes;
			if (p2pTxDiffBytes < 0)
				p2pTxDiffBytes = -(p2pTxDiffBytes);
			if (p2pRxDiffBytes < 0)
				p2pRxDiffBytes = -(p2pRxDiffBytes);
		} else {
			p2pTxDiffBytes = 0;
			p2pRxDiffBytes = 0;
		}
		prPerMonitor->ulThroughput = txDiffBytes + rxDiffBytes +
					     p2pTxDiffBytes + p2pRxDiffBytes;
		prPerMonitor->ulThroughput =
			wlanData2RateInMs(prPerMonitor->ulThroughput,
			prPerMonitor->u4UpdatePeriod);
		prPerMonitor->ulThroughput <<= 3;

		if (txDiffBytes)
			prPerMonitor->ulWlanTxTp =
				wlanData2RateInMs(txDiffBytes,
				prPerMonitor->u4UpdatePeriod);

		if (rxDiffBytes)
			prPerMonitor->ulWlanRxTp =
				wlanData2RateInMs(rxDiffBytes,
				prPerMonitor->u4UpdatePeriod);

		if (p2pTxDiffBytes)
			prPerMonitor->ulP2PTxTp =
				wlanData2RateInMs(p2pTxDiffBytes,
				prPerMonitor->u4UpdatePeriod);

		if (p2pRxDiffBytes)
			prPerMonitor->ulP2PRxTp =
				wlanData2RateInMs(p2pRxDiffBytes,
				prPerMonitor->u4UpdatePeriod);
		DBGLOG(SW4, INFO,
			"Tput: %ld > [%ld][%ld] [%ld][%ld], Pending[%d], Used[%d] PER[%ld %ld]\n",
			prPerMonitor->ulThroughput,
			txDiffBytes, rxDiffBytes,
			p2pTxDiffBytes, p2pRxDiffBytes,
			GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
			GLUE_GET_REF_CNT(prPerMonitor->u4UsedCnt),
			prPerMonitor->ulTotalTxSuccessCount,
			prPerMonitor->ulTotalTxFailCount);
	}

	prPerMonitor->ulLastTxBytes = latestTxBytes;
	prPerMonitor->ulLastRxBytes = latestRxBytes;
	prPerMonitor->ulP2PLastTxBytes = p2pLatestTxBytes;
	prPerMonitor->ulP2PLastRxBytes = p2pLatestRxBytes;

	prPerMonitor->u4TarPerfLevel = PERF_MON_TP_MAX_THRESHOLD;
	for (u4Idx = 0; u4Idx < PERF_MON_TP_MAX_THRESHOLD;
	     u4Idx++) {
		if ((prPerMonitor->ulThroughput >> 20) <
		    prAdapter->rWifiVar.u4PerfMonTpTh[u4Idx]) {
			prPerMonitor->u4TarPerfLevel = u4Idx;
			break;
		}
	}

	if (!wlan_perf_monitor_force_enable &&
	    (wlan_fb_power_down ||
	     prGlueInfo->fgIsInSuspendMode ||
	     !(netif_carrier_ok(prNetDev) ||
	       (prP2pBssInfo->eConnectionState ==
		PARAM_MEDIA_STATE_CONNECTED) ||
	       (prP2pBssInfo->rStaRecOfClientList.u4NumElem > 0))))
		kalPerMonStop(prGlueInfo);
	else {
		if ((prPerMonitor->u4TarPerfLevel !=
		     prPerMonitor->u4CurrPerfLevel) &&
		    (prAdapter->rWifiVar.u4BoostCpuTh <
		     PERF_MON_TP_MAX_THRESHOLD)) {

			DBGLOG(SW4, INFO,
			"PerfMon total:%3lu.%03lu mbps lv:%u th:%u fg:0x%lx\n",
			(unsigned long) (prPerMonitor->ulThroughput >> 20),
			(unsigned long) ((prPerMonitor->ulThroughput >> 10)
					& BITS(0, 9)),
			prPerMonitor->u4TarPerfLevel,
			prAdapter->rWifiVar.u4BoostCpuTh,
			prPerMonitor->ulPerfMonFlag);

			kalBoostCpu(prAdapter, prPerMonitor->u4TarPerfLevel,
				    prAdapter->rWifiVar.u4BoostCpuTh);
		}
		prPerMonitor->u4UpdatePeriod =
			prAdapter->rWifiVar.u4PerfMonUpdatePeriod;
#if (CFG_SUPPORT_PERF_IND == 1)
		/* Only routine start timer when Tput exist*/
		if (u4CurrentTp > 0) {
			cnmTimerStartTimer(prGlueInfo->prAdapter,
				&prPerMonitor->rPerfMonTimer,
				prPerMonitor->u4UpdatePeriod);
		} else {
			KAL_CLR_BIT(PERF_MON_RUNNING_BIT,
				prPerMonitor->ulPerfMonFlag);
			kalPerfDisableDataRssiUpdate(prAdapter);
		}
#else
		cnmTimerStartTimer(prGlueInfo->prAdapter,
			&prPerMonitor->rPerfMonTimer,
			prPerMonitor->u4UpdatePeriod);
#endif
	}
	prPerMonitor->u4CurrPerfLevel =
		prPerMonitor->u4TarPerfLevel;

	/* check tx hang */
	prAdapter->u4HifChkFlag |= HIF_CHK_TX_HANG;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	DBGLOG(SW4, TRACE, "exit kalPerMonHandler\n");
}

uint32_t kalPerMonGetInfo(IN struct ADAPTER *prAdapter,
			  IN uint8_t *pucBuf, IN uint32_t u4Max)
{
	struct PERF_MONITOR_T *prPerMonitor;
	uint32_t u4Len = 0;
	unsigned long ulWlanTxTpInBits, ulWlanRxTpInBits,
		 ulP2PTxTpInBits, ulP2PRxTpInBits;

	prPerMonitor = &prAdapter->rPerMonitor;

	ulWlanTxTpInBits = prPerMonitor->ulWlanTxTp << 3;
	ulWlanRxTpInBits = prPerMonitor->ulWlanRxTp << 3;
	ulP2PTxTpInBits = prPerMonitor->ulP2PTxTp << 3;
	ulP2PRxTpInBits = prPerMonitor->ulP2PRxTp << 3;

	LOGBUF(pucBuf, u4Max, u4Len,
	       "\nWi-Fi Throughput (update period %ums):\n",
	       prPerMonitor->u4UpdatePeriod);

	LOGBUF(pucBuf, u4Max, u4Len,
	       "wlan Tx: %3lu.%03lu mbps, Rx %3lu.%03lu mbps\n",
	       (ulWlanTxTpInBits >> 20),
	       ((ulWlanTxTpInBits >> 10) & BITS(0, 9)),
	       (ulWlanRxTpInBits >> 20),
	       ((ulWlanRxTpInBits >> 10) & BITS(0, 9)));

	LOGBUF(pucBuf, u4Max, u4Len,
	       "p2p  Tx: %3lu.%03lu mbps, Rx %3lu.%03lu mbps\n",
	       (ulP2PTxTpInBits >> 20), ((ulP2PTxTpInBits >> 10) & BITS(0,
					 9)),
	       (ulP2PRxTpInBits >> 20), ((ulP2PRxTpInBits >> 10) & BITS(0,
					 9)));

	LOGBUF(pucBuf, u4Max, u4Len, "Total: %3lu.%03lu mbps\n",
	       (prPerMonitor->ulThroughput >> 20),
	       ((prPerMonitor->ulThroughput >> 10) & BITS(0, 9)));

	LOGBUF(pucBuf, u4Max, u4Len,
	       "Performance level: %u threshold: %u flag: 0x%lx\n",
	       prPerMonitor->u4CurrPerfLevel,
	       prAdapter->rWifiVar.u4BoostCpuTh,
	       prPerMonitor->ulPerfMonFlag);

	return u4Len;
}

int32_t __weak kalBoostCpu(IN struct ADAPTER *prAdapter,
			   IN uint32_t u4TarPerfLevel, IN uint32_t u4BoostCpuTh)
{
	DBGLOG(SW4, INFO, "enter kalBoostCpu\n");
	return 0;
}

int32_t __weak kalSetCpuNumFreq(uint32_t u4CoreNum,
				uint32_t u4Freq)
{
	DBGLOG(SW4, INFO,
	       "enter weak kalSetCpuNumFreq, u4CoreNum:%d, urFreq:%d\n",
	       u4CoreNum, u4Freq);
	return 0;
}

void __weak kalSetEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
				   uint32_t size, bool enable)
{
	DBGLOG(SW4, WARN, "EMI MPU function is not defined\n");
}

void __weak kalSetDrvEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
				      uint32_t size)
{
	DBGLOG(SW4, WARN, "DRV EMI MPU function is not defined\n");
}

int32_t kalPerMonSetForceEnableFlag(uint8_t uFlag)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)
				       wlan_fb_notifier_priv_data;

	wlan_perf_monitor_force_enable = uFlag == 0 ? FALSE : TRUE;
	DBGLOG(SW4, INFO,
	       "uFlag:%d, wlan_perf_monitor_ctrl_flag:%d\n", uFlag,
	       wlan_perf_monitor_force_enable);

	if (wlan_perf_monitor_force_enable && prGlueInfo
	    && !kalIsHalted())
		kalPerMonEnable(prGlueInfo);

	return 0;
}

static int wlan_fb_notifier_callback(struct notifier_block
				     *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int32_t blank = 0;
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)
				       wlan_fb_notifier_priv_data;

	/* If we aren't interested in this event, skip it immediately ... */
	if ((event != FB_EVENT_BLANK) || !prGlueInfo)
		return 0;

	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted()) {
		kalHaltUnlock();
		return 0;
	}

	blank = *(int32_t *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		kalPerMonEnable(prGlueInfo);
		wlan_fb_power_down = FALSE;
		break;
	case FB_BLANK_POWERDOWN:
		wlan_fb_power_down = TRUE;
		if (!wlan_perf_monitor_force_enable)
			kalPerMonDisable(prGlueInfo);
		break;
	default:
		break;
	}

	kalHaltUnlock();
	return 0;
}

int32_t kalFbNotifierReg(IN struct GLUE_INFO *prGlueInfo)
{
	int32_t i4Ret;

	wlan_fb_notifier_priv_data = prGlueInfo;
	wlan_fb_notifier.notifier_call = wlan_fb_notifier_callback;

	i4Ret = fb_register_client(&wlan_fb_notifier);
	if (i4Ret)
		DBGLOG(SW4, WARN, "Register wlan_fb_notifier failed:%d\n",
		       i4Ret);
	else
		DBGLOG(SW4, TRACE, "Register wlan_fb_notifier succeed\n");
	return i4Ret;
}

void kalFbNotifierUnReg(void)
{
	fb_unregister_client(&wlan_fb_notifier);
	wlan_fb_notifier_priv_data = NULL;
}

#if CFG_SUPPORT_DFS
void kalIndicateChannelSwitch(IN struct GLUE_INFO *prGlueInfo,
				IN enum ENUM_CHNL_EXT eSco,
				IN uint8_t ucChannelNum)
{
	struct cfg80211_chan_def chandef;
	struct ieee80211_channel *prChannel = NULL;
	enum nl80211_channel_type rChannelType;

	if (ucChannelNum <= 14) {
		prChannel =
		    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
			ieee80211_channel_to_frequency(ucChannelNum,
			KAL_BAND_2GHZ));
	} else {
		prChannel =
		    ieee80211_get_channel(priv_to_wiphy(prGlueInfo),
			ieee80211_channel_to_frequency(ucChannelNum,
			KAL_BAND_5GHZ));
	}

	if (!prChannel) {
		DBGLOG(REQ, ERROR, "ieee80211_get_channel fail!\n");
		return;
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

	DBGLOG(REQ, STATE, "DFS channel switch to %d\n", ucChannelNum);

	cfg80211_chandef_create(&chandef, prChannel, rChannelType);
	cfg80211_ch_switch_notify(prGlueInfo->prDevHandler, &chandef);
}
#endif

void kalInitDevWakeup(struct ADAPTER *prAdapter, struct device *prDev)
{
	/*
	 * The remote wakeup function will be disabled after
	 * first time resume, we need to call device_init_wakeup()
	 * to notify usbcore that we support wakeup function,
	 * so usbcore will re-enable our remote wakeup function
	 * before entering suspend.
	 */
	if (prAdapter->rWifiVar.ucWow)
		device_init_wakeup(prDev, TRUE);
}

u_int8_t kalIsOuiMask(const uint8_t pucMacAddrMask[MAC_ADDR_LEN])
{
	return (pucMacAddrMask[0] == 0xFF &&
		pucMacAddrMask[1] == 0xFF &&
		pucMacAddrMask[2] == 0xFF);
}

u_int8_t kalIsValidMacAddr(const uint8_t *addr)
{
	return (addr != NULL) && is_valid_ether_addr(addr);
}

#if (KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE)
u_int8_t kalParseRandomMac(const struct net_device *ndev,
		uint8_t *pucMacAddr, uint8_t *pucMacAddrMask,
		uint8_t *pucRandomMac)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;
	uint8_t ucMacAddr[MAC_ADDR_LEN];

	if (!ndev) {
		log_dbg(SCN, ERROR, "Invalid net device\n");
		return FALSE;
	}

	prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) netdev_priv(ndev);

	if (!prNetDevPrivate || !(prNetDevPrivate->prGlueInfo)
		|| !(prNetDevPrivate->prGlueInfo->prAdapter)) {
		log_dbg(SCN, ERROR, "Invalid private param\n");
		return FALSE;
	}

	prAdapter = prNetDevPrivate->prGlueInfo->prAdapter;
	ucBssIndex = prNetDevPrivate->ucBssIdx;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo) {
		log_dbg(SCN, WARN, "Invalid bss info (ind=%u)\n", ucBssIndex);
		return FALSE;
	}

	if (!kalIsOuiMask(pucMacAddrMask) && !prBssInfo->fgIsScanOuiSet) {
		eth_zero_addr(pucRandomMac);
		eth_zero_addr(pucMacAddrMask);
		log_dbg(SCN, INFO, "random mac enabled.\n");
		return TRUE;
	}

	if (pucMacAddr)
		COPY_MAC_ADDR(ucMacAddr, pucMacAddr);
	else
		eth_zero_addr(ucMacAddr);

	if (prBssInfo->fgIsScanOuiSet) {
		kalMemCopy(ucMacAddr, prBssInfo->ucScanOui, MAC_OUI_LEN);
		kalMemSet(pucMacAddrMask, 0xFF, MAC_OUI_LEN);
	}
	get_random_mask_addr(pucRandomMac, ucMacAddr, pucMacAddrMask);
	log_dbg(SCN, INFO, "random mac=" MACSTR ", mac_addr=" MACSTR
		", mac_addr_mask=%pM\n", MAC2STR(pucRandomMac),
		MAC2STR(ucMacAddr), pucMacAddrMask);

	return TRUE;
}

u_int8_t kalScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_scan_request *request, uint8_t *pucRandomMac)
{
	uint8_t ucMacAddr[MAC_ADDR_LEN];
	uint8_t ucMacAddrMask[MAC_ADDR_LEN];

	ASSERT(request);
	ASSERT(pucRandomMac);

	if (!(request->flags & NL80211_SCAN_FLAG_RANDOM_ADDR)) {
		log_dbg(SCN, TRACE, "Scan random mac is not set\n");
		return FALSE;
	}
#if KERNEL_VERSION(4, 10, 0) <= CFG80211_VERSION_CODE
	{
		if (kalIsValidMacAddr(request->bssid)) {
			COPY_MAC_ADDR(pucRandomMac, request->bssid);
			log_dbg(SCN, INFO, "random mac=" MACSTR "\n",
				pucRandomMac);
			return TRUE;
		}
	}
#endif
	COPY_MAC_ADDR(ucMacAddr, request->mac_addr);
	COPY_MAC_ADDR(ucMacAddrMask, request->mac_addr_mask);

	return kalParseRandomMac(ndev, ucMacAddr, ucMacAddrMask, pucRandomMac);
}

u_int8_t kalSchedScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_sched_scan_request *request,
	uint8_t *pucRandomMac, uint8_t *pucRandomMacMask)
{
	uint8_t ucMacAddr[MAC_ADDR_LEN];

	ASSERT(request);
	ASSERT(pucRandomMac);
	ASSERT(pucRandomMacMask);

	if (!(request->flags & NL80211_SCAN_FLAG_RANDOM_ADDR)) {
		log_dbg(SCN, TRACE, "Scan random mac is not set\n");
		return FALSE;
	}
	COPY_MAC_ADDR(ucMacAddr, request->mac_addr);
	COPY_MAC_ADDR(pucRandomMacMask, request->mac_addr_mask);

	return kalParseRandomMac(ndev, ucMacAddr,
		pucRandomMacMask, pucRandomMac);
}
#else /* if (KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE) */
u_int8_t kalScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_scan_request *request, uint8_t *pucRandomMac)
{
	return FALSE;
}

u_int8_t kalSchedScanParseRandomMac(const struct net_device *ndev,
	const struct cfg80211_sched_scan_request *request,
	uint8_t *pucRandomMac, uint8_t *pucRandomMacMask)
{
	return FALSE;
}
#endif

void kalScanReqLog(struct cfg80211_scan_request *request)
{
#if (KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE)
	scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Scan flags=0x%x [mac]addr="
		MACSTR " mask=" MACSTR "\n",
		request->flags,
		MAC2STR(request->mac_addr),
		MAC2STR(request->mac_addr_mask));
#else
	scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Scan flags=0x%x\n",
		request->flags);
#endif
	kalScanSsidLog(request, SCAN_LOG_MSG_MAX_LEN);
	kalScanChannelLog(request, SCAN_LOG_MSG_MAX_LEN);
}

void kalScanChannelLog(struct cfg80211_scan_request *request,
	const uint16_t logBufLen)
{
	char logBuf[logBufLen];
	uint32_t idx = 0;
	int i = 0;
	/* the decimal value could be 0 ~ 65535 */
	const uint8_t dataLen = 6;

	/* The maximum characters of int32_t could be 10. Thus, the
	 * length should be 10+14 for the format "n_channels=%u: ".
	 */
	idx += kalSnprintf(logBuf, 24, "n_channels=%u: ", request->n_channels);

	for (i = 0; i < request->n_channels; ++i) {
		if (dataLen+1 > logBufLen) {
			scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Need buffer size %u for log\n",
				dataLen+1);
			break;
		} else if (idx+dataLen+1 > logBufLen) {
			logBuf[idx] = 0; /* terminating null byte */
			scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "%s\n",
				logBuf);
			idx = 0;
		}

		/* number + terminating null byte + a space */
		idx += kalSnprintf(logBuf+idx, dataLen+1, "%u ",
			request->channels[i]->hw_value);
	}
	if (idx != 0) {
		logBuf[idx] = 0; /* terminating null byte */
		scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "%s\n",
			logBuf);
		idx = 0;
	}
}

void kalScanSsidLog(struct cfg80211_scan_request *request,
	const uint16_t logBufLen)
{
	char logBuf[logBufLen];
	uint32_t idx = 0;
	int i = 0;

	/* The maximum characters of uint32_t could be 10. Thus, the
	 * length should be 10+11 for the format "n_ssids=%d: ".
	 */
	idx += kalSnprintf(logBuf, 21, "n_ssids=%d: ", request->n_ssids);

	for (i = 0; i < request->n_ssids; ++i) {
		uint8_t len = request->ssids[i].ssid_len;

		if (len == 0) {
			continue;
		} else if (len+1+1 > logBufLen) {
			scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Need buffer size %u for log\n",
				len+1+1);
			break;
		} else if (idx+len+1+1 > logBufLen) {
			logBuf[idx] = 0; /* terminating null byte */
			scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "%s\n",
				logBuf);
			idx = 0;
		}

		/* SSID without terminating null byte */
		kalStrnCpy(logBuf+idx, request->ssids[i].ssid, len);
		idx = idx + len;

		kalMemCopy(logBuf+idx, " ", 1);
		idx = idx + 1;
	}
	if (idx != 0) {
		logBuf[idx] = 0; /* terminating null byte */
		scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "%s\n",
			logBuf);
		idx = 0;
	}
}

void kalScanResultLog(struct ADAPTER *prAdapter, struct ieee80211_mgmt *mgmt)
{
	scanLogCacheAddBSS(
		&(prAdapter->rWifiVar.rScanInfo.rScanLogCache.rBSSListCFG),
		prAdapter->rWifiVar.rScanInfo.rScanLogCache.arBSSListBufCFG,
		LOG_SCAN_RESULT_D2K,
		mgmt->bssid,
		mgmt->seq_ctrl);
}

void kalScanLogCacheFlushBSS(struct ADAPTER *prAdapter,
	const uint16_t logBufLen)
{
	scanLogCacheFlushBSS(
		&(prAdapter->rWifiVar.rScanInfo.rScanLogCache.rBSSListCFG),
		LOG_SCAN_DONE_D2K, logBufLen);
}

void kalRxNapiSetEnable(
		IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t enable)
{
#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	DBGLOG(RX, STATE, "RX NAPI %s\n", (enable?"Enable":"Disable"));
	prGlueInfo->ucRxNapiEnable = enable;
#endif
}

uint8_t kalRxNapiIsEnable(IN struct GLUE_INFO *prGlueInfo)
{
#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	return prGlueInfo->ucRxNapiEnable;
#else
	return FALSE;
#endif
}

uint8_t kalRxNapiValidSkb(struct GLUE_INFO *prGlueInfo,
	struct sk_buff *prSkb)
{
#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	struct WIFI_VAR *prWifiVar = &prGlueInfo->prAdapter->rWifiVar;
	struct PERF_MONITOR_T *prPerMonitor =
		&prGlueInfo->prAdapter->rPerMonitor;
	struct net_device *prNetDev = prGlueInfo->prDevHandler;

	uint8_t *pucMacHdr;
	uint8_t ucIpProto;
	uint16_t u2EtherType;

#if CFG_SUPPORT_SNIFFER
	if (prGlueInfo->fgIsEnableMon)
		return FALSE;
#endif
	prNetDev = (struct net_device *)wlanGetNetInterfaceByBssIdx(
		prGlueInfo, GLUE_GET_PKT_BSS_IDX(prSkb));
	if (!prNetDev)
		prNetDev = prGlueInfo->prDevHandler;
	/*ap/p2p not support yet*/
	if (NL80211_IFTYPE_STATION !=
		prNetDev->ieee80211_ptr->iftype)
		return FALSE;
	if (!kalRxNapiIsEnable(prGlueInfo))
		return FALSE;

	/* Tput condition check, if PerfMon running */
	if (KAL_TEST_BIT(PERF_MON_RUNNING_BIT, prPerMonitor->ulPerfMonFlag)) {
		/* Skip NAPI if Tput too low */
		uint16_t u2TputMbps = 0;

		if (prWifiVar->ucRxNapiNoTx) {
			u2TputMbps += prPerMonitor->ulWlanRxTp >> 17;
			u2TputMbps += prPerMonitor->ulP2PRxTp  >> 17;
		} else
			u2TputMbps = prPerMonitor->ulThroughput >> 20;

		if (u2TputMbps < prWifiVar->ucRxNapiThreshold)
			return FALSE;
	}

	/* Get mac_hdr to check caller */
	if (!skb_mac_header_was_set(prSkb))
		skb_reset_mac_header(prSkb);
	pucMacHdr = skb_mac_header(prSkb);

	/* If in main thread, check settings */
	if (prSkb->data == pucMacHdr) {
		/* If we need RxThread mode, return FALSE here. */
		if (prWifiVar->ucRxNapiThread)
			return FALSE;
	}

	/* Need to check IP protocol? */
	if (!prWifiVar->ucRxNapiPktChk)
		return TRUE;

	WLAN_GET_FIELD_BE16(&pucMacHdr[ETHER_TYPE_LEN_OFFSET], &u2EtherType);

	/* Check if pkt-len is valid or not */
	if (u2EtherType != ETH_P_IPV4 ||
		prSkb->len <= ETH_HLEN+IPV4_HDR_LEN)
		return TRUE;

	/* Check type of IP-pkt */
	ucIpProto = pucMacHdr[ETH_HLEN+IPV4_HDR_IP_PROTOCOL_OFFSET];
	/* NAPI may drop UDP */
	if (ucIpProto == IP_PRO_UDP)
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif
}

int kalRxNapiPoll(struct napi_struct *napi, int budget)
{
#if IS_ENABLED(CFG_RX_NAPI_SUPPORT)
	uint32_t work_done = 0;
	struct sk_buff *prSkb = NULL;
	struct GLUE_INFO *prGlueInfo =
		container_of(napi, struct GLUE_INFO, rNapi);

	while ((work_done < budget) &&
	       (!skb_queue_empty(&prGlueInfo->rRxNapiSkbQ))) {
		uint8_t ucAccept = TRUE;

		prSkb = skb_dequeue(&prGlueInfo->rRxNapiSkbQ);
		if (!prSkb)
			break;

#if IS_ENABLED(CFG_GRO_SUPPORT)
		if (napi_gro_receive(napi, prSkb) == GRO_DROP)
#else
		if (netif_receive_skb(prSkb) != NET_RX_SUCCESS)
#endif /* CFG_GRO_SUPPORT */
			ucAccept = FALSE;

		if (ucAccept)
			work_done++;
	}
	if (work_done < budget) {
		if (!skb_queue_empty(&prGlueInfo->rRxNapiSkbQ))
			napi_schedule(napi);
		else
			napi_complete(napi);
	}
	return work_done;
#else
	return 0;
#endif
}

unsigned long kal_kallsyms_lookup_name(const char *name)
{
	unsigned long ret = 0;

	ret = kallsyms_lookup_name(name);
	if (ret) {
#ifdef CONFIG_ARM
#ifdef CONFIG_THUMB2_KERNEL
		/*set bit 0 in address for thumb mode*/
		ret |= 1;
#endif
#endif
	}
	return ret;
}


