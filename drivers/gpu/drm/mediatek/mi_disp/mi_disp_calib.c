#include "mi_disp_calib.h"
struct panel_package_cmd_set gcmd;
static int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;
	int max_index = length/DATA_NUM;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			pr_err("format error\n");
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	};

	*cnt = count;
	pr_info("panel_send_cmds:dsi_panel_get_cmd_pkt_count:length = %d max_index:%d count = %d\n", length, max_index, count);
	return 0;
}

static int dsi_panel_alloc_cmd_packets(struct panel_package_cmd_set *cmd,
					u32 packet_count)
{
	u32 size;

	size = packet_count * sizeof(*cmd->cmds);
	cmd->cmds = kzalloc(size, GFP_KERNEL);
	if (!cmd->cmds)
		return -ENOMEM;

	cmd->count = packet_count;
	return 0;
}

static int dsi_panel_create_cmd_packets(const char *data,
					u32 length,
					u32 count,
					struct panel_package_dsi_cmd_desc *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;

	for (i = 0; i < count; i++) {
		u32 size;

		cmd[i].msg.tx_len = ((data[5] << 8) | (data[6]));
		cmd[i].post_wait_ms = data[4];

		size = cmd[i].msg.tx_len * sizeof(u8);
		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmd[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmd[i].msg.tx_buf = payload;
		data += (7 + cmd[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		kfree(cmd[i].msg.tx_buf);
		cmd[i].msg.tx_buf = NULL;
	}

	return rc;
}

static int panel_send_package_cmd_from_file(const char *data, u32 length, struct panel_package_cmd_set *cmd)
{
	int rc = 0;
	u32 packet_count = 0;

	if (!data) {
		pr_debug("panel_send_cmds: /data/panelon_cmd.txt is null\n");
		rc = -ENOTSUPP;
		goto error;
	}

	rc = dsi_panel_get_cmd_pkt_count(data, length, &packet_count);
	if (rc) {
		pr_err("panel_send_cmds: get cmd packets count failed, rc=%d\n", rc);
		goto error;
	}
	pr_info("panel_send_cmds: packet-count:%d, data length:%d\n", packet_count, length);

	rc = dsi_panel_alloc_cmd_packets(cmd, packet_count);
	if (rc) {
		pr_err("panel_send_cmds: failed to allocate cmd packets, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_create_cmd_packets(data, length, packet_count,
					  cmd->cmds);
	if (rc) {
		pr_err("panel_send_cmds: failed to create cmd packets, rc=%d\n", rc);
		goto error_free_mem;
	}

	return rc;
error_free_mem:
	kfree(cmd->cmds);
	cmd->cmds = NULL;
error:
	return rc;

}

static int ch_check(char ch)
{
	int rc = -1;
	if ((ch >= '0' && ch <= '9')
		|| (ch >= 'A' && ch <= 'F')
		|| (ch >= 'a' && ch <= 'f'))
		rc = 0;

	return rc;
}

int mi_read_initcode(void)
{
	int calib_status = 0;
	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos = 0;
	int i = 0;
	char ch[3] = {0};
	struct buf_data init_data;
	int count = 0;
	int rc = 0;

	fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open("/mnt/vendor/persist/display/display_calib.mcr", O_RDWR, 0644);
	if (IS_ERR_OR_NULL(fp)) {
		calib_status = 1;
		fp = filp_open("/mnt/vendor/persist/display/display_calib_default.mcr", O_RDWR, 0644);
		if (IS_ERR_OR_NULL(fp)) {
			rc = PTR_ERR(fp);
			pr_err("filp_open(/mnt/vendor/persist/display/display_calib_default.mcr) fail, rc = %d\n", rc);
			goto error;
		} else {
			pr_err("filp_open(/mnt/vendor/persist/display/display_calib_default.mcr) success\n");
		}
	} else {
		pr_info("filp_open(/mnt/vendor/persist/display/display_calib.mcr) success\n");
	}
	init_data.data = kzalloc(DATA_NUM, GFP_KERNEL);
	while (vfs_read(fp, &ch[0], 1, &pos) > 0) {

		if (ch_check(ch[0])) {
			continue;
		}
		count = vfs_read(fp, &ch[1], 1, &pos);
		pr_debug("panel_send_cmds:count = %ld pos = %ld ch[1] = %c\n", count, pos, ch[1]);
		if (ch_check(ch[1])) {
			continue;
		}
		if (i < DATA_NUM)
			kstrtou8(ch, 16, &init_data.data[i]);
		else{
			pr_err("panel_send_cmds:cmd num over DATA_NUM = %d\n", i);
			break;
		}

		i++;
	}
	init_data.length = i;
	pr_info("panel_send_cmds: begin data:%d end data:%d init_data.length = %d\n", init_data.data[0], init_data.data[i-1], init_data.length);

	rc = panel_send_package_cmd_from_file(init_data.data, init_data.length, &gcmd);


	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}

	set_fs(fs);
	kfree(init_data.data);
error:
	return calib_status;
}

