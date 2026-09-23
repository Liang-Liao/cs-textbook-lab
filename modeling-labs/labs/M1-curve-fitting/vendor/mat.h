#ifndef MLAB_MAT_H
#define MLAB_MAT_H

/* Row-major dense matrix: data[i*cols + j] */
typedef struct {
    int rows, cols;
    double *data;
} mlab_mat;

#define MLAB_MAT_AT(A, i, j) ((A)->data[(size_t)(i) * (size_t)(A)->cols + (size_t)(j)])

void mlab_mat_zero(mlab_mat *A);
void mlab_mat_identity(mlab_mat *A);
void mlab_mat_copy(mlab_mat *dst, const mlab_mat *src);
/* C = A*B; C must be rows(A) x cols(B) */
void mlab_mat_mul(mlab_mat *C, const mlab_mat *A, const mlab_mat *B);
/* y = A*x */
void mlab_mat_vec(double *y, const mlab_mat *A, const double *x);
/* y = A^T * x */
void mlab_mat_tvec(double *y, const mlab_mat *A, const double *x);
/* B = A^T */
void mlab_mat_transpose(mlab_mat *B, const mlab_mat *A);
/* Frobenius norm */
double mlab_mat_frob(const mlab_mat *A);
/* ||Ax-b||_2 / ||b||_2 (if ||b||==0 use 1) */
double mlab_rel_residual(const mlab_mat *A, const double *x, const double *b);

/* Allocate rows*cols doubles; returns 0 on success. */
int mlab_mat_alloc(mlab_mat *A, int rows, int cols);
void mlab_mat_free(mlab_mat *A);

#endif /* MLAB_MAT_H */
