#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <asm/io.h>

#include <mach/mt_spm_sleep.h>

#include "../lastpc.h"

struct lastpc_imp {
	struct lastpc_plt plt;
	void __iomem *toprgu_reg;
};

#define to_lastpc_imp(p)	container_of((p), struct lastpc_imp, plt);

static int lastpc_plt_start(struct lastpc_plt *plt)
{
#if 0
	struct lastpc_imp *drv = to_lastpc_imp(plt);

	/* magic number?? Hmmm, you might want to chat with DE, Tsou-Han Chiu */
	writel(0x59000001, drv->toprgu_reg);
#endif
	return 0;
}

#if 0
static int lastpc_plt_dump(struct lastpc_plt *plt, char *buf, int len)
{
	void __iomem *base = plt->common->base;
	int ret = -1, cnt = num_possible_cpus();
	char *ptr = buf;
	unsigned long pc_value;
	unsigned long fp_value;
	unsigned long sp_value;
	unsigned long size = 0;
	unsigned long offset = 0;
	char str[KSYM_SYMBOL_LEN];
	int i;
	int cluster, cpu_in_cluster;

	if (cnt < 0) {
		return ret;
	}

	/* lastpc base is the MCU base plus 0x410 */
	base += 0x410;

	/* Get PC, FP, SP and save to buf */
	for (i = 0; i < cnt; i++) {
		cluster = i / 4;
		cpu_in_cluster = i % 4;
		pc_value = readl(IOMEM((base+0x0) + (cpu_in_cluster << 5) + (0x100 * cluster)));
		fp_value = readl(IOMEM((base+0x10) + (cpu_in_cluster << 5) + (0x100 * cluster)));
		sp_value = readl(IOMEM((base+0x18) + (cpu_in_cluster << 5) + (0x100 * cluster)));
		kallsyms_lookup((unsigned long)pc_value, &size, &offset, NULL, str);
		ptr += sprintf(ptr, "[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n", i, pc_value, str, offset, fp_value, sp_value);
		printk("[LAST PC] CORE_%d PC = 0x%lx(%s), FP = 0x%lx, SP = 0x%lx\n", i, pc_value, str, fp_value, sp_value);
	}

	return 0;
}
#else
/* Use SPM to dump lastpc, since rome has chip issue for lastpc */
#ifdef CONFIG_ARM64
extern uint32_t get_suspend_debug_flag(void);

static int lastpc_plt_dump(struct lastpc_plt *plt, char *buf, int len)
{
	int ret = -1, cnt = num_possible_cpus();
	char *ptr = buf;
	unsigned long pc_value;
	unsigned long fp_value;
	unsigned long sp_value;
	unsigned long size = 0;
	unsigned long offset = 0;
	char str[KSYM_SYMBOL_LEN];
	unsigned int *data = NULL;
	int i;

	if (cnt < 0) {
		return ret;
	}

	/* data would point to core0_pc, core0_sp, core0_fp, core1_pc, core1_sp, core1_fp,... */
	data = kmalloc(cnt*8*3, GFP_ATOMIC);
	if (!data) {
		pr_err("%s:%d: kmalloc failed\n", __func__, __LINE__);
		return ret;
	}

	/* 0x3 means complete */
	if (get_suspend_debug_flag() == 0x3) {
		read_pcm_data(data, cnt*3*2);
	/* 0x0 means MD32 didn't start */
	} else if (get_suspend_debug_flag() == 0x0) {
		memset(data, 0xa, cnt*8*3);
	/* 0x1 means MD32 didn't finish its job */
	} else if (get_suspend_debug_flag() == 0x1) {
		memset(data, 0xb, cnt*8*3);
	/* 0x2 means MD32 finished its job but not record its start */
	} else if (get_suspend_debug_flag() == 0x2) {
		memset(data, 0xc, cnt*8*3);
	/* other cases mean the debug flag is polluted */
	} else {
		memset(data, 0xd, cnt*8*3);
	}

	/* Get PC, FP, SP and save to buf */
	for (i = 0; i < cnt*3*2; i+=6) {
		pc_value = 0;
		fp_value = 0;
		sp_value = 0;
		pc_value = data[i+1];
		pc_value <<= 32;
		pc_value |= data[i];
		fp_value = data[i+3];
		fp_value <<= 32;
		fp_value |= data[i+2];
		sp_value = data[i+5];
		sp_value <<= 32;
		sp_value |= data[i+4];
		kallsyms_lookup((unsigned long)pc_value, &size, &offset, NULL, str);
		ptr += sprintf(ptr, "[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n", (i/6), pc_value, str, offset, fp_value, sp_value);
		printk("[LAST PC] CORE_%d PC = 0x%lx(%s), FP = 0x%lx, SP = 0x%lx\n", (i/6), pc_value, str, fp_value, sp_value);
	}

	kfree(data);
	return 0;
}
#else
#define	LASTPC					0X20
#define	LASTSP					0X24
#define	LASTFP  0X28
#define	MUX_CONTOL_CA7_REG		(base + 0x140)
#define	MUX_READ_CA7_REG		(base + 0x144)
#define	MUX_CONTOL_CA17_REG		(base + 0x21C)
#define	MUX_READ_CA17_REG		(base + 0x25C)

static const u32 LASTPC_MAGIC_NUM[] = {0X3, 0XB, 0X33, 0X43};

static int lastpc_plt_dump(struct lastpc_plt *plt, char *buf, int len)
{
	/* Get core numbers */
	int ret = -1, cnt = num_possible_cpus();
	char *ptr = buf;
	unsigned int pc_value;
	unsigned int pc_i1_value;
	unsigned int fp_value;
	unsigned int sp_value;
	unsigned long size = 0;
	unsigned long offset = 0;
	char str[KSYM_SYMBOL_LEN];
	int i;
	int cluster, cpu_in_cluster;
	void __iomem *base = plt->common->base;

	if(cnt < 0)
		return ret;

	/* Get PC, FP, SP and save to buf */
	for (i = 0; i < cnt; i++) {
		cluster = i / 4;
		cpu_in_cluster = i % 4;
		if(cluster == 0) {
			writel(LASTPC + i, MUX_CONTOL_CA7_REG);
			pc_value = readl(MUX_READ_CA7_REG);
			writel(LASTSP + i, MUX_CONTOL_CA7_REG);
			sp_value = readl(MUX_READ_CA7_REG);
			writel(LASTFP + i, MUX_CONTOL_CA7_REG);
			fp_value = readl(MUX_READ_CA7_REG);
			kallsyms_lookup((unsigned long)pc_value, &size, &offset, NULL, str);
			ptr += sprintf(ptr, "CORE_%d PC = 0x%x(%s + 0x%lx), FP = 0x%x, SP = 0x%x\n", i, pc_value, str, offset, fp_value, sp_value);
		}
		else{
			writel(LASTPC_MAGIC_NUM[cpu_in_cluster], MUX_CONTOL_CA17_REG);
			pc_value = readl(MUX_READ_CA17_REG);
			writel(LASTPC_MAGIC_NUM[cpu_in_cluster] + 1, MUX_CONTOL_CA17_REG);
			pc_i1_value = readl(MUX_READ_CA17_REG);
			ptr += sprintf(ptr, "CORE_%d PC_i0 = 0x%x, PC_i1 = 0x%x\n", i, pc_value, pc_i1_value);
		}
	}

	//printk("CORE_%d PC = 0x%x(%s), FP = 0x%x, SP = 0x%x\n", i, pc_value, str, fp_value, sp_value);
	return 0;
}
#endif
#endif

static int reboot_test(struct lastpc_plt *plt)
{
	return 0;
}

static struct lastpc_plt_operations lastpc_ops = {
	.start = lastpc_plt_start,
	.dump  = lastpc_plt_dump,
	.reboot_test = reboot_test,
};

static int __init lastpc_init(void)
{
	struct lastpc_imp *drv = NULL;
	int ret = 0;

	drv = kzalloc(sizeof(struct lastpc_imp), GFP_KERNEL);
	if (!drv) {
		pr_err("%s:%d: kzalloc fail.\n", __func__, __LINE__);
		return -ENOMEM;
	}

	drv->plt.ops = &lastpc_ops;
	drv->plt.chip_code = 0x6795;
	drv->plt.min_buf_len = 2048; //TODO: can calculate the len by how many levels of bt we want
	drv->toprgu_reg = ioremap(0x10007040, 0x4); //magic number?? Hmmm, you might want to chat with DE, Tsou-Han Chiu

	ret = lastpc_register(&drv->plt);
	if (ret) {
		pr_err("%s:%d: lastpc_register failed\n", __func__, __LINE__);
		goto register_lastpc_err;
	}

	return 0;

register_lastpc_err:
	kfree(drv);
	return ret;
}

arch_initcall(lastpc_init);
