// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/io.h>
#include "conap_scp.h"
#include "conap_scp_priv.h"
#include "conap_scp_shm.h"
#include "conap_platform_data.h"


#define SCIF_VERSION                       0x20200729
#define SCIF_RESET_KEYWORD                 0x5C1FDEAD
#define SCIF_HEADER_SIZE                   sizeof(struct scif_shm_header)
#define SCIF_CONTROL_SIZE                  sizeof(struct scif_control)
#define SCIF_SHM_GUARD_PATTERN             0x46494353    /* ASCII for "SCIF" */
#define SCIF_MSG_GUARD_PATTERN             0x5F504D47    /* ASCII for "GMP_" */
#define SCIF_MSG_MAX_LENGTH                3072

/* TODO: should be config by projects */

struct scif_shm_layout {
	struct scif_shm_header header;
	struct scif_control master;
	struct scif_control slave;
};

struct conap_scp_shm_info {
	unsigned int shm_phy_addr;
	unsigned int shm_size;
	void __iomem *shm_vir_addr;

	void __iomem *apd_shm_addr;
	struct scif_shm_layout *adp_shm_layout;

	unsigned int master_rbf_len;
	unsigned int slave_rbf_len;
	unsigned int *master_rbf;
	unsigned int *slave_rbf;
};

struct conap_scp_shm_info g_shm_info;
const unsigned int g_msg_hdr_sz = sizeof(struct scif_msg_header);

unsigned int conap_scp_shm_get_master_rbf_len(void)
{
	return g_shm_info.master_rbf_len;
}

unsigned int conap_scp_shm_get_slave_rbf_len(void)
{
	return g_shm_info.slave_rbf_len;
}

static void _memcpy(uint8_t* dst, uint8_t *src, uint32_t size)
{
	uint32_t *d = (uint32_t*)dst;
	uint32_t *s = (uint32_t*)src;
	uint32_t sz = size>>2;

	for (; sz > 0; sz--, s++, d++)
		*d = *s;
}


int conap_scp_shm_write_rbf(struct scif_msg_header *msg_header,
					uint8_t* buf, uint32_t size)
{
	struct scif_control *sctrl = NULL, *mctrl = NULL;
	uint32_t read_idx, write_idx, buf_len, old_write, bewrite;
	uint32_t cpsz1, cpsz2;
	uint8_t *rbf_addr;
	uint8_t *body_ptr;

	sctrl = &g_shm_info.adp_shm_layout->slave;
	mctrl = &g_shm_info.adp_shm_layout->master;
	read_idx = sctrl->rx_read_idx;
	write_idx = mctrl->tx_write_idx;
	buf_len = mctrl->tx_buf_len;
	rbf_addr = (uint8_t*)g_shm_info.master_rbf;

	old_write = write_idx;
	if (write_idx == buf_len)
		write_idx = 0;

	//pr_info("[%s] =a= master rbf=[%x] buf_len=[%d] widx=[%d] ridx=[%d]", __func__, g_shm_info.slave_rbf,
	//			buf_len, write_idx, read_idx);

	bewrite = write_idx + msg_header->msg_len;

	/* Queue full */
	if ((write_idx < read_idx && bewrite > read_idx) ||
		(bewrite > buf_len && (bewrite % buf_len) > read_idx)) {
		pr_warn("[conap_write_rbf] widx=[%d] ridx=[%d] msgLen=[%d]",
					write_idx, read_idx, msg_header->msg_len);
		return SCIF_ERR_QUEUE_FULL;
	}

	/* msg header */
	if (write_idx + g_msg_hdr_sz > buf_len) {
		cpsz1 = buf_len - write_idx;
		cpsz2 = g_msg_hdr_sz - cpsz1;
		_memcpy((uint8_t*)(rbf_addr + write_idx), (uint8_t*)msg_header, cpsz1);
		write_idx = 0;
		_memcpy((uint8_t*)(rbf_addr), ((uint8_t*)msg_header) + cpsz1, cpsz2);
		write_idx = cpsz2;
	} else {

		_memcpy((uint8_t*)(rbf_addr + write_idx), (uint8_t*)msg_header, g_msg_hdr_sz);
		write_idx += g_msg_hdr_sz;
	}

	/* msg body */
	if (buf) {
		if (write_idx + size > buf_len) {
			cpsz1 = buf_len - write_idx;
			cpsz2 = size - cpsz1;
			_memcpy((uint8_t*)(rbf_addr + write_idx), (uint8_t*)buf, cpsz1);
			write_idx = 0;
			_memcpy((uint8_t*)rbf_addr, buf + cpsz1, cpsz2);
			write_idx = cpsz2;
		} else {
			body_ptr = (uint8_t*)(rbf_addr + write_idx);

			_memcpy((uint8_t*)(rbf_addr + write_idx), buf, size);
			write_idx += size;
		}
	}

	mctrl->tx_write_idx = write_idx;

	//pr_info("[%s] =c= rbf_addr=[%x] widx=[%d] ridx=[%d] msg_len=[%d]", __func__,
	//		rbf_addr, write_idx, read_idx, msg_header->msg_len);
	return 0;
}

int conap_scp_shm_has_pending_data(struct scif_msg_header *header)
{
	struct scif_control *mctrl = NULL, *sctrl = NULL;
	uint32_t diff = 0;
	uint32_t ridx, widx, buf_len;
	uint32_t cpsz1, cpsz2;
	uint8_t *rbf_rptr;

	sctrl = &g_shm_info.adp_shm_layout->slave;
	mctrl = &g_shm_info.adp_shm_layout->master;

	ridx = mctrl->rx_read_idx;
	widx = sctrl->tx_write_idx;
	buf_len = sctrl->tx_buf_len;

	if (ridx == widx) {
		return 0;
	}

	//pr_info("pending_data w=[%x] r=[%x] txbuflen=[%d]\n", widx, ridx, sctrl->tx_buf_len);
	if (widx > sctrl->tx_buf_len || ridx > mctrl->tx_buf_len) {
		return SCIF_ERR_SHM_CORRUPTED;
	}

	if (widx == buf_len)
		widx = 0;
	if (ridx == buf_len)
		ridx = 0;

	rbf_rptr = (uint8_t*)g_shm_info.slave_rbf;

	if (ridx + g_msg_hdr_sz > buf_len) {
		cpsz1 = buf_len - ridx;
		cpsz2 = g_msg_hdr_sz - cpsz1;
		_memcpy((uint8_t*)header, rbf_rptr + ridx, cpsz1);
		_memcpy((uint8_t*)header + cpsz1, rbf_rptr, cpsz2);
	} else {
		memcpy_fromio((uint8_t*)header, ((uint8_t*)rbf_rptr) + ridx, g_msg_hdr_sz);
	}

	if (ridx == widx) {
		return 0;
	}

	if (ridx > widx)
		diff = buf_len - ridx + widx;
	else
		diff = widx - ridx;

	return diff;
}

int conap_scp_shm_shift_msg(struct scif_msg_header *header)
{
	struct scif_control *mctrl = NULL;
	uint32_t ridx, buf_len;

	mctrl = &g_shm_info.adp_shm_layout->master;
	buf_len = mctrl->tx_buf_len;
	ridx = mctrl->rx_read_idx;

	if (ridx + header->msg_len > buf_len)
		mctrl->rx_read_idx = ridx + header->msg_len - buf_len;
	else
		mctrl->rx_read_idx = ridx + header->msg_len;
	return 0;
}

int conap_scp_shm_collect_msg_body(struct scif_msg_header *header,
							uint32_t *buf, uint32_t sz)
{
	struct scif_control *sctrl = NULL, *mctrl = NULL;
	uint32_t msg_body_sz = header->msg_len - g_msg_hdr_sz;
	uint32_t buf_len, ridx;
	uint32_t *rbf = NULL;
	uint32_t cp_sz1, cp_sz2;
	uint8_t *wbuf_ptr = (uint8_t*)buf;

	sctrl = &g_shm_info.adp_shm_layout->slave;
	mctrl = &g_shm_info.adp_shm_layout->master;
	buf_len = sctrl->tx_buf_len;
	ridx = mctrl->rx_read_idx;
	rbf = (unsigned int*)g_shm_info.slave_rbf;

	if (ridx + g_msg_hdr_sz == buf_len)
		ridx = 0;
	else if (ridx + g_msg_hdr_sz > buf_len)
		ridx = g_msg_hdr_sz - (buf_len - ridx);
	else
		ridx += g_msg_hdr_sz;

	if (ridx + msg_body_sz > buf_len) {
		cp_sz1 = buf_len - ridx;
		cp_sz2 = msg_body_sz - cp_sz1;
		_memcpy(wbuf_ptr, (uint8_t*)rbf + ridx, cp_sz1);
		_memcpy(wbuf_ptr + cp_sz1, (uint8_t*)rbf, cp_sz2);
		mctrl->rx_read_idx = cp_sz2;
	} else {
		_memcpy(wbuf_ptr, (uint8_t*)rbf + ridx, msg_body_sz);
		mctrl->rx_read_idx = ridx + msg_body_sz;
	}

	return 0;
}

int conap_scp_shm_reset(struct conap_scp_shm_config* shm_config)
{
	struct scif_shm_layout *shm = NULL;
	struct scif_shm_header *header = NULL;
	unsigned int shm_size = SCIF_HEADER_SIZE
						+ SCIF_CONTROL_SIZE + SCIF_CONTROL_SIZE
						+ shm_config->conap_scp_shm_master_rbf_size
						+ shm_config->conap_scp_shm_slave_rbf_size;

	pr_info("[%s] header=[%d] control=[%d] shmsize=[%d]", __func__, SCIF_HEADER_SIZE, SCIF_CONTROL_SIZE, shm_size);

	shm = g_shm_info.adp_shm_layout;

	shm->header.pattern[0] = SCIF_SHM_GUARD_PATTERN;
    shm->header.pattern[1] = SCIF_SHM_GUARD_PATTERN;
    shm->header.version = SCIF_VERSION;
    shm->header.size = shm_size;
    shm->header.master_ctrl_offset = SCIF_HEADER_SIZE;
    shm->header.slave_ctrl_offset = SCIF_HEADER_SIZE + SCIF_CONTROL_SIZE;
    shm->header.master_rbf_offset = SCIF_HEADER_SIZE + (SCIF_CONTROL_SIZE * 2);
    shm->header.slave_rbf_offset = SCIF_HEADER_SIZE + (SCIF_CONTROL_SIZE * 2)
			+ shm_config->conap_scp_shm_master_rbf_size;

	// master control init
	shm->master.state = 0;
    shm->master.version = SCIF_VERSION;
    shm->master.tx_buf_len = shm_config->conap_scp_shm_master_rbf_size;
    shm->master.feature_set = 0;
    shm->master.tx_write_idx = 0;
    shm->master.rx_read_idx = 0;
    shm->master.reason = 0;
    shm->master.reserved = 0;

    // slave control init
	shm->slave.state = 0;
    shm->slave.version = SCIF_VERSION;
    shm->slave.tx_buf_len = shm_config->conap_scp_shm_slave_rbf_size;
	shm->slave.feature_set = 0;
    shm->slave.tx_write_idx = 0;
    shm->slave.rx_read_idx = 0;
    shm->slave.reason = 0;
    shm->slave.reserved = 0;

	header = &(g_shm_info.adp_shm_layout->header);

	g_shm_info.master_rbf = (unsigned int*)(g_shm_info.shm_vir_addr + header->master_rbf_offset);
	g_shm_info.slave_rbf = (unsigned int*)(g_shm_info.shm_vir_addr + header->slave_rbf_offset);
	g_shm_info.master_rbf_len = g_shm_info.adp_shm_layout->master.tx_buf_len;
	g_shm_info.slave_rbf_len = g_shm_info.adp_shm_layout->slave.tx_buf_len;

	pr_info("[%s] virtal=[%x] master=[%x] slave_rbf=[%x]", __func__,
				g_shm_info.shm_vir_addr, g_shm_info.master_rbf, g_shm_info.slave_rbf);
	return 0;
}

int conap_scp_shm_init(phys_addr_t emi_phy_addr)
{
	int ret;
	struct scif_shm_layout *adp_shm = NULL;
	struct conap_scp_shm_config *shm_config = NULL;

	shm_config = conap_scp_get_shm_info();
	if (shm_config == NULL) {
		pr_err("[%s] shm info not found", __func__);
		return -1;
	}

	memset(&g_shm_info, 0, sizeof(struct conap_scp_shm_info));

	g_shm_info.shm_phy_addr = emi_phy_addr + shm_config->scp_shm_offset;
	g_shm_info.shm_size = shm_config->scp_shm_size;

	pr_info("[%s] [%x] phy_addr=[%x][%x] ", __func__, emi_phy_addr,
				g_shm_info.shm_phy_addr, g_shm_info.shm_size);

	g_shm_info.shm_vir_addr = ioremap(g_shm_info.shm_phy_addr, g_shm_info.shm_size);
	if (g_shm_info.shm_vir_addr == NULL) {
		return -1;
	}

	adp_shm = (struct scif_shm_layout*)(g_shm_info.shm_vir_addr);

	pr_info("[%s] vir_addr=[%p] offset=[%p] shm=[%p]", __func__,
		g_shm_info.shm_vir_addr, shm_config->scp_shm_offset, adp_shm);

	g_shm_info.adp_shm_layout = adp_shm;

	ret = conap_scp_shm_reset(shm_config);
	if (ret) {
		pr_err("reset shm fail");
		return ret;
	}

	return 0;
}

int conap_scp_shm_deinit(void)
{
	if (g_shm_info.shm_vir_addr) {
		iounmap(g_shm_info.shm_vir_addr);
		g_shm_info.shm_vir_addr = NULL;
	}
	return 0;
}
