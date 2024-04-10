#define LOG_TAG             "Earjack"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "cts_strerror.h"

#ifdef CONFIG_CTS_EARJACK_DETECT
struct cts_earjack_detect_data {
    bool enable;
    bool running;
    bool state;

    const char *earjack_state_filepath;
    struct delayed_work poll_work;
    u32 poll_interval;

#ifdef CONFIG_CTS_SYSFS
    bool sysfs_attr_group_created;
#endif /* CONFIG_CTS_SYSFS */

    struct chipone_ts_data *cts_data;
};

/* Over-ride setting for DTS */
//#define CFG_CTS_DEF_EARJACK_DET_ENABLE

#define CFG_CTS_DEF_EARJACK_POLL_INTERVAL   2000u
#if defined(CONFIG_MTK_PLATFORM)
#define CFG_CTS_DEF_EARJACK_STATE_FILEPATH  \
    "/sys/bus/platform/drivers/Accdet_Driver/state"
#elif defined(CONFIG_ARCH_SPREADRUM)
#define CFG_CTS_DEF_EARJACK_STATE_FILEPATH  \
    "/sys/kernel/headset/state"
#else
/* Current QCOM NOT support earjack detect, please disable it or define the file path */
#define CFG_CTS_DEF_EARJACK_STATE_FILEPATH ""
#endif

static const char *earjack_state_str(bool state)
{
    return state ? "ATTACHED" : "DETACHED";
}

static int parse_earjack_detect_dt(struct cts_earjack_detect_data *ed_data,
    struct device_node *np)
{
    const char *filepath;
    int ret;

    cts_info("Parse dt");

#ifdef CFG_CTS_DEF_EARJACK_DET_ENABLE
    ed_data->enable = true;
#else /* CFG_CTS_DEF_EARJACK_DET_ENABLE */
    ed_data->enable =
        of_property_read_bool(np, "chipone,touch-earjack-detect-enable");
#endif /* CFG_CTS_DEF_EARJACK_DET_ENABLE */

    ret = of_property_read_string(np,
        "chipone,touch-earjack-state-filepath", &filepath);
    if (ret) {
        cts_warn("Parse state filepath failed %d(%s)", ret, cts_strerror(ret));
        filepath = CFG_CTS_DEF_EARJACK_STATE_FILEPATH;
    }
    ed_data->earjack_state_filepath = kstrdup(filepath, GFP_KERNEL);
    if (ed_data->earjack_state_filepath == NULL) {
        cts_err("Dup earjack state filepath failed");
        return -ENOMEM;
    }

    ed_data->poll_interval = CFG_CTS_DEF_EARJACK_POLL_INTERVAL;
    ret = of_property_read_u32(np,
        "chipone,touch-earjack-poll-interval", &ed_data->poll_interval);
    if (ret) {
        cts_warn("Parse poll interval failed %d(%s)", ret, cts_strerror(ret));
    }

    return 0;
}

static int start_earjack_detect(struct cts_earjack_detect_data *ed_data)
{
    if (!ed_data->enable) {
        cts_warn("Start detect while NOT enabled");
        return -EINVAL;
    }

    if (ed_data->running) {
        cts_warn("Start detect while already RUNNING");
        return 0;
    }

    if (ed_data->earjack_state_filepath == NULL ||
        ed_data->earjack_state_filepath[0] == '\n') {
        cts_warn("Start detect with filepath = NULL/NUL");
        return -EINVAL;
    }

    cts_info("Start detect check file: '%s'", ed_data->earjack_state_filepath);

    if (!queue_delayed_work(ed_data->cts_data->workqueue,
        &ed_data->poll_work,
        msecs_to_jiffies(ed_data->poll_interval))) {
        cts_warn("Queue detect work while already on the queue");
    }

    ed_data->running = true;

    return 0;
}

static int stop_earjack_detect(struct cts_earjack_detect_data *ed_data)
{
    if (!ed_data->running) {
        cts_warn("Stop detect while NOT running");
        return 0;
    }

    cts_info("Stop detect");

    if (!cancel_delayed_work_sync(&ed_data->poll_work)) {
        cts_warn("Cancel poll work while NOT pending");
    }
    ed_data->running = false;

    return 0;
}

static int get_earjack_state(struct cts_earjack_detect_data *ed_data)
{
    int ret;
    char buff[10];
    u32 state;
#ifndef CFG_CTS_FOR_GKI
    struct file *file = NULL;
    loff_t pos = 0;
    int read_size;
#endif

    cts_dbg("Get state from file '%s'", ed_data->earjack_state_filepath);

#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): filp_open is forbiddon with GKI Version!", __func__);
#else
    file = filp_open(ed_data->earjack_state_filepath, O_RDONLY, 0);
    if (IS_ERR(file)) {
        ret = (int)PTR_ERR(file);
        cts_err("Open file '%s' failed %d(%s)",
            ed_data->earjack_state_filepath, ret, cts_strerror(ret));
        return PTR_ERR(file);
    }

    memset(buff, 0, sizeof(buff));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
    read_size = kernel_read(file, buff, sizeof(buff), &pos);
#else
    read_size = kernel_read(file, pos, buff, sizeof(buff));
#endif
    if (read_size < 0) {
        cts_err("Read state file '%s' failed %d(%s)",
            ed_data->earjack_state_filepath,
            read_size, cts_strerror(read_size));
        filp_close(file, NULL);
        return read_size;
    }

    cts_dbg("Read state file content: '%s'", buff);

    ret = filp_close(file, NULL);
    if (ret) {
        cts_warn("Close file '%s' failed %d(%s)",
            ed_data->earjack_state_filepath, ret, cts_strerror(ret));
    }
#endif

    ret = kstrtou32(buff, 0, &state);
    if (ret) {
        cts_err("Invalid string from state file: '%s'", buff);
        return ret;
    }

    /* Assume "0" means detached */
    ed_data->state = !!state;

    cts_dbg("State: %s", earjack_state_str(ed_data->state));

    return 0;
}

/* Sysfs */
#ifdef CONFIG_CTS_SYSFS
extern int argc;
extern char *argv[];
extern int parse_arg(const char *buf, size_t count);

#define EARJACK_DET_SYSFS_GROUP_NAME    "earjack-det"

static int enable_earjack_detect(struct cts_earjack_detect_data *ed_data)
{
    cts_info("Enable detect");

    ed_data->enable = true;

    return 0;
}

static int disable_earjack_detect(struct cts_earjack_detect_data *ed_data)
{
    int ret;

    cts_info("Disable detect");

    ret = stop_earjack_detect(ed_data);
    if (ret) {
        cts_err("Stop detect failed %d(%s)", ret, cts_strerror(ret));
    }

    ed_data->enable = false;

    return 0;
}

static ssize_t earjack_detect_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_earjack_detect_data *ed_data = cts_data->earjack_detect_data;

    cts_info("Read sysfs '"EARJACK_DET_SYSFS_GROUP_NAME"/%s'", attr->attr.name);

    return scnprintf(buf, PAGE_SIZE,
        "Earjack detect: %s\n",
        ed_data->enable ? "ENABLED" : "DISABLED");
}

/* Echo 0/n/N/of/Of/Of/OF/1/y/Y/on/oN/On/ON > enable */
static ssize_t earjack_detect_enable_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_earjack_detect_data *ed_data = cts_data->earjack_detect_data;
    bool enable;
    int  ret;

    cts_info("Write sysfs '"EARJACK_DET_SYSFS_GROUP_NAME"/%s' size %zu",
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

    if (enable) {
        ret = enable_earjack_detect(ed_data);
    } else {
        ret = disable_earjack_detect(ed_data);
    }
    if (ret) {
        cts_err("%s earjack detect failed %d(%s)",
            enable ? "Enable" : "Disable", ret, cts_strerror(ret));
        return ret;
    }

    return count;
}
static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
    earjack_detect_enable_show, earjack_detect_enable_store);

static ssize_t earjack_detect_running_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_earjack_detect_data *ed_data = cts_data->earjack_detect_data;

    cts_info("Read sysfs '"EARJACK_DET_SYSFS_GROUP_NAME"/%s'",
        attr->attr.name);

    return scnprintf(buf, PAGE_SIZE,
        "Earjack detect: %sRunning\n",
        ed_data->running ? "" : "Not-");
}

/* Echo 0/n/N/of/Of/Of/OF/1/y/Y/on/oN/On/ON > runing */
static ssize_t earjack_detect_running_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_earjack_detect_data *ed_data = cts_data->earjack_detect_data;
    bool running;
    int  ret;

    cts_info("Write sysfs '"EARJACK_DET_SYSFS_GROUP_NAME"/%s' size %zu",
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

    if (running) {
        ret = start_earjack_detect(ed_data);
    } else {
        ret = stop_earjack_detect(ed_data);
    }
    if (ret) {
        cts_err("%s earjack detect failed %d(%s)",
            running ? "Start" : "Stop", ret, cts_strerror(ret));
        return ret;
    }

    return count;
}
static DEVICE_ATTR(running, S_IWUSR | S_IRUGO,
    earjack_detect_running_show, earjack_detect_running_store);

static ssize_t earjack_state_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_earjack_detect_data *ed_data = cts_data->earjack_detect_data;
    int ret;

    cts_info("Read sysfs '"EARJACK_DET_SYSFS_GROUP_NAME"/%s'",
        attr->attr.name);

    ret = get_earjack_state(ed_data);
    if (ret) {
        return scnprintf(buf, PAGE_SIZE,
            "Get earjack state failed %d(%s)\n",
            ret, cts_strerror(ret));
    }

    return scnprintf(buf, PAGE_SIZE,
        "Earjack state: %s\n", earjack_state_str(ed_data->state));
}
static DEVICE_ATTR(state,   S_IRUGO, earjack_state_show, NULL);

static ssize_t earjack_detect_param_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_earjack_detect_data *ed_data = cts_data->earjack_detect_data;

    cts_info("Read sysfs '"EARJACK_DET_SYSFS_GROUP_NAME"/%s'",
        attr->attr.name);

    return scnprintf(buf, PAGE_SIZE,
        "Filepath: '%s'\n"
        "Poll int: %dms\n",
        ed_data->earjack_state_filepath, ed_data->poll_interval);
}

/* echo filepath [interval] > param */
static ssize_t earjack_detect_param_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_earjack_detect_data *ed_data = cts_data->earjack_detect_data;
    u32 interval;
    int ret;

    cts_info("Write sysfs '"EARJACK_DET_SYSFS_GROUP_NAME"/%s' size %zu",
        attr->attr.name, count);

    parse_arg(buf, count);

    if (argc < 1 || argc > 2) {
        cts_err("Invalid num of args");
        return -EINVAL;
    }

    interval = ed_data->poll_interval;
    if (argc > 1) {
        ret = kstrtou32(argv[1], 0, &interval);
        if (ret) {
            cts_err("Arg interval is invalid");
            return -EINVAL;
        }
    }

    if (ed_data->earjack_state_filepath) {
        kfree(ed_data->earjack_state_filepath);
    }
    ed_data->earjack_state_filepath = kstrdup(argv[0], GFP_KERNEL);
    if (ed_data->earjack_state_filepath == NULL) {
        cts_err("Dup earjack state filepath failed");
        return -ENOMEM;
    }
    ed_data->poll_interval = interval;

    return count;
}
static DEVICE_ATTR(param, S_IWUSR | S_IRUGO,
    earjack_detect_param_show, earjack_detect_param_store);

static struct attribute *earjack_detect_attrs[] = {
    &dev_attr_enable.attr,
    &dev_attr_running.attr,
    &dev_attr_state.attr,
    &dev_attr_param.attr,
    NULL
};

static const struct attribute_group earjack_detect_attr_group = {
    .name  = EARJACK_DET_SYSFS_GROUP_NAME,
    .attrs = earjack_detect_attrs,
};
#endif /* CONFIG_CTS_SYSFS */

static void poll_earjack_state_work(struct work_struct *work)
{
    struct cts_earjack_detect_data *ed_data;
    bool prev_state = false;
    int ret;

    cts_dbg("Poll earjack state work");

    ed_data = container_of(to_delayed_work(work),
        struct cts_earjack_detect_data, poll_work);

    prev_state = ed_data->state;

    ret = get_earjack_state(ed_data);
    if (ret) {
        cts_err("Get state failed %d(%s)", ret, cts_strerror(ret));
    } else {
        if (ed_data->state != prev_state) {
            cts_info("State changed: %s -> %s",
                earjack_state_str(prev_state),
                earjack_state_str(ed_data->state));

            cts_lock_device(&ed_data->cts_data->cts_dev);
            ret = cts_set_dev_earjack_attached(
                &ed_data->cts_data->cts_dev, ed_data->state);
            cts_unlock_device(&ed_data->cts_data->cts_dev);
            if (ret) {
                cts_err("Set dev earjack attached to %s failed %d(%s)",
                    earjack_state_str(ed_data->state),
                    ret, cts_strerror(ret));
                /* Set to previous state, try set again in next loop */
                ed_data->state = prev_state;
            }
        }
    }

    if (!queue_delayed_work(ed_data->cts_data->workqueue,
        &ed_data->poll_work,
        msecs_to_jiffies(ed_data->poll_interval))) {
        cts_warn("Queue detect work while already on the queue");
    }
}

int cts_earjack_detect_init(struct chipone_ts_data *cts_data)
{
    struct cts_earjack_detect_data *ed_data;
    int ret = 0;

    if (cts_data == NULL) {
        cts_err("Init detect with cts_data = NULL");
        return -EFAULT;
    }

    ed_data = kzalloc(sizeof(*ed_data), GFP_KERNEL);
    if (ed_data == NULL) {
        cts_err("Alloc earjack detect data failed");
        return -ENOMEM;
    }

    cts_info("Init detect");

#ifdef CONFIG_CTS_OF
    ret = parse_earjack_detect_dt(ed_data, cts_data->device->of_node);
#else /* CONFIG_CTS_OF */
#ifdef CFG_CTS_DEF_EARJACK_DET_ENABLE
    ed_data->enable = true;
#else /* CFG_CTS_DEF_EARJACK_DET_ENABLE */
    ed_data->enable = false;
#endif /* CFG_CTS_DEF_EARJACK_DET_ENABLE */
    ed_data->poll_interval = CFG_CTS_DEF_EARJACK_POLL_INTERVAL;
    ed_data->earjack_state_filepath = kstrdup(
        CFG_CTS_DEF_EARJACK_STATE_FILEPATH, GFP_KERNEL);
    if (ed_data->earjack_state_filepath == NULL) {
        cts_err("Dup earjack state filepath failed");
        ret = -ENOMEM;
    }
#endif /* CONFIG_CTS_OF */
    if (ret) {
        cts_err("Get detect param failed %d(%s)",
            ret, cts_strerror(ret));
        goto free_ed_data;
    }

    cts_info("Detect: %sABLED",  ed_data->enable ? "EN" : "DIS");
    cts_info("  Filepath: '%s'", ed_data->earjack_state_filepath);
    cts_info("  Poll Int: %dms", ed_data->poll_interval);

    INIT_DELAYED_WORK(&ed_data->poll_work, poll_earjack_state_work);

#ifdef CONFIG_CTS_SYSFS
    cts_info("Create sysfs attr group '%s'", EARJACK_DET_SYSFS_GROUP_NAME);
    ret = sysfs_create_group(&cts_data->device->kobj,
        &earjack_detect_attr_group);
    if (ret) {
        cts_warn("Create sysfs attr group '%s' failed %d(%s)",
            EARJACK_DET_SYSFS_GROUP_NAME, ret, cts_strerror(ret));
    } else {
        ed_data->sysfs_attr_group_created = true;
    }
#endif /* CONFIG_CTS_SYSFS */

    cts_data->earjack_detect_data = ed_data;
    ed_data->cts_data = cts_data;

    return 0;

free_ed_data:
    kfree(ed_data);

    return ret;
}

int cts_earjack_detect_deinit(struct chipone_ts_data *cts_data)
{
    struct cts_earjack_detect_data *ed_data;
    int ret;

    if (cts_data == NULL) {
        cts_err("Deinit detect with cts_data = NULL");
        return -EFAULT;
    }

    ed_data = cts_data->earjack_detect_data;
    if (ed_data == NULL) {
        cts_warn("Deinit detect with earjack_detect_data = NULL");
        return 0;
    }

    cts_info("Deinit detect");

    if (ed_data->running) {
        ret = stop_earjack_detect(ed_data);
        if (ret) {
            cts_err("Stop detect failed %d(%s)",
                ret, cts_strerror(ret));
        }
    }

#ifdef CONFIG_CTS_SYSFS
    if (ed_data->sysfs_attr_group_created) {
        cts_info("Remove sysfs attr group '%s'", EARJACK_DET_SYSFS_GROUP_NAME);
        sysfs_remove_group(&cts_data->device->kobj, &earjack_detect_attr_group);
        ed_data->sysfs_attr_group_created = false;
    }
#endif /* CONFIG_CTS_SYSFS */

    if(ed_data->earjack_state_filepath) {
        kfree(ed_data->earjack_state_filepath);
        ed_data->earjack_state_filepath = NULL;
    }

    kfree(ed_data);
    cts_data->earjack_detect_data = NULL;

    return 0;
}

int cts_is_earjack_attached(struct chipone_ts_data *cts_data, bool *attached)
{
    struct cts_earjack_detect_data *ed_data;
    int ret;

    if (cts_data == NULL) {
        cts_err("Get state with cts_data = NULL");
        return -EFAULT;
    }

    ed_data = cts_data->earjack_detect_data;
    if (ed_data == NULL) {
        cts_err("Get state with earjack_detect_data = NULL");
        return -ENODEV;
    }

    ret = get_earjack_state(ed_data);
    if (ret) {
        cts_err("Get state failed %d(%s)", ret, cts_strerror(ret));
        return ret;
    }

    cts_info("Get curr state: %s", earjack_state_str(ed_data->state));

    *attached = ed_data->state;

    return 0;
}

int cts_start_earjack_detect(struct chipone_ts_data *cts_data)
{
    struct cts_earjack_detect_data *ed_data;

    if (cts_data == NULL) {
        cts_err("Start detect with cts_data = NULL");
        return -EFAULT;
    }

    ed_data = cts_data->earjack_detect_data;
    if (ed_data == NULL) {
        cts_err("Start detect with earjack_detect_data = NULL");
        return -ENODEV;
    }

    return start_earjack_detect(ed_data);
}

int cts_stop_earjack_detect(struct chipone_ts_data *cts_data)
{
    struct cts_earjack_detect_data *ed_data;

    if (cts_data == NULL) {
        cts_err("Stop detect with cts_data = NULL");
        return -EFAULT;
    }

    ed_data = cts_data->earjack_detect_data;
    if (ed_data == NULL) {
        cts_err("Stop detect with earjack_detect_data = NULL");
        return -ENODEV;
    }

    return stop_earjack_detect(ed_data);
}
#endif /* CONFIG_CTS_EARJACK_DETECT */

