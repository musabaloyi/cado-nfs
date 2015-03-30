#ifndef	CADO_UTILS_H_
#define	CADO_UTILS_H_

#include "mod_ul.h"
#include <stdint.h>

#include <limits.h>

/* It's awful, but I confess that this ULONG_BITS is not ``portable'',
 * norm-wise.  GMP_LIMB_BITS is at hand, but could differ. An #ifdef
 * switch depending on macros like __x86_64 is considerably more fragile.
 *
 * Now that cmake build cado, en external test could set this. Alas, I've yet
 * to decide on what I want for this test.
 */
#define	ULONG_BITS	((int) (sizeof(unsigned long) * CHAR_BIT))

#define RELATION_MAX_BYTES 4096

#include "modul_poly.h"
#include "double_poly.h"
#include "getprime.h"
#include "timing.h"
#include "gmp_aux.h"
#include "cado_poly.h"
#include "rootfinder.h"
#include "params.h"
#include "gcd.h"
#include "discriminant.h"
#include "mpz_array.h"
#include "gzip.h"
#include "misc.h"
#include "mpz_poly.h"
#include "crc.h"
#include "usp.h"
#include "purgedfile.h"
#include "bit_vector.h"
#include "fix-endianness.h"
#include "memusage.h"
#include "bit_vector.h"
#include "renumber.h"
#include "cado_popen.h"
#include "sm_utils.h"
#include "memalloc.h"
#include "mpz_vector.h"
#include "memory.h"
#include "relation-tools.h"
#include "stats.h"
#include "lll.h"
#include "verbose.h"
#include "lognorm_skewness.h"
#include "mpz_poly.h"
#include "version_info.h"

#endif	/* CADO_UTILS_H_ */
