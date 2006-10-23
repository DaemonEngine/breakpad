// Copyright (c) 2006, Google Inc.
// All rights reserved.
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
//     * Neither the name of Google Inc. nor the names of its
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

// Unit test for MinidumpProcessor.  Uses a pre-generated minidump and
// corresponding symbol file, and checks the stack frames for correctness.

#include <string>
#include "google/call_stack.h"
#include "google/minidump_processor.h"
#include "google/stack_frame.h"
#include "google/symbol_supplier.h"
#include "processor/minidump.h"
#include "processor/scoped_ptr.h"

using std::string;
using google_airbag::CallStack;
using google_airbag::MinidumpProcessor;
using google_airbag::scoped_ptr;

#define ASSERT_TRUE(cond) \
  if (!(cond)) {                                                        \
    fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_EQ(e1, e2) ASSERT_TRUE((e1) == (e2))

namespace google_airbag {

class TestSymbolSupplier : public SymbolSupplier {
 public:
  virtual string GetSymbolFile(MinidumpModule *module);
};

string TestSymbolSupplier::GetSymbolFile(MinidumpModule *module) {
  if (*(module->GetName()) == "c:\\test_app.exe") {
    return string(getenv("srcdir") ? getenv("srcdir") : ".") +
      "/src/processor/testdata/minidump2.sym";
  }

  return "";
}

}  // namespace google_airbag

using google_airbag::TestSymbolSupplier;

static bool RunTests() {

  TestSymbolSupplier supplier;
  MinidumpProcessor processor(&supplier);

  string minidump_file = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                         "/src/processor/testdata/minidump2.dmp";

  scoped_ptr<CallStack> stack(processor.Process(minidump_file));
  ASSERT_TRUE(stack.get());
  ASSERT_EQ(stack->frames()->size(), 4);

  ASSERT_EQ(stack->frames()->at(0)->module_base, 0x400000);
  ASSERT_EQ(stack->frames()->at(0)->module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(0)->function_name, "CrashFunction()");
  ASSERT_EQ(stack->frames()->at(0)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(0)->source_line, 65);

  ASSERT_EQ(stack->frames()->at(1)->module_base, 0x400000);
  ASSERT_EQ(stack->frames()->at(1)->module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(1)->function_name, "main");
  ASSERT_EQ(stack->frames()->at(1)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(1)->source_line, 70);

  // This comes from the CRT
  ASSERT_EQ(stack->frames()->at(2)->module_base, 0x400000);
  ASSERT_EQ(stack->frames()->at(2)->module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(2)->function_name, "__tmainCRTStartup");
  ASSERT_EQ(stack->frames()->at(2)->source_file_name,
            "f:\\rtm\\vctools\\crt_bld\\self_x86\\crt\\src\\crt0.c");
  ASSERT_EQ(stack->frames()->at(2)->source_line, 318);

  // No debug info available for kernel32.dll
  ASSERT_EQ(stack->frames()->at(3)->module_base, 0x7c800000);
  ASSERT_EQ(stack->frames()->at(3)->module_name,
            "C:\\WINDOWS\\system32\\kernel32.dll");
  ASSERT_TRUE(stack->frames()->at(3)->function_name.empty());
  ASSERT_TRUE(stack->frames()->at(3)->source_file_name.empty());
  ASSERT_EQ(stack->frames()->at(3)->source_line, 0);

  return true;
}

int main(int argc, char *argv[]) {
  if (!RunTests()) {
    return 1;
  }

  return 0;
}
