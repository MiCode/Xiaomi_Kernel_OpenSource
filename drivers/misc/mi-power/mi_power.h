#include<linux/list.h>
#include<linux/seq_file.h>
#include<linux/kobject.h>
#include<linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <linux/types.h>
#include <linux/time64.h>

struct timespec;

struct power_info
{
	struct class *power_class;
	struct device *power_dev;
	int major;
};

struct wakeup_device {
	struct list_head list;
	char *name;
	void *data;
	unsigned int count;
	bool (*check_wakeup_event)(void *data);
};

struct system_event_file_ops {
	int (*event_show)(struct seq_file *seq, void *v);
};

enum system_event_type {
	SYSTEM_EVENT_ALARM,
	SYSTEM_EVENT_UEVENT,
	SYSTEM_EVENT_MAX,
};

struct system_event {
	char creator_info[100];
	struct timespec64 set_time;
};

enum system_dbg_type {
	DEBUG_INFO_CLOCK,
	DEBUG_INFO_REGULATOR,
	DEBUG_INFO_GPIO,
	DEBUG_INFO_RPMH_SOC_STATS,
	DEBUG_INFO_RPMH_SUBSYSTEM_STATS,
	DEBUG_INFO_MAX,
};

enum power_mode_type {
	POWER_PERF_MODE,
	POWER_BALANCE_MODE,
	POWER_SAVE_MODE,
	POWER_DEEP_SAVE_MODE,
	POWER_DIVINA_MODE, /*for xiaobai test mode index 2*/
	POWER_MODE_MAX,
};

struct system_dbg_info {
	struct list_head list;
	int type;
	void (*system_dbg_info_print)(struct system_dbg_info *info);
	void *data;
};

struct system_event_recorder {
	struct list_head list;
	char *name;
	enum system_event_type type;
	struct system_event_file_ops *ops;
	void (*system_event_record)(struct system_event_recorder *recorder, void *data);
	void (*system_event_print_records)(struct system_event_recorder *recorder);
	u32 max_num;
	void *buff;
	u32 count;
	u32 index_head;
	u32 index_tail;
};

struct stats_config {
	unsigned int offset_addr;
	unsigned int ddr_offset_addr;
	unsigned int num_records;
	bool appended_stats_avail;
};


struct stats_prv_data {
	const struct stats_config *config;
	void __iomem *reg;
};

struct sleep_stats_data {
	dev_t		dev_no;
	struct class	*stats_class;
	struct device	*stats_device;
	struct cdev	stats_cdev;
	const struct stats_config	**config;
	void __iomem	*reg_base;
	void __iomem	*ddr_reg;
	void __iomem	**reg;
	u32	ddr_key;
	u32	ddr_entry_count;
};

int pm_register_system_dbg_info(struct system_dbg_info *info);
void alarm_record_add(struct alarm *alarm);
int pm_register_wakeup_device(struct wakeup_device *dev);
void pm_system_dbg_info_print(enum system_dbg_type type);
int pm_register_system_event_recorder(struct system_event_recorder *rec);
void pm_trigger_system_event_record(enum system_event_type type, void *data);
void uevent_record_add(struct kobj_uevent_env *uevent_env);
bool power_debug_print_enabled(void);
int power_mode_register_notifier(struct notifier_block *nb);
int power_mode_unregister_notifier(struct notifier_block *nb);

struct gpio_dbg_device {
	struct list_head list;
	char *name;
	void (*gpiolib_dbg_info_print)(struct gpio_chip *chip);
	struct gpio_chip *chip;
};
void log_irq_wakeup_reason(int irq);
void gpiochip_add_dbg_device(struct gpio_dbg_device *dev);
void soc_sleep_stats_dbg_register(struct stats_prv_data *prv_data);
