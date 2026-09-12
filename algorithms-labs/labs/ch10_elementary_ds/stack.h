#ifndef CLRS_STACK_H
#define CLRS_STACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 10.1 Stacks — fixed-capacity array.
 * `top` is the number of elements (empty when 0).
 */
typedef struct {
  int *a;
  size_t capacity;
  size_t top;
} IntStack;

void stack_init(IntStack *s, size_t capacity);
void stack_destroy(IntStack *s);
int stack_empty(const IntStack *s);
int stack_full(const IntStack *s);
void stack_push(IntStack *s, int x);
int stack_pop(IntStack *s);
int stack_peek(const IntStack *s);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_STACK_H */
