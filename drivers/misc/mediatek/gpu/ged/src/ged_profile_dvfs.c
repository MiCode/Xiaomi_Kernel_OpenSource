#include <linux/slab.h>
#include <linux/sched.h>
#include "ged_base.h"
#include "ged_log.h"
#include "ged_profile_dvfs.h"

static GED_LOG_BUF_HANDLE ghLogBuf = 0;
static struct mutex gsMutex;
static GED_BOOL gbAllowRecord = GED_FALSE;

GED_ERROR ged_profile_dvfs_init(void)
{
    mutex_init(&gsMutex);

#if 0
    ghLogBuf = ged_log_buf_alloc(320, 64 * 320, GED_LOG_BUF_TYPE_QUEUEBUFFER_AUTO_INCREASE, NULL, "profile_dvfs");
#endif

    return GED_OK;
}

GED_ERROR ged_profile_dvfs_enable(void)
{
    GED_ERROR ret;

    mutex_lock(&gsMutex);

    if (NULL == ghLogBuf)
    {
        ghLogBuf = ged_log_buf_alloc(320, 64 * 320, GED_LOG_BUF_TYPE_QUEUEBUFFER_AUTO_INCREASE, NULL, "profile_dvfs");
    }

    ret = ghLogBuf ? GED_OK : GED_ERROR_FAIL;

    mutex_unlock(&gsMutex);

    return ret;
}

void ged_profile_dvfs_disable(void)
{
    mutex_lock(&gsMutex);

    if (NULL != ghLogBuf)
    {
        ged_log_buf_free(ghLogBuf);
        ghLogBuf = NULL;
    }

    mutex_unlock(&gsMutex);
}

void ged_profile_dvfs_start(void)
{
    gbAllowRecord = GED_TRUE;
}

void ged_profile_dvfs_stop(void)
{
    gbAllowRecord = GED_FALSE;
}

void ged_profile_dvfs_ignore_lines(int i32LineCount)
{
    mutex_lock(&gsMutex);

    if (ghLogBuf)
    {
        ged_log_buf_ignore_lines(ghLogBuf, i32LineCount);
    }

    mutex_unlock(&gsMutex);
}

void ged_profile_dvfs_exit(void)
{
    ged_profile_dvfs_disable();
}

void ged_profile_dvfs_record_freq_volt(unsigned int ui32Frequency, unsigned int ui32Voltage)
{
    mutex_lock(&gsMutex);

    if (ghLogBuf && gbAllowRecord)
    {
    	/* copy & modify from ./kernel/printk.c */
    	unsigned long long t;
    	unsigned long nanosec_rem;

    	t = cpu_clock(smp_processor_id());
    	nanosec_rem = do_div(t, 1000000000) / 1000;

        ged_log_buf_print(ghLogBuf, "%5lu.%06lu,freq_volt,%u,%u", (unsigned long) t, nanosec_rem, ui32Frequency, ui32Voltage);
    }
    mutex_unlock(&gsMutex);
}

void ged_profile_dvfs_record_temp(int i32Temp)
{
    mutex_lock(&gsMutex);

    if (ghLogBuf && gbAllowRecord)
    {
    	/* copy & modify from ./kernel/printk.c */
    	unsigned long long t;
    	unsigned long nanosec_rem;

    	t = cpu_clock(smp_processor_id());
    	nanosec_rem = do_div(t, 1000000000) / 1000;

        ged_log_buf_print(ghLogBuf, "%5lu.%06lu,temp,%d", (unsigned long) t, nanosec_rem, i32Temp);
    }
    mutex_unlock(&gsMutex);

}

void ged_profile_dvfs_record_thermal_limit(unsigned int ui32FreqLimit)
{
    mutex_lock(&gsMutex);

    if (ghLogBuf && gbAllowRecord)
    {
    	/* copy & modify from ./kernel/printk.c */
    	unsigned long long t;
    	unsigned long nanosec_rem;

    	t = cpu_clock(smp_processor_id());
    	nanosec_rem = do_div(t, 1000000000) / 1000;

        ged_log_buf_print(ghLogBuf, "%5lu.%06lu,thermal_limit,%u", (unsigned long) t, nanosec_rem, ui32FreqLimit);
    }
    mutex_unlock(&gsMutex);
}

void ged_profile_dvfs_record_gpu_loading(unsigned int ui32GpuLoading)
{
    mutex_lock(&gsMutex);

    if (ghLogBuf && gbAllowRecord)
    {
    	/* copy & modify from ./kernel/printk.c */
    	unsigned long long t;
    	unsigned long nanosec_rem;

    	t = cpu_clock(smp_processor_id());
    	nanosec_rem = do_div(t, 1000000000) / 1000;

        ged_log_buf_print(ghLogBuf, "%5lu.%06lu,gpu_load,%u", (unsigned long) t, nanosec_rem, ui32GpuLoading);
    }

    mutex_unlock(&gsMutex);
}

void ged_profile_dvfs_record_clock_on(void)
{
    mutex_lock(&gsMutex);

    if (ghLogBuf && gbAllowRecord)
    {
    	/* copy & modify from ./kernel/printk.c */
    	unsigned long long t;
    	unsigned long nanosec_rem;

    	t = cpu_clock(smp_processor_id());
    	nanosec_rem = do_div(t, 1000000000) / 1000;

        ged_log_buf_print(ghLogBuf, "%5lu.%06lu,gpu_clock,1", (unsigned long) t, nanosec_rem);
    }

    mutex_unlock(&gsMutex);
}

void ged_profile_dvfs_record_clock_off(void)
{
    mutex_lock(&gsMutex);

    if (ghLogBuf && gbAllowRecord)
    {
    	/* copy & modify from ./kernel/printk.c */
    	unsigned long long t;
    	unsigned long nanosec_rem;

    	t = cpu_clock(smp_processor_id());
    	nanosec_rem = do_div(t, 1000000000) / 1000;

        ged_log_buf_print(ghLogBuf, "%5lu.%06lu,gpu_clock,0", (unsigned long) t, nanosec_rem);
    }

    mutex_unlock(&gsMutex);
}
