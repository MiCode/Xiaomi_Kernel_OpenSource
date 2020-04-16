#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include "drv2624.h"
#include "parse_rtp.h"

static struct rtp_head head = { 0 };

static struct idx_element *index_table;
static unsigned char *instruction_data;;

unsigned char *instruction_begin;;
unsigned char *instruction_current;
unsigned char *instruction_end;
bool open_rtp;
int pop_running_effect_command(char *command, int *data)
{
	pr_info("%s: enter!\n", __func__);
	if (!open_rtp) {
		return -INVALID;
	}
	if (instruction_end == instruction_current)
		return -INVALID;
	if ((instruction_current - instruction_begin) % 2 == 0)
		*command = 'w';
	else
		*command = 'd';
	*data = *instruction_current;
	instruction_current++;
	return SUCCESS;
}

int set_running_effect_id(int id)
{
	if (!open_rtp) {
		return -INVALID;
	}
	if (index_table[id].length == 0) {
		pr_info("%s: index_table[id].length=0\n", __func__);
		return -INVALID;
	}
	instruction_begin = instruction_data + index_table[id].offset;
	instruction_current = instruction_begin;
	instruction_end =
	    instruction_data + index_table[id].offset + index_table[id].length;

	return index_table[id].duration;
}

static int rtp_parse_header(unsigned char *pData, unsigned int nSize)
{
	unsigned char *pDataStart = pData;
	unsigned char pMagicNumber[] = { 0xcf, 0xcf, 0x00, 0x00 };
	if (nSize < 16) {
		pr_info("rtp: Header too short\n");
		return -INVALID;
	}

	if (memcmp(pData, pMagicNumber, 4)) {
		pr_info("rtp: Magic number doesn't match\n");
		return -INVALID;
	}
	pData += 4;

	memcpy(&head.ver, pData, 4);
	pr_info("rtp version : %d\n", head.ver);
	pData += 4;

	memcpy(&head.effect_num, pData, 4);
	pr_info("rtp effect_num : %d\n", head.effect_num);
	pData += 8;
	return pData - pDataStart;
}

int rtp_parse(unsigned char *pData, unsigned int nSize)
{
	int head_size = 0;
	int i;
	pr_info("%s begin\n", __func__);
	open_rtp = true;
	head_size = rtp_parse_header(pData, nSize);
	pr_info("header size: %d\n", head_size);
	if (head_size < 0 || head_size >= nSize) {
		pr_info("rtp file: Wrong Header\n");
		open_rtp = false;
		return -INVALID;
	}

	pData += RTP_HEAD_SIZE;
	index_table = (struct idx_element *)pData;
	for (i = 0; i < 24; i++) {
		pr_info("%s: idx_tabel[%d] = %d, %d, %d\n",
		       __func__, i, index_table[i].offset,
		       index_table[i].length, index_table[i].duration);
	}
	instruction_data = pData + INDEX_NUM * 6;
	pr_info("%s: idx_element size = %d\n", __func__,
	       sizeof(struct idx_element));
	pr_info("%s end\n", __func__);
	return SUCCESS;
}
