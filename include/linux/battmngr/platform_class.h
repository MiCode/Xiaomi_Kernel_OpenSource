#ifndef PLATFORM_CLASS_H
#define PLATFORM_CLASS_H

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>

static int log_level = 2;
#define class_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[battmngr_class] " fmt, ##__VA_ARGS__);	\
} while (0)

#define class_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[battmngr_class] " fmt, ##__VA_ARGS__);	\
} while (0)

#define class_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[battmngr_class] " fmt, ##__VA_ARGS__);	\
} while (0)

struct battmngr_properties {
  	const char *alias_name;
};
 
  /* Data of notifier from battmngr device */
struct chgdev_notify {
  	bool vbusov_stat;
};

struct battmngr_device {
  	struct battmngr_properties props;
  	struct chgdev_notify noti;
  	const struct battmngr_ops *ops;
  	struct mutex ops_lock;
  	struct device dev;
  	struct srcu_notifier_head evt_nh;
  	void	*driver_data;
  	bool is_polling_mode;
};

struct battmngr_ops {
    /* fg ops */
     //int (*get_soc_decimal_rate)();
     int (*fg_soc)(int*);
     int (*fg_curr)(int*);
     int (*fg_volt)(int*);
     int (*fg_temp)(int*);
     int (*charge_status)(int*);
     int (*fg1_ibatt)(int*);
     int (*fg2_ibatt)(int*);
     int (*fg1_volt)(int*);
     int (*fg2_volt)(int*);
     int (*fg1_fcc)(int*);
     int (*fg2_fcc)(int*);
     int (*fg1_rm)(int*);
     int (*fg2_rm)(int*);
     int (*fg1_raw_soc)(int*);
     int (*fg2_raw_soc)(int*);
     int (*fg1_soc)(int*);
     int (*fg2_soc)(int*);
     int (*set_fg1_fastcharge)(int fc);
     int (*set_fg2_fastcharge)(int fc);
     int (*get_fg1_fastcharge)(int*);
     int (*get_fg2_fastcharge)(bool fc);
     int (*fg1_temp)(int*);
     int (*fg2_temp)(int*);
     int (*fg_suspend)(bool suspend);
     int (*set_term_cur)(int cur);
     int (*get_batt_auth)(int*);
     int (*get_chip_ok)(int*);

    /* nanp ops */
     int (*online)(u8*);
     int (*real_type)(int*);
     int (*usb_type)(int*);
     int (*input_curr_limit)(int*);
     int (*power_max)(int*);

};

#define to_battmngr_device(obj) container_of(obj, struct battmngr_device, dev)

extern struct battmngr_device *battmngr_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct battmngr_ops *ops,
		const struct battmngr_properties *props);

extern void battmngr_device_unregister(struct battmngr_device *adapter_dev);

extern struct battmngr_device *get_adapter_by_name(const char *name);

extern int battmngr_qtiops_get_fg_soc(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg_curr(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg_volt(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg_temp(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_charge_status(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg1_ibatt(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg2_ibatt(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg1_volt(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg2_volt(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg1_fcc(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg2_fcc(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg1_rm(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg2_rm(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg1_raw_soc(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg2_raw_soc(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg1_soc(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg2_soc(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_set_fg1_fastcharge(struct battmngr_device *battmg_dev, int value);
extern int battmngr_qtiops_set_fg2_fastcharge(struct battmngr_device *battmg_dev, int value);
extern int battmngr_qtiops_get_fg1_fastcharge(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg1_temp(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_fg2_temp(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_set_fg_suspend(struct battmngr_device *battmg_dev, int value);
extern int battmngr_qtiops_set_term_cur(struct battmngr_device *battmg_dev, int value);
extern int battmngr_qtiops_get_batt_auth(struct battmngr_device *battmg_dev);
extern int battmngr_qtiops_get_chip_ok(struct battmngr_device *battmg_dev);

extern bool check_qti_ops(struct battmngr_device **battmg_dev);

extern int battmngr_noops_get_online(struct battmngr_device *battmg_dev);
extern int battmngr_noops_get_usb_type(struct battmngr_device *battmg_dev);
extern int battmngr_noops_get_real_type(struct battmngr_device *battmg_dev);
extern int battmngr_noops_get_input_curr_limit(struct battmngr_device *battmg_dev);
extern int battmngr_noops_get_power_max(struct battmngr_device *battmg_dev);
extern struct battmngr_device* check_nano_ops(void);


#endif
