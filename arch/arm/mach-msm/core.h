#include <asm/soc.h>

extern struct arm_soc_smp_init_ops	msm_soc_smp_init_ops;
extern struct arm_soc_smp_ops		msm_soc_smp_ops;
extern struct arm_soc_desc		msm_soc_desc;

extern void msm_cpu_die(unsigned int cpu);
