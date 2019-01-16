#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <mach/m4u.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "extd_utils.h"
#include "extd_drv_log.h"
///#include "debug.h"
static DEFINE_SEMAPHORE(extd_mutex);

int extd_mutex_init(struct mutex *m)
{
	DISPDBG("mutex init:\n");
    return 0;
	mutex_init(m);
	return 0;
}

int extd_sw_mutex_lock(struct mutex*m)
{
	///mutex_lock(m);
	down_interruptible(&extd_mutex);
	DISPDBG("mutex: lock\n");
	return 0;
}

int extd_mutex_trylock(struct mutex*m)
{
	int ret = 0;
	///ret = mutex_trylock(m);
	DISPDBG("mutex: trylock\n");
	return ret;
}


int extd_sw_mutex_unlock(struct mutex*m)
{
	///mutex_unlock(m);
	up(&extd_mutex);
	DISPDBG("mutex: unlock\n");
	return 0;
}

int extd_msleep(unsigned int ms)
{
	DISPDBG("sleep %dms\n", ms);
	msleep(ms);
	return 0;
}

long int extd_get_time_us(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

