#define LOG_TAG         "Charger"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"

#ifdef CONFIG_CTS_CHARGER_DETECT

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
/**
 * match_string - matches given string in an array
 * @array:  array of strings
 * @n:      number of strings in the array or -1 for NULL terminated arrays
 * @string: string to match with
 *
 * Return:
 * index of a @string in the @array if matches, or %-EINVAL otherwise.
 */
int match_string(const char *const *array, size_t n, const char *string)
{
    int index;
    const char *item;

    for (index = 0; index < n; index++) {
        item = array[index];
        if (!item)
            break;
        if (!strcmp(item, string))
            return index;
    }

    return -EINVAL;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
#define CFG_CTS_CHARGER_DETECT_PSY_NOTIFY
#endif
#define CFG_CTS_CHARGER_DETECT_PSY_POLL

#include <linux/power_supply.h>
#include <linux/notifier.h>

enum cts_charger_detect_type {
    CTS_CHGR_DET_TYPE_NONE = 0,
    CTS_CHGR_DET_TYPE_PSY_NOTIFY,
    CTS_CHGR_DET_TYPE_POLL_PSP,
    CTS_CHGR_DET_TYPE_MAX
};

/* Over-written setting for DTS */
/* #define CFG_CTS_DEF_CHGR_DET_ENABLE */

/* Default settings */
#ifdef CFG_CTS_CHARGER_DETECT_PSY_NOTIFY
#define CFG_CTS_DEF_CHGR_DET_TYPE               CTS_CHGR_DET_TYPE_PSY_NOTIFY
#else /* CFG_CTS_CHARGER_DETECT_PSY_NOTIFY */
#define CFG_CTS_DEF_CHGR_DET_TYPE               CTS_CHGR_DET_TYPE_POLL_PSP
#endif /* CFG_CTS_CHARGER_DETECT_PSY_NOTIFY */

#define CFG_CTS_DEF_CHGR_DET_PSY_NAME           "usb"
#define CFG_CTS_DEF_CHGR_DET_PSY_PROP           POWER_SUPPLY_PROP_ONLINE
#define CFG_CTS_DEF_CHGR_DET_PSP_POLL_INTERVAL  2000u

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
/* Lower version has no power_supply_get_property() */
#define POWER_SUPPLY_GET_PROPERTY(psy, psp, val) \
    power_supply_get_property(psy, psp, val)
#else
#define POWER_SUPPLY_GET_PROPERTY(psy, psp, val) \
    psy->get_property(psy, psp, val)
#endif

struct cts_charger_detect_data {
    bool enable;
    bool running;
    bool state;

    /* Parameter */
    enum cts_charger_detect_type type;
    const char *psy_name;
    enum power_supply_property psp;
    u32 psp_poll_interval;    /* Unit is ms */

    /* Power supply notify */
#ifdef CFG_CTS_CHARGER_DETECT_PSY_NOTIFY
    bool notifier_registered;
    struct notifier_block psy_notifier;
#endif

#ifdef CFG_CTS_CHARGER_DETECT_PSY_POLL
    /* Polling power supply property */
    struct delayed_work psp_poll_work;
#endif

    struct work_struct set_charger_state_work;

    /* Sysfs */
#ifdef CONFIG_CTS_SYSFS
    bool sysfs_created;
#endif

    struct chipone_ts_data *cts_data;

};

#define POWER_SUPPLY_PROP_PREFIX        "POWER_SUPPLY_PROP_"
#define POWER_SUPPLY_PROP_TOKEN(property)   \
    {.prop = POWER_SUPPLY_PROP_ ##property, \
    .name = POWER_SUPPLY_PROP_PREFIX #property}

const static struct {
    enum power_supply_property prop;
    const char *name;
} power_supply_prop_token[] = {
    POWER_SUPPLY_PROP_TOKEN(STATUS),
    POWER_SUPPLY_PROP_TOKEN(PRESENT),
    POWER_SUPPLY_PROP_TOKEN(ONLINE),
};

static const char *power_supply_prop_str(enum power_supply_property prop)
{
    int i;

    for (i = 0; i < sizeof(power_supply_prop_token); i++) {
        if (prop == power_supply_prop_token[i].prop)
            return power_supply_prop_token[i].name;
    }

    return "Unknown";
}

static enum power_supply_property power_supply_prop_from_name(const char *name)
{
    size_t offset = strlen(POWER_SUPPLY_PROP_PREFIX);
    int i;

    for (i = 0; i < ARRAY_SIZE(power_supply_prop_token); i++) {
        if (strcasecmp(power_supply_prop_token[i].name + offset, name) == 0
        || strcasecmp(power_supply_prop_token[i].name, name) == 0) {
            return power_supply_prop_token[i].prop;
        }
    }

    return -ENOENT;
}

static const char *charger_detect_type_text[] = {
    "none", "notify", "poll",
};

static const char *charger_detect_type_str(enum cts_charger_detect_type type)
{
    return type < ARRAY_SIZE(charger_detect_type_text) ?
        charger_detect_type_text[type] : "Unknown";
}

static int parse_charger_detect_dt(struct cts_charger_detect_data *cd_data,
                struct device_node *np)
{
    const char *psy_name;
    const char *type_str;
    const char *psp_str;
    int ret;

    cts_info("Parse dt");

#ifdef CFG_CTS_DEF_CHGR_DET_ENABLE
    cd_data->enable = true;
#else /* CFG_CTS_DEF_CHGR_DET_ENABLE */
    cd_data->enable =
    of_property_read_bool(np, "chipone,touch-charger-detect-enable");
#endif /* CFG_CTS_DEF_CHGR_DET_ENABLE */

    cd_data->type = CFG_CTS_DEF_CHGR_DET_TYPE;
    ret = of_property_read_string(np,
            "chipone,touch-charger-detect-type", &type_str);
    if (ret)
        cts_warn("Parse detect type failed %d", ret);
    else {
        int type = match_string(charger_detect_type_text,
                ARRAY_SIZE(charger_detect_type_text),
                type_str);
        if (type < 0)
            cts_err("Parse detect type '%s' invalid", type_str);
        else
            cd_data->type = type;
    }

    ret = of_property_read_string(np, "chipone,touch-charger-detect-psy-name",
            &psy_name);
    if (ret) {
        cts_warn("Parse detect psy name failed %d", ret);
        psy_name = CFG_CTS_DEF_CHGR_DET_PSY_NAME;
    } else {
        if (power_supply_get_by_name(psy_name) == NULL) {
            cts_warn("Power supply '%s' not found", psy_name);
            psy_name = CFG_CTS_DEF_CHGR_DET_PSY_NAME;
        }
    }
    cd_data->psy_name = kstrdup(psy_name, GFP_KERNEL);
    if (cd_data->psy_name == NULL) {
        cts_err("Alloc mem for psy name failed");
        return -ENOMEM;
    }

    cd_data->psp = CFG_CTS_DEF_CHGR_DET_PSY_PROP;
    ret = of_property_read_string(np, "chipone,touch-charger-detect-psp",
            &psp_str);
    if (ret) {
        cts_warn("Parse detect psp failed %d", ret);
    } else {
        struct power_supply *psy;
        enum power_supply_property psp;
        union power_supply_propval val;

        psp = power_supply_prop_from_name(psp_str);
        if (psp < 0) {
            cts_warn("Parse detect psp: '%s' invalid", psp_str);
        } else {
            psy = power_supply_get_by_name(cd_data->psy_name);
            if (psy != NULL && POWER_SUPPLY_GET_PROPERTY(psy, psp, &val) >= 0) {
                cts_err("Parse detect psp invalid");
            } else {
                cd_data->psp = psp;
            }
        }
    }

    cd_data->psp_poll_interval = CFG_CTS_DEF_CHGR_DET_PSP_POLL_INTERVAL;
    ret = of_property_read_u32(np,
            "chipone,touch-charger-detect-psp-poll-interval",
            &cd_data->psp_poll_interval);
    if (ret)
        cts_warn("Parse detect psp poll interval failed %d", ret);

    return 0;
}

static int start_charger_detect(struct cts_charger_detect_data *cd_data)
{
    if (!cd_data->enable) {
        cts_warn("Start detect while NOT enabled");
        return -EINVAL;
    }

    if (cd_data->running) {
        cts_warn("Start detect while already RUNNING");
        return 0;
    }

    cts_info("Start detect type: %d(%s)",
        cd_data->type, charger_detect_type_str(cd_data->type));

#ifdef CFG_CTS_CHARGER_DETECT_PSY_NOTIFY
    if (cd_data->type == CTS_CHGR_DET_TYPE_PSY_NOTIFY) {
        int ret = power_supply_reg_notifier(&cd_data->psy_notifier);

        if (ret) {
            cts_err("Register notifier failed: %d", ret);
            return ret;
        }
        cd_data->notifier_registered = true;

        cd_data->running = true;

        return 0;
    }
#endif /* CFG_CTS_CHARGER_DETECT_PSY_NOTIFY */

#ifdef CFG_CTS_CHARGER_DETECT_PSY_POLL
    if (cd_data->type == CTS_CHGR_DET_TYPE_POLL_PSP) {
        if (!queue_delayed_work(cd_data->cts_data->workqueue,
        &cd_data->psp_poll_work,
        msecs_to_jiffies(cd_data->psp_poll_interval))) {
            cts_warn("Queue detect work while already on the queue");
        }
        cd_data->running = true;

        return 0;
    }
#endif /* CFG_CTS_CHARGER_DETECT_PSY_POLL */

    return -ENOTSUPP;
}

static int stop_charger_detect(struct cts_charger_detect_data *cd_data)
{
    if (!cd_data->running) {
        cts_warn("Stop detect while NOT running");
        return 0;
    }

    cts_info("Stop detect type: %d(%s)", cd_data->type,
            charger_detect_type_str(cd_data->type));

#ifdef CFG_CTS_CHARGER_DETECT_PSY_NOTIFY
    if (cd_data->type == CTS_CHGR_DET_TYPE_PSY_NOTIFY) {
        if (cd_data->notifier_registered) {
            power_supply_unreg_notifier(&cd_data->psy_notifier);
            cd_data->notifier_registered = false;
        }
        cd_data->running = false;

        return 0;
    }
#endif /* CFG_CTS_CHARGER_DETECT_PSY_NOTIFY */

#ifdef CFG_CTS_CHARGER_DETECT_PSY_POLL
    if (cd_data->type == CTS_CHGR_DET_TYPE_POLL_PSP) {
        if (!cancel_delayed_work_sync(&cd_data->psp_poll_work))
            cts_warn("Cancel poll psp work while NOT pending");

        cd_data->running = false;

        return 0;
    }
#endif /* CFG_CTS_CHARGER_DETECT_PSY_POLL */

    return 0;
}

static int enable_charger_detect(struct cts_charger_detect_data *cd_data)
{
    cts_info("Enable detect type: %d(%s)", cd_data->type,
            charger_detect_type_str(cd_data->type));

    cd_data->enable = true;

    return 0;
}

static int disable_charger_detect(struct cts_charger_detect_data *cd_data)
{
    int ret;

    cts_info("Disable detect type: %d(%s)", cd_data->type,
            charger_detect_type_str(cd_data->type));

    ret = stop_charger_detect(cd_data);
    if (ret)
        cts_err("Disable detect failed %d", ret);

    cd_data->enable = false;

    return 0;
}

static int get_charger_state(struct cts_charger_detect_data *cd_data)
{
    struct power_supply *psy;
    union power_supply_propval propval;
    int ret;

    cts_dbg("Get state from psy: '%s' prop: %d(%s)",
        cd_data->psy_name, cd_data->psp,
        power_supply_prop_str(cd_data->psp));

    psy = power_supply_get_by_name(cd_data->psy_name);
    if (cd_data->psy_name == NULL || psy == NULL) {
        cts_err("Get state from psy: '%s' not found", cd_data->psy_name);
        return -EINVAL;
    }

    ret = POWER_SUPPLY_GET_PROPERTY(psy, cd_data->psp, &propval);
    if (ret < 0) {
        cts_err("Get state from psy: '%s' prop: %d(%s) failed",
            cd_data->psy_name, cd_data->psp,
            power_supply_prop_str(cd_data->psp));
        return -EINVAL;
    }

    /* ONLY for bool type */
    cd_data->state = !!propval.intval;

    cts_dbg("State: %s", cd_data->state ? "ATTACHED" : "DETACHED");

    return 0;
}

static int switch_charger_detect_type(struct cts_charger_detect_data *cd_data,
        enum cts_charger_detect_type type)
{
    bool running;

    cts_info("Switch detect type to %d(%s)",
        type, charger_detect_type_str(type));

    if (type >= CTS_CHGR_DET_TYPE_MAX) {
        cts_err("Switch detect type %d invalid", type);
        return -EINVAL;
    }

    if (cd_data->type == type) {
        cts_warn("Switch detect type equal");
        return 0;
    }

    running = cd_data->running;

    if (running) {
        int ret = stop_charger_detect(cd_data);
        if (ret) {
            cts_err("Stop detect failed %d", ret);
            return ret;
        }
    }

    cd_data->type = type;

    if (running) {
        int ret = start_charger_detect(cd_data);
        if (ret) {
            cts_err("Start detect failed %d", ret);
            cd_data->type = CTS_CHGR_DET_TYPE_NONE;
            return ret;
        }
    }

    return 0;
}

static void set_dev_charger_state_work(struct work_struct *work)
{
    struct cts_charger_detect_data *cd_data;
    int ret;

    cd_data = container_of(work, struct cts_charger_detect_data,
            set_charger_state_work);

    cts_lock_device(&cd_data->cts_data->cts_dev);
    ret = cts_set_dev_charger_attached(&cd_data->cts_data->cts_dev,
            cd_data->state);
    cts_unlock_device(&cd_data->cts_data->cts_dev);
    if (ret) {
        cts_err("Set dev charger attached to %s failed %d",
            cd_data->state ? "ATTACHED" : "DETATCHED", ret);
        /* Set to previous state, try set again in next loop */
        cd_data->state = !cd_data->state;
    }
}

#ifdef CFG_CTS_CHARGER_DETECT_PSY_NOTIFY
static int psy_notify_callback(struct notifier_block *nb,
        unsigned long action, void *data)
{
    struct cts_charger_detect_data *cd_data;
    struct power_supply *psy;
    bool prev_state;
    int ret;

    if (nb == NULL || data == NULL) {
        cts_err("PSY notify callback with notifier %p or data %p = NULL",
            nb, data);
        return NOTIFY_DONE;
    }

    if (action != PSY_EVENT_PROP_CHANGED) {
        cts_dbg("Notify event %ld not care", action);
        return NOTIFY_DONE;
    }

    cd_data = container_of(nb, struct cts_charger_detect_data, psy_notifier);

    psy = (struct power_supply *)data;
    if (strcmp(psy->desc->name, cd_data->psy_name) != 0) {
        cts_dbg("Notify from power supply '%s' not care", psy->desc->name);
        return NOTIFY_DONE;
    }

    prev_state = cd_data->state;

    ret = get_charger_state(cd_data);
    if (ret < 0) {
        cts_err("Get state failed %d", ret);
        return NOTIFY_DONE;
    }

    cts_info("State changed: %s -> %s", prev_state ? "ATTACHED" : "DETACHED",
            cd_data->state ? "ATTACHED" : "DETACHED");

    if (!queue_work(cd_data->cts_data->workqueue,
    &cd_data->set_charger_state_work)) {
        cts_warn("Set device charger state work is PENDING");
    }

    return NOTIFY_OK;
}
#endif /* CFG_CTS_CHARGER_DETECT_PSY_NOTIFY */

#ifdef CFG_CTS_CHARGER_DETECT_PSY_POLL
static void poll_psp_work(struct work_struct *work)
{
    struct cts_charger_detect_data *cd_data;
    bool prev_state;
    int ret;

    cts_dbg("Poll psp work");

    cd_data = container_of(to_delayed_work(work),
        struct cts_charger_detect_data, psp_poll_work);

    prev_state = cd_data->state;

    ret = get_charger_state(cd_data);
    if (ret < 0) {
        cts_err("Get state failed %d", ret);
    } else {
        if (cd_data->state != prev_state) {
            cts_info("State changed: %s -> %s",
                prev_state ? "ATTACHED" : "DETACHED",
                cd_data->state ? "ATTACHED" : "DETACHED");

            if (!queue_work(cd_data->cts_data->workqueue,
            &cd_data->set_charger_state_work)) {
                cts_warn("Set device charger state work is PENDING");
            }
        }
    }

    if (!queue_delayed_work(cd_data->cts_data->workqueue,
        &cd_data->psp_poll_work,
        msecs_to_jiffies(cd_data->psp_poll_interval))) {
        cts_warn("Queue detect work while already on the queue");
    }
}
#endif /* CFG_CTS_CHARGER_DETECT_PSY_POLL */

static int init_charger_detect(struct cts_charger_detect_data *cd_data)
{
    int ret;

#ifdef CFG_CTS_CHARGER_DETECT_PSY_NOTIFY
    cd_data->psy_notifier.notifier_call = psy_notify_callback;
#endif

#ifdef CFG_CTS_CHARGER_DETECT_PSY_POLL
    INIT_DELAYED_WORK(&cd_data->psp_poll_work, poll_psp_work);
#endif

    INIT_WORK(&cd_data->set_charger_state_work, set_dev_charger_state_work);

    ret = get_charger_state(cd_data);
    if (ret)
        cts_err("Get state failed %d", ret);

    return 0;
}

/* Sysfs */
#ifdef CONFIG_CTS_SYSFS
extern int argc;
extern char *argv[];
extern int parse_arg(const char *buf, size_t count);

#define CHARGER_DET_SYSFS_GROUP_NAME "charger-det"

static ssize_t charger_detect_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;

    cts_info("Read sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s'",
        attr->attr.name);

    return scnprintf(buf, PAGE_SIZE, "Charger detect: %s\n",
            cd_data->enable ? "ENABLED" : "DISABLED");
}

/* Echo 0/n/N/of/Of/Of/OF/1/y/Y/on/oN/On/ON > enable */
static ssize_t charger_detect_enable_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;
    bool enable;
    int ret;

    cts_info("Write sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s' size %zu",
        attr->attr.name, count);

    parse_arg(buf, count);

    if (argc != 1) {
        cts_err("Invalid num of args");
        return -EINVAL;
    }

    ret = kstrtobool(argv[0], &enable);
    if (ret) {
        cts_err("Invalid param of enable");
        return ret;
    }

    if (enable)
        ret = enable_charger_detect(cd_data);
    else
        ret = disable_charger_detect(cd_data);

    if (ret) {
        cts_err("%s charger detect failed %d",
            enable ? "Enable" : "Disable", ret);
        return ret;
    }

    return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
        charger_detect_enable_show, charger_detect_enable_store);

static ssize_t charger_detect_running_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;

    cts_info("Read sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s'",
            attr->attr.name);

    return scnprintf(buf, PAGE_SIZE, "Charger detect: %sRunning\n",
            cd_data->running ? "" : "Not-");
}

/* Echo 0/n/N/of/Of/Of/OF/1/y/Y/on/oN/On/ON > runing */
static ssize_t charger_detect_running_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;
    bool running;
    int ret;

    cts_info("Write sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s' size %zu",
            attr->attr.name, count);

    parse_arg(buf, count);

    if (argc != 1) {
        cts_err("Invalid num of args");
        return -EINVAL;
    }

    ret = kstrtobool(argv[0], &running);
    if (ret) {
        cts_err("Invalid param of running");
        return ret;
    }

    if (running)
        ret = start_charger_detect(cd_data);
    else
        ret = stop_charger_detect(cd_data);

    if (ret) {
        cts_err("%s charger detect failed %d", running ? "Start" : "Stop", ret);
        return ret;
    }

    return count;
}

static DEVICE_ATTR(running, S_IWUSR | S_IRUGO,
        charger_detect_running_show, charger_detect_running_store);

static ssize_t charger_detect_type_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;

    cts_info("Read sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s'",
            attr->attr.name);

    return scnprintf(buf, PAGE_SIZE, "Charger detect type: %d(%s)\n",
            cd_data->type, charger_detect_type_str(cd_data->type));
}

/* echo charger_detect_type > type */
static ssize_t charger_detect_type_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;
    int type, ret;

    cts_info("Write sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s' size %zu",
            attr->attr.name, count);

    parse_arg(buf, count);

    if (argc != 1) {
        cts_err("");
        return -EINVAL;
    }

    type = match_string(charger_detect_type_text,
            ARRAY_SIZE(charger_detect_type_text), argv[0]);
    if (type < 0) {
        cts_err("Invalid charger detect type: '%s'", argv[0]);
        return -EINVAL;
    }

    ret = switch_charger_detect_type(cd_data, type);
    if (ret) {
        cts_err("Switch charger detect type failed %d", ret);
        return ret;
    }

    return count;
}

static DEVICE_ATTR(type, S_IWUSR | S_IRUGO,
        charger_detect_type_show, charger_detect_type_store);

static ssize_t charger_state_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;
    int ret;

    cts_info("Read sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s'",
            attr->attr.name);

    ret = get_charger_state(cd_data);
    if (ret) {
        return scnprintf(buf, PAGE_SIZE, "Get charge state failed %d\n", ret);
    }

    return scnprintf(buf, PAGE_SIZE, "Charger state: %s\n",
            cd_data->state ? "ATTACHED" : "DETACHED");
}

static DEVICE_ATTR(state, S_IRUGO, charger_state_show, NULL);

static ssize_t charger_detect_param_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;
    struct power_supply *psy;

    cts_info("Read sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s'",
            attr->attr.name);

    psy = power_supply_get_by_name(cd_data->psy_name);

    return scnprintf(buf, PAGE_SIZE,
            "Power supply name: %s(%sExist)\n"
            "Power supply prop: %d(%s)\n"
            "Poll interval    : %dms\n",
            cd_data->psy_name, psy == NULL ? "Non-" : "",
            cd_data->psp, power_supply_prop_str(cd_data->psp),
            cd_data->psp_poll_interval);
}

/* echo psy_name psy_property [interval] > param */
static ssize_t charger_detect_param_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_charger_detect_data *cd_data = cts_data->charger_detect_data;
    struct power_supply *psy;
    union power_supply_propval propval;
    enum power_supply_property psp;
    u32 interval;
    int ret;

    cts_info("Write sysfs '" CHARGER_DET_SYSFS_GROUP_NAME "/%s' size %zu",
            attr->attr.name, count);

    parse_arg(buf, count);

    if (argc < 2 || argc > 3) {
        cts_err("Invalid num of args");
        return -EINVAL;
    }

    psy = power_supply_get_by_name(argv[0]);
    if (psy == NULL) {
        cts_err("Power supply '%s' not found", argv[0]);
        return -EINVAL;
    }

    psp = power_supply_prop_from_name(argv[1]);
    if (psp < 0 || POWER_SUPPLY_GET_PROPERTY(psy, psp, &propval) < 0) {
        cts_err("Power supply '%s' property '%s' not valid",
            argv[0], power_supply_prop_str(psp));
        return -EINVAL;
    }

    interval = cd_data->psp_poll_interval;
    if (argc > 2) {
        ret = kstrtou32(argv[2], 0, &interval);
        if (ret) {
            cts_err("Arg interval is invalid");
            return -EINVAL;
        }
    }

    if (cd_data->psy_name)
        kfree(cd_data->psy_name);

    cd_data->psy_name = kstrdup(argv[0], GFP_KERNEL);
    if (cd_data->psy_name == NULL) {
        cts_err("Dup power supply name failed");
        return -ENOMEM;
    }

    cd_data->psp = psp;
    cd_data->psp_poll_interval = interval;

    return count;
}

static DEVICE_ATTR(param, S_IWUSR | S_IRUGO,
        charger_detect_param_show, charger_detect_param_store);

static struct attribute *charger_detect_attrs[] = {
    &dev_attr_enable.attr,
    &dev_attr_running.attr,
    &dev_attr_type.attr,
    &dev_attr_state.attr,
    &dev_attr_param.attr,
    NULL
};

static const struct attribute_group charger_detect_attr_group = {
    .name = CHARGER_DET_SYSFS_GROUP_NAME,
    .attrs = charger_detect_attrs,
};
#endif /* CONFIG_CTS_SYSFS */

int cts_charger_detect_init(struct chipone_ts_data *cts_data)
{
    struct cts_charger_detect_data *cd_data;
    int ret = 0;

    cts_info("Init detect");

    if (cts_data == NULL) {
        cts_err("Init detect while cts_data = NULL");
        return -EFAULT;
    }

    cd_data = kzalloc(sizeof(*cd_data), GFP_KERNEL);
    if (cd_data == NULL) {
        cts_err("Alloc charger detect data failed");
        return -ENOMEM;
    }

#ifdef CONFIG_CTS_OF
    ret = parse_charger_detect_dt(cd_data, cts_data->device->of_node);
#else /* CONFIG_CTS_OF */
#ifdef CFG_CTS_DEF_CHGR_DET_ENABLE
    cd_data->enable = true;
#else /* CFG_CTS_DEF_CHGR_DET_ENABLE */
    cd_data->enable = false;
#endif /* CFG_CTS_DEF_CHGR_DET_ENABLE */

    cd_data->type = CFG_CTS_DEF_CHGR_DET_TYPE;
    cd_data->psp = CFG_CTS_DEF_CHGR_DET_PSY_PROP;
    cd_data->psp_poll_interval = CFG_CTS_DEF_CHGR_DET_PSP_POLL_INTERVAL;
    cd_data->psy_name = kstrdup(CFG_CTS_DEF_CHGR_DET_PSY_NAME, GFP_KERNEL);
    if (cd_data->psy_name == NULL) {
        cts_err("Dup power supply name failed");
        ret = -ENOMEM;
    }
#endif /* CONFIG_CTS_OF */
    if (ret) {
        cts_err("Get detect param failed %d", ret);
        goto free_cd_data;
    }

    cts_info("Detect: %sABLED", cd_data->enable ? "EN" : "DIS");
    cts_info("  Type    : %s", charger_detect_type_str(cd_data->type));
    cts_info("  PSY Name: %s, prop: %d(%s)", cd_data->psy_name,
        cd_data->psp, power_supply_prop_str(cd_data->psp));
    cts_info("  Poll Int: %dms", cd_data->psp_poll_interval);

    ret = init_charger_detect(cd_data);
    if (ret) {
        cts_err("Init detect failed %d", ret);
        goto free_psy_name;
    }

#ifdef CONFIG_CTS_SYSFS
    cts_info("Create sysfs group '%s'", CHARGER_DET_SYSFS_GROUP_NAME);
    ret = sysfs_create_group(&cts_data->device->kobj,
            &charger_detect_attr_group);
    if (ret)
        cts_warn("Create sysfs group failed %d", ret);
    else
        cd_data->sysfs_created = true;

#endif /* CONFIG_CTS_SYSFS */

    cts_data->charger_detect_data = cd_data;
    cd_data->cts_data = cts_data;

    return 0;

free_psy_name:
    kfree(cd_data->psy_name);
free_cd_data:
    kfree(cd_data);

    return ret;
}

int cts_charger_detect_deinit(struct chipone_ts_data *cts_data)
{
    struct cts_charger_detect_data *cd_data;
    int ret;

    cts_info("Deinit detect");

    if (cts_data == NULL) {
        cts_err("Deinit detect with cts_data = NULL");
        return -EFAULT;
    }

    cd_data = cts_data->charger_detect_data;
    if (cd_data == NULL) {
        cts_warn("Deinit detect with charger_detect_data = NULL");
        return 0;
    }

    if (cd_data->running) {
        ret = stop_charger_detect(cd_data);
        if (ret)
            cts_err("Stop detect failed %d", ret);
    }

#ifdef CONFIG_CTS_SYSFS
    if (cd_data->sysfs_created) {
        cts_info("Remove sysfs group '%s'", CHARGER_DET_SYSFS_GROUP_NAME);
        sysfs_remove_group(&cts_data->device->kobj,
                &charger_detect_attr_group);
        cd_data->sysfs_created = false;
    }
#endif /* CONFIG_CTS_SYSFS */

    if (cd_data->psy_name) {
        cts_info("Kfree power supply name");
        kfree(cd_data->psy_name);
        cd_data->psy_name = NULL;
    }

    kfree(cd_data);
    cts_data->charger_detect_data = NULL;

    return 0;
}

int cts_is_charger_attached(struct chipone_ts_data *cts_data, bool *attached)
{
    struct cts_charger_detect_data *cd_data;
    int ret;

    if (cts_data == NULL) {
        cts_err("Get state with cts_data = NULL");
        return -EFAULT;
    }

    cd_data = cts_data->charger_detect_data;
    if (cd_data == NULL) {
        cts_err("Get state with charger_detect_data = NULL");
        return -ENODEV;
    }

    cts_info("Get state using type %d(%s) param",
        cd_data->type, charger_detect_type_str(cd_data->type));

    ret = get_charger_state(cd_data);
    if (ret) {
        cts_err("Get state failed %d", ret);
        return ret;
    }

    *attached = cd_data->state;

    return 0;
}

int cts_start_charger_detect(struct chipone_ts_data *cts_data)
{
    struct cts_charger_detect_data *cd_data;

    if (cts_data == NULL) {
        cts_err("Start detect with cts_data = NULL");
        return -EFAULT;
    }

    cd_data = cts_data->charger_detect_data;
    if (cd_data == NULL) {
        cts_err("Start detect with charger_detect_data = NULL");
        return -ENODEV;
    }

    return start_charger_detect(cd_data);
}

int cts_stop_charger_detect(struct chipone_ts_data *cts_data)
{
    struct cts_charger_detect_data *cd_data;

    if (cts_data == NULL) {
        cts_err("Stop detect with cts_data = NULL");
        return -EFAULT;
    }

    cd_data = cts_data->charger_detect_data;
    if (cd_data == NULL) {
        cts_err("Stop detect with charger_detect_data = NULL");
        return -ENODEV;
    }

    return stop_charger_detect(cd_data);
}
#endif /* CONFIG_CTS_CHARGER_DETECT */
