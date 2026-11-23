#include "powerinsight_gpu_stats.h"

#include <linux/slab.h>
#include "utils/powerinsight_utils.h"
#include "powerinsight_ioctl.h"

#define IOC_GPU_ENABLE POWERINSIGHT_GPU_DIR_IOC(_IOC_WRITE, 1, int)
#define IOC_GPU_INFO_GET POWERINSIGHT_GPU_DIR_IOC(_IOC_READ, 2, struct powerinsight_gpu_stats)

static int powerinsight_set_gpu_enable(void __user *argp);
static int powerinsight_get_gpu_info(void __user *argp);

static DEFINE_MUTEX(gpu_lock);
static int freq_num = 0;
static struct powerinsight_gpu_freq_info *freq_stats = NULL;
static struct powerinsight_gpu_stats_ops *stats_op = NULL;

long powerinsight_ioctl_gpu(unsigned int cmd, void __user *argp)
{
	int rc = 0;

	switch (cmd) {
	case IOC_GPU_ENABLE:
		rc = powerinsight_set_gpu_enable(argp);
		break;
	case IOC_GPU_INFO_GET:
		rc = powerinsight_get_gpu_info(argp);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}


static void powerinsight_reset_gpu_stats(void)
{
	mutex_lock(&gpu_lock);
	if (freq_stats) {
		kfree(freq_stats);
		freq_stats = NULL;
	}
	mutex_unlock(&gpu_lock);
	freq_num = 0;
	stats_op = NULL;
}

int powerinsight_set_gpu_enable(void __user *argp)
{
	int enable = 1;

	if (!stats_op)
		return -EOPNOTSUPP;

	if (!stats_op->enable)
		return -EOPNOTSUPP;

	if (!stats_op->get_num)
		return -EOPNOTSUPP;

	if (get_enable_value(argp, &enable)) {
		pr_err("Failed to get value from user");
		goto enable_failed;
	}

	if (stats_op->enable(enable)) {
		pr_err("Failed to enable stats");
		goto enable_failed;
	}

	if (enable) {
		freq_num = stats_op->get_num();
		if (freq_num <= 0) {
			pr_err("Failed to get freq num: %d", freq_num);
			goto init_failed;
		}
	} else {
		powerinsight_reset_gpu_stats();
	}

	return 0;

init_failed:
	(void)stats_op->enable(0);

enable_failed:
	powerinsight_gpu_unregister_ops();
	return -EFAULT;
}

static void powerinsight_add_gpu_delta_stats(
	struct powerinsight_gpu_stats *stats, int64_t freq, int64_t run_time, int64_t idle_time)
{
	if (unlikely((stats->num < 0) || (stats->num >= POWERINSIGHT_GPU_FREQ_MAX_SIZE))) {
		return;
	}

	if ((run_time <= 0) && (idle_time <= 0)) {
		return;
	}

	stats->array[stats->num].run_time = (run_time > 0) ? run_time : 0;
	stats->array[stats->num].idle_time = (idle_time > 0) ? idle_time : 0;
	stats->array[stats->num].freq = freq;
	stats->num++;
}

int powerinsight_get_gpu_info(void __user *argp)
{
	int i;
	struct powerinsight_gpu_freq_info *new_stats = NULL;
	struct powerinsight_gpu_stats delta_stats;

	if (!stats_op || !stats_op->get_num || !stats_op->get_stats || (freq_num <= 0)) {
		pr_err("GPU stats not supported");
		return -EOPNOTSUPP;
	}

	new_stats = kzalloc(freq_num * sizeof(struct powerinsight_gpu_freq_info), GFP_KERNEL);
	if (!new_stats) {
		pr_err("Failed to alloc");
		return -ENOMEM;
	}

	if (stats_op->get_stats(new_stats, freq_num)) {
		pr_err("Failed to get stats");
		kfree(new_stats);
		return -EFAULT;
	}

	mutex_lock(&gpu_lock);
	memset(&delta_stats, 0, sizeof(struct powerinsight_gpu_stats));
	for (i = 0; i < freq_num; i++) {
		if (new_stats[i].freq <= 0)
			continue;

		if (freq_stats && (freq_stats[i].freq == new_stats[i].freq)) {
			powerinsight_add_gpu_delta_stats(&delta_stats, new_stats[i].freq,
				new_stats[i].run_time - freq_stats[i].run_time,
				new_stats[i].idle_time - freq_stats[i].idle_time);
		} else {
			powerinsight_add_gpu_delta_stats(&delta_stats, new_stats[i].freq,
				new_stats[i].run_time, new_stats[i].idle_time);
		}
	}
	if (freq_stats) {
		kfree(freq_stats);
	}
	freq_stats = new_stats;
	mutex_unlock(&gpu_lock);

	if (delta_stats.num > 0) {
		if (unlikely(copy_to_user(argp, &delta_stats, sizeof(struct powerinsight_gpu_stats))))
			return -EFAULT;
	}

	return 0;
}

int powerinsight_gpu_register_ops(struct powerinsight_gpu_stats_ops *op)
{
	if (!stats_op)
		stats_op = op;
	return 0;
}

int powerinsight_gpu_unregister_ops(void)
{
	if (stats_op)
		stats_op = NULL;
	return 0;
}

void powerinsight_gpu_stats_init(void)
{
}

void powerinsight_gpu_stats_exit(void)
{
	powerinsight_reset_gpu_stats();
}
