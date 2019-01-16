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

extern struct my_attr power_resume_time_neogo_attr;
extern struct my_attr skip_session_req_attr;
extern struct my_attr skip_enable_session_attr;
extern struct my_attr skip_mac_init_attr;
extern struct my_attr resistor_control_attr;
extern struct my_attr hw_dbg_attr;
extern struct my_attr skip_port_pm_attr;

static struct my_attr my_attr_test = {
	.attr.name = "my_attr_test",
	.attr.mode = 0644,
	.value = 1
};


static struct attribute *myattr[] = {
	(struct attribute*)&my_attr_test,
	(struct attribute*)&power_resume_time_neogo_attr,
	(struct attribute*)&skip_session_req_attr,
	(struct attribute*)&skip_enable_session_attr,
	(struct attribute*)&skip_mac_init_attr,
	(struct attribute*)&resistor_control_attr,
	(struct attribute*)&hw_dbg_attr,
	(struct attribute*)&skip_port_pm_attr,
	NULL
};



static struct IC_USB_CMD ic_cmd;
unsigned int g_ic_usb_status= ((USB_PORT1_DISCONNECT_DONE) << USB_PORT1_STS_SHIFT);
static struct sock *netlink_sock;
static u_int g_pid;
static struct proc_dir_entry *proc_drv_icusb_dir_entry = NULL;

extern void musbfsh_start_session(void);
extern void musbfsh_start_session_pure(void );
extern void musbfsh_stop_session(void);
extern void musbfsh_init_phy_by_voltage(enum PHY_VOLTAGE_TYPE);
extern enum PHY_VOLTAGE_TYPE get_usb11_phy_voltage(void);
extern void mt65xx_usb11_mac_reset_and_phy_stress_set(void);
extern int is_usb11_enabled(void);


static void icusb_dump_data(char *buf, int len);
static void set_icusb_phy_power_negotiation_fail(void);
static void set_icusb_phy_power_negotiation_ok(void);
static void set_icusb_data_of_interface_power_request(short data);

static void icusb_resume_time_negotiation(struct usb_device *dev)
{
	int ret;
	int retries = IC_USB_RETRIES_RESUME_TIME_NEGOTIATION;
	char resume_time_negotiation_data[IC_USB_LEN_RESUME_TIME_NEGOTIATION];
	while(retries-- > 0)
	{
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
		}
		else
		{
			MYDBG("");
			icusb_dump_data(resume_time_negotiation_data, IC_USB_LEN_RESUME_TIME_NEGOTIATION);
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
	int power_negotiation_done = 0 ;
	enum PHY_VOLTAGE_TYPE phy_volt; 

	while(retries-- > 0)
	{
		MYDBG("");
		power_negotiation_done = 0 ;
		ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				IC_USB_REQ_GET_INTERFACE_POWER,
				IC_USB_REQ_TYPE_GET_INTERFACE_POWER,
				IC_USB_WVALUE_POWER_NEGOTIATION,
				IC_USB_WINDEX_POWER_NEGOTIATION,
				get_power_negotiation_data,
				IC_USB_LEN_POWER_NEGOTIATION,
				USB_CTRL_GET_TIMEOUT);
		if (ret < 0) {
			MYDBG("ret : %d\n", ret);
			continue;
		}
		else
		{
			MYDBG("");
			icusb_dump_data(get_power_negotiation_data, IC_USB_LEN_POWER_NEGOTIATION);

			/* copy the prefer bit from get interface power */
			set_power_negotiation_data[0] = (get_power_negotiation_data[0] & IC_USB_PREFER_CLASSB_ENABLE_BIT);

			/* set our current voltage */
			phy_volt = get_usb11_phy_voltage();
			if(phy_volt == VOL_33)
			{
				set_power_negotiation_data[0] |= (char)IC_USB_CLASSB;
			}
			else if(phy_volt == VOL_18)
			{
				set_power_negotiation_data[0] |= (char)IC_USB_CLASSC;
			}
			else
			{
				MYDBG("");
			}

			/* set current */
			if(set_power_negotiation_data[1] > IC_USB_CURRENT)
			{
				MYDBG("");
				set_power_negotiation_data[1] = IC_USB_CURRENT;
			}else{
				MYDBG("");
				set_power_negotiation_data[1] = get_power_negotiation_data[1];
			}
			MYDBG("power_negotiation_data[0] : 0x%x , power_negotiation_data[1] : 0x%x, IC_USB_CURRENT :%d",set_power_negotiation_data[0], set_power_negotiation_data[1], IC_USB_CURRENT);

			ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
					IC_USB_REQ_SET_INTERFACE_POWER,
					IC_USB_REQ_TYPE_SET_INTERFACE_POWER,
					IC_USB_WVALUE_POWER_NEGOTIATION,
					IC_USB_WINDEX_POWER_NEGOTIATION,
					set_power_negotiation_data,
					IC_USB_LEN_POWER_NEGOTIATION,
					USB_CTRL_SET_TIMEOUT);

			if (ret < 0) {
				MYDBG("ret : %d\n", ret);
			}
			else
			{
				MYDBG("");
				power_negotiation_done = 1 ;
				break;
			}
		//	break;
		}
	}

	MYDBG("retries : %d\n", retries);
	if(!power_negotiation_done){
		set_icusb_phy_power_negotiation_fail();
	}else{
		set_icusb_data_of_interface_power_request(*((short *)get_power_negotiation_data));
		set_icusb_phy_power_negotiation_ok();
	}
}

void usb11_wait_disconnect_done(int value)
{
	if(is_usb11_enabled())
	{
		while(1)
		{
			unsigned int ic_usb_status = g_ic_usb_status;
			MYDBG("ic_usb_status : %x\n", ic_usb_status);
			ic_usb_status &= (USB_PORT1_STS_MSK << USB_PORT1_STS_SHIFT);
			MYDBG("ic_usb_status : %x\n", ic_usb_status);

			if(ic_usb_status == (USB_PORT1_DISCONNECT_DONE << USB_PORT1_STS_SHIFT))
			{
				MYDBG("USB_PORT1_DISCONNECT_DONE\n");
				break;
			}

			if(ic_usb_status == (USB_PORT1_DISCONNECTING << USB_PORT1_STS_SHIFT))
			{
				MYDBG("USB_PORT1_DISCONNECTING\n");
			}

			msleep(10);
		}
	}
	else
	{
		MYDBG("usb11 is not enabled, skip usb11_wait_disconnect_done()\n");
	}

}

int check_usb11_sts_disconnect_done(void)
{
	unsigned int ic_usb_status = g_ic_usb_status;
	MYDBG("ic_usb_status : %x\n", ic_usb_status);
	ic_usb_status &= (USB_PORT1_STS_MSK << USB_PORT1_STS_SHIFT);
	MYDBG("ic_usb_status : %x\n", ic_usb_status);

	if(ic_usb_status == (USB_PORT1_DISCONNECT_DONE << USB_PORT1_STS_SHIFT))
	{
		MYDBG("USB_PORT1_DISCONNECT_DONE got\n");
		return 1;
	}
	else
	{
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
	for(i =0 ; i< len ; i++)
	{
		MYDBG("data[%d]: %x\n", i, buf[i]);
	}

}

int usb11_init_phy_by_voltage(enum PHY_VOLTAGE_TYPE phy_volt)
{
	musbfsh_init_phy_by_voltage(phy_volt);
	return 0;
}

int usb11_session_control(enum SESSION_CONTROL_ACTION action)
{

	if(action == START_SESSION)
		musbfsh_start_session();
	else if(action == STOP_SESSION) 
	{
		//musbfsh_stop_session();
		if(!is_usb11_enabled())		
		{
			mt65xx_usb11_mac_reset_and_phy_stress_set();
		}
		else
		{
			MYDBG("usb11 has been enabled, skip mt65xx_usb11_mac_reset_and_phy_stress_set()\n");
		}
	}
	else
		MYDBG("unknown action\n");


	return 0;
}
static void udp_reply(int pid,int seq,void *payload)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int size=strlen(payload)+1;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;
	//3.10 specific
	nlh = __nlmsg_put(skb, pid, seq, 0, size, 0);
	nlh->nlmsg_flags = 0;
	data=NLMSG_DATA(nlh);
	memcpy(data, payload, size);

	//3.10 specific
	NETLINK_CB(skb).portid = 0; /* from kernel */
	NETLINK_CB(skb).dst_group = 0; /* unicast */
	ret=netlink_unicast(netlink_sock, skb, pid, MSG_DONTWAIT);
	if (ret <0)
	{
		MYDBG("send failed\n");
	}
	return;

#if 0
nlmsg_failure: /* Used by NLMSG_PUT */
	if (skb)
		kfree_skb(skb);
#endif
}

/* Receive messages from netlink socket. */
static void udp_receive(struct sk_buff *skb)
{
	u_int uid, seq;
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
	MYDBG("recv skb from user space uid:%d pid:%d seq:%d\n",uid,g_pid,seq);
	MYDBG("data is :%s\n",(char *)data);


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
	sscanf(buf, "%d", &a->value);
	return sizeof(int);
}

static struct sysfs_ops myops = {
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
static ssize_t musbfsh_ic_tmp_proc_entry(struct file *file_ptr, const char __user *user_buffer, size_t count, loff_t *position)
{
	char cmd[64];
	int ret = copy_from_user((char *) &cmd, user_buffer, count);

	if(ret != 0)
	{
		return -EFAULT;
	}


	if(cmd[0] == '4')
	{
		MYDBG("");
		udp_reply(g_pid, 0, "HELLO, SS7_IC_USB!!!");
	}

	MYDBG("");

	return count;
}

struct file_operations musbfsh_ic_tmp_proc_fops = {
	.write = musbfsh_ic_tmp_proc_entry
};

void create_ic_tmp_entry(void)
{
	struct proc_dir_entry *prEntry;

	if (NULL == proc_drv_icusb_dir_entry)
	{
		MYDBG("[%s]: /proc/driver/icusb not exist\n", __func__);
		return;
	}

	prEntry = proc_create("IC_TMP_ENTRY", 0660, proc_drv_icusb_dir_entry, &musbfsh_ic_tmp_proc_fops);
	if (prEntry)
	{
		MYDBG("add /proc/IC_TMP_ENTRY ok\n");
	}
	else
	{
		MYDBG("add /proc/IC_TMP_ENTRY fail\n");
	}
}

static ssize_t musbfsh_ic_usb_cmd_proc_status_read(struct file *file_ptr, char __user *user_buffer, size_t count, loff_t *position)
{
	int len;
	MYDBG("");

	if( copy_to_user(user_buffer, &g_ic_usb_status, sizeof(g_ic_usb_status)) != 0 )
	{
		return -EFAULT;
	}

//	*position += count;
	len = sizeof(g_ic_usb_status);
	return len;
}


ssize_t musbfsh_ic_usb_cmd_proc_entry(struct file *file_ptr, const char __user *user_buffer, size_t count, loff_t *position)
{
	

	int ret = copy_from_user((char *) &ic_cmd, user_buffer, count);
	
	if(ret != 0)
	{
		return -EFAULT;
	}
	MYDBG("type : %x, length : %x, data[0] : %x\n", ic_cmd.type, ic_cmd.length, ic_cmd.data[0]);
	
	switch(ic_cmd.type)
	{
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

static struct file_operations musbfsh_ic_usb_cmd_proc_fops = {
	.read = musbfsh_ic_usb_cmd_proc_status_read,
	.write = musbfsh_ic_usb_cmd_proc_entry
};

void create_ic_usb_cmd_proc_entry(void)
{
	struct proc_dir_entry *prEntry;

	MYDBG("");
	proc_drv_icusb_dir_entry = proc_mkdir("driver/icusb", NULL);
	
	if (NULL == proc_drv_icusb_dir_entry)
	{
		MYDBG("[%s]: mkdir /proc/driver/icusb failed\n", __func__);
		return;
	}

	prEntry = proc_create("IC_USB_CMD_ENTRY", 0660, proc_drv_icusb_dir_entry, &musbfsh_ic_usb_cmd_proc_fops);
	if (prEntry)
	{
		MYDBG("add IC_USB_CMD_ENTRY ok\n");
		netlink_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &nl_cfg);		
	}
	else
	{
		MYDBG("add IC_USB_CMD_ENTRY fail\n");
	}
}

void set_icusb_phy_power_negotiation(struct usb_device *udev)
{
	if(power_resume_time_neogo_attr.value)
	{
		icusb_power_negotiation(udev);
		icusb_resume_time_negotiation(udev);
	}
	else
	{
		set_icusb_phy_power_negotiation_ok() ;
	}
}
#if 0
static int usb_icusb_probe(struct usb_interface *iface,
			 const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(iface);
	struct usb_host_interface *interface;
	struct usb_icusb *icusb;

	interface = iface->altsetting;
	printk(" extralen = %d\n",interface->extralen);

	if (interface->extralen < ICCD_CLASS_DESCRIPTOR_LENGTH)
		return -ENODEV;

	icusb = kzalloc(sizeof(struct usb_icusb), GFP_KERNEL);

	if (dev->manufacturer)
		strlcpy(icusb->name, dev->manufacturer, sizeof(icusb->name));

	if (dev->product) {
		if (dev->manufacturer)
			strlcat(icusb->name, " ", sizeof(icusb->name));
		strlcat(icusb->name, dev->product, sizeof(icusb->name));
	}

	if (!strlen(icusb->name))
		snprintf(icusb->name, sizeof(icusb->name),
			 "USB ICUSB =  %04x:%04x",
			 le16_to_cpu(dev->descriptor.idVendor),
			 le16_to_cpu(dev->descriptor.idProduct));
	printk("icusb_DRIVER = %s\n",icusb->name);

	if(power_resume_time_neogo_attr.value)
	{
		icusb_power_negotiation(dev);
		icusb_resume_time_negotiation(dev);
	}
	else
	{
		set_icusb_phy_power_negotiation_ok() ;
	}
	
//	usb_set_intfdata(iface, icusb);
	
	return -ENODEV;
}

static void usb_icusb_disconnect(struct usb_interface *intf)
{
	struct usb_icusb *icusb = usb_get_intfdata (intf);
	printk("usb_icusb_disconnect\n");

	if(!check_usb11_sts_disconnect_done()){
		set_usb11_sts_disconnecting();
	}
	mt65xx_usb11_mac_reset_and_phy_stress_set();	
//	usb_set_intfdata(intf, NULL);
	
	if (icusb) {
		kfree(icusb);
	}
	set_icusb_sts_disconnect_done();	
}

static int  usb_icusb_suspend(struct usb_interface *intf, pm_message_t message)
{
	printk("usb_icusb_suspend\n");
	return 0;
}
static int usb_icusb_resume(struct usb_interface *intf)
{
	printk("usb_icusb_resume\n");
	return 0;
}
static int usb_icusb_pre_reset(struct usb_interface *intf)
{
	printk("usb_icusb_pre_reset\n");
	return 0;

}
static int usb_icusb_post_reset(struct usb_interface *intf)
{
	printk("usb_icusb_post_reset\n");
	return 0;
}

static int usb_icusb_reset_resume(struct usb_interface *intf)
{
	printk("usb_icusb_reset_resume\n");
	return 0;
}



static struct usb_device_id usb_icusb_id_table [] = {
    { .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS,
      .bInterfaceClass = ICCD_INTERFACE_CLASS},
	{ }						/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, usb_icusb_id_table);

static struct usb_driver usb_icusb_driver = {
	.name =		"usbicusb",
	.probe =	usb_icusb_probe,
	.disconnect =	usb_icusb_disconnect,
	.suspend = usb_icusb_suspend,
	.resume = usb_icusb_resume,
	.pre_reset = usb_icusb_pre_reset,
	.post_reset = usb_icusb_post_reset,
	.reset_resume = usb_icusb_reset_resume,
	.id_table =	usb_icusb_id_table,
};


static int __init icusb_init(void)
{
	int rc;
		printk("icusb_init\n");
	if ((rc = usb_register(&usb_icusb_driver)) != 0)
		goto err_register;
			printk("icusb_register done\n");
//	create_icusb_cmd_proc_entry();
	//3.10 specific
	netlink_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &nl_cfg);

	
err_register:
	return rc;
}

static void __exit icusb_exit(void)
{
	usb_deregister(&usb_icusb_driver);
}


module_init(icusb_init);
module_exit(icusb_exit);
#endif

