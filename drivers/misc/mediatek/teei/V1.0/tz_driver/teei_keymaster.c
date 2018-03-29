#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

#include "teei_keymaster.h"
#include "teei_id.h"
#include "sched_status.h"
#include "nt_smc_call.h"

#define FDRV_CALL       0x02
#define KEYMASTER_SYS_NO       101

struct fdrv_call_struct {
	int fdrv_call_type;
	int fdrv_call_buff_size;
	int retVal;
};

extern int add_work_entry(int work_type, unsigned long buff);

unsigned long create_keymaster_fdrv(int buff_size)
{
	long retVal = 0;
	unsigned long irq_flag = 0;
	unsigned long temp_addr = 0;
	struct message_head msg_head;
	struct create_fdrv_struct msg_body;
	struct ack_fast_call_struct msg_ack;

	if (message_buff == NULL) {
		pr_err("[%s][%d]: There is NO command buffer!.\n", __func__, __LINE__);
		return NULL;
	}


	if (buff_size > VDRV_MAX_SIZE) {
		pr_err("[%s][%d]: keymaster Drv buffer is too large, Can NOT create it.\n", __FILE__, __LINE__);
		return NULL;
	}

#ifdef UT_DMA_ZONE
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(buff_size, SZ_4K)));
#else
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(buff_size, SZ_4K)));
#endif

	if (temp_addr == NULL) {
		pr_err("[%s][%d]: kmalloc keymaster drv buffer failed.\n", __FILE__, __LINE__);
		return NULL;
	}

	memset(&msg_head, 0, sizeof(struct message_head));
	memset(&msg_body, 0, sizeof(struct create_fdrv_struct));
	memset(&msg_ack, 0, sizeof(struct ack_fast_call_struct));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = FAST_CALL_TYPE;
	msg_head.child_type = FAST_CREAT_FDRV;
	msg_head.param_length = sizeof(struct create_fdrv_struct);

	msg_body.fdrv_type = KEYMASTER_SYS_NO;
	msg_body.fdrv_phy_addr = virt_to_phys(temp_addr);
	msg_body.fdrv_size = buff_size;

	//local_irq_save(irq_flag);

	/* Notify the T_OS that there is ctl_buffer to be created. */
	memcpy(message_buff, &msg_head, sizeof(struct message_head));
	memcpy(message_buff + sizeof(struct message_head), &msg_body, sizeof(struct create_fdrv_struct));
	Flush_Dcache_By_Area((unsigned long)message_buff, (unsigned long)message_buff + MESSAGE_SIZE);

	/* Call the smc_fast_call */
	down(&(smc_lock));
	invoke_fastcall();
	down(&(boot_sema));
	/* up(&(boot_sema)); */

	Invalidate_Dcache_By_Area((unsigned long)message_buff, (unsigned long)message_buff + MESSAGE_SIZE);
	memcpy(&msg_head, message_buff, sizeof(struct message_head));
	memcpy(&msg_ack, message_buff + sizeof(struct message_head), sizeof(struct ack_fast_call_struct));

	//local_irq_restore(irq_flag);

	/* Check the response from T_OS. */
	if ((msg_head.message_type == FAST_CALL_TYPE) && (msg_head.child_type == FAST_ACK_CREAT_FDRV)) {
		retVal = msg_ack.retVal;

		if (retVal == 0) {
			/* pr_debug("[%s][%d]: %s end.\n", __func__, __LINE__, __func__); */
			return temp_addr;
		}
	} else
		retVal = NULL;

	/* Release the resource and return. */
	free_pages(temp_addr, get_order(ROUND_UP(buff_size, SZ_4K)));

	pr_err("[%s][%d]: %s failed!\n", __func__, __LINE__, __func__);
	return retVal;
}



void set_keymaster_command(unsigned long memory_size)
{

	pr_debug("[%s][%d]", __func__, __LINE__);
	struct fdrv_message_head fdrv_msg_head;

	memset(&fdrv_msg_head, 0, sizeof(struct fdrv_message_head));

	fdrv_msg_head.driver_type = KEYMASTER_SYS_NO;
	fdrv_msg_head.fdrv_param_length = sizeof(unsigned int);

	memcpy(fdrv_message_buff, &fdrv_msg_head, sizeof(struct fdrv_message_head));

	Flush_Dcache_By_Area((unsigned long)fdrv_message_buff, (unsigned long)fdrv_message_buff + MESSAGE_SIZE);

	return;
}

int __send_keymaster_command(unsigned long share_memory_size)
{
	set_keymaster_command(share_memory_size);
	Flush_Dcache_By_Area((unsigned long)keymaster_buff_addr, keymaster_buff_addr + KEYMASTER_BUFF_SIZE);

	fp_call_flag = GLSCH_HIGH;
	n_invoke_t_drv(0, 0, 0);

	return 0;

}


static void secondary_send_keymaster_command(void *info)
{
	struct keymaster_command_struct *cd = (struct keymaster_command_struct *)info;

	/* with a rmb() */
	rmb();

	cd->retVal = __send_keymaster_command(cd->mem_size);

	/* with a wmb() */
	wmb();
}



int send_keymaster_command(unsigned long share_memory_size)
{

	struct fdrv_call_struct fdrv_ent;
	int cpu_id = 0;
	int retVal = 0;

	down(&fdrv_lock);
	mutex_lock(&pm_mutex);

	down(&smc_lock);

	if (teei_config_flag == 1)
		complete(&global_down_lock);

#if 0
	keymaster_command_entry.mem_size = share_memory_size;
#else
	fdrv_ent.fdrv_call_type = KEYMASTER_SYS_NO;
	fdrv_ent.fdrv_call_buff_size = share_memory_size;
#endif
	/* with a wmb() */
	wmb();

#if 0
	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_send_keymaster_command, (void *)(&keymaster_command_entry), 1);
	put_online_cpus();

#else
	Flush_Dcache_By_Area((unsigned long)&fdrv_ent, (unsigned long)&fdrv_ent + sizeof(struct fdrv_call_struct));
	retVal = add_work_entry(FDRV_CALL, (unsigned long)&fdrv_ent);

	if (retVal != 0) {
		mutex_unlock(&pm_mutex);
		up(&fdrv_lock);
		return retVal;
	}

#endif

	down(&fdrv_sema);

	/* with a rmb() */
	rmb();

	Invalidate_Dcache_By_Area((unsigned long)keymaster_buff_addr, keymaster_buff_addr + KEYMASTER_BUFF_SIZE);
	Invalidate_Dcache_By_Area((unsigned long)&fdrv_ent, (unsigned long)&fdrv_ent + sizeof(struct fdrv_call_struct));

	mutex_unlock(&pm_mutex);
	up(&fdrv_lock);

#if 0
	return keymaster_command_entry.retVal;
#else
	return fdrv_ent.retVal;
#endif
}
