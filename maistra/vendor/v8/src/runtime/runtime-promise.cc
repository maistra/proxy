// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/debug/debug.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/microtask-queue.h"
#include "src/logging/counters.h"
#include "src/objects/elements.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_PromiseRejectEventFromStack) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  Handle<Object> rejected_promise = promise;
  if (isolate->debug()->is_active()) {
    // If the Promise.reject() call is caught, then this will return
    // undefined, which we interpret as being a caught exception event.
    rejected_promise = isolate->GetPromiseOnStackOnThrow();
  }
  isolate->RunAllPromiseHooks(PromiseHookType::kResolve, promise,
                              isolate->factory()->undefined_value());
  isolate->debug()->OnPromiseReject(rejected_promise, value);

  // Report only if we don't actually have a handler.
  if (!promise->has_handler()) {
    isolate->ReportPromiseReject(promise, value,
                                 v8::kPromiseRejectWithNoHandler);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseRejectAfterResolved) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, reason, 1);
  isolate->ReportPromiseReject(promise, reason,
                               v8::kPromiseRejectAfterResolved);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseResolveAfterResolved) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, resolution, 1);
  isolate->ReportPromiseReject(promise, resolution,
                               v8::kPromiseResolveAfterResolved);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseRevokeReject) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  // At this point, no revocation has been issued before
  CHECK(!promise->has_handler());
  isolate->ReportPromiseReject(promise, Handle<Object>(),
                               v8::kPromiseHandlerAddedAfterReject);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_EnqueueMicrotask) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  Handle<CallableTask> microtask = isolate->factory()->NewCallableTask(
      function, handle(function->native_context(), isolate));
  MicrotaskQueue* microtask_queue =
      function->native_context().microtask_queue();
  if (microtask_queue) microtask_queue->EnqueueMicrotask(*microtask);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PerformMicrotaskCheckpoint) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  MicrotasksScope::PerformCheckpoint(reinterpret_cast<v8::Isolate*>(isolate));
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_RunMicrotaskCallback) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, microtask_callback, 0);
  CONVERT_ARG_CHECKED(Object, microtask_data, 1);
  MicrotaskCallback callback = ToCData<MicrotaskCallback>(microtask_callback);
  void* data = ToCData<void*>(microtask_data);
  callback(data);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseStatus) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);

  return Smi::FromInt(promise->status());
}

RUNTIME_FUNCTION(Runtime_PromiseHookInit) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, parent, 1);
  isolate->RunPromiseHook(PromiseHookType::kInit, promise, parent);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookBefore) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, promise, 0);
  if (promise->IsJSPromise()) {
    isolate->OnPromiseBefore(Handle<JSPromise>::cast(promise));
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookAfter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, promise, 0);
  if (promise->IsJSPromise()) {
    isolate->OnPromiseAfter(Handle<JSPromise>::cast(promise));
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_RejectPromise) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, reason, 1);
  CONVERT_ARG_HANDLE_CHECKED(Oddball, debug_event, 2);
  return *JSPromise::Reject(promise, reason,
                            debug_event->BooleanValue(isolate));
}

RUNTIME_FUNCTION(Runtime_ResolvePromise) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, resolution, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSPromise::Resolve(promise, resolution));
  return *result;
}

// A helper function to be called when constructing AggregateError objects. This
// takes care of the Error-related construction, e.g., stack traces.
RUNTIME_FUNCTION(Runtime_ConstructAggregateErrorHelper) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, new_target, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, message, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, options, 3);

  DCHECK_EQ(*target, *isolate->aggregate_error_function());

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      ErrorUtils::Construct(isolate, target, new_target, message, options));
  return *result;
}

// A helper function to be called when constructing AggregateError objects. This
// takes care of the Error-related construction, e.g., stack traces.
RUNTIME_FUNCTION(Runtime_ConstructInternalAggregateErrorHelper) {
  HandleScope scope(isolate);
  DCHECK_GE(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(Smi, message, 0);

  Handle<Object> arg0;
  if (args.length() >= 2) {
    DCHECK(args[1].IsObject());
    arg0 = args.at<Object>(1);
  }

  Handle<Object> arg1;
  if (args.length() >= 3) {
    DCHECK(args[2].IsObject());
    arg1 = args.at<Object>(2);
  }

  Handle<Object> arg2;
  if (args.length() >= 4) {
    CHECK(args[3].IsObject());
    arg2 = args.at<Object>(3);
  }

  Handle<Object> options;
  if (args.length() >= 5) {
    CHECK(args[4].IsObject());
    options = args.at<Object>(4);
  } else {
    options = isolate->factory()->undefined_value();
  }

  Handle<Object> message_string = MessageFormatter::Format(
      isolate, MessageTemplate(message->value()), arg0, arg1, arg2);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      ErrorUtils::Construct(isolate, isolate->aggregate_error_function(),
                            isolate->aggregate_error_function(), message_string,
                            options));
  return *result;
}

}  // namespace internal
}  // namespace v8
