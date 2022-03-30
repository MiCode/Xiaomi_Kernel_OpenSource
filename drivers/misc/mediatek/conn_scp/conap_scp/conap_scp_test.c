// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>

#include "conap_scp.h"
#include "conap_scp_priv.h"
#include "conap_scp_test.h"

#define CONAP_DEV_NUM 1
#define CONAP_DRVIER_NAME "conap_drv"
#define CONAP_DEVICE_NAME "conap_dev"
#define CONAP_DEV_MAJOR 155
#define CONAP_DEV_IOC_MAGIC 0xc2

/* device node related */
static int g_conap_major = CONAP_DEV_MAJOR;
static struct class *p_conap_class;
static struct device *p_conap_dev;
static struct cdev g_conap_dev;
static ssize_t conap_dev_write(struct file *filp, const char __user *buffer, size_t count,
					loff_t *f_pos);

const struct file_operations g_conap_dev_fops = {
	.write = conap_dev_write,
};

typedef int(*CONAP_TEST_FUNC) (int par1, int par2, int par3);
static int conap_ut_send_msg(int par1, int par2, int par3);

static const CONAP_TEST_FUNC conap_test_func[] = {
	[0x1] = conap_ut_send_msg,
};

/**************************************************************************************/
/**************************************************************************************/
#define CONAP_SCP_TEST_INST_SZ 2
struct conap_scp_test_ctx {
	enum conap_scp_drv_type drv_type;
	char thread_name[64];
	struct task_struct *thread;
	struct conap_scp_drv_cb scp_test_cb;
};

struct conap_scp_test_ctx *g_em_test_ctx;
struct conap_scp_test_ctx *g_gps_test_ctx;


#define ELEM_MAX_LEN_SSID		32
#define MAC_ADDR_LEN			6
struct wlan_sensorhub_beacon_frame {
	uint16_t ucRcpiValue;			/* Rcpi */
	uint8_t aucBSSID[MAC_ADDR_LEN];		/* BSSID */
	uint8_t	ucChannel;			/* Channel */
	uint8_t ucBand;				/* 0: 2G4, 1: 5G, 2: 6G*/
	uint8_t	aucSsid[ELEM_MAX_LEN_SSID];	/* Ssid */
};

#define WF_SCAN_INFO_MAX_SZ  12
struct wlan_out_data {
	int result;
	uint32_t timestamp;
	uint32_t size;
	struct wlan_sensorhub_beacon_frame frame[WF_SCAN_INFO_MAX_SZ];
};
struct wlan_out_data g_wlan_out_data;

/*********************************************/
/* callback functions */
/*********************************************/
void conap_scp_test_em_msg_notify_cb(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	struct wlan_out_data *out_data;
	struct wlan_sensorhub_beacon_frame *frame;
	int i;

	out_data = (struct wlan_out_data *)buf;

	pr_info("[%s] result=[%x][%x][%x]", __func__, out_data->result,
				out_data->timestamp, out_data->size);
	for (i = 0; i < 12; i++) {
		frame = &(out_data->frame[i]);
		pr_info("[%s] [%x] [%x-%x-%x-%x-%x-%x]", __func__, frame->ucRcpiValue,
					frame->aucBSSID[0], frame->aucBSSID[1], frame->aucBSSID[2],
					frame->aucBSSID[3], frame->aucBSSID[4], frame->aucBSSID[5]);
	}
}

void conap_scp_test_em_state_change(int state)
{
	pr_info("[%s] reason=[%d]", __func__, state);
}

#define EM_MSG_START_TEST 1
int test_case_func_3(struct conap_scp_test_ctx *ctx)
{
	int ret;
	int i;

	ret = conap_scp_is_drv_ready(ctx->drv_type);
	pr_info("[%s][Test-3] drv=[%d] EM ready =[%d]", __func__, ctx->drv_type, ret);

	memset(&g_wlan_out_data, 0, sizeof(struct wlan_out_data));

	g_wlan_out_data.result = 0x99;
	g_wlan_out_data.timestamp = 0x32;
	g_wlan_out_data.size = WF_SCAN_INFO_MAX_SZ;
	for (i = 0; i < WF_SCAN_INFO_MAX_SZ; i++)
		memset(&(g_wlan_out_data.frame[i]), 0x11+i,
					sizeof(struct wlan_sensorhub_beacon_frame));

	ret = conap_scp_send_message(ctx->drv_type, EM_MSG_START_TEST,
				(unsigned char *)&g_wlan_out_data, sizeof(struct wlan_out_data));
	pr_info("[%s] send msg [%d]", ctx->thread_name, ret);
	return 0;
}

static int conap_scp_test_thread(void *pvData)
{
	int ret, loop = 0;
	struct conap_scp_test_ctx *ctx = (struct conap_scp_test_ctx *)pvData;

	WARN_ON(ctx == NULL);

	pr_info("[%s] init drv=[%d]", ctx->thread_name, ctx->drv_type);
	ret = conap_scp_register_drv(ctx->drv_type, &ctx->scp_test_cb);
	pr_info("[%s] scp register [%d] [%d]", ctx->thread_name, ret, ctx->drv_type);

	while (1) {
		if (kthread_should_stop())
			break;
		pr_info("[test_thread] loop =%d=", loop);
		msleep(1000);
		test_case_func_3(ctx);
		msleep(1000);

		//if (true) break;
		loop++;
	}

	pr_info("[%s] thread stop!!", ctx->thread_name);
	return 0;
}

int conap_ut_send_msg(int par1, int par2, int par3)
{
	struct conap_scp_test_ctx *test_ctx;
	//struct conap_scp_drv_cb scp_test_cb;

	/* ======= EM ====== */
	g_em_test_ctx = kmalloc(sizeof(struct conap_scp_test_ctx), GFP_KERNEL);
	if (g_em_test_ctx == NULL)
		return -1;
	test_ctx = g_em_test_ctx;

	//for (int i = 0; i < CONAP_SCP_TEST_INST_SZ; i++) {
	//for (i = 0; i < 1; i++) {
		memset(test_ctx, 0, sizeof(struct conap_scp_test_ctx));
		test_ctx->drv_type = DRV_TYPE_EM;
		//memcpy(&test_ctx->scp_test_cb, &g_scp_test_cb, sizeof(struct conap_scp_drv_cb));
		test_ctx->scp_test_cb.conap_scp_msg_notify_cb = conap_scp_test_em_msg_notify_cb;
		test_ctx->scp_test_cb.conap_scp_state_notify_cb = conap_scp_test_em_state_change;

		snprintf(test_ctx->thread_name, 64, "conap_test_em");

		test_ctx->thread = kthread_create(conap_scp_test_thread, test_ctx,
									test_ctx->thread_name);

		if (IS_ERR(test_ctx->thread)) {
			kfree(g_em_test_ctx);
			return -1;
		}
		wake_up_process(test_ctx->thread);
	//}
	return 0;
}
/**************************************************************************************/

ssize_t conap_dev_write(struct file *filp, const char __user *buffer, size_t count,
					loff_t *f_pos)
{
	size_t len = count;
	char buf[256];
	char *pBuf;
	char *pDelimiter = " \t";
	int x = 0, y = 0, z = 0;
	char *pToken = NULL;
	long res = 0;
	//static int test_enabled = -1;

	pr_info("write parameter len = %d\n\r", (int) len);
	if (len >= sizeof(buf)) {
		pr_info("input handling fail!\n");
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_info("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		kstrtol(pToken, 16, &res);
		x = (int)res;
	} else {
		x = 0;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		kstrtol(pToken, 16, &res);
		y = (int)res;
		pr_info("y = 0x%08x\n\r", y);
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		kstrtol(pToken, 16, &res);
		z = (int)res;
	}

	pr_info("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	if (ARRAY_SIZE(conap_test_func) > x &&
		conap_test_func[x] != NULL)
		(*conap_test_func[x]) (x, y, z);
	else
		pr_info("no handler defined for command id(0x%08x)\n\r", x);

	return len;
}


int conap_scp_test_init(void)
{
	dev_t dev_id = MKDEV(g_conap_major, 0);
	int ret = 0;

	ret = register_chrdev_region(dev_id, CONAP_DEV_NUM,
						CONAP_DRVIER_NAME);
	if (ret) {
		pr_info("fail to register chrdev.(%d)\n", ret);
		return -1;
	}

	cdev_init(&g_conap_dev, &g_conap_dev_fops);
	g_conap_dev.owner = THIS_MODULE;

	ret = cdev_add(&g_conap_dev, dev_id, CONAP_DEV_NUM);
	if (ret) {
		pr_info("cdev_add() fails (%d)\n", ret);
		goto err1;
	}

	p_conap_class = class_create(THIS_MODULE, CONAP_DEVICE_NAME);
	if (IS_ERR(p_conap_class)) {
		pr_info("class create fail, error code(%ld)\n",
						PTR_ERR(p_conap_class));
		goto err2;
	}

	p_conap_dev = device_create(p_conap_class, NULL, dev_id,
						NULL, CONAP_DEVICE_NAME);
	if (IS_ERR(p_conap_dev)) {
		pr_info("device create fail, error code(%ld)\n",
						PTR_ERR(p_conap_dev));
		goto err3;
	}

	return 0;
err3:

	pr_info("[%s] err3", __func__);
	if (p_conap_class) {
		class_destroy(p_conap_class);
		p_conap_class = NULL;
	}
err2:
	pr_info("[%s] err2", __func__);
	cdev_del(&g_conap_dev);

err1:
	pr_info("[%s] err1", __func__);
	unregister_chrdev_region(dev_id, CONAP_DEV_NUM);

	return -1;
}

int conap_scp_test_deinit(void)
{
	dev_t dev_id = MKDEV(g_conap_major, 0);

	if (p_conap_dev) {
		device_destroy(p_conap_class, dev_id);
		p_conap_dev = NULL;
	}

	if (p_conap_class) {
		class_destroy(p_conap_class);
		p_conap_class = NULL;
	}

	cdev_del(&g_conap_dev);
	unregister_chrdev_region(dev_id, CONAP_DEV_NUM);

	return 0;
}
