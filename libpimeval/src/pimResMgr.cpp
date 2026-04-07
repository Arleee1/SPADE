// File: pimResMgr.cpp
// PIMeval Simulator - PIM Resource Manager
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "pimResMgr.h"            // for pimResMgr
#include "pimDevice.h"            // for pimDevice
#include "gridLayoutOptimizer.h"  // for optimizeGridLayout
#include "gridLogger.h"           // for logGridCoreLocations2D
#include <cstdio>                 // for printf
#include <algorithm>              // for sort, prev
#include <stdexcept>              // for throw, invalid_argument
#include <memory>                 // for make_unique
#include <cassert>                // for assert
#include <string>                 // for string


//! @brief  Print info of a PIM region
void
pimRegion::print(uint64_t regionId) const
{
  printf("{ PIM-Region %lu: CoreId = %d, Loc = (%u, %u), Size = (%u, %u) }\n",
         regionId, m_coreId, m_rowIdx, m_colIdx, m_numAllocRows, m_numAllocCols);
}

//! @brief  Print info of a PIM object
void
pimObjInfo::print() const
{
  std::printf("----------------------------------------\n");
  std::printf("PIM-Object: ObjId = %d, AllocType = %d, Regions =\n",
              m_objId, static_cast<int>(m_allocType));
  for (size_t i = 0; i < m_regions.size(); ++i) {
    m_regions[i].print(i);
  }
  std::printf("----------------------------------------\n");
}

//! @brief  Finalize obj info
void
pimObjInfo::finalize()
{
  std::unordered_map<PimCoreId, int> coreIdCnt;
  for (const auto& region : m_regions) {
    PimCoreId coreId = region.getCoreId();
    coreIdCnt[coreId]++;
    unsigned numRegionsPerCore = coreIdCnt[coreId];
    if (m_maxNumRegionsPerCore < numRegionsPerCore) {
      m_maxNumRegionsPerCore = numRegionsPerCore;
    }
  }
  m_numCoresUsed = coreIdCnt.size();
  m_numCoreAvailable = m_device->getNumCores();
  m_isLoadBalanced = m_device->getConfig().isLoadBalanced();

  const pimRegion& region = m_regions[0];
  m_maxElementsPerRegion = (uint64_t)region.getNumAllocRows() * region.getNumAllocCols() / m_bitsPerElementPadded;
  m_numColsPerElem = region.getNumColsPerElem();
}

//! @brief  Get number of bits per element
unsigned
pimObjInfo::getBitsPerElement(PimBitWidth bitWidthType) const
{
  switch (bitWidthType) {
    case PimBitWidth::ACTUAL:
    case PimBitWidth::SIM:
    case PimBitWidth::HOST:
      return pimUtils::getNumBitsOfDataType(m_dataType, bitWidthType);
    case PimBitWidth::PADDED:
      return m_bitsPerElementPadded;
    default:
      assert(0);
  }
  return 0;
}

//! @brief  Get all regions on a specific PIM core for current PIM object
std::vector<pimRegion>
pimObjInfo::getRegionsOfCore(PimCoreId coreId) const
{
  std::vector<pimRegion> regions;
  for (const auto& region : m_regions) {
    if (region.getCoreId() == coreId) {
      regions.push_back(region);
    }
  }
  return regions;
}

//! @brief  Copy data from host memory to PIM object data holder, with ref support
void
pimObjInfo::copyFromHost(void* src, uint64_t idxBegin, uint64_t idxEnd)
{
  // handle reference
  if (m_refObjId != -1) {
    pimObjInfo& refObj = m_device->getResMgr()->getObjInfo(m_refObjId);
    if (isDualContactRef()) {
      uint64_t numBytes = refObj.m_data.getNumBytes(idxBegin, idxEnd);
      std::vector<uint8_t> buffer(numBytes);
      std::memcpy(buffer.data(), src, numBytes);
      for (auto& byte : buffer) { byte = ~byte; }
      refObj.m_data.copyFromHost(buffer.data(), idxBegin, idxEnd);
    } else {
      assert(0); // to be extended
    }
    return;
  }
  m_data.copyFromHost(src, idxBegin, idxEnd);
}

//! @brief  Copy data from PIM object data holder to host memory, with ref support
void
pimObjInfo::copyToHost(void* dest, uint64_t idxBegin, uint64_t idxEnd) const
{
  // handle reference
  if (m_refObjId != -1) {
    pimObjInfo &refObj = m_device->getResMgr()->getObjInfo(m_refObjId);
    if (isDualContactRef()) {
      uint64_t numBytes = refObj.m_data.getNumBytes(idxBegin, idxEnd);
      std::vector<uint8_t> buffer(numBytes);
      refObj.m_data.copyToHost(buffer.data(), idxBegin, idxEnd);
      for (auto& byte : buffer) { byte = ~byte; }
      std::memcpy(dest, buffer.data(), numBytes);
    } else {
      assert(0); // to be extended
    }
    return;
  }
  m_data.copyToHost(dest, idxBegin, idxEnd);
}

//! @brief  Copy data from a PIM object data holder to another, with ref support
void
pimObjInfo::copyToObj(pimObjInfo& destObj, uint64_t idxBegin, uint64_t idxEnd) const
{
  // handle reference
  if (m_refObjId != -1) {
    pimObjInfo &refObj = m_device->getResMgr()->getObjInfo(m_refObjId);
    if (isDualContactRef()) {
      uint64_t numBytes = m_data.getNumBytes(idxBegin, idxEnd);
      std::vector<uint8_t> buffer(numBytes);
      m_data.copyToHost(buffer.data(), idxBegin, idxEnd);
      for (auto& byte : buffer) { byte = ~byte; }
      refObj.m_data.copyFromHost(buffer.data(), idxBegin, idxEnd);
    } else {
      assert(0); // to be extended
    }
    return;
  }
  m_data.copyToObj(destObj.m_data, idxBegin, idxEnd);
}

//! @brief  Set an element at index with bit presentation, with ref support
void
pimObjInfo::setElementBits(uint64_t index, uint64_t bits)
{
  // handle reference
  if (m_refObjId != -1) {
    pimObjInfo& refObj = m_device->getResMgr()->getObjInfo(m_refObjId);
    if (isDualContactRef()) {
      bits = ~bits;
      refObj.m_data.setElementBits(index, bits);
    } else {
      assert(0); // to be extended
    }
    return;
  }
  m_data.setElementBits(index, bits);
}

//! @brief  Get bit representation of an element at index, with ref support
uint64_t
pimObjInfo::getElementBits(uint64_t index) const
{
  // handle reference
  if (m_refObjId != -1) {
    pimObjInfo& refObj = m_device->getResMgr()->getObjInfo(m_refObjId);
    if (isDualContactRef()) {
      uint64_t bits = 0;
      refObj.m_data.getElementBits(index, bits);
      bits = ~bits;
      return bits;
    } else {
      assert(0); // to be extended
    }
    return 0;
  }
  uint64_t bits = 0;
  m_data.getElementBits(index, bits);
  return bits;
}

//! @brief  Sync PIM object data from simulated memory
void
pimObjInfo::syncFromSimulatedMem()
{
  pimObjInfo &obj = (m_refObjId != -1 ? m_device->getResMgr()->getObjInfo(m_refObjId) : *this);
  unsigned numBits = getBitsPerElement(PimBitWidth::SIM);
  for (size_t i = 0; i < m_regions.size(); ++i) {
    pimRegion& region = m_regions[i];
    PimCoreId coreId = region.getCoreId();
    pimCore& core = m_device->getCore(coreId);
    uint64_t elemIdxBegin = region.getElemIdxBegin();
    uint64_t numElemInRegion = region.getNumElemInRegion();
    for (uint64_t j = 0; j < numElemInRegion; ++j) {
      auto [rowLoc, colLoc] = region.locateIthElemInRegion(j);
      uint64_t bits = isVLayout() ? core.getBitsV(rowLoc, colLoc, numBits)
                                  : core.getBitsH(rowLoc, colLoc, numBits);
      obj.m_data.setElementBits(elemIdxBegin + j, bits);
    }
  }
}

//! @brief  Sync PIM object data to simulated memory
void
pimObjInfo::syncToSimulatedMem() const
{
  const pimObjInfo &obj = (m_refObjId != -1 ? m_device->getResMgr()->getObjInfo(m_refObjId) : *this);
  unsigned numBits = getBitsPerElement(PimBitWidth::SIM);
  for (size_t i = 0; i < m_regions.size(); ++i) {
    const pimRegion& region = m_regions[i];
    PimCoreId coreId = region.getCoreId();
    pimCore& core = m_device->getCore(coreId);
    uint64_t elemIdxBegin = region.getElemIdxBegin();
    uint64_t numElemInRegion = region.getNumElemInRegion();
    for (uint64_t j = 0; j < numElemInRegion; ++j) {
      uint64_t bits = 0;
      obj.m_data.getElementBits(elemIdxBegin + j, bits);
      auto [rowLoc, colLoc] = region.locateIthElemInRegion(j);
      if (isVLayout()) {
        core.setBitsV(rowLoc, colLoc, bits, numBits);
      } else {
        core.setBitsH(rowLoc, colLoc, bits, numBits);
      }
    }
  }
}


//! @brief  pimResMgr ctor
pimResMgr::pimResMgr(pimDevice* device)
  : m_device(device),
    m_availObjId(0)
{
  unsigned numCores = m_device->getNumCores();
  unsigned numRowsPerCore = m_device->getNumRows();
  for (unsigned i = 0; i < numCores; ++i) {
    m_coreUsage[i] = std::make_unique<coreUsage>(numRowsPerCore);
  }
  m_debugAlloc = (m_device->getConfig().getDebug() & pimSimConfig::DEBUG_ALLOC);
}

//! @brief  pimResMgr dtor
pimResMgr::~pimResMgr()
{
}

//! @brief  Allocate a new PIM object
//!         For V layout, dataType determines the number of rows per region
//!         For H layout, dataType determines the number of bits per element
PimObjId
pimResMgr::pimAlloc(PimAllocEnum allocType, uint64_t numElements, PimDataType dataType)
{
  if (m_debugAlloc) {
    printf("PIM-Debug: pimAlloc: Request: %s %lu elements of type %s\n",
           pimUtils::pimAllocEnumToStr(allocType).c_str(), numElements,
           pimUtils::pimDataTypeEnumToStr(dataType).c_str());
  }

  if (numElements == 0) {
    printf("PIM-Error: pimAlloc: Invalid input parameter: 0 element\n");
    return -1;
  }

  unsigned bitsPerElement = pimUtils::getNumBitsOfDataType(dataType, PimBitWidth::SIM);

  std::vector<PimCoreId> sortedCoreId = getCoreIdsSortedByLeastUsage();
  pimObjInfo newObj(m_availObjId, dataType, allocType, numElements, bitsPerElement, m_device);
  m_availObjId++;

  unsigned numCores = m_device->getNumCores();
  unsigned numCols = m_device->getNumCols();
  unsigned numRowsToAlloc = 0;
  uint64_t numRegions = 0;
  unsigned numColsToAllocLast = 0;
  uint64_t numElemPerRegion = 0;
  uint64_t numElemPerRegionLast = 0;
  unsigned numColsPerElem = 0;
  if (allocType == PIM_ALLOC_V || allocType == PIM_ALLOC_V1) {
    // allocate one region per core, with vertical layout
    numRowsToAlloc = bitsPerElement;
    numRegions = (numElements - 1) / numCols + 1;
    numColsToAllocLast = numElements % numCols;
    if (numColsToAllocLast == 0) {
      numColsToAllocLast = numCols;
    }
    numElemPerRegion = numCols;
    numElemPerRegionLast = numColsToAllocLast;
    numColsPerElem = 1;
  } else if (allocType == PIM_ALLOC_H || allocType == PIM_ALLOC_H1) {
    // allocate one region per core, with horizontal layout
    numRowsToAlloc = 1;
    numRegions = (numElements * bitsPerElement - 1) / numCols + 1;
    numColsToAllocLast = (numElements * bitsPerElement) % numCols;
    if (numColsToAllocLast == 0) {
      numColsToAllocLast = numCols;
    }
    numElemPerRegion = numCols / bitsPerElement;
    numElemPerRegionLast = numColsToAllocLast / bitsPerElement;
    numColsPerElem = bitsPerElement;
  } else {
    printf("PIM-Error: pimAlloc: Unsupported allocation type %s\n",
           pimUtils::pimAllocEnumToStr(allocType).c_str());
    return -1;
  }

  if (m_debugAlloc) {
    printf("PIM-Debug: pimAlloc: Allocate %lu regions among %u cores\n",
           numRegions, numCores);
    printf("PIM-Debug: pimAlloc: Each region has %u rows x %u cols with %lu elements\n",
           numRowsToAlloc, numCols, numElemPerRegion);
    printf("PIM-Debug: pimAlloc: Last region has %u rows x %u cols with %lu elements\n",
           numRowsToAlloc, numColsToAllocLast, numElemPerRegionLast);
  }

  if (numRegions > numCores) {
    if (allocType == PIM_ALLOC_V1 || allocType == PIM_ALLOC_H1) {
      printf("PIM-Error: pimAlloc: Allocation type %s does not allow to allocate more regions (%lu) than number of cores (%u)\n",
             pimUtils::pimAllocEnumToStr(allocType).c_str(), numRegions, numCores);
      return -1;
    }
  }

  // create new regions
  bool success = true;
  for (unsigned i = 0; i < numCores; ++i) {
    m_coreUsage.at(i)->newAllocStart();
  }
  if (allocType == PIM_ALLOC_V || allocType == PIM_ALLOC_V1 || allocType == PIM_ALLOC_H || allocType == PIM_ALLOC_H1) {
    uint64_t elemIdx = 0;
    for (uint64_t i = 0; i < numRegions; ++i) {
      PimCoreId coreId = sortedCoreId[i % numCores];
      unsigned numColsToAlloc = (i == numRegions - 1 ? numColsToAllocLast : numCols);
      unsigned numElemInRegion = (i == numRegions - 1 ? numElemPerRegionLast : numElemPerRegion);
      pimRegion newRegion = findAvailRegionOnCore(coreId, numRowsToAlloc, numColsToAlloc);
      if (!newRegion.isValid()) {
        printf("PIM-Error: pimAlloc: Failed: Out of PIM memory\n");
        success = false;
        break;
      }
      newRegion.setElemIdxBegin(elemIdx);
      elemIdx += numElemInRegion;
      newRegion.setElemIdxEnd(elemIdx); // exclusive
      newRegion.setNumColsPerElem(numColsPerElem);
      newObj.addRegion(newRegion);

      // add to core usage map
      auto alloc = std::make_pair(newRegion.getRowIdx(), numRowsToAlloc);
      m_coreUsage.at(coreId)->addRange(alloc, newObj.getObjId());
    }
  }
  for (unsigned i = 0; i < numCores; ++i) {
    m_coreUsage.at(i)->newAllocEnd(success); // rollback if failed
  }

  if (!success) {
    return -1;
  }

  PimObjId objId = -1;
  if (newObj.isValid()) {
    objId = newObj.getObjId();
    newObj.finalize();
    // update new object to resource mgr
    m_objMap.insert(std::make_pair(newObj.getObjId(), newObj));
  }

  if (m_debugAlloc) {
    if (newObj.isValid()) {
      printf("PIM-Debug: pimAlloc: Allocated PIM object %d successfully\n", objId);
      newObj.print();
    } else {
      printf("PIM-Debug: pimAlloc: Failed\n");
    }
  }
  return objId;
}

//! @brief  Allocate a new PIM object of type buffer
PimObjId
pimResMgr::pimAllocBuffer(uint32_t numElements, PimDataType dataType)
{
  if (m_debugAlloc) {
    printf("PIM-Debug: pimAlloc: Request: Global Buffer %u elements of type %s\n",
           numElements, pimUtils::pimDataTypeEnumToStr(dataType).c_str());
  }

  if (numElements == 0) {
    printf("PIM-Error: pimAlloc: Invalid input parameter: 0 element\n");
    return -1;
  }

  unsigned bitsPerElement = pimUtils::getNumBitsOfDataType(dataType, PimBitWidth::SIM);

  if (numElements * bitsPerElement > m_device->getBufferSize() * 8) {
    printf("PIM-Error: pimAlloc: Invalid input parameter: %u elements exceeds buffer size %u bytes\n",
           numElements, m_device->getBufferSize());
    return -1;
  }

  pimObjInfo newObj(m_availObjId, dataType, PIM_ALLOC_H, numElements, bitsPerElement, m_device, true);
  m_availObjId++;

  unsigned numCols = m_device->getNumCols();
  unsigned numRowsToAlloc = 1;
  uint64_t numRegions = 1; // AiM buffer size will be always size of one row; For UPMEM this will be different
  unsigned numColsToAllocLast = 0;
  uint64_t numElemPerRegion = 0;
  uint64_t numElemPerRegionLast = 0;
  unsigned numColsPerElem = 0;
  numColsToAllocLast = (numElements * bitsPerElement) % numCols;
  if (numColsToAllocLast == 0) {
    numColsToAllocLast = numCols;
  }
  numElemPerRegion = numCols / bitsPerElement;
  numElemPerRegionLast = numColsToAllocLast / bitsPerElement;
  numColsPerElem = bitsPerElement;

  if (m_debugAlloc) {
    printf("PIM-Debug: pimAlloc: Allocate %lu regions\n", numRegions);
    printf("PIM-Debug: pimAlloc: Each region has %u rows x %u cols with %lu elements\n",
           numRowsToAlloc, numCols, numElemPerRegion);
    printf("PIM-Debug: pimAlloc: Last region has %u rows x %u cols with %lu elements\n",
           numRowsToAlloc, numColsToAllocLast, numElemPerRegionLast);
  }

  // create new regions
  bool success = true;
  uint64_t elemIdx = 0;
  unsigned numColsToAlloc = numCols;
  unsigned numElemInRegion = numElemPerRegionLast;
  pimRegion newRegion;
  newRegion.setCoreId(0);  // Assign global buffer to core 0; this is fine for global buffers; for UPMEM, this will be different
  newRegion.setRowIdx(0);
  newRegion.setColIdx(0);
  newRegion.setNumAllocRows(numRowsToAlloc);
  newRegion.setNumAllocCols(numColsToAlloc);
  newRegion.setIsBuffer(true);
  newRegion.setElemIdxBegin(elemIdx);
  newRegion.setIsValid(true);
  elemIdx += numElemInRegion;
  newRegion.setElemIdxEnd(elemIdx); // exclusive
  newRegion.setNumColsPerElem(numColsPerElem);
  newObj.addRegion(newRegion);

  if (!success) {
    return -1;
  }

  PimObjId objId = -1;
  if (newObj.isValid()) {
    objId = newObj.getObjId();
    newObj.finalize();
    // update new object to resource mgr
    m_objMap.insert(std::make_pair(newObj.getObjId(), newObj));
  }

  if (m_debugAlloc) {
    if (newObj.isValid()) {
      printf("PIM-Debug: pimAlloc: Allocated PIM object of type Buffer %d successfully\n", objId);
      newObj.print();
    } else {
      printf("PIM-Debug: pimAlloc: Failed\n");
    }
  }
  return objId;
}

//! @brief checks padding for alloc associated, returns new bitsPerElement, 0 on error
unsigned
pimResMgr::checkPaddingAllocAssoc(const char* cmdName, PimAllocEnum allocType, PimDataType dataType, unsigned bitsPerElement, unsigned bitsPerElementAssoc) const
{
  if (allocType == PIM_ALLOC_V || allocType == PIM_ALLOC_V1) {
    if (m_debugAlloc) {
      printf("PIM-Debug: %s: New object of data type %s (%u bits) is associated with object (%u bits) in V layout\n",
             cmdName, pimUtils::pimDataTypeEnumToStr(dataType).c_str(), bitsPerElement, bitsPerElementAssoc);
    }
    return bitsPerElement;  // no specific requirement on data type for V layout
  } else if (allocType == PIM_ALLOC_H || allocType == PIM_ALLOC_H1) {
    if ((bitsPerElement > bitsPerElementAssoc) && (m_device->getSimTarget() != PIM_DEVICE_BANK_LEVEL && m_device->getSimTarget() != PIM_DEVICE_FULCRUM)) {
      printf("PIM-Error: %s: New object data type %s (%u bits) is wider than associated object (%u bits), which is not supported in H layout\n",
            cmdName, pimUtils::pimDataTypeEnumToStr(dataType).c_str(), bitsPerElement, bitsPerElementAssoc);
      return 0;
    } else if (bitsPerElement < bitsPerElementAssoc) {
      if (m_debugAlloc) {
        printf("PIM-Debug: %s: New object of data type %s (%u bits) is padded to associated object (%u bits) in H layout\n",
                cmdName, pimUtils::pimDataTypeEnumToStr(dataType).c_str(), bitsPerElement, bitsPerElementAssoc);
      }
      return bitsPerElementAssoc;  // padding
    } else {
      // same bit width, no padding needed
      if (m_debugAlloc) {
        printf("PIM-Debug: %s: New object of data type %s (%u bits) is associated with object (%u bits) in H layout\n",
                cmdName, pimUtils::pimDataTypeEnumToStr(dataType).c_str(), bitsPerElement, bitsPerElementAssoc);
      }
      return bitsPerElement;
    }
  } else {
    printf("PIM-Error: %s: Unsupported allocation type %s\n",
           cmdName, pimUtils::pimAllocEnumToStr(allocType).c_str());
    return 0;
  }
}

//! @brief  Allocate a PIM object associated with an existing object
//!         Number of elements must be identical between the two associated objects
//!         For V layout, no specific requirement on data type
//!         For H layout, data type of the new object must be equal to or narrower than the associated object
PimObjId
pimResMgr::pimAllocAssociated(PimObjId assocId, PimDataType dataType)
{
  if (m_debugAlloc) {
    printf("PIM-Debug: pimAllocAssociated: Request: Data type %s associated with PIM object ID %d\n",
           pimUtils::pimDataTypeEnumToStr(dataType).c_str(), assocId);
  }

  // check if assoc obj is valid
  if (m_objMap.find(assocId) == m_objMap.end()) {
    printf("PIM-Error: pimAllocAssociated: Invalid associated PIM object ID %d\n", assocId);
    return -1;
  }

  // associated object must not be a buffer
  const pimObjInfo& assocObj = m_objMap.at(assocId);
  if (assocObj.isBuffer()) {
    printf("PIM-Error: pimAllocAssociated: Associated PIM object ID %d is a buffer, which is not allowed.\n", assocId);
    return -1;
  }

  // get regions of the assoc obj
  unsigned numCores = m_device->getNumCores();

  // check if the request can be associated with ref
  PimAllocEnum allocType = assocObj.getAllocType();
  uint64_t numElements = assocObj.getNumElements();
  unsigned bitsPerElement = pimUtils::getNumBitsOfDataType(dataType, PimBitWidth::SIM);
  unsigned bitsPerElementAssoc = assocObj.getBitsPerElement(PimBitWidth::PADDED);

  bitsPerElement = checkPaddingAllocAssoc("pimAllocAssociated", allocType, dataType, bitsPerElement, bitsPerElementAssoc);
  if (bitsPerElement == 0) {
    return -1; // error
  }

  // allocate associated regions
  pimObjInfo newObj(m_availObjId, dataType, allocType, numElements, bitsPerElement, m_device);
  m_availObjId++;

  unsigned numCols = m_device->getNumCols();
  uint64_t numRegions = 0;
  unsigned numColsToAllocLast = 0;
  uint64_t numElemPerRegion = 0;
  uint64_t numElemPerRegionLast = 0;
  unsigned numColsPerElem = 0;

  bool extraRegions = (allocType == PIM_ALLOC_H || allocType == PIM_ALLOC_H1) && (bitsPerElement > bitsPerElementAssoc) && (m_device->getSimTarget() == PIM_DEVICE_BANK_LEVEL || m_device->getSimTarget() == PIM_DEVICE_FULCRUM);

  // The reason other horizontal bit-parallel (AiM, Aquabolt) PIM is not included in this condition is that
  // they support only 16-bit floats/ints.
  // If more bit-parallel PIMs are added, this condition should be extended.
  if (extraRegions) {
    // allocate one region per core, with horizontal layout
    numRegions = (numElements * bitsPerElement - 1) / numCols + 1;

    // This is a controversial design decision. I am not fully sold on this
    // TODO: discuss with professor before implementing the `non-controversial` design
    if (numRegions > assocObj.getRegions().size()) {
      printf("PIM-Error: pimAllocAssociated: Allocation type %s does not allow to allocate more regions (%lu) than associated object (%lu)\n",
             pimUtils::pimAllocEnumToStr(allocType).c_str(), numRegions, assocObj.getRegions().size());
      return -1;
    }

    if (numRegions > numCores) {
      printf("PIM-Error: pimAllocAssociated: Allocation type %s does not allow to allocate more regions (%lu) than number of cores (%u)\n",
             pimUtils::pimAllocEnumToStr(allocType).c_str(), numRegions, numCores);
      return -1;
    }

    numColsToAllocLast = (numElements * bitsPerElement) % numCols;
    if (numColsToAllocLast == 0) {
      numColsToAllocLast = numCols;
    }
    numElemPerRegion = numCols / bitsPerElement;
    numElemPerRegionLast = numColsToAllocLast / bitsPerElement;
    numColsPerElem = bitsPerElement;
  }

  bool success = true;
  for (unsigned i = 0; i < numCores; ++i) {
    m_coreUsage.at(i)->newAllocStart();
  }

  unsigned regionIdx = 0;
  uint64_t elemIdx = 0;
  for (const pimRegion& region : assocObj.getRegions()) {
    if (extraRegions) {
      PimCoreId coreId = region.getCoreId();
      unsigned numAllocRows = region.getNumAllocRows() * bitsPerElement / bitsPerElementAssoc;
      unsigned numAllocCols = (regionIdx == numRegions - 1 ? numColsToAllocLast : numCols);
      pimRegion newRegion = findAvailRegionOnCore(coreId, numAllocRows, numAllocCols);
      if (!newRegion.isValid()) {
        printf("PIM-Error: pimAllocAssociated: Failed: Out of PIM memory\n");
        success = false;
        break;
      }
      newRegion.setElemIdxBegin(elemIdx);
      elemIdx += (regionIdx == numRegions - 1 ? numElemPerRegionLast : numElemPerRegion);
      if (elemIdx != region.getElemIdxEnd()) {
        printf("PIM-Error: pimAllocAssociated: Mismatch in element index range: %lu vs %lu\n",
               elemIdx, region.getElemIdxEnd());
        success = false;
        break;
      }
      newRegion.setElemIdxEnd(region.getElemIdxEnd()); // exclusive
      newRegion.setNumColsPerElem(numColsPerElem);
      newObj.addRegion(newRegion);

      // add to core usage map
      auto alloc = std::make_pair(newRegion.getRowIdx(), numAllocRows);
      m_coreUsage.at(coreId)->addRange(alloc, newObj.getObjId());
    } else {
      PimCoreId coreId = region.getCoreId();
      unsigned numAllocRows = region.getNumAllocRows();
      unsigned numAllocCols = region.getNumAllocCols();
      if (allocType == PIM_ALLOC_V || allocType == PIM_ALLOC_V1) {
        numAllocRows = bitsPerElement;
      }
      pimRegion newRegion = findAvailRegionOnCore(coreId, numAllocRows, numAllocCols);
      if (!newRegion.isValid()) {
        printf("PIM-Error: pimAllocAssociated: Failed: Out of PIM memory\n");
        success = false;
        break;
      }
      newRegion.setElemIdxBegin(region.getElemIdxBegin());
      newRegion.setElemIdxEnd(region.getElemIdxEnd()); // exclusive
      newRegion.setNumColsPerElem(region.getNumColsPerElem());
      newObj.addRegion(newRegion);

      // add to core usage map
      auto alloc = std::make_pair(newRegion.getRowIdx(), numAllocRows);
      m_coreUsage.at(coreId)->addRange(alloc, newObj.getObjId());
    }
    regionIdx++;
  }
  for (unsigned i = 0; i < numCores; ++i) {
    m_coreUsage.at(i)->newAllocEnd(success); // rollback if failed
  }

  if (!success) {
    return -1;
  }

  PimObjId objId = -1;
  if (newObj.isValid()) {
    objId = newObj.getObjId();
    newObj.finalize();
    newObj.setAssocObjId(assocObj.getAssocObjId());
    // update new object to resource mgr
    m_objMap.insert(std::make_pair(newObj.getObjId(), newObj));
  }

  if (m_debugAlloc) {
    if (newObj.isValid()) {
      printf("PIM-Debug: pimAllocAssociated: Allocated PIM object %d successfully\n", objId);
      newObj.print();
    } else {
      printf("PIM-Debug: pimAllocAssociated: Failed\n");
    }
  }
  return objId;
}

//! @brief  Get cores to use for grid
std::vector<PimCoreId>
pimResMgr::getCoresForGrid(
                            size_t numCoresVertical,
                            size_t numCoresHorizontal,
                            size_t numElementsPerCoreVertical,
                            size_t numElementsPerCoreHorizontal,
                            PimAllocationStrategy allocationStrategy,
                            size_t stencilRadius) const
{
  const uint64_t numCores = numCoresVertical * numCoresHorizontal;
  if (allocationStrategy == PimAllocationStrategy::PIM_ALLOCATION_STRATEGY_LEAST_USED_CORES) {
    std::vector<PimCoreId> sortedCoreId = getCoreIdsSortedByLeastUsage();
    if (numCores > sortedCoreId.size()) {
      printf("PIM-Error: getCoresForGrid: Attempting to allocate %u cores when device only has %lu cores\n",
            numCores, sortedCoreId.size());
      return {};
    }
    return std::vector<PimCoreId>(sortedCoreId.begin(), sortedCoreId.begin() + numCores);
  } else if (allocationStrategy == PimAllocationStrategy::PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT) {
    const uint64_t subarraysPerBank = m_device->getNumSubarrayPerBank();
    const uint64_t banksPerRank = m_device->getNumBankPerRank();
    const uint64_t ranks = m_device->getNumRanks();

    TotalMoveCostParams params;
    params.subarrayBlockWidth = numElementsPerCoreHorizontal;
    params.subarrayBlockHeight = numElementsPerCoreVertical;
    // The naming here conflates subarrays and cores, so for bank core devices the number of subarrays (read: cores) per bank is 1
    // 2 subarrays per core
    params.subarraysPerBank = m_device->isBankCoreDevice() ? 1 : subarraysPerBank / 2;
    params.banksPerRank = banksPerRank;
    params.ranks = ranks;
    params.totalGridWidth = numCoresHorizontal;
    params.totalGridHeight = numCoresVertical;
    params.radius = static_cast<uint64_t>(stencilRadius);
    params.transferCostSubarrayToSubarray = m_device->getInterSubarrayMsPerByte();
    params.transferCostBankToBank = m_device->getInterBankMsPerByte();
    params.transferCostRankToRank = m_device->getInterRankMsPerByte();
    const GridLayoutConfig layout = optimizeGridLayout(params);

    const uint64_t numRankHorizontal = layout.rankGridWidth;
    const uint64_t numRankVertical = layout.rankGridHeight;
    const uint64_t numBankHorizontalPerRank = layout.bankGridWidth;
    const uint64_t numBankVerticalPerRank = layout.bankGridHeight;
    const uint64_t numSubarrayHorizontalPerBank = layout.subarrayGridWidth;
    const uint64_t numSubarrayVerticalPerBank = layout.subarrayGridHeight;
    const uint64_t rankTileWidth = numBankHorizontalPerRank * numSubarrayHorizontalPerBank;
    const uint64_t rankTileHeight = numBankVerticalPerRank * numSubarrayVerticalPerBank;

    std::vector<PimCoreId> cores(numCores);
    for (uint64_t coreRow = 0; coreRow < numCoresVertical; ++coreRow) {
      const uint64_t rankRow = coreRow / rankTileHeight;
      const uint64_t bankRow = (coreRow / numSubarrayVerticalPerBank) % numBankVerticalPerRank;
      const uint64_t subarrayRow = coreRow % numSubarrayVerticalPerBank;
      for (uint64_t coreCol = 0; coreCol < numCoresHorizontal; ++coreCol) {
        const uint64_t coreIdx = coreRow * numCoresHorizontal + coreCol;

        const uint64_t rankCol = coreCol / rankTileWidth;
        const uint64_t bankCol = (coreCol / numSubarrayHorizontalPerBank) % numBankHorizontalPerRank;
        const uint64_t subarrayCol = coreCol % numSubarrayHorizontalPerBank;

        if (rankRow >= numRankVertical || rankCol >= numRankHorizontal) {
          printf("PIM-Error: getCoresForGrid: Optimizer returned insufficient rank grid dimensions\n");
          return {};
        }

        const unsigned rankId = rankRow * numRankHorizontal + rankCol;
        const unsigned bankId = bankRow * numBankHorizontalPerRank + bankCol;
        const unsigned subarrayId = subarrayRow * numSubarrayHorizontalPerBank + subarrayCol;

        PimCoreLocation loc;
        if(m_device->isBankCoreDevice()) {
          BankCoreLocation bankLoc{bankId};
          loc = {rankId, 0, bankLoc};
        } else {
          SubarrayCoreLocation subarrayLoc{bankId, subarrayId};
          loc = {rankId, 0, subarrayLoc};
        }
        if(loc.chip != 0) {
          printf("PIM-Error: getCoresForGrid: Unsupported multi-chip configuration for stencil-based allocation strategy\n");
          return {};
        }
        cores[coreIdx] = m_device->getCoreIdIgnoreChip(loc);
      }
    }
    return cores;
  } else {
    printf("PIM-Error: pimAllocGrid: Unsupported grid allocation strategy\n");
    return {};
  }
}

//! @brief  Allocate a new PIM object with detailed parameters
PimObjGrid
pimResMgr::pimAllocGrid(PimAllocEnum allocType, PimDataType dataType,
                          size_t numCoresVertical, size_t numCoresHorizontal,
                          size_t numElementsPerCoreVertical, size_t numElementsPerCoreHorizontal,
                          PimAllocationStrategy allocationStrategy,
                          size_t stencilRadius)
{
  if (m_debugAlloc) {
    printf("PIM-Debug: pimAllocGrid: Request: %s (%lu x %lu) cores of (%lu x %lu) elements of type %s\n",
           pimUtils::pimAllocEnumToStr(allocType).c_str(), numCoresHorizontal, numCoresVertical,
           numElementsPerCoreVertical, numElementsPerCoreHorizontal,
           pimUtils::pimDataTypeEnumToStr(dataType).c_str());
  }

  if (m_device->getConfig().isLoadBalanced()) {
    printf("PIM-Error: pimAllocGrid: Grid allocation is not supported under load-balanced configuration\n");
    return {};
  }

  if (numElementsPerCoreVertical == 0 || numElementsPerCoreHorizontal == 0) {
    printf("PIM-Error: pimAllocGrid: Invalid input parameter: 0 elements per core\n");
    return {};
  }

  unsigned numCoresToAlloc = numCoresVertical * numCoresHorizontal;
  if (numCoresToAlloc == 0) {
    printf("PIM-Error: pimAllocGrid: Invalid input parameter: 0 cores\n");
    return {};
  }

  unsigned numCores = m_device->getNumCores();
  if(numCoresToAlloc > numCores) {
    printf("PIM-Error: pimAllocGrid: Attempting to allocate %u cores when device only has %u cores\n",
            numCoresToAlloc, numCores);
    return {};
  }

  unsigned bitsPerElement = pimUtils::getNumBitsOfDataType(dataType, PimBitWidth::SIM);

  unsigned rowsPerCoreToAlloc = 0;
  unsigned colsPerCoreToAlloc = 0;
  unsigned rowsPerCorePerObjToAlloc = 0;
  unsigned numColsPerElem = 0;
  if (allocType == PIM_ALLOC_V || allocType == PIM_ALLOC_V1) {
    rowsPerCoreToAlloc = numElementsPerCoreVertical * bitsPerElement;
    colsPerCoreToAlloc = numElementsPerCoreHorizontal;
    rowsPerCorePerObjToAlloc = bitsPerElement;
    numColsPerElem = 1;
  } else if (allocType == PIM_ALLOC_H || allocType == PIM_ALLOC_H1) {
    rowsPerCoreToAlloc = numElementsPerCoreVertical;
    colsPerCoreToAlloc = numElementsPerCoreHorizontal * bitsPerElement;
    rowsPerCorePerObjToAlloc = 1;
    numColsPerElem = bitsPerElement;
  } else {
    printf("PIM-Error: pimAllocGrid: Unsupported allocation type %s\n",
           pimUtils::pimAllocEnumToStr(allocType).c_str());
    return {};
  }

  if (m_debugAlloc) {
    printf("PIM-Debug: pimAllocGrid: Allocate %lu regions among %u cores\n",
           numElementsPerCoreVertical * numCoresToAlloc, numCores);
    printf("PIM-Debug: pimAllocGrid: Each region has %u rows x %u cols with %lu elements\n",
           rowsPerCorePerObjToAlloc, colsPerCoreToAlloc, numElementsPerCoreHorizontal);
  }

  unsigned numCols = m_device->getNumCols();
  if(colsPerCoreToAlloc > numCols) {
    printf("PIM-Error: pimAllocGrid: Attempting to allocate too many columns (%u requested, %u available)\n", colsPerCoreToAlloc, numCols);
    return {};
  }

  std::vector<pimObjInfo> newObjs;
  newObjs.reserve(numElementsPerCoreVertical);
  for(size_t i=0; i<numElementsPerCoreVertical; ++i) {
    newObjs.emplace_back(m_availObjId, dataType, allocType, numCoresToAlloc * numElementsPerCoreHorizontal, bitsPerElement, m_device, numCoresVertical, numCoresHorizontal);
    m_availObjId++;
  }

  const std::vector<PimCoreId>& coreIds = getCoresForGrid(
                                            numCoresVertical,
                                            numCoresHorizontal,
                                            numElementsPerCoreVertical,
                                            numElementsPerCoreHorizontal,
                                            allocationStrategy,
                                            stencilRadius);

  std::vector<GridCoreLocationTuple> coreLocationTuples;
  coreLocationTuples.reserve(coreIds.size());
  for (const PimCoreId coreId : coreIds) {
    const PimCoreLocation coreLoc = m_device->getCoreLocationIgnoreChip(coreId);
    if(coreLoc.chip != 0) {
      printf("PIM-Error: pimAllocGrid: Unsupported multi-chip configuration for logging core locations\n");
      return {};
    }
    unsigned bank = 0;
    std::optional<unsigned> subarray;
    if (std::holds_alternative<BankCoreLocation>(coreLoc.loc)) {
      bank = std::get<BankCoreLocation>(coreLoc.loc).bankCoreIdx;
    } else {
      const SubarrayCoreLocation& subarrayLoc = std::get<SubarrayCoreLocation>(coreLoc.loc);
      bank = subarrayLoc.bank;
      subarray = subarrayLoc.subarrayCoreIdx;
    }
    coreLocationTuples.emplace_back(coreLoc.rank, coreLoc.chip, bank, subarray);
  }

  const std::string gridLogFileName = allocationStrategy == PimAllocationStrategy::PIM_ALLOCATION_STRATEGY_LEAST_USED_CORES ?
                                    "pim_alloc_grid_least_used_cores.csv" :
                                    "pim_alloc_grid_stencil_9_point.csv";
  if (!logGridCoreLocations2D(coreLocationTuples, numCoresVertical, numCoresHorizontal, gridLogFileName)) {
    std::printf("PIM-Error: pimAllocGrid: Failed to log core locations to %s\n", gridLogFileName.c_str());
  }

  // create new regions
  bool success = true;
  for (unsigned i = 0; i < numCoresToAlloc; ++i) {
    m_coreUsage.at(coreIds[i])->newAllocStart();
  }
  if (allocType == PIM_ALLOC_V || allocType == PIM_ALLOC_V1 || allocType == PIM_ALLOC_H || allocType == PIM_ALLOC_H1) {
    for (uint64_t newObjIdx = 0; newObjIdx < numElementsPerCoreVertical; ++newObjIdx) {
      uint64_t elemIdx = 0;
      for (uint64_t sortedCoreIdx = 0; sortedCoreIdx < numCoresToAlloc; ++sortedCoreIdx) {
        PimCoreId coreId = coreIds[sortedCoreIdx];
        // unsigned numColsToAlloc = (i == numRegions - 1 ? numColsToAllocLast : numCols);
        // unsigned numElemInRegion = (i == numRegions - 1 ? numElemPerRegionLast : numElemPerRegion);
        pimRegion newRegion = findAvailRegionOnCore(coreId, rowsPerCorePerObjToAlloc, colsPerCoreToAlloc);
        if (!newRegion.isValid()) {
          printf("PIM-Error: pimAlloc: Failed: Out of PIM memory\n");
          success = false;
          break;
        }
        newRegion.setElemIdxBegin(elemIdx);
        elemIdx += numElementsPerCoreHorizontal;
        newRegion.setElemIdxEnd(elemIdx); // exclusive
        newRegion.setNumColsPerElem(numColsPerElem);
        newObjs[newObjIdx].addRegion(newRegion);

        // add to core usage map
        auto alloc = std::make_pair(newRegion.getRowIdx(), rowsPerCorePerObjToAlloc);
        m_coreUsage.at(coreId)->addRange(alloc, newObjs[newObjIdx].getObjId());
      }
    }
  }
  for (unsigned i = 0; i < numCoresToAlloc; ++i) {
    m_coreUsage.at(coreIds[i])->newAllocEnd(success); // rollback if failed
  }

  if (!success) {
    return {};
  }

  std::vector<PimObjId> objIds(numElementsPerCoreVertical);
  for(size_t i=0; i<objIds.size(); ++i) {
    pimObjInfo& newObj = newObjs[i];
    if(newObj.isValid()) {
      objIds[i] = newObj.getObjId();
      newObj.finalize();

      // Associate all objects with first
      if(i != 0) {
        newObj.setAssocObjId(objIds[0]);
      }
      m_objMap.insert(std::make_pair(newObj.getObjId(), newObj));
    }
  }

  if (m_debugAlloc) {
    for(auto& newObj : newObjs) {
      if (newObj.isValid()) {
        printf("PIM-Debug: pimAllocGrid: Allocated PIM object %d successfully\n", newObj.getObjId());
        newObj.print();
      } else {
        printf("PIM-Debug: pimAllocGrid: Failed\n");
      }
    }
  }
  return objIds;
}

//! @brief  Allocate a new PIM Grid object associated with an existing object
PimObjGrid
pimResMgr::pimAllocGridAssociated(PimObjId assocId, PimDataType dataType, size_t numElementsPerCoreVertical)
{
  if (m_debugAlloc) {
    printf("PIM-Debug: pimAllocGridAssociated: Request: Data type %s associated with PIM object ID %d for grid with %lu elements per core vertically\n",
          pimUtils::pimDataTypeEnumToStr(dataType).c_str(), assocId, numElementsPerCoreVertical);
  }

  // check if assoc obj is valid
  if (m_objMap.find(assocId) == m_objMap.end()) {
    printf("PIM-Error: pimAllocGridAssociated: Invalid associated PIM object ID %d\n", assocId);
    return {};
  }

  // associated object must not be a buffer
  const pimObjInfo& assocObj = m_objMap.at(assocId);
  if (assocObj.isBuffer()) {
    printf("PIM-Error: pimAllocGridAssociated: Associated PIM object ID %d is a buffer, which is not allowed.\n", assocId);
    return {};
  }

  // associated object must be part of a grid
  if (!assocObj.isGridMember()) {
    printf("PIM-Error: pimAllocGridAssociated: Associated PIM object ID %d is not part of a grid, which is required for grid association.\n", assocId);
    return {};
  }

  unsigned numCores = m_device->getNumCores();

  // check if the request can be associated with ref
  PimAllocEnum allocType = assocObj.getAllocType();
  uint64_t numElements = assocObj.getNumElements();
  unsigned bitsPerElement = pimUtils::getNumBitsOfDataType(dataType, PimBitWidth::SIM);
  unsigned bitsPerElementAssoc = assocObj.getBitsPerElement(PimBitWidth::PADDED);

  bitsPerElement = checkPaddingAllocAssoc("pimAllocGridAssociated", allocType, dataType, bitsPerElement, bitsPerElementAssoc);
  if (bitsPerElement == 0) {
    return {}; // error
  }

  uint64_t numCoresVertical = assocObj.getNumCoresVertical();
  uint64_t numCoresHorizontal = assocObj.getNumCoresHorizontal();


  // allocate associated regions

  std::vector<pimObjInfo> newObjs;
  newObjs.reserve(numElementsPerCoreVertical);
  for(size_t i=0; i<numElementsPerCoreVertical; ++i) {
    newObjs.emplace_back(m_availObjId, dataType, allocType, numElements, bitsPerElement, m_device, numCoresVertical, numCoresHorizontal);
    m_availObjId++;
  }

  unsigned numCols = m_device->getNumCols();
  uint64_t numRegions = 0;
  unsigned numColsToAllocLast = 0;
  uint64_t numElemPerRegion = 0;
  uint64_t numElemPerRegionLast = 0;
  unsigned numColsPerElem = 0;

  bool extraRegions = (allocType == PIM_ALLOC_H || allocType == PIM_ALLOC_H1) && (bitsPerElement > bitsPerElementAssoc) && (m_device->getSimTarget() == PIM_DEVICE_BANK_LEVEL || m_device->getSimTarget() == PIM_DEVICE_FULCRUM);

  // The reason other horizontal bit-parallel (AiM, Aquabolt) PIM is not included in this condition is that
  // they support only 16-bit floats/ints.
  // If more bit-parallel PIMs are added, this condition should be extended.
  if (extraRegions) {
    // allocate one region per core, with horizontal layout
    numRegions = (numElements * bitsPerElement - 1) / numCols + 1;

    // This is a controversial design decision. I am not fully sold on this
    // TODO: discuss with professor before implementing the `non-controversial` design
    if (numRegions > assocObj.getRegions().size()) {
      printf("PIM-Error: pimAllocGridAssociated: Allocation type %s does not allow to allocate more regions (%lu) than associated object (%lu)\n",
             pimUtils::pimAllocEnumToStr(allocType).c_str(), numRegions, assocObj.getRegions().size());
      return {};
    }

    if (numRegions > numCores) {
      printf("PIM-Error: pimAllocGridAssociated: Allocation type %s does not allow to allocate more regions (%lu) than number of cores (%u)\n",
             pimUtils::pimAllocEnumToStr(allocType).c_str(), numRegions, numCores);
      return {};
    }

    numColsToAllocLast = (numElements * bitsPerElement) % numCols;
    if (numColsToAllocLast == 0) {
      numColsToAllocLast = numCols;
    }
    numElemPerRegion = numCols / bitsPerElement;
    numElemPerRegionLast = numColsToAllocLast / bitsPerElement;
    numColsPerElem = bitsPerElement;
  }

  bool success = true;
  for (unsigned i = 0; i < numCores; ++i) {
    m_coreUsage.at(i)->newAllocStart();
  }

  for(auto& newObj : newObjs) {
    unsigned regionIdx = 0;
    uint64_t elemIdx = 0;
    for (const pimRegion& region : assocObj.getRegions()) {
      if (extraRegions) {
        PimCoreId coreId = region.getCoreId();
        unsigned numAllocRows = region.getNumAllocRows() * bitsPerElement / bitsPerElementAssoc;
        unsigned numAllocCols = (regionIdx == numRegions - 1 ? numColsToAllocLast : numCols);
        pimRegion newRegion = findAvailRegionOnCore(coreId, numAllocRows, numAllocCols);
        if (!newRegion.isValid()) {
          printf("PIM-Error: pimAllocGridAssociated: Failed: Out of PIM memory\n");
          success = false;
          break;
        }
        newRegion.setElemIdxBegin(elemIdx);
        elemIdx += (regionIdx == numRegions - 1 ? numElemPerRegionLast : numElemPerRegion);
        if (elemIdx != region.getElemIdxEnd()) {
          printf("PIM-Error: pimAllocGridAssociated:: Mismatch in element index range: %lu vs %lu\n",
                elemIdx, region.getElemIdxEnd());
          success = false;
          break;
        }
        newRegion.setElemIdxEnd(region.getElemIdxEnd()); // exclusive
        newRegion.setNumColsPerElem(numColsPerElem);
        newObj.addRegion(newRegion);

        // add to core usage map
        auto alloc = std::make_pair(newRegion.getRowIdx(), numAllocRows);
        m_coreUsage.at(coreId)->addRange(alloc, newObj.getObjId());
      } else {
        PimCoreId coreId = region.getCoreId();
        unsigned numAllocRows = region.getNumAllocRows();
        unsigned numAllocCols = region.getNumAllocCols();
        if (allocType == PIM_ALLOC_V || allocType == PIM_ALLOC_V1) {
          numAllocRows = bitsPerElement;
        }
        pimRegion newRegion = findAvailRegionOnCore(coreId, numAllocRows, numAllocCols);
        if (!newRegion.isValid()) {
          printf("PIM-Error: pimAllocGridAssociated:: Failed: Out of PIM memory\n");
          success = false;
          break;
        }
        newRegion.setElemIdxBegin(region.getElemIdxBegin());
        newRegion.setElemIdxEnd(region.getElemIdxEnd()); // exclusive
        newRegion.setNumColsPerElem(region.getNumColsPerElem());
        newObj.addRegion(newRegion);

        // add to core usage map
        auto alloc = std::make_pair(newRegion.getRowIdx(), numAllocRows);
        m_coreUsage.at(coreId)->addRange(alloc, newObj.getObjId());
      }
      regionIdx++;
    }
  }
  for (unsigned i = 0; i < numCores; ++i) {
    m_coreUsage.at(i)->newAllocEnd(success); // rollback if failed
  }

  if (!success) {
    return {};
  }

  std::vector<PimObjId> objIds(numElementsPerCoreVertical);
  for(size_t i=0; i<objIds.size(); ++i) {
    pimObjInfo& newObj = newObjs[i];
    if(newObj.isValid()) {
      objIds[i] = newObj.getObjId();
      newObj.finalize();
      newObj.setAssocObjId(assocObj.getAssocObjId());
      m_objMap.insert(std::make_pair(newObj.getObjId(), newObj));
    }
  }

  if (m_debugAlloc) {
    for(auto& newObj : newObjs) {
      if (newObj.isValid()) {
        printf("PIM-Debug: pimAllocGridAssociated: Allocated PIM object %d successfully\n", newObj.getObjId());
        newObj.print();
      } else {
        printf("PIM-Debug: pimAllocGridAssociated: Failed\n");
      }
    }
  }

  return objIds;
}

//! @brief  Free a PIM object
bool
pimResMgr::pimFree(PimObjId objId)
{
  if (m_objMap.find(objId) == m_objMap.end()) {
    printf("PIM-Error: pimFree: Invalid PIM object ID %d\n", objId);
    return false;
  }
  unsigned numCores = m_device->getNumCores();
  const pimObjInfo& obj = m_objMap.at(objId);

  if (!obj.isDualContactRef()) {
    for (unsigned i = 0; i < numCores; ++i) {
      m_coreUsage.at(i)->deleteObj(objId);
    }
  }
  m_objMap.erase(objId);

  // free all reference as well
  if (m_refMap.find(objId) != m_refMap.end()) {
    for (auto refId : m_refMap.at(objId)) {
      m_objMap.erase(refId);
    }
  }

  if (m_debugAlloc) {
    printf("PIM-Debug: pimFree: Deleted object %d\n", objId);
  }
  return true;
}

//! @brief Free a PIM Grid object
bool
pimResMgr::pimFreeGrid(PimObjGrid& grid)
{
  bool ok = true;
  for (PimObjId objId : grid) {
    ok &= this->pimFree(objId);
  }
  return ok;
}

//! @brief  Create an obj referencing to a range of an existing obj
PimObjId
pimResMgr::pimCreateRangedRef(PimObjId refId, uint64_t idxBegin, uint64_t idxEnd)
{
  assert(0); // todo
  return -1;
}

//! @brief  Create an obj referencing to negation of an existing obj based on dual-contact memory cells
PimObjId
pimResMgr::pimCreateDualContactRef(PimObjId refId)
{
  // check if ref obj is valid
  if (m_objMap.find(refId) == m_objMap.end()) {
    std::printf("PIM-Error: Invalid ref object ID %d for PIM dual contact ref\n", refId);
    return -1;
  }

  const pimObjInfo& refObj = m_objMap.at(refId);
  if (refObj.isDualContactRef()) {
    std::printf("PIM-Error: Cannot create dual contact ref of dual contact ref %d\n", refId);
    return -1;
  }

  // The dual-contact ref has exactly same regions as the ref object.
  // The refObjId field points to the ref object.
  // The isDualContactRef field indicates that values need to be negated during read/write.
  pimObjInfo newObj = refObj;
  PimObjId objId = m_availObjId++;
  newObj.setObjId(objId);
  newObj.setRefObjId(refObj.getObjId());
  m_refMap[refObj.getObjId()].insert(objId);
  newObj.setIsDualContactRef(true);
  m_objMap.insert(std::make_pair(newObj.getObjId(), newObj));

  return objId;
}

//! @brief  Alloc resource on a specific core. Perform row allocation for now.
pimRegion
pimResMgr::findAvailRegionOnCore(PimCoreId coreId, unsigned numAllocRows, unsigned numAllocCols) const
{
  pimRegion region;
  region.setCoreId(coreId);
  region.setColIdx(0);
  region.setNumAllocRows(numAllocRows);
  region.setNumAllocCols(numAllocCols);

  // try to find an available slot
  unsigned prevAvail = m_coreUsage.at(coreId)->findAvailRange(numAllocRows);
  if (m_device->getNumRows() - prevAvail >= numAllocRows) {
    region.setRowIdx(prevAvail);
    region.setIsValid(true);
    return region;
  }

  return region;
}

//! @brief  Get a list of core IDs sorted by least usage
std::vector<PimCoreId>
pimResMgr::getCoreIdsSortedByLeastUsage() const
{
  std::vector<std::pair<unsigned, unsigned>> usages;
  for (unsigned coreId = 0; coreId < m_device->getNumCores(); ++coreId) {
    unsigned usage = m_coreUsage.at(coreId)->getTotRowsInUse();
    usages.emplace_back(usage, coreId);
  }
  std::sort(usages.begin(), usages.end());
  std::vector<PimCoreId> result;
  for (const auto& it : usages) {
    result.push_back(it.second);
  }
  return result;
}

//! @brief  Find next available range of rows with a given size
unsigned
pimResMgr::coreUsage::findAvailRange(unsigned numRowsToAlloc)
{
  unsigned prevAvail = 0;
  for (const auto& it : m_rangesInUse) {
    unsigned rowIdx = it.first.first;
    unsigned numRows = it.first.second;
    if (rowIdx - prevAvail >= numRowsToAlloc) {
      return prevAvail;
    }
    prevAvail = rowIdx + numRows;
  }
  return prevAvail;
}

//! @brief  Add a new range to core usage.
//! The new range will be aggregated with previous adjacent range if they are from same object
//! Returned range is after aggregation
void
pimResMgr::coreUsage::addRange(std::pair<unsigned, unsigned> range, PimObjId objId)
{
  m_totRowsInUse += range.second;
  // aggregate with the prev range
  if (!m_rangesInUse.empty()) {
    auto it = std::prev(m_rangesInUse.end());
    unsigned lastIdx = it->first.first;
    unsigned lastSize = it->first.second;
    PimObjId lastObjId = it->second;
    if (lastIdx + lastSize == range.first && lastObjId == objId) {
      m_newAlloc.erase(it->first);
      m_rangesInUse.erase(it);
      range = std::make_pair(lastIdx, lastSize + range.second);
    }
  }
  m_rangesInUse.insert(std::make_pair(range, objId));
  m_newAlloc.insert(range);
}

//! @brief  Delete an object from core usage
void
pimResMgr::coreUsage::deleteObj(PimObjId objId)
{
  for (auto it = m_rangesInUse.begin(); it != m_rangesInUse.end();) {
    if (it->second == objId) {
      m_totRowsInUse -= it->first.second;
      it = m_rangesInUse.erase(it);
    } else {
      ++it;
    }
  }
}

//! @brief  Start a new allocation. This is preparing for rollback
void
pimResMgr::coreUsage::newAllocStart()
{
  m_newAlloc.clear();
}

//! @brief  End a new allocation. If failed, rollback all regions
void
pimResMgr::coreUsage::newAllocEnd(bool success)
{
  if (!success) {
    for (const auto &range : m_newAlloc) {
      m_rangesInUse.erase(range);
      m_totRowsInUse -= range.second;
    }
  }
  m_newAlloc.clear();
}

//! @brief check if a PIM grid is valid
bool
pimResMgr::isValidGrid(const PimObjGrid& pimGrid) const
{
  if(pimGrid.empty()) {
    return false;
  }
  for(PimObjId objId : pimGrid) {
    if(!isValidObjId(objId)) {
      return false;
    }
  }
  return true;
}

//! @brief  If a PIM object uses vertical data layout
bool
pimResMgr::isVLayoutObj(PimObjId objId) const
{
  const pimObjInfo& obj = getObjInfo(objId);
  return obj.isVLayout();
}

//! @brief  If a PIM object uses horizontal data layout
bool
pimResMgr::isHLayoutObj(PimObjId objId) const
{
  const pimObjInfo& obj = getObjInfo(objId);
  return obj.isHLayout();
}

//! @brief  If a PIM object uses hybrid data layout
bool
pimResMgr::isHybridLayoutObj(PimObjId objId) const
{
  return false;
}

