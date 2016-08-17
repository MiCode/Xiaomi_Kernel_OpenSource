struct arm_soc_desc;
extern struct arm_soc_desc tegra_soc_desc;

struct arm_soc_smp_init_ops;
struct arm_soc_smp_ops;
extern struct arm_soc_smp_init_ops	tegra_soc_smp_init_ops;
extern struct arm_soc_smp_ops		tegra_soc_smp_ops;

extern unsigned long tegra_tsec_start;
extern unsigned long tegra_tsec_size;

#ifdef CONFIG_CACHE_L2X0
void tegra_init_cache(bool init);
#else
static inline void tegra_init_cache(bool init) {}
#endif

extern void tegra_cpu_die(unsigned int cpu);
extern int tegra_cpu_kill(unsigned int cpu);
extern unsigned long tegra_avp_kernel_start;
extern unsigned long tegra_avp_kernel_size;
