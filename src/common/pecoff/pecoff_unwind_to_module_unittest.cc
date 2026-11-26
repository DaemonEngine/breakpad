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

// pecoff_unwind_to_module_unittest.cc: Unit tests for
// google_breakpad::X86_64UnwindInfoToStackFrameEntry.

#ifdef HAVE_CONFIG_H
#include <config.h>  // Must come first
#endif

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "breakpad_googletest_includes.h"
#include "common/module.h"
#include "common/pecoff/pecoff_unwind_to_module.h"

using google_breakpad::Module;
using google_breakpad::RVAToPointer;
using google_breakpad::X86_64UnwindInfoToStackFrameEntry;
using std::vector;

namespace {

// Windows' numbers for the registers used here.
const uint8_t kRbx = 3;
const uint8_t kRbp = 5;
const uint8_t kRsi = 6;

const uint8_t kFlagChainInfo = 0x4;

const Module::Address kFunctionAddress = 0x140001000;
const Module::Address kFunctionSize = 0x80;
const uint32_t kUnwindInfoRVA = 0x43000;
const uint32_t kChainedUnwindInfoRVA = 0x43100;

// An UNWIND_CODE slot: an operation, or an operand of the operation before.
struct Slot {
  uint8_t low;
  uint8_t high;
};

Slot Operation(uint8_t code_offset, uint8_t operation, uint8_t info) {
  return Slot{code_offset, static_cast<uint8_t>(operation | (info << 4))};
}
Slot Operand(uint16_t value) {
  return Slot{static_cast<uint8_t>(value & 0xff),
              static_cast<uint8_t>(value >> 8)};
}
Slot PushNonvol(uint8_t code_offset, uint8_t reg) {
  return Operation(code_offset, 0, reg);
}
Slot AllocSmall(uint8_t code_offset, uint32_t size) {
  return Operation(code_offset, 2, static_cast<uint8_t>((size - 8) / 8));
}
Slot SetFpreg(uint8_t code_offset) {
  return Operation(code_offset, 3, 0);
}

// The bytes of an UNWIND_INFO structure, with |slots| in the order they're
// stored (the reverse of the prologue's).
vector<uint8_t> UnwindInfo(uint8_t prologue_size, const vector<Slot>& slots,
                           uint8_t frame_register = 0,
                           uint8_t frame_offset = 0, uint8_t flags = 0,
                           uint8_t version = 1) {
  vector<uint8_t> bytes = {
    static_cast<uint8_t>(version | (flags << 3)),
    prologue_size,
    static_cast<uint8_t>(slots.size()),
    static_cast<uint8_t>(frame_register | (frame_offset << 4))
  };
  for (const Slot& slot : slots) {
    bytes.push_back(slot.low);
    bytes.push_back(slot.high);
  }
  if (slots.size() % 2)
    bytes.insert(bytes.end(), {0, 0});
  return bytes;
}

// Appends the RUNTIME_FUNCTION of the function this one continues.
void AppendChainedFunction(vector<uint8_t>* bytes, uint32_t unwind_info_rva) {
  const uint32_t fields[3] = {0x1000, 0x1080, unwind_info_rva};
  for (uint32_t field : fields) {
    for (int shift = 0; shift < 32; shift += 8)
      bytes->push_back(static_cast<uint8_t>(field >> shift));
  }
}

class X86_64UnwindInfoTest : public ::testing::Test {
 protected:
  std::unique_ptr<Module::StackFrameEntry> Convert() {
    RVAToPointer rva_to_pointer = [this](uint32_t rva) -> const uint8_t* {
      auto found = memory_.find(rva);
      return found == memory_.end() ? nullptr : found->second.data();
    };
    return X86_64UnwindInfoToStackFrameEntry(kFunctionAddress, kFunctionSize,
                                             kUnwindInfoRVA, rva_to_pointer);
  }

  // The file's contents, by RVA.
  std::map<uint32_t, vector<uint8_t>> memory_;
};

}  // namespace

TEST_F(X86_64UnwindInfoTest, NoPrologue) {
  memory_[kUnwindInfoRVA] = UnwindInfo(0, {});
  auto entry = Convert();
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFunctionAddress, entry->address);
  EXPECT_EQ(kFunctionSize, entry->size);
  EXPECT_EQ("$rsp 8 +", entry->initial_rules[".cfa"]);
  EXPECT_EQ(".cfa -8 + ^", entry->initial_rules[".ra"]);
  EXPECT_TRUE(entry->rule_changes.empty());
}

// push rsi; push rbx; sub rsp, 0x58
TEST_F(X86_64UnwindInfoTest, PushesAndAllocation) {
  memory_[kUnwindInfoRVA] = UnwindInfo(
      6, {AllocSmall(6, 0x58), PushNonvol(2, kRbx), PushNonvol(1, kRsi)});
  auto entry = Convert();
  ASSERT_TRUE(entry);
  EXPECT_EQ("$rsp 8 +", entry->initial_rules[".cfa"]);

  Module::RuleMap& after_push_rsi = entry->rule_changes[kFunctionAddress + 1];
  EXPECT_EQ("$rsp 16 +", after_push_rsi[".cfa"]);
  EXPECT_EQ(".cfa -16 + ^", after_push_rsi["$rsi"]);

  Module::RuleMap& after_push_rbx = entry->rule_changes[kFunctionAddress + 2];
  EXPECT_EQ("$rsp 24 +", after_push_rbx[".cfa"]);
  EXPECT_EQ(".cfa -24 + ^", after_push_rbx["$rbx"]);

  EXPECT_EQ("$rsp 112 +", entry->rule_changes[kFunctionAddress + 6][".cfa"]);
  EXPECT_EQ(3U, entry->rule_changes.size());
}

// push rbp; mov rbp, rsp; sub rsp, 0x30 (GCC at -O0)
TEST_F(X86_64UnwindInfoTest, FrameRegisterBeforeAllocation) {
  memory_[kUnwindInfoRVA] = UnwindInfo(
      8, {AllocSmall(8, 0x30), SetFpreg(4), PushNonvol(1, kRbp)}, kRbp, 0);
  auto entry = Convert();
  ASSERT_TRUE(entry);
  EXPECT_EQ(".cfa -16 + ^", entry->rule_changes[kFunctionAddress + 1]["$rbp"]);
  EXPECT_EQ("$rbp 16 +", entry->rule_changes[kFunctionAddress + 4][".cfa"]);
  // The allocation doesn't move the CFA, which is now found from rbp.
  EXPECT_EQ("$rbp 16 +", entry->rule_changes[kFunctionAddress + 8][".cfa"]);
}

// push rbp; push rbx; sub rsp, 0x38; lea rbp, [rsp + 0x30]
TEST_F(X86_64UnwindInfoTest, FrameRegisterAfterAllocation) {
  memory_[kUnwindInfoRVA] = UnwindInfo(
      11, {SetFpreg(11), AllocSmall(6, 0x38), PushNonvol(2, kRbx),
           PushNonvol(1, kRbp)},
      kRbp, 3);
  auto entry = Convert();
  ASSERT_TRUE(entry);
  EXPECT_EQ("$rsp 80 +", entry->rule_changes[kFunctionAddress + 6][".cfa"]);
  // rbp = rsp + 0x30, and the CFA is rsp + 80, so rbp + 32.
  EXPECT_EQ("$rbp 32 +", entry->rule_changes[kFunctionAddress + 11][".cfa"]);
}

// sub rsp, 0x28; mov [rsp + 0x10], rbx
TEST_F(X86_64UnwindInfoTest, SavedRegister) {
  memory_[kUnwindInfoRVA] = UnwindInfo(
      9, {Operation(9, 4, kRbx), Operand(0x10 / 8), AllocSmall(4, 0x28)});
  auto entry = Convert();
  ASSERT_TRUE(entry);
  // Saved at rsp + 0x10, where rsp = CFA - 0x30.
  EXPECT_EQ(".cfa -32 + ^", entry->rule_changes[kFunctionAddress + 9]["$rbx"]);
  EXPECT_EQ("$rsp 48 +", entry->rule_changes[kFunctionAddress + 9][".cfa"]);
}

TEST_F(X86_64UnwindInfoTest, LargeAllocations) {
  // sub rsp, 0x1000; sub rsp, 0x123456
  memory_[kUnwindInfoRVA] = UnwindInfo(
      14, {Operation(14, 1, 1), Operand(0x3456), Operand(0x12),
           Operation(7, 1, 0), Operand(0x1000 / 8)});
  auto entry = Convert();
  ASSERT_TRUE(entry);
  EXPECT_EQ("$rsp 4104 +", entry->rule_changes[kFunctionAddress + 7][".cfa"]);
  EXPECT_EQ("$rsp 1197150 +",
            entry->rule_changes[kFunctionAddress + 14][".cfa"]);
}

// A later part of a function (push rbx; sub rsp, 0x20) whose prologue has
// already run by its first instruction.
TEST_F(X86_64UnwindInfoTest, ChainedUnwindInfo) {
  memory_[kChainedUnwindInfoRVA] =
      UnwindInfo(5, {AllocSmall(5, 0x20), PushNonvol(1, kRbx)});
  vector<uint8_t> part = UnwindInfo(0, {}, 0, 0, kFlagChainInfo);
  AppendChainedFunction(&part, kChainedUnwindInfoRVA);
  memory_[kUnwindInfoRVA] = part;

  auto entry = Convert();
  ASSERT_TRUE(entry);
  EXPECT_EQ("$rsp 48 +", entry->initial_rules[".cfa"]);
  EXPECT_EQ(".cfa -16 + ^", entry->initial_rules["$rbx"]);
  EXPECT_TRUE(entry->rule_changes.empty());
}

TEST_F(X86_64UnwindInfoTest, MachineFrameIsNotConverted) {
  memory_[kUnwindInfoRVA] = UnwindInfo(1, {Operation(1, 10, 0)});
  EXPECT_FALSE(Convert());
}

TEST_F(X86_64UnwindInfoTest, UnknownVersionIsNotConverted) {
  memory_[kUnwindInfoRVA] = UnwindInfo(0, {}, 0, 0, 0, 3);
  EXPECT_FALSE(Convert());
}

TEST_F(X86_64UnwindInfoTest, MissingUnwindInfoIsNotConverted) {
  EXPECT_FALSE(Convert());
}
