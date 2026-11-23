#include <linux/slab.h>
#include <linux/printk.h>
#include "gpu_stats.h"

static struct powerinsight_gpu_opp_stats *freq_stats;
static unsigned int opp_size;

int powerinsight_gpu_query_opp_cost(struct powerinsight_gpu_opp_stats *report, int opp_num)
{
	if (report == NULL)
		return -EINVAL;
	if (freq_stats == NULL)
		return -EINVAL;
	if (opp_size == opp_num) {
		memcpy(report, freq_stats, opp_num * sizeof(struct powerinsight_gpu_opp_stats));
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(powerinsight_gpu_query_opp_cost);

unsigned int powerinsight_gpu_get_opp_cnt(void)
{
	return opp_size;
}
EXPORT_SYMBOL_GPL(powerinsight_gpu_get_opp_cnt);

void powerinsight_gpu_update_opp_cost(unsigned long total_time,
	unsigned long busy_time, unsigned int idx)
{
	if (freq_stats && idx < opp_size) {
		freq_stats[idx].active += busy_time;
		freq_stats[idx].idle += total_time - busy_time;
	}

	return;
}
EXPORT_SYMBOL_GPL(powerinsight_gpu_update_opp_cost);

int powerinsight_gpu_init_opp_cost(u32 *freq_table, int max_state)
{
    int i;

    if (!freq_table || max_state <= 0) {
        pr_err("Invalid frequency table or state count\n");
        return -EINVAL;
    }

    opp_size = max_state;
    freq_stats = kzalloc(sizeof(*freq_stats) * opp_size, GFP_KERNEL);
    if (!freq_stats)
        return -ENOMEM;

    for (i = 0; i < opp_size; i++)
        freq_stats[i].freq = freq_table[i];

    return 0;
}
EXPORT_SYMBOL_GPL(powerinsight_gpu_init_opp_cost);

void powerinsight_gpu_close_opp_cost(void)
{
	opp_size = 0;
	if (freq_stats) {
		kfree(freq_stats);
		freq_stats = NULL;
	}
}
EXPORT_SYMBOL_GPL(powerinsight_gpu_close_opp_cost);

MODULE_LICENSE("GPL");
