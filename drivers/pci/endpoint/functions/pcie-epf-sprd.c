// SPDX-License-Identifier: GPL-2.0
/*
 * UNISOC PCIe endpoint function driver.
 *
 * Copyright (C) 2019 UNISOC Communications Inc.
 * Author: Ziyi Zhang <ziyi.zhang@unisoc.com>,
 * Wenping Zhou <<wenping.zhou@unisoc.com>>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pci-epc.h>
#include <linux/pci-epf.h>
#include <linux/pci_ids.h>
#include <linux/pci_regs.h>
#include <linux/pcie-epf-sprd.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/soc/sprd/hwfeature.h>
#include <linux/workqueue.h>
#include <uapi/linux/sched/types.h>

#include "../../controller/dwc/pcie-designware.h"

#define SPRD_EPF_NAME "pci_epf_sprd"

/* doorbell reg define */
#define DOOR_BELL_BASE		0x10000
#define DOOR_BELL_ENABLE	0x10
#define DOOR_BELL_STATUS	0x14

/* used 0x18 and 0x1c to save the smem base and size. */
#define DOOR_BELL_SMEMBASE	0x18
#define DOOR_BELL_SMEMSIZE	0x1c

#define IPA_HW_IRQ_CNT		4
#define IPA_HW_IRQ_BASE		16

#define REQUEST_BASE_IRQ	(IPA_HW_IRQ_BASE + IPA_HW_IRQ_CNT)
#define REQUEST_BASE_IRQ_DEFECT	16

#define PCIE_WRITE_DATA	0x5a5a5a5a

struct sprd_ep_res {
	struct list_head	list;
	phys_addr_t		rc_addr;
	phys_addr_t		cpu_phy_addr;
	void __iomem		*cpu_vir_addr;
	size_t			size;
};

struct sprd_pci_epf {
	struct pci_epf		*epf;
	struct list_head	res_list;
	spinlock_t		lock;
	int			irq_number;
	u32			bak_irq_status;

	bool		no_msi;
#ifdef CONFIG_SPRD_IPA_PCIE_WORKROUND
	struct sprd_ep_res	ipa_res;
#endif

	struct work_struct linkup_work;
};

struct sprd_pci_epf_notify {
	void	(*notify)(int event, void *data);
	void	*data;
};

struct sprd_pci_epf_irq_handler {
	irq_handler_t	handler;
	void		*data;
};

#ifdef CONFIG_SPRD_PCIE_DOORBELL_WORKAROUND
static void __iomem	*g_write_addr[SPRD_FUNCTION_MAX];
#endif
static void __iomem	*g_irq_addr[SPRD_FUNCTION_MAX];

static struct sprd_pci_epf *g_epf_sprd[SPRD_FUNCTION_MAX];
static struct sprd_pci_epf_notify g_epf_notify[SPRD_FUNCTION_MAX];
static struct sprd_pci_epf_irq_handler
		g_epf_handler[SPRD_FUNCTION_MAX][PCIE_DBEL_IRQ_MAX];
static int g_irq_number[SPRD_FUNCTION_MAX];
static struct pci_epf_header sprd_header[SPRD_FUNCTION_MAX];

static irqreturn_t sprd_epf_irq_handler(int irq_number, void *private);

static bool sprd_epf_is_defective_chip(void)
{
	/* In pi board, the AB and AA chip use the same flow, so both return false. */
	/*
	 *static bool first_read = true, defective;

	 *if (first_read) {
	 *	first_read = false;
	 *	defective = sprd_kproperty_chipid("UD710-AB") == 0;
	 *}
	 */

	return false;
}

int sprd_pci_epf_register_notify(int function,
				 void (*notify)(int event, void *data),
				 void *data)

{
	struct sprd_pci_epf_notify *epf_notify;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_notify = &g_epf_notify[function];
	epf_notify->notify = notify;
	epf_notify->data = data;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_register_notify);

int sprd_pci_epf_unregister_notify(int function)
{
	struct sprd_pci_epf_notify *epf_notify;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_notify = &g_epf_notify[function];
	epf_notify->notify = NULL;
	epf_notify->data = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_unregister_notify);

int sprd_pci_epf_register_irq_handler(int function,
				      int irq,
				      irq_handler_t handler,
				      void *data)

{
	struct sprd_pci_epf_irq_handler *epf_handler;
	struct sprd_pci_epf *epf_sprd;

	if (function >= SPRD_FUNCTION_MAX || irq >= PCIE_DBEL_IRQ_MAX)
		return -EINVAL;

	epf_handler = &g_epf_handler[function][irq];
	epf_handler->handler = handler;
	epf_handler->data = data;

	/* if have irq before register, handle it */
	epf_sprd = g_epf_sprd[function];
	if (handler && epf_sprd &&
	    (BIT(irq) & epf_sprd->bak_irq_status)) {
		epf_sprd->bak_irq_status &= ~BIT(irq);
		handler(irq, data);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_register_irq_handler);

#ifdef CONFIG_SPRD_PCIE_DOORBELL_WORKAROUND
int sprd_pci_epf_set_write_addr(int function, void __iomem *write_addr)
{
	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	g_write_addr[function] = write_addr;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_set_write_addr);
#endif

int sprd_pci_epf_set_irq_addr(int function, void __iomem *irq_addr)
{
	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	g_irq_addr[function] = irq_addr;

	return 0;
}

int sprd_pci_epf_unregister_irq_handler(int function, int irq)
{
	struct sprd_pci_epf_irq_handler *epf_handler;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_handler = &g_epf_handler[function][irq];
	epf_handler->handler = NULL;
	epf_handler->data = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_unregister_irq_handler);

int sprd_pci_epf_register_irq_handler_ex(int function,
					 int from_irq,
					 int to_irq,
					 irq_handler_t handler,
					 void *data)
{
	int i, ret;

	for (i = from_irq; i < to_irq + 1; i++) {
		ret = sprd_pci_epf_register_irq_handler(function,
							i, handler, data);
		if (ret)
			return ret;
	}

	return 0;
}

int sprd_pci_epf_unregister_irq_handler_ex(int function,
					   int from_irq,
					   int to_irq)
{
	int i, ret;

	for (i = from_irq; i < to_irq + 1; i++) {
		ret = sprd_pci_epf_unregister_irq_handler(function, i);
		if (ret)
			return ret;
	}

	return 0;
}

int sprd_pci_epf_set_irq_number(int function, int irq_number)
{
	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	g_irq_number[function] = irq_number;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_set_irq_number);

int sprd_pci_epf_raise_irq(int function, int irq)
{
	u8 msi_count;
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;
	void __iomem *legacy_addr;
	u32 value;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return -EINVAL;

	dev = &epf_sprd->epf->dev;
	epc = epf_sprd->epf->epc;
	msi_count = pci_epc_get_msi(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no);

	dev_dbg(dev,
		"raise irq func_no=%d, vfunc_no=%d, irq=%d, msi_count=%d\n",
		function, epf_sprd->epf->vfunc_no, irq, msi_count);

	if (sprd_epf_is_defective_chip())
		irq += REQUEST_BASE_IRQ_DEFECT;
	else
		irq += REQUEST_BASE_IRQ;

	if (epf_sprd->no_msi) {
		/* update irq in legacy addr. */
		if (g_irq_addr[function]) {
			legacy_addr = g_irq_addr[0];
			value = readl(legacy_addr);
			value |= BIT(irq);
			writel(value, legacy_addr);
		}
		return pci_epc_raise_irq(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
					 PCI_EPC_IRQ_LEGACY, 0);
	}

	if (irq > msi_count || msi_count <= 0) {
		dev_err(dev,
			"raise irq func_no=%d, irq=%d, msi_count=%d\n",
			function, irq, msi_count);
		return -EINVAL;
	}

	/* *
	 * in dw_pcie_ep_raise_msi_irq, write to the data reg
	 * is (irq -1) , so here, we must pass (irq + 1)
	 */
	return pci_epc_raise_irq(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
				 PCI_EPC_IRQ_MSI, irq + 1);
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_raise_irq);

/*
 * sprd_pci_epf_start
 * will wakeup host to rescan ep
 */
int sprd_pci_epf_start(int function)
{
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return -EINVAL;

	epc = epf_sprd->epf->epc;
	dev = &epf_sprd->epf->dev;

	dev_info(dev, "pci_epc_start.\n");

	return pci_epc_start(epc);
}

void __iomem *sprd_pci_epf_map_memory(int function,
				      phys_addr_t rc_addr,
				      size_t size)
{
	int ret;
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;
	unsigned long flags;
	struct sprd_ep_res *res;
	phys_addr_t cpu_phys_addr;
	void __iomem *cpu_vir_addr;

	if (function >= SPRD_FUNCTION_MAX)
		return NULL;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return NULL;

	dev = &epf_sprd->epf->dev;
	res = devm_kzalloc(dev, sizeof(*res), GFP_KERNEL);
	if (!res)
		return NULL;

	dev_info(dev, "ep: epf map rc_addr=0x%lx, size=0x%lx\n",
		(unsigned long)rc_addr,
		(unsigned long)size);

	epc = epf_sprd->epf->epc;
	size = PAGE_ALIGN(size);

	cpu_vir_addr = pci_epc_mem_alloc_addr(epc, &cpu_phys_addr, size);
	if (!cpu_vir_addr) {
		dev_err(dev,
			"failed to alloc epc memory,  size = 0x%lx!\n",
			size);
		devm_kfree(dev, res);
		return NULL;
	}

	dev_dbg(dev, "map: func_no=%d, vfunc_no=%d, ep_addr=0x%llx, rc_addr=0x%llx\n",
		function, epf_sprd->epf->vfunc_no, cpu_phys_addr, rc_addr);

	ret = pci_epc_map_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
			       cpu_phys_addr, rc_addr, size);
	if (ret) {
		dev_err(dev, "failed to map address!\n");
		pci_epc_mem_free_addr(epc, cpu_phys_addr, cpu_vir_addr, size);
		devm_kfree(dev, res);
		return NULL;
	}

	res->rc_addr = rc_addr;
	res->cpu_phy_addr = cpu_phys_addr;
	res->cpu_vir_addr = cpu_vir_addr;
	res->size = size;

	spin_lock_irqsave(&epf_sprd->lock, flags);
	list_add_tail(&res->list, &epf_sprd->res_list);
	spin_unlock_irqrestore(&epf_sprd->lock, flags);

	return cpu_vir_addr;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_map_memory);

#ifdef CONFIG_SPRD_IPA_PCIE_WORKROUND
void __iomem *sprd_epf_ipa_map(phys_addr_t src_addr,
			       phys_addr_t dst_addr, size_t size)
{
	int ret;
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;
	void __iomem *cpu_vir_addr;

	epf_sprd = g_epf_sprd[SPRD_FUNCTION_0];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return NULL;

	dev = &epf_sprd->epf->dev;

	dev_info(dev, "ep: epf ipa map src_addr=0x%lx, dst_addr=0x%lx size=0x%lx\n",
		(unsigned long)src_addr,
		(unsigned long)dst_addr,
		(unsigned long)size);

	/* only support one ipa res, must unmap before remap */
	if (epf_sprd->ipa_res.cpu_phy_addr) {
		dev_err(dev, "failed to map, must unmap the old ipa map!\n");
		return NULL;
	}

	epc = epf_sprd->epf->epc;
	size = PAGE_ALIGN(size);
	ret = pci_epc_map_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no, src_addr,
			       dst_addr, size);
	if (ret) {
		dev_err(dev, "failed to map ipa address!\n");
		return NULL;
	}

	cpu_vir_addr = ioremap_nocache(src_addr, size);

	epf_sprd->ipa_res.cpu_phy_addr = src_addr;
	epf_sprd->ipa_res.rc_addr = dst_addr;
	epf_sprd->ipa_res.size = size;
	epf_sprd->ipa_res.cpu_vir_addr = cpu_vir_addr;

	return cpu_vir_addr;
}
EXPORT_SYMBOL_GPL(sprd_epf_ipa_map);

void sprd_epf_ipa_unmap(void __iomem *cpu_vir_addr)
{
	struct sprd_pci_epf *epf_sprd;
	struct pci_epc *epc;

	epf_sprd = g_epf_sprd[SPRD_FUNCTION_0];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return;

	dev_info(&epf_sprd->epf->dev, "ipa unmap\n");

	if (!cpu_vir_addr || cpu_vir_addr != epf_sprd->ipa_res.cpu_vir_addr)
		return;

	iounmap(cpu_vir_addr);
	epc = epf_sprd->epf->epc;
	pci_epc_unmap_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
			   epf_sprd->ipa_res.cpu_phy_addr);

	epf_sprd->ipa_res.cpu_vir_addr = NULL;
	epf_sprd->ipa_res.cpu_phy_addr = 0;
	epf_sprd->ipa_res.rc_addr = 0;
	epf_sprd->ipa_res.size = 0;
}
EXPORT_SYMBOL_GPL(sprd_epf_ipa_unmap);
#endif

void sprd_pci_epf_unmap_memory(int function, const void __iomem *cpu_vir_addr)
{
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;
	struct sprd_ep_res *res, *next;
	unsigned long flags;

	if (function >= SPRD_FUNCTION_MAX)
		return;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return;

	dev = &(epf_sprd->epf->dev);
	epc = epf_sprd->epf->epc;

	dev_info(dev, "map: unpap func_no=%d, cpu_vir_addr=0x%p\n",
		function, cpu_vir_addr);

	/* find the res from list */
	spin_lock_irqsave(&epf_sprd->lock, flags);
	list_for_each_entry_safe(res, next, &epf_sprd->res_list, list) {
		if (res->cpu_vir_addr == cpu_vir_addr) {
			pci_epc_unmap_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
					   res->cpu_phy_addr);
			pci_epc_mem_free_addr(epc,
					      res->cpu_phy_addr,
					      res->cpu_vir_addr,
					      res->size);
			list_del(&res->list);
			devm_kfree(dev, res);
			break;
		}
	}
	spin_unlock_irqrestore(&epf_sprd->lock, flags);
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_unmap_memory);

static void sprd_pci_epf_restore(struct sprd_pci_epf *epf_sprd)
{
	struct pci_epc *epc;
	struct device *dev;
	struct sprd_ep_res *res;
	unsigned long flags;

	dev = &epf_sprd->epf->dev;
	epc = epf_sprd->epf->epc;

#ifdef CONFIG_SPRD_IPA_PCIE_WORKROUND
	res = &epf_sprd->ipa_res;
	if (res->rc_addr) {
		dev_info(dev, "map: ep_addr=0x%lx, rc_addr=0x%lx, size=0x%lx\n",
			(unsigned long)res->cpu_phy_addr,
			(unsigned long)res->rc_addr,
			res->size);

		/* after pci reset, the outbound must remap */
		pci_epc_unmap_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
				   res->cpu_phy_addr);
		if (pci_epc_map_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
			res->cpu_phy_addr, res->rc_addr, res->size))
			dev_err(dev, "failed to map rc_addr=0x%lx!\n",
				(unsigned long)res->rc_addr);
	}
#endif

	/* find the res from list, reconfig them */
	spin_lock_irqsave(&epf_sprd->lock, flags);
	list_for_each_entry(res, &epf_sprd->res_list, list) {
		/* after pci reset, the outbound must remap */
		pci_epc_unmap_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
				   res->cpu_phy_addr);
		if (pci_epc_map_addr(epc, epf_sprd->epf->func_no, epf_sprd->epf->vfunc_no,
			res->cpu_phy_addr, res->rc_addr, res->size))
			dev_err(dev, "failed to map rc_addr=0x%lx!\n",
				(unsigned long)res->rc_addr);
	}
	spin_unlock_irqrestore(&epf_sprd->lock, flags);
}

int sprd_epf_get_smem(int function, u32 *base, u32 *size)
{
	struct sprd_pci_epf *epf_sprd;
	struct dw_pcie_ep *ep;
	struct dw_pcie *pci;
	struct device *dev;
	u32 base_reg, size_reg;

	if (function >= SPRD_FUNCTION_MAX || !base || !size)
		return -EINVAL;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return -EINVAL;

	ep = epc_get_drvdata(epf_sprd->epf->epc);
	pci = to_dw_pcie_from_ep(ep);
	dev = &(epf_sprd->epf->dev);

	base_reg = dw_pcie_readl_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_SMEMBASE);
	size_reg = dw_pcie_readl_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_SMEMSIZE);

	if (!base_reg || !size_reg)
		return -EINVAL;

	*base = base_reg;
	*size = size_reg;
	dev_info(dev, "*bas=%#x, *size=%#x\n", *base, *size);

	return 0;
}

static irqreturn_t sprd_epf_irq_handler(int irq_number, void *private)
{
	struct sprd_pci_epf_irq_handler *epf_handler;
	struct sprd_pci_epf *epf_sprd = (struct sprd_pci_epf *)private;
	struct device *dev = &(epf_sprd->epf->dev);
	struct dw_pcie_ep *ep = epc_get_drvdata(epf_sprd->epf->epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	int irq, function;
	u32 enable, status;
#ifdef CONFIG_SPRD_PCIE_DOORBELL_WORKAROUND
	int try_cnt = 10;
	void __iomem *write_addr;
#endif

	function = epf_sprd->epf->func_no;
	if (function >= SPRD_FUNCTION_MAX)
		return IRQ_NONE;

	enable = BIT(PCIE_DBEL_IRQ_MAX) - 1;

	/*
	 * before read doorbell must do a write action
	 * to make PCIE exit L1 state.
	 */
#ifdef CONFIG_SPRD_PCIE_DOORBELL_WORKAROUND
	write_addr = g_write_addr[function];
	if (write_addr)
		writel(PCIE_WRITE_DATA, write_addr);
#endif

	/* read irq status and clear irq */
	status = dw_pcie_readl_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_STATUS);
	dw_pcie_writel_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_STATUS, 0x0);

	dev_dbg(dev, "func_no=%d, enable=%#x, status=%#x\n",
		function, enable, status);

#ifdef CONFIG_SPRD_PCIE_DOORBELL_WORKAROUND
	/* if can't read status, try agian. */
	while (!status && try_cnt--) {
		if (write_addr)
			writel(PCIE_WRITE_DATA, write_addr);
		status = dw_pcie_readl_dbi(pci,
					   DOOR_BELL_BASE + DOOR_BELL_STATUS);
		dw_pcie_writel_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_STATUS, 0x0);
	}
#endif

	status &= enable;

	for (irq = 0; irq < PCIE_DBEL_IRQ_MAX; irq++) {
		if (status & BIT(irq)) {
			epf_handler = &g_epf_handler[function][irq];
			if (epf_handler->handler)
				epf_handler->handler(irq, epf_handler->data);
			else
				epf_sprd->bak_irq_status |= BIT(irq);
		}
	}
	return IRQ_HANDLED;
}

static void sprd_epf_linkup_work_fn(struct work_struct *work)
{
	struct dw_pcie_ep *ep;
	struct dw_pcie *pci;
	struct sprd_pci_epf *epf_sprd = container_of(work, struct sprd_pci_epf,
						     linkup_work);
	struct pci_epf *epf = epf_sprd->epf;
	struct device *dev = &epf->dev;

	/* restore config */
	sprd_pci_epf_restore(epf_sprd);

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		/* enable door bell irq */
		dev_info(dev, "enable doorbell irq.");
		ep = epc_get_drvdata(epf->epc);
		pci = to_dw_pcie_from_ep(ep);
		dw_pcie_writel_dbi(pci,
				   DOOR_BELL_BASE + DOOR_BELL_ENABLE,
				   BIT(PCIE_DBEL_IRQ_MAX) - 1);
	}
}

static int sprd_epf_bind(struct pci_epf *epf)
{
#ifdef CONFIG_SPRD_EPF_ENABLE_EPCONF
	int ret;
#endif
	int msi_count;
	void  (*notify)(int event, void *data);
	struct pci_epc *epc = epf->epc;
	struct device *dev = &epf->dev;
	struct dw_pcie_ep *ep;
	struct dw_pcie *pci;
	struct sprd_pci_epf *epf_sprd = epf_get_drvdata(epf);

	if (WARN_ON_ONCE(!epc))
		return -EINVAL;

	dev_info(dev, "bind: func_no = %d, epf->func_no = =%d\n",
		epf->func_no, epf->msi_interrupts);

	epf->nb.notifier_call = sprd_pci_epf_notifier;
	pci_epc_register_notifier(epc, &epf->nb);

	msi_count = pci_epc_get_msi(epc, epf->func_no, epf->vfunc_no);
	if (msi_count < PCIE_MSI_IRQ_MAX) {
		dev_info(dev,
			"leagacy is true, msi_count=%d\n", msi_count);
		epf_sprd->no_msi = true;
	}

	/*
	 * Normal chip, after orca boot, pcie is already linked.
	 * So after epf bind, must do the linkup work.
	 */
	if (!sprd_epf_is_defective_chip()) {
		/* first, linkup notify */
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_LINK_UP,
			       g_epf_notify[epf->func_no].data);
		schedule_work(&epf_sprd->linkup_work);
	} else {
		/* defective chip must enable door bell irq here. */
		ep = epc_get_drvdata(epf->epc);
		pci = to_dw_pcie_from_ep(ep);
		dw_pcie_writel_dbi(pci,
				   DOOR_BELL_BASE + DOOR_BELL_ENABLE,
				   BIT(PCIE_DBEL_IRQ_MAX) - 1);
	}

	/*
	 * if the feature CONFIG_SPRD_EPF_ENABLE_EPCONF
	 * (default closed) is not opend, we can't set the header
	 * and the msi, just use the default configuration.
	 */
#ifdef CONFIG_SPRD_EPF_ENABLE_EPCONF
	ret = pci_epc_write_header(epc, epf->func_no, epf->vfunc_no, epf->header);
	if (ret) {
		dev_err(dev, "bind: header write failed, ret=%d\n", ret);
		return ret;
	}

	ret = pci_epc_set_msi(epc, epf->func_no, epf->vfunc_no, epf->msi_interrupts);
	if (ret) {
		dev_err(dev, "bind: set msi failed, ret=%d\n", ret);
		return ret;
	}
#endif

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_BIND, g_epf_notify[epf->func_no].data);
	}

	return 0;
}

static void sprd_epf_unbind(struct pci_epf *epf)
{
	void (*notify)(int event, void *data);
	struct device *dev = &epf->dev;
	struct pci_epc *epc = epf->epc;

	dev_info(dev, "unbind: func_no = %d\n", epf->func_no);

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_UNBIND,
			       g_epf_notify[epf->func_no].data);
	}

	pci_epc_stop(epc);
}

static int sprd_pci_epf_notifier(struct notifier_block *nb, unsigned long val,
				 void *data)
{
	struct pci_epf *epf = container_of(nb, struct pci_epf, nb);
	struct sprd_pci_epf *epf_sprd = epf_get_drvdata(epf);
	void  (*notify)(int event, void *data);
	struct device *dev = &epf->dev;

	switch (val) {
	case LINK_UP:
		dev_info(dev, "linkup: func_no = %d\n", epf->func_no);
		/* first, linkup notify */
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_LINK_UP, g_epf_notify[epf->func_no].data);

		schedule_work(&epf_sprd->linkup_work);
		break;

	default:
		dev_err(&epf->dev, "Invalid EPF notifier event\n");
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static const struct pci_epf_device_id sprd_epf_ids[] = {
	{
		.name = SPRD_EPF_NAME,
	},
	{},
};

static int sprd_epf_probe(struct pci_epf *epf)
{
	int ret;
	void (*notify)(int event, void *data);
	struct sprd_pci_epf *epf_sprd;
	struct device *dev = &epf->dev;

	dev_info(dev, "probe.");

	epf_sprd = devm_kzalloc(dev, sizeof(*epf_sprd), GFP_KERNEL);
	if (!epf_sprd)
		return -ENOMEM;

	if (epf->func_no >= SPRD_FUNCTION_MAX) {
		dev_err(dev, "probe: func_no = %d\n", epf->func_no);
		return -EINVAL;
	}

	epf->header = &sprd_header[epf->func_no];
	epf->header->vendorid = PCI_ANY_ID,
	epf->header->deviceid = PCI_ANY_ID,
	epf->header->baseclass_code = PCI_CLASS_OTHERS,
	epf->header->interrupt_pin  = PCI_INTERRUPT_UNKNOWN,

	epf_sprd->epf = epf;
	spin_lock_init(&epf_sprd->lock);
	INIT_LIST_HEAD(&epf_sprd->res_list);
	INIT_WORK(&epf_sprd->linkup_work, sprd_epf_linkup_work_fn);

	g_epf_sprd[epf->func_no] = epf_sprd;
	notify = g_epf_notify[epf->func_no].notify;
	if (notify)
		notify(SPRD_EPF_PROBE, g_epf_notify[epf->func_no].data);

	if (g_irq_number[epf->func_no] > 0) {
		epf_sprd->irq_number = g_irq_number[epf->func_no];
		ret = request_irq(epf_sprd->irq_number,
			   sprd_epf_irq_handler,
			   IRQF_NO_SUSPEND,
			   SPRD_EPF_NAME,
			   epf_sprd);
		if (ret)
			dev_warn(dev,
				 "request irq number=%d, ret=%d\n",
				 epf_sprd->irq_number,
				 ret);
	}

	epf_set_drvdata(epf, epf_sprd);
	return 0;
}

static void sprd_epf_remove(struct pci_epf *epf)
{
	unsigned long flags;
	struct sprd_ep_res *res, *next;
	void  (*notify)(int event, void *data);
	struct device *dev = &epf->dev;
	struct pci_epc *epc = epf->epc;
	struct sprd_pci_epf *epf_sprd = epf_get_drvdata(epf);

	dev_info(dev, "remove.");

	cancel_work_sync(&epf_sprd->linkup_work);

	spin_lock_irqsave(&epf_sprd->lock, flags);
	list_for_each_entry_safe(res, next, &epf_sprd->res_list, list) {
		pci_epc_unmap_addr(epc, epf->func_no, epf->vfunc_no, res->cpu_phy_addr);
		pci_epc_mem_free_addr(epc, res->cpu_phy_addr,
				      res->cpu_vir_addr,
				      res->size);
		list_del(&res->list);
		devm_kfree(dev, res);
	}
	spin_unlock_irqrestore(&epf_sprd->lock, flags);

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_REMOVE,
				 g_epf_notify[epf->func_no].data);

		g_epf_sprd[epf->func_no] = NULL;
	}

	devm_kfree(dev, epf_sprd);
	epf_set_drvdata(epf, NULL);
}

static struct pci_epf_ops ops = {
	.unbind	= sprd_epf_unbind,
	.bind	= sprd_epf_bind,
};

static struct pci_epf_driver sprd_epf_driver = {
	.driver.name	= SPRD_EPF_NAME,
	.probe		= sprd_epf_probe,
	.remove		= sprd_epf_remove,
	.id_table	= sprd_epf_ids,
	.ops		= &ops,
	.owner		= THIS_MODULE,
};

static int sprd_pci_epf_show(struct seq_file *m, void *unused)
{
	struct sprd_pci_epf *epf_sprd;
	struct pci_epf *epf;
	struct dw_pcie_ep *ep;
	struct dw_pcie *pci;
	u32 index;
	u32 offset;

	epf_sprd = g_epf_sprd[SPRD_FUNCTION_0];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return -1;

	epf = epf_sprd->epf;
	ep = epc_get_drvdata(epf->epc);
	pci = to_dw_pcie_from_ep(ep);

	seq_printf(m, "epf_sprd->no_msi=%d\n", epf_sprd->no_msi);

	/* display ep outbound info */
	for (index = 0; index < pci->num_ob_windows; index++) {
		offset = PCIE_GET_ATU_OUTB_UNR_REG_OFFSET(index);
		seq_printf(m, "OUTBOUND[%d]:\n", index);
		seq_printf(m, "pcie_atu_unr_lower_src_base : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_LOWER_BASE +
									offset));
		seq_printf(m, "pcie_atu_unr_upper_src_base : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_UPPER_BASE +
									offset));
		seq_printf(m, "pcie_atu_unr_lower_limit          : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_LOWER_LIMIT +
									offset));
		seq_printf(m, "pcie_atu_unr_upper_limit          : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_UPPER_LIMIT +
									offset));
		seq_printf(m, "pcie_atu_unr_lower_dst_base : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_LOWER_TARGET +
									offset));
		seq_printf(m, "pcie_atu_unr_upper_dst_base : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_UPPER_TARGET +
									offset));
		seq_printf(m, "pcie_atu_unr_region_ctrl1   : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_REGION_CTRL1 +
									offset));
		seq_printf(m, "pcie_atu_unr_region_ctrl2   : 0x%lx\n",
			   (unsigned long)dw_pcie_readl_dbi(pci, PCIE_ATU_UNR_REGION_CTRL2 +
									offset));
	}
	return 0;
}

static int sprd_pci_epf_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_pci_epf_show, NULL);
}

static const struct file_operations sprd_pci_epf_fops = {
	.owner = THIS_MODULE,
	.open = sprd_pci_epf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *g_epf_debugfs_root;

static int sprd_pci_epf_init_debugfs(void)
{
	struct dentry *g_epf_debugfs_root = debugfs_create_dir("epf", NULL);

	if (!g_epf_debugfs_root)
		return -ENXIO;

	debugfs_create_file("ep", 0444,
			    g_epf_debugfs_root,
			    NULL, &sprd_pci_epf_fops);
	return 0;
}

static void sprd_pci_epf_remove_debugfs(void)
{
	debugfs_remove_recursive(g_epf_debugfs_root);
}

static int __init pci_epf_sprd_init(void)
{
	int ret;

	ret = sprd_pci_epf_init_debugfs();
	if (ret) {
		pr_err("%s: failed to init SPRD PCI epf debugfs, ret=%d\n",
			   __func__, ret);
		return ret;
	}

	ret = pci_epf_register_driver(&sprd_epf_driver);
	if (ret) {
		pr_err("%s: failed to register SPRD PCIe epf driver, ret=%d\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}
module_init(pci_epf_sprd_init);

static void __exit pci_epf_sprd_exit(void)
{
	sprd_pci_epf_remove_debugfs();
	pci_epf_unregister_driver(&sprd_epf_driver);
}
module_exit(pci_epf_sprd_exit);

MODULE_DESCRIPTION("UNISOC SPRD PCIe endpoint function driver");
MODULE_AUTHOR("Ziyi Zhang <ziyi.zhang@unisoc.com>");
MODULE_LICENSE("GPL v2");
