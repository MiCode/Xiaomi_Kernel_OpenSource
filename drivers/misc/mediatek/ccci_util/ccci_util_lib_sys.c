#include <mach/sync_write.h>
#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include "ccci_util_log.h"

#define CCCI_KOBJ_NAME "ccci"

struct ccci_info
{
	struct kobject kobj;
	unsigned int ccci_attr_count;
};

struct ccci_attribute
{
	struct attribute attr;
	ssize_t (*show)(char *buf);
	ssize_t (*store)(const char *buf, size_t count);
};

#define CCCI_ATTR(_name, _mode, _show, _store)			\
static struct ccci_attribute ccci_attr_##_name = {		\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.show = _show,						\
	.store = _store,					\
}


static struct ccci_info *ccci_sys_info = NULL;

static void ccci_obj_release(struct kobject *kobj)
{
	struct ccci_info *ccci_info_temp = container_of(kobj, struct ccci_info, kobj);
	kfree(ccci_info_temp);
	ccci_sys_info = NULL; // as ccci_info_temp==ccci_sys_info
}

static ssize_t ccci_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct ccci_attribute *a = container_of(attr, struct ccci_attribute, attr);

	if (a->show)
		len = a->show(buf);

	return len;
}

static ssize_t ccci_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_attribute *a = container_of(attr, struct ccci_attribute, attr);

	if (a->store)
		len = a->store(buf, count);

	return len;
}

static struct sysfs_ops ccci_sysfs_ops = {
	.show  = ccci_attr_show,
	.store = ccci_attr_store
};

//=======================================
// CCCI common sys attribute part
//=======================================
// Sys -- boot status
static get_status_func_t get_status_func[MAX_MD_NUM];
static boot_md_func_t boot_md_func[MAX_MD_NUM];
static int get_md_status(int md_id, char val[], int size)
{
	if((md_id<MAX_MD_NUM) && (get_status_func[md_id]!=NULL))
		(get_status_func[md_id])(md_id, val, size);
	else
		snprintf(val, 32, "md%d:n/a", md_id+1);

	return 0;
}

static int trigger_md_boot(int md_id)
{
	if((md_id<MAX_MD_NUM) && (boot_md_func[md_id]!=NULL)) {
		(boot_md_func[md_id])(md_id);
		return 0;
	}

	return -1;
}

static ssize_t boot_status_show(char *buf)
{
	char md1_sta_str[32];
	char md2_sta_str[32];
	char md3_sta_str[32];
	char md5_sta_str[32];

	// MD1 ---
	get_md_status(MD_SYS1, md1_sta_str, 32);
	// MD2 ---
	get_md_status(MD_SYS2, md2_sta_str, 32);
	// MD3
	get_md_status(MD_SYS3, md3_sta_str, 32);
	// MD5
	get_md_status(MD_SYS5, md5_sta_str, 32);

	// Final string
	return snprintf(buf, 32*4+3*4 +1, "%s | %s | %s | md4:n/a | %s\n", md1_sta_str, md2_sta_str, md3_sta_str, md5_sta_str);
}

static ssize_t boot_status_store(const char *buf, size_t count)
{
	unsigned int md_id;

	md_id = buf[0] - '0';
	CCCI_UTIL_INF_MSG( "md%d get boot store\n", md_id+1);
	if (md_id < MAX_MD_NUM) {
		if(trigger_md_boot(md_id)!=0)
			CCCI_UTIL_INF_MSG( "md%d n/a\n", md_id+1);
	} else
		CCCI_UTIL_INF_MSG( "invalid id(%d)\n", md_id+1);
	return count;
}
CCCI_ATTR(boot, 0660, &boot_status_show, &boot_status_store);

// Sys -- enable status
static ssize_t ccci_md_enable_show(char *buf)
{
	int i;
	char md_en[MAX_MD_NUM];

	for(i=0; i<MAX_MD_NUM; i++) {
		if(get_modem_is_enabled(MD_SYS1+i))
			md_en[i] = 'E';
		else
			md_en[i] = 'D';
	}

	// Final string
	return snprintf(buf, 32, "%c-%c-%c-%c-%c (1->5)\n", md_en[0], md_en[1], md_en[2], md_en[3], md_en[4]);
}

CCCI_ATTR(md_en, 0660, &ccci_md_enable_show, NULL);

// Sys -- Versin
static ssize_t ccci_version_show(char *buf)
{
#ifdef ENABLE_EXT_MD_DSDA	
	return snprintf(buf, 16, "%d\n", 5); // DSDA
#else
	return snprintf(buf, 16, "%d\n", 3); // ECCCI
#endif
}

CCCI_ATTR(version, 0644, &ccci_version_show, NULL);

extern unsigned int ccci_debug_enable;
static ssize_t debug_enable_show(char *buf)
{
	int curr = 0;

	curr = snprintf(buf, 16, "%d\n", ccci_debug_enable);
    return curr;
}

static ssize_t debug_enable_store(const char *buf, size_t count)
{
	ccci_debug_enable = buf[0] - '0';
	return count;
}

CCCI_ATTR(debug, 0660, &debug_enable_show, &debug_enable_store);

// Sys -- Runtime register debug
static char aat_cmd[32];
static unsigned long  aat_para[8];
static void __iomem * aat_runtime_map_base_vir;
static phys_addr_t aat_runtime_map_base_phy;
static unsigned int aat_err_no;
static char aat_err_str[64];
static int aat_show_case;
static unsigned long  ast_dump_start_addr;
static unsigned long  ast_dump_size;
static unsigned long  ast_dump_width;
static unsigned char read8(unsigned char *addr)
{
	return ioread8((void __iomem *)addr);
}

static unsigned short read16(unsigned short *addr)
{
	return ioread16((void __iomem *)addr);
}

static unsigned int read32(unsigned int *addr)
{
	return ioread32((void __iomem *)addr);
}

static void aat_write(unsigned long  base, unsigned long  val, unsigned long  width)
{
	switch(width)
	{
	case 4L:
		mt_reg_sync_writel(val, base );
		break;
	case 2L:
		mt_reg_sync_writew(val, base);
		break;
	default:
		mt_reg_sync_writeb(val, base);
		break;
	}
}

static int dump_line_with_8(unsigned long addr, char buf[], int num, int size)
{
	int i, ch_in_line = 0;
	unsigned char tmp;
	unsigned long  tag_addr = addr - ((unsigned long )aat_runtime_map_base_vir) + ((unsigned long )aat_runtime_map_base_phy);

	ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "[0x%016lX] ", tag_addr);
	for(i=0; i<num; i++) {
		tmp = read8((unsigned char *)(addr+i));
		ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "%02x ", tmp);
	}
	ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "\n");
	return ch_in_line;
}

static int dump_line_with_16(unsigned long  addr, char buf[], int num, int size)
{
	int i, ch_in_line = 0;
	unsigned short tmp;
	unsigned long  tag_addr = addr - ((unsigned long )aat_runtime_map_base_vir) + ((unsigned long )aat_runtime_map_base_phy);

	ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "[0x%016lX] ", tag_addr);
	for(i=0; i<num; i+=2) {
		tmp = read16((unsigned short *)(addr+i));
		ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "%04x ", tmp);
	}
	ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "\n");
	return ch_in_line;
}

static int dump_line_with_32(unsigned long  addr, char buf[], int num, int size)
{
	int i, ch_in_line = 0;
	unsigned int tmp;
	unsigned long  tag_addr = addr - ((unsigned long )aat_runtime_map_base_vir) + ((unsigned long )aat_runtime_map_base_phy);

	ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "[0x%016lX] ", tag_addr);

	for(i=0; i<num; i+=4) {
		tmp = read32((unsigned int *)(addr+i));
		ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "%08x ", tmp);
	}
	ch_in_line += snprintf(&buf[ch_in_line], size - ch_in_line, "\n");
	return ch_in_line;
}
static int runtime_reg_dump(unsigned long  start_addr, unsigned int size, unsigned int access_width, char out_buf[])
{
	unsigned int i, buf_idx, dump_num_in_line, has_dumped;
	int (*rd_func)(unsigned long , char*, int, int);
	unsigned long  curr_addr;

	if(size > 4096)
		size = 4096;

	switch(access_width)
	{
	case 4:
		rd_func = dump_line_with_32;
		size = ((size+3)&(~0x3));
		break;
	case 2:
		rd_func = dump_line_with_16;
		size = ((size+1)&(~0x1));
		break;
	default:// means 1
		rd_func = dump_line_with_8;
		break;
	}

	curr_addr = start_addr;
	buf_idx = 0;
	dump_num_in_line = 0;
	has_dumped = 0;

	for(i=0; i<size; i+= 16) {
		dump_num_in_line = size-has_dumped;
		if(dump_num_in_line > 16)
			dump_num_in_line = 16;
		buf_idx += rd_func(curr_addr, &out_buf[buf_idx], dump_num_in_line, 4096-buf_idx);
		curr_addr += dump_num_in_line;
		has_dumped += dump_num_in_line;
	}

	return buf_idx;
}
static int command_parser(const char *input_str, char cmd[], unsigned long  out_put[])
{
	int i=0, j, k;
	char tmp_buf[32];
	char filter_str[256];

	// Filter out 0xD and 0xA
	j=0;
	i=0;
	while((i<256)&&(input_str[i]!='\0')) {
		if((input_str[i] == 0xD)||(input_str[i] == 0xA)) {
			i++;
			continue;
		}
		filter_str[j] = input_str[i];
		i++;
		j++;
	}
	filter_str[j] = '\0';

	i=0;
	// Parse command
	if(filter_str[i] == '\0')
		return 0;
	j=0;
	while(filter_str[i] != '\0') {
		if(filter_str[i] != '_') {
			cmd[j] = filter_str[i];
			j++;
			i++;
		} else {
			i++;
			break;
		}
	}
	cmd[j] = '\0';

	// Parse parameters
	for(k=0; k<4; k++) {
		if(filter_str[i] == '\0')
			return k+1;
		j=0;
		while(filter_str[i] != '\0') {
			if(filter_str[i] != '_') {
				tmp_buf[j] = filter_str[i];
				j++;
				i++;
			} else {
				i++;
				break;
			}
		}
		tmp_buf[j] = '\0';
		// change string to number
		sscanf(tmp_buf, "%lX", &out_put[k]);
	}

	return k+1;
}

extern void spm_ap_mdsrc_req(unsigned char);
static void cmd_process(char cmd[], unsigned long  para[], int para_num)
{
	if((para_num == 2)&&(strcmp(cmd, "IOR")==0)) {
		CCCI_UTIL_DBG_MSG("Command:%s, Base addr:0x%p\n", cmd, (void*)para[0]);
		// Do IO-REMAP here
		if(aat_runtime_map_base_vir == NULL) {
			aat_runtime_map_base_vir = ioremap_nocache((phys_addr_t)para[0], 4096);
			if(aat_runtime_map_base_vir <=0 ) {
				snprintf(aat_err_str, 64, "Map phy addr:0x%p fail\n", (void*)para[0]);
				aat_err_no = 0x00002100;
			} else {
				snprintf(aat_err_str, 64, "Map phy addr:0x%p OK\n", (void*)para[0]);
				aat_err_no = 0x00001000;
				aat_runtime_map_base_phy = (phys_addr_t)para[0];
			}
		} else {
			snprintf(aat_err_str, 64, "Please Un-map phy addr:0x%pa\n", &aat_runtime_map_base_phy);
			aat_err_no = 0x00002100;
		}
		aat_show_case = 0; //Error info
	} else if((para_num == 4)&&(strcmp(cmd, "WR")==0)) {
		CCCI_UTIL_DBG_MSG("Command:%s, Offset:0x%p, value:0x%p, type:0x%p\n", cmd, (void*)para[0], (void*)para[1], (void*)para[2]);
		// Do wirte operation here
		if(aat_runtime_map_base_vir != NULL) {
			if(para[0] >= 4096L) {
				snprintf(aat_err_str, 64, "Offset should less than 4096\n");
				aat_err_no = 0x00002100;
			} else {
				aat_write( ((unsigned long )aat_runtime_map_base_vir)+para[0], para[1], para[2]);
				snprintf(aat_err_str, 64, "Write phy addr:0x%p OK\n", (void*)((unsigned long)aat_runtime_map_base_phy+para[0]));
				aat_err_no = 0x00001000;
			}
		} else {
			snprintf(aat_err_str, 64, "Please do io map first\n");
			aat_err_no = 0x00002100;
		}
		aat_show_case = 0; //Error info
	} else if((para_num == 4)&&(strcmp(cmd, "QUERY")==0)) {
		CCCI_UTIL_DBG_MSG("Command:%s, Offset:0x%p, value:0x%p, type:0x%p\n", cmd, (void*)para[0], (void*)para[1], (void*)para[2]);
		// Do read operation here
		if(aat_runtime_map_base_vir != NULL) {
			if(para[0] >= 4096L) {
				snprintf(aat_err_str, 64, "Offset should less than 4096\n");
				aat_err_no = 0x00002100;
				aat_show_case = 0; //Error info
			} else if((para[0]+para[1]) > 4096L) {
				snprintf(aat_err_str, 64, "Offset + Size should less than 4096\n");
				aat_err_no = 0x00002100;
				aat_show_case = 0; //Error info
			} else {
				snprintf(aat_err_str, 64, "Query offset:0x%p size:0x%p OK\n", (void*)para[0], (void*)para[1]);
				ast_dump_start_addr = (unsigned long)aat_runtime_map_base_vir + para[0];
				ast_dump_size = para[1];
				ast_dump_width = para[2];
				aat_err_no = 0x00001000;
				aat_show_case = 1; //Dump data
			}
		} else {
			snprintf(aat_err_str, 64, "Please do io map first\n");
			aat_err_no = 0x00002100;
			aat_show_case = 0; //Error info
		}
	} else if((para_num == 1)&&(strcmp(cmd, "IOU")==0)) {
		CCCI_UTIL_DBG_MSG("Command:%s\n", cmd);
		// Do IO-UNMAP here
		if(aat_runtime_map_base_vir != NULL) {
			iounmap(aat_runtime_map_base_vir);
			aat_runtime_map_base_vir = NULL;
			snprintf(aat_err_str, 64, "IO Un-map phy addr:0x%pa OK\n", &aat_runtime_map_base_phy);
			aat_err_no = 0x00001000;
		} else {
			snprintf(aat_err_str, 64, "No need Un-map\n");
			aat_err_no = 0x00001000;
		}
		aat_show_case = 0; //Error info
	} else if((para_num == 1)&&(strcmp(cmd, "MDFU")==0)) { //MD Force Unlock
		CCCI_UTIL_DBG_MSG("Command:%s\n", cmd);
		// Do Un-lock operation here, unlock sleep/clock here
		snprintf(aat_err_str, 64, "Force MD un-lock\n");
#ifndef FEATURE_FPGA_PORTING		
		spm_ap_mdsrc_req(0);
#endif
		aat_err_no = 0x00001000;
		aat_show_case = 0; //Error info
	} else if((para_num == 1)&&(strcmp(cmd, "MDFL")==0)) { //MD Force Lock
		CCCI_UTIL_DBG_MSG("Command:%s\n", cmd);
		// Do lock operation here, unlock sleep/clock here
		snprintf(aat_err_str, 64, "Force MD lock\n");
#ifndef FEATURE_FPGA_PORTING
		spm_ap_mdsrc_req(1);
#endif
		aat_err_no = 0x00001000;
		aat_show_case = 0; //Error info
	}else {
		CCCI_UTIL_ERR_MSG("In-valid command:%s and parameter number:%d\n", cmd, para_num);
		snprintf(aat_err_str, 64, "In-valid command:%s and parameter number:%d\n", cmd, para_num);
		aat_show_case = 0; //Error info
	}
}

static ssize_t aat_show(char *buf)
{
	unsigned int curr = 0;
	unsigned int dump_size;

	curr = snprintf(buf, 4096, "0x%08x: %s\n", (unsigned int)aat_err_no, aat_err_str);
	
	if(aat_show_case == 1) {
		dump_size = 4096 - curr;
		if(dump_size > ((unsigned int)ast_dump_size))
			dump_size = (unsigned int)ast_dump_size;
		curr += runtime_reg_dump(ast_dump_start_addr, dump_size, (unsigned int)ast_dump_width, &buf[curr]);
	}

	return (ssize_t)curr;
}

static ssize_t aat_store(const char *buf, size_t count)
{
	int para_num = command_parser(buf, aat_cmd, aat_para);
	cmd_process(aat_cmd, aat_para, para_num);
	return count;
}

CCCI_ATTR(aat, 0600, &aat_show, &aat_store);

static ssize_t kcfg_setting_show(char *buf)
{
	unsigned int curr = 0;
	unsigned int actual_write;
	char md_en[MAX_MD_NUM];
	unsigned int md_num = 0;
	int i;

	for(i=0; i<MAX_MD_NUM; i++) {
		if(get_modem_is_enabled(MD_SYS1+i)) {
			md_num++;
			md_en[i] = '1';
		} else
			md_en[i] = '0';
	}
	// MD enable setting part
	actual_write = snprintf(&buf[curr], 4096-16-curr, "[modem num]:%d\n", md_num); // Reserve 16 byte to store size info
	curr+= actual_write;
	actual_write = snprintf(&buf[curr], 4096-16-curr, "[modem en]:%c-%c-%c-%c-%c\n", md_en[0], md_en[1], md_en[2], md_en[3], md_en[4]); // Reserve 16 byte to store size info
	curr+= actual_write;

	// Feature option part
	#ifdef CONFIG_EVDO_DT_SUPPORT
	actual_write = snprintf(&buf[curr], 4096-curr, "[EVDO_DT_SUPPORT]:1\n");
	#else
	actual_write = snprintf(&buf[curr], 4096-curr, "[EVDO_DT_SUPPORT]:0\n");
	#endif
	curr+= actual_write;
	#ifdef CONFIG_MTK_LTE_DC_SUPPORT
	actual_write = snprintf(&buf[curr], 4096-curr, "[MTK_LTE_DC_SUPPORT]:1\n");
	#else
	actual_write = snprintf(&buf[curr], 4096-curr, "[MTK_LTE_DC_SUPPORT]:0\n");
	#endif
	curr+= actual_write;

	// Add total size to tail
	actual_write = snprintf(&buf[curr], 4096-curr, "total:%d\n", curr);
	curr+= actual_write;

	CCCI_UTIL_INF_MSG("cfg_info_buffer size:%d\n", curr);
	return (ssize_t)curr;
}

static ssize_t kcfg_setting_store(const char *buf, size_t count)
{
	return count;
}

CCCI_ATTR(kcfg_setting, 0444, &kcfg_setting_show, &kcfg_setting_store);

// Sys -- Add to group
static struct attribute *ccci_default_attrs[] = {
	&ccci_attr_boot.attr,
	&ccci_attr_version.attr,
	&ccci_attr_md_en.attr,
	&ccci_attr_debug.attr,
	&ccci_attr_aat.attr,
	&ccci_attr_kcfg_setting.attr,
	NULL
};

static struct kobj_type ccci_ktype = {
	.release	= ccci_obj_release,
	.sysfs_ops 	= &ccci_sysfs_ops,
	.default_attrs 	= ccci_default_attrs
};

int ccci_sysfs_add_modem(int md_id, void *kobj, void *ktype, get_status_func_t get_sta_func, boot_md_func_t boot_func)
{
	int ret;
	static int md_add_flag = 0;
	
	if(!ccci_sys_info) {
		CCCI_UTIL_ERR_MSG("common sys not ready\n");
		return -CCCI_ERR_SYSFS_NOT_READY;
	}

	if(md_add_flag & (1<<md_id)) {
		CCCI_UTIL_ERR_MSG("md%d sys dup add\n", md_id+1);
		return -CCCI_ERR_SYSFS_NOT_READY;
	}

	ret = kobject_init_and_add((struct kobject *)kobj, (struct kobj_type *)ktype, &ccci_sys_info->kobj, "mdsys%d", md_id+1);
	if (ret < 0) {
		kobject_put(kobj);
		CCCI_UTIL_ERR_MSG_WITH_ID(md_id, "fail to add md kobject\n");
	}else{
		md_add_flag |= (1<<md_id);
		get_status_func[md_id] = get_sta_func;
		boot_md_func[md_id] = boot_func;
	}

	return ret;
}

int ccci_common_sysfs_init(void)
{
	int ret = 0;
	int i;

	ccci_sys_info = kmalloc(sizeof(struct ccci_info), GFP_KERNEL);
	if (!ccci_sys_info)
		return -ENOMEM;

	memset(ccci_sys_info, 0, sizeof(struct ccci_info));

	ret = kobject_init_and_add(&ccci_sys_info->kobj, &ccci_ktype, kernel_kobj, CCCI_KOBJ_NAME);
	if (ret < 0) {
		kobject_put(&ccci_sys_info->kobj);
		CCCI_UTIL_ERR_MSG("fail to add ccci kobject\n");
		return ret;
	}
	for(i=0; i<MAX_MD_NUM; i++) {
		get_status_func[i] = NULL;
		boot_md_func[i] = NULL;
	}

	ccci_sys_info->ccci_attr_count = ARRAY_SIZE(ccci_default_attrs)-1;
	CCCI_UTIL_DBG_MSG("ccci attr cnt %d\n", ccci_sys_info->ccci_attr_count);
	return ret;
}
