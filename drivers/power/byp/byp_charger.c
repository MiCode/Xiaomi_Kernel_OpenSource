#include "byp_charger.h"

/*********************I2C API*********************/
/**
 * return 0 for successful operation,
 * return negative value for err operation
 */
static int sc8315_i2c_write_bytes(struct universal_pmic_data *sc, uint16_t reg, uint8_t len, uint8_t *val)
{
    int ret = 0;
    if (sc == NULL) {
        SC8315_ERR("%s : sc is NULL\n", __func__);
        return 0;
    }
    if (sc->regmap == NULL) {
        SC8315_ERR("%s : sc->regmap is NULL\n", __func__);
        return 0;
    }
    ret = regmap_bulk_write(sc->regmap, reg, val, len);
    if (ret < 0) {
        SC8315_ERR("i2c bulk read failed\n");
    }
    return ret;
}

/**
 * return 0 for successful operation,
 * return negative value for err operation
 */
static int sc8315_i2c_read_bytes(struct universal_pmic_data *sc, uint16_t reg, uint8_t len, uint8_t *val)
{
    int ret = 0;
    if (sc == NULL) {
        SC8315_ERR("%s : sc is NULL\n", __func__);
        return 0;
    }
    if (sc->regmap == NULL) {
        SC8315_ERR("%s : sc->regmap is NULL\n", __func__);
        return 0;
    }
    ret = regmap_bulk_read(sc->regmap, reg, val, len);
    if (ret < 0) {
        SC8315_ERR("i2c bulk read failed\n");
    }
    return ret;
}

static int sc8315_i2c_write_byte(struct universal_pmic_data *sc, uint16_t reg, uint8_t val)
{
    return sc8315_i2c_write_bytes(sc, reg, 1, &val);
}

static int sc8315_i2c_read_byte(struct universal_pmic_data *sc, uint16_t reg, uint8_t *val)
{
    return sc8315_i2c_read_bytes(sc, reg, 1, val);
}

static int sc8315_field_read(struct universal_pmic_data *sc,
                enum sc8315_fields field_id, int *val)
{
    int ret;
    uint8_t reg_val = 0;
    uint8_t mask = GENMASK(sc8315_reg_fields[field_id].msb, sc8315_reg_fields[field_id].lsb);

    ret = sc8315_i2c_read_byte(sc, sc8315_reg_fields[field_id].reg, &reg_val);
    if (ret < 0) {
        SC8315_ERR("sc8315 read field %d fail: %d\n", field_id, ret);
        return ret;
    }

    reg_val &= mask;
    reg_val >>= sc8315_reg_fields[field_id].lsb;

    *val = reg_val;
    return ret;
}

static int sc8315_field_write(struct universal_pmic_data *sc,
                enum sc8315_fields field_id, int val)
{
    int ret = 0;
    uint8_t reg_val = 0, tmp = 0;
    uint8_t mask = GENMASK(sc8315_reg_fields[field_id].msb, sc8315_reg_fields[field_id].lsb);

    ret = sc8315_i2c_read_byte(sc, sc8315_reg_fields[field_id].reg, &reg_val);
    if (ret < 0) {
        goto out;
    }

    tmp = reg_val & ~mask;
    val <<= sc8315_reg_fields[field_id].lsb;
    tmp |= val  & mask;

    if (sc8315_reg_fields[field_id].force_write || tmp != reg_val) {
        ret = sc8315_i2c_write_byte(sc, sc8315_reg_fields[field_id].reg, tmp);
    }

out:
    if (ret < 0) {
        SC8315_ERR("sc8315 write field %d fail: %d\n", field_id, ret);
    }
    return ret;
}

/*********************CHIP API*********************/
__maybe_unused
static int sc8315_get_device_id(struct universal_pmic_data *sc)
{
    int *reg_val = (int *) &sc->chip_id;
    return sc8315_field_read(sc, F_DEVICE_ID, reg_val);
}

__maybe_unused
static int sc8315_set_config_reset(struct universal_pmic_data *sc, bool en)
{
    int reg_val = en ? true : false ;
    return sc8315_field_write(sc, F_RESET, reg_val);
}

__maybe_unused
static int sc8315_set_config_device_en(struct universal_pmic_data *sc, bool en)
{
    int reg_val = en ? true : false;
    return sc8315_field_write(sc, F_DEVICE_EN, reg_val);
}

__maybe_unused
static int sc8315_set_config_mode(struct universal_pmic_data *sc, int mode)
{
    int reg_val = mode & 0x03;
    return sc8315_field_write(sc, F_MODE_CFG, reg_val);
}

__maybe_unused
static int sc8315_set_config_vout_dischg(struct universal_pmic_data *sc, bool en)
{
    int reg_val = en ? true : false ;
    return sc8315_field_write(sc, F_VOUT_DISCHG, reg_val);
}

__maybe_unused
static int sc8315_set_config_ooa(struct universal_pmic_data *sc, bool en)
{
    int reg_val = en ? true : false ;
    return sc8315_field_write(sc, F_EN_OOA, reg_val);
}

__maybe_unused
static int sc8315_set_config_ssfm(struct universal_pmic_data *sc, bool en)
{
    int reg_val = en ? true : false ;
    return sc8315_field_write(sc, F_SSFM, reg_val);
}

__maybe_unused
static int sc8315_set_config_fpwm(struct universal_pmic_data *sc, bool val)
{
    int reg_val = val ? true : false ;
    return sc8315_field_write(sc, F_FPWM_CFG, reg_val);
}

__maybe_unused
static int sc8315_set_vout(struct universal_pmic_data *sc, uint32_t vol_mv)
{
    int val;

    if (vol_mv < 2850) {
        vol_mv = 2850;
    } else if (vol_mv > 5500) {
        vol_mv = 5500;
    }
    SC8315_ERR("%s:%dmV\n", __func__, vol_mv);
    val = (vol_mv - 2850) / 50;
    val &= 0x3f;
    return sc8315_field_write(sc, F_VOUT_REG, val);
}

__maybe_unused
static int sc8315_set_ilim_off(struct universal_pmic_data *sc, bool en)
{
    int reg_val = en ? false : true ;
    return sc8315_field_write(sc, F_ILIM_OFF, reg_val);
}

__maybe_unused
static int sc8315_set_ilim(struct universal_pmic_data *sc, uint32_t curr)
{
    int val;

    if (curr < 4000) {
        curr = 4000;
    } else if (curr > 8000) {
        curr = 8000;
    }
    SC8315_ERR("%s:%dmV\n", __func__, curr);
    val = (curr - 4000) / 500;
    val &= 0x0f;
    return sc8315_field_write(sc, F_ILIM, val);
}

__maybe_unused
static int sc8315_set_soft_start(struct universal_pmic_data *sc, bool en)
{
    int reg_val = en ? true : false ;
    return sc8315_field_write(sc, F_SOFT_START, reg_val);
}

__maybe_unused
static int sc8315_set_ilin1_set(struct universal_pmic_data *sc, uint32_t curr)
{
    int reg_val;

    if (curr <= 250) {
        reg_val = 0;
    } else if (curr <= 500) {
        reg_val = 1;
    } else if (curr <= 1000) {
        reg_val = 2;
    } else {
        reg_val = 3;
    }
    SC8315_ERR("%s:%dmV\n", __func__, curr);
    return sc8315_field_write(sc, F_ILIN1_SET, reg_val);
}

__maybe_unused
static int sc8315_set_cfg_dcdc_comp(struct universal_pmic_data *sc, uint8_t val)
{
    int reg_val = val & 0x03;
    return sc8315_field_write(sc, F_DCDC_COMP, reg_val);
}

__maybe_unused
static int sc8315_set_cfg_comphi(struct universal_pmic_data *sc, uint8_t val)
{
    int reg_val = val & 0x03;
    return sc8315_field_write(sc, F_N_COMPHI, reg_val);
}

__maybe_unused
static int sc8315_set_cfg_t_ocp(struct universal_pmic_data *sc, uint8_t val)
{
    int reg_val = !!val;
    return sc8315_field_write(sc, F_T_BPS_OCP, reg_val);
}

__maybe_unused
static int sc8315_set_cfg_t_ilim(struct universal_pmic_data *sc, uint8_t time_ms)
{
    int reg_val;
    if (time_ms <= 1) {
        reg_val = 0;
    } else if (time_ms <= 5) {
        reg_val = 1;
    } else if (time_ms <= 10) {
        reg_val = 2;
    } else {
        reg_val = 3;
    }
    return sc8315_field_write(sc, F_T_ILIM, reg_val);
}

__maybe_unused
static int sc8315_get_stat_pgood(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_PGOOD, val);
}

__maybe_unused
static int sc8315_get_stat_fault(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_FAULT, val);
}

__maybe_unused
static int sc8315_get_stat_bst_ocp(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_BST_OCP, val);
}

__maybe_unused
static int sc8315_get_stat_bps_ocp(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_BPS_OCP, val);
}

__maybe_unused
static int sc8315_get_stat_op_mode(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_OPMODE, val);
}

__maybe_unused
static int sc8315_get_stat_dcdc_mode(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_DCDCMODE, val);
}

__maybe_unused
static int sc8315_get_stat_hotide(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_HOTDE, val);
}

__maybe_unused
static int sc8315_get_stat_tsd(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_TSD, val);
}

__maybe_unused
static int sc8315_get_stat_comp_hi(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_COMP_HI, val);
}

__maybe_unused
static int sc8315_get_stat_vout_ovp(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_VOUT_OVP, val);
}

__maybe_unused
static int sc8315_get_stat_vin_ovp(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_VIN_OVP, val);
}

__maybe_unused
static int sc8315_get_stat_vin_ok(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_VIN_OK, val);
}

__maybe_unused
static int sc8315_get_stat_ss_flt(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_SS_FLT, val);
}

__maybe_unused
static int sc8315_get_stat_lin_flt(struct universal_pmic_data *sc, int *val)
{
    return sc8315_field_read(sc, F_LIN_FLT, val);
}

static int __hl7603_read_reg(struct universal_pmic_data *chip, u8 reg, u8 *data)
{
	s32 ret;

	
	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		msleep(500);
		ret = i2c_smbus_read_byte_data(chip->client, reg);
		if (ret < 0) {
			pr_info("i2c read fail: can't read from reg 0x%02X\n", reg);
			return ret;
		}
		*data = (u8)ret;

		return 0;
	}
	*data = (u8)ret;

	return 0;
}

static int __hl7603_write_reg(struct universal_pmic_data *chip, int reg, u8 val)
{
	s32 ret;
	pr_info("enter __hl7603_write_reg\n");
	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		pr_info("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			   val, reg, ret);
		return ret;
	}

	return 0;
}
static int hl7603_read_byte(struct universal_pmic_data *chip, u8 reg, u8 *data)
{
	int ret;
	pr_info("enter hl7603_read_byte\n");
	mutex_lock(&chip->i2c_rw_lock);
	ret = __hl7603_read_reg(chip, reg, data);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int hl7603_write_byte(struct universal_pmic_data *chip, u8 reg, u8 data)
{
	int ret;
	pr_info("enter hl7603_write_byte\n");
	mutex_lock(&chip->i2c_rw_lock);
	ret = __hl7603_write_reg(chip, reg, data);
	mutex_unlock(&chip->i2c_rw_lock);

	if (ret)
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int __maybe_unused hl7603_update_bits(struct universal_pmic_data *chip, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&chip->i2c_rw_lock);
	ret = __hl7603_read_reg(chip, reg, &tmp);
	if (ret) {
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __hl7603_write_reg(chip, reg, tmp);
	if (ret)
		pr_info("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&chip->i2c_rw_lock);
	return ret;
}


/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
int hl7603_read_interface(struct universal_pmic_data *chip, u8 RegNum,
				u8 *val, u8  MASK, u8 SHIFT)
{
	u8 hl7603_reg = 0;
	u8 ret = 0;

	ret = hl7603_read_byte(chip, RegNum, &hl7603_reg);
	hl7603_print("[hl7603_read_interface] Reg[%x]=0x%x\n", RegNum, hl7603_reg); 
	hl7603_reg &= (MASK << SHIFT);
	*val = (hl7603_reg >> SHIFT);
	hl7603_print("[hl7603_read_interface] val=0x%x\n", *val);  

	return ret;
}

int hl7603_config_interface(struct universal_pmic_data *chip, u8 RegNum,
				u8 val, u8 MASK, u8 SHIFT)
{
	u8 hl7603_reg = 0;
	int ret = 0;
	pr_info("enter hl7603_config_interface\n");
	ret = hl7603_read_byte(chip, RegNum, &hl7603_reg);
	
	hl7603_reg &= ~(MASK << SHIFT);
	hl7603_reg |= (val << SHIFT);

	ret = hl7603_write_byte(chip, RegNum, hl7603_reg);

	return ret;
}

u32 hl7603_get_reg_value(struct universal_pmic_data *chip, u32 reg)
{
	u32 ret = 0;
	u8 reg_val = 0;
	pr_info("enter hl7603_get_reg_value\n");
	ret = hl7603_read_interface(chip, (u8) reg, &reg_val, 0xFF, 0x0);

	if (ret == 0)
		hl7603_print("ret=%d\n", ret);
	return reg_val;
}


void hl7603_dump_register(struct universal_pmic_data *chip)
{
	u8 i = 0x0;
	u8 i_max = 0x5;

	for (i = 0x0; i <= i_max; i++) {
		hl7603_print("[0x%2x]=0x%2x ", i, hl7603_get_reg_value(chip, i));
	}
}


int hl7603_is_enabled(struct universal_pmic_data *chip)
{
	int ret = -1;
	unsigned char en;

	ret = hl7603_read_interface(chip, HL7603_CONFIG1, &en, 
                                   HL7603_DEV_EN_MASK, HL7603_DEV_EN_SHIFT);
	return (ret == 0) ? en : -EIO;
}


int hl7603_enable(struct universal_pmic_data *chip, unsigned char en)
{
	int ret = 1;
	//struct universal_pmic_data *chip ;

	ret = hl7603_config_interface(chip, HL7603_CONFIG1, en, 
                                     HL7603_DEV_EN_MASK, HL7603_DEV_EN_SHIFT);

	hl7603_print("[%s] en=%d, ret=%d\n", __func__, en, ret);

	return ret;
}

int hl7603_set_mode(struct universal_pmic_data *chip, unsigned char mode)
{
	int ret;

	if (mode != 0 && mode != 2 ) {
		hl7603_print("[%s] error mode = %d only 0 or 3\n", __func__, mode);
		return -1;
	}

	ret = hl7603_config_interface(chip, HL7603_CONFIG1, mode, HL7603_MODE_CFG_MASK,HL7603_MODE_CFG_SHIFT);

	return ret;
}

static int universal_pmic_read_byte(struct universal_pmic_data *chip, u8 reg, u8 *data)
{
    int ret;
    
    mutex_lock(&chip->i2c_rw_lock);
    ret = i2c_smbus_read_byte_data(chip->client, reg);
    mutex_unlock(&chip->i2c_rw_lock);
    
    if (ret < 0) {
        dev_err(chip->dev, "Failed to read reg 0x%02X: %d\n", reg, ret);
        return ret;
    }
    
    *data = (u8)ret;
    return 0;
}

static enum pmic_type detect_pmic_type(struct universal_pmic_data *chip)
{
    u8 chip_id;
    int ret;
    
    ret = universal_pmic_read_byte(chip, 0x00, &chip_id);
    if (ret < 0) {
        dev_err(chip->dev, "Failed to read chip ID\n");
        return PMIC_UNKNOWN;
    }
    
    chip->chip_id = chip_id;
    dev_err(chip->dev, "chip_id = 0x%02X\n", chip_id);
    
    switch (chip_id) {
    case HL7603_CHIP_ID:
        dev_err(chip->dev, "Detected HL7603 chip\n");
        return PMIC_HL7603;
        
    case HL7603A_CHIP_ID:
        dev_err(chip->dev, "Detected HL7603A chip\n");
        return PMIC_HL7603A;
        
    case SC8315_CHIP_ID:
        dev_err(chip->dev, "Detected SC8315 chip\n");
        return PMIC_SC8315;
        
    default:
        dev_warn(chip->dev, "Unknown chip ID: 0x%02X\n", chip_id);
        return PMIC_UNKNOWN;
    }
}

static int hl7603_reg_init(struct universal_pmic_data *chip)
{
	kal_uint32 ret = 0;
	pr_info("enter hl7603_reg_init\n");
	ret = hl7603_config_interface(chip, HL7603_VOUT_VSEL, chip->voltage_value, 
                                      HL7603_VOUT_REG_MASK, HL7603_VOUT_REG_SHIFT);
	if (ret)
		return -1;

	ret = hl7603_set_mode(chip, 2);
	if (ret)
		return -1;
	
	ret = hl7603_enable(chip, 1);
	if (ret)
		return -1;
        ret =  __hl7603_write_reg(chip, 0xa7, 0xf9);
        hl7603_print("set hl7603 0xa7 ret = %d\n", ret);
        ret = __hl7603_write_reg(chip, 0x22, 0x00);
        hl7603_print("set hl7603 0x22 ret = %d\n", ret);
	hl7603_print("%s_hw_init,chip_id=%d\n", chip->name,chip->chip_id);
  	return 0;
}

static int sc8315_reg_init(struct universal_pmic_data *sc)
{
    int ret = 0;
    int i;
    struct {
        enum sc8315_fields field_id;
        int conv_data;
    } props[] = {
        {F_DEVICE_EN, sc->params.sc8315.cfg_dev_en},
        {F_VOUT_REG, sc->params.sc8315.vout},
        {F_ILIN1_SET, sc->params.sc8315.ilin1_set},
        {F_ILIM, sc->params.sc8315.ilim},
    };
  
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        SC8315_ERR("%s (%d)\n", __func__, props[i].conv_data);
        ret = sc8315_field_write(sc, props[i].field_id, props[i].conv_data);
        if (ret < 0) {
            SC8315_ERR("set %d failed(%d)\n", props[i].field_id, ret);
            return ret;
        }
    }
    ret = sc8315_i2c_write_byte(sc, 0x82, 0x0d);
    SC8315_ERR("set 0x82 reg ret = %d\n", ret);
    return ret;
}

static ssize_t sc8315_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct universal_pmic_data *sc = dev_get_drvdata(dev);
    uint8_t addr, tmpbuf[300];
    int len, idx, ret, val;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8315");
    for (addr = 0x0; addr <= 0x06; addr++) {
        if(addr <= 0x06) {
            ret = regmap_read(sc->regmap, addr, &val);
            if (ret == 0) {
                len = snprintf(tmpbuf, PAGE_SIZE - idx,
                        "Reg[%.2X] = 0x%.2x\n", addr, val);
                memcpy(&buf[idx], tmpbuf, len);
                idx += len;
            }
        }
    }

    return idx;
}

static ssize_t sc8315_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct universal_pmic_data *sc = dev_get_drvdata(dev);
    int ret;
    int reg, val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if ((ret == 2 ) && (reg <= 0x06)) {
        regmap_write(sc->regmap, reg, val);
    }
    return count;
}

static DEVICE_ATTR(registers, 0660, sc8315_show_registers, sc8315_store_register);
static void sc8315_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}

static int hl7603_reg_reset(void *dev_data)
{
	int ret;
	u8 reg;
	struct universal_pmic_data *di = dev_data;

	hl7603_config_interface(di, HL7603_CONFIG1, HL7603_RESET_MASK,HL7603_VOUT_REG_SHIFT, 0x01);
	msleep(10); 
	ret = hl7603_read_byte(di, HL7603_CONFIG1, &reg);
	if (ret)
		return -EPERM;

	dev_info(di->dev, "reg_reset [%x]=0x%x\n", HL7603_CONFIG1, reg);
	return 0;
}

// 设备树兼容性表
static const struct of_device_id universal_pmic_of_match[] = {
    { .compatible = "halo,hl7603", },
    { .compatible = "halo,hl7603a", },
    { .compatible = "southchip,sc8315", },
    { .compatible = "universal,pmic", },
    { }
};
MODULE_DEVICE_TABLE(of, universal_pmic_of_match);

// I2C设备ID表
static const struct i2c_device_id universal_pmic_i2c_id[] = {
    { "hl7603", 0 },
    { "hl7603a", 0 },
    { "sc8315", 0 },
    { "universal_pmic", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, universal_pmic_i2c_id);

static int hl7603_parse_dts(struct universal_pmic_data *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret = 0;
	u32 voltage;
	pr_info("enter hl7603_parse_dts\n");
	ret = of_property_read_u32(np, "halo,hl7603,hl7603_vout_voltage",
				   &voltage);
	if (ret) {
		hl7603_print("failed to read bat-ovp-threshold=%d\n",ret);
		return ret;
	}

	chip->voltage_value = (u8)voltage;
	return 0;
}

static int hl7603a_parse_dts(struct universal_pmic_data *chip)
{
    struct device_node *np = chip->dev->of_node;
    u32 voltage;
    int ret;
    pr_info("enter hl7603a_parse_dts\n");
    ret = of_property_read_u32(np, "halo,hl7603a-vout-voltage", &voltage); // 读取到 u32
    if (ret) {
        dev_err(chip->dev, "Failed to read voltage: %d\n", ret);
        return ret;
    }

    if (voltage > 255) { // 确保值在 0-255 范围内
        dev_err(chip->dev, "电压值超限: %d\n", voltage);
        return -EINVAL;
    }

    chip->voltage_value = (u8)voltage; // 类型转换
    return 0;
}

static int sc8315_parse_dts(struct universal_pmic_data *sc)
{
    struct device_node *np = sc->dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,sc8315,device_en", &(sc->params.sc8315.cfg_dev_en)},
        {"sc,sc8315,vout", &(sc->params.sc8315.vout)},
        {"sc,sc8315,ilim", &(sc->params.sc8315.ilim)},
        {"sc,sc8315,ilin1_set", &(sc->params.sc8315.ilin1_set)},
    };

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name,
                        props[i].conv_data);
        if (ret < 0) {
            SC8315_ERR("can not read %s \n", props[i].name);
            return ret;
        }
    }

    return 0;
}


static int universal_pmic_reg_init(struct universal_pmic_data *chip)
{
    switch (chip->type) {
    case PMIC_HL7603:
    case PMIC_HL7603A:
        return hl7603_reg_init(chip);
        
    case PMIC_SC8315:
        return sc8315_reg_init(chip);
        
    default:
        return -ENODEV;
    }
}

static const struct regmap_config sc8315_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    // .max_register = SC8315_REGMAX,
};

static const struct i2c_device_id universal_pmic_id_table[] = {
    { "hl7603", 0 },
    { "hl7603a", 0 },
    { "sc8315", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, universal_pmic_id_table);

/**********************************************************
  *   主驱动函数
  *********************************************************/
static int universal_pmic_probe(struct i2c_client *client)
{
    struct universal_pmic_data *chip;
    int ret;
    
    dev_err(&client->dev, "Universal PMIC driver probing\n");
    
    const struct i2c_device_id *id = i2c_match_id(universal_pmic_id_table, client);
    if (!id) {
        dev_err(&client->dev, "No matching device ID found\n");
        return -ENODEV;
    }

    if (!client || !client->dev.of_node)
        return -ENODEV;
    
    chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip)
        return -ENOMEM;
    
    chip->client = client;
    chip->dev = &client->dev;
    mutex_init(&chip->i2c_rw_lock);
    
    i2c_set_clientdata(client, chip);
    
    // 检测芯片类型
    chip->type = detect_pmic_type(chip);
    if (chip->type == PMIC_UNKNOWN) {
        dev_err(chip->dev, "Unknown or unsupported PMIC chip\n");
        ret = -ENODEV;
        goto cleanup;
    }
    
    // 解析设备树参数
    switch (chip->type) {
    case PMIC_HL7603:
        ret = hl7603_parse_dts(chip);
        ret = hl7603_reg_reset(chip);
        break;
    case PMIC_HL7603A:
        ret = hl7603a_parse_dts(chip);
        ret = hl7603_reg_reset(chip);
        break;
    case PMIC_SC8315:
        chip->regmap = devm_regmap_init_i2c(client,
                            &sc8315_regmap_config);
        if (IS_ERR(chip->regmap)) {
            SC8315_ERR("Failed to initialize regmap\n");
            return -EINVAL;
        }
        sc8315_create_device_node(&(client->dev));
        ret = sc8315_parse_dts(chip);
        break;
    default:
        ret = -ENODEV;
        break;
    }
    
    if (ret)
        goto cleanup;
    
    // 初始化芯片
    ret = universal_pmic_reg_init(chip);
    if (ret)
        goto cleanup;
    ret = sc8315_i2c_write_byte(chip, 0x03, 0x03);
    SC8315_ERR("set 0x03 reg ret = %d\n", ret);    
    dev_err(chip->dev, "Universal PMIC driver probed successfully\n");
    return 0;
    
cleanup:
    mutex_destroy(&chip->i2c_rw_lock);
    devm_kfree(&client->dev, chip);
    return ret;
}

static void universal_pmic_remove(struct i2c_client *client)
{
    struct universal_pmic_data *chip = i2c_get_clientdata(client);
    
    switch (chip->type) {
    case PMIC_HL7603:
        hl7603_reg_reset(chip);
        break;
    case PMIC_HL7603A:
        hl7603_reg_reset(chip);
        break;
    case PMIC_SC8315:
        sc8315_set_config_reset(chip, true);
        break;
    default:
        break;
    }
    mutex_destroy(&chip->i2c_rw_lock);
}

static void universal_pmic_shutdown(struct i2c_client *client)
{
    // 关机时的清理操作
    struct universal_pmic_data *chip = i2c_get_clientdata(client);
    
    if (chip) {
        // 重置芯片
        switch (chip->type) {
    case PMIC_HL7603:
        hl7603_reg_reset(chip);
        break;
    case PMIC_HL7603A:
        hl7603_reg_reset(chip);
        break;
    case PMIC_SC8315:
        sc8315_set_config_reset(chip, true);
        break;
    default:
        break;
    }
    }
}

static struct i2c_driver universal_pmic_driver = {
    .driver = {
        .name = "universal_pmic",
        .owner = THIS_MODULE,
        .of_match_table = universal_pmic_of_match,
    },
    .probe = universal_pmic_probe,
    .remove = universal_pmic_remove,
    .shutdown = universal_pmic_shutdown,
    .id_table = universal_pmic_i2c_id,
};

module_i2c_driver(universal_pmic_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Universal PMIC driver for HL7603/HL7603A/SC8315");
MODULE_AUTHOR("Universal PMIC Driver Author");