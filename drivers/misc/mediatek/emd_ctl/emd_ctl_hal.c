// dummy implement for custom API,
#include <linux/sched.h>
#include <emd_ctl.h>

void cm_gpio_setup(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_gpio_setup!\n");
}
void cm_hold_wakeup_md_signal(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_hold_wakeup_md_signal!\n");
}
void cm_release_wakeup_md_signal(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_release_wakeup_md_signal!\n");
}
void cm_enable_ext_md_wakeup_irq(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_enable_ext_md_wakeup_irq!\n");
}
int  cm_do_md_power_on(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_do_md_power_on!\n");
    return 0;
}
int  cm_do_md_power_off(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_do_md_power_off!\n");
    return 0;
}  

int  cm_do_md_go(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_do_md_go!\n");
    return 0;
}
int cm_register_irq_cb(int type, void(*irq_cb)(void))
{
    EMD_MSG_INF("chr","TODO: Dummy cm_register_irq_cb!\n");
    return 0;
}
int cm_get_assertlog_status(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_get_assertlog_status!\n");
    return 0;
}
void cm_disable_ext_md_wdt_irq(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_disable_ext_md_wdt_irq!\n");
    return 0;
}
void cm_disable_ext_md_wakeup_irq(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_disable_ext_md_wakeup_irq!\n");
    return 0;
}
void cm_disable_ext_md_exp_irq(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_disable_ext_md_exp_irq!\n");
    return 0;
}


