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

#include <atlbase.h>
#include <DbgHelp.h>
#include <dia2.h>
#include <stdio.h>

#include "common/windows/pdb_source_line_writer.h"
#include "common/windows/guid_string.h"

// This constant may be missing from DbgHelp.h.  See the documentation for
// IDiaSymbol::get_undecoratedNameEx.
#ifndef UNDNAME_NO_ECSU
#define UNDNAME_NO_ECSU 0x8000  // Suppresses enum/class/struct/union.
#endif  // UNDNAME_NO_ECSU

namespace google_airbag {

PDBSourceLineWriter::PDBSourceLineWriter() : output_(NULL) {
}

PDBSourceLineWriter::~PDBSourceLineWriter() {
}

bool PDBSourceLineWriter::Open(const wstring &file, FileFormat format) {
  Close();

  if (FAILED(CoInitialize(NULL))) {
    fprintf(stderr, "CoInitialize failed\n");
    return false;
  }

  CComPtr<IDiaDataSource> data_source;
  if (FAILED(data_source.CoCreateInstance(CLSID_DiaSource))) {
    fprintf(stderr, "CoCreateInstance CLSID_DiaSource failed "
            "(msdia80.dll unregistered?)\n");
    return false;
  }

  switch (format) {
    case PDB_FILE:
      if (FAILED(data_source->loadDataFromPdb(file.c_str()))) {
        fprintf(stderr, "loadDataFromPdb failed\n");
        return false;
      }
      break;
    case EXE_FILE:
      if (FAILED(data_source->loadDataForExe(file.c_str(), NULL, NULL))) {
        fprintf(stderr, "loadDataForExe failed\n");
        return false;
      }
      break;
    default:
      fprintf(stderr, "Unknown file format\n");
      return false;
  }

  if (FAILED(data_source->openSession(&session_))) {
    fprintf(stderr, "openSession failed\n");
  }

  return true;
}

bool PDBSourceLineWriter::PrintLines(IDiaEnumLineNumbers *lines) {
  // The line number format is:
  // <rva> <line number> <source file id>
  CComPtr<IDiaLineNumber> line;
  ULONG count;

  while (SUCCEEDED(lines->Next(1, &line, &count)) && count == 1) {
    DWORD rva;
    if (FAILED(line->get_relativeVirtualAddress(&rva))) {
      fprintf(stderr, "failed to get line rva\n");
      return false;
    }

    DWORD length;
    if (FAILED(line->get_length(&length))) {
      fprintf(stderr, "failed to get line code length\n");
      return false;
    }

    DWORD source_id;
    if (FAILED(line->get_sourceFileId(&source_id))) {
      fprintf(stderr, "failed to get line source file id\n");
      return false;
    }

    DWORD line_num;
    if (FAILED(line->get_lineNumber(&line_num))) {
      fprintf(stderr, "failed to get line number\n");
      return false;
    }

    fprintf(output_, "%x %x %d %d\n", rva, length, line_num, source_id);
    line.Release();
  }
  return true;
}

bool PDBSourceLineWriter::PrintFunction(IDiaSymbol *function) {
  // The function format is:
  // FUNC <address> <length> <param_stack_size> <function>
  DWORD rva;
  if (FAILED(function->get_relativeVirtualAddress(&rva))) {
    fprintf(stderr, "couldn't get rva\n");
    return false;
  }

  ULONGLONG length;
  if (FAILED(function->get_length(&length))) {
    fprintf(stderr, "failed to get function length\n");
    return false;
  }

  CComBSTR name;
  int stack_param_size;
  if (!GetSymbolFunctionName(function, &name, &stack_param_size)) {
    return false;
  }

  // If the decorated name didn't give the parameter size, try to
  // calculate it.
  if (stack_param_size < 0) {
    stack_param_size = GetFunctionStackParamSize(function);
  }

  fprintf(output_, "FUNC %x %llx %x %ws\n",
          rva, length, stack_param_size, name);

  CComPtr<IDiaEnumLineNumbers> lines;
  if (FAILED(session_->findLinesByRVA(rva, DWORD(length), &lines))) {
    return false;
  }

  if (!PrintLines(lines)) {
    return false;
  }
  return true;
}

bool PDBSourceLineWriter::PrintSourceFiles() {
  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global))) {
    fprintf(stderr, "get_globalScope failed\n");
    return false;
  }

  CComPtr<IDiaEnumSymbols> compilands;
  if (FAILED(global->findChildren(SymTagCompiland, NULL,
                                  nsNone, &compilands))) {
    fprintf(stderr, "findChildren failed\n");
    return false;
  }

  CComPtr<IDiaSymbol> compiland;
  ULONG count;
  while (SUCCEEDED(compilands->Next(1, &compiland, &count)) && count == 1) {
    CComPtr<IDiaEnumSourceFiles> source_files;
    if (FAILED(session_->findFile(compiland, NULL, nsNone, &source_files))) {
      return false;
    }
    CComPtr<IDiaSourceFile> file;
    while (SUCCEEDED(source_files->Next(1, &file, &count)) && count == 1) {
      DWORD file_id;
      if (FAILED(file->get_uniqueId(&file_id))) {
        return false;
      }

      CComBSTR file_name;
      if (FAILED(file->get_fileName(&file_name))) {
        return false;
      }

      fwprintf(output_, L"FILE %d %s\n", file_id, file_name);
      file.Release();
    }
    compiland.Release();
  }
  return true;
}

bool PDBSourceLineWriter::PrintFunctions() {
  CComPtr<IDiaEnumSymbolsByAddr> symbols;
  if (FAILED(session_->getSymbolsByAddr(&symbols))) {
    fprintf(stderr, "failed to get symbol enumerator\n");
    return false;
  }

  CComPtr<IDiaSymbol> symbol;
  if (FAILED(symbols->symbolByAddr(1, 0, &symbol))) {
    fprintf(stderr, "failed to enumerate symbols\n");
    return false;
  }

  DWORD rva_last = 0;
  if (FAILED(symbol->get_relativeVirtualAddress(&rva_last))) {
    fprintf(stderr, "failed to get symbol rva\n");
    return false;
  }

  ULONG count;
  do {
    DWORD tag;
    if (FAILED(symbol->get_symTag(&tag))) {
      fprintf(stderr, "failed to get symbol tag\n");
      return false;
    }

    // For a given function, DIA seems to give either a symbol with
    // SymTagFunction or SymTagPublicSymbol, but not both.  This means
    // that PDBSourceLineWriter will output either a FUNC or PUBLIC line,
    // but not both.
    if (tag == SymTagFunction) {
      if (!PrintFunction(symbol)) {
        return false;
      }
    } else if (tag == SymTagPublicSymbol) {
      if (!PrintCodePublicSymbol(symbol)) {
        return false;
      }
    }
    symbol.Release();
  } while (SUCCEEDED(symbols->Next(1, &symbol, &count)) && count == 1);

  return true;
}

bool PDBSourceLineWriter::PrintFrameData() {
  // It would be nice if it were possible to output frame data alongside the
  // associated function, as is done with line numbers, but the DIA API
  // doesn't make it possible to get the frame data in that way.

  CComPtr<IDiaEnumTables> tables;
  if (FAILED(session_->getEnumTables(&tables)))
    return false;

  // Pick up the first table that supports IDiaEnumFrameData.
  CComPtr<IDiaEnumFrameData> frame_data_enum;
  CComPtr<IDiaTable> table;
  ULONG count;
  while (!frame_data_enum &&
         SUCCEEDED(tables->Next(1, &table, &count)) &&
         count == 1) {
    table->QueryInterface(_uuidof(IDiaEnumFrameData),
                          reinterpret_cast<void**>(&frame_data_enum));
    table.Release();
  }
  if (!frame_data_enum)
    return false;

  CComPtr<IDiaFrameData> frame_data;
  while (SUCCEEDED(frame_data_enum->Next(1, &frame_data, &count)) &&
         count == 1) {
    DWORD type;
    if (FAILED(frame_data->get_type(&type)))
      return false;

    DWORD rva;
    if (FAILED(frame_data->get_relativeVirtualAddress(&rva)))
      return false;

    DWORD code_size;
    if (FAILED(frame_data->get_lengthBlock(&code_size)))
      return false;

    DWORD prolog_size;
    if (FAILED(frame_data->get_lengthProlog(&prolog_size)))
      return false;

    // epliog_size is always 0.
    DWORD epilog_size = 0;

    // parameter_size is the size of parameters passed on the stack.  If any
    // parameters are not passed on the stack (such as in registers), their
    // sizes will not be included in parameter_size.
    DWORD parameter_size;
    if (FAILED(frame_data->get_lengthParams(&parameter_size)))
      return false;

    DWORD saved_register_size;
    if (FAILED(frame_data->get_lengthSavedRegisters(&saved_register_size)))
      return false;

    DWORD local_size;
    if (FAILED(frame_data->get_lengthLocals(&local_size)))
      return false;

    // get_maxStack can return S_FALSE, just use 0 in that case.
    DWORD max_stack_size = 0;
    if (FAILED(frame_data->get_maxStack(&max_stack_size)))
      return false;

    // get_programString can return S_FALSE, indicating that there is no
    // program string.  In that case, check whether %ebp is used.
    HRESULT program_string_result;
    CComBSTR program_string;
    if (FAILED(program_string_result = frame_data->get_program(
        &program_string))) {
      return false;
    }

    // get_allocatesBasePointer can return S_FALSE, treat that as though
    // %ebp is not used.
    BOOL allocates_base_pointer = FALSE;
    if (program_string_result != S_OK) {
      if (FAILED(frame_data->get_allocatesBasePointer(
          &allocates_base_pointer))) {
        return false;
      }
    }

    fprintf(output_, "STACK WIN %x %x %x %x %x %x %x %x %x %d ",
            type, rva, code_size, prolog_size, epilog_size,
            parameter_size, saved_register_size, local_size, max_stack_size,
            program_string_result == S_OK);
    if (program_string_result == S_OK) {
      fprintf(output_, "%ws\n", program_string);
    } else {
      fprintf(output_, "%d\n", allocates_base_pointer);
    }

    frame_data.Release();
  }

  return true;
}

bool PDBSourceLineWriter::PrintCodePublicSymbol(IDiaSymbol *symbol) {
  BOOL is_code;
  if (FAILED(symbol->get_code(&is_code))) {
    return false;
  }
  if (!is_code) {
    return true;
  }

  DWORD rva;
  if (FAILED(symbol->get_relativeVirtualAddress(&rva))) {
    return false;
  }

  CComBSTR name;
  int stack_param_size;
  if (!GetSymbolFunctionName(symbol, &name, &stack_param_size)) {
    return false;
  }

  fprintf(output_, "PUBLIC %x %x %ws\n", rva,
          stack_param_size > 0 ? stack_param_size : 0, name);
  return true;
}

// wcstol_positive_strict is sort of like wcstol, but much stricter.  string
// should be a buffer pointing to a null-terminated string containing only
// decimal digits.  If the entire string can be converted to an integer
// without overflowing, and there are no non-digit characters before the
// result is set to the value and this function returns true.  Otherwise,
// this function returns false.  This is an alternative to the strtol, atoi,
// and scanf families, which are not as strict about input and in some cases
// don't provide a good way for the caller to determine if a conversion was
// successful.
static bool wcstol_positive_strict(wchar_t *string, int *result) {
  int value = 0;
  for (wchar_t *c = string; *c != '\0'; ++c) {
    int last_value = value;
    value *= 10;
    // Detect overflow.
    if (value / 10 != last_value || value < 0) {
      return false;
    }
    if (*c < '0' || *c > '9') {
      return false;
    }
    unsigned int c_value = *c - '0';
    last_value = value;
    value += c_value;
    // Detect overflow.
    if (value < last_value) {
      return false;
    }
    // Forbid leading zeroes unless the string is just "0".
    if (value == 0 && *(c+1) != '\0') {
      return false;
    }
  }
  *result = value;
  return true;
}

// static
bool PDBSourceLineWriter::GetSymbolFunctionName(IDiaSymbol *function,
                                                BSTR *name,
                                                int *stack_param_size) {
  *stack_param_size = -1;
  const DWORD undecorate_options = UNDNAME_NO_MS_KEYWORDS |
                                   UNDNAME_NO_FUNCTION_RETURNS |
                                   UNDNAME_NO_ALLOCATION_MODEL |
                                   UNDNAME_NO_ALLOCATION_LANGUAGE |
                                   UNDNAME_NO_THISTYPE |
                                   UNDNAME_NO_ACCESS_SPECIFIERS |
                                   UNDNAME_NO_THROW_SIGNATURES |
                                   UNDNAME_NO_MEMBER_TYPE |
                                   UNDNAME_NO_RETURN_UDT_MODEL |
                                   UNDNAME_NO_ECSU;

  // Use get_undecoratedNameEx to get readable C++ names with arguments.
  if (function->get_undecoratedNameEx(undecorate_options, name) != S_OK) {
    if (function->get_name(name) != S_OK) {
      fprintf(stderr, "failed to get function name\n");
      return false;
    }
    // If a name comes from get_name because no undecorated form existed,
    // it's already formatted properly to be used as output.  Don't do any
    // additional processing.
  } else {
    // C++ uses a bogus "void" argument for functions and methods that don't
    // take any parameters.  Take it out of the undecorated name because it's
    // ugly and unnecessary.
    const wchar_t *replace_string = L"(void)";
    const size_t replace_length = wcslen(replace_string);
    const wchar_t *replacement_string = L"()";
    size_t length = wcslen(*name);
    if (length >= replace_length) {
      wchar_t *name_end = *name + length - replace_length;
      if (wcscmp(name_end, replace_string) == 0) {
        wcscpy_s(name_end, replace_length, replacement_string);
        length = wcslen(*name);
      }
    }

    // Undecorate names used for stdcall and fastcall.  These names prefix
    // the identifier with '_' (stdcall) or '@' (fastcall) and suffix it
    // with '@' followed by the number of bytes of parameters, in decimal.
    // If such a name is found, take note of the size and undecorate it.
    // Only do this for names that aren't C++, which is determined based on
    // whether the undecorated name contains any ':' or '(' characters.
    if (!wcschr(*name, ':') && !wcschr(*name, '(') &&
        (*name[0] == '_' || *name[0] == '@')) {
      wchar_t *last_at = wcsrchr(*name + 1, '@');
      if (last_at && wcstol_positive_strict(last_at + 1, stack_param_size)) {
        // If this function adheres to the fastcall convention, it accepts up
        // to the first 8 bytes of parameters in registers (%ecx and %edx).
        // We're only interested in the stack space used for parameters, so
        // so subtract 8 and don't let the size go below 0.
        if (*name[0] == '@') {
          if (*stack_param_size > 8) {
            *stack_param_size -= 8;
          } else {
            *stack_param_size = 0;
          }
        }

        // Undecorate the name by moving it one character to the left in its
        // buffer, and terminating it where the last '@' had been.
        wcsncpy_s(*name, length, *name + 1, last_at - *name - 1);
      } else if (*name[0] == '_') {
        // This symbol's name is encoded according to the cdecl rules.  The
        // name doesn't end in a '@' character followed by a decimal positive
        // integer, so it's not a stdcall name.  Strip off the leading
        // underscore.
        wcsncpy_s(*name, length, *name + 1, length - 1);
      }
    }
  }

  return true;
}

// static
int PDBSourceLineWriter::GetFunctionStackParamSize(IDiaSymbol *function) {
  // This implementation is highly x86-specific.

  // Gather the symbols corresponding to data.
  CComPtr<IDiaEnumSymbols> data_children;
  if (FAILED(function->findChildren(SymTagData, NULL, nsNone,
                                    &data_children))) {
    return 0;
  }

  // lowest_base is the lowest %ebp-relative byte offset used for a parameter.
  // highest_end is one greater than the highest offset (i.e. base + length).
  // Stack parameters are assumed to be contiguous, because in reality, they
  // are.
  int lowest_base = INT_MAX;
  int highest_end = INT_MIN;

  CComPtr<IDiaSymbol> child;
  DWORD count;
  while (SUCCEEDED(data_children->Next(1, &child, &count)) && count == 1) {
    // If any operation fails at this point, just proceed to the next child.
    // Use the next_child label instead of continue because child needs to
    // be released before it's reused.  Declare constructable/destructable
    // types early to avoid gotos that cross initializations.
    CComPtr<IDiaSymbol> child_type;

    // DataIsObjectPtr is only used for |this|.  Because |this| can be passed
    // as a stack parameter, look for it in addition to traditional
    // parameters.
    DWORD child_kind;
    if (FAILED(child->get_dataKind(&child_kind)) ||
        (child_kind != DataIsParam && child_kind != DataIsObjectPtr)) {
      goto next_child;
    }

    // Only concentrate on register-relative parameters.  Parameters may also
    // be enregistered (passed directly in a register), but those don't
    // consume any stack space, so they're not of interest.
    DWORD child_location_type;
    if (FAILED(child->get_locationType(&child_location_type)) ||
        child_location_type != LocIsRegRel) {
      goto next_child;
    }

    // Of register-relative parameters, the only ones that make any sense are
    // %ebp- or %esp-relative.  Note that MSVC's debugging information always
    // gives parameters as %ebp-relative even when a function doesn't use a
    // traditional frame pointer and stack parameters are accessed relative to
    // %esp, so just look for %ebp-relative parameters.  If you wanted to
    // access parameters, you'd probably want to treat these %ebp-relative
    // offsets as if they were relative to %esp before a function's prolog
    // executed.
    DWORD child_register;
    if (FAILED(child->get_registerId(&child_register)) ||
        child_register != CV_REG_EBP) {
      goto next_child;
    }

    LONG child_register_offset;
    if (FAILED(child->get_offset(&child_register_offset))) {
      goto next_child;
    }

    if (FAILED(child->get_type(&child_type))) {
      goto next_child;
    }

    ULONGLONG child_length;
    if (FAILED(child_type->get_length(&child_length))) {
      goto next_child;
    }

    int child_end = child_register_offset + static_cast<ULONG>(child_length);
    if (child_register_offset < lowest_base) {
      lowest_base = child_register_offset;
    }
    if (child_end > highest_end) {
      highest_end = child_end;
    }

next_child:
    child.Release();
  }

  int param_size = 0;
  // Make sure lowest_base isn't less than 4, because [%esp+4] is the lowest
  // possible address to find a stack parameter before executing a function's
  // prolog (see above).  Some optimizations cause parameter offsets to be
  // lower than 4, but we're not concerned with those because we're only
  // looking for parameters contained in addresses higher than where the
  // return address is stored.
  if (lowest_base < 4) {
    lowest_base = 4;
  }
  if (highest_end > lowest_base) {
    // All stack parameters are pushed as at least 4-byte quantities.  If the
    // last type was narrower than 4 bytes, promote it.  This assumes that all
    // parameters' offsets are 4-byte-aligned, which is always the case.  Only
    // worry about the last type, because we're not summing the type sizes,
    // just looking at the lowest and highest offsets.
    int remainder = highest_end % 4;
    if (remainder) {
      highest_end += 4 - remainder;
    }

    param_size = highest_end - lowest_base;
  }

  return param_size;
}

bool PDBSourceLineWriter::WriteMap(FILE *map_file) {
  bool ret = false;
  output_ = map_file;
  if (PrintSourceFiles() && PrintFunctions() && PrintFrameData()) {
    ret = true;
  }

  output_ = NULL;
  return ret;
}

void PDBSourceLineWriter::Close() {
  session_.Release();
}

wstring PDBSourceLineWriter::GetModuleGUID() {
  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global))) {
    return L"";
  }

  GUID guid;
  if (FAILED(global->get_guid(&guid))) {
    return L"";
  }

  return GUIDString::GUIDToWString(&guid);
}

}  // namespace google_airbag
