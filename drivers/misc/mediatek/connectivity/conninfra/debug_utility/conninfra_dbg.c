/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "osal.h"
#include "conninfra_dbg.h"
#include "conninfra.h"
#include "conninfra_core.h"
#include "emi_mng.h"
#include "connsys_debug_utility.h"

#define CONNINFRA_DBG_PROCNAME "driver/conninfra_dbg"

#define BUF_LEN_MAX 384

#if (defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MTK_ENG_BUILD))
#define WMT_EMI_DEBUG_BUF_SIZE (8*1024)
#else
#define WMT_EMI_DEBUG_BUF_SIZE (32*1024)
#endif

static struct proc_dir_entry *g_conninfra_dbg_entry;

#if CONNINFRA_DBG_SUPPORT
static ssize_t conninfra_dbg_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static ssize_t conninfra_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);

static int conninfra_dbg_hwver_get(int par1, int par2, int par3);

static int conninfra_dbg_chip_rst(int par1, int par2, int par3);

static int conninfra_dbg_read_chipid(int par1, int par2, int par3);

static int conninfra_dbg_force_conninfra_wakeup(int par1, int par2, int par3);
static int conninfra_dbg_force_conninfra_sleep(int par1, int par2, int par3);

static int conninfra_dbg_reg_read(int par1, int par2, int par3);
static int conninfra_dbg_reg_write(int par1, int par2, int par3);

static int conninfra_dbg_efuse_read(int par1, int par2, int par3);
static int conninfra_dbg_efuse_write(int par1, int par2, int par3);

static int conninfra_dbg_ap_reg_read(int par1, int par2, int par3);
static int conninfra_dbg_ap_reg_write(int par1, int par2, int par3);


#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
/* consys log, need this ?? */
static int conninfra_dbg_set_fw_log_mode(int par1, int par2, int par3);
static int conninfra_dbg_fw_log_dump_emi(int par1, int offset, int size);
#endif

static int conninfra_dbg_suspend_debug(int par1, int offset, int size);
static int conninfra_dbg_fw_log_ctrl(int par1, int onoff, int level);

static int conninfra_dbg_thermal_query(int par1, int count, int interval);
static int conninfra_dbg_thermal_ctrl(int par1, int par2, int par3);

static int conninfra_dbg_connsys_emi_dump(int par1, int par2, int par3);
#endif /* CONNINFRA_DBG_SUPPORT */

static int conninfra_dbg_connsys_coredump_ctrl(int par1, int par2, int par3);
static int conninfra_dbg_connsys_coredump_mode_query(int par1, int par2, int par3);

static const CONNINFRA_DEV_DBG_FUNC conninfra_dev_dbg_func[] = {
#if CONNINFRA_DBG_SUPPORT
	[0x0] = conninfra_dbg_hwver_get,
	[0x1] = conninfra_dbg_chip_rst,
	[0x2] = conninfra_dbg_read_chipid,

	[0x3] = conninfra_dbg_force_conninfra_wakeup,
	[0x4] = conninfra_dbg_force_conninfra_sleep,
	[0x5] = conninfra_dbg_reg_read,
	[0x6] = conninfra_dbg_reg_write,

	[0x7] = conninfra_dbg_efuse_read,
	[0x8] = conninfra_dbg_efuse_write,
	[0x9] = conninfra_dbg_ap_reg_read,
	[0xa] = conninfra_dbg_ap_reg_write,

	[0xb] = conninfra_dbg_fw_log_ctrl,
	[0xc] = conninfra_dbg_thermal_query,
	[0xd] = conninfra_dbg_thermal_ctrl,

	[0xf] = conninfra_dbg_suspend_debug,
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	[0x10] = conninfra_dbg_set_fw_log_mode,
	[0x11] = conninfra_dbg_fw_log_dump_emi,
#endif
	[0x12] = conninfra_dbg_connsys_emi_dump,
#endif /* CONNINFRA_DBG_SUPPORT */
	[0x13] = conninfra_dbg_connsys_coredump_ctrl,
	[0x14] = conninfra_dbg_connsys_coredump_mode_query,
};

#define CONNINFRA_DBG_DUMP_BUF_SIZE 1024
char g_dump_buf[CONNINFRA_DBG_DUMP_BUF_SIZE];
char *g_dump_buf_ptr;
int g_dump_buf_len;
static OSAL_SLEEPABLE_LOCK g_dump_lock;

#if CONNINFRA_DBG_SUPPORT
int conninfra_dbg_hwver_get(int par1, int par2, int par3)
{
	pr_info("query chip version\n");
	/* TODO: */
	return 0;
}

int conninfra_dbg_chip_rst(int par1, int par2, int par3)
{
	/* TODO: */
	conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "Conninfra_dbg");
	return 0;
}


int conninfra_dbg_read_chipid(int par1, int par2, int par3)
{
	//pr_info("chip id = %d\n", wmt_lib_get_icinfo(WMTCHIN_CHIPID));

	return 0;
}

int conninfra_dbg_reg_read(int par1, int par2, int par3)
{
	int ret = 0, sz;
	char buf[CONNINFRA_DBG_DUMP_BUF_SIZE];

	/* par2-->register address */
	/* par3-->register mask */
	unsigned int value = 0x0;
	int iRet;

	iRet = conninfra_core_reg_read(par2, &value, par3);
	ret = snprintf(buf, CONNINFRA_DBG_DUMP_BUF_SIZE,
			"read chip register (0x%08x) with mask (0x%08x) %s, value = 0x%08x\n",
			par2, par3, iRet != 0 ? "failed" : "succeed", iRet != 0 ? -1 : value);
	if (ret < 0) {
		pr_info("read chip register (0x%08x) with mask (0x%08x) error(%d)\n",
			par2, par3, ret);
	} else
		pr_info("%s", buf);

	ret = osal_lock_sleepable_lock(&g_dump_lock);
	if (ret) {
		pr_err("dump_lock fail!!");
		return ret;
	}

	if (g_dump_buf_len < CONNINFRA_DBG_DUMP_BUF_SIZE) {
		sz = strlen(buf);
		sz = (sz < CONNINFRA_DBG_DUMP_BUF_SIZE - g_dump_buf_len) ?
				sz : CONNINFRA_DBG_DUMP_BUF_SIZE - g_dump_buf_len;
		strncpy(g_dump_buf + g_dump_buf_len, buf, sz);
		g_dump_buf_len += sz;
	}

	osal_unlock_sleepable_lock(&g_dump_lock);

	return 0;
}

int conninfra_dbg_reg_write(int par1, int par2, int par3)
{
	/* par2-->register address */
	/* par3-->value to set */
	int ret;

	ret = conninfra_core_reg_write(par2, par3, 0xffffffff);
	pr_info("write chip register (0x%08x) with value (0x%08x) %s\n",
		      par2, par3, ret != 0 ? "failed" : "succeed");
	return 0;
}


int conninfra_dbg_force_conninfra_wakeup(int par1, int par2, int par3)
{
	int ret;

	ret = conninfra_core_force_conninfra_wakeup();
	if (ret)
		pr_info("conninfra wakup fail\n");
	else
		pr_info("conninfra wakup success\n");

	return 0;
}

int conninfra_dbg_force_conninfra_sleep(int par1, int par2, int par3)
{
	int ret;

	ret = conninfra_core_force_conninfra_sleep();
	if (ret)
		pr_info("conninfra sleep fail\n");
	else
		pr_info("conninfra sleep success\n");

	return 0;
}

int conninfra_dbg_efuse_read(int par1, int par2, int par3)
{
#if 0
	/* par2-->efuse address */
	/* par3-->register mask */
	Uint value = 0x0;
	Uint iRet = -1;

	iRet = wmt_lib_efuse_rw(0, par2, &value, par3);
	pr_info("read combo chip efuse (0x%08x) with mask (0x%08x) %s, value = 0x%08x\n",
		      par2, par3, iRet != 0 ? "failed" : "succeed", iRet != 0 ? -1 : value);
#endif
	return 0;
}

int conninfra_dbg_efuse_write(int par1, int par2, int par3)
{
#if 0
	/* par2-->efuse address */
	/* par3-->value to set */
	Uint iRet = -1;

	iRet = wmt_lib_efuse_rw(1, par2, &par3, 0xffffffff);
	pr_info("write combo chip efuse (0x%08x) with value (0x%08x) %s\n",
		      par2, par3, iRet != 0 ? "failed" : "succeed");
#endif
	return 0;
}


static int conninfra_dbg_ap_reg_read(int par1, int par2, int par3)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	pr_info("AP register read, reg address:0x%x\n", par2);
	ap_reg_base = ioremap_nocache(par2, 0x4);
	if (ap_reg_base) {
		value = readl(ap_reg_base);
		pr_info("AP register read, reg address:0x%x, value:0x%x\n", par2, value);
		iounmap(ap_reg_base);
	} else
		pr_err("AP register ioremap fail!\n");

	return 0;
}

static int conninfra_dbg_ap_reg_write(int par1, int par2, int par3)
{
	int value = 0x0;
	unsigned char *ap_reg_base = NULL;

	pr_info("AP register write, reg address:0x%x, value:0x%x\n", par2, par3);

	ap_reg_base = ioremap_nocache(par2, 0x4);
	if (ap_reg_base) {
		writel(par3, ap_reg_base);
		value = readl(ap_reg_base);
		pr_info("AP register write done, value after write:0x%x\n", value);
		iounmap(ap_reg_base);
	} else
		pr_err("AP register ioremap fail!\n");

	return 0;
}

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
static int conninfra_dbg_set_fw_log_mode(int par1, int par2, int par3)
{
	connsys_dedicated_log_set_log_mode(par2);
	return 0;
}

static int conninfra_dbg_fw_log_dump_emi(int par1, int offset, int size)
{
	//connsys_dedicated_log_dump_emi(offset, size);
	return 0;
}
#endif

/********************************************************/
/* par2:       */
/*     0: Off  */
/*     others: alarm time (seconds) */
/********************************************************/
static int conninfra_dbg_suspend_debug(int par1, int par2, int par3)
{
#if 0
	if (par2 > 0)
		connsys_log_alarm_enable(par2);
	else
		connsys_log_alarm_disable();
#endif
	return 0;
}


static int conninfra_dbg_fw_log_ctrl(int par1, int onoff, int level)
{
	/* Parameter format
	 *	onoff:
	 *		- bit 24~31: unused
	 *		- bit 16~23: subsys type
	 *		- bit 8~15 : unused
	 *		- bit 0~7  : on/off setting
	 *	level: only lowest 8 bites will be used.
	 *		- bit 8~31 : unused
	 *		- bit 0~7  : log level setting
	 * Example:
	 *	1. To turn on MCU log
	 *		onoff = 0x0001
	 *	2. To turn on Subsys Z log
	 *		onoff = 0x0z01 (z = subsys)
	 *	3. To turn off Subsys Z log
	 *		onoff = 0x0z00 (z = subsys)
	 */
#if 0
	UINT8 type = (unsigned char)(onoff >> 16);

	pr_info("Configuring firmware log: type:%d, on/off:%d, level:%d\n",
			type, (unsigned char)onoff, (unsigned char)level);
	//wmt_lib_fw_log_ctrl(type, (unsigned char)onoff, (unsigned char)level);
#endif
	return 0;
}

int conninfra_dbg_thermal_query(int par1, int count, int interval)
{
	int temp, ret;

	ret = conninfra_core_thermal_query(&temp);

	pr_info("[Conninfra_Thermal_Query] ret=[%d] temp=[%d]", ret, temp);

	return 0;
}

int conninfra_dbg_thermal_ctrl(int par1, int par2, int par3)
{
#if 0
	if (par2 == 0) {
		if (par3 >= 99) {
			pr_info("Can`t set temp threshold greater or queal 99\n");
			return -1;
		}
		wmt_dev_set_temp_threshold(par3);
	}
#endif
	return 0;
}

static int conninfra_dbg_connsys_emi_dump(int par1, int par2, int par3)
{
	unsigned int start;
	// make size 16-byte alignment
	int size = (((par3 + 15) >> 4) << 4);
	void __iomem *vir_addr = NULL;
	char* buf = NULL;
	struct consys_emi_addr_info* addr_info = emi_mng_get_phy_addr();
	int i;

	if (par2 & 0xf) {
		pr_err("EMI dump fail: wrong offset(0x%x), should be 16-byte alignment\n", par2);
		return -1;
	}

	start = (unsigned int)(par2 + addr_info->emi_ap_phy_addr);

	buf = (char*)osal_malloc(sizeof(char)*size);
	if (buf == NULL) {
		pr_err("[%s] allocate buffer fail\n", __func__);
		return -1;
	}

	pr_info("EMI dump, offset=0x%x(physical addr=0x%x), size=0x%x\n", par2, start, size);
	vir_addr = ioremap_nocache(start, size);
	if (!vir_addr) {
		pr_err("ioremap fail");
		osal_free(buf);
		return -1;
	}
	memcpy_fromio(buf, vir_addr, size);
	for (i = 0; i < size; i+= 16) {
		pr_info(
			"EMI[0x%x]: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			par2 + i,
			buf[i + 0], buf[i + 1], buf[i + 2], buf[i + 3],
			buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7],
			buf[i + 8], buf[i + 9], buf[i + 0xa], buf[i + 0xb],
			buf[i + 0xc], buf[i + 0xd], buf[i + 0xe], buf[i + 0xf]);
	}

	iounmap(vir_addr);
	osal_free(buf);
	return 0;
}
#endif /* CONNINFRA_DBG_SUPPORT */

static int conninfra_dbg_connsys_coredump_ctrl(int par1, int par2, int par3)
{
	unsigned int orig_mode = connsys_coredump_get_mode();

	pr_info("Setup coredump mode from %d to %d\n", orig_mode, par2);
	connsys_coredump_set_dump_mode(par2);
	return 0;
}

static int conninfra_dbg_connsys_coredump_mode_query(int par1, int par2, int par3)
{
	unsigned int orig_mode = connsys_coredump_get_mode();

	pr_info("Connsys coredump mode is [%d]\n", orig_mode);
	return orig_mode;
}

ssize_t conninfra_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	int dump_len;

	ret = osal_lock_sleepable_lock(&g_dump_lock);
	if (ret) {
		pr_err("dump_lock fail!!");
		return ret;
	}

	if (g_dump_buf_len == 0)
		goto exit;

	if (*f_pos == 0)
		g_dump_buf_ptr = g_dump_buf;

	dump_len = g_dump_buf_len >= count ? count : g_dump_buf_len;
	ret = copy_to_user(buf, g_dump_buf_ptr, dump_len);
	if (ret) {
		pr_err("copy to dump info buffer failed, ret:%d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	*f_pos += dump_len;
	g_dump_buf_len -= dump_len;
	g_dump_buf_ptr += dump_len;
	pr_info("conninfra_dbg:after read,wmt for dump info buffer len(%d)\n", g_dump_buf_len);

	ret = dump_len;
exit:

	osal_unlock_sleepable_lock(&g_dump_lock);
	return ret;
}

ssize_t conninfra_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	size_t len = count;
	char buf[256];
	char* pBuf;
	int x = 0, y = 0, z = 0;
	char* pToken = NULL;
	char* pDelimiter = " \t";
	long res = 0;
	static char dbg_enabled;

	pr_info("write parameter len = %d\n\r", (int) len);
	if (len >= osal_sizeof(buf)) {
		pr_err("input handling fail!\n");
		len = osal_sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_info("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = osal_strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x = (int)res;
	} else {
		x = 0;
	}

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		y = (int)res;
		pr_info("y = 0x%08x\n\r", y);
	} else {
		y = 3000;
		/*efuse, register read write default value */
		if (0x5 == x || 0x6 == x)
			y = 0x80000000;
	}

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		z = (int)res;
	} else {
		z = 10;
		/*efuse, register read write default value */
		if (0x5 == x || 0x6 == x)
			z = 0xffffffff;
	}

	pr_info("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	/* For eng and userdebug load, have to enable wmt_dbg by writing 0xDB9DB9 to
	 * "/proc/driver/wmt_dbg" to avoid some malicious use
	 */
#if CONNINFRA_DBG_SUPPORT
	if (0xDB9DB9 == x) {
		dbg_enabled = 1;
		return len;
	}
#endif
	/* For user load, only 0x13 is allowed to execute */
	/* allow command 0x2e to enable catch connsys log on userload  */
	if (0 == dbg_enabled && (x != 0x13) && (x != 0x14)) {
		pr_info("please enable conninfra debug first\n\r");
		return len;
	}

	if (osal_array_size(conninfra_dev_dbg_func) > x && NULL != conninfra_dev_dbg_func[x])
		(*conninfra_dev_dbg_func[x]) (x, y, z);
	else
		pr_warn("no handler defined for command id(0x%08x)\n\r", x);

	return len;
}

int conninfra_dev_dbg_init(void)
{
	static const struct file_operations conninfra_dbg_fops = {
		.owner = THIS_MODULE,
		.read = conninfra_dbg_read,
		.write = conninfra_dbg_write,
	};
	int i_ret = 0;

	g_conninfra_dbg_entry = proc_create(CONNINFRA_DBG_PROCNAME, 0664, NULL, &conninfra_dbg_fops);
	if (g_conninfra_dbg_entry == NULL) {
		pr_err("Unable to create / wmt_aee proc entry\n\r");
		i_ret = -1;
	}

	osal_sleepable_lock_init(&g_dump_lock);

	return i_ret;
}

int conninfra_dev_dbg_deinit(void)
{
	osal_sleepable_lock_deinit(&g_dump_lock);

	if (g_conninfra_dbg_entry != NULL) {
		proc_remove(g_conninfra_dbg_entry);
		g_conninfra_dbg_entry = NULL;
	}

	return 0;
}
