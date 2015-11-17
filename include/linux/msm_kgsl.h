#ifndef _MSM_KGSL_H
#define _MSM_KGSL_H

#include <uapi/linux/msm_kgsl.h>

/* Limits mitigations APIs */
void *kgsl_pwr_limits_add(enum kgsl_deviceid id);
void kgsl_pwr_limits_del(void *limit);
int kgsl_pwr_limits_set_freq(void *limit, unsigned int freq);
void kgsl_pwr_limits_set_default(void *limit);
unsigned int kgsl_pwr_limits_get_freq(enum kgsl_deviceid id);

#endif /* _MSM_KGSL_H */
