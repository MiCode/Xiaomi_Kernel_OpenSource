#ifndef __ETM_H
#define __ETM_H

struct etm_driver_data {
	void __iomem *etm_regs;
	int is_ptm;
	const int *pwr_down;
};

struct etb_driver_data {
	void __iomem *etb_regs;
	void __iomem *funnel_regs;
	void __iomem *tpiu_regs;
	void __iomem *dem_regs;
	int use_etr;
	u32 etr_len;
	u32 etr_virt;
	dma_addr_t etr_phys;
};

extern void trace_start_by_cpus(const struct cpumask *mask, int init_etb);

#endif
