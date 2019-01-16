#ifndef _MT_CPUXGPT_H_
#define _MT_CPUXGPT_H_


typedef enum cpuxgpt_num {
	CPUXGPT0 = 0,
	CPUXGPT1,
	CPUXGPT2,
	CPUXGPT3,
	CPUXGPT4,
	CPUXGPT5,
	CPUXGPT6,
	CPUXGPT7,
	CPUXGPTNUMBERS,
} CPUXGPT_NUM;

#define CPUXGPT0_IRQID 88
#define CPUXGPT1_IRQID 89
#define CPUXGPT2_IRQID 90
#define CPUXGPT3_IRQID 91
#define CPUXGPT4_IRQID 92
#define CPUXGPT5_IRQID 93
#define CPUXGPT6_IRQID 94
#define CPUXGPT7_IRQID 95

#define CPUXGPT_IRQID_BASE CPUXGPT0_IRQID

#define CPUXGPT_BASE 0xF0200000
#define INDEX_BASE  (CPUXGPT_BASE+0x0060)
#define CTL_BASE    (CPUXGPT_BASE+0x0064)

/* REG */
#define INDEX_CTL_REG  0x000
#define INDEX_STA_REG  0x004
#define INDEX_CNT_L_INIT    0x008
#define INDEX_CNT_H_INIT    0x00C
#define INDEX_IRQ_MASK    0x030	/* 0~7 bit mask cnt0~cnt7 interrupt */

#define INDEX_CMP_BASE  0x034




/* CTL_REG SET */
#define EN_CPUXGPT 0x01
#define EN_AHLT_DEBUG 0x02
/* #define CLK_DIV1  (0b001 << 8) */
/* #define CLK_DIV2  (0b010 << 8) */
/* #define CLK_DIV4  (0b100 << 8) */
#define CLK_DIV1  (0x1 << 8)
#define CLK_DIV2  (0x2 << 8)
#define CLK_DIV4  (0x4 << 8)
#define CLK_DIV_MASK (~(0x7<<8))

#define CPUX_GPT0_ACK            (1<<0x0)
#define CPUX_GPT1_ACK            (1<<0x1)
#define CPUX_GPT2_ACK            (1<<0x2)
#define CPUX_GPT3_ACK            (1<<0x3)
#define CPUX_GPT4_ACK            (1<<0x4)
#define CPUX_GPT5_ACK            (1<<0x5)
#define CPUX_GPT6_ACK            (1<<0x6)
#define CPUX_GPT7_ACK            (1<<0x7)

void enable_cpuxgpt(void);
void set_cpuxgpt_clk(unsigned int div);
void disable_cpuxgpt(void);
int cpu_xgpt_set_timer(int id, u64 ns);
int cpu_xgpt_set_cmp(CPUXGPT_NUM cpuxgpt_num, u64 count);
int cpu_xgpt_register_timer(unsigned int id, irqreturn_t(*func) (int irq, void *dev_id));
void cpu_xgpt_set_init_count(unsigned int countH, unsigned int countL);
void cpu_xgpt_halt_on_debug_en(int en);
u64 localtimer_get_phy_count(void);
unsigned int cpu_xgpt_irq_dis(int cpuxgpt_num);
void restore_cpuxgpt(void);
void save_cpuxgpt(void);

#endif
