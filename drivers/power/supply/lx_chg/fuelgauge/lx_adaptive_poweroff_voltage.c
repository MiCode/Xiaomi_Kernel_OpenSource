#include "lx_adaptive_poweroff_voltage.h"

#define CHG_INT_MAX (((int)((unsigned int)~0 >> 1))/1000)
#define COUNT_CONDS_MAX_CNT 4
static const struct poweroff_voltge_condition_count count_conds_temp1[COUNT_CONDS_MAX_CNT] = {
    /* TEMP1: -20℃ ~ -6℃*/
    {.count_min = -1, .count_max = 80,   \
        {.poweroff_voltage = 2800, .shutdown_delay_voltage = 2850, .termv = 2900} },
    {.count_min = 80, .count_max = 199, \
        {.poweroff_voltage = 2850, .shutdown_delay_voltage = 2900, .termv = 2950} },
    {.count_min = 199, .count_max = 559,\
        {.poweroff_voltage = 2900, .shutdown_delay_voltage = 2950, .termv = 3000} },
    {.count_min = 559, .count_max = CHG_INT_MAX, \
        {.poweroff_voltage = 2950, .shutdown_delay_voltage = 3000, .termv = 3050} },
};
static const struct poweroff_voltge_condition_count count_conds_temp2[COUNT_CONDS_MAX_CNT] = {
    /* TEMP2: -6℃ ~ 0℃*/
    {.count_min = -1, .count_max = 80, \
        {.poweroff_voltage = 2800, .shutdown_delay_voltage = 2850, .termv = 2900} },
    {.count_min = 80, .count_max = 199, \
        {.poweroff_voltage = 2950, .shutdown_delay_voltage = 3000, .termv = 3050} },
    {.count_min = 199, .count_max = 559, \
        {.poweroff_voltage = 3000, .shutdown_delay_voltage = 3050, .termv = 3100} },
    {.count_min = 559, .count_max = CHG_INT_MAX, \
        {.poweroff_voltage = 3050, .shutdown_delay_voltage = 3100, .termv = 3150} },
};
static const struct poweroff_voltge_condition_count count_conds_temp3[COUNT_CONDS_MAX_CNT] = {
    /* TEMP3 0℃ ~ 10℃*/
    {.count_min = -1, .count_max = 80, \
        {.poweroff_voltage = 2850, .shutdown_delay_voltage = 2900, .termv = 2900} },
    {.count_min = 80, .count_max = 199, \
        {.poweroff_voltage = 3000, .shutdown_delay_voltage = 3050, .termv = 3050} },
    {.count_min = 199, .count_max = 559, \
        {.poweroff_voltage = 3050, .shutdown_delay_voltage = 3100, .termv = 3100} },
    {.count_min = 559, .count_max = CHG_INT_MAX, \
        {.poweroff_voltage = 3100, .shutdown_delay_voltage = 3150, .termv = 3150} },
};
static const struct poweroff_voltge_condition_count count_conds_temp4[COUNT_CONDS_MAX_CNT] = {   
    /* TEMP4: 10℃ ~ 60℃*/
    {.count_min = -1, .count_max = 80, \
        {.poweroff_voltage = 3000, .shutdown_delay_voltage = 3050, .termv = 3050} },
    {.count_min = 80, .count_max = 199, \
        {.poweroff_voltage = 3100, .shutdown_delay_voltage = 3150, .termv = 3150} },
    {.count_min = 199, .count_max = 559, \
        {.poweroff_voltage = 3200, .shutdown_delay_voltage = 3250, .termv = 3250} },
    {.count_min = 559, .count_max = CHG_INT_MAX, \
        {.poweroff_voltage = 3300, .shutdown_delay_voltage = 3340, .termv = 3340} },
};

static const struct poweroff_voltage_condition_temp_count p19a_poweroff_vol_count_conds[] = {
    /*  -20℃ ~ -6℃*/
    {.temp_min = -200, .temp_max = -60,  count_conds_temp1 },
    /*  -6℃ ~ 0℃*/
    {.temp_min = -60, .temp_max = 0,  count_conds_temp2 },
    /*  0℃ ~ 10℃*/
    {.temp_min = 0, .temp_max = 100, count_conds_temp3 },
    /*  10℃ ~ 60℃*/
    {.temp_min = 100, .temp_max = 600, count_conds_temp4 },
};

static const struct poweroff_voltage_condition_temp_cyclecount p19a_poweroff_vol_cyclecount_conds[] = {
    /*  ~ 0℃*/
    {.temp_min = -1000, .temp_max = 0, .cyclecount_min = -1, .cyclecount_max = 600, \
        {.poweroff_voltage = 2800, .shutdown_delay_voltage = 2850, .termv = 2900} },
    {.temp_min = -1000, .temp_max = 0, .cyclecount_min = 600, .cyclecount_max = 1200, \
        {.poweroff_voltage = 3000, .shutdown_delay_voltage = 3050, .termv = 3100} },
    {.temp_min = -1000, .temp_max = 0, .cyclecount_min = 1200, .cyclecount_max = CHG_INT_MAX, \
        {.poweroff_voltage = 3050, .shutdown_delay_voltage = 3100, .termv = 3150} },

    /* 0℃ ~ */
    {.temp_min = 0, .temp_max = 2000, .cyclecount_min = -1, .cyclecount_max = 600, \
        {.poweroff_voltage = 3000, .shutdown_delay_voltage = 3050, .termv = 3050} },
    {.temp_min = 0, .temp_max = 2000, .cyclecount_min = 600, .cyclecount_max = 1200, \
        {.poweroff_voltage = 3200, .shutdown_delay_voltage = 3250, .termv = 3250} },
    {.temp_min = 0, .temp_max = 2000, .cyclecount_min = 1200, .cyclecount_max = CHG_INT_MAX, \
        {.poweroff_voltage = 3300, .shutdown_delay_voltage = 3340, .termv = 3340} },
};


int select_poweroff_by_count(int temp, int count_x1000, struct poweroff_voltage_config *conf)
{
    int ret = -EINVAL;
    int temp_index = 0xFF, i;
    int temp_min, temp_max;
    int count_max_x1000, count_min_x1000;
    static pre_temp_index = 0xFF;
    const struct poweroff_voltge_condition_count *count_conds;
    if(pre_temp_index != 0xFF){
        i = pre_temp_index;
        temp_min = p19a_poweroff_vol_count_conds[i].temp_min-TEMP_DELTA;
        temp_max = p19a_poweroff_vol_count_conds[i].temp_max+TEMP_DELTA;
        if(temp>= temp_min && temp<temp_max){
            temp_index = i;
        }
    }
    if(temp_index == 0xFF){
        for(i=0; i<ARRAY_SIZE(p19a_poweroff_vol_count_conds); i++){
            temp_min = p19a_poweroff_vol_count_conds[i].temp_min;
            temp_max = p19a_poweroff_vol_count_conds[i].temp_max;
            if(temp>= temp_min && temp<temp_max){
                temp_index = i;
                pre_temp_index = temp_index;
                break;
            }
        }
        pr_err("%s can not find conditions for temp:%d \n", __func__, temp);
        return ret;
    }
    count_conds = p19a_poweroff_vol_count_conds[temp_index].count_conds;
    for(i=0; i<COUNT_CONDS_MAX_CNT; i++){
        count_min_x1000 = count_conds[i].count_min*1000;
        count_max_x1000 = count_conds[i].count_max*1000;
        if(count_x1000>count_min_x1000 && count_x1000 <= count_max_x1000){
            memcpy(conf, &count_conds[i].poweroff_conf, sizeof(struct poweroff_voltage_config));
            ret = 0;
            break;
        }
        pr_err("%s can not find conditions for temp:%d count(x1000):%d \n", __func__, temp, count_x1000);
    }
#if 1 // 1 for debug
    if( ret==0 ){
        pr_info("%s temp:%d count(x1000):%d select index:%d [%d,%d,%d] \n", __func__, \
                temp, count_x1000, i+1,conf->poweroff_voltage, conf->shutdown_delay_voltage, conf->termv);
    } else {
        pr_err("%s no config for temp:%d count(x1000):%d!!! \n", __func__, temp, count_x1000);
    }
#endif
    return ret;
}

int select_poweroff_by_cyclecount(int temp, int cyclecount, struct poweroff_voltage_config *conf)
{
    int ret = -EINVAL;
    int i;
    for(i=0; i<ARRAY_SIZE(p19a_poweroff_vol_cyclecount_conds); i++){
        if( (temp>=p19a_poweroff_vol_cyclecount_conds[i].temp_min && temp<p19a_poweroff_vol_cyclecount_conds[i].temp_max) \
                && (cyclecount>p19a_poweroff_vol_cyclecount_conds[i].cyclecount_min && cyclecount<=p19a_poweroff_vol_cyclecount_conds[i].cyclecount_max) ){
            memcpy(conf, &p19a_poweroff_vol_cyclecount_conds[i].poweroff_conf, sizeof(struct poweroff_voltage_config));
            ret = 0;
            break;
        }
    }
#if 1 // 1 for debug
    if( ret==0 ){
        pr_info("%s temp:%d cyclecount:%d select index:%d [%d,%d,%d] \n", __func__, \
               temp, cyclecount, i, conf->poweroff_voltage, conf->shutdown_delay_voltage, conf->termv);
    } else {
        pr_err("%s no config for temp:%d cyclecount:%d!!! \n", __func__, temp, cyclecount);
    }
#endif
    return ret;
}