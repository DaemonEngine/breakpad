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

#include <map>
#include <pthread.h>

#include "client/mac/handler/exception_handler.h"
#include "client/mac/handler/minidump_generator.h"

namespace google_airbag {

using std::map;

// These structures and techniques are illustrated in
// Mac OS X Internals, Amit Singh, ch 9.7
struct ExceptionMessage {
  mach_msg_header_t           header;
  mach_msg_body_t             body;
  mach_msg_port_descriptor_t  thread;
  mach_msg_port_descriptor_t  task;
  NDR_record_t                ndr;
  exception_type_t            exception;
  mach_msg_type_number_t      code_count;
  integer_t                   code[EXCEPTION_CODE_MAX];
  char                        padding[512];
};

struct ExceptionParameters {
  ExceptionParameters() : count(0) {}
  mach_msg_type_number_t count;
  exception_mask_t masks[EXC_TYPES_COUNT];
  mach_port_t ports[EXC_TYPES_COUNT];
  exception_behavior_t behaviors[EXC_TYPES_COUNT];
  thread_state_flavor_t flavors[EXC_TYPES_COUNT];
};

struct ExceptionReplyMessage {
  mach_msg_header_t  header;
  NDR_record_t       ndr;
  kern_return_t      return_code;
};

// Each thread needs to keep track of its ExceptionParameters.  Since the time
// that they are needed is when calling through exc_server(), we have no way
// of retrieving the values from the class.  Therefore, we'll create a map
// that will allows storage per thread.
static map<pthread_t, ExceptionParameters *> *s_exception_parameter_map = NULL;

extern "C"
{
  // Forward declarations for functions that need "C" style compilation
  boolean_t exc_server(mach_msg_header_t *request,
                       mach_msg_header_t *reply);

  kern_return_t catch_exception_raise(mach_port_t target_port,
                                      mach_port_t failed_thread,
                                      mach_port_t task,
                                      exception_type_t exception,
                                      exception_data_t code,
                                      mach_msg_type_number_t code_count);

  kern_return_t ForwardException(mach_port_t task,
                                 mach_port_t failed_thread,
                                 exception_type_t exception,
                                 exception_data_t code,
                                 mach_msg_type_number_t code_count);

  kern_return_t exception_raise(mach_port_t target_port,
                                mach_port_t failed_thread,
                                mach_port_t task,
                                exception_type_t exception,
                                exception_data_t exception_code,
                                mach_msg_type_number_t exception_code_count);

  kern_return_t
    exception_raise_state(mach_port_t target_port,
                          mach_port_t failed_thread,
                          mach_port_t task,
                          exception_type_t exception,
                          exception_data_t exception_code,
                          mach_msg_type_number_t code_count,
                          thread_state_flavor_t *target_flavor,
                          thread_state_t thread_state,
                          mach_msg_type_number_t thread_state_count,
                          thread_state_t thread_state,
                          mach_msg_type_number_t *thread_state_count);

  kern_return_t
    exception_raise_state_identity(mach_port_t target_port,
                                   mach_port_t failed_thread,
                                   mach_port_t task,
                                   exception_type_t exception,
                                   exception_data_t exception_code,
                                   mach_msg_type_number_t exception_code_count,
                                   thread_state_flavor_t *target_flavor,
                                   thread_state_t thread_state,
                                   mach_msg_type_number_t thread_state_count,
                                   thread_state_t thread_state,
                                   mach_msg_type_number_t *thread_state_count);
}

ExceptionHandler::ExceptionHandler(const string &dump_path,
                                   FilterCallback filter,
                                   MinidumpCallback callback,
                                   void *callback_context,
                                   bool install_handler)
    : dump_path_(),
      filter_(filter),
      callback_(callback),
      callback_context_(callback_context),
      handler_thread_(NULL),
      handler_port_(0),
      previous_(NULL),
      installed_exception_handler_(false),
      is_in_teardown_(false),
      last_minidump_write_result_(false),
      use_minidump_write_mutex_(false) {
  // This will update to the ID and C-string pointers
  set_dump_path(dump_path);
  MinidumpGenerator::GatherSystemInformation();
  Setup(install_handler);
}

ExceptionHandler::~ExceptionHandler() {
  Teardown();
}

bool ExceptionHandler::WriteMinidump() {
  // If we're currently writing, just return
  if (use_minidump_write_mutex_)
    return false;

  use_minidump_write_mutex_ = true;
  last_minidump_write_result_ = false;

  // Lock the mutex.  Since we just created it, this will return immediately.
  if (pthread_mutex_lock(&minidump_write_mutex_) == 0) {
    // Send an empty message to the handle port so that a minidump will
    // be written
    SendEmptyMachMessage();

    // Wait for the minidump writer to complete its writing.  It will unlock
    // the mutex when completed
    pthread_mutex_lock(&minidump_write_mutex_);
  }
  
  use_minidump_write_mutex_ = false;
  UpdateNextID();
  return last_minidump_write_result_;
}

// static
bool ExceptionHandler::WriteMinidump(const string &dump_path,
                                     MinidumpCallback callback,
                                     void *callback_context) {
  ExceptionHandler handler(dump_path, NULL, callback, callback_context, false);
  return handler.WriteMinidump();
}

bool ExceptionHandler::WriteMinidumpWithException(int exception_type,
                                                  int exception_code,
                                                  mach_port_t thread_name) {
  bool result = false;
  string minidump_id;

  // Putting the MinidumpGenerator in its own context will ensure that the
  // destructor is executed, closing the newly created minidump file.
  if (!dump_path_.empty()) {
    MinidumpGenerator md;
    if (exception_type && exception_code) {
      // If this is a real exception, give the filter (if any) a chance to
      // decided if this should be sent
      if (filter_ && !filter_(callback_context_))
        return false;

      md.SetExceptionInformation(exception_type, exception_code, thread_name);
    }

    result = md.Write(next_minidump_path_c_);
  }

  // Call user specified callback (if any)
  if (callback_) {
    // If the user callback returned true and we're handling an exception
    // (rather than just writing out the file), then we should exit without
    // forwarding the exception to the next handler.
    if (callback_(dump_path_c_, next_minidump_id_c_, callback_context_, 
                  result)) {
      if (exception_type && exception_code)
        exit(exception_type);
    }
  }

  return result;
}

kern_return_t ForwardException(mach_port_t task, mach_port_t failed_thread,
                               exception_type_t exception,
                               exception_data_t code,
                               mach_msg_type_number_t code_count) {
  ExceptionParameters *previous = (*s_exception_parameter_map)[pthread_self()];

  // If we don't have the previous data, we need to just exit
  if (!previous)
    exit(exception);

  // Find the first exception handler that matches the exception
  unsigned int found;
  for (found = 0; found < previous->count; ++found) {
    if (previous->masks[found] & (1 << exception)) {
      break;
    }
  }

  // Nothing to forward
  if (found == previous->count) {
    fprintf(stderr, "** No previous ports for forwarding!! \n");
    exit(KERN_FAILURE);
  }

  mach_port_t target_port = previous->ports[found];
  exception_behavior_t target_behavior = previous->behaviors[found];
  thread_state_flavor_t target_flavor = previous->flavors[found];
  kern_return_t result;

  mach_msg_type_number_t thread_state_count = THREAD_STATE_MAX;
  thread_state_data_t thread_state;
  switch (target_behavior) {
    case EXCEPTION_DEFAULT:
      result = exception_raise(target_port, failed_thread, task, exception,
                               code, code_count);
      break;

    case EXCEPTION_STATE:
      result = thread_get_state(failed_thread, target_flavor, thread_state,
                                &thread_state_count);
      if (result == KERN_SUCCESS)
        result = exception_raise_state(target_port, failed_thread, task,
                                       exception, code,
                                       code_count, &target_flavor,
                                       thread_state, thread_state_count,
                                       thread_state, &thread_state_count);
      if (result == KERN_SUCCESS)
        result = thread_set_state(failed_thread, target_flavor, thread_state,
                                  thread_state_count);
      break;

    case EXCEPTION_STATE_IDENTITY:
      result = thread_get_state(failed_thread, target_flavor, thread_state,
                                &thread_state_count);
      if (result == KERN_SUCCESS)
        result = exception_raise_state_identity(target_port, failed_thread,
                                                task, exception, code,
                                                code_count, &target_flavor,
                                                thread_state,
                                                thread_state_count,
                                                thread_state,
                                                &thread_state_count);
      if (result == KERN_SUCCESS)
        result = thread_set_state(failed_thread, target_flavor, thread_state,
                                  thread_state_count);
      break;

    default:
      result = KERN_FAILURE;
      break;
  }

  return result;
}

// Callback from exc_server()
kern_return_t catch_exception_raise(mach_port_t port, mach_port_t failed_thread,
                                    mach_port_t task,
                                    exception_type_t exception,
                                    exception_data_t code,
                                    mach_msg_type_number_t code_count) {
  return ForwardException(task, failed_thread, exception, code, code_count);
}

// static
void *ExceptionHandler::WaitForMessage(void *exception_handler_class) {
  ExceptionHandler *self =
    reinterpret_cast<ExceptionHandler *>(exception_handler_class);
  ExceptionMessage receive;

  // Save a pointer to our instance so that it will be available in the
  // routines that are called from exc_server();
  if (!s_exception_parameter_map)
    s_exception_parameter_map = new map<pthread_t, ExceptionParameters *>;

  (*s_exception_parameter_map)[pthread_self()] = self->previous_;

  // Wait for the exception info
  while (1) {
    receive.header.msgh_local_port = self->handler_port_;
    receive.header.msgh_size = sizeof(receive);
    kern_return_t result = mach_msg(&(receive.header),
                                    MACH_RCV_MSG | MACH_RCV_LARGE, 0,
                                    sizeof(receive), self->handler_port_,
                                    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    if (result == KERN_SUCCESS) {
      // Uninstall our handler so that we don't get in a loop if the process of
      // writing out a minidump causes an exception.
      self->UninstallHandler();
      
      // If the actual exception code is zero, then we're calling this handler
      // in a way that indicates that we want to either exit this thread or
      // generate a minidump
      if (!receive.exception) {
        if (self->is_in_teardown_)
          return NULL;

        // Write out the dump and save the result for later retrieval
        self->last_minidump_write_result_ =
          self->WriteMinidumpWithException(0, 0, 0);
        if (self->use_minidump_write_mutex_)
          pthread_mutex_unlock(&self->minidump_write_mutex_);
      } else {
        // Generate the minidump with the exception data.
        self->WriteMinidumpWithException(receive.exception, receive.code[0],
                                         receive.thread.name);
      
        // Pass along the exception to the server, which will setup the message
        // and call catch_exception_raise() and put the KERN_SUCCESS into the
        // reply.
        ExceptionReplyMessage reply;
        if (!exc_server(&receive.header, &reply.header))
          exit(1);

        // Send a reply and exit
        result = mach_msg(&(reply.header), MACH_SEND_MSG,
                          reply.header.msgh_size, 0, MACH_PORT_NULL,
                          MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
      }
    }
  }

  return NULL;
}

bool ExceptionHandler::InstallHandler() {
  // Get the actual previous data
  exception_mask_t exception_mask = EXC_MASK_ALL &
  ~(EXC_MASK_BREAKPOINT | EXC_MASK_MACH_SYSCALL |
    EXC_MASK_SYSCALL | EXC_MASK_RPC_ALERT);

  previous_ = new ExceptionParameters();
  previous_->count = EXC_TYPES_COUNT;
  mach_port_t current_task = mach_task_self();
  kern_return_t result = task_get_exception_ports(current_task, exception_mask,
                                                  previous_->masks,
                                                  &previous_->count,
                                                  previous_->ports,
                                                  previous_->behaviors,
                                                  previous_->flavors);
  
  // Setup the exception ports on this task
  if (result == KERN_SUCCESS)
    result = task_set_exception_ports(current_task, exception_mask,
                                      handler_port_, EXCEPTION_DEFAULT,
                                      THREAD_STATE_NONE);

  installed_exception_handler_ = (result == KERN_SUCCESS);

  return installed_exception_handler_;
}

bool ExceptionHandler::UninstallHandler() {
  kern_return_t result = KERN_SUCCESS;
  
  if (installed_exception_handler_) {
    mach_port_t current_task = mach_task_self();
    
    // Restore the previous ports
    for (unsigned int i = 0; i < previous_->count; ++i) {
       result = task_set_exception_ports(current_task, previous_->masks[i],
                                        previous_->ports[i],
                                        previous_->behaviors[i],
                                        previous_->flavors[i]);
      if (result != KERN_SUCCESS)
        return false;
    }
    
    delete previous_;
    previous_ = NULL;
    installed_exception_handler_ = false;
  }
  
  return result == KERN_SUCCESS;
}

bool ExceptionHandler::Setup(bool install_handler) {
  if (pthread_mutex_init(&minidump_write_mutex_, NULL))
    return false;

  // Create a receive right
  mach_port_t current_task = mach_task_self();
  kern_return_t result = mach_port_allocate(current_task,
                                            MACH_PORT_RIGHT_RECEIVE,
                                            &handler_port_);
  // Add send right
  if (result == KERN_SUCCESS)
    result = mach_port_insert_right(current_task, handler_port_, handler_port_,
                                    MACH_MSG_TYPE_MAKE_SEND);

  if (install_handler && result == KERN_SUCCESS)
    if (!InstallHandler())
      return false;

  if (result == KERN_SUCCESS) {
    // Install the handler in its own thread
    if (pthread_create(&handler_thread_, NULL, &WaitForMessage, this) == 0) {
      pthread_detach(handler_thread_);
    }
  }

  return result == KERN_SUCCESS ? true : false;
}

bool ExceptionHandler::Teardown() {
  kern_return_t result = KERN_SUCCESS;
  is_in_teardown_ = true;

  if (!UninstallHandler())
    return false;
  
  // Send an empty message so that the handler_thread exits
  if (SendEmptyMachMessage()) {
    mach_port_t current_task = mach_task_self();
    result = mach_port_deallocate(current_task, handler_port_);
    if (result != KERN_SUCCESS)
      return false;
  } else {
    return false;
  }
  if (s_exception_parameter_map)
    s_exception_parameter_map->erase(handler_thread_);
  
  handler_thread_ = NULL;
  handler_port_ = NULL;
  pthread_mutex_destroy(&minidump_write_mutex_);

  return result == KERN_SUCCESS;
}

bool ExceptionHandler::SendEmptyMachMessage() {
  ExceptionMessage empty;
  memset(&empty, 0, sizeof(empty));
  empty.header.msgh_size = sizeof(empty) - sizeof(empty.padding);
  empty.header.msgh_remote_port = handler_port_;
  empty.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
                                          MACH_MSG_TYPE_MAKE_SEND_ONCE);
  kern_return_t result = mach_msg(&(empty.header),
                                  MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                                  empty.header.msgh_size, 0, 0,
                                  MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

  return result == KERN_SUCCESS;
}

void ExceptionHandler::UpdateNextID() {
  next_minidump_path_ =
    (MinidumpGenerator::UniqueNameInDirectory(dump_path_, &next_minidump_id_));

  next_minidump_path_c_ = next_minidump_path_.c_str();
  next_minidump_id_c_ = next_minidump_id_.c_str();
}

}  // namespace google_airbag
