#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/bug.h>
#include <linux/workqueue.h>

#include <mach/mtk_thermal_platform.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_storage_logger.h>
#include <mach/mtk_mdm_monitor.h>

#include <mach/mt_thermal.h>

#include <linux/aee.h>

#include <linux/mtk_gpu_utility.h>

//************************************
// Definition
//************************************

/* Number of CPU CORE */
#define NUMBER_OF_CORE (8)

#if defined(CONFIG_MTK_SMART_BATTERY)
// global variable from battery driver...
extern kal_bool gFG_Is_Charging;
#endif

extern unsigned int mt_gpufreq_get_cur_freq(void);

// This function pointer is for GPU LKM to register a function to get GPU loading.
unsigned long (*mtk_thermal_get_gpu_loading_fp)(void) = NULL;


//************************************
// Global Variable
//************************************
static bool enable_ThermalMonitorXlog = false;

static DEFINE_MUTEX(MTM_SYSINFO_LOCK);

//************************************
//  Macro
//************************************
#define THRML_LOG(fmt, args...) \
    do { \
        if (enable_ThermalMonitorXlog) { \
            xlog_printk(ANDROID_LOG_INFO, "THERMAL/PLATFORM", fmt, ##args); \
        } \
    } while(0)


#define THRML_ERROR_LOG(fmt, args...) \
    do { \
        xlog_printk(ANDROID_LOG_INFO, "THERMAL/PLATFORM", fmt, ##args); \
    } while(0)


//************************************
//  Define
//************************************

//*********************************************
// System Information Monitor
//*********************************************
static mm_segment_t oldfs;

/*
 *  Read Battery Information.
 *
 *  "cat /sys/devices/platform/mt6575-battery/FG_Battery_CurrentConsumption"
 *  "cat /sys/class/power_supply/battery/batt_vol"
 *  "cat /sys/class/power_supply/battery/batt_temp"
 */
static int get_sys_battery_info(char* dev)
{
    int fd;
    int nRet;
    int nReadSize;
    char *pvalue = NULL;
    char buf[64];

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    fd = sys_open(dev, O_RDONLY, 0);
    if (fd < 0)
    {
        THRML_LOG("[get_sys_battery_info] open fail dev:%s fd:%d \n", dev, fd);
        set_fs(oldfs);
        return fd;
    }

    nReadSize = sys_read(fd, buf, sizeof(buf) - 1);
    THRML_LOG("[get_sys_battery_info] nReadSize:%d\n", nReadSize);
    nRet = simple_strtol(buf, &pvalue, 10);

    set_fs(oldfs);
    sys_close(fd);

    return nRet;
}

//*********************************************
// Get Wifi Tx throughput
//*********************************************
static int get_sys_wifi_throughput(char* dev, int nRetryNr)
{
    int fd;
    int nRet;
    int nReadSize;
    int nRetryCnt=0;
    char *pvalue = NULL;
    char buf[64];

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    /* If sys_open fail, it will retry "nRetryNr" times. */
    do {
        fd = sys_open(dev, O_RDONLY, 0);
        if(nRetryCnt > nRetryNr) {
            THRML_LOG("[get_sys_wifi_throughput] open fail dev:%s fd:%d \n", dev, fd);
            set_fs(oldfs);
            return fd;
        }
        nRetryCnt++;
    } while(fd < 0);

    if(nRetryCnt > 1) {
       THRML_LOG("[get_sys_wifi_throughput] open fail nRetryCnt:%d \n", nRetryCnt);
    }

    nReadSize = sys_read(fd, buf, sizeof(buf) - 1);
    THRML_LOG("[get_sys_wifi_throughput] nReadSize:%d\n", nReadSize);
    nRet = simple_strtol(buf, &pvalue, 10);

    set_fs(oldfs);
    sys_close(fd);

    return nRet;
}

//*********************************************
// For get_sys_cpu_usage_info_ex()
//*********************************************

#define CPU_USAGE_CURRENT_FIELD (0)
#define CPU_USAGE_SAVE_FIELD    (1)
#define CPU_USAGE_FRAME_FIELD   (2)

struct cpu_index_st
{
    unsigned long  u[3];
    unsigned long  s[3];
    unsigned long  n[3];
    unsigned long  i[3];
    unsigned long  w[3];
    unsigned long  q[3];
    unsigned long  sq[3];
    unsigned long  tot_frme;
    unsigned long  tz;
    int  usage;
    int  freq;
};

struct gpu_index_st
{
    int  usage;
    int  freq;
};

#define NO_CPU_CORES (8)
static struct cpu_index_st cpu_index_list[NO_CPU_CORES];   ///< 4-Core is maximum
static int cpufreqs[NO_CPU_CORES];
static int cpuloadings[NO_CPU_CORES];

#define SEEK_BUFF(x, c)  while(*x != c)x++; \
                            x++;

#define TRIMz_ex(tz, x)   ((tz = (unsigned long long)(x)) < 0 ? 0 : tz)

//*********************************************
// CPU Index
//*********************************************
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <asm/cputime.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/time.h>

#ifdef arch_idle_time

static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}

#endif

static int get_sys_cpu_usage_info_ex(void)
{
    int nCoreIndex = 0, i;

    for (i = 0; i < NO_CPU_CORES; i ++)
        cpuloadings[i] = 0;

	for_each_online_cpu(nCoreIndex) {

		/* Get CPU Info */
		cpu_index_list[nCoreIndex].u[CPU_USAGE_CURRENT_FIELD] = kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_USER];
        cpu_index_list[nCoreIndex].n[CPU_USAGE_CURRENT_FIELD] = kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_NICE];
        cpu_index_list[nCoreIndex].s[CPU_USAGE_CURRENT_FIELD] = kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_SYSTEM];
        cpu_index_list[nCoreIndex].i[CPU_USAGE_CURRENT_FIELD] = get_idle_time(nCoreIndex);
        cpu_index_list[nCoreIndex].w[CPU_USAGE_CURRENT_FIELD] = get_iowait_time(nCoreIndex);
        cpu_index_list[nCoreIndex].q[CPU_USAGE_CURRENT_FIELD] = kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_IRQ];
        cpu_index_list[nCoreIndex].sq[CPU_USAGE_CURRENT_FIELD] = kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_SOFTIRQ];

		/* Frame */
	    cpu_index_list[nCoreIndex].u[CPU_USAGE_FRAME_FIELD] = cpu_index_list[nCoreIndex].u[CPU_USAGE_CURRENT_FIELD] -
                                                                cpu_index_list[nCoreIndex].u[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].n[CPU_USAGE_FRAME_FIELD] = cpu_index_list[nCoreIndex].n[CPU_USAGE_CURRENT_FIELD] -
                                                                cpu_index_list[nCoreIndex].n[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].s[CPU_USAGE_FRAME_FIELD] = cpu_index_list[nCoreIndex].s[CPU_USAGE_CURRENT_FIELD] -
                                                                cpu_index_list[nCoreIndex].s[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].i[CPU_USAGE_FRAME_FIELD] = TRIMz_ex(cpu_index_list[nCoreIndex].tz,
                                                                (cpu_index_list[nCoreIndex].i[CPU_USAGE_CURRENT_FIELD] -
                                                                cpu_index_list[nCoreIndex].i[CPU_USAGE_SAVE_FIELD])) ;
		cpu_index_list[nCoreIndex].w[CPU_USAGE_FRAME_FIELD] = cpu_index_list[nCoreIndex].w[CPU_USAGE_CURRENT_FIELD] -
                                                                cpu_index_list[nCoreIndex].w[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].q[CPU_USAGE_FRAME_FIELD] = cpu_index_list[nCoreIndex].q[CPU_USAGE_CURRENT_FIELD] -
                                                                cpu_index_list[nCoreIndex].q[CPU_USAGE_SAVE_FIELD] ;
		cpu_index_list[nCoreIndex].sq[CPU_USAGE_FRAME_FIELD] = cpu_index_list[nCoreIndex].sq[CPU_USAGE_CURRENT_FIELD] -
                                                                cpu_index_list[nCoreIndex].sq[CPU_USAGE_SAVE_FIELD];

		/* Total Frame */
		cpu_index_list[nCoreIndex].tot_frme = cpu_index_list[nCoreIndex].u[CPU_USAGE_FRAME_FIELD] +
											  cpu_index_list[nCoreIndex].n[CPU_USAGE_FRAME_FIELD] +
											  cpu_index_list[nCoreIndex].s[CPU_USAGE_FRAME_FIELD] +
											  cpu_index_list[nCoreIndex].i[CPU_USAGE_FRAME_FIELD] +
											  cpu_index_list[nCoreIndex].w[CPU_USAGE_FRAME_FIELD] +
											  cpu_index_list[nCoreIndex].q[CPU_USAGE_FRAME_FIELD] +
											  cpu_index_list[nCoreIndex].sq[CPU_USAGE_FRAME_FIELD];

		/* CPU Usage */
		if (cpu_index_list[nCoreIndex].tot_frme > 0) {
			cpuloadings[nCoreIndex] = (100-(((int)cpu_index_list[nCoreIndex].i[CPU_USAGE_FRAME_FIELD]*100)/(int)cpu_index_list[nCoreIndex].tot_frme));
		} else {
			/* CPU unplug case */
			cpuloadings[nCoreIndex] = 0;
		}

		cpu_index_list[nCoreIndex].u[CPU_USAGE_SAVE_FIELD]  = cpu_index_list[nCoreIndex].u[CPU_USAGE_CURRENT_FIELD];
	    cpu_index_list[nCoreIndex].n[CPU_USAGE_SAVE_FIELD]  = cpu_index_list[nCoreIndex].n[CPU_USAGE_CURRENT_FIELD];
	    cpu_index_list[nCoreIndex].s[CPU_USAGE_SAVE_FIELD]  = cpu_index_list[nCoreIndex].s[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].i[CPU_USAGE_SAVE_FIELD]  = cpu_index_list[nCoreIndex].i[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].w[CPU_USAGE_SAVE_FIELD]  = cpu_index_list[nCoreIndex].w[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].q[CPU_USAGE_SAVE_FIELD]  = cpu_index_list[nCoreIndex].q[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].sq[CPU_USAGE_SAVE_FIELD] = cpu_index_list[nCoreIndex].sq[CPU_USAGE_CURRENT_FIELD];

		THRML_LOG("CPU%d Frame:%d USAGE:%d\n", nCoreIndex, cpu_index_list[nCoreIndex].tot_frme, cpuloadings[nCoreIndex]);

        for (i=0 ; i<3 ; i++) {
            THRML_LOG("Index %d [u:%d] [n:%d] [s:%d] [i:%d] [w:%d] [q:%d] [sq:%d] \n",
                      i,
                      cpu_index_list[nCoreIndex].u[i],
                      cpu_index_list[nCoreIndex].n[i],
                      cpu_index_list[nCoreIndex].s[i],
                      cpu_index_list[nCoreIndex].i[i],
                      cpu_index_list[nCoreIndex].w[i],
                      cpu_index_list[nCoreIndex].q[i],
                      cpu_index_list[nCoreIndex].sq[i]);

        }
	}

    return 0;

}

extern int mtktscpu_limited_dmips;
static bool dmips_limit_warned = false;
static int check_dmips_limit = 0;

#include <linux/cpufreq.h>
static int get_sys_all_cpu_freq_info(void)
{
    int i;
    int cpu_total_dmips = 0;

    for (i=0 ; i<NO_CPU_CORES ; i++)
    {
        cpufreqs[i] = cpufreq_quick_get(i)/1000; // MHz
        cpu_total_dmips += cpufreqs[i];
    }

    cpu_total_dmips /= 1000;
    // TODO: think a way to easy start and stop, and start for only once
    if (1 == check_dmips_limit)
    {
        if (cpu_total_dmips > mtktscpu_limited_dmips)
        {
            THRML_ERROR_LOG("cpu %d over limit %d\n", cpu_total_dmips, mtktscpu_limited_dmips);
            if (dmips_limit_warned == false)
            {
                aee_kernel_warning("thermal", "cpu %d over limit %d\n", cpu_total_dmips, mtktscpu_limited_dmips);
                dmips_limit_warned = true;
            }
        }
    }

    return 0;
}

static int mtk_thermal_validation_rd(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", check_dmips_limit);

	return 0;
}

static ssize_t mtk_thermal_validation_wr(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int check_switch;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
	{
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d", &check_switch) == 1)
	{
	    if (1 == check_switch)
	    {
	        dmips_limit_warned = false;
		    check_dmips_limit = check_switch;
		}
		else if (0 == check_switch)
		{
		    check_dmips_limit = check_switch;
		}
		return count;
	}
	else
	{
		THRML_ERROR_LOG("[mtk_thermal_validation_wr] bad argument\n");
	}
	return -EINVAL;
}

static int mtk_thermal_validation_open(struct inode *inode, struct file *file)
{
    return single_open(file, mtk_thermal_validation_rd, NULL);
}

static const struct file_operations mtk_thermal_validation_fops = {
    .owner = THIS_MODULE,
    .open = mtk_thermal_validation_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mtk_thermal_validation_wr,
    .release = single_release,
};


/* Init */
static int __init mtk_thermal_platform_init(void)
{
    int err = 0;
    struct proc_dir_entry *entry;
    entry = proc_create("driver/tm_validation", S_IRUGO | S_IWUSR, NULL, &mtk_thermal_validation_fops);
    if (!entry)
    {
        THRML_ERROR_LOG("[mtk_thermal_platform_init] Can not create /proc/driver/tm_validation\n");
    }

    return err;
}

/* Exit */
static void __exit mtk_thermal_platform_exit(void)
{

}

int mtk_thermal_get_cpu_info(
    int *nocores,
    int **cpufreq,
    int **cpuloading)
{
    //******************
    // CPU Usage
    //******************
    mutex_lock(&MTM_SYSINFO_LOCK);

    /* Read CPU Usage Information */
    get_sys_cpu_usage_info_ex();

    get_sys_all_cpu_freq_info();

    mutex_unlock(&MTM_SYSINFO_LOCK);

    if (nocores)
        *nocores = NO_CPU_CORES;

    if (cpufreq)
        *cpufreq = cpufreqs;

    if (cpuloading)
        *cpuloading = cpuloadings;

    return 0;
}

#define NO_GPU_CORES (1)
static int gpufreqs[NO_GPU_CORES];
static int gpuloadings[NO_GPU_CORES];

int mtk_thermal_get_gpu_info(
    int *nocores,
    int **gpufreq,
    int **gpuloading)
{
    //******************
    // GPU Index
    //******************
    THRML_LOG("[mtk_thermal_get_gpu_info] \n");

    if (nocores)
        *nocores = NO_GPU_CORES;

    if (gpufreq)
    {
        gpufreqs[0] = mt_gpufreq_get_cur_freq()/1000; // MHz
        *gpufreq = gpufreqs;
    }

    if (gpuloading)
    {
        unsigned int rd_gpu_loading = 0;
#ifndef CONFIG_ARM64 // work around for 6795
        if (mtk_get_gpu_loading(&rd_gpu_loading))
#else
        if (1)
#endif
        {
            gpuloadings[0] = (int) rd_gpu_loading;
            *gpuloading = gpuloadings;
        }

    }

    return 0;
}

int mtk_thermal_get_batt_info(
    int *batt_voltage,
    int *batt_current,
    int *batt_temp)
{
    //******************
    // Battery
    //******************

    /* Read Battery Information */
    if (batt_current)
    {
        *batt_current = get_sys_battery_info("/sys/devices/platform/battery/FG_Battery_CurrentConsumption");
        // the return value is 0.1mA
        if (*batt_current%10 <5)
            *batt_current /= 10;
        else
            *batt_current = 1+(*batt_current/10);


#if defined(CONFIG_MTK_SMART_BATTERY)
        if (KAL_TRUE == gFG_Is_Charging)
        {
            *batt_current *= -1;
        }
#endif
    }

    if (batt_voltage)
        *batt_voltage = get_sys_battery_info("/sys/class/power_supply/battery/batt_vol");

    if (batt_temp)
        *batt_temp = get_sys_battery_info("/sys/class/power_supply/battery/batt_temp");

    return 0;
}

#define NO_EXTRA_THERMAL_ATTR (7)
static char* extra_attr_names[NO_EXTRA_THERMAL_ATTR] = {0};
static int extra_attr_values[NO_EXTRA_THERMAL_ATTR] = {0};
static char* extra_attr_units[NO_EXTRA_THERMAL_ATTR] = {0};

int mtk_thermal_get_extra_info(
    int *no_extra_attr,
    char ***attr_names,
    int **attr_values,
    char ***attr_units)
{

    int size, i=0;

    if (no_extra_attr)
        *no_extra_attr = NO_EXTRA_THERMAL_ATTR;

    //******************
    // Modem Index
    //******************
    THRML_LOG("[mtk_thermal_get_gpu_info] mtk_mdm_get_md_info\n");
    {
        struct md_info *p_info;
        mtk_mdm_get_md_info(&p_info, &size);
        THRML_LOG("[mtk_thermal_get_gpu_info] mtk_mdm_get_md_info size %d\n", size);
        if (size <= NO_EXTRA_THERMAL_ATTR-1)
        {
            for (i=0; i<size; i++)
            {
                extra_attr_names[i] = p_info[i].attribute;
                extra_attr_values[i] = p_info[i].value;
                extra_attr_units[i] = p_info[i].unit;
            }
        }
    }

    //******************
    // Wifi Index
    //******************
    /* Get Wi-Fi Tx throughput */
    extra_attr_names[i] = "WiFi_TP";
    extra_attr_values[i] = get_sys_wifi_throughput("/proc/driver/thermal/wifi_tx_thro", 3);
    extra_attr_units[i] = "Kbps";

    if (attr_names)
        *attr_names = extra_attr_names;

    if (attr_values)
        *attr_values = extra_attr_values;

    if (attr_units)
        *attr_units = extra_attr_units;

    return 0;

}

extern int force_get_tbat(void);

int mtk_thermal_force_get_batt_temp(
    void)
{
    int ret = 0;

    ret = force_get_tbat();

    return ret;
}

static unsigned int _thermal_scen = 0;

unsigned int mtk_thermal_set_user_scenarios(
    unsigned int mask)
{
    if ((mask & MTK_THERMAL_SCEN_CALL)) // only one scen is handled now...
    {
        set_taklking_flag(true); // make mtk_ts_cpu.c aware of call scenario
        _thermal_scen |= (unsigned int) MTK_THERMAL_SCEN_CALL;
    }
    return _thermal_scen;
}

unsigned int mtk_thermal_clear_user_scenarios(
    unsigned int mask)
{
    if ((mask & MTK_THERMAL_SCEN_CALL)) // only one scen is handled now...
    {
        set_taklking_flag(false); // make mtk_ts_cpu.c aware of call scenario
        _thermal_scen &= ~((unsigned int) MTK_THERMAL_SCEN_CALL);
    }
    return _thermal_scen;
}

//*********************************************
// Export Interface
//*********************************************

EXPORT_SYMBOL(mtk_thermal_get_cpu_info);
EXPORT_SYMBOL(mtk_thermal_get_gpu_info);
EXPORT_SYMBOL(mtk_thermal_get_batt_info);
EXPORT_SYMBOL(mtk_thermal_get_extra_info);
EXPORT_SYMBOL(mtk_thermal_force_get_batt_temp);
EXPORT_SYMBOL(mtk_thermal_set_user_scenarios);
EXPORT_SYMBOL(mtk_thermal_clear_user_scenarios);
EXPORT_SYMBOL(mtk_thermal_get_gpu_loading_fp);
module_init(mtk_thermal_platform_init);
module_exit(mtk_thermal_platform_exit);


