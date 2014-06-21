/**
 * \file common.h -- \brief Common library utilities and definitions.
 *
 * Copyright (C) 2013, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2013, Athena Elafrou
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */

#ifndef SPARSEX_COMMON_H
#define SPARSEX_COMMON_H

#include <sparsex/error.h>
#include <sparsex/types.h>
#include "sparsex/internals/logger/LoggerUtil.hpp"
#include "sparsex/internals/Vector.hpp"

#include <stdlib.h>

/**
 *  \brief The sparse matrix handle type.
 */
typedef struct matrix spx_matrix_t;

/**
 *  \brief The input matrix handle type.
 */
typedef struct input spx_input_t;

/**
 *  \brief The vector handle type.
 */
typedef struct vec spx_vector_t;

/**
 *  \brief Partitioning object.
 */
typedef struct partition spx_partition_t;

/**
 *  \brief Dense array object that represents a permutation.
 */
typedef spx_index_t spx_perm_t;

/**
 *  \brief Matrix property type.
 */
typedef int spx_property_t;

/**
 *  \brief A vector copy-mode type.
 */
typedef int spx_copymode_t;

#define INVALID_INPUT ((spx_input_t *) NULL)
#define INVALID_MAT ((spx_matrix_t *) NULL)
#define INVALID_VEC ((spx_vector_t *) NULL)
#define INVALID_PART ((spx_partition_t *) NULL)
#define INVALID_PERM ((spx_perm_t *) NULL)

#define OP_REORDER          42
#define OP_SHARE            43
#define OP_COPY             44
#define INDEXING_ZERO_BASED 0
#define INDEXING_ONE_BASED  1

static inline
int check_indexing(int base)
{
    return (base == 0 || base == 1);
}

static inline
int check_mat_dim(spx_index_t dim)
{
    return (dim >= 0);
}

static inline
int check_vec_dim(const spx_vector_t *x, unsigned long dim)
{
    return (x->size == dim);
}

/**
 *  \brief Logging utilities.
 */
static inline
void spx_log_disable_all()
{
    DisableLogging();
}

static inline
void spx_log_disable_error()
{
    DisableError();
}

static inline
void spx_log_disable_warning()
{
    DisableWarning();
}

static inline
void spx_log_disable_info()
{
    DisableInfo();
}

static inline
void spx_log_enable_all_console()
{
    AlwaysUseConsole();
}

static inline
void spx_log_enable_all_file(const char *file)
{
    AlwaysUseFile(file);
}

/**
 *  \brief Library initialization routine.
 */
void spx_init();

/**
 *  \brief Library shutdown routine.
 */
void spx_finalize();

/**
 *  \brief malloc() wrapper.
 */
#define spx_malloc(type, size) \
	(type *) malloc_internal(size, __FILE__, __LINE__, __FUNCTION__)
void *malloc_internal(size_t x, const char *sourcefile, unsigned long lineno,
                      const char *function);

/**
 *  \brief free() wrapper.
 */
#define spx_free(object) \
    free_internal(object, __FILE__, __LINE__, __FUNCTION__)
void free_internal(void *ptr, const char *sourcefile, unsigned long lineno,
                   const char *function);

#endif /* SPARSEX_COMMON_H */
