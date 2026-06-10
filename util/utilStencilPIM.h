// Util: Stencil PIM utilities
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef PIM_FUNC_SIM_APPS_UTIL_STENCIL_PIM_H
#define PIM_FUNC_SIM_APPS_UTIL_STENCIL_PIM_H

#include "utilStencil.h"
#include "libpimeval.h"


//! @brief  Sums the neighbors of each element in a stencil row to compute the horizontal stencil sum
//!
//! Sums radius number of elements to the left and right of center element, including center element
//! Puts each result pimRowSum[i] where i is the center index
//! Formula: pimRowSum[i] = Σ (j ∈ [i-radius, i+radius]) mid[j]
//! Works by shifting mid to the left and right and adding shifted versions
//! @param[in]  mid  PIM row to be summed
//! @param[out]  pimRowSum  The resultant PIM object to place the sum into
//! @param[in,out]  shiftBackup  Temporary PIM object used for calculations
//! @param[in]  radius  The stencil radius
void sumStencilRow(PimObjId mid, PimObjId pimRowSum, PimObjId shiftBackup, const uint64_t radius) {
  PimStatus status;

  if(radius == 0) {
    return;
  }

  status = pimCopyObjectToObject(mid, shiftBackup);
  assert (status == PIM_OK);

  status = pimShiftElementsRight(shiftBackup);
  assert (status == PIM_OK);

  status = pimAdd(mid, shiftBackup, pimRowSum);
  assert (status == PIM_OK);

  for(uint64_t shiftIter=1; shiftIter<radius; ++shiftIter) {
    status = pimShiftElementsRight(shiftBackup);
    assert (status == PIM_OK);

    status = pimAdd(pimRowSum, shiftBackup, pimRowSum);
    assert (status == PIM_OK);
  }

  status = pimCopyObjectToObject(mid, shiftBackup);
  assert (status == PIM_OK);

  for(uint64_t shiftIter=0; shiftIter<radius; ++shiftIter) {
    status = pimShiftElementsLeft(shiftBackup);
    assert (status == PIM_OK);

    status = pimAdd(pimRowSum, shiftBackup, pimRowSum);
    assert (status == PIM_OK);
  }
}

#endif