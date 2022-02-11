/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2021 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "atl_dump_parser.h"
#include "../atl_dump.h"

#define DEV_FILE "device.txt"
#define REG_FILE "registers.txt"
#define FW_IFACE_FILE "fw_interface.txt"
#define ACT_RES_FILE "action_resolver.txt"
#define RING_FILE "rings.txt"

struct section_header {
	u32 type;
	u32 length;
};

char out_folder[64];

int dump_device_info(struct atl_crash_dump *crash_dump)
{
	char file_name[64];
	FILE *file;

	snprintf(file_name, 64, "%s/%s", out_folder, DEV_FILE);
	file = fopen(file_name, "w");
	if (file == NULL) {
		printf("Error opening file %s\n", file_name);
		return -1;
	}

	fprintf(file, "Driver version: %s\n", crash_dump->drv_version);
	fprintf(file, "FW version: %s\n", crash_dump->fw_version);

	fclose(file);

	return 0;
}

int dump_registers(struct atl_crash_dump_regs *reg)
{
	int block = 0x1000/4, i, offset;
	char file_name[64];
	FILE *file;

	snprintf(file_name, 64, "%s/%s", out_folder, REG_FILE);
	file = fopen(file_name, "w");
	if (file == NULL) {
		printf("Error opening file %s\n", file_name);
		return -1;
	}

	/* Skip MIF block */

	offset = block * 4;
	fprintf(file, "PCI Registers\n");
	fprintf(file, "=============\n");
	for (i = 0; i < block; i++)
		fprintf(file, "0x%08x: 0x%08x\n", offset + i * 4, reg->layout.pci[i]);

	offset += block * 4;
	fprintf(file, "\n\nInterrupt Registers\n");
	fprintf(file, "=====================\n");
	for (i = 0; i < block; i++)
		fprintf(file, "0x%08x: 0x%08x\n", offset + i * 4, reg->layout.itr[i]);

	offset += block * 4;
	fprintf(file, "\n\nCOM Registers\n");
	fprintf(file, "===============\n");
	for (i = 0; i < block; i++)
		fprintf(file, "0x%08x: 0x%08x\n", offset + i * 4, reg->layout.com[i]);

	offset += block * 4;
	fprintf(file, "\n\nMac Phy Registers\n");
	fprintf(file, "=====================\n");
	for (i = 0; i < block; i++)
		fprintf(file, "0x%08x: 0x%08x\n", offset + i * 4, reg->layout.mac_phy[i]);

	offset += block * 4;
	fprintf(file, "\n\nRx Registers\n");
	fprintf(file, "===============\n");
	/* Rx block size is 0x2000 */
	for (i = 0; i < block * 2; i++)
		fprintf(file, "0x%08x: 0x%08x\n", offset + i * 4, reg->layout.rx[i]);

	offset += block * 2 * 4;
	fprintf(file, "\n\nTx Registers\n");
	fprintf(file, "===============\n");
	/* Tx block size is 0x2000 */
	for (i = 0; i < block * 2; i++)
		fprintf(file, "0x%08x: 0x%08x\n", offset + i * 4, reg->layout.tx[i]);

	fclose(file);

	return 0;
}

int dump_fwiface(struct atl_crash_dump_fwiface *fwiface)
{
	int block =  0x1000/4, i;
	char file_name[64];
	FILE *file;

	snprintf(file_name, 64, "%s/%s", out_folder, FW_IFACE_FILE);
	file = fopen(file_name, "w");
	if (file == NULL) {
		printf("Error opening file %s\n", file_name);
		return -1;
	}

	fprintf(file, "FW input interface\n");
	fprintf(file, "==================\n");
	for (i = 0; i < block; i += 4)
		fprintf(file, "0x%08x:\t0x%08x 0x%08x 0x%08x 0x%08x\n", i,
			fwiface->fw_interface_in[i], fwiface->fw_interface_in[i + 1],
			fwiface->fw_interface_in[i + 2], fwiface->fw_interface_in[i + 3]);

	fprintf(file, "\n\nFW output interface\n");
	fprintf(file, "=======================\n");
	for (i = 0; i < block; i += 4)
		fprintf(file, "0x%08x:\t0x%08x 0x%08x 0x%08x 0x%08x\n", i,
			fwiface->fw_interface_out[i], fwiface->fw_interface_out[i + 1],
			fwiface->fw_interface_out[i + 2], fwiface->fw_interface_out[i + 3]);

	fclose(file);

	return 0;
}

int dump_act_res(struct atl_crash_dump_act_res *act_res)
{
	char file_name[64];
	int i, idx;
	FILE *file;

	snprintf(file_name, 64, "%s/%s", out_folder, ACT_RES_FILE);
	file = fopen(file_name, "w");
	if (file == NULL) {
		printf("Error opening file %s\n", file_name);
		return -1;
	}

	fprintf(file, "RECORD  RESOLVER TAG    TAG Mask        Action\n");
	fprintf(file, "==================================================\n");
	for (i = 0, idx = 0; i < ATL_ACT_RES_TABLE_SIZE / 3; i++) {
		fprintf(file, "%d\t0x%08x\t0x%08x\t0x%08x\n", i, act_res->act_res_data[idx++],
			act_res->act_res_data[idx++], act_res->act_res_data[idx++]);
	}

	fclose(file);

	return 0;
}

int dump_ring(struct atl_crash_dump_ring *ring)
{
	char file_name[64];
	FILE *file;
	u32 *data;
	int i;

	/* Assumption - file 'rings.txt' shouldn't exists */
	snprintf(file_name, 64, "%s/%s", out_folder, RING_FILE);
	file = fopen(file_name, "a+");
	if (file == NULL) {
		printf("Error opening file %s\n", file_name);
		return -1;
	}

	fprintf(file, "Ring Index %d\n", ring->index);
	fprintf(file, "==============\n");
	fprintf(file, "Rx head = 0x%08x tail = 0x%08x\n", ring->rx_head, ring->rx_tail);
	fprintf(file, "Tx head = 0x%08x tail = 0x%08x\n", ring->tx_head, ring->tx_tail);
	fprintf(file, "HW Ring rx_size = %d tx_size = %d\n", ring->rx_ring_size, ring->tx_ring_size);
	fprintf(file, "Rx Ring descriptor:\n");
	fprintf(file, "-------------------\n");
	data = (u32 *)ring->ring_data;
	for (i = 0; i < ring->rx_ring_size; i += 4)
		fprintf(file, "0x%08x:\t0x%08x 0x%08x 0x%08x 0x%08x\n", i, data[i], data[i + 1],
			data[i + 2], data[i + 3]);
	fprintf(file, "Tx Ring descriptor:\n");
	fprintf(file, "-------------------\n");
	data = (u32 *)ring->ring_data + ring->rx_ring_size;
	for (i = 0; i < ring->tx_ring_size; i += 4)
		fprintf(file, "0x%08x:\t0x%08x 0x%08x 0x%08x 0x%08x\n", i, data[i], data[i + 1],
			data[i + 2], data[i + 3]);
	fprintf(file, "\n");

	fclose(file);

	return 0;

}

int main(int argc, char *argv[])
{
	struct atl_crash_dump *crash_dump;
	struct section_header *header;
	int i, offset, ret = 0;
	time_t t = time(NULL);
	struct tm *tm_val;
	FILE *input_file;
	struct stat st;
	char *buffer;

	if (argc < 2) {
		printf("No intput file\n");
		return -1;
	}

	if (stat(argv[1], &st) == -1) {
		printf("File stat error\n");
		return -1;
	}

	buffer = malloc(st.st_size);
	if (!buffer) {
		printf("Mem alloc error\n");
		return -1;
	}
	/* open the source file for reading */
	input_file = fopen(argv[1],"rb");
	if (input_file == NULL) {
		ret = -1;
		printf("Error opening file %s\n", argv[1]);
		goto err;
	}

	fread(buffer, sizeof(char), st.st_size, input_file);
	fclose(input_file);

	tm_val = localtime(&t);
	tm_val = NULL;
	if (tm_val)
		snprintf(out_folder, 64, "%s_%02d-%02d-%02d_%02d-%02d-%02d", argv[1],
			 tm_val->tm_mon + 1, (int) tm_val->tm_mday, 1900 +  tm_val->tm_year,
			 tm_val->tm_hour,  tm_val->tm_min,  tm_val->tm_sec);
	else
		snprintf(out_folder, 64, "%s_parsed", argv[1]);
	if (stat(out_folder, &st) == -1) {
		if (mkdir(out_folder, 0644) == -1) {
			ret = -1;
			printf("Failed to creat folder` %s\n", out_folder);
			goto err;
		}
	}

	crash_dump = (struct atl_crash_dump *)buffer;
	ret = dump_device_info(crash_dump);
	if (ret)
		goto err;

	offset = offsetof(struct atl_crash_dump, antigua);
	for (i = 0; i < crash_dump->sections_count; i++) {
		header = (struct section_header *)(buffer + offset);
		switch (header->type) {
		case atl_crash_dump_type_regs:
			ret = dump_registers((struct atl_crash_dump_regs *)(buffer + offset));
			if (ret)
				goto err;
			break;
		case atl_crash_dump_type_fwiface:
			ret = dump_fwiface((struct atl_crash_dump_fwiface *)(buffer + offset));
			if (ret)
				goto err;
			break;
		case atl_crash_dump_type_act_res:
			ret = dump_act_res((struct atl_crash_dump_act_res *)(buffer + offset));
			if (ret)
				goto err;
			break;
		case atl_crash_dump_type_ring:
			ret = dump_ring((struct atl_crash_dump_ring *)(buffer + offset));
			if (ret)
				goto err;
			break;
		default:
			printf("Parsing Error - invalid section\n");
			goto err;
		}
		offset += header->length;
	}

err:
	free(buffer);
	if (ret)
		printf("Failed to parse the dump\n");
	else
		printf("Parsing is completed successfully\n");

	return 0;
}
