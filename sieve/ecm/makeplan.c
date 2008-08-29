#include <stdlib.h>
#include <stdio.h>
#include <gmp.h>
#include "utils.h" /* for getprime() */
#include "pm1.h"
#include "pp1.h"
#include "ecm.h"
#include "prac_bc.h"

void 
pm1_make_plan (pm1_plan_t *plan, const unsigned int B1, const unsigned int B2,
	       int verbose)
{
  mpz_t E;
  unsigned int p;
  size_t tmp_E_nrwords;
  
  /* Generate the exponent for stage 1 */
  plan->B1 = B1;
  mpz_init (E);
  mpz_set_ui (E, 1UL);
  for (p = 2; p <= B1; p = (unsigned int) getprime (p))
    {
      unsigned long q;
      /* FIXME: use p^k s.t. (p-1)p^(k-1) <= B1 instead, except for
         p=2 because our base 2 is a QR for primes == 1 (mod 8) already */
      for (q = p; q * p < B1; q *= p);
      mpz_mul_ui (E, E, q);
    }
  
  if (verbose)
    gmp_printf ("pm1_make_plan: E = %Zd;\n", E);
  
  plan->E = mpz_export (NULL, &tmp_E_nrwords, -1, sizeof(unsigned long),
                        0, 0, E);
  plan->E_nrwords = (unsigned int) tmp_E_nrwords;
  mpz_clear (E);
  /* Find highest set bit in E. */
  ASSERT (plan->E[plan->E_nrwords - 1] != 0);
  plan->E_mask = ~0UL - (~0UL >> 1); /* Only MSB set */
  while ((plan->E[plan->E_nrwords - 1] & plan->E_mask) == 0UL)
    plan->E_mask >>= 1;
  getprime (0);
  
  stage2_make_plan (&(plan->stage2), B1, B2, verbose);
}


void 
pm1_clear_plan (pm1_plan_t *plan)
{
  stage2_clear_plan (&(plan->stage2));

  free (plan->E);
  plan->E = NULL;
  plan->E_nrwords = 0;
  plan->B1 = 0;
}


/* Make byte code for addition chain for stage 1, and the parameters for 
   stage 2 */

void 
pp1_make_plan (pp1_plan_t *plan, const unsigned int B1, const unsigned int B2,
	       int verbose)
{
  unsigned int p;
  const unsigned int addcost = 1, doublecost = 1;
  const unsigned int compress = 1;
  
  /* Make bytecode for stage 1 */
  plan->B1 = B1;
  bytecoder_init (compress);
  for (p = 2; p <= B1; p = (unsigned int) getprime (p))
    {
      unsigned long q;
      for (q = p; q <= B1; q *= p)
	prac_bytecode (p, addcost, doublecost);
    }
  bytecoder_flush ();
  plan->bc_len = bytecoder_size ();
  plan->bc = (char *) malloc (plan->bc_len);
  ASSERT (plan->bc);
  bytecoder_read (plan->bc);
  bytecoder_clear ();

  if (verbose)
    {
      printf ("Byte code for stage 1 (length %d): ", plan->bc_len);
      for (p = 0; p < plan->bc_len; p++)
	printf ("%s%d", (p == 0) ? "" : ", ", (int) (plan->bc[p]));
      printf ("\n");
    }
    
  /* Make stage 2 plan */
  stage2_make_plan (&(plan->stage2), B1, B2, verbose);
}

void 
pp1_clear_plan (pp1_plan_t *plan)
{
  stage2_clear_plan (&(plan->stage2));
  free (plan->bc);
  plan->bc = NULL;
  plan->bc_len = 0;
  plan->B1 = 0;
}


/* Make byte code for addition chain for stage 1, and the parameters for 
   stage 2 */

void 
ecm_make_plan (ecm_plan_t *plan, const unsigned int B1, const unsigned int B2,
	       const int parameterization, const unsigned long sigma, 
	       const int verbose)
{
  unsigned int p, q;
  const unsigned int addcost = 6, doublecost = 5; /* TODO: find good ratio */
  const unsigned int compress = 0;
  
  plan->exp2 = 0;
  for (q = 1; q <= B1 / 2; q *= 2)
    plan->exp2++;
  
  /* Make bytecode for stage 1 */
  plan->B1 = B1;
  plan->parameterization = parameterization;
  plan->sigma = sigma;
  bytecoder_init (compress);
  p = (unsigned int) getprime (2UL);
  ASSERT (p == 3);
  for ( ; p <= B1; p = (unsigned int) getprime (p))
    {
      for (q = 1; q <= B1 / p; q *= p)
	prac_bytecode (p, addcost, doublecost);
    }
  bytecoder_flush ();
  plan->bc_len = bytecoder_size ();
  plan->bc = (char *) malloc (plan->bc_len);
  ASSERT (plan->bc);
  bytecoder_read (plan->bc);
  bytecoder_clear ();
  getprime (0);

  if (verbose)
    {
      printf ("Byte code for stage 1 (length %d): ", plan->bc_len);
      for (p = 0; p < plan->bc_len; p++)
	printf ("%s%d", (p == 0) ? "" : ", ", (int) (plan->bc[p]));
      printf ("\n");
    }
    
  /* Make stage 2 plan */
  stage2_make_plan (&(plan->stage2), B1, B2, verbose);
}

void 
ecm_clear_plan (ecm_plan_t *plan)
{
  stage2_clear_plan (&(plan->stage2));
  free (plan->bc);
  plan->bc = NULL;
  plan->bc_len = 0;
  plan->B1 = 0;
}
