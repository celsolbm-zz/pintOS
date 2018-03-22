#ifndef _FIXED_POINT_H
#define _FIXED_POINT_H

#include <stdint.h>
#include <limits.h>

#define p 17
#define q 14
#define f (1 << q)

int32_t int_to_fixed (int);
int fixed_to_int_zero (int32_t);
int fixed_to_int_near (int32_t);
int32_t add_fixed (int32_t, int32_t);
int32_t sub_fixed (int32_t, int32_t);
int32_t add_int_fixed (int32_t, int);
int32_t sub_int_fixed (int32_t, int);
int32_t multi_fixed (int32_t, int32_t);
int32_t multi_int_fixed (int32_t, int);
int32_t div_fixed (int32_t, int32_t);
int32_t div_int_fixed (int32_t, int);


int32_t
int_to_fixed (int n)
{
  return (int32_t) n * f;
}

int
fixed_to_int_zero (int32_t x)
{
  return (int) x / f;
}

int
fixed_to_int_near (int32_t x)
{
  if (x >= 0) {
    return ((int) x + (f / 2)) / f;
  } else {
    return ((int) x - (f / 2)) / f;
  }
}

int32_t
add_fixed (int32_t x, int32_t y)
{
  return x + y;
}

int32_t
sub_fixed (int32_t x, int32_t y)
{
  return x - y;
}

int32_t
add_int_fixed (int32_t x, int n)
{
  return (x + int_to_fixed (n));
}

int32_t
sub_int_fixed (int32_t x, int n)
{
  return (x - int_to_fixed(n));
}

int32_t
multi_fixed (int32_t x, int32_t y)
{
  int64_t result = (int64_t) x * y;
  result /= f;
  return (int32_t) result;
}

int32_t
multi_int_fixed (int32_t x, int n)
{
  return x * (int32_t) n;
}

int32_t
div_fixed (int32_t x, int32_t y)
{
  int64_t result = (int64_t) x * f;
  result /= y;
  return (int32_t) result;
}

int32_t
div_int_fixed (int32_t x, int n)
{
  return x / (int32_t) n;
}

#endif /* _FIXED_POINT_H */
