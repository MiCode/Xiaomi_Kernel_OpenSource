#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <ccci.h>

typedef struct _ccci_node
{
    char *name;
    char *type;
    int  idx;
    int  ext_num;
}ccci_node_t;

typedef struct _ccci_node_type
{
    char *type;
    int  major;
    int  minor_start;
    int  range;
}ccci_node_type_t;

typedef struct _ccci_node_type_table
{
    int major;
    ccci_node_type_t array[CCCI_NODE_TYPE_NUM];
}ccci_node_type_table_t;

static ccci_node_t ccci1_node_list[] = {
    //{"ccci_sys_rx",        "std chr",        2,        0},
    //{"ccci_sys_tx",        "std chr",        3,        0},
    {"ccci_pcm_rx",        "std chr",        4,        0},
    {"ccci_pcm_tx",        "std chr",        5,        0},
    {"ccci_uem_rx",        "std chr",        18,        0},
    {"ccci_uem_tx",        "std chr",        19,        0},
    {"ccci_md_log_rx",    "std chr",        42,        0},
    {"ccci_md_log_tx",    "std chr",        43,        0},
#ifdef CONFIG_MTK_ICUSB_SUPPORT
    {"ttyC",            "tty",            0,        4},
#else
    {"ttyC",        "tty",            0,        3},
#endif
    {"ccci_ipc_1220_0",    "ipc",            0,        0}, //used by AGPS
    {"ccci_ipc_2",        "ipc",            2,        0}, //used by GPS

    {"ccci_fs",            "fs",            0,        0},
#if defined(CONFIG_MTK_TC1_FEATURE)
    {"ccci_rpc",        "rpc",            0,        0},
#endif
    {"ccci_monitor",    "vir chr",        0,        0},
    {"ccci_ioctl",        "vir chr",        1,        5},
};

static ccci_node_t ccci2_node_list[] = {
    //{"ccci2_sys_rx",    "std chr",        2,        0},
    //{"ccci2_sys_tx",    "std chr",        3,        0},
    {"ccci2_pcm_rx",    "std chr",        4,        0},
    {"ccci2_pcm_tx",    "std chr",        5,        0},
    {"ccci2_uem_rx",    "std chr",        18,        0},
    {"ccci2_uem_tx",    "std chr",        19,        0},
    {"ccci2_md_log_rx",    "std chr",        42,        0}, 
    {"ccci2_md_log_tx",    "std chr",        43,        0},

#ifdef CONFIG_MTK_ICUSB_SUPPORT
    {"ccci2_tty",            "tty",            0,        4},
#else
    {"ccci2_tty",            "tty",            0,        3},
#endif

    {"ccci2_ipc_",        "ipc",            0,        1}, 

    {"ccci2_fs",        "fs",            0,        0},
#if defined(CONFIG_MTK_TC1_FEATURE)
    {"ccci2_rpc",        "rpc",            0,        0},
#endif

    {"ccci2_monitor",    "vir chr",        0,        0},
    {"ccci2_ioctl",        "vir chr",        1,        5},
};

static ccci_node_type_table_t    ccci_node_type_table[MAX_MD_NUM];
static void                        *dev_class = NULL;


static void init_ccci_node_type_table(void)
{
    int i;
    int curr = 0;
    int major;
    ccci_node_type_table_t *curr_table = NULL;
    
    memset(ccci_node_type_table, 0, sizeof(ccci_node_type_table));

    for(i = 0; i < MAX_MD_NUM; i++){
        major = get_dev_major_for_md_sys(i);
        curr_table = &ccci_node_type_table[i];
        curr = 0;
        if(major < 0)
            continue;

        curr_table->major = major;

        curr_table->array[0].type = "std chr";
        curr_table->array[0].major = major;
        curr_table->array[0].minor_start = curr;
        curr_table->array[0].range = STD_CHR_DEV_NUM;
        curr += STD_CHR_DEV_NUM;

        curr_table->array[1].type = "ipc";
        curr_table->array[1].major = major;
        curr_table->array[1].minor_start = curr;
        curr_table->array[1].range = IPC_DEV_NUM;
        curr += IPC_DEV_NUM;

        curr_table->array[2].type = "fs";
        curr_table->array[2].major = major;
        curr_table->array[2].minor_start = curr;
        curr_table->array[2].range = FS_DEV_NUM;
        curr += FS_DEV_NUM;

        curr_table->array[3].type = "vir chr";
        curr_table->array[3].major = major;
        curr_table->array[3].minor_start = curr;
        curr_table->array[3].range = VIR_CHR_DEV_NUM;
        curr += VIR_CHR_DEV_NUM;

        curr_table->array[4].type = "tty";
        curr_table->array[4].major = major;
        curr_table->array[4].minor_start = curr;
        curr_table->array[4].range = TTY_DEV_NUM;
        curr += TTY_DEV_NUM;
        
#if defined(CONFIG_MTK_TC1_FEATURE)
        curr_table->array[5].type = "rpc";
        curr_table->array[5].major = major;
        curr_table->array[5].minor_start = curr;
        curr_table->array[5].range = RPC_DEV_NUM;
        curr += RPC_DEV_NUM;
#endif        
    }
}


int get_md_id_by_dev_major(int dev_major)
{
    int i ;
    
    for(i = 0; i < MAX_MD_NUM; i++){
        if(ccci_node_type_table[i].major == dev_major)
            return i;
    }

    return -1;
}


int get_dev_id_by_md_id(int md_id, char node_name[], int *major, int* minor)
{
    int    i;
    ccci_node_type_table_t    *curr_table = NULL;
    
    curr_table = &ccci_node_type_table[md_id];

    for(i = 0; i < CCCI_NODE_TYPE_NUM; i++) {
        if(curr_table->array[i].type == NULL)
            break;
        if(strcmp(curr_table->array[i].type, node_name) == 0) {
            if(major != NULL)
                *major = curr_table->array[i].major;
            if(minor != NULL)
                *minor = curr_table->array[i].minor_start;
            return 0;
        }
    }

    return -1;
}


/***************************************************************************
 * Make device node helper function section
 ***************************************************************************/
static void* create_dev_class(struct module *owner, const char *name)
{
    int err = 0;
    
    struct class *dev_class = class_create(owner, name);
    if(IS_ERR(dev_class)){
        err = PTR_ERR(dev_class);
        CCCI_MSG("create %s class fail: %d\n", name, err);
        return NULL;
    }

    return dev_class;
}


static int register_dev_node(void *dev_class, const char *name, int major_id, int minor_start_id, int index)
{
    int ret = 0;
    dev_t dev;
    struct device *devices;

    if(index >= 0) {
        dev = MKDEV(major_id, minor_start_id) + index;
        devices = device_create( (struct class *)dev_class, NULL, dev, NULL, "%s%d", name, index );
    } else {
        dev = MKDEV(major_id, minor_start_id);
        devices = device_create( (struct class *)dev_class, NULL, dev, NULL, "%s", name );
    }

    if(IS_ERR(devices)) {
        ret = PTR_ERR(devices);
    }
    
    return ret;
}



static void release_dev_node(void *dev_class, int major_id, int minor_start_id, int index)
{
    dev_t dev;

    if(index >= 0) {
        dev = MKDEV(major_id, minor_start_id) + index;
        device_destroy((struct class *)dev_class, dev);
    } else {
        dev = MKDEV(major_id, minor_start_id);
        device_destroy((struct class *)dev_class, dev);
    }
}



/* ccci sysfs kobject */
typedef struct ccci_info
{
    struct kobject kobj;
    unsigned int ccci_attr_count;
}ccci_info_t;

typedef struct ccci_attribute
{
    struct attribute attr;
    ssize_t (*show)(char *buf);
    ssize_t (*store)(const char *buf, size_t count);
}ccci_attribute_t;

#define CCCI_ATTR(_name, _mode, _show, _store)                \
    ccci_attribute_t ccci_attr_##_name = {                    \
    .attr = {.name = __stringify(_name), .mode = _mode },    \
    .show = _show,                                            \
    .store = _store,                                        \
}

/* common func declare */
void    ccci_attr_release(struct kobject *kobj);
ssize_t ccci_attr_show(struct kobject *kobj, struct attribute *attr, char *buf);
ssize_t ccci_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count);


/* private func declear of specific attr */
//ssize_t show_attr_boot(char *buf);
//ssize_t store_attr_boot(const char *buf, size_t count);

ssize_t show_attr_md1_postfix(char *buf);
ssize_t show_attr_md2_postfix(char *buf);
ssize_t show_attr_version(char *buf)
{
    return snprintf(buf, 16, "%d\n", 2); // hardcode
}

/* global vars */
static ccci_info_t *ccci_sys_info = NULL;
struct sysfs_ops ccci_sysfs_ops = {
    .show  = ccci_attr_show,
    .store = ccci_attr_store
};

CCCI_ATTR(boot, 0660, NULL, NULL);
CCCI_ATTR(modem_info, 0644, NULL, NULL);
CCCI_ATTR(md1_postfix, 0644, show_attr_md1_postfix, NULL);
CCCI_ATTR(md2_postfix, 0644, show_attr_md2_postfix, NULL);
CCCI_ATTR(version, 0644, show_attr_version, NULL);

struct attribute *ccci_default_attrs[] = {
    &ccci_attr_boot.attr,
    &ccci_attr_modem_info.attr,
    &ccci_attr_md1_postfix.attr,
    &ccci_attr_md2_postfix.attr,
    &ccci_attr_version.attr,
    NULL
};

struct kobj_type ccci_ktype = {
    .release        = ccci_attr_release,
    .sysfs_ops         = &ccci_sysfs_ops,
    .default_attrs     = ccci_default_attrs
};

/* common func implement */
ssize_t ccci_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    ssize_t len = 0;
    ccci_attribute_t *a = container_of(attr, ccci_attribute_t, attr);

    if (a->show)
    {
        len = a->show(buf);
    }

    return len;
}

ssize_t ccci_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
    ssize_t len = 0;
    ccci_attribute_t *a = container_of(attr, ccci_attribute_t, attr);

    if (a->store)
    {
        len = a->store(buf, count);
    }

    return len;
}

void ccci_attr_release(struct kobject *kobj)
{
    ccci_info_t *ccci_info_temp = container_of(kobj, ccci_info_t, kobj);
    kfree(ccci_info_temp);
    ccci_sys_info = NULL;
}


/* private func implement */
#if 0
ssize_t show_attr_boot(char *buf)
{
    sprintf(buf, "show ccci info success\n");
    
    CCCI_MSG("show_attr_boot!\n");

    return strlen(buf);
}

ssize_t store_attr_boot(const char *buf, size_t count)
{
    int test = 0;
    sscanf(buf, "%d", &test);

    CCCI_MSG("store_attr_boot!\n", test);
    return strlen(buf);
}
#endif

ssize_t show_attr_md1_postfix(char *buf)
{
    get_md_post_fix(MD_SYS1, buf, NULL);
    
    CCCI_MSG("md1: %s\n", buf);
    
    return strlen(buf);
}

ssize_t show_attr_md2_postfix(char *buf)
{
    get_md_post_fix(MD_SYS2, buf, NULL);
    
    CCCI_MSG("md2: %s\n", buf);
    
    return strlen(buf);
}


int register_ccci_attr_func(const char *buf, ssize_t (*show)(char*), ssize_t (*store)(const char*,size_t))
{
    int i = 0;
    ccci_attribute_t *ccci_attr_temp = NULL;
    
    while(ccci_default_attrs[i])
    {
        if (!strncmp(ccci_default_attrs[i]->name, buf, strlen(ccci_default_attrs[i]->name))) {
            ccci_attr_temp = container_of(ccci_default_attrs[i], ccci_attribute_t, attr);
            break;
        }
        i++;
    }
    if (ccci_attr_temp) {
        ccci_attr_temp->show  = show;
        ccci_attr_temp->store = store;
        return 0;
    } else {
        CCCI_MSG("fail to register ccci attibute!\n");
        return -1;
    }
}



#define CCCI_KOBJ_NAME        "ccci"
extern struct kobject *kernel_kobj;
int ccci_attr_install(void)
{
    int ret = 0;

    ccci_sys_info = kmalloc(sizeof(ccci_info_t), GFP_KERNEL);
    if (!ccci_sys_info)
        return -ENOMEM;

    memset(ccci_sys_info, 0, sizeof(ccci_info_t));

    ret = kobject_init_and_add(&ccci_sys_info->kobj, &ccci_ktype, kernel_kobj, CCCI_KOBJ_NAME);
    if (ret < 0) {
        kobject_put(&ccci_sys_info->kobj);
        CCCI_MSG("fail to add ccci kobject in kernel\n");
        return ret;
    }

    ccci_sys_info->ccci_attr_count = sizeof(*ccci_default_attrs)/sizeof(struct attribute);

    return ret;

}


int init_ccci_dev_node(void)
{
    int ret = 0;
    init_ccci_node_type_table();

    // Make device class
    dev_class = create_dev_class(THIS_MODULE, "ccci_node");
    if(dev_class == NULL)
        return -1;

    ret = ccci_attr_install();
    
    return ret;
}


void release_ccci_dev_node(void)
{
    if (dev_class)
        class_destroy((struct class *)dev_class);

    if (ccci_sys_info) {    
        if (&ccci_sys_info->kobj)
            kobject_put(&ccci_sys_info->kobj);
        
        kfree(ccci_sys_info);
        ccci_sys_info = NULL;
    }
}


int mk_ccci_dev_node(int md_id)
{
    int    i, j, major, minor, num;
    ccci_node_t *dev_node;
    int        ret = 0;

    if (md_id == MD_SYS1) {
        dev_node = ccci1_node_list;
        num = sizeof(ccci1_node_list)/sizeof(ccci_node_t);
    } else if (md_id == MD_SYS2) {
        dev_node = ccci2_node_list;
        num = sizeof(ccci2_node_list)/sizeof(ccci_node_t);
    } else {
        return -1;
    }

    for(i = 0; i < num; i++) {
        if(get_dev_id_by_md_id(md_id, dev_node[i].type, &major, &minor) < 0)
            break;

        minor = minor + dev_node[i].idx;
        if(dev_node[i].ext_num == 0) {
            ret = register_dev_node(dev_class, dev_node[i].name, major, minor, -1);
            if(ret < 0)
                CCCI_MSG_INF(md_id, "cci", "create %s device fail: %d\n", dev_node[i].name, ret);
        } else {
            for(j = 0; j < dev_node[i].ext_num; j++) {
                ret = register_dev_node(dev_class, dev_node[i].name, major, minor, j);
                if(ret < 0) {
                    CCCI_MSG_INF(md_id, "cci", "create %s device fail: %d\n", dev_node[i].name, ret);
                    return ret;
                }
            }
        }
    }

    return ret;
}


void ccci_dev_node_exit(int md_id)
{
    int    i, j, major, minor, num;
    ccci_node_t *dev_node;

    if (md_id == MD_SYS1) {
        dev_node = ccci1_node_list;
        num = sizeof(ccci1_node_list)/sizeof(ccci_node_t);
    } else if (md_id == MD_SYS2) {
        dev_node = ccci2_node_list;
        num = sizeof(ccci2_node_list)/sizeof(ccci_node_t);
    } else {
        return;
    }

    for(i = 0; i < num; i++) {
        if(get_dev_id_by_md_id(md_id, dev_node[i].type, &major, &minor) < 0)
            break;

        minor = minor + dev_node[i].idx;
        if(dev_node[i].ext_num == 0) {
            release_dev_node(dev_class, major, minor, -1);
        } else {
            for(j = 0; j < dev_node[i].ext_num; j++)
                release_dev_node(dev_class, major, minor, j);
        }
    }
}



