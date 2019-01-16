#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <asm/system.h>

#include <linux/timer.h>
#include <linux/pid.h>
/* For using net dev + */
#include <linux/netdevice.h>
/* For using net dev - */

#include "mach/mtk_thermal_monitor.h"
#include <mach/system.h>


// Extern two API functions from battery driver to limit max charging current. 

#define mtk_cooler_mdulthro_dprintk_always(fmt, args...) \
  do { xlog_printk(ANDROID_LOG_INFO, "thermal/cooler/mdulthro", fmt, ##args); } while(0)

#define mtk_cooler_mdulthro_dprintk(fmt, args...) \
  do { \
    if (1 == cl_mdulthro_klog_on) { \
      xlog_printk(ANDROID_LOG_INFO, "thermal/cooler/mdulthro", fmt, ##args); \
    } \
  } while(0)

#define MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO  1

#define MTK_CL_MDULTHRO_GET_LIMIT(limit, state) \
    do { (limit) = (short) (((unsigned long) (state))>>16); } while(0)

#define MTK_CL_MDULTHRO_SET_LIMIT(limit, state) \
    do { state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); } while(0)

#define MTK_CL_MDULTHRO_GET_CURR_STATE(curr_state, state) \
    do { curr_state = (((unsigned long) (state))&0xFFFF); } while(0)

#define MTK_CL_MDULTHRO_SET_CURR_STATE(curr_state, state) \
    do { if (0 == curr_state) \
           state &= ~0x1; \
         else \
           state |= 0x1; \
    } while(0)

struct mdul_stats {
   unsigned long pre_time;
   unsigned long pre_tx_bytes;
};

static int cl_mdulthro_klog_on = 0;
#if 0
static struct thermal_cooling_device *cl_mdulthro_dev[MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO] = {0};
static unsigned long cl_mdulthro_state[MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO] = {0};
static int cl_mdulthro_cur_limit = 65535;
#endif

struct wmt_stats {
   unsigned long pre_time;
   unsigned long pre_tx_bytes;
};

static struct timer_list wmt_stats_timer;
static struct wmt_stats wmt_stats_info;
static unsigned long pre_time;
static unsigned long tx_throughput;

/*New Wifi throttling Algo+*/
//over_up_time * polling interval > up_duration --> throttling
static unsigned int over_up_time = 0; //polling time
static unsigned int up_duration = 30; //sec
static unsigned int up_denominator = 2;
static unsigned int up_numerator = 1;

//below_low_time * polling interval > low_duration --> throttling
static unsigned int below_low_time = 0; //polling time
static unsigned int low_duration = 10; //sec
static unsigned int low_denominator = 2;
static unsigned int low_numerator = 3;

static unsigned int low_rst_time = 0;
static unsigned int low_rst_max = 3;

static int polling_interval = 1; //second
/*New Wifi throttling Algo-*/

#define UNK_STAT -1
#define LOW_STAT 0
#define MID_STAT 1
#define HIGH_STAT 2
#define WFD_STAT 3

#define MAX_LEN	256
#define COOLER_THRO_NUM 3
#define COOLER_NUM 10
#define ONE_MBITS_PER_SEC 1000

static unsigned int tm_pid = 0;
static unsigned int tm_input_pid = 0;
static struct task_struct g_task;
static struct task_struct *pg_task = &g_task;

static unsigned int cl_pa1_dev_state =0;
static unsigned int cl_pa2_dev_state =0;

static struct thermal_cooling_device *cl_pa1_dev;
static struct thermal_cooling_device *cl_pa2_dev;

static unsigned long get_tx_bytes(void)
{
	struct net_device *dev;
	struct net *net;
	unsigned long tx_bytes = 0;

	read_lock(&dev_base_lock);
	for_each_net(net) {
		for_each_netdev(net, dev) {
			if(!strncmp(dev->name, "ccemni", 6) || !strncmp(dev->name, "ccmni", 5)) {
				struct rtnl_link_stats64 temp;
				const struct rtnl_link_stats64 *stats = dev_get_stats(dev, &temp);
				tx_bytes = tx_bytes + stats->tx_bytes;
			}
		}
	}
	read_unlock(&dev_base_lock);
	return tx_bytes;
}

static int wmt_cal_stats(unsigned long data)
{
	struct wmt_stats *stats_info = (struct wmt_stats*) data;
	struct timeval cur_time;

	mtk_cooler_mdulthro_dprintk("[%s] pre_time=%lu, pre_data=%lu\n", __func__, pre_time, stats_info->pre_tx_bytes);

	do_gettimeofday(&cur_time);

	if (pre_time != 0 && cur_time.tv_sec > pre_time) {
		unsigned long tx_bytes = get_tx_bytes();
		if (tx_bytes > stats_info->pre_tx_bytes) {

			tx_throughput = ((tx_bytes - stats_info->pre_tx_bytes) / (cur_time.tv_sec - pre_time)) >> 7;

			mtk_cooler_mdulthro_dprintk("[%s] cur_time=%lu, cur_data=%lu, tx_throughput=%luKb/s\n", __func__, cur_time.tv_sec, tx_bytes, tx_throughput );

			stats_info->pre_tx_bytes = tx_bytes;
		} else if (tx_bytes < stats_info->pre_tx_bytes) {
			/* Overflow */
			tx_throughput = ((0xffffffff - stats_info->pre_tx_bytes + tx_bytes) / (cur_time.tv_sec - pre_time)) >> 7;;
			stats_info->pre_tx_bytes = tx_bytes;
			mtk_cooler_mdulthro_dprintk("[%s] cur_tx(%lu) < pre_tx\n", __func__, tx_bytes);
		} else {
			/* No traffic */
			tx_throughput = 0;
			mtk_cooler_mdulthro_dprintk("[%s] cur_tx(%lu) = pre_tx\n", __func__, tx_bytes);
		}
	} else {
		/* Overflow possible ??*/
		tx_throughput = 0;
		mtk_cooler_mdulthro_dprintk("[%s] cur_time(%lu) < pre_time\n", __func__, cur_time.tv_sec);
	}

	pre_time = cur_time.tv_sec;
	mtk_cooler_mdulthro_dprintk("[%s] pre_time=%lu, tv_sec=%lu\n",__func__, pre_time, cur_time.tv_sec);

	wmt_stats_timer.expires = jiffies + 1 * HZ;
	add_timer(&wmt_stats_timer);
	return 0;
}

static int wmt_send_signal(int level)
{
	int ret = 0;
	int thro = level;

	if (tm_input_pid == 0) {
		mtk_cooler_mdulthro_dprintk("[%s] pid is empty\n", __func__);
		ret = -1;
	}

	mtk_cooler_mdulthro_dprintk_always("[%s] pid is %d, %d, %d\n", __func__, tm_pid, tm_input_pid, thro);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;
		info.si_signo = SIGIO;
		info.si_errno = 1; // for md ul throttling
		info.si_code = thro;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0) 
	    mtk_cooler_mdulthro_dprintk("[%s] ret=%d\n", __func__, ret);

	return ret;
}

static inline unsigned long thro(unsigned long a, unsigned int b, unsigned int c) {

	unsigned long tmp;

	tmp = (a << 10) * b / c;

	return tmp >> 10;
}

static int wmt_judge_throttling(int index, int is_on, int interval)
{
	/*
	 *     throttling_stat
	 *        2 ( pa1=1,pa2=1 )
	 * UPPER ----
	 *        1 ( pa1=1,pa2=0 )
	 * LOWER ----
	 *        0 ( pa1=0,pa2=0 )
	 */
	static unsigned int throttling_pre_stat = 0;
	static int mail_box[2] = {-1,-1};

	static bool is_reset = false;

	unsigned long cur_thro = tx_throughput;
	static unsigned long thro_constraint = 99 * 1000;

	int cur_wifi_stat = 0;

	mtk_cooler_mdulthro_dprintk("[%s]+ [0]=%d, [1]=%d || [%d] is %s\n", __func__, mail_box[0], mail_box[1],
											index, (is_on==1?"ON":"OFF"));
	mail_box[index] = is_on;

	if (mail_box[0] >= 0 && mail_box[1] >= 0) {
		cur_wifi_stat = mail_box[0] + mail_box[1];

#if 0   // no such feature for now
		/*
		 * If Wifi-display is on, go to WFD_STAT state, and reset the throttling.
		 */
		if (tm_wfd_stat == 2)
			cur_wifi_stat = WFD_STAT;
#endif

		switch(cur_wifi_stat) {

#if 0
			case WFD_STAT:
				if (throttling_pre_stat != WFD_STAT) {
					/*
					 * Enter Wifi-Display status, reset all throttling. Dont affect the performance of Wifi-Display.
					 */
					wmt_send_signal(-1);
					below_low_time = 0;
					over_up_time = 0;
					throttling_pre_stat = WFD_STAT;
					wmt_tm_printk("WFD is on, reset everything!");
				}
			break;
#endif

			case HIGH_STAT:
				if (throttling_pre_stat < HIGH_STAT || throttling_pre_stat == WFD_STAT) {
					if (cur_thro > 0) /*Wifi is working!!*/
						thro_constraint = thro(cur_thro, up_numerator, up_denominator);
					else /*At this moment, current throughput is none. Use the previous constraint.*/
						thro_constraint = thro(thro_constraint, up_numerator, up_denominator);

					mtk_cooler_mdulthro_dprintk_always("LOW/MID-->HIGH:%lu <- (%d / %d) %lu", thro_constraint, up_numerator, up_denominator, cur_thro);

					//wmt_send_signal( thro_constraint / ONE_MBITS_PER_SEC);
					wmt_send_signal( thro_constraint); // change to use kbps
					throttling_pre_stat = HIGH_STAT;
					over_up_time = 0;
				} else if (throttling_pre_stat == HIGH_STAT) {
					over_up_time++;
					if ( (over_up_time * interval) >= up_duration) {
						if (cur_thro < thro_constraint) /*real throughput may have huge variant*/
							thro_constraint = thro(cur_thro, up_numerator, up_denominator);
						else /* current throughput is large than constraint. WHAT!!!*/
							thro_constraint = thro(thro_constraint, up_numerator, up_denominator);

						mtk_cooler_mdulthro_dprintk_always("HIGH-->HIGH:%lu <- (%d / %d) %lu", thro_constraint, up_numerator, up_denominator, cur_thro);

						//wmt_send_signal( thro_constraint / ONE_MBITS_PER_SEC);
						wmt_send_signal( thro_constraint); // change to use kbps
						over_up_time = 0;
					}
				} else {
					mtk_cooler_mdulthro_dprintk("[%s] Error state1!!\n", __func__, throttling_pre_stat);
				}
				mtk_cooler_mdulthro_dprintk_always("case2 time=%d\n", over_up_time);
			break;

			case MID_STAT:
				if (throttling_pre_stat == LOW_STAT) {
					below_low_time = 0;
					throttling_pre_stat = MID_STAT;
					mtk_cooler_mdulthro_dprintk_always("[%s] Go up!!\n", __func__);
				} else if (throttling_pre_stat == HIGH_STAT) {
					over_up_time = 0;
					throttling_pre_stat = MID_STAT;
					mtk_cooler_mdulthro_dprintk_always("[%s] Go down!!\n", __func__);
				} else {
					throttling_pre_stat = MID_STAT;
					mtk_cooler_mdulthro_dprintk("[%s] pre_stat=%d!!\n", __func__, throttling_pre_stat);
				}
			break;

			case LOW_STAT:
				if (throttling_pre_stat == WFD_STAT) {
					throttling_pre_stat = LOW_STAT;
					mtk_cooler_mdulthro_dprintk_always("[%s] pre_stat=%d!!\n", __func__, throttling_pre_stat);
				} else if (throttling_pre_stat > LOW_STAT) {
					if (cur_thro < 5000 && cur_thro > 0) {
						thro_constraint = cur_thro * 3;
					} else if (cur_thro >= 5000) {
						thro_constraint = thro(cur_thro, low_numerator, low_denominator);
					} else {
						thro_constraint = thro(thro_constraint, low_numerator, low_denominator);
					}

					mtk_cooler_mdulthro_dprintk_always("MID/HIGH-->LOW:%lu <- (%d / %d) %lu", thro_constraint, low_numerator, low_denominator, cur_thro);
					//wmt_send_signal( thro_constraint / ONE_MBITS_PER_SEC);
					wmt_send_signal( thro_constraint); // change to use kbps
					throttling_pre_stat = LOW_STAT;
					below_low_time = 0;
					low_rst_time = 0;
					is_reset = false;
				} else if (throttling_pre_stat == LOW_STAT) {
					below_low_time++;
					if ( (below_low_time*interval) >= low_duration) {
						if (low_rst_time >= low_rst_max && !is_reset) {
							mtk_cooler_mdulthro_dprintk_always("over rst time=%d", low_rst_time);

							wmt_send_signal(-1); //reset
							low_rst_time = low_rst_max;
							is_reset = true;
						} else if(!is_reset) {
							if (cur_thro < 5000 && cur_thro > 0) {
								thro_constraint = cur_thro * 3;
							} else if (cur_thro >= 5000) {
								thro_constraint = thro(cur_thro, low_numerator, low_denominator);
								low_rst_time++;
							} else {
								thro_constraint = thro(thro_constraint, low_numerator, low_denominator);
								low_rst_time++;
							}

							mtk_cooler_mdulthro_dprintk_always("LOW-->LOW:%lu <-(%d / %d) %lu", thro_constraint, low_numerator, low_denominator, cur_thro);

							//wmt_send_signal( thro_constraint / ONE_MBITS_PER_SEC);
							wmt_send_signal( thro_constraint); // change to use kbps
							below_low_time = 0;
						} else {
							mtk_cooler_mdulthro_dprintk("Have reset, no control!!");
						}
					}
				} else {
					mtk_cooler_mdulthro_dprintk_always("[%s] Error state3 %d!!\n", __func__, throttling_pre_stat);
				}
				mtk_cooler_mdulthro_dprintk("case0 time=%d, rst=%d %d\n", below_low_time, low_rst_time, is_reset);
			break;

			default:
				mtk_cooler_mdulthro_dprintk_always("[%s] Error cur_wifi_stat=%d!!\n", __func__, cur_wifi_stat);
			break;
		}

		mail_box[0] = UNK_STAT;
		mail_box[1] = UNK_STAT;
	} else {
		mtk_cooler_mdulthro_dprintk("[%s] dont get all info!!\n", __func__);
	}
	return 0;
}

/* +mtktspa_cooling_pa1_ops+ */
static int wmt_cl_pa1_get_max_state(struct thermal_cooling_device *cool_dev,
             unsigned long *pv)
{
    *pv = 1;
    mtk_cooler_mdulthro_dprintk("[%s] %d\n", __func__, *pv);
    return 0;
}

static int wmt_cl_pa1_get_cur_state(struct thermal_cooling_device *cool_dev,
             unsigned long *pv)
{
    *pv = cl_pa1_dev_state;
    mtk_cooler_mdulthro_dprintk("[%s] %d\n", __func__, *pv);
    return 0;
}

static int wmt_cl_pa1_set_cur_state(struct thermal_cooling_device *cool_dev,
             unsigned long v)
{
	int ret = 0;

	mtk_cooler_mdulthro_dprintk("[%s] %d\n", __func__, v);

	cl_pa1_dev_state = (unsigned int)v;

	if (cl_pa1_dev_state == 1) {
		ret = wmt_judge_throttling(0, 1, polling_interval);
	} else {
		ret = wmt_judge_throttling(0, 0, polling_interval);
	}
	if (ret != 0) 
	    mtk_cooler_mdulthro_dprintk_always("[%s] ret=%d\n", __func__, ret);
    return ret;
}
/* -mtktspa_cooling_pa1_ops- */

/* +mtktspa_cooling_pa2_ops+ */
static int wmt_cl_pa2_get_max_state(struct thermal_cooling_device *cool_dev,
             unsigned long *pv)
{
    *pv = 1;
    mtk_cooler_mdulthro_dprintk("[%s] %d\n", __func__, *pv);
    return 0;
}

static int wmt_cl_pa2_get_cur_state(struct thermal_cooling_device *cool_dev,
             unsigned long *pv)
{
    *pv = cl_pa2_dev_state;
    mtk_cooler_mdulthro_dprintk("[%s] %d\n", __func__, *pv);
    return 0;
}

static int wmt_cl_pa2_set_cur_state(struct thermal_cooling_device *cool_dev,
             unsigned long v)
{
	int ret = 0;

	mtk_cooler_mdulthro_dprintk("[%s] %d\n", __func__, v);

	cl_pa2_dev_state = (unsigned int)v;

	if (cl_pa2_dev_state == 1) {
		ret = wmt_judge_throttling(1, 1, polling_interval);
	} else {
		ret = wmt_judge_throttling(1, 0, polling_interval);
	}
	if (ret != 0) 
	    mtk_cooler_mdulthro_dprintk_always("[%s] ret=%d\n", __func__, ret);
	return ret;
}

static struct thermal_cooling_device_ops mtktspa_cooling_pa1_ops = {
	.get_max_state = wmt_cl_pa1_get_max_state,
	.get_cur_state = wmt_cl_pa1_get_cur_state,
	.set_cur_state = wmt_cl_pa1_set_cur_state,
};

static struct thermal_cooling_device_ops mtktspa_cooling_pa2_ops = {
	.get_max_state = wmt_cl_pa2_get_max_state,
	.get_cur_state = wmt_cl_pa2_get_cur_state,
	.set_cur_state = wmt_cl_pa2_set_cur_state,
};
/* -mtktspa_cooling_pa2_ops- */

#if 0
static int 
mtk_cl_mdulthro_get_max_state(struct thermal_cooling_device *cdev,
                          unsigned long *state)
{        
  *state = 1;
  mtk_cooler_mdulthro_dprintk("mtk_cl_mdulthro_get_max_state() %s %d\n", cdev->type, *state);
  return 0;
}

static int 
mtk_cl_mdulthro_get_cur_state(struct thermal_cooling_device *cdev,
                          unsigned long *state)
{
  MTK_CL_MDULTHRO_GET_CURR_STATE(*state, *((unsigned long*) cdev->devdata));
  mtk_cooler_mdulthro_dprintk("mtk_cl_mdulthro_get_cur_state() %s %d\n", cdev->type, *state);
  mtk_cooler_mdulthro_dprintk("mtk_cl_mdulthro_get_cur_state() %s limit=%d\n", cdev->type, get_bat_charging_current_level()/100);
  return 0;
}

static int 
mtk_cl_mdulthro_set_cur_state(struct thermal_cooling_device *cdev,
                          unsigned long state)
{
  mtk_cooler_mdulthro_dprintk("mtk_cl_mdulthro_set_cur_state() %s %d\n", cdev->type, state);
  MTK_CL_MDULTHRO_SET_CURR_STATE(state, *((unsigned long*) cdev->devdata));
  mtk_cooler_mdulthro_dprintk("mtk_cl_mdulthro_set_cur_state() %s limit=%d\n", cdev->type, get_bat_charging_current_level()/100);
  
  return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_mdulthro_ops = {
  .get_max_state = mtk_cl_mdulthro_get_max_state,
  .get_cur_state = mtk_cl_mdulthro_get_cur_state,
  .set_cur_state = mtk_cl_mdulthro_set_cur_state,
};
#endif

static int mtk_cooler_mdulthro_register_ltf(void)
{
#if 0
  int i;
#endif
  mtk_cooler_mdulthro_dprintk("register ltf\n");

  cl_pa1_dev = mtk_thermal_cooling_device_register("cl-mdulthro-upper", NULL,
			    &mtktspa_cooling_pa1_ops);

  cl_pa2_dev = mtk_thermal_cooling_device_register("cl-mdulthro-lower", NULL,
			    &mtktspa_cooling_pa2_ops);

#if 0
  for (i = MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO; i-- > 0; )
  {
    char temp[20] = {0};
    sprintf(temp, "mtk-cl-mdulthro%02d", i);
    cl_mdulthro_dev[i] = mtk_thermal_cooling_device_register(temp, 
                                                         (void*) &cl_mdulthro_state[i], // put mdulthro state to cooler devdata
                                                         &mtk_cl_mdulthro_ops);
  }
#endif

  return 0;
}

static void mtk_cooler_mdulthro_unregister_ltf(void)
{
#if 0
    int i;
#endif
    mtk_cooler_mdulthro_dprintk("unregister ltf\n");

    if (cl_pa1_dev) {
        mtk_thermal_cooling_device_unregister(cl_pa1_dev);
        cl_pa1_dev = NULL;
    }

    if (cl_pa2_dev) {
        mtk_thermal_cooling_device_unregister(cl_pa2_dev);
        cl_pa2_dev = NULL;
    }

#if 0
  for (i = MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO; i-- > 0; )
  {
    if (cl_mdulthro_dev[i])
    {
      mtk_thermal_cooling_device_unregister(cl_mdulthro_dev[i]);
      cl_mdulthro_dev[i] = NULL;
      cl_mdulthro_state[i] = 0;
    }
  }
#endif
}

#if 0
static int _mtk_cl_mdulthro_proc_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    char *p = buf;

    mtk_cooler_mdulthro_dprintk("[_mtk_cl_mdulthro_proc_read] invoked.\n");

    /**
     * The format to print out: 
     *  kernel_log <0 or 1>
     *  <mtk-cl-mdulthro<ID>> <bcc limit>
     *  ..
     */
    if (NULL == data)
    {
        mtk_cooler_mdulthro_dprintk("[_mtk_cl_mdulthro_proc_read] null data\n");
    }
    else
    {
        int i = 0;

        p += sprintf(p, "klog %d\n", cl_mdulthro_klog_on);
        p += sprintf(p, "curr_limit %d\n", cl_mdulthro_cur_limit);

        for (; i < MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO; i++)
        {
            int limit;
            unsigned int curr_state;
            
            MTK_CL_MDULTHRO_GET_LIMIT(limit, cl_mdulthro_state[i]);
            MTK_CL_MDULTHRO_GET_CURR_STATE(curr_state, cl_mdulthro_state[i]);
        
            p += sprintf(p, "mtk-cl-mdulthro%02d %d mA, state %d\n", i, limit, curr_state);
        }
    }

    *start = buf + off;

    len = p - buf;
    if (len > off)
        len -= off;
    else
        len = 0;

    return len < count ? len  : count;
}

static ssize_t _mtk_cl_mdulthro_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int len = 0;
    char desc[128];
    int klog_on, limit0, limit1, limit2;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len))
    {
        return 0;
    }
    desc[len] = '\0';

    /**
     * sscanf format <klog_on> <mtk-cl-mdulthro00 limit> <mtk-cl-mdulthro01 limit> ...
     * <klog_on> can only be 0 or 1
     * <mtk-cl-mdulthro00 limit> can only be positive integer or -1 to denote no limit
     */

    if (NULL == data)
    {
        mtk_cooler_mdulthro_dprintk("[_mtk_cl_mdulthro_proc_write] null data\n");
        return -EINVAL;
    }

    // WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 3
#if (3 == MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO)
    MTK_CL_MDULTHRO_SET_LIMIT(-1, cl_mdulthro_state[0]);
    MTK_CL_MDULTHRO_SET_LIMIT(-1, cl_mdulthro_state[1]);
    MTK_CL_MDULTHRO_SET_LIMIT(-1, cl_mdulthro_state[2]);

    if (1 <= sscanf(desc, "%d %d %d %d",
                    &klog_on, &limit0, &limit1, &limit2))
    {
        if (klog_on == 0 || klog_on == 1)
        {
          cl_mdulthro_klog_on = klog_on;
        }

        if (limit0 >= -1) MTK_CL_MDULTHRO_SET_LIMIT(limit0, cl_mdulthro_state[0]);
        if (limit1 >= -1) MTK_CL_MDULTHRO_SET_LIMIT(limit1, cl_mdulthro_state[1]);
        if (limit2 >= -1) MTK_CL_MDULTHRO_SET_LIMIT(limit2, cl_mdulthro_state[2]);
        
        return count;
    }
    else
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO!"
#endif
    {
        mtk_cooler_mdulthro_dprintk("[_mtk_cl_mdulthro_proc_write] bad argument\n");
    }

    return -EINVAL;
}
#endif

int md_tx_thro_read( char *buf, char **start, off_t offset, int count, int *eof, void *data )
{
	count = sprintf(buf, "%lu\n", tx_throughput);

	mtk_cooler_mdulthro_dprintk("[%s] tx=%lu\n", __func__, tx_throughput);

	return count;
}

/*New Wifi throttling Algo+*/
int mdulthro_param_read( char *buf, char **start, off_t offset, int count, int *eof, void *data )
{
	int ret;
	char tmp[MAX_LEN] = {0};

	sprintf(tmp, "[up]\t%3d(sec)\t%2d/%2d\n[low]\t%3d(sec)\t%2d/%2d\nrst=%2d\ninterval=%d\n", up_duration, up_numerator, up_denominator, \
								low_duration, low_numerator, low_denominator, low_rst_max, polling_interval);
	ret = strlen(tmp);

	memcpy(buf, tmp, ret*sizeof(char));

	mtk_cooler_mdulthro_dprintk_always("[%s] [up]%d %d/%d, [low]%d %d/%d, rst=%d, interval=%d\n", __func__, up_duration, \
		up_numerator, up_denominator, low_duration, low_numerator, low_denominator, low_rst_max, polling_interval);

	return ret;
}

ssize_t mdulthro_param_write( struct file *filp, const char __user *buf, unsigned long len, void *data )
{
	char desc[MAX_LEN] = {0};

	unsigned int tmp_up_dur = 30;
	unsigned int tmp_up_den = 2;
	unsigned int tmp_up_num = 1;

	unsigned int tmp_low_dur = 3;
	unsigned int tmp_low_den = 2;
	unsigned int tmp_low_num = 3;

	unsigned int tmp_low_rst_max = 3;

	int tmp_polling_interval = 1;

	unsigned int tmp_log = 0;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len)) {
		return -EFAULT;
	}

	if (sscanf(desc, "%d %d/%d %d %d/%d %d %d", &tmp_up_dur, &tmp_up_num, &tmp_up_den, &tmp_low_dur, \
								&tmp_low_num, &tmp_low_den, &tmp_low_rst_max, &tmp_polling_interval) == 8) {

		up_duration = tmp_up_dur;
		up_denominator = tmp_up_den;
		up_numerator = tmp_up_num;

		low_duration = tmp_low_dur;
		low_denominator = tmp_low_den;
		low_numerator = tmp_low_num;

		low_rst_max = tmp_low_rst_max;
		polling_interval = tmp_polling_interval;

		over_up_time = 0;
		below_low_time = 0;
		low_rst_time = 0;

		mtk_cooler_mdulthro_dprintk_always("[%s] %s [up]%d %d/%d, [low]%d %d/%d, rst=%d, interval\n", __func__, desc, up_duration, \
			up_numerator, up_denominator, low_duration, low_numerator, low_denominator, low_rst_max, polling_interval);

		return len;
	} else if (sscanf(desc, "log=%d", &tmp_log) == 1) {
		if (tmp_log == 1)
			cl_mdulthro_klog_on = 1;
		else
			cl_mdulthro_klog_on = 0;

		return len;
	} else {
		mtk_cooler_mdulthro_dprintk_always("[%s] bad argument = %s\n", __func__, desc);
	}
    return -EINVAL;
}
/*New Wifi throttling Algo-*/

int mdulthro_pid_read( char *buf, char **start, off_t offset , int count, int *eof, void *data )
{
	int ret;
	char tmp[MAX_LEN] = {0};

	sprintf(tmp, "%d\n", tm_input_pid);
	ret = strlen(tmp);

	memcpy(buf, tmp, ret*sizeof(char));

	mtk_cooler_mdulthro_dprintk_always("[%s] %s = %d\n", __func__, buf, tm_input_pid);

	return ret;
}

ssize_t mdulthro_pid_write( struct file *filp, const char __user *buf, unsigned long len, void *data )
{
	int ret = 0;
	char tmp[MAX_LEN] = {0};

	len = (len < (MAX_LEN-1)) ? len : (MAX_LEN-1);
	/* write data to the buffer */
	if ( copy_from_user(tmp, buf, len) ) {
		return -EFAULT;
	}

	ret = kstrtouint(tmp, 10, &tm_input_pid);
	if (ret)
		WARN_ON(1);

	mtk_cooler_mdulthro_dprintk_always("[%s] %s = %d\n", __func__, tmp, tm_input_pid);

	return len;
}

static int mdulthro_proc_register(void)
{
    struct proc_dir_entry *entry = NULL;
    struct proc_dir_entry *mdulthro_proc_dir = NULL;

    mtk_cooler_mdulthro_dprintk("[%s]\n", __func__);

    mdulthro_proc_dir = proc_mkdir("mdulthro", NULL);
    if (!mdulthro_proc_dir) {
        mtk_cooler_mdulthro_dprintk("[%s]: mkdir /proc/mdulthro failed\n", __func__);
    } else {

#if 0
        entry = create_proc_entry("mdulthro", S_IRUGO | S_IWUSR | S_IWGRP, mdulthro_proc_dir);
        if (NULL != entry)
        {
          entry->read_proc = _mtk_cl_mdulthro_proc_read;
          entry->write_proc = _mtk_cl_mdulthro_proc_write;
          entry->data = cl_mdulthro_state;
          entry->gid = 1000; // allow system process to write this proc
        }
#endif

        entry = create_proc_entry("tm_pid", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, mdulthro_proc_dir);
        if (entry) {
            entry->read_proc = mdulthro_pid_read;
            entry->write_proc = mdulthro_pid_write;
            entry->gid = 1000; // allow system process to write this proc
        }

        entry = create_proc_entry("mdulthro_param", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, mdulthro_proc_dir);
        if (entry) {
            entry->read_proc = mdulthro_param_read;
            entry->write_proc = mdulthro_param_write;
            entry->gid = 1000; // allow system process to write this proc
        }

        entry = create_proc_entry("tx_thro", S_IRUGO | S_IWUSR, mdulthro_proc_dir);
        if (entry) {
            entry->read_proc = md_tx_thro_read;
        }
    }
    return 0;
}

static int __init mtk_cooler_mdulthro_init(void)
{
    int err = 0;
#if 0
    int i;
#endif

#if 0
    for (i = MAX_NUM_INSTANCE_MTK_COOLER_MDULTHRO; i-- > 0; )
    {
        cl_mdulthro_dev[i] = NULL;
        cl_mdulthro_state[i] = 0;
    }
#endif

    /* init a timer for stats tx bytes */
    wmt_stats_info.pre_time = 0;
    wmt_stats_info.pre_tx_bytes = 0;

    init_timer(&wmt_stats_timer);
    wmt_stats_timer.function = (void *)&wmt_cal_stats;
    wmt_stats_timer.data = (unsigned long) &wmt_stats_info;
    wmt_stats_timer.expires = jiffies + 1 * HZ;
    add_timer(&wmt_stats_timer);

    err = mdulthro_proc_register();
    if(err)
        return err;


    //cl_mdulthro_dev = NULL;

    mtk_cooler_mdulthro_dprintk("init\n");

    err = mtk_cooler_mdulthro_register_ltf();
    if (err)
        goto err_unreg;

    return 0;

err_unreg:
    mtk_cooler_mdulthro_unregister_ltf();
    return err;
}

static void __exit mtk_cooler_mdulthro_exit(void)
{
    mtk_cooler_mdulthro_dprintk("exit\n");

    /* remove the proc file */
    //remove_proc_entry("driver/mtk-cl-mdulthro", NULL);

    mtk_cooler_mdulthro_unregister_ltf();

    del_timer(&wmt_stats_timer);
}

module_init(mtk_cooler_mdulthro_init);
module_exit(mtk_cooler_mdulthro_exit);



