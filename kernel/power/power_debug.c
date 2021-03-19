#include<linux/init.h>
#include<linux/module.h>
#include<linux/slab.h>
#include<linux/string.h>
#include<linux/errno.h>
#include<linux/spinlock_types.h>
#include<linux/power_debug.h>
#include<linux/syscore_ops.h>
#include<linux/device.h>

static LIST_HEAD(wakeup_devices);
static LIST_HEAD(system_event_recorders);
static DEFINE_SPINLOCK(wakeup_lock);
static spinlock_t records_lock;

static struct class *power_debug_class;

int pm_register_wakeup_device(struct wakeup_device *dev)
{
	int ret = 0;
	struct list_head *list;
	struct wakeup_device *device;

	if (dev==NULL)
		return EINVAL;

	list_for_each(list, &wakeup_devices){
		device = list_entry(list, struct wakeup_device, list);
		if(device->name == dev->name)
			return EEXIST;
        }

	spin_lock(&wakeup_lock);
	list_add_tail(&(dev->list), &wakeup_devices);
	spin_unlock(&wakeup_lock);

	return ret;
}

int pm_register_system_event_recorder(struct system_event_recorder *rec)
{
	int ret = 0;
	struct list_head *list;
	struct system_event_recorder *recorder;

	if (rec==NULL)
		return -EINVAL;

	if (rec->max_num > 0) {
		rec->buff = kmalloc(sizeof(struct system_event) * rec->max_num, GFP_KERNEL);
		if (!rec->buff){
			printk("%s: failed to alloc mem for recorder buffer\n", __func__);
			return ret;
		}
	}

	list_for_each(list, &system_event_recorders) {
		recorder = list_entry(list, struct system_event_recorder, list);
		if(recorder->type == rec->type)
			return -EEXIST;
	}

	spin_lock(&records_lock);
	list_add_tail(&(rec->list), &system_event_recorders);
	spin_unlock(&records_lock);

	return ret;	
}

void pm_trigger_system_event_record(enum system_event_type type, void *data)
{
	struct system_event_recorder *rec;

	if (type >= SYSTEM_EVENT_MAX || data == NULL)
		return;
	
	spin_lock(&records_lock);
	list_for_each_entry(rec, &system_event_recorders, list){
		if(rec->type == type)
			rec->system_event_record(rec, data);
	}
	
	spin_unlock(&records_lock);

	return;
}

static int pm_debug_suspend()
{
	int ret = 0;

	return ret;
}

static void pm_debug_resume(void)
{
	struct wakeup_device *dev;

	spin_lock(&wakeup_lock);

	list_for_each_entry(dev, &wakeup_devices, list){
		if (dev->check_wakeup_event)
			dev->check_wakeup_event(dev->data);
	}

	spin_unlock(&wakeup_lock);

}
 
static struct syscore_ops pm_debug_ops = {
	.suspend = pm_debug_suspend,
	.resume = pm_debug_resume,
};

static ssize_t wakeup_devices_show (struct class *cls, struct class_attribute *attr, char *buf)
{
	struct wakeup_device *dev;
	int written = 0;

	spin_lock(&wakeup_lock);
	
	list_for_each_entry(dev, &wakeup_devices, list) {
		if (dev->name){
			written += sprintf(buf+written, "%s ", dev->name);
		}
	}

	spin_unlock(&wakeup_lock);

	written += sprintf(buf+written, "\n");

	return written;
}

static ssize_t wakeup_devices_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	return ret;	
}

static struct class_attribute power_debug_attrs[] = {
	__ATTR(wakeup_device_list, 0664, wakeup_devices_show, wakeup_devices_store),
	__ATTR_NULL
};

static int __init pm_debug_init(void)
{
	int ret = 0;
	int i;

	power_debug_class = class_create(THIS_MODULE, "power_debug");
	if (IS_ERR(power_debug_class)){
		printk("failed to create power debug class\n");
		return PTR_ERR(power_debug_class);
	}

	for (i = 0; power_debug_attrs[i].attr.name!= NULL; i++){
		ret = class_create_file(power_debug_class, &power_debug_attrs[i]);
		if (ret != 0){
			printk("failed to create attribute file\n");
			return ret;
		}
	}

	register_syscore_ops(&pm_debug_ops);

	return ret;
}

static void __exit pm_debug_exit()
{
}

core_initcall(pm_debug_init);
module_exit(pm_debug_exit);
