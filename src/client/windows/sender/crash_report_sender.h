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

#ifndef CLIENT_WINDOWS_SENDER_CRASH_REPORT_SENDER_H__
#define CLIENT_WINDOWS_SENDER_CRASH_REPORT_SENDER_H__

// CrashReportSender is a "static" class which provides an API to upload
// crash reports via HTTP.  A crash report is formatted as a multipart POST
// request, which contains a set of caller-supplied string key/value pairs,
// and a minidump file to upload.
//
// To use this library in your project, you will need to link against
// wininet.lib.

#include <string>
#include <map>

namespace google_airbag {

using std::wstring;
using std::map;

class CrashReportSender {
 public:
  // Sends the specified minidump file, along with the map of
  // name value pairs, as a multipart POST request to the given URL.
  // Parameter names must contain only printable ASCII characters,
  // and may not contain a quote (") character.
  // Only HTTP URLs are currently supported.  Returns true on success.
  // TODO(bryner): we should expose the response to the caller.
  static bool SendCrashReport(const wstring &url,
                              const map<wstring, wstring> &parameters,
                              const wstring &dump_file_name);

 private:
  // No instances of this class should be created.
  // Disallow all constructors, destructors, and operator=.
  CrashReportSender();
  explicit CrashReportSender(const CrashReportSender &);
  void operator=(const CrashReportSender &);
  ~CrashReportSender();
};

}  // namespace google_airbag

#endif  // CLIENT_WINDOWS_SENDER_CRASH_REPORT_SENDER_H__
