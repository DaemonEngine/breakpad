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

// pecoff_unwind_to_module.h: Convert the x86-64 Windows unwind information
// of a PE/COFF file (its .pdata and .xdata sections) into Breakpad STACK CFI.
//
// Every x86-64 Windows function that uses the stack has an entry in .pdata
// describing its prologue, whether or not the file has any debug
// information. MinGW's prebuilt runtime libraries (libstdc++, libgcc) are
// built without DWARF, so for their functions this is the only unwind
// information there is.

#ifndef COMMON_PECOFF_PECOFF_UNWIND_TO_MODULE_H__
#define COMMON_PECOFF_PECOFF_UNWIND_TO_MODULE_H__

#include <stdint.h>

#include <functional>
#include <memory>

#include "common/module.h"

namespace google_breakpad {

// Returns a pointer to the bytes at a relative virtual address (RVA) of the
// file, or nullptr if no section contains it.
typedef std::function<const uint8_t*(uint32_t rva)> RVAToPointer;

// Converts the unwind information (an UNWIND_INFO structure) of the function
// at |function_address| (|function_size| bytes long) into a STACK CFI entry
// that recovers the caller's stack pointer, return address and saved
// nonvolatile registers at every instruction after the prologue, and at
// each step of the prologue. |unwind_info_rva| is the function's
// RUNTIME_FUNCTION::UnwindInfoAddress. Chained unwind information is
// followed through |rva_to_pointer|.
//
// Returns nullptr if the unwind information can't be read or describes
// something STACK CFI can't express (a machine frame, pushed by interrupt
// and exception handlers).
std::unique_ptr<Module::StackFrameEntry> X86_64UnwindInfoToStackFrameEntry(
    Module::Address function_address,
    Module::Address function_size,
    uint32_t unwind_info_rva,
    const RVAToPointer& rva_to_pointer);

}  // namespace google_breakpad

#endif  // COMMON_PECOFF_PECOFF_UNWIND_TO_MODULE_H__
