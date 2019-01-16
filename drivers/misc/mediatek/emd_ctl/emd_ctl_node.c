#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <emd_ctl.h>
#define EMD_NODE_TYPE_NUM 2
typedef struct _emd_node
{
    char *name;
    char *type;
    int  idx;
    int  ext_num;
}emd_node_t;

typedef struct _emd_node_type
{
    char *type;
    int  major;
    int  minor_start;
    int  range;
}emd_node_type_t;

typedef struct _emd_node_type_table
{
    int major;
    emd_node_type_t array[EMD_NODE_TYPE_NUM];
}emd_node_type_table_t;


static emd_node_t emd1_node_list[] = {
    {"emd_ctl",          "chr",        0,        EMD_CHR_CLIENT_NUM},
    {"emd_cfifo",        "cfifo",      0,        EMD_CFIFO_NUM*2},
};
int emd_get_dev_major_for_md_sys(int md_id)
{
    if(md_id==0)
    {
        return EMD_CHR_DEV_MAJOR;
    }
    else
    {
        return -1;
    }
}
static emd_node_type_table_t    emd_node_type_table[EMD_MAX_NUM];
static void                        *dev_class = NULL;
static void init_emd_node_type_table(void)
{
    int i;
    int curr = 0;
    int major;
    emd_node_type_table_t *curr_table = NULL;
    
    memset(emd_node_type_table, 0, sizeof(emd_node_type_table));

    for(i = 0; i < EMD_MAX_NUM; i++){
        major = emd_get_dev_major_for_md_sys(i);
        curr_table = &emd_node_type_table[i];
        curr = 0;
        if(major < 0)
            continue;

        curr_table->major = major;

        curr_table->array[0].type = "chr";
        curr_table->array[0].major = major;
        curr_table->array[0].minor_start = curr;
        curr_table->array[0].range = EMD_CHR_CLIENT_NUM;
        curr += EMD_CHR_CLIENT_NUM;

        curr_table->array[1].type = "cfifo";
        curr_table->array[1].major = major;
        curr_table->array[1].minor_start = curr;
        curr_table->array[1].range = EMD_CFIFO_NUM*2;
        curr += EMD_CFIFO_NUM*2;
    }
}


int emd_get_md_id_by_dev_major(int dev_major)
{
    int i ;
    
    for(i = 0; i < EMD_MAX_NUM; i++){
        if(emd_node_type_table[i].major == dev_major)
            return i;
    }

    return -1;
}


int emd_get_dev_id_by_md_id(int md_id, char node_name[], int *major, int* minor)
{
    int    i;
    emd_node_type_table_t    *curr_table = NULL;
    
    curr_table = &emd_node_type_table[md_id];

    for(i = 0; i < EMD_NODE_TYPE_NUM; i++) {
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
        EMD_MSG_INF("node","create %s class fail: %d\n", name, err);
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

void release_emd_dev_node(void)
{
    if (dev_class)
        class_destroy((struct class *)dev_class);
}

int emd_dev_node_init(int md_id)
{
    int    i, j, major, minor, num;
    emd_node_t *dev_node;
    int        ret = 0;
    if(dev_class==NULL)
    {
        init_emd_node_type_table();
        dev_class = create_dev_class(THIS_MODULE, "emd_node");
    }
    if (md_id == 0) {
        dev_node = emd1_node_list;
        num = sizeof(emd1_node_list)/sizeof(emd_node_t);
    } else {
        return -1;
    }

    for(i = 0; i < num; i++) {
        if(emd_get_dev_id_by_md_id(md_id, dev_node[i].type, &major, &minor) < 0)
            break;

        minor = minor + dev_node[i].idx;
        if(dev_node[i].ext_num == 0) {
            ret = register_dev_node(dev_class, dev_node[i].name, major, minor, -1);
            if(ret < 0)
                EMD_MSG_INF("node","create %s device fail: %d\n", dev_node[i].name, ret);
        } else {
            for(j = 0; j < dev_node[i].ext_num; j++) {
                ret = register_dev_node(dev_class, dev_node[i].name, major, minor, j);
                if(ret < 0) {
                    EMD_MSG_INF("node","create %s device fail: %d\n", dev_node[i].name, ret);
                    return ret;
                }
            }
        }
    }

    return ret;
}

void emd_dev_node_exit(int md_id)
{
    int    i, j, major, minor, num;
    emd_node_t *dev_node;

    if (md_id == 0) {
        dev_node = emd1_node_list;
        num = sizeof(emd1_node_list)/sizeof(emd_node_t);
    } else {
        return;
    }

    for(i = 0; i < num; i++) {
        if(emd_get_dev_id_by_md_id(md_id, dev_node[i].type, &major, &minor) < 0)
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



