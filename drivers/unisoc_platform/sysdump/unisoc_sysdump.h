/* SPDX-License-Identifier: GPL-2.0 */

#if IS_ENABLED(CONFIG_SPRD_SYSDUMP)
/**
  * save extend debug information of modules in minidump, such as: cm4, iram...
  *
  * @name:       the name of the modules, and the string will be a part
  *              of the file name.
  *              note: special characters can't be included in the file name,
  *              such as:'?','*','/','\','<','>',':','"','|'.
  *
  * @paddr_start:the start paddr in memory of the modules debug information
  * @paddr_end:  the end paddr in memory of the modules debug information
  *
  * Return: 0 means success, -1 means fail.
  */
extern int minidump_save_extend_information(const char *name,
		unsigned long paddr_start,
		unsigned long paddr_end);


extern int minidump_change_extend_information(const char *name,
		unsigned long paddr_start,
		unsigned long paddr_end);
#if IS_ENABLED(CONFIG_SPRD_SHUTDOWN_DETECT)
 /*
  * save kernel log and android log to partition
  *
  * @time:	the time when the problem ocurred
  * @reason:	the reason which the shutdown ocurred
  *
  */
extern void shutdown_save_log_to_partition(char *time, char *reason);
#endif //CONFIG_SPRD_SHUTDOWN_DETECT

extern int last_kmsg_init(void);

extern void last_kmsg_exit(void);

extern char *ylog_buffer;
#define DEVICE_NAME_YLOG "ylog_buffer"
#define YLOG_BUF_SIZE (1 * 1024 * 1024)

/**
  * save regs and flush cache at current cpu.
  * @p:		unused;
  * @regs:	addr of struct pt_regs
  *
  */
extern void sysdump_ipi(void *p, struct pt_regs *regs);

extern int get_kernel_log_to_buffer(char *buf, size_t buf_size);

extern unsigned long unisoc_virt_to_phys(const void *kaddr);
#if IS_ENABLED(CONFIG_SPRD_MINI_SYSDUMP)
/**
  * for wdh and so on...
  * @regs:	addr of struct pt_regs
  * @reason:	reason
  *
  */
extern void prepare_dump_info_for_wdh(struct pt_regs *regs, const char *reason);
#else /*!CONFIG_SPRD_MINI_SYSDUMP*/
static inline void prepare_dump_info_for_wdh(struct pt_regs *regs, const char *reason) {}
#endif /*CONFIG_SPRD_MINI_SYSDUMP*/

#else /*!CONFIG_SPRD_SYSDUMP*/
static inline int minidump_save_extend_information(const char *name,
		unsigned long paddr_start,
		unsigned long paddr_end)
{
	return 0;
}
static inline int minidump_change_extend_information(const char *name,
		unsigned long paddr_start,
		unsigned long paddr_end)
{
	return 0;
}
static inline void shutdown_save_log_to_partition(char *time, char *reason) {}
static inline void sysdump_ipi(void *p, struct pt_regs *regs) {}
static inline void prepare_dump_info_for_wdh(struct pt_regs *regs, const char *reason) {}
static inline int get_kernel_log_to_buffer(char *buf, size_t buf_size) { return -1; }
static inline unsigned long unisoc_virt_to_phys(const void *kaddr) { return 0; }

#endif /*CONFIG_SPRD_SYSDUMP*/
