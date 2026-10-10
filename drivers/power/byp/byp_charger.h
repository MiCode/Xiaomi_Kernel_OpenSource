#ifndef _BYP_CHARGER_H_
#define _BYP_CHARGER_H_

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/regmap.h>

#define SC8315_DRV_VERSION          "1.1"
#define SC8315_REGMAX               0x06

#define SC8315_ERR(fmt, ...)        pr_err("SC8315:" fmt, ##__VA_ARGS__)
#define SC8315_INFO(fmt, ...)       pr_err("SC8315:" fmt, ##__VA_ARGS__)

enum pmic_type {
    PMIC_UNKNOWN = 0,
    PMIC_HL7603,
    PMIC_HL7603A,
    PMIC_SC8315
};

struct universal_pmic_data {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *regmap;
    struct mutex i2c_rw_lock;
    
    enum pmic_type type;
    char *name;
    u8 voltage_value;
    u8 chip_id;
    
    // 芯片特定参数
    union {
        struct {
            // HL7603特定参数
            u8 mode_shift;
            u8 en_shift;
        } hl7603;
        
        struct {
            // SC8315特定参数
            int cfg_reset;
            int cfg_dev_en;
            int cfg_mode;
            int cfg_vout_dischg;
            int cfg_ooa_en;
            int cfg_ssfm;
            int cfg_fpwm_cfg;
            int cfg_t_ilim;
            int cfg_t_bps_ocp;
            int cfg_n_comphi;
            int cfg_dcdc_comp;
            int vout;
            int ilim_off;
            int ilim;
            int soft_start;
            int ilin1_set;
        } sc8315;
    } params;
};

enum sc8315_fields {
    F_DEVICE_ID,
    F_RESET, F_DEVICE_EN, F_MODE_CFG, F_VOUT_DISCHG, F_EN_OOA, F_SSFM, F_FPWM_CFG,
    F_VOUT_REG,
    F_ILIN1_SET, F_ILIM_OFF, F_SOFT_START, F_ILIM,
    F_DCDC_COMP, F_N_COMPHI, F_T_BPS_OCP, F_T_ILIM,
    F_TSD, F_HOTDE, F_DCDCMODE, F_OPMODE, F_BPS_OCP, F_BST_OCP, F_FAULT, F_PGOOD,
    F_LIN_FLT, F_SS_FLT, F_VIN_OK, F_VIN_OVP, F_VOUT_OVP, F_COMP_HI,
    F_MAX_FIELDS,
};

struct sc8315_parm {
    int cfg_reset;
    int cfg_dev_en;
    int cfg_mode;
    int cfg_vout_dischg;
    int cfg_ooa_en;
    int cfg_ssfm;
    int cfg_fpwm_cfg;
    int cfg_t_ilim;
    int cfg_t_bps_ocp;
    int cfg_n_comphi;
    int cfg_dcdc_comp;
    int vout;
    int ilim_off;
    int ilim;
    int soft_start;
    int ilin1_set;
};

struct sc_reg_field {
    uint32_t reg;
    uint32_t lsb;
    uint32_t msb;
    bool force_write;
};


#define SC_REG_FIELD(_reg, _lsb, _msb) {           \
                    .reg = _reg,                \
                    .lsb = _lsb,                \
                    .msb = _msb,                \
                    }

#define SC_REG_FIELD_FORCE_WRITE(_reg, _lsb, _msb) {           \
                    .reg = _reg,                \
                    .lsb = _lsb,                \
                    .msb = _msb,                \
                    .force_write = true,        \
                    }

//REGISTER
static const struct sc_reg_field sc8315_reg_fields[] = {
    /*reg00*/
    [F_DEVICE_ID]     = SC_REG_FIELD(0x00, 0, 7),
    /*reg01*/
    [F_RESET]         = SC_REG_FIELD(0x01, 7, 7),
    [F_DEVICE_EN]     = SC_REG_FIELD(0x01, 6, 6),
    [F_MODE_CFG]      = SC_REG_FIELD(0x01, 4, 5),
    [F_VOUT_DISCHG]   = SC_REG_FIELD(0x01, 3, 3),
    [F_EN_OOA]        = SC_REG_FIELD(0x01, 2, 2),
    [F_SSFM]          = SC_REG_FIELD(0x01, 1, 1),
    [F_FPWM_CFG]      = SC_REG_FIELD(0x01, 0, 0),
    /*reg02*/
    [F_VOUT_REG]      = SC_REG_FIELD(0x02, 0, 5),
    /*reg03*/
    [F_ILIN1_SET]     = SC_REG_FIELD(0x03, 6, 7),
    [F_ILIM_OFF]      = SC_REG_FIELD(0x03, 5, 5),
    [F_SOFT_START]    = SC_REG_FIELD(0x03, 4, 4),
    [F_ILIM]          = SC_REG_FIELD(0x03, 0, 3),
    /*reg04*/
    [F_DCDC_COMP]     = SC_REG_FIELD(0x04, 6, 7),
    [F_N_COMPHI]      = SC_REG_FIELD(0x04, 4, 5),
    [F_T_BPS_OCP]     = SC_REG_FIELD(0x04, 3, 3),
    [F_T_ILIM]        = SC_REG_FIELD(0x04, 0, 1),
    /*reg05*/
    [F_TSD]           = SC_REG_FIELD(0x05, 7, 7),
    [F_HOTDE]         = SC_REG_FIELD(0x05, 6, 6),
    [F_DCDCMODE]      = SC_REG_FIELD(0x05, 5, 5),
    [F_OPMODE]        = SC_REG_FIELD(0x05, 4, 4),
    [F_BPS_OCP]       = SC_REG_FIELD(0x05, 3, 3),
    [F_BST_OCP]       = SC_REG_FIELD(0x05, 2, 2),
    [F_FAULT]         = SC_REG_FIELD(0x05, 1, 1),
    [F_PGOOD]         = SC_REG_FIELD(0x05, 0, 0),
    /*reg06*/
    [F_LIN_FLT]       = SC_REG_FIELD(0x06, 7, 7),
    [F_SS_FLT]        = SC_REG_FIELD(0x06, 6, 6),
    [F_VIN_OK]        = SC_REG_FIELD(0x06, 5, 5),
    [F_VIN_OVP]       = SC_REG_FIELD(0x06, 4, 4),
    [F_VOUT_OVP]      = SC_REG_FIELD(0x06, 3, 3),
    [F_COMP_HI]       = SC_REG_FIELD(0x06, 2, 2),
};

#define HL7603_CONFIG0					0x00

#define HL7603_DEV_ID_MASK        		(BIT(7) |BIT(6) |BIT(5) |BIT(4))
#define HL7603_DEV_ID_SHIFT       		4
#define HL7603_DEV_REV_MASK        		(BIT(3) |BIT(2) |BIT(1) |BIT(0))
#define HL7603_DEV_REV_SHIFT       		0

#define HL7603_CONFIG1					0x01

#define HL7603_RESET_MASK               		BIT(7)
#define HL7603_RESET_SHIFT              		7
#define HL7603_DEV_EN_MASK             	BIT(6)
#define HL7603_DEV_EN_SHIFT             	6
#define HL7603_MODE_CFG_MASK        	(BIT(5) |BIT(4))
#define HL7603_MODE_CFG_SHIFT       		4
#define HL7603_VOUT_DISCHG_MASK          BIT(3)
#define HL7603_VOUT_DISCHG_SHIFT         3
#define HL7603_EN_OOA_MASK             	BIT(2)
#define HL7603_EN_OOA_SHIFT             	2
#define HL7603_FPWM_CFG_MASK              BIT(1)
#define HL7603_FPWM_CFG_SHIFT             1

#define HL7603_VOUT_VSEL				0x02
#define HL7603_VOUT_REG_MASK               (BIT(5)|BIT(4)|BIT(3)|BIT(2)|BIT(1)|BIT(0))
#define HL7603_VOUT_REG_SHIFT              	0

#define HL7603_ILIMSET1					0x03

#define HL7603_ILIN1_SET_MASK        		(BIT(7) |BIT(6))
#define HL7603_ILIN1_SET_SHIFT       		6
#define HL7603_ILIM_OFF_MASK    		BIT(5)
#define HL7603_ILIM_OFF_SHIFT   		5
#define HL7603_SOFT_START_MASK   		BIT(4)
#define HL7603_SOFT_START_SHIFT   		4
#define HL7603_ILIM_MASK        			(BIT(3) |BIT(2)|BIT(0))
#define HL7603_ILIM_SHIFT       			0

#define HL7603_ILIMSET2					0x04

#define HL7603_T_ILIM_H_MASK        		(BIT(1) |BIT(0))
#define HL7603_T_ILIM_H_SHIFT       		0

#define HL7603_STATUS					0x05

#define HL7603_TSD_MASK               		BIT(7)
#define HL7603_TSD_SHIFT              		7
#define HL7603_HOTDIE_MASK             	BIT(6)
#define HL7603_HOTDIE_SHIFT             	6
#define HL7603_DCDCMODE_MASK        	BIT(5)
#define HL7603_DCDCMODE_SHIFT       	5
#define HL7603_OPMODE_MASK        		BIT(4)
#define HL7603_OPMODE_SHIFT       		4
#define HL7603_VIN_OVP_MASK         	 	BIT(3)
#define HL7603_VIN_OVP_SHIFT         		3	
#define HL7603_VOUT_OVP_MASK             	BIT(2)
#define HL7603_VOUT_OVP_SHIFT             	2
#define HL7603_FAULT_MASK              		BIT(1)
#define HL7603_FAULT_SHIFT             		1
#define HL7603_PGOOD_MASK              	BIT(0)
#define HL7603_PGOOD_SHIFT             		0

#define HL7603_CHIP_ID         0xB3
#define HL7603A_CHIP_ID        0xB4
#define SC8315_CHIP_ID         0x05

typedef unsigned char  kal_uint8;
typedef unsigned short  kal_uint16;
typedef unsigned int  kal_uint32;


//extern void hl7603_driver_probe(void);
//extern int hl7603_enable(int id, unsigned char en);
//extern int hl7603_set_mode(int id, unsigned char mode);

//extern void hl7603_dump_register(int id);
//extern int hl7603_is_enabled(int id);


#define EXTBUCK_hl7603	1
/**********************************************************
  *   Global Variable
  *********************************************************/
//static struct mt_i2c_t hl7603_i2c;


#define hl7603_print(fmt, args...)	dev_err(chip->dev, "[HL7603] " fmt, ##args)

#endif