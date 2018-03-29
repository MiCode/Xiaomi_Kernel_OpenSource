#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include "nt_smc_call.h"
#include "backward_driver.h"
#include "teei_id.h"

#define BDRV_CALL       0x03

#define VFS_SYS_NO      0x08
#define REETIME_SYS_NO  0x07

struct bdrv_call_struct {
	int bdrv_call_type;
	struct service_handler *handler;
	int retVal;
};

extern int add_work_entry(int work_type, unsigned long buff);
static long register_shared_param_buf(struct service_handler *handler);

void set_ack_vdrv_cmd(unsigned int sys_num)
{
	if (boot_soter_flag == START_STATUS) {

		struct message_head msg_head;
		struct ack_vdrv_struct ack_body;

		memset(&msg_head, 0, sizeof(struct message_head));

		msg_head.invalid_flag = VALID_TYPE;
		msg_head.message_type = STANDARD_CALL_TYPE;
		msg_head.child_type = N_ACK_T_INVOKE_DRV;
		msg_head.param_length = sizeof(struct ack_vdrv_struct);

		ack_body.sysno = sys_num;

		memcpy(message_buff, &msg_head, sizeof(struct message_head));
		memcpy(message_buff + sizeof(struct message_head), &ack_body, sizeof(struct ack_vdrv_struct));

		Flush_Dcache_By_Area((unsigned long)message_buff, (unsigned long)message_buff + MESSAGE_SIZE);
	} else {
		*((int *)bdrv_message_buff) = sys_num;
		Flush_Dcache_By_Area((unsigned long)bdrv_message_buff, (unsigned long)bdrv_message_buff + MESSAGE_SIZE);
	}

	return;
}



static void secondary_invoke_fastcall(void *info)
{
	n_invoke_t_fast_call(0, 0, 0);
}


void invoke_fastcall(void)
{
	int cpu_id = 0;

	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_invoke_fastcall, NULL, 1);
	put_online_cpus();
}

static long register_shared_param_buf(struct service_handler *handler)
{

	long retVal = 0;
	unsigned long irq_flag = 0;
	struct message_head msg_head;
	struct create_vdrv_struct msg_body;
	struct ack_fast_call_struct msg_ack;

	if (message_buff == NULL) {
		pr_err("[%s][%d]: There is NO command buffer!.\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (handler->size > VDRV_MAX_SIZE) {
		pr_err("[%s][%d]: The vDrv buffer is too large, DO NOT Allow to create it.\n", __FILE__, __LINE__);
		return -EINVAL;
	}

#ifdef UT_DMA_ZONE
	handler->param_buf = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(handler->size, SZ_4K)));
#else
	handler->param_buf = (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(handler->size, SZ_4K)));
#endif
	if (handler->param_buf == NULL) {
		pr_err("[%s][%d]: kmalloc vdrv_buffer failed.\n", __FILE__, __LINE__);
		return -ENOMEM;
	}

	memset(&msg_head, 0, sizeof(struct message_head));
	memset(&msg_body, 0, sizeof(struct create_vdrv_struct));
	memset(&msg_ack, 0, sizeof(struct ack_fast_call_struct));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = FAST_CALL_TYPE;
	msg_head.child_type = FAST_CREAT_VDRV;
	msg_head.param_length = sizeof(struct create_vdrv_struct);

	msg_body.vdrv_type = handler->sysno;
	msg_body.vdrv_phy_addr = virt_to_phys(handler->param_buf);
	msg_body.vdrv_size = handler->size;

	//local_irq_save(irq_flag);

	/* Notify the T_OS that there is ctl_buffer to be created. */
	memcpy(message_buff, &msg_head, sizeof(struct message_head));
	memcpy(message_buff + sizeof(struct message_head), &msg_body, sizeof(struct create_vdrv_struct));
	Flush_Dcache_By_Area((unsigned long)message_buff, (unsigned long)message_buff + MESSAGE_SIZE);

	down(&(smc_lock));

	invoke_fastcall();

	down(&(boot_sema));

	Invalidate_Dcache_By_Area((unsigned long)message_buff, (unsigned long)message_buff + MESSAGE_SIZE);
	memcpy(&msg_head, message_buff, sizeof(struct message_head));
	memcpy(&msg_ack, message_buff + sizeof(struct message_head), sizeof(struct ack_fast_call_struct));

	//local_irq_restore(irq_flag);

	/* Check the response from T_OS. */
	if ((msg_head.message_type == FAST_CALL_TYPE) && (msg_head.child_type == FAST_ACK_CREAT_VDRV)) {
		retVal = msg_ack.retVal;

		if (retVal == 0) {
			/* pr_debug("[%s][%d]: %s end.\n", __FILE__, __LINE__, __func__); */
			return retVal;
		}
	} else {
		retVal = -EAGAIN;
	}

	/* Release the resource and return. */
	free_pages(handler->param_buf, get_order(ROUND_UP(handler->size, SZ_4K)));
	handler->param_buf = NULL;

	return retVal;
}

/******************************TIME**************************************/
#include <linux/time.h>

struct service_handler reetime;
static long reetime_init(struct service_handler *handler)
{
	return register_shared_param_buf(handler);
}

static void reetime_deinit(struct service_handler *handler)
{
	return;
}

int __reetime_handle(struct service_handler *handler)
{
	struct timeval tv;
	void *ptr = NULL;
	int tv_sec;
	int tv_usec;
	do_gettimeofday(&tv);
	ptr = handler->param_buf;
	tv_sec = tv.tv_sec;
	*((int *)ptr) = tv_sec;
	tv_usec = tv.tv_usec;
	*((int *)ptr + 1) = tv_usec;

	Flush_Dcache_By_Area((unsigned long)handler->param_buf, (unsigned long)handler->param_buf + handler->size);

	set_ack_vdrv_cmd(handler->sysno);
	teei_vfs_flag = 0;

	n_ack_t_invoke_drv(0, 0, 0);

	return 0;
}

static void secondary_reetime_handle(void *info)
{
	struct reetime_handle_struct *cd = (struct reetime_handle_struct *)info;

	/* with a rmb() */
	rmb();

	cd->retVal = __reetime_handle(cd->handler);

	/* with a wmb() */
	wmb();
}

static int reetime_handle(struct service_handler *handler)
{
	int cpu_id = 0;
	int retVal = 0;
	struct bdrv_call_struct *reetime_bdrv_ent = NULL;

	down(&smc_lock);

#if 0
	reetime_handle_entry.handler = handler;
#else
	reetime_bdrv_ent = (struct bdrv_call_struct *)kmalloc(sizeof(struct bdrv_call_struct), GFP_KERNEL);
	reetime_bdrv_ent->handler = handler;
	reetime_bdrv_ent->bdrv_call_type = REETIME_SYS_NO;
#endif
	/* with a wmb() */
	wmb();

#if 0
	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_reetime_handle, (void *)(&reetime_handle_entry), 1);
	put_online_cpus();
#else
	retVal = add_work_entry(BDRV_CALL, (unsigned long)reetime_bdrv_ent);
	if (retVal != 0) {
		up(&smc_lock);
		return retVal;
	}
#endif
	/* with a rmb() */
	rmb();
#if 0
	return reetime_handle_entry.retVal;
#else
	return 0;
#endif
}

/********************************************************************
 *                      VFS functions                               *
 ********************************************************************/
struct service_handler vfs_handler;
static unsigned long para_vaddr;
static unsigned long buff_vaddr;


int vfs_thread_function(unsigned long virt_addr, unsigned long para_vaddr, unsigned long buff_vaddr)
{
	Invalidate_Dcache_By_Area((unsigned long)virt_addr, virt_addr + VFS_SIZE);
	daulOS_VFS_share_mem = virt_addr;
#ifdef VFS_RDWR_SEM
	up(&VFS_rd_sem);
	down_interruptible(&VFS_wr_sem);
#else
	complete(&VFS_rd_comp);
	wait_for_completion_interruptible(&VFS_wr_comp);
#endif

}

static long vfs_init(struct service_handler *handler) /*! init service */
{
	long retVal = 0;

	retVal = register_shared_param_buf(handler);
	vfs_flush_address = handler->param_buf;

	return retVal;
}

static void vfs_deinit(struct service_handler *handler) /*! stop service  */
{
	return;
}

int __vfs_handle(struct service_handler *handler) /*! invoke handler */
{
	Flush_Dcache_By_Area((unsigned long)handler->param_buf, (unsigned long)handler->param_buf + handler->size);

	set_ack_vdrv_cmd(handler->sysno);
	teei_vfs_flag = 0;

	n_ack_t_invoke_drv(0, 0, 0);

	return 0;
}

static void secondary_vfs_handle(void *info)
{
	struct vfs_handle_struct *cd = (struct vfs_handle_struct *)info;
	/* with a rmb() */
	rmb();

	cd->retVal = __vfs_handle(cd->handler);

	/* with a wmb() */
	wmb();
}

static int vfs_handle(struct service_handler *handler)
{
	int cpu_id = 0;

	int retVal = 0;

	struct bdrv_call_struct *vfs_bdrv_ent = NULL;

	vfs_thread_function(handler->param_buf, para_vaddr, buff_vaddr);
	down(&smc_lock);
#if 0
	vfs_handle_entry.handler = handler;
#else
	vfs_bdrv_ent = (struct bdrv_call_struct *)kmalloc(sizeof(struct bdrv_call_struct), GFP_KERNEL);
	vfs_bdrv_ent->handler = handler;
	vfs_bdrv_ent->bdrv_call_type = VFS_SYS_NO;
#endif
	/* with a wmb() */
	wmb();
#if 0
	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_vfs_handle, (void *)(&vfs_handle_entry), 1);
	put_online_cpus();
#else
	Flush_Dcache_By_Area((unsigned long)vfs_bdrv_ent, (unsigned long)vfs_bdrv_ent + sizeof(struct bdrv_call_struct));
	retVal = add_work_entry(BDRV_CALL, (unsigned long)vfs_bdrv_ent);
	if (retVal != 0) {
		up(&smc_lock);
		return retVal;
	}

#endif

	/* with a rmb() */
	rmb();

#if 0
	return vfs_handle_entry.retVal;
#else
	return 0;
#endif
}

long init_all_service_handlers(void)
{
	long retVal = 0;

	reetime.init = reetime_init;
	reetime.deinit = reetime_deinit;
	reetime.handle = reetime_handle;
	reetime.size = 0x1000;
	reetime.sysno  = 7;

	vfs_handler.init = vfs_init;
	vfs_handler.deinit = vfs_deinit;
	vfs_handler.handle = vfs_handle;
	vfs_handler.size = 0x80000;
	vfs_handler.sysno = 8;

	pr_debug("[%s][%d] begin to init reetime service!\n", __func__, __LINE__);
	retVal = reetime.init(&reetime);
	if (retVal < 0) {
		pr_err("[%s][%d] init reetime service failed!\n", __func__, __LINE__);
		return retVal;
	}
	pr_debug("[%s][%d] init reetime service successfully!\n", __func__, __LINE__);

	pr_debug("[%s][%d] begin to init vfs service!\n", __func__, __LINE__);
	retVal = vfs_handler.init(&vfs_handler);
	if (retVal < 0) {
		pr_err("[%s][%d] init vfs service failed!\n", __func__, __LINE__);
		return retVal;
	}
	pr_debug("[%s][%d] init vfs service successfully!\n", __func__, __LINE__);

	return 0;
}
