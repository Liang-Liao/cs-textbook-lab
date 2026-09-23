#include "mat.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_mat_zero(mlab_mat *A)
{
    if (!A || !A->data) return;
    memset(A->data, 0, (size_t)A->rows * (size_t)A->cols * sizeof(double));
}

void mlab_mat_identity(mlab_mat *A)
{
    int i, j;
    if (!A || !A->data) return;
    for (i = 0; i < A->rows; ++i) {
        for (j = 0; j < A->cols; ++j)
            A->data[(size_t)i * (size_t)A->cols + (size_t)j] = (i == j) ? 1.0 : 0.0;
    }
}

void mlab_mat_copy(mlab_mat *dst, const mlab_mat *src)
{
    dst->rows = src->rows;
    dst->cols = src->cols;
    memcpy(dst->data, src->data,
           (size_t)src->rows * (size_t)src->cols * sizeof(double));
}

void mlab_mat_mul(mlab_mat *C, const mlab_mat *A, const mlab_mat *B)
{
    int i, j, k;
    for (i = 0; i < A->rows; ++i) {
        for (j = 0; j < B->cols; ++j) {
            double s = 0.0;
            for (k = 0; k < A->cols; ++k)
                s += A->data[(size_t)i * (size_t)A->cols + (size_t)k] *
                     B->data[(size_t)k * (size_t)B->cols + (size_t)j];
            C->data[(size_t)i * (size_t)C->cols + (size_t)j] = s;
        }
    }
}

void mlab_mat_vec(double *y, const mlab_mat *A, const double *x)
{
    int i, j;
    for (i = 0; i < A->rows; ++i) {
        double s = 0.0;
        for (j = 0; j < A->cols; ++j)
            s += A->data[(size_t)i * (size_t)A->cols + (size_t)j] * x[j];
        y[i] = s;
    }
}

void mlab_mat_tvec(double *y, const mlab_mat *A, const double *x)
{
    int i, j;
    for (j = 0; j < A->cols; ++j) y[j] = 0.0;
    for (i = 0; i < A->rows; ++i) {
        for (j = 0; j < A->cols; ++j)
            y[j] += A->data[(size_t)i * (size_t)A->cols + (size_t)j] * x[i];
    }
}

void mlab_mat_transpose(mlab_mat *B, const mlab_mat *A)
{
    int i, j;
    for (i = 0; i < A->rows; ++i)
        for (j = 0; j < A->cols; ++j)
            B->data[(size_t)j * (size_t)B->cols + (size_t)i] =
                A->data[(size_t)i * (size_t)A->cols + (size_t)j];
}

double mlab_mat_frob(const mlab_mat *A)
{
    double s = 0.0;
    size_t k, nn = (size_t)A->rows * (size_t)A->cols;
    for (k = 0; k < nn; ++k) s += A->data[k] * A->data[k];
    return sqrt(s);
}

double mlab_rel_residual(const mlab_mat *A, const double *x, const double *b)
{
    int n = A->rows;
    double nb, *ax = (double *)malloc((size_t)n * sizeof(double));
    double r;
    if (!ax) return -1.0;
    mlab_mat_vec(ax, A, x);
    {
        int i;
        for (i = 0; i < n; ++i) ax[i] -= b[i];
    }
    nb = mlab_nrm2(b, n);
    if (nb == 0.0) nb = 1.0;
    r = mlab_nrm2(ax, n) / nb;
    free(ax);
    return r;
}

int mlab_mat_alloc(mlab_mat *A, int rows, int cols)
{
    A->rows = rows;
    A->cols = cols;
    A->data = (double *)malloc((size_t)rows * (size_t)cols * sizeof(double));
    return A->data ? 0 : -1;
}

void mlab_mat_free(mlab_mat *A)
{
    if (A && A->data) {
        free(A->data);
        A->data = 0;
    }
}
