#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/spinlock.h>
#include <soc/qcom/dcvs.h>
#include <soc/qcom/pmu_lib.h>
#include <linux/sched/clock.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/rwlock.h>
#include <linux/soc/qcom/qcom_aoss.h>
#include <linux/cpufreq.h>
#include <linux/sched/cpufreq.h>

#define MAX_MEMLAT_GRPS NUM_DCVS_HW_TYPES

enum common_ev_idx {
        INST_IDX,
        CYC_IDX,
        FE_STALL_IDX,
        BE_STALL_IDX,
        NUM_COMMON_EVS
};

enum grp_ev_idx {
        MISS_IDX,
        WB_IDX,
        ACC_IDX,
        NUM_GRP_EVS
};

struct cpu_ctrs {
        u64                             common_ctrs[NUM_COMMON_EVS];
        u64                             grp_ctrs[MAX_MEMLAT_GRPS][NUM_GRP_EVS];
};

struct cpu_stats {
        struct cpu_ctrs                 prev;
        struct cpu_ctrs                 curr;
        struct cpu_ctrs                 delta;
	struct cpu_ctrs                 delta_pre;
        struct qcom_pmu_data            raw_ctrs;
        bool                            idle_sample;
        ktime_t                         sample_ts;
        ktime_t                         last_sample_ts;
        spinlock_t                      ctrs_lock;
        u32                             freq_mhz;
        u32                             fe_stall_pct;
        u32                             be_stall_pct;
        u32                             ipm[MAX_MEMLAT_GRPS];
        u32                             wb_pct[MAX_MEMLAT_GRPS];
};

struct cpufreq_memfreq_map {
 	unsigned int			cpufreq_mhz;
 	unsigned int			memfreq_khz;
};

static bool is_dcvs_arbi_enable = false;
static bool is_dcvs_arbi_partial_on = false;
static bool is_change_right = false;
static unsigned int ddr_freq_table[20]; // 最多20个频点
static unsigned int freq_table_size = 0;
static struct kobject *cpu_kobj;
static struct kobject *dcvs_arbi_kobj;
static struct kobject *ddr_kobj;
static bool is_down = false;
static bool is_up = false;
static bool is_second = true;
static int is_test = 0;//22->ib1,-22->ib0,33->freq,66->revert
static bool is_partial_on = false;
static u32 map_freq[2] = {0,0};

static u64 pre_be_stall;
static u64 pre_cyc;
static u32 pre_be_stall_pct;
static u32 pre_ipm[MAX_MEMLAT_GRPS];

static bool is_be_stall = false;
static bool is_be_ok = false;
static bool is_ipm[MAX_MEMLAT_GRPS] = {false};
static bool is_wb[MAX_MEMLAT_GRPS] = {false};

#define FLOOR_FREQ  600000
#define CEIL_FREQ   3000000
#define QUIT_FREQ   1500000

static ssize_t dcvs_arbi_enable_show(struct kobject *kobj,
                                    struct kobj_attribute *attr,
                                    char *buf)
{
	if (is_dcvs_arbi_enable)
		return scnprintf(buf, PAGE_SIZE, "%u\n", 1);
	else
		return scnprintf(buf, PAGE_SIZE, "%u\n", 0);
}

static ssize_t dcvs_arbi_enable_store(struct kobject *kobj,
                                     struct kobj_attribute *attr,
                                     const char *buf, size_t count)
{
	int ret;

	ret = kstrtobool(buf, &is_dcvs_arbi_enable);
        if (ret < 0)
                return ret;

    return count;
}

static struct kobj_attribute dcvs_arbi_enable_attr =
    __ATTR(dcvs_arbi_enable, 0664, dcvs_arbi_enable_show, dcvs_arbi_enable_store);

static ssize_t dcvs_arbi_second_show(struct kobject *kobj,
                                    struct kobj_attribute *attr,
                                    char *buf)
{
        if (is_second)
                return scnprintf(buf, PAGE_SIZE, "%u\n", 1);
        else
                return scnprintf(buf, PAGE_SIZE, "%u\n", 0);
}

static ssize_t dcvs_arbi_second_store(struct kobject *kobj,
                                     struct kobj_attribute *attr,
                                     const char *buf, size_t count)
{
        int ret;
	int value;

	ret = kstrtoint(buf, 10, &value);
        if (ret < 0)
                return ret;
	if (value == 1)
		is_second = true;
	else if (value == 0)
		is_second = false;
	else
		is_test = value;

	return count;
}

static struct kobj_attribute dcvs_arbi_second_attr =
    __ATTR(dcvs_arbi_second, 0664, dcvs_arbi_second_show, dcvs_arbi_second_store);

static ssize_t dcvs_arbi_partial_on_show(struct kobject *kobj,
                                    struct kobj_attribute *attr,
                                    char *buf)
{
        if (is_partial_on)
                return scnprintf(buf, PAGE_SIZE, "%u\n", 1);
        else
                return scnprintf(buf, PAGE_SIZE, "%u\n", 0);
}

static ssize_t dcvs_arbi_partial_on_store(struct kobject *kobj,
                                     struct kobj_attribute *attr,
                                     const char *buf, size_t count)
{
        int ret;

        ret = kstrtobool(buf, &is_partial_on);
	is_dcvs_arbi_partial_on = is_partial_on;
        if (ret < 0)
                return ret;

    return count;
}

static struct kobj_attribute dcvs_arbi_partial_on_attr =
    __ATTR(dcvs_arbi_partial_on, 0664, dcvs_arbi_partial_on_show, dcvs_arbi_partial_on_store);

bool dcvs_arbi_get_enable_status(void)
{
    return is_dcvs_arbi_enable;
}
EXPORT_SYMBOL(dcvs_arbi_get_enable_status);

void mi_record_pre_sampling_stats(struct cpu_stats *stats)
{
	if (!is_dcvs_arbi_enable)
                return;
	memcpy(&stats->delta_pre, &stats->delta, sizeof(stats->delta));
}
EXPORT_SYMBOL(mi_record_pre_sampling_stats);

void mi_calculate_sampling_stats(struct cpu_stats *stats)
{
	int grp;
	struct cpu_ctrs *delta;

        if (!is_dcvs_arbi_enable)
                return;
        if (!is_second)
                return;

	delta = &stats->delta_pre;
	pre_be_stall = delta->common_ctrs[BE_STALL_IDX];
        pre_cyc = delta->common_ctrs[CYC_IDX];
        pre_be_stall_pct = mult_frac(100,
                                    pre_be_stall, pre_cyc);
        for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
            if (grp != DCVS_DDR)
               continue;
            pre_ipm[grp] = stats->ipm[grp];
            break;
        }
	if (stats->be_stall_pct >= pre_be_stall_pct) {
		if (mult_frac(100,stats->be_stall_pct - pre_be_stall_pct, pre_be_stall_pct) > 10)//threshold 10%
			is_be_stall = true;
		else {
			is_be_stall = false;
			is_be_ok = false;
		}
	}
	if (stats->be_stall_pct < pre_be_stall_pct) {
		 if (mult_frac(100, pre_be_stall_pct - stats->be_stall_pct, pre_be_stall_pct) > 10)
			 is_be_ok = true;
		 else {
			 is_be_stall = false;
			 is_be_ok = false;
		 }
	}
        for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
	    if (grp != DCVS_DDR)
		    continue;
	    if (stats->ipm[grp] < pre_ipm[grp]) {
		    if (mult_frac(100,pre_ipm[grp]-stats->ipm[grp],pre_ipm[grp]) > 10)//threshold 10%
	                    is_ipm[grp] = true;
	            else
			    is_ipm[grp] = false;
	    }
	    if (stats->ipm[grp] >= pre_ipm[grp]) {
		    is_ipm[grp] = false;
	    }
	    if (stats->wb_pct[grp] > 40)//threshold 40%
	            is_wb[grp] = true;
	    else
		    is_wb[grp] = false;
	    break;
        }
}
EXPORT_SYMBOL(mi_calculate_sampling_stats);

u32 mi_recalculate_freq_mhz(struct cpu_ctrs *delta, s64 delta_us, u32 freq_mhz)
{
	if (!is_dcvs_arbi_enable)
		return freq_mhz;
	if (!is_second)
		return freq_mhz;

	return (delta->common_ctrs[CYC_IDX]-delta->common_ctrs[BE_STALL_IDX]) / delta_us;
}
EXPORT_SYMBOL(mi_recalculate_freq_mhz);

void mi_cpufreq_to_memfreq(struct cpufreq_memfreq_map *map, u32 cpu_mhz)
{
	struct cpufreq_memfreq_map *tmp;
	u32 diff_pre, diff_after;

        if (!is_dcvs_arbi_enable)
                return;
	if (!is_second || is_dcvs_arbi_partial_on)
		return;

	if (map && map->memfreq_khz > CEIL_FREQ) {
		tmp = map - 1;
		diff_pre = cpu_mhz - tmp->cpufreq_mhz;
		diff_after = map->cpufreq_mhz - cpu_mhz;
		if (diff_pre <= diff_after)
			map--;
	}
	if (is_down) {
                map--;
                is_down = false;
        }
	if (is_up) {
		map++;
		is_up = false;
	}
}
EXPORT_SYMBOL(mi_cpufreq_to_memfreq);

void mi_calculate_mon_sampling_freq(u32 hw_type, struct cpu_stats *stats, int max_cpu)
{
        #if 0
	u32 memfreq;
        u32 diff_pre, diff_after;
	int j = 0;
        #endif

	if (!is_dcvs_arbi_enable)
                return;
	if (!is_second)
                return;
        if (hw_type != DCVS_DDR)
                return;

        if (stats->delta.common_ctrs[INST_IDX] > (stats->delta_pre.common_ctrs[INST_IDX] * 2)
			&& (stats->delta.grp_ctrs[hw_type][MISS_IDX] * 2) < stats->delta_pre.grp_ctrs[hw_type][MISS_IDX])
		is_down = true;
	if ((stats->delta.common_ctrs[BE_STALL_IDX] * 2) < stats->delta_pre.common_ctrs[BE_STALL_IDX]
                        || stats->delta.grp_ctrs[hw_type][WB_IDX] > (stats->delta_pre.grp_ctrs[hw_type][WB_IDX] * 10))
                is_down = true;

        if ((stats->delta_pre.grp_ctrs[hw_type][MISS_IDX] * 2) < stats->delta.grp_ctrs[hw_type][MISS_IDX]
			|| (stats->delta_pre.common_ctrs[BE_STALL_IDX] * 2) < stats->delta.common_ctrs[BE_STALL_IDX])
                is_up = true;

	if (max_cpu < 6)
		map_freq[0] = stats->freq_mhz;
	else
		map_freq[1] = stats->freq_mhz;

        #if 0
	if (is_dcvs_arbi_partial_on)
		return;
	for (int i = 0; i < freq_table_size; i++) {
               if (max_memfreq > ddr_freq_table[i])
		       continue;
	       if (i == 0 || i == (freq_table_size - 1))
		       return max_memfreq;
	       if ((is_be_stall && is_ipm[hw_type]) || is_wb[hw_type]) {
		       memfreq = ddr_freq_table[i++];
		       return memfreq;
	       }
	       diff_pre = max_memfreq - ddr_freq_table[i-1];
	       diff_after = ddr_freq_table[i] - max_memfreq;
	       j = (diff_pre < diff_after) ? (i-1) : i;
	       if (is_be_stall)
		       memfreq = ddr_freq_table[j+1];
	       else if (is_be_ok) {
		       int m = 0;
		       m = (j == 0) ? j : (j-1);
		       memfreq = ddr_freq_table[m];
	       } else {
		       memfreq = ddr_freq_table[i];
	       }
	       return memfreq;
        }
	return max_memfreq;
        #endif
}
EXPORT_SYMBOL(mi_calculate_mon_sampling_freq);

void mi_update_memlat_fp_vote(u32 max_freqs, int hw_type)
{
	if (!is_dcvs_arbi_enable)
		return;
	if (!is_second)
                return;
	if (hw_type != DCVS_DDR)
		return;

	if (!is_change_right && max_freqs < CEIL_FREQ && is_test == 33)
	{
		max_freqs = CEIL_FREQ;
		stat_data(max_freqs/1000);
		//pr_err("fp_vote max_freqs = %d\n", max_freqs/1000);
		is_ready_send = true;
		is_change_right = true;
		wake_up_interruptible(&mi_wait_queue);
	}
}
EXPORT_SYMBOL(mi_update_memlat_fp_vote);

void mi_update_memlat_fp_vote_revert(u32 max_freqs, int hw_type)
{
	if (hw_type != DCVS_DDR)
		return;
	if (!is_change_right)
		return;

	if (is_dcvs_arbi_enable && is_test == 66) {
		is_change_right = false;
                stat_data(0);
		is_ready_send = true;
                wake_up_interruptible(&mi_wait_queue);
	}
	if (!is_dcvs_arbi_enable || !is_second) {
		is_change_right = false;
		stat_data(0);
		is_ready_send = true;
                wake_up_interruptible(&mi_wait_queue);
	}
}
EXPORT_SYMBOL(mi_update_memlat_fp_vote_revert);

u32 update_new_freq(int hw_type, u32 ib)
{
	if (!is_dcvs_arbi_enable || hw_type != DCVS_DDRQOS)
                return ib;
	if (is_test == 22)
		return 1;
	else if (is_test == -22)
		return 0;
	if (is_second && is_test == 33)
		return 0;
	return ib;
}
EXPORT_SYMBOL(update_new_freq);

static int parse_ddr_freq_table(struct device_node *node)
{
    struct device_node *freq_node;
    const unsigned int *prop;
    int len, i;

    freq_node = of_parse_phandle(node, "qcom,freq-tbl", 0);
    if (!freq_node) {
        pr_err("Failed to find qcom,freq-tbl node\n");
        return -ENODEV;
    }

    prop = of_get_property(freq_node, "qcom,freq-tbl", &len);
    if (!prop) {
        pr_err("Failed to get qcom,freq-tbl property\n");
        of_node_put(freq_node);
        return -ENODATA;
    }

    freq_table_size = len / sizeof(unsigned int);
    if (freq_table_size > ARRAY_SIZE(ddr_freq_table)) {
        pr_warn("Frequency table too large, truncating to %zu entries\n",
               ARRAY_SIZE(ddr_freq_table));
        freq_table_size = ARRAY_SIZE(ddr_freq_table);
    }

    memset(ddr_freq_table, 0, sizeof(ddr_freq_table));
    for (i = 0; i < freq_table_size; i++) {
        ddr_freq_table[i] = prop[i];
        pr_info("DDR freq[%d]: %u Hz\n", i, ddr_freq_table[i]);
    }

    of_node_put(freq_node);
    return 0;
}

static int find_and_parse_dcvs_node(void)
{
    struct device_node *node;
    int ret = -ENODEV;

    node = of_find_compatible_node(NULL, NULL, "qcom,dcvs");
    if (!node) {
        pr_err("Failed to find qcom,dcvs node\n");
        return ret;
    }

    node = of_get_child_by_name(node, "ddr");
    if (!node) {
        pr_err("Failed to find ddr node under qcom,dcvs\n");
        return ret;
    }

    if (!of_device_is_compatible(node, "qcom,dcvs-hw")) {
        pr_err("ddr node is not dcvs-hw compatible\n");
        goto out;
    }

    ret = parse_ddr_freq_table(node);

out:
    of_node_put(node);
    return ret;
}

#if 0
static struct kobject *find_child_kobj(struct kobject *parent, const char *name) {
    struct kobject *child = NULL;
    struct kernfs_node *kn;

    if (!parent || !name) return NULL;

    spin_lock(&sysfs_lock);
    kn = parent->sd;
    if (kn) {
        kn = kernfs_find_and_get(kn, name);
        if (kn) {
            child = kn->priv;
            kobject_get(child);
            kernfs_put(kn);
        }
    }
    spin_unlock(&sysfs_lock);

    return child;
}
#endif

static int __init dcvs_arbi_init(void)
{
    int ret;
    struct device *dev_root;

    dev_root = bus_get_dev_root(&cpu_subsys);
    if (!dev_root) {
        pr_err("Cannot get cpu subsystem dev root\n");
        return -ENODEV;
    }
    if (dev_root)
	    cpu_kobj = &dev_root->kobj;
    if (!cpu_kobj || !cpu_kobj->sd) {
        pr_err("Invalid cpu kobject\n");
        put_device(dev_root);
        return -ENODEV;
    }
#if 0
    bus_dcvs_kobj = find_child_kobj(cpu_kobj, "bus_dcvs");
    if (!bus_dcvs_kobj)
	    goto err_cpu;

    ddr_kobj = find_child_kobj(bus_dcvs_kobj, "DDR");
    if (!ddr_kobj)
	    goto err_bus;

    target_kobj = find_child_kobj(ddr_kobj, "memlat");
    if (!target_kobj)
	    goto err_ddr;
#endif
    dcvs_arbi_kobj = kobject_create_and_add("dcvs_arbi", cpu_kobj);
    if (!dcvs_arbi_kobj) {
        pr_err("Failed to create dcvs_arbi kobject\n");
        ret = -ENOMEM;
        goto err_cpu_kobj;
    }

    ddr_kobj = kobject_create_and_add("ddr", dcvs_arbi_kobj);
    if (!ddr_kobj) {
        pr_err("Failed to create ddr kobject\n");
        ret = -ENOMEM;
        goto err_dcvs_arbi_kobj;
    }

    ret = sysfs_create_file(ddr_kobj, &dcvs_arbi_enable_attr.attr);
    if (ret) {
        pr_err("Failed to create dcvs_arbi_enable attribute\n");
        goto err_ddr_kobj;
    }

    ret = sysfs_create_file(ddr_kobj, &dcvs_arbi_second_attr.attr);
    if (ret) {
        pr_err("Failed to create dcvs_arbi_second attribute\n");
        goto err_ddr_kobj;
    }

    ret = sysfs_create_file(ddr_kobj, &dcvs_arbi_partial_on_attr.attr);
    if (ret) {
        pr_err("Failed to create dcvs_arbi_partial_on attribute\n");
        goto err_ddr_kobj;
    }

    ret = find_and_parse_dcvs_node();
    if (ret) {
        pr_err("Failed to parse DDR frequency table\n");
        goto err_sysfs_file;
    }

    pr_info("DCVS Arbi Initialized\n");
    return 0;

err_sysfs_file:
    sysfs_remove_file(ddr_kobj, &dcvs_arbi_enable_attr.attr);
    sysfs_remove_file(ddr_kobj, &dcvs_arbi_partial_on_attr.attr);
    sysfs_remove_file(ddr_kobj, &dcvs_arbi_second_attr.attr);
err_ddr_kobj:
    kobject_put(ddr_kobj);
err_dcvs_arbi_kobj:
    kobject_put(dcvs_arbi_kobj);
err_cpu_kobj:
    put_device(dev_root);
    return ret;
}

static void __exit dcvs_arbi_exit(void)
{
    sysfs_remove_file(ddr_kobj, &dcvs_arbi_enable_attr.attr);
    sysfs_remove_file(ddr_kobj, &dcvs_arbi_second_attr.attr);
    sysfs_remove_file(ddr_kobj, &dcvs_arbi_partial_on_attr.attr);

    if (ddr_kobj) {
        kobject_put(ddr_kobj);
        ddr_kobj = NULL;
    }
    if (dcvs_arbi_kobj) {
        kobject_put(dcvs_arbi_kobj);
        dcvs_arbi_kobj = NULL;
    }
    if (cpu_kobj) {
        struct device *dev_root = container_of(cpu_kobj, struct device, kobj);
        put_device(dev_root);
        cpu_kobj = NULL;
    }
    pr_info("DCVS Arbi Unloaded\n");
}

module_init(dcvs_arbi_init);
module_exit(dcvs_arbi_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dcvs Arbi Moudle");
