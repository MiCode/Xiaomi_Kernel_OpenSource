
extern struct semaphore smc_lock;
extern int forward_call_flag;
extern int irq_call_flag;
extern int fp_call_flag;
extern int keymaster_call_flag;
extern int teei_vfs_flag;
extern struct completion global_down_lock;
extern unsigned long teei_config_flag;

extern int get_current_cpuid(void);
