/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "bt.h"

/*******************************************************************************
*				 C O N S T A N T S
********************************************************************************
*/
#define BT_DBG_PROCNAME "driver/bt_dbg"
#define BUF_LEN_MAX 384
#define BT_DBG_DUMP_BUF_SIZE 1024
#define BT_DBG_PASSWD "4w2T8M65K5?2af+a "
#define BT_DBG_USER_TRX_PREFIX "[user-trx] "

/*******************************************************************************
*			       D A T A	 T Y P E S
********************************************************************************
*/
typedef int(*BT_DEV_DBG_FUNC) (int par1, int par2, int par3);
typedef struct {
  BT_DEV_DBG_FUNC func;
  bool turn_off_availavle; // function can be work when bt off
} tBT_DEV_DBG_STRUCT;

/*******************************************************************************
*			      P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int bt_dbg_get_bt_state(int par1, int par2, int par3);
static int bt_dbg_setlog_level(int par1, int par2, int par3);

/*******************************************************************************
*			     P R I V A T E   D A T A
********************************************************************************
*/
extern struct bt_dbg_st g_bt_dbg_st;
extern UINT32 gBtDbgLevel;
static struct proc_dir_entry *g_bt_dbg_entry;
static struct mutex g_bt_lock;
static char g_bt_dump_buf[BT_DBG_DUMP_BUF_SIZE];
static char *g_bt_dump_buf_ptr;
static int g_bt_dump_buf_len;
static bool g_bt_turn_on = FALSE;
static bool g_bt_dbg_enable = FALSE;

static const tBT_DEV_DBG_STRUCT bt_dev_dbg_struct[] = {
	[0xb] = {bt_dbg_setlog_level,		TRUE},
	[0xe] = {bt_dbg_get_bt_state,		TRUE},
};

/*******************************************************************************
*			       F U N C T I O N S
********************************************************************************
*/

void _bt_dbg_reset_dump_buf(void)
{
	memset(g_bt_dump_buf, '\0', BT_DBG_DUMP_BUF_SIZE);
	g_bt_dump_buf_ptr = g_bt_dump_buf;
	g_bt_dump_buf_len = 0;
}

int bt_dbg_get_bt_state(int par1, int par2, int par3)
{
	// 0x01: bt on, 0x00: bt off
	BT_LOG_PRT_INFO("g_bt_turn_on[%d]\n", g_bt_turn_on);
	_bt_dbg_reset_dump_buf();
	g_bt_dump_buf[0] = g_bt_turn_on;
	g_bt_dump_buf[1] = '\0';
	g_bt_dump_buf_len = 2;
	return 0;
}

int bt_dbg_setlog_level(int par1, int par2, int par3)
{
	if (par2 < BT_LOG_ERR || par2 > BT_LOG_DBG) {
		gBtDbgLevel = BT_LOG_INFO;
	} else {
		gBtDbgLevel = par2;
	}
	BT_LOG_PRT_INFO("gBtDbgLevel = %d\n", gBtDbgLevel);
	return 0;
}

ssize_t bt_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	int dump_len;

	ret = mutex_lock_killable(&g_bt_lock);
	if (ret) {
		BT_LOG_PRT_ERR("dump_lock fail!!\n");
		return ret;
	}

	if (g_bt_dump_buf_len == 0)
		goto exit;

	if (*f_pos == 0)
		g_bt_dump_buf_ptr = g_bt_dump_buf;

	dump_len = g_bt_dump_buf_len >= count ? count : g_bt_dump_buf_len;
	ret = copy_to_user(buf, g_bt_dump_buf_ptr, dump_len);
	if (ret) {
		BT_LOG_PRT_ERR("copy to dump info buffer failed, ret:%d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	*f_pos += dump_len;
	g_bt_dump_buf_len -= dump_len;
	g_bt_dump_buf_ptr += dump_len;
	BT_LOG_PRT_INFO("after read, wmt for dump info buffer len(%d)\n", g_bt_dump_buf_len);

	ret = dump_len;
exit:

	mutex_unlock(&g_bt_lock);
	return ret;
}

int _osal_strtol(const char *str, unsigned int adecimal, long *res)
{
	if (sizeof(long) == 4)
		return kstrtou32(str, adecimal, (unsigned int *) res);
	else
		return kstrtol(str, adecimal, res);
}

void bt_dbg_user_trx_cb(char *buf, int len)
{
	unsigned char *ptr = g_bt_dbg_st.rx_buf;
	unsigned int i = 0, evt_len = 0;
	int ret = 0;

	/* 1. bluedroid use partial read, this callback will enter several times
	   2. this function read and parse the command_complete event */
	memcpy(&g_bt_dbg_st.rx_buf[g_bt_dbg_st.rx_len], buf, len);
	g_bt_dbg_st.rx_len += len;

	// check the complete packet is read out by bluedroid
	if(g_bt_dbg_st.rx_len != (g_bt_dbg_st.rx_buf[2] + 3))
		return;

	// if this event is not the desire one, skip and reset buffer
	if((g_bt_dbg_st.rx_buf[4] + (g_bt_dbg_st.rx_buf[5] << 8)) != g_bt_dbg_st.trx_opcode) {
		g_bt_dbg_st.rx_len = 0;
		memset(g_bt_dbg_st.rx_buf, 0, sizeof(g_bt_dbg_st.rx_buf));
		return;
	}

	// desire rx event is received, write to read buffer as string
	evt_len = g_bt_dbg_st.rx_len;
	BT_LOG_PRT_INFO_RAW(g_bt_dbg_st.rx_buf, evt_len, "%s: len[%ud], RxEvt: ", __func__, evt_len);
	if(evt_len * 5 + 2 > BT_DBG_DUMP_BUF_SIZE)
		return;

	_bt_dbg_reset_dump_buf();
	for (i = 0; i < evt_len; i++) {
		ret = snprintf(g_bt_dump_buf + 5*i, 6, "0x%02X ", ptr[i]);
		if (ret < 0)  {
			BT_LOG_PRT_ERR("error snprintf return value = [%d]\n", ret);
			return;
		}
	}

	g_bt_dump_buf[5*evt_len] = '\n';
	g_bt_dump_buf[5*evt_len + 1] = '\0';
	g_bt_dump_buf_len = 5*evt_len + 1;

	// complete trx process
	complete(&g_bt_dbg_st.trx_comp);
}

void bt_dbg_user_trx_proc(char *cmd_raw)
{
#define LEN_64 64
	unsigned char hci_cmd[LEN_64];
	unsigned int len = 0;
	long tmp = 0;
	char *ptr = NULL, *pRaw = NULL;

	// Parse command raw data
	memset(hci_cmd, 0, sizeof(hci_cmd));
	pRaw = cmd_raw;
	ptr = cmd_raw;
	while(*ptr != '\0' && pRaw != NULL) {
		if (len > LEN_64 - 1) {
			BT_LOG_PRT_INFO("%s: skip since cmd length exceed!", __func__);
			return;
		}
		ptr = strsep(&pRaw, " ");
		if (ptr != NULL) {
			_osal_strtol(ptr, 16, &tmp);
			hci_cmd[len++] = (unsigned char)tmp;
		}
	}

	// Initialize rx variables
	g_bt_dbg_st.rx_len = 0;
	g_bt_dbg_st.trx_opcode = hci_cmd[1] + (hci_cmd[2] << 8);
	memset(g_bt_dbg_st.rx_buf, 0, sizeof(g_bt_dbg_st.rx_buf));
	BT_LOG_PRT_INFO_RAW(hci_cmd, len, "%s: len[%ud], TxCmd: ", __func__, len);

	// Send command and wait for command_complete event
	g_bt_dbg_st.trx_enable = TRUE;
	send_hci_frame(hci_cmd, len);
	if (!wait_for_completion_timeout(&g_bt_dbg_st.trx_comp, msecs_to_jiffies(2000)))
		BT_LOG_PRT_ERR("%s: wait event timeout!", __func__);
	g_bt_dbg_st.trx_enable = FALSE;
}

ssize_t bt_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	bool is_passwd = FALSE, is_turn_on = FALSE;
	size_t len = count;
	char buf[256], *pBuf;
	int x = 0, y = 0, z = 0;
	long res = 0;
	char* pToken = NULL;
	char* pDelimiter = " \t";

	if (len <= 0 || len >= sizeof(buf)) {
		BT_LOG_PRT_ERR("input handling fail!\n");
		len = sizeof(buf) - 1;
		return -1;
	}

	memset(buf, 0, sizeof(buf));
	if (copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = '\0';
	BT_LOG_PRT_INFO("g_bt_turn_on[%d], dbg_enable[%d], len[%d], data = %s\n",
		g_bt_turn_on, g_bt_dbg_enable, (int)len, buf);

	/* Check debug function is enabled or not
	 *   - not enable yet: user should enable it
	 *   - already enabled: user can disable it
	 */
	if (len > strlen(BT_DBG_PASSWD) &&
		0 == memcmp(buf, BT_DBG_PASSWD, strlen(BT_DBG_PASSWD))) {
		is_passwd = TRUE;
		if (0 == memcmp(buf + strlen(BT_DBG_PASSWD), "ON", strlen("ON")))
			is_turn_on = TRUE;
	}
	if(!g_bt_dbg_enable) {
		if(is_passwd && is_turn_on)
			g_bt_dbg_enable = TRUE;
		return len;
	} else {
		if(is_passwd && !is_turn_on) {
			g_bt_dbg_enable = FALSE;
			return len;
		}
	}

	/* Mode 1: User trx flow: send command, get response */
	if (0 == memcmp(buf, BT_DBG_USER_TRX_PREFIX, strlen(BT_DBG_USER_TRX_PREFIX))) {
		if(!g_bt_turn_on) // only work when bt on
			return len;
		buf[len - 1] = '\0';
		bt_dbg_user_trx_proc(buf + strlen(BT_DBG_USER_TRX_PREFIX));
		return len;
	}

	/* Mode 2: Debug cmd flow, parse three parameters */
	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		_osal_strtol(pToken, 16, &res);
		x = (int)res;
	} else {
		x = 0;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		_osal_strtol(pToken, 16, &res);
		y = (int)res;
		BT_LOG_PRT_INFO("y = 0x%08x\n", y);
	} else {
		y = 3000;
		/*efuse, register read write default value */
		if (0x5 == x || 0x6 == x)
			y = 0x80000000;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		_osal_strtol(pToken, 16, &res);
		z = (int)res;
	} else {
		z = 10;
		/*efuse, register read write default value */
		if (0x5 == x || 0x6 == x)
			z = 0xffffffff;
	}

	BT_LOG_PRT_INFO("x(0x%08x), y(0x%08x), z(0x%08x)\n", x, y, z);
	if (ARRAY_SIZE(bt_dev_dbg_struct) > x && NULL != bt_dev_dbg_struct[x].func) {
		if(!g_bt_turn_on && !bt_dev_dbg_struct[x].turn_off_availavle) {
			BT_LOG_PRT_WARN("command id(0x%08x) only work when bt on!\n", x);
		} else {
			(*bt_dev_dbg_struct[x].func) (x, y, z);
		}
	} else {
		BT_LOG_PRT_WARN("command id(0x%08x) no handler defined!\n", x);
	}

	return len;
}

int bt_dev_dbg_init(void)
{
	int i_ret = 0;
	static const struct file_operations bt_dbg_fops = {
		.owner = THIS_MODULE,
		.read = bt_dbg_read,
		.write = bt_dbg_write,
	};

	// initialize debug function struct
	g_bt_dbg_st.trx_enable = FALSE;
	g_bt_dbg_st.trx_opcode = 0;
	g_bt_dbg_st.trx_cb = bt_dbg_user_trx_cb;
	init_completion(&g_bt_dbg_st.trx_comp);

	g_bt_dbg_entry = proc_create(BT_DBG_PROCNAME, 0664, NULL, &bt_dbg_fops);
	if (g_bt_dbg_entry == NULL) {
		BT_LOG_PRT_ERR("Unable to create [%s] bt proc entry\n", BT_DBG_PROCNAME);
		i_ret = -1;
	}

	mutex_init(&g_bt_lock);

	BT_LOG_PRT_INFO("create [%s] done\n", BT_DBG_PROCNAME);
	return i_ret;
}

int bt_dev_dbg_deinit(void)
{
	mutex_destroy(&g_bt_lock);

	if (g_bt_dbg_entry != NULL) {
		proc_remove(g_bt_dbg_entry);
		g_bt_dbg_entry = NULL;
	}

	return 0;
}


int bt_dev_dbg_set_state(bool turn_on)
{
	g_bt_turn_on = turn_on;
	return 0;
}
