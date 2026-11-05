#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "boot_monitor.h"
#include "boot_fail.h"

struct PACK(log_table_header) black_box_log_table_header;
struct PACK(log_error_type_header) error_type_header;
struct PACK(log_error_header) error_header;
struct PACK (log_boot_monitor_header) boot_monitor_header;
struct block_device *bdev = NULL;
struct mutex write_bm_mutex;

static void add_timeout_header(char *oops_buf);

static int version_change = 0;
static int bootindex= 0;
static char before_fingerprint[MAX_CMDLINE_PARAM_LEN] = {0};

static char bm_devices_name[128] = {0};
module_param_string(devname, bm_devices_name, 128, 0644);

/*add device for write log to  bootfail partition*/
static struct page *page_read(struct address_space *mapping, pgoff_t index)
{
	return read_mapping_page(mapping, index, NULL);
}

int _partition_bm_read(struct block_device *dev, loff_t from,
		size_t len, void *buf)
{
	struct page *page;
	pgoff_t index = from >> PAGE_SHIFT;
	int offset = from & (PAGE_SIZE-1);
	int cpylen;
	u_char *ubuf = (u_char *)buf;

	while (len) {
		if ((offset + len) > PAGE_SIZE)
			cpylen = PAGE_SIZE - offset;	// multiple pages
		else
			cpylen = len;	// this page
		len = len - cpylen;

		page = page_read(dev->bd_inode->i_mapping, index);
		if (IS_ERR(page))
			return PTR_ERR(page);

		memcpy(ubuf, page_address(page) + offset, cpylen);
		put_page(page);

		ubuf += cpylen;
		offset = 0;
		index++;
	}
	return 0;
}

static int _partition_write(struct block_device *dev, const void *buf,
		loff_t to, size_t len)
{
	struct page *page;
	struct address_space *mapping = dev->bd_inode->i_mapping;
	pgoff_t index = to >> PAGE_SHIFT;	// page index
	int offset = to & ~PAGE_MASK;	// page offset
	int cpylen;
	u_char *ubuf = (u_char *)buf;

	while (len) {
		if ((offset+len) > PAGE_SIZE)
			cpylen = PAGE_SIZE - offset;	// multiple pages
		else
			cpylen = len;			// this page
		len = len - cpylen;

		page = page_read(mapping, index);
		if (IS_ERR(page))
			return PTR_ERR(page);

		if (memcmp(page_address(page)+offset, ubuf, cpylen)) {
			lock_page(page);
			memcpy(page_address(page) + offset, ubuf, cpylen);
			set_page_dirty(page);
			unlock_page(page);
			balance_dirty_pages_ratelimited(mapping);
		}
		put_page(page);

		ubuf += cpylen;
		offset = 0;
		index++;
	}
	return 0;
}

int partition_bm_write(loff_t to, size_t len, const void *buf)
{
	int err;

	err = -1;
	if (IS_ERR(bdev) || bdev == NULL) {
		MTN_PRINT_ERR("%s-%d:failed:IS_ERR(bdev)\n", __func__, __LINE__);
		return err;
	}
	if (buf == NULL) {
		MTN_PRINT_ERR("%s-%d:buf is NULL\n", __func__, __LINE__);
		return err;
	}
	mutex_lock(&write_bm_mutex);
	err = _partition_write(bdev, buf, to, len);
	mutex_unlock(&write_bm_mutex);
	if (err > 0)
		err = 0;

	sync_blockdev(bdev);
	return err;
}

static inline void kill_final_newline(char *str)
{
	char *newline = strrchr(str, '\n');
	if (newline && !newline[1])
		*newline = 0;
}

int get_bm_devices(void)
{
	int err;
	int i;
	const fmode_t mode = BLK_OPEN_READ | BLK_OPEN_WRITE;
	struct address_space *mapping;
	char buf[128];
	char *str = buf;

	err = -1;

	if (strnlen(bm_devices_name, sizeof(buf)) >= sizeof(buf)) {
		MTN_PRINT_ERR("%s-%d:parameter too long\n", __func__, __LINE__);
		return 0;
	}

	strcpy(str, bm_devices_name);
	kill_final_newline(str);

	/* Get a handle on the device */
	bdev = blkdev_get_by_path(str, mode, THIS_MODULE, NULL);
	MTN_PRINT_INFO("%s-%d:bm dev: str %s:%zu\n", __func__, __LINE__, str, strlen(str));

	/*
	 * We might not have the root device mounted at this point.
	 * Try to resolve the device name by other means.
	 */
	for (i = 0; IS_ERR(bdev) && i <= DEFAULT_TIMEOUT; i++) {
		MTN_PRINT_ERR("%s-%d:bm dev: i %d\n", __func__, __LINE__, i);
		if (i) {
			/*
			 * Calling wait_for_device_probe in the first loop
			 * was not enough, sleep for a bit in subsequent
			 * go-arounds.
			 */
			msleep(1000);
		}
		wait_for_device_probe();

		bdev = blkdev_get_by_path(str, mode, THIS_MODULE, NULL);
		MTN_PRINT_ERR("%s-%d:blkdev_get_by_dev end !\n", __func__, __LINE__);
	}
	if (IS_ERR(bdev)) {
		MTN_PRINT_ERR("%s-%d: error: cannot open device %s\n",  __func__, __LINE__, str);
		return err;
	}

	/*dev wirte lock*/
	mutex_init(&write_bm_mutex);

	err = 0;
	mapping = bdev->bd_inode->i_mapping;

	return err;
}

static void initerrortypeheader(void) {
	error_type_header.magic = LOG_ERROR_TYPE_HEADER_MAGIC;
	error_type_header.version = BOOTFAIL_VERSION;
	error_type_header.header_size = HEADER_SIZE_4K;
	error_type_header.uefi_log_offset = UEFI_LOG_OFFSET;
	error_type_header.boot_index = 0;
	error_type_header.error_num = CRASH_MAX - 1;
	error_type_header.error_header_size = HEADER_SIZE_4K;
	error_type_header.current_error = 0;
	memset(error_type_header.error_record, 0, sizeof(error_type_header.error_record));
	memset(error_type_header.stage, 0, sizeof(error_type_header.stage));
	error_type_header.status = true;
}

static void initerrorheader(void) {
	error_header.magic = LOG_ERROR_HEADER_MAGIC;
	error_header.version = ERRORTYPE_VERSION;
	error_header.header_size = HEADER_SIZE_4K;
	error_header.boot_index = 0;
	error_header.error_type = 0;
	error_header.current_error = 0;
	error_header.error_current = false;
	memset(error_header.error_record, 0, sizeof(error_header.error_record));
}

static void initbootheader(void) {
	boot_monitor_header.magic = LOG_MONITOR_HEADER_MAGIC;
	boot_monitor_header.version = BOOT_MONITOR_VERSION;
	boot_monitor_header.boot_index = 0;
	boot_monitor_header.version_change = 0;
	memset(boot_monitor_header.os_version, 0, sizeof(boot_monitor_header.os_version));
	memset(boot_monitor_header.event, 0, sizeof(boot_monitor_header.event));
	boot_monitor_header.status = true;
}
/*print blackbox partition header*/
/*static void printlogtableheader(void) {
	MTN_PRINT_INFO("log_table_header :magic=0x%lx\n"
                                    "version=%u\n"
                                    "header_size=%u\n"
                                    "boot_index=%u\n"
                                    "log_content_offset=%u\n"
                                    "minidump_offset=%u\n"
                                    "minidump_is_valid=%u\n"
                                    "control_content_offset=0x%x\n"
                                    "boot_content_offset=0x%x\n",
       black_box_log_table_header.magic, black_box_log_table_header.version, black_box_log_table_header.header_size, \
	   black_box_log_table_header.boot_index, black_box_log_table_header.log_content_offset, \
       black_box_log_table_header.minidump_offset, black_box_log_table_header.minidump_is_valid, \
	   black_box_log_table_header.control_content_offset, black_box_log_table_header.boot_content_offset);
}*/

/*static void printerrortypeheader(void) {
	MTN_PRINT_INFO("error_type_header:magic=0x%lx\n"
                                    "version=%u\n"
                                    "header_size=%u\n"
                                    "uefi_log_offset=0x%lx\n"
                                    "boot_index=%u\n"
                                    "error_num=%u\n"
                                    "error_header_size=%u\n"
                                    "current_error=%lu\n"
                                    "error_record=%s\n"
                                    "stage=%s\n",
       error_type_header.magic, error_type_header.version, error_type_header.header_size, \
	   error_type_header.uefi_log_offset, error_type_header.boot_index, \
       error_type_header.error_num, error_type_header.error_header_size, \
	   error_type_header.current_error, error_type_header.error_record, error_type_header.stage);
}*/

/*static void printerrorheader(void) {
	MTN_PRINT_INFO("error_header     :magic=0x%lx\n"
                                    "version=%u\n"
                                    "header_size=%u\n"
                                    "boot_index=%u\n"
                                    "error_type=%lu\n"
                                    "current_error=%u\n"
                                    "error_current=%d\n"
                                    "error_record=%s\n",
       error_header.magic, error_header.version, error_header.header_size, error_header.boot_index, error_header.error_type, \
       error_header.current_error, error_header.error_current, error_header.error_record);
}*/

/*static void printbootheader(void) {
    MTN_PRINT_INFO("boot_monitor     :magic=0x%lx\n"
                                    "version=%u\n"
                                    "boot_index=%u\n"
                                    "version_change=%u\n"
                                    "os_version=%s\n"
                                    "event=%s\n"
                                    "status=%d\n",
        boot_monitor_header.magic, boot_monitor_header.version, boot_monitor_header.boot_index, \
		boot_monitor_header.version_change, boot_monitor_header.os_version, boot_monitor_header.event, boot_monitor_header.status);
}*/

int write_blackbox_header(struct bootmonitor_context *cxt, char *buffer, int event) {
	int err;
	int event_count;

	MTN_PRINT_START();
	err = -1;
	if (IS_ERR(bdev) || bdev == NULL) {
		MTN_PRINT_ERR("%s-%d:failed:IS_ERR(bdev)\n", __func__, __LINE__);
		return err;
	}

	if (event <= 2) {
		event_count = 1;
	} else {
		event_count = event - 1;
	}
	if (event_count >= BOITFAIL_MAX) {
		MTN_PRINT_ERR("%s-%d:event_count too large\n", __func__, __LINE__);
		return err;
	}
	MTN_PRINT_INFO("%s-%d:event_count is %d\n", __func__, __LINE__, event_count);

	/*update black_box_log_table_header*/
	err = _partition_bm_read(bdev, BLACK_BOX_OFFSET, sizeof(struct log_table_header), &black_box_log_table_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:_partition_bm_read fail\n", __func__, __LINE__);
		return err;
	}
	if (black_box_log_table_header.magic != LOG_TABLE_HEADER_MAGIC) {
		MTN_PRINT_INFO("%s-%d:black_box_log_table_header.magic error\n", __func__, __LINE__);
		return -1;
	}
	if (black_box_log_table_header.boot_content_offset != BOOT_CONTENT_OFFSET) {
		black_box_log_table_header.boot_content_offset = BOOT_CONTENT_OFFSET;
		MTN_PRINT_INFO("%s-%d:black_box_log_table_header.boot_content_offset=0x%x\n", __func__, __LINE__, black_box_log_table_header.boot_content_offset);
	}
	//printlogtableheader();
	err = partition_bm_write(BLACK_BOX_OFFSET, sizeof(struct log_table_header), &black_box_log_table_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write fail\n", __func__, __LINE__);
		return err;
	}
	/*add bootindex in log title*/
	bootindex = black_box_log_table_header.boot_index;

	/*update error_type_header*/
	err = _partition_bm_read(bdev, black_box_log_table_header.boot_content_offset, sizeof(struct log_error_type_header), &error_type_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:_partition_bm_read fail\n", __func__, __LINE__);
		return err;
	}
	if (error_type_header.magic != LOG_ERROR_TYPE_HEADER_MAGIC) {
		MTN_PRINT_INFO("%s-%d:error_type_header.magic error\n", __func__, __LINE__);
		initerrortypeheader();
	}
	error_type_header.error_num = CRASH_MAX - 1;
	error_type_header.boot_index = black_box_log_table_header.boot_index;
	error_type_header.current_error = error_init_type_table[event_count];
	error_type_header.status = false;
	strncpy(error_type_header.stage, CURRENT_BOOT_STAGE_INIT, sizeof(error_type_header.stage) - 1);
	error_type_header.stage[sizeof(error_type_header.stage) - 1] = '\0';
	strncpy(error_type_header.error_record, CURRENT_BOOT_INIT, sizeof(error_type_header.error_record) - 1);
	error_type_header.error_record[sizeof(error_type_header.error_record) - 1] = '\0';

	/*update error_header*/
	err = _partition_bm_read(bdev, black_box_log_table_header.boot_content_offset + error_type_header.header_size + INIT_INDEX * HEADER_SIZE_4K,
	                        sizeof(struct log_error_header), &error_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:_partition_bm_read faild\n", __func__, __LINE__);
		return err;
	}
	if (error_header.magic != LOG_ERROR_HEADER_MAGIC) {
		MTN_PRINT_INFO("%s-%d:error_header.magic error\n", __func__, __LINE__);
		initerrorheader();
	}
	error_header.error_type = error_init_type_table[event_count];
	error_header.current_error += 1;
	error_header.boot_index = black_box_log_table_header.boot_index;
	error_header.error_current = true;
	strncpy(error_header.error_record, bm_boot_events[bm_event_counts].name, sizeof(error_header.error_record) - 1);
	error_header.error_record[sizeof(error_header.error_record) - 1] = '\0';
	//printerrorheader();
	err = partition_bm_write(black_box_log_table_header.boot_content_offset + error_type_header.header_size + (INIT_INDEX + event_count)* HEADER_SIZE_4K,
                            sizeof(struct log_error_header), &error_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write fail\n", __func__, __LINE__);
		return err;
	}

	/*update boot_monitor_header*/
	err = _partition_bm_read(bdev, black_box_log_table_header.boot_content_offset + PARTITION_LOG_OFFSET,
	                        sizeof(struct log_boot_monitor_header), &boot_monitor_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:_partition_bm_read fail\n", __func__, __LINE__);
		return err;
	}
	if (boot_monitor_header.magic != LOG_MONITOR_HEADER_MAGIC) {
		MTN_PRINT_INFO("%s-%d:boot_monitor_header.magic\n", __func__, __LINE__);
		initbootheader();
		/*for blackbox-bootfail init, the version is change*/
		version_change = 1;
		boot_monitor_header.version_change = version_change;
		/*record boefor version(null)*/
		strncpy(before_fingerprint, BEFORE_VERSION, sizeof(before_fingerprint) - 1);
		before_fingerprint[sizeof(before_fingerprint) - 1] = '\0';
	} else {
		/*record version change status*/
		MTN_PRINT_INFO("build_fingerprint=%s, boot_monitor_header.os_version=%s\n", build_fingerprint, boot_monitor_header.os_version);
		if (strcmp(build_fingerprint, boot_monitor_header.os_version) == 0)
			version_change = 0;
		else
			version_change = 1;
		boot_monitor_header.version_change = version_change;
		/*record boefor version*/
		strncpy(before_fingerprint, boot_monitor_header.os_version, sizeof(boot_monitor_header.os_version) - 1);
		before_fingerprint[sizeof(before_fingerprint) - 1] = '\0';
	}
	boot_monitor_header.boot_index = black_box_log_table_header.boot_index;
	/*record this version*/
	strncpy(boot_monitor_header.os_version, build_fingerprint, sizeof(boot_monitor_header.os_version) - 1);
	boot_monitor_header.os_version[sizeof(boot_monitor_header.os_version) - 1] = '\0';
	/*record timeout event*/
	strncpy(boot_monitor_header.event, bm_boot_events[bm_event_counts].name, sizeof(boot_monitor_header.event) - 1);
	boot_monitor_header.event[sizeof(boot_monitor_header.event) - 1] = '\0';
	boot_monitor_header.status = false;
	if (version_change == 1) {
		error_type_header.status = true;
		boot_monitor_header.status = true;
	}
	//printerrortypeheader();
	err = partition_bm_write(black_box_log_table_header.boot_content_offset, sizeof(struct log_error_type_header), &error_type_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write fail\n", __func__, __LINE__);
		return err;
	}

	//printbootheader();
	err = partition_bm_write(black_box_log_table_header.boot_content_offset + PARTITION_LOG_OFFSET,
	                        sizeof(struct log_boot_monitor_header), &boot_monitor_header);
	if (err != 0) {
		MTN_PRINT_ERR("%s-%d:partition_bm_write fail\n", __func__, __LINE__);
		return err;
	}
	/*add title in log*/
	add_timeout_header(buffer);
	add_timeout_header(cxt->oops_buf);
	MTN_PRINT_END();
	return err;
}

/*get kmsg and pmsg*/
static void add_timeout_header(char *oops_buf) {
    char str_buf[BOOT_MONITOR_HEADER_SIZE] = {0};
	int ret_len;
	unsigned long local_time;
	struct timespec64 now;
	struct tm ts;

	ret_len = 0;
	MTN_PRINT_START();
	ktime_get_coarse_real_ts64(&now);
	/*set title time to UTC+8*/
	local_time = (unsigned long)(now.tv_sec + 8 * 60 * 60);
	time64_to_tm(local_time, 0, &ts);
    ret_len = snprintf(str_buf, BOOT_MONITOR_HEADER_SIZE,
			"\n```\n## BOOT INDEX:%d\n## BOOT EVENTS:%s\n## BOOT MODE:%s\n##CURRENT VEISION:%s\n##BEFORE VEISION:%s\n"
			"## VEISION_CHANGE:%d\n##### %04ld-%02d-%02d %02d:%02d:%02d\n```c\n",
            bootindex, bm_boot_events[bm_event_counts].name, bm_boot_mode, build_fingerprint, before_fingerprint, version_change, \
			ts.tm_year+1900, ts.tm_mon + 1, ts.tm_mday, \
			ts.tm_hour, ts.tm_min, ts.tm_sec);

	if (ret_len >= sizeof(str_buf))
        	ret_len = sizeof(str_buf);

    memcpy(oops_buf, str_buf, ret_len);
	MTN_PRINT_END();
}

void monitor_get_kmsg(char *buffer) {
    struct kmsg_dump_iter iter;
    size_t ret_len = 0;

	kmsg_dump_rewind(&iter);
	MTN_PRINT_START();
	kmsg_dump_get_buffer(&iter, true,
			     buffer + BOOT_MONITOR_HEADER_SIZE,
			     PARTITION_KMSG_SIZE - HEADER_SIZE_4K - BOOT_MONITOR_HEADER_SIZE, &ret_len);
	MTN_PRINT_ERR("ret_len is %zu\n", ret_len);
	MTN_PRINT_END();
}

void monitor_get_pmsg(struct bootmonitor_context *cxt) {
    char *pmsg_buffer_start = NULL;
	struct pmsg_buffer_hdr *p_hdr = NULL;

	MTN_PRINT_START();
	pmsg_buffer_start = phys_to_virt(
		(cxt->monitor_data.mem_address + cxt->monitor_data.mem_size)-
		cxt->monitor_data.pmsg_size);

	p_hdr = (struct pmsg_buffer_hdr *)pmsg_buffer_start;

	if (p_hdr->sig == 0x43474244) {
		void *oopsbuf = cxt->oops_buf + BOOT_MONITOR_HEADER_SIZE;
		uint8_t *p_buff_end = (uint8_t *)p_hdr->data + atomic_read(&p_hdr->size);
		int pmsg_cp_size = 0;
		int pstart = p_hdr->start.counter;
		int psize = p_hdr->size.counter;

		pmsg_cp_size = PARTITION_PMSG_SIZE - BOOT_MONITOR_HEADER_SIZE;
		if (psize <= pmsg_cp_size)
			pmsg_cp_size = psize;

		if (pstart >= pmsg_cp_size)
			memcpy(oopsbuf, p_hdr->data, pmsg_cp_size);
		else {
			memcpy(oopsbuf, p_buff_end - (pmsg_cp_size - pstart),
					pmsg_cp_size - pstart);
			memcpy(oopsbuf + (pmsg_cp_size - pstart), p_hdr->data,
					pstart);
		}
	} else
	MTN_PRINT_INFO("mtdoops: read pmsg failed sig = 0x%x \n", p_hdr->sig);
	MTN_PRINT_END();
}

static int monitor_parse_dt_u32(struct platform_device *pdev,
				const char *propname,
				u32 default_value, u32 *value)
{
	u32 val32;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, propname, &val32);
	if (ret == -EINVAL) {
		/* field is missing, use default value. */
		val32 = default_value;
	} else if (ret < 0) {
		MTN_PRINT_ERR("failed to parse property %s: %d\n",
			propname, ret);
		return ret;
	}
	/* Sanity check our results. */
	if (val32 > INT_MAX) {
		MTN_PRINT_ERR("%s:%u > INT_MAX\n", propname, val32);
		return -EOVERFLOW;
	}
	*value = val32;
	return 0;
}

static int boot_monitor_probe(struct platform_device *pdev)
{
	struct bootmonitor_context *cxt;
	struct resource *res;
	u32 value;
	int ret;
	cxt = &boot_cxt;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		MTN_PRINT_ERR("failed to locate DT /reserved-memory resource\n");
		return -EINVAL;
	}
	cxt->monitor_data.mem_size = resource_size(res);
	cxt->monitor_data.mem_address = res->start;

#define parse_u32(name, field, default_value) {				\
		ret = monitor_parse_dt_u32(pdev, name, default_value,	\
					    &value);			\
		if (ret < 0)						\
			return ret;					\
		field = value;						\
	}

	parse_u32("console-size", cxt->monitor_data.console_size, 0);
	parse_u32("pmsg-size", cxt->monitor_data.pmsg_size, 0);

#undef parse_u32

	MTN_PRINT_INFO( "pares, mem_address =0x%llx, mem_size =0x%lx, console-size =0x%lx, pmsg-size =0x%lx\n",
			cxt->monitor_data.mem_address, cxt->monitor_data.mem_size, cxt->monitor_data.console_size, cxt->monitor_data.pmsg_size);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "xiaomi,bootmonitor_pmsg" },
	{}
};

struct platform_driver boot_monitor_driver = {
	.probe		= boot_monitor_probe,
	.driver		= {
		.name		= "bootmonitor_pmsg",
		.of_match_table	= dt_match,
	},
};
