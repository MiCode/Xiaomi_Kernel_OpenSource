/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /os/linux/gl_sys.c
 */

/*! \file   "gl_sys.c"
 *  \brief  This file defines the interface which can interact with users
 *          in /sys fs.
 *
 *    Detail description.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "gl_os.h"
#include "gl_kal.h"
#include "debug.h"
#include "wlan_lib.h"
#include "debug.h"
#include "wlan_oid.h"
#include <linux/rtc.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

#if WLAN_INCLUDE_SYS

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define MTK_INFO_MAX_SIZE 128

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

static struct GLUE_INFO *g_prGlueInfo;
static struct kobject *wifi_kobj;
static uint8_t aucMacAddrOverride[] = "FF:FF:FF:FF:FF:FF";
static uint8_t aucDefaultFWVersion[] = "Unknown";
static u_int8_t fgIsMacAddrOverride = FALSE;
static int32_t g_i4PM = -1;
static char acVerInfo[MTK_INFO_MAX_SIZE];
static char acSoftAPInfo[MTK_INFO_MAX_SIZE];

#if BUILD_QA_DBG
static uint32_t g_u4Memdump = 3;
#else
static uint32_t g_u4Memdump = 2;
#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static ssize_t pm_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_i4PM),
		"%d", g_i4PM);
}

static void pm_EnterCtiaMode(void)
{
	if (!g_prGlueInfo)
		DBGLOG(INIT, ERROR, "g_prGlueInfo is null\n");
	else if (g_i4PM == -1)
		DBGLOG(INIT, TRACE, "keep default\n");
	else {
		g_prGlueInfo->prAdapter->fgEnDbgPowerMode = !g_i4PM;
		nicEnterCtiaMode(g_prGlueInfo->prAdapter,
			!g_i4PM,
			FALSE);
	}
}

static ssize_t pm_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtoint(buf, 10, &g_i4PM);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf pm fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set PM to %d.\n",
			g_i4PM);

		pm_EnterCtiaMode();
	}

	return (i4Ret == 0) ? count : 0;
}

static ssize_t macaddr_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(aucMacAddrOverride),
		"%s", aucMacAddrOverride);
}

static ssize_t macaddr_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = sscanf(buf, "%18s", (uint8_t *)&aucMacAddrOverride);

	if (!i4Ret)
		DBGLOG(INIT, ERROR, "sscanf mac format fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set macaddr to %s.\n",
			aucMacAddrOverride);
	}

	fgIsMacAddrOverride = TRUE;

	return (i4Ret > 0) ? count : 0;
}

static ssize_t wifiver_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(acVerInfo), "%s",
		acVerInfo);
}

static ssize_t softap_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, sizeof(acSoftAPInfo), "%s", acSoftAPInfo);
}

static ssize_t memdump_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_u4Memdump),
		"%d", g_u4Memdump);
}

static ssize_t memdump_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtouint(buf, 10, &g_u4Memdump);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf memdump fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set memdump to %d.\n",
			g_u4Memdump);
	}

	return (i4Ret == 0) ? count : 0;
}


/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

static struct kobj_attribute macaddr_attr
	= __ATTR(mac_addr, 0664, macaddr_show, macaddr_store);

static struct kobj_attribute wifiver_attr
	= __ATTR(wifiver, 0664, wifiver_show, NULL);

static struct kobj_attribute softap_attr
	= __ATTR(softap, 0664, softap_show, NULL);

static struct kobj_attribute pm_attr
	= __ATTR(pm, 0664, pm_show, pm_store);

static struct kobj_attribute memdump_attr
	= __ATTR(memdump, 0664, memdump_show, memdump_store);

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
void sysCreateMacAddr(void)
{
	if (g_prGlueInfo) {
		uint8_t rMacAddr[MAC_ADDR_LEN];

		COPY_MAC_ADDR(rMacAddr,
			g_prGlueInfo->prAdapter->rWifiVar.aucMacAddress);

		kalSnprintf(aucMacAddrOverride,
			sizeof(aucMacAddrOverride),
			"%pM",
			MAC2STR(rMacAddr));

		DBGLOG(INIT, TRACE,
			"Init macaddr to " MACSTR ".\n",
			MAC2STR(rMacAddr));
	}
}

void sysInitMacAddr(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &macaddr_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitMacAddr(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &macaddr_attr.attr);
}

void sysInitPM(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &pm_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitPM(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &pm_attr.attr);
}

void sysCreateWifiVer(void)
{
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

	char aucDriverVersionStr[] = STR(NIC_DRIVER_MAJOR_VERSION) "_"
		STR(NIC_DRIVER_MINOR_VERSION) "_"
		STR(NIC_DRIVER_SERIAL_VERSION) "-"
		DRIVER_BUILD_DATE;
	uint16_t u2NvramVer = 0;
	uint8_t ucOffset = 0;

	kalMemZero(acVerInfo, sizeof(acVerInfo));

	ucOffset += kalSnprintf(acVerInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%s\n", "Mediatek");

	ucOffset += kalSnprintf(acVerInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "DRIVER_VER: %s\n", aucDriverVersionStr);

	if (g_prGlueInfo)
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "FW_VER: %s\n"
			, g_prGlueInfo->prAdapter->rVerInfo.aucReleaseManifest);
	else {
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "FW_VER: %s\n"
			, aucDefaultFWVersion);
	}

	if (g_prGlueInfo) {
		kalCfgDataRead16(g_prGlueInfo,
			OFFSET_OF(struct WIFI_CFG_PARAM_STRUCT,
			u2Part1OwnVersion), &u2NvramVer);
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "NVRAM: 0x%x\n", u2NvramVer);
	} else {
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "NVRAM: Unknown\n");
	}
}

void sysInitWifiVer(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &wifiver_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create wifiver entry\n");

	sysCreateWifiVer();
}

void sysUninitWifiVer(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &wifiver_attr.attr);
}

void sysCreateSoftap(void)
{
	struct REG_INFO *prRegInfo = NULL;

	uint8_t ucOffset = 0;
	u_int8_t fgDbDcModeEn = FALSE;

	/* Log SoftAP/hotspot information into .softap.info
	 * #Support wifi and hotspot at the same time?
	 * DualBandConcurrency=no
	 * # Supporting 5Ghz
	 * 5G=check NVRAM ucEnable5GBand
	 * # Max support client count
	 * maxClient=P2P_MAXIMUM_CLIENT_COUNT
	 * #Supporting android_net_wifi_set_Country_Code_Hal
	 * HalFn_setCountryCodeHal=yes ,
	 * call mtk_cfg80211_vendor_set_country_code
	 * #Supporting android_net_wifi_getValidChannels
	 * HalFn_getValidChannels=yes,
	 * call mtk_cfg80211_vendor_get_channel_list
	*/

	if (g_prGlueInfo) {
		prRegInfo = &(g_prGlueInfo->rRegInfo);
#if CFG_SUPPORT_DBDC
		fgDbDcModeEn = g_prGlueInfo->prAdapter->rWifiVar.fgDbDcModeEn;
#endif
	}

	kalMemZero(acSoftAPInfo, sizeof(acSoftAPInfo));

	ucOffset = 0;

	if (g_prGlueInfo) {
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "DualBandConcurrency=%s\n"
			, fgDbDcModeEn ? "yes" : "no");
	} else
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "DualBandConcurrency=no\n");

	if (prRegInfo)
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "5G=%s\n", prRegInfo->ucEnable5GBand ? "yes" : "no");
	else
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "5G=yes\n");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "maxClient=%d\n", P2P_MAXIMUM_CLIENT_COUNT);

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "HalFn_setCountryCodeHal=%s\n", "yes");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "HalFn_getValidChannels=%s\n", "yes");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "DualInterface=%s\n", "yes");
}

void sysInitSoftap(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &softap_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create softap entry\n");

	sysCreateSoftap();
}

void sysUninitSoftap(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &softap_attr.attr);
}

void sysInitMemdump(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &memdump_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create softap entry\n");
}

void sysUninitMemdump(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &memdump_attr.attr);
}

int32_t sysCreateFsEntry(struct GLUE_INFO *prGlueInfo)
{
	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

	g_prGlueInfo = prGlueInfo;

	sysCreateMacAddr();
	pm_EnterCtiaMode();
	sysCreateWifiVer();
	sysCreateSoftap();

	return 0;
}

int32_t sysRemoveSysfs(void)
{
	g_prGlueInfo = NULL;

	return 0;
}

int32_t sysInitFs(void)
{
	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

	wifi_kobj = kobject_create_and_add("wifi", NULL);
	kobject_get(wifi_kobj);
	kobject_uevent(wifi_kobj, KOBJ_ADD);

	sysInitMacAddr();
	sysInitWifiVer();
	sysInitSoftap();
	sysInitPM();
	sysInitMemdump();

	return 0;
}

int32_t sysUninitSysFs(void)
{
	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

	sysUninitMemdump();
	sysUninitPM();
	sysUninitSoftap();
	sysUninitWifiVer();
	sysUninitMacAddr();

	kobject_put(wifi_kobj);
	kobject_uevent(wifi_kobj, KOBJ_REMOVE);
	wifi_kobj = NULL;

	return 0;
}

void sysMacAddrOverride(uint8_t *prMacAddr)
{
	DBGLOG(INIT, TRACE,
		"Override=%d\n", fgIsMacAddrOverride);

	if (!fgIsMacAddrOverride)
		return;

	wlanHwAddrToBin(
		aucMacAddrOverride,
		prMacAddr);

	DBGLOG(INIT, TRACE,
		"Init macaddr to " MACSTR ".\n",
		MAC2STR(prMacAddr));
}

#endif
