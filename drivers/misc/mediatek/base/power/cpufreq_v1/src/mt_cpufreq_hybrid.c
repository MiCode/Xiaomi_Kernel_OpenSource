/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mt_io.h>
#include <mt-plat/aee.h>

#ifdef CONFIG_HYBRID_CPU_DVFS
#include "spm2_ipi.h"
#include "mt_cpufreq_hybrid.h"

int dvfs_to_spm2_command(u32 cmd, struct cdvfs_data *cdvfs_d)
{
#define OPT				(1) /* reserve for extensibility */
#define DVFS_D_LEN		(4) /* # of cmd + arg0 + arg1 + ... */
	unsigned int len = DVFS_D_LEN;
	int ack_data;
	unsigned int ret = 0;

	pr_debug("#@# %s(%d) cmd %x\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case IPI_DVFS_INIT:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to initialize PowerMCU DVFS, segment code = %d\n", cdvfs_d->u.set_fv.arg[0]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_DVFS:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to set cluster%d to freq%d\n", cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_MIN_MAX:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to set cluster%d MIN/MAX idx to (%d, %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1], cdvfs_d->u.set_fv.arg[2]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_CLUSTER_ON_OFF:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to set cluster%d ON/OFF state to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_SET_VOLT:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to set cluster%d volt to %d)\n",
			cdvfs_d->u.set_fv.arg[0], cdvfs_d->u.set_fv.arg[1]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_GET_VOLT:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to get volt from Buck%d\n", cdvfs_d->u.set_fv.arg[0]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		pr_err("Get volt = %d\n", ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_GET_FREQ:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to get freq from cluster%d\n", cdvfs_d->u.set_fv.arg[0]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		pr_err("Get freq = %d\n", ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	case IPI_PAUSE_DVFS:
		cdvfs_d->cmd = cmd;

		pr_err("I'd like to set dvfs enable status to %d\n", cdvfs_d->u.set_fv.arg[0]);

		/* ret = spm2_ipi_send_sync(iSPEED_DEV_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data); */
		ret = spm2_ipi_send_sync(IPI_ID_CPU_DVFS, OPT, cdvfs_d, len, &ack_data);
		if (ret != 0) {
			pr_err("#@# %s(%d) spm2_ipi_send_sync ret %d\n", __func__, __LINE__, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_err("#@# %s(%d) cmd(%d) return %d\n", __func__, __LINE__, cmd, ret);
		}
		break;

	default:
		pr_err("#@# %s(%d) cmd(%d) wrong!!!\n", __func__, __LINE__, cmd);
		break;
	}

	return ret;
}

#include <linux/of_address.h>

static void __iomem *csram_base;

#define csram_read(offs)		__raw_readl(csram_base + (offs))
#define csram_write(offs, val)		mt_reg_sync_writel(val, csram_base + (offs))

void print_log_content(unsigned int *local_buf)
{
	struct cpu_dvfs_log *log_box = (struct cpu_dvfs_log *)local_buf;

	pr_crit("log_box->time_stamp_log = 0x%x\n", log_box->time_stamp_log);
	pr_crit("log_box->vproc_ll_log = 0x%x\n", log_box->vproc_ll_log);
	pr_crit("log_box->opp_ll_log = 0x%x\n", log_box->opp_ll_log);
	pr_crit("log_box->wfi_ll_log = 0x%x\n", log_box->wfi_ll_log);
	pr_crit("log_box->vsram_ll_log = 0x%x\n", log_box->vsram_ll_log);
	pr_crit("log_box->vproc_l_log = 0x%x\n", log_box->vproc_l_log);
	pr_crit("log_box->opp_l_log = 0x%x\n", log_box->opp_l_log);
	pr_crit("log_box->wfi_l_log = 0x%x\n", log_box->wfi_l_log);
	pr_crit("log_box->vsram_l_log = 0x%x\n", log_box->vsram_l_log);
	pr_crit("log_box->vproc_b_log = 0x%x\n", log_box->vproc_b_log);
	pr_crit("log_box->opp_b_log = 0x%x\n", log_box->opp_b_log);
	pr_crit("log_box->wfi_b_log = 0x%x\n", log_box->wfi_b_log);
	pr_crit("log_box->vsram_b_log = 0x%x\n", log_box->vsram_b_log);
	pr_crit("log_box->vproc_cci_log = 0x%x\n", log_box->vproc_cci_log);
	pr_crit("log_box->opp_cci_log = 0x%x\n", log_box->opp_cci_log);
	pr_crit("log_box->wfi_cci_log = 0x%x\n", log_box->wfi_cci_log);
	pr_crit("log_box->vsram_cci_log = 0x%x\n", log_box->vsram_cci_log);
}

void print_log_content_time_stamp(unsigned int *local_buf)
{
	struct cpu_dvfs_log *log_box = (struct cpu_dvfs_log *)local_buf;
	unsigned long long time_stamp_buf = 0;

	time_stamp_buf = ((unsigned long long)log_box->time_stamp_log << 32) |
		(unsigned long long)(log_box->time_stamp_2_log);
	pr_crit("log_box->time_stamp_log = 0x%llu\n", time_stamp_buf);
}

static struct hrtimer nfy_trig_timer;
static struct task_struct *dvfs_nfy_task;
static atomic_t dvfs_nfy_req = ATOMIC_INIT(0);

#define OFFS_LOG_S		0x0
#define OFFS_LOG_E		0x960
#define MAX_LOG_FETCH		20
#define LOG_ENTRY 6

/* log_box[MAX_LOG_FETCH] is also used to save last log entry */
/* static struct dvfs_log log_box[1 + MAX_LOG_FETCH]; */
static unsigned int next_log_offs = OFFS_LOG_S;

static void fetch_dvfs_log_and_notify(void)
{
	/* int i, j; */
	int j;
	unsigned int buf[LOG_ENTRY] = {0};

	/* pr_crit("DVFS - Do thread task Begin!\n"); */

	/* for (i = 1; i <= MAX_LOG_FETCH; i++) { */
		for (j = 0; j < LOG_ENTRY; j++) {
			/* buf[i] = csram_read(i*4); */

			buf[j] = csram_read(next_log_offs);
			/* pr_crit("DVFS - buf[%d] = addr[0x%x] = 0x%x!\n",
				j, next_log_offs, buf[j]); */
			next_log_offs += 4;

			if (next_log_offs >= OFFS_LOG_E)
				next_log_offs = OFFS_LOG_S;
		}
		/* print_log_content_time_stamp(buf);
		print_log_content(buf);
	}

	pr_crit("DVFS - Do thread task Done!\n"); */
}

static inline void start_notify_trigger_timer(u32 intv_ms)
{
	hrtimer_start(&nfy_trig_timer, ms_to_ktime(intv_ms), HRTIMER_MODE_REL);
}

static inline void stop_notify_trigger_timer(void)
{
	hrtimer_cancel(&nfy_trig_timer);
}

static inline void kick_kthread_to_notify(void)
{
	atomic_inc(&dvfs_nfy_req);
	wake_up_process(dvfs_nfy_task);
}

static int dvfs_nfy_task_fn(void *data)
{
	while (1) {
		set_current_state(TASK_UNINTERRUPTIBLE);

		if (atomic_read(&dvfs_nfy_req) <= 0) {
			schedule();
			continue;
		}

		__set_current_state(TASK_RUNNING);

		fetch_dvfs_log_and_notify();
		atomic_dec(&dvfs_nfy_req);
	}

	return 0;
}

#define DVFS_NOTIFY_INTV	20		/* ms */
static enum hrtimer_restart nfy_trig_timer_fn(struct hrtimer *timer)
{
	kick_kthread_to_notify();

	hrtimer_forward_now(timer, ms_to_ktime(DVFS_NOTIFY_INTV));

	return HRTIMER_RESTART;
}

static int create_resource_for_dvfs_notify(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 2 };	/* lower than WDK */

	/* init hrtimer */
	hrtimer_init(&nfy_trig_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	nfy_trig_timer.function = nfy_trig_timer_fn;

	/* create kthread */
	dvfs_nfy_task = kthread_create(dvfs_nfy_task_fn, NULL, "dvfs_log_dump");

	if (IS_ERR(dvfs_nfy_task))
		return PTR_ERR(dvfs_nfy_task);

	sched_setscheduler_nocheck(dvfs_nfy_task, SCHED_FIFO, &param);
	get_task_struct(dvfs_nfy_task);

	return 0;
}

static int _mt_cmcu_pdrv_probe(struct platform_device *pdev)
{
	int r;

	csram_base = of_iomap(pdev->dev.of_node, 0);

	if (!csram_base)
		return -ENOMEM;

	r = create_resource_for_dvfs_notify();
	if (r) {
		pr_crit("FAILED TO CREATE RESOURCE FOR NOTIFY (%d)\n", r);
		return r;
	}

	return 0;
}


static int _mt_cmcu_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id cmcu_of_match[] = {
	{ .compatible = "mediatek,mt6797-cmcu", },
	{}
};
#endif

static struct platform_driver _mt_cmcu_pdrv = {
	.probe = _mt_cmcu_pdrv_probe,
	.remove = _mt_cmcu_pdrv_remove,
	.driver = {
		   .name = "cmcu",
		   .owner = THIS_MODULE,
		   .of_match_table	= of_match_ptr(cmcu_of_match),
	},
};

int cpuhvfs_set_cluster_on_off(int cluster_id, int state)
{
	struct cdvfs_data cdvfs_d;

	/* Cluster, ON:1/OFF:0 */
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = state;

	dvfs_to_spm2_command(IPI_SET_CLUSTER_ON_OFF, &cdvfs_d);

	return 0;
}

int cpuhvfs_set_mix_max(int cluster_id, int base, int limit)
{
	struct cdvfs_data cdvfs_d;

	/* Cluster, MIN, MAX */
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = base;
	cdvfs_d.u.set_fv.arg[2] = limit;

	dvfs_to_spm2_command(IPI_SET_MIN_MAX, &cdvfs_d);

	return 0;
}

int cpuhvfs_get_freq(int cluster_id)
{
	struct cdvfs_data cdvfs_d;
	int ret = 0;

	/* Cluster, Freq */
	cdvfs_d.u.set_fv.arg[0] = cluster_id;

	ret = dvfs_to_spm2_command(IPI_GET_FREQ, &cdvfs_d);

	return ret;
}

int cpuhvfs_set_freq(int cluster_id, unsigned int freq)
{
	struct cdvfs_data cdvfs_d;

	/* Cluster, Freq */
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = freq;

	dvfs_to_spm2_command(IPI_SET_DVFS, &cdvfs_d);

	return 0;
}

int cpuhvfs_get_volt(int buck_id)
{
	struct cdvfs_data cdvfs_d;
	int ret = 0;

	/* Cluster, Volt */
	cdvfs_d.u.set_fv.arg[0] = buck_id;

	ret = dvfs_to_spm2_command(IPI_GET_VOLT, &cdvfs_d);

	return ret;
}

int cpuhvfs_set_volt(int cluster_id, unsigned int volt)
{
	struct cdvfs_data cdvfs_d;

	/* Cluster, Volt */
	cdvfs_d.u.set_fv.arg[0] = cluster_id;
	cdvfs_d.u.set_fv.arg[1] = volt;

	dvfs_to_spm2_command(IPI_SET_VOLT, &cdvfs_d);

	return 0;
}

int cpuhvfs_pause_dvfsp_running(enum pause_src src)
{
	struct cdvfs_data cdvfs_d;

	/* set on = 0 */
	cdvfs_d.u.set_fv.arg[0] = 0;

	dvfs_to_spm2_command(IPI_PAUSE_DVFS, &cdvfs_d);

	return 0;
}

void cpuhvfs_unpause_dvfsp_to_run(enum pause_src src)
{
	struct cdvfs_data cdvfs_d;

	/* set on = 1 */
	cdvfs_d.u.set_fv.arg[0] = 1;

	dvfs_to_spm2_command(IPI_PAUSE_DVFS, &cdvfs_d);
}

/*
* Module driver
*/
int cpuhvfs_module_init(void)
{
	struct cdvfs_data cdvfs_d;

	/* seg code */
	cdvfs_d.u.set_fv.arg[0] = 0;

	dvfs_to_spm2_command(IPI_DVFS_INIT, &cdvfs_d);

	return 0;
}

static int __init cmcu_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&_mt_cmcu_pdrv);

	if (ret)
		pr_err("fail to register cmcu driver @ %s()\n", __func__);

	start_notify_trigger_timer(DVFS_NOTIFY_INTV);

	return ret;
}

static int cpuhvfs_pre_module_init(void)
{
	int r;
	/* struct cpuhvfs_data *cpuhvfs = &g_cpuhvfs;

	r = cpuhvfs->dvfsp->init_dvfsp(cpuhvfs->dvfsp); */
	/* Test */
	struct cdvfs_data cdvfs_d;

	/* Cluster, Freq */
	cdvfs_d.u.set_fv.arg[0] = 0;
	cdvfs_d.u.set_fv.arg[1] = 1950000;

	dvfs_to_spm2_command(IPI_SET_DVFS, &cdvfs_d);

	r = cmcu_module_init();
	if (r) {
		/* cpuhvfs_err("FAILED TO INIT DVFS POWERMCU (%d)\n", r); */
		return r;
	}

	/* init_cpuhvfs_debug_repo(cpuhvfs); */

	return 0;
}
fs_initcall(cpuhvfs_pre_module_init);
#endif
MODULE_DESCRIPTION("Hybrid CPU DVFS Driver v0.1");
MODULE_LICENSE("GPL");
