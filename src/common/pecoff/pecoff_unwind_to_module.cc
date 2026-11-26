// Copyright 2026 Google LLC
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// pecoff_unwind_to_module.cc: See pecoff_unwind_to_module.h.
//
// The unwind information format is documented at
// https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64

#ifdef HAVE_CONFIG_H
#include <config.h>  // Must come first
#endif

#include "common/pecoff/pecoff_unwind_to_module.h"

#include <string>
#include <vector>

namespace google_breakpad {

namespace {

// UNWIND_CODE operations.
enum UnwindOperationCode {
  UWOP_PUSH_NONVOL = 0,
  UWOP_ALLOC_LARGE = 1,
  UWOP_ALLOC_SMALL = 2,
  UWOP_SET_FPREG = 3,
  UWOP_SAVE_NONVOL = 4,
  UWOP_SAVE_NONVOL_FAR = 5,
  UWOP_EPILOG = 6,      // UWOP_SAVE_XMM in version 1.
  UWOP_SPARE_CODE = 7,  // UWOP_SAVE_XMM_FAR in version 1.
  UWOP_SAVE_XMM128 = 8,
  UWOP_SAVE_XMM128_FAR = 9,
  UWOP_PUSH_MACHFRAME = 10
};

// UNWIND_INFO flag: a RUNTIME_FUNCTION for the function this one continues
// follows the unwind codes.
const uint8_t kUnwindFlagChainInfo = 0x4;

// Chained unwind information can't legitimately be longer than this; a
// longer chain is a loop in a damaged file.
const int kMaxChainLength = 32;

// Breakpad's names for the registers, in the order Windows numbers them in
// unwind information.
const char* const kRegisterNames[16] = {
  "$rax", "$rcx", "$rdx", "$rbx", "$rsp", "$rbp", "$rsi", "$rdi",
  "$r8",  "$r9",  "$r10", "$r11", "$r12", "$r13", "$r14", "$r15"
};

// One operation of a prologue, decoded from its one to three UNWIND_CODE
// slots.
struct UnwindOperation {
  uint8_t code_offset;  // Offset of the end of its instruction in the
                        // prologue.
  uint8_t operation;    // An UnwindOperationCode.
  uint8_t info;         // Register number, for most operations.
  uint32_t operand;     // Byte count or offset, where there is one.
};

// The parts of an UNWIND_INFO structure used here.
struct UnwindInfo {
  uint8_t frame_register;
  uint8_t frame_offset;  // Scaled by 16.
  // In the order they're stored: the reverse of the prologue's.
  std::vector<UnwindOperation> operations;
  // The function this one continues, if it is part of a function.
  bool is_chained;
  uint32_t chained_unwind_info_rva;
};

uint16_t ReadSlot(const uint8_t* codes, size_t slot) {
  return codes[slot * 2] | (codes[slot * 2 + 1] << 8);
}

bool ParseUnwindInfo(const uint8_t* data, UnwindInfo* info) {
  const uint8_t version = data[0] & 0x7;
  const uint8_t flags = data[0] >> 3;
  if (version != 1 && version != 2)
    return false;
  const uint8_t slot_count = data[2];
  info->frame_register = data[3] & 0xf;
  info->frame_offset = data[3] >> 4;
  const uint8_t* codes = data + 4;

  info->operations.clear();
  for (size_t slot = 0; slot < slot_count;) {
    UnwindOperation operation;
    operation.code_offset = codes[slot * 2];
    operation.operation = codes[slot * 2 + 1] & 0xf;
    operation.info = codes[slot * 2 + 1] >> 4;
    operation.operand = 0;
    size_t slots_used = 1;
    switch (operation.operation) {
      case UWOP_PUSH_NONVOL:
      case UWOP_SET_FPREG:
      case UWOP_PUSH_MACHFRAME:
        break;
      case UWOP_ALLOC_SMALL:
        operation.operand = operation.info * 8 + 8;
        break;
      case UWOP_ALLOC_LARGE:
        if (operation.info == 0) {
          slots_used = 2;
          if (slot + 1 < slot_count)
            operation.operand = ReadSlot(codes, slot + 1) * 8;
        } else {
          slots_used = 3;
          if (slot + 2 < slot_count)
            operation.operand = ReadSlot(codes, slot + 1) |
                                (ReadSlot(codes, slot + 2) << 16);
        }
        break;
      case UWOP_SAVE_NONVOL:
        slots_used = 2;
        if (slot + 1 < slot_count)
          operation.operand = ReadSlot(codes, slot + 1) * 8;
        break;
      case UWOP_SAVE_NONVOL_FAR:
        slots_used = 3;
        if (slot + 2 < slot_count)
          operation.operand = ReadSlot(codes, slot + 1) |
                              (ReadSlot(codes, slot + 2) << 16);
        break;
      case UWOP_EPILOG:
      case UWOP_SAVE_XMM128:
        slots_used = 2;
        break;
      case UWOP_SPARE_CODE:
      case UWOP_SAVE_XMM128_FAR:
        slots_used = 3;
        break;
      default:
        return false;
    }
    if (slot + slots_used > slot_count)
      return false;
    // Epilog codes describe epilogues, not the prologue; the other
    // operations Breakpad has no use for are the XMM register saves.
    if (operation.operation != UWOP_EPILOG &&
        operation.operation != UWOP_SPARE_CODE &&
        operation.operation != UWOP_SAVE_XMM128 &&
        operation.operation != UWOP_SAVE_XMM128_FAR) {
      info->operations.push_back(operation);
    }
    slot += slots_used;
  }

  info->is_chained = (flags & kUnwindFlagChainInfo) != 0;
  info->chained_unwind_info_rva = 0;
  if (info->is_chained) {
    // A RUNTIME_FUNCTION (BeginAddress, EndAddress, UnwindInfoAddress)
    // follows the slots, which are padded to an even number.
    const uint8_t* chained = codes + ((slot_count + 1) & ~1) * 2;
    info->chained_unwind_info_rva = chained[8] | (chained[9] << 8) |
                                    (chained[10] << 16) | (chained[11] << 24);
  }
  return true;
}

// Follows the low-bit indirection some linkers use: an UnwindInfoAddress
// with its lowest bit set is the RVA of another RUNTIME_FUNCTION, whose
// unwind information applies.
const uint8_t* FindUnwindInfo(uint32_t rva, const RVAToPointer& rva_to_pointer) {
  for (int i = 0; i < kMaxChainLength && (rva & 1); ++i) {
    const uint8_t* function = rva_to_pointer(rva & ~1u);
    if (!function)
      return nullptr;
    rva = function[8] | (function[9] << 8) | (function[10] << 16) |
          (function[11] << 24);
  }
  return (rva & 1) ? nullptr : rva_to_pointer(rva);
}

// Tracks where the caller's frame is while the prologue runs: the canonical
// frame address (CFA) is the stack pointer before the call, so the return
// address is at CFA - 8.
class FrameState {
 public:
  FrameState() : stack_size_(8), frame_register_(-1), frame_base_(0) {}

  // Applies |operation|, putting the rules it changes into |changes|.
  // Returns false for an operation STACK CFI can't express.
  bool Apply(const UnwindOperation& operation, const UnwindInfo& info,
             Module::RuleMap* changes) {
    switch (operation.operation) {
      case UWOP_PUSH_NONVOL:
        stack_size_ += 8;
        (*changes)[kRegisterNames[operation.info]] = SavedAt(-stack_size_);
        break;
      case UWOP_ALLOC_SMALL:
      case UWOP_ALLOC_LARGE:
        stack_size_ += operation.operand;
        break;
      case UWOP_SET_FPREG:
        // The frame register is set to RSP + frame_offset * 16. From here on
        // the CFA is found from it, so the stack pointer can move freely.
        frame_register_ = info.frame_register;
        frame_base_ = stack_size_ - info.frame_offset * 16;
        break;
      case UWOP_SAVE_NONVOL:
      case UWOP_SAVE_NONVOL_FAR:
        // Saved at an offset from the stack pointer as it is at this point
        // of the prologue, like RtlVirtualUnwind reads it.
        (*changes)[kRegisterNames[operation.info]] =
            SavedAt(static_cast<int64_t>(operation.operand) - stack_size_);
        break;
      default:
        // UWOP_PUSH_MACHFRAME: a hardware interrupt or exception frame,
        // whose caller's state is in the machine frame, not a CFA.
        return false;
    }
    (*changes)[".cfa"] = CFARule();
    return true;
  }

  std::string CFARule() const {
    if (frame_register_ >= 0) {
      return std::string(kRegisterNames[frame_register_]) + " " +
             std::to_string(frame_base_) + " +";
    }
    return "$rsp " + std::to_string(stack_size_) + " +";
  }

 private:
  static std::string SavedAt(int64_t cfa_offset) {
    return ".cfa " + std::to_string(cfa_offset) + " + ^";
  }

  int64_t stack_size_;     // Bytes between the CFA and the stack pointer.
  int frame_register_;     // -1 until the frame register is set.
  int64_t frame_base_;     // CFA - frame register, once it's set.
};

}  // namespace

std::unique_ptr<Module::StackFrameEntry> X86_64UnwindInfoToStackFrameEntry(
    Module::Address function_address,
    Module::Address function_size,
    uint32_t unwind_info_rva,
    const RVAToPointer& rva_to_pointer) {
  // This function's unwind information, then the functions it continues, in
  // the order an unwinder applies them.
  std::vector<UnwindInfo> chain;
  uint32_t rva = unwind_info_rva;
  do {
    if (chain.size() == kMaxChainLength)
      return nullptr;
    const uint8_t* data = FindUnwindInfo(rva, rva_to_pointer);
    if (!data)
      return nullptr;
    chain.emplace_back();
    if (!ParseUnwindInfo(data, &chain.back()))
      return nullptr;
    rva = chain.back().chained_unwind_info_rva;
  } while (chain.back().is_chained);

  auto entry = std::make_unique<Module::StackFrameEntry>();
  entry->address = function_address;
  entry->size = function_size;
  entry->initial_rules[".cfa"] = "$rsp 8 +";
  entry->initial_rules[".ra"] = ".cfa -8 + ^";

  // Replay the prologue: the outermost function continued first, then each
  // part continuing it, each in prologue order (the reverse of the order the
  // codes are stored in). A function that continues another (chained unwind
  // information) is a later part of it, so the prologue has already run by
  // its first instruction and every rule is in force from its start.
  const bool has_own_prologue = chain.size() == 1;
  FrameState state;
  for (auto info = chain.rbegin(); info != chain.rend(); ++info) {
    for (auto operation = info->operations.rbegin();
         operation != info->operations.rend(); ++operation) {
      Module::RuleMap changes;
      if (!state.Apply(*operation, *info, &changes))
        return nullptr;
      const Module::Address address =
          has_own_prologue ? function_address + operation->code_offset
                           : function_address;
      Module::RuleMap& rules = (address == function_address)
                                   ? entry->initial_rules
                                   : entry->rule_changes[address];
      for (const auto& change : changes)
        rules[change.first] = change.second;
    }
  }
  return entry;
}

}  // namespace google_breakpad
