#ifndef __MT_REG_DUMP_H
#define __MT_REG_DUMP_H

#define CORE0_PC (MCUSYS_CFGREG_BASE + 0x300)
#define CORE0_FP (MCUSYS_CFGREG_BASE + 0x304)
#define CORE0_SP (MCUSYS_CFGREG_BASE + 0x308)
#define CORE1_PC (MCUSYS_CFGREG_BASE + 0x310)
#define CORE1_FP (MCUSYS_CFGREG_BASE + 0x314)
#define CORE1_SP (MCUSYS_CFGREG_BASE + 0x318)
#define CORE2_PC (MCUSYS_CFGREG_BASE + 0x320)
#define CORE2_FP (MCUSYS_CFGREG_BASE + 0x324)
#define CORE2_SP (MCUSYS_CFGREG_BASE + 0x328)
#define CORE3_PC (MCUSYS_CFGREG_BASE + 0x330)
#define CORE3_FP (MCUSYS_CFGREG_BASE + 0x334)
#define CORE3_SP (MCUSYS_CFGREG_BASE + 0x338)

struct mt_reg_dump {
	unsigned int pc;
	unsigned int fp;
	unsigned int sp;
	unsigned int core_id;
};

extern int mt_reg_dump(char *buf);

#endif

