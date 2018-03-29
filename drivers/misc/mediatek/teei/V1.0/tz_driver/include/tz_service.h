#ifndef TZ_SERVICE_H
#define TZ_SERVICE_H

enum {
	TEEI_SERVICE_SOCKET,
	TEEI_SERVICE_TIME,
	TEEI_SERVICE_VFS,
	TEEI_DRIVERS,
	TEEI_SERVICE_MAX
};

struct service_handler {
	unsigned int sysno; /*! 服务调用号 */
	void *param_buf; /*! 双系统通信缓冲区 */
	unsigned size;
	void (*init)(struct service_handler *handler); /*! 服务初始化处理 */
	void (*deinit)(struct service_handler *handler); /*! 服务停止处理 */
	int (*handle)(struct service_handler *handler); /*! 服务调用 */
};

/** @brief
 *
 */

extern char *tsc_drivers_buf;
extern struct semaphore printer_rd_sem;
extern struct semaphore printer_wr_sem;
extern struct semaphore smc_lock;
extern struct semaphore cpu_down_lock;
extern struct semaphore boot_sema;
extern unsigned long boot_vfs_addr;
extern unsigned long boot_soter_flag;
extern int forward_call_flag;
extern unsigned int soter_error_flag;

#ifdef VFS_RDWR_SEM
extern struct semaphore VFS_rd_sem;
extern struct semaphore VFS_wr_sem;
#else
extern struct completion VFS_rd_comp;
extern struct completion VFS_wr_comp;
#endif

int wait_for_service_done(void);
int register_switch_irq_handler(void);
int teei_service_init(void);
int add_nq_entry(unsigned char *command_buff, int command_length, int valid_flag);
void set_sch_nq_cmd(void);
void set_sch_load_img_cmd(void);
long create_cmd_buff(void);
void set_fp_command(unsigned long memory_size);
/* int send_fp_command(unsigned long share_memory_size); */
void set_keymaster_command(unsigned long memory_size);
int send_keymaster_command(unsigned long share_memory_size);
void set_gatekeeper_command(unsigned long memory_size);
int send_gatekeeper_command(unsigned long share_memory_size);
extern unsigned char *get_nq_entry(unsigned char *buffer_addr);
#endif
