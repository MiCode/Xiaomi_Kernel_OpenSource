#ifndef __INTEL_WCOVE_BCU_H__
#define __INTEL_WCOVE_BCU_H__

#define DRIVER_NAME "wcove_bcu"
#define DEVICE_NAME "wcove_pmic_bcu"

/* Generic bit representaion macros */
#define B0	(1 << 0)
#define B1	(1 << 1)
#define B2	(1 << 2)
#define B3	(1 << 3)
#define B4	(1 << 4)
#define B5	(1 << 5)
#define B6	(1 << 6)
#define B7	(1 << 7)

/* IRQ registers */
#define BCUIRQ_REG              0x6E07
#define IRQLVL1_REG             0x6E02
#define MIRQLVL1_REG            0x6E0E

/*IRQ Mask Register*/
#define MBCUIRQ_REG             0x6E14

/* Status registers */
#define SBCUIRQ_REG             0x6EBB
#define SBCUCTRL_REG            0x6EBC

/* Voltage Trip Point Configuration Register */
#define VWARNA_CFG_REG          0x6EB4
#define VWARNB_CFG_REG          0x6EB5
#define VCRIT_CFG_REG           0x6EB6

/* Current Trip Point Configuration Register */
#define ICCMAXVCC_CFG_REG       0x6EFB
#define ICCMAXVNN_CFG_REG       0x6EFC
#define ICCMAXVGG_CFG_REG       0x6EFD

/* Output Pin Behavior Register */
#define BCUDISB_BEH_REG         0x6EB8
#define BCUDISCRIT_BEH_REG      0x6EB9
#define BCUVSYS_DRP_BEH_REG     0x6EBA

#define MAX_VOLTAGE_TRIP_POINTS 3
#define MAX_CURRENT_TRIP_POINTS 3

#define MBCU                    B2

#define VWARNA_EN               B3
#define ICCMAXVCC_EN            B7

#define MVCRIT			B2
#define MVWARNA			B1
#define MVWARNB			B0

#define VWARNB                  B0
#define VWARNA                  B1
#define VCRIT                   B2
#define GSMPULSE                B3
#define TXPWRTH                 B4

#define SVWARNB                 B0
#define SVWARNA                 B1
#define SCRIT                   B2

#define SBCUDISB                B2
#define SBCUDISCRIT             B1

/* Max length of the register name string */
#define MAX_REGNAME_LEN		20

/* Max number register from platform config */
#define MAX_BCUCFG_REGS         10

/* check whether bit is sticky or not by checking bit 2 */
#define IS_BCUDISB_STICKY(data)		(!!(data & B2))

/* Check  BCUDISB Output Pin enable on assertion of VWARNB crossing */
#define IS_ASSRT_ON_BCUDISB(data)	(!!(data & B0))

/* Macro to get the mode of acess for the BCU registers	*/
#define MODE(r)	(((r != BCUIRQ_REG) && (r != IRQLVL1_REG) && \
			(r != SBCUIRQ_REG))	\
			? (S_IRUGO | S_IWUSR) : S_IRUGO)

/* Generic macro to assign the parameters (reg name and address) */
#define reg_info(x)	{ .name = #x, .addr = x, .mode = MODE(x) }

/**
 * These values are read from platform.
 * platform get these entries - default register configurations
 * BCU is programmed to these default values during boot time.
 */
struct wcpmic_bcu_config_data {
	u16 addr;
	u8 data;
};

struct wcove_bcu_platform_data {
	struct wcpmic_bcu_config_data config[MAX_BCUCFG_REGS];
	int num_regs;
};

struct bcu_reg_info {
	char	name[MAX_REGNAME_LEN];	/* register name   */
	u16	addr;			/* offset address  */
	u16	mode;			/* permission mode */
};

#endif /* __INTEL_WCOVE_BCU_H__ */
