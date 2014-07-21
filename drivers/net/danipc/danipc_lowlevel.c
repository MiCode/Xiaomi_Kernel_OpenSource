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


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/irq.h>

#include "danipc_k.h"


#include "ipc_api.h"

#include "danipc_lowlevel.h"

#define AF_THRESHOLD		0x7d

#define FIFO_THR_AF_CFG_F0	0x34
#define CDU_INT0_MASK_F0	0x44
#define CDU_INT0_ENABLE_F0	0x4c
#define CDU_INT0_CLEAR_F0	0x64

#define IPC_INTR_FIFO_AF	5
#define IPC_INTR(intr)		(1 << (intr))


/* IPC and Linux coexistence.
 * IPC uses/needs physical addresses with bit 31 set while Linux obviously
 * uses virtual addresses. So when writing an address to IPC / reading from IPC
 * make sure it is converted from virtual to IPC address
 * (physical address with bit 31 set) and vice versa.
 * For every cpuid (except my own) FIFO buffers of both priorities remapped.
 * It is guaranteed (?) that FIFO buffers are in contiguous memory of 16kB long.
 * So remapping of 2*16kB is a safe way to access all possible FIFO buffers.
 * For my own CPU just take physical address.
 * Vladik, 21.08.2011
 */

#define FIFO_MAP_SIZE		SZ_256K
#define FIFO_MAP_MASK		(FIFO_MAP_SIZE - 1)

uint8_t	__iomem			*ipc_buffers;

struct ipc_to_virt_map		ipc_to_virt_map[PLATFORM_MAX_NUM_OF_NODES][2];

static void __iomem		*krait_ipc_mux;

static void init_own_ipc_to_virt_map(struct danipc_priv *priv)
{
	struct ipc_to_virt_map *high_map =
		&ipc_to_virt_map[LOCAL_IPC_ID][ipc_trns_prio_1];
	struct ipc_to_virt_map *low_map =
		&ipc_to_virt_map[LOCAL_IPC_ID][ipc_trns_prio_0];

	/* This prevents remapping by remap_fifo_mem() */
	high_map->vaddr		= &ipc_buffers[0];
	high_map->paddr		= priv->res_start[IPC_BUFS_RES];

	low_map->vaddr		= &ipc_buffers[IPC_BUF_COUNT_MAX *
							IPC_BUF_SIZE_MAX];
	low_map->paddr		= priv->res_start[IPC_BUFS_RES] +
							(IPC_BUF_SIZE / 2);
}


static void unmap_ipc_to_virt_map(void)
{
	int		cpuid;

	for (cpuid = 0; cpuid < PLATFORM_MAX_NUM_OF_NODES; cpuid++) {
		if (cpuid == LOCAL_IPC_ID)
			continue;
		if (ipc_to_virt_map[cpuid][ipc_trns_prio_1].vaddr)
			iounmap(ipc_to_virt_map[cpuid][ipc_trns_prio_1].vaddr);
		if (ipc_to_virt_map[cpuid][ipc_trns_prio_0].vaddr)
			iounmap(ipc_to_virt_map[cpuid][ipc_trns_prio_0].vaddr);
	}
}

static void remap_fifo_mem(const int cpuid, const unsigned prio,
				const uint32_t paddr)
{
	struct ipc_to_virt_map *const map = ipc_to_virt_map[cpuid];
	unsigned other_prio = (prio == ipc_trns_prio_0) ?
				ipc_trns_prio_1 : ipc_trns_prio_0;
	uint32_t start_addr;
	uint32_t map_size;
	uint32_t map_mask;

	/* Use shared memory size if defined for given CPU.
	 * Since shared memory is used for both FIFO priorities, remap
	 * only once for this CPU.
	 */
	if (ipc_shared_mem_sizes[cpuid]) {
		map_size = ipc_shared_mem_sizes[cpuid];
		map_mask = map_size - 1;
		start_addr = ((paddr + map_mask) & ~map_mask) - map_size;
		map[prio].paddr = map[other_prio].paddr = start_addr;
		map[prio].vaddr = map[other_prio].vaddr =
			ioremap_nocache(start_addr, 2 * map_size);
	} else {
		map_size = FIFO_MAP_SIZE;
		map_mask = FIFO_MAP_MASK;
		start_addr = ((paddr + map_mask) & ~map_mask) - 2 * map_size;
		map[prio].paddr = start_addr;
		map[prio].vaddr = ioremap_nocache(start_addr, 2 * map_size);
	}

	if (!map[prio].vaddr) {
		pr_err(
			"%s:%d cpuid = %d priority = %u cannot remap FIFO memory at addr. 0x%x\n",
			__func__, __LINE__, cpuid, prio, start_addr);
		BUG();
	}
}

uint32_t virt_to_ipc(const int cpuid, const unsigned prio, void *vaddr)
{
	if (likely(prio <= ipc_trns_prio_1)) {
		struct ipc_to_virt_map	*map = &ipc_to_virt_map[cpuid][prio];
		int		offset;

		if (unlikely(!map->paddr)) {
			pr_err("%s:%d: cpuid = %d priority = %u unmapped\n",
				__func__, __LINE__, cpuid, prio);
			BUG();
		}
		offset = (unsigned)vaddr - (unsigned)map->vaddr;
		return map->paddr + offset;
	} else {
		pr_err("%s:%d: cpuid = %d illegal priority = %u\n",
			__func__, __LINE__, cpuid, prio);
		BUG();
	}

	return 0;
}

void *ipc_to_virt(const int cpuid, const unsigned prio, const uint32_t ipc_addr)
{
	if (likely(prio <= ipc_trns_prio_1 ||
			cpuid < PLATFORM_MAX_NUM_OF_NODES)) {
		struct ipc_to_virt_map	*map = &ipc_to_virt_map[cpuid][prio];
		const uint32_t	paddr = ipc_addr;
		int		offset;

		if (unlikely(!map->paddr))
			remap_fifo_mem(cpuid, prio, paddr);
		offset = paddr - map->paddr;
		return (uint8_t *)map->vaddr + offset;
	} else {
		pr_err("%s:%d: cpuid = %d illegal priority = %u\n",
			__func__, __LINE__, cpuid, prio);
		BUG();
	}
	return NULL;
}


void high_prio_rx(unsigned long data)
{
	struct net_device	*dev = (struct net_device *)data;
	const unsigned	base_addr = ipc_regs[LOCAL_IPC_ID];

	/* Clear interrupt source. */
	__raw_writel_no_log(IPC_INTR(IPC_INTR_FIFO_AF),
				(void *)(base_addr + CDU_INT0_CLEAR_F0));

	/* Process all messages. */
	ipc_recv(IPC_FIFO_BUF_NUM_HIGH, ipc_trns_prio_1);

	/* Unmask IPC AF interrupt again. */
	__raw_writel_no_log(~IPC_INTR(IPC_INTR_FIFO_AF),
					(void *)(base_addr + CDU_INT0_MASK_F0));

	enable_irq(dev->irq);
}

irqreturn_t danipc_interrupt(int irq, void *data)
{
	struct net_device	*dev = (struct net_device *)data;
	struct danipc_priv	*priv = netdev_priv(dev);
	const unsigned	base_addr = ipc_regs[LOCAL_IPC_ID];

	/* Mask all IPC interrupts. */
	__raw_writel_no_log(~0, (void *)(base_addr + CDU_INT0_MASK_F0));

	disable_irq_nosync(irq);

	tasklet_schedule(&priv->rx_task);

	return IRQ_HANDLED;
}

void danipc_init_irq(struct net_device *dev, struct danipc_priv *priv)
{
	const unsigned	base_addr = ipc_regs[LOCAL_IPC_ID];

	__raw_writel_no_log(AF_THRESHOLD,
				(void *)(base_addr + FIFO_THR_AF_CFG_F0));
	__raw_writel_no_log(IPC_INTR(IPC_INTR_FIFO_AF),
				(void *)(base_addr + CDU_INT0_CLEAR_F0));
	__raw_writel_no_log(IPC_INTR(IPC_INTR_FIFO_AF),
				(void *)(base_addr + CDU_INT0_ENABLE_F0));
	__raw_writel_no_log(~IPC_INTR(IPC_INTR_FIFO_AF),
				(void *)(base_addr + CDU_INT0_MASK_F0));

	/* Enable passing all IPC interrupts. */
	__raw_writel_no_log(~0, krait_ipc_mux);
}


static void remap_agent_table(struct danipc_priv *priv)
{
	agent_table = ioremap_nocache(priv->res_start[AGENT_TABLE_RES],
					priv->res_len[AGENT_TABLE_RES]);
	if (!agent_table) {
		pr_err("%s: cannot remap IPC global agent table\n", __func__);
		BUG();
	}
}

static void unmap_agent_table(void)
{
	if (agent_table)
		iounmap(agent_table);
}

static void prepare_node(const int cpuid)
{
	struct ipc_to_virt_map	*map;

	ipc_regs[cpuid] = (uintptr_t)ioremap_nocache(ipc_regs_phys[cpuid],
							ipc_regs_len[cpuid]);
	map = &ipc_to_virt_map[cpuid][ipc_trns_prio_0];
	atomic_set(&map->pending_skbs, 0);

	map = &ipc_to_virt_map[cpuid][ipc_trns_prio_1];
	atomic_set(&map->pending_skbs, 0);
}

static void prepare_nodes(void)
{
	int		n;

	for (n = 0; n < PLATFORM_MAX_NUM_OF_NODES; n++)
		if (ipc_regs_phys[n])
			prepare_node(n);
}

static void unmap_nodes_memory(void)
{
	int		n;

	for (n = 0; n < PLATFORM_MAX_NUM_OF_NODES; n++)
		if (ipc_regs[n])
			iounmap((void __iomem *)ipc_regs[n]);
}

static void *alloc_ipc_buffers(struct danipc_priv *priv)
{
	ipc_buffers = ioremap_nocache(priv->res_start[IPC_BUFS_RES],
					priv->res_len[IPC_BUFS_RES]);
	if (ipc_buffers)
		memset(ipc_buffers, 0, priv->res_len[IPC_BUFS_RES]);
	else {
		pr_err("%s: cannot allocate IPC buffers!\n", __func__);
		BUG();
	}
	return ipc_buffers;
}

static void free_ipc_buffers(void)
{
	if (ipc_buffers)
		iounmap(ipc_buffers);
}

static void remap_krait_ipc_mux(struct danipc_priv *priv)
{
	krait_ipc_mux = ioremap_nocache(priv->res_start[KRAIT_IPC_MUX_RES],
					priv->res_len[KRAIT_IPC_MUX_RES]);
	if (!krait_ipc_mux) {
		pr_err("%s: cannot remap Krait IPC mux\n", __func__);
		BUG();
	}
}


static void unmap_krait_ipc_mux(void)
{
	if (krait_ipc_mux)
		iounmap(krait_ipc_mux);
}


int danipc_ll_init(struct danipc_priv *priv)
{
	int		rc = -1;

	if (alloc_ipc_buffers(priv) != NULL) {
		prepare_nodes();
		remap_agent_table(priv);
		init_own_ipc_to_virt_map(priv);
		remap_krait_ipc_mux(priv);
		rc = ipc_init();
	}

	return rc;
}


void danipc_ll_cleanup(void)
{
	unmap_krait_ipc_mux();
	unmap_ipc_to_virt_map();
	unmap_agent_table();
	unmap_nodes_memory();
	free_ipc_buffers();
}


void danipc_poll(struct net_device *dev)
{
	(void)dev;
	ipc_recv(IPC_FIFO_BUF_NUM_LOW, ipc_trns_prio_0);
}
