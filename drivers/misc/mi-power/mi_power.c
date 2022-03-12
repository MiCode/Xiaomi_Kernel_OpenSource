#include<linux/init.h>
#include<linux/module.h>
#include<linux/slab.h>
#include<linux/string.h>
#include<linux/errno.h>
#include<linux/spinlock_types.h>
#include<linux/syscore_ops.h>
#include<linux/device.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/timekeeping.h>
#include <uapi/linux/gpio.h>
#include <linux/gpio/driver.h>
#include <../drivers/gpio/gpiolib.h>
#include <linux/acpi.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/percpu.h>
#include <linux/refcount.h>
#include <linux/msm_rtb.h>
#include <linux/wakeup_reason.h>

#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic-common.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/irqchip/irq-partition-percpu.h>

#include <asm/cputype.h>
#include <asm/exception.h>
#include <asm/smp_plat.h>
#include <asm/virt.h>


#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/soc/qcom/smem.h>
#include <asm/arch_timer.h>
#include <linux/alarmtimer.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/rtc.h>

#include <linux/notifier.h>
#include <clocksource/arm_arch_timer.h>

#include "mi_power.h"

#include <trace/events/power.h>

#define TAGS "[xiaomi_power]"
#define POWER_DEBUG(fmt, args...)		pr_debug("%s %s: "fmt"", TAGS, __func__, ##args)
#define POWER_INFO(fmt, args...)		pr_info("%s %s:  "fmt"", TAGS, __func__, ##args)
#define POWER_ERR(fmt, args...)			pr_err("%s %s:  "fmt"", TAGS, __func__,  ##args)

static LIST_HEAD(wakeup_devices);
static LIST_HEAD(system_event_recorders);
static LIST_HEAD(system_dbg_info_list);
static DEFINE_SPINLOCK(wakeup_lock);
static DEFINE_SPINLOCK(records_lock);
static DEFINE_SPINLOCK(dbg_info_lock);

static struct class *power_debug_class;

static uint8_t power_debug_enable = 0;
static uint8_t gpower_mode = POWER_PERF_MODE;

#define GKI_HOOK 1

#if GKI_HOOK
/*
 * struct wakeup_irq_node
 *
 * wakeup reason info
 */

static struct spinlock* wakeup_reason_lock = NULL;
static struct list_head* leaf_irqs = NULL;   /* kept in ascending IRQ sorted order */
static struct list_head* parent_irqs = NULL; /* unordered */
static int* wakeup_reason;
static bool* capture_reasons;
static struct kmem_cache *wakeup_irq_nodes_cache;

static const char *default_irq_name = "(unnamed)";
int subsystem_sleep_stats_sysfs_print(char *buf);
static void soc_sleep_stats_dbg_info_print(struct system_dbg_info *info);

enum wakeup_reason_flag {
	RESUME_NONE = 0,
	RESUME_IRQ,
	RESUME_ABORT,
	RESUME_ABNORMAL,
};

struct wakeup_irq_node {
	struct list_head siblings;
	int irq;
	const char *irq_name;
};

/**
 * gic info
 * print gic status info
 */

struct redist_region {
	void __iomem		*redist_base;
	phys_addr_t		phys_base;
	bool			single_redist;
};

#endif

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
EXPORT_SYMBOL(pm_register_wakeup_device);

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
			POWER_ERR("failed to alloc mem for recorder buffer\n");
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

int pm_register_system_dbg_info(struct system_dbg_info *info)
{
	struct system_dbg_info *in;
	struct list_head *list;

	if (info == NULL)
		return 0;

	if (info->type >= DEBUG_INFO_MAX){
		POWER_ERR("error type.");
		return 0;
	}

	list_for_each(list, &system_dbg_info_list){
		in = list_entry(list, struct system_dbg_info, list);
		if(in->type == info->type)
			return EEXIST;
   }

	spin_lock(&dbg_info_lock);
	list_add_tail(&(info->list), &system_dbg_info_list);
	spin_unlock(&dbg_info_lock);

	return 0;
}
EXPORT_SYMBOL(pm_register_system_dbg_info);

void pm_system_dbg_info_print(enum system_dbg_type type)
{
	struct system_dbg_info *info;

	POWER_INFO("power-debug:  pm_system_dbg_info_print : type: %d",type);

	if (type >= DEBUG_INFO_MAX)
		POWER_ERR("error type.");

	spin_lock(&dbg_info_lock);

	list_for_each_entry(info, &system_dbg_info_list, list){
		if (info->type == type)
			info->system_dbg_info_print(info->data);
	}

	spin_unlock(&dbg_info_lock);
	return;
}
EXPORT_SYMBOL(pm_system_dbg_info_print);

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

static ssize_t power_debug_show (struct class *cls, struct class_attribute *attr, char *buf)
{
	int enable = 0;


	if (power_debug_enable)
		enable = scnprintf(buf, PAGE_SIZE, "%d\n", 1);
	else
		enable = scnprintf(buf, PAGE_SIZE, "%d\n", 0);

	return enable;
}

/**
 * soc_lpm_suspend_info - for GKI
 * print soc suspend clock gpio regulator status
 */

static void soc_lpm_suspend_info(void) {
	pm_system_dbg_info_print(DEBUG_INFO_RPMH_SOC_STATS);
	pm_system_dbg_info_print(DEBUG_INFO_RPMH_SUBSYSTEM_STATS);
  return;
}

/* use for default close debug info need set persist.power.enable_debug  property to open */
static void power_debug_suspend_property_trace_probe(void *unused,const char *action, int val, bool start)
{
	POWER_INFO("register power suspend trace start=%d,val=%d \n", start, val);
	if (start && val > 0 && !strcmp("machine_suspend", action)) {
		POWER_INFO("Enabled power debug infos :");
        pm_system_dbg_info_print(DEBUG_INFO_GPIO);
	}
}

static ssize_t power_debug_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
    int ret;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

    if (val >=1)
        val =1;

    if (val == power_debug_enable)
        return count;

    if (val){
		power_debug_enable = 1;
		ret = register_trace_suspend_resume(power_debug_suspend_property_trace_probe, NULL);
	  }
	else
	  {
		power_debug_enable = 0;
		ret = unregister_trace_suspend_resume(power_debug_suspend_property_trace_probe, NULL);
	  }

	if (ret) {
		POWER_ERR("Failed to %sregister suspend trace callback, ret=%d\n",
			val ? "" : "un", ret);
			
		return ret;
	}
	
	return count;

}


static ssize_t power_mode_show (struct class *cls, struct class_attribute *attr, char *buf)
{
	int enable = 0;
    if (0< gpower_mode < POWER_MODE_MAX)
		enable = scnprintf(buf, PAGE_SIZE, "%d\n", gpower_mode);
	return enable;
}


static ssize_t power_mode_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
    if (0< val < POWER_MODE_MAX)
		gpower_mode = val;

	return count;
}

static ssize_t subsystem_sleep_state_show (struct class *cls, struct class_attribute *attr, char *buf)
{
	return subsystem_sleep_stats_sysfs_print(buf);
}

#if 0
static ssize_t soc_sleep_state_show (struct class *cls, struct class_attribute *attr, char *buf)
{
	soc_sleep_stats_dbg_info_print(NULL);
	return sprintf(buf, "%s\n", "done");
}
#endif

uint8_t mi_power_mode(void)
{
	uint8_t power_mode = POWER_PERF_MODE;
	switch (gpower_mode) {
	case 0:
		power_mode = POWER_PERF_MODE;
		break;
	case 1:
		power_mode = POWER_BALANCE_MODE;
		break;
	case 2:
		power_mode = POWER_SAVE_MODE;
		break;
	case 3:
		power_mode = POWER_DEEP_SAVE_MODE;
		break;
	case 4:
		power_mode = POWER_DIVINA_MODE;
		break;
	default:
		power_mode = POWER_PERF_MODE;
		break;
}
	return power_mode;
}
EXPORT_SYMBOL(mi_power_mode);


ssize_t mi_power_save_battery_cave(ssize_t capcity)
{
	ssize_t capcity_tmp =0;
	/* if not in pPOWER_DIVINA_MODE mode do nothing */
	if ( mi_power_mode() < POWER_SAVE_MODE)
	    return capcity;
	capcity_tmp = capcity;
	/* improve 20% - 100% battery cap cave  from %20 -0% move 5% */
	if (capcity_tmp > 20 )
	{
		capcity = ((capcity_tmp -20) * 100 + 106) /107 + 25;
	}
	else if (capcity_tmp >= 0 )
	{
		capcity = (capcity_tmp * 100 + 79) / 80;
	}
	if (capcity >= 100)
	    capcity = 100;
	if (capcity <=0)
	    capcity = 0;
	return capcity;
}
EXPORT_SYMBOL(mi_power_save_battery_cave);

static struct class_attribute power_debug_attrs[] = {
	__ATTR(wakeup_device_list, 0664, wakeup_devices_show, wakeup_devices_store),
	__ATTR(debug_suspend, 0664, power_debug_show, power_debug_store),
	__ATTR(power_mode, 0664, power_mode_show, power_mode_store),
	__ATTR(subsystem_sleep_state, 0664, subsystem_sleep_state_show, NULL),
	/*__ATTR(soc_sleep_state, 0664, soc_sleep_state_show, NULL),*/
	__ATTR_NULL
};

#if GKI_HOOK
/**
 * wakeup_reason_lock - wakeup info print
 *
 */

static void init_node(struct wakeup_irq_node *p, int irq)
{
	struct irq_desc *desc;

	INIT_LIST_HEAD(&p->siblings);

	p->irq = irq;
	desc = irq_to_desc(irq);
	if (desc && desc->action && desc->action->name)
		p->irq_name = desc->action->name;
	else
		p->irq_name = default_irq_name;
}

static struct wakeup_irq_node *create_node(int irq)
{
	struct wakeup_irq_node *result;

	result = kmem_cache_alloc(wakeup_irq_nodes_cache, GFP_ATOMIC);
	if (unlikely(!result))
		pr_warn("Failed to log wakeup IRQ %d\n", irq);
	else
		init_node(result, irq);

	return result;
}

static bool add_sibling_node_sorted(struct list_head *head, int irq)
{
	struct wakeup_irq_node *n;
	struct list_head *predecessor = head;

	if (unlikely(WARN_ON(!head)))
		return NULL;

	if (!list_empty(head))
		list_for_each_entry(n, head, siblings) {
			if (n->irq < irq)
				predecessor = &n->siblings;
			else if (n->irq == irq)
				return true;
			else
				break;
		}

	n = create_node(irq);
	if (n) {
		list_add(&n->siblings, predecessor);
		return true;
	}

	return false;
}

static struct wakeup_irq_node *find_node_in_list(struct list_head *head,
						 int irq)
{
	struct wakeup_irq_node *n;

	if (unlikely(WARN_ON(!head)))
		return NULL;

	list_for_each_entry(n, head, siblings)
		if (n->irq == irq)
			return n;

	return NULL;
}

void log_irq_wakeup_reason(int irq)
{
	unsigned long flags;

  if(!parent_irqs || !leaf_irqs || !wakeup_reason_lock){
  	pr_err("%s: null irqs list\n",__func__);
  return;
  }

	spin_lock_irqsave(wakeup_reason_lock, flags);
	if (*wakeup_reason == RESUME_ABNORMAL || *wakeup_reason == RESUME_ABORT) {
		spin_unlock_irqrestore(wakeup_reason_lock, flags);
		return;
	}

	if (!(*capture_reasons)) {
		spin_unlock_irqrestore(wakeup_reason_lock, flags);
		return;
	}

	if (find_node_in_list(parent_irqs, irq) == NULL)
		add_sibling_node_sorted(leaf_irqs, irq);

	*wakeup_reason = RESUME_IRQ;
	spin_unlock_irqrestore(wakeup_reason_lock, flags);
}

EXPORT_SYMBOL(log_irq_wakeup_reason);


#endif


#if 0
/**
 * gpiochip_add_dbg_device -
 * print soc gpio  status
 */

DEFINE_SPINLOCK(gpio_dbg_lock);
LIST_HEAD(gpio_dbg_devices);

void gpiochip_add_dbg_device(struct gpio_dbg_device *dev)
{
	struct list_head *list;
	struct gpio_dbg_device *device;

	if (dev == NULL)
		return;

	list_for_each(list, &gpio_dbg_devices) {
		device = list_entry(list, struct gpio_dbg_device, list);
		if (device->name == dev->name)
			return;
	}

	spin_lock(&gpio_dbg_lock);
	list_add_tail(&(dev->list), &gpio_dbg_devices);
	spin_unlock(&gpio_dbg_lock);

	return;
}
EXPORT_SYMBOL(gpiochip_add_dbg_device);


extern int gpiod_get_direction(struct gpio_desc *desc);

static void gpiochip_system_dbg_info_print(struct gpio_dbg_device *ddev)
{
	unsigned		i;
	struct gpio_device      *gdev = ddev->chip->gpiodev;
	struct gpio_chip	*chip = gdev->chip;
	unsigned		gpio = gdev->base;
	struct gpio_desc	*gdesc = &gdev->descs[0];
	bool			is_out;
	bool			is_irq;
	bool			active_low;
	struct device 		*parent = chip->parent;

	if (parent && chip->label)
		pr_info("%s: GPIOs %d-%d, parent: %s/%s, %s",
			dev_name(&gdev->dev), gdev->base, gdev->base + gdev->ngpio-1,
			parent->bus? parent->bus->name:"no-bus",
			dev_name(parent), chip->label);
	else if (parent)
		pr_info("%s: GPIOs %d-%d, parent: %s/%s",
			dev_name(&gdev->dev), gdev->base, gdev->base + gdev->ngpio-1,
			parent->bus? parent->bus->name:"no-bus",
			dev_name(parent));

	if (ddev->gpiolib_dbg_info_print)
		ddev->gpiolib_dbg_info_print(chip);
	else {
		for (i = 0; i < gdev->ngpio; i++, gpio++, gdesc++) {
			if (!test_bit(FLAG_REQUESTED, &gdesc->flags)) {
				if (gdesc->name) {
					pr_info(" gpio-%-3d (%-20.20s)\n",
						   gpio, gdesc->name);
				}
				continue;
			}

			gpiod_get_direction(gdesc);
			is_out = test_bit(FLAG_IS_OUT, &gdesc->flags);
			is_irq = test_bit(FLAG_USED_AS_IRQ, &gdesc->flags);
			active_low = test_bit(FLAG_ACTIVE_LOW, &gdesc->flags);
			pr_info(" gpio-%-3d (%-20.20s|%-20.20s) %s %s %s%s",
				gpio, gdesc->name ? gdesc->name : "", gdesc->label,
				is_out ? "out" : "in ",
				chip->get ? (chip->get(chip, i) ? "hi" : "lo") : "?  ",
				is_irq ? "IRQ " : "",
				active_low ? "ACTIVE LOW" : "");
			pr_info("\n");

		}
	}

	return;
}

static void gpio_system_dbg_info_print(struct system_dbg_info *info)
{
	struct gpio_dbg_device *ddev;

	spin_lock(&gpio_dbg_lock);
	list_for_each_entry(ddev, &gpio_dbg_devices, list)
		gpiochip_system_dbg_info_print(ddev);

	spin_unlock(&gpio_dbg_lock);

	return;
}

struct system_dbg_info gpio_system_dbg_info = {
	.type = DEBUG_INFO_GPIO,
	.system_dbg_info_print = gpio_system_dbg_info_print,
	.data = &gpio_system_dbg_info
};
#endif

/* msm pinctrl register start */


/* msm pinctrl register end*/


/* MSM pmic gpio register start*/




/*msm pmic gpio register end */


/* gpio : msm pinctrl register pm device */

/**
 * RPMH master subsystem stats dbg print
 *  on msm8450
 */

struct subsystem_data {
	const char *name;
	u32 smem_item;
	u32 pid;
};

struct sleep_stats {
	u32 stat_type;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
};

static struct sleep_stats_data *ss_data = NULL;
static struct sleep_stats *sleep_stats_g = NULL;

#define SUBSYSTEM_STATS_OTHERS_NUM		(-2)

enum subsystem_smem_id {
	AOSD = 0,
	CXSD = 1,
	DDR = 2,
	DDR_STATS = 3,
	MPSS = 605,
	ADSP,
	CDSP,
	SLPI,
	GPU,
	DISPLAY,
	SLPI_ISLAND = 613,
	APSS = 631,
};

enum subsystem_pid {
	PID_APSS = 0,
	PID_MPSS = 1,
	PID_ADSP = 2,
	PID_SLPI = 3,
	PID_CDSP = 5,
	PID_WPSS = 13,
	PID_GPU = PID_APSS,
	PID_DISPLAY = PID_APSS,
	PID_OTHERS = SUBSYSTEM_STATS_OTHERS_NUM,
};

static struct subsystem_data subsystems[] = {
	{ "apss", APSS, QCOM_SMEM_HOST_ANY },
	{ "modem", MPSS, PID_MPSS },
	{ "wpss", MPSS, PID_MPSS },
	{ "adsp", ADSP, PID_ADSP },
	{ "adsp_island", SLPI_ISLAND, PID_ADSP },
	{ "cdsp", CDSP, PID_CDSP },
	{ "slpi", SLPI, PID_SLPI },
	{ "gpu", GPU, PID_GPU },
	{ "display", DISPLAY, PID_DISPLAY },
	{ "slpi_island", SLPI_ISLAND, PID_SLPI },
	{ "aosd", AOSD, PID_OTHERS },
	{ "cxsd", CXSD, PID_OTHERS },
	{ "ddr", DDR, PID_OTHERS },
};

static int soc_rpmh_master_sysfs_print_one(struct sleep_stats *stat,const char *name, char *buf)
{
	u64 accumulated = stat->accumulated;
	char stat_type[5] = {0};
	int cnt = 0;
	memcpy(stat_type, &stat->stat_type, sizeof(u32));
	/*
	 * If a subsystem is in sleep when reading the sleep stats adjust
	 * the accumulated sleep duration to show actual sleep time.
	 */
	if (stat->last_entered_at > stat->last_exited_at)
		accumulated += arch_timer_read_counter()
			       - stat->last_entered_at;
	cnt += sprintf(buf+cnt, "%s:", name);
    cnt += sprintf(buf+cnt, "%d\n", stat_type);
	cnt += sprintf(buf+cnt, "Count = %u\n", stat->count);
	cnt += sprintf(buf+cnt, "Last Entered At = %llu\n", stat->last_entered_at);
	cnt += sprintf(buf+cnt, "Last Exited At = %llu\n", stat->last_exited_at);
	cnt += sprintf(buf+cnt, "Accumulated Duration = %llu\n", accumulated);

	return cnt;
}

int subsystem_sleep_stats_sysfs_print(char *buf)
{

	struct sleep_stats *stat = NULL;
	int i, idx;
	int cnt= 0;

	for (i = 0; i < ARRAY_SIZE(subsystems); i++) {
		if (subsystems[i].pid == SUBSYSTEM_STATS_OTHERS_NUM) {
			stat = sleep_stats_g;
			idx = subsystems[i].smem_item;
			if (NULL != ss_data && NULL != stat)
				memcpy_fromio(stat, ss_data->reg[idx], sizeof(*stat));
			else {
				stat = NULL;
				POWER_ERR("ss_data is null.");
			}
		} else {
			stat = (struct sleep_stats *) qcom_smem_get(
							subsystems[i].pid,
							subsystems[i].smem_item, NULL);
		}
		if (!IS_ERR_OR_NULL(stat))
			cnt += soc_rpmh_master_sysfs_print_one(stat,
						subsystems[i].name, buf+cnt);
		else
			cnt += sprintf(buf+cnt, "subsystem %s stats %s.\n", subsystems[i].name,
						IS_ERR(stat)?"is not support":"is NULL");
	}

	return cnt;
}

static void soc_rpmh_master_print_one(struct sleep_stats *stat,const char *name)
{
	u64 accumulated = stat->accumulated;
	char stat_type[5] = {0};
	memcpy(stat_type, &stat->stat_type, sizeof(u32));
	/*
	 * If a subsystem is in sleep when reading the sleep stats adjust
	 * the accumulated sleep duration to show actual sleep time.
	 */
	if (stat->last_entered_at > stat->last_exited_at)
		accumulated += arch_timer_read_counter()
			- stat->last_entered_at;
	POWER_INFO("%s:%u:%llu\n", name, stat->count, accumulated);
	POWER_INFO("Last Entered/Exit:%llu/%llu\n", stat->last_entered_at, stat->last_exited_at);
}

void subsystem_sleep_stats_print(struct system_dbg_info *info)
{

	struct sleep_stats *stat = NULL;
    int i, idx;

	for (i = 0; i < ARRAY_SIZE(subsystems); i++) {
		if (subsystems[i].pid == SUBSYSTEM_STATS_OTHERS_NUM) {
			stat = sleep_stats_g;
			idx = subsystems[i].smem_item;
			if (NULL != ss_data && NULL != stat)
				memcpy_fromio(stat, ss_data->reg[idx], sizeof(*stat));
			else {
				stat = NULL;
                POWER_ERR("ss_data is null.");
            }
		} else {
			stat = (struct sleep_stats *) qcom_smem_get(
							subsystems[i].pid,
							subsystems[i].smem_item, NULL);
		}
		if (!IS_ERR_OR_NULL(stat))
				soc_rpmh_master_print_one(stat,
						subsystems[i].name);
		else
			POWER_ERR("subsystem %s stats %s.\n", subsystems[i].name, IS_ERR(stat)?"is not support":"is null");
	}

	return;
}

struct system_dbg_info subsystem_sleep_dbg_info = {
	.type = DEBUG_INFO_RPMH_SUBSYSTEM_STATS,
	.system_dbg_info_print = subsystem_sleep_stats_print,
};

/**
 * soc sleep stats info print
 *  on MSM8450
 */

#define STAT_TYPE_ADDR		0x0
#define COUNT_ADDR		0x4
#define LAST_ENTERED_AT_ADDR	0x8
#define LAST_EXITED_AT_ADDR	0x10
#define ACCUMULATED_ADDR	0x18
#define CLIENT_VOTES_ADDR	0x1c

struct stats_entry {
	uint32_t name;
	uint32_t count;
	uint64_t duration;
};


struct appended_stats {
	u32 client_votes;
	u32 reserved[3];
};

static struct stats_prv_data *soc_data;

static void soc_rpmh_dbg_info_print_one(struct sleep_stats *stat)
{
	u64 accumulated = stat->accumulated;
	char stat_type[5] = {0};
	memcpy(stat_type, &stat->stat_type, sizeof(u32));
	/*
	 * If a subsystem is in sleep when reading the sleep stats adjust
	 * the accumulated sleep duration to show actual sleep time.
	 */
	if (stat->last_entered_at > stat->last_exited_at)
		accumulated += arch_timer_read_counter()
			       - stat->last_entered_at;

	POWER_INFO("%d\n", stat_type);
	POWER_INFO("Count = %u\n", stat->count);
	POWER_INFO("Last Entered At = %llu\n", stat->last_entered_at);
	POWER_INFO("Last Exited At = %llu\n", stat->last_exited_at);
	POWER_INFO("Accumulated Duration = %llu\n", accumulated);
}


static void soc_sleep_stats_dbg_info_print(struct system_dbg_info *info)
{
	struct stats_prv_data *prv_data = soc_data;
	void __iomem *reg = prv_data->reg;
	struct sleep_stats stat;

	stat.count = readl_relaxed(reg + COUNT_ADDR);
	stat.last_entered_at = readq(reg + LAST_ENTERED_AT_ADDR);
	stat.last_exited_at = readq(reg + LAST_EXITED_AT_ADDR);
	stat.accumulated = readq(reg + ACCUMULATED_ADDR);

	soc_rpmh_dbg_info_print_one(&stat);

	if (prv_data->config->appended_stats_avail) {
		struct appended_stats app_stat;

		app_stat.client_votes = readl_relaxed(reg + CLIENT_VOTES_ADDR);
		POWER_INFO("Client_votes = %#x\n", app_stat.client_votes);
	}

	return;
}

void soc_sleep_stats_dbg_register(struct stats_prv_data *prv_data)
{
	if (prv_data == NULL)
	  return;

    soc_data = prv_data;

}
EXPORT_SYMBOL(soc_sleep_stats_dbg_register);

void subsystem_sleep_stats_dbg_register(struct sleep_stats_data *prv_data)
{
	if (prv_data == NULL)
		return;

    POWER_INFO("entry.");
    ss_data = prv_data;

}
EXPORT_SYMBOL(subsystem_sleep_stats_dbg_register);

void subsystem_sleep_stats_dbg_unregister(void)
{
    POWER_INFO("entry.");
    ss_data = NULL;
}
EXPORT_SYMBOL(subsystem_sleep_stats_dbg_unregister);

struct system_dbg_info soc_sleep_stats_dbg_info = {
	.type = DEBUG_INFO_RPMH_SOC_STATS,
	.system_dbg_info_print = soc_sleep_stats_dbg_info_print,
};


#if 0
/**
 * timerfd status info
 * get timerfd infos
 */

struct timerfd_ctx {
	union {
		struct hrtimer tmr;
		struct alarm alarm;
	} t;
	ktime_t tintv;
	ktime_t moffs;
	wait_queue_head_t wqh;
	u64 ticks;
	int clockid;
	short unsigned expired;
	short unsigned settime_flags;
	struct rcu_head rcu;
	struct list_head clist;
	spinlock_t cancel_lock;
	bool might_cancel;
};

static inline bool isalarm(struct timerfd_ctx *ctx)
{
	return ctx->clockid == CLOCK_REALTIME_ALARM ||
		ctx->clockid == CLOCK_BOOTTIME_ALARM;
}
#endif

static void power_debug_suspend_trace_probe(void *unused,const char *action, int val, bool start)
{
	POWER_DEBUG("register power suspend trace start=%d,val=%d \n", start,val);

	if (start && val > 0 && !strcmp("machine_suspend", action)) {
		POWER_INFO("soc_lpm debug infos :\n");

        soc_lpm_suspend_info();
	}
}

static ssize_t ddr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = sleep_stats_g;

    if (NULL != ss_data && NULL != stat) {
        memcpy_fromio(stat, ss_data->reg[DDR], sizeof(*stat));
    } else
        POWER_ERR("ss_data is null.");

    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "ddr", buf);
    else
        POWER_ERR("subsystem ddr stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(ddr);

static ssize_t cxsd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = sleep_stats_g;

    if (NULL != ss_data)
        memcpy_fromio(stat, ss_data->reg[CXSD], sizeof(*stat));
    else
        POWER_ERR("ss_data is null.");

    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "cxsd", buf);
    else
        POWER_ERR("subsystem cxsd stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(cxsd);

static ssize_t aosd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = sleep_stats_g;

    if (NULL != ss_data)
        memcpy_fromio(stat, ss_data->reg[AOSD], sizeof(*stat));
    else
        POWER_ERR("ss_data is null.");

    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "aosd", buf);
    else
        POWER_ERR("subsystem aosd stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(aosd);

static ssize_t slpi_island_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = NULL;

    stat = (struct sleep_stats *) qcom_smem_get(PID_SLPI, SLPI_ISLAND, NULL);
    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "slpi_island", buf);
    else
        POWER_ERR("subsystem slpi_island stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(slpi_island);

static ssize_t slpi_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = NULL;

    stat = (struct sleep_stats *) qcom_smem_get(PID_SLPI, SLPI, NULL);
    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "slpi", buf);
    else
        POWER_ERR("subsystem slpi stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(slpi);

static ssize_t cdsp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = NULL;

    stat = (struct sleep_stats *) qcom_smem_get(PID_CDSP, CDSP, NULL);
    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "cdsp", buf);
    else
        POWER_ERR("subsystem cdsp stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(cdsp);

static ssize_t adsp_island_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = NULL;

    stat = (struct sleep_stats *) qcom_smem_get(PID_ADSP, SLPI_ISLAND, NULL);
    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "adsp_island", buf);
    else
        POWER_ERR("subsystem adsp_island stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(adsp_island);

static ssize_t adsp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = NULL;

    stat = (struct sleep_stats *) qcom_smem_get(PID_ADSP, ADSP, NULL);
    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "adsp", buf);
    else
        POWER_ERR("subsystem adsp stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(adsp);

static ssize_t modem_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = NULL;

    stat = (struct sleep_stats *) qcom_smem_get(PID_MPSS, MPSS, NULL);
    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "modem", buf);
    else
        POWER_ERR("subsystem modem stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(modem);

static ssize_t apss_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    struct sleep_stats *stat = NULL;

    stat = (struct sleep_stats *) qcom_smem_get(PID_APSS, APSS, NULL);
    if (!IS_ERR_OR_NULL(stat))
        cnt += soc_rpmh_master_sysfs_print_one(stat, "apss", buf);
    else
        POWER_ERR("subsystem apss stats %s.\n", IS_ERR(stat)?"is not support":"is null");
    return cnt;
}
static DEVICE_ATTR_RO(apss);

static struct attribute *subsystem_sysfs[] = {
	&dev_attr_apss.attr,
	&dev_attr_modem.attr,
	&dev_attr_adsp.attr,
	&dev_attr_adsp_island.attr,
	&dev_attr_cdsp.attr,
	&dev_attr_slpi.attr,
	&dev_attr_slpi_island.attr,
	&dev_attr_aosd.attr,
	&dev_attr_cxsd.attr,
	&dev_attr_ddr.attr,
	NULL,
};

const struct attribute_group qcom_sleep_stats_sysfs_group = {
    .name = "qcom_sleep_stats",
	.attrs = subsystem_sysfs,
};

#define MI_POWER_DEVICE "mi_power_device"

#define POWERDEV_MAJOR  0
#define POWERDEV_MINOR  1

#define MI_POWER_MODULE "mi_power_module"

static struct power_info *mi_power;
struct file_operations power_ops = {
	.owner  = THIS_MODULE,
};

#if 0
static struct attribute *mi_power_sysfs[] = {
    NULL,
};
const struct attribute_group mi_power_attr_group = {
    .name = "mi_power",
	.attrs = mi_power_sysfs,
};
static struct kobject *mi_kobj;
static struct kobject *qcom_kobj;
int misysfs_init(void) {
	int error;

	mi_kobj = kobject_create_and_add("mi_power", NULL);
	if (!mi_kobj) {
		error = -ENOMEM;
		goto exit;
	}
	error = sysfs_create_group(mi_kobj, &mi_power_attr_group);
	if (error)
		goto kset_exit;

	return 0;

kset_exit:
	kobject_put(mi_kobj);
exit:
	return error;
}
static int qsysfs_init(void)
{
	int retval;

	qcom_kobj = kobject_create_and_add("qcom_sleep", mi_kobj);
	if (!qcom_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(qcom_kobj, &qcom_sleep_stats_sysfs_group);
	if (retval)
		kobject_put(qcom_kobj);

	return retval;
}

static int __init pm_debug_init(void)
{
    int ret = 0;

    ret = misysfs_init();
    if (ret)
        return ret;

    /*ret = qsysfs_init();*/
    /*if (ret)*/
        /*return ret;*/

    return ret;
}
#else
static int __init pm_debug_init(void)
{
	int ret = 0;
	int i;

	mi_power = kzalloc(sizeof(struct power_info), GFP_KERNEL);
    if(!mi_power) {
        return -ENOMEM;
    }

	sleep_stats_g = kzalloc(sizeof(struct sleep_stats), GFP_KERNEL);
    if (!sleep_stats_g) {
        ret=-ENOMEM;
        goto sleep_stats_err;
    }

	mi_power->power_class = class_create(THIS_MODULE, "power_debug");
	if (IS_ERR(power_debug_class)){
		POWER_ERR("failed to create power debug class");
        goto register_class_err;
	}

	for (i = 0; power_debug_attrs[i].attr.name!= NULL; i++){
		ret = class_create_file(mi_power->power_class, &power_debug_attrs[i]);
		if (ret != 0){
			POWER_ERR("failed to create attribute file");
			goto class_unregister;
		}
	}

	register_syscore_ops(&pm_debug_ops);

    pm_register_system_dbg_info(&subsystem_sleep_dbg_info);
    /*pm_register_system_dbg_info(&soc_sleep_stats_dbg_info);*/
	/*pm_register_system_dbg_info(&gpio_system_dbg_info);*/
    /*pm_register_wakeup_device(&gic_wakeup_device);*/
    /*pm_register_wakeup_device(&msm_pinctrl_wakeup_device);*/

   register_trace_suspend_resume(power_debug_suspend_trace_probe, NULL);

	mi_power->major = register_chrdev(POWERDEV_MAJOR, MI_POWER_MODULE, &power_ops);
	if (mi_power->major < 0) {
		ret = mi_power->major;
		POWER_ERR("mi power info chrdev creation failed (err = %d)\n", ret);
	}
	mi_power->power_dev = device_create(mi_power->power_class, NULL, MKDEV(mi_power->major, POWERDEV_MINOR), NULL, MI_POWER_DEVICE);
	if (IS_ERR(mi_power->power_dev)) {
		ret = -ENODEV;
		POWER_ERR("mi power info device creation failed (err = %d)\n", ret);
	}

   ret = sysfs_create_group(&mi_power->power_dev->kobj, &qcom_sleep_stats_sysfs_group);
   if (ret) {
       POWER_ERR("sysfs_create_group failed.\n");
   }

   POWER_INFO("register sysfs succecss.\n");

	return ret;

class_unregister:
    class_destroy(power_debug_class);
sleep_stats_err:
register_class_err:
    kfree(mi_power);

    return ret;
}
#endif
static void __exit pm_debug_exit()
{
	int ret = 0;

	ret = unregister_trace_suspend_resume(power_debug_suspend_trace_probe, NULL);
}

MODULE_LICENSE("GPL");
core_initcall(pm_debug_init);
module_exit(pm_debug_exit);
