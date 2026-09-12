#include "stack.h"

#include <stdlib.h>

#include "clrs.h"

void stack_init(IntStack *s, size_t capacity) {
  CLRS_ASSERT(capacity > 0, "capacity > 0");
  s->a = clrs_xmalloc(capacity * sizeof(int));
  s->capacity = capacity;
  s->top = 0;
}

void stack_destroy(IntStack *s) {
  free(s->a);
  s->a = NULL;
  s->capacity = 0;
  s->top = 0;
}

int stack_empty(const IntStack *s) { return s->top == 0; }

int stack_full(const IntStack *s) { return s->top >= s->capacity; }

void stack_push(IntStack *s, int x) {
  CLRS_ASSERT(!stack_full(s), "stack overflow");
  s->a[s->top++] = x;
}

int stack_pop(IntStack *s) {
  CLRS_ASSERT(!stack_empty(s), "stack underflow");
  return s->a[--s->top];
}

int stack_peek(const IntStack *s) {
  CLRS_ASSERT(!stack_empty(s), "stack empty");
  return s->a[s->top - 1];
}
