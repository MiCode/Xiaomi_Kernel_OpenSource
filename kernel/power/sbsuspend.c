/* kernel/power/sbsuspend.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/sbsuspend.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
//#include <linux/leds-mt65xx.h>


enum {
    DEBUG_USER_STATE = 1U << 0,
    DEBUG_SUSPEND = 1U << 2,
    DEBUG_VERBOSE = 1U << 3,
};
int sbsuspend_debug_mask = DEBUG_USER_STATE | DEBUG_SUSPEND | DEBUG_VERBOSE;;
module_param_named(sbsuspend_debug_mask, sbsuspend_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define _TAG_PM_M "SB_PM"
#define pm_warn(fmt, ...)    \
    if (sbsuspend_debug_mask) pr_warn("[%s][%s]" fmt, _TAG_PM_M, __func__, ##__VA_ARGS__);

#define sb_attr(_name)                         \
static struct kobj_attribute _name##_attr = {   \
    .attr = {                                   \
        .name = __stringify(_name),             \
        .mode = 0644,                           \
    },                                          \
    .show = _name##_show,                       \
    .store = _name##_store,                     \
}



//sb_state_t sb_state = SB_STATE_DISABLE;
static DEFINE_MUTEX(sb_mutex);
static LIST_HEAD(sb_handlers);
static int sb_bypass = 0;
int sb_handler_count = 0;
int sb_handler_forbid_id = 0x0;
const char *const sb_states[SB_STATE_MAX] = {
    [SB_STATE_DISABLE]  = "disable",
    [SB_STATE_ENABLE]   = "enable",
    [SB_STATE_SUSPEND]  = "suspend",
    [SB_STATE_RESUME]   = "resume",
};



extern struct kobject *power_kobj;



void register_sb_handler(struct sb_handler *handler)
{
    struct list_head *pos;

    mutex_lock(&sb_mutex);

    list_for_each(pos, &sb_handlers) {
        struct sb_handler *e;
        e = list_entry(pos, struct sb_handler, link);
        if (e->level > handler->level)
            break;
    }
    list_add_tail(&handler->link, pos);
    sb_handler_count++;

    //if ((state & SUSPENDED) && handler->suspend)
    //    handler->suspend(handler);

    mutex_unlock(&sb_mutex);
}
EXPORT_SYMBOL(register_sb_handler);

void unregister_sb_handler(struct sb_handler *handler)
{
    mutex_lock(&sb_mutex);

    list_del(&handler->link);
    sb_handler_count--;

    mutex_unlock(&sb_mutex);
}
EXPORT_SYMBOL(unregister_sb_handler);

void sb_enable(void)
{
    struct sb_handler *pos;
    int count = 0;

    pr_warn("@@@@@@@@@@@@@@@@@@@\n@@@__sb_enable__@@@\n@@@@@@@@@@@@@@@@@@@\n");

    mutex_lock(&sb_mutex);

    //pm_warn("turn backlight off\n");
    //mt65xx_leds_brightness_set(MT65XX_LED_TYPE_BUTTON, 0);
    //mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, 0);
    
    pm_warn("sb_handler_count = %d, sb_handler_forbid_id = 0x%x\n", sb_handler_count, sb_handler_forbid_id);
    list_for_each_entry(pos, &sb_handlers, link) {
        if (pos->enable != NULL) {
            if (!(sb_handler_forbid_id & (0x1 << count))) {
                if (sbsuspend_debug_mask & DEBUG_SUSPEND)
                    pm_warn("sb enable handler %d: [%pf], level: %d\n", count, pos->enable, pos->level);
                pos->enable(pos);
            }
            count++; 
        }
    }

    if (sbsuspend_debug_mask & DEBUG_SUSPEND)
        pm_warn("sb enable handler done\n");

    mutex_unlock(&sb_mutex);

}
EXPORT_SYMBOL(sb_enable);

void sb_disable(void)
{
    struct sb_handler *pos;
    int count = 0;

    pr_warn("@@@@@@@@@@@@@@@@@@@@\n@@@__sb_disable__@@@\n@@@@@@@@@@@@@@@@@@@@\n");

    mutex_lock(&sb_mutex);

    pm_warn("sb_handler_count = %d, sb_handler_forbid_id = 0x%x\n", sb_handler_count, sb_handler_forbid_id);
    list_for_each_entry_reverse(pos, &sb_handlers, link) {
        if (pos->disable != NULL) {
            if (!(sb_handler_forbid_id & (0x1 << (sb_handler_count - 1 - count)))) {
                if (sbsuspend_debug_mask & DEBUG_SUSPEND)
                    pm_warn("sb disable handler %d: [%pf], level: %d\n", count, pos->disable, pos->level);
                pos->disable(pos);
            }
            count++; 
        }
    }

    if (sbsuspend_debug_mask & DEBUG_SUSPEND)
        pm_warn("sb disable handler done\n");

    //pm_warn("turn backlight on\n");
    //mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, 255);
    //we don't need to turn keypad light on when SmartBook plug-out
    //mt65xx_leds_brightness_set(MT65XX_LED_TYPE_BUTTON, 255);

    mutex_unlock(&sb_mutex);
}
EXPORT_SYMBOL(sb_disable);

void sb_suspend(void)
{
    struct sb_handler *pos;
    int count = 0;

    pr_warn("@@@@@@@@@@@@@@@@@@@\n@@@__sb_suspend__@@@\n@@@@@@@@@@@@@@@@@@@\n");

    mutex_lock(&sb_mutex);

    pm_warn("sb_handler_count = %d, sb_handler_forbid_id = 0x%x\n", sb_handler_count, sb_handler_forbid_id);
    list_for_each_entry(pos, &sb_handlers, link) {
        if (pos->suspend != NULL) {
            if (!(sb_handler_forbid_id & (0x1 << count))) {
                if (sbsuspend_debug_mask & DEBUG_SUSPEND)
                    pm_warn("sb enable handler %d: [%pf], level: %d\n", count, pos->suspend, pos->level);
                pos->suspend(pos);
            }
            count++; 
        }
    }

    if (sbsuspend_debug_mask & DEBUG_SUSPEND)
        pm_warn("sb enable handler done\n");

    mutex_unlock(&sb_mutex);

}
EXPORT_SYMBOL(sb_suspend);

void sb_resume(void)
{
    struct sb_handler *pos;
    int count = 0;

    pr_warn("@@@@@@@@@@@@@@@@@@@@\n@@@__sb_resume__@@@\n@@@@@@@@@@@@@@@@@@@@\n");

    mutex_lock(&sb_mutex);

    pm_warn("sb_handler_count = %d, sb_handler_forbid_id = 0x%x\n", sb_handler_count, sb_handler_forbid_id);
    list_for_each_entry_reverse(pos, &sb_handlers, link) {
        if (pos->resume != NULL) {
            if (!(sb_handler_forbid_id & (0x1 << (sb_handler_count - 1 - count)))) {
                if (sbsuspend_debug_mask & DEBUG_SUSPEND)
                    pm_warn("sb disable handler %d: [%pf], level: %d\n", count, pos->resume, pos->level);
                pos->resume(pos);
            }
            count++; 
        }
    }

    if (sbsuspend_debug_mask & DEBUG_SUSPEND)
        pm_warn("sb disable handler done\n");

    mutex_unlock(&sb_mutex);
}
EXPORT_SYMBOL(sb_resume);

void sb_plug_in(void)
{
    struct sb_handler *pos;
    int count = 0;

    if (sb_bypass)
        return;

    pr_warn("@@@@@@@@@@@@@@@@@@@\n@@@__sb_plug_in__@@@\n@@@@@@@@@@@@@@@@@@@\n");

    mutex_lock(&sb_mutex);

    pm_warn("sb_handler_count = %d, sb_handler_forbid_id = 0x%x\n", sb_handler_count, sb_handler_forbid_id);
    list_for_each_entry(pos, &sb_handlers, link) {
        if (pos->plug_in != NULL) {
            if (!(sb_handler_forbid_id & (0x1 << count))) {
                if (sbsuspend_debug_mask & DEBUG_SUSPEND)
                    pm_warn("sb plug_in handler %d: [%pf], level: %d\n", count, pos->plug_in, pos->level);
                pos->plug_in(pos);
            }
            count++; 
        }
    }

    if (sbsuspend_debug_mask & DEBUG_SUSPEND)
        pm_warn("sb plug_in handler done\n");

    mutex_unlock(&sb_mutex);

}
EXPORT_SYMBOL(sb_plug_in);

void sb_plug_out(void)
{
    struct sb_handler *pos;
    int count = 0;

    if (sb_bypass)
        return;

    pr_warn("@@@@@@@@@@@@@@@@@@@@\n@@@__sb_plug_out__@@@\n@@@@@@@@@@@@@@@@@@@@\n");

    mutex_lock(&sb_mutex);

    pm_warn("sb_handler_count = %d, sb_handler_forbid_id = 0x%x\n", sb_handler_count, sb_handler_forbid_id);
    list_for_each_entry_reverse(pos, &sb_handlers, link) {
        if (pos->plug_out != NULL) {
            if (!(sb_handler_forbid_id & (0x1 << (sb_handler_count - 1 - count)))) {
                if (sbsuspend_debug_mask & DEBUG_SUSPEND)
                    pm_warn("sb plug_out handler %d: [%pf], level: %d\n", count, pos->plug_out, pos->level);
                pos->plug_out(pos);
            }
            count++; 
        }
    }

    if (sbsuspend_debug_mask & DEBUG_SUSPEND)
        pm_warn("sb plug_out handler done\n");

    mutex_unlock(&sb_mutex);
}
EXPORT_SYMBOL(sb_plug_out);

/*
void sb_event(sb_event_t event)
{
    mutex_lock(&sb_mutex);
    
    if (sbsuspend_debug_mask & DEBUG_USER_STATE) {
        struct timespec ts;
        struct rtc_time tm;
        getnstimeofday(&ts);
        rtc_time_to_tm(ts.tv_sec, &tm);
        pm_warn("%s (%d->%d) at %lld "
            "(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
            event != PM_SUSPEND_ON ? "sleep" : "wakeup",
            sb_state, event,
            ktime_to_ns(ktime_get()),
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
    }
    
    switch (sb_state)
    {
        case SB_STATE_DISABLE:
            
        case SB_STATE_ENABLE:
            
    if (!old_sleep && new_state != PM_SUSPEND_ON) {
        state |= SUSPEND_REQUESTED;
        sb_enable();
    } else if (old_sleep && new_state == PM_SUSPEND_ON) {
        state &= ~SUSPEND_REQUESTED;
        sb_disable();
    }
    sb_state = new_state;

    mutex_unlock(&sb_mutex);
    
}
*/

/**
 *    state - control system power state.
 *
 *    show() returns what states are supported, which is hard-coded to
 *    'standby' (Power-On Suspend), 'mem' (Suspend-to-RAM), and
 *    'disk' (Suspend-to-Disk).
 *
 *    store() accepts one of those strings, translates it into the
 *    proper enumerated value, and initiates a suspend transition.
 */
static ssize_t sb_state_show(struct kobject *kobj, struct kobj_attribute *attr,
              char *buf)
{
    char *s = buf;
    int i;

    for (i = 0; i < SB_STATE_MAX; i++) {
        if (sb_states[i])
            s += sprintf(s,"%s ", sb_states[i]);
    }
    if (s != buf)
        /* convert the last space to a newline */
        *(s-1) = '\n';

    return (s - buf);
}

static sb_state_t decode_sb_state(const char *buf, size_t n)
{
    sb_state_t state = SB_STATE_DISABLE;
    const char * const *s;
    char *p;
    int len;

    p = memchr(buf, '\n', n);
    len = p ? p - buf : n;

    for (s = &sb_states[state]; state < SB_STATE_MAX; s++, state++)
        if (*s && len == strlen(*s) && !strncmp(buf, *s, len))
            return state;

    return SB_STATE_MAX;
}

static ssize_t sb_state_store(struct kobject *kobj, struct kobj_attribute *attr,
               const char *buf, size_t n)
{
    sb_state_t state;
    int error = 0;
    char cmd[32];
    int param;

    if (sscanf(buf, "%s %d", cmd, &param) == 2)
    {
        if (!strcmp(cmd, "bypass"))
            sb_bypass = param;
    }
    else
    {
        if (!sb_bypass)
        {
            state = decode_sb_state(buf, n);
            
            switch (state)
            {
                case SB_STATE_DISABLE:
                    sb_disable();
                    break;
                case SB_STATE_ENABLE:
                    sb_enable();
                    break;
                case SB_STATE_SUSPEND:
                    sb_suspend();
                    break;
                case SB_STATE_RESUME:
                    sb_resume();
                    break;
                default:
                    error = -EINVAL;
                    break;
            }
        }
    }

    return error ? error : n;
}

sb_attr(sb_state);




static int __init sbsuspend_init(void)
{
    int err = 0;
    
    err |= sysfs_create_file(power_kobj, &sb_state_attr.attr);
    if (err) {
        printk("[%s]: fail to create sysfs\n", __func__);
    }
    return 0;
}

static void  __exit sbsuspend_exit(void)
{
}

core_initcall(sbsuspend_init);
module_exit(sbsuspend_exit);
