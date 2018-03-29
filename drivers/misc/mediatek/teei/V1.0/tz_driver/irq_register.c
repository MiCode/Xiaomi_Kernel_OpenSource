#include <linux/irq.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/freezer.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/cpu.h>
#include "teei_id.h"
#include "sched_status.h"
#include "irq_register.h"
#include "nt_smc_call.h"
#include "teei_id.h"
#include "teei_common.h"

static struct teei_smc_cmd *get_response_smc_cmd(void)
{
	struct NQ_entry *nq_ent = NULL;

	nq_ent = get_nq_entry(t_nt_buffer);

	if (nq_ent == NULL)
		return NULL;

	return (struct teei_smc_cmd *)phys_to_virt((unsigned long)(nq_ent->buffer_addr));
}


static irqreturn_t nt_sched_irq_handler(void)
{
	if (boot_soter_flag == START_STATUS) {
		forward_call_flag = GLSCH_FOR_SOTER;
		up(&smc_lock);
		return IRQ_HANDLED;
	} else {
		if (teei_config_flag == 1)
			complete(&global_down_lock);

		up(&smc_lock);
		return IRQ_HANDLED;
	}

}


int register_sched_irq_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 5);

	retVal = request_irq(irq_num, nt_sched_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 284 IRQ */
	retVal = request_irq(SCHED_IRQ, nt_sched_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", SCHED_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", SCHED_IRQ);

#endif
	return 0;

}


static irqreturn_t nt_soter_irq_handler(void)
{
	irq_call_flag = GLSCH_HIGH;
	up(&smc_lock);
#if 1

	if (teei_config_flag == 1)
		complete(&global_down_lock);

#endif
	return IRQ_HANDLED;
}


int register_soter_irq_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 6);

	retVal = request_irq(irq_num, nt_soter_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 285 IRQ */
	retVal = request_irq(SOTER_IRQ, nt_soter_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", SOTER_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", SOTER_IRQ);

#endif

	return 0;
}


static irqreturn_t nt_error_irq_handler(void)
{
	pr_err("secure system ERROR !\n");
	soter_error_flag = 1;
	up(&(boot_sema));
	up(&smc_lock);
	return IRQ_HANDLED;
}


int register_error_irq_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 0);

	retVal = request_irq(irq_num, nt_error_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 276 IRQ */
	retVal = request_irq(SOTER_ERROR_IRQ, nt_error_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", SOTER_ERROR_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", SOTER_ERROR_IRQ);

#endif

	return 0;
}


static irqreturn_t nt_fp_ack_handler(void)
{
	fp_call_flag = GLSCH_NONE;
	up(&fdrv_sema);
	up(&smc_lock);
	return IRQ_HANDLED;
}


int register_fp_ack_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 7);

	retVal = request_irq(irq_num, nt_fp_ack_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 287 IRQ */
	retVal = request_irq(FP_ACK_IRQ, nt_fp_ack_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", FP_ACK_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", FP_ACK_IRQ);

#endif

	return 0;
}

static irqreturn_t nt_keymaster_ack_handler(void)
{
	keymaster_call_flag = GLSCH_NONE;
	up(&boot_sema);
	up(&smc_lock);
	return IRQ_HANDLED;
}


int register_keymaster_ack_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 7);

	retVal = request_irq(irq_num, nt_keymaster_ack_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 287 IRQ */
	retVal = request_irq(KEYMASTER_ACK_IRQ, nt_keymaster_ack_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", KEYMASTER_ACK_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", KEYMASTER_ACK_IRQ);

#endif

	return 0;
}

int get_bdrv_id(void)
{
	int driver_id = 0;

	Invalidate_Dcache_By_Area(bdrv_message_buff, bdrv_message_buff + MESSAGE_LENGTH);
	driver_id = *((int *)bdrv_message_buff);
	return driver_id;
}

void add_bdrv_queue(int bdrv_id)
{
	work_ent.call_no = bdrv_id;
	INIT_WORK(&(work_ent.work), work_func);
	queue_work(secure_wq, &(work_ent.work));

	return;
}

static irqreturn_t nt_bdrv_handler(void)
{
	int bdrv_id = 0;

	bdrv_id = get_bdrv_id();

	teei_vfs_flag = 1;

	add_bdrv_queue(bdrv_id);
	up(&smc_lock);

	return IRQ_HANDLED;
}


int register_bdrv_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 2);

	retVal = request_irq(irq_num, nt_bdrv_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 278 IRQ */
	retVal = request_irq(BDRV_IRQ, nt_bdrv_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", BDRV_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", BDRV_IRQ);

#endif

	return 0;
}



static irqreturn_t nt_boot_irq_handler(void)
{
	if (boot_soter_flag == START_STATUS) {
		pr_debug("boot irq  handler if\n");
		boot_soter_flag = END_STATUS;
		up(&smc_lock);
		up(&(boot_sema));
		return IRQ_HANDLED;
	} else {
		pr_debug("boot irq hanler else\n");

		if (forward_call_flag == GLSCH_NONE)
			forward_call_flag = GLSCH_NEG;
		else
			forward_call_flag = GLSCH_NONE;

		up(&smc_lock);
		up(&(boot_sema));

		return IRQ_HANDLED;
	}
}

int register_tlog_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 1);

	retVal = request_irq(irq_num, tlog_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 277 IRQ */
	retVal = request_irq(TEEI_LOG_IRQ, tlog_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", TEEI_LOG_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", TEEI_LOG_IRQ);

#endif

	return 0;
}

int register_boot_irq_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 4);

	retVal = request_irq(irq_num, nt_boot_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 283 IRQ */
	retVal = request_irq(BOOT_IRQ, nt_boot_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", BOOT_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", BOOT_IRQ);

#endif

	return 0;
}


static void secondary_load_func(void)
{
	Flush_Dcache_By_Area((unsigned long)boot_vfs_addr, (unsigned long)boot_vfs_addr + VFS_SIZE);
	pr_debug("[%s][%d]: %s end.\n", __func__, __LINE__, __func__);
	n_ack_t_load_img(0, 0, 0);

	return ;
}


void load_func(struct work_struct *entry)
{
	int cpu_id = 0;

	vfs_thread_function(boot_vfs_addr, NULL, NULL);

	down(&smc_lock);

	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_load_func, NULL, 1);
	put_online_cpus();

	return;
}

void work_func(struct work_struct *entry)
{

	struct work_entry *md = container_of(entry, struct work_entry, work);
	int sys_call_num = md->call_no;

	if (sys_call_num == reetime.sysno) {
		reetime.handle(&reetime);
		Flush_Dcache_By_Area(reetime.param_buf, reetime.param_buf + reetime.size);
	} else if (sys_call_num == vfs_handler.sysno) {
		vfs_handler.handle(&vfs_handler);
		Flush_Dcache_By_Area(vfs_handler.param_buf, vfs_handler.param_buf + vfs_handler.size);
	}

	return;
}

static irqreturn_t nt_switch_irq_handler(void)
{
	unsigned long irq_flag = 0;
	struct teei_smc_cmd *command = NULL;
	struct semaphore *cmd_sema = NULL;
	struct message_head *msg_head = NULL;
	struct ack_fast_call_struct *msg_ack = NULL;

	if (boot_soter_flag == START_STATUS) {
		/* pr_debug("[%s][%d] ==== boot_soter_flag == START_STATUS ========\n", __func__, __LINE__); */
		INIT_WORK(&(load_ent.work), load_func);
		queue_work(secure_wq, &(load_ent.work));
		up(&smc_lock);

		return IRQ_HANDLED;

	} else {
		msg_head = (struct message_head *)message_buff;

		if (FAST_CALL_TYPE == msg_head->message_type) {
			/* pr_debug("[%s][%d] ==== FAST_CALL_TYPE ACK ========\n", __func__, __LINE__); */
			return IRQ_HANDLED;
		} else if (STANDARD_CALL_TYPE == msg_head->message_type) {
			/* Get the smc_cmd struct */
			if (msg_head->child_type == VDRV_CALL_TYPE) {
				/* pr_debug("[%s][%d] ==== VDRV_CALL_TYPE ========\n", __func__, __LINE__); */
				work_ent.call_no = msg_head->param_length;
				INIT_WORK(&(work_ent.work), work_func);
				queue_work(secure_wq, &(work_ent.work));
				up(&smc_lock);
#if 0
			} else if (msg_head->child_type == FDRV_ACK_TYPE) {
				/* pr_debug("[%s][%d] ==== FDRV_ACK_TYPE ========\n", __func__, __LINE__); */
				/*
				if(forward_call_flag == GLSCH_NONE)
					forward_call_flag = GLSCH_NEG;
				else
					forward_call_flag = GLSCH_NONE;
				*/
				up(&boot_sema);
				up(&smc_lock);
#endif
			} else {
				/* pr_debug("[%s][%d] ==== STANDARD_CALL_TYPE ACK ========\n", __func__, __LINE__); */

				forward_call_flag = GLSCH_NONE;
				command = get_response_smc_cmd();

				if (NULL == command) {
					pr_err("command IS NULL!!!\n");
					return IRQ_NONE;
				}

				/* Get the semaphore */
				cmd_sema = (struct semaphore *)(command->teei_sema);

				/* Up the semaphore */
				up(cmd_sema);
				up(&smc_lock);
			}

			return IRQ_HANDLED;
		} else {
			pr_err("[%s][%d] ==== Unknown IRQ ========\n", __func__, __LINE__);
			return IRQ_NONE;
		}
	}
}

int register_switch_irq_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 3);

	retVal = request_irq(irq_num, nt_switch_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 282 IRQ */
	retVal = request_irq(SWITCH_IRQ, nt_switch_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", SWITCH_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", SWITCH_IRQ);

#endif

	return 0;
}


static irqreturn_t ut_drv_irq_handler(void)
{
	uint64_t irq_id = 0;
	int retVal = 0;

	/* Get the interrupt ID */
	nt_get_non_irq_num(&irq_id);

	switch (irq_id) {
	case SCHED_IRQ:
		retVal = nt_sched_irq_handler();
		break;

	case SWITCH_IRQ:
		retVal = nt_switch_irq_handler();
		break;

	case BDRV_IRQ:
		retVal = nt_bdrv_handler();
		break;

	case TEEI_LOG_IRQ:
		retVal = tlog_handler();
		break;

	case FP_ACK_IRQ:
		retVal = nt_fp_ack_handler();
		break;

	case SOTER_ERROR_IRQ:
		retVal = nt_error_irq_handler();
		break;

	case BOOT_IRQ:
		retVal = nt_boot_irq_handler();
		break;

	default:
		retVal = -EINVAL;
		pr_err("get undefine IRQ from secure OS!\n");
	}

	return retVal;
}


int register_ut_irq_handler(void)
{
	int retVal = 0;

#ifdef CONFIG_OF
	int irq_num = 0;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "microtrust,utos");
	irq_num = irq_of_parse_and_map(node, 5);

	retVal = request_irq(irq_num, ut_drv_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("[CONFIG_OF] [%s] ERROR for request_irq %d error code : %d.\n", __func__, irq_num, retVal);
	else
		pr_debug("[CONFIG_OF] [%s] request irq [ %d ] OK.\n", __func__, irq_num);

#else
	/* register 284 IRQ */
	retVal = request_irq(UT_DRV_IRQ, ut_drv_irq_handler, 0, "tz_drivers_service", NULL);

	if (retVal)
		pr_err("ERROR for request_irq %d error code : %d.\n", UT_DRV_IRQ, retVal);
	else
		pr_debug("request irq [ %d ] OK.\n", UT_DRV_IRQ);

#endif

}
