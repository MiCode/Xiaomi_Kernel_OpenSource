#ifndef __LX_ADAPTIVE_POWEROFF_VOLTAGE_H_
#define __LX_ADAPTIVE_POWEROFF_VOLTAGE_H_
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>

#define TEMP_DELTA  20  // 2℃
#define SW_LOW_BAT_OCP_CONF_MA  (-1000*1000)  // 1A
#define SW_LOW_BAT_UVLO_CONF_MV  (2900)

struct poweroff_voltage_config {
    int poweroff_voltage;
    int shutdown_delay_voltage;
    int termv;
};

struct poweroff_voltge_condition_count {
    int count_min;
    int count_max;
    const struct poweroff_voltage_config poweroff_conf;
};

struct poweroff_voltage_condition_temp_count {
    int temp_min;
    int temp_max;
    const struct poweroff_voltge_condition_count *count_conds;
};



struct poweroff_voltage_condition_temp_cyclecount {
    int temp_min;
    int temp_max;
    int cyclecount_min;
    int cyclecount_max;
    const struct poweroff_voltage_config poweroff_conf;
};

int select_poweroff_by_count(int temp, int count_x1000, struct poweroff_voltage_config *conf);
int select_poweroff_by_cyclecount(int temp, int cyclecount, struct poweroff_voltage_config *conf);
#endif //__LX_ADAPTIVE_POWEROFF_VOLTAGE_H_