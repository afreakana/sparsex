/*
 * Copyright (C) 2009-2014, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2009-2011, Kornilios Kourtis
 * Copyright (C) 2011-2014, Vasileios Karakasis
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */ 

/**
 * \file block_row_tmpl.c
 * \brief Template for runtime code generation.
 * 
 * \author Computing Systems Laboratory (CSLab), NTUA
 * \date 2011&ndash;2014
 * \copyright This file is distributed under the BSD License. See LICENSE.txt
 * for details.
 */

static inline spx_value_t
block_row_${r}x${c}_case(uint8_t **ctl, uint8_t size, spx_value_t **values,
			 spx_value_t **x_curr, spx_value_t **y_curr,
			 spx_value_t scale_f)
{
  register spx_value_t *y_curr_ = *y_curr;
  register spx_value_t *x_curr_ = *x_curr;

  for (spx_index_t i = 0; i < ${c}; i++) {
    register spx_value_t xr = x_curr_[i];
    for (spx_index_t j = 0; j < ${r}; j++) {
      y_curr_[j] += xr * (**values) * scale_f;
      (*values)++;
    }
  }

  return 0;
}
