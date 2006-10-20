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

#ifndef GOOGLE_STACK_FRAME_H__
#define GOOGLE_STACK_FRAME_H__

#include <string>
#include "google/airbag_types.h"

namespace google_airbag {

using std::string;

struct StackFrame {
  StackFrame()
      : instruction(),
        module_base(),
        module_name(),
        function_base(),
        function_name(),
        source_file_name(),
        source_line() {}
  virtual ~StackFrame() {}

  // The program counter location as an absolute virtual address.  For the
  // innermost called frame in a stack, this will be an exact program counter
  // or instruction pointer value.  For all other frames, this will be within
  // the instruction that caused execution to branch to a called function,
  // but may not necessarily point to the exact beginning of that instruction.
  u_int64_t instruction;

  // The base address of the module.
  u_int64_t module_base;

  // The module in which the instruction resides.
  string module_name;

  // The start address of the function, may be omitted if debug symbols
  // are not available.
  u_int64_t function_base;

  // The function name, may be omitted if debug symbols are not available.
  string function_name;

  // The source file name, may be omitted if debug symbols are not available.
  string source_file_name;

  // The (1-based) source line number, may be omitted if debug symbols are
  // not available.
  int source_line;
};

}  // namespace google_airbag

#endif  // GOOGLE_STACK_FRAME_H__
