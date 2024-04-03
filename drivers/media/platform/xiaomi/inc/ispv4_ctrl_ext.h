#ifndef _ISPV4_CTRL_EXT_H_
#define _ISPV4_CTRL_EXT_H_

#include <linux/types.h>
#include <linux/platform_device.h>

#define ISPV4_PMIC_ON   1
#define ISPV4_PMIC_OFF  0

enum ispv4_io_irq_option {
	ISPV4_WDT_IRQ = 0,
	ISPV4_MBOX_IRQ,
	ISPV4_PMU_IRQ = ISPV4_MBOX_IRQ, /*shared interrupt with mbox intc*/
	ISPV4_BUSMON_IRQ,
	ISPV4_SOF_IRQ = ISPV4_BUSMON_IRQ, /*shared interrupt with busmonitor intc*/
	ISPV4_MAX_IRQ,
};

typedef struct io_irq_info {
	int32_t gpio[ISPV4_MAX_IRQ];
	uint32_t gpio_irq[ISPV4_MAX_IRQ];
} ispv4_irq_info_t;

struct ispv4_ctrl_data {
	struct device comp_dev;
	struct platform_device *pdev;
	//CLK
	struct clk *bb_clk;
	struct clk *sleep_clk;
	struct pinctrl *pinctrl_clk;
	//IO
	int32_t ispv4_mipi_iso_en;
	int32_t ispv4_pmic_pwr_on;
	int32_t ispv4_reset_n;
	int32_t ispv4_pmic_irq;
	int32_t ispv4_pmic_pwr_gd;
	uint32_t ispv4_pmic_irqnum;
	uint32_t ispv4_pmic_pwr_gd_irqnum;
	ispv4_irq_info_t irq_info;
	//Ctrl
	struct completion pwr_gd_com;
	struct completion pwr_err_com;
	struct completion pmu_com;
	bool wdt_en_flag;
	//dbg
	struct dentry *dbg_dentry;
	struct dentry *dbg_clk_dentry;
	struct dentry *dbg_pmic_pon_dentry;
	struct dentry *dbg_release_rst_dentry;
	struct dentry *dbg_pwr_cpu_dentry;
	struct dentry *dbg_pwr_seq_dentry;
	//Callback
	int (*wdt_notify)(void * priv);
	void *wdt_notify_priv;
	int (*sof_notify)(void * priv);
	void *sof_notify_priv;
	//Pmu_reply
	bool pmu_reply;
};

int ispv4_power_on_cpu(struct platform_device *pdev);
int ispv4_power_on_sequence_preconfig(struct platform_device *pdev);
int ispv4_power_on_pmic(struct platform_device *pdev);
int ispv4_power_on_chip(struct platform_device *pdev);
#if !(IS_ENABLED(CONFIG_MIISP_CHIP))
void ispv4_fpga_reset(struct platform_device *pdev);
#endif

#endif
