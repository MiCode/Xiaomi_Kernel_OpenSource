#ifndef __ISP_GLOBAL_H_INCLUDED__
#define __ISP_GLOBAL_H_INCLUDED__

#include <stdint.h>

#define IS_ISP_2300_MEDFIELD

#include "isp2300_medfield_params.h"

#define ISP_PMEM_WIDTH_LOG2		9
#define ISP_PMEM_SIZE			ISP_PMEM_DEPTH

#define ISP_NWAY_LOG2			6
#define ISP_VEC_NELEMS_LOG2		ISP_NWAY_LOG2

/* The number of data bytes in a vector disregarding the reduced precision */
#define ISP_VEC_BYTES			(ISP_VEC_NELEMS*sizeof(uint16_t))

/* ISP Registers */
#define ISP_PC_REG				0x05
#define ISP_SC_REG				0x00
#define ISP_IRQ_READY_REG		0x00
#define ISP_IRQ_CLEAR_REG		0x00
#define ISP_BROKEN_BIT			0x04
#define ISP_IDLE_BIT			0x05
#define ISP_STALLING_BIT		0x07
#define ISP_IRQ_CLEAR_BIT		0x08
#define ISP_IRQ_READY_BIT		0x0A
#define ISP_SLEEPING_BIT		0x0B

#define ISP_CTRL_SINK_REG		0x06
#define ISP_DMEM_SINK_REG		0x06
#define ISP_VMEM_SINK_REG		0x06
#define ISP_FIFO0_SINK_REG		0x06
#define ISP_FIFO1_SINK_REG		0x06
#define ISP_FIFO2_SINK_REG		0x06
#define ISP_FIFO3_SINK_REG		0x06
#define ISP_FIFO4_SINK_REG		0x06
#define ISP_FIFO5_SINK_REG		0x06
#define ISP_VAMEM1_SINK_REG		0x06
#define ISP_VAMEM2_SINK_REG		0x06

/* ISP Register bits */
#define ISP_CTRL_SINK_BIT		0x00
#define ISP_DMEM_SINK_BIT		0x01
#define ISP_VMEM_SINK_BIT		0x02
#define ISP_FIFO0_SINK_BIT		0x03
#define ISP_FIFO1_SINK_BIT		0x04
#define ISP_FIFO2_SINK_BIT		0x05
#define ISP_FIFO3_SINK_BIT		0x06
#define ISP_FIFO4_SINK_BIT		0x07
#define ISP_FIFO5_SINK_BIT		0x08
#define ISP_VAMEM1_SINK_BIT		0x09
#define ISP_VAMEM2_SINK_BIT		0x0A

#endif /* __ISP_GLOBAL_H_INCLUDED__ */
