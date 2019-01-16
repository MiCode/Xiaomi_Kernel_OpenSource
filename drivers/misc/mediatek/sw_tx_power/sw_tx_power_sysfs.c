#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "sw_tx_power.h"

struct swtp_sysfs_obj {
	struct kobject kobj;
	int swtp_mode;
};

#define to_swtp_sysfs_obj(x) 	container_of(x, struct swtp_sysfs_obj, kobj)

struct swtp_sysfs_attribute{
	struct attribute attr;
	ssize_t (*show)(struct swtp_sysfs_obj *swtp_sysfs, struct swtp_sysfs_attribute *attr, char *buf);
	ssize_t (*store)(struct swtp_sysfs_obj *swtp_sysfs, struct swtp_sysfs_attribute *attr, const char *buf, size_t len);
};

#define to_swtp_sysfs_attr(x)	container_of(x, struct swtp_sysfs_attribute, attr)

static ssize_t swtp_attr_show (struct kobject *kobj,
                               struct attribute *attr,
                               char *buf)
{
    struct swtp_sysfs_attribute *attribute;
    struct swtp_sysfs_obj       *swtp_sysfs;

    attribute  = to_swtp_sysfs_attr(attr);
    swtp_sysfs = to_swtp_sysfs_obj(kobj);

    if(!attribute->show) 
	return -EIO;

    return attribute->show(swtp_sysfs, attribute, buf);
}

static ssize_t swtp_attr_store (struct kobject *kobj,
                                struct attribute *attr,
                                const char *buf, size_t len)
{
    struct swtp_sysfs_attribute *attribute;
    struct swtp_sysfs_obj       *swtp_sysfs;

    attribute  = to_swtp_sysfs_attr(attr);
    swtp_sysfs = to_swtp_sysfs_obj(kobj);

    if(!attribute->store) 
	return -EIO;

    return attribute->store(swtp_sysfs, attribute, buf, len);
}

static struct sysfs_ops swtp_sysfs_ops = {
	.show = swtp_attr_show,
	.store = swtp_attr_store,
};

static void swtp_sysfs_release(struct kobject *kobj)
{
    struct swtp_sysfs_obj       *swtp_sysfs;

    swtp_sysfs = to_swtp_sysfs_obj(kobj);
    kfree(swtp_sysfs);
}

extern int swtp_set_mode(unsigned int ctrid, unsigned int enable);
extern int swtp_reset_mode(void);
extern unsigned int swtp_get_mode(swtp_state_type *swtp_super_state, swtp_state_type *swtp_normal_state);

static ssize_t swtp_sysfs_show(struct swtp_sysfs_obj *swtp_sysfs_obj, 
                               struct swtp_sysfs_attribute *attr,
                               char *buf)
{
    unsigned int run_mode;
    int buf_len=0;
    swtp_state_type swtp_super_state, swtp_normal_state;

    run_mode = swtp_get_mode(&swtp_super_state, &swtp_normal_state);

    buf_len += sprintf(buf, "run_mode: 0x%x/0x%x\n", run_mode, SWTP_CTRL_MAX_STATE);
    buf_len += sprintf(buf+buf_len, "[SM] : 0x%x, 0x%x, 0x%x\n", swtp_super_state.enable, \
					      swtp_super_state.mode, \
                                              swtp_super_state.setvalue);

    buf_len += sprintf(buf+buf_len, "[UM] : 0x%x, 0x%x, 0x%x\n", swtp_normal_state.enable,\
					      swtp_normal_state.mode, \
                                              swtp_normal_state.setvalue);
    return buf_len;
}

#define SWTP_AP_PARAM	"ap_param "
#define SWTP_2_DIGIT	(2)
#define SWTP_1_DIGIT	(1)
#define SWTP_ENTER	(1)

static ssize_t swtp_sysfs_store(struct swtp_sysfs_obj *swtp_sysfs_obj, 
                                struct swtp_sysfs_attribute *attr,
                                const char *buf, size_t len)
{ 
    char *stop;
    long set_mode, temp;
    int pstrlen;

    if(strstr(buf, "off"))
	 swtp_reset_mode();
    else if(strstr(buf, SWTP_AP_PARAM))
    {
        pstrlen = strlen(SWTP_AP_PARAM);
        printk("[swtp] sysfs_store %s %d %d\n", buf, len, pstrlen);

        if(len == pstrlen + SWTP_2_DIGIT + SWTP_ENTER) 
		set_mode = (buf[pstrlen] -'0')*10+(buf[pstrlen+1] -'0');
        else if(len == pstrlen + SWTP_1_DIGIT + SWTP_ENTER) 
		set_mode = (buf[pstrlen] -'0');
	else 
	{
		printk("[swtp] parameter error\n"); 
		return 0;
	}
	
        if(set_mode < SWTP_CTRL_MAX_STATE)
	     swtp_set_mode(set_mode, SWTP_MODE_ON);

        printk("[swtp] sysfs_store %ld %ld\n", set_mode, temp);      
    }
    else
    {
        printk("[swtp] command error\n");      
    }	

    return len;
}

static struct swtp_sysfs_attribute swtp_ctrl_attribute = 
	__ATTR(swtp0, 0666, swtp_sysfs_show, swtp_sysfs_store);

struct attribute *swtp_attributes[] = {
	&swtp_ctrl_attribute.attr,
	NULL,
};

struct kobj_type swtp_ktype = {
	.sysfs_ops = &swtp_sysfs_ops,
        .release = swtp_sysfs_release,
	.default_attrs = swtp_attributes,
};

static struct kset *swtp_kset;
static struct swtp_sysfs_obj *swtp_obj;

static int __init swtp_sysfs_init(void) 
{
    struct swtp_sysfs_obj  *obj;

    printk(KERN_ALERT "[swtp] swtp_sysfs_init.\n");  

    // located under /sys/kernel/swtp_info
    //
    swtp_kset = kset_create_and_add("swtp", NULL, kernel_kobj);
    if(!swtp_kset)
    {
    	printk(KERN_ALERT "[swtp] kset_create_and_add.\n"); 
	return -ENOMEM;
    }

    obj= kzalloc(sizeof(*obj), GFP_KERNEL);
    if(!obj)
    {
    	printk(KERN_ALERT "[swtp] kzalloc.\n"); 
	return -1;
    }

    obj->kobj.kset = swtp_kset;

    if (kobject_init_and_add(&obj->kobj, &swtp_ktype, NULL, "ctrl")) {
            kobject_put(&obj->kobj);
            printk(KERN_ALERT "[swtp] kobject_init_and_add.\n"); 
            return -ENOMEM;
    }

    kobject_uevent(&obj->kobj, KOBJ_ADD);
    swtp_obj = obj;

    return 0;
}

static void __exit swtp_sysfs_exit(void)
{
    kobject_put(&swtp_obj->kobj);    

    return;
}

module_init(swtp_sysfs_init);
module_exit(swtp_sysfs_exit);
