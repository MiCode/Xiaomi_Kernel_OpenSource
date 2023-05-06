
/*
 * ln8000 battery charging driver
*/

#include "inc/ln8000.h"
#include "inc/ln8000_reg.h"
#include "inc/ln8000_iio.h"

/**
 * I2C control functions : when occurred I2C tranfer fault, we
 * will retry to it. (default count:3)
 */
#define I2C_RETRY_CNT   3
static int ln8000_read_reg(struct ln8000_info *info, u8 addr, u8 *data)
{
        int i, ret = 0;

        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_read_byte_data(info->client, addr);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_info("failed-read, reg(0x%02X), ret(%d)\n", addr, ret);
                } else {
                        *data = (u8)ret;
                }
        }
        mutex_unlock(&info->i2c_lock);
        return ret;
}

static int ln8000_bulk_read_reg(struct ln8000_info *info, u8 addr, void *data, int count)
{
        int i, ret = 0;
        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_read_i2c_block_data(info->client, addr, count, data);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_info("failed-bulk-read, reg(0x%02X, %d bytes), ret(%d)\n", addr, count, ret);
                } else {
                        break;
                }
        }
        mutex_unlock(&info->i2c_lock);
        return ret;
}

static int ln8000_write_reg(struct ln8000_info *info, u8 addr, u8 data)
{
        int i, ret = 0;

        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_write_byte_data(info->client, addr, data);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_info("failed-write, reg(0x%02X), ret(%d)\n", addr, ret);
                } else {
                        break;
                }
        }
        mutex_unlock(&info->i2c_lock);
        return ret;
}

static int ln8000_update_reg(struct ln8000_info *info, u8 addr, u8 mask, u8 data)
{
        int i, ret;
        u8 old_val, new_val;

        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_read_byte_data(info->client, addr);
                if (ret < 0) {
                        ln_err("failed-update, reg(0x%02X), ret(%d)\n", addr, ret);
                } else {
                        old_val = ret & 0xff;
                        new_val = (data & mask) | (old_val & ~(mask));
                        ret = i2c_smbus_write_byte_data(info->client, addr,
                                                        new_val);
                        if (ret < 0) {
                                ln_err("failed-update, reg(0x%02X), ret(%d)\n",
                                       addr, ret);
                        } else {
                                break;
                        }
                }
        }
        mutex_unlock(&info->i2c_lock);

        return ret;
}

/**
 * Register control functions
 */
int ln8000_set_vac_ovp(struct ln8000_info *info, unsigned int ovp_th)
{
        u8 cfg;

        if (ovp_th <= 6500000) {
                cfg = LN8000_VAC_OVP_6P5V;
        } else if (ovp_th <= 11000000) {
                cfg = LN8000_VAC_OVP_11V;
        } else if (ovp_th <= 12000000) {
                cfg = LN8000_VAC_OVP_12V;
        } else {
                cfg = LN8000_VAC_OVP_13V;
        }

        return ln8000_update_reg(info, LN8000_REG_GLITCH_CTRL, 0x3 << 2, cfg << 2);
}

/* battery float voltage */
int ln8000_set_vbat_float(struct ln8000_info *info, unsigned int cfg)
{
        u8 val;
        unsigned int adj_cfg = cfg - 30000;     /* adjust v_float bg offset (-30mV) */

        ln_info("ori_cfg=%d, adj_cfg=%d\n", cfg, adj_cfg);

        if (adj_cfg < LN8000_VBAT_FLOAT_MIN)
                val = 0x00;
        else if (adj_cfg > LN8000_VBAT_FLOAT_MAX)
                val = 0xFF;
        else
                val = (adj_cfg - LN8000_VBAT_FLOAT_MIN) / LN8000_VBAT_FLOAT_LSB;

        return ln8000_write_reg(info, LN8000_REG_V_FLOAT_CTRL, val);
}

int ln8000_set_iin_limit(struct ln8000_info *info, unsigned int cfg)
{
        u8 val = cfg / LN8000_IIN_CFG_LSB;

        ln_info("iin_limit=%dmV(iin_ctrl=0x%x)\n", cfg / 1000, val);

        return ln8000_update_reg(info, LN8000_REG_IIN_CTRL, 0x7F, val);
}

int ln8000_set_ntc_alarm(struct ln8000_info *info, unsigned int cfg)
{
        int ret;

        /* update lower bits */
        ret = ln8000_write_reg(info, LN8000_REG_NTC_CTRL, (cfg & 0xFF));
        if (ret < 0)
                return ret;

        /* update upper bits */
        ret = ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x3, (cfg >> 8));
        return ret;
}

/* battery voltage OV protection */
int ln8000_enable_vbat_ovp(struct ln8000_info *info, bool enable)
{
        u8 val;

        val = (enable) ? 0 : 1;//disable
        val <<= LN8000_BIT_DISABLE_VBAT_OV;

        return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL, BIT(LN8000_BIT_DISABLE_VBAT_OV), val);
}

int ln8000_enable_vbat_regulation(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_DISABLE_VFLOAT_LOOP,
                                !(enable) << LN8000_BIT_DISABLE_VFLOAT_LOOP);
}

int ln8000_enable_vbat_loop_int(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_ENABLE_VFLOAT_LOOP_INT,
                                enable << LN8000_BIT_ENABLE_VFLOAT_LOOP_INT);
}

/* input current OC protection */
int ln8000_enable_iin_ocp(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL,
                                0x1 << LN8000_BIT_DISABLE_IIN_OCP,
                                !(enable) << LN8000_BIT_DISABLE_IIN_OCP);
}

int ln8000_enable_iin_regulation(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_DISABLE_IIN_LOOP,
                                !(enable) << LN8000_BIT_DISABLE_IIN_LOOP);
}

int ln8000_enable_iin_loop_int(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_ENABLE_IIN_LOOP_INT,
                                enable << LN8000_BIT_ENABLE_IIN_LOOP_INT);
}

int ln8000_enable_tdie_prot(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_TEMP_MAX_EN, enable << LN8000_BIT_TEMP_MAX_EN);
}

int ln8000_enable_tdie_regulation(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_TEMP_REG_EN, enable << LN8000_BIT_TEMP_REG_EN);
}

/* ADC channel enable */
static int ln8000_set_adc_ch(struct ln8000_info *info, unsigned int ch, bool enable)
{
        u8 mask;
        u8 val;
        int ret;

        if ((ch > LN8000_ADC_CH_ALL) || (ch < 1))
                return -EINVAL;

        if (ch == LN8000_ADC_CH_ALL) {
                // update all channels
                val  = (enable) ? 0x3E : 0x00;
                ret  = ln8000_write_reg(info, LN8000_REG_ADC_CFG, val);
        } else {
                // update selected channel
                mask = 1<<(ch-1);
                val  = (enable) ? 1 : 0;
                val <<= (ch-1);
                ret  = ln8000_update_reg(info, LN8000_REG_ADC_CFG, mask, val);
        }

        return ret;
}

/* BUS temperature monitoring (protection+alarm) */
int ln8000_enable_tbus_monitor(struct ln8000_info *info, bool enable)
{
        int ret;

        /* enable BUS monitoring */
        ret = ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 1, enable << 1);
        if (ret < 0)
                return ret;

        /* enable BUS ADC channel */
        if (enable) {
                ret = ln8000_set_adc_ch(info, LN8000_ADC_CH_TSBUS, true);
        }
        return ret;
}

/* BAT temperature monitoring (protection+alarm) */
int ln8000_enable_tbat_monitor(struct ln8000_info *info, bool enable)
{
        int ret;

        /* enable BAT monitoring */
        ret = ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 0, enable << 0);
        if (ret < 0)
                return ret;

        /* enable BAT ADC channel */
        if (enable) {
                ret = ln8000_set_adc_ch(info, LN8000_ADC_CH_TSBAT, true);
        }
        return ret;
}

/* watchdog timer */
int ln8000_enable_wdt(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 7, enable << 7);
}

#if 0
static int ln8000_set_wdt(struct ln8000_info *info, unsigned int cfg)
{
        if (cfg >= LN8000_WATCHDOG_MAX) {
                cfg = LN8000_WATCHDOG_40SEC;
        }

        return ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x3 << 5, cfg << 5);
}
#endif

/* unplug / reverse-current detection */
int ln8000_enable_rcp(struct ln8000_info *info, bool enable)
{
        info->rcp_en = enable;

        return ln8000_update_reg(info, LN8000_REG_SYS_CTRL,
                                BIT(LN8000_BIT_REV_IIN_DET),
                                enable << LN8000_BIT_REV_IIN_DET);
}

/* auto-recovery */
int ln8000_enable_auto_recovery(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0xF << 4, ((0xF << 4) * enable));
}

int ln8000_enable_rcp_auto_recovery(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 6, enable << 6);
}

int ln8000_set_adc_mode(struct ln8000_info *info, unsigned int cfg)
{
        return ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x7 << 5, cfg << 5);
}

int ln8000_set_adc_hib_delay(struct ln8000_info *info, unsigned int cfg)
{
        return ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x3 << 3, cfg << 3);
}

/* enable/disable STANDBY */
static inline void ln8000_sw_standby(struct ln8000_info *info, bool standby)
{
        u8 val = (standby) ? BIT(LN8000_BIT_STANDBY_EN) : 0x00;
        ln8000_update_reg(info, LN8000_REG_SYS_CTRL, BIT(LN8000_BIT_STANDBY_EN), val);
}

/* Convert Raw ADC Code */
static void ln8000_convert_adc_code(struct ln8000_info *info, unsigned int ch, u8 *sts, int *result)
{
        int adc_raw;	// raw ADC value
        int adc_final;	// final (converted) ADC value

        switch (ch) {
        case LN8000_ADC_CH_VOUT:
                adc_raw   = ((sts[1] & 0xFF)<<2) | ((sts[0] & 0xC0)>>6);
                adc_final = adc_raw * LN8000_ADC_VOUT_STEP;//uV
                break;
        case LN8000_ADC_CH_VIN:
                adc_raw   = ((sts[1] & 0x3F)<<4) | ((sts[0] & 0xF0)>>4);
                adc_final = adc_raw * LN8000_ADC_VIN_STEP;//uV
                break;
        case LN8000_ADC_CH_VBAT:
                adc_raw   = ((sts[1] & 0x03)<<8) | (sts[0] & 0xFF);
                adc_final = adc_raw * LN8000_ADC_VBAT_STEP;//uV
                break;
        case LN8000_ADC_CH_VAC:
                adc_raw   = (((sts[1] & 0x0F)<<6) | ((sts[0] & 0xFC)>>2)) + LN8000_ADC_VAC_OS;
                adc_final = adc_raw * LN8000_ADC_VAC_STEP;//uV
                break;
        case LN8000_ADC_CH_IIN:
                adc_raw   = ((sts[1] & 0x03)<<8) | (sts[0] & 0xFF);
                adc_final = adc_raw * LN8000_ADC_IIN_STEP;//uA
                break;
        case LN8000_ADC_CH_DIETEMP:
                adc_raw   = ((sts[1] & 0x0F)<<6) | ((sts[0] & 0xFC)>>2);
                adc_final = (935 - adc_raw) * LN8000_ADC_DIETEMP_STEP / LN8000_ADC_DIETEMP_DENOM;//dC
                if (adc_final > LN8000_ADC_DIETEMP_MAX)
                        adc_final = LN8000_ADC_DIETEMP_MAX;
                else if (adc_final < LN8000_ADC_DIETEMP_MIN)
                        adc_final = LN8000_ADC_DIETEMP_MIN;
                break;
        case LN8000_ADC_CH_TSBAT:
                adc_raw   = ((sts[1] & 0x3F)<<4) | ((sts[0] & 0xF0)>>4);
                adc_final = adc_raw * LN8000_ADC_NTCV_STEP;//(NTC) uV
                break;
        case LN8000_ADC_CH_TSBUS:
                adc_raw   = ((sts[1] & 0xFF)<<2) | ((sts[0] & 0xC0)>>6);
                adc_final = adc_raw * LN8000_ADC_NTCV_STEP;//(NTC) uV
                break;
        default:
                adc_raw   = -EINVAL;
                adc_final = -EINVAL;
                break;
        }

        *result = adc_final;
        return;
}

static void ln8000_print_regmap(struct ln8000_info *info)
{
        const u8 print_reg_num = (LN8000_REG_CHARGE_CTRL - LN8000_REG_INT1_MSK) + 1;
        u8 regs[64] = {0x0, };
        char temp_buf[128] = {0,};
        int i, ret;

        for (i = 0; i < print_reg_num; ++i) {
                ret = ln8000_read_reg(info, LN8000_REG_INT1_MSK + i, &regs[i]);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_err("fail to read reg for print_regmap[%d]\n", i);
                        regs[i] = 0xFF;
                }
                sprintf(temp_buf + strlen(temp_buf), "0x%02X[0x%02X],", LN8000_REG_INT1_MSK + i, regs[i]);
                if (((i+1) % 10 == 0) || ((i+1) == print_reg_num)) {
                        ln_err("%s\n", temp_buf);
                        memset(temp_buf, 0x0, sizeof(temp_buf));
                }
        }
}

/**
 * LN8000 device driver control routines
 */
int ln8000_check_status(struct ln8000_info *info)
{
        u8 val[4];

        if (ln8000_bulk_read_reg(info, LN8000_REG_SYS_STS, val, 4) < 0) {
                return -EINVAL;
        }

        mutex_lock(&info->data_lock);

        info->vbat_regulated  = LN8000_STATUS(val[0], LN8000_MASK_VFLOAT_LOOP_STS);
        info->iin_regulated   = LN8000_STATUS(val[0], LN8000_MASK_IIN_LOOP_STS);
        info->pwr_status      = val[0] & (LN8000_MASK_BYPASS_ENABLED | LN8000_MASK_SWITCHING_ENABLED | \
                                      LN8000_MASK_STANDBY_STS | LN8000_MASK_SHUTDOWN_STS);
        info->tdie_fault      = LN8000_STATUS(val[1], LN8000_MASK_TEMP_MAX_STS);
        info->tdie_alarm      = LN8000_STATUS(val[1], LN8000_MASK_TEMP_REGULATION_STS);
        if (!info->pdata->tbat_mon_disable || !info->pdata->tbus_mon_disable) {
                info->tbus_tbat_fault = LN8000_STATUS(val[1], LN8000_MASK_NTC_SHUTDOWN_STS); //tbus or tbat
                info->tbus_tbat_alarm = LN8000_STATUS(val[1], LN8000_MASK_NTC_ALARM_STS);//tbus or tbat
        }
        info->iin_rc          = LN8000_STATUS(val[1], LN8000_MASK_REV_IIN_STS);

        info->wdt_fault  = LN8000_STATUS(val[2], LN8000_MASK_WATCHDOG_TIMER_STS);
        info->vbat_ov    = LN8000_STATUS(val[2], LN8000_MASK_VBAT_OV_STS);
        info->vac_unplug = LN8000_STATUS(val[2], LN8000_MASK_VAC_UNPLUG_STS);
        info->vac_ov     = LN8000_STATUS(val[2], LN8000_MASK_VAC_OV_STS);
        info->vbus_ov    = LN8000_STATUS(val[2], LN8000_MASK_VIN_OV_STS);
        info->volt_qual  = !(LN8000_STATUS(val[2], 0x7F));
        if (info->volt_qual == 1 && info->chg_en == 1) {
                info->volt_qual = !(LN8000_STATUS(val[3], 1 << 5));
                if (info->volt_qual == 0) {
                        ln_info("volt_fault_detected (volt_qual=%d\n", info->volt_qual);
                }
        }
        info->iin_oc     = LN8000_STATUS(val[3], LN8000_MASK_IIN_OC_DETECTED);

        mutex_unlock(&info->data_lock);

        return 0;
}

static void ln8000_irq_sleep(struct ln8000_info *info, int suspend)
{
        if (info->client->irq <= 0)
                return;

        if (suspend) {
                ln_info("disable/suspend IRQ\n");
                disable_irq(info->client->irq);
        } else {
                ln_info("enable/resume IRQ\n");
                enable_irq(info->client->irq);
        }
}

void ln8000_soft_reset(struct ln8000_info *info)
{
        ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0xC6);

        ln8000_irq_sleep(info, 1);

        ln_info("Trigger soft-reset\n");
        ln8000_update_reg(info, LN8000_REG_BC_OP_2, 0x1 << 0, 0x1 << 0);
        msleep(5 * 2);  /* ln8000 min wait time 5ms (after POR) */

        ln8000_irq_sleep(info, 0);
}

void ln8000_update_opmode(struct ln8000_info *info)
{
        unsigned int op_mode;
        u8 val;

        /* chack mode status */
        ln8000_read_reg(info, LN8000_REG_SYS_STS, &val);
        if (val == 0x0) {
                /* wait for translate state. */
                msleep(5);
                ln8000_read_reg(info, LN8000_REG_SYS_STS, &val);
        }

        if (val & LN8000_MASK_SHUTDOWN_STS) {
                op_mode = LN8000_OPMODE_STANDBY;
        } else if (val & LN8000_MASK_STANDBY_STS) {
                op_mode = LN8000_OPMODE_STANDBY;
        } else if (val & LN8000_MASK_SWITCHING_ENABLED) {
                op_mode = LN8000_OPMODE_SWITCHING;
        } else if (val & LN8000_MASK_BYPASS_ENABLED) {
                op_mode = LN8000_OPMODE_BYPASS;
        } else {
                op_mode = LN8000_OPMODE_UNKNOWN;
        }

        if (op_mode != info->op_mode) {
                /* IC already has been entered standby_mode, need to trigger standbt_en bit */
                if (op_mode == LN8000_OPMODE_STANDBY) {
                        ln8000_update_reg(info, LN8000_REG_SYS_CTRL, 1 << LN8000_BIT_STANDBY_EN, 1 << LN8000_BIT_STANDBY_EN);
                        ln_info("forced trigger standby_en\n");
                }
                ln_info("op_mode has been changed [%d]->[%d] (sys_st=0x%x)\n", info->op_mode, op_mode, val);
                info->op_mode = op_mode;
        }

        return;
}

int ln8000_change_opmode(struct ln8000_info *info, unsigned int target_mode)
{
        int ret = 0;
        u8 val, msk = (0x1 << LN8000_BIT_STANDBY_EN | 0x1 << LN8000_BIT_SOFT_START_EN | 0x1 << LN8000_BIT_EN_1TO1);

        /* clear latched status */
        ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 2, 0x1 << 2);
        ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 2, 0x0 << 2);

        switch(target_mode) {
        case LN8000_OPMODE_STANDBY:
                val = (1 << LN8000_BIT_STANDBY_EN);
                break;
        case LN8000_OPMODE_BYPASS:
                val = (0 << LN8000_BIT_STANDBY_EN) | (0 << LN8000_BIT_SOFT_START_EN) | (1 << LN8000_BIT_EN_1TO1);
                break;
        case LN8000_OPMODE_SWITCHING:
                val = (0 << LN8000_BIT_STANDBY_EN) | (0 << LN8000_BIT_SOFT_START_EN) | (0 << LN8000_BIT_EN_1TO1);
                break;
        default:
                ln_err("invalid index (target_mode=%d)\n", target_mode);
                return -EINVAL;
        }
        ret = ln8000_update_reg(info, LN8000_REG_SYS_CTRL, msk, val);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                return -EINVAL;
        }
        ln_info("changed opmode [%d] -> [%d]\n", info->op_mode, target_mode);
        info->op_mode = target_mode;

        return 0;
}

static int ln8000_init_device(struct ln8000_info *info)
{
        unsigned int vbat_float;

        /* config default charging paramter by dt */
        vbat_float = info->pdata->bat_ovp_th * 100 / 102;   /* ovp thershold = v_float x 1.02 */
        vbat_float = (vbat_float /1000) * 1000;
        ln_info("bat_ovp_th=%d, vbat_float=%d\n", info->pdata->bat_ovp_th, vbat_float);
        ln8000_set_vbat_float(info, vbat_float);
        info->vbat_ovp_alarm_th = info->pdata->bat_ovp_alarm_th;
        ln8000_set_vac_ovp(info, info->pdata->bus_ovp_th);
        info->vin_ovp_alarm_th = info->pdata->bus_ovp_alarm_th;
        ln8000_set_iin_limit(info, info->pdata->bus_ocp_th - 700000);
        info->iin_ocp_alarm_th = info->pdata->bus_ocp_alarm_th;
        ln8000_set_ntc_alarm(info, info->pdata->ntc_alarm_cfg);

        /* disable VBAT_REG, IIN_REG. DON'T change this, we can't support regulation mode */
        ln8000_write_reg(info, LN8000_REG_REGULATION_CTRL, 0x30);
        ln8000_enable_tdie_regulation(info, 0);
        ln8000_enable_auto_recovery(info, 0);

        /* config charging protection */
        ln8000_enable_vbat_ovp(info, !info->pdata->vbat_ovp_disable);
        ln8000_enable_iin_ocp(info, !info->pdata->iin_ocp_disable);
        ln8000_enable_tdie_prot(info, info->pdata->tdie_prot_disable);
        ln8000_enable_rcp(info, 1);
        ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);

        /* wdt : disable, adc : shutdown mode */
        ln8000_enable_wdt(info, false);
        ln8000_set_adc_mode(info, ADC_SHUTDOWN_MODE);//disable before updating
        ln8000_set_adc_hib_delay(info, ADC_HIBERNATE_4S);
        ln8000_set_adc_ch(info, LN8000_ADC_CH_ALL, true);
        ln8000_enable_tbus_monitor(info, !info->pdata->tbus_mon_disable);//+enables ADC ch
        ln8000_enable_tbat_monitor(info, !info->pdata->tbat_mon_disable);//+enables ADC ch
        ln8000_set_adc_mode(info, ADC_AUTO_HIB_MODE);

        /* mark sw initialized (used CHARGE_CTRL bit:7) */
        ln8000_update_reg(info, LN8000_REG_CHARGE_CTRL, 0x1 << 7, 0x1 << 7);
        ln8000_write_reg(info, LN8000_REG_THRESHOLD_CTRL, 0x0E);
        /* restore regval for prevent EOS attack */
        ln8000_read_reg(info, LN8000_REG_REGULATION_CTRL, &info->regulation_ctrl);
        ln8000_read_reg(info, LN8000_REG_ADC_CTRL, &info->adc_ctrl);
        ln8000_read_reg(info, LN8000_REG_V_FLOAT_CTRL, &info->v_float_ctrl);
        ln8000_read_reg(info, LN8000_REG_CHARGE_CTRL, &info->charge_ctrl);

        ln8000_print_regmap(info);

        ln_info(" done.\n");

        return 0;
}

int ln8000_get_adc_data(struct ln8000_info *info, unsigned int ch, int *result)
{
        int ret;
        u8 sts[2];

        /* pause adc update */
        ret  = ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 1, 0x1 << 1);
        if (ret < 0) {
                ln_err("fail to update bit PAUSE_ADC_UPDATE:1 (ret=%d)\n", ret);
                return ret;
        }

        switch (ch) {
        case LN8000_ADC_CH_VOUT:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC04_STS, sts, 2);
                break;
        case LN8000_ADC_CH_VIN:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC03_STS, sts, 2);
                break;
        case LN8000_ADC_CH_VBAT:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC06_STS, sts, 2);
                break;
        case LN8000_ADC_CH_VAC:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC02_STS, sts, 2);
                break;
        case LN8000_ADC_CH_IIN:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC01_STS, sts, 2);
                break;
        case LN8000_ADC_CH_DIETEMP:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC07_STS, sts, 2);
                break;
        case LN8000_ADC_CH_TSBAT:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC08_STS, sts, 2);
                break;
        case LN8000_ADC_CH_TSBUS:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC09_STS, sts, 2);
                break;
        default:
                ln_err("invalid ch(%d)\n", ch);
                ret = -EINVAL;
                break;
        }

        /* resume adc update */
        ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 1, 0x0 << 1);

        if (IS_ERR_VALUE((unsigned long)ret) == false) {
                ln8000_convert_adc_code(info, ch, sts, result);
        }

        return ret;
}

int psy_chg_get_charging_enabled(struct ln8000_info *info)
{
        int enabled = 0;    /* disabled */

        if (info->chg_en) {
                ln8000_update_opmode(info);
                if (info->op_mode >= LN8000_OPMODE_BYPASS) {
                        enabled = 1;
                }
        }
        ln_err("%s: chg_en:%d, op_mode:%d\n", __func__, info->chg_en, info->op_mode);

        return enabled;
}

static int ln8000_check_regmap_data(struct ln8000_info *info)
{
        u8 regulation_ctrl;
        u8 adc_ctrl;
        u8 v_float_ctrl;
        u8 charge_ctrl;

        ln8000_read_reg(info, LN8000_REG_REGULATION_CTRL, &regulation_ctrl);
        ln8000_read_reg(info, LN8000_REG_ADC_CTRL, &adc_ctrl);
        ln8000_read_reg(info, LN8000_REG_V_FLOAT_CTRL, &v_float_ctrl);
        ln8000_read_reg(info, LN8000_REG_CHARGE_CTRL, &charge_ctrl);

        if ((info->regulation_ctrl != regulation_ctrl) ||
            (info->adc_ctrl != adc_ctrl) ||
            (info->charge_ctrl != charge_ctrl) ||
            (info->v_float_ctrl != v_float_ctrl)) {
                /* Decide register map was reset. caused by EOS */
                ln_err("decided register map RESET, re-initialize device\n");
                ln_err("regulation_ctrl = 0x%x : 0x%x\n", info->regulation_ctrl, regulation_ctrl);
                ln_err("adc_ctrl        = 0x%x : 0x%x\n", info->adc_ctrl, adc_ctrl);
                ln_err("charge_ctrl     = 0x%x : 0x%x\n", info->charge_ctrl, charge_ctrl);
                ln_err("vbat_float      = 0x%x : 0x%x\n", info->v_float_ctrl, v_float_ctrl);
                ln8000_init_device(info);
                msleep(300);
        }

        return 0;
}

int psy_chg_get_ti_alarm_status(struct ln8000_info *info)
{
        int alarm;
        unsigned int v_offset;
        bool bus_ovp, bus_ocp, bat_ovp;
        u8 val[4];

        /* valid check the regmap data for recovery EOS attack */
        ln8000_check_regmap_data(info);
        ln8000_check_status(info);
        ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
        ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
        ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);

        bus_ovp = (info->vbus_uV > info->vin_ovp_alarm_th) ? 1 : 0;
        bus_ocp = (info->iin_uA > info->iin_ocp_alarm_th) ? 1 : 0;
        bat_ovp = (info->vbat_uV > info->vbat_ovp_alarm_th) ? 1 : 0;

        /* BAT alarm status not support (ovp/ocp/ucp) */
        alarm = ((bus_ovp << BUS_OVP_ALARM_SHIFT) |
                (bus_ocp << BUS_OCP_ALARM_SHIFT) |
                (bat_ovp << BAT_OVP_ALARM_SHIFT) |
                (info->tbus_tbat_alarm << BAT_THERM_ALARM_SHIFT) |
                (info->tbus_tbat_alarm << BUS_THERM_ALARM_SHIFT) |
                (info->tdie_alarm << DIE_THERM_ALARM_SHIFT));

        if (info->vbus_uV < (info->vbat_uV * 2)) {
                v_offset = 0;
        } else {
                v_offset = info->vbus_uV - (info->vbat_uV * 2);
        }

#if defined(LN8000_RCP_PATCH)
        /* after charging-enabled, When the input current rises above 400mA, it activates rcp. */
        if (info->chg_en && !(info->rcp_en)) {
                if (info->iin_uA > 400000) {
                        ln8000_enable_rcp(info, 1);
                        ln_info("enabled rcp\n");
                }
        }

        /* If an unplug event occurs when vbus voltage lower then iin(70mA) and v_offset(100mV), switch to standby mode. */
        if (info->chg_en && !(info->rcp_en)) {
                if (info->iin_uA < 70000 && v_offset < 100000) {
                        ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
                        ln_info("forced change standby_mode for prevent reverse current\n");
                        ln8000_enable_rcp(info, 1);
                        info->chg_en = 0;
                }
        }
#endif

        ln8000_bulk_read_reg(info, LN8000_REG_SYS_STS, val, 4);
        ln_info("st:0x%x:0x%x:0x%x:0x%x alarm=0x%x, adc_vin=%d, adc_iin=%d, adc_vbat=%d, v_offset=%d\n",
                val[0], val[1], val[2], val[3], alarm, info->vbus_uV/1000, info->iin_uA/1000,
                info->vbat_uV/1000, v_offset/1000);

        return alarm;
}

int psy_chg_get_it_bus_error_status(struct ln8000_info *info)
{
        ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);

        if (info->vbus_uV < 6000000) {
                return VBUS_ERROR_LOW;
        } else if (info->vbus_uV > 13000000) {
                return VBUS_ERROR_HIGHT;
        }
        return VBUS_ERROR_NONE;
}

int psy_chg_get_ti_fault_status(struct ln8000_info *info)
{
        int fault;

        ln8000_check_status(info);

        /* BAT ocp fault status not suppport */
        fault = ((info->vbat_ov << BAT_OVP_FAULT_SHIFT) |
                (info->vbus_ov << BUS_OVP_FAULT_SHIFT) |
                (info->iin_oc << BUS_OCP_FAULT_SHIFT) |
                (info->tbus_tbat_fault << BAT_THERM_FAULT_SHIFT) |
                (info->tbus_tbat_fault << BUS_THERM_FAULT_SHIFT) |
                (info->tdie_fault << DIE_THERM_FAULT_SHIFT));

        if (info->chg_en && info->volt_qual == 0) {
                fault  |= ((1 << BAT_OVP_FAULT_SHIFT) |
                (1 << BUS_OVP_FAULT_SHIFT));
        }

        if (fault) {
                ln_info("fault=0x%x\n", fault);
        }

        return fault;
}

int psy_chg_set_charging_enable(struct ln8000_info *info, int val)
{
        int op_mode;

        /* ignore duplicate request command*/
        ln_info("info->chg_en=%d,val=%d\n",info->chg_en,val);
        if (info->chg_en == val)
                return 0;

        if (val) {
                /* valid check the regmap data for recovery EOS attack */
                ln8000_check_regmap_data(info);
                ln_info("start charging\n");
                op_mode = LN8000_OPMODE_SWITCHING;
#if defined(LN8000_RCP_PATCH)
                /* when the start-up to charging, we need to disabled rcp. */
                ln8000_enable_rcp(info, 0);
#endif
        } else {
                ln_info("stop charging\n");
                op_mode = LN8000_OPMODE_STANDBY;
#if defined(LN8000_RCP_PATCH)
                ln8000_enable_rcp(info, 1);
#endif
        }

        ln8000_change_opmode(info, op_mode);
        msleep(10);
        ln8000_update_opmode(info);

        info->chg_en = val;
        if (val) {
                ln8000_print_regmap(info);
        }

        ln_info("op_mode=%d\n", info->op_mode);

        return 0;
}

int psy_chg_set_present(struct ln8000_info *info, int val)
{
        bool usb_present = (bool)val;

      //  if (usb_present != info->usb_present) {
                ln_info("changed usb_present [%d] -> [%d]\n", info->usb_present, usb_present);
                if (usb_present) {
                        ln8000_init_device(info);
                }
                info->usb_present = usb_present;
 //       }

        return 0;
}

#if 0
static int psy_chg_set_bus_protection_for_qc3(struct ln8000_info *info, int hvdcp3_type)
{
        ln_info("hvdcp3_type: %d\n", hvdcp3_type);

        if (hvdcp3_type == HVDCP3_CLASSA_18W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC_CLASS_A - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC_CLASS_A;
        } else if (hvdcp3_type == HVDCP3_CLASSB_27W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC_CLASS_B - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC_CLASS_B;
        } else if (hvdcp3_type == HVDCP3_P_CLASSA_18W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC35);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC35;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC35_CLASS_A_P - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC35_CLASS_A_P;
        } else if (hvdcp3_type == HVDCP3_P_CLASSB_27W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC35);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC35;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC35_CLASS_A_P - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC35_CLASS_A_P;
        } else {
                ln8000_set_vac_ovp(info, info->pdata->bus_ovp_th);
                info->vin_ovp_alarm_th = info->pdata->bus_ovp_alarm_th;
                ln8000_set_iin_limit(info, info->pdata->bus_ocp_th - 700000);
                info->iin_ocp_alarm_th = info->pdata->bus_ocp_alarm_th;
        }

        ln8000_print_regmap(info);

        return 0;
}
#endif

static int read_reg(void *data, u64 *val)
{
        struct ln8000_info *info = data;
        int ret;
        u8 temp;

        ret = ln8000_read_reg(info, info->debug_address, &temp);
        if (ret) {
                ln_err("Unable to read reg(0x%02X), ret=%d\n", info->debug_address, ret);
                return -EAGAIN;
        }
        *val = (u64)temp;
        return 0;
}

static int write_reg(void *data, u64 val)
{
        struct ln8000_info *info = data;
        int ret;
        u8 temp = (u8) val;

        ret = ln8000_write_reg(info, info->debug_address, temp);
        if (ret) {
                ln_err("Unable to write reg(0x%02X), data(0x%02X), ret=%d\n",
                info->debug_address, temp, ret);
                return -EAGAIN;
        }
        return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, read_reg, write_reg, "0x%02llX\n");

static int ln8000_create_debugfs_entries(struct ln8000_info *info)
{
        struct dentry *ent;

        info->debug_root = debugfs_create_dir(ln8000_dev_name[info->dev_role], NULL);
        if (!info->debug_root) {
                ln_err("unable to create debug dir\n");
                return -ENOENT;
        } else {
                debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO, info->debug_root, &(info->debug_address));
                ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO, info->debug_root, info, &register_debug_ops);
                if (!ent) {
                        ln_err("unable to create data debug file\n");
                        return -ENOENT;
                }
        }

        return 0;
}


static void ln8000_dump_regs_workfunc(struct work_struct *work)
{
	struct ln8000_info *info = container_of(work, struct ln8000_info, dump_regs_work.work);

	ln_err("%s: start dump_regs_work\n", __func__);
	ln8000_print_regmap(info);
	schedule_delayed_work(&info->dump_regs_work, msecs_to_jiffies(300000));
}

static irqreturn_t ln8000_interrupt_handler(int irq, void *data)
{
        return IRQ_HANDLED;
}

static int ln8000_irq_init(struct ln8000_info *info)
{
        return 0;
}

static void determine_initial_status(struct ln8000_info *info)
{
    if (info->client->irq)
            ln8000_interrupt_handler(info->client->irq, info);
}

static const struct of_device_id ln8000_dt_match[] = {
    {
        .compatible = "lionsemi,ln8000",
        .data = &ln8000_mode_data[LN8000_STDALONE],
    },
    {
        .compatible = "lionsemi,ln8000-master",
        .data = &ln8000_mode_data[LN8000_MASTER],
    },
    {
        .compatible = "lionsemi,ln8000-slave",
        .data = &ln8000_mode_data[LN8000_SLAVE],
    },
    { },
};
MODULE_DEVICE_TABLE(of, ln8000_dt_match);

static const struct i2c_device_id ln8000_id[] = {
    { "ln8000-standalone", LN_ROLE_STANDALONE },
    { "ln8000-master", LN_ROLE_MASTER },
    { "ln8000-slave", LN_ROLE_SLAVE },
    { }
};
MODULE_DEVICE_TABLE(i2c, ln8000_id);

static int ln8000_parse_dt(struct ln8000_info *info)
{
        struct device *dev = info->dev;
        struct ln8000_platform_data *pdata = info->pdata;
        struct device_node *np = dev->of_node;
        u32 prop;
        int ret;

        if (np == NULL)
                return -EINVAL;

        /* device configuration */
        ret = of_property_read_u32(np, "ln8000_charger,bat-ovp-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bat_ovp_th, (prop*1000/*uV*/), LN8000_BAT_OVP_DEFAULT);
        ret = of_property_read_u32(np, "ln8000_charger,bat-ovp-alarm-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bat_ovp_alarm_th, (prop*1000/*uV*/), 0);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ovp-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ovp_th, (prop*1000/*uV*/), LN8000_BUS_OVP_DEFAULT);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ovp-alarm-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ovp_alarm_th, (prop*1000/*uA*/), 0);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ocp-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ocp_th, (prop*1000/*uA*/), LN8000_BUS_OCP_DEFAULT);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ocp-alarm-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ocp_alarm_th, (prop*1000/*uA*/), 0);
        ret = of_property_read_u32(np, "ln8000_charger,ntc-alarm-cfg", &prop);
        LN8000_PARSE_PROP(ret, pdata, ntc_alarm_cfg, prop, LN8000_NTC_ALARM_CFG_DEFAULT);

        /* protection/alarm disable (defaults to enable) */
        pdata->vbat_ovp_disable     = of_property_read_bool(np, "ln8000_charger,vbat-ovp-disable");
        pdata->vbat_reg_disable     = of_property_read_bool(np, "ln8000_charger,vbat-reg-disable");
        pdata->iin_ocp_disable      = of_property_read_bool(np, "ln8000_charger,iin-ocp-disable");
        pdata->iin_reg_disable      = of_property_read_bool(np, "ln8000_charger,iin-reg-disable");
        pdata->tbus_mon_disable     = of_property_read_bool(np, "ln8000_charger,tbus-mon-disable");
        pdata->tbat_mon_disable     = of_property_read_bool(np, "ln8000_charger,tbat-mon-disable");
        pdata->tdie_prot_disable    = of_property_read_bool(np, "ln8000_charger,tdie-prot-disable");
        pdata->tdie_reg_disable     = of_property_read_bool(np, "ln8000_charger,tdie-reg-disable");
        pdata->revcurr_prot_disable = of_property_read_bool(np, "ln8000_charger,revcurr-prot-disable");

        /* override device tree */
/*
        if (info->dev_role == LN_ROLE_MASTER) {
                ln_info("disable TS_BAT monitor for primary device on dual-mode\n");
                pdata->tbat_mon_disable = true;
        } else if (info->dev_role == LN_ROLE_SLAVE) {
                ln_info("disable VBAT_OVP and TS_BAT for secondary device on dual-mode\n");
                pdata->vbat_ovp_disable   = true;
                pdata->tbat_mon_disable   = true;
        }
*/
        ln_info("vbat_ovp_disable = %d\n", pdata->vbat_ovp_disable);
        ln_info("vbat_reg_disable = %d\n", pdata->vbat_reg_disable);
        ln_info("iin_ocp_disable = %d\n", pdata->iin_ocp_disable);
        ln_info("iin_reg_disable = %d\n", pdata->iin_reg_disable);
        ln_info("tbus_mon_disable = %d\n", pdata->tbus_mon_disable);
        ln_info("tbat_mon_disable = %d\n", pdata->tbat_mon_disable);
        ln_info("tdie_prot_disable = %d\n", pdata->tdie_prot_disable);
        ln_info("tdie_reg_disable = %d\n", pdata->tdie_reg_disable);
        ln_info("revcurr_prot_disable = %d\n", pdata->revcurr_prot_disable);

        return 0;
}

static int ln8000_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct ln8000_info *info;
        int ret = 0;
        struct iio_dev *indio_dev;
        const struct of_device_id *of_id;
        struct device_node *node = client->dev.of_node;
        static int probe_cnt = 0;

        dev_err(&client->dev, "%s: Start! probe_cnt = %d\n", __func__, ++probe_cnt);
        indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*info));
        info = iio_priv(indio_dev);
        info->indio_dev = indio_dev;

        /* detect device on connected i2c bus */
        ret = i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                dev_err(&client->dev, "fail to detect ln8000 on i2c_bus(addr=0x%x)\n", client->addr);
                ret = -EPROBE_DEFER;
                msleep(500);
                if (probe_cnt >= PROBE_CNT_MAX)
                        return -ENODEV;
                else
                        goto no_cp_device;
        }
        dev_info(&client->dev, "device id=0x%x\n", ret);

        of_id = of_match_node(ln8000_dt_match, node);
        if (of_id == NULL) {
            dev_err(&client->dev,"%s: device tree match not found!\n", __func__);
            return -ENODEV;
        }
        dev_info(&client->dev,"%s: matched to %s\n", __func__, of_id->compatible);
        info->dev_role = *(int *)of_id->data;

        info->pdata = devm_kzalloc(&client->dev, sizeof(struct ln8000_platform_data), GFP_KERNEL);
        if (info->pdata == NULL) {
            ln_err("fail to alloc devm for ln8000_platform_data\n");
            //kfree(info);
            iio_device_free(info->indio_dev);
            return -ENOMEM;
        }
        info->dev = &client->dev;
        info->client = client;
        ret = ln8000_parse_dt(info);
        if (IS_ERR_VALUE((unsigned long)ret)) {
            ln_err("fail to parsed dt\n");
            goto err_devmem;
        }

        mutex_init(&info->data_lock);
        mutex_init(&info->i2c_lock);
        mutex_init(&info->irq_lock);
    	mutex_init(&info->irq_complete);
        i2c_set_clientdata(client, info);

        ln8000_soft_reset(info);
        ln8000_init_device(info);

        INIT_DELAYED_WORK(&info->dump_regs_work, ln8000_dump_regs_workfunc);

        ret = ln_init_iio_psy(info);
        if (ret)
                goto err_cleanup;
        ret = ln8000_irq_init(info);
        if (ret < 0) {
                return ret;
        }

        if (client->irq) {
                ret = devm_request_threaded_irq(&client->dev, client->irq,
                                                NULL, ln8000_interrupt_handler,
                                                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                                "ln8000-charger-irq", info);
                if (ret < 0) {
                        ln_err("request irq for irq=%d failed, ret =%d\n",
                        client->irq, ret);
                        goto err_wakeup;
                }
                enable_irq_wake(client->irq);
        } else {
                ln_info("don't support isr(irq=%d)\n", info->client->irq);
        }

        device_init_wakeup(info->dev, 1);

        ret = ln8000_create_debugfs_entries(info);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                goto err_wakeup;
        }

        determine_initial_status(info);
        schedule_delayed_work(&info->dump_regs_work, msecs_to_jiffies(10000));

        info->usb_present = TRUE;
        dev_err(&client->dev, "%s: End!\n", __func__);

        return 0;

err_wakeup:

err_cleanup:
        i2c_set_clientdata(client, NULL);
        mutex_destroy(&info->data_lock);
        mutex_destroy(&info->i2c_lock);
        mutex_destroy(&info->irq_lock);
        cancel_delayed_work_sync(&info->dump_regs_work);
err_devmem:
        //kfree(info->pdata);
        //kfree(info);
        devm_kfree(&client->dev,info->pdata);
        iio_device_free(info->indio_dev);
no_cp_device:

        return ret;
}

static int ln8000_remove(struct i2c_client *client)
{
        struct ln8000_info *info = i2c_get_clientdata(client);

        ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);

        debugfs_remove_recursive(info->debug_root);

        i2c_set_clientdata(info->client, NULL);

        mutex_destroy(&info->data_lock);
        mutex_destroy(&info->i2c_lock);
        mutex_destroy(&info->irq_lock);
        cancel_delayed_work_sync(&info->dump_regs_work);

        //kfree(info->pdata);
        //kfree(info);
        devm_kfree(&client->dev,info->pdata);
        iio_device_free(info->indio_dev);

        return 0;
}

static void ln8000_shutdown(struct i2c_client *client)
{
        struct ln8000_info *info = i2c_get_clientdata(client);

        ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
        cancel_delayed_work_sync(&info->dump_regs_work);
}

#if defined(CONFIG_PM)
static int ln8000_suspend(struct device *dev)
{
        struct ln8000_info *info = dev_get_drvdata(dev);

        mutex_lock(&info->irq_complete);
        //sc->resume_completed = false;
        mutex_unlock(&info->irq_complete);
        return 0;
}

static int ln8000_resume(struct device *dev)
{
        struct ln8000_info *info = dev_get_drvdata(dev);

        mutex_lock(&info->irq_complete);
        //sc->resume_completed = false;
        mutex_unlock(&info->irq_complete);

        return 0;
}

static const struct dev_pm_ops ln8000_pm_ops = {
        .suspend = ln8000_suspend,
        .resume = ln8000_resume,
};
#endif

static struct i2c_driver ln8000_driver = {
        .driver   = {
                .name = "ln8000_charger",
                .owner = THIS_MODULE,
                .of_match_table = of_match_ptr(ln8000_dt_match),
#if defined(CONFIG_PM)
                .pm   = &ln8000_pm_ops,
#endif
        },
        .probe    = ln8000_probe,
        .remove   = ln8000_remove,
        .shutdown = ln8000_shutdown,
        .id_table = ln8000_id,
};
module_i2c_driver(ln8000_driver);

MODULE_AUTHOR("sungdae choi<sungdae@lionsemi.com>");
MODULE_DESCRIPTION("LIONSEMI LN8000 charger driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.3.0");

