#ifndef __DMA_PUBLIC_H_INCLUDED__
#define __DMA_PUBLIC_H_INCLUDED__

#include "system_types.h"

typedef struct dma_state_s		dma_state_t;

/*! Read the control registers of DMA[ID]
 
 \param	ID[in]				DMA identifier
 \param	state[out]			input formatter state structure

 \return none, state = DMA[ID].state
 */
extern void dma_get_state(
	const dma_ID_t		ID,
	dma_state_t			*state);

/*! Write to a control register of DMA[ID]

 \param	ID[in]				DMA identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return none, DMA[ID].ctrl[reg] = value
 */
STORAGE_CLASS_DMA_H void dma_reg_store(
	const dma_ID_t		ID,
	const unsigned int	reg,
	const hrt_data		value);

/*! Read from a control register of DMA[ID]
 
 \param	ID[in]				DMA identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return DMA[ID].ctrl[reg]
 */
STORAGE_CLASS_DMA_H hrt_data dma_reg_load(
	const dma_ID_t		ID,
	const unsigned int	reg);

#endif /* __DMA_PUBLIC_H_INCLUDED__ */
