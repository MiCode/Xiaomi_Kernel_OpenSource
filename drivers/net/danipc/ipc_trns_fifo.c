/*
	All files except if stated otherwise in the begining of the file
	are under the ISC license:
	----------------------------------------------------------------------

	Copyright (c) 2010-2012 Design Art Networks Ltd.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


/* -----------------------------------------------------------
 * Include section
 * -----------------------------------------------------------
 */

#include <linux/io.h>

#include "ipc_reg.h"
#include "ipc_api.h"

#include "danipc_lowlevel.h"


/* -----------------------------------------------------------
 * MACRO (define) section
 * -----------------------------------------------------------
 */

#define TCSR_ipc_if_FIFO_RD_ACCESS_2_OFFSET		(0x18)
#define TCSR_ipc_if_FIFO_RD_ACCESS_0_OFFSET		(0x8)

#define TCSR_IPC_FIFO_RD_IN_LOW_ADDR(_cpuid)				\
	(uint32_t)(IPC_array_hw_access[(_cpuid)] +			\
				TCSR_ipc_if_FIFO_RD_ACCESS_2_OFFSET)
#define TCSR_IPC_FIFO_RD_IN_HIGH_ADDR(_cpuid)				\
	(uint32_t)(IPC_array_hw_access[(_cpuid)] +			\
				TCSR_ipc_if_FIFO_RD_ACCESS_0_OFFSET)

#define IPC_FIFO_RD_OUT_HIGH_ADDR(_cpuid)	(((_cpuid)&1) ?		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_RD_ACCESS_5_OFFSET) :		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_RD_ACCESS_1_OFFSET))

#define IPC_FIFO_RD_OUT_LOW_ADDR(_cpuid)	(((_cpuid)&1) ?		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_RD_ACCESS_7_OFFSET) :		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_RD_ACCESS_3_OFFSET))

#define IPC_FIFO_WR_IN_HIGH_ADDR(_cpuid)	(((_cpuid)&1) ?		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_WR_ACCESS_4_OFFSET) :		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_WR_ACCESS_0_OFFSET))

#define IPC_FIFO_WR_OUT_HIGH_ADDR(_cpuid)	(((_cpuid)&1) ?		\
	((uint32_t)(IPC_array_hw_access[(_cpuid)]) +			\
			dan_ipc_if_FIFO_WR_ACCESS_5_OFFSET) :		\
	((uint32_t)(IPC_array_hw_access[(_cpuid)]) +			\
			dan_ipc_if_FIFO_WR_ACCESS_1_OFFSET))

#define IPC_FIFO_WR_IN_LOW_ADDR(_cpuid)	(((_cpuid)&1) ?			\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_WR_ACCESS_6_OFFSET) :		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_WR_ACCESS_2_OFFSET))

#define IPC_FIFO_WR_OUT_LOW_ADDR(_cpuid)	(((_cpuid)&1) ?		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_WR_ACCESS_7_OFFSET) :		\
	((uint32_t)IPC_array_hw_access[(_cpuid)] +			\
			dan_ipc_if_FIFO_WR_ACCESS_3_OFFSET))


uint32_t IPC_array_hw_access_phys[PLATFORM_MAX_NUM_OF_NODES];
unsigned IPC_hw_access_phys_len[PLATFORM_MAX_NUM_OF_NODES];

/* Remapped addresses from IPC_array_hw_access_phys */
void __iomem *IPC_array_hw_access[PLATFORM_MAX_NUM_OF_NODES];

/* -----------------------------------------------------------
 * Global prototypes section
 * -----------------------------------------------------------
 */

/* IPC_trns_fifo_buffer_alloc
 *
 * Transport layer buffer allocation API
 * use to allocate buffer when message is to be sent
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
char *IPC_trns_fifo_buffer_alloc(
	uint8_t			dest_agent_id,
	enum IPC_trns_priority	pri
)
{
	uint32_t	buff_addr;
	uint32_t	fifo_addr;
	uint8_t	cpu_id = IPC_GetNode(dest_agent_id);

	if (pri == IPC_trns_prio_0)
		fifo_addr = IPC_FIFO_RD_OUT_LOW_ADDR(cpu_id);
	else
		fifo_addr = IPC_FIFO_RD_OUT_HIGH_ADDR(cpu_id);
	buff_addr = __raw_readl_no_log((void *)fifo_addr);

	return  (char *) ((buff_addr) ?
				ipc_to_virt(cpu_id, pri, buff_addr) : 0);
}



/* ipc_fifo_buffer_free:
 *
 * Transport layer buffer free API
 * use to free buffer when message is receievd
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
void IPC_trns_fifo_buffer_free(char *ptr, uint8_t dest_agent_id,
				enum IPC_trns_priority pri)
{
	uint32_t	fifo_addr;
	uint8_t	cpu_id = IPC_GetNode(dest_agent_id);

	if (likely(ptr)) {
		if (pri == IPC_trns_prio_0)
			fifo_addr = IPC_FIFO_WR_OUT_LOW_ADDR(cpu_id);
		else
			fifo_addr = IPC_FIFO_WR_OUT_HIGH_ADDR(cpu_id);

		__raw_writel_no_log(virt_to_ipc(cpu_id, pri, (void *)ptr),
					(void *)fifo_addr);
	}
}

/* IPC_trns_fifo_msg_send:
 *
 * Transport layer message sent API
 * use to send message when message is to be sent
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
int32_t IPC_trns_fifo_buf_send(char *ptr, uint8_t destId,
				enum IPC_trns_priority pri)
{
	uint8_t	cpu_id = IPC_GetNode(destId);
	uint32_t	fifo_addr;

	if (pri == IPC_trns_prio_0)
		fifo_addr = IPC_FIFO_WR_IN_LOW_ADDR(cpu_id);
	else
		fifo_addr = IPC_FIFO_WR_IN_HIGH_ADDR(cpu_id);

	__raw_writel_no_log(virt_to_ipc(cpu_id, pri, (void *)ptr),
				(void *)fifo_addr);

	return 0;
}


/* IPC_trns_fifo2eth_buffer_alloc:
 *
 * Transport layer buffer allocation API
 * use to allocate buffer when message is to be sent
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
char *IPC_trns_fifo2eth_buffer_alloc(
	uint8_t			dest_agent_id,
	enum IPC_trns_priority	pri
)
{
	uint32_t	buff_addr;
	uint32_t	fifo_addr;
	uint8_t		cpu_id = 0;

	if (pri == IPC_trns_prio_0)
		fifo_addr = IPC_FIFO_RD_OUT_LOW_ADDR(cpu_id);
	else
		fifo_addr = IPC_FIFO_RD_OUT_HIGH_ADDR(cpu_id);
	buff_addr = __raw_readl_no_log((void *)fifo_addr);
	return  (char *)buff_addr;
}

/* IPC_trns_fifo2eth_buffer_free:
 *
 * Transport layer buffer free API
 * use to free buffer when message is receievd
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
void IPC_trns_fifo2eth_buffer_free(char *ptr,
					uint8_t dest_agent_id,
					enum IPC_trns_priority pri)
{
	uint8_t  cpu_id = 0;
	uint32_t fifo_addr;

	if (likely(ptr)) {
		if (pri == IPC_trns_prio_0)
			fifo_addr = IPC_FIFO_WR_OUT_LOW_ADDR(cpu_id);
		else
			fifo_addr = IPC_FIFO_WR_OUT_HIGH_ADDR(cpu_id);

		__raw_writel_no_log((uint32_t)ptr, (void *)fifo_addr);
	}
}

/* IPC_trns_fifo_msg_send:
 *
 * Transport layer message sent API
 * use to send message when message is to be sent
 * from an agent on the phy to an agent on the mac
 * (i.e. using fifo based transport to a predefined proxy)
 *
 */
int32_t IPC_trns_fifo2eth_buffer_send(char *ptr, uint8_t destId,
						   enum IPC_trns_priority pri)
{
	uint8_t		cpu_id = 0;
	uint32_t	fifo_addr;

	if (pri == IPC_trns_prio_0)
		fifo_addr = IPC_FIFO_WR_IN_LOW_ADDR(cpu_id);
	else
		fifo_addr = IPC_FIFO_WR_IN_HIGH_ADDR(cpu_id);

	__raw_writel_no_log((uint32_t)ptr, (void *)fifo_addr);

	return 0;
}

/* -----------------------------------------------------------
 * Function:	IPC_trns_fifo_buff_init
 * Description:	Initialize IPC buffer for current node
 * Input:		cpu_id:	node ID ()
 * Output:		None
 * -----------------------------------------------------------
 */
void IPC_trns_fifo_buff_init(uint8_t cpu_id)
{
	uint8_t	ix;
	uint32_t	fifo_addr;

	uint32_t buf_addr = virt_to_ipc(cpu_id, IPC_trns_prio_1, ipc_buffers);

	fifo_addr = IPC_FIFO_WR_OUT_HIGH_ADDR(cpu_id);

	for (ix = 0; ix < IPC_FIFO_BUF_NUM_HIGH;
			ix++, buf_addr += IPC_BUF_SIZE_MAX)
		__raw_writel_no_log(buf_addr, (void *)fifo_addr);

	fifo_addr = IPC_FIFO_WR_OUT_LOW_ADDR(cpu_id);

	for (ix = 0; ix < IPC_FIFO_BUF_NUM_LOW;
			ix++, buf_addr += IPC_BUF_SIZE_MAX)
		__raw_writel_no_log(buf_addr, (void *)fifo_addr);
}


/* -----------------------------------------------------------
 * Function:	IPC_trns_fifo_buf_read
 * Description:	Get message from node associated FIFO
 * Input:		agentId: NOT USED, current node ID already detected
 * Output:		None
 * -----------------------------------------------------------
 */
char *IPC_trns_fifo_buf_read(enum IPC_trns_priority pri)
{
	uint32_t	fifo_addr;
	uint32_t	buff_addr = 0;
	uint8_t		cpu_id = IPC_OwnNode;

	if (pri == IPC_trns_prio_0)
		fifo_addr = TCSR_IPC_FIFO_RD_IN_LOW_ADDR(cpu_id);
	else
		fifo_addr = TCSR_IPC_FIFO_RD_IN_HIGH_ADDR(cpu_id);
	buff_addr = __raw_readl_no_log((void *)fifo_addr);

	return  (char *)((buff_addr) ? ipc_to_virt(cpu_id, pri, buff_addr) : 0);
}
