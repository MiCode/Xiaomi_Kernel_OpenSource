#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_common.h>

int ioburst_monitor_en = 0;
unsigned long ioburst_monitor_interval = 2000;
unsigned long ioburst_trigger_cnt = 0;
unsigned long ioburst_trigger_value = 1000;
unsigned long long last_iorequest_cnt;
struct scsi_device *sdev = NULL;
struct device *ioturbo_dev = NULL;

struct delayed_work ioturbo_delay_wq;
struct workqueue_struct *ioburst_monitor_wq = NULL;

#define SCSI_LUN 		0

static void look_up_scsi_device(int lun)
{
	struct Scsi_Host *shost;

	shost = scsi_host_lookup(0);
	if (!shost)
		return;

	sdev = scsi_device_lookup(shost, 0, 0, lun);
	if (!sdev)
		return;

	printk(KERN_ERR "scsi device proc name is %s\n", sdev->host->hostt->proc_name);

	scsi_device_put(sdev);

	scsi_host_put(shost);

	return;
}


static void ioburst_monitor(struct work_struct *work)
{
	char * s_c[2];
	int iopressure = 0;
	unsigned long long iorequest_cnt = 0;

	if(!sdev) {
		look_up_scsi_device(SCSI_LUN);
		if (!sdev) {
			ioburst_monitor_en = 0;
		}
	}

	if(ioburst_monitor_en) {
		iorequest_cnt = atomic_read(&sdev->iorequest_cnt);
		if (last_iorequest_cnt != 0)
			iopressure = iorequest_cnt - last_iorequest_cnt;

		printk(KERN_ERR "iopressure=%d,iorequest_cnt=%lld\n",iopressure,iorequest_cnt);
		
		last_iorequest_cnt = iorequest_cnt;

		if(iopressure > ioburst_trigger_value) {
			s_c[0] = "NAME=ioburst_trigger";
			s_c[1] = NULL;
			kobject_uevent_env(&ioturbo_dev->kobj, KOBJ_CHANGE, s_c);
			ioburst_monitor_en = 0;
			printk(KERN_ERR "in ioturbo_uevent send.\n");
			last_iorequest_cnt = 0;
		}
		
	}

	queue_delayed_work(ioburst_monitor_wq, &ioturbo_delay_wq, msecs_to_jiffies(ioburst_monitor_interval));

	return;
}

static ssize_t ioburst_monitor_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	snprintf(buf, 2, "%d\n", ioburst_monitor_en);
	rc = strlen(buf);

	return rc;
}

static ssize_t ioburst_monitor_enable_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	ioburst_monitor_en = (int)simple_strtoul(buf, NULL, 2);

	return count;
}

static ssize_t ioturbo_trigger_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	snprintf(buf, 6, "%d\n", ioburst_trigger_value);
	rc = strlen(buf);
	return rc;
}

static ssize_t ioturbo_trigger_value_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	if (kstrtoul(buf, 0, &ioburst_trigger_value))
		return -EINVAL;

	printk(KERN_ERR "ioburst_trigger_value = %d.\n, ioburst_trigger_value");

	return count;
}

static ssize_t ioturbo_monitor_interval_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	snprintf(buf, 6, "%d\n", ioburst_monitor_interval);
	rc = strlen(buf);
	return rc;
}

static ssize_t ioturbo_monitor_interval_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	if (kstrtoul(buf, 0, &ioburst_monitor_interval))
		return -EINVAL;

	printk(KERN_ERR "ioburst_monitor_interval = %d ms.\n, ioburst_monitor_interval");

	return count;
}

static ssize_t ioturbo_trigger_ioburst_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	snprintf(buf, 2, "%d\n", ioburst_trigger_cnt);
	rc = strlen(buf);
	return rc;
}

static ssize_t ioturbo_trigger_ioburst_store( struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	ioburst_trigger_cnt++;
	printk(KERN_ERR "ioburst_trigger_cnt = %d.\n, ioburst_trigger_cnt");
	return count;
}

static DEVICE_ATTR(monitor_enable, S_IRUGO|S_IWUSR, ioburst_monitor_enable_show, ioburst_monitor_enable_store);
static DEVICE_ATTR(monitor_interval, S_IRUGO|S_IWUSR, ioturbo_monitor_interval_show, ioturbo_monitor_interval_store);
static DEVICE_ATTR(trigger_value, S_IRUGO|S_IWUSR, ioturbo_trigger_value_show, ioturbo_trigger_value_store);
static DEVICE_ATTR(trigger_ioburst, S_IRUGO|S_IWUSR, ioturbo_trigger_ioburst_show, ioturbo_trigger_ioburst_store);

static const struct attribute *ioturbo_event_attr[] = {
	&dev_attr_monitor_enable.attr,
	&dev_attr_monitor_interval.attr,
	&dev_attr_trigger_value.attr,
	&dev_attr_trigger_ioburst.attr,
	NULL,
};

static const struct attribute_group ioturbo_event_attr_group = {
	.attrs = (struct attribute **) ioturbo_event_attr,
};

static struct class ioturbo_event_class = {
	.name =         "ioturbo",
	.owner =        THIS_MODULE,
};

static int __init ioturbo_uevent_init(void)
{
	int ret = 0;

	printk(KERN_ERR "in ioturbo_uevent_init.\n");

	ret = class_register(&ioturbo_event_class);
	if( ret < 0 ){
		printk(KERN_ERR "ioturbo_event: class_register fail\n");
		return ret;
	}

	ioturbo_dev = device_create(&ioturbo_event_class, NULL, MKDEV(0, 0), NULL, "ioturbo_event");
	if( ioturbo_dev ){
		ret = sysfs_create_group(&ioturbo_dev->kobj, &ioturbo_event_attr_group);
		if( ret < 0 ){
			printk(KERN_ERR "ioturbo_event:sysfs_create_group fail\n");
			return ret;
		}
	}else{
		printk(KERN_ERR "ioturbo_event:device_create fail\n");
		return -1;
	}
	INIT_DELAYED_WORK(&ioturbo_delay_wq, ioburst_monitor);
	ioburst_monitor_wq = create_workqueue("ioburst_monitor");
	queue_delayed_work(ioburst_monitor_wq, &ioturbo_delay_wq, msecs_to_jiffies(10000));

	return 0;
}

static void __exit ioturbo_uevent_exit(void)
{
	destroy_workqueue(ioburst_monitor_wq);
	sysfs_remove_group(&ioturbo_dev->kobj, &ioturbo_event_attr_group);
	device_destroy(&ioturbo_event_class, MKDEV(0, 0));
	class_unregister(&ioturbo_event_class);
}

module_init(ioturbo_uevent_init);
module_exit(ioturbo_uevent_exit);
MODULE_DESCRIPTION("Interface for xiaomi ioturbo");
MODULE_AUTHOR("caotianyang <caotianyang@xiaomi.com>");
MODULE_LICENSE("GPL");

