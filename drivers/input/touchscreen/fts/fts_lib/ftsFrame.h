/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*                  FTS functions for getting frames			 *
*                                                                        *
**************************************************************************
**************************************************************************

*/


#include "ftsSoftware.h"



#define BYTES_PER_NODE						2

#define OFFSET_LENGTH						2

#define FRAME_DATA_HEADER				8

#define FRAME_HEADER_SIGNATURE			0xB5

#define FRAME_DATA_READ_RETRY			2

typedef struct {
	DataHeader header;
	short *node_data;
	int node_data_size;
} MutualSenseFrame;

typedef struct {
	DataHeader header;
	short *force_data;
	short *sense_data;
} SelfSenseFrame;

int getChannelsLength(void);
int getFrameData(u16 address, int size, short **frame);
int getSenseLen(void);
int getForceLen(void);
int requestFrame(u16 type);
int readFrameDataHeader(u16 type, DataHeader *header);
int getMSFrame2(u16 type, MutualSenseFrame *frame);
int getMSFrame(u16 type, MutualSenseFrame *frame, int keep_first_row);
int getSSFrame2(u16 type, SelfSenseFrame *frame);
