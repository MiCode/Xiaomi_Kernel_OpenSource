#ifndef __GDC_PUBLIC_H_INCLUDED__
#define __GDC_PUBLIC_H_INCLUDED__

/*! Write the bicubic interpolation table of GDC[ID]

 \param	ID[in]				GDC identifier
 \param data[in]			The data matrix to be written

 \pre
	- data must point to a matrix[4][HRT_GDC_N]

 \implementation dependent
	- The value of "HRT_GDC_N" is device specific
	- The LUT should not be partially written
	- The LUT format is a quadri-phase interpolation
	  table. The layout is device specific
	- The range of the values data[n][m] is device
	  specific

 \return none, GDC[ID].lut[0...3][0...HRT_GDC_N-1] = data
 */
STORAGE_CLASS_EXTERN void gdc_lut_store(
	const gdc_ID_t		ID,
	const int			data[4][HRT_GDC_N]);

/*! Return the integer representation of 1.0 of GDC[ID]
 
 \param	ID[in]				GDC identifier

 \return unity
 */
STORAGE_CLASS_EXTERN int gdc_get_unity(
	const gdc_ID_t		ID);

#endif /* __GDC_PUBLIC_H_INCLUDED__ */
