/**
 * @file vcs_elem.cpp
 */
/*
 * $Id$
 */

#include "vcs_solve.h"
#include "vcs_internal.h" 
#include "math.h"

namespace VCSnonideal {

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

void VCS_SOLVE::vcs_elab(void)
   
  /*************************************************************************
   *
   * vcs_elab:
   *
   *  Computes the elemental abundances vector, ga[], and stores it
   *  back into the global structure
   *************************************************************************/
{
  for (int j = 0; j < m_numElemConstraints; ++j) {
    ga[j] = 0.0;
    for (int i = 0; i < m_numSpeciesTot; ++i) {
      if (SpeciesUnknownType[i] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	ga[j] += FormulaMatrix[j][i] * soln[i];
      }
    }
  }
}
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/ 
/*
 *
 * vcs_elabcheck:
 *
 *   This function checks to see if the element abundances are in 
 *   compliance. If they are, then TRUE is returned. If not, 
 *   FALSE is returned. Note the number of constraints checked is 
 *   usually equal to the number of components in the problem. This 
 *   routine can check satisfaction of all of the constraints in the
 *   problem, which is equal to ne. However, the solver can't fix
 *   breakage of constraints above nc, because that nc is the
 *   range space by definition. Satisfaction of extra constraints would
 *   have had to occur in the problem specification. 
 *
 *   The constraints should be broken up into 2  sections. If 
 *   a constraint involves a formula matrix with positive and 
 *   negative signs, and eaSet = 0.0, then you can't expect that the
 *   sum will be zero. There may be roundoff that inhibits this.
 *   However, if the formula matrix is all of one sign, then
 *   this requires that all species with nonzero entries in the
 *   formula matrix be identically zero. We put this into
 *   the logic below.
 *
 * Input
 * -------
 *   ibound = 1 : Checks constraints up to the number of elements
 *            0 : Checks constraints up to the number of components.
 *
 */
int VCS_SOLVE::vcs_elabcheck(int ibound) {
  int i; 
  int top = m_numComponents;
  double eval, scale;
  int numNonZero;
  bool multisign = false;
  if (ibound) {
    top = m_numElemConstraints;  
  }
  /*
   * Require 12 digits of accuracy on non-zero constraints.
   */
  for (i = 0; i < top; ++i) {
    if (fabs(ga[i] - gai[i]) > (fabs(gai[i]) * 1.0e-12)) {
      /*
       * This logic is for charge neutrality condition
       */
      if (m_elType[i] == VCS_ELEM_TYPE_CHARGENEUTRALITY) {
	AssertThrowVCS(gai[i] == 0.0, "vcs_elabcheck");
      }
      if (gai[i] == 0.0 || (m_elType[i] == VCS_ELEM_TYPE_ELECTRONCHARGE)) {
	scale = VCS_DELETE_MINORSPECIES_CUTOFF;
	/*
	 * Find out if the constraint is a multisign constraint.
	 * If it is, then we have to worry about roundoff error
	 * in the addition of terms. We are limited to 13
	 * digits of finite arithmetic accuracy.
	 */
	numNonZero = 0;
	multisign = false;
	for (int kspec = 0; kspec < m_numSpeciesTot; kspec++) {
	  eval = FormulaMatrix[i][kspec];
	  if (eval < 0.0) {
	    multisign = true;
	  }
	  if (eval != 0.0) {
	    scale = MAX(scale, fabs(eval * soln[kspec]));
	    numNonZero++;
	  }
	}
	if (multisign) {
	  if (fabs(ga[i] - gai[i]) > 1e-11 * scale) {
	    return FALSE;
	  }
	} else {
	  if (fabs(ga[i] - gai[i]) > VCS_DELETE_MINORSPECIES_CUTOFF) {
	    return FALSE;
	  }
	}
      } else {
	/*
	 * For normal element balances, we require absolute compliance
	 * even for rediculously small numbers.
	 */
	if (m_elType[i] == VCS_ELEM_TYPE_ABSPOS) {
	  return FALSE;
	} else {
	  return FALSE;
	}
      }
    }
  }
  return TRUE;
} /* vcs_elabcheck() *********************************************************/

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

void VCS_SOLVE::vcs_elabPhase(int iphase, double * const elemAbundPhase)
   
  /*************************************************************************
   *
   * vcs_elabPhase:
   *
   *  Computes the elemental abundances vector for a single phase,
   *  elemAbundPhase[], and returns it through the argument list.
   *  The mole numbers of species are taken from the current value
   *  in soln[].
   *************************************************************************/
{
  int i, j;
  for (j = 0; j < m_numElemConstraints; ++j) {
    elemAbundPhase[j] = 0.0;
    for (i = 0; i < m_numSpeciesTot; ++i) {
      if (SpeciesUnknownType[i] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	if (PhaseID[i] == iphase) {
	  elemAbundPhase[j] += FormulaMatrix[j][i] * soln[i];
	}
      }
    }
  }
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

int VCS_SOLVE::vcs_elcorr(double aa[], double x[])
   
  /**************************************************************************
   *
   * vcs_elcorr:
   *
   * This subroutine corrects for element abundances. At the end of the 
   * surbroutine, the total moles in all phases are recalculated again,
   * because we have changed the number of moles in this routine.
   *
   * Input
   *  -> temporary work vectors:
   *     aa[ne*ne]
   *     x[ne]
   *
   * Return Values:
   *      0 = Nothing of significance happened,   
   *          Element abundances were and still are good.
   *      1 = The solution changed significantly; 
   *          The element abundances are now good.
   *      2 = The solution changed significantly,
   *          The element abundances are still bad.
   *      3 = The solution changed significantly,
   *          The element abundances are still bad and a component 
   *          species got zeroed out.
   *
   *  Internal data to be worked on::
   *
   *  ga    Current element abundances
   *  gai   Required elemental abundances
   *  soln     Current mole number of species.
   *  FormulaMatrix[][]  Formular matrix of the species
   *  ne    Number of elements
   *  nc    Number of components.
   *
   * NOTES:
   *  This routine is turning out to be very problematic. There are 
   *  lots of special cases and problems with zeroing out species.
   *
   *  Still need to check out when we do loops over nc vs. ne.
   *  
   *************************************************************************/
{
  int i, j, retn = 0, kspec, goodSpec, its; 
  double xx, par, saveDir, dir;

#ifdef DEBUG
  double l2before = 0.0, l2after = 0.0;
  std::vector<double> ga_save(m_numElemConstraints, 0.0);
  vcs_dcopy(VCS_DATA_PTR(ga_save), VCS_DATA_PTR(ga), m_numElemConstraints);
  if (vcs_debug_print_lvl >= 2) {
    plogf("   --- vcsc_elcorr: Element abundances correction routine");
    if (m_numElemConstraints != m_numComponents) {
      plogf(" (m_numComponents != m_numElemConstraints)");
    }
    plogf("\n");
  }

  for (i = 0; i < m_numElemConstraints; ++i) {
    x[i] = ga[i] - gai[i];
  }
  l2before = 0.0;
  for (i = 0; i < m_numElemConstraints; ++i) {
    l2before += x[i] * x[i];
  }
  l2before = sqrt(l2before/m_numElemConstraints);
#endif

  /*
   * Special section to take out single species, single component,
   * moles. These are species which have non-zero entries in the
   * formula matrix, and no other species have zero values either.
   *
   */
  int numNonZero = 0;
  bool changed = false;
  bool multisign = false;
  for (i = 0; i < m_numElemConstraints; ++i) {
    numNonZero = 0;
    multisign = false;
    for (kspec = 0; kspec < m_numSpeciesTot; kspec++) {
      if (SpeciesUnknownType[kspec] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	double eval = FormulaMatrix[i][kspec];
	if (eval < 0.0) {
	  multisign = true;
	}
	if (eval != 0.0) {
	  numNonZero++;
	}
      }
    }
    if (!multisign) {
      if (numNonZero < 2) {
	for (kspec = 0; kspec < m_numSpeciesTot; kspec++) {
	  if (SpeciesUnknownType[kspec] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	    double eval = FormulaMatrix[i][kspec];
	    if (eval > 0.0) {
	      soln[kspec] = gai[i] / eval;
	      changed = true;
	    }
	  }
	}
      } else {
	int numCompNonZero = 0;
	int compID = -1;
	for (kspec = 0; kspec < m_numComponents; kspec++) {
	  if (SpeciesUnknownType[kspec] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	    double eval = FormulaMatrix[i][kspec];
	    if (eval > 0.0) {
	      compID = kspec;
	      numCompNonZero++;
	    }
	  }
	}
	if (numCompNonZero == 1) {
	  double diff = gai[i];
	  for (kspec = m_numComponents; kspec < m_numSpeciesTot; kspec++) {
	    if (SpeciesUnknownType[kspec] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	      double eval = FormulaMatrix[i][kspec];
	      diff -= eval * soln[kspec];
	    }
	    soln[compID] = MAX(0.0,diff/FormulaMatrix[i][compID]);
	    changed = true;
	  }
	}
      }
    }
  }
  if (changed) {
    vcs_elab();
  }

  /*
   *  Section to check for maximum bounds errors on all species
   *  due to elements.
   *    This may only be tried on element types which are VCS_ELEM_TYPE_ABSPOS.
   *    This is because no other species may have a negative number of these.
   *
   *  Note, also we can do this over ne, the number of elements, not just
   *  the number of components.
   */
  changed = false;
  for (i = 0; i < m_numElemConstraints; ++i) {
    int elType = m_elType[i];
    if (elType == VCS_ELEM_TYPE_ABSPOS) {
      for (kspec = 0; kspec < m_numSpeciesTot; kspec++) {
	if (SpeciesUnknownType[kspec] != VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	  double atomComp = FormulaMatrix[i][kspec];
	  if (atomComp > 0.0) {
	    double maxPermissible = gai[i] / atomComp;
	    if (soln[kspec] > maxPermissible) {
	      
#ifdef DEBUG
	      if (vcs_debug_print_lvl >= 3) {
		plogf("  ---  vcs_elcorr: Reduced species %s from %g to %g due to %s max bounds constraint\n",
		       SpName[kspec].c_str(), soln[kspec], maxPermissible, ElName[i].c_str());
	      }
#endif
	      soln[kspec] = maxPermissible;
	      changed = true;
	      if (soln[kspec] < VCS_DELETE_MINORSPECIES_CUTOFF) {
		soln[kspec] = 0.0;
		if (SSPhase[kspec]) {
		  spStatus[kspec] =  VCS_SPECIES_ZEROEDSS;
		} else {
		  spStatus[kspec] =  VCS_SPECIES_ZEROEDMS;
		} 
#ifdef DEBUG
		if (vcs_debug_print_lvl >= 2) {
		  plogf("  ---  vcs_elcorr: Zeroed species %s and changed status to %d due to max bounds constraint\n",
			 SpName[kspec].c_str(), spStatus[kspec]);
		}
#endif
	      }
	    }
	  }
	}
      }
    }
  }

  // Recalculate the element abundances if something has changed.
  if (changed) {
    vcs_elab();
  }

  /*
   * Ok, do the general case. Linear algebra problem is 
   * of length nc, not ne, as there may be degenerate rows when
   * nc .ne. ne.
   */
  for (i = 0; i < m_numComponents; ++i) {
    x[i] = ga[i] - gai[i];
    if (fabs(x[i]) > 1.0E-13) retn = 1;
    for (j = 0; j < m_numComponents; ++j) {
      aa[j + i*m_numElemConstraints] = FormulaMatrix[j][i];
    }
  }
  i = vcsUtil_mlequ(aa, m_numElemConstraints, m_numComponents, x, 1);
  if (i == 1) {
    plogf("vcs_elcorr ERROR: mlequ returned error condition\n");
    return VCS_FAILED_CONVERGENCE;
  }
  /*
   * Now apply the new direction without creating negative species.
   */
  par = 0.5;
  for (i = 0; i < m_numComponents; ++i) {
    if (soln[i] > 0.0) {
      xx = -x[i] / soln[i];
      if (par < xx) par = xx;
    }
  }
  if (par > 100.0) {
    par = 100.0;
  }
  par = 1.0 / par;
  if (par < 1.0 && par > 0.0) {
    retn = 2;
    par *= 0.9999;
    for (i = 0; i < m_numComponents; ++i) {
      double tmp = soln[i] + par * x[i];
      if (tmp > 0.0) {
	soln[i] = tmp;
      } else {
	if (SSPhase[i]) {
	  soln[i] =  0.0;
	}  else {
	  soln[i] = soln[i] * 0.0001;
	}
      }
    }
  } else {
    for (i = 0; i < m_numComponents; ++i) {
      double tmp = soln[i] + x[i];
      if (tmp > 0.0) {
	soln[i] = tmp;
      } else { 
	if (SSPhase[i]) {
	  soln[i] =  0.0;
	}  else {
	  soln[i] = soln[i] * 0.0001;
	}
      }
    }
  }
   
  /*
   *   We have changed the element abundances. Calculate them again
   */
  vcs_elab();
  /*
   *   We have changed the total moles in each phase. Calculate them again
   */
  vcs_tmoles();
    
  /*
   *       Try some ad hoc procedures for fixing the problem
   */
  if (retn >= 2) {
    /*
     *       First find a species whose adjustment is a win-win
     *       situation.
     */
    for (kspec = 0; kspec < m_numSpeciesTot; kspec++) {
      if (SpeciesUnknownType[kspec] == VCS_SPECIES_TYPE_INTERFACIALVOLTAGE) {
	continue;
      }
      saveDir = 0.0;
      goodSpec = TRUE;
      for (i = 0; i < m_numComponents; ++i) {
	dir = FormulaMatrix[i][kspec] *  (gai[i] - ga[i]);
	if (fabs(dir) > 1.0E-10) {
	  if (dir > 0.0) {
	    if (saveDir < 0.0) {
	      goodSpec = FALSE;
	      break;
	    }
	  } else {
	    if (saveDir > 0.0) {
	      goodSpec = FALSE;
	      break;
	    }		   
	  }
	  saveDir = dir;
	} else {
	  if (FormulaMatrix[i][kspec] != 0.) {
	    goodSpec = FALSE;
	    break;
	  }
	}
      }
      if (goodSpec) {
	its = 0;
	xx = 0.0;
	for (i = 0; i < m_numComponents; ++i) {
	  if (FormulaMatrix[i][kspec] != 0.0) {
	    xx += (gai[i] - ga[i]) / FormulaMatrix[i][kspec];
	    its++;
	  }
	}
	if (its > 0) xx /= its;
	soln[kspec] += xx;
	soln[kspec] = MAX(soln[kspec], 1.0E-10);
	/*
	 *   If we are dealing with a deleted species, then
	 *   we need to reinsert it into the active list.
	 */
	if (kspec >= m_numSpeciesRdc) {
	  vcs_reinsert_deleted(kspec);	
	  soln[m_numSpeciesRdc - 1] = xx;
	  vcs_elab();
	  goto L_CLEANUP;
	}
	vcs_elab();  
      }
    }     
  }
  if (vcs_elabcheck(0)) {
    retn = 1;
    goto L_CLEANUP;
  }
  
  for (i = 0; i < m_numElemConstraints; ++i) {
    if (m_elType[i] == VCS_ELEM_TYPE_CHARGENEUTRALITY ||
	(m_elType[i] == VCS_ELEM_TYPE_ABSPOS && gai[i] == 0.0)) { 
      for (kspec = 0; kspec < m_numSpeciesRdc; kspec++) {
	if (ga[i] > 0.0) {
	  if (FormulaMatrix[i][kspec] < 0.0) {
	    soln[kspec] -= ga[i] / FormulaMatrix[i][kspec] ;
	    if (soln[kspec] < 0.0) {
	      soln[kspec] = 0.0;
	    }
	    vcs_elab();
	    break;
	  }
	}
	if (ga[i] < 0.0) {
	  if (FormulaMatrix[i][kspec] > 0.0) {
	    soln[kspec] -= ga[i] / FormulaMatrix[i][kspec];
	    if (soln[kspec] < 0.0) {
	      soln[kspec] = 0.0;
	    }
	    vcs_elab();
	    break;
	  }
	}
      }
    }
  }
  if (vcs_elabcheck(1)) {
    retn = 1;      
    goto L_CLEANUP;
  }

  /*
   *  For electron charges element types, we try positive deltas
   *  in the species concentrations to match the desired
   *  electron charge exactly.
   */
 for (i = 0; i < m_numElemConstraints; ++i) {
   double dev = gai[i] - ga[i];
   if (m_elType[i] == VCS_ELEM_TYPE_ELECTRONCHARGE && (fabs(dev) > 1.0E-300)) {
     bool useZeroed = true;
     for (kspec = 0; kspec < m_numSpeciesRdc; kspec++) {
       if (dev < 0.0) {
	 if (FormulaMatrix[i][kspec] < 0.0) {
	   if (soln[kspec] > 0.0) {
	     useZeroed = false;
	   }
	 }
       } else {
	 if (FormulaMatrix[i][kspec] > 0.0) {
	   if (soln[kspec] > 0.0) {
	     useZeroed = false;
	   }
	 }
       }
     }
     for (kspec = 0; kspec < m_numSpeciesRdc; kspec++) {
	if (soln[kspec] > 0.0 || useZeroed) {
	  if (dev < 0.0) {
	    if (FormulaMatrix[i][kspec] < 0.0) {
	      double delta = dev / FormulaMatrix[i][kspec] ;
	      soln[kspec] += delta;
	      if (soln[kspec] < 0.0) {
		soln[kspec] = 0.0;
	      }
	      vcs_elab();
	      break;
	    }
	  }
	  if (dev > 0.0) {
	    if (FormulaMatrix[i][kspec] > 0.0) {
	      double delta = dev / FormulaMatrix[i][kspec] ;
	      soln[kspec] += delta;
	      if (soln[kspec] < 0.0) {
		soln[kspec] = 0.0;
	      }
	      vcs_elab();
	      break;
	    }
	  }
	}
      }
    }
  }
  if (vcs_elabcheck(1)) {
    retn = 1;      
    goto L_CLEANUP;
  }

 L_CLEANUP: ;
  vcs_tmoles();
#ifdef DEBUG
  l2after = 0.0;
  for (i = 0; i < m_numElemConstraints; ++i) {
    l2after += SQUARE(ga[i] - gai[i]);
  }
  l2after = sqrt(l2after/m_numElemConstraints);
  if (vcs_debug_print_lvl >= 2) {
    plogf("   ---    Elem_Abund:  Correct             Initial  "
	   "              Final\n");
    for (i = 0; i < m_numElemConstraints; ++i) {
      plogf("   ---       "); plogf("%-2.2s", ElName[i].c_str());
      plogf(" %20.12E %20.12E %20.12E\n", gai[i], ga_save[i], ga[i]);
    }
    plogf("   ---            Diff_Norm:         %20.12E %20.12E\n",
	   l2before, l2after);
  }
#endif
  return retn;
} /* vcs_elcorr() ************************************************************/

}

