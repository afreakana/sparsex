#ifndef __SPM_CRS_VHUFF_H__
#define __SPM_CRS_VHUFF_H__

#include <inttypes.h>
#include "macros.h"
#include "huffman.h"
#include "spmv_method.h"

//#define SPM_CRS_VI_VIDX_TYPE uint8_t

#define _NAME(val_type, ci_bits, name) \
	spm_crs ## ci_bits ## _vh_ ## val_type ## name

#define DECLARE_CRS_VH(val_type, ci_bits)        \
	typedef struct {                                  \
		UINT_TYPE(ci_bits)  *col_ind, *row_ptr;   \
		uint64_t            nz, nrows, ncols;     \
		unsigned char       *hvals;               \
		unsigned long       hvals_bits;           \
		huff_node_t         *htree;               \
		unsigned long       hsym_bits;            \
	} _NAME(val_type, ci_bits, _t);          \
	                                                  \
	_NAME(val_type, ci_bits, _t) *           \
	_NAME(val_type, ci_bits, _init_mmf)(     \
		char *mmf_file,                           \
		unsigned long *rows_nr,                   \
		unsigned long *cols_nr,                   \
		unsigned long *nz_nr                      \
	);                                                \
	                                                  \
	void                                              \
	_NAME(val_type, ci_bits, _destroy)(      \
		_NAME(val_type, ci_bits, _t) *m  \
	);                                                \
	                                                  \
	unsigned long                                     \
	_NAME(val_type, ci_bits, _size)(         \
		_NAME(val_type, ci_bits, _t) *m  \
	);                                                \

DECLARE_CRS_VH(double, 32)
DECLARE_CRS_VH(float,  32)
DECLARE_CRS_VH(double, 64)
DECLARE_CRS_VH(float,  64)

#undef _NAME
#undef DECLARE_CRS_VI

#define SPM_CRS_VH_NAME(name) \
	CON5(spm_crs, SPM_CRSVH_CI_BITS, _vh_, ELEM_TYPE, name)
#define SPM_CRSVH_TYPE SPM_CRS_VH_NAME(_t)

#endif