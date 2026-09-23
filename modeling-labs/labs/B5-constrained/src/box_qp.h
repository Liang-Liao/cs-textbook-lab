#ifndef BOX_QP_H
#define BOX_QP_H

/* Lab-local to B5: active-set enumeration for box QP (small n). */
int mlab_box_qp_active_set(const double *Q, const double *c, const double *lb,
                           const double *ub, int n, double *x_opt, double *obj_out);

#endif
