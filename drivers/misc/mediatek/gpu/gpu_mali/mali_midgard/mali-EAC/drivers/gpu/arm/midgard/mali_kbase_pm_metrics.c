/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





/**
 * @file mali_kbase_pm_metrics.c
 * Metrics for power management
 */

#include <mali_kbase.h>
#include <mali_kbase_pm.h>
#include <mali_kbase_config_defaults.h>

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#if KBASE_PM_EN

/* When VSync is being hit aim for utilisation between 70-90% */
#define KBASE_PM_VSYNC_MIN_UTILISATION          25//70
#define KBASE_PM_VSYNC_MAX_UTILISATION          45//90
/* Otherwise aim for 10-40% */
#define KBASE_PM_NO_VSYNC_MIN_UTILISATION       10
#define KBASE_PM_NO_VSYNC_MAX_UTILISATION       40

#ifdef CONFIG_MALI_MIDGARD_DVFS

int g_current_sample_gl_utilization = 0;
int g_current_sample_cl_utilization[2] = {0};

/* MTK GPU DVFS */
#include "mt_gpufreq.h"
#include "random.h"

int g_dvfs_enabled = 1;
int g_input_boost_enabled = 1;
enum kbase_pm_dvfs_action g_current_action = KBASE_PM_DVFS_NOP;
int g_dvfs_freq = DEFAULT_PM_DVFS_FREQ;
int g_dvfs_threshold_max = KBASE_PM_VSYNC_MAX_UTILISATION;
int g_dvfs_threshold_min = KBASE_PM_VSYNC_MIN_UTILISATION;
int g_dvfs_deferred_count = 3;
#endif // CONFIG_MALI_MIDGARD_DVFS
int g_touch_boost_flag = 0;
int g_touch_boost_id = 0;

int g_early_suspend = 0;
extern unsigned int g_power_status;

#include <linux/suspend.h>

static enum hrtimer_restart dvfs_callback(struct hrtimer *timer);

void mali_SODI_begin(void)
{
		struct list_head *entry;
	  const struct list_head *kbdev_list;    
	  kbdev_list = kbase_dev_list_get();
	  
	  list_for_each(entry, kbdev_list) 
	  {
	  	struct kbase_device *kbdev = NULL;
	  	kbdev = list_entry(entry, struct kbase_device, entry);	  	
	  	
	  	if (MALI_TRUE == kbdev->pm.metrics.timer_active)
	  	{
				 unsigned long flags;	
				
				 spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
				 kbdev->pm.metrics.timer_active = MALI_FALSE;				
				 spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
				 
				 hrtimer_cancel(&kbdev->pm.metrics.timer);
	  	}
	  }	  
	  kbase_dev_list_put(kbdev_list);	  	  
}
KBASE_EXPORT_TEST_API(mali_SODI_begin);


void mali_SODI_exit(void)
{	  
		struct list_head *entry;
	  const struct list_head *kbdev_list;	  	    
	  kbdev_list = kbase_dev_list_get();	
	    
	  list_for_each(entry, kbdev_list) 
	  {
	  	 struct kbase_device *kbdev = NULL;
	  	 kbdev = list_entry(entry, struct kbase_device, entry);	  	

	  	 if((MALI_FALSE == kbdev->pm.metrics.timer_active) && (g_early_suspend==0))
	  	 {		
	  	 	 unsigned long flags;	
	  	 	 
			 	 spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
  		 	 kbdev->pm.metrics.timer_active = MALI_TRUE;
  		 	 spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
  		 	 
  		 	 hrtimer_init(&kbdev->pm.metrics.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  		 	 kbdev->pm.metrics.timer.function = dvfs_callback;
  		 	 hrtimer_start(&kbdev->pm.metrics.timer, HR_TIMER_DELAY_MSEC(kbdev->pm.platform_dvfs_frequency), HRTIMER_MODE_REL);
  		 }
  	}
  	kbase_dev_list_put(kbdev_list);  	
}
KBASE_EXPORT_TEST_API(mali_SODI_exit);


/* Shift used for kbasep_pm_metrics_data.time_busy/idle - units of (1 << 8) ns
   This gives a maximum period between samples of 2^(32+8)/100 ns = slightly under 11s.
   Exceeding this will cause overflow */
#define KBASE_PM_TIME_SHIFT			8

#ifdef CONFIG_MALI_MIDGARD_DVFS

void mtk_get_touch_boost_flag(int *touch_boost_flag, int *touch_boost_id)
{
    *touch_boost_flag = g_touch_boost_flag;
    *touch_boost_id = g_touch_boost_id;
    return;
}

void mtk_set_touch_boost_flag(int touch_boost_id)
{
    g_touch_boost_flag = 1;
    g_touch_boost_id = touch_boost_id;
    return;
}

void mtk_clear_touch_boost_flag(void)
{
    g_touch_boost_flag = 0;
    return;
}

int mtk_get_input_boost_enabled()
{
    return g_input_boost_enabled;
}

int mtk_get_dvfs_enabled()
{
    return g_dvfs_enabled;
}

int mtk_get_dvfs_freq()
{
    return g_dvfs_freq;
}

int mtk_get_dvfs_threshold_max()
{
    return g_dvfs_threshold_max;
}

int mtk_get_dvfs_threshold_min()
{
    return g_dvfs_threshold_min;
}

int mtk_get_dvfs_deferred_count()
{
    return g_dvfs_deferred_count;
}

enum kbase_pm_dvfs_action mtk_get_dvfs_action()
{
    return g_current_action;
}

void mtk_clear_dvfs_action()
{
    g_current_action = KBASE_PM_DVFS_NONSENSE;    
}

static enum hrtimer_restart dvfs_callback(struct hrtimer *timer)
{
	unsigned long flags;
	enum kbase_pm_dvfs_action action;
	struct kbasep_pm_metrics_data *metrics;

	KBASE_DEBUG_ASSERT(timer != NULL);
	
	metrics = container_of(timer, struct kbasep_pm_metrics_data, timer);
	action = kbase_pm_get_dvfs_action(metrics->kbdev);

	g_current_action = action;
	spin_lock_irqsave(&metrics->lock, flags);
    metrics->kbdev->pm.platform_dvfs_frequency = mtk_get_dvfs_freq();

	if (metrics->timer_active)
		hrtimer_start(timer,
					  HR_TIMER_DELAY_MSEC(metrics->kbdev->pm.platform_dvfs_frequency),
					  HRTIMER_MODE_REL);

	spin_unlock_irqrestore(&metrics->lock, flags);

	return HRTIMER_NORESTART;
}
#endif /* CONFIG_MALI_MIDGARD_DVFS */

mali_error kbasep_pm_metrics_init(struct kbase_device *kbdev)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	kbdev->pm.metrics.kbdev = kbdev;
	kbdev->pm.metrics.vsync_hit = 1; /* [MTK] vsync notifiy is not implemented yet, assumed always hit. */
	kbdev->pm.metrics.utilisation = 0;
	kbdev->pm.metrics.util_cl_share[0] = 0;
	kbdev->pm.metrics.util_cl_share[1] = 0;
	kbdev->pm.metrics.util_gl_share = 0;

	kbdev->pm.metrics.time_period_start = ktime_get();
	kbdev->pm.metrics.time_busy = 0;
	kbdev->pm.metrics.time_idle = 0;
	kbdev->pm.metrics.gpu_active = MALI_TRUE;
	kbdev->pm.metrics.active_cl_ctx[0] = 0;
	kbdev->pm.metrics.active_cl_ctx[1] = 0;
	kbdev->pm.metrics.active_gl_ctx = 0;
	kbdev->pm.metrics.busy_cl[0] = 0;
	kbdev->pm.metrics.busy_cl[1] = 0;
	kbdev->pm.metrics.busy_gl = 0;

	spin_lock_init(&kbdev->pm.metrics.lock);

#ifdef CONFIG_MALI_MIDGARD_DVFS
	kbdev->pm.metrics.timer_active = MALI_TRUE;
	hrtimer_init(&kbdev->pm.metrics.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	kbdev->pm.metrics.timer.function = dvfs_callback;

	hrtimer_start(&kbdev->pm.metrics.timer, HR_TIMER_DELAY_MSEC(kbdev->pm.platform_dvfs_frequency), HRTIMER_MODE_REL);

#endif /* CONFIG_MALI_MIDGARD_DVFS */

	kbase_pm_register_vsync_callback(kbdev);

	return MALI_ERROR_NONE;
}

KBASE_EXPORT_TEST_API(kbasep_pm_metrics_init)

void kbasep_pm_metrics_term(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_MIDGARD_DVFS
	unsigned long flags;
	KBASE_DEBUG_ASSERT(kbdev != NULL);
		
	if (MALI_TRUE == kbdev->pm.metrics.timer_active)
	{	
		 spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
		 kbdev->pm.metrics.timer_active = MALI_FALSE;
		 spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
  	 
		 hrtimer_cancel(&kbdev->pm.metrics.timer);
	}
#endif /* CONFIG_MALI_MIDGARD_DVFS */

	kbase_pm_unregister_vsync_callback(kbdev);
}

KBASE_EXPORT_TEST_API(kbasep_pm_metrics_term)

/*caller needs to hold kbdev->pm.metrics.lock before calling this function*/
void kbasep_pm_record_job_status(struct kbase_device *kbdev)
{
	ktime_t now;
	ktime_t diff;
	u32 ns_time;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	now = ktime_get();
	diff = ktime_sub(now, kbdev->pm.metrics.time_period_start);

	ns_time = (u32) (ktime_to_ns(diff) >> KBASE_PM_TIME_SHIFT);
	kbdev->pm.metrics.time_busy += ns_time;
	kbdev->pm.metrics.busy_gl += ns_time * kbdev->pm.metrics.active_gl_ctx;
	kbdev->pm.metrics.busy_cl[0] += ns_time * kbdev->pm.metrics.active_cl_ctx[0];
	kbdev->pm.metrics.busy_cl[1] += ns_time * kbdev->pm.metrics.active_cl_ctx[1];
	kbdev->pm.metrics.time_period_start = now;
}

KBASE_EXPORT_TEST_API(kbasep_pm_record_job_status)

void kbasep_pm_record_gpu_idle(struct kbase_device *kbdev)
{
	unsigned long flags;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);

	KBASE_DEBUG_ASSERT(kbdev->pm.metrics.gpu_active == MALI_TRUE);

	kbdev->pm.metrics.gpu_active = MALI_FALSE;

	kbasep_pm_record_job_status(kbdev);

	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
}

KBASE_EXPORT_TEST_API(kbasep_pm_record_gpu_idle)

void kbasep_pm_record_gpu_active(struct kbase_device *kbdev)
{
	unsigned long flags;
	ktime_t now;
	ktime_t diff;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);

	KBASE_DEBUG_ASSERT(kbdev->pm.metrics.gpu_active == MALI_FALSE);

	kbdev->pm.metrics.gpu_active = MALI_TRUE;

	now = ktime_get();
	diff = ktime_sub(now, kbdev->pm.metrics.time_period_start);

	kbdev->pm.metrics.time_idle += (u32) (ktime_to_ns(diff) >> KBASE_PM_TIME_SHIFT);
	kbdev->pm.metrics.time_period_start = now;

	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
}

KBASE_EXPORT_TEST_API(kbasep_pm_record_gpu_active)

void kbase_pm_report_vsync(struct kbase_device *kbdev, int buffer_updated)
{
	unsigned long flags;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
	kbdev->pm.metrics.vsync_hit = buffer_updated;
	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
}

KBASE_EXPORT_TEST_API(kbase_pm_report_vsync)

/*caller needs to hold kbdev->pm.metrics.lock before calling this function*/
static void kbase_pm_get_dvfs_utilisation_calc(struct kbase_device *kbdev, ktime_t now)
{
	ktime_t diff;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	diff = ktime_sub(now, kbdev->pm.metrics.time_period_start);

	if (kbdev->pm.metrics.gpu_active) {
		u32 ns_time = (u32) (ktime_to_ns(diff) >> KBASE_PM_TIME_SHIFT);
		kbdev->pm.metrics.time_busy += ns_time;
		kbdev->pm.metrics.busy_cl[0] += ns_time * kbdev->pm.metrics.active_cl_ctx[0];
		kbdev->pm.metrics.busy_cl[1] += ns_time * kbdev->pm.metrics.active_cl_ctx[1];
		kbdev->pm.metrics.busy_gl += ns_time * kbdev->pm.metrics.active_gl_ctx;
	} else {
		kbdev->pm.metrics.time_idle += (u32) (ktime_to_ns(diff) >> KBASE_PM_TIME_SHIFT);
	}
}

/*caller needs to hold kbdev->pm.metrics.lock before calling this function*/
static void kbase_pm_get_dvfs_utilisation_reset(struct kbase_device *kbdev, ktime_t now)
{
	kbdev->pm.metrics.time_period_start = now;
	kbdev->pm.metrics.time_idle = 0;
	kbdev->pm.metrics.time_busy = 0;
	kbdev->pm.metrics.busy_cl[0] = 0;
	kbdev->pm.metrics.busy_cl[1] = 0;
	kbdev->pm.metrics.busy_gl = 0;
}

void kbase_pm_get_dvfs_utilisation(struct kbase_device *kbdev, unsigned long *total, unsigned long *busy, bool reset)
{
	ktime_t now = ktime_get();
	unsigned long tmp, flags;

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
	kbase_pm_get_dvfs_utilisation_calc(kbdev, now);

	tmp = kbdev->pm.metrics.busy_gl;
	tmp += kbdev->pm.metrics.busy_cl[0];
	tmp += kbdev->pm.metrics.busy_cl[1];

	*busy = tmp;
	*total = tmp + kbdev->pm.metrics.time_idle;

	if (reset)
		kbase_pm_get_dvfs_utilisation_reset(kbdev, now);
	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
}

#ifdef CONFIG_MALI_MIDGARD_DVFS

/*caller needs to hold kbdev->pm.metrics.lock before calling this function*/
int kbase_pm_get_dvfs_utilisation_old(struct kbase_device *kbdev, int *util_gl_share, int util_cl_share[2])
{
	int utilisation;
	int busy;
	ktime_t now = ktime_get();

	kbase_pm_get_dvfs_utilisation_calc(kbdev, now);

	if (kbdev->pm.metrics.time_idle + kbdev->pm.metrics.time_busy == 0) {
		/* No data - so we return NOP */
		utilisation = -1;
		if (util_gl_share)
			*util_gl_share = -1;
		if (util_cl_share) {
			util_cl_share[0] = -1;
			util_cl_share[1] = -1;
		}
		goto out;
	}

	utilisation = (100 * kbdev->pm.metrics.time_busy) /
			(kbdev->pm.metrics.time_idle +
			 kbdev->pm.metrics.time_busy);

	busy = kbdev->pm.metrics.busy_gl +
		kbdev->pm.metrics.busy_cl[0] +
		kbdev->pm.metrics.busy_cl[1];

	if (busy != 0) {
		if (util_gl_share)
			*util_gl_share =
				(100 * kbdev->pm.metrics.busy_gl) / busy;
		if (util_cl_share) {
			util_cl_share[0] =
				(100 * kbdev->pm.metrics.busy_cl[0]) / busy;
			util_cl_share[1] =
				(100 * kbdev->pm.metrics.busy_cl[1]) / busy;
		}
	} else {
		if (util_gl_share)
			*util_gl_share = -1;
		if (util_cl_share) {
			util_cl_share[0] = -1;
			util_cl_share[1] = -1;
		}
	}

out:
	kbase_pm_get_dvfs_utilisation_reset(kbdev, now);

	return utilisation;
}

enum kbase_pm_dvfs_action kbase_pm_get_dvfs_action(struct kbase_device *kbdev)
{
	unsigned long flags;
	int utilisation, util_gl_share;
	int util_cl_share[2];
	int random_action;
	enum kbase_pm_dvfs_action action;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);

	utilisation = kbase_pm_get_dvfs_utilisation_old(kbdev, &util_gl_share, util_cl_share);

	if (utilisation < 0 || util_gl_share < 0 || util_cl_share[0] < 0 || util_cl_share[1] < 0) {
		action = KBASE_PM_DVFS_NOP;
		utilisation = 0;
		util_gl_share = 0;
		util_cl_share[0] = 0;
		util_cl_share[1] = 0;
		goto out;
	}

	if (kbdev->pm.metrics.vsync_hit) {
		/* VSync is being met */
		if (utilisation < mtk_get_dvfs_threshold_min())
			action = KBASE_PM_DVFS_CLOCK_DOWN;
		else if (utilisation > mtk_get_dvfs_threshold_max())
			action = KBASE_PM_DVFS_CLOCK_UP;
		else
			action = KBASE_PM_DVFS_NOP;
	} else {
		/* VSync is being missed */
		if (utilisation < KBASE_PM_NO_VSYNC_MIN_UTILISATION)
			action = KBASE_PM_DVFS_CLOCK_DOWN;
		else if (utilisation > KBASE_PM_NO_VSYNC_MAX_UTILISATION)
			action = KBASE_PM_DVFS_CLOCK_UP;
		else
			action = KBASE_PM_DVFS_NOP;
	}

	// get a radom action for stress test
	if (mtk_get_dvfs_enabled() == 2)
	{
		get_random_bytes( &random_action, sizeof(random_action));
        random_action = random_action%3;
        //pr_debug("[MALI] GPU DVFS stress test - genereate random action here: action = %d", random_action);
        pr_debug("[MALI] GPU DVFS stress test - genereate random action here: action = %d", random_action);
        action = random_action;
	}

	kbdev->pm.metrics.utilisation = utilisation;
	kbdev->pm.metrics.util_cl_share[0] = util_cl_share[0];
	kbdev->pm.metrics.util_cl_share[1] = util_cl_share[1];
	kbdev->pm.metrics.util_gl_share = util_gl_share;

	g_current_sample_gl_utilization = utilisation;
	g_current_sample_cl_utilization[0] = util_cl_share[0];
	g_current_sample_cl_utilization[1] = util_cl_share[1];
   
out:
#if 0 /// def CONFIG_MALI_MIDGARD_DVFS
	kbase_platform_dvfs_event(kbdev, utilisation, util_gl_share, util_cl_share);
#endif				/*CONFIG_MALI_MIDGARD_DVFS */
	kbdev->pm.metrics.time_idle = 0;
	kbdev->pm.metrics.time_busy = 0;
	kbdev->pm.metrics.busy_cl[0] = 0;
	kbdev->pm.metrics.busy_cl[1] = 0;
	kbdev->pm.metrics.busy_gl = 0;
	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);

	return action;
}
KBASE_EXPORT_TEST_API(kbase_pm_get_dvfs_action)

mali_bool kbase_pm_metrics_is_active(struct kbase_device *kbdev)
{
	mali_bool isactive;
	unsigned long flags;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
	isactive = (kbdev->pm.metrics.timer_active == MALI_TRUE);
	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);

	return isactive;
}

KBASE_EXPORT_TEST_API(kbase_pm_metrics_is_active)

u32 kbasep_get_gl_utilization(void)
{
	return g_current_sample_gl_utilization;
}
KBASE_EXPORT_TEST_API(kbasep_get_gl_utilization)

u32 kbasep_get_cl_js0_utilization(void)
{
	return g_current_sample_cl_utilization[0];
}
KBASE_EXPORT_TEST_API(kbasep_get_cl_js0_utilization)

u32 kbasep_get_cl_js1_utilization(void)
{
	return g_current_sample_cl_utilization[1];
}
KBASE_EXPORT_TEST_API(kbasep_get_cl_js1_utilization)


//extern unsigned int (*mtk_get_gpu_loading_fp)(void) = kbasep_get_gl_utilization;

static unsigned int _mtk_gpu_dvfs_index_to_frequency(int iFreq)
{
    unsigned int iCurrentFreqCount;
    iCurrentFreqCount =mt_gpufreq_get_dvfs_table_num();
    if(iCurrentFreqCount == 2) // Denali-1
    {
        switch(iFreq)
        {
            case 0:
                return 4500000;
            case 1:
                return 2800000;
        }    
    }
	else if(iCurrentFreqCount == 3)//6735p
	{
	   switch(iFreq)
        {
            case 0:
                return 5500000;
            case 1:
                return 4500000;
			case 2:
				return 3000000;
        }    
	}
	return 0;
}

///=====================================================================================
///  The below function is added by Mediatek
///  In order to provide the debug sysfs command for change parameter dynamiically
///===================================================================================== 

/// 1. For GPU memory usage
static int proc_gpu_memoryusage_show(struct seq_file *m, void *v)
{
	ssize_t ret = 0;
   int total_size_in_bytes;
   int peak_size_in_bytes;

   total_size_in_bytes = kbase_report_gpu_memory_usage();   
   peak_size_in_bytes = kbase_report_gpu_memory_peak();
  
   ret = seq_printf(m, "curr: %10u, peak %10u\n", total_size_in_bytes, peak_size_in_bytes);

   return ret;
}

static int kbasep_gpu_memoryusage_debugfs_open(struct inode *in, struct file *file)
{
    return single_open(file, proc_gpu_memoryusage_show, NULL);
}

static const struct file_operations kbasep_gpu_memory_usage_debugfs_open = {
    .open    = kbasep_gpu_memoryusage_debugfs_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};



/// 2. For GL/CL utilization
static int proc_gpu_utilization_show(struct seq_file *m, void *v)
{
    unsigned long gl, cl0, cl1;
    unsigned int iCurrentFreq;

    iCurrentFreq = mt_gpufreq_get_cur_freq_index();
    
    gl  = kbasep_get_gl_utilization();
    cl0 = kbasep_get_cl_js0_utilization();
    cl1 = kbasep_get_cl_js1_utilization();

    seq_printf(m, "gpu/cljs0/cljs1=%lu/%lu/%lu, frequency=%d(kHz) power(0:off, 1:0n):%d\n", gl, cl0, cl1, _mtk_gpu_dvfs_index_to_frequency(iCurrentFreq), g_power_status);

    return 0;
}

static int kbasep_gpu_utilization_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_utilization_show , NULL);
}

static const struct file_operations kbasep_gpu_utilization_debugfs_fops = {
	.open    = kbasep_gpu_utilization_debugfs_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};



/// 3. For query GPU frequency
static int proc_gpu_frequency_show(struct seq_file *m, void *v)
{

    unsigned int iCurrentFreq;

    iCurrentFreq = mt_gpufreq_get_cur_freq_index();

    seq_printf(m, "GPU Frequency: %u(kHz)\n", _mtk_gpu_dvfs_index_to_frequency(iCurrentFreq));

    return 0;
}

static int kbasep_gpu_frequency_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_frequency_show , NULL);
}

static const struct file_operations kbasep_gpu_frequency_debugfs_fops = {
	.open    = kbasep_gpu_frequency_debugfs_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};



/// 4. For query GPU dynamically enable DVFS
static int proc_gpu_dvfs_enabled_show(struct seq_file *m, void *v)
{

    int dvfs_enabled;

    dvfs_enabled = mtk_get_dvfs_enabled();

    seq_printf(m, "dvfs_enabled: %d\n", dvfs_enabled);

    return 0;
}

static int kbasep_gpu_dvfs_enable_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_dvfs_enabled_show , NULL);
}

static ssize_t kbasep_gpu_dvfs_enable_write(struct file *file, const char __user *buffer, 
                size_t count, loff_t *data)
{
    char desc[32]; 
    int len = 0;
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if(!strncmp(desc, "1", 1))
        g_dvfs_enabled = 1;
    else if(!strncmp(desc, "0", 1))
        g_dvfs_enabled = 0;
    else if(!strncmp(desc, "2", 1))
        g_dvfs_enabled = 2;

    return count;
}

static const struct file_operations kbasep_gpu_dvfs_enable_debugfs_fops = {
	.open    = kbasep_gpu_dvfs_enable_debugfs_open,
	.read    = seq_read,
	.write   = kbasep_gpu_dvfs_enable_write,
	.release = single_release,
};



/// 5. For query GPU dynamically enable input boost
static int proc_gpu_input_boost_show(struct seq_file *m, void *v)
{

    int input_boost_enabled;

    input_boost_enabled = mtk_get_input_boost_enabled();

    seq_printf(m, "GPU input boost enabled: %d\n", input_boost_enabled);

    return 0;
}

static int kbasep_gpu_input_boost_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_input_boost_show , NULL);
}

static ssize_t kbasep_gpu_input_boost_write(struct file *file, const char __user *buffer, 
                size_t count, loff_t *data)
{
    char desc[32]; 
    int len = 0;
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if(!strncmp(desc, "1", 1))
        g_input_boost_enabled = 1;
    else if(!strncmp(desc, "0", 1))
        g_input_boost_enabled = 0;

    return count;
}

static const struct file_operations kbasep_gpu_input_boost_debugfs_fops = {
	.open    = kbasep_gpu_input_boost_debugfs_open,
	.read    = seq_read,
	.write   = kbasep_gpu_input_boost_write,
	.release = single_release,
};



/// 6. For query GPU dynamically set dvfs frequency (ms)
static int proc_gpu_dvfs_freq_show(struct seq_file *m, void *v)
{

    int dvfs_freq;

    dvfs_freq = mtk_get_dvfs_freq();

    seq_printf(m, "GPU DVFS freq : %d(ms)\n", dvfs_freq);

    return 0;
}

static int kbasep_gpu_dvfs_freq_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_dvfs_freq_show , NULL);
}

static ssize_t kbasep_gpu_dvfs_freq_write(struct file *file, const char __user *buffer, 
                size_t count, loff_t *data)
{
    char desc[32]; 
    int len = 0;
    int dvfs_freq = 0;
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if(sscanf(desc, "%d", &dvfs_freq) == 1)
        g_dvfs_freq = dvfs_freq;
    else 
        //pr_debug("[MALI] warning! echo [dvfs_freq(ms)] > /proc/mali/dvfs_freq\n");
        pr_debug("[MALI] warning! echo [dvfs_freq(ms)] > /proc/mali/dvfs_freq\n");

    return count;
}

static const struct file_operations kbasep_gpu_dvfs_freq_debugfs_fops = {
	.open    = kbasep_gpu_dvfs_freq_debugfs_open,
	.read    = seq_read,
	.write   = kbasep_gpu_dvfs_freq_write,
	.release = single_release,
};



/// 7.For query GPU dynamically set dvfs threshold
static int proc_gpu_dvfs_threshold_show(struct seq_file *m, void *v)
{

    int threshold_max, threshold_min;

    threshold_max = mtk_get_dvfs_threshold_max();
    threshold_min = mtk_get_dvfs_threshold_min();

    seq_printf(m, "GPU DVFS threshold : max:%d min:%d\n", threshold_max, threshold_min);

    return 0;
}

static int kbasep_gpu_dvfs_threshold_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_dvfs_threshold_show , NULL);
}

static ssize_t kbasep_gpu_dvfs_threshold_write(struct file *file, const char __user *buffer, 
                size_t count, loff_t *data)
{
    char desc[32]; 
    int len = 0;
    int threshold_max, threshold_min;
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if(sscanf(desc, "%d %d", &threshold_max, &threshold_min) == 2)
    {
        g_dvfs_threshold_max = threshold_max;
        g_dvfs_threshold_min = threshold_min;
    }
    else 
        //pr_debug("[MALI] warning! echo [dvfs_threshold_max] [dvfs_threshold_min] > /proc/mali/dvfs_threshold\n");
        pr_debug("[MALI] warning! echo [dvfs_threshold_max] [dvfs_threshold_min] > /proc/mali/dvfs_threshold\n");

    return count;
}

static const struct file_operations kbasep_gpu_dvfs_threshold_debugfs_fops = {
	.open    = kbasep_gpu_dvfs_threshold_debugfs_open,
	.read    = seq_read,
	.write   = kbasep_gpu_dvfs_threshold_write,
	.release = single_release,
};



/// 8.For query GPU dynamically set dvfs deferred count
static int proc_gpu_dvfs_deferred_count_show(struct seq_file *m, void *v)
{

    int deferred_count;

    deferred_count = mtk_get_dvfs_deferred_count();

    seq_printf(m, "GPU DVFS deferred_count : %d\n", deferred_count);

    return 0;
}

static int kbasep_gpu_dvfs_deferred_count_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_dvfs_deferred_count_show , NULL);
}

static ssize_t kbasep_gpu_dvfs_deferred_count_write(struct file *file, const char __user *buffer, 
                size_t count, loff_t *data)
{
    char desc[32]; 
    int len = 0;
    int dvfs_deferred_count;
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if(sscanf(desc, "%d", &dvfs_deferred_count) == 1)
        g_dvfs_deferred_count = dvfs_deferred_count;
    else 
        //pr_debug("[MALI] warning! echo [dvfs_deferred_count] > /proc/mali/dvfs_deferred_count\n");
        pr_debug("[MALI] warning! echo [dvfs_deferred_count] > /proc/mali/dvfs_deferred_count\n");

    return count;
}

static const struct file_operations kbasep_gpu_dvfs_deferred_count_debugfs_fops = {
	.open    = kbasep_gpu_dvfs_deferred_count_debugfs_open,
	.read    = seq_read,
	.write   = kbasep_gpu_dvfs_deferred_count_write,
	.release = single_release,
};


/// 9. For query the support command
static int proc_gpu_help_show(struct seq_file *m, void *v)
{
    seq_printf(m, "======================================================================\n");
    seq_printf(m, "A.For Query GPU/CPU related Command:\n");
    seq_printf(m, "  cat /proc/mali/utilization\n");
    seq_printf(m, "  cat /proc/mali/frequency\n");
    seq_printf(m, "  cat /proc/mali/memory_usage\n");
    seq_printf(m, "  cat /proc/gpufreq/gpufreq_var_dump\n");
    seq_printf(m, "  cat /proc/pm_init/ckgen_meter_test\n");
    seq_printf(m, "  cat /proc/cpufreq/cpufreq_cur_freq\n");
    seq_printf(m, "======================================================================\n");
    seq_printf(m, "B.For Fix GPU Frequency:\n");
    seq_printf(m, "  echo > (450000, 280000) /proc/gpufreq/gpufreq_opp_freq\n");
    seq_printf(m, "  echo 0 > /proc/gpufreq/gpufreq_opp_freq(re-enable GPU DVFS)\n");
    seq_printf(m, "C.For Turn On/Off CPU core number:\n");
    seq_printf(m, "  echo (1, 0) > /sys/devices/system/cpu/cpu1/online\n");
    seq_printf(m, "  echo (1, 0) > /sys/devices/system/cpu/cpu2/online\n");
    seq_printf(m, "  echo (1, 0) > /sys/devices/system/cpu/cpuN/online\n");    
    seq_printf(m, "D.For CPU Performance mode(Force CPU to run at highest speed:\n");
    seq_printf(m, " echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor\n");
    seq_printf(m, " echo interactive > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor(re-enable CPU DVFS)\n");
    seq_printf(m, "==============================================================================================\n");
    seq_printf(m, "E.For GPU advanced debugging command:\n");
    seq_printf(m, " echo [dvfs_freq(ms)] > /proc/mali/dvfs_freq\n");
    seq_printf(m, " echo [dvfs_thr_max] [dvfs_thr_min] > /proc/mali/dvfs_threshold\n");
    seq_printf(m, " echo [dvfs_deferred_count] > /proc/mali/dvfs_deferred_count\n");    
    seq_printf(m, "==============================================================================================\n");

    return 0;
}

static int kbasep_gpu_help_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, proc_gpu_help_show , NULL);
}

static const struct file_operations kbasep_gpu_help_debugfs_fops = {
	.open    = kbasep_gpu_help_debugfs_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *mali_pentry;

void proc_mali_register(void)
{    
    mali_pentry = proc_mkdir("mali", NULL);    
   
    if (!mali_pentry)
        return;
         
    proc_create("help", 0, mali_pentry, &kbasep_gpu_help_debugfs_fops);        
    proc_create("memory_usage", 0, mali_pentry, &kbasep_gpu_memory_usage_debugfs_open);
    proc_create("utilization", 0, mali_pentry, &kbasep_gpu_utilization_debugfs_fops);
    proc_create("frequency", 0, mali_pentry, &kbasep_gpu_frequency_debugfs_fops);
    proc_create("dvfs_enable", S_IRUGO | S_IWUSR, mali_pentry, &kbasep_gpu_dvfs_enable_debugfs_fops);
    proc_create("input_boost", S_IRUGO | S_IWUSR, mali_pentry, &kbasep_gpu_input_boost_debugfs_fops);
    proc_create("dvfs_freq", S_IRUGO | S_IWUSR, mali_pentry, &kbasep_gpu_dvfs_freq_debugfs_fops);
    proc_create("dvfs_threshold", S_IRUGO | S_IWUSR, mali_pentry, &kbasep_gpu_dvfs_threshold_debugfs_fops);
    proc_create("dvfs_deferred_count", S_IRUGO | S_IWUSR, mali_pentry, &kbasep_gpu_dvfs_deferred_count_debugfs_fops);
}
void proc_mali_unregister(void)
{
    if (!mali_pentry)
        return;

    remove_proc_entry("help", mali_pentry);
    remove_proc_entry("memory_usage", mali_pentry);
    remove_proc_entry("utilization", mali_pentry);
    remove_proc_entry("frequency", mali_pentry);
    remove_proc_entry("mali", NULL);
    mali_pentry = NULL;
}
#else
#define proc_mali_register() do{}while(0)
#define proc_mali_unregister() do{}while(0)
#endif /// CONFIG_PROC_FS

#endif /* CONFIG_MALI_MIDGARD_DVFS */

#endif  /* KBASE_PM_EN */
