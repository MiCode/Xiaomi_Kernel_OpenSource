#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/semaphore.h>

#include "notify_queue.h"
#include "teei_id.h"

/***********************************************************************

 create_notify_queue:
     Create the two way notify queues between T_OS and NT_OS.

 argument:
     size    the notify queue size.

 return value:
     EINVAL  invalid argument
     ENOMEM  no enough memory
     EAGAIN  The command ID in the response is NOT accordant to the request.

 ***********************************************************************/

static long create_notify_queue(unsigned long msg_buff, unsigned long size)
{
	long retVal = 0;
	unsigned long irq_flag = 0;
	struct message_head msg_head;
	struct create_NQ_struct msg_body;
	struct ack_fast_call_struct msg_ack;


	/* Check the argument */
	if (size > MAX_BUFF_SIZE) {
		pr_err("[%s][%d]: The NQ buffer size is too large, DO NOT Allow to create it.\n", __FILE__, __LINE__);
		retVal = -EINVAL;
		goto return_fn;
	}

	/* Create the double NQ buffer. */
#ifdef UT_DMA_ZONE
	nt_t_buffer = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(size, SZ_4K)));
#else
	nt_t_buffer = (unsigned long) __get_free_pages(GFP_KERNEL , get_order(ROUND_UP(size, SZ_4K)));
#endif
	if (nt_t_buffer == NULL) {
		pr_err("[%s][%d]: kmalloc nt_t_buffer failed.\n", __func__, __LINE__);
		retVal =  -ENOMEM;
		goto return_fn;
	}

#ifdef UT_DMA_ZONE
	t_nt_buffer = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA, get_order(ROUND_UP(size, SZ_4K)));
#else
	t_nt_buffer = (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(size, SZ_4K)));
#endif

	if (t_nt_buffer == NULL) {
		pr_err("[%s][%d]: kmalloc t_nt_buffer failed.\n", __func__, __LINE__);
		retVal =  -ENOMEM;
		goto Destroy_nt_t_buffer;
	}

	memset(&msg_head, 0, sizeof(struct message_head));
	memset(&msg_body, 0, sizeof(struct create_NQ_struct));
	memset(&msg_ack, 0, sizeof(struct ack_fast_call_struct));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = FAST_CALL_TYPE;
	msg_head.child_type = FAST_CREAT_NQ;
	msg_head.param_length = sizeof(struct create_NQ_struct);

	msg_body.n_t_nq_phy_addr = virt_to_phys(nt_t_buffer);
	msg_body.n_t_size = size;
	msg_body.t_n_nq_phy_addr = virt_to_phys(t_nt_buffer);
	msg_body.t_n_size = size;

	//local_irq_save(irq_flag);

	/* Notify the T_OS that there are two QN to be created. */
	memcpy(msg_buff, &msg_head, sizeof(struct message_head));
	memcpy(msg_buff + sizeof(struct message_head), &msg_body, sizeof(struct create_NQ_struct));
	Flush_Dcache_By_Area((unsigned long)msg_buff, (unsigned long)msg_buff + MESSAGE_SIZE);

	down(&(smc_lock));

	/* Call the smc_fast_call */
	/* n_invoke_t_fast_call(0, 0, 0); */
	invoke_fastcall();

	down(&(boot_sema));

	Invalidate_Dcache_By_Area((unsigned long)msg_buff, (unsigned long)msg_buff + MESSAGE_SIZE);
	memcpy(&msg_head, msg_buff, sizeof(struct message_head));
	memcpy(&msg_ack, msg_buff + sizeof(struct message_head), sizeof(struct ack_fast_call_struct));

	//local_irq_restore(irq_flag);

	/* Check the response from T_OS. */

	if ((msg_head.message_type == FAST_CALL_TYPE) && (msg_head.child_type == FAST_ACK_CREAT_NQ)) {
		retVal = msg_ack.retVal;

		if (retVal == 0)
			goto return_fn;
		else
			goto Destroy_t_nt_buffer;
	} else
		retVal = -EAGAIN;

/* Release the resource and return. */
Destroy_t_nt_buffer:
	free_pages(t_nt_buffer, get_order(ROUND_UP(size, SZ_4K)));
Destroy_nt_t_buffer:
	free_pages(nt_t_buffer, get_order(ROUND_UP(size, SZ_4K)));
return_fn:
	return retVal;
}

void NQ_init(unsigned long NQ_buff)
{
	memset((char *)NQ_buff, 0, NQ_BUFF_SIZE);
}

long init_nq_head(unsigned char *buffer_addr)
{
	struct NQ_head *temp_head = NULL;

	temp_head = (struct NQ_head *)buffer_addr;
	memset(temp_head, 0, NQ_BLOCK_SIZE);
	temp_head->start_index = 0;
	temp_head->end_index = 0;
	temp_head->Max_count = BLOCK_MAX_COUNT;
	Flush_Dcache_By_Area((unsigned long)temp_head, (unsigned long)temp_head + NQ_BLOCK_SIZE);
	return 0;
}

static __always_inline unsigned int get_end_index(struct NQ_head *nq_head)
{
	if (nq_head->end_index == BLOCK_MAX_COUNT)
		return 1;
	else
		return nq_head->end_index + 1;

}


int add_nq_entry(unsigned char *command_buff, int command_length, int valid_flag)
{
	struct NQ_head *temp_head = NULL;
	struct NQ_entry *temp_entry = NULL;

	temp_head = (struct NQ_head *)nt_t_buffer;

	if (temp_head->start_index == ((temp_head->end_index + 1) % temp_head->Max_count))
		return -ENOMEM;
	temp_entry = nt_t_buffer + NQ_BLOCK_SIZE + temp_head->end_index * NQ_BLOCK_SIZE;

	temp_entry->valid_flag = valid_flag;
	temp_entry->length = command_length;
	temp_entry->buffer_addr = command_buff;

	temp_head->end_index = (temp_head->end_index + 1) % temp_head->Max_count;

	Flush_Dcache_By_Area((unsigned long)nt_t_buffer, (unsigned long)(nt_t_buffer + NQ_BUFF_SIZE));
	return 0;
}


unsigned char *get_nq_entry(unsigned char *buffer_addr)
{
	struct NQ_head *temp_head = NULL;
	struct NQ_entry *temp_entry = NULL;

	Invalidate_Dcache_By_Area(buffer_addr, (unsigned long)buffer_addr + NQ_BUFF_SIZE);
	temp_head = (struct NQ_head *)buffer_addr;

	if (temp_head->start_index == temp_head->end_index) {
		pr_err("[cache] temp_head->start_index = %d  temp_head->end_index = %d\n ", temp_head->start_index,  temp_head->end_index);
		return NULL;
	}

	temp_entry = buffer_addr + NQ_BLOCK_SIZE + temp_head->start_index * NQ_BLOCK_SIZE;
	temp_head->start_index = (temp_head->start_index + 1) % temp_head->Max_count;

	Flush_Dcache_By_Area((unsigned long)buffer_addr, (unsigned long)temp_head + NQ_BUFF_SIZE);

	return temp_entry;
}

long create_nq_buffer(void)
{
	long retVal = 0;

	retVal = create_notify_queue(message_buff, NQ_SIZE);

	if (retVal < 0) {
		pr_err("[%s][%d]:create_notify_queue failed with errno %ld.\n", __func__, __LINE__, retVal);
		return -EINVAL;
	}

	NQ_init(t_nt_buffer);
	NQ_init(nt_t_buffer);

	init_nq_head(t_nt_buffer);
	init_nq_head(nt_t_buffer);

	return 0;
}
