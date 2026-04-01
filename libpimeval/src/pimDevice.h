// File: pimDevice.h
// PIMeval Simulator - PIM Device
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef LAVA_PIM_DEVICE_H
#define LAVA_PIM_DEVICE_H

#include "libpimeval.h"
#include "pimSimConfig.h"
#include "pimCore.h"
#include "pimCmd.h"
#include "pimPerfEnergyBase.h"
#ifdef DRAMSIM3_INTEG
#include "cpu.h"
#endif
#include <memory>

struct BankCoreLocation {
  unsigned bankCoreIdx;
};

struct SubarrayCoreLocation {
  unsigned bank;
  unsigned subarrayCoreIdx;
};

struct PimCoreLocation {
  unsigned rank;
  unsigned chip;
  std::variant<BankCoreLocation, SubarrayCoreLocation> loc;
};

class pimResMgr;


//! @class  pimDevice
//! @brief  PIM device
class pimDevice
{
public:
  pimDevice(const pimSimConfig& config);
  ~pimDevice();

  const pimSimConfig& getConfig() const { return m_config; }

  PimDeviceEnum getDeviceType() const { return m_config.getDeviceType(); }
  PimDeviceEnum getSimTarget() const { return m_config.getSimTarget(); }
  unsigned getNumRanks() const { return m_config.getNumRanks(); }
  unsigned getNumChipPerRank() const { return m_numChipPerRank; }
  unsigned getNumBankPerRank() const { return m_config.getNumBankPerRank(); }
  unsigned getNumSubarrayPerBank() const { return m_config.getNumSubarrayPerBank(); }
  unsigned getNumRowPerSubarray() const { return m_config.getNumRowPerSubarray(); }
  unsigned getNumColPerSubarray() const { return m_config.getNumColPerSubarray(); }
  unsigned getOnChipBufferSize() const { return m_config.getBufferSize(); }

  unsigned getNumCores() const { return m_numCores; }
  unsigned getNumRows() const { return m_numRows; }
  unsigned getNumCols() const { return m_numCols; }
  unsigned getBufferSize() const { return m_bufferSize; }
  bool isValid() const { return m_isValid; }

  PimCoreLocation getCoreLocation(PimCoreId coreId) const;
  PimCoreId getCoreId(PimCoreLocation coreLoc) const;
  PimCoreLocation getCoreLocationIgnoreChip(PimCoreId coreId) const;
  PimCoreId getCoreIdIgnoreChip(PimCoreLocation coreLoc) const;
  unsigned getNumCoresInNextLevel() const { return m_numCoreInNextLevel; }
  bool isBankCoreDevice() const { return m_bankCoreDevice; }

  bool isVLayoutDevice() const;
  bool isHLayoutDevice() const;
  bool isHybridLayoutDevice() const;

  PimObjId pimAlloc(PimAllocEnum allocType, uint64_t numElements, PimDataType dataType);
  PimObjId pimAllocAssociated(PimObjId assocId, PimDataType dataType);
  PimObjId pimAllocBuffer(uint32_t numElements, PimDataType dataType);
  PimObjGrid pimAllocGrid(PimAllocEnum allocType, PimDataType dataType,
                          size_t numCoresVertical, size_t numCoresHorizontal,
                          size_t numElementsPerCoreVertical, size_t numElementsPerCoreHorizontal,
                          PimAllocationStrategy allocationStrategy);
  PimObjGrid pimAllocGridAssociated(PimObjId assocId, PimDataType dataType, size_t numElementsPerCoreVertical);
  bool pimFree(PimObjId obj);
  bool pimFreeGrid(PimObjGrid& grid);
  PimObjId pimCreateRangedRef(PimObjId refId, uint64_t idxBegin, uint64_t idxEnd);
  PimObjId pimCreateDualContactRef(PimObjId refId);

  bool pimCopyMainToDevice(void* src, PimObjId dest, uint64_t idxBegin = 0, uint64_t idxEnd = 0);
  bool pimCopyDeviceToMain(PimObjId src, void* dest, uint64_t idxBegin = 0, uint64_t idxEnd = 0);
  bool pimCopyMainToDeviceWithType(PimCopyEnum copyType, void* src, PimObjId dest, uint64_t idxBegin = 0, uint64_t idxEnd = 0);
  bool pimCopyDeviceToMainWithType(PimCopyEnum copyType, PimObjId src, void* dest, uint64_t idxBegin = 0, uint64_t idxEnd = 0);
  bool pimCopyHostToGrid(void* src, PimObjGrid& destGrid,
                            uint64_t idxBeginX = 0, uint64_t idxEndX = 0,
                            uint64_t idxBeginY = 0, uint64_t idxEndY = 0,
                            uint64_t idxEndXLast = 0, uint64_t idxEndYLast = 0);
  bool pimCopyGridToHost(PimObjGrid& srcGrid, void* dest,
                            uint64_t idxBeginX = 0, uint64_t idxEndX = 0,
                            uint64_t idxBeginY = 0, uint64_t idxEndY = 0,
                            uint64_t idxEndXLast = 0, uint64_t idxEndYLast = 0);
  bool pimCopyGridHalo(PimObjGrid& srcGrid, uint64_t numHalo);
  bool pimCopyDeviceToDevice(PimObjId src, PimObjId dest, uint64_t idxBegin = 0, uint64_t idxEnd = 0);

  pimResMgr* getResMgr() { return m_resMgr.get(); }
  pimPerfEnergyBase* getPerfEnergyModel() { return m_perfEnergyModel.get(); }
  pimCore& getCore(PimCoreId coreId) { return m_cores[coreId]; }
  bool executeCmd(std::unique_ptr<pimCmd> cmd);

private:
  bool init();
  bool adjustConfigForSimTarget(unsigned& numRanks, unsigned& numBankPerRank, unsigned& numSubarrayPerBank, unsigned& numRows, unsigned& numCols);

  const pimSimConfig& m_config;
  unsigned m_numChipPerRank = 0;
  unsigned m_numCores = 0;
  unsigned m_numRows = 0;
  unsigned m_numCols = 0;
  unsigned m_bufferSize = 0;
  unsigned m_numCoreInNextLevel = 0;
  bool m_isValid = false;
  bool m_isInit = false;
  bool m_bankCoreDevice = false; // true if device has bank-level cores, false if device has subarray-level cores
  std::unique_ptr<pimResMgr> m_resMgr;
  std::unique_ptr<pimPerfEnergyBase> m_perfEnergyModel;
  std::vector<pimCore> m_cores;

#ifdef DRAMSIM3_INTEG
  dramsim3::PIMCPU* m_hostMemory = nullptr;
  dramsim3::PIMCPU* m_deviceMemory = nullptr;
  dramsim3::Config* m_deviceMemoryConfig = nullptr;
#endif
};

#endif

