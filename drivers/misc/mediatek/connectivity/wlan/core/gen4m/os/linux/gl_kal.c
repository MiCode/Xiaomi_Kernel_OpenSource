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
#include <stdarg.h>

#if CFG_SUPPORT_AGPS_ASSIST
#include <net/netlink.h>
#endif

#if CFG_TC1_FEATURE
#include <tc1_partition.h>
#endif

/* for rps */
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/sch_generic.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/debugfs.h>

/* for uevent */
#include <linux/miscdevice.h>   /* for misc_register, and SYNTH_MINOR */
#include <linux/kobject.h>

/* for wifi standalone log */
#define CREATE_TRACE_POINTS
#include "mtk_wifi_trace.h"
#include <linux/jiffies.h>
#include <linux/ratelimit.h>
#include <linux/rtc.h>

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

static struct notifier_block wlan_fb_notifier;
void *wlan_fb_notifier_priv_data;

static struct miscdevice wlan_object;

static unsigned long rtc_update;
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

static void kalDumpHifStats(IN struct ADAPTER *prAdapter);
static uint32_t kalPerMonUpdate(IN struct ADAPTER *prAdapter);

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
	(uint8_t *) CFG_FW_FILENAME "_",
	NULL
};

static uint8_t *apucCr4FwName[] = {
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

#if !CONFIG_WLAN_DRV_BUILD_IN
/*----------------------------------------------------------------------------*/
/*!
 * \brief  To leverage systrace, use the same name, i.e. tracing_mark_write,
 *         which ftrace would write when systrace writes msg to
 *         /sys/kernel/debug/tracing/trace_marker with trace_options enabling
 *         print-parent
 *
 */
/*----------------------------------------------------------------------------*/
void tracing_mark_write(const char *fmt, ...)
{
	const uint32_t BUFFER_SIZE = 1024;
	va_list ap;
	char buf[BUFFER_SIZE];

	if ((aucDebugModule[DBG_TRACE_IDX] & DBG_CLASS_TEMP) == 0)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, BUFFER_SIZE, fmt, ap);
	buf[BUFFER_SIZE - 1] = '\0';
	va_end(ap);

	trace_printk("%s", buf);
}
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
		ret = _kalRequestFirmware(&fw_entry, apucNameTable[ucNameIdx],
				       prGlueInfo->prDev);

		if (ret) {
			DBGLOG(INIT, TRACE,
			       "Request FW image: %s failed, errno[%d]\n",
			       apucNameTable[ucNameIdx], fgResult);
			continue;
		} else {
			DBGLOG(INIT, INFO, "Request FW image: %s done\n",
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
	int ret = 0;

	for (sub_idx = 0; apucNameTable[sub_idx]; sub_idx++) {
		if ((*pucNameIdx + 3) >= ucMaxNameIdx) {
			/* the table is not large enough */
			DBGLOG(INIT, ERROR,
			       "kalFirmwareImageMapping >> file name array is not enough.\n");
			ASSERT(0);
			continue;
		}

		/* Type 1. WIFI_RAM_CODE_MTxxxx */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
			"%s%x", apucNameTable[sub_idx], chip_id);
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);

		/* Type 2. WIFI_RAM_CODE_MTxxxx.bin */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
			 "%s%x.bin",
			 apucNameTable[sub_idx], chip_id);
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);

		/* Type 3. WIFI_RAM_CODE_MTxxxx_Ex */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
			 "%s%x_E%u",
			 apucNameTable[sub_idx], chip_id,
			 wlanGetEcoVersion(prGlueInfo->prAdapter));
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);

		/* Type 4. WIFI_RAM_CODE_MTxxxx_Ex.bin */
		ret = kalSnprintf(*(apucName + (*pucNameIdx)), FILE_NAME_MAX,
			 "%s%x_E%u.bin",
			 apucNameTable[sub_idx], chip_id,
				 wlanGetEcoVersion(prGlueInfo->prAdapter));
		if (ret >= 0 && ret < CFG_FW_NAME_MAX_LEN)
			(*pucNameIdx) += 1;
		else
			DBGLOG(INIT, ERROR,
					"[%u] kalSnprintf failed, ret: %d\n",
					__LINE__, ret);
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
	uint8_t idx = 0, max_idx,
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
			else
				kalSnprintf(apucName[idx], FILE_NAME_MAX,
					"mt%x_patch_e%x_hdr.bin", chip_id,
					wlanGetEcoVersion(
						prGlueInfo->prAdapter));
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
			if (!prFwBuffer) {
				DBGLOG(INIT, ERROR, "vmalloc(%u) failed\n",
					ALIGN_4(u4FwSize));
				kalFirmwareClose(prGlueInfo);
				break;
			}
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
		/*DBGLOG(INIT, LOUD, "SPIN_LOCK[%u] Acq\n", rLockCategory);*/
#if CFG_USE_SPIN_LOCK_BOTTOM_HALF
		spin_lock_bh(&prGlueInfo->rSpinLock[rLockCategory]);
#else /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
		spin_lock_irqsave(&prGlueInfo->rSpinLock[rLockCategory],
				  ulFlags);
#endif /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */

		*plFlags = ulFlags;

		/*DBGLOG(INIT, LOUD, "SPIN_LOCK[%u] Acqed\n", rLockCategory);*/
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
		/* DBGLOG(INIT, LOUD, "SPIN_UNLOCK[%u]\n", rLockCategory); */
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

	DBGLOG(INIT, INFO,
			MACSTR ", " MACSTR ".\n",
			prGlueInfo->prDevHandler->dev_addr,
			MAC2STR(pucMacAddr));

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
			/* printk("Wrong skb->protocol( = %08x) for TX Checksum
			 *        Offload.\n", skb->protocol);
			 */
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

	prChipInfo = prGlueInfo->prAdapter->chip_info;
	u4TxHeadRoomSize = NIC_TX_DESC_AND_PADDING_LENGTH +
			   prChipInfo->txd_append_size;

	if (in_interrupt())
		prSkb = __dev_alloc_skb(u4Size + u4TxHeadRoomSize,
					GFP_ATOMIC | __GFP_NOWARN);
	else
		prSkb = __dev_alloc_skb(u4Size + u4TxHeadRoomSize,
					GFP_KERNEL);

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

	if (skb_tailroom(skb) < 0 || u4PacketLen > skb_tailroom(skb)) {
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
		prGlueInfo->rTimeoutWakeLock, MSEC_TO_JIFFIES(
		prGlueInfo->prAdapter->rWifiVar.u4WakeLockRxTimeout));

	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_RX_GRO
/*----------------------------------------------------------------------------*/
/*!
 * \brief To indicate Tput is higher than ucGROEnableTput or not.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 *
 * \retval TRUE Success.
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t kal_is_skb_gro(struct ADAPTER *prAdapter, uint8_t ucBssIdx)
{
	struct PERF_MONITOR *prPerMonitor;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

	prPerMonitor = &prAdapter->rPerMonitor;
	if (prPerMonitor->ulRxTp[ucBssIdx] > prWifiVar->ucGROEnableTput)
		return 1;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief flush Rx packet to kernel if kernel buffer is full or timeout(1ms)
 *
 * @param[in] prAdapter Pointer to the Adapter structure.
 *
 * @retval VOID
 *
 */
/*----------------------------------------------------------------------------*/
void kal_gro_flush(struct ADAPTER *prAdapter, struct net_device *prDev)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
			(struct NETDEV_PRIVATE_GLUE_INFO *) netdev_priv(prDev);

	if (CHECK_FOR_TIMEOUT(kalGetTimeTick(),
		prNetDevPrivate->tmGROFlushTimeout,
		prWifiVar->ucGROFlushTimeout)) {
		napi_gro_flush(&prNetDevPrivate->napi, false);
		DBGLOG_LIMITED(INIT, TRACE, "napi_gro_flush:%p\n", prDev);
	}
	GET_CURRENT_SYSTIME(&prNetDevPrivate->tmGROFlushTimeout);
}
#endif
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
	uint8_t ucBssIdx;
#if CFG_SUPPORT_RX_GRO
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = NULL;
#endif

	ASSERT(prGlueInfo);
	ASSERT(pvPkt);

	prSkb = pvPkt;
	prChipInfo = prGlueInfo->prAdapter->chip_info;
	ucBssIdx = GLUE_GET_PKT_BSS_IDX(prSkb);
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

	if (ucBssIdx < MAX_BSSID_NUM) {
		prNetDev = (struct net_device *)wlanGetNetInterfaceByBssIdx(
			   prGlueInfo, ucBssIdx);
	} else {
		DBGLOG(RX, WARN, "Error ucBssIdx =%x\n", ucBssIdx);
		DBGLOG(RX, WARN, "Error pkt info =%x:%x:%x:%x:%x:%x:%x:%x\n",
			GLUE_GET_PKT_TID(prSkb),
			GLUE_IS_PKT_FLAG_SET(prSkb),
			GLUE_GET_PKT_HEADER_LEN(prSkb),
			GLUE_GET_PKT_FRAME_LEN(prSkb),
			GLUE_GET_PKT_ARRIVAL_TIME(prSkb),
			GLUE_GET_PKT_IP_ID(prSkb),
			GLUE_GET_PKT_SEQ_NO(prSkb),
			GLUE_GET_PKT_IS_PROF_MET(prSkb));
	}
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
#if CFG_SUPPORT_PERF_IND
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

#if (CFG_SUPPORT_STATISTICS == 1)
	StatsEnvRxTime2Host(prGlueInfo->prAdapter, prSkb);
#endif

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

	kalTraceEvent("Rx id=0x%04x sn=%d",
		GLUE_GET_PKT_IP_ID(prSkb),
		GLUE_GET_PKT_SEQ_NO(prSkb));

#if CFG_SUPPORT_RX_GRO
	if (ucBssIdx < MAX_BSSID_NUM &&
		kal_is_skb_gro(prGlueInfo->prAdapter, ucBssIdx)) {
		/* GRO receive function can't be interrupt so it need to
		 * disable preempt and protect by spin lock
		 */
		preempt_disable();
		prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prNetDev);
		spin_lock_bh(&prNetDevPrivate->napi_spinlock);
		napi_gro_receive(&prNetDevPrivate->napi, prSkb);
		kal_gro_flush(prGlueInfo->prAdapter, prNetDev);
		spin_unlock_bh(&prNetDevPrivate->napi_spinlock);
		preempt_enable();
		DBGLOG_LIMITED(INIT, INFO, "napi_gro_receive:%p\n", prNetDev);
		return WLAN_STATUS_SUCCESS;
	}
#endif
	if (!in_interrupt())
		netif_rx_ni(prSkb);
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
			     IN uint32_t u4BufLen, IN uint8_t ucBssIndex)
{

	uint32_t bufLen;
	struct PARAM_STATUS_INDICATION *pStatus;
	struct PARAM_AUTH_EVENT *pAuth;
	struct PARAM_PMKID_CANDIDATE_LIST *pPmkid;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	struct PARAM_SSID ssid = {0};
	struct ieee80211_channel *prChannel = NULL;
	struct cfg80211_bss *bss = NULL;
	uint8_t ucChannelNum;
	struct BSS_DESC *prBssDesc = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t fgScanAborted = FALSE;
	struct net_device *prDevHandler;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	struct FT_IES *prFtIEs;

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	struct cfg80211_roam_info rRoamInfo = { 0 };
#endif

	GLUE_SPIN_LOCK_DECLARATION();

	kalMemZero(arBssid, MAC_ADDR_LEN);

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	pStatus = (struct PARAM_STATUS_INDICATION *)pvBuf;
	pAuth = (struct PARAM_AUTH_EVENT *)pStatus;
	pPmkid = (struct PARAM_PMKID_CANDIDATE_LIST *)(pStatus + 1);

	prDevHandler = wlanGetNetDev(prGlueInfo, ucBssIndex);

	switch (eStatus) {
	case WLAN_STATUS_ROAM_OUT_FIND_BEST:
	case WLAN_STATUS_MEDIA_CONNECT:
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
		/* clear the count */
		prGlueInfo->prAdapter->rLinkQualityInfo.u8TxTotalCount = 0;
		prGlueInfo->prAdapter->rLinkQualityInfo.u8RxTotalCount = 0;
		prGlueInfo->prAdapter->rLinkQualityInfo.u8RxErrCount = 0;
#endif /* CFG_SUPPORT_LINK_QUALITY_MONITOR */
		kalSetMediaStateIndicated(prGlueInfo,
			MEDIA_STATE_CONNECTED,
			ucBssIndex);

		/* indicate assoc event */
		SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
		wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
					&arBssid[0], sizeof(arBssid), &bufLen);
		wext_indicate_wext_event(prGlueInfo, SIOCGIWAP, arBssid,
					 bufLen, ucBssIndex);

		/* switch netif on */
		netif_carrier_on(prDevHandler);

		do {

			uint8_t aucSsid[PARAM_MAX_LEN_SSID + 1] = {0};

			/* print message on console */
			SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
			wlanQueryInformation(prGlueInfo->prAdapter,
			     wlanoidQuerySsid, &ssid, sizeof(ssid), &bufLen);

			kalStrnCpy(aucSsid, ssid.aucSsid, sizeof(aucSsid) - 1);
			aucSsid[sizeof(aucSsid) - 1] = '\0';

			DBGLOG(INIT, INFO,
				"[wifi] %s netif_carrier_on [ssid:%s " MACSTR
				"], Mac:" MACSTR "\n",
				prGlueInfo->prDevHandler->name, aucSsid,
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
					ucBssIndex);
			if (ucChannelNum <= 14) {
				prChannel =
					ieee80211_get_channel(
						priv_to_wiphy(prGlueInfo),
						ieee80211_channel_to_frequency
						(ucChannelNum, KAL_BAND_2GHZ));
			} else {
				prChannel =
					ieee80211_get_channel(
						priv_to_wiphy(prGlueInfo),
						ieee80211_channel_to_frequency
						(ucChannelNum, KAL_BAND_5GHZ));
			}

			if (!prChannel)
				DBGLOG(SCN, ERROR,
				       "prChannel is NULL and ucChannelNum is %d\n",
				       ucChannelNum);

			/* ensure BSS exists */
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
			bss = cfg80211_get_bss(
				priv_to_wiphy(prGlueInfo), prChannel, arBssid,
				ssid.aucSsid, ssid.u4SsidLen,
				IEEE80211_BSS_TYPE_ESS,
				IEEE80211_PRIVACY_ANY);
#else
			bss = cfg80211_get_bss(
				priv_to_wiphy(prGlueInfo), prChannel, arBssid,
				ssid.aucSsid, ssid.u4SsidLen,
				WLAN_CAPABILITY_ESS,
				WLAN_CAPABILITY_ESS);
#endif
			if (bss == NULL) {
				/* create BSS on-the-fly */
				prBssDesc =
					aisGetTargetBssDesc(prAdapter,
					ucBssIndex);

				if (prBssDesc != NULL && prChannel != NULL) {
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
					bss = cfg80211_inform_bss(
			priv_to_wiphy(prGlueInfo),
			prChannel,
			CFG80211_BSS_FTYPE_PRESP,
			arBssid,
			0, /* TSF */
			prBssDesc->u2CapInfo,
			prBssDesc->u2BeaconInterval, /* beacon interval */
			prBssDesc->aucIEBuf, /* IE */
			prBssDesc->u2IELength, /* IE Length */
			RCPI_TO_dBm(prBssDesc->ucRCPI) * 100, /* MBM */
			GFP_KERNEL);
#else
					bss = cfg80211_inform_bss(
			priv_to_wiphy(prGlueInfo),
			prChannel,
			arBssid,
			0, /* TSF */
			prBssDesc->u2CapInfo,
			prBssDesc->u2BeaconInterval, /* beacon interval */
			prBssDesc->aucIEBuf, /* IE */
			prBssDesc->u2IELength, /* IE Length */
			RCPI_TO_dBm(prBssDesc->ucRCPI) * 100, /* MBM */
			GFP_KERNEL);
#endif
				}
			}

#if (CFG_SUPPORT_STATISTICS == 1)
			StatsResetTxRx();
#endif
			/* remove all bsses that before and only channel
			 * different with the current connected one
			 * if without this patch, UI will show channel A is
			 * connected even if AP has change channel from A to B
			 */
			while (ucLoopCnt--) {
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
				bss_others = cfg80211_get_bss(
						priv_to_wiphy(prGlueInfo),
						NULL, arBssid, ssid.aucSsid,
						ssid.u4SsidLen,
						IEEE80211_BSS_TYPE_ESS,
						IEEE80211_PRIVACY_ANY);
#else
				bss_others = cfg80211_get_bss(
						priv_to_wiphy(prGlueInfo),
						NULL, arBssid, ssid.aucSsid,
						ssid.u4SsidLen,
						WLAN_CAPABILITY_ESS,
						WLAN_CAPABILITY_ESS);
#endif
				if (bss && bss_others && bss_others != bss) {
					DBGLOG(SCN, INFO,
					       "remove BSSes that only channel different\n");
					cfg80211_unlink_bss(
						priv_to_wiphy(prGlueInfo),
						bss_others);
					cfg80211_put_bss(
						priv_to_wiphy(prGlueInfo),
						bss_others);
				} else
					break;
			}

			/* CFG80211 Indication */
			prConnSettings =
				aisGetConnSettings(prAdapter, ucBssIndex);
			if (eStatus == WLAN_STATUS_ROAM_OUT_FIND_BEST) {
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
				rRoamInfo.bss = bss;
				rRoamInfo.req_ie = prConnSettings->aucReqIe;
				rRoamInfo.req_ie_len =
					prConnSettings->u4ReqIeLength;
				rRoamInfo.resp_ie = prConnSettings->aucRspIe;
				rRoamInfo.resp_ie_len =
					prConnSettings->u4RspIeLength;

				cfg80211_roamed(prDevHandler,
					&rRoamInfo, GFP_KERNEL);
#else
				cfg80211_roamed_bss(
					prDevHandler,
					bss,
					prConnSettings->aucReqIe,
					prConnSettings->u4ReqIeLength,
					prConnSettings->aucRspIe,
					prConnSettings->u4RspIeLength,
					GFP_KERNEL);
#endif
			} else {
				cfg80211_connect_result(
					prDevHandler,
					arBssid,
					prConnSettings->aucReqIe,
					prConnSettings->u4ReqIeLength,
					prConnSettings->aucRspIe,
					prConnSettings->u4RspIeLength,
					WLAN_STATUS_SUCCESS,
					GFP_KERNEL);
			}

			/* Check SAP channel */
			p2pFuncSwitchSapChannel(prGlueInfo->prAdapter);

		}

		break;

	case WLAN_STATUS_MEDIA_DISCONNECT:
	case WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY:
#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
		/* clear the count */
		prGlueInfo->prAdapter->rLinkQualityInfo.u8TxTotalCount = 0;
		prGlueInfo->prAdapter->rLinkQualityInfo.u8RxTotalCount = 0;
		prGlueInfo->prAdapter->rLinkQualityInfo.u8RxErrCount = 0;
#endif
		/* indicate disassoc event */
		wext_indicate_wext_event(prGlueInfo, SIOCGIWAP, NULL, 0
			, ucBssIndex);
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
		       prDevHandler->name);
#endif

		netif_carrier_off(prDevHandler);

		/* Full2Partial: reset */
		if (prGlueInfo->prAdapter) {
			struct SCAN_INFO *prScanInfo =
				&(prGlueInfo->prAdapter->rWifiVar.rScanInfo);
			prScanInfo->fgIsScanForFull2Partial = FALSE;
			prScanInfo->u4LastFullScanTime = 0;
		}

		if (prGlueInfo->fgIsRegistered == TRUE) {
			struct BSS_INFO *prBssInfo =
				aisGetAisBssInfo(prAdapter, ucBssIndex);
			uint16_t u2DeauthReason = 0;
#if CFG_WPS_DISCONNECT || (KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE)

			if (prBssInfo)
				u2DeauthReason = prBssInfo->u2DeauthReason;

#if CFG_SUPPORT_BIGDATA_PIP
			{
			struct _REPORT_DISCONNECT {
				uint16_t	u2Id;
				uint16_t	u2Len;
				uint8_t  ucReasonType;
				uint16_t  u2ReasonCode;
				int8_t  cRssi;
				uint16_t  u2DataRate;
				uint8_t  ucChannelNo;
			} __KAL_ATTRIB_PACKED__;

			struct _REPORT_DISCONNECT rPayload = {0};
			struct BSS_DESC *prTemp = NULL;
			int8_t ret = 0;

			/* u2Id: Define category of report data.
			 * 0 - Antswp
			 * 1 - GPS Blank On
			 * 2 - Disconnect Reason
			 * others - Reserved
			 *
			 * u2Len: length of data.
			 * Report data format:
			 * | u2Id | u2Len | data |
			 */
			rPayload.u2Id = 2;
			rPayload.u2Len = 7;

			if (eStatus ==
			WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY) {
				rPayload.ucReasonType = 2;
				rPayload.u2ReasonCode = 0;
			} else if (u2DeauthReason >=
				REASON_CODE_BEACON_TIMEOUT*100) {
				rPayload.ucReasonType = 0;
				rPayload.u2ReasonCode =
				u2DeauthReason - REASON_CODE_BEACON_TIMEOUT*100;
			} else {
				rPayload.ucReasonType = 1;
				rPayload.u2ReasonCode = u2DeauthReason;
			}

			if (prGlueInfo->prAdapter) {
				rPayload.cRssi =
					prGlueInfo->prAdapter->rLinkQuality
					.rLq[ucBssIndex].cRssi;
				rPayload.u2DataRate =
					prGlueInfo->prAdapter->rLinkQuality
					.rLq[ucBssIndex].u2LinkSpeed;

				prTemp =
					aisGetTargetBssDesc(
						prGlueInfo->prAdapter,
						ucBssIndex);

				if (prTemp)
					rPayload.ucChannelNo =
						prTemp->ucChannelNum;

				DBGLOG(INIT, TRACE,
		    "[D2F]type(%u)-reason(%u)-rssi(%d)-speed(%u)-channel(%u)\n",
					rPayload.ucReasonType,
					rPayload.u2ReasonCode,
					rPayload.cRssi,
					rPayload.u2DataRate,
					rPayload.ucChannelNo);

				/* dumpMemory8((uint8_t *)&rPayload,
				 * sizeof(rPayload));
				 */

				ret = kalBigDataPip(prGlueInfo->prAdapter,
					(uint8_t *)&rPayload,
					sizeof(rPayload));

				if ((ret != 1) && (ret != -ETIME))
					DBGLOG(INIT, ERROR,
					"[D2F]data pip report fail(%d).\n",
					ret);

			}
			}
#endif
			/* CFG80211 Indication */
			DBGLOG(INIT, INFO,
			    "[wifi]Indicate disconnection: Reason=%d Locally[%d]\n",
			    u2DeauthReason,
			    (eStatus ==
				WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY));
			cfg80211_disconnected(prDevHandler,
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
				cfg80211_disconnected(prDevHandler,
						      u2DeauthReason, NULL, 0,
						      GFP_KERNEL);
			}


#endif
		}
		prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
		if (prConnSettings && prConnSettings->assocIeLen > 0) {
			kalMemFree(prConnSettings->pucAssocIEs, VIR_MEM_TYPE,
				   prConnSettings->assocIeLen);
			prConnSettings->assocIeLen = 0;
		}

		prFtIEs = aisGetFtIe(prAdapter, ucBssIndex);
		if (prFtIEs) {
			kalMemFree(prFtIEs->pucIEBuf,
				VIR_MEM_TYPE,
				prFtIEs->u4IeLength);
			kalMemZero(prFtIEs,
				sizeof(*prFtIEs));
		}

		kalSetMediaStateIndicated(prGlueInfo,
			MEDIA_STATE_DISCONNECTED,
			ucBssIndex);

		/* Check SAP channel */
		p2pFuncSwitchSapChannel(prGlueInfo->prAdapter);

		break;

	case WLAN_STATUS_SCAN_COMPLETE:
		if (pvBuf && u4BufLen == sizeof(uint8_t))
			fgScanAborted = *(uint8_t *)pvBuf;

		/* indicate scan complete event */
		wext_indicate_wext_event(prGlueInfo, SIOCGIWSCAN, NULL, 0,
			ucBssIndex);

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
		if (netif_running(prDevHandler))
			netif_wake_queue(prDevHandler);
		break;
#endif

	case WLAN_STATUS_MEDIA_SPECIFIC_INDICATION:
		if (pStatus) {
			switch (pStatus->eStatusType) {
			case ENUM_STATUS_TYPE_AUTHENTICATION:
			{
				struct PARAM_INDICATION_EVENT *prEvent =
				(struct PARAM_INDICATION_EVENT *) pvBuf;

				/*
				 *  printk(KERN_NOTICE
				 *      "ENUM_STATUS_TYPE_AUTHENTICATION:
				 *      L(%d) [" MACSTR "] F:%lx\n",
				 *      pAuth->Request[0].Length,
				 *      MAC2STR(pAuth->Request[0].Bssid),
				 *      pAuth->Request[0].Flags);
				 */
				/* indicate (UC/GC) MIC ERROR event only */
				if ((prEvent->rAuthReq.u4Flags ==
				     PARAM_AUTH_REQUEST_PAIRWISE_ERROR) ||
				    (prEvent->rAuthReq.u4Flags ==
				     PARAM_AUTH_REQUEST_GROUP_ERROR)) {
					cfg80211_michael_mic_failure(
					    prDevHandler, NULL,
					    (prEvent->rAuthReq.u4Flags ==
					    PARAM_AUTH_REQUEST_PAIRWISE_ERROR)
						? NL80211_KEYTYPE_PAIRWISE :
						NL80211_KEYTYPE_GROUP,
					    0, NULL, GFP_KERNEL);
					wext_indicate_wext_event(prGlueInfo,
					    IWEVMICHAELMICFAILURE,
					    (unsigned char *)
						&prEvent->rAuthReq,
					    prEvent->rAuthReq.u4Length,
					    ucBssIndex);
				}
				break;
			}
			case ENUM_STATUS_TYPE_CANDIDATE_LIST:
			{
				struct PARAM_INDICATION_EVENT *prEvent =
				(struct PARAM_INDICATION_EVENT *) pvBuf;

				cfg80211_pmksa_candidate_notify(
					prDevHandler,
					1000,
					prEvent->rCandi.arBSSID,
					prEvent->rCandi.u4Flags,
					GFP_KERNEL);

				wext_indicate_wext_event(
					prGlueInfo,
					IWEVPMKIDCAND,
					(unsigned char *) &prEvent->rCandi,
					sizeof(struct PARAM_PMKID_CANDIDATE),
					ucBssIndex);

				break;
			}
			case ENUM_STATUS_TYPE_FT_AUTH_STATUS: {
				struct cfg80211_ft_event_params *prFtEvent =
					aisGetFtEventParam(prAdapter,
					ucBssIndex);

				cfg80211_ft_event(prDevHandler,
						  prFtEvent);
			}
				break;

			default:
				/* case ENUM_STATUS_TYPE_MEDIA_STREAM_MODE */
				/*
				 * printk(KERN_NOTICE "unknown media specific
				 * indication type:%x\n", pStatus->StatusType);
				 */
				break;
			}
		} else {
			/*
			 * printk(KERN_WARNING "media specific indication
			 * buffer NULL\n");
			 */
		}
		break;

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	case WLAN_STATUS_BWCS_UPDATE: {
		wext_indicate_wext_event(prGlueInfo, IWEVCUSTOM, pvBuf,
					 sizeof(struct PTA_IPC), ucBssIndex);
	}

	break;

#endif
	case WLAN_STATUS_JOIN_FAILURE: {
		struct BSS_DESC *prBssDesc =
			aisGetTargetBssDesc(prAdapter, ucBssIndex);
		struct CONNECTION_SETTINGS *prConnSettings =
			aisGetConnSettings(prAdapter, ucBssIndex);

		if (prBssDesc) {
			DBGLOG(INIT, INFO, "JOIN Failure: u2JoinStatus=%d",
				prBssDesc->u2JoinStatus);
			COPY_MAC_ADDR(arBssid, prBssDesc->aucBSSID);
		} else {
			DBGLOG(INIT, INFO, "JOIN Failure: No TargetBssDesc");
			COPY_MAC_ADDR(arBssid,
				prConnSettings->aucBSSID);
		}
		if (prBssDesc && prBssDesc->u2JoinStatus
		    && prBssDesc->u2JoinStatus != STATUS_CODE_AUTH_TIMEOUT
		    && prBssDesc->u2JoinStatus != STATUS_CODE_ASSOC_TIMEOUT)
			cfg80211_connect_result(prDevHandler,
				arBssid,
				prConnSettings->aucReqIe,
				prConnSettings->u4ReqIeLength,
				prConnSettings->aucRspIe,
				prConnSettings->u4RspIeLength,
				prBssDesc->u2JoinStatus,
				GFP_KERNEL);
		else
			cfg80211_connect_result(prDevHandler,
				arBssid,
				prConnSettings->aucReqIe,
				prConnSettings->u4ReqIeLength,
				prConnSettings->aucRspIe,
				prConnSettings->u4RspIeLength,
				WLAN_STATUS_AUTH_TIMEOUT,
				GFP_KERNEL);
		kalSetMediaStateIndicated(prGlueInfo,
			MEDIA_STATE_DISCONNECTED,
			ucBssIndex);

		/* Check SAP channel */
		p2pFuncSwitchSapChannel(prGlueInfo->prAdapter);

		break;
	}
	default:
		/*
		 *  printk(KERN_WARNING "unknown indication:%lx\n", eStatus);
		 */
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
			IN u_int8_t fgReassocRequest,
			IN uint8_t ucBssIndex)
{
	uint8_t *cp;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;

	ASSERT(prGlueInfo);

	prConnSettings = aisGetConnSettings(
		prGlueInfo->prAdapter,
		ucBssIndex);

	/* reset */
	prConnSettings->u4ReqIeLength = 0;

	if (fgReassocRequest) {
		if (u4FrameBodyLen < 15) {
			/*
			 * printk(KERN_WARNING "frameBodyLen too short:%d\n",
			 * frameBodyLen);
			 */
			return;
		}
	} else {
		if (u4FrameBodyLen < 9) {
			/*
			 * printk(KERN_WARNING "frameBodyLen too short:%d\n",
			 * frameBodyLen);
			 */
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
				 u4FrameBodyLen, ucBssIndex);

	if (u4FrameBodyLen <= CFG_CFG80211_IE_BUF_LEN) {
		prConnSettings->u4ReqIeLength = u4FrameBodyLen;
		kalMemCopy(prConnSettings->aucReqIe, cp, u4FrameBodyLen);
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
			     IN uint32_t u4FrameBodyLen,
			     IN uint8_t ucBssIndex)
{
	uint32_t u4IEOffset =
		6;	/* cap_info, status_code & assoc_id */
	uint32_t u4IELength = u4FrameBodyLen - u4IEOffset;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;

	ASSERT(prGlueInfo);

	prConnSettings = aisGetConnSettings(
		prGlueInfo->prAdapter,
		ucBssIndex);

	/* reset */
	prConnSettings->u4RspIeLength = 0;

	if (u4IELength <= CFG_CFG80211_IE_BUF_LEN) {
		prConnSettings->u4RspIeLength = u4IELength;
		kalMemCopy(prConnSettings->aucRspIe, pucFrameBody + u4IEOffset,
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
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4TxHeadRoomSize = 0;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prOrgSkb);
	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	prChipInfo = prGlueInfo->prAdapter->chip_info;
	u4TxHeadRoomSize = NIC_TX_DESC_AND_PADDING_LENGTH +
		prChipInfo->txd_append_size;

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

#if CFG_SUPPORT_WIFI_SYSDVT
	if (prAdapter->u2TxTest != TX_TEST_UNLIMITIED) {
		if (prAdapter->u2TxTestCount < prAdapter->u2TxTest) {
			DBGLOG(INIT, STATE,
				"DVT TX Test enable, skip this frame\n");
			dev_kfree_skb(prOrgSkb);
			return WLAN_STATUS_SUCCESS;
		}
		prAdapter->u2TxTestCount++;
	}
#endif /* CFG_SUPPORT_WIFI_SYSDVT */

	if (skb_headroom(prOrgSkb) < u4TxHeadRoomSize) {
		/*
		 * Should not happen
		 * kernel crash may happen as skb shared info
		 * channged.
		 * offer an change for lucky anyway
		 */
		prSkbNew = skb_realloc_headroom(prOrgSkb, u4TxHeadRoomSize);
		if (!prSkbNew) {
			dev_kfree_skb(prOrgSkb);
			DBGLOG(INIT, ERROR,
				"prChipInfo = %pM, u4TxHeadRoomSize: %u\n",
				prChipInfo, u4TxHeadRoomSize);
			return WLAN_STATUS_NOT_ACCEPTED;
		}
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
		kalTraceInt(prTxQueue->u4NumElem, "TxQueue");
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
#if CFG_SUPPORT_PERF_IND
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
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate;

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			netdev_priv(prDev);
	return (void *) &prNetDevPrivate->stats;
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
			DBGLOG(TX, INFO,
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
	uint16_t u2DstPort;
	struct BOOTP_PROTOCOL *prBootp;
	uint32_t u4DhcpMagicCode;
	uint16_t u2IpId;
#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_TCP_IP_CHKSUM_OFFLOAD)
	struct ADAPTER *prAdapter = NULL;
#endif
#endif /* Automation */

	/* IPv4 version check */
	ucIpVersion = (pucIpHdr[0] & IP_VERSION_MASK) >>
		      IP_VERSION_OFFSET;
	if (ucIpVersion != IP_VERSION_4) {
		DBGLOG(TX, WARN, "Invalid IPv4 packet version: %u\n",
		       ucIpVersion);
		return FALSE;
	}

	ucIpProto = pucIpHdr[IPV4_HDR_IP_PROTOCOL_OFFSET];
	u2IpId = (pucIpHdr[IPV4_ADDR_LEN] << 8) | pucIpHdr[IPV4_ADDR_LEN + 1];
	GLUE_SET_PKT_IP_ID(prPacket, u2IpId);

#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_TCP_IP_CHKSUM_OFFLOAD)
	prAdapter = prGlueInfo->prAdapter;

	/* set IP CHECKSUM to 0xff for verify CSO function */
	if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_IP
		&& CSO_TX_IPV4_ENABLED(prAdapter)) {
		pucIpHdr[IPV4_HDR_IP_CSUM_OFFSET] = 0xff;
		pucIpHdr[IPV4_HDR_IP_CSUM_OFFSET+1] = 0xff;
	}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
#endif /* Automation */

	if (ucIpProto == IP_PRO_UDP) {
		pucUdpHdr = &pucIpHdr[IPV4_HDR_LEN];

#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_TCP_IP_CHKSUM_OFFLOAD)
		/* set UDP CHECKSUM to 0xff for verify CSO function */
		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_UDP
			&& CSO_TX_UDP_ENABLED(prAdapter)) {
			pucUdpHdr[UDP_HDR_UDP_CSUM_OFFSET] = 0xff;
			pucUdpHdr[UDP_HDR_UDP_CSUM_OFFSET+1] = 0xff;
		}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
#endif /* Automation */

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
				ucSeqNo = nicIncreaseTxSeqNum(
							prGlueInfo->prAdapter);
				GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
				prTxPktInfo->u2Flag |= BIT(ENUM_PKT_DHCP);
			}
		} else if (u2DstPort == UDP_PORT_DNS) {
			ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
			GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
			prTxPktInfo->u2Flag |= BIT(ENUM_PKT_DNS);
		}
	} else if (ucIpProto == IP_PRO_ICMP) {
		pucIcmp = &pucIpHdr[20];
		ucIcmpType = pucIcmp[0];
		if (ucIcmpType == 3) /* don't log network unreachable packet */
			return FALSE;
		ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
		GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
		prTxPktInfo->u2Flag |= BIT(ENUM_PKT_ICMP);
	}
#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_TCP_IP_CHKSUM_OFFLOAD)
	else if (ucIpProto == IP_PRO_TCP) {
		uint8_t *pucTcpHdr;

		pucTcpHdr = &pucIpHdr[IPV4_HDR_LEN];
		/* set TCP CHECKSUM to 0xff for verify CSO function */
		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_TCP
			&& CSO_TX_TCP_ENABLED(prAdapter)) {
			pucTcpHdr[TCP_HDR_TCP_CSUM_OFFSET] = 0xff;
			pucTcpHdr[TCP_HDR_TCP_CSUM_OFFSET+1] = 0xff;
		}
	}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
#endif /* Automation */

	return TRUE;
}

u_int8_t
kalIPv6FrameClassifier(IN struct GLUE_INFO *prGlueInfo,
		       IN void *prPacket, IN uint8_t *pucIpv6Hdr,
		       OUT struct TX_PACKET_INFO *prTxPktInfo)
{
	uint8_t ucIpv6Proto;
	uint8_t *pucL3Hdr;
	struct ADAPTER *prAdapter = NULL;

	prAdapter = prGlueInfo->prAdapter;
	ucIpv6Proto = pucIpv6Hdr[IPV6_HDR_IP_PROTOCOL_OFFSET];

	if (ucIpv6Proto == IP_PRO_UDP) {
		pucL3Hdr = &pucIpv6Hdr[IPV6_HDR_LEN];
#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_TCP_IP_CHKSUM_OFFLOAD)
		/* set UDP CHECKSUM to 0xff for verify CSO function */
		if ((prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_UDP)
			&& CSO_TX_UDP_ENABLED(prAdapter)) {
			pucL3Hdr[UDP_HDR_UDP_CSUM_OFFSET] = 0xff;
			pucL3Hdr[UDP_HDR_UDP_CSUM_OFFSET+1] = 0xff;
		}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
#endif /* Automation */
	} else if (ucIpv6Proto == IP_PRO_TCP) {
		pucL3Hdr = &pucIpv6Hdr[IPV6_HDR_LEN];
#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_TCP_IP_CHKSUM_OFFLOAD)
		/* set TCP CHECKSUM to 0xff for verify CSO function */
		if ((prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_TCP)
			&& CSO_TX_TCP_ENABLED(prAdapter)) {
			pucL3Hdr[TCP_HDR_TCP_CSUM_OFFSET] = 0xff;
			pucL3Hdr[TCP_HDR_TCP_CSUM_OFFSET+1] = 0xff;
		}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
#endif /* Automation */
	}

	return TRUE;
}

u_int8_t
kalArpFrameClassifier(IN struct GLUE_INFO *prGlueInfo,
		      IN void *prPacket, IN uint8_t *pucIpHdr,
		      OUT struct TX_PACKET_INFO *prTxPktInfo)
{
	uint8_t ucSeqNo;
	ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
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

		ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
		GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
#if CFG_SUPPORT_WAPI
	} else if (u2EthType == ETH_WPI_1X) {
		ucSeqNo = nicIncreaseTxSeqNum(prGlueInfo->prAdapter);
		GLUE_SET_PKT_SEQ_NO(prPacket, ucSeqNo);
		prTxPktInfo->u2Flag |= BIT(ENUM_PKT_NON_PROTECTED_1X);
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
			ucUserPriority = getUpFromDscp(
				prGlueInfo,
				GLUE_GET_PKT_BSS_IDX(prSkb),
				(pucNextProtocol[1] >> 2) & 0x3F);
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

	case ETH_P_IPV6:
#if CFG_SUPPORT_WIFI_SYSDVT
#if (CFG_TCP_IP_CHKSUM_OFFLOAD)
		kalIPv6FrameClassifier(prGlueInfo, prPacket,
				       pucNextProtocol, prTxPktInfo);
#endif
#endif

#if DSCP_SUPPORT
		if (GLUE_GET_PKT_BSS_IDX(prSkb) != P2P_DEV_BSS_INDEX) {
			uint16_t u2Tmp;
			uint8_t ucIpTos;

			WLAN_GET_FIELD_BE16(pucNextProtocol, &u2Tmp);
			ucIpTos = u2Tmp >> 4;

			ucUserPriority = getUpFromDscp(
				prGlueInfo,
				GLUE_GET_PKT_BSS_IDX(prSkb),
				(ucIpTos >> 2) & 0x3F);
			if (ucUserPriority != 0xFF)
				prSkb->priority = ucUserPriority;
		}
#endif
		break;

	default:
		/* 4 <4> Handle 802.3 format if LEN <= 1500 */
		if (u2EtherTypeLen <= ETH_802_3_MAX_LEN)
			prTxPktInfo->u2Flag |= BIT(ENUM_PKT_802_3);
		break;
	}

	STATS_TX_PKT_INFO_DISPLAY(prSkb);

	/* 4 <4.1> Check for PAL (BT over Wi-Fi) */
	/* Move to kalBowFrameClassifier */

	/* 4 <5> Return the value of Priority Parameter. */
	/* prSkb->priority is assigned by Linux wireless utility
	 * function(cfg80211_classify8021d)
	 */
	/* at net_dev selection callback (ndo_select_queue) */
	prTxPktInfo->ucPriorityParam = prSkb->priority;

#if CFG_CHANGE_PRIORITY_BY_SKB_MARK_FIELD
	/* Raise priority for special packet if skb mark filed
	 * marked with pre-defined value.
	 */
	if (prSkb->mark == NIC_TX_SKB_PRIORITY_MARK1 ||
	    (prSkb->mark & BIT(NIC_TX_SKB_PRIORITY_MARK_BIT))) {
		prTxPktInfo->ucPriorityParam = NIC_TX_PRIORITY_DATA_TID;
		DBGLOG_LIMITED(INIT, TRACE,
				"skb mark field=[%x]", prSkb->mark);
	}
#endif

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
		/* WARN_ON(TRUE); */
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

	if (!pThread || kalIsResetting())
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

uint8_t GET_IOCTL_BSSIDX(
	IN struct ADAPTER *prAdapter)
{
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	if (prAdapter) {
		struct GL_IO_REQ *prIoReq = NULL;

		prIoReq =
			&(prAdapter->prGlueInfo
			->OidEntry);

		ucBssIndex = prIoReq->ucBssIndex;
	}

	return ucBssIndex;
}

void SET_IOCTL_BSSIDX(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex)
{
	if (prAdapter) {
		struct GL_IO_REQ *prIoReq = NULL;

		prIoReq =
			&(prAdapter->prGlueInfo
			->OidEntry);

		prIoReq->ucBssIndex = ucBssIndex;
	}
}

uint32_t
kalIoctl(IN struct GLUE_INFO *prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN void *pvInfoBuf,
	 IN uint32_t u4InfoBufLen, IN u_int8_t fgRead,
	 IN u_int8_t fgWaitResp, IN u_int8_t fgCmd,
	 OUT uint32_t *pu4QryInfoLen)
{
	return kalIoctlByBssIdx(
		prGlueInfo,
		pfnOidHandler,
		pvInfoBuf,
		u4InfoBufLen,
		fgRead,
		fgWaitResp,
		fgCmd,
		pu4QryInfoLen,
		AIS_DEFAULT_INDEX);
}

uint32_t
kalIoctlByBssIdx(IN struct GLUE_INFO *prGlueInfo,
	 IN PFN_OID_HANDLER_FUNC pfnOidHandler,
	 IN void *pvInfoBuf,
	 IN uint32_t u4InfoBufLen, IN u_int8_t fgRead,
	 IN u_int8_t fgWaitResp, IN u_int8_t fgCmd,
	 OUT uint32_t *pu4QryInfoLen,
	 IN uint8_t ucBssIndex)
{
	struct GL_IO_REQ *prIoReq = NULL;
	struct KAL_THREAD_SCHEDSTATS schedstats;
	uint32_t ret = WLAN_STATUS_SUCCESS;
	uint32_t waitRet = 0;

	KAL_TIME_INTERVAL_DECLARATION();

	KAL_REC_TIME_START();

	if (kalIsResetting())
		return WLAN_STATUS_SUCCESS;

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);

	if (wlanIsChipAssert(prGlueInfo->prAdapter))
		return WLAN_STATUS_SUCCESS;

	/* GLUE_SPIN_LOCK_DECLARATION(); */

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

	if (prGlueInfo->main_thread == NULL) {
		dump_stack();
		DBGLOG(OID, WARN, "skip executing request.\n");
		up(&prGlueInfo->ioctl_sem);
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
	SET_IOCTL_BSSIDX(
		prGlueInfo->prAdapter,
		ucBssIndex);

	/* <5> Reset the status of pending OID */
	prGlueInfo->rPendStatus = WLAN_STATUS_FAILURE;
	/* prGlueInfo->u4TimeoutFlag = 0; */
	prGlueInfo->u4OidCompleteFlag = 0;

	/* <6> Check if we use the command queue */
	prIoReq->u4Flag = fgCmd;

	/* <7> schedule the OID bit
	 * Use memory barrier to ensure OidEntry is written done and then set
	 * bit.
	 */
	smp_mb();
	set_bit(GLUE_FLAG_OID_BIT, &prGlueInfo->ulFlag);

	/* <7.1> Hold wakelock to ensure OS won't be suspended */
	KAL_WAKE_LOCK_TIMEOUT(prGlueInfo->prAdapter,
		prGlueInfo->rTimeoutWakeLock, MSEC_TO_JIFFIES(
		prGlueInfo->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	/* <8> Wake up main thread to handle kick start the I/O request.
	 * Use memory barrier to ensure set bit is done and then wake up main
	 * thread.
	 */
	smp_mb();
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
	} else {

#if 0
		/* Case 2: timeout */
		/* clear pending OID's cmd in CMD queue */
		if (fgCmd) {
			prGlueInfo->u4TimeoutFlag = 1;
			wlanReleasePendingOid(prGlueInfo->prAdapter, 0);
		}
#endif
		/* note: do not dump main_thread's call stack here, */
		/*       because it may be running on other cpu.    */
		DBGLOG(OID, WARN,
			"wait main_thread timeout, duration:%llums, sched(x%llu/r%llu/i%llu)\n",
			schedstats.time, schedstats.exec,
			schedstats.runnable, schedstats.iowait);

		ret = WLAN_STATUS_FAILURE;
	}

	/* <10> Clear bit for error handling */
	clear_bit(GLUE_FLAG_OID_BIT, &prGlueInfo->ulFlag);

	up(&prGlueInfo->ioctl_sem);
	up(&g_halt_sem);

	KAL_REC_TIME_END();
	if (ret != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, WARN, "ret(%x) time: %lu us\n",
			ret,
			KAL_GET_TIME_INTERVAL());

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

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME ||
			prCmdInfo->eCmdType == COMMAND_TYPE_DATA_FRAME) {
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

		if ((prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME ||
			prCmdInfo->eCmdType == COMMAND_TYPE_DATA_FRAME)
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

#if CFG_SUPPORT_LOWLATENCY_MODE
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to check use packet data into command Queue flow
 *
 * \param prAdapter     Pointer of prAdapter Structure
 * \param prAdapter     Pointer of prAdapter Structure
 * \retval is TRUE (run command Data path flow) or FALSE (not use)
 */
/*----------------------------------------------------------------------------*/
u_int8_t kalIsCommandDataPath(IN struct ADAPTER
				  *prAdapter, IN void *prPacket)
{
	struct sk_buff *skb = (struct sk_buff *)prPacket;

	if (prAdapter->rWifiVar.ucLowLatencyCmdData == 0)
		return FALSE;

	if (!prAdapter->fgTxDupCertificate)
		return FALSE;

	if (!prAdapter->fgEnTxDupDetect)
		return FALSE;

	/* Only detect for special mark packet */
	if (!prAdapter->rWifiVar.ucLowLatencyCmdDataAllPacket &&
	    !(skb->mark & BIT(NIC_TX_SKB_DUP_DETECT_MARK_BIT))) {
		DBGLOG_LIMITED(TX, TRACE,
			"mark flag=[%x]\n", skb->mark);
		return FALSE;
	}

	/* Not send special mark flag packets via command path */
	if (GLUE_IS_PKT_FLAG_SET(prPacket)) {
		DBGLOG_LIMITED(TX, TRACE,
			"Packet flag=[%d]\n", GLUE_IS_PKT_FLAG_SET(prPacket));
		return FALSE;
	}

	/* Only handle checksum calculated packets, must not enable
	 * checksum offload for command data packet. Refer to
	 * kalQueryTxChksumOffloadParam()
	 */
	if (skb->ip_summed != CHECKSUM_NONE) {
		DBGLOG_LIMITED(TX, TRACE,
			"Packet checksum not calculated, ip_summed=[%d]",
			skb->ip_summed);
		return FALSE;
	}

	if (prAdapter->tmTxDataInterval > 0 &&
	    !CHECK_FOR_TIMEOUT(kalGetTimeTick(),
				prAdapter->tmTxDataInterval, 10)) {
		DBGLOG_LIMITED(TX, TRACE, "Packet interval too close");
		return FALSE;
	}
	GET_CURRENT_SYSTIME(&prAdapter->tmTxDataInterval);

	DBGLOG_LIMITED(TX, INFO,
	       "Change data to cmd data frame. Packet flag=[%d], ip_summed=[%d] mark=[%x]\n",
		GLUE_IS_PKT_FLAG_SET(prPacket),
		skb->ip_summed,
		skb->mark);

	DBGLOG_LIMITED(TX, INFO,
	       "Change data to cmd data frame. Packet flag=[%d], ip_summed=[%d] mark=[%x]\n",
		GLUE_IS_PKT_FLAG_SET(prPacket),
		skb->ip_summed,
		skb->mark);

	return TRUE;
}
#endif

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
#if CFG_SUPPORT_LOWLATENCY_MODE
	/* Data frame use command path */
	else if (kalIsCommandDataPath(prGlueInfo->prAdapter,
						(void *) prSkb)) {
		u4Status = wlanProcessCmdDataFrame(prGlueInfo->prAdapter,
					       (void *) prSkb);
		if (u4Status == WLAN_STATUS_SUCCESS)
			GLUE_INC_REF_CNT(
				prGlueInfo->i4TxPendingSecurityFrameNum);

	}
#endif
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

			kalTraceBegin("Move TxQueue %d", prTempQue->u4NumElem);
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

				if (wlanGetTxPendingFrameCount(
					    prGlueInfo->prAdapter) > 0)
					wlanTxPendingPackets(
						prGlueInfo->prAdapter,
						pfgNeedHwAccess);
			}
			kalTraceEnd();

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
	bool fgEnInt;
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	KAL_WAKE_LOCK_T *prHifThreadWakeLock;

	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter,
			   prHifThreadWakeLock, "WLAN hif_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, prHifThreadWakeLock);
#endif

	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	prGlueInfo->u4HifThreadPid = KAL_GET_CURRENT_THREAD_ID();

	set_user_nice(current, prAdapter->rWifiVar.cThreadNice);

	while (TRUE) {

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
			|| kalIsResetting()
			) {
			DBGLOG(INIT, INFO, "hif_thread should stop now...\n");
			break;
		}

		/* Unlock wakelock if hif_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_HIF_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter,
					prHifThreadWakeLock);

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

		kalTraceBegin("hif_thread");

#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
					  prHifThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter,
				      prHifThreadWakeLock);
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
		fgEnInt = (prGlueInfo->ulFlag | GLUE_FLAG_INT_BIT) != 0;
		if (test_and_clear_bit(GLUE_FLAG_INT_BIT,
				       &prGlueInfo->ulFlag) ||
		    test_and_clear_bit(GLUE_FLAG_DRV_INT_BIT,
				       &prGlueInfo->ulFlag)) {
			kalTraceBegin("INT");
			/* the Wi-Fi interrupt is already disabled in mmc
			 * thread, so we set the flag only to enable the
			 * interrupt later
			 */
			prAdapter->fgIsIntEnable = FALSE;
			if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
				|| kalIsResetting()
				) {
				/* Should stop now... skip pending interrupt */
				DBGLOG(INIT, INFO,
				       "ignore pending interrupt\n");
			} else {
				/* DBGLOG(INIT, INFO, ("HIF Interrupt!\n")); */
				prGlueInfo->TaskIsrCnt++;
				wlanIST(prAdapter, fgEnInt);
			}
			kalTraceEnd();
		}

		/* Skip Tx request if SER is operating */
		if ((prAdapter->fgIsFwOwn == FALSE) &&
		    !nicSerIsTxStop(prAdapter)) {
			/* TX Commands */
			if (test_and_clear_bit(GLUE_FLAG_HIF_TX_CMD_BIT,
					       &prGlueInfo->ulFlag))
				TRACE(wlanTxCmdMthread(prAdapter), "TX_CMD");

			/* Process TX data packet to HIF */
			if (test_and_clear_bit(GLUE_FLAG_HIF_TX_BIT,
					       &prGlueInfo->ulFlag))
				TRACE(nicTxMsduQueueMthread(prAdapter),	"TX");
		}

		/* Read chip status when chip no response */
		if (test_and_clear_bit(GLUE_FLAG_HIF_PRT_HIF_DBG_INFO_BIT,
				       &prGlueInfo->ulFlag))
			TRACE(halPrintHifDbgInfo(prAdapter), "DBG_INFO");

		/* Update Tx Quota */
		if (test_and_clear_bit(GLUE_FLAG_UPDATE_WMM_QUOTA_BIT,
					&prGlueInfo->ulFlag))
			TRACE(halUpdateTxMaxQuota(prAdapter), "UPDATE_WMM");

		/* Set FW own */
		if (test_and_clear_bit(GLUE_FLAG_HIF_FW_OWN_BIT,
				       &prGlueInfo->ulFlag))
			prAdapter->fgWiFiInSleepyState = TRUE;

		kalDumpHifStats(prAdapter);

		/* Release to FW own */
		wlanReleasePowerControl(prAdapter);
		kalTraceEnd(); /* hif_thread */
	}

	complete(&prGlueInfo->rHifHaltComp);
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
				 prHifThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, prHifThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter,
			      prHifThreadWakeLock);
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

	int ret = 0;
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	KAL_WAKE_LOCK_T *prRxThreadWakeLock;
#endif
	uint32_t u4LoopCount;

	/* for spin lock acquire and release */
	KAL_SPIN_LOCK_DECLARATION();

#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter,
			   prRxThreadWakeLock, "WLAN rx_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, prRxThreadWakeLock);
#endif

	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	prGlueInfo->u4RxThreadPid = KAL_GET_CURRENT_THREAD_ID();

	set_user_nice(current,
		      prGlueInfo->prAdapter->rWifiVar.cThreadNice);

	prTempRxQue = &rTempRxQue;

	while (TRUE) {

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
			|| kalIsResetting()
			) {
			DBGLOG(INIT, INFO, "rx_thread should stop now...\n");
			break;
		}

		/* Unlock wakelock if rx_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_RX_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter,
					prRxThreadWakeLock);

		/*
		 * sleep on waitqueue if no events occurred.
		 */
		do {
			ret = wait_event_interruptible(prGlueInfo->waitq_rx,
			    ((prGlueInfo->ulFlag & GLUE_FLAG_RX_PROCESS) != 0));
		} while (ret != 0);

		kalTraceBegin("rx_thread");

#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
					  prRxThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter,
				      prRxThreadWakeLock);
#endif
		if (test_and_clear_bit(GLUE_FLAG_RX_TO_OS_BIT,
				       &prGlueInfo->ulFlag)) {
			kalTraceBegin("RX_TO_OS");
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
					prGlueInfo->rTimeoutWakeLock,
					MSEC_TO_JIFFIES(prGlueInfo->prAdapter
					->rWifiVar.u4WakeLockRxTimeout));
				}
			}
			kalTraceEnd(); /* RX_TO_OS */
		}

		kalTraceEnd(); /* rx_thread*/
	}

	complete(&prGlueInfo->rRxHaltComp);
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
				 prRxThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, prRxThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter,
			      prRxThreadWakeLock);
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
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	KAL_WAKE_LOCK_T *prTxThreadWakeLock;
#endif

#if CFG_SUPPORT_MULTITHREAD
	prGlueInfo->u4TxThreadPid = KAL_GET_CURRENT_THREAD_ID();
#endif

	current->flags |= PF_NOFREEZE;
	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);
	set_user_nice(current,
		      prGlueInfo->prAdapter->rWifiVar.cThreadNice);

#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	KAL_WAKE_LOCK_INIT(prGlueInfo->prAdapter,
			   prTxThreadWakeLock, "WLAN main_thread");
	KAL_WAKE_LOCK(prGlueInfo->prAdapter, prTxThreadWakeLock);
#endif

	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	rtc_update = jiffies;	/* update rtc_update time base */

	while (TRUE) {
#ifdef UT_TEST_MODE
		testThreadBegin(prGlueInfo->prAdapter);
#endif

#if CFG_ENABLE_WIFI_DIRECT
		/*run p2p multicast list work. */
		if (test_and_clear_bit(GLUE_FLAG_SUB_MOD_MULTICAST_BIT,
				       &prGlueInfo->ulFlag))
			p2pSetMulticastListWorkQueueWrapper(prGlueInfo);
#endif

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
			|| kalIsResetting()
			) {
			DBGLOG(INIT, INFO, "%s should stop now...\n",
			       KAL_GET_CURRENT_THREAD_NAME());
			break;
		}

		/* Unlock wakelock if main_thread going to idle */
		if (!(prGlueInfo->ulFlag & GLUE_FLAG_MAIN_PROCESS))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter,
					prTxThreadWakeLock);

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
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
		if (!KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
					  prTxThreadWakeLock))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter,
				      prTxThreadWakeLock);
#endif
		kalTraceBegin("main_thread");

#if CFG_ENABLE_WIFI_DIRECT
		/*run p2p multicast list work. */
		if (test_and_clear_bit(GLUE_FLAG_SUB_MOD_MULTICAST_BIT,
				       &prGlueInfo->ulFlag))
			p2pSetMulticastListWorkQueueWrapper(prGlueInfo);

		if (test_and_clear_bit(GLUE_FLAG_FRAME_FILTER_BIT,
				       &prGlueInfo->ulFlag)
			&& prGlueInfo->prP2PDevInfo) {
			/* P2p info will be null after p2p removed. */
			p2pFuncUpdateMgmtFrameRegister(prGlueInfo->prAdapter,
			       prGlueInfo->prP2PDevInfo->u4OsMgmtFrameFilter);
		}
#endif

		if (test_and_clear_bit(GLUE_FLAG_FRAME_FILTER_AIS_BIT,
				       &prGlueInfo->ulFlag)) {
			uint32_t i;

			kalTraceBegin("FRAME_FILTER_AIS");
			for (i = 0; i < KAL_AIS_NUM; i++) {
				struct AIS_FSM_INFO *prAisFsmInfo =
					(struct AIS_FSM_INFO *)NULL;

			/* printk("prGlueInfo->u4OsMgmtFrameFilter = %x",
			 * prGlueInfo->u4OsMgmtFrameFilter);
			 */
				prAisFsmInfo =
					aisGetAisFsmInfo(prGlueInfo->prAdapter,
					i);

				if (!prAisFsmInfo)
					continue;

				prAisFsmInfo->u4AisPacketFilter =
					prGlueInfo->u4OsMgmtFrameFilter;
			}
			kalTraceEnd();
		}

		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
			|| kalIsResetting()
			) {
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
			kalTraceBegin("INT");
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

			if (prGlueInfo->ulFlag & GLUE_FLAG_HALT
				|| kalIsResetting()
				) {
				/* Should stop now... skip pending interrupt */
				DBGLOG(INIT, INFO,
					"ignore pending interrupt\n");
			} else {
				prGlueInfo->TaskIsrCnt++;
				wlanIST(prGlueInfo->prAdapter, true);
			}
			kalTraceEnd();
		}

		if (test_and_clear_bit(GLUE_FLAG_UPDATE_WMM_QUOTA_BIT,
					&prGlueInfo->ulFlag))
			TRACE(halUpdateTxMaxQuota(prGlueInfo->prAdapter),
					"UPDATE_WMM");
#endif
		/* transfer ioctl to OID request */
#ifdef UT_TEST_MODE
		testProcessOid(prGlueInfo->prAdapter);
#endif
		if (test_and_clear_bit(GLUE_FLAG_OID_BIT,
				       &prGlueInfo->ulFlag)) {
			kalTraceBegin("OID");
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
			kalTraceEnd();
		}

		/*
		 *
		 * if TX request, clear the TXREQ flag. TXREQ set by
		 * kalSetEvent/GlueSetEvent
		 * indicates the following requests occur
		 *
		 */

#ifdef UT_TEST_MODE
		testProcessTxReq(prGlueInfo->prAdapter);
#endif

		if (test_and_clear_bit(GLUE_FLAG_TXREQ_BIT,
				       &prGlueInfo->ulFlag))
			TRACE(kalProcessTxReq(prGlueInfo, &fgNeedHwAccess),
				"TXREQ");

#if CFG_SUPPORT_MULTITHREAD
		/* Process RX */
#ifdef UT_TEST_MODE
		testProcessRFBs(prGlueInfo->prAdapter);
#endif

		if (test_and_clear_bit(GLUE_FLAG_RX_BIT,
				       &prGlueInfo->ulFlag))
			TRACE(nicRxProcessRFBs(prGlueInfo->prAdapter), "RX");

		if (test_and_clear_bit(GLUE_FLAG_TX_CMD_DONE_BIT,
				       &prGlueInfo->ulFlag))
			TRACE(wlanTxCmdDoneMthread(prGlueInfo->prAdapter),
				"TX_CMD");
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
#ifdef UT_TEST_MODE
		testTimeoutCheck(prGlueInfo->prAdapter);
#endif

		/* update current throughput */
		kalPerMonUpdate(prGlueInfo->prAdapter);

		if (test_and_clear_bit(GLUE_FLAG_TIMEOUT_BIT,
				       &prGlueInfo->ulFlag))
			TRACE(wlanTimerTimeoutCheck(prGlueInfo->prAdapter),
				"TIMEOUT");

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		if (prGlueInfo->fgEnSdioTestPattern == TRUE)
			kalSetEvent(prGlueInfo);
#endif
#ifdef UT_TEST_MODE
		testThreadEnd(prGlueInfo->prAdapter);
#endif
#if (CFG_SUPPORT_CONNINFRA == 1)
		if (test_and_clear_bit(GLUE_FLAG_SER_TIMEOUT_BIT,
			&prGlueInfo->ulFlag))
			GL_RESET_TRIGGER(prGlueInfo->prAdapter,
				RST_FLAG_DO_CORE_DUMP);
#endif
		kalTraceEnd(); /* main_thread */
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
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	if (KAL_WAKE_LOCK_ACTIVE(prGlueInfo->prAdapter,
				 prTxThreadWakeLock))
		KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, prTxThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(prGlueInfo->prAdapter,
			      prTxThreadWakeLock);
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
#if CFG_TC1_FEATURE
		/*LGE_FacReadWifiMacAddr(prMacAddr);*/
		TC1_FAC_NAME(FacReadWifiMacAddr)(prMacAddr);
		DBGLOG(INIT, INFO,
			"MAC address: " MACSTR, MAC2STR(prMacAddr));
#else
		if (prGlueInfo->fgNvramAvailable == FALSE) {
			DBGLOG(INIT, INFO, "glLoadNvram fail\n");
			return FALSE;
		}
		kalMemCopy(prMacAddr, prGlueInfo->rRegInfo.aucMacAddr,
			   PARAM_MAC_ADDR_LEN * sizeof(uint8_t));
#endif
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
	IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucBssIndex)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->eParamMediaStateIndicated[ucBssIndex];
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
			       eParamMediaStateIndicate,
			       IN uint8_t ucBssIndex)
{
	ASSERT(prGlueInfo);
	ASSERT(ucBssIndex >= 0 && ucBssIndex < KAL_AIS_NUM)
	prGlueInfo->eParamMediaStateIndicated[ucBssIndex] =
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
		DBGLOG(OID, INFO, "Clear pending OID CMD ID[0x%02X] SEQ[%u]\n",
				prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);
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
#if CFG_DBG_MGT_BUF
	struct MEM_TRACK *prMemTrack = NULL;
#endif

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(prGlueInfo);
	ASSERT(prQueueEntry);

	prCmdQue = &prGlueInfo->rCmdQueue;

	prCmdInfo = (struct CMD_INFO *) prQueueEntry;

#if CFG_DBG_MGT_BUF
	if (prCmdInfo->pucInfoBuffer &&
			!IS_FROM_BUF(prGlueInfo->prAdapter,
				prCmdInfo->pucInfoBuffer)) {
		prMemTrack = (struct MEM_TRACK *)
			((uint8_t *)prCmdInfo->pucInfoBuffer -
				sizeof(struct MEM_TRACK));
		prMemTrack->u2CmdIdAndWhere = 0;
		prMemTrack->u2CmdIdAndWhere |= prCmdInfo->ucCID;
	}
#endif

	DBGLOG_LIMITED(INIT, TRACE,
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

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
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
		 IN uint8_t ucBssIndex,
		 IN uint32_t status)
{
	uint8_t fgAborted = (status != WLAN_STATUS_SUCCESS) ? TRUE : FALSE;
	ASSERT(prGlueInfo);

	if (IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		scanLogEssResult(prGlueInfo->prAdapter);

	scanReportBss2Cfg80211(prGlueInfo->prAdapter,
			       BSS_TYPE_INFRASTRUCTURE, NULL);

	/* check for system configuration for generating error message on scan
	 * list
	 */
	wlanCheckSystemConfiguration(prGlueInfo->prAdapter);

	kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_SCAN_COMPLETE,
		&fgAborted, sizeof(fgAborted), ucBssIndex);
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
		ies = *(struct cfg80211_bss_ies **)
		(((size_t)&(bss->pub)) + offsetof(struct cfg80211_bss, ies));
		if (rcu_access_pointer(bss->pub.ies) == ies) {
			ies->tsf = le64_to_cpu(new_timestamp);
		} else {
			log_limited_dbg(REQ, WARN, "Update tsf fail. bss->pub.ies=%p ies=%p\n",
				bss->pub.ies, ies);
		}
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
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
void kalTimeoutHandler(struct timer_list *timer)
#else
void kalTimeoutHandler(unsigned long arg)
#endif
{
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	struct GLUE_INFO *prGlueInfo =
		from_timer(prGlueInfo, timer, tickfn);
#else
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)arg;
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

void kalSetSerTimeoutEvent(struct GLUE_INFO *pr)
{
	set_bit(GLUE_FLAG_SER_TIMEOUT_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq);
}

void kalSetIntEvent(struct GLUE_INFO *pr)
{
	KAL_WAKE_LOCK(pr->prAdapter, pr->rIntrWakeLock);

	set_bit(GLUE_FLAG_INT_BIT, &pr->ulFlag);

	/* when we got interrupt, we wake up servie thread */
#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&pr->waitq_hif);
#else
	wake_up_interruptible(&pr->waitq);
#endif
}

void kalSetDrvIntEvent(struct GLUE_INFO *pr)
{
	KAL_WAKE_LOCK(pr->prAdapter, pr->rIntrWakeLock);

	set_bit(GLUE_FLAG_DRV_INT_BIT, &pr->ulFlag);

	/* when we got interrupt, we wake up servie thread */
#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&pr->waitq_hif);
#else
	wake_up_interruptible(&pr->waitq);
#endif
}

void kalSetWmmUpdateEvent(struct GLUE_INFO *pr)
{
	set_bit(GLUE_FLAG_UPDATE_WMM_QUOTA_BIT, &pr->ulFlag);
#if CFG_SUPPORT_MULTITHREAD
	wake_up_interruptible(&pr->waitq_hif);
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

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	set_bit(GLUE_FLAG_HIF_TX_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}

void kalSetFwOwnEvent2Hif(struct GLUE_INFO *pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	set_bit(GLUE_FLAG_HIF_FW_OWN_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}

void kalSetTxEvent2Rx(struct GLUE_INFO *pr)
{
	if (!pr->rx_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	set_bit(GLUE_FLAG_RX_TO_OS_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_rx);
}

void kalSetTxCmdEvent2Hif(struct GLUE_INFO *pr)
{
	if (!pr->hif_thread)
		return;

	KAL_WAKE_LOCK_TIMEOUT(pr->prAdapter, pr->rTimeoutWakeLock,
			      MSEC_TO_JIFFIES(
			      pr->prAdapter->rWifiVar.u4WakeLockThreadWakeup));

	set_bit(GLUE_FLAG_HIF_TX_CMD_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq_hif);
}

void kalSetTxCmdDoneEvent(struct GLUE_INFO *pr)
{
	/* do we need wake lock here */
	set_bit(GLUE_FLAG_TX_CMD_DONE_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq);
}

void kalSetRxProcessEvent(struct GLUE_INFO *pr)
{
	/* do we need wake lock here ? */
	set_bit(GLUE_FLAG_RX_BIT, &pr->ulFlag);
	wake_up_interruptible(&pr->waitq);
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
	      IN uint8_t ucBssIndex,
	      IN int8_t cRssi, IN int8_t cLinkQuality)
{
	struct iw_statistics *pStats = (struct iw_statistics *)NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (IS_BSS_AIS(prBssInfo))
		pStats = (struct iw_statistics *)
			(&(prGlueInfo->rIwStats[ucBssIndex]));
#if CFG_ENABLE_WIFI_DIRECT
#if CFG_SUPPORT_P2P_RSSI_QUERY
	else if (IS_BSS_P2P(prBssInfo))
		pStats = (struct iw_statistics *)
			(&(prGlueInfo->rP2pIwStats));
#endif
#endif
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
uint32_t kalGetMfpSetting(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucBssIndex)
{
	uint32_t u4RsnMfp = RSN_AUTH_MFP_DISABLED;
	struct GL_WPA_INFO *prWpaInfo;

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	ASSERT(prGlueInfo);

	switch (prWpaInfo->u4Mfp) {
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
uint8_t kalGetRsnIeMfpCap(IN struct GLUE_INFO *prGlueInfo,
	IN uint8_t ucBssIndex)
{
	struct GL_WPA_INFO *prWpaInfo;

	ASSERT(prGlueInfo);

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	return prWpaInfo->ucRSNMfpCap;
}
#endif

struct file *kalFileOpen(const char *filePath, int openModes,
			 int createModes)
{
	struct file *prFile = NULL;
	mm_segment_t oldFs;
	long errval = 0;

	oldFs = get_fs();
	set_fs(get_ds());
	prFile = filp_open(filePath, openModes, createModes);
	set_fs(oldFs);
	if (IS_ERR(prFile)) {
		errval = PTR_ERR(prFile);
		DBGLOG(INIT, TRACE, "kalFileOpen() fail: %ld\n", errval);
		return NULL;
	}
	return prFile;
}

void kalFileClose(struct file *prFile)
{
	filp_close(prFile, NULL);
}

uint32_t kalFileRead(struct file *prFile,
		     unsigned long long pos, unsigned char *buffer,
		     unsigned int len)
{
	mm_segment_t oldFs;
	int retval;

	oldFs = get_fs();
	set_fs(get_ds());

#if KERNEL_VERSION(4, 13, 0) <= CFG80211_VERSION_CODE
	retval = kernel_read(prFile, buffer, len, &pos);
#else
	retval = vfs_read(prFile, buffer, len, &pos);
#endif

	set_fs(oldFs);
	return retval;
}

uint32_t kalFileWrite(struct file *prFile,
		      unsigned long long pos, unsigned char *buffer,
		      unsigned int len)
{
	mm_segment_t oldFs;
	int retval;

	oldFs = get_fs();
	set_fs(get_ds());

#if KERNEL_VERSION(4, 13, 0) <= CFG80211_VERSION_CODE
	retval = kernel_write(prFile, buffer, len, &pos);
#else
	retval = vfs_write(prFile, buffer, len, &pos);
#endif

	set_fs(oldFs);
	return retval;
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
	ret = _kalRequestFirmware(&fw, pucPath, dev);

	if (ret != 0) {
		DBGLOG(INIT, TRACE, "kalRequestFirmware %s Fail, errno[%d]!!\n",
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
		struct ieee80211_mgmt *prMgmtFrame = (struct ieee80211_mgmt
						      *)pucBeaconProbeResp;
		char *pucBssSubType =
			ieee80211_is_beacon(prMgmtFrame->frame_control) ?
			"beacon" : "probe_resp";

#if CFG_SUPPORT_TSF_USING_BOOTTIME
		prMgmtFrame->u.beacon.timestamp = kalGetBootTime();
#endif

		kalScanResultLog(prGlueInfo->prAdapter,
			(struct ieee80211_mgmt *)pucBeaconProbeResp);

		log_dbg(SCN, TRACE, "cfg80211_inform_bss_frame %s bss=" MACSTR
			" sn=%u ch=%u rssi=%d len=%u tsf=%llu\n", pucBssSubType,
			MAC2STR(prMgmtFrame->bssid), prMgmtFrame->seq_ctrl,
			ucChannelNum, i4SignalStrength, u4FrameLen,
			prMgmtFrame->u.beacon.timestamp);

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
		  IN uint8_t ucChannelNum, IN uint32_t u4DurationMs,
		  IN uint8_t ucBssIndex)
{
	struct ieee80211_channel *prChannel = NULL;
	enum nl80211_channel_type rChannelType;

	/* ucChannelNum = wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter,
	 *                NETWORK_TYPE_AIS_INDEX);
	 */

	if (prGlueInfo->fgIsRegistered == TRUE) {
		struct net_device *prDevHandler =
			wlanGetNetDev(prGlueInfo, ucBssIndex);

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

		if (!prChannel) {
			DBGLOG(AIS, ERROR, "prChannel is NULL, return!");
			return;
		}

		cfg80211_ready_on_channel(
			prDevHandler->ieee80211_ptr,
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
			  IN enum ENUM_CHNL_EXT eSco, IN uint8_t ucChannelNum,
			  IN uint8_t ucBssIndex)
{
	struct ieee80211_channel *prChannel = NULL;
	enum nl80211_channel_type rChannelType;

	ucChannelNum =
		wlanGetChannelNumberByNetwork(prGlueInfo->prAdapter,
			ucBssIndex);

	if (prGlueInfo->fgIsRegistered == TRUE) {
		struct net_device *prDevHandler =
			wlanGetNetDev(prGlueInfo, ucBssIndex);

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

		if (!prChannel) {
			DBGLOG(AIS, ERROR, "prChannel is NULL, return!");
			return;
		}

		cfg80211_remain_on_channel_expired(
			prDevHandler->ieee80211_ptr,
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
			IN uint8_t *pucFrameBuf, IN uint32_t u4FrameLen,
			IN uint8_t ucBssIndex)
{

	do {
		struct net_device *prDevHandler;

		if ((prGlueInfo == NULL)
		    || (pucFrameBuf == NULL)
		    || (u4FrameLen == 0)) {
			DBGLOG(AIS, TRACE,
			       "Unexpected pointer PARAM. 0x%lx, 0x%lx, %d.",
			       prGlueInfo, pucFrameBuf, u4FrameLen);
			ASSERT(FALSE);
			break;
		}

		prDevHandler =
			wlanGetNetDev(prGlueInfo, ucBssIndex);

		cfg80211_mgmt_tx_status(
			prDevHandler->ieee80211_ptr,
			u8Cookie, pucFrameBuf, u4FrameLen, fgIsAck, GFP_KERNEL);

	} while (FALSE);

}				/* kalIndicateMgmtTxStatus */

void kalIndicateRxMgmtFrame(IN struct GLUE_INFO *prGlueInfo,
			    IN struct SW_RFB *prSwRfb,
			    IN uint8_t ucBssIndex)
{
	int32_t i4Freq = 0;
	uint8_t ucChnlNum = 0;

	do {
		struct net_device *prDevHandler;

		if ((prGlueInfo == NULL) || (prSwRfb == NULL)) {
			ASSERT(FALSE);
			break;
		}

		ucChnlNum = prSwRfb->ucChnlNum;

		i4Freq = nicChannelNum2Freq(ucChnlNum) / 1000;

		if (!prGlueInfo->fgIsRegistered) {
			DBGLOG(AIS, WARN,
				"Net dev is not ready!\n");
			break;
		}

		if (prGlueInfo->u4OsMgmtFrameFilter == 0) {
			DBGLOG(AIS, WARN,
				"The cfg80211 hasn't do mgmt register!\n");
			break;
		}

		prDevHandler =
			wlanGetNetDev(prGlueInfo, ucBssIndex);

#if (KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(prDevHandler->ieee80211_ptr,
			i4Freq,	/* in MHz */
			RCPI_TO_dBm((uint8_t) nicRxGetRcpiValueFromRxv(
			prGlueInfo->prAdapter,
			RCPI_MODE_MAX, prSwRfb)),
			prSwRfb->pvHeader, prSwRfb->u2PacketLen,
			NL80211_RXMGMT_FLAG_ANSWERED);

#elif (KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE)
		cfg80211_rx_mgmt(prDevHandler->ieee80211_ptr,
			i4Freq,	/* in MHz */
			RCPI_TO_dBm((uint8_t)
			nicRxGetRcpiValueFromRxv(
				prGlueInfo->prAdapter, RCPI_MODE_WF0, prSwRfb)),
			prSwRfb->pvHeader, prSwRfb->u2PacketLen,
			NL80211_RXMGMT_FLAG_ANSWERED,
			GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(prDevHandler->ieee80211_ptr,
			i4Freq,	/* in MHz */
			RCPI_TO_dBm((uint8_t)
			nicRxGetRcpiValueFromRxv(
				prGlueInfo->prAdapter, RCPI_MODE_WF0, prSwRfb)),
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
		kalMemCopy(&pucIpv4Addrs[(u4NumIPv4+1) * u4AddrLen],
			&prIfa->ifa_mask, u4AddrLen);
		prIfa = prIfa->ifa_next;

		DBGLOG(INIT, INFO,
			"IPv4 addr [%u][" IPV4STR "] mask [" IPV4STR "]\n",
			u4NumIPv4,
			IPV4TOSTR(&pucIpv4Addrs[u4NumIPv4*u4AddrLen]),
			IPV4TOSTR(&pucIpv4Addrs[(u4NumIPv4+1)*u4AddrLen]));

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
		IPV4_ADDR_LEN) * u4NumIPv4Addr * 2);
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
			&pucIPv4Addr[i*u4AddrLen*2], u4AddrLen*2);

		prParamNetAddr = (struct PARAM_NETWORK_ADDRESS *)
			((unsigned long) prParamNetAddr +
			(unsigned long) (u4AddrLen*2 +
			OFFSET_OF(struct PARAM_NETWORK_ADDRESS, aucAddress)));
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
	uint8_t pucIPv4Addr[IPV4_ADDR_LEN * CFG_PF_ARP_NS_MAX_NUM*2];
	uint8_t pucIPv6Addr[IPV6_ADDR_LEN * CFG_PF_ARP_NS_MAX_NUM];
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

#if 0

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
#endif

static inline void __mt_update_tracing_mark_write_addr(void)
{
#if 0
	if (unlikely(tracing_mark_write_addr == 0))
		kallsyms_on_each_symbol(
			__mt_find_tracing_mark_write_symbol_fn, NULL);
#endif
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

static u_int8_t kalSendUevent(const char *src)
{
	int ret;
	char *envp[2];
	char event_string[32];

	envp[0] = event_string;
	envp[1] = NULL;

	/*send uevent*/
	strlcpy(event_string, src, sizeof(event_string));
	if (event_string[0] == '\0') { /*string is null*/
		return FALSE;
	}
	ret = kobject_uevent_env(
			&wlan_object.this_device->kobj,
			KOBJ_CHANGE, envp);
	if (ret != 0) {
		DBGLOG(INIT, WARN, "uevent failed\n");
		return FALSE;
	}

	return TRUE;
}

int kalWlanUeventInit(void)
{
	int ret = 0;

	/* dev init */
	wlan_object.name = "wlan";
	wlan_object.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&wlan_object);
	if (ret) {
		DBGLOG(INIT, WARN, "misc_register error:%d\n", ret);
		return ret;
	}

	ret = kobject_uevent(
			&wlan_object.this_device->kobj, KOBJ_ADD);

	if (ret) {
		misc_deregister(&wlan_object);
		DBGLOG(INIT, WARN, "uevent creat fail:%d\n", ret);
		return ret;
	}

	return ret;
}

void kalWlanUeventDeinit(void)
{
	misc_deregister(&wlan_object);
}

#if CFG_SUPPORT_DATA_STALL
u_int8_t kalIndicateDriverEvent(struct ADAPTER *prAdapter,
				uint32_t event,
				uint16_t dataLen,
				uint8_t ucBssIdx,
				u_int8_t fgForceReport)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	char uevent[30];

	wiphy = priv_to_wiphy(prAdapter->prGlueInfo);
	wdev = (wlanGetNetDev(prAdapter->prGlueInfo,
		ucBssIdx))->ieee80211_ptr;

	if (!wiphy || !wdev || !prWifiVar)
		return -EINVAL;

	if (!fgForceReport) {
		if (prAdapter->tmReportinterval > 0 &&
			!CHECK_FOR_TIMEOUT(kalGetTimeTick(),
			prAdapter->tmReportinterval,
			prWifiVar->u4ReportEventInterval*1000)) {
			return -ETIME;
		}
		GET_CURRENT_SYSTIME(&prAdapter->tmReportinterval);
	}

	kalSnprintf(uevent, sizeof(uevent), "code=%d", event);
	kalSendUevent(uevent);

	skb = cfg80211_vendor_event_alloc(wiphy, wdev,
		dataLen,
		WIFI_EVENT_DRIVER_ERROR, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	if (dataLen > 0 &&
		unlikely(nla_put(skb, WIFI_ATTRIBUTE_ERROR_REASON
		, dataLen, &event) < 0))
		goto nla_put_failure;

	DBGLOG(INIT, ERROR, "DPP event to cfg80211[id:%d][len:%d][F:%d]:%d\n",
		WIFI_EVENT_DRIVER_ERROR,
		dataLen, fgForceReport, event);

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return TRUE;
nla_put_failure:
	kfree_skb(skb);
	return FALSE;
}
#endif

#if CFG_SUPPORT_BIGDATA_PIP
int8_t kalBigDataPip(struct ADAPTER *prAdapter,
					uint8_t *payload,
					uint16_t dataLen)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

	wiphy = priv_to_wiphy(prAdapter->prGlueInfo);
	wdev = ((prAdapter->prGlueInfo)->prDevHandler)->ieee80211_ptr;

	if (!wiphy || !wdev || !prWifiVar || !payload)
		return -EINVAL;

	/* Max length of report data is discussed to 500.*/
	if (dataLen > 500)
		return -EINVAL;

	if (prAdapter->tmDataPipReportinterval > 0 &&
		!CHECK_FOR_TIMEOUT(kalGetTimeTick(),
		prAdapter->tmDataPipReportinterval, 20)) {
		return -ETIME;
	}
	GET_CURRENT_SYSTIME(&prAdapter->tmDataPipReportinterval);

	skb = cfg80211_vendor_event_alloc(wiphy, wdev, dataLen,
		WIFI_EVENT_BIGDATA_PIP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	if (dataLen > 0 &&
		unlikely(nla_put(skb, WIFI_ATTRIBUTE_PIP_PAYLOAD
		, dataLen, payload) < 0))
		goto nla_put_failure;

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return TRUE;
nla_put_failure:
	kfree_skb(skb);
	return FALSE;
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
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	struct timespec64 ts;
#else
	struct timespec ts;
#endif
	uint64_t bootTime = 0;

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	ktime_get_boottime_ts64(&ts);
#elif KERNEL_VERSION(2, 6, 39) <= LINUX_VERSION_CODE
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
}

void kalWowCmdEventSetCb(IN struct ADAPTER *prAdapter,
			IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf)
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
	struct BSS_INFO *prAisBssInfo = NULL;

	kalMemZero(&rCmdWowlanParam,
		   sizeof(struct CMD_WOWLAN_PARAM));

	kalMemZero(&rCmdPacket_Filter_Cap,
		   sizeof(struct CMD_PACKET_FILTER_CAP));

	prGlueInfo->prAdapter->fgSetPfCapabilityDone = FALSE;
	prGlueInfo->prAdapter->fgSetWowDone = FALSE;

	prAisBssInfo = aisGetConnectedBssInfo(
		prGlueInfo->prAdapter);

	if (prAisBssInfo)
		DBGLOG(PF, INFO,
	       "PF, pAd ucBssIndex=%d, ucOwnMacIndex=%d\n",
	       prAisBssInfo->ucBssIndex,
	       prAisBssInfo->ucOwnMacIndex);

	DBGLOG(PF, INFO, "profile wow=%d, GpioInterval=%d\n",
	       prGlueInfo->prAdapter->rWifiVar.ucWow,
	       prGlueInfo->prAdapter->rWowCtrl.astWakeHif[0].u4GpioInterval);

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

	if (enable)
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

	/* ARP offload */
	wlanSetSuspendMode(prGlueInfo, enable);
	/* p2pSetSuspendMode(prGlueInfo, TRUE); */

	/* Let WOW enable/disable as last command, so we can back/restore DMA
	 * classify filter in FW
	 */
	rCmdWowlanParam.ucScenarioID = pWOW_CTRL->ucScenarioId;
	rCmdWowlanParam.ucBlockCount = pWOW_CTRL->ucBlockCount;
	kalMemCopy(&rCmdWowlanParam.astWakeHif[0],
		   &pWOW_CTRL->astWakeHif[0], sizeof(struct WOW_WAKE_HIF));

	/* copy UDP/TCP port setting */
	kalMemCopy(&rCmdWowlanParam.stWowPort,
		   &prGlueInfo->prAdapter->rWowCtrl.stWowPort,
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
		rCmdWowlanParam.ucDetectType = WOWLAN_DETECT_TYPE_MAGIC |
				       WOWLAN_DETECT_TYPE_ONLY_PHONE_SUSPEND;
		rCmdWowlanParam.u2FilterFlag = WOWLAN_FF_DROP_ALL |
				       WOWLAN_FF_SEND_MAGIC_TO_HOST |
				       WOWLAN_FF_ALLOW_1X |
				       WOWLAN_FF_ALLOW_ARP_REQ2ME;
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
		kalMsleep(5);

		if (wait > 100) {
			DBGLOG(INIT, ERROR, "WoW timeout.PF:%d. WoW:%d\n",
				prGlueInfo->prAdapter->fgSetPfCapabilityDone,
				prGlueInfo->prAdapter->fgSetWowDone);
			break;
		}
		if ((prGlueInfo->prAdapter->fgSetPfCapabilityDone == TRUE)
			&& (prGlueInfo->prAdapter->fgSetWowDone == TRUE)) {
			DBGLOG(INIT, STATE, "WoW process done\n");
			break;
		}
		wait++;
	}

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
	struct PERF_MONITOR *prPerMonitor;

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

#define PERF_UPDATE_PERIOD      1000 /* ms */
#if (CFG_SUPPORT_PERF_IND == 1)
void kalPerfIndReset(IN struct ADAPTER *prAdapter)
{
	uint8_t i;

	for (i = 0; i < BSSID_NUM; i++) {
		prAdapter->prGlueInfo->PerfIndCache.u4CurTxBytes[i] = 0;
		prAdapter->prGlueInfo->PerfIndCache.u4CurRxBytes[i] = 0;
		prAdapter->prGlueInfo->PerfIndCache.u2CurRxRate[i] = 0;
		prAdapter->prGlueInfo->PerfIndCache.ucCurRxRCPI0[i] = 0;
		prAdapter->prGlueInfo->PerfIndCache.ucCurRxRCPI1[i] = 0;
	}
} /* kalPerfIndReset */

void kalSetPerfReport(IN struct ADAPTER *prAdapter)
{
	struct CMD_PERF_IND *prCmdPerfReport;
	uint8_t i;
	uint32_t u4CurrentTp = 0;

	DEBUGFUNC("kalSetPerfReport()");

	prCmdPerfReport = (struct CMD_PERF_IND *)
		cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
		sizeof(struct CMD_PERF_IND));

	if (!prCmdPerfReport) {
		DBGLOG(SW4, ERROR,
			"cnmMemAlloc for kalSetPerfReport failed!\n");
		return;
	}
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
		DBGLOG(SW4, TRACE,
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
			sizeof(*prCmdPerfReport),
			(uint8_t *) prCmdPerfReport, NULL, 0);
	}
	cnmMemFree(prAdapter, prCmdPerfReport);
}				/* kalSetPerfReport */
#endif

inline int32_t kalPerMonInit(IN struct GLUE_INFO
			     *prGlueInfo)
{
	struct PERF_MONITOR *prPerMonitor;
	struct net_device *prDevHandler = NULL;
	uint8_t i = 0;

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

	/* sync data with netdev */
	GET_CURRENT_SYSTIME(&prPerMonitor->rLastUpdateTime);
	for (i = 0; i < BSS_DEFAULT_NUM; i++) {
		prDevHandler = wlanGetNetDev(prGlueInfo, i);
		if (prDevHandler) {
			prPerMonitor->ulLastTxBytes[i] =
				prDevHandler->stats.tx_bytes;
			prPerMonitor->ulLastRxBytes[i] =
				prDevHandler->stats.tx_bytes;
			prPerMonitor->ulLastTxPackets[i] =
				prDevHandler->stats.tx_packets;
			prPerMonitor->ulLastRxPackets[i] =
				prDevHandler->stats.rx_packets;
		}
	}

#if CFG_SUPPORT_PERF_IND
	kalPerfIndReset(prGlueInfo->prAdapter);
#endif
	/* enable rps on all cpu cores */
	kalSetRpsMap(prGlueInfo, 0xff);
	KAL_SET_BIT(PERF_MON_INIT_BIT, prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, INFO, "exit %s\n", __func__);
	return 0;
}

inline int32_t kalPerMonDisable(IN struct GLUE_INFO
				*prGlueInfo)
{
	struct PERF_MONITOR *prPerMonitor;

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
	struct PERF_MONITOR *prPerMonitor;

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
	struct PERF_MONITOR *prPerMonitor;

	prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;
	DBGLOG(SW4, TEMP, "enter %s\n", __func__);

	if (!wlan_perf_monitor_force_enable &&
		(wlan_fb_power_down
		|| prGlueInfo->fgIsInSuspendMode
		))

		return 0;

	if (KAL_TEST_BIT(PERF_MON_DISABLE_BIT,
			 prPerMonitor->ulPerfMonFlag) ||
	    KAL_TEST_BIT(PERF_MON_RUNNING_BIT,
			 prPerMonitor->ulPerfMonFlag))
		return 0;

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
	struct PERF_MONITOR *prPerMonitor;

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

		prPerMonitor->u4CurrPerfLevel = 0;
		prPerMonitor->u4TarPerfLevel = 0;
		/*Cancel CPU performance mode request*/
		kalBoostCpu(prGlueInfo->prAdapter,
			    prPerMonitor->u4TarPerfLevel,
			    prGlueInfo->prAdapter->rWifiVar.u4BoostCpuTh);
	}
	DBGLOG(SW4, INFO, "perf monitor stopped\n");
	return 0;
}

inline int32_t kalPerMonDestroy(IN struct GLUE_INFO
				*prGlueInfo)
{
	struct PERF_MONITOR *prPerMonitor = &prGlueInfo->prAdapter->rPerMonitor;

	kalPerMonDisable(prGlueInfo);
	KAL_CLR_BIT(PERF_MON_INIT_BIT, prPerMonitor->ulPerfMonFlag);
	DBGLOG(SW4, INFO, "exit %s\n", __func__);
	return 0;
}

static uint32_t kalPerMonUpdate(IN struct ADAPTER *prAdapter)
{
	struct PERF_MONITOR *perf = &prAdapter->rPerMonitor;
	struct GLUE_INFO *glue = prAdapter->prGlueInfo;
	struct BSS_INFO *bss;
	struct net_device *ndev = NULL;
	struct GL_HIF_INFO *hif = &glue->rHifInfo;
	struct WIFI_LINK_QUALITY_INFO *lq = &prAdapter->rLinkQualityInfo;
	OS_SYSTIME now, last;
	int32_t period;
	uint8_t i, j;
	signed long txDiffBytes[BSS_DEFAULT_NUM],
		    rxDiffBytes[BSS_DEFAULT_NUM],
		    rxDiffPkts[BSS_DEFAULT_NUM],
		    txDiffPkts[BSS_DEFAULT_NUM];
	signed long lastTxBytes, lastRxBytes, lastTxPkts, lastRxPkts;
	unsigned long currentTxBytes, currentRxBytes;
	unsigned long currentTxPkts, currentRxPkts;
	uint64_t throughput = 0;
	char *buf = NULL, *head1, *head2, *head3, *head4;
	char *pos = NULL, *end = NULL;
	uint32_t slen;

	GET_CURRENT_SYSTIME(&now);
	last = perf->rLastUpdateTime;

	if (!KAL_TEST_BIT(PERF_MON_INIT_BIT, perf->ulPerfMonFlag) ||
	    !CHECK_FOR_TIMEOUT(now, last,
			MSEC_TO_SYSTIME(perf->u4UpdatePeriod)))
		return WLAN_STATUS_PENDING;

	perf->rLastUpdateTime = now;

	period = ((int32_t) now - (int32_t) last) * MSEC_PER_SEC / KAL_HZ;
	if (period < 0) {
		/* overflow should not happen */
		DBGLOG(SW4, WARN, "wrong period: now=%u, last=%u, period=%d\n",
			now, last, period);
		goto fail;
	}

	for (i = 0; i < BSS_DEFAULT_NUM; i++) {
		ndev = wlanGetNetDev(glue, i);
		bss = GET_BSS_INFO_BY_INDEX(prAdapter, i);
		if (IS_BSS_ALIVE(prAdapter, bss) && ndev) {
			currentTxBytes = ndev->stats.tx_bytes;
			currentRxBytes = ndev->stats.rx_bytes;
			currentTxPkts = ndev->stats.tx_packets;
			currentRxPkts = ndev->stats.rx_packets;
		} else {
			currentTxBytes = perf->ulLastTxBytes[i];
			currentRxBytes = perf->ulLastRxBytes[i];
			currentTxPkts = perf->ulLastTxPackets[i];
			currentRxPkts = perf->ulLastRxPackets[i];
		}
		lastTxBytes = (signed long) perf->ulLastTxBytes[i];
		lastRxBytes = (signed long) perf->ulLastRxBytes[i];
		perf->ulLastTxBytes[i] = currentTxBytes;
		perf->ulLastRxBytes[i] = currentRxBytes;
		txDiffBytes[i] = (signed long) currentTxBytes - lastTxBytes;
		rxDiffBytes[i] = (signed long) currentRxBytes - lastRxBytes;

		lastTxPkts = (signed long) perf->ulLastTxPackets[i];
		lastRxPkts = (signed long) perf->ulLastRxPackets[i];
		perf->ulLastTxPackets[i] = currentTxPkts;
		perf->ulLastRxPackets[i] = currentRxPkts;
		txDiffPkts[i] = (signed long) currentTxPkts - lastTxPkts;
		rxDiffPkts[i] = (signed long) currentRxPkts - lastRxPkts;

		if (txDiffBytes[i] < 0 || rxDiffBytes[i] < 0) {
			/* overflow should not happen */
			DBGLOG(SW4, WARN,
				"[i]wrong bytes: tx[%lu][%ld][%ld], rx[%lu][%ld][%ld],\n",
				i, currentTxBytes, lastTxBytes, txDiffBytes[i],
				currentRxBytes, lastRxBytes, rxDiffBytes[i]);
			goto fail;
		}

		/* Divsion first to avoid overflow */
		perf->ulTxTp[i] = (txDiffBytes[i] / period) * MSEC_PER_SEC;
		perf->ulRxTp[i] = (rxDiffBytes[i] / period) * MSEC_PER_SEC;

		throughput += txDiffBytes[i] + rxDiffBytes[i];
	}

	perf->ulThroughput = throughput * MSEC_PER_SEC;
	do_div(perf->ulThroughput, period);
	perf->ulThroughput <<= 3;

	/* The length should include
	 * 1. "[%ld:%ld:%ld:%ld]" for each bss, %ld range is
	 *    [-2147483647, +2147483647]
	 * 2. "[%d:...:%d]" for pending frame num, %d range is [-32767, 32767]
	 * 3. "[%u]" for each TX ring, %u range is [0, 65536]
	 * 4. ["%lu:%lu:%lu:%lu] dropped packets by each ndev, "%lu" range is
	 *    [0, 4294967295]
	 */
	slen = (11 * 4 + 5) * BSS_DEFAULT_NUM + 1 +
	       (6 * CFG_MAX_TXQ_NUM + 2 - 1) * MAX_BSSID_NUM + 1 +
	       (5 + 2) * NUM_OF_TX_RING + 1 +
	       (10 * 4 + 5) * BSS_DEFAULT_NUM + 1;
	pos = buf = kalMemAlloc(slen, VIR_MEM_TYPE);
	if (pos == NULL) {
		DBGLOG(SW4, INFO, "Can't allocate memory\n");
		return WLAN_STATUS_RESOURCES;
	}
	memset(buf, 0, slen);
	end = buf + slen;
	head1 = pos;
	for (i = 0; i < BSS_DEFAULT_NUM; ++i) {
		pos += kalSnprintf(pos, end - pos, "[%ld:%ld:%ld:%ld]",
			txDiffBytes[i], txDiffPkts[i],
			rxDiffBytes[i], rxDiffPkts[i]);
	}
	pos++;
	head2 = pos;
	for (i = 0; i < MAX_BSSID_NUM; ++i) {
		pos += kalSnprintf(pos, end - pos, "[");
		for (j = 0; j < CFG_MAX_TXQ_NUM - 1; ++j) {
			pos += kalSnprintf(pos, end - pos, "%d:",
				glue->ai4TxPendingFrameNumPerQueue[i][j]);
		}
		pos += kalSnprintf(pos, end - pos, "%d]",
			glue->ai4TxPendingFrameNumPerQueue[i][j]);
	}
	pos++;
	head3 = pos;
	for (i = 0; i < NUM_OF_TX_RING; ++i) {
		pos += kalSnprintf(pos, end - pos, "[%u]",
			hif->TxRing[i].u4UsedCnt);
	}
	pos++;
	head4 = pos;
	for (i = 0; i < BSS_DEFAULT_NUM; ++i) {
		ndev = wlanGetNetDev(glue, i);
		if (ndev) {
			pos += kalSnprintf(pos, end - pos, "[%lu:%lu:%lu:%lu]",
				ndev->stats.tx_dropped,
				atomic_long_read(&ndev->tx_dropped),
				ndev->stats.rx_dropped,
				atomic_long_read(&ndev->rx_dropped));
		}
	}

#define TEMP_LOG_TEMPLATE \
	"<%dms> Tput: %llu(%lu.%03lumbps) %s Pending: %d/%d %s Used: " \
	"%u/%d/%d %s LQ[%lu:%lu:%lu] Drop: %s lv:%u th:%u fg:0x%lx\n"
	DBGLOG(SW4, INFO, TEMP_LOG_TEMPLATE,
		period,	perf->ulThroughput,
		(unsigned long) (perf->ulThroughput >> 20),
		(unsigned long) ((perf->ulThroughput >> 10) & BITS(0, 9)),
		head1, GLUE_GET_REF_CNT(glue->i4TxPendingFrameNum),
		prAdapter->rWifiVar.u4NetifStopTh, head2,
		hif->rTokenInfo.u4UsedCnt, HIF_TX_MSDU_TOKEN_NUM,
		TX_RING_SIZE, head3, lq->u8TxTotalCount, lq->u8RxTotalCount,
		lq->u8DiffIdleSlotCount, head4, perf->u4CurrPerfLevel,
		prAdapter->rWifiVar.u4BoostCpuTh,
		perf->ulPerfMonFlag);
#undef TEMP_LOG_TEMPLATE

	kalTraceEvent("Tput: %lu.%03lumbps",
		(unsigned long) (perf->ulThroughput >> 20),
		(unsigned long) ((perf->ulThroughput >> 10) & BITS(0, 9)));
	kalMemFree(buf, VIR_MEM_TYPE, slen);
	return WLAN_STATUS_SUCCESS;
fail:
	return WLAN_STATUS_FAILURE;
}

void kalPerMonHandler(IN struct ADAPTER *prAdapter,
		      unsigned long ulParam)
{
	/*Calculate current throughput*/
	struct PERF_MONITOR *prPerMonitor;
	uint32_t u4Idx = 0;
	uint8_t	i =	0;
	bool keep_alive = FALSE;
	struct net_device *prDevHandler = NULL;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
#if CFG_SUPPORT_PERF_IND || CFG_SUPPORT_DATA_STALL
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#endif

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT)
		return;

	prPerMonitor = &prAdapter->rPerMonitor;
	DBGLOG(SW4, TRACE, "enter kalPerMonHandler\n");

#if (CFG_SUPPORT_PERF_IND == 1)
	if (prWifiVar->fgPerfIndicatorEn)
		kalSetPerfReport(prAdapter);

	kalPerfIndReset(prAdapter);
#endif
	for (i = 0; i < BSS_DEFAULT_NUM; i++) {
		struct BSS_INFO *prBssInfo;

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);
		prDevHandler = wlanGetNetDev(prGlueInfo, i);
		if (IS_BSS_ALIVE(prAdapter, prBssInfo) && prDevHandler) {
			keep_alive |= netif_carrier_ok(prDevHandler);
		}
	}

#if CFG_SUPPORT_DATA_STALL
		/* test mode event */
		if (prWifiVar->u4ReportEventInterval == 0)
			KAL_REPORT_ERROR_EVENT(prAdapter,
				EVENT_TEST_MODE, 0, 0,  FALSE);
#endif
	prPerMonitor->u4TarPerfLevel = PERF_MON_TP_MAX_THRESHOLD;
	for (u4Idx = 0; u4Idx < PERF_MON_TP_MAX_THRESHOLD; u4Idx++) {
		if ((prPerMonitor->ulThroughput >> 20) <
		    prAdapter->rWifiVar.u4PerfMonTpTh[u4Idx]) {
			prPerMonitor->u4TarPerfLevel = u4Idx;
			break;
		}
	}

	if (!wlan_perf_monitor_force_enable &&
			(wlan_fb_power_down ||
			prGlueInfo->fgIsInSuspendMode ||
			!keep_alive))
		kalPerMonStop(prGlueInfo);
	else {
		if (kalCheckTputLoad(prAdapter,
			prPerMonitor->u4CurrPerfLevel,
			prPerMonitor->u4TarPerfLevel,
			GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
			GLUE_GET_REF_CNT(prPerMonitor->u4UsedCnt))) {

			DBGLOG(SW4, INFO,
			"PerfMon overloading total:%3lu.%03lu mbps lv:%u th:%u fg:0x%lx Pending[%d], Used[%d]\n",
			(unsigned long) (prPerMonitor->ulThroughput >> 20),
			(unsigned long) ((prPerMonitor->ulThroughput >> 10)
					& BITS(0, 9)),
			prPerMonitor->u4TarPerfLevel,
			prAdapter->rWifiVar.u4BoostCpuTh,
			prPerMonitor->ulPerfMonFlag,
			GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum),
			GLUE_GET_REF_CNT(prPerMonitor->u4UsedCnt));

			/* boost current level due to overloading */
			kalBoostCpu(prAdapter,
				prPerMonitor->u4TarPerfLevel,
				prPerMonitor->u4TarPerfLevel);
		} else if ((prPerMonitor->u4TarPerfLevel !=
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

		cnmTimerStartTimer(prGlueInfo->prAdapter,
				&prPerMonitor->rPerfMonTimer,
				prPerMonitor->u4UpdatePeriod);

	}
	prPerMonitor->u4CurrPerfLevel =
		prPerMonitor->u4TarPerfLevel;

#ifdef CFG_SUPPORT_LINK_QUALITY_MONITOR
	prAdapter->u4LinkQualityCounter++;
	if ((prAdapter->u4LinkQualityCounter %
	     CFG_LQ_MONITOR_FREQUENCY) == 0) {
		prAdapter->u4LinkQualityCounter = 0;
		if (prGlueInfo->fgIsInSuspendMode)
			DBGLOG(SW4, TRACE,
				"Skip wlanLinkQualityMonitor due to in suspend mode\n");
		else
			wlanLinkQualityMonitor(prGlueInfo, FALSE);
	}
#endif /* CFG_SUPPORT_LINK_QUALITY_MONITOR */

	/* check tx hang */
	prAdapter->u4HifChkFlag |= HIF_CHK_TX_HANG;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);

	DBGLOG(SW4, TRACE, "exit kalPerMonHandler\n");
}

uint32_t kalPerMonGetInfo(IN struct ADAPTER *prAdapter,
			  IN uint8_t *pucBuf, IN uint32_t u4Max)
{
	struct PERF_MONITOR *prPerMonitor;
	uint32_t u4Len = 0;
	unsigned long ulWlanTxTpInBits, ulWlanRxTpInBits,
		 ulP2PTxTpInBits, ulP2PRxTpInBits;

	prPerMonitor = &prAdapter->rPerMonitor;

	ulWlanTxTpInBits = prPerMonitor->ulTxTp[0] << 3;
	ulWlanRxTpInBits = prPerMonitor->ulRxTp[0] << 3;
	ulP2PTxTpInBits = prPerMonitor->ulTxTp[1] << 3;
	ulP2PRxTpInBits = prPerMonitor->ulRxTp[1] << 3;

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

uint32_t __weak kalGetCpuBoostThreshold(void)
{
	DBGLOG(SW4, WARN, "enter kalGetCpuBoostThreshold\n");
	/*  1, stands for 20Mbps */
	return 1;
}

int32_t __weak kalSetCpuNumFreq(uint32_t u4CoreNum,
				uint32_t u4Freq)
{
	DBGLOG(SW4, INFO,
	       "enter weak kalSetCpuNumFreq, u4CoreNum:%d, urFreq:%d\n",
	       u4CoreNum, u4Freq);
	return 0;
}

int32_t __weak kalGetFwFlavor(uint8_t *flavor)
{
	DBGLOG(SW4, INFO, "NO firmware flavor build.\n");
	return 0;
}

int32_t __weak kalGetConnsysVerId(void)
{
	DBGLOG(SW4, WARN, "NO CONNSYS version ID.\n");
	return 0;
}

void __weak kalSetEmiMpuProtection(phys_addr_t emiPhyBase, bool enable)
{
	DBGLOG(SW4, WARN, "EMI MPU function is not defined\n");
}

void __weak kalSetDrvEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
				      uint32_t size)
{
	DBGLOG(SW4, WARN, "DRV EMI MPU function is not defined\n");
}

int32_t __weak kalCheckTputLoad(IN struct ADAPTER *prAdapter,
			 IN uint32_t u4CurrPerfLevel,
			 IN uint32_t u4TarPerfLevel,
			 IN int32_t i4Pending,
			 IN uint32_t u4Used)
{
	DBGLOG(SW4, TRACE, "enter kalCheckTputLoad\n");
	return FALSE;
}

/* mimic store_rps_map as net-sysfs.c does */
int wlan_set_rps_map(struct netdev_rx_queue *queue, unsigned long rps_value)
{
#if KERNEL_VERSION(4, 14, 0) <= CFG80211_VERSION_CODE
	struct rps_map *old_map, *map;
	cpumask_var_t mask;
	int cpu, i;
	static DEFINE_MUTEX(rps_map_mutex);

	if (!alloc_cpumask_var(&mask, GFP_KERNEL))
		return -ENOMEM;

	*cpumask_bits(mask) = rps_value;
	map = kzalloc(max_t(unsigned int,
			RPS_MAP_SIZE(cpumask_weight(mask)), L1_CACHE_BYTES),
			GFP_KERNEL);
	if (!map) {
		free_cpumask_var(mask);
		return -ENOMEM;
	}

	i = 0;
	for_each_cpu_and(cpu, mask, cpu_online_mask)
		map->cpus[i++] = cpu;

	if (i) {
		map->len = i;
	} else {
		kfree(map);
		map = NULL;
	}

	mutex_lock(&rps_map_mutex);
	old_map = rcu_dereference_protected(queue->rps_map,
				mutex_is_locked(&rps_map_mutex));
	rcu_assign_pointer(queue->rps_map, map);
	if (map)
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		static_branch_inc(&rps_needed);
#else
		static_key_slow_inc(&rps_needed);
#endif
	if (old_map)
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		static_branch_dec(&rps_needed);
#else
		static_key_slow_dec(&rps_needed);
#endif
	mutex_unlock(&rps_map_mutex);

	if (old_map)
		kfree_rcu(old_map, rcu);
	free_cpumask_var(mask);

	return 0;
#else
	return 0;
#endif
}

void kalSetRpsMap(IN struct GLUE_INFO *glue, IN unsigned long value)
{
	int32_t i = 0, j = 0;
	struct net_device *dev = NULL;

	for (i = 0; i < BSS_DEFAULT_NUM; i++) {
		dev = wlanGetNetDev(glue, i);
		if (dev) {
			for (j = 0; j < dev->real_num_rx_queues; ++j)
				wlan_set_rps_map(&dev->_rx[j], value);
		}
	}
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

	if (pucMacAddr)
		COPY_MAC_ADDR(ucMacAddr, pucMacAddr);
	else
		eth_zero_addr(ucMacAddr);

	/* Randomize all 6-bytes MAC.
	 * Not to keep first 3-bytes MAC OUI to be constant.
	 */
	get_random_mask_addr(pucRandomMac, ucMacAddr, pucMacAddrMask);

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
				MAC2STR(pucRandomMac));
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
	char *strbuf = NULL, *pos = NULL, *end = NULL;
	uint32_t slen = 0;
	int i, snum, cnum;

	snum = min_t(int, request->n_ssids, SCN_SSID_MAX_NUM + 1);
	cnum = min_t(u32, request->n_channels, MAXIMUM_OPERATION_CHANNEL_LIST);

	for (i = 0; i < snum; ++i) {
		if (request->ssids[i].ssid_len > 0)
			slen += request->ssids[i].ssid_len + 1;
	}

	/* The length should be added 11 + 15 + 8 + 1 for the format
	 * "n_ssids=%d:" and " n_channels=%u:" and 2 " ..." and null byte.
	 */
	slen += 35 + 4 * cnum;
	pos = strbuf = kalMemAlloc(slen, VIR_MEM_TYPE);
	if (strbuf == NULL) {
		scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Can't allocate memory\n");
		return;
	}
	end = strbuf + slen;

	pos += kalSnprintf(pos, end - pos,
		"n_ssids=%d:", request->n_ssids % 100);
	for (i = 0; i < snum; ++i) {
		uint8_t len = request->ssids[i].ssid_len;
		char ssid[PARAM_MAX_LEN_SSID + 1] = {0};

		if (len == 0)
			continue;
		kalStrnCpy(ssid, request->ssids[i].ssid, sizeof(ssid) - 1);
		ssid[sizeof(ssid) - 1] = '\0';
		pos += kalSnprintf(pos, end - pos, " %s", ssid);
	}
	if (snum < request->n_ssids)
		pos += kalSnprintf(pos, end - pos, "%s", " ...");

	pos += kalSnprintf(pos, end - pos, " n_channels=%u:",
		request->n_channels % 100);
	for (i = 0; i < cnum; ++i) {
		pos += kalSnprintf(pos, end - pos, " %u",
			request->channels[i]->hw_value % 1000);
	}
	if (cnum < request->n_channels)
		pos += kalSnprintf(pos, end - pos, "%s", " ...");

#if (KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE)
	scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Scan flags=0x%x [mac]addr="
		MACSTR " mask=" MACSTR " %s\n",
		request->flags,
		MAC2STR(request->mac_addr),
		MAC2STR(request->mac_addr_mask), strbuf);
#else
	scanlog_dbg(LOG_SCAN_REQ_K2D, INFO, "Scan flags=0x%x %s\n",
		request->flags, strbuf);
#endif

	kalMemFree(strbuf, VIR_MEM_TYPE, slen);
}

void kalScanResultLog(struct ADAPTER *prAdapter, struct ieee80211_mgmt *mgmt)
{
	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_CFG);
	scanLogCacheAddBSS(
		&(prAdapter->rWifiVar.rScanInfo.rScanLogCache.rBSSListCFG),
		prAdapter->rWifiVar.rScanInfo.rScanLogCache.arBSSListBufCFG,
		LOG_SCAN_RESULT_D2K,
		mgmt->bssid,
		mgmt->seq_ctrl);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_CFG);
}

void kalScanLogCacheFlushBSS(struct ADAPTER *prAdapter,
	const uint16_t logBufLen)
{
	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_CFG);
	scanLogCacheFlushBSS(
		&(prAdapter->rWifiVar.rScanInfo.rScanLogCache.rBSSListCFG),
		LOG_SCAN_DONE_D2K);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_BSSLIST_CFG);
}


u_int8_t
kalChannelScoSwitch(IN enum nl80211_channel_type channel_type,
		IN enum ENUM_CHNL_EXT *prChnlSco)
{
	u_int8_t fgIsValid = FALSE;

	do {
		if (prChnlSco) {
			switch (channel_type) {
			case NL80211_CHAN_NO_HT:
				*prChnlSco = CHNL_EXT_SCN;
				break;
			case NL80211_CHAN_HT20:
				*prChnlSco = CHNL_EXT_SCN;
				break;
			case NL80211_CHAN_HT40MINUS:
				*prChnlSco = CHNL_EXT_SCA;
				break;
			case NL80211_CHAN_HT40PLUS:
				*prChnlSco = CHNL_EXT_SCB;
				break;
			default:
				ASSERT(FALSE);
				*prChnlSco = CHNL_EXT_SCN;
				break;
			}
		}
		fgIsValid = TRUE;
	} while (FALSE);

	return fgIsValid;
}

u_int8_t
kalChannelFormatSwitch(IN struct cfg80211_chan_def *channel_def,
		IN struct ieee80211_channel *channel,
		IN struct RF_CHANNEL_INFO *prRfChnlInfo)
{
	u_int8_t fgIsValid = FALSE;

	do {
		if (channel == NULL)
			break;

		DBGLOG(P2P, INFO, "switch channel band: %d, freq: %d\n",
				channel->band, channel->center_freq);

		if (prRfChnlInfo) {
			prRfChnlInfo->ucChannelNum =
				nicFreq2ChannelNum(channel->center_freq * 1000);

			switch (channel->band) {
			case KAL_BAND_2GHZ:
				prRfChnlInfo->eBand = BAND_2G4;
				break;
			case KAL_BAND_5GHZ:
				prRfChnlInfo->eBand = BAND_5G;
				break;
			default:
				prRfChnlInfo->eBand = BAND_2G4;
				break;
			}
		}

		if (channel_def && prRfChnlInfo) {
			switch (channel_def->width) {
			case NL80211_CHAN_WIDTH_20_NOHT:
			case NL80211_CHAN_WIDTH_20:
				prRfChnlInfo->ucChnlBw = MAX_BW_20MHZ;
				break;
			case NL80211_CHAN_WIDTH_40:
				prRfChnlInfo->ucChnlBw = MAX_BW_40MHZ;
				break;
			case NL80211_CHAN_WIDTH_80:
				prRfChnlInfo->ucChnlBw = MAX_BW_80MHZ;
				break;
			case NL80211_CHAN_WIDTH_80P80:
				prRfChnlInfo->ucChnlBw = MAX_BW_80_80_MHZ;
				break;
			case NL80211_CHAN_WIDTH_160:
				prRfChnlInfo->ucChnlBw = MAX_BW_160MHZ;
				break;
			default:
				prRfChnlInfo->ucChnlBw = MAX_BW_20MHZ;
				break;
			}
			prRfChnlInfo->u2PriChnlFreq = channel->center_freq;
			prRfChnlInfo->u4CenterFreq1 = channel_def->center_freq1;
			prRfChnlInfo->u4CenterFreq2 = channel_def->center_freq2;
		}

		fgIsValid = TRUE;
	} while (FALSE);

	return fgIsValid;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Notify kernel to remove/unlink bss.
 *
 * \param[in] prGlueInfo     Pointer of GLUE Data Structure
 * \param[in] prBssDesc      Pointer of BSS_DESC we want to remove
 *
 */
/*----------------------------------------------------------------------------*/
void kalRemoveBss(struct GLUE_INFO *prGlueInfo,
	uint8_t aucBSSID[],
	uint8_t ucChannelNum,
	enum ENUM_BAND eBand)
{
	struct cfg80211_bss *bss = NULL;
	struct ieee80211_channel *prChannel = NULL;

	if (ucChannelNum <= 14) {
		prChannel = ieee80211_get_channel(
			priv_to_wiphy(prGlueInfo),
			ieee80211_channel_to_frequency(
				ucChannelNum,
				KAL_BAND_2GHZ)
		);
	} else {
		prChannel = ieee80211_get_channel(
			priv_to_wiphy(prGlueInfo),
			ieee80211_channel_to_frequency(
				ucChannelNum,
				KAL_BAND_5GHZ)
		);
	}

#if (KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE)
	bss = cfg80211_get_bss(priv_to_wiphy(prGlueInfo),
			prChannel, /* channel */
			aucBSSID,
			NULL, /* ssid */
			0, /* ssid length */
			IEEE80211_BSS_TYPE_ESS,
			IEEE80211_PRIVACY_ANY);
#else
	bss = cfg80211_get_bss(priv_to_wiphy(prGlueInfo),
			prChannel, /* channel */
			aucBSSID,
			NULL, /* ssid */
			0, /* ssid length */
			WLAN_CAPABILITY_ESS,
			WLAN_CAPABILITY_ESS);
#endif

	if (bss != NULL) {
		cfg80211_unlink_bss(priv_to_wiphy(prGlueInfo), bss);
		cfg80211_put_bss(priv_to_wiphy(prGlueInfo), bss);
	}
}

int kalMaskMemCmp(const void *cs, const void *ct,
	const void *mask, size_t count)
{
	const uint8_t *su1, *su2, *su3;
	int res = 0;

	for (su1 = cs, su2 = ct, su3 = mask;
		count > 0; ++su1, ++su2, ++su3, count--) {
		if (mask != NULL)
			res = ((*su1)&(*su3)) - ((*su2)&(*su3));
		else
			res = (*su1) - (*su2);
		if (res != 0)
			break;
	}
	return res;
}

/*
 * This func is mainly from bionic's strtok.c
 */
int8_t *strtok_r(int8_t *s, const int8_t *delim, int8_t **last)
{
	char *spanp;
	int c, sc;
	char *tok;


	if (s == NULL) {
		s = *last;
		if (s == 0)
			return NULL;
	}
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return NULL;
	}
	tok = s - 1;

	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			sc = *spanp++;
			if (sc == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*last = s;
				return tok;
			}
		} while (sc != 0);
	}
}

int8_t atoi(uint8_t ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 87;
	else if (ch >= 'A' && ch <= 'F')
		return ch - 55;
	else if (ch >= '0' && ch <= '9')
		return ch - 48;

	return 0;
}

#if CFG_SUPPORT_WPA3
int kalExternalAuthRequest(IN struct ADAPTER *prAdapter,
				   IN uint8_t uBssIndex)
{
	struct cfg80211_external_auth_params params;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;
	struct BSS_DESC *prBssDesc = NULL;
	struct net_device *ndev = NULL;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, uBssIndex);
	if (!prAisFsmInfo) {
		DBGLOG(SAA, WARN,
		       "SAE auth failed with NULL prAisFsmInfo\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	prBssDesc = prAisFsmInfo->prTargetBssDesc;
	if (!prBssDesc) {
		DBGLOG(SAA, WARN,
		       "SAE auth failed without prTargetBssDesc\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	ndev = wlanGetNetDev(prGlueInfo, uBssIndex);
	params.action = NL80211_EXTERNAL_AUTH_START;
	COPY_MAC_ADDR(params.bssid, prBssDesc->aucBSSID);
	COPY_SSID(params.ssid.ssid, params.ssid.ssid_len,
		  prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	params.key_mgmt_suite = RSN_AKM_SUITE_SAE;
	DBGLOG(AIS, INFO, "[WPA3] "MACSTR" %s %d %d %02x-%02x-%02x-%02x",
	       MAC2STR(params.bssid), params.ssid.ssid,
	       params.ssid.ssid_len, params.action,
	       (uint8_t) (params.key_mgmt_suite & 0x000000FF),
	       (uint8_t) ((params.key_mgmt_suite >> 8) & 0x000000FF),
	       (uint8_t) ((params.key_mgmt_suite >> 16) & 0x000000FF),
	       (uint8_t) ((params.key_mgmt_suite >> 24) & 0x000000FF));
	return cfg80211_external_auth_request(ndev, &params, GFP_KERNEL);
}
#endif

const uint8_t *kalFindIeMatchMask(uint8_t eid,
				const uint8_t *ies, int len,
				const uint8_t *match,
				int match_len, int match_offset,
				const uint8_t *match_mask)
{
	/* match_offset can't be smaller than 2, unless match_len is
	 * zero, in which case match_offset must be zero as well.
	 */
	if (WARN_ON((match_len && match_offset < 2) ||
		(!match_len && match_offset)))
		return NULL;
	while (len >= 2 && len >= ies[1] + 2) {
		if ((ies[0] == eid) &&
			(ies[1] + 2 >= match_offset + match_len) &&
			!kalMaskMemCmp(ies + match_offset,
			match, match_mask, match_len))
			return ies;
		len -= ies[1] + 2;
		ies += ies[1] + 2;
	}
	return NULL;
}

int _kalSnprintf(char *buf, size_t size, const char *fmt, ...)
{
	int retval;
	va_list ap;

	va_start(ap, fmt);
	retval = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return (retval < 0)?(0):(retval);
}

int _kalSprintf(char *buf, const char *fmt, ...)
{
	int retval;
	va_list ap;

	va_start(ap, fmt);
	retval = vsprintf(buf, fmt, ap);
	va_end(ap);
	return (retval < 0)?(0):(retval);
}
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
void kal_do_gettimeofday(struct timeval *tv)
{
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	tv->tv_sec = now.tv_sec;
	tv->tv_usec = now.tv_nsec / NSEC_PER_USEC;
}
#endif

static void kalDumpHifStats(IN struct ADAPTER *prAdapter)
{
	struct HIF_STATS *prHifStats;
	struct GL_HIF_INFO *prHifInfo;
	struct RTMP_TX_RING *prTxRing;
	struct RTMP_RX_RING *prRxRing;
	struct RX_CTRL *prRxCtrl;
	uint8_t i = 0;
	uint32_t u4BufferSize = 512, pos = 0;
	char *buf;

	if (!prAdapter)
		return;

	prHifStats = &prAdapter->rHifStats;
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	prRxCtrl = &prAdapter->rRxCtrl;

	if (time_before(jiffies, prHifStats->ulUpdatePeriod))
		return;

	buf = (char *) kalMemAlloc(u4BufferSize, VIR_MEM_TYPE);
	if (!buf)
		return;
	kalMemZero(buf, u4BufferSize);

	prHifStats->ulUpdatePeriod = jiffies +
			prAdapter->rWifiVar.u4PerfMonUpdatePeriod * HZ / 1000;

	pos += kalSnprintf(buf + pos, u4BufferSize - pos,
			"I[%u %u]",
			GLUE_GET_REF_CNT(prHifStats->u4HwIsrCount),
			GLUE_GET_REF_CNT(prHifStats->u4SwIsrCount));
	pos += kalSnprintf(buf + pos, u4BufferSize - pos,
			" T[%u %u %u / %u %u %u %u]",
			GLUE_GET_REF_CNT(prHifStats->u4CmdInCount),
			GLUE_GET_REF_CNT(prHifStats->u4CmdTxCount),
			GLUE_GET_REF_CNT(prHifStats->u4CmdTxdoneCount),
			GLUE_GET_REF_CNT(prHifStats->u4DataInCount),
			GLUE_GET_REF_CNT(prHifStats->u4DataTxCount),
			GLUE_GET_REF_CNT(prHifStats->u4DataTxdoneCount),
			GLUE_GET_REF_CNT(prHifStats->u4DataMsduRptCount));
	pos += kalSnprintf(buf + pos, u4BufferSize - pos,
			" R[%u / %u]",
			GLUE_GET_REF_CNT(prHifStats->u4DataRxCount),
			GLUE_GET_REF_CNT(prHifStats->u4EventRxCount));
	for (i = 0; i < NUM_OF_TX_RING; ++i) {
		prTxRing = &prHifInfo->TxRing[i];
		pos += kalSnprintf(buf + pos, u4BufferSize - pos, "%s%u%s",
				(i == 0) ? " T_R[" : "",
				prTxRing->u4UsedCnt,
				(i == NUM_OF_TX_RING - 1) ? "] " : " ");
	}
	for (i = 0; i < NUM_OF_RX_RING; ++i) {
		prRxRing = &prHifInfo->RxRing[i];
		pos += kalSnprintf(buf + pos, u4BufferSize - pos, "%s%u%s",
				(i == 0) ? " R_R[" : "",
				prRxRing->u4PendingCnt,
				(i == NUM_OF_RX_RING - 1) ? "]" : " ");
	}
	pos += kalSnprintf(buf + pos, u4BufferSize - pos,
			" Tok[%u/%u] Rfb[%u/%u]",
			prHifInfo->rTokenInfo.u4UsedCnt,
			HIF_TX_MSDU_TOKEN_NUM,
			prRxCtrl->rFreeSwRfbList.u4NumElem,
			CFG_RX_MAX_PKT_NUM);
	DBGLOG(HAL, INFO, "%s\n", buf);
	kalMemFree(buf, VIR_MEM_TYPE, u4BufferSize);
}

uint32_t kalSetSuspendFlagToEMI(IN struct ADAPTER
					*prAdapter, IN u_int8_t fgSuspend)
{
#if CFG_MTK_ANDROID_EMI
	uint32_t u4Offset = prAdapter->u4HostStatusEmiOffset
				& WIFI_EMI_ADDR_MASK;
	uint32_t suspendFlag = 0;

	if (!gConEmiPhyBase) {
#if (CFG_SUPPORT_CONNINFRA == 1)
		conninfra_get_phy_addr(
			(unsigned int *)&gConEmiPhyBase,
			(unsigned int *)&gConEmiSize);
#endif

		if (!gConEmiPhyBase) {
			DBGLOG(INIT, ERROR,
				"[EMI_Suspend] gConEmiPhyBase invalid\n");
			return WLAN_STATUS_FAILURE;
		}
	}
	suspendFlag = (fgSuspend == TRUE) ? 0x11111111 : 0x22222222;

	DBGLOG(INIT, TRACE,
		"[EMI_Suspend] EmiPhyBase:0x%llx offset:0x%x set 0x%x",
		(uint64_t)gConEmiPhyBase, u4Offset, suspendFlag);

	wf_ioremap_write((gConEmiPhyBase + u4Offset), suspendFlag);

#endif /* CFG_MTK_ANDROID_EMI */
	return WLAN_STATUS_SUCCESS;
}

void kalPrintUTC(char *msg_buf, int msg_buf_size)
{
	int ret = 0;
	struct rtc_time tm;
	struct timeval tv = { 0 };
	struct rtc_time tm_android;
	struct timeval tv_android = { 0 };

	do_gettimeofday(&tv);
	tv_android = tv;
	rtc_time_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tv_android.tv_sec, &tm_android);
	if (tm.tm_sec%10 == 0) {
		ret = snprintf(msg_buf, msg_buf_size,
			"[RT:%lld] %d-%02d-%02d %02d:%02d:%02d.%u UTC;"
			"android time %d-%02d-%02d %02d:%02d:%02d.%03d",
			sched_clock(), tm.tm_year + 1900, tm.tm_mon + 1,
			tm.tm_mday, tm.tm_hour, tm.tm_min,
			tm.tm_sec, (unsigned int)tv.tv_usec,
			tm_android.tm_year + 1900, tm_android.tm_mon + 1,
			tm_android.tm_mday, tm_android.tm_hour,
			tm_android.tm_min, tm_android.tm_sec,
			(unsigned int)tv_android.tv_usec);
		if (ret < 0) {
			kalPrintLog("[%u] snprintf failed, ret: %d",
				__LINE__, ret);
		} else {
			trace_wifi_standalone_log(msg_buf);
		}
	}
}

void kalPrintTrace(char *buffer, const int len)
{
	if (buffer[len - 1] == '\n')
		buffer[len - 1] = '\0';

	if (len < WIFI_LOG_MSG_MAX) {
		trace_wifi_standalone_log(buffer);
	} else {
		char sub_buffer[WIFI_LOG_MSG_MAX];

		strncpy(sub_buffer, buffer,
			WIFI_LOG_MSG_MAX - 1);
		sub_buffer[WIFI_LOG_MSG_MAX - 1] = '\0';
		trace_wifi_standalone_log(sub_buffer);

		strncpy(sub_buffer, buffer + WIFI_LOG_MSG_MAX - 1,
			WIFI_LOG_MSG_MAX - 1);
		sub_buffer[WIFI_LOG_MSG_MAX - 1] = '\0';
		trace_wifi_standalone_log(sub_buffer);
	}

	if (time_after(jiffies, rtc_update)) {
		rtc_update = jiffies + (1 * HZ);
		kalPrintUTC(buffer, WIFI_LOG_MSG_BUFFER);
	}
}

void kalPrintLog(const char *fmt, ...)
{
	char buffer[WIFI_LOG_MSG_BUFFER];
	int ret = 0;
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
	if (ret < 0) {
		kalPrintLog("[%u] vsnprintf failed, ret: %d",
			__LINE__, ret);
	} else if (get_wifi_standalone_log_mode() == 1) {
		kalPrintTrace(buffer, strlen(buffer));
	} else {
		pr_info("%s%s", WLAN_TAG, buffer);
	}

	va_end(args);
}

void kalPrintLogLimited(const char *fmt, ...)
{
	#ifdef CONFIG_PRINTK
	static DEFINE_RATELIMIT_STATE(_rs,
		DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);

	if (__ratelimit(&_rs)) {
		char buffer[WIFI_LOG_MSG_BUFFER];
		int ret = 0;
		struct va_format vaf;
		va_list args;

		va_start(args, fmt);
		vaf.fmt = fmt;
		vaf.va = &args;
		ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
		if (ret < 0) {
			kalPrintLog("[%u] vsnprintf failed, ret: %d",
				__LINE__, ret);
		} else if (get_wifi_standalone_log_mode() == 1) {
			kalPrintTrace(buffer, strlen(buffer));
		} else {
			pr_info("%s%s", WLAN_TAG, buffer);
		}

		va_end(args);
	}
	#endif
}

#if KERNEL_VERSION(5, 4, 0) <= CFG80211_VERSION_CODE
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
