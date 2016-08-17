/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * Copyright (C) 2011-2013 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/div64.h>
#include <asm/system.h>
#include <linux/version.h>
#include <asm/cputype.h>
#include <linux/interrupt.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/jiffies.h>

#include <trace/events/nvsecurity.h>

#include "tf_defs.h"
#include "tf_comm.h"
#include "tf_protocol.h"
#include "tf_util.h"
#include "tf_conn.h"

/*
 * Structure common to all SMC operations
 */
struct tf_generic_smc {
	u32 reg0;
	u32 reg1;
	u32 reg2;
	u32 reg3;
	u32 reg4;
};

/*----------------------------------------------------------------------------
 * SMC operations
 *----------------------------------------------------------------------------*/

static inline void tf_smc_generic_call(
	struct tf_generic_smc *generic_smc)
{
#ifdef CONFIG_SMP
	long ret;
	cpumask_t saved_cpu_mask;
	cpumask_t local_cpu_mask = CPU_MASK_NONE;

	cpu_set(0, local_cpu_mask);
	cpumask_copy(&saved_cpu_mask, tsk_cpus_allowed(current));
	ret = sched_setaffinity(0, &local_cpu_mask);
	if (ret != 0)
		dprintk(KERN_ERR "sched_setaffinity #1 -> 0x%lX", ret);
#endif

	trace_smc_generic_call(NVSEC_SMC_START);

	__asm__ volatile(
		"mov r0, %2\n"
		"mov r1, %3\n"
		"mov r2, %4\n"
		"mov r3, %5\n"
		"mov r4, %6\n"
		".word    0xe1600070              @ SMC 0\n"
		"mov %0, r0\n"
		"mov %1, r1\n"
		: "=r" (generic_smc->reg0), "=r" (generic_smc->reg1)
		: "r" (generic_smc->reg0), "r" (generic_smc->reg1),
		  "r" (generic_smc->reg2), "r" (generic_smc->reg3),
		  "r" (generic_smc->reg4)
		: "r0", "r1", "r2", "r3", "r4");

	trace_smc_generic_call(NVSEC_SMC_DONE);

#ifdef CONFIG_SMP
		ret = sched_setaffinity(0, &saved_cpu_mask);
		if (ret != 0)
			dprintk(KERN_ERR "sched_setaffinity #2 -> 0x%lX", ret);
#endif
}

/*
 * Calls the get protocol version SMC.
 * Fills the parameter pProtocolVersion with the version number returned by the
 * SMC
 */
static inline void tf_smc_get_protocol_version(u32 *protocol_version)
{
	struct tf_generic_smc generic_smc;

	generic_smc.reg0 = TF_SMC_GET_PROTOCOL_VERSION;
	generic_smc.reg1 = 0;
	generic_smc.reg2 = 0;
	generic_smc.reg3 = 0;
	generic_smc.reg4 = 0;

	tf_smc_generic_call(&generic_smc);
	*protocol_version = generic_smc.reg1;
}


/*
 * Calls the init SMC with the specified parameters.
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
static inline int tf_smc_init(u32 shared_page_descriptor)
{
	struct tf_generic_smc generic_smc;

	generic_smc.reg0 = TF_SMC_INIT;
	/* Descriptor for the layer 1 shared buffer */
	generic_smc.reg1 = shared_page_descriptor;
	generic_smc.reg2 = 0;
	generic_smc.reg3 = 0;
	generic_smc.reg4 = 0;

	tf_smc_generic_call(&generic_smc);
	if (generic_smc.reg0 != S_SUCCESS)
		printk(KERN_ERR "tf_smc_init:"
			" r0=0x%08X upon return (expected 0x%08X)!\n",
			generic_smc.reg0,
			S_SUCCESS);

	return generic_smc.reg0;
}

/*
 * Calls the WAKE_UP SMC.
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
static inline int tf_smc_wake_up(u32 l1_shared_buffer_descriptor,
	u32 shared_mem_start_offset,
	u32 shared_mem_size)
{
	struct tf_generic_smc generic_smc;

	generic_smc.reg0 = TF_SMC_WAKE_UP;
	generic_smc.reg1 = shared_mem_start_offset;
	/* long form command */
	generic_smc.reg2 = shared_mem_size | 0x80000000;
	generic_smc.reg3 = l1_shared_buffer_descriptor;
	generic_smc.reg4 = 0;

	tf_smc_generic_call(&generic_smc);

	if (generic_smc.reg0 != S_SUCCESS)
		printk(KERN_ERR "tf_smc_wake_up:"
			" r0=0x%08X upon return (expected 0x%08X)!\n",
			generic_smc.reg0,
			S_SUCCESS);

	return generic_smc.reg0;
}

/*
 * Calls the N-Yield SMC.
 */
static inline void tf_smc_nyield(void)
{
	struct tf_generic_smc generic_smc;

	generic_smc.reg0 = TF_SMC_N_YIELD;
	generic_smc.reg1 = 0;
	generic_smc.reg2 = 0;
	generic_smc.reg3 = 0;
	generic_smc.reg4 = 0;

	tf_smc_generic_call(&generic_smc);
}

#ifdef CONFIG_SECURE_TRACES
static void tf_print_secure_traces(struct tf_comm *comm)
{
	spin_lock(&(comm->lock));
	if (comm->l1_buffer->traces_status != 0) {
		if (comm->l1_buffer->traces_status > 1)
			pr_info("TF : traces lost...\n");
		pr_info("TF : %s", comm->l1_buffer->traces_buffer);
		comm->l1_buffer->traces_status = 0;
	}
	spin_unlock(&(comm->lock));
}
#endif

/* Yields the Secure World */
int tf_schedule_secure_world(struct tf_comm *comm)
{
	tf_set_current_time(comm);

	/* yield to the Secure World */
	tf_smc_nyield();

#ifdef CONFIG_SECURE_TRACES
	tf_print_secure_traces(comm);
#endif

	return 0;
}

/*
 * Returns the L2 descriptor for the specified user page.
 */

#define L2_INIT_DESCRIPTOR_BASE           (0x00000003)
#define L2_INIT_DESCRIPTOR_V13_12_SHIFT   (4)

static u32 tf_get_l2init_descriptor(u32 vaddr)
{
	struct page *page;
	u32 paddr;
	u32 descriptor;

	descriptor = L2_INIT_DESCRIPTOR_BASE;

	/* get physical address and add to descriptor */
	page = virt_to_page(vaddr);
	paddr = page_to_phys(page);
	descriptor |= (paddr & L2_DESCRIPTOR_ADDR_MASK);

	/* Add virtual address v[13:12] bits to descriptor */
	descriptor |= (DESCRIPTOR_V13_12_GET(vaddr)
		<< L2_INIT_DESCRIPTOR_V13_12_SHIFT);

	descriptor |= tf_get_l2_descriptor_common(vaddr, &init_mm);


	return descriptor;
}


/*----------------------------------------------------------------------------
 * Power management
 *----------------------------------------------------------------------------*/

/*
 * Free the memory used by the W3B buffer for the specified comm.
 * This function does nothing if no W3B buffer is allocated for the device.
 */
static inline void tf_free_w3b(struct tf_comm *comm)
{
	tf_cleanup_shared_memory(
		&(comm->w3b_cpt_alloc_context),
		&(comm->w3b_shmem_desc),
		0);

	tf_release_coarse_page_table_allocator(&(comm->w3b_cpt_alloc_context));

	internal_vfree((void *)comm->w3b);
	comm->w3b = 0;
	comm->w3b_shmem_size = 0;
	clear_bit(TF_COMM_FLAG_W3B_ALLOCATED, &(comm->flags));
}


/*
 * Allocates the W3B buffer for the specified comm.
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
static inline int tf_allocate_w3b(struct tf_comm *comm)
{
	int error;
	u32 flags;
	u32 config_flag_s;
	u32 *w3b_descriptors;
	u32 w3b_descriptor_count;
	u32 w3b_current_size;

	config_flag_s = tf_read_reg32(&comm->l1_buffer->config_flag_s);

retry:
	if ((test_bit(TF_COMM_FLAG_W3B_ALLOCATED, &(comm->flags))) == 0) {
		/*
		 * Initialize the shared memory for the W3B
		 */
		tf_init_coarse_page_table_allocator(
			&comm->w3b_cpt_alloc_context);
	} else {
		/*
		 * The W3B is allocated but do we have to reallocate a bigger
		 * one?
		 */
		/* Check H bit */
		if ((config_flag_s & (1<<4)) != 0) {
			/* The size of the W3B may change after SMC_INIT */
			/* Read the current value */
			w3b_current_size = tf_read_reg32(
				&comm->l1_buffer->w3b_size_current_s);
			if (comm->w3b_shmem_size > w3b_current_size)
				return 0;

			tf_free_w3b(comm);
			goto retry;
		} else {
			return 0;
		}
	}

	/* check H bit */
	if ((config_flag_s & (1<<4)) != 0)
		/* The size of the W3B may change after SMC_INIT */
		/* Read the current value */
		comm->w3b_shmem_size = tf_read_reg32(
			&comm->l1_buffer->w3b_size_current_s);
	else
		comm->w3b_shmem_size = tf_read_reg32(
			&comm->l1_buffer->w3b_size_max_s);

	comm->w3b = (u32) internal_vmalloc(comm->w3b_shmem_size);
	if (comm->w3b == 0) {
		printk(KERN_ERR "tf_allocate_w3b():"
			" Out of memory for W3B buffer (%u bytes)!\n",
			(unsigned int)(comm->w3b_shmem_size));
		error = -ENOMEM;
		goto error;
	}

	/* initialize the w3b_shmem_desc structure */
	comm->w3b_shmem_desc.type = TF_SHMEM_TYPE_PM_HIBERNATE;
	INIT_LIST_HEAD(&(comm->w3b_shmem_desc.list));

	flags = (TF_SHMEM_TYPE_READ | TF_SHMEM_TYPE_WRITE);

	/* directly point to the L1 shared buffer W3B descriptors */
	w3b_descriptors = comm->l1_buffer->w3b_descriptors;

	/*
	 * tf_fill_descriptor_table uses the following parameter as an
	 * IN/OUT
	 */

	error = tf_fill_descriptor_table(
		&(comm->w3b_cpt_alloc_context),
		&(comm->w3b_shmem_desc),
		comm->w3b,
		NULL,
		w3b_descriptors,
		comm->w3b_shmem_size,
		&(comm->w3b_shmem_offset),
		false,
		flags,
		&w3b_descriptor_count);
	if (error != 0) {
		printk(KERN_ERR "tf_allocate_w3b():"
			" tf_fill_descriptor_table failed with "
			"error code 0x%08x!\n",
			error);
		goto error;
	}

	set_bit(TF_COMM_FLAG_W3B_ALLOCATED, &(comm->flags));

	/* successful completion */
	return 0;

error:
	tf_free_w3b(comm);

	return error;
}

/*
 * Perform a Secure World shutdown operation.
 * The routine does not return if the operation succeeds.
 * the routine returns an appropriate error code if
 * the operation fails.
 */
int tf_pm_shutdown(struct tf_comm *comm)
{
#ifdef CONFIG_TFN
	/* this function is useless for the TEGRA product */
	return 0;
#else
	int error;
	union tf_command command;
	union tf_answer answer;

	dprintk(KERN_INFO "tf_pm_shutdown()\n");

	memset(&command, 0, sizeof(command));

	command.header.message_type = TF_MESSAGE_TYPE_MANAGEMENT;
	command.header.message_size =
			(sizeof(struct tf_command_management) -
				sizeof(struct tf_command_header))/sizeof(u32);

	command.management.command = TF_MANAGEMENT_SHUTDOWN;

	error = tf_send_receive(
		comm,
		&command,
		&answer,
		NULL,
		false);

	if (error != 0) {
		dprintk(KERN_ERR "tf_pm_shutdown(): "
			"tf_send_receive failed (error %d)!\n",
			error);
		return error;
	}

#ifdef CONFIG_TF_DRIVER_DEBUG_SUPPORT
	if (answer.header.error_code != 0)
		dprintk(KERN_ERR "tf_driver: shutdown failed.\n");
	else
		dprintk(KERN_INFO "tf_driver: shutdown succeeded.\n");
#endif

	return answer.header.error_code;
#endif
}


/*
 * Perform a Secure World hibernate operation.
 * The routine does not return if the operation succeeds.
 * the routine returns an appropriate error code if
 * the operation fails.
 */
int tf_pm_hibernate(struct tf_comm *comm)
{
#ifdef CONFIG_TFN
	/* this function is useless for the TEGRA product */
	return 0;
#else
	int error;
	union tf_command command;
	union tf_answer answer;
	u32 first_command;
	u32 first_free_command;

	dprintk(KERN_INFO "tf_pm_hibernate()\n");

	error = tf_allocate_w3b(comm);
	if (error != 0) {
		dprintk(KERN_ERR "tf_pm_hibernate(): "
			"tf_allocate_w3b failed (error %d)!\n",
			error);
		return error;
	}

	/*
	 * As the polling thread is already hibernating, we
	 * should send the message and receive the answer ourself
	 */

	/* build the "prepare to hibernate" message */
	command.header.message_type = TF_MESSAGE_TYPE_MANAGEMENT;
	command.management.command = TF_MANAGEMENT_HIBERNATE;
	/* Long Form Command */
	command.management.shared_mem_descriptors[0] = 0;
	command.management.shared_mem_descriptors[1] = 0;
	command.management.w3b_size =
		comm->w3b_shmem_size | 0x80000000;
	command.management.w3b_start_offset =
		comm->w3b_shmem_offset;
	command.header.operation_id = (u32) &answer;

	tf_dump_command(&command);

	/* find a slot to send the message in */

	/* AFY: why not use the function tf_send_receive?? We are
	 * duplicating a lot of subtle code here. And it's not going to be
	 * tested because power management is currently not supported by the
	 * secure world. */
	for (;;) {
		int queue_words_count, command_size;

		spin_lock(&(comm->lock));

		first_command = tf_read_reg32(
			&comm->l1_buffer->first_command);
		first_free_command = tf_read_reg32(
			&comm->l1_buffer->first_free_command);

		queue_words_count = first_free_command - first_command;
		command_size     = command.header.message_size
			+ sizeof(struct tf_command_header);
		if ((queue_words_count + command_size) <
				TF_N_MESSAGE_QUEUE_CAPACITY) {
			/* Command queue is not full */
			memcpy(&comm->l1_buffer->command_queue[
				first_free_command %
					TF_N_MESSAGE_QUEUE_CAPACITY],
				&command,
				command_size * sizeof(u32));

			tf_write_reg32(&comm->l1_buffer->first_free_command,
				first_free_command + command_size);

			spin_unlock(&(comm->lock));
			break;
		}

		spin_unlock(&(comm->lock));
		(void)tf_schedule_secure_world(comm);
	}

	/* now wait for the answer, dispatching other answers */
	while (1) {
		u32 first_answer;
		u32 first_free_answer;

		/* check all the answers */
		first_free_answer = tf_read_reg32(
			&comm->l1_buffer->first_free_answer);
		first_answer = tf_read_reg32(
			&comm->l1_buffer->first_answer);

		if (first_answer != first_free_answer) {
			int bFoundAnswer = 0;

			do {
				/* answer queue not empty */
				union tf_answer tmp_answer;
				struct tf_answer_header header;
				/* size of the command in words of 32bit */
				int command_size;

				/* get the message_size */
				memcpy(&header,
					&comm->l1_buffer->answer_queue[
						first_answer %
						TF_S_ANSWER_QUEUE_CAPACITY],
					sizeof(struct tf_answer_header));
				command_size = header.message_size +
					sizeof(struct tf_answer_header);

				/*
				 * NOTE: message_size is the number of words
				 * following the first word
				 */
				memcpy(&tmp_answer,
					&comm->l1_buffer->answer_queue[
						first_answer %
						TF_S_ANSWER_QUEUE_CAPACITY],
					command_size * sizeof(u32));

				tf_dump_answer(&tmp_answer);

				if (tmp_answer.header.operation_id ==
						(u32) &answer) {
					/*
					 * this is the answer to the "prepare to
					 * hibernate" message
					 */
					memcpy(&answer,
						&tmp_answer,
						command_size * sizeof(u32));

					bFoundAnswer = 1;
					tf_write_reg32(
						&comm->l1_buffer->first_answer,
						first_answer + command_size);
					break;
				} else {
					/*
					 * this is a standard message answer,
					 * dispatch it
					 */
					struct tf_answer_struct
						*answerStructure;

					answerStructure =
						(struct tf_answer_struct *)
						tmp_answer.header.operation_id;

					memcpy(answerStructure->answer,
						&tmp_answer,
						command_size * sizeof(u32));

					answerStructure->answer_copied = true;
				}

				tf_write_reg32(
					&comm->l1_buffer->first_answer,
					first_answer + command_size);
			} while (first_answer != first_free_answer);

			if (bFoundAnswer)
				break;
		}

		/*
		 * since the Secure World is at least running the "prepare to
		 * hibernate" message, its timeout must be immediate So there is
		 * no need to check its timeout and schedule() the current
		 * thread
		 */
		(void)tf_schedule_secure_world(comm);
	} /* while (1) */

	printk(KERN_INFO "tf_driver: hibernate.\n");
	return 0;
#endif
}


/*
 * Perform a Secure World resume operation.
 * The routine returns once the Secure World is active again
 * or if an error occurs during the "resume" process
 */
int tf_pm_resume(struct tf_comm *comm)
{
#ifdef CONFIG_TFN
	/* this function is useless for the TEGRA product */
	return 0;
#else
	int error;
	u32 status;

	dprintk(KERN_INFO "tf_pm_resume()\n");

	error = tf_smc_wake_up(
		tf_get_l2init_descriptor((u32)comm->l1_buffer),
		comm->w3b_shmem_offset,
		comm->w3b_shmem_size);

	if (error != 0) {
		dprintk(KERN_ERR "tf_pm_resume(): "
			"tf_smc_wake_up failed (error %d)!\n",
			error);
		return error;
	}

	status = ((tf_read_reg32(&(comm->l1_buffer->status_s))
		& TF_STATUS_POWER_STATE_MASK)
		>> TF_STATUS_POWER_STATE_SHIFT);

	while ((status != TF_POWER_MODE_ACTIVE)
			&& (status != TF_POWER_MODE_PANIC)) {
		tf_smc_nyield();

		status = ((tf_read_reg32(&(comm->l1_buffer->status_s))
			& TF_STATUS_POWER_STATE_MASK)
			>> TF_STATUS_POWER_STATE_SHIFT);

		/*
		 * As this may last quite a while, call the kernel scheduler to
		 * hand over CPU for other operations
		 */
		schedule();
	}

	switch (status) {
	case TF_POWER_MODE_ACTIVE:
		break;

	case TF_POWER_MODE_PANIC:
		dprintk(KERN_ERR "tf_pm_resume(): "
			"Secure World POWER_MODE_PANIC!\n");
		return -EINVAL;

	default:
		dprintk(KERN_ERR "tf_pm_resume(): "
			"unexpected Secure World POWER_MODE (%d)!\n", status);
		return -EINVAL;
	}

	dprintk(KERN_INFO "tf_pm_resume() succeeded\n");
	return 0;
#endif
}

/*----------------------------------------------------------------------------
 * Communication initialization and termination
 *----------------------------------------------------------------------------*/

/*
 * Handles the software interrupts issued by the Secure World.
 */
static irqreturn_t tf_soft_int_handler(int irq, void *dev_id)
{
	struct tf_comm *comm = (struct tf_comm *) dev_id;

	if (comm->l1_buffer == NULL)
		return IRQ_NONE;

	if ((tf_read_reg32(&comm->l1_buffer->status_s) &
			TF_STATUS_P_MASK) == 0)
		/* interrupt not issued by the Trusted Foundations Software */
		return IRQ_NONE;

	/*
	 * This "reply" from N-world to S-world is not required
	 * in the new design of S-interrupt processing.
	 * Moreover, the call
	 * tf_smc_reset_irq() -> tf_smc_generic_call() ->
	 * sched_setaffinity(0, &local_cpu_mask)
	 * can break the atomic behavior of Linux scheduler.
	 */

	/* signal N_SM_EVENT */
	wake_up(&comm->wait_queue);

	return IRQ_HANDLED;
}

/*
 * Initializes the communication with the Secure World.
 * The L1 shared buffer is allocated and the Secure World
 * is yielded for the first time.
 * returns successfuly once the communication with
 * the Secure World is up and running
 *
 * Returns 0 upon success or appropriate error code
 * upon failure
 */
int tf_init(struct tf_comm *comm)
{
	int error;
	struct page *buffer_page;
	u32 protocol_version;

	dprintk(KERN_INFO "tf_init()\n");

	spin_lock_init(&(comm->lock));
	comm->flags = 0;
	comm->l1_buffer = NULL;
	init_waitqueue_head(&(comm->wait_queue));

	/*
	 * Check the Secure World protocol version is the expected one.
	 */
	tf_smc_get_protocol_version(&protocol_version);

	if ((GET_PROTOCOL_MAJOR_VERSION(protocol_version))
			!= TF_S_PROTOCOL_MAJOR_VERSION) {
		printk(KERN_ERR "tf_init():"
			" Unsupported Secure World Major Version "
			"(0x%02X, expected 0x%02X)!\n",
			GET_PROTOCOL_MAJOR_VERSION(protocol_version),
			TF_S_PROTOCOL_MAJOR_VERSION);
		error = -EIO;
		goto error;
	}

	/*
	 * Register the software interrupt handler if required to.
	 */
	if (comm->soft_int_irq != -1) {
		dprintk(KERN_INFO "tf_init(): "
			"Registering software interrupt handler (IRQ %d)\n",
			comm->soft_int_irq);

		error = request_irq(comm->soft_int_irq,
			tf_soft_int_handler,
			IRQF_SHARED,
			TF_DEVICE_BASE_NAME,
			comm);
		if (error != 0) {
			dprintk(KERN_ERR "tf_init(): "
				"request_irq failed for irq %d (error %d)\n",
			comm->soft_int_irq, error);
			goto error;
		}
		set_bit(TF_COMM_FLAG_IRQ_REQUESTED, &(comm->flags));
	}

	/*
	 * Allocate and initialize the L1 shared buffer.
	 */
	comm->l1_buffer = (void *) internal_get_zeroed_page(GFP_KERNEL);
	if (comm->l1_buffer == NULL) {
		printk(KERN_ERR "tf_init():"
			" get_zeroed_page failed for L1 shared buffer!\n");
		error = -ENOMEM;
		goto error;
	}

	/*
	 * Ensure the page storing the L1 shared buffer is mapped.
	 */
	buffer_page = virt_to_page(comm->l1_buffer);
	trylock_page(buffer_page);

	dprintk(KERN_INFO "tf_init(): "
		"L1 shared buffer allocated at virtual:%p, "
		"physical:%p (page:%p)\n",
		comm->l1_buffer,
		(void *)virt_to_phys(comm->l1_buffer),
		buffer_page);

	set_bit(TF_COMM_FLAG_L1_SHARED_ALLOCATED, &(comm->flags));

	/*
	 * Init SMC
	 */
	error = tf_smc_init(
		tf_get_l2init_descriptor((u32)comm->l1_buffer));
	if (error != S_SUCCESS) {
		dprintk(KERN_ERR "tf_init(): "
			"tf_smc_init failed (error 0x%08X)!\n",
			error);
		goto error;
	}

	/*
	 * check whether the interrupts are actually enabled
	 * If not, remove irq handler
	 */
	if ((tf_read_reg32(&comm->l1_buffer->config_flag_s) &
			TF_CONFIG_FLAG_S) == 0) {
		if (test_and_clear_bit(TF_COMM_FLAG_IRQ_REQUESTED,
				&(comm->flags)) != 0) {
			dprintk(KERN_INFO "tf_init(): "
				"Interrupts not used, unregistering "
				"softint (IRQ %d)\n",
				comm->soft_int_irq);

			free_irq(comm->soft_int_irq, comm);
		}
	} else {
		if (test_bit(TF_COMM_FLAG_IRQ_REQUESTED,
				&(comm->flags)) == 0) {
			/*
			 * Interrupts are enabled in the Secure World, but not
			 * handled by driver
			 */
			dprintk(KERN_ERR "tf_init(): "
				"soft_interrupt argument not provided\n");
			error = -EINVAL;
			goto error;
		}
	}

	/*
	 * Successful completion.
	 */

	/* yield for the first time */
	(void)tf_schedule_secure_world(comm);

	dprintk(KERN_INFO "tf_init(): Success\n");
	return S_SUCCESS;

error:
	/*
	 * Error handling.
	 */
	dprintk(KERN_INFO "tf_init(): Failure (error %d)\n",
		error);
	tf_terminate(comm);
	return error;
}


/*
 * Attempt to terminate the communication with the Secure World.
 * The L1 shared buffer is freed.
 * Calling this routine terminates definitaly the communication
 * with the Secure World : there is no way to inform the Secure World of a new
 * L1 shared buffer to be used once it has been initialized.
 */
void tf_terminate(struct tf_comm *comm)
{
	dprintk(KERN_INFO "tf_terminate()\n");

	set_bit(TF_COMM_FLAG_TERMINATING, &(comm->flags));

	if ((test_bit(TF_COMM_FLAG_W3B_ALLOCATED,
			&(comm->flags))) != 0) {
		dprintk(KERN_INFO "tf_terminate(): "
			"Freeing the W3B buffer...\n");
		tf_free_w3b(comm);
	}

	if ((test_bit(TF_COMM_FLAG_L1_SHARED_ALLOCATED,
			&(comm->flags))) != 0) {
		__clear_page_locked(virt_to_page(comm->l1_buffer));
		internal_free_page((unsigned long) comm->l1_buffer);
	}

	if ((test_bit(TF_COMM_FLAG_IRQ_REQUESTED,
			&(comm->flags))) != 0) {
		dprintk(KERN_INFO "tf_terminate(): "
			"Unregistering softint (IRQ %d)\n",
			comm->soft_int_irq);
		free_irq(comm->soft_int_irq, comm);
	}
}
