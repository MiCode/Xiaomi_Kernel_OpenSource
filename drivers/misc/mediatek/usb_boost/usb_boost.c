/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/kdev_t.h>

#include "usb_boost.h"
#define USB_BOOST_CLASS_NAME "usb_boost"
enum{
	ATTR_ENABLE,
	ATTR_TIMEOUT,
	ATTR_POLLING_INTVAL,
	ATTR_RAW,
	_ATTR_PARA_RW_MAXID,

	ATTR_CMD,

	ATTR_RO_REF_TIME,
	ATTR_RO_IS_RUNNING,
	ATTR_RO_WORK_CNT,
	_ATTR_PARA_MAXID
};
enum{
	_ATTR_ARG_BEGIN = _ATTR_PARA_MAXID - 1,
	ATTR_ARG1,
	ATTR_ARG2,
	ATTR_ARG3,
	_ATTR_ARG_MAXID
};

#define _ATTR_MAXID _ATTR_ARG_MAXID
static char *attr_name[_ATTR_MAXID] = {
	/* para part */
	"enable",
	"timeout",
	"poll_intval",
	"raw",
	"para_rx_maxid",

	"cmd",

	"ro_ref_time",
	"ro_is_running",
	"ro_work_cnt",

	/* arg part */
	"arg1",
	"arg2",
	"arg3",
};
enum{
	CMD_BOOST_TEST,
	CMD_OVERHEAD_TEST,
	CMD_OVERHEAD_TEST_BY_ID,
	CMD_DUMP_INFO,
	CMD_HOOK_NORMAL,
	CMD_HOOK_EMPTY,
	CMD_HOOK_CNT,
	_CMD_MAXID
};
static char *type_name[_TYPE_MAXID] = {
	"cpu_freq",
	"cpu_core",
};
#define MAX_LEN_WQ_NAME 32
static int trigger_cnt_disabled;
static int enabled;
static int inited;
static struct class *usb_boost_class;
static int cpu_freq_dft_para[_ATTR_PARA_RW_MAXID] = {1, 3, 300, 0};
static int cpu_core_dft_para[_ATTR_PARA_RW_MAXID] = {1, 3, 300, 0};
static void __usb_boost_empty(void) { return; }
static void __usb_boost_cnt(void) { trigger_cnt_disabled++; return; }
static void __usb_boost_by_id_empty(int id) { return; }
static void __request_empty(int id) { return; }

struct boost_ops {
	void (*boost)(void);

	void (*boost_by_id[_TYPE_MAXID])(int);
};

struct boost_ops __the_boost_ops = {
	__usb_boost_empty,
	{__usb_boost_by_id_empty,
	 __usb_boost_by_id_empty} };

/* -1 denote not used*/
static struct act_arg_obj cpu_freq_dft_arg = {1000000000, -1, -1};
static struct act_arg_obj cpu_core_dft_arg = {2, -1, -1};
static int test_diff_sec, test_diff_usec;

struct control_ops {
	int (*act[_ACT_MAXID]) (struct act_arg_obj *arg);
};
static struct mtk_usb_boost {
	struct control_ops ops;
	struct device_attribute attr[_ATTR_MAXID];
	int para[_ATTR_PARA_RW_MAXID];
	struct work_struct	work;
	struct workqueue_struct	*wq;
	int id;
	struct device *dev;
	int cmd;
	int is_running;
	struct timeval tv_ref_time;
	int work_cnt;
	struct act_arg_obj act_arg;
	void (*request_func)(int);
} boost_inst[_TYPE_MAXID];

static int update_time(int id);
static bool check_timeout(int id);
static void __usb_boost_by_id(int id);

/* public to IP platform level */
void usb_boost_set_para_and_arg(int id, int *para, int para_range, struct act_arg_obj *act_arg)
{
	int i;
	struct mtk_usb_boost *ptr_inst = &boost_inst[id];
	int *ptr_para = ptr_inst->para;

	USB_BOOST_NOTICE("para_range:<%d>\n", para_range);
	if (para_range > _ATTR_PARA_RW_MAXID) {
		USB_BOOST_NOTICE("ERROR, over range !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		USB_BOOST_NOTICE("para_range<%d>, _ATTR_PARA_RW_MAXID<%d>\n", para_range, _ATTR_PARA_RW_MAXID);
		return;
	}

	for (i = 0; i < para_range; i++, ptr_para++)
		*ptr_para = para[i];

	boost_inst[id].act_arg.arg1 = act_arg->arg1;
	boost_inst[id].act_arg.arg2 = act_arg->arg2;
	boost_inst[id].act_arg.arg3 = act_arg->arg3;

	/* hook callback by enable flag */
	if (para[0])
		__the_boost_ops.boost_by_id[id] = __usb_boost_by_id;
	else
		__the_boost_ops.boost_by_id[id] = __usb_boost_by_id_empty;
}

void usb_boost(void)
{
	__the_boost_ops.boost();
}

void usb_boost_by_id(int id)
{
	__the_boost_ops.boost_by_id[id](id);
}

void register_usb_boost_act(int type_id, int action_id, int (*func) (struct act_arg_obj *arg))
{
	boost_inst[type_id].ops.act[action_id] = func;
}

static void __request_it(int id)
{
	USB_BOOST_DBG("ID<%d>, WQ<%p>, WORK<%p>\n", id, boost_inst[id].wq, &(boost_inst[id].work));
	queue_work(boost_inst[id].wq, &(boost_inst[id].work));
	USB_BOOST_DBG("\n");
}

static void __usb_boost_by_id(int id)
{
	update_time(id);
	boost_inst[id].request_func(id);
}

static void __usb_boost(void)
{
	int id;

	USB_BOOST_DBG("\n");
	for (id = 0; id < _TYPE_MAXID; id++)
		usb_boost_by_id(id);
}

static void __boost_act(int type_id, int action_id)
{
	int (*func)(struct act_arg_obj *arg) = boost_inst[type_id].ops.act[action_id];
	struct act_arg_obj *arg = &boost_inst[type_id].act_arg;

	if (func)
		func(arg);
}

static void dump_info(int id)
{
	int n = 0;
	struct mtk_usb_boost *ptr_inst = &boost_inst[id];
	int *ptr_para = ptr_inst->para;

	/* PARA */
	for (n = 0; n < _ATTR_PARA_RW_MAXID; n++, ptr_para++)
		USB_BOOST_NOTICE("id<%d>, attr<%s>, val<%d>\n", id, attr_name[n], *ptr_para);

	/* RO */
	USB_BOOST_NOTICE("id<%d>, attr<%s>, val<%d,%d>\n", id, attr_name[ATTR_RO_REF_TIME],
			(unsigned int)boost_inst[id].tv_ref_time.tv_sec,
			(unsigned int)boost_inst[id].tv_ref_time.tv_usec);
	USB_BOOST_NOTICE("id<%d>, attr<%s>, val<%d>\n", id, attr_name[ATTR_RO_IS_RUNNING], boost_inst[id].is_running);
	USB_BOOST_NOTICE("id<%d>, attr<%s>, val<%d>\n", id, attr_name[ATTR_RO_WORK_CNT], boost_inst[id].work_cnt);

	/* ARG */
	USB_BOOST_NOTICE("id<%d>, attr<%s>, val<%d>\n", id, attr_name[ATTR_ARG1], boost_inst[id].act_arg.arg1);
	USB_BOOST_NOTICE("id<%d>, attr<%s>, val<%d>\n", id, attr_name[ATTR_ARG2], boost_inst[id].act_arg.arg2);
	USB_BOOST_NOTICE("id<%d>, attr<%s>, val<%d>\n", id, attr_name[ATTR_ARG3], boost_inst[id].act_arg.arg3);

}
static int update_time(int id)
{
	do_gettimeofday(&boost_inst[id].tv_ref_time);
	USB_BOOST_DBG("id:%d, ref<%d,%d>\n",
			id, (int)boost_inst[id].tv_ref_time.tv_sec, (int)boost_inst[id].tv_ref_time.tv_usec);
	return 1;
}

static bool check_timeout(int id)
{
	struct timeval tv, *ref;
	int diff_sec;

	ref = &boost_inst[id].tv_ref_time;
	do_gettimeofday(&tv);
	diff_sec = tv.tv_sec - ref->tv_sec;
	if (diff_sec >= boost_inst[id].para[ATTR_TIMEOUT]) {
		USB_BOOST_DBG("id<%d>, cur<%d,%d>, ref<%d,%d>\n",
				id, (int)tv.tv_sec, (int)tv.tv_usec, (int)ref->tv_sec, (int)ref->tv_usec);
		return true;
	}
	USB_BOOST_DBG("id<%d>, cur<%d,%d>, ref<%d,%d>\n",
			id, (int)tv.tv_sec, (int)tv.tv_usec, (int)ref->tv_sec, (int)ref->tv_usec);
	return false;
}

static void boost_work(struct work_struct *work_struct)
{
	struct mtk_usb_boost *ptr_inst = container_of(work_struct, struct mtk_usb_boost, work);
	int id = ptr_inst->id;
	int raw = ptr_inst->para[ATTR_RAW];
	int poll_intval = ptr_inst->para[ATTR_POLLING_INTVAL];

	ptr_inst->is_running = true;
	boost_inst[id].request_func = __request_empty;
	ptr_inst->work_cnt++;
	USB_BOOST_NOTICE("id:%d, begin of work\n", id);

	/* dump_info(id); */
	__boost_act(id, ACT_HOLD);
	/* dump_info(id); */
	while (1) {
		int timeout;

		USB_BOOST_NOTICE("id:%d, running of work\n", id);
		if (!ptr_inst->para[ATTR_ENABLE]) {
			/* dump_info(id); */
			break;
		}
		timeout = check_timeout(id);
		if (timeout) {
			/* dump_info(id); */
			break;
		}

		if (raw)
			__boost_act(id, ACT_HOLD);

		msleep(poll_intval);

	}

	/* dump_info(id); */
	__boost_act(id, ACT_RELEASE);
	boost_inst[id].request_func = __request_it;
	ptr_inst->is_running = false;
	USB_BOOST_NOTICE("id:%d, end of work\n", id);
	/* dump_info(id); */
}

static void default_setting(void)
{
	usb_boost_set_para_and_arg(TYPE_CPU_FREQ, cpu_freq_dft_para,
			sizeof(cpu_freq_dft_para)/sizeof(int), &cpu_freq_dft_arg);
	usb_boost_set_para_and_arg(TYPE_CPU_CORE, cpu_core_dft_para,
			sizeof(cpu_core_dft_para)/sizeof(int), &cpu_core_dft_arg);
}

static int which_attr(struct mtk_usb_boost *inst, struct device_attribute
		      *attr)
{
	int i;

	for (i = 0; i < _ATTR_MAXID; i++) {
		if (attr == &inst->attr[i])
			return i;
	}
	return -1;
}

static void test_loops(int id)
{
	int n;
	struct timeval tv_before, tv_after;
#define TEST_LOOP 100000
	do_gettimeofday(&tv_before);
	if (id < 0) {
		for (n = 0; n < TEST_LOOP; n++)
			usb_boost();
	} else {
		for (n = 0; n < TEST_LOOP; n++)
			usb_boost_by_id(id);
	}
	do_gettimeofday(&tv_after);
	test_diff_sec = tv_after.tv_sec - tv_before.tv_sec;
	test_diff_usec = tv_after.tv_usec - tv_before.tv_usec;
	USB_BOOST_NOTICE("id<%d>, loops:%d, spent %d sec, %d usec\n", id, TEST_LOOP, test_diff_sec, test_diff_usec);
}

static ssize_t attr_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int i, ret, idx = -1;
	long tmp;

	for (i = 0; i < _TYPE_MAXID; i++) {
		if (boost_inst[i].dev == dev) {
			idx = which_attr(&boost_inst[i], attr);
			break;
		}
	}
	if (i == _TYPE_MAXID)
		return 0;

	if (idx < 0) {
		USB_BOOST_NOTICE("sorry, I cannot find rawbulk fn '%s'\n", attr->attr.name);
		goto exit;
	}

	ret = kstrtol(buf, 0, &tmp);

	switch (idx) {
	/* normal usage */
	case ATTR_ENABLE:
		/* hook callback by enable flag */
		if (tmp)
			__the_boost_ops.boost_by_id[i] = __usb_boost_by_id;
		else
			__the_boost_ops.boost_by_id[i] = __usb_boost_by_id_empty;
		boost_inst[i].para[idx] = (int)tmp;
		break;
	case ATTR_TIMEOUT:
	case ATTR_POLLING_INTVAL:
	case ATTR_RAW:
		boost_inst[i].para[idx] = (int)tmp;
		break;
	/* command series */
	case ATTR_CMD:
		boost_inst[i].cmd = (int)tmp;
		switch (tmp) {
		case CMD_BOOST_TEST:
			USB_BOOST_NOTICE("usb_boost_by_id <%d>\n", i);
			usb_boost_by_id(i);
			break;
		case CMD_OVERHEAD_TEST:
			test_loops(-1);
			break;
		case CMD_OVERHEAD_TEST_BY_ID:
			test_loops(i);
			break;
		case CMD_DUMP_INFO:
			dump_info(i);
			break;
		case CMD_HOOK_NORMAL:
			__the_boost_ops.boost = __usb_boost;
			enabled = 1;
			break;
		case CMD_HOOK_EMPTY:
			__the_boost_ops.boost = __usb_boost_empty;
			enabled = 0;
			test_loops(-1);
			break;
		case CMD_HOOK_CNT:
			__the_boost_ops.boost = __usb_boost_cnt;
			enabled = 0;
			test_loops(-1);
			break;
		default:
			break;
		}
		break;
	/* ARG usage */
	case ATTR_ARG1:
		boost_inst[i].act_arg.arg1 = (int)tmp;
		break;
	case ATTR_ARG2:
		boost_inst[i].act_arg.arg2 = (int)tmp;
		break;
	case ATTR_ARG3:
		boost_inst[i].act_arg.arg3 = (int)tmp;
		break;
	default:
		break;
	}

exit:
	return count;
}

static ssize_t attr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	int idx;
	ssize_t count = 0;

	for (i = 0; i < _TYPE_MAXID; i++) {
		if (boost_inst[i].dev == dev) {
			idx = which_attr(&boost_inst[i], attr);
			break;
		}
	}
	if (i == _TYPE_MAXID)
		return 0;

	switch (idx) {
	/* normal usage */
	case ATTR_ENABLE:
	case ATTR_TIMEOUT:
	case ATTR_POLLING_INTVAL:
	case ATTR_RAW:
		count = sprintf(buf, "%d\n", boost_inst[i].para[idx]);
		break;
	case _ATTR_PARA_RW_MAXID:
		count = sprintf(buf, "%d\n", _ATTR_PARA_RW_MAXID);
		break;
	/* command series */
	case ATTR_CMD:
		switch (boost_inst[i].cmd) {
		case CMD_BOOST_TEST:
		case CMD_DUMP_INFO:
			count = sprintf(buf, "cmd<%d>\n", boost_inst[i].cmd);
			break;
		case CMD_OVERHEAD_TEST:
		case CMD_OVERHEAD_TEST_BY_ID:
		case CMD_HOOK_NORMAL:
		case CMD_HOOK_EMPTY:
		case CMD_HOOK_CNT:
			count = sprintf(buf, "cmd<%d>, <%d, %d>\n", boost_inst[i].cmd, test_diff_sec, test_diff_usec);
			break;
		default:
			break;
		}
		break;
	/* RO usage */
	case ATTR_RO_REF_TIME:
		count = sprintf(buf, "<%d,%d>\n",
				(int)boost_inst[i].tv_ref_time.tv_sec, (int)boost_inst[i].tv_ref_time.tv_usec);
		break;
	case ATTR_RO_IS_RUNNING:
		count = sprintf(buf, "%s\n", boost_inst[i].is_running ? "true" : "false");
		break;
	case ATTR_RO_WORK_CNT:
		count = sprintf(buf, "%d\n", boost_inst[i].work_cnt);
		break;
	/* ARG usage */
	case ATTR_ARG1:
		count = sprintf(buf, "%d\n", boost_inst[i].act_arg.arg1);
		break;
	case ATTR_ARG2:
		count = sprintf(buf, "%d\n", boost_inst[i].act_arg.arg2);
		break;
	case ATTR_ARG3:
		count = sprintf(buf, "%d\n", boost_inst[i].act_arg.arg3);
		break;
	default:
		break;
	}
	return count;
}

static int create_sys_fs(void)
{
	int i;

	USB_BOOST_NOTICE("\n");
	usb_boost_class = class_create(THIS_MODULE, USB_BOOST_CLASS_NAME);
	if (IS_ERR(usb_boost_class))
		return PTR_ERR(usb_boost_class);

	for (i = 0 ; i < _TYPE_MAXID ; i++) {

		boost_inst[i].dev = device_create(usb_boost_class, NULL, MKDEV(0,
							  i), NULL, type_name[i]);
		{
			int n, ret;

			for (n = 0; n < _ATTR_MAXID; n++) {
				boost_inst[i].attr[n].attr.name = attr_name[n];
				boost_inst[i].attr[n].attr.mode = 0600;
				boost_inst[i].attr[n].show = attr_show;
				boost_inst[i].attr[n].store = attr_store;

				ret = device_create_file(boost_inst[i].dev, &boost_inst[i].attr[n]);
				if (ret < 0) {
					while (--n >= 0)
						device_remove_file(boost_inst[i].dev, &boost_inst[i].attr[n]);
					return ret;
				}
			}
		}
	}
	return 0;

}

int usb_boost_init(void)
{
	int id;

	test_loops(-1);
	for (id = 0; id < _TYPE_MAXID; id++) {
		int count;
		char wq_name[MAX_LEN_WQ_NAME];

		count = sprintf(wq_name, "%s_wq", type_name[id]);
		wq_name[count] = '\0';
		test_loops(id);
		boost_inst[id].id  = id;
		update_time(id);
		boost_inst[id].wq  = create_singlethread_workqueue(wq_name);
		INIT_WORK(&boost_inst[id].work, boost_work);
		USB_BOOST_DBG("ID<%d>, WQ<%p>, WORK<%p>\n", id, boost_inst[id].wq, &(boost_inst[id].work));
		boost_inst[id].request_func = __request_it;
	}
	/* hook workable interface */
	__the_boost_ops.boost = __usb_boost;
	enabled = 1;

	create_sys_fs();
	default_setting();
	inited = 1;

	return 0;
}
module_param(trigger_cnt_disabled, int, 0600);
module_param(enabled, int, 0600);
module_param(inited, int, 0600);
