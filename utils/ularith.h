/* Some commonly used assembly helper macros for arithmetic on 
   unsigned long. 
   Defining ULARITH_VERBOSE_ASM puts in the asm output a line for each
   function call that shows what registers/memory locations the operands 
   are in.
   Defining ULARITH_NO_ASM avoids asm macros and uses the C fallback code
   where available.
*/

#ifndef UL_ARITH_H__

#define UL_ARITH_H__

#include <assert.h>
#include <limits.h>
#include <gmp.h>
#include "macros.h"

/* <limits.h> defines LONG_BIT only with _XOPEN_SOURCE defined, but if 
   another header (such as <stdio.h>) already included <features.h> before 
   _XOPEN_SOURCE was set to 1, future includes of <features.h> are
   short-circuited and _XOPEN_SOURCE is ignored. */

#ifndef LONG_BIT
#ifdef LONG_MAX /* ISO C99 requires LONG_MAX in limits.h */
#if LONG_MAX == 2147483647L
#define LONG_BIT 32
#else
#define LONG_BIT 64
#endif /* if LONG_MAX == 2147483647L */
#elif defined __LONG_MAX__
#if __LONG_MAX__ == 2147483647L
#define LONG_BIT 32
#else
#define LONG_BIT 64
#endif /* if __LONG_MAX__ == 2147483647L */
#else /* elif defined __LONG_MAX__ */
#error Cannot guess LONG_BIT, please define
#endif /* elif defined __LONG_MAX__ else */
#endif /* ifndef LONG_BIT */

#ifndef ASSERT
#define ASSERT(x)	assert(x)
#endif

#ifdef WANT_ASSERT_EXPENSIVE
#define ASSERT_EXPENSIVE(x) ASSERT(x)
#else
#define ASSERT_EXPENSIVE(x)
#endif

/* On 32 bit x86, the general constrains for, e.g., the source operand
   of add is "g". For x86_64, it is "rme", since immediate constants
   must be 32 bit. */
#if defined(__i386__) && defined(__GNUC__)
#define ULARITH_CONSTRAINT_G "g"
#elif defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
#define ULARITH_CONSTRAINT_G "rme"
#endif

/* Increases r if a != 0 */
static inline void
ularith_inc_nz (unsigned long *r, const unsigned long a)
{
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_inc_nz (%0, %1)\n" : : "X" (*r), "X" (a));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "cmpq $1, %1\n\t"
    "sbbq $-1, %0\n\t"
    : "+r" (*r)
    : "rm" (a)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "cmpl $1, %1\n\t"
    "sbbl $-1, %0\n\t"
    : "+r" (*r)
    : "rm" (a)
    : "cc");
#else
  if (a != 0)
    r += 1;
#endif
}

/* Let a = a1 + 2^k * a2, b = b1 + 2^k * b2, where k is number of bits
   in an unsigned long. Return 1 if a > b, and 0 if a <= b. */
static inline int
ularith_gt_2ul_2ul(unsigned long, unsigned long,
                   unsigned long, unsigned long) ATTRIBUTE((const));
static inline int
ularith_gt_2ul_2ul(const unsigned long a1, const unsigned long a2,
                   const unsigned long b1, const unsigned long b2)
{
  return a2 > b2 || (a2 == b2 && a1 > b1);
}

/* Add an unsigned long to two unsigned longs with carry propagation from 
   low word (r1) to high word (r2). Any carry out from high word is lost. */

static inline void
ularith_add_ul_2ul (unsigned long *r1, unsigned long *r2, 
			const unsigned long a)
{
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_add_ul_2ul (%0, %1, %2)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "addq %2, %0\n\t"
    "adcq $0, %1\n"
    : "+&r" (*r1), "+r" (*r2) 
    : "rme" (a)
    : "cc"); /* TODO: add commutativity and alternative for add to 
                memory */
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "addl %2, %0\n\t"
    "adcl $0, %1\n"
    : "+&r" (*r1), "+r" (*r2)
    : "g" (a)
    : "cc");
#else
  *r1 += a;
  if (*r1 < a)
    (*r2)++;
#endif
}


/* Add two unsigned longs to two unsigned longs with carry propagation from 
   low word (r1) to high word (r2). Any carry out from high word is lost. */

static inline void
ularith_add_2ul_2ul (unsigned long *r1, unsigned long *r2, 
			 const unsigned long a1, const unsigned long a2)
{
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_add_2ul_2ul (%0, %1, %2, %3)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a1), "X" (a2));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "addq %2, %0\n\t"
    "adcq %3, %1\n"
    : "+&r" (*r1), "+r" (*r2)
    : "rme" (a1), "rme" (a2)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "addl %2, %0\n\t"
    "adcl %3, %1\n"
    : "+&r" (*r1), "+r" (*r2)
    : "g" (a1), "g" (a2)
    : "cc");
#else
  *r1 += a1;
  *r2 += a2 + (*r1 < a1);
#endif
}

/* Adds two unsigned longs from two unsigned longs with carry propagation 
   from low word (r1) to high word (r2). Returns 1 if there was a carry out 
   from high word, otherwise returns 0. */

static inline char
ularith_add_2ul_2ul_cy (unsigned long *r1, unsigned long *r2, 
			const unsigned long a1, const unsigned long a2)
{
  char cy;
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_add_2ul_2ul_cy (%0, %1, %2, %3)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a1), "X" (a2));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "addq %3, %0\n\t"
    "adcq %4, %1\n\t"
    "setc %2\n"
    : "+&r" (*r1), "+r" (*r2), "=r" (cy)
    : "rme" (a1), "rme" (a2)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "addl %3, %0\n\t"
    "adcl %4, %1\n"
    "setc %2\n"
    : "+&r" (*r1), "+r" (*r2), "=r" (cy)
    : "g" (a1), "g" (a2)
    : "cc");
#else
  unsigned long u1 = *r1 + a1, u2 = *r2 + a2;
  if (u1 < *r1)
    u2++;
  /* Overflow occurred iff the sum is smaller than one of the summands */
  cy = ularith_gt_2ul_2ul(a1, a2, u1, u2);
  *r1 = u1;
  *r2 = u2;
#endif
  return cy;
}


/* Requires a < m and b <= m, then r == a+b (mod m) and r < m */
MAYBE_UNUSED
static inline void
ularith_addmod_ul_ul (unsigned long *r, const unsigned long a,
               const unsigned long b, const unsigned long m)
{
  ASSERT_EXPENSIVE (a < m && b <= m);

#if (defined(__i386__) && defined(__GNUC__)) || defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  {
    unsigned long t = a + b, tr = a - m;

    __asm__ __VOLATILE (
      "add %2, %0\n\t"   /* tr += b */
      "cmovnc %1, %0\n\t"  /* if (!cy) tr = t */
      : "+&r" (tr)
      : "rm" (t), ULARITH_CONSTRAINT_G (b)
      : "cc"
    );
    ASSERT_EXPENSIVE (tr == ((a >= m - b) ? (a - (m - b)) : (a + b)));
    r[0] = tr;
  }
#else
  r[0] = (b >= m - a) ? (b - (m - a)) : (a + b);
#endif

  ASSERT_EXPENSIVE (r[0] < m);
}


/* Subtract an unsigned long from two unsigned longs with borrow propagation 
   from low word (r1) to high word (r2). Any borrow out from high word is 
   lost. */

static inline void
ularith_sub_ul_2ul (unsigned long *r1, unsigned long *r2, 
			const unsigned long a)
{
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_sub_ul_2ul  (%0, %1, %2)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "subq %2, %0\n\t"
    "sbbq $0, %1\n"
    : "+&r" (*r1), "+r" (*r2)
    : "rme" (a)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "subl %2, %0\n\t"
    "sbbl $0, %1\n"
    : "+&r" (*r1), "+r" (*r2)
    : "g" (a)
    : "cc");
#else
  unsigned long u = *r1;
  *r1 -= a;
  if (*r1 > u)
    (*r2)--;
#endif
}


/* Subtract two unsigned longs from two unsigned longs with borrow propagation 
   from low word (r1) to high word (r2). Any borrow out from high word is 
   lost. */

static inline void
ularith_sub_2ul_2ul (unsigned long *r1, unsigned long *r2, 
			 const unsigned long a1, const unsigned long a2)
{
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_sub_2ul_2ul (%0, %1, %2, %3)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a1), "X" (a2));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "subq %2, %0\n\t"
    "sbbq %3, %1\n"
    : "+&r" (*r1), "+r" (*r2)
    : "rme" (a1), "rme" (a2)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "subl %2, %0\n\t"
    "sbbl %3, %1\n"
    : "+&r" (*r1), "+r" (*r2)
    : "g" (a1), "g" (a2)
    : "cc");
#else
  unsigned long u = *r1;
  *r1 -= a1;
  *r2 -= a2;
  if (*r1 > u)
    (*r2)--;
#endif
}

/* Subtract two unsigned longs from two unsigned longs with borrow propagation 
   from low word (r1) to high word (r2). Returns 1 if there was a borrow out 
   from high word, otherwise returns 0. */

static inline char
ularith_sub_2ul_2ul_cy (unsigned long *r1, unsigned long *r2, 
			const unsigned long a1, const unsigned long a2)
{
  char cy;
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_sub_2ul_2ul_cy (%0, %1, %2, %3)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a1), "X" (a2));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "subq %3, %0\n\t"
    "sbbq %4, %1\n\t"
    "setc %2\n"
    : "+&r" (*r1), "+r" (*r2), "=r" (cy)
    : "rme" (a1), "rme" (a2)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "subl %3, %0\n\t"
    "sbbl %4, %1\n"
    "setc %2\n"
    : "+&r" (*r1), "+r" (*r2), "=q" (cy)
    : "g" (a1), "g" (a2)
    : "cc");
#else
  unsigned long u1 = *r1 - a1, u2 = *r2 - a2;
  if (a1 > *r1)
    u2--;
  cy = ularith_gt_2ul_2ul(a1, a2, *r1, *r2);
  *r1 = u1;
  *r2 = u2;
#endif
  return cy;
}

/* Subtract only if result is non-negative */

static inline void
ularith_sub_ul_ul_ge (unsigned long *r, const unsigned long a)
{
  unsigned long t = *r;
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_sub_2ul_2ul_ge (%0, %1, %2, %3)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a1), "X" (a2));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "subq %2, %0\n\t" /* r -= a */
    "cmovc %1, %0\n\t" /* If there's a borrow, restore r from t */
    : "+&r" (*r)
    : "r" (t), "rme" (a)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "subl %2, %0\n\t"
    "cmovc %1, %0\n\t"
    : "+&r" (*r)
    : "r" (t), "g" (a)
    : "cc");
#else
  t -= a;
  if (*r >= a)
    *r = t;
#endif
}


static inline void
ularith_sub_2ul_2ul_ge (unsigned long *r1, unsigned long *r2, 
			const unsigned long a1, const unsigned long a2)
{
  unsigned long t1 = *r1, t2 = *r2;
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_sub_2ul_2ul_ge (%0, %1, %2, %3)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a1), "X" (a2));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "subq %4, %0\n\t" /* r1 -= a1 */
    "sbbq %5, %1\n\t" /* r2 -= a2 + cy */
    "cmovc %2, %0\n\t" /* If there's a borrow, restore r1 from t1 */
    "cmovc %3, %1\n\t" /* and r2 from t2 */
    : "+&r" (*r1), "+&r" (*r2)
    : "r" (t1), "r" (t2), "rme" (a1), "rme" (a2)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE ( "subl %4, %0\n\t"
    "sbbl %5, %1\n\t"
    "cmovc %2, %0\n\t"
    "cmovc %3, %1\n\t"
    : "+&r" (*r1), "+&r" (*r2)
    : "r" (t1), "r" (t2), "g" (a1), "g" (a2)
    : "cc");
#else
  if (!ularith_gt_2ul_2ul(a1, a2, *r1, *r2))
    {
      *r1 = t1 - a1;
      *r2 = t2 - a2 - (a1 > t1);
    }
#endif
}


MAYBE_UNUSED
static inline void
ularith_submod_ul_ul (unsigned long *r, const unsigned long a,
                      const unsigned long b, const unsigned long m)
{
  ASSERT_EXPENSIVE (a < m && b < m);
#if (defined(__i386__) && defined(__GNUC__)) || defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  {
    unsigned long tr, t = a;
    __asm__ __VOLATILE (
      "sub %2, %1\n\t"  /* t -= b ( = a - b) */
      "lea (%1,%3,1), %0\n\t" /* tr = t + m ( = a - b + m) */
      "cmovnc %1, %0\n\t" /* if (a >= b) tr = t */
      : "=&r" (tr), "+&r" (t)
      : ULARITH_CONSTRAINT_G (b), "r" (m)
      : "cc"
    );
    r[0] = tr;
  }
#elif 1
  /* Seems to be faster than the one below */
  {
    unsigned long t = 0UL, tr;
    if ((tr = a - b) > a)
      t = m;
    r[0] = tr + t;
  }
#else
  r[0] = (a < b) ? (a - b + m) : (a - b);
#endif

  ASSERT_EXPENSIVE (r[0] < m);
}


/* Multiply two unsigned long "a" and "b" and put the result as 
   r2:r1 (r2 being the high word) */

static inline void
ularith_mul_ul_ul_2ul (unsigned long *r1, unsigned long *r2, 
			   const unsigned long a, const unsigned long b)
{
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_mul_ul_ul_2ul (%0, %1, %2, %3)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a), "X" (b));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "mulq %3"
    : "=a" (*r1), "=d" (*r2)
    : "%0" (a), "rm" (b)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "mull %3"
    : "=a" (*r1), "=d" (*r2)
    : "%0" (a), "rm" (b)
    : "cc");
#else
  const int half = LONG_BIT / 2;
  const unsigned long mask = (1UL << half) - 1UL;
  unsigned long t1, t2, p1, p2;

  t1 = (a & mask) * (b & mask);
  t2 = 0UL;
  p1 = (a >> half) * (b & mask);
  p2 = p1 >> half;
  p1 = (p1 & mask) << half;
  ularith_add_2ul_2ul (&t1, &t2, p1, p2);
  p1 = (a & mask) * (b >> half);
  p2 = p1 >> half;
  p1 = (p1 & mask) << half;
  ularith_add_2ul_2ul (&t1, &t2, p1, p2);
  t2 += (a >> half) * (b >> half);
  *r1 = t1; 
  *r2 = t2;
#endif
}


static inline void
ularith_sqr_ul_2ul (unsigned long *r1, unsigned long *r2, 
		    const unsigned long a)
{
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_mul_ul_ul_2ul (%0, %1, %2)\n" : : 
           "X" (*r1), "X" (*r2), "X" (a));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "mulq %%rax"
    : "=a" (*r1), "=d" (*r2)
    : "0" (a)
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "mull %%eax"
    : "=a" (*r1), "=d" (*r2)
    : "0" (a)
    : "cc");
#else
  const int half = LONG_BIT / 2;
  const unsigned long mask = (1UL << half) - 1UL;
  unsigned long t1, t2, p1, p2;

  t1 = (a & mask) * (a & mask);
  t2 = 0UL;
  p1 = (a >> half) * (a & mask);
  p2 = p1 >> half;
  p1 = (p1 & mask) << half;
  ularith_add_2ul_2ul (&t1, &t2, p1, p2);
  ularith_add_2ul_2ul (&t1, &t2, p1, p2);
  t2 += (a >> half) * (a >> half);
  *r1 = t1; 
  *r2 = t2;
#endif
}


/* Integer division of a two ulong value a2:a1 by a ulong divisor. Returns
   quotient and remainder. */

static inline void
ularith_div_2ul_ul_ul (unsigned long *q, unsigned long *r, 
			   const unsigned long a1, const unsigned long a2, 
			   const unsigned long b)
{
  ASSERT(a2 < b); /* Or there will be quotient overflow */
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_div_2ul_ul_ul (%0, %1, %2, %3, %4)\n" : : 
           "X" (*q), "X" (*r), "X" (a1), "X" (a2), "X" (b));
#endif
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
  __asm__ __VOLATILE (
    "divq %4"
    : "=a" (*q), "=d" (*r)
    : "0" (a1), "1" (a2), "rm" (b)
    : "cc");
#elif defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "divl %4"
    : "=a" (*q), "=d" (*r)
    : "0" (a1), "1" (a2), "rm" (b)
    : "cc");
#else
  mp_limb_t A[2] = {a1, a2};
  ASSERT(sizeof(unsigned long) == sizeof(mp_limb_t));
  r[0] = mpn_divmod_1 (A, A, 2, b);
  q[0] = A[0];
#endif
}


/* Integer division of a two longint value by a longint divisor. Returns
   only remainder. */

static inline void
ularith_div_2ul_ul_ul_r (unsigned long *r, unsigned long a1,
                 const unsigned long a2, const unsigned long b)
{
  ASSERT(a2 < b); /* Or there will be quotient overflow */
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_div_2ul_ul_ul_r (%0, %1, %2, %3)\n" : : 
           "X" (*r), "X" (a1), "X" (a2), "X" (b));
#endif
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
  __asm__ __VOLATILE (
    "divq %3"
    : "+a" (a1), "=d" (*r)
    : "1" (a2), "rm" (b)
    : "cc");
#elif defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "divl %3"
    : "+a" (a1), "=d" (*r)
    : "1" (a2), "rm" (b)
    : "cc");
#else
  mp_limb_t A[2] = {a1, a2};
  ASSERT(sizeof(unsigned long) == sizeof(mp_limb_t));
  r[0] = mpn_divmod_1 (A, A, 2, b);
#endif
}


/* Set *r to lo shifted right by i bits, filling in the low bits from "hi" into the high
   bits of *r. I.e., *r = (hi * 2^64 + lo) / 2^i. Assumes 0 <= i < LONG_BIT. */
MAYBE_UNUSED
static inline void
ularith_shrd (unsigned long *r, const unsigned long hi, const unsigned long lo,
              const unsigned char i)
{
  ASSERT_EXPENSIVE (0 <= i && i < LONG_BIT);
#ifdef ULARITH_VERBOSE_ASM
/* Disable the "uninitialized" warning here, as *r is only written to and
   does not need to be initialized, but we need to write (*r) here so the
   "X" constraint can be resolved even when r does not have an address, e.g.,
   when it is passed around in a register. It seems that "X" is assumed by
   gcc as possibly referring to an input, and since "X" matches anything,
   that's probably a neccessary assumtion to make. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  __asm__ ("# ularith_shrd (*r=%0, hi=%1, lo=%2, i=%3)\n" : : 
           "X" (*r), "X" (hi), "X" (lo), "X" (i));
#pragma GCC diagnostic pop
#endif

#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
    "shrdq %3, %1, %0\n"
    : "=rm" (*r)
    : "r" (hi), "0" (lo), "cJ" (i) /* i can be in %cl or a literal constant < 64 */
    : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "shrdl %3, %1, %0\n # shift %0 right by %3 bits, shifting in the low bits from %1"
    : "=rm" (*r)
    : "r" (hi), "0" (lo), "cI" (i) /* i can be in %cl or a literal constant < 32 */
    : "cc");
#else
  if (i > 0) /* shl by LONG_BIT is no-op on x86! */
    *r = (lo >> i) | (hi << (LONG_BIT - i));
  else
    *r = lo;
#endif
}

/* Shift *r left by i bits, filling in the high bits from a into the low
   bits of *r. Assumes 0 <= i < LONG_BIT */
MAYBE_UNUSED
static inline void
ularith_shld (unsigned long *r, const unsigned long a, const int i)
{
  ASSERT_EXPENSIVE (0 <= i && i < LONG_BIT);
#ifdef ULARITH_VERBOSE_ASM
  __asm__ ("# ularith_shld (%0, %1, %2)\n" : : 
           "X" (*r), "X" (a), "X" (i));
#endif
#if !defined (ULARITH_NO_ASM) && defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  __asm__ __VOLATILE (
  "shldq %b2, %1, %0\n"
  /* the b modifier makes gcc print the byte part of the register (%cl) */
  : "+rm" (*r)
  : "r" (a), "cJ" (i) /* i can be in %cx or a constant < 64 */
  : "cc");
#elif !defined (ULARITH_NO_ASM) && defined(__i386__) && defined(__GNUC__)
  __asm__ __VOLATILE (
    "shldl %b2, %1, %0\n"
    : "+rm" (*r)
    : "r" (a), "cI" (i) /* i can be in %cx or a constant < 32 */
    : "cc");
#else
  if (i > 0) /* shr by LONG_BIT is no-op on x86! */
  *r = (*r << i) | (a >> (LONG_BIT - i));
#endif
}

/* Returns number of trailing zeros in a. a must not be zero */
MAYBE_UNUSED
static inline int
ularith_ctz (const unsigned long a)
{
#if !defined (ULARITH_NO_ASM) && defined(__GNUC__) && \
    (__GNUC__ >= 4 || __GNUC__ >= 3 && __GNUC_MINOR__ >= 4)
  ASSERT_EXPENSIVE (a != 0UL);
  return __builtin_ctzl(a);
#else
  static const unsigned char trailing_zeros[256] =
    {8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0};
  char lsh, t = 0;
  unsigned long y = a;
  ASSERT_EXPENSIVE (a != 0UL);
  do {
    lsh = trailing_zeros [(unsigned char) y];
    y >>= lsh;
    t += lsh;
  } while (lsh == 8);
  return (int) t;
#endif
}

/* Returns number of leading zeros in a. a must not be zero */
MAYBE_UNUSED
static inline int
ularith_clz (const unsigned long a)
{
#if !defined (ULARITH_NO_ASM) && defined(__GNUC__) && \
    (__GNUC__ >= 4 || __GNUC__ >= 3 && __GNUC_MINOR__ >= 4)
  ASSERT_EXPENSIVE (a != 0UL);
  return __builtin_clzl(a);
#else
  unsigned long t = 1UL << (LONG_BIT - 1);
  int i;
  ASSERT_EXPENSIVE (a != 0UL);
  for (i = 0; (a & t) == 0UL; i++)
    t >>= 1;
  return i;
#endif
}


/* Compute 1/n (mod 2^wordsize) */
MAYBE_UNUSED
static inline unsigned long
ularith_invmod (const unsigned long n)
{
  unsigned long r;

  ASSERT (n % 2UL != 0UL);
  
  /* Suggestion from PLM: initing the inverse to (3*n) XOR 2 gives the
     correct inverse modulo 32, then 3 (for 32 bit) or 4 (for 64 bit) 
     Newton iterations are enough. */
  r = (3UL * n) ^ 2UL;
  /* Newton iteration */
  r = 2UL * r - (unsigned int) r * (unsigned int) r * (unsigned int) n;
  r = 2UL * r - (unsigned int) r * (unsigned int) r * (unsigned int) n;
#if LONG_BIT == 32
  r = 2UL * r - r * r * n;
#else /* LONG_BIT == 32 */
  r = 2UL * r - (unsigned int) r * (unsigned int) r * (unsigned int) n;
#if 0
  r = 2UL * r - r * r * n;
#else
  /*
    r*n = k*2^32 + 1
        
    r' = 2 * r - r * r * n
    r' = 2 * r - r * (k*2^32 + 1)
    r' = 2 * r - r * k*2^32 - r
    r' = r - r * k*2^32
    r' = r - ((r * k) % 2^32) * 2^32
  */
  unsigned int k = (unsigned int)(r * n >> 32);
  k *= (unsigned int) r;
  r = r - ((unsigned long)k << 32);
#endif
#endif /* LONG_BIT == 32 */

  return r;
}

/* Compute n/2 (mod m), where m must be odd. */
static inline unsigned long
ularith_div2mod (const unsigned long n, const unsigned long m)
{
#if (defined(__i386__) && defined(__GNUC__)) || defined(HAVE_GCC_STYLE_AMD64_INLINE_ASM)
  unsigned long s = n, t = m;
  ASSERT_EXPENSIVE (m % 2UL != 0UL);

  __asm__ __VOLATILE(
	  "add %1, %0\n\t"
	  "rcr $1, %0\n\t"
	  "shr $1, %1\n\t"
	  "cmovnc %1, %0\n"
	  : "+&r" (t), "+&r" (s)
	  : : "cc"
	  );
  return t;
#else
  ASSERT_EXPENSIVE (m % 2UL != 0UL);
  if (n % 2UL == 0UL)
    return n / 2UL;
  else
    return n / 2UL + m / 2UL + 1UL;
#endif
}


/* Integer (truncated) square root of n */
static inline unsigned long
ularith_sqrt (const unsigned long n)
{
  unsigned int i;
  unsigned long xs, c, d, s2;
  const unsigned int l = (unsigned int)sizeof (unsigned long) * 8 - 1
                       - (unsigned int)__builtin_clzl(n);

  d = n; /* d = n - x^2 */
  xs = 0UL;
  s2 = 1UL << (l - l % 2);

  for (i = l / 2; i != 0; i--)
    {
      /* Here, s2 = 1 << (2*i) */
      /* xs = x << (i + 1), the value of x shifted left i+1 bits */

      c = xs + s2; /* c = (x + 2^i) ^ 2 - x^2 = 2^(i+1) * x + 2^(2*i) */
      xs >>= 1; /* Now xs is shifted only i positions */
      if (d >= c)
        {
          d -= c;
          xs |= s2; /* x |= 1UL << i <=> xs |= 1UL << (2*i) */
        }
      s2 >>= 2;
    }

  c = xs + s2;
  xs >>= 1;   
  if (d >= c) 
    xs |= s2;
  
  return xs;
}

/* Given r = -rem/p (mod den), we want num/(den*2^k) (mod p) ==
   (ratio + rem/den)/2^k (mod p).
   Using (a variant of) Bezout's identity, we have, for some non-negative
   integer t,
   r * p - t * den = -rem, or
   r * p + rem = t * den,
   thus den | (r * p + rem), and thus
   t = (r * p + rem) / den is an integer and satisfies
   t = rem/den (mod p).

   We have 0 <= r <= den-1 and rem <= den-1, and thus
   0 <= t = p * r/den + rem/den <=
   p (1 - 1/den) + 1 - 1/den =
   p + 1 - (p + 1)/den < p + 1.
   Thus t is almost a properly reduced residue for rem/den (mod p).
   As p fits in unsigned long, so does t, and we can compute t modulo
   2^LONGBITS; since den is odd, we can multiply by den^{-1} mod 2^LONGBITS
   to effect division by den.

   Finally we compute (t + ratio)/2^k mod p = num/(den*2^k) mod p.  */

static inline unsigned long
ularith_post_process_inverse(const unsigned long r, const unsigned long p,
  const unsigned long rem, const unsigned long den_inv,
  const unsigned long ratio, const unsigned long k)
{
  unsigned long t = (r * p + rem) * den_inv;
  const unsigned long ratio_p = (ratio >= p) ? ratio % p : ratio;
  ASSERT_ALWAYS(t <= p); /* Cheap and fairly strong test */
  /* ularith_addmod_ul_ul() accepts third operand == p and still produces
     a properly reduced sum mod p. */
  ularith_addmod_ul_ul (&t, ratio_p, t, p);

  ASSERT_EXPENSIVE(t < p);
  ASSERT_EXPENSIVE(k == 0 || p % 2 == 1);
  for (unsigned long j = 0; j < k; j++) {
    t = ularith_div2mod(t, p);
  }
  return t;
}

#endif /* ifndef UL_ARITH_H__ */
