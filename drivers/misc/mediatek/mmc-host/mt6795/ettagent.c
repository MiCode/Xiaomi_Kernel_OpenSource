#include <linux/slab.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <../drivers/mmc/core/core.h>


#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include "mt_sd.h"
#include "sdio_autok.h"

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/syscalls.h>

#define MAX_ARGS_BUF    2048
extern struct msdc_host *mtk_msdc_host[];

#define DEVNAME_SIZE 80
#define TUNING_TEST_TIME (64) 
#define COMPLEX_OUTPUT 0
static const unsigned int tuning_data[] = {0xAA55AA55,0xAA558080,0x807F8080,
                                                 0x807F7F7F,0x807F7F7F,0x404040BF,
                                                 0xBFBF40BF,0xBFBF2020,0x20DF2020,
                                                 0x20DFDFDF,0x101010EF,0xEFEF10EF,
                                                 0xEFEF0808,0x08F70808,0x08F7F7F7,
                                                 0x040404FB,0xFBFB04FB,0xFBFB0202,
                                                 0x02FD0202,0x02FDFDFD,0x010101FE,
                                                 0xFEFE01FE,0xFEFE0000,0x00FF0000,
                                                 0x00FFFFFF,0x000000FF,0xFFFF00FF,
                                                 0xFFFF0000,0xFF0FFF00,0xFFCCC3CC,
                                                 0xC33CCCFF,0xFEFFFEEF,0xFFDFFFDD,
                                                 0xFFFBFFFB,0xBFFF7FFF,0x77F7BDEF,
                                                 0xFFF0FFF0,0x0FFCCC3C,0xCC33CCCF,
                                                 0xFFEFFFEE,0xFFFDFFFD,0xDFFFBFFF,
                                                 0xBBFFF7FF,0xF77F7BDE};
#if 1                                              
typedef enum
{
    E_RESULT_PASS = 0,
    E_RESULT_CMD_CRC = 1,
    E_RESULT_W_CRC = 2,
    E_RESULT_R_CRC = 3,
    E_RESULT_ERR = 4,
    E_RESULT_START = 5,
    E_RESULT_PW_SMALL = 6,
    E_RESULT_KEEP_OLD = 7,
    E_RESULT_TO = 8,
    E_RESULT_CMP_ERR = 9,
    E_RESULT_DATA_ERR = 10,
    E_RESULT_STOP_ERR = 11,
    E_RESULT_STOP_TMO = 12,
    E_RESULT_MAX
}E_RESULT_TYPE;


#endif
#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)

enum SDIO_TEST_TYPE {
    DATA_READ,                       // command response sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)
    DATA_WRITE,                     // read data sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)
    CMD,                    // write data sample selection (MSDC_SMPL_RISING, MSDC_SMPL_FALLING)
    TOTAL_TMODE_COUNT
};

#define TEST_MODE_COUNT (TOTAL_TMODE_COUNT) 
#define ARGS_TYPE_COUNT (E_AUTOK_PARM_MAX) 

#define TUNING_DATA_NO  (sizeof(tuning_data)/sizeof(unsigned int))
static unsigned int g_test_write_pattern[TUNING_TEST_TIME*TUNING_DATA_NO];
static unsigned int g_test_read_pattern[TUNING_TEST_TIME];
static const unsigned char tuning_cmd[] = { 0x55, 0xAA, 0x5A, 0xA5,   /* 01010101, 10101010, 01011010, 10100101 */
                                            0x55, 0xAA, 0x5A, 0xA5,   /* 01010101, 10101010, 01011010, 10100101 */
                                            0x55, 0xAA, 0x5A, 0xA5,   /* 01010101, 10101010, 01011010, 10100101 */
                                            0x55, 0xAA, 0x5A, 0xA5,   /* 01010101, 10101010, 01011010, 10100101 */
                                            };
#define FAKE_PATTERN    0x43144314
unsigned int fake_tuning_data[] = {FAKE_PATTERN};
#define TUNING_CMD_NO  (sizeof(tuning_cmd)/sizeof(unsigned char)) 


static struct kobject *sdio_kobj;
//test mode kobjects
static struct kobject *test_kobj;
static struct kobject **tmode_kobj;
static struct attribute **test_attrs;
static struct kobj_attribute **test_kattr;

// reg kobjects
static struct kobject *reg_kobj;
struct attribute **reg_attrs;
struct kobj_attribute **reg_kattr;
// freq kobjects
static struct kobject *freq_kobj;
struct attribute **freq_attrs;
struct kobj_attribute **freq_kattr;
//void (*func)(void); 

static E_RESULT_TYPE autok_read_test(struct msdc_host *host, unsigned int [], int len);
static E_RESULT_TYPE autok_write_test(struct msdc_host *host, unsigned int [], int len);
static E_RESULT_TYPE autok_cmd_test(struct msdc_host *host, unsigned int [], int len);
static int msdc_ettagent_write(struct msdc_host *host, unsigned int u4Addr, unsigned int u4Func, void *pBuffer, unsigned int u4Len, unsigned int u4Cmd);
static int msdc_ettagent_read(struct msdc_host *host, unsigned int u4Addr, unsigned int u4Func, void *pBuffer, unsigned int u4Len, unsigned int u4Cmd);
//extern int msdc_sdio_adjust_param(struct msdc_host *host, enum AUTOK_PARAM param, u32 *value, int rw);
static void containGen(unsigned int pattern[], int size);
//extern void udelay (unsigned long usec);
typedef E_RESULT_TYPE (*AGENT_TEST_CB) (struct msdc_host *host, unsigned int [], int len);

char test_mode[][DEVNAME_SIZE] = {"DATA_READ", "DATA_WRITE", "CMD"};
char args_type[][DEVNAME_SIZE] = 
    {"CMD_EDGE", "RDATA_EDGE", "WDATA_EDGE", 
    "CLK_DRV", "CMD_DRV", "DAT_DRV", 
    "DAT0_RD_DLY", "DAT1_RD_DLY", "DAT2_RD_DLY", "DAT3_RD_DLY", 
    "DAT_WRD_DLY", "DAT_RD_DLY", "CMD_RESP_RD_DLY", "CMD_RD_DLY",
    "DATA_DLYLINE_SEL", 
    "READ_DATA_SMPL_SEL", "WRITE_DATA_SMPL_SEL",
    "INT_DAT_LATCH_CK", 
    "CKGEN_MSDC_DLY_SEL", 
    "CMD_RSP_TA_CNTR",
    "WRDAT_CRCS_TA_CNTR",
    "PAD_CLK_TXDLY"};
char result_type[][DEVNAME_SIZE] =
{
    "E_RESULT_PASS",
    "E_RESULT_CMD_CRC",
    "E_RESULT_W_CRC",
    "E_RESULT_R_CRC",
    "E_RESULT_ERR",
    "E_RESULT_START",
    "E_RESULT_PW_SMALL",
    "E_RESULT_KEEP_OLD",
    "E_RESULT_TO",
    "E_RESULT_CMP_ERR",
    "E_RESULT_DATA_ERR",
    "E_RESULT_STOP_ERR",
    "E_RESULT_STOP_TMO",
};
AGENT_TEST_CB test_mode_cb[] = {autok_read_test, autok_write_test, autok_cmd_test };
//extern int board_sdio_ctrl (unsigned int sdio_port_num, unsigned int on);         
int pre_pattern;
int post_pattern;
#if 0
static int test_mode_change(struct msdc_host *host)
{
//  int i;
  E_RESULT_TYPE res = E_RESULT_PASS;
  unsigned char reg=0;

  /*32bit*/
  if (msdc_ettagent_read(host, SDIO_IP_WTMCR, 1, &reg, 1, CMD_52) != 0){
    printk("[ERR] sdio_func1_rd_cmd52 err\r\n") ;
    res = E_RESULT_CMD_CRC;
    goto end;
  }

  printk("SDIO_IP_WTMCR read = 0x%x\r\n", reg);
  reg &= ~(0x3);
  printk("SDIO_IP_WTMCR write = 0x%x\r\n", reg);
  if (msdc_ettagent_write(host, SDIO_IP_WTMCR, 1, &reg, 1, CMD_52) != 0){
    printk("[ERR] sdio_func1_wr_cmd52 err\r\n");
    res = E_RESULT_CMD_CRC;
    goto end;
  }
  
end:
    return res;
}


#define SDIO_CCCR_ABORT     0x06    /* function abort/card reset */
static int check_recovery(struct msdc_host* host)
{
  int ret = 0;
  int cnt=0;
  unsigned char data;
  while (1) {
    ret = msdc_ettagent_read(host, SDIO_CCCR_ABORT, 0, &data, sizeof(unsigned char), CMD_52);
    if(0 == ret)
      break;
    if(++cnt > 1000) {
      break;
    }
    msleep(5);
  }
  return ret;
}
#endif
static ssize_t test_store(struct kobject *kobj, struct kobj_attribute *attr,
                        const char *buf, size_t count)
{
    char cur_name[DEVNAME_SIZE]="";
    int test_len, cur_len;
    int i;
    int id;
    u32 value;
    int select;
    //char p_name[DEVNAME_SIZE]="";
    struct msdc_host *host;
    id = 3;
    select = -1;
    sscanf(kobj->name, "%d", &id);
    host = mtk_msdc_host[id];
    sscanf(attr->attr.name, "%s", cur_name);
    for(i=0; i<TEST_MODE_COUNT; i++){
        test_len = strlen(test_mode[i]);
        cur_len = strlen(cur_name);
        if((test_len==cur_len) && (strncmp(test_mode[i], cur_name, cur_len)==0)){
            select = i;
            break;   
        }
    }
    sscanf(buf, "%du", &value);
    switch(select){
        case DATA_READ:
            //test_mode_change(host);
            break;
        case DATA_WRITE:
            //check_recovery(host);
            break;
        case CMD:  // 1:mmc_claim, 0:mmc_release
            if(value){
                mmc_claim_host(host->mmc);
            } else {
                mmc_release_host(host->mmc); 
            }
            break;
    }
    //sscanf(buf, "%x", &fake_tuning_data[0]);
    return count;
}

static ssize_t test_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buf)
{    
    char cur_name[DEVNAME_SIZE]="";
    
    int test_len, cur_len;
    int i;
    int id;
    int len;
    int select;
    E_RESULT_TYPE result;
    //char p_name[DEVNAME_SIZE]="";
    struct msdc_host *host;
    unsigned int *pattern;
    
    id = 3;
    len = 0;
    sscanf(kobj->name, "%d", &id);
    host = mtk_msdc_host[id];
    sscanf(attr->attr.name, "%s", cur_name);
    select = 0;
    for(i=0; i<TEST_MODE_COUNT; i++){
        test_len = strlen(test_mode[i]);
        cur_len = strlen(cur_name);
        if((test_len==cur_len) && (strncmp(test_mode[i], cur_name, cur_len)==0)){
            select = i;
            break;   
        }
    }
    //mmc_claim_host(host->mmc);
    //board_sdio_ctrl(id, false);
    //board_sdio_ctrl(id, true);
    pre_pattern = -1;
    post_pattern = -1;
    if(fake_tuning_data[0] == FAKE_PATTERN){
        pattern = (unsigned int *)tuning_data;
        len = TUNING_DATA_NO;
    } else {
        pattern = (unsigned int *)fake_tuning_data;
        len = (sizeof(fake_tuning_data)/sizeof(unsigned int));   
    }
        
    containGen(pattern, len);
    result = test_mode_cb[select](host, pattern, len);
    
    //mmc_release_host(host->mmc);
    //len = snprintf(buf, 3*DEVNAME_SIZE, "res:[%d:%d:%s:%s]%8x\n", id, select, cur_name, result_type[result],pattern[0]);
    len = snprintf(buf, DEVNAME_SIZE, "%d", result);
#if COMPLEX_OUTPUT    
    if(select == DATA_READ){
        char data_buf[1024] = "";     
        int j, count = 0;
        for(i=0; i<(TUNING_TEST_TIME/4); i++){
            for(j=0; j<4; j++){            
                len = snprintf(data_buf + count, 32, "%8x ", g_test_read_pattern[i*4+j]);
                count += len;
            }
            len = snprintf(data_buf + count, 32, "\n");
            count += len;
        }
        
        memset(g_test_read_pattern, 0, TUNING_TEST_TIME*4);
        len = snprintf(buf, 2048, "res:[%d:%d:%s:%s]\n[PreData:%8x][PostData:%8x]\nREAD_PAT:\n%s\n", id, select, cur_name, result_type[result], pre_pattern, post_pattern, data_buf);
        
        
    }else if(select == DATA_WRITE)
        len = snprintf(buf, 3*DEVNAME_SIZE, "res:[%d:%d:%s:%s]\n[PreData:%8x][Status:%8x]", id, select, cur_name, result_type[result], pre_pattern, post_pattern);
    else if(select == CMD)
        len = snprintf(buf, 3*DEVNAME_SIZE, "res:[%d:%d:%s:%s]\n[Send %8x to WTMDPCR0]", id, select, cur_name, result_type[result], pre_pattern);
#endif    
    fake_tuning_data[0] = FAKE_PATTERN;
    return len;
}

static int create_test_nodes(int size, struct kobject *parent_kobj)
{
    int i, j;
    int retval = 0;
    char *name;
    //struct attribute *temp_attr;
    //char test_mode[][DEVNAME_SIZE] = {"DATA_READ", "DATA_WRITE", "CMD"};
    //int test_mode_count = sizeof(test_mode)/sizeof(test_mode[0]);
    
    test_attrs = kzalloc(TEST_MODE_COUNT * size * sizeof(struct attribute*), GFP_KERNEL);
    test_kattr = kzalloc(TEST_MODE_COUNT * size * sizeof(struct kobj_attribute*), GFP_KERNEL);
    
    tmode_kobj = kzalloc(size * sizeof(struct kobject*), GFP_KERNEL);
    for(i=0; i<size; i++){
        name = kzalloc(DEVNAME_SIZE, GFP_KERNEL);
        sprintf(name, "%d", i);
        tmode_kobj[i] = kobject_create_and_add(name, parent_kobj);
        if (tmode_kobj[i] == NULL)
            return -ENOMEM;
        for(j=0; j<TEST_MODE_COUNT; j++){    
            test_attrs[i*TEST_MODE_COUNT+j] = kzalloc(sizeof(struct attribute), GFP_KERNEL);
            test_kattr[i*TEST_MODE_COUNT+j] = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
            name = kzalloc(DEVNAME_SIZE, GFP_KERNEL);
            sprintf(name, "%s", test_mode[j]);
            test_attrs[i*TEST_MODE_COUNT+j]->name = name;
            test_attrs[i*TEST_MODE_COUNT+j]->mode = 0777;
            test_kattr[i*TEST_MODE_COUNT+j]->attr = *test_attrs[i*TEST_MODE_COUNT+j];//*temp_attr;
            test_kattr[i*TEST_MODE_COUNT+j]->show = test_show;
            test_kattr[i*TEST_MODE_COUNT+j]->store = test_store;
            //test_attrs[i*TEST_MODE_COUNT+j] = &test_kattr[i*TEST_MODE_COUNT+j]->attr;
            retval = sysfs_create_file(tmode_kobj[i], &test_kattr[i*TEST_MODE_COUNT+j]->attr);
            if (retval)
                kobject_put(tmode_kobj[i]);
            
        }
    }
    return 0;
}

u32 msdc_reg_offset = 0xffffffff;
static ssize_t reg_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buf)
{
    int id;
    int len;
    void __iomem *base;
    struct msdc_host *host;
    volatile u32* reg_addr;
    volatile unsigned int tv = 0xffffffff;
    id = 3;
    sscanf(attr->attr.name, "%d", &id);
    host = mtk_msdc_host[id];
    base = host->base;

    if(msdc_reg_offset != 0xffffffff){
        reg_addr = ((volatile u32*)(base + msdc_reg_offset));
        tv = sdr_read32(reg_addr);
    }
    len = snprintf(buf, 3*DEVNAME_SIZE, "res:[%x]\n", tv);
    
    return len;
}

static ssize_t reg_store(struct kobject *kobj, struct kobj_attribute *attr,
                        const char *buf, size_t count)
{
    int id;
    u32 offset;
    struct msdc_host *host;
    id = 3;
    
    sscanf(attr->attr.name, "%d", &id);
    host = mtk_msdc_host[id];
    
    sscanf(buf, "%x", &offset);
    if(offset > 0x104 || (offset%4)>0)
        msdc_reg_offset = 0xffffffff;
    else 
        msdc_reg_offset = offset;
    return count;
}

static int create_reg_node(int size, struct kobject *parent_kobj)
{
    int i;
    int retval = 0;
    char *name;
    
    reg_attrs = kzalloc(size * sizeof(struct attribute*), GFP_KERNEL);
    reg_kattr = kzalloc(size * sizeof(struct kobj_attribute*), GFP_KERNEL);
    
    for(i=0; i<size; i++){
        name = kzalloc(DEVNAME_SIZE, GFP_KERNEL);
        sprintf(name, "%d", i);
        reg_attrs[i] = kzalloc(sizeof(struct attribute), GFP_KERNEL);
        reg_kattr[i] = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
        reg_attrs[i]->name = name;
        reg_attrs[i]->mode = 0777;
        reg_kattr[i]->attr = *reg_attrs[i];
        reg_kattr[i]->show = reg_show;
        reg_kattr[i]->store = reg_store;
        
        retval = sysfs_create_file(parent_kobj, &reg_kattr[i]->attr);
        if (retval)
            kobject_put(parent_kobj);
    }
    return 0;
}


static ssize_t freq_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buf)
{
    int id;
    int len;
    void __iomem *base;
    struct msdc_host *host;
    
    u32 hz = 0;
    id = 3;
    sscanf(attr->attr.name, "%d", &id);
    host = mtk_msdc_host[id];
    base = host->base;
    hz = (host->mmc->ios.clock)/1000000;
    
    len = snprintf(buf, 3*DEVNAME_SIZE, "res:[%d]mhz\n", hz);
    
    return len;
}

static ssize_t freq_store(struct kobject *kobj, struct kobj_attribute *attr,
                        const char *buf, size_t count)
{
    int id;
    u32 mhz;
    struct msdc_host *host;
    struct mmc_host *mmc;
    struct mmc_ios *ios;
    id = 3;
    
    sscanf(attr->attr.name, "%d", &id);
    host = mtk_msdc_host[id];
    
    sscanf(buf, "%d", &mhz);
    mmc = host->mmc;
    ios = &mmc->ios;
    ios->clock = 0;
    host->mmc->ops->set_ios(mmc, ios);
    ios->clock = mhz*1000000;
    host->mmc->ops->set_ios(mmc, ios);
    //mmc_set_clock(host->mmc, mhz*1000000);
    return count;
}

static int create_freq_node(int size, struct kobject *parent_kobj)
{
    int i;
    int retval = 0;
    char *name;
    
    freq_attrs = kzalloc(size * sizeof(struct attribute*), GFP_KERNEL);
    freq_kattr = kzalloc(size * sizeof(struct kobj_attribute*), GFP_KERNEL);
    
    for(i=0; i<size; i++){
        name = kzalloc(DEVNAME_SIZE, GFP_KERNEL);
        sprintf(name, "%d", i);
        freq_attrs[i] = kzalloc(sizeof(struct attribute), GFP_KERNEL);
        freq_kattr[i] = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
        freq_attrs[i]->name = name;
        freq_attrs[i]->mode = 0777;
        freq_kattr[i]->attr = *freq_attrs[i];
        freq_kattr[i]->show = freq_show;
        freq_kattr[i]->store = freq_store;
        
        retval = sysfs_create_file(parent_kobj, &freq_kattr[i]->attr);
        if (retval)
            kobject_put(parent_kobj);
    }
    return 0;
}

#ifdef MOD_BUILD
static int __init ettagent_init(void)
#else
int ettagent_init(void)
#endif
{
    int retval = 0;

    /* create node /sys/mtk_sdio */
    sdio_kobj = kobject_create_and_add("mtk_sdio", NULL);
    if (sdio_kobj == NULL)
        return -ENOMEM;
        
    test_kobj = kobject_create_and_add("test", sdio_kobj);
    if (test_kobj == NULL)
        return -ENOMEM;
    
    reg_kobj = kobject_create_and_add("reg", sdio_kobj);
    if (reg_kobj == NULL)
        return -ENOMEM;
    
    freq_kobj = kobject_create_and_add("freq", sdio_kobj);
    if (freq_kobj == NULL)
        return -ENOMEM;
            
    if((retval=create_test_nodes(HOST_MAX_NUM, test_kobj)) != 0)
        return retval;
          
    if((retval=create_reg_node(HOST_MAX_NUM, reg_kobj)) != 0)
        return retval;
    
    if((retval=create_freq_node(HOST_MAX_NUM, freq_kobj)) != 0)
        return retval;
    
    return retval;
}

#ifdef MOD_BUILD
static void __exit ettagent_exit(void)
#else
void ettagent_exit(void)
#endif
{      
    int i;  
    kobject_put(reg_kobj);
    kobject_put(test_kobj);
    kobject_put(freq_kobj);
    kobject_put(sdio_kobj);
    for(i=0; i<HOST_MAX_NUM*TEST_MODE_COUNT; i++){
        struct attribute *attr = *(test_attrs+i);
        kfree(attr->name);
        kfree(attr);
        kfree(test_kattr[i]);
    }
    for(i=0; i<HOST_MAX_NUM; i++)
        kobject_put(tmode_kobj[i]);
    kfree(tmode_kobj);
    
    for(i=0; i<HOST_MAX_NUM; i++){
        struct attribute *attr = *(reg_attrs+i);
        kfree(attr->name);
        kfree(attr);
        kfree(reg_kattr[i]);
    }
    
    for(i=0; i<HOST_MAX_NUM; i++){
        struct attribute *attr = *(freq_attrs+i);
        kfree(attr->name);
        kfree(attr);
        kfree(freq_kattr[i]);
    }
}

//===============================================================================================================//
#define E_RESULT_DATA_ERR (E_RESULT_MAX)
#define E_RESULT_STOP_ERR (E_RESULT_MAX+1)
#define E_RESULT_STOP_TMO (E_RESULT_MAX+2)
static E_RESULT_TYPE errMapping(struct msdc_host *host)
{

    E_RESULT_TYPE res= E_RESULT_PASS;

    switch(host->error)
    {
        case REQ_CMD_EIO:
            res = E_RESULT_CMD_CRC;
            break;
        case REQ_CMD_TMO:
            res = E_RESULT_TO;
            break;
        case REQ_DAT_ERR:
            res = E_RESULT_DATA_ERR;
            break;
        case REQ_STOP_EIO:
            res = E_RESULT_STOP_ERR;
            break;
        case REQ_STOP_TMO:
            res = E_RESULT_STOP_TMO;
            break;
        default:
            res = E_RESULT_ERR;
            break;
    }

    return res;
}

static void containGen(unsigned int pattern[], int size)
{
    unsigned int i,j;

    unsigned int *pData = g_test_write_pattern;

    for(j=0; j<size; j++) {
        for(i=0; i<TUNING_TEST_TIME; i++) {
            *pData = pattern[j];
            pData++;
        }
    }
}

static E_RESULT_TYPE autok_write_test(struct msdc_host *host, unsigned int test_data[], int test_len)
{
    int i;
    E_RESULT_TYPE res = E_RESULT_PASS;
    unsigned int reg;
    unsigned char *data;
    
    /*use test mode to test write*/
    for(i=0; i<test_len; i++) {
        data = (unsigned char *)&test_data[i];
        pre_pattern = test_data[i];
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR1, LTE_MODEM_FUNC, (void*)data, 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR1+1, LTE_MODEM_FUNC, (void*)(data+1), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR1+2, LTE_MODEM_FUNC, (void*)(data+2), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR1+3, LTE_MODEM_FUNC, (void*)(data+3), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }

        if(msdc_ettagent_write(host, SDIO_IP_WTMDR, LTE_MODEM_FUNC, (void*)&(g_test_write_pattern[i*TUNING_TEST_TIME]),(4*TUNING_TEST_TIME), CMD_53) != 0) {
            res = errMapping(host);
            goto end;
        }

        data = (unsigned char *)&reg;
        if(msdc_ettagent_read(host, SDIO_IP_WTMCR, LTE_MODEM_FUNC, (void*)data, 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_read(host, SDIO_IP_WTMCR+1, LTE_MODEM_FUNC, (void*)(data+1), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_read(host, SDIO_IP_WTMCR+2, LTE_MODEM_FUNC, (void*)(data+2), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_read(host, SDIO_IP_WTMCR+3, LTE_MODEM_FUNC, (void*)(data+3), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        
        if((reg & TEST_MODE_STATUS) == TEST_MODE_STATUS) {
            post_pattern = reg;
            res = E_RESULT_ERR;
            goto end;
        }
    }
end:
    return res;
}

static E_RESULT_TYPE autok_read_test(struct msdc_host *host, unsigned int test_data[], int test_len)
{
    int i;
    E_RESULT_TYPE res = E_RESULT_PASS;
    unsigned char *data;
    
    /*use test mode to test read*/
    for(i=0; i<test_len; i++) {
        memset(g_test_read_pattern, 0, TUNING_TEST_TIME*4);
        data = (unsigned char *)&test_data[i];
        pre_pattern = test_data[i];
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0, LTE_MODEM_FUNC, (void*)data, 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0+1, LTE_MODEM_FUNC, (void*)(data+1), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0+2, LTE_MODEM_FUNC, (void*)(data+2), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0+3, LTE_MODEM_FUNC, (void*)(data+3), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }

        if(msdc_ettagent_read(host, SDIO_IP_WTMDR, LTE_MODEM_FUNC, (void*)g_test_read_pattern, (4*TUNING_TEST_TIME), CMD_53) != 0) {
            res = errMapping(host);
            goto end;
        }
        
        if(memcmp(g_test_read_pattern, &g_test_write_pattern[i*TUNING_TEST_TIME], 4*TUNING_TEST_TIME) != 0) {
            post_pattern = g_test_read_pattern[0];
            res = E_RESULT_CMP_ERR;
            goto end;
        }
    }
end:
    return res;
}

static E_RESULT_TYPE autok_cmd_test(struct msdc_host *host, unsigned int test_data[], int test_len)
{
    int i;
    E_RESULT_TYPE res = E_RESULT_PASS;
    unsigned char *data;

    /*use test mode to test CMD*/
    for(i=0; i<TUNING_CMD_NO; i+=4) {
        data = (unsigned char *)&tuning_cmd[i];
        pre_pattern = tuning_cmd[i];
        post_pattern = tuning_cmd[i];
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0, LTE_MODEM_FUNC, (void*)data, 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0+1, LTE_MODEM_FUNC, (void*)(data+1), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0+2, LTE_MODEM_FUNC, (void*)(data+2), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
        if(msdc_ettagent_write(host, SDIO_IP_WTMDPCR0+3, LTE_MODEM_FUNC, (void*)(data+3), 1, CMD_52) != 0) {
            res = E_RESULT_CMD_CRC;
            goto end;
        }
    }

end:
    return res;
}

static int msdc_ettagent_read(struct msdc_host *host, unsigned int u4Addr, unsigned int u4Func, void *pBuffer, unsigned int u4Len, unsigned int u4Cmd)
{
    int ret = 0;
    u8 *value = (u8 *) pBuffer;
    struct sdio_func *sdioFunc;

    if((pBuffer==NULL) || (host==NULL))
    {
        printk("[%s] pBuffer = %p, host = %p\n", __func__, pBuffer, host);
        return -1;
    }
        
    if( ((u4Cmd == 53) && (u4Len < 4)) ||
        ((u4Cmd == 52) && (u4Len > 1)) )
    {
        printk("[%s] u4Cmd = %d, u4Len = %d\n", __func__, u4Cmd, u4Len);
        return -1;
    }

    sdioFunc = host->mmc->card->sdio_func[u4Func - 1];

    //sdio_claim_host(sdioFunc);
    if(u4Cmd == 53)
        ret = sdio_readsb(sdioFunc, pBuffer, u4Addr, u4Len);
    else if(u4Cmd == 52)
        *value = sdio_readb(sdioFunc, u4Addr, &ret);
    else
    {
        printk("[%s] Doesn't support u4Cmd = %d\n", __func__, u4Cmd);
        ret = -1;
    }
    //sdio_release_host(sdioFunc);
    
//    printk("Isaac: host->error = %d\n", host->error);

    return ret;
}

/*************************************************************************
* FUNCTION
*  msdc_ettagent_write
*
* DESCRIPTION
*  This function for auto-K, write to sdio device
*
* PARAMETERS
*    host: msdc host manipulator pointer
*    u4Addr: sdio device address
*    u4Func: sdio device function
*    pBuffer: content write to device
*    u4Len: write data length
*    u4Cmd: transferred cmd (cmd52/cmd53)
*
* RETURN VALUES
*    error code: refer to errno.h
*************************************************************************/
static int msdc_ettagent_write(struct msdc_host *host, unsigned int u4Addr, unsigned int u4Func, void *pBuffer, unsigned int u4Len, unsigned int u4Cmd)
{
    int ret = 0;
    u8 *value = (u8 *) pBuffer;
    struct sdio_func *sdioFunc;

    if((pBuffer==NULL) || (host==NULL))
    {
        printk("[%s] pBuffer = %p, host = %p\n", __func__, pBuffer, host);
        return -1;
    }
        
    if( ((u4Cmd == 53) && (u4Len < 4)) ||
        ((u4Cmd == 52) && (u4Len > 1)) )
    {
        printk("[%s] u4Cmd = %d, u4Len = %d\n", __func__, u4Cmd, u4Len);
        return -1;
    }

    sdioFunc = host->mmc->card->sdio_func[u4Func - 1];

    //sdio_claim_host(sdioFunc);
    if(u4Cmd == 53)
        ret = sdio_writesb(sdioFunc, u4Addr, pBuffer, u4Len);
    else if(u4Cmd == 52)
        sdio_writeb(sdioFunc, *value, u4Addr, &ret);
    else
    {
        printk("[%s] Doesn't support u4Cmd = %d\n", __func__, u4Cmd);
        ret = -1;
    }
    //sdio_release_host(sdioFunc);
    return ret;
}

#ifdef MODULE_BUILD
module_init(ettagent_init);
module_init(ettagent_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek SDIO ETT Agent Proc");
MODULE_LICENSE("GPL");
#endif
