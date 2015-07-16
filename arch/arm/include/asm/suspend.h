#ifndef __ASM_ARM_SUSPEND_H
#define __ASM_ARM_SUSPEND_H

struct sleep_save_sp {
	u32 *save_ptr_stash;
	u32 save_ptr_stash_phys;
};

extern void cpu_resume(void);
extern int cpu_suspend(unsigned long);

extern int __cpu_suspend(unsigned long, int (*fn)(unsigned long));
extern int __cpu_suspend_enter(unsigned long arg, int (*fn)(unsigned long),
							unsigned int);

#endif
