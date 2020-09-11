/*
 * ICUSB - for MUSB Host Driver
 *
 * Copyright 2015 Mediatek Inc.
 *	Marvin Lin <marvin.lin@mediatek.com>
 *	Arvin Wang <arvin.wang@mediatek.com>
 *	Vincent Fan <vincent.fan@mediatek.com>
 *	Bryant Lu <bryant.lu@mediatek.com>
 *	Yu-Chang Wang <yu-chang.wang@mediatek.com>
 *	Macpaul Lin <macpaul.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>

/*
 * Version Information
 */
#define DRIVER_VERSION ""
#define DRIVER_AUTHOR ""
#define DRIVER_DESC "USB ICUSB DRIVER"
#define DRIVER_LICENSE "GPL"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define ICCD_INTERFACE_CLASS 0x0B
#define ICCD_CLASS_DESCRIPTOR_LENGTH	(0x36)

#include "usb.h"
#include "musbfsh_icusb.h"

struct usb_icusb {
	char name[128];
};

struct my_attr power_resume_time_neogo_attr = {
	.attr.name = "power_resume_time_neogo",
	.attr.mode = 0644,
#ifdef MTK_ICUSB_POWER_AND_RESUME_TIME_NEOGO_SUPPORT
	.value = 1
#else
	.value = 0
#endif
};

static struct my_attr my_attr_test = {
	.attr.name = "my_attr_test",
	.attr.mode = 0644,
	.value = 1
};

static struct attribute *myattr[] = {
	(struct attribute *)&my_attr_test,
	(struct attribute *)&power_resume_time_neogo_attr,
	(struct attribute *)&skip_session_req_attr,
	(struct attribute *)&skip_enable_session_attr,
	(struct attribute *)&skip_mac_init_attr,
	(struct attribute *)&resistor_control_attr,
	(struct attribute *)&hw_dbg_attr,
	(struct attribute *)&skip_port_pm_attr,
	NULL
};

static struct IC_USB_CMD ic_cmd;
unsigned int g_ic_usb_status =
	((USB_PORT1_DISCONNECT_DONE) << USB_PORT1_STS_SHIFT);
static struct sock *netlink_sock;
static u_int g_pid;
static struct proc_dir_entry *proc_drv_icusb_dir_entry;

static void icusb_dump_data(char *buf, int len);
static void set_icusb_phy_power_negotiation_fail(void);
static void set_icusb_phy_power_negotiation_ok(void);
static void set_icusb_data_of_interface_power_request(short data);

static void icusb_resume_time_negotiation(struct usb_device *dev)
{
	int ret;
	int retries = IC_USB_RETRIES_RESUME_TIME_NEGOTIATION;
	char resume_time_negotiation_data[IC_USB_LEN_RESUME_TIME_NEGOTIATION];

	while (retries-- > 0) {
		MYDBG("");
		ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				      IC_USB_REQ_GET_INTERFACE_RESUME_TIME,
				      IC_USB_REQ_TYPE_GET_INTERFACE_RESUME_TIME,
				      IC_USB_WVALUE_RESUME_TIME_NEGOTIATION,
				      IC_USB_WINDEX_RESUME_TIME_NEGOTIATION,
				      resume_time_negotiation_data,
				      IC_USB_LEN_RESUME_TIME_NEGOTIATION,
				      USB_CTRL_GET_TIMEOUT);
		if (ret < 0) {
			MYDBG("ret : %d\n", ret);
			continue;
		} else {
			MYDBG("");
			icusb_dump_data(resume_time_negotiation_data,
					IC_USB_LEN_RESUME_TIME_NEGOTIATION);
			break;
		}

	}
}

void icusb_power_negotiation(struct usb_device *dev)
{
	int ret;
	int retries = IC_USB_RETRIES_POWER_NEGOTIATION;
	char get_power_negotiation_data[IC_USB_LEN_POWER_NEGOTIATION];
	char set_power_negotiation_data[IC_USB_LEN_POWER_NEGOTIATION];
	int power_negotiation_done = 0;
	enum PHY_VOLTAGE_TYPE phy_volt;

	while (retries-- > 0) {
		MYDBG("");
		power_negotiation_done = 0;
		ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				      IC_USB_REQ_GET_IFACE_POWER,
				      IC_USB_REQ_TYPE_GET_IFACE_POWER,
				      IC_USB_WVALUE_POWER_NEGOTIATION,
				      IC_USB_WINDEX_POWER_NEGOTIATION,
				      get_power_negotiation_data,
				      IC_USB_LEN_POWER_NEGOTIATION,
				      USB_CTRL_GET_TIMEOUT);
		if (ret < 0) {
			MYDBG("ret : %d\n", ret);
			continue;
		} else {
			MYDBG("");
			icusb_dump_data(get_power_negotiation_data,
					IC_USB_LEN_POWER_NEGOTIATION);

			/* copy the prefer bit from get interface power */
			set_power_negotiation_data[0] =
				(get_power_negotiation_data[0] &
					IC_USB_PREFER_CLASSB_ENABLE_BIT);

			/* set our current voltage */
			phy_volt = get_usb11_phy_voltage();
			if (phy_volt == VOL_33)
				set_power_negotiation_data[0] |=
					(char)IC_USB_CLASSB;
			else if (phy_volt == VOL_18)
				set_power_negotiation_data[0] |=
					(char)IC_USB_CLASSC;
			else
				MYDBG("");

			/* set current */
			if (set_power_negotiation_data[1] > IC_USB_CURRENT) {
				MYDBG("");
				set_power_negotiation_data[1] = IC_USB_CURRENT;
			} else {
				MYDBG("");
				set_power_negotiation_data[1] =
					get_power_negotiation_data[1];
			}
			MYDBG("power_negotiation_data[0] : 0x%x",
			      set_power_negotiation_data[0]);
			MYDBG("power_negotiation_data[1] : 0x%x",
			     set_power_negotiation_data[1]);
			MYDBG("IC_USB_CURRENT :%d\n", IC_USB_CURRENT);

			ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
					      IC_USB_REQ_SET_IFACE_POWER,
					      IC_USB_REQ_TYPE_SET_IFACE_POWER,
					      IC_USB_WVALUE_POWER_NEGOTIATION,
					      IC_USB_WINDEX_POWER_NEGOTIATION,
					      set_power_negotiation_data,
					      IC_USB_LEN_POWER_NEGOTIATION,
					      USB_CTRL_SET_TIMEOUT);

			if (ret < 0) {
				MYDBG("ret : %d\n", ret);
			} else {
				MYDBG("");
				power_negotiation_done = 1;
				break;
			}
			/* break; */
		}
	}

	MYDBG("retries : %d\n", retries);
	if (!power_negotiation_done) {
		set_icusb_phy_power_negotiation_fail();
	} else {
		set_icusb_data_of_interface_power_request(
			*((short *)get_power_negotiation_data));
		set_icusb_phy_power_negotiation_ok();
	}
}

void usb11_wait_disconnect_done(int value)
{
	if (is_usb11_enabled()) {
		while (1) {
			unsigned int ic_usb_status = g_ic_usb_status;

			MYDBG("ic_usb_status : %x\n", ic_usb_status);
			ic_usb_status &=
				(USB_PORT1_STS_MSK << USB_PORT1_STS_SHIFT);
			MYDBG("ic_usb_status : %x\n", ic_usb_status);

			if (ic_usb_status ==
				(USB_PORT1_DISCONNECT_DONE <<
					USB_PORT1_STS_SHIFT)) {
				MYDBG("USB_PORT1_DISCONNECT_DONE\n");
				break;
			}

			if (ic_usb_status ==
				(USB_PORT1_DISCONNECTING <<
					USB_PORT1_STS_SHIFT))
				MYDBG("USB_PORT1_DISCONNECTING\n");

			mdelay(10);
		}
	} else {
		MYDBG("usb11 is not enabled, skip\n");
		MYDBG("done()\n");
	}

}

int check_usb11_sts_disconnect_done(void)
{
	unsigned int ic_usb_status = g_ic_usb_status;

	MYDBG("ic_usb_status : %x\n", ic_usb_status);
	ic_usb_status &= (USB_PORT1_STS_MSK << USB_PORT1_STS_SHIFT);
	MYDBG("ic_usb_status : %x\n", ic_usb_status);

	if (ic_usb_status ==
	    (USB_PORT1_DISCONNECT_DONE << USB_PORT1_STS_SHIFT)) {
		MYDBG("USB_PORT1_DISCONNECT_DONE got\n");
		return 1;
	} else {
		return 0;
	}

}

void set_usb11_sts_connect(void)
{
	MYDBG("...................");
	g_ic_usb_status &= ~(USB_PORT1_STS_MSK << USB_PORT1_STS_SHIFT);
	g_ic_usb_status |= ((USB_PORT1_CONNECT) << USB_PORT1_STS_SHIFT);
}

void set_usb11_sts_disconnecting(void)
{
	MYDBG("...................");
	g_ic_usb_status &= ~(USB_PORT1_STS_MSK << USB_PORT1_STS_SHIFT);
	g_ic_usb_status |= ((USB_PORT1_DISCONNECTING) << USB_PORT1_STS_SHIFT);
}

void set_icusb_sts_disconnect_done(void)
{
	MYDBG("...................");
	g_ic_usb_status &= ~(USB_PORT1_STS_MSK << USB_PORT1_STS_SHIFT);
	g_ic_usb_status |= ((USB_PORT1_DISCONNECT_DONE) << USB_PORT1_STS_SHIFT);
}

void set_icusb_data_of_interface_power_request(short data)
{
	MYDBG("...................");
	g_ic_usb_status |= ((data) << PREFER_VOL_CLASS_SHIFT);
}

void reset_usb11_phy_power_negotiation_status(void)
{
	MYDBG("...................");

	g_ic_usb_status &= ~(PREFER_VOL_STS_MSK << PREFER_VOL_STS_SHIFT);
	g_ic_usb_status |= ((PREFER_VOL_NOT_INITED) << PREFER_VOL_STS_SHIFT);

}

void set_icusb_phy_power_negotiation_fail(void)
{
	MYDBG("...................");

	g_ic_usb_status &= ~(PREFER_VOL_STS_MSK << PREFER_VOL_STS_SHIFT);
	g_ic_usb_status |= ((PREFER_VOL_PWR_NEG_FAIL) << PREFER_VOL_STS_SHIFT);

}

void set_icusb_phy_power_negotiation_ok(void)
{
	MYDBG("...................");

	g_ic_usb_status &= ~(PREFER_VOL_STS_MSK << PREFER_VOL_STS_SHIFT);
	g_ic_usb_status |= ((PREFER_VOL_PWR_NEG_OK) << PREFER_VOL_STS_SHIFT);

}


void usb11_phy_prefer_3v_status_check(void)
{
	unsigned int ic_usb_status = g_ic_usb_status;

	MYDBG("ic_usb_status : %x\n", ic_usb_status);
	ic_usb_status &= (PREFER_VOL_STS_MSK << PREFER_VOL_STS_SHIFT);
	MYDBG("ic_usb_status : %x\n", ic_usb_status);
}


void icusb_dump_data(char *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		MYDBG("data[%d]: %x\n", i, buf[i]);

}

int usb11_init_phy_by_voltage(enum PHY_VOLTAGE_TYPE phy_volt)
{
	musbfsh_init_phy_by_voltage(phy_volt);
	return 0;
}

int usb11_session_control(enum SESSION_CONTROL_ACTION action)
{
	if (action == START_SESSION)
		musbfsh_start_session();
	else if (action == STOP_SESSION) {
		/* musbfsh_stop_session(); */
		if (!is_usb11_enabled()) {
			mt65xx_usb11_mac_reset_and_phy_stress_set();
		} else {
			MYDBG("usb11 has been enabled, skip");
			MYDBG("mt65xx_usb11_mac_reset_and_phy_stress_set()\n");
		}
	} else
		MYDBG("unknown action\n");


	return 0;
}

static void udp_reply(int pid, int seq, void *payload)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int size = strlen(payload) + 1;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;
	/* 3.10 specific */
	nlh = __nlmsg_put(skb, pid, seq, 0, size, 0);
	nlh->nlmsg_flags = 0;
	data = NLMSG_DATA(nlh);
	memcpy(data, payload, size);

	/* 3.10 specific */
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */
	ret = netlink_unicast(netlink_sock, skb, pid, MSG_DONTWAIT);
	if (ret < 0)
		MYDBG("send failed\n");
}

/* Receive messages from netlink socket. */
static void udp_receive(struct sk_buff *skb)
{
	kuid_t uid,
	u_int seq;
	void *data;
	struct nlmsghdr *nlh;
	char reply_data[16];

	MYDBG("");
	nlh = (struct nlmsghdr *)skb->data;

	/* global here */
	g_pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;
	data = NLMSG_DATA(nlh);
	MYDBG("recv skb from user space pid:%d seq:%d\n",
	      g_pid, seq);
	MYDBG("data is :%s\n", (char *)data);


	sprintf(reply_data, "%d", g_pid);
	udp_reply(g_pid, 0, reply_data);
}

struct netlink_kernel_cfg nl_cfg = {
	.input = udp_receive,
};


static ssize_t default_show(struct kobject *kobj, struct attribute *attr,
			    char *buf)
{
	struct my_attr *a = container_of(attr, struct my_attr, attr);

	return scnprintf(buf, PAGE_SIZE, "%d\n", a->value);
}

static ssize_t default_store(struct kobject *kobj, struct attribute *attr,
			     const char *buf, size_t len)
{
	struct my_attr *a = container_of(attr, struct my_attr, attr);
	int result = kstrtoul(buf, 0, (unsigned long *)&a->value);

	if (result)
		return sizeof(int);
	else
		return -EINVAL;
}

static const struct sysfs_ops myops = {
	.show = default_show,
	.store = default_store,
};

static struct kobj_type mytype = {
	.sysfs_ops = &myops,
	.default_attrs = myattr,
};

struct kobject *mykobj;
void create_icusb_sysfs_attr(void)
{
	int err = -1;

	mykobj = kzalloc(sizeof(*mykobj), GFP_KERNEL);
	if (mykobj) {
		MYDBG("");
		kobject_init(mykobj, &mytype);
		if (kobject_add(mykobj, NULL, "%s", "icusb_attr")) {
			err = -1;
			MYDBG("Sysfs creation failed\n");
			kobject_put(mykobj);
			mykobj = NULL;
		}
		err = 0;
	}
	return;

}

static ssize_t musbfsh_ic_tmp_proc_entry(struct file *file_ptr,
					 const char __user *user_buffer,
					 size_t count, loff_t *position)
{
	char cmd[64];
	int ret = copy_from_user((char *)&cmd, user_buffer, count);

	if (ret != 0)
		return -EFAULT;

	if (cmd[0] == '4') {
		MYDBG("");
		udp_reply(g_pid, 0, "HELLO, SS7_IC_USB!!!");
	}

	MYDBG("");

	return count;
}

const struct file_operations musbfsh_ic_tmp_proc_fops = {
	.write = musbfsh_ic_tmp_proc_entry
};

void create_ic_tmp_entry(void)
{
	struct proc_dir_entry *pr_entry;

	if (proc_drv_icusb_dir_entry == NULL) {
		MYDBG("[%s]: /proc/driver/icusb not exist\n", __func__);
		return;
	}

	pr_entry =
		proc_create("IC_TMP_ENTRY", 0660, proc_drv_icusb_dir_entry,
			&musbfsh_ic_tmp_proc_fops);
	if (pr_entry)
		MYDBG("add /proc/IC_TMP_ENTRY ok\n");
	else
		MYDBG("add /proc/IC_TMP_ENTRY fail\n");
}

static ssize_t musbfsh_ic_usb_cmd_proc_status_read(struct file *file_ptr,
						   char __user *user_buffer,
						   size_t count,
						   loff_t *position)
{
	int len;

	MYDBG("");

	if (copy_to_user(user_buffer,
			 &g_ic_usb_status, sizeof(g_ic_usb_status)) != 0)
		return -EFAULT;

	/* *position += count; */
	len = sizeof(g_ic_usb_status);
	return len;
}


ssize_t musbfsh_ic_usb_cmd_proc_entry(struct file *file_ptr,
				      const char __user *user_buffer,
				      size_t count, loff_t *position)
{


	int ret = copy_from_user((char *)&ic_cmd, user_buffer, count);

	if (ret != 0)
		return -EFAULT;

	MYDBG("type : %x, length : %x, data[0] : %x\n",
	      ic_cmd.type, ic_cmd.length, ic_cmd.data[0]);

	switch (ic_cmd.type) {
	case USB11_SESSION_CONTROL:
		MYDBG("");
		usb11_session_control(ic_cmd.data[0]);
		break;
	case USB11_INIT_PHY_BY_VOLTAGE:
		MYDBG("");
		usb11_init_phy_by_voltage(ic_cmd.data[0]);
		break;
	case USB11_WAIT_DISCONNECT_DONE:
		MYDBG("");
		usb11_wait_disconnect_done(ic_cmd.data[0]);
		break;
		/*--- special purpose ---*/
	case 's':
		MYDBG("create sysfs\n");
		create_icusb_sysfs_attr();
		break;
	case 't':
		MYDBG("create tmp proc\n");
		create_ic_tmp_entry();
		break;
	}
	return count;
}

static const struct file_operations musbfsh_ic_usb_cmd_proc_fops = {
	.read = musbfsh_ic_usb_cmd_proc_status_read,
	.write = musbfsh_ic_usb_cmd_proc_entry
};

void create_ic_usb_cmd_proc_entry(void)
{
	struct proc_dir_entry *prEntry;

	MYDBG("");
	proc_drv_icusb_dir_entry = proc_mkdir("driver/icusb", NULL);

	if (proc_drv_icusb_dir_entry == NULL) {
		MYDBG("[%s]: mkdir /proc/driver/icusb failed\n", __func__);
		return;
	}

	prEntry =
	    proc_create("IC_USB_CMD_ENTRY", 0660, proc_drv_icusb_dir_entry,
			&musbfsh_ic_usb_cmd_proc_fops);
	if (prEntry) {
		MYDBG("add IC_USB_CMD_ENTRY ok\n");
		netlink_sock = netlink_kernel_create(&init_net,
						     NETLINK_USERSOCK, &nl_cfg);
	} else {
		MYDBG("add IC_USB_CMD_ENTRY fail\n");
	}
}

void set_icusb_phy_power_negotiation(struct usb_device *udev)
{
	if (power_resume_time_neogo_attr.value) {
		icusb_power_negotiation(udev);
		icusb_resume_time_negotiation(udev);
	} else {
		set_icusb_phy_power_negotiation_ok();
	}
}
