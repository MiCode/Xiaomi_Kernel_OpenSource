#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include <asm/setup.h>
#include "mtk_charger.h"
#include "mtk_battery.h"
#include "tcpci_typec.h"
/* N17 code for HQ-292525 by tongjiacheng at 20230506 start */
#include "adapter_class.h"
/* N17 code for HQ-292525 by tongjiacheng at 20230506 end */

static int log_level = 2;

#define sysfs_err(fmt, ...)     \
do {                                    \
    if (log_level > 0)                  \
        printk(KERN_ERR "[hq_charger_sysfs]" fmt, ##__VA_ARGS__);   \
}while (0)

#define sysfs_info(fmt, ...)    \
do {                                    \
    if (log_level > 1)          \
        printk(KERN_ERR "[hq_charger_sysfs]" fmt, ##__VA_ARGS__);    \
}while (0)

#define sysfs_dbg(fmt, ...)    \
do {                                        \
    if (log_level >=2 )          \
        printk(KERN_ERR "[hq_charger_sysfs]" fmt, ##__VA_ARGS__);    \
}while (0)

#define SHUTDOWN_DELAY_VOL 3400

struct sysfs_desc {
	struct mtk_charger *info;
	struct mtk_battery *gm;
	struct charger_device *chg_dev;

	struct tcpc_device *tcpc;

	struct power_supply *chg_psy;
	struct power_supply *bat_psy;
	struct power_supply *usb_psy;

	struct delayed_work shutdown_dwork;

	struct notifier_block tcpc_nb;
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 start*/
	struct task_struct *soc_decimal_task;
	bool wakeup_thread;
	wait_queue_head_t wq;
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 end*/
/*N17 code for HQ-299258 by tongjiacheng at 2023/7/5 start*/
	bool shutdown_delay;
/*N17 code for HQ-299258 by tongjiacheng at 2023/7/5 end*/

/* N17 code for HQ-308084 by liunianliang at 2023/7/31 start */
	struct notifier_block psy_nb;
/* N17 code for HQ-308084 by liunianliang at 2023/7/31 end */
};

static struct sysfs_desc *g_desc;

static const char *const real_type_name[] = {
	"Unknown", "USB", "USB_CDP", "USB_FLOAT",
	"USB_DCP", "USB_HVDCP", "USB_PD",
};

static const char *const typec_mode_name[] = {
/*N17 code for HQHW-4214 by wangtingting at 2023/06/21 start*/
	"Nothing attached", "Source attached", "Sink attached",
/*N17 code for HQHW-4214 by wangtingting at 2023/06/21 end*/
	"Audio Adapter", "Debug Accessory",
};


enum chr_type {
	CHARGER_UNKNOWN = 0,
	STANDARD_HOST,		/* USB : 450mA */
	CHARGING_HOST,
	NONSTANDARD_CHARGER,	/* AC : 450mA~1A */
	STANDARD_CHARGER,	/* AC : ~1A */
	HVDCP_CHARGER,		/* AC: QC charger */
	PD_CAHRGER,		/* AC: PD charger */
	APPLE_2_1A_CHARGER,	/* 2.1A apple charger */
	APPLE_1_0A_CHARGER,	/* 1A apple charger */
	APPLE_0_5A_CHARGER,	/* 0.5A apple charger */
	WIRELESS_CHARGER,
};

static int shutdown_delay;
static void shutdown_delay_handler(struct work_struct *work)
{
	struct sysfs_desc *desc;
	struct mtk_battery *gm;

	char sd_str[32];
	char *envp[] = { sd_str, NULL };

	desc = container_of(to_delayed_work(work),
			    struct sysfs_desc, shutdown_dwork);

	gm = desc->gm;

	/* N17 code for HQ-290778 by tongjiacheng at 20230530 start */
	if (gm->bs_data.bat_capacity <= 1) {
        /* N17 code for HQ-298644 by tongjiacheng at 20230608 start */
		if (gm->bs_data.bat_batt_vol <= SHUTDOWN_DELAY_VOL &&
		gm->bs_data.bat_status != POWER_SUPPLY_STATUS_CHARGING)
			shutdown_delay = true;
		else if (gm->bs_data.bat_status == POWER_SUPPLY_STATUS_CHARGING &&
		shutdown_delay)
			shutdown_delay = false;
		else
			shutdown_delay = false;
        /* N17 code for HQ-298644 by tongjiacheng at 20230608 end */
	} else
		shutdown_delay = false;
/*N17 code for HQ-299258 by tongjiacheng at 2023/7/5 start*/
	if (desc->shutdown_delay != shutdown_delay) {
		sysfs_dbg("pre status:%d cur status: %d\n", 
				desc->shutdown_delay, shutdown_delay);

		sprintf(envp[0], "POWER_SUPPLY_SHUTDOWN_DELAY=%d",
			shutdown_delay);
		kobject_uevent_env(&desc->bat_psy->dev.kobj,
				   KOBJ_CHANGE, envp);

		desc->shutdown_delay  = shutdown_delay;
	}
/*N17 code for HQ-299258 by tongjiacheng at 2023/7/5 end */
	/* N17 code for HQ-290778 by tongjiacheng at 20230530 end */
	mod_delayed_work(system_wq,
			 &(desc->shutdown_dwork), msecs_to_jiffies(2000));
}

static ssize_t shutdown_delay_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%d\n", shutdown_delay);
}

static struct device_attribute shutdown_delay_attr =
__ATTR(shutdown_delay, 0444, shutdown_delay_show, NULL);

/*N17 code for HQ-290781 by wangtingting at 2023/5/29 start*/
static void wake_up_soc_decimal_task(struct sysfs_desc *desc)
{
	desc->wakeup_thread = true;
	wake_up_interruptible(&desc->wq);
}
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 end*/
static int quick_chr_type;
static int quick_chr_type_noti(struct notifier_block *nb,
			       unsigned long event, void *v)
{
	int res = 0;
	char chr_type_str[64];
	char *envp[] = { chr_type_str, NULL };
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 start*/
	struct sysfs_desc *desc;

	desc = container_of(nb,
					struct sysfs_desc, tcpc_nb);

	switch (desc->info->chr_type) {
	case POWER_SUPPLY_TYPE_USB:
		/* N17 code for HQ-308327 by liunianliang at 2023/7/31 start */
		res = 0;
		if (desc->info->pd_type ==
			MTK_PD_CONNECT_PE_READY_SNK_APDO){
			if (desc->info->pd_adapter->verifed)
				res = 3;
			else
				res = 2;
		}
		break;
		/* N17 code for HQ-308327 by liunianliang at 2023/7/31 end */
	case POWER_SUPPLY_TYPE_USB_CDP:
		res = 0;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		res = 0;
		if (desc->info->pd_type ==
		    MTK_PD_CONNECT_PE_READY_SNK_APDO){
			if (desc->info->pd_adapter->verifed)
				res = 3;
			else
				res = 2;
        	}
		break;
	case POWER_SUPPLY_TYPE_USB_ACA:
		res = 1;
		break;
	default:
		break;
	}

	quick_chr_type = res;

	if (desc->info->pd_type
			== MTK_PD_CONNECT_PE_READY_SNK_APDO)
			wake_up_soc_decimal_task(desc);
	else
			desc->wakeup_thread = false;

	sprintf(envp[0], "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", res);
	kobject_uevent_env(&(desc->info->chg_psy->dev.kobj),
			   KOBJ_CHANGE, envp);
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 end*/
	return 0;
}

/* N17 code for HQ-308084 by liunianliang at 2023/7/31 start */
static int psy_change_noti(struct notifier_block *nb,
			       unsigned long event, void *v)
{
	struct power_supply *psy = v;
	struct sysfs_desc *desc;
	char chr_type_str[64];
	char *envp[] = { chr_type_str, NULL };
	int ret = 0;
	int res = 0;

	desc = container_of(nb, struct sysfs_desc, psy_nb);

	if (event != PSY_EVENT_PROP_CHANGED || !desc || !psy)
		return NOTIFY_OK;

	pr_info("%s psy->desc->name(%s)\n", __func__, psy->desc->name);
	pr_info("%s desc->info->chr_type(%d)\n", __func__, desc->info->chr_type);
	pr_info("%s desc->info->pd_type(%d)\n", __func__, desc->info->pd_type);

	if (strcmp(psy->desc->name, "mt6360_chg.3.auto") == 0) {
		switch (desc->info->chr_type) {
		case POWER_SUPPLY_TYPE_USB:
			if (desc->info->pd_type == MTK_PD_CONNECT_PE_READY_SNK)
				res = 1;
			break;
		default:
			break;
		}

	}

	if (res != 0) {
		sprintf(envp[0], "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", res);
		ret = kobject_uevent_env(&(desc->info->psy1->dev.kobj),
				   KOBJ_CHANGE, envp);
		if (ret)
			sysfs_err("%s send uevent fail(%d)\n", __func__, ret);
	}

	return 0;
}
/* N17 code for HQ-308084 by liunianliang at 2023/7/31 end */

/*N17 code for HQ-290781 by wangtingtin at 2023/5/29 start*/
static int get_uisoc_decimal_rate(struct sysfs_desc *desc, int *val)
{
	static int mtk_soc_decimal_rate[24] = {0,32,10,30,20,28,30,28,40,28,50,28,60,28,70,28,80,28,90,26,95,10,99,5};
	static int *dec_rate_seq = &mtk_soc_decimal_rate[0];
	static int dec_rate_len = 24;
	int i, soc = 0;

	for (i = 0; i < dec_rate_len; i += 2) {
		if (soc < dec_rate_seq[i]) {
			*val = dec_rate_seq[i - 1];
			return soc;
		}
	}
	*val = dec_rate_seq[dec_rate_len - 1];
	return soc;
}
static void get_uisoc_decimal(struct sysfs_desc *desc, int *val)
{
	int dec_rate, soc_dec, soc, hal_soc;
	static int last_val = 0, last_soc_dec = 0, last_hal_soc = 0;

	hal_soc = desc->gm->ui_soc ;

	soc_dec = desc->gm->fg_cust_data.ui_old_soc % 100;
	soc = get_uisoc_decimal_rate(desc, &dec_rate);

	if (soc_dec >= 0 && soc_dec < (50 - dec_rate))
		*val = soc_dec + 50;
	else if (soc_dec >= (50 - dec_rate) && soc_dec < 50)
		*val = soc_dec + 50 - dec_rate;
	else
		*val = soc_dec -50;
	if (last_hal_soc == hal_soc) {
		if ((last_val > *val && hal_soc != soc) || (last_soc_dec == soc_dec && hal_soc == soc)) {
			if (last_val > 50)
				*val = last_val + (100 - last_val - dec_rate) / 2;
			else
				*val = last_val + dec_rate / 4;
		} else if (last_val > *val) {
			*val = last_val;
		}
	}
	if (last_val != *val)
		last_val = *val;
	if (last_soc_dec != soc_dec)
		last_soc_dec = soc_dec;
	if (last_hal_soc != hal_soc)
		last_hal_soc = hal_soc;
}
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 end*/

/*N17 code for HQ-290781 by wangtingting at 2023/5/29 start*/
static int soc_decimal_threadfn(void *param)
{
	int ret;
	int soc_decimal;
	int soc_decimal_rate;
	char soc_decimal_str[64];
	char soc_decimal_rate_str[64];
	char *envp[] = {soc_decimal_str,
					soc_decimal_rate_str, NULL};
	struct sysfs_desc *desc = (struct sysfs_desc *)param;

	while(!kthread_should_stop()) {
			wait_event_interruptible(desc->wq, desc->wakeup_thread);

			get_uisoc_decimal(desc, &soc_decimal);
			get_uisoc_decimal_rate(desc, &soc_decimal_rate);

			sprintf( envp[0],"POWER_SUPPLY_SOC_DECIMAL=%d", soc_decimal);
			sprintf( envp[1],"POWER_SUPPLY_SOC_DECIMAL_RATE=%d", soc_decimal_rate);

			ret = kobject_uevent_env(&desc->bat_psy->dev.kobj,
					KOBJ_CHANGE, envp);
			if (ret < 0){
					sysfs_err("send uevent fail");

					return -1;
			}

			msleep(100);
	}
	return 0;
}
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 end*/

static ssize_t quick_charge_type_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	/* N17 code for HQ-291012 by tongjiacheng at 20230601 start */
	if (g_desc->info->chr_type == POWER_SUPPLY_TYPE_USB_ACA)
		quick_chr_type = 1;
	/* N17 code for HQ-291012 by tongjiacheng at 20230601 end */

	return sprintf(buf, "%d\n", quick_chr_type);
}

static struct device_attribute quick_charge_type_attr =
__ATTR(quick_charge_type, 0444, quick_charge_type_show, NULL);

static ssize_t real_type_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int chr_type = CHARGER_UNKNOWN;

/*N17 code for HQHW-4214 by miaozhichao at 2023/06/21 start*/
	switch (g_desc->info->chr_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		chr_type = CHARGER_UNKNOWN;
		break;
/*N17 code for HQHW-4213 by miaozhichao at 2023/06/26 start*/
	case POWER_SUPPLY_TYPE_USB:
		if (g_desc->info->usb_type == POWER_SUPPLY_USB_TYPE_DCP && g_desc->info->pd_type == MTK_PD_CONNECT_PE_READY_SNK){
			chr_type = PD_CAHRGER;
		} else if (g_desc->info->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
			chr_type = NONSTANDARD_CHARGER;
		} else if (g_desc->info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || g_desc->info->pd_type == MTK_PD_CONNECT_PE_READY_SNK
			|| g_desc->info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {  /* N17 code for HQ-308327 by liunianliang at 2023/7/31, add APDO */
			chr_type = PD_CAHRGER;
		} else
			chr_type = STANDARD_HOST;
		break;
/*N17 code for HQHW-4213 by miaozhichao at 2023/06/26 end*/
/*N17 code for HQHW-4214 by miaozhichao at 2023/06/21 end*/
	case POWER_SUPPLY_TYPE_USB_CDP:
		chr_type = CHARGING_HOST;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		chr_type = STANDARD_CHARGER;
		if (g_desc->info->pd_type ==
		    MTK_PD_CONNECT_PE_READY_SNK_APDO
		    || g_desc->info->pd_type ==
		    MTK_PD_CONNECT_PE_READY_SNK_PD30
		    || g_desc->info->pd_type ==
		    MTK_PD_CONNECT_PE_READY_SNK)
			chr_type = PD_CAHRGER;
		break;
	case POWER_SUPPLY_TYPE_USB_ACA:
		chr_type = HVDCP_CHARGER;
		break;
	default:
		chr_type = NONSTANDARD_CHARGER;
	}

	return sprintf(buf, "%s\n", real_type_name[chr_type]);
}

static struct device_attribute real_type_attr =
__ATTR(real_type, 0444, real_type_show, NULL);

#if IS_ENABLED(CONFIG_TCPC_CLASS)
static int typec_cc_orientation_handler(struct tcpc_device *tcpc)
{
	int typec_cc_orientation = 0;

	tcpci_get_cc(tcpc);

	if (typec_get_cc1() == 0 && typec_get_cc2() == 0)
		typec_cc_orientation = 0;
	else if (typec_get_cc2() == 0)
		typec_cc_orientation = 1;
	else if (typec_get_cc1() == 0)
		typec_cc_orientation = 2;

	return typec_cc_orientation;
}

static ssize_t typec_cc_orientation_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct sysfs_desc *desc;
	struct power_supply *psy;

	psy = dev_get_drvdata(dev);
	desc = power_supply_get_drvdata(psy);

	return sprintf(buf, "%d\n",
		       typec_cc_orientation_handler(desc->tcpc));
}

static struct device_attribute typec_cc_orientation_attr =
__ATTR(typec_cc_orientation, 0444, typec_cc_orientation_show, NULL);

static ssize_t typec_mode_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sysfs_desc *desc;
	struct power_supply *psy;
	struct tcpc_device *tcpc;

	psy = dev_get_drvdata(dev);
	desc = power_supply_get_drvdata(psy);
	tcpc = desc->tcpc;
/* N17 code for HQ-308497 by miaozhichao at 20230722 start */
	if (tcpc->typec_attach_new > ARRAY_SIZE(typec_mode_name))
		return sprintf(buf, "%s\n", "Unknown");
/* N17 code for HQ-308497 by miaozhichao at 20230722 end */
	return sprintf(buf, "%s\n",
		       typec_mode_name[tcpc->typec_attach_new]);
}

static struct device_attribute typec_mode_attr =
__ATTR(typec_mode, 0444, typec_mode_show, NULL);
#endif				/* CONFIG_TCPC_CLASS */

/* N17 code for HQ-293951 by tongjiacheng at 20230504 start */
static ssize_t usb_otg_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int res = 0;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc;

	tcpc = g_desc->tcpc;

	if (tcpc->typec_attach_new == TYPEC_ATTACHED_SRC)
		res = 1;
#endif

	return sprintf(buf, "%d\n", res);
}

static struct device_attribute usb_otg_attr =
__ATTR(usb_otg, 0444, usb_otg_show, NULL);
/* N17 code for HQ-293951 by tongjiacheng at 20230504 end */
/* N17 code for HQ-294995 by miaozhichao at 20230525 start */
static ssize_t shipmode_count_reset_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_desc->info->ship_mode);
}

static ssize_t shipmode_count_reset_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	if (kstrtobool(buf, &g_desc->info->ship_mode)) {
		sysfs_err("parsing number fail\n");
		return -EINVAL;
	}
	return count;
}

static struct device_attribute shipmode_count_reset_attr =
__ATTR(shipmode_count_reset, 0644, shipmode_count_reset_show, shipmode_count_reset_store);

/* N17 code for HQ-294995 by miaozhichao at 20230525 end */

/* N17 code for HQ-292525 by tongjiacheng at 20230506 start */
static ssize_t input_suspend_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	bool enable = false;

	enable = g_desc->info->disable_charger;

	return sprintf(buf, "%d\n", enable);
}

static ssize_t input_suspend_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct charger_device *chg_dev;
	struct battery_data *bs_data;
	bool enable = false;
	union power_supply_propval val;
	int ret;

	chg_dev = g_desc->info->chg1_dev;
	bs_data = &g_desc->gm->bs_data;

	if (kstrtobool(buf, &enable)) {
		sysfs_err("parsing number fail\n");
		return -EINVAL;
	}

	val.intval = enable;
	/* set disable charger value(1: disabel charger thread, 0: enable charger thread) */
	g_desc->info->disable_charger = enable;

	/* set battery icon */
	ret = power_supply_set_property(g_desc->info->chg_psy,
					POWER_SUPPLY_PROP_STATUS, &val);
	if (ret) {
		sysfs_err("set charger enable fail(%d)\n", ret);
		return ret;
	}

	/* set power path & wake up charger */
	ret = power_supply_set_property(g_desc->info->psy1,
					POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
					&val);
	if (ret) {
		sysfs_err("set power path fail(%d)\n", ret);
		return ret;
	}

	/* send hard reset & wake up PE5 thread */
/* N17 code for HQ-292280 by tongjiacheng at 20230610 start */
	if (!enable && 
		g_desc->info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		ret =
		    srcu_notifier_call_chain(&
					     (g_desc->info->pd_adapter->
					      evt_nh),
					     MTK_PD_CONNECT_HARD_RESET,
					     NULL);
	msleep(1);
	g_desc->info->pd_type = 
		MTK_PD_CONNECT_PE_READY_SNK_APDO; //重新设置pd type为PPS状态
	}
/* N17 code for HQ-292280 by tongjiacheng at 20230610 end */
	/* notifier to mtk battery */
	power_supply_changed(g_desc->info->psy1);

	return count;
}

static struct device_attribute input_suspend_attr =
__ATTR(input_suspend, 0644, input_suspend_show, input_suspend_store);
/* N17 code for HQ-292525 by tongjiacheng at 20230506 end */
/* N17 code for HQ-295595 by tongjiacheng at 20230509 start*/
static ssize_t chip_ok_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret = 0;
	bool chip_ok_status = false;
	union power_supply_propval val;
	struct power_supply *psy = 
			power_supply_get_by_name("batt_verify");

	if (psy) {
		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_AUTHENTIC, &val);
		if (ret < 0)
			chip_ok_status = false;

		if (val.intval)
			chip_ok_status = true;
		else
			chip_ok_status = false;
	}
	else
		chip_ok_status = false;

	return sprintf(buf, "%d\n", chip_ok_status);
}

static struct device_attribute chip_ok_attr =
__ATTR(chip_ok, 0444, chip_ok_show, NULL);
/* N17 code for HQ-295595 by tongjiacheng at 20230509 end*/

/* N17 code for HQ-292265 by tongjiacheng at 20230515 start */
static ssize_t batt_id_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u8 id = 0;
	struct power_supply *psy;
	union power_supply_propval val;

	psy = power_supply_get_by_name("batt_verify");
	if (psy) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TYPE,
					&val);
		if (ret)
			id = 0xff;

		id = val.intval;
	}
	else
		id = 0xff;

	return sprintf(buf, "%d\n", id);
}

static struct device_attribute batt_id_attr =
__ATTR(batt_id, 0444, batt_id_show, NULL);
/* N17 code for HQ-292265 by tongjiacheng at 20230515 end */
/* N17 code for HQ-290782 by wangtingting at 2023/05/18 start*/
static ssize_t apdo_max_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_desc->info->pd_adapter->apdo_max/1000000);
}
static struct device_attribute apdo_max_attr =
__ATTR(apdo_max, 0444, apdo_max_show, NULL);
/* N17 code for HQ-290782 by wangtingting at 2023/05/18 end*/

/* N17 code for HQ-308327 by liunianliang at 2023/7/31 start */
static ssize_t pd_verifed_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_desc->info->pd_adapter->verifed);
}
static struct device_attribute pd_verifed_attr =
__ATTR(pd_verifed, 0444, pd_verifed_show, NULL);
/* N17 code for HQ-308327 by liunianliang at 2023/7/31 end */

static struct attribute *usb_psy_attrs[] = {
	&quick_charge_type_attr.attr,
	&real_type_attr.attr,
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	&typec_cc_orientation_attr.attr,
	&typec_mode_attr.attr,
#endif				/* CONFIG_TCPC_CLASS */
/* N17 code for HQ-293951 by tongjiacheng at 20230504 start */
	&usb_otg_attr.attr,
/* N17 code for HQ-293951 by tongjiacheng at 20230504 end */
/* N17 code for HQ-290782 by wangtingting at 2023/05/18 start */
	&apdo_max_attr.attr,
/* N17 code for HQ-290782 by wangtingting at 2023/05/18 end */
/* N17 code for HQ-308327 by liunianliang at 2023/7/31 start */
	&pd_verifed_attr.attr,
/* N17 code for HQ-308327 by liunianliang at 2023/7/31 end */
	NULL,
};

static struct attribute *bat_psy_attrs[] = {
	&shutdown_delay_attr.attr,
/* N17 code for HQ-292525 by tongjiacheng at 20230506 start */
	&input_suspend_attr.attr,
/* N17 code for HQ-292525 by tongjiacheng at 20230506 end */
/* N17 code for HQ-295595 by tongjiacheng at 20230509 start*/
	&chip_ok_attr.attr,
/* N17 code for HQ-295595 by tongjiacheng at 20230509 end*/
/* N17 code for HQ-292265 by tongjiacheng at 20230515 start */
	&batt_id_attr.attr,
/* N17 code for HQ-292265 by tongjiacheng at 20230515 end */
/* N17 code for HQ-294995 by miaozhichao at 20230525 start */
	&shipmode_count_reset_attr.attr,
/* N17 code for HQ-294995 by miaozhichao at 20230525 end */
	NULL,
};

static const struct attribute_group usb_psy_group = {
	.attrs = usb_psy_attrs,
};

static const struct attribute_group bat_psy_group = {
	.attrs = bat_psy_attrs,
};

static int sysfs_setup_files(struct sysfs_desc *desc)
{
	int ret;

	if (!desc->usb_psy || !desc->bat_psy || !desc->info) {
		sysfs_err("%s find psy fail\n", __func__);
		ret = -EINVAL;
		goto _out;
	}

	ret = sysfs_create_group(&(desc->bat_psy->dev.kobj),
				 &bat_psy_group);
	if (ret) {
		sysfs_err("%s create battery node fail(%d)\n",
			  __func__, ret);
		goto _out;
	}

	ret = sysfs_create_group(&(desc->usb_psy->dev.kobj),
				 &usb_psy_group);
	if (ret) {
		sysfs_err("%s create usb node fail(%d)\n", __func__, ret);
		goto _out;
	}

	return 0;

      _out:
	return ret;
}

static enum power_supply_property usb_psy_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
};

static int psy_usb_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		/* N17 code for HQHW-4906 by p-gucheng at 20230812 */
		val->intval = charger_dev_get_online (g_desc->info->chg1_dev);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = psy->desc->type;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.properties = usb_psy_properties,
	.num_properties = ARRAY_SIZE(usb_psy_properties),
	.type = POWER_SUPPLY_TYPE_USB,
	.get_property = psy_usb_get_property,
};

static int init_psy_tcpc(struct sysfs_desc *desc)
{
	struct power_supply_config cfg = {
		.drv_data = desc,
	};

	desc->usb_psy = power_supply_register(&(desc->info->pdev->dev),
					      &usb_psy_desc, &cfg);
	if (IS_ERR(desc->usb_psy)) {
		sysfs_err("%s register usb psy fail(%d)\n", __func__,
			  PTR_ERR(desc->usb_psy));
		return -PTR_ERR(desc->usb_psy);
	}

	desc->bat_psy = power_supply_get_by_name("battery");
	if (!desc->bat_psy) {
		sysfs_err("%s get battery psy fail\n", __func__);
		return -PTR_ERR(desc->bat_psy);
	}

	desc->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!desc->tcpc) {
		sysfs_err("%s get typec device fail\n", __func__);
		return -PTR_ERR(desc->tcpc);
	}

	desc->gm = power_supply_get_drvdata(desc->bat_psy);
	if (!desc->gm) {
		sysfs_err("%s get battery info fail\n", __func__);
		return -PTR_ERR(desc->gm);
	}

	return 0;
}

static int __init hq_charger_sysfs_init(void)
{
	int ret;
	struct sysfs_desc *desc;
	struct power_supply *main_psy;

	sysfs_info("%s\n", __func__);

	desc = kzalloc(sizeof(struct sysfs_desc), GFP_KERNEL);
	if (!desc) {
		sysfs_err("%s alloc desc mem fail\n", __func__);
		ret = -ENOMEM;
		goto alloc_err;
	}

	main_psy = power_supply_get_by_name("mtk-master-charger");
	if (!main_psy) {
		sysfs_err("%s get main charger psy fail\n", __func__);
		ret = -EINVAL;
		goto main_psy_err;
	}

	desc->info = power_supply_get_drvdata(main_psy);

	ret = init_psy_tcpc(desc);
	if (ret)
		goto psy_tcpc_err;

	INIT_DELAYED_WORK(&(desc->shutdown_dwork), shutdown_delay_handler);
	schedule_delayed_work(&(desc->shutdown_dwork),
			      msecs_to_jiffies(2000));

	ret = sysfs_setup_files(desc);
	if (ret)
		goto sysfs_setup_err;

	desc->tcpc_nb.notifier_call = quick_chr_type_noti;
	ret = register_tcp_dev_notifier(desc->tcpc, &desc->tcpc_nb,
					TCP_NOTIFY_TYPE_USB |
					TCP_NOTIFY_TYPE_MISC |
					TCP_NOTIFY_TYPE_VBUS);
	if (ret < 0) {
		sysfs_err("%s register tcpc notifier fail(%d)\n", __func__,
			  ret);
		ret = -EINVAL;
		goto tcpc_noti_err;
	}

/* N17 code for HQ-308084 by liunianliang at 2023/7/31 start */
	desc->psy_nb.notifier_call = psy_change_noti;
	ret = power_supply_reg_notifier(&desc->psy_nb);
	if (ret < 0) {
		sysfs_err("%s register psy notifier fail(%d)\n", __func__,
			  ret);
		ret = -EINVAL;
		goto main_psy_err;
	}
/* N17 code for HQ-308084 by liunianliang at 2023/7/31 end */

/*N17 code for HQ-290781 by wangtingting at 2023/5/29 start*/
	desc->wakeup_thread = false;
	init_waitqueue_head(&desc->wq);
	desc->soc_decimal_task = kthread_run(soc_decimal_threadfn, desc,
							"soc_decimal_task");
	if (IS_ERR(desc->soc_decimal_task)) {
			ret = -PTR_ERR(desc->soc_decimal_task);
			sysfs_err("%s run task fail(%d)\n", __func__, ret);
	}
/*N17 code for HQ-290781 by wangtingting at 2023/5/29 end*/
	g_desc = desc;

	return 0;

      tcpc_noti_err:
      sysfs_setup_err:
	cancel_delayed_work(&(desc->shutdown_dwork));
      psy_tcpc_err:
      main_psy_err:
	kfree(desc);
      alloc_err:
	return ret;
}

static void __exit hq_charger_sysfs_exit(void)
{

}

late_initcall_sync(hq_charger_sysfs_init);
module_exit(hq_charger_sysfs_exit);

MODULE_LICENSE("GPL");
