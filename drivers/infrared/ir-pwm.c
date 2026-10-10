#include "ir-pwm.h"

#define USE_USLEEP_OPTIMISE
#define USE_IRQ_DISABLED
// #define PRECISION_TEST

static struct irled_pwm_dev g_irled_pwm_dev;

#ifdef PRECISION_TEST
static u64 hrt_time[100][2];
static int hrt_idx = 0;
#endif

static int pwm_config(void)
{
    int ret = 0;

    LCT_INFO_MSG("enter\n");

    ret = clk_set_rate(g_irled_pwm_dev.s_clk, PWM_FREQ); //38.4kHz
    if (ret < 0) {
        LCT_ERR_MSG("Can't set pwm_clk rate 38400Hz (%d)\n", ret);
        clk_put(g_irled_pwm_dev.s_clk);
        return ret;
    }

    // Note: no need to config duty cycle, we config 30% as default.
    // ret = clk_set_duty_cycle(g_irled_pwm_dev.s_clk, 3, 10); //30%
    if (ret < 0) {
        LCT_ERR_MSG("Can't set pwm_clk duty cycle 3/10 (%d)\n", ret);
        clk_put(g_irled_pwm_dev.s_clk);
        return ret;
    }

    LCT_INFO_MSG("exit\n");
    return 0;
}

static long pwm_ir_tx_work(void *arg)
{
    struct pwm_ir_packet *pkt = arg;

    u64 ts_st;
    int t_tgt, t_tat_temp;
    int cpu = 0; //cpu 0 always online
    unsigned long flags;

    LCT_INFO_MSG("enter\n");

    clk_prepare(g_irled_pwm_dev.s_clk);
#ifdef USE_IRQ_DISABLED
    local_irq_save(flags);
#endif

    for (; pkt->next < pkt->length; pkt->next++) {
        if (signal_pending(current))
            break;
        if (pkt->next & 0x01) {
#ifdef PRECISION_TEST
            hrt_time[hrt_idx][0] = cpu_clock(cpu);
#endif

            //clk_disable_unprepare(g_irled_pwm_dev.s_clk);
            clk_disable(g_irled_pwm_dev.s_clk);
#ifdef PRECISION_TEST
            hrt_time[hrt_idx][1] = cpu_clock(cpu);
            hrt_idx++;
#endif
            if (g_irled_pwm_dev.use_cal_mode) {
                t_tgt = pkt->buffer[pkt->next] - g_irled_pwm_dev.g_cal_disable_time_with_us;
                t_tat_temp = t_tgt;
            }
        } else {
#ifdef PRECISION_TEST
            hrt_time[hrt_idx][0] = cpu_clock(cpu);
#endif

            //clk_prepare_enable(g_irled_pwm_dev.s_clk);
            clk_enable(g_irled_pwm_dev.s_clk);
#ifdef PRECISION_TEST
            hrt_time[hrt_idx][1] = cpu_clock(cpu);
            hrt_idx++;
#endif
            if (g_irled_pwm_dev.use_cal_mode) {
                t_tgt = pkt->buffer[pkt->next] - g_irled_pwm_dev.g_cal_enable_time_with_us;
                t_tat_temp = t_tgt;
            }
        }

        if (!g_irled_pwm_dev.use_cal_mode)
            t_tgt = pkt->buffer[pkt->next];
        ts_st = cpu_clock(cpu);
        while (t_tgt > 0) {
            if (t_tgt < 8) {
                udelay(t_tgt);
                t_tgt = 0;
            } else {
                t_tgt = t_tgt >> 1;
                if (t_tgt < 5000) {
                    udelay(t_tgt);
                } else {
#ifdef USE_USLEEP_OPTIMISE
                    usleep_range(t_tgt, t_tgt);
#ifdef USE_IRQ_DISABLED
                    local_irq_disable(); //usleep have enabled irq, so disabled again.
#endif
#else
                    udelay(t_tgt);
#endif
                }
                t_tgt = t_tat_temp - ((cpu_clock(cpu) - ts_st) / 1000);
            }
        }
    }
    if (pkt->next & 0x01) {
        LCT_INFO_MSG("clk_disable before clk_unprepare\n");
        clk_disable(g_irled_pwm_dev.s_clk);
    }
#ifdef USE_IRQ_DISABLED
    local_irq_restore(flags);
#endif
    clk_unprepare(g_irled_pwm_dev.s_clk);
    // clk_disable_unprepare(g_irled_pwm_dev.s_clk);

    LCT_INFO_MSG("exit\n");
    return pkt->next ? : -ERESTARTSYS;
}


static int pwm_ir_tx_transmit_with_delay(struct pwm_ir_packet *pkt)
{
    int rc = -ENODEV;

    LCT_INFO_MSG("enter\n");

#ifdef BIND_CPU_RUN_MODE
    int cpu;
    for_each_online_cpu(cpu) {
        if (cpu != 0) {
            rc = work_on_cpu(cpu, pwm_ir_tx_work, pkt);
            break;
        }
    }
    if (rc == -ENODEV) {
        LCT_INFO_MSG("pwm-ir: can't run on the auxiliary cpu\n");
        rc = pwm_ir_tx_work(pkt);
    }
#else
    rc = pwm_ir_tx_work(pkt);
#endif

    LCT_INFO_MSG("exit\n");
    return rc;
}

static ssize_t pwm_ir_write(struct file *filp, const char __user *buf_user, size_t size, loff_t *offp)
{
    struct pwm_ir_packet pkt = {};
    int ret;
    static int buf[BUF_MAX_SIZE] = {0};
    int temp_buf_size = size;
    int arraynum = 0;
    int i, ii;

    LCT_DEBUG_MSG("enter\n");
    LCT_DEBUG_MSG("buf_size = %d, sizeof(int) = %d\n", size, sizeof(int));

    if (temp_buf_size > BUF_MAX_SIZE) {
        LCT_INFO_MSG("Write buffer too large(%d), catch %d byte\n", temp_buf_size, BUF_MAX_SIZE);
        temp_buf_size = BUF_MAX_SIZE;
    }

    ret = copy_from_user(buf, buf_user, temp_buf_size);
    if (ret != 0) {
        LCT_ERR_MSG("copy_from_user fail ret(%d)\n", ret);
        goto pwm_write_error;
    }

    arraynum = temp_buf_size / (sizeof(int));
    LCT_INFO_MSG("pattern len = %d\n", arraynum);

    for (i = 0; i < (arraynum >> 3); i++) {
        ii = (i << 3);
        LCT_INFO_MSG("pattern[%d]: %d, %d, %d, %d, %d, %d, %d, %d\n", ii,
                      buf[ii + 0], buf[ii + 1], buf[ii + 2], buf[ii + 3],
                      buf[ii + 4], buf[ii + 5], buf[ii + 6], buf[ii + 7]);
    }
    i = (i << 3);
    for (; i < arraynum; i++)
        LCT_INFO_MSG("patterns[%d]: %d", i, buf[i]);

    mutex_lock(&g_irled_pwm_dev.write_mutex);

    ret = pwm_config();
    if (ret != 0)
        goto pwm_write_error;

    pkt.buffer = &buf[0];
    pkt.length = arraynum;
    pkt.next = 0;

#ifdef PRECISION_TEST
    hrt_idx = 0;
    memset(hrt_time, 0, sizeof(hrt_time));
#endif

    ret = pwm_ir_tx_transmit_with_delay(&pkt);
    if (ret < 0)
        goto pwm_write_error;
    memset(buf, 0x0, BUF_MAX_SIZE); // clean buffer

    mutex_unlock(&g_irled_pwm_dev.write_mutex);

    LCT_INFO_MSG("IR key send ok!\n");

    return temp_buf_size; // write success, return write size

pwm_write_error:
    mutex_unlock(&g_irled_pwm_dev.write_mutex);
    return ret;
}

#ifdef PRECISION_TEST
static ssize_t pwm_ir_read(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
    int i;

    for (i = 0; i < hrt_idx; i++) {
        if (hrt_time[i + 1][1] != 0) {
            LCT_INFO_MSG("hrt_idx=%d, cal_time=%lld", i, hrt_time[i + 1][1] - hrt_time[i][0]);
        }
    }
    hrt_idx = 0;
    memset(hrt_time, 0, sizeof(hrt_time));

    return 0;
}
#endif

static struct file_operations pwm_ir_ops = {
    .owner = THIS_MODULE,
    .write = pwm_ir_write,
#ifdef PRECISION_TEST
    .read = pwm_ir_read,
#endif
};

static struct miscdevice pwm_ir_misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &pwm_ir_ops,
};

static int irled_gpio_pin_ctrl(struct device *dev, const char *name)
{
    struct pinctrl *pmw0_pinctrl;
    struct pinctrl_state *pmw0_state;
    int ret = 0;

    pmw0_pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR(pmw0_pinctrl)) {
        LCT_ERR_MSG("devm_pinctrl_get error\n");
        return -EINVAL;
    }

    pmw0_state = pinctrl_lookup_state(pmw0_pinctrl, name);
    if (IS_ERR(pmw0_state)) {
        LCT_ERR_MSG("Couldn't find pinctrl\n");
        return -EINVAL;
    }

    ret = pinctrl_select_state(pmw0_pinctrl, pmw0_state);
    if (ret < 0) {
        LCT_ERR_MSG("Unable to configure pinctrl\n");
        return -EINVAL;
    }
    LCT_INFO_MSG("ok");

    return ret;
}

static int irled_power_on(void)
{
    int rc = 0;
    unsigned int uA_load = g_irled_pwm_dev.load_with_uA;
    unsigned int min_uV = g_irled_pwm_dev.voltage_with_uV;
    unsigned int max_uV = g_irled_pwm_dev.voltage_with_uV;

    LCT_DEBUG_MSG("enter\n");
    if (g_irled_pwm_dev.power_enable_gpio) {
        if (gpio_is_valid(g_irled_pwm_dev.power_enable_gpio)) {
            rc = gpio_direction_output(g_irled_pwm_dev.power_enable_gpio, 1);
            if (rc) {
                LCT_ERR_MSG("Unable to gpio_direction_output high\n");
            }
        }
        LCT_INFO_MSG("enable power gpio ok\n");
    }

    if (g_irled_pwm_dev.vdd_use_pmic) {
        if (regulator_is_enabled(g_irled_pwm_dev.vdd)) {
            LCT_INFO_MSG("vdd is on, don't set repeatedly!\n");
            return rc;
        }

        rc = regulator_set_load(g_irled_pwm_dev.vdd, uA_load);
        if (rc < 0) {
            LCT_ERR_MSG("vdd regulator_set_load(uA_load=%d) failed, rc=%d\n", uA_load, rc);
            return rc;
        }

        if (regulator_count_voltages(g_irled_pwm_dev.vdd) > 0) {
            rc = regulator_set_voltage(g_irled_pwm_dev.vdd, min_uV, max_uV);
            if (rc) {
                LCT_ERR_MSG("unable to set voltage on vdd\n");
                return rc;
            }
        }

        rc = regulator_enable(g_irled_pwm_dev.vdd);
        if (rc) {
            LCT_ERR_MSG("can't enable vdd\n");
            return rc;
        }

        LCT_INFO_MSG("enable vdd ok\n");
    }
    LCT_DEBUG_MSG("exit\n");

    return rc;
}

static int irled_power_off(void)
{
    int rc = 0;
    unsigned int uA_load = 0;

    LCT_DEBUG_MSG("enter\n");
    if (g_irled_pwm_dev.vdd_use_pmic) {
        rc = regulator_set_load(g_irled_pwm_dev.vdd, uA_load);
        if (rc < 0) {
            LCT_ERR_MSG("vdd regulator_set_load(uA_load=%d) failed, rc=%d\n", uA_load, rc);
        }

        if (regulator_is_enabled(g_irled_pwm_dev.vdd)) {
            regulator_disable(g_irled_pwm_dev.vdd);
        }
        LCT_INFO_MSG("disable vdd ok\n");
    }
    LCT_DEBUG_MSG("exit\n");

    return rc;
}

static int irled_parse_dts(struct device *dev)
{
    struct device_node *np = dev->of_node;
    int ret;

    if (!np) {
        LCT_ERR_MSG("np is NULL\n");
        return -EINVAL;
    }

    g_irled_pwm_dev.power_enable_gpio = of_get_named_gpio(np, "lct,irled_power_enable", 0);
    LCT_INFO_MSG("irled_power_enable gpio: %d \n", g_irled_pwm_dev.power_enable_gpio);

    g_irled_pwm_dev.vdd_use_pmic = of_property_read_bool(np, DTS_VDD_USE_PMIC_STR);
    g_irled_pwm_dev.use_cal_mode = of_property_read_bool(np, DTS_USE_CAL_MODE_STR);

    LCT_INFO_MSG("vdd_use_pmic: %d", g_irled_pwm_dev.vdd_use_pmic);
    if (g_irled_pwm_dev.vdd_use_pmic) {
        g_irled_pwm_dev.vdd = regulator_get(dev, "vdd");
        if (IS_ERR(g_irled_pwm_dev.vdd)) {
            LCT_ERR_MSG("unable to get vdd\n");
            return -EINVAL;
        }
        LCT_INFO_MSG("success to get vdd\n");

        ret = of_property_read_u32(np, DTS_LOAD_WITH_UA_STR, &g_irled_pwm_dev.load_with_uA);
        if (ret < 0) {
            LCT_ERR_MSG("fail to read lct,load_with_uA, ret = %d\n", ret);
            return -EINVAL;
        }
        LCT_INFO_MSG("load_with_uA: %d\n", g_irled_pwm_dev.load_with_uA);

        ret = of_property_read_u32(np, DTS_VOLTAGE_WITH_UV_STR, &g_irled_pwm_dev.voltage_with_uV);
        if (ret < 0) {
            LCT_ERR_MSG("Fail to read lct,voltage_with_uV, ret = %d\n", ret);
            return -EINVAL;
        }
        LCT_INFO_MSG("voltage_with_uV: %d\n", g_irled_pwm_dev.voltage_with_uV);
    }

    LCT_INFO_MSG("use_cal_mode: %d", g_irled_pwm_dev.use_cal_mode);
    if (g_irled_pwm_dev.use_cal_mode) {
        ret = of_property_read_u32(np, DTS_CALIBRATION_ENABLE_STR, &g_irled_pwm_dev.g_cal_enable_time_with_us);
        if (ret < 0) {
            LCT_ERR_MSG("fail to read lct,cal_enable_time_with_us, ret = %d\n", ret);
            return -EINVAL;
        }
        LCT_INFO_MSG("cal_enable_time_with_us: %d\n", g_irled_pwm_dev.g_cal_enable_time_with_us);

        ret = of_property_read_u32(np, DTS_CALIBRATION_DISABLE_STR, &g_irled_pwm_dev.g_cal_disable_time_with_us);
        if (ret < 0) {
            LCT_ERR_MSG("Fail to read lct,g_cal_disable_time_with_us, ret = %d\n", ret);
            return -EINVAL;
        }
        LCT_INFO_MSG("cal_disable_time_with_us: %d\n", g_irled_pwm_dev.g_cal_disable_time_with_us);
    }
    LCT_INFO_MSG("exit\n");

    return 0;
}

static int irled_pwm_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    int ret;

    LCT_INFO_MSG("enter\n");
    g_irled_pwm_dev.s_clk = NULL;
    g_irled_pwm_dev.use_cal_mode = false;
    g_irled_pwm_dev.g_cal_enable_time_with_us = 0;
    g_irled_pwm_dev.g_cal_disable_time_with_us = 0;
    g_irled_pwm_dev.vdd_use_pmic = false;
    g_irled_pwm_dev.load_with_uA = 0;
    g_irled_pwm_dev.voltage_with_uV = 0;
    g_irled_pwm_dev.vdd = NULL;

    mutex_init(&g_irled_pwm_dev.write_mutex);

    ret = irled_parse_dts(dev);
    if (ret) {
        LCT_ERR_MSG("unable to irled_parse_dts\n");
        return ret;
    }

    irled_power_on();

    // config GPIO as PWM function.
    ret = irled_gpio_pin_ctrl(dev, PWM_ACTIVE);
    if (ret < 0)
        return ret;

    g_irled_pwm_dev.s_clk = clk_get(dev, GPIO_CLOCK_NAME);
    if (g_irled_pwm_dev.s_clk == NULL) {
        LCT_ERR_MSG("get %s error\n", GPIO_CLOCK_NAME);
        return -EINVAL;
    }

    /* ret = pwm_config();
    if (ret < 0)
        return ret; */

    ret = misc_register(&pwm_ir_misc_dev);
    if (ret) {
        LCT_ERR_MSG("register miscdev failed, ret = %d\n", ret);
        return ret;
    }
    LCT_INFO_MSG("ok\n");

    return 0;
}

static int irled_pwm_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    LCT_INFO_MSG("enter\n");
    misc_deregister(&pwm_ir_misc_dev);

    if (g_irled_pwm_dev.s_clk != NULL)
        clk_disable_unprepare(g_irled_pwm_dev.s_clk);

    // config GPIO as normal gpio function.
    irled_gpio_pin_ctrl(dev, PWM_SLEEP);

    irled_power_off();

    g_irled_pwm_dev.s_clk = NULL;
    g_irled_pwm_dev.vdd = NULL;

    return 0;
}

static const struct of_device_id irled_pwm_match_table[] = {
    { .compatible = "qcom-ir-pwm", },
    {},
};

static struct platform_driver irled_pwm_driver = {
    .probe = irled_pwm_probe,
    .remove = irled_pwm_remove,
    .driver = {
        .name = "qcom-ir-pwm",
        .owner = THIS_MODULE,
        .of_match_table = irled_pwm_match_table,
    },
};

static int __init irled_pwm_init(void)
{
    int ret = platform_driver_register(&irled_pwm_driver);
    LCT_INFO_MSG("ret: %d\n", ret);

    return ret;
}

static void __exit irled_pwm_exit(void)
{
    LCT_INFO_MSG("enter\n");
    platform_driver_unregister(&irled_pwm_driver);
}

module_init(irled_pwm_init);
module_exit(irled_pwm_exit);

MODULE_AUTHOR("<lct@longcheer.com>");
MODULE_DESCRIPTION("Qualcomm PWM IR Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
