#include "utdriver_macro.h"

extern unsigned long boot_soter_flag;
extern unsigned long message_buff;
extern unsigned long bdrv_message_buff;
unsigned long teei_vfs_flag;
extern struct semaphore smc_lock;

extern struct completion VFS_rd_comp;
extern struct completion VFS_wr_comp;

unsigned char *daulOS_VFS_share_mem;

static unsigned char *vfs_flush_address;
static unsigned char *printer_share_mem;
static unsigned int printer_shmem_flags;


extern struct semaphore boot_sema;

extern int get_current_cpuid(void);

/******************************
 * Message header
 ******************************/

struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};

struct create_vdrv_struct {
	unsigned int vdrv_type;
	unsigned int vdrv_phy_addr;
	unsigned int vdrv_size;
};

struct ack_vdrv_struct {
	unsigned int sysno;
};

struct ack_fast_call_struct {
	int retVal;
};

struct service_handler {
	unsigned int sysno; /*! 服务调用号 */
	void *param_buf; /*! 双系统通信缓冲区 */
	unsigned size;
	long (*init)(struct service_handler *handler); /*! 服务初始化处理 */
	void (*deinit)(struct service_handler *handler); /*! 服务停止处理 */
	int (*handle)(struct service_handler *handler); /*! 服务调用 */
};

enum {
	TEEI_SERVICE_SOCKET,
	TEEI_SERVICE_TIME,
	TEEI_SERVICE_VFS,
	TEEI_DRIVERS,
	TEEI_SERVICE_MAX
};

struct TEEI_printer_command {
	int func;
	int cmd_size;

	union func_arg {
		struct func_write {
			int length;
			int timeout;
		} func_write_args;
	} args;

};

union TEEI_printer_response {
	int value;
};

struct reetime_handle_struct {
	struct service_handler *handler;
	int retVal;
};

struct reetime_handle_struct reetime_handle_entry;

struct vfs_handle_struct {
	struct service_handler *handler;
	int retVal;
};

struct vfs_handle_struct vfs_handle_entry;


