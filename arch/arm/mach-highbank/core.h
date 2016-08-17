#include <asm/soc.h>

extern void highbank_set_cpu_jump(int cpu, void *jump_addr);
extern void highbank_clocks_init(void);
extern void highbank_restart(char, const char *);
extern void __iomem *scu_base_addr;
#ifdef CONFIG_DEBUG_HIGHBANK_UART
extern void highbank_lluart_map_io(void);
#else
static inline void highbank_lluart_map_io(void) {}
#endif

extern void highbank_cpu_die(unsigned int cpu);

extern struct arm_soc_smp_init_ops	highbank_soc_smp_init_ops;
extern struct arm_soc_smp_ops		highbank_soc_smp_ops;
