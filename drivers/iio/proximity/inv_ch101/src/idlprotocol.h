/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2019, Chirp Microsystems.  All rights reserved.
 *
 * Chirp Microsystems CONFIDENTIAL
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CHIRP MICROSYSTEMS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDLPROTOCOL_H_
#define IDLPROTOCOL_H_

#include <stdint.h>

#define IDL_PACKET_DEFAULTDATASIZE	1

#define IDL_PACKET_INVALID		0x00
#define IDL_PACKET_FWVERSION		0x01
#define IDL_PACKET_SINGLEREGREAD	0x02
#define IDL_PACKET_SINGLEREGWRITE	0x03
#define IDL_PACKET_MULREGREAD		0x04
#define IDL_PACKET_MULREGWRITE		0x05
#define IDL_PACKET_RESET		0x06
#define IDL_PACKET_OPENSENSOR		0x07
#define IDL_PACKET_CLOSESENSOR		0x08
#define IDL_PACKET_ENABLEDATABUF	0x09
#define IDL_PACKET_GETDATABUF		0x0A
#define IDL_PACKET_DISABLEDATABUF	0x0B
#define IDL_PACKET_RAWREAD		0x0C
#define IDL_PACKET_RAWWRITE		0x0D
#define IDL_PACKET_OPENSENSORSEC	0x0E
#define IDL_PACKET_CUSTOMSENSINIT	0x0F

#define IDL_SENSORID_ERROR		0x00
#define IDL_SENSORID_OK			0x01

#define IDL_INTERFACEID_I2C		0
#define IDL_INTERFACEID_SPI1		1
#define IDL_INTERFACEID_SPI2		2

#define IDL_PACKET_RESULT_ERROR		0x00

#define IDL_PACKET_RAWREADSIZE		9		//max size
#define IDL_PACKET_RAWWRITESIZE		5		//max size

/*  HEADER  */
struct IDLP_HEADER {
	u8 size;
	u8 size1;
	u8 packetid;
};

/*  FWVERSION  */
struct IDLPREQ_FWVERSION {
	struct IDLP_HEADER header;
};

struct IDLPRSP_FWVERSION {
	struct IDLP_HEADER header;
	u8 major;
	u8 minor;
	u8 build;
};

/* ** RESET * */
struct IDLPREQ_RESET {
	struct IDLP_HEADER header;
};

struct IDLPRSP_RESET {
	struct IDLP_HEADER header;
};

/*  REGREAD  */
struct IDLPREQ_REGREAD {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
};

struct IDLPRSP_REGREAD {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 value;
};

/*  REGWRITE  */
struct IDLPREQ_REGWRITE {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 value;
};

struct IDLPRSP_REGWRITE {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 result;
};

/*  OPENSENSOR  */
struct IDLPREQ_OPENSENSOR {
	struct IDLP_HEADER header;
	u8 interfaceid;
	u8 intopt;
	u8 options;
};

struct IDLPREQ_OPENSENSORSEC {
	struct IDLP_HEADER header;
	u8 interfaceid;
	u8 intopt;
	u8 options;
	u8 options1; //press
};

struct IDLPRSP_OPENSENSOR {
	struct IDLP_HEADER header;
	u8 sensorid;
};

/*  CLOSESENSOR  */
struct IDLPREQ_CLOSESENSOR {
	struct IDLP_HEADER header;
	u8 sensorid;
};

struct tag_idlpacket_rsp_closesensor {
	struct IDLP_HEADER header;
	u8 result;
} IDLPRSP_CLOSESENSOR;

/*  ENABLEDATABUF  */
struct IDLPREQ_ENABLEDATABUF {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 intregaddr;
	u8 intregdata;
	u8 buffersize0;
	u8 buffersize1;
	u8 regscount;
//	u8 regs[248];
};

struct IDLPRSP_ENABLEDATABUF {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 result;
};

/*  GETDATABUF  */
struct IDLPREQ_GETDATABUF {
	struct IDLP_HEADER header;
	u8 sensorid;
};

#define MAXDATABUF 16002//3600//250//1008 //250
struct IDLPRSP_GETDATABUF {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 eom;
	u8 data[MAXDATABUF];
};

/*  DISABLEDATABUF  */
struct IDLPREQ_DISABLEDATABUF {
	struct IDLP_HEADER header;
	u8 sensorid;
};

struct tag_idlpacket_rsp_disabledatabuf {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 result;
};

/*  MULREGREAD  */
struct IDLPREQ_MULREGREAD {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
};

struct IDLPRSP_MULREGREAD {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
	u8 values[250];
};

/*  MULREGWRITE  */
struct IDLPREQ_MULREGWRITE {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
//	u8 values[250];
};

struct IDLPRSP_MULREGWRITE {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
};

struct IDLPREQ_RAWWRITE {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
	u8 values[IDL_PACKET_RAWWRITESIZE];
};

struct IDLPRSP_RAWWRITE {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
	u8 result;
};

struct IDLPREQ_RAWREAD {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
};

struct IDLPRSP_RAWREAD {
	struct IDLP_HEADER header;
	u8 sensorid;
	u8 addr;
	u8 stride;
	u8 size;
	u8 values[IDL_PACKET_RAWREADSIZE];
};

#endif /* IDLPROTOCOL_H_ */
