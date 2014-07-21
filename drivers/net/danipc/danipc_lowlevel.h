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


#ifndef __DANIPC_LOWLEVEL_H__
#define __DANIPC_LOWLEVEL_H__

#include <linux/irqflags.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#define IPC_DUMMY_ADDR			0
#define PLATFORM_MAX_NUM_OF_NODES	16
#define IPC_BUF_SIZE			((2 * IPC_BUF_COUNT_MAX) *	\
						IPC_BUF_SIZE_MAX)

extern uint8_t __iomem			*ipc_buffers;
extern uint32_t				ipc_regs_phys[];
extern unsigned				ipc_regs_len[];
extern uintptr_t			ipc_regs[];
extern uint32_t				ipc_shared_mem_sizes[];
extern struct agent_entry __iomem	*agent_table;

#define LOCAL_IPC_ID		8 /*CHIP_IPC_KRAIT_ADDR*/

struct ipc_to_virt_map {
	/* Physical address of the FIFO data buffer *without* bit 31 set. */
	uint32_t		paddr;

	/* Virtual address of the FIFO data buffer. */
	void __iomem		*vaddr;

	/* How many skbs destined for this core are on delayed_skb list */
	atomic_t		pending_skbs;
};

extern uint32_t virt_to_ipc(const int cpuid, const unsigned prio, void *v_addr);
extern void *ipc_to_virt(const int cpuid, const unsigned prio,
				const uint32_t raw_ipc_addr);

#define __IPC_AGENT_ID(cpuid, lid)					\
			(((cpuid&(PLATFORM_MAX_NUM_OF_NODES-1)) << 4) +	\
				(0x0f & (lid)))

extern unsigned	ipc_init(void);
extern void	ipc_trns_fifo_buf_init(uint8_t cpu_id);
extern void	ipc_route_table_init(struct ipc_trns_func const *ptr);
extern char	*ipc_trns_fifo_buf_alloc(uint8_t dest_aid,
						   enum ipc_trns_prio pri);
extern void	ipc_trns_fifo_buf_free(char *ptr, uint8_t dest_aid,
						enum ipc_trns_prio pri);
extern int32_t	ipc_trns_fifo_buf_send(char *ptr, uint8_t destId,
						enum ipc_trns_prio pri);
extern char	*ipc_trns_fifo2eth_buf_alloc(uint8_t dest_aid,
						enum ipc_trns_prio pri);
extern void	ipc_trns_fifo2eth_buf_free(char *ptr, uint8_t dest_aid,
						enum ipc_trns_prio pri);
extern int32_t	ipc_trns_fifo2eth_buf_send(char *ptr, uint8_t destId,
						enum ipc_trns_prio pri);
extern char	*ipc_trns_fifo_buf_read(enum ipc_trns_prio pri);
extern void	ipc_agent_table_clean(void);
extern uint8_t	ipc_get_own_node(void);
extern struct ipc_trns_func const *get_trns_funcs(uint8_t cpuid);


extern void	handle_incoming_packet(char *const packet,
					uint8_t cpu_id,
					enum ipc_trns_prio pri);

extern struct ipc_to_virt_map	ipc_to_virt_map[PLATFORM_MAX_NUM_OF_NODES][2];

#endif /* __DANIPC_LOWLEVEL_H__ */
