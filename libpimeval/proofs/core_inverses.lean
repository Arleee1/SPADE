-- File: core_inverses.lean
-- PIMeval Simulator - Proof of Core Location and ID Inverses
-- Copyright (c) 2026 University of Virginia
-- This file is licensed under the MIT License.
-- See the LICENSE file in the root of this repository for more details.

-- This file is not required to be run as part of the main simulation
-- Specifically, this file shows that pimDevice::getCoreLocation and pimDevice::getCoreId are inverses of each other for both Bank Core Device and Subarray Core Device configurations.

def getCoreLocation_Bank (id NumCoreInNextLevel NumChipPerRank : Nat) : Nat × Nat × Nat :=
  let coresPerChip := NumCoreInNextLevel
  let coresPerRank := coresPerChip * NumChipPerRank
  let rank := id / coresPerRank
  let rem1 := id % coresPerRank
  let chip := rem1 / coresPerChip
  let rem2 := rem1 % coresPerChip
  let bankCoreIdx := rem2
  (rank, chip, bankCoreIdx)

def getCoreId_Bank (rank chip bankCoreIdx NumCoreInNextLevel NumChipPerRank : Nat) : Nat :=
  let coresPerRank := NumChipPerRank * NumCoreInNextLevel
  rank * coresPerRank + chip * NumCoreInNextLevel + bankCoreIdx

theorem bank_inverse (id NumCoreInNextLevel NumChipPerRank : Nat) :
  let loc := getCoreLocation_Bank id NumCoreInNextLevel NumChipPerRank
  getCoreId_Bank loc.1 loc.2.1 loc.2.2 NumCoreInNextLevel NumChipPerRank = id := by
  intro loc
  change getCoreId_Bank (getCoreLocation_Bank id NumCoreInNextLevel NumChipPerRank).1 (getCoreLocation_Bank id NumCoreInNextLevel NumChipPerRank).2.1 (getCoreLocation_Bank id NumCoreInNextLevel NumChipPerRank).2.2 NumCoreInNextLevel NumChipPerRank = id
  dsimp [getCoreLocation_Bank, getCoreId_Bank]

  have h_comm : NumChipPerRank * NumCoreInNextLevel = NumCoreInNextLevel * NumChipPerRank := Nat.mul_comm NumChipPerRank NumCoreInNextLevel
  rw [h_comm]

  have step1 : (id % (NumCoreInNextLevel * NumChipPerRank) / NumCoreInNextLevel) * NumCoreInNextLevel + id % (NumCoreInNextLevel * NumChipPerRank) % NumCoreInNextLevel = id % (NumCoreInNextLevel * NumChipPerRank) := by
    have h := Nat.div_add_mod (id % (NumCoreInNextLevel * NumChipPerRank)) NumCoreInNextLevel
    rw [Nat.mul_comm NumCoreInNextLevel] at h
    exact h

  rw [Nat.add_assoc]
  rw [step1]

  have step2 : (id / (NumCoreInNextLevel * NumChipPerRank)) * (NumCoreInNextLevel * NumChipPerRank) + id % (NumCoreInNextLevel * NumChipPerRank) = id := by
    have h := Nat.div_add_mod id (NumCoreInNextLevel * NumChipPerRank)
    rw [Nat.mul_comm (NumCoreInNextLevel * NumChipPerRank)] at h
    exact h
  exact step2

theorem bank_inverse2 (rank chip bankCoreIdx NumCoreInNextLevel NumChipPerRank : Nat)
  (h_chip : chip < NumChipPerRank)
  (h_bank : bankCoreIdx < NumCoreInNextLevel)
  (h_NumCoreInNextLevel_pos : NumCoreInNextLevel > 0)
  (h_NumChipPerRank_pos : NumChipPerRank > 0) :
  let id := getCoreId_Bank rank chip bankCoreIdx NumCoreInNextLevel NumChipPerRank
  getCoreLocation_Bank id NumCoreInNextLevel NumChipPerRank = (rank, chip, bankCoreIdx) := by
  intro id
  let n := NumCoreInNextLevel
  let c := NumChipPerRank
  let coresPerRank := n * c
  let rem := chip * n + bankCoreIdx

  have h_n_pos : 0 < n := h_NumCoreInNextLevel_pos
  have h_c_pos : 0 < c := h_NumChipPerRank_pos
  have h_coresPerRank_pos : 0 < coresPerRank := by
    dsimp [coresPerRank]
    exact Nat.mul_pos h_n_pos h_c_pos

  have h_rem_lt : rem < coresPerRank := by
    have h1 : chip * n + bankCoreIdx < chip * n + n := Nat.add_lt_add_left h_bank (chip * n)
    have h2 : chip * n + n = chip.succ * n := by
      rw [Nat.succ_mul]
    have h3 : chip.succ * n ≤ c * n := Nat.mul_le_mul_right n (Nat.succ_le_of_lt h_chip)
    have h4 : c * n = coresPerRank := by
      dsimp [coresPerRank]
      rw [Nat.mul_comm]
    have h5 : rem < chip.succ * n := by
      dsimp [rem]
      simpa [h2] using h1
    exact Nat.lt_of_lt_of_le h5 (by simpa [h4] using h3)

  have h_rem_div_coresPerRank : rem / coresPerRank = 0 := Nat.div_eq_of_lt h_rem_lt
  have h_rem_mod_coresPerRank : rem % coresPerRank = rem := Nat.mod_eq_of_lt h_rem_lt

  have h_bank_div_n : bankCoreIdx / n = 0 := Nat.div_eq_of_lt h_bank
  have h_bank_mod_n : bankCoreIdx % n = bankCoreIdx := Nat.mod_eq_of_lt h_bank

  have h_chip_from_rem : rem / n = chip := by
    dsimp [rem]
    have h := Nat.add_mul_div_right bankCoreIdx chip h_n_pos
    rw [h_bank_div_n] at h
    simpa [Nat.add_comm, Nat.zero_add] using h

  have h_bank_from_rem : rem % n = bankCoreIdx := by
    calc
      rem % n = (bankCoreIdx + chip * n) % n := by
        dsimp [rem]
        rw [Nat.add_comm]
      _ = bankCoreIdx % n := Nat.add_mul_mod_self_right bankCoreIdx chip n
      _ = bankCoreIdx := h_bank_mod_n

  have h_rank_from_id : (rem + rank * coresPerRank) / coresPerRank = rank := by
    have h := Nat.add_mul_div_right rem rank h_coresPerRank_pos
    rw [h_rem_div_coresPerRank] at h
    simpa [Nat.zero_add] using h

  have h_rem_from_id : (rem + rank * coresPerRank) % coresPerRank = rem := by
    calc
      (rem + rank * coresPerRank) % coresPerRank = rem % coresPerRank :=
        Nat.add_mul_mod_self_right rem rank coresPerRank
      _ = rem := h_rem_mod_coresPerRank

  have h_id_rewrite :
      getCoreId_Bank rank chip bankCoreIdx n c = rem + rank * coresPerRank := by
    dsimp [getCoreId_Bank, rem, coresPerRank]
    rw [Nat.mul_comm c n]
    rw [Nat.add_assoc]
    rw [Nat.add_comm (rank * (n * c)) (chip * n + bankCoreIdx)]

  change getCoreLocation_Bank (getCoreId_Bank rank chip bankCoreIdx n c) n c = (rank, chip, bankCoreIdx)
  rw [h_id_rewrite]
  dsimp [getCoreLocation_Bank]
  apply Prod.ext
  · simpa [coresPerRank] using h_rank_from_id
  · apply Prod.ext
    · calc
        (rem + rank * coresPerRank) % (n * c) / n = ((rem + rank * coresPerRank) % coresPerRank) / n := by
          rfl
        _ = rem / n := by rw [h_rem_from_id]
        _ = chip := h_chip_from_rem
    · calc
        (rem + rank * coresPerRank) % (n * c) % n = ((rem + rank * coresPerRank) % coresPerRank) % n := by
          rfl
        _ = rem % n := by rw [h_rem_from_id]
        _ = bankCoreIdx := h_bank_from_rem

-- ==========================================
-- 2. Subarray Core Device
-- ==========================================

def getCoreLocation_Subarray (id NumCoreInNextLevel NumBankPerRank NumChipPerRank : Nat) : Nat × Nat × Nat × Nat :=
  let coresPerBank := NumCoreInNextLevel
  let coresPerChip := coresPerBank * (NumBankPerRank / NumChipPerRank)
  let coresPerRank := coresPerChip * NumChipPerRank
  let rank := id / coresPerRank
  let rem1 := id % coresPerRank
  let chip := rem1 / coresPerChip
  let rem2 := rem1 % coresPerChip
  let bank := rem2 / coresPerBank
  let rem3 := rem2 % coresPerBank
  let subarrayCoreIdx := rem3
  (rank, chip, bank, subarrayCoreIdx)

def getCoreId_Subarray (rank chip bank subarrayCoreIdx NumCoreInNextLevel NumBankPerRank NumChipPerRank : Nat) : Nat :=
  let numCorePerRank := NumBankPerRank * NumCoreInNextLevel
  let numCorePerChip := NumCoreInNextLevel * (NumBankPerRank / NumChipPerRank)
  let numCorePerBank := NumCoreInNextLevel
  rank * numCorePerRank + chip * numCorePerChip + bank * numCorePerBank + subarrayCoreIdx

theorem subarray_inverse (id NumCoreInNextLevel NumBankPerRank NumChipPerRank : Nat) (h_divides : NumBankPerRank % NumChipPerRank = 0) :
  let loc := getCoreLocation_Subarray id NumCoreInNextLevel NumBankPerRank NumChipPerRank
  getCoreId_Subarray loc.1 loc.2.1 loc.2.2.1 loc.2.2.2 NumCoreInNextLevel NumBankPerRank NumChipPerRank = id := by
  intro loc
  dsimp [getCoreLocation_Subarray, getCoreId_Subarray]

  let cBank := NumCoreInNextLevel
  let cChip := NumCoreInNextLevel * (NumBankPerRank / NumChipPerRank)
  let cRank := cChip * NumChipPerRank

  change (id / cRank) * (NumBankPerRank * NumCoreInNextLevel) + (id % cRank / cChip) * cChip + (id % cRank % cChip / cBank) * cBank + id % cRank % cChip % cBank = id

  have h_cRank : NumBankPerRank * NumCoreInNextLevel = cRank := by
    dsimp [cRank, cChip]
    have h1 := Nat.div_add_mod NumBankPerRank NumChipPerRank
    rw [h_divides] at h1
    rw [Nat.add_zero] at h1
    rw [Nat.mul_comm] at h1
    rw [Nat.mul_assoc]
    have comm : NumCoreInNextLevel * ((NumBankPerRank / NumChipPerRank) * NumChipPerRank) = NumCoreInNextLevel * NumBankPerRank := by rw [h1]
    rw [comm]
    exact Nat.mul_comm NumBankPerRank NumCoreInNextLevel

  rw [h_cRank]

  have step1 : (id % cRank % cChip / cBank) * cBank + (id % cRank % cChip % cBank) = id % cRank % cChip := by
    have h := Nat.div_add_mod (id % cRank % cChip) cBank
    rw [Nat.mul_comm cBank] at h
    exact h

  have add_assoc_2: (id / cRank * cRank + id % cRank / cChip * cChip + id % cRank % cChip / cBank * cBank + id % cRank % cChip % cBank) =
                    (id / cRank * cRank + id % cRank / cChip * cChip + (id % cRank % cChip / cBank * cBank + id % cRank % cChip % cBank)) := by
    rw [Nat.add_assoc (id / cRank * cRank + id % cRank / cChip * cChip)]
  rw [add_assoc_2]
  rw [step1]

  have step2 : (id % cRank / cChip) * cChip + id % cRank % cChip = id % cRank := by
    have h := Nat.div_add_mod (id % cRank) cChip
    rw [Nat.mul_comm cChip] at h
    exact h

  rw [Nat.add_assoc]
  rw [step2]

  have step3 : (id / cRank) * cRank + id % cRank = id := by
    have h := Nat.div_add_mod id cRank
    rw [Nat.mul_comm cRank] at h
    exact h
  exact step3

theorem subarray_inverse2 (rank chip bank subarrayCoreIdx NumSubarrayPerBank NumBankPerRank NumChipPerRank : Nat)
  (h_chip : chip < NumChipPerRank)
  (h_bank : bank < NumBankPerRank / NumChipPerRank)
  (h_subarray : subarrayCoreIdx < NumSubarrayPerBank)
  (h_NumSubarrayPerBank_pos : NumSubarrayPerBank > 0)
  (h_NumChipPerRank_pos : NumChipPerRank > 0)
  (h_divides : NumBankPerRank % NumChipPerRank = 0) :
  let id := getCoreId_Subarray rank chip bank subarrayCoreIdx NumSubarrayPerBank NumBankPerRank NumChipPerRank
  getCoreLocation_Subarray id NumSubarrayPerBank NumBankPerRank NumChipPerRank = (rank, chip, bank, subarrayCoreIdx) := by
  intro id
  let cBank := NumSubarrayPerBank
  let bankPerChip := NumBankPerRank / NumChipPerRank
  let cChip := cBank * bankPerChip
  let cRank := cChip * NumChipPerRank
  let remBank := bank * cBank + subarrayCoreIdx
  let remRank := chip * cChip + remBank

  have h_cBank_pos : 0 < cBank := h_NumSubarrayPerBank_pos
  have h_chipPerRank_pos : 0 < NumChipPerRank := h_NumChipPerRank_pos
  have h_bankPerChip_pos : 0 < bankPerChip := Nat.lt_of_le_of_lt (Nat.zero_le bank) h_bank
  have h_cChip_pos : 0 < cChip := by
    dsimp [cChip]
    exact Nat.mul_pos h_cBank_pos h_bankPerChip_pos
  have h_cRank_pos : 0 < cRank := by
    dsimp [cRank]
    exact Nat.mul_pos h_cChip_pos h_chipPerRank_pos

  have h_remBank_lt_cChip : remBank < cChip := by
    have h1 : bank * cBank + subarrayCoreIdx < bank * cBank + cBank := Nat.add_lt_add_left h_subarray (bank * cBank)
    have h2 : bank * cBank + cBank = bank.succ * cBank := by
      rw [Nat.succ_mul]
    have h3 : bank.succ * cBank ≤ bankPerChip * cBank := Nat.mul_le_mul_right cBank (Nat.succ_le_of_lt h_bank)
    have h4 : bankPerChip * cBank = cChip := by
      dsimp [cChip]
      rw [Nat.mul_comm]
    have h5 : remBank < bank.succ * cBank := by
      dsimp [remBank]
      simpa [h2] using h1
    exact Nat.lt_of_lt_of_le h5 (by simpa [h4] using h3)

  have h_remRank_lt_cRank : remRank < cRank := by
    have h1 : chip * cChip + remBank < chip * cChip + cChip := Nat.add_lt_add_left h_remBank_lt_cChip (chip * cChip)
    have h2 : chip * cChip + cChip = chip.succ * cChip := by
      rw [Nat.succ_mul]
    have h3 : chip.succ * cChip ≤ NumChipPerRank * cChip := Nat.mul_le_mul_right cChip (Nat.succ_le_of_lt h_chip)
    have h4 : NumChipPerRank * cChip = cRank := by
      dsimp [cRank]
      rw [Nat.mul_comm]
    have h5 : remRank < chip.succ * cChip := by
      dsimp [remRank]
      simpa [h2] using h1
    exact Nat.lt_of_lt_of_le h5 (by simpa [h4] using h3)

  have h_remBank_div_cChip : remBank / cChip = 0 := Nat.div_eq_of_lt h_remBank_lt_cChip
  have h_remBank_mod_cChip : remBank % cChip = remBank := Nat.mod_eq_of_lt h_remBank_lt_cChip
  have h_remRank_div_cRank : remRank / cRank = 0 := Nat.div_eq_of_lt h_remRank_lt_cRank
  have h_remRank_mod_cRank : remRank % cRank = remRank := Nat.mod_eq_of_lt h_remRank_lt_cRank

  have h_sub_div_cBank : subarrayCoreIdx / cBank = 0 := Nat.div_eq_of_lt h_subarray
  have h_sub_mod_cBank : subarrayCoreIdx % cBank = subarrayCoreIdx := Nat.mod_eq_of_lt h_subarray

  have h_bank_from_remBank : remBank / cBank = bank := by
    dsimp [remBank]
    have h := Nat.add_mul_div_right subarrayCoreIdx bank h_cBank_pos
    rw [h_sub_div_cBank] at h
    simpa [Nat.add_comm, Nat.zero_add] using h

  have h_sub_from_remBank : remBank % cBank = subarrayCoreIdx := by
    calc
      remBank % cBank = (subarrayCoreIdx + bank * cBank) % cBank := by
        dsimp [remBank]
        rw [Nat.add_comm]
      _ = subarrayCoreIdx % cBank := Nat.add_mul_mod_self_right subarrayCoreIdx bank cBank
      _ = subarrayCoreIdx := h_sub_mod_cBank

  have h_chip_from_remRank : remRank / cChip = chip := by
    calc
      remRank / cChip = (remBank + chip * cChip) / cChip := by
        dsimp [remRank]
        rw [Nat.add_comm]
      _ = remBank / cChip + chip := Nat.add_mul_div_right remBank chip h_cChip_pos
      _ = chip := by rw [h_remBank_div_cChip, Nat.zero_add]

  have h_remBank_from_remRank : remRank % cChip = remBank := by
    calc
      remRank % cChip = (remBank + chip * cChip) % cChip := by
        dsimp [remRank]
        rw [Nat.add_comm]
      _ = remBank % cChip := Nat.add_mul_mod_self_right remBank chip cChip
      _ = remBank := h_remBank_mod_cChip

  have h_div_bankPerChip : NumChipPerRank * bankPerChip = NumBankPerRank := by
    dsimp [bankPerChip]
    have h_div := Nat.div_add_mod NumBankPerRank NumChipPerRank
    rw [h_divides, Nat.add_zero] at h_div
    exact h_div

  have h_numCorePerRank_eq_cRank : NumBankPerRank * cBank = cRank := by
    calc
      NumBankPerRank * cBank = (NumChipPerRank * bankPerChip) * cBank := by
        rw [←h_div_bankPerChip]
      _ = NumChipPerRank * (bankPerChip * cBank) := by rw [Nat.mul_assoc]
      _ = NumChipPerRank * (cBank * bankPerChip) := by rw [Nat.mul_comm bankPerChip cBank]
      _ = (cBank * bankPerChip) * NumChipPerRank := by rw [Nat.mul_comm]
      _ = cRank := by dsimp [cRank, cChip]

  have h_rank_from_id : (remRank + rank * cRank) / cRank = rank := by
    have h := Nat.add_mul_div_right remRank rank h_cRank_pos
    rw [h_remRank_div_cRank] at h
    simpa [Nat.zero_add] using h

  have h_remRank_from_id : (remRank + rank * cRank) % cRank = remRank := by
    calc
      (remRank + rank * cRank) % cRank = remRank % cRank := Nat.add_mul_mod_self_right remRank rank cRank
      _ = remRank := h_remRank_mod_cRank

  have h_id_rewrite :
      getCoreId_Subarray rank chip bank subarrayCoreIdx cBank NumBankPerRank NumChipPerRank = rank * cRank + remRank := by
    dsimp [getCoreId_Subarray, remRank, remBank, cBank, cChip]
    rw [h_numCorePerRank_eq_cRank]
    rw [Nat.add_assoc]
    rw [Nat.add_assoc]

  change getCoreLocation_Subarray (getCoreId_Subarray rank chip bank subarrayCoreIdx cBank NumBankPerRank NumChipPerRank) cBank NumBankPerRank NumChipPerRank = (rank, chip, bank, subarrayCoreIdx)
  rw [h_id_rewrite]
  have h_rank_from_id' : (rank * cRank + remRank) / cRank = rank := by
    simpa [Nat.add_comm] using h_rank_from_id
  have h_remRank_from_id' : (rank * cRank + remRank) % cRank = remRank := by
    simpa [Nat.add_comm] using h_remRank_from_id
  dsimp [getCoreLocation_Subarray, bankPerChip, cChip, cRank, remRank, remBank]
  apply Prod.ext
  · exact h_rank_from_id'
  · apply Prod.ext
    · calc
        (rank * cRank + remRank) % cRank / cChip = remRank / cChip := by rw [h_remRank_from_id']
        _ = chip := h_chip_from_remRank
    · apply Prod.ext
      · calc
          (rank * cRank + remRank) % cRank % cChip / cBank = remBank / cBank := by
            rw [h_remRank_from_id', h_remBank_from_remRank]
          _ = bank := h_bank_from_remBank
      · calc
          (rank * cRank + remRank) % cRank % cChip % cBank = remBank % cBank := by
            rw [h_remRank_from_id', h_remBank_from_remRank]
          _ = subarrayCoreIdx := h_sub_from_remBank
