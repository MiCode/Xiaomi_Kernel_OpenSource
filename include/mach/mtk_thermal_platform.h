#ifndef _MTK_THERMAL_PLATFORM_H
#define _MTK_THERMAL_PLATFORM_H

#include <linux/thermal.h>

extern
int mtk_thermal_get_cpu_info(int *nocores, int **cpufreq, int **cpuloading);

extern
int mtk_thermal_get_gpu_info(int *nocores, int **gpufreq, int **gpuloading);

extern
int mtk_thermal_get_batt_info(int *batt_voltage, int *batt_current, int *batt_temp);

extern
int mtk_thermal_get_extra_info(int *no_extra_attr,
			       char ***attr_names, int **attr_values, char ***attr_unit);

extern
int mtk_thermal_force_get_batt_temp(void);


enum {
	MTK_THERMAL_SCEN_CALL = 0x1
};

extern
unsigned int mtk_thermal_set_user_scenarios(unsigned int mask);

extern
unsigned int mtk_thermal_clear_user_scenarios(unsigned int mask);


#endif				/* _MTK_THERMAL_PLATFORM_H */
