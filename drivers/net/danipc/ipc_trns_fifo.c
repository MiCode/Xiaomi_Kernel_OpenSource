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

#define TCSR_IPC_IF_FIFO_RD_ACCESS_2_OFFSET		0x18
#define TCSR_IPC_IF_FIFO_RD_ACCESS_0_OFFSET		0x8

#define TCSR_IPC_FIFO_RD_IN_LOW_ADDR(cpuid)				\
	(ipc_regs[cpuid] + TCSR_IPC_IF_FIFO_RD_ACCESS_2_OFFSET)
#define TCSR_IPC_FIFO_RD_IN_HIGH_ADDR(cpuid)				\
	(ipc_regs[cpuid] + TCSR_IPC_IF_FIFO_RD_ACCESS_0_OFFSET)


#define IPC_FIFO_ACCESS(cpuid, odd, even)		({		\
	const typeof(cpuid) __cpuid = cpuid;				\
	ipc_regs[__cpuid] + ((__cpuid & 1) ? (odd) : (even)); })

#define IPC_FIFO_RD_OUT_HIGH_ADDR(cpuid)				\
	IPC_FIFO_ACCESS(cpuid, DAN_IPC_IF_FIFO_RD_5, DAN_IPC_IF_FIFO_RD_1)

#define IPC_FIFO_RD_OUT_LOW_ADDR(cpuid)					\
	IPC_FIFO_ACCESS(cpuid, DAN_IPC_IF_FIFO_RD_7, DAN_IPC_IF_FIFO_RD_3)

#define IPC_FIFO_WR_IN_HIGH_ADDR(cpuid)					\
	IPC_FIFO_ACCESS(cpuid, DAN_IPC_IF_FIFO_WR_4, DAN_IPC_IF_FIFO_WR_0)

#define IPC_FIFO_WR_OUT_HIGH_ADDR(cpuid)				\
	IPC_FIFO_ACCESS(cpuid, DAN_IPC_IF_FIFO_WR_5, DAN_IPC_IF_FIFO_WR_1)

#define IPC_FIFO_WR_IN_LOW_ADDR(cpuid)					\
	IPC_FIFO_ACCESS(cpuid, DAN_IPC_IF_FIFO_WR_6, DAN_IPC_IF_FIFO_WR_2)

#define IPC_FIFO_WR_OUT_LOW_ADDR(cpuid)					\
	IPC_FIFO_ACCESS(cpuid, DAN_IPC_IF_FIFO_WR_7, DAN_IPC_IF_FIFO_WR_3)


uint32_t		ipc_regs_phys[PLATFORM_MAX_NUM_OF_NODES];
unsigned		ipc_regs_len[PLATFORM_MAX_NUM_OF_NODES];
uint32_t		ipc_shared_mem_sizes[PLATFORM_MAX_NUM_OF_NODES];

/* Remapped addresses from ipc_regs_phys */
uintptr_t		ipc_regs[PLATFORM_MAX_NUM_OF_NODES];

/* -----------------------------------------------------------
 * Global prototypes section
 * -----------------------------------------------------------
 */

/* ipc_trns_fifo_buf_alloc
 *
 * Transport layer buffer allocation API
 * use to allocate buffer when message is to be sent
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
char *ipc_trns_fifo_buf_alloc(uint8_t dest_aid, enum ipc_trns_prio prio)
{
	uint32_t		buff_addr;
	uint32_t		fifo_addr;
	const uint8_t		cpuid = ipc_get_node(dest_aid);

	if (prio == ipc_trns_prio_0)
		fifo_addr = IPC_FIFO_RD_OUT_LOW_ADDR(cpuid);
	else
		fifo_addr = IPC_FIFO_RD_OUT_HIGH_ADDR(cpuid);
	buff_addr = __raw_readl_no_log((void *)fifo_addr);

	return  (char *) ((buff_addr) ?
				ipc_to_virt(cpuid, prio, buff_addr) : 0);
}



/* ipc_trns_fifo_buf_free
 *
 * Transport layer buffer free API
 * use to free buffer when message is receievd
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
void ipc_trns_fifo_buf_free(char *ptr, uint8_t dest_aid,
				enum ipc_trns_prio prio)
{
	uint32_t		fifo_addr;
	const uint8_t		cpuid = ipc_get_node(dest_aid);

	if (likely(ptr)) {
		if (prio == ipc_trns_prio_0)
			fifo_addr = IPC_FIFO_WR_OUT_LOW_ADDR(cpuid);
		else
			fifo_addr = IPC_FIFO_WR_OUT_HIGH_ADDR(cpuid);

		__raw_writel_no_log(virt_to_ipc(cpuid, prio, (void *)ptr),
					(void *)fifo_addr);
	}
}

/* ipc_trns_fifo_buf_send
 *
 * Transport layer message sent API
 * use to send message when message is to be sent
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
int32_t ipc_trns_fifo_buf_send(char *ptr, uint8_t dest_aid,
				enum ipc_trns_prio prio)
{
	uint32_t		fifo_addr;
	const uint8_t		cpuid = ipc_get_node(dest_aid);

	if (prio == ipc_trns_prio_0)
		fifo_addr = IPC_FIFO_WR_IN_LOW_ADDR(cpuid);
	else
		fifo_addr = IPC_FIFO_WR_IN_HIGH_ADDR(cpuid);

	__raw_writel_no_log(virt_to_ipc(cpuid, prio, (void *)ptr),
				(void *)fifo_addr);

	return 0;
}


/* ipc_trns_fifo2eth_buf_alloc:
 *
 * Transport layer buffer allocation API
 * use to allocate buffer when message is to be sent
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
char *ipc_trns_fifo2eth_buf_alloc(
	uint8_t			dest_aid,
	enum ipc_trns_prio	prio
)
{
	uint32_t		buff_addr;
	uint32_t		fifo_addr;

	(void)dest_aid;
	if (prio == ipc_trns_prio_0)
		fifo_addr = IPC_FIFO_RD_OUT_LOW_ADDR(0);
	else
		fifo_addr = IPC_FIFO_RD_OUT_HIGH_ADDR(0);
	buff_addr = __raw_readl_no_log((void *)fifo_addr);
	return  (char *)buff_addr;
}

/* ipc_trns_fifo2eth_buf_free:
 *
 * Transport layer buffer free API
 * use to free buffer when message is receievd
 * from an agent on the phy to another agent on the
 * phy (i.e. using fifo based transport)
 *
 */
void ipc_trns_fifo2eth_buf_free(char *ptr, uint8_t dest_aid,
					enum ipc_trns_prio prio)
{
	uint32_t fifo_addr;

	(void)dest_aid;
	if (likely(ptr)) {
		if (prio == ipc_trns_prio_0)
			fifo_addr = IPC_FIFO_WR_OUT_LOW_ADDR(0);
		else
			fifo_addr = IPC_FIFO_WR_OUT_HIGH_ADDR(0);

		__raw_writel_no_log((uint32_t)ptr, (void *)fifo_addr);
	}
}

/* ipc_trns_fifo2eth_buf_send
 *
 * Transport layer message sent API
 * use to send message when message is to be sent
 * from an agent on the phy to an agent on the mac
 * (i.e. using fifo based transport to a predefined proxy)
 *
 */
int32_t ipc_trns_fifo2eth_buf_send(char *ptr, uint8_t dest_aid,
						   enum ipc_trns_prio prio)
{
	uint32_t fifo_addr;

	(void)dest_aid;
	if (prio == ipc_trns_prio_0)
		fifo_addr = IPC_FIFO_WR_IN_LOW_ADDR(0);
	else
		fifo_addr = IPC_FIFO_WR_IN_HIGH_ADDR(0);

	__raw_writel_no_log((uint32_t)ptr, (void *)fifo_addr);

	return 0;
}

/* -----------------------------------------------------------
 * Function:	ipc_trns_fifo_buf_init
 * Description:	Initialize IPC buffer for current node
 * Input:		cpuid:	node ID ()
 * Output:		None
 * -----------------------------------------------------------
 */
void ipc_trns_fifo_buf_init(uint8_t cpuid)
{
	uint32_t		fifo_addr;
	unsigned		ix;
	uint32_t		buf_addr = virt_to_ipc(cpuid, ipc_trns_prio_1,
								ipc_buffers);

	fifo_addr = IPC_FIFO_WR_OUT_HIGH_ADDR(cpuid);

	for (ix = 0; ix < IPC_FIFO_BUF_NUM_HIGH;
			ix++, buf_addr += IPC_BUF_SIZE_MAX)
		__raw_writel_no_log(buf_addr, (void *)fifo_addr);

	fifo_addr = IPC_FIFO_WR_OUT_LOW_ADDR(cpuid);

	for (ix = 0; ix < IPC_FIFO_BUF_NUM_LOW;
			ix++, buf_addr += IPC_BUF_SIZE_MAX)
		__raw_writel_no_log(buf_addr, (void *)fifo_addr);
}


/* -----------------------------------------------------------
 * Function:	ipc_trns_fifo_buf_read
 * Description:	Get message from node associated FIFO
 * Input:		agentId: NOT USED, current node ID already detected
 * Output:		None
 * -----------------------------------------------------------
 */
char *ipc_trns_fifo_buf_read(enum ipc_trns_prio prio)
{
	uint32_t		fifo_addr;
	uint32_t		buff_addr = 0;
	const uint8_t		cpuid = ipc_own_node;

	if (prio == ipc_trns_prio_0)
		fifo_addr = TCSR_IPC_FIFO_RD_IN_LOW_ADDR(cpuid);
	else
		fifo_addr = TCSR_IPC_FIFO_RD_IN_HIGH_ADDR(cpuid);
	buff_addr = __raw_readl_no_log((void *)fifo_addr);

	return  (char *)((buff_addr) ? ipc_to_virt(cpuid, prio, buff_addr) : 0);
}
