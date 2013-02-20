/* arch/arm/mach-msm/htc_battery.c
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <mach/msm_fast_timer.h>
#include <mach/msm_rpcrouter.h>
#include <mach/board.h>

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/switch.h>

#include "board-mahimahi.h"

extern void notify_usb_connected(int);

static char *supply_list[] = {
	"battery",
};

static struct switch_dev dock_switch = {
	.name = "dock",
};

static int vbus_present;
static int usb_status;
static bool dock_mains;

struct dock_state {
	struct mutex lock;
	u32 t;
	u32 last_edge_t[2];
	u32 last_edge_i[2];
	bool level;
	bool dock_connected_unknown;
};

static struct workqueue_struct *dock_wq;
static struct work_struct dock_work;
static struct wake_lock dock_work_wake_lock;
static struct dock_state ds = {
	.lock = __MUTEX_INITIALIZER(ds.lock),
};

#define _GPIO_DOCK MAHIMAHI_GPIO_DOCK

#define dock_out(n) gpio_direction_output(_GPIO_DOCK, n)
#define dock_out2(n) gpio_set_value(_GPIO_DOCK, n)
#define dock_in() gpio_direction_input(_GPIO_DOCK)
#define dock_read() gpio_get_value(_GPIO_DOCK)

#define MFM_DELAY_NS 10000

static int dock_get_edge(struct dock_state *s, u32 timeout, u32 tmin, u32 tmax)
{
	bool lin;
	bool in = s->level;
	u32 t;
	do {
		lin = in;
		in = dock_read();
		t = msm_read_fast_timer();
		if (in != lin) {
			s->last_edge_t[in] = t;
			s->last_edge_i[in] = 0;
			s->level = in;
			if ((s32)(t - tmin) < 0 || (s32)(t - tmax) > 0)
				return -1;
			return 1;
		}
	} while((s32)(t - timeout) < 0);
	return 0;
}

static bool dock_sync(struct dock_state *s, u32 timeout)
{
	u32 t;

	s->level = dock_read();
	t = msm_read_fast_timer();

	if (!dock_get_edge(s, t + timeout, 0, 0))
		return false;
	s->last_edge_i[s->level] = 2;
	return !!dock_get_edge(s,
			s->last_edge_t[s->level] + MFM_DELAY_NS * 4, 0, 0);
}

static int dock_get_next_bit(struct dock_state *s)
{
	u32 i = s->last_edge_i[!s->level] + ++s->last_edge_i[s->level];
	u32 target = s->last_edge_t[!s->level] + MFM_DELAY_NS * i;
	u32 timeout = target + MFM_DELAY_NS / 2;
	u32 tmin = target - MFM_DELAY_NS / 4;
	u32 tmax = target + MFM_DELAY_NS / 4;
	return dock_get_edge(s, timeout, tmin, tmax);
}

static u32 dock_get_bits(struct dock_state *s, int count, int *errp)
{
	u32 data = 0;
	u32 m = 1;
	int ret;
	int err = 0;
	while (count--) {
		ret = dock_get_next_bit(s);
		if (ret)
			data |= m;
		if (ret < 0)
			err++;
		m <<= 1;
	}
	if (errp)
		*errp = err;
	return data;
}

static void dock_delay(u32 timeout)
{
	timeout += msm_read_fast_timer();
	while (((s32)(msm_read_fast_timer() - timeout)) < 0)
		;
}

static int dock_send_bits(struct dock_state *s, u32 data, int count, int period)
{
	u32 t, t0, to;

	dock_out2(s->level);
	t = to = 0;
	t0 = msm_read_fast_timer();

	while (count--) {
		if (data & 1)
			dock_out2((s->level = !s->level));

		t = msm_read_fast_timer() - t0;
		if (t - to > period / 2) {
			pr_info("dock: to = %d, t = %d\n", to, t);
			return -EIO;
		}

		to += MFM_DELAY_NS;
		do {
			t = msm_read_fast_timer() - t0;
		} while (t < to);
		if (t - to > period / 4) {
			pr_info("dock: to = %d, t = %d\n", to, t);
			return -EIO;
		}
		data >>= 1;
	}
	return 0;
}

static u32 mfm_encode(u16 data, int count, bool p)
{
	u32 mask;
	u32 mfm = 0;
	u32 clock = ~data & ~(data << 1 | !!p);
	for (mask = 1UL << (count - 1); mask; mask >>= 1) {
		mfm |= (data & mask);
		mfm <<= 1;
		mfm |= (clock & mask);
	}
	return mfm;
}

static u32 mfm_decode(u32 mfm)
{
	u32 data = 0;
	u32 clock = 0;
	u32 mask = 1;
	while (mfm) {
		if (mfm & 1)
			clock |= mask;
		mfm >>= 1;
		if (mfm & 1)
			data |= mask;
		mfm >>= 1;
		mask <<= 1;
	}
	return data;
}

static int dock_command(struct dock_state *s, u16 cmd, int len, int retlen)
{
	u32 mfm;
	int count;
	u32 data = cmd;
	int ret;
	int err = -1;
	unsigned long flags;

	data = data << 2 | 3; /* add 0101 mfm data*/
	mfm = mfm_encode(data, len, false);
	count = len * 2 + 2;

	msm_enable_fast_timer();
	local_irq_save(flags);
	ret = dock_send_bits(s, mfm, count, MFM_DELAY_NS);
	if (!ret) {
		dock_in();
		if (dock_sync(s, MFM_DELAY_NS * 5))
			ret = dock_get_bits(s, retlen * 2, &err);
		else
			ret = -1;
		dock_out(s->level);
	}
	local_irq_restore(flags);

	dock_delay((ret < 0) ? MFM_DELAY_NS * 6 : MFM_DELAY_NS * 2);
	msm_disable_fast_timer();
	if (ret < 0) {
		pr_warning("dock_command: %x: no response\n", cmd);
		return ret;
	}
	data = mfm_decode(ret);
	mfm = mfm_encode(data, retlen, true);
	if (mfm != ret || err) {
		pr_warning("dock_command: %x: bad response, "
			   "data %x, mfm %x %x, err %d\n",
			   cmd, data, mfm, ret, err);
		return -EIO;
	}
	return data;
}

static int dock_command_retry(struct dock_state *s, u16 cmd, size_t len, size_t retlen)
{
	int retry = 20;
	int ret;
	while (retry--) {
		ret = dock_command(s, cmd, len, retlen);
		if (ret >= 0)
			return ret;
		if (retry != 19)
			msleep(10);
	}
	s->dock_connected_unknown = true;
	return -EIO;
}

static int dock_read_single(struct dock_state *s, int addr)
{
	int ret = -1, last;
	int retry = 20;
	while (retry--) {
		last = ret;
		ret = dock_command_retry(s, addr << 1, 6, 8);
		if (ret < 0 || ret == last)
			return ret;
	}
	return -EIO;
}

static int dock_read_multi(struct dock_state *s, int addr, u8 *data, size_t len)
{
	int ret;
	int i;
	u8 suml, sumr = -1;
	int retry = 20;
	while (retry--) {
		suml = 0;
		for (i = 0; i <= len; i++) {
			ret = dock_command_retry(s, (addr + i) << 1, 6, 8);
			if (ret < 0)
				return ret;
			if (i < len) {
				data[i] = ret;
				suml += ret;
			} else
				sumr = ret;
		}
		if (sumr == suml)
			return 0;

		pr_warning("dock_read_multi(%x): bad checksum, %x != %x\n",
			   addr, sumr, suml);
	}
	return -EIO;
}

static int dock_write_byte(struct dock_state *s, int addr, u8 data)
{
	return dock_command_retry(s, 1 | addr << 1 | data << 4, 6 + 8, 1);
}

static int dock_write_multi(struct dock_state *s, int addr, u8 *data, size_t len)
{
	int ret;
	int i;
	u8 sum;
	int retry = 2;
	while (retry--) {
		sum = 0;
		for (i = 0; i < len; i++) {
			sum += data[i];
			ret = dock_write_byte(s, addr + i, data[i]);
			if (ret < 0)
				return ret;
		}
		ret = dock_write_byte(s, addr + len, sum);
		if (ret <= 0)
			return ret;
	}
	return -EIO;
}

static int dock_acquire(struct dock_state *s)
{
	mutex_lock(&s->lock);
	dock_in();
	if (dock_read()) {
		/* Allow some time for the dock pull-down resistor to discharge
		 * the capasitor.
		 */
		msleep(20);
		if (dock_read()) {
			mutex_unlock(&s->lock);
			return -ENOENT;
		}
	}
	dock_out(0);
	s->level = false;
	return 0;
}

static void dock_release(struct dock_state *s)
{
	dock_in();
	mutex_unlock(&s->lock);
}

enum {
	DOCK_TYPE = 0x0,
	DOCK_BT_ADDR = 0x1, /* - 0x7 */

	DOCK_PIN_CODE = 0x0,
};

static ssize_t bt_addr_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	u8 bt_addr[6];

	ret = dock_acquire(&ds);
	if (ret < 0)
		return ret;
	ret = dock_read_multi(&ds, DOCK_BT_ADDR, bt_addr, 6);
	dock_release(&ds);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		bt_addr[0], bt_addr[1], bt_addr[2],
		bt_addr[3], bt_addr[4], bt_addr[5]);
}
static DEVICE_ATTR(bt_addr, S_IRUGO | S_IWUSR, bt_addr_show, NULL);

static ssize_t bt_pin_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int ret, i;
	u8 pin[4];

	if (size < 4)
		return -EINVAL;

	for (i = 0; i < sizeof(pin); i++) {
		if ((pin[i] = buf[i] - '0') > 10)
			return -EINVAL;
	}

	ret = dock_acquire(&ds);
	if (ret < 0)
		return ret;
	ret = dock_write_multi(&ds, DOCK_PIN_CODE, pin, 4);
	dock_release(&ds);
	if (ret < 0)
		return ret;

	return size;
}
static DEVICE_ATTR(bt_pin, S_IRUGO | S_IWUSR, NULL, bt_pin_store);


static int power_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	if (psy->type == POWER_SUPPLY_TYPE_MAINS)
		val->intval = (vbus_present && (usb_status == 2 || dock_mains));
	else
		val->intval = vbus_present;
	return 0;
}

static enum power_supply_property power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static struct power_supply ac_supply = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.supplied_to = supply_list,
	.num_supplicants = ARRAY_SIZE(supply_list),
	.properties = power_properties,
	.num_properties = ARRAY_SIZE(power_properties),
	.get_property = power_get_property,
};

static struct power_supply usb_supply = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.supplied_to = supply_list,
	.num_supplicants = ARRAY_SIZE(supply_list),
	.properties = power_properties,
	.num_properties = ARRAY_SIZE(power_properties),
	.get_property = power_get_property,
};

/* rpc related */
#define APP_BATT_PDEV_NAME		"rs30100001:00000000"
#define APP_BATT_PROG			0x30100001
#define APP_BATT_VER			MSM_RPC_VERS(0,0)
#define HTC_PROCEDURE_BATTERY_NULL	0
#define HTC_PROCEDURE_GET_BATT_LEVEL	1
#define HTC_PROCEDURE_GET_BATT_INFO	2
#define HTC_PROCEDURE_GET_CABLE_STATUS	3
#define HTC_PROCEDURE_SET_BATT_DELTA	4

static struct msm_rpc_endpoint *endpoint;

struct battery_info_reply {
	u32 batt_id;		/* Battery ID from ADC */
	u32 batt_vol;		/* Battery voltage from ADC */
	u32 batt_temp;		/* Battery Temperature (C) from formula and ADC */
	u32 batt_current;	/* Battery current from ADC */
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;		/* Full capacity of battery (mAh) */
};

static void dock_work_proc(struct work_struct *work)
{
	int dockid;

	if (!vbus_present || dock_acquire(&ds))
		goto no_dock;

	if (ds.dock_connected_unknown) {
		/* force a new dock notification if a command failed */
		switch_set_state(&dock_switch, 0);
		ds.dock_connected_unknown = false;
	}

	dockid = dock_read_single(&ds, DOCK_TYPE);
	dock_release(&ds);

	pr_info("Detected dock with ID %02x\n", dockid);
	if (dockid >= 0) {
		msm_hsusb_set_vbus_state(0);
		dock_mains = !!(dockid & 0x80);
		switch_set_state(&dock_switch, (dockid & 1) ? 2 : 1);
		goto done;
	}
no_dock:
	dock_mains = false;
	switch_set_state(&dock_switch, 0);
	msm_hsusb_set_vbus_state(vbus_present);
done:
	power_supply_changed(&ac_supply);
	power_supply_changed(&usb_supply);
	wake_unlock(&dock_work_wake_lock);
}

static int htc_battery_probe(struct platform_device *pdev)
{
	struct rpc_request_hdr req;	
	struct htc_get_batt_info_rep {
		struct rpc_reply_hdr hdr;
		struct battery_info_reply info;
	} rep;

	int rc;

	endpoint = msm_rpc_connect(APP_BATT_PROG, APP_BATT_VER, 0);
	if (IS_ERR(endpoint)) {
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
		       __FUNCTION__, PTR_ERR(endpoint));
		return PTR_ERR(endpoint);
	}

	/* must do this or we won't get cable status updates */
	rc = msm_rpc_call_reply(endpoint, HTC_PROCEDURE_GET_BATT_INFO,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
	if (rc < 0)
		printk(KERN_ERR "%s: get info failed\n", __FUNCTION__);

	power_supply_register(&pdev->dev, &ac_supply);
	power_supply_register(&pdev->dev, &usb_supply);

	INIT_WORK(&dock_work, dock_work_proc);
	dock_wq = create_singlethread_workqueue("dock");

	return 0;
}

static struct platform_driver htc_battery_driver = {
	.probe	= htc_battery_probe,
	.driver	= {
		.name	= APP_BATT_PDEV_NAME,
		.owner	= THIS_MODULE,
	},
};

/* batt_mtoa server definitions */
#define BATT_MTOA_PROG				0x30100000
#define BATT_MTOA_VERS				0
#define RPC_BATT_MTOA_NULL			0
#define RPC_BATT_MTOA_SET_CHARGING_PROC		1
#define RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC	2
#define RPC_BATT_MTOA_LEVEL_UPDATE_PROC		3

struct rpc_batt_mtoa_cable_status_update_args {
	int status;
};

static int handle_battery_call(struct msm_rpc_server *server,
			       struct rpc_request_hdr *req, unsigned len)
{	
	struct rpc_batt_mtoa_cable_status_update_args *args;

	if (req->procedure != RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC)
		return 0;

	args = (struct rpc_batt_mtoa_cable_status_update_args *)(req + 1);
	args->status = be32_to_cpu(args->status);
	pr_info("cable_status_update: status=%d\n",args->status);

	args->status = !!args->status;

	vbus_present = args->status;
	wake_lock(&dock_work_wake_lock);
	queue_work(dock_wq, &dock_work);
	return 0;
}

void notify_usb_connected(int status)
{
	printk("### notify_usb_connected(%d) ###\n", status);
	usb_status = status;
	power_supply_changed(&ac_supply);
	power_supply_changed(&usb_supply);
}

int is_ac_power_supplied(void)
{
	return vbus_present && (usb_status == 2 || dock_mains);
}

static struct msm_rpc_server battery_server = {
	.prog = BATT_MTOA_PROG,
	.vers = BATT_MTOA_VERS,
	.rpc_call = handle_battery_call,
};

static int __init htc_battery_init(void)
{
	int ret;
	gpio_request(_GPIO_DOCK, "dock");
	dock_in();
	wake_lock_init(&dock_work_wake_lock, WAKE_LOCK_SUSPEND, "dock");
	platform_driver_register(&htc_battery_driver);
	msm_rpc_create_server(&battery_server);
	if (switch_dev_register(&dock_switch) == 0) {
		ret = device_create_file(dock_switch.dev, &dev_attr_bt_addr);
		WARN_ON(ret);
		ret = device_create_file(dock_switch.dev, &dev_attr_bt_pin);
		WARN_ON(ret);
	}

	return 0;
}

module_init(htc_battery_init);
MODULE_DESCRIPTION("HTC Battery Driver");
MODULE_LICENSE("GPL");

