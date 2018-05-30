/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2018, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                  FTS functions for getting frames                      *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 */

#ifndef __FTS_FRAME_H
#define __FTS_FRAME_H

#include "ftsSoftware.h"

//Number of data bytes for each node
#define BYTES_PER_NODE                 2
#define OFFSET_LENGTH                  2
#define FRAME_DATA_HEADER              8
#define FRAME_HEADER_SIGNATURE         0xB5
#define FRAME_DATA_READ_RETRY          2

struct MutualSenseFrame {
	struct DataHeader header;
	short *node_data;
	int node_data_size;
};

struct SelfSenseFrame {
	struct DataHeader header;
	short *force_data;
	short *sense_data;
};

int getOffsetFrame(u16 address, u16 *offset);
int getChannelsLength(void);
int getFrameData(u16 address, int size, short **frame);
int getMSFrame(u16 type, struct MutualSenseFrame *frame, int keep_first_row);
//int getMSKeyFrame(u16 type, short **frame);
//int getSSFrame(u16 type, short **frame);
//int getNmsFrame(u16 type, short ***frames, int * sizes,
//int keep_first_row, int fs, int n);
int getSenseLen(void);
int getForceLen(void);
int requestFrame(u16 type);
int readFrameDataHeader(u16 type, struct DataHeader *header);
int getMSFrame2(u16 type, struct MutualSenseFrame *frame);
int getSSFrame2(u16 type, struct SelfSenseFrame *frame);

#endif

