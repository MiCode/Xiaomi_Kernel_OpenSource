#include <linux/slab.h>
#include "../common/powerinsight_plat.h"
#include "../common/powerinsight_gpu.h"
#include "../gpu_stats/gpu_stats.h"
#include <linux/module.h>

static atomic_t stats_enable = ATOMIC_INIT(0);
static int freq_num = -1;
static struct powerinsight_gpu_opp_stats *stat_data = NULL;

extern int powerinsight_gpu_query_opp_cost(struct powerinsight_gpu_opp_stats *report, int opp_num);
extern unsigned int powerinsight_gpu_get_opp_cnt(void);

static int powerinsight_set_gpu_enable(int enable)
{
    atomic_set(&stats_enable, enable ? 1 : 0);

    return 0;
}

static int powerinsight_get_gpu_info(struct powerinsight_gpu_freq_info *data, int num)
{
    int ret, i;

    if (!atomic_read(&stats_enable))
        return -EPERM;

    if (!data) {
        pr_err("powerinsight_get_gpu_info: NULL data pointer\n");
        return -EINVAL;
    }

    if (num != freq_num) {
        pr_err("powerinsight_get_gpu_info: Invalid num %d != stored freq_num %d\n", 
               num, freq_num);
        return -EINVAL;
    }

    if (!stat_data) {
        stat_data = kzalloc(freq_num * sizeof(struct powerinsight_gpu_opp_stats), GFP_KERNEL);
        if (!stat_data) {
            pr_err("powerinsight_get_gpu_info: Failed to allocate %zu bytes for stat_data\n",
                   freq_num * sizeof(struct powerinsight_gpu_opp_stats));
            return -ENOMEM;
        }
    }

    ret = powerinsight_gpu_query_opp_cost(stat_data, freq_num);

    if (!ret) {
        for (i = 0; i < freq_num; i++) {
            data[i].freq = stat_data[i].freq;
            data[i].run_time = stat_data[i].active;
            data[i].idle_time = stat_data[i].idle;
        }
    } else {
        pr_err("powerinsight_get_gpu_info: powerinsight_gpu_query_opp_cost failed with ret = %d\n", ret);
    }

    return ret;
}

static int powerinsight_get_gpu_freq_num(void)
{
    int num;
    
    num = (int)powerinsight_gpu_get_opp_cnt();

    if (freq_num < 0)
        freq_num = num;

    return num;
}

static struct powerinsight_gpu_stats_ops gpu_ops = {
    .enable = powerinsight_set_gpu_enable,
    .get_stats = powerinsight_get_gpu_info,
    .get_num = powerinsight_get_gpu_freq_num,
};

int powerinsight_gpu_freq_stats_init(void)
{
    powerinsight_register_module_ops(POWERINSIGHT_MODULE_GPU, &gpu_ops);

    return 0;
}

void powerinsight_gpu_freq_stats_exit(void)
{
    powerinsight_unregister_module_ops(POWERINSIGHT_MODULE_GPU);
    atomic_set(&stats_enable, 0);
    freq_num = -1;

    if (stat_data) {
        kfree(stat_data);
        stat_data = NULL;
    }
}
