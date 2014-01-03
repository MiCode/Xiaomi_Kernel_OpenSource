/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/batterydata-lib.h>

int linear_interpolate(int y0, int x0, int y1, int x1, int x)
{
	if (y0 == y1 || x == x0)
		return y0;
	if (x1 == x0 || x == x1)
		return y1;

	return y0 + ((y1 - y0) * (x - x0) / (x1 - x0));
}

int is_between(int left, int right, int value)
{
	if (left >= right && left >= value && value >= right)
		return 1;
	if (left <= right && left <= value && value <= right)
		return 1;
	return 0;
}

static int interpolate_single_lut_scaled(struct single_row_lut *lut,
						int x, int scale)
{
	int i, result;

	if (x < lut->x[0] * scale) {
		pr_debug("x %d less than known range return y = %d lut = %pS\n",
							x, lut->y[0], lut);
		return lut->y[0];
	}
	if (x > lut->x[lut->cols - 1] * scale) {
		pr_debug("x %d more than known range return y = %d lut = %pS\n",
						x, lut->y[lut->cols - 1], lut);
		return lut->y[lut->cols - 1];
	}

	for (i = 0; i < lut->cols; i++)
		if (x <= lut->x[i] * scale)
			break;
	if (x == lut->x[i] * scale) {
		result = lut->y[i];
	} else {
		result = linear_interpolate(
			lut->y[i - 1],
			lut->x[i - 1] * scale,
			lut->y[i],
			lut->x[i] * scale,
			x);
	}
	return result;
}

int interpolate_fcc(struct single_row_lut *fcc_temp_lut, int batt_temp)
{
	return interpolate_single_lut_scaled(fcc_temp_lut,
						batt_temp,
						DEGC_SCALE);
}

int interpolate_scalingfactor_fcc(struct single_row_lut *fcc_sf_lut,
		int cycles)
{
	/*
	 * sf table could be null when no battery aging data is available, in
	 * that case return 100%
	 */
	if (fcc_sf_lut)
		return interpolate_single_lut_scaled(fcc_sf_lut, cycles, 1);
	else
		return 100;
}

int interpolate_scalingfactor(struct sf_lut *sf_lut, int row_entry, int pc)
{
	int i, scalefactorrow1, scalefactorrow2, scalefactor, rows, cols;
	int row1 = 0;
	int row2 = 0;

	/*
	 * sf table could be null when no battery aging data is available, in
	 * that case return 100%
	 */
	if (!sf_lut)
		return 100;

	rows = sf_lut->rows;
	cols = sf_lut->cols;
	if (pc > sf_lut->percent[0]) {
		pr_debug("pc %d greater than known pc ranges for sfd\n", pc);
		row1 = 0;
		row2 = 0;
	}
	if (pc < sf_lut->percent[rows - 1]) {
		pr_debug("pc %d less than known pc ranges for sf\n", pc);
		row1 = rows - 1;
		row2 = rows - 1;
	}
	for (i = 0; i < rows; i++) {
		if (pc == sf_lut->percent[i]) {
			row1 = i;
			row2 = i;
			break;
		}
		if (pc > sf_lut->percent[i]) {
			row1 = i - 1;
			row2 = i;
			break;
		}
	}

	if (row_entry < sf_lut->row_entries[0] * DEGC_SCALE)
		row_entry = sf_lut->row_entries[0] * DEGC_SCALE;
	if (row_entry > sf_lut->row_entries[cols - 1] * DEGC_SCALE)
		row_entry = sf_lut->row_entries[cols - 1] * DEGC_SCALE;

	for (i = 0; i < cols; i++)
		if (row_entry <= sf_lut->row_entries[i] * DEGC_SCALE)
			break;
	if (row_entry == sf_lut->row_entries[i] * DEGC_SCALE) {
		scalefactor = linear_interpolate(
				sf_lut->sf[row1][i],
				sf_lut->percent[row1],
				sf_lut->sf[row2][i],
				sf_lut->percent[row2],
				pc);
		return scalefactor;
	}

	scalefactorrow1 = linear_interpolate(
				sf_lut->sf[row1][i - 1],
				sf_lut->row_entries[i - 1] * DEGC_SCALE,
				sf_lut->sf[row1][i],
				sf_lut->row_entries[i] * DEGC_SCALE,
				row_entry);

	scalefactorrow2 = linear_interpolate(
				sf_lut->sf[row2][i - 1],
				sf_lut->row_entries[i - 1] * DEGC_SCALE,
				sf_lut->sf[row2][i],
				sf_lut->row_entries[i] * DEGC_SCALE,
				row_entry);

	scalefactor = linear_interpolate(
				scalefactorrow1,
				sf_lut->percent[row1],
				scalefactorrow2,
				sf_lut->percent[row2],
				pc);

	return scalefactor;
}

/* get ocv given a soc  -- reverse lookup */
int interpolate_ocv(struct pc_temp_ocv_lut *pc_temp_ocv,
				int batt_temp, int pc)
{
	int i, ocvrow1, ocvrow2, ocv, rows, cols;
	int row1 = 0;
	int row2 = 0;

	rows = pc_temp_ocv->rows;
	cols = pc_temp_ocv->cols;
	if (pc > pc_temp_ocv->percent[0]) {
		pr_debug("pc %d greater than known pc ranges for sfd\n", pc);
		row1 = 0;
		row2 = 0;
	}
	if (pc < pc_temp_ocv->percent[rows - 1]) {
		pr_debug("pc %d less than known pc ranges for sf\n", pc);
		row1 = rows - 1;
		row2 = rows - 1;
	}
	for (i = 0; i < rows; i++) {
		if (pc == pc_temp_ocv->percent[i]) {
			row1 = i;
			row2 = i;
			break;
		}
		if (pc > pc_temp_ocv->percent[i]) {
			row1 = i - 1;
			row2 = i;
			break;
		}
	}

	if (batt_temp < pc_temp_ocv->temp[0] * DEGC_SCALE)
		batt_temp = pc_temp_ocv->temp[0] * DEGC_SCALE;
	if (batt_temp > pc_temp_ocv->temp[cols - 1] * DEGC_SCALE)
		batt_temp = pc_temp_ocv->temp[cols - 1] * DEGC_SCALE;

	for (i = 0; i < cols; i++)
		if (batt_temp <= pc_temp_ocv->temp[i] * DEGC_SCALE)
			break;
	if (batt_temp == pc_temp_ocv->temp[i] * DEGC_SCALE) {
		ocv = linear_interpolate(
				pc_temp_ocv->ocv[row1][i],
				pc_temp_ocv->percent[row1],
				pc_temp_ocv->ocv[row2][i],
				pc_temp_ocv->percent[row2],
				pc);
		return ocv;
	}

	ocvrow1 = linear_interpolate(
				pc_temp_ocv->ocv[row1][i - 1],
				pc_temp_ocv->temp[i - 1] * DEGC_SCALE,
				pc_temp_ocv->ocv[row1][i],
				pc_temp_ocv->temp[i] * DEGC_SCALE,
				batt_temp);

	ocvrow2 = linear_interpolate(
				pc_temp_ocv->ocv[row2][i - 1],
				pc_temp_ocv->temp[i - 1] * DEGC_SCALE,
				pc_temp_ocv->ocv[row2][i],
				pc_temp_ocv->temp[i] * DEGC_SCALE,
				batt_temp);

	ocv = linear_interpolate(
				ocvrow1,
				pc_temp_ocv->percent[row1],
				ocvrow2,
				pc_temp_ocv->percent[row2],
				pc);

	return ocv;
}

int interpolate_pc(struct pc_temp_ocv_lut *pc_temp_ocv,
				int batt_temp, int ocv)
{
	int i, j, pcj, pcj_minus_one, pc;
	int rows = pc_temp_ocv->rows;
	int cols = pc_temp_ocv->cols;

	if (batt_temp < pc_temp_ocv->temp[0] * DEGC_SCALE) {
		pr_debug("batt_temp %d < known temp range\n", batt_temp);
		batt_temp = pc_temp_ocv->temp[0] * DEGC_SCALE;
	}

	if (batt_temp > pc_temp_ocv->temp[cols - 1] * DEGC_SCALE) {
		pr_debug("batt_temp %d > known temp range\n", batt_temp);
		batt_temp = pc_temp_ocv->temp[cols - 1] * DEGC_SCALE;
	}

	for (j = 0; j < cols; j++)
		if (batt_temp <= pc_temp_ocv->temp[j] * DEGC_SCALE)
			break;
	if (batt_temp == pc_temp_ocv->temp[j] * DEGC_SCALE) {
		/* found an exact match for temp in the table */
		if (ocv >= pc_temp_ocv->ocv[0][j])
			return pc_temp_ocv->percent[0];
		if (ocv <= pc_temp_ocv->ocv[rows - 1][j])
			return pc_temp_ocv->percent[rows - 1];
		for (i = 0; i < rows; i++) {
			if (ocv >= pc_temp_ocv->ocv[i][j]) {
				if (ocv == pc_temp_ocv->ocv[i][j])
					return pc_temp_ocv->percent[i];
				pc = linear_interpolate(
					pc_temp_ocv->percent[i],
					pc_temp_ocv->ocv[i][j],
					pc_temp_ocv->percent[i - 1],
					pc_temp_ocv->ocv[i - 1][j],
					ocv);
				return pc;
			}
		}
	}

	/*
	 * batt_temp is within temperature for
	 * column j-1 and j
	 */
	if (ocv >= pc_temp_ocv->ocv[0][j])
		return pc_temp_ocv->percent[0];
	if (ocv <= pc_temp_ocv->ocv[rows - 1][j - 1])
		return pc_temp_ocv->percent[rows - 1];

	pcj_minus_one = 0;
	pcj = 0;
	for (i = 0; i < rows-1; i++) {
		if (pcj == 0
			&& is_between(pc_temp_ocv->ocv[i][j],
				pc_temp_ocv->ocv[i+1][j], ocv)) {
			pcj = linear_interpolate(
				pc_temp_ocv->percent[i],
				pc_temp_ocv->ocv[i][j],
				pc_temp_ocv->percent[i + 1],
				pc_temp_ocv->ocv[i+1][j],
				ocv);
		}

		if (pcj_minus_one == 0
			&& is_between(pc_temp_ocv->ocv[i][j-1],
				pc_temp_ocv->ocv[i+1][j-1], ocv)) {
			pcj_minus_one = linear_interpolate(
				pc_temp_ocv->percent[i],
				pc_temp_ocv->ocv[i][j-1],
				pc_temp_ocv->percent[i + 1],
				pc_temp_ocv->ocv[i+1][j-1],
				ocv);
		}

		if (pcj && pcj_minus_one) {
			pc = linear_interpolate(
				pcj_minus_one,
				pc_temp_ocv->temp[j-1] * DEGC_SCALE,
				pcj,
				pc_temp_ocv->temp[j] * DEGC_SCALE,
				batt_temp);
			return pc;
		}
	}

	if (pcj)
		return pcj;

	if (pcj_minus_one)
		return pcj_minus_one;

	pr_debug("%d ocv wasn't found for temp %d in the LUT returning 100%%\n",
							ocv, batt_temp);
	return 100;
}
