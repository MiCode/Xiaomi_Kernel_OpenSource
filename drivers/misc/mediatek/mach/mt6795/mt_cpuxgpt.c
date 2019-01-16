
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/smp.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_cpuxgpt.h>
#include <mach/mt_timer.h>
#include <mach/irqs.h>

/*IRQ for per CPU setting*/
extern void mt_gic_cfg_irq2cpu(unsigned int irq, unsigned int cpu, unsigned int set);

static DEFINE_SPINLOCK(cpuxgpt_reg_lock);

static irqreturn_t(*user_handlers[CPUXGPTNUMBERS]) (int irq, void *dev_id) = {
0};

static unsigned int __read_cpuxgpt(unsigned int reg_index)
{
	unsigned int value = 0;
	DRV_WriteReg32(INDEX_BASE, reg_index);

	value = DRV_Reg32(CTL_BASE);
	return value;
}

static void __write_cpuxgpt(unsigned int reg_index, unsigned int value)
{

	DRV_WriteReg32(INDEX_BASE, reg_index);
	DRV_WriteReg32(CTL_BASE, value);
}


static void __cpuxgpt_enable(void)
{
	unsigned int tmp = 0;
	spin_lock(&cpuxgpt_reg_lock);
	tmp = __read_cpuxgpt(INDEX_CTL_REG);
	tmp |= EN_CPUXGPT;
	__write_cpuxgpt(INDEX_CTL_REG, tmp);
	spin_unlock(&cpuxgpt_reg_lock);
}

static void __cpuxgpt_disable(void)
{
	unsigned int tmp = 0;
	spin_lock(&cpuxgpt_reg_lock);
	tmp = __read_cpuxgpt(INDEX_CTL_REG);
	tmp &= (~EN_CPUXGPT);
	__write_cpuxgpt(INDEX_CTL_REG, tmp);
	spin_unlock(&cpuxgpt_reg_lock);
}

static void __cpuxgpt_halt_on_debug_en(int en)
{
	unsigned int tmp = 0;
	spin_lock(&cpuxgpt_reg_lock);
	tmp = __read_cpuxgpt(INDEX_CTL_REG);
	if (1 == en) {
		tmp |= EN_AHLT_DEBUG;
	}
	if (0 == en) {
		tmp &= (~EN_AHLT_DEBUG);
	}
	__write_cpuxgpt(INDEX_CTL_REG, tmp);
	spin_unlock(&cpuxgpt_reg_lock);
}


/* 26M DIV */
static void __cpuxgpt_set_clk(unsigned int div)
{
	unsigned int tmp = 0;

	/* printk("%s fwq  div is  0x%x\n",__func__, div); */
	if (div != CLK_DIV1 && div != CLK_DIV2 && div != CLK_DIV4) {
		printk("%s error: div is not right\n", __func__);
	}
	spin_lock(&cpuxgpt_reg_lock);
	tmp = __read_cpuxgpt(INDEX_CTL_REG);
	tmp &= CLK_DIV_MASK;
	tmp |= div;
	__write_cpuxgpt(INDEX_CTL_REG, tmp);
	spin_unlock(&cpuxgpt_reg_lock);
}

#if 0
static unsigned int __cpuxgpt_get_status(void)
{
	unsigned int tmp = 0;
	tmp = __read_cpuxgpt(INDEX_STA_REG);
	return tmp;
}
#endif
static void __cpuxgpt_set_init_cnt(unsigned int countH, unsigned int countL)
{

	spin_lock(&cpuxgpt_reg_lock);
	__write_cpuxgpt(INDEX_CNT_H_INIT, countH);
	__write_cpuxgpt(INDEX_CNT_L_INIT, countL);	/* update count when countL programmed */
	spin_unlock(&cpuxgpt_reg_lock);
}

static unsigned int __cpuxgpt_irq_en(int cpuxgpt_num)
{
	unsigned int tmp = 0;
	spin_lock(&cpuxgpt_reg_lock);
	tmp = __read_cpuxgpt(INDEX_IRQ_MASK);
	tmp |= (1 << cpuxgpt_num);
	__write_cpuxgpt(INDEX_IRQ_MASK, tmp);
	spin_unlock(&cpuxgpt_reg_lock);
	return 0;
}

static unsigned int __cpuxgpt_irq_dis(int cpuxgpt_num)
{
	unsigned int tmp = 0;
	spin_lock(&cpuxgpt_reg_lock);
	tmp = __read_cpuxgpt(INDEX_IRQ_MASK);
	tmp &= (~(1 << cpuxgpt_num));
	__write_cpuxgpt(INDEX_IRQ_MASK, tmp);
	spin_unlock(&cpuxgpt_reg_lock);

	return 0;
}

static unsigned int __cpuxgpt_set_cmp(CPUXGPT_NUM cpuxgpt_num, int countH, int countL)
{
	spin_lock(&cpuxgpt_reg_lock);
	__write_cpuxgpt(INDEX_CMP_BASE + (cpuxgpt_num * 0x8) + 0x4, countH);
	__write_cpuxgpt(INDEX_CMP_BASE + (cpuxgpt_num * 0x8), countL);	/* update CMP  when countL programmed */
	spin_unlock(&cpuxgpt_reg_lock);
	return 0;
}

static irqreturn_t __cpuxgpt0_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt0 irq accured\n" ); */
	__cpuxgpt_irq_dis(0);
	user_handlers[0] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t __cpuxgpt1_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt1 irq accured\n" ); */
	__cpuxgpt_irq_dis(1);
	user_handlers[1] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t __cpuxgpt2_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt2 irq accured\n" ); */
	__cpuxgpt_irq_dis(2);
	user_handlers[2] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t __cpuxgpt3_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt3 irq accured\n" ); */
	__cpuxgpt_irq_dis(3);
	user_handlers[3] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t __cpuxgpt4_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt4 irq accured\n" ); */
	__cpuxgpt_irq_dis(4);
	user_handlers[4] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t __cpuxgpt5_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt5 irq accured\n" ); */
	__cpuxgpt_irq_dis(5);
	user_handlers[5] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t __cpuxgpt6_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt6 irq accured\n" ); */
	__cpuxgpt_irq_dis(6);
	user_handlers[6] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t __cpuxgpt7_irq_handler(int irq, void *dev_id)
{
	/* printk("cpuxgpt7 irq accured\n" ); */
	__cpuxgpt_irq_dis(7);
	user_handlers[7] (irq, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t(*cpuxgpt_irq_handler[]) (int irq, void *dev_id) = {
__cpuxgpt0_irq_handler, __cpuxgpt1_irq_handler, __cpuxgpt2_irq_handler, __cpuxgpt3_irq_handler, __cpuxgpt4_irq_handler, __cpuxgpt5_irq_handler, __cpuxgpt6_irq_handler, __cpuxgpt7_irq_handler,};	/* support 8 timer call back */

static int __get_irq_id(int id)
{
	switch (id) {
	case 0:
		return CPUXGPT0_IRQID;
	case 1:
		return CPUXGPT1_IRQID;
	case 2:
		return CPUXGPT2_IRQID;
	case 3:
		return CPUXGPT3_IRQID;
	case 4:
		return CPUXGPT4_IRQID;
	case 5:
		return CPUXGPT5_IRQID;
	case 6:
		return CPUXGPT6_IRQID;
	case 7:
		return CPUXGPT7_IRQID;

	}
	printk("%s:fine irq id error\n", __func__);
	return -1;
}

void enable_cpuxgpt(void)
{
	__cpuxgpt_enable();
	printk("%s: reg(%x)\n", __func__, __read_cpuxgpt(INDEX_CTL_REG));
}

void disable_cpuxgpt(void)
{
	__cpuxgpt_disable();
	printk("%s: reg(%x)\n", __func__, __read_cpuxgpt(INDEX_CTL_REG));
}


void set_cpuxgpt_clk(unsigned int div)
{
	__cpuxgpt_set_clk(div);
	/* printk("%s: reg(%x)\n",__func__,__read_cpuxgpt(INDEX_CTL_REG)); */
}

static unsigned int g_ctl;
void restore_cpuxgpt(void)
{
	__write_cpuxgpt(INDEX_CTL_REG, g_ctl);
	printk("g_ctl:0x%x, %s\n", __read_cpuxgpt(INDEX_CTL_REG), __func__);

}

void save_cpuxgpt(void)
{
	g_ctl = __read_cpuxgpt(INDEX_CTL_REG);
	printk("g_ctl:0x%x, %s\n", g_ctl, __func__);
}

int cpu_xgpt_register_timer(unsigned int id, irqreturn_t(*func) (int irq, void *dev_id))
{
	int ret = 0;
	int irq_id = 0;
	char *name;
	if (id > 7 || id < 0) {
		printk("%s: err idnumber id=%d should be 0~7\n", __func__, id);
		return -1;
	}

	switch (id) {
	case 0:
		name = "mtk_cpuxgpt0";
		break;
	case 1:
		name = "mtk_cpuxgpt1";
		break;
	case 2:
		name = "mtk_cpuxgpt2";
		break;
	case 3:
		name = "mtk_cpuxgpt3";
		break;
	case 4:
		name = "mtk_cpuxgpt4";
		break;
	case 5:
		name = "mtk_cpuxgpt5";
		break;
	case 6:
		name = "mtk_cpuxgpt6";
		break;
	case 7:
		name = "mtk_cpuxgpt7";
		break;

	}
	if (func) {
		user_handlers[id] = func;
	}
	/* sprintf(name, "mtk_cpuxgpt%d", id); */

	irq_id = __get_irq_id(id);

	/*cpuxgpt assigne  for per core */
	mt_gic_cfg_irq2cpu(irq_id, 0, 0);	/*don't trigger IRQ to CPU0 */
	mt_gic_cfg_irq2cpu(irq_id, (irq_id - CPUXGPT_IRQID_BASE) % num_possible_cpus(), 1);	/*trigger IRQ to CPUx */

	ret =
	    request_irq(irq_id, (irq_handler_t) cpuxgpt_irq_handler[id], IRQF_TRIGGER_HIGH, name,
			NULL);
	if (ret != 0) {
		printk("%s:%s fail to register irq\n", __func__, name);
		return ret;
	}

	printk("%s:%s register irq (%d) ok\n", __func__, name, irq_id);

	return 0;
}

int cpu_xgpt_set_timer(int id, u64 ns)
{
	u64 count = 0;
	u64 now = 0;
	u64 set_count = 0;
	unsigned int set_count_lo = 0;
	unsigned int set_count_hi = 0;
	count = ns;
	now = localtimer_get_phy_count();
	do_div(count, 1000 / 13);

	set_count = count + now;
	set_count_lo = 0x00000000FFFFFFFF & set_count;
	set_count_hi = (0xFFFFFFFF00000000 & set_count) >> 32;

	printk("%s:set cpuxgpt(%d) count(%u,%u)\n", __func__, id, set_count_hi, set_count_lo);

	__cpuxgpt_set_cmp(id, set_count_hi, set_count_lo);
	__cpuxgpt_irq_en(id);
	return 0;
}

int cpu_xgpt_set_cmp_HL(CPUXGPT_NUM cpuxgpt_num, int countH, int countL)
{
	__cpuxgpt_set_cmp(cpuxgpt_num, countH, countL);
	__cpuxgpt_irq_en(cpuxgpt_num);

	return 0;
}

void cpu_xgpt_set_init_count(unsigned int countH, unsigned int countL)
{
	__cpuxgpt_set_init_cnt(countH, countL);

}

void cpu_xgpt_halt_on_debug_en(int en)
{
	__cpuxgpt_halt_on_debug_en(en);
}

u64 localtimer_get_phy_count(void)
{
	u64 cval = 0;

	asm volatile ("mrrc p15, 0, %Q0, %R0, c14":"=r" (cval));

	return cval;
}

unsigned int cpu_xgpt_irq_dis(int cpuxgpt_num)
{
	__cpuxgpt_irq_dis(cpuxgpt_num);

	return 0;
}

int cpu_xgpt_set_cmp(CPUXGPT_NUM cpuxgpt_num, u64 count)
{

	unsigned int set_count_lo = 0;
	unsigned int set_count_hi = 0;

	set_count_lo = 0x00000000FFFFFFFF & count;
	set_count_hi = (0xFFFFFFFF00000000 & count) >> 32;

	cpu_xgpt_set_cmp_HL(cpuxgpt_num, set_count_hi, set_count_lo);
	return 0;
}
