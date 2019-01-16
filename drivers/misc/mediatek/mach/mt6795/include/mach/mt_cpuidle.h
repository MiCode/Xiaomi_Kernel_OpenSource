#ifndef MT_CPUIDLE_H
#define MT_CPUIDLE_H

// cpu dormant return code
#define MT_CPU_DORMANT_RESET 0
#define MT_CPU_DORMANT_ABORT  1
#define MT_CPU_DORMANT_BREAK   2
#define MT_CPU_DORMANT_BYPASS  8
#define MT_CPU_DORMANT_BREAK_V(v)  (MT_CPU_DORMANT_BREAK | ((v)<<0x8))

#define CPU_PM_BREAK   (0)
#define IRQ_PENDING_1  (1)
#define IRQ_PENDING_2  (2)
#define IRQ_PENDING_3  (3)


// CPU dormant flags
#define DORMANT_BREAK_CHECK 	(1<<0)
#define DORMANT_SKIP_1		(1<<1)
#define DORMANT_SKIP_2		(1<<2)
#define DORMANT_SKIP_3		(1<<3)

#define DORMANT_LOUIS_OFF	(1<<8)
#define DORMANT_INNER_OFF	(1<<9)
#define DORMANT_OUTER_OFF	(1<<10)
#define DORMANT_CPUSYS_OFF	(1<<11)
#define DORMANT_GIC_OFF		(1<<12)
#define DORMANT_SNOOP_OFF	(1<<13)


#define DORMANT_ALL_OFF 	( DORMANT_OUTER_OFF	\
				  | DORMANT_INNER_OFF	\
				  | DORMANT_CPUSYS_OFF	\
				  | DORMANT_LOUIS_OFF	\
				  | DORMANT_GIC_OFF	\
				  | DORMANT_SNOOP_OFF )
#define DORMANT_MODE_MASK	(0x0ffff00)

//depreded #define CPU_DORMANT_MODE (DORMANT_LOUIS_OFF)
#define CPU_SHUTDOWN_MODE (DORMANT_ALL_OFF)

#define CPU_MCDI_MODE (DORMANT_LOUIS_OFF)
#define CPU_SODI_MODE (DORMANT_LOUIS_OFF                \
                       | DORMANT_CPUSYS_OFF             \
                       | DORMANT_GIC_OFF                \
                       | DORMANT_SNOOP_OFF)

#define CPU_DEEPIDLE_MODE (DORMANT_LOUIS_OFF            \
                           | DORMANT_CPUSYS_OFF         \
                           | DORMANT_GIC_OFF            \
                           | DORMANT_SNOOP_OFF)

#define CPU_SUSPEND_MODE (DORMANT_ALL_OFF)

#if 1
#define IS_DORMANT_SKIP_1(a) 		(((a) & DORMANT_SKIP_1) == DORMANT_SKIP_1)
#define IS_DORMANT_SKIP_2(a) 		(((a) & DORMANT_SKIP_2) == DORMANT_SKIP_2)
#define IS_DORMANT_SKIP_3(a) 		(((a) & DORMANT_SKIP_3) == DORMANT_SKIP_3)
#else 
#define IS_DORMANT_SKIP_1(a) 		(0)
#define IS_DORMANT_SKIP_2(a) 		(0)
#define IS_DORMANT_SKIP_3(a) 		(0)
#endif


#define IS_DORMANT_BREAK_CHECK(a) 	(((a) & DORMANT_BREAK_CHECK) == DORMANT_BREAK_CHECK)
#define IS_DORMANT_SNOOP_OFF(a) 	(((a) & DORMANT_SNOOP_OFF) == DORMANT_SNOOP_OFF)
#define IS_DORMANT_INNER_OFF(a) 	(((a) & DORMANT_INNER_OFF) == DORMANT_INNER_OFF)
#define IS_DORMANT_CPUSYS_OFF(a) 	(((a) & DORMANT_CPUSYS_OFF) == DORMANT_CPUSYS_OFF)
#define IS_DORMANT_GIC_OFF(a) 		(((a) & DORMANT_GIC_OFF) == DORMANT_GIC_OFF)
#define IS_CPU_SHUTDOWN_MODE(a) 	(((a) & MODE_MASK) == CPU_SHUTDOWN_MODE)
#define IS_CPU_DORMANT_MODE(a) 		(((a) & MODE_MASK) == CPU_DORMANT_MODE)

#define _IS_DORMANT_SET(flag, feature)   (((flag) & (feature)) == (feature))


#if 1 //defined(MT_CPUIDLE)
//cpuidle arch
int mt_cpu_dormant_init(void);

/** mt_cpu_dormant: 
 * cpu do the context save and issue WFI to SPM for trigger power-down, 
 * and finally restore context after reset
 * 
 * input:
 * data - the flags to decide detail of flow. a bitwise arguments:
 * 	-- CPU_DORMANT_MODE 
 * 	-- CPU_SHUTDOWN_MODE
 *	-- (optional) DORMANT_BREAK_CHECK 
 *
 * return:
 * MT_CPU_DORMANT_RESET: cpu is reset from power-down state.
 * MT_CPU_DORMANT_ABORT: cpu issue WFI and return for a pending interrupt.
 * MT_CPU_DORMANT_BREAK:  cpu dormant flow broken before by validating a SPM interrupt.
 * MT_CPU_DORMANT_BYPASS: (for debug only) to bypass all dormant flow.
 **/
int mt_cpu_dormant(unsigned long data);

#else //#if defined(MT_CPUIDLE)
static int mt_cpu_dormant_init(void) { return 0; }
static int mt_cpu_dormant(unsigned long data) { return 0; }

#endif //#if defined(MT_CPUIDLE)

static inline int mt_cpu_dormant_interruptible(unsigned long data) 
{
	return mt_cpu_dormant(data | DORMANT_BREAK_CHECK);
}



#endif
