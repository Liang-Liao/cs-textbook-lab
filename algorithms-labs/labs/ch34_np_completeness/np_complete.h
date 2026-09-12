#ifndef CLRS_NP_COMPLETE_H
#define CLRS_NP_COMPLETE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.34 NP-completeness — decision problems and reductions.
 * Graphs undirected, vertices 0..n-1.
 * All routines enumerate bitmasks; instances with n > 20 (or nvars > 20)
 * return -1 instead of an answer. Intended for tiny instances / teaching.
 */

/* Decision: does G have a vertex cover of size <= k?
 * Returns 1/0; -1 if n > 20. k < 0 returns 0. */
int vertex_cover_decision(size_t n, const int *eu, const int *ev, size_t m,
                          int k);

/* Decision: independent set of size >= k. Returns 1/0; -1 if n > 20.
 * k <= 0 is trivially satisfiable. */
int independent_set_decision(size_t n, const int *eu, const int *ev, size_t m,
                             int k);

/* Decision: clique of size >= k (undirected). Returns 1/0; -1 if n > 20.
 * k <= 0 is trivially satisfiable. */
int clique_decision(size_t n, const int *eu, const int *ev, size_t m, int k);

/*
 * CNF-SAT decision for tiny n.
 * clauses[i][0..2] are 3 literals: variable v as +(v+1) or -(v+1)
 * (1-based var index, negative means NOT). vars in 0..nvars-1.
 * nclauses clauses. Returns 1 if satisfiable, 0 if not; -1 if nvars > 20.
 */
int sat3_decision(int nvars, const int *clauses /* nclauses*3 */, int nclauses);

/*
 * CLRS 34.5 reduction 3-SAT <=p CLIQUE (Theorem 34.10).
 * Builds the reduction graph of 3*nclauses vertices (vertex 3*c+j = the
 * j-th literal of clause c); edges join literal occurrences of different
 * clauses with consistent (non-contradictory) literals. The formula is
 * satisfiable iff the graph has a clique of size nclauses — check with
 * clique_decision(3*nclauses, eu, ev, edges, nclauses).
 *
 * Writes edges into eu/ev (capacity max_edges; at most C(3m,2)-3m edges)
 * and returns the number written.
 */
size_t sat3_to_clique(int nvars, const int *clauses /* nclauses*3 */,
                      int nclauses, int *eu, int *ev, size_t max_edges);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_NP_COMPLETE_H */
