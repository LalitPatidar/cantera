/*  $Author$
 *  $Date$
 *  $Revision$
 */

/*
 * Copywrite (2005) Sandia Corporation. Under the terms of 
 * Contract DE-AC04-94AL85000 with Sandia Corporation, the
 * U.S. Government retains certain rights in this software.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "vcs_solve.h"
#include "vcs_internal.h" 
#include "vcs_VolPhase.h"

namespace VCSnonideal {
   
  /*
   * vcs_elem_rearrange:
   *
   *       This subroutine handles the rearrangement of the constraint
   *    equations represented by the Formula Matrix. Rearrangement is only
   *    necessary when the number of components is less than the number of
   *    elements. For this case, some constraints can never be satisfied 
   *    exactly, because the range space represented by the Formula
   *    Matrix of the components can't span the extra space. These 
   *    constraints, which are out of the range space of the component
   *    Formula matrix entries, are migrated to the back of the Formula
   *    matrix.
   *
   *       A prototypical example is an extra element column in 
   *    FormulaMatrix[], 
   *    which is identically zero. For example, let's say that argon is
   *    has an element column in FormulaMatrix[], but no species in the 
   *     mechanism
   *    actually contains argon. Then, nc < ne. Also, without perturbation
   *    of FormulaMatrix[] vcs_basopt[] would produce a zero pivot 
   *    because the matrix
   *    would be singular (unless the argon element column was already the
   *    last column of  FormulaMatrix[]. 
   *       This routine borrows heavily from vcs_basopt's algorithm. It 
   *    finds nc constraints which span the range space of the Component
   *    Formula matrix, and assigns them as the first nc components in the
   *    formular matrix. This guarrantees that vcs_basopt[] has a
   *    nonsingular matrix to invert.
   *
   * Other Variables 
   *  aw[i] = Mole fraction work space        (ne in length)
   *  sa[j] = Gramm-Schmidt orthog work space (ne in length)
   *  ss[j] = Gramm-Schmidt orthog work space (ne in length)
   *  sm[i+j*ne] = QR matrix work space (ne*ne in length)
   *
   */
int VCS_SOLVE::vcs_elem_rearrange(double *aw, double *sa, double *sm,
				  double *ss) {
   int  j, k, l, i, jl, ml, jr, lindep, ielem;
   int ncomponents = m_numComponents;
   double test = -1.0E10;
#ifdef DEBUG
   if (vcs_debug_print_lvl >= 2) {
     plogf("   "); for(i=0; i<77; i++) plogf("-"); plogf("\n");
     plogf("   --- Subroutine elem_rearrange() called to ");
     plogf("check stoich. coefficent matrix\n");
     plogf("   ---    and to rearrange the element ordering once\n");
   }
#endif
   
   /* 
   *        Use a temporary work array for the element numbers 
   *        Also make sure the value of test is unique.
   */
   lindep = FALSE;
   do {
      lindep = FALSE;
      for (i = 0; i < m_numElemConstraints; ++i) {
	test -= 1.0;
	aw[i] = gai[i];
	if (test == aw[i]) lindep = TRUE;
      }
   } while (lindep);
   
   /*
   *        Top of a loop of some sort based on the index JR. JR is the 
   *       current number independent elements found. 
   */
   jr = -1;
   do {
      ++jr;
      /* 
      *     Top of another loop point based on finding a linearly 
      *     independent species
      */
      do {
	 /*
	 *    Search the remaining part of the mole fraction vector, AW, 
	 *    for the largest remaining species. Return its identity in K. 
	 */
	 k = m_numElemConstraints;
	 for (ielem = jr; ielem < m_numElemConstraints; ielem++) {
	   if (ElActive[ielem]) {
	     if (aw[ielem] != test) {
	       k = ielem;
	       break;
	     }
	   }
	 }
	 if (k == m_numElemConstraints) {
	   plogf("Shouldn't be here\n");
	   exit(-1);
	 }
	 
	 /*
	 *  Assign a large negative number to the element that we have
	 *  just found, in order to take it out of further consideration.
	 */
	 aw[k] = test;
	 
	 /* *********************************************************** */
	 /* **** CHECK LINEAR INDEPENDENCE OF CURRENT FORMULA MATRIX    */
	 /* **** LINE WITH PREVIOUS LINES OF THE FORMULA MATRIX  ****** */
	 /* *********************************************************** */
	 /*    
	 *          Modified Gram-Schmidt Method, p. 202 Dalquist 
	 *          QR factorization of a matrix without row pivoting. 
	 */
	 jl = jr;
	 /*
	 *   Fill in the row for the current element, k, under consideration
	 *   The row will contain the Formula matrix value for that element
	 *   from the current component.
	 */
	 for (j = 0; j < ncomponents; ++j) {
	    sm[j + jr*ncomponents] = FormulaMatrix[k][j];
	 } 
	 if (jl > 0) {
	    /*
	    *         Compute the coefficients of JA column of the 
	    *         the upper triangular R matrix, SS(J) = R_J_JR 
	    *         (this is slightly different than Dalquist) 
	    *         R_JA_JA = 1 
	    */
	    for (j = 0; j < jl; ++j) {
	       ss[j] = 0.0;
	       for (i = 0; i < ncomponents; ++i) {
		  ss[j] += sm[i + jr*ncomponents] * sm[i + j*ncomponents];
	       }
	       ss[j] /= sa[j];
	    }
	    /* 
	    *     Now make the new column, (*,JR), orthogonal to the 
	    *     previous columns
	    */
	    for (j = 0; j < jl; ++j) {
	       for (l = 0; l < ncomponents; ++l) {
		  sm[l + jr*ncomponents] -= ss[j] * sm[l + j*ncomponents];
	       }
	    }
	 }
	 
	 /*
	 *        Find the new length of the new column in Q. 
	 *        It will be used in the denominator in future row calcs. 
	 */
	 sa[jr] = 0.0;
	 for (ml = 0; ml < ncomponents; ++ml) {
	    sa[jr] += SQUARE(sm[ml + jr*ncomponents]);
	 }
	 /* **************************************************** */
	 /* **** IF NORM OF NEW ROW  .LT. 1E-6 REJECT ********** */
	 /* **************************************************** */
	 if (sa[jr] < 1.0e-6)  lindep = TRUE;
	 else                  lindep = FALSE;
      } while(lindep);
      /* ****************************************** */
      /* **** REARRANGE THE DATA ****************** */
      /* ****************************************** */
      if (jr != k) {
#ifdef DEBUG
	if (vcs_debug_print_lvl >= 2) {
	  plogf("   ---   "); plogf("%-2.2s", (ElName[k]).c_str());
	  plogf("(%9.2g) replaces ", gai[k]);
	  plogf("%-2.2s", (ElName[jr]).c_str());
	  plogf("(%9.2g) as element %3d\n", gai[jr], jr);
	}
#endif
	 vcs_switch_elem_pos(jr, k);
	 vcsUtil_dsw(aw, jr, k);
      }
  
      /*
      *      If we haven't found enough components, go back 
      *      and find some more. (nc -1 is used below, because
      *      jr is counted from 0, via the C convention.
      */
   } while (jr < (ncomponents-1));
   return VCS_SUCCESS;
} /* vcs_elem_rearrange() ****************************************************/

//  Swaps the indecises for all of the global data for two elements, ipos
//  and jpos.
/*
 *  This function knows all of the element information with VCS_SOLVE, and
 *  can therefore switch element positions
 *
 *  @param ipos  first global element index
 *  @param jpos  second global element index
 */
void VCS_SOLVE::vcs_switch_elem_pos(int ipos, int jpos) {
   if (ipos == jpos) return;
   int j;
   double dtmp;
   vcs_VolPhase *volPhase;
#ifdef DEBUG
   if (ipos < 0 || ipos > (m_numElemConstraints - 1) ||
       jpos < 0 || jpos > (m_numElemConstraints - 1)    ) {
      plogf("vcs_switch_elem_pos: ifunc = 0: inappropriate args: %d %d\n",
	     ipos, jpos);
   }
#endif  
   /*
    * Change the element Global Index list in each phase object
    * to reflect the switch in the element positions.
    */
   for (int iph = 0; iph < NPhase; iph++) {
     volPhase = VPhaseList[iph];
     for (int e = 0; e < volPhase->nElemConstraints; e++) {
       if (volPhase->ElGlobalIndex[e] == ipos) {
          volPhase->ElGlobalIndex[e] = jpos; 
       }
       if (volPhase->ElGlobalIndex[e] == jpos) {
          volPhase->ElGlobalIndex[e] =ipos; 
       }
     } 
   }
   vcsUtil_dsw(VCS_DATA_PTR(gai), ipos, jpos);
   vcsUtil_dsw(VCS_DATA_PTR(ga), ipos, jpos);
   vcsUtil_isw(VCS_DATA_PTR(IndEl),     ipos, jpos);
   vcsUtil_isw(VCS_DATA_PTR(m_elType),  ipos, jpos);
   vcsUtil_isw(VCS_DATA_PTR(ElActive),  ipos, jpos);
   for (j = 0; j < m_numSpeciesTot; ++j) {
      SWAP(FormulaMatrix[ipos][j], FormulaMatrix[jpos][j], dtmp);
   }
   vcsUtil_stsw(ElName, ipos, jpos);
} /* vcs_switch_elem_pos() ***************************************************/
}

