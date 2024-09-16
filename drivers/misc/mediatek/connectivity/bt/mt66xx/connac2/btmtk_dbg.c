/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "btmtk_define.h"
#include "btmtk_mt66xx_reg.h"
#include "btmtk_chip_if.h"
#include "conninfra.h"

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
static ssize_t bt_dbg_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static ssize_t bt_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static int bt_dbg_hwver_get(int par1, int par2, int par3);
static int bt_dbg_chip_rst(int par1, int par2, int par3);
static int bt_dbg_read_chipid(int par1, int par2, int par3);
static int bt_dbg_force_bt_wakeup(int par1, int par2, int par3);
static int bt_dbg_get_fwp_datetime(int par1, int par2, int par3);
static int bt_dbg_get_bt_patch_path(int par1, int par2, int par3);
extern int fwp_if_get_datetime(char *buf, int max_len);
extern int fwp_if_get_bt_patch_path(char *buf, int max_len);
#if (CUSTOMER_FW_UPDATE == 1)
static int bt_dbg_set_fwp_update_enable(int par1, int par2, int par3);
static int bt_dbg_get_fwp_update_info(int par1, int par2, int par3);
extern void fwp_if_set_update_enable(int par);
extern int fwp_if_get_update_info(char *buf, int max_len);
#endif
static int bt_dbg_reg_read(int par1, int par2, int par3);
static int bt_dbg_reg_write(int par1, int par2, int par3);
static int bt_dbg_ap_reg_read(int par1, int par2, int par3);
static int bt_dbg_ap_reg_write(int par1, int par2, int par3);
static int bt_dbg_setlog_level(int par1, int par2, int par3);
static int bt_dbg_set_rt_thread(int par1, int par2, int par3);
static int bt_dbg_get_bt_state(int par1, int par2, int par3);
static int bt_dbg_rx_buf_control(int par1, int par2, int par3);
static int bt_dbg_set_rt_thread_runtime(int par1, int par2, int par3);
static void bt_dbg_user_trx_proc(char *cmd_raw);
static void bt_dbg_user_trx_cb(char *buf, int len);

extern int32_t btmtk_send_data(struct hci_dev *hdev, uint8_t *buf, uint32_t count);
extern int32_t btmtk_set_wakeup(struct hci_dev *hdev);
extern int32_t btmtk_set_sleep(struct hci_dev *hdev, u_int8_t need_wait);
extern void bt_trigger_reset(void);



/*******************************************************************************
*			     P R I V A T E   D A T A
********************************************************************************
*/
extern struct btmtk_dev *g_bdev;
extern struct bt_dbg_st g_bt_dbg_st;
static struct proc_dir_entry *g_bt_dbg_entry;
static struct mutex g_bt_lock;
static char g_bt_dump_buf[BT_DBG_DUMP_BUF_SIZE];
static char *g_bt_dump_buf_ptr;
static int g_bt_dump_buf_len;
static bool g_bt_turn_on = FALSE;
static bool g_bt_dbg_enable = FALSE;

static const tBT_DEV_DBG_STRUCT bt_dev_dbg_struct[] = {
	[0x0] = {bt_dbg_hwver_get, 				FALSE},
	[0x1] = {bt_dbg_chip_rst, 				FALSE},
	[0x2] = {bt_dbg_read_chipid, 			FALSE},
	[0x3] = {bt_dbg_force_bt_wakeup,		FALSE},
	[0x4] = {bt_dbg_reg_read, 				FALSE},
	[0x5] = {bt_dbg_reg_write, 				FALSE},
	[0x6] = {bt_dbg_get_fwp_datetime,		TRUE},
#if (CUSTOMER_FW_UPDATE == 1)
	[0x7] = {bt_dbg_set_fwp_update_enable, 	TRUE},
	[0x8] = {bt_dbg_get_fwp_update_info,	FALSE},
#endif
	[0x9] = {bt_dbg_ap_reg_read,		FALSE},
	[0xa] = {bt_dbg_ap_reg_write,		TRUE},
	[0xb] = {bt_dbg_setlog_level,		TRUE},
	[0xc] = {bt_dbg_get_bt_patch_path,	TRUE},
	[0xd] = {bt_dbg_set_rt_thread,		TRUE},
	[0xe] = {bt_dbg_get_bt_state,		TRUE},
	[0xf] = {bt_dbg_rx_buf_control,		TRUE},
	[0x10] = {bt_dbg_set_rt_thread_runtime,		FALSE},
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

int bt_dbg_hwver_get(int par1, int par2, int par3)
{
	BTMTK_INFO("query chip version");
	/* TODO: */
	return 0;
}

int bt_dbg_chip_rst(int par1, int par2, int par3)
{
	if(par2 == 0)
		bt_trigger_reset();
	else
		conninfra_trigger_whole_chip_rst(CONNDRV_TYPE_BT, "bt_dbg");
	return 0;
}


int bt_dbg_read_chipid(int par1, int par2, int par3)
{
	return 0;
}

int bt_dbg_reg_read(int par1, int par2, int par3)
{
	uint32_t *dynamic_remap_addr = NULL;
	uint32_t *dynamic_remap_value = NULL;

	/* TODO: */
	dynamic_remap_addr = ioremap_nocache(0x18001104, 4);
	if (dynamic_remap_addr) {
		*dynamic_remap_addr = par2;
		BTMTK_DBG("read address = [0x%08x]", par2);
	} else {
		BTMTK_ERR("ioremap 0x18001104 fail");
		return -1;
	}
	iounmap(dynamic_remap_addr);

	dynamic_remap_value = ioremap_nocache(0x18900000, 4);
	if (dynamic_remap_value)
		BTMTK_INFO("%s: 0x%08x value = [0x%08x]", __func__, par2,
							*dynamic_remap_value);
	else {
		BTMTK_ERR("ioremap 0x18900000 fail");
		return -1;
	}
	iounmap(dynamic_remap_value);
	return 0;
}

int bt_dbg_reg_write(int par1, int par2, int par3)
{
	uint32_t *dynamic_remap_addr = NULL;
	uint32_t *dynamic_remap_value = NULL;

	/* TODO: */
	dynamic_remap_addr = ioremap_nocache(0x18001104, 4);
	if (dynamic_remap_addr) {
		*dynamic_remap_addr = par2;
		BTMTK_DBG("write address = [0x%08x]", par2);
	} else {
		BTMTK_ERR("ioremap 0x18001104 fail");
		return -1;
	}
	iounmap(dynamic_remap_addr);

	dynamic_remap_value = ioremap_nocache(0x18900000, 4);
	if (dynamic_remap_value)
		*dynamic_remap_value = par3;
	else {
		BTMTK_ERR("ioremap 0x18900000 fail");
		return -1;
	}
	iounmap(dynamic_remap_value);
	return 0;

}

int bt_dbg_ap_reg_read(int par1, int par2, int par3)
{
	uint32_t *remap_addr = NULL;

	/* TODO: */
	remap_addr = ioremap_nocache(par2, 4);
	if (!remap_addr) {
		BTMTK_ERR("ioremap [0x%08x] fail", par2);
		return -1;
	}

	BTMTK_INFO("%s: 0x%08x value = [0x%08x]", __func__, par2, *remap_addr);
	iounmap(remap_addr);
	return 0;
}

int bt_dbg_ap_reg_write(int par1, int par2, int par3)
{
	uint32_t *remap_addr = NULL;

	/* TODO: */
	remap_addr = ioremap_nocache(par2, 4);
	if (!remap_addr) {
		BTMTK_ERR("ioremap [0x%08x] fail", par2);
		return -1;
	}

	*remap_addr = par3;
	iounmap(remap_addr);
	return 0;
}

int bt_dbg_setlog_level(int par1, int par2, int par3)
{
	if (par2 < BTMTK_LOG_LVL_ERR || par2 > BTMTK_LOG_LVL_DBG) {
		btmtk_log_lvl = BTMTK_LOG_LVL_INFO;
	} else {
		btmtk_log_lvl = par2;
	}
	return 0;
}

int bt_dbg_set_rt_thread(int par1, int par2, int par3)
{
	g_bt_dbg_st.rt_thd_enable = par2;
	return 0;
}

int bt_dbg_set_rt_thread_runtime(int par1, int par2, int par3)
{
	struct sched_param params;
	int policy = 0;
	int ret = 0;

	/* reference parameter:
		- normal: 0x10 0x01(SCHED_FIFO) 0x01
		- normal: 0x10 0x01(SCHED_FIFO) 0x50(MAX_RT_PRIO - 20)
	*/
	if (par2 > SCHED_DEADLINE || par3 > MAX_RT_PRIO) {
		BTMTK_INFO("%s: parameter not allow!", __func__);
		return 0;
	}
	policy = par2;
	params.sched_priority = par3;
	ret = sched_setscheduler(g_bdev->tx_thread, policy, &params);
	BTMTK_INFO("%s: ret[%d], policy[%d], sched_priority[%d]", __func__, ret, policy, params.sched_priority);

	return 0;
}

int bt_dbg_get_bt_state(int par1, int par2, int par3)
{
	// 0x01: bt on, 0x00: bt off
	BTMTK_INFO("%s: g_bt_turn_on[%d]", __func__, g_bt_turn_on);
	_bt_dbg_reset_dump_buf();
	g_bt_dump_buf[0] = g_bt_turn_on;
	g_bt_dump_buf[1] = '\0';
	g_bt_dump_buf_len = 2;
	return 0;
}

int bt_dbg_force_bt_wakeup(int par1, int par2, int par3)
{
	int ret;

	BTMTK_INFO("%s", __func__);

	switch(par2) {
	case 0:
		g_bdev->psm.force_on = FALSE;
		ret = btmtk_set_sleep(g_bdev->hdev, TRUE);
		break;

	case 1:
		g_bdev->psm.force_on = TRUE;
		ret = btmtk_set_wakeup(g_bdev->hdev);
		break;
	default:
		BTMTK_ERR("Not support");
		return -1;
	}

	BTMTK_INFO("bt %s %s", (par2 == 1) ? "wakeup" : "sleep",
			        (ret) ? "fail" : "success");

	return 0;
}

int bt_dbg_get_fwp_datetime(int par1, int par2, int par3)
{
	_bt_dbg_reset_dump_buf();
	g_bt_dump_buf_len = fwp_if_get_datetime(g_bt_dump_buf, BT_DBG_DUMP_BUF_SIZE);
	return 0;
}

int bt_dbg_get_bt_patch_path(int par1, int par2, int par3)
{
	_bt_dbg_reset_dump_buf();
	g_bt_dump_buf_len = fwp_if_get_bt_patch_path(g_bt_dump_buf, BT_DBG_DUMP_BUF_SIZE);
	return 0;
}

#if (CUSTOMER_FW_UPDATE == 1)
int bt_dbg_set_fwp_update_enable(int par1, int par2, int par3)
{
	fwp_if_set_update_enable(par2);
	return 0;
}


int bt_dbg_get_fwp_update_info(int par1, int par2, int par3)
{
	_bt_dbg_reset_dump_buf();
	g_bt_dump_buf_len = fwp_if_get_update_info(g_bt_dump_buf, BT_DBG_DUMP_BUF_SIZE);
	return 0;
}
#endif

int bt_dbg_rx_buf_control(int par1, int par2, int par3)
{
	/*
		0x00: disable
		0x01: wait rx buffer available for max 200ms
	*/
	BTMTK_INFO("%s: rx_buf_ctrl[%d] set to [%d]", __func__, g_bt_dbg_st.rx_buf_ctrl, par2);
	g_bt_dbg_st.rx_buf_ctrl = par2;
	return 0;
}

ssize_t bt_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	int dump_len;

	ret = mutex_lock_killable(&g_bt_lock);
	if (ret) {
		BTMTK_ERR("%s: dump_lock fail!!", __func__);
		return ret;
	}

	if (g_bt_dump_buf_len == 0)
		goto exit;

	if (*f_pos == 0)
		g_bt_dump_buf_ptr = g_bt_dump_buf;

	dump_len = g_bt_dump_buf_len >= count ? count : g_bt_dump_buf_len;
	ret = copy_to_user(buf, g_bt_dump_buf_ptr, dump_len);
	if (ret) {
		BTMTK_ERR("%s: copy to dump info buffer failed, ret:%d", __func__, ret);
		ret = -EFAULT;
		goto exit;
	}

	*f_pos += dump_len;
	g_bt_dump_buf_len -= dump_len;
	g_bt_dump_buf_ptr += dump_len;
	BTMTK_INFO("%s: after read, wmt for dump info buffer len(%d)", __func__, g_bt_dump_buf_len);

	ret = dump_len;
exit:

	mutex_unlock(&g_bt_lock);
	return ret;
}

int osal_strtol(const char *str, unsigned int adecimal, long *res)
{
	if (sizeof(long) == 4)
		return kstrtou32(str, adecimal, (unsigned int *) res);
	else
		return kstrtol(str, adecimal, res);
}

void bt_dbg_user_trx_cb(char *buf, int len)
{
	unsigned char *ptr = buf;
	int i = 0;

	// if this event is not the desire one, skip and reset buffer
	if((buf[3] + (buf[4] << 8)) != g_bt_dbg_st.trx_opcode)
		return;

	// desire rx event is received, write to read buffer as string
	BTMTK_INFO_RAW(buf, len, "%s: len[%d], RxEvt: ", __func__, len);
	if((len + 1)*5 + 2 > BT_DBG_DUMP_BUF_SIZE)
		return;

	_bt_dbg_reset_dump_buf();
	// write event packet type
	if (snprintf(g_bt_dump_buf, 6, "0x04 ") < 0) {
		BTMTK_INFO("%s: snprintf error", __func__);
		goto end;

	}
	for (i = 0; i < len; i++) {
		if (snprintf(g_bt_dump_buf + 5*(i+1), 6, "0x%02X ", ptr[i]) < 0) {
			BTMTK_INFO("%s: snprintf error", __func__);
			goto end;
		}
	}
	len++;
	g_bt_dump_buf[5*len] = '\n';
	g_bt_dump_buf[5*len + 1] = '\0';
	g_bt_dump_buf_len = 5*len + 1;

end:
	// complete trx process
	complete(&g_bt_dbg_st.trx_comp);
}

void bt_dbg_user_trx_proc(char *cmd_raw)
{
#define LEN_64 64
	unsigned char hci_cmd[LEN_64];
	int len = 0;
	long tmp = 0;
	char *ptr = NULL, *pRaw = NULL;

	// Parse command raw data
	memset(hci_cmd, 0, sizeof(hci_cmd));
	pRaw = cmd_raw;
	ptr = cmd_raw;
	while(*ptr != '\0' && pRaw != NULL) {
		if (len > LEN_64 - 1) {
			BTMTK_INFO("%s: skip since cmd length exceed!", __func__);
			return;
		}
		ptr = strsep(&pRaw, " ");
		if (ptr != NULL) {
			osal_strtol(ptr, 16, &tmp);
			hci_cmd[len++] = (unsigned char)tmp;
		}
	}
	BTMTK_INFO_RAW(hci_cmd, len, "%s: len[%d], TxCmd: ", __func__, len);

	// Send command and wait for command_complete event
	g_bt_dbg_st.trx_opcode = hci_cmd[1] + (hci_cmd[2] << 8);
	g_bt_dbg_st.trx_enable = TRUE;
	btmtk_send_data(g_bdev->hdev, hci_cmd, len);
	if (!wait_for_completion_timeout(&g_bt_dbg_st.trx_comp, msecs_to_jiffies(2000)))
		BTMTK_ERR("%s: wait event timeout!", __func__);
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
		BTMTK_ERR("%s: input handling fail!", __func__);
		len = sizeof(buf) - 1;
		return -1;
	}

	memset(buf, 0, sizeof(buf));
	if (copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = '\0';
	BTMTK_INFO("%s: g_bt_turn_on[%d], dbg_enable[%d], len[%d], data = %s",
		__func__, g_bt_turn_on, g_bt_dbg_enable, (int)len, buf);

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
		osal_strtol(pToken, 16, &res);
		x = (int)res;
	} else {
		x = 0;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		y = (int)res;
		BTMTK_INFO("%s: y = 0x%08x\n\r", __func__, y);
	} else {
		y = 3000;
		/*efuse, register read write default value */
		if (0x5 == x || 0x6 == x)
			y = 0x80000000;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		z = (int)res;
	} else {
		z = 10;
		/*efuse, register read write default value */
		if (0x5 == x || 0x6 == x)
			z = 0xffffffff;
	}

	BTMTK_INFO("%s: x(0x%08x), y(0x%08x), z(0x%08x)", __func__, x, y, z);
	if (ARRAY_SIZE(bt_dev_dbg_struct) > x && NULL != bt_dev_dbg_struct[x].func) {
		if(!g_bt_turn_on && !bt_dev_dbg_struct[x].turn_off_availavle) {
			BTMTK_WARN("%s: command id(0x%08x) only work when bt on!", __func__, x);
		} else {
			(*bt_dev_dbg_struct[x].func) (x, y, z);
		}
	} else {
		BTMTK_WARN("%s: command id(0x%08x) no handler defined!", __func__, x);
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
	g_bt_dbg_st.rt_thd_enable = FALSE;
	g_bt_dbg_st.trx_enable = FALSE;
	g_bt_dbg_st.trx_cb = bt_dbg_user_trx_cb;
	g_bt_dbg_st.rx_buf_ctrl = TRUE;
	init_completion(&g_bt_dbg_st.trx_comp);

	g_bt_dbg_entry = proc_create(BT_DBG_PROCNAME, 0664, NULL, &bt_dbg_fops);
	if (g_bt_dbg_entry == NULL) {
		BTMTK_ERR("Unable to create [%s] bt proc entry", BT_DBG_PROCNAME);
		i_ret = -1;
	}

	mutex_init(&g_bt_lock);

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
