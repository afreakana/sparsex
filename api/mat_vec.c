/**
 * SparseX/mat_vec.c -- Sparse matrix routines.
 *
 * Copyright (C) 2013, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2013, Athena Elafrou
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */

#include "mat_vec.h"
#include "CsxMatvec.hpp"
#include "SparseMatrixWrapper.hpp"

#include <unistd.h>
#include <float.h>

/**
 *  \brief The sparse matrix internal representation.
 */
struct matrix {
    index_t nrows, ncols, nnz;
    int symmetric;          /**< Flag that indicates whether the symmetric
                               version of CSX will be used */
    perm_t *permutation;    /**< The permutation, in case the matrix has been
                               reordered */
    void *csx;              /**< The tuned matrix representation, i.e. the input
                               matrix transformed to the CSX format */
};

/**
 *  \brief The input matrix internal representation.
 */
struct input {
    index_t nrows, ncols, nnz;
    char type;      /**< A character that stores the format of the input matrix
                       ('C' for CSR or 'M' for MMF) */
    void *mat;      /**< The input matrix representation */
};

struct partition {
    unsigned int nr_partitions;
    size_t *parts;
    int *nodes;
};

/**
 *  \brief Allocates an input structure.
 */
static void input_alloc_struct(input_t **A)
{
    *A = spx_malloc(input_t, sizeof(input_t));
    (*A)->nrows = 0;
    (*A)->ncols = 0;
    (*A)->nnz = 0;
    (*A)->type = 'U';
    (*A)->mat = INVALID_INPUT;
}

/**
 *  \brief Frees an input structure.
 */
static void input_free_struct(input_t *A)
{
    if (A->mat) {
        if (A->type == 'C') {
            DestroyCSR(A->mat);
        } else {
            DestroyMMF(A->mat);
        }
    }

    spx_free(A);
}

/**
 *  \brief Allocates a matrix structure.
 */
static void mat_alloc_struct(matrix_t **A)
{
    *A = spx_malloc(matrix_t, sizeof(matrix_t));
    (*A)->nrows = 0;
    (*A)->ncols = 0;
    (*A)->nnz = 0;
    (*A)->permutation = INVALID_PERM;
    (*A)->csx = INVALID_MAT;
    (*A)->symmetric = 0;
}

/**
 *  \brief Frees a matrix structure.
 */
static void mat_free_struct(matrix_t *A)
{
    if (A->csx) {
        DestroyCsx(A->csx);
    }

    if (A->permutation) {
        spx_free(A->permutation);
    }

    spx_free(A);
}

/**
 *  \brief Allocates a partition structure.
 */
static void part_alloc_struct(partition_t **p)
{
    *p = spx_malloc(partition_t, sizeof(partition_t));
    (*p)->nr_partitions = 0;
    (*p)->parts = NULL;
    (*p)->nodes = NULL;
}

/**
 *  \brief Frees a partition structure.
 */
static void part_free_struct(partition_t *p)
{
    if (p->parts) {
        spx_free(p->parts);
    }

    if (p->nodes) {
        spx_free(p->nodes);
    }

    spx_free(p);
}

input_t *spx_input_load_csr(index_t *rowptr, index_t *colind, value_t *values,
                            index_t nr_rows, index_t nr_cols,
                            property_t indexing)
{
    /* Check validity of input arguments */
    if (!check_mat_dim(nr_rows) || !check_mat_dim(nr_cols)) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid matrix dimensions");
        return INVALID_INPUT;
    }

    if (!check_indexing(indexing)) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid indexing");
        return INVALID_INPUT;
    }

    if (!rowptr) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid rowptr argument");
        return INVALID_INPUT;
    }

    if (!colind) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid colind argument");
        return INVALID_INPUT;
    }

    if (!values) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid values argument");
        return INVALID_INPUT;
    }

    /* Create CSR wrapper */
    input_t *A = INVALID_INPUT;
    input_alloc_struct(&A);
    A->type = 'C';
    A->nrows = nr_rows;
    A->ncols = nr_cols;
    A->nnz = rowptr[nr_rows] - indexing;
    A->mat = CreateCSR(rowptr, colind, values, nr_rows, nr_cols, !indexing);
    if (!A->mat) {
        input_free_struct(A);
        SETERROR_1(SPX_ERR_INPUT_MAT, "creating CSR wrapper failed");
        return INVALID_INPUT;
    }

    return A;
}

input_t *spx_input_load_mmf(const char *filename)
{
    /* Check validity of input argument */
    if (!filename) {
        SETERROR_0(SPX_ERR_FILE);
        return INVALID_INPUT;
    }

    if (access(filename, F_OK | R_OK) == -1) {  //not 100% safe check
        SETERROR_0(SPX_ERR_FILE);
        return INVALID_INPUT;
    }

    /* Load matrix */
    input_t *A = INVALID_INPUT;
    input_alloc_struct(&A);
    A->type = 'M';
    A->mat = CreateMMF(filename, &(A->nrows), &(A->ncols), &(A->nnz));
    if (!A->mat) {
        input_free_struct(A);
        SETERROR_1(SPX_ERR_INPUT_MAT, "loading matrix from MMF file failed");
        return INVALID_INPUT;
    }

    return A;
}

spx_error_t spx_input_destroy(input_t *A)
{
    /* Check validity of input argument */
    if (!A) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid input handle");
        return SPX_FAILURE;
    }

    /* Free allocated memory of matrix handle */
    input_free_struct(A);

    return SPX_SUCCESS;
}

matrix_t *spx_mat_tune(input_t *input, ...)
{
    /* Check validity of input argument */
    if (!input) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid input matrix");
        return INVALID_MAT;
    }

    /* Check optional arguments */
    va_list ap;
    va_start(ap, input);
    int option = va_arg(ap, int);
    va_end(ap);

    /* Convert to CSX */
    matrix_t *tuned = INVALID_MAT;
    perm_t *permutation = INVALID_PERM;
    mat_alloc_struct(&tuned);
    tuned->nrows = input->nrows;
    tuned->ncols = input->ncols;
    tuned->nnz = input->nnz;
    if (input->type == 'C') {
        if (option == OP_REORDER) {
            input->mat = ReorderCSR(input->mat, &permutation);
        }
        tuned->csx = TuneCSR(input->mat, &(tuned->symmetric));
    } else if (input->type == 'M') {
        if (option == OP_REORDER) {
            input->mat = ReorderMMF(input->mat, &permutation);
        }
        tuned->csx = TuneMMF(input->mat, &(tuned->symmetric));
    }

    if (!tuned->csx) {
        mat_free_struct(tuned);
        SETERROR_0(SPX_ERR_TUNED_MAT);
        return INVALID_MAT;
    }

    tuned->permutation = permutation;
    permutation = INVALID_PERM;

    /* Create local buffers in case of CSX-Sym */
    if (tuned->symmetric) {
        spm_mt_t *spm_mt = (spm_mt_t *) tuned->csx;
        unsigned int nr_threads = spm_mt->nr_threads;
        /* spm_mt->local_buffers = */
        /*     spx_malloc(vector_t *, nr_threads*sizeof(vector_t *)); */
        spm_mt->local_buffers =
            (vector_t **) malloc(nr_threads*sizeof(vector_t *));

        unsigned int i;
#ifdef SPM_NUMA
        for (i = 1; i < nr_threads; i++) {
            int node = spm_mt->spm_threads[i].node;
            spm_mt->local_buffers[i] = vec_create_onnode(tuned->nrows, node); // FIXME interleaved ?
        }
#else
        for (i = 1; i < nr_threads; i++)
            spm_mt->local_buffers[i] = vec_create(tuned->nrows, NULL);
#endif
    }

    return tuned;
}

spx_error_t spx_mat_get_entry(const matrix_t *A, index_t row,
                              index_t column, value_t *value)
{
    /* Check validity of input argument */
    if (!A) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid matrix handle");
        return SPX_FAILURE;
    }

    if (row <= 0  || row > A->nrows || 
        column <= 0  || column > A->ncols) {
        SETERROR_0(SPX_OUT_OF_BOUNDS);
        return SPX_FAILURE;
    }

    /* Search for element */
    if (A->permutation != INVALID_PERM) {
        row = A->permutation[row - 1] + 1;
        column = A->permutation[column - 1] + 1;
    }

    int err = GetValue(A->csx, row, column, value);
    if (err != 0) {
        SETERROR_0(SPX_ERR_ENTRY_NOT_FOUND);
        return SPX_FAILURE;
    }

    return SPX_SUCCESS;
}

spx_error_t spx_mat_set_entry(matrix_t *A, index_t row, index_t column,
                              value_t value)
{
    /* Check validity of input arguments */
    if (!A) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid matrix handle");
        return SPX_FAILURE;
    }

    if (row <= 0  || row > A->nrows || 
        column <= 0  || column > A->ncols) {
        SETERROR_0(SPX_OUT_OF_BOUNDS);
        SETWARNING(SPX_WARN_ENTRY_NOT_SET);
        return SPX_FAILURE;
    }

    /* Set new value */
    if (A->permutation != INVALID_PERM) {
        row = A->permutation[row - 1] + 1;
        column = A->permutation[column - 1] + 1;
    }

    int err = SetValue(A->csx, row, column, value);
    if (err != 0) {
        SETERROR_0(SPX_ERR_ENTRY_NOT_FOUND);
        return SPX_FAILURE;
    }

    return SPX_SUCCESS;
}

spx_error_t spx_mat_save(const matrix_t *A, const char *filename)
{
    /* Check validity of input arguments */
    if (!A) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid matrix handle");
        return SPX_FAILURE;
    }

    if (!filename) {
        SETWARNING(SPX_WARN_CSXFILE);
    }

    /* Save the tuned matrix */
    SaveTuned(A->csx, filename, A->permutation);

    return SPX_SUCCESS;
}

matrix_t *spx_mat_restore(const char *filename)
{
    /* Check validity of input argument */
    if (!filename) {
        SETERROR_0(SPX_ERR_FILE);
        return INVALID_MAT;
    }

    if (access(filename, F_OK | R_OK) == -1) {  //not 100% safe check
        SETERROR_0(SPX_ERR_FILE);
        return INVALID_MAT;
    }

    /* Load tuned matrix from file */
    matrix_t *A = INVALID_MAT;
    mat_alloc_struct(&A);
    A->csx = LoadTuned(filename, &(A->nrows), &(A->ncols), &(A->nnz),
                       &(A->permutation));

    return A;
}

index_t spx_mat_get_nrows(const matrix_t *A)
{
    return  A->nrows;
}

index_t spx_mat_get_ncols(const matrix_t *A)
{
    return A->ncols;
}

partition_t *spx_mat_get_parts(matrix_t *A)
{
#ifdef SPM_NUMA
    spm_mt_t *spm_mt = (spm_mt_t *) A->csx;
    partition_t *ret = INVALID_PART;
    part_alloc_struct(&ret);
	ret->parts = (size_t *) malloc(sizeof(*ret->parts)*spm_mt->nr_threads);
	ret->nodes = (int *) malloc(sizeof(*ret->nodes)*spm_mt->nr_threads);
    ret->nr_partitions = spm_mt->nr_threads;

    unsigned int i;
	for (i = 0; i < spm_mt->nr_threads; i++) {
		spm_mt_thread_t *spm = spm_mt->spm_threads + i;
		ret->parts[i] = spm->nr_rows * sizeof(double);
		ret->nodes[i] = spm->node;
	}

    return ret;
#else
    return INVALID_PART;
#endif
}

perm_t *spx_mat_get_perm(const matrix_t *A)
{
    return A->permutation;
}

spx_error_t spx_matvec_mult(scalar_t alpha, const matrix_t *A, vector_t *x,
                                  scalar_t beta, vector_t *y)
{
    /* Check validity of input arguments */
    if (!A) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid matrix handle");
        return SPX_FAILURE;
    }

    if (!x) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid vector x");
        return SPX_FAILURE;
    }

    if (!y) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid vector y");
        return SPX_FAILURE;
    }

    /* Check compatibility between matrix and vector dimensions */
    if (!check_vec_dim(x, A->ncols) && !check_vec_dim(y, A->nrows)) {
        SETERROR_0(SPX_ERR_VEC_DIM);
        return SPX_FAILURE;
    }

    /* Compute kernel */
    if (!A->symmetric) {
        matvec_mt(A->csx, x, alpha, y, beta);
    } else {
        matvec_sym_mt(A->csx, x, alpha, y, beta);
    }

    return SPX_SUCCESS;
}

spx_error_t spx_mat_destroy(matrix_t *A)
{
    /* Check validity of input argument */
    if (!A) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid matrix handle");
        return SPX_FAILURE;
    }

    /* Free allocated memory of matrix handle */
    mat_free_struct(A);

    return SPX_SUCCESS;
}

spx_error_t spx_part_destroy(partition_t *p)
{
#ifdef SPM_NUMA
    /* Check validity of input argument */
    if (!p) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid partition handle");
        return SPX_FAILURE;
    }

    /* Free allocated memory of matrix handle */
    part_free_struct(p);
#endif
    return SPX_SUCCESS;
}

void spx_option_set(const char *option, const char *value)
{
    SetPropertyByMnemonic(option, value);
}

void spx_options_set_from_env()
{
    SetPropertiesFromEnv();
}

vector_t *vec_create_numa(unsigned long size, partition_t *p)
{
	vector_t *v = vec_create_interleaved(size, p->parts, p->nr_partitions,
                                         p->nodes);
	return v;
}

vector_t *vec_create_from_buff_numa(double *buff, unsigned long size,
                                    partition_t *p)
{
    vector_t *v = vec_create_numa(size, p);
    unsigned int i;
    for (i = 0; i < size; i++)
        v->elements[i] = buff[i];

#if defined (SPM_NUMA) && defined (NUMA_CHECKS)
    print_alloc_status("vector", check_interleaved(v->elements, p->parts,
                                                   p->nr_partitions,
                                                   p->nodes));
#endif

    return v;
}

vector_t *vec_create_random_numa(unsigned long size, partition_t *p)
{
    vector_t *v = vec_create_numa(size, p);
    vec_init_rand_range(v, (double) -0.01, (double) 0.1);

#if defined (SPM_NUMA) && defined (NUMA_CHECKS)
    print_alloc_status("vector", check_interleaved(v->elements, p->parts,
                                                   p->nr_partitions,
                                                   p->nodes));
#endif

    return v;
}

spx_error_t spx_vec_set_entry(vector_t *v, int idx, double val)
{
    if (idx <= 0  || idx > v->size) {
        SETERROR_0(SPX_OUT_OF_BOUNDS);
        SETWARNING(SPX_WARN_ENTRY_NOT_SET);
        return SPX_FAILURE;
    }

    vec_set_entry(v, idx, val);

    return SPX_SUCCESS;
}

spx_error_t spx_vec_reorder(vector_t *v, perm_t *p)
{
    unsigned long i;
    vector_t *permuted_v = NULL;

    if (p == INVALID_PERM) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid permutation");
        return SPX_FAILURE;
    }

    permuted_v = vec_create(v->size, NULL);
    for (i = 0; i < v->size; i++) {
        permuted_v->elements[p[i]] = v->elements[i];
    }

#ifdef SPM_NUMA
    vec_copy(permuted_v, v);
    vec_destroy(permuted_v);
#else
    v->elements = permuted_v->elements;
    permuted_v->elements = NULL;
    permuted_v = NULL;
#endif
    return SPX_SUCCESS;
}

spx_error_t spx_vec_inv_reorder(vector_t *v, perm_t *p)
{
    unsigned long i;
    vector_t *permuted_v = NULL;

    //check v1 v2 dimensions
    if (p == INVALID_PERM) {
        SETERROR_1(SPX_ERR_ARG_INVALID, "invalid permutation");
        return SPX_FAILURE;
    }

    permuted_v = vec_create(v->size, NULL);
    for (i = 0; i < v->size; i++) {
        permuted_v->elements[i] = v->elements[p[i]];
    }

#ifdef SPM_NUMA
    vec_copy(permuted_v, v);
    vec_destroy(permuted_v);
#else
    v->elements = permuted_v->elements;
    permuted_v->elements = NULL;
    permuted_v = NULL;
#endif
    return SPX_SUCCESS;
}