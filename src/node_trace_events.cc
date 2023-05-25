#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_v8_platform-inl.h"
#include "tracing/agent.h"
#include "util-inl.h"
#include "v8.h"

#include <set>
#include <string>

namespace node {

class ExternalReferenceRegistry;

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Uint8Array;
using v8::Value;

class NodeCategorySet : public BaseObject {
 public:
  static void Initialize(Local<Object> target,
                  Local<Value> unused,
                  Local<Context> context,
                  void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void New(const FunctionCallbackInfo<Value>& args);
  static void Enable(const FunctionCallbackInfo<Value>& args);
  static void Disable(const FunctionCallbackInfo<Value>& args);

  const std::set<std::string>& GetCategories() const { return categories_; }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("categories", categories_);
  }

  SET_MEMORY_INFO_NAME(NodeCategorySet)
  SET_SELF_SIZE(NodeCategorySet)

 private:
  NodeCategorySet(Environment* env,
                  Local<Object> wrap,
                  std::set<std::string>&& categories) :
        BaseObject(env, wrap), categories_(std::move(categories)) {
    MakeWeak();
  }

  bool enabled_ = false;
  const std::set<std::string> categories_;
};

void NodeCategorySet::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  std::set<std::string> categories;
  CHECK(args[0]->IsArray());
  Local<Array> cats = args[0].As<Array>();
  for (size_t n = 0; n < cats->Length(); n++) {
    Local<Value> category;
    if (!cats->Get(env->context(), n).ToLocal(&category)) return;
    Utf8Value val(env->isolate(), category);
    if (!*val) return;
    categories.emplace(*val);
  }
  CHECK_NOT_NULL(GetTracingAgentWriter());
  new NodeCategorySet(env, args.This(), std::move(categories));
}

void NodeCategorySet::Enable(const FunctionCallbackInfo<Value>& args) {
  NodeCategorySet* category_set;
  ASSIGN_OR_RETURN_UNWRAP(&category_set, args.Holder());
  CHECK_NOT_NULL(category_set);
  const auto& categories = category_set->GetCategories();
  if (!category_set->enabled_ && !categories.empty()) {
    // Starts the Tracing Agent if it wasn't started already (e.g. through
    // a command line flag.)
    StartTracingAgent();
    GetTracingAgentWriter()->Enable(categories);
    category_set->enabled_ = true;
  }
}

void NodeCategorySet::Disable(const FunctionCallbackInfo<Value>& args) {
  NodeCategorySet* category_set;
  ASSIGN_OR_RETURN_UNWRAP(&category_set, args.Holder());
  CHECK_NOT_NULL(category_set);
  const auto& categories = category_set->GetCategories();
  if (category_set->enabled_ && !categories.empty()) {
    GetTracingAgentWriter()->Disable(categories);
    category_set->enabled_ = false;
  }
}

void GetEnabledCategories(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  std::string categories =
      GetTracingAgentWriter()->agent()->GetEnabledCategories();
  if (!categories.empty()) {
    args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          categories.c_str(),
                          NewStringType::kNormal,
                          categories.size()).ToLocalChecked());
  }
}

static void SetTraceCategoryStateUpdateHandler(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_trace_category_state_function(args[0].As<Function>());
}

void NodeCategorySet::Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  // Get the pointer of the memory for the flag that
  // store if trace is enabled for http the pointer will
  // always be the same and if the category does not exist
  // it creates: https://github.com/nodejs/node/blob/6bbf2a57fcf33266c5859497f8cc32e1389a358a/deps/v8/src/libplatform/tracing/tracing-controller.cc#L322-L342
  uint8_t* http_flag_pointer = const_cast<uint8_t*>(
    TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("node.http"));

  // NO_OP deleter
  // the idea of this chunk of code is to construct
  auto nop_deleter = [](void*, size_t, void*) {};
  auto backing_store = v8::ArrayBuffer::NewBackingStore(
    http_flag_pointer, 1, nop_deleter, nullptr);
  v8::Local<ArrayBuffer> array_buffer = v8::ArrayBuffer::New(
    isolate, std::move(backing_store));
  v8::Local<Uint8Array> uint8Array = v8::Uint8Array::New(array_buffer, 0, 1);

  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "tracingCategories"),
            uint8Array)
      .Check();

  SetMethod(context, target, "getEnabledCategories", GetEnabledCategories);
  SetMethod(context,
            target,
            "setTraceCategoryStateUpdateHandler",
            SetTraceCategoryStateUpdateHandler);

  Local<FunctionTemplate> category_set =
      NewFunctionTemplate(isolate, NodeCategorySet::New);
  category_set->InstanceTemplate()->SetInternalFieldCount(
      NodeCategorySet::kInternalFieldCount);
  SetProtoMethod(isolate, category_set, "enable", NodeCategorySet::Enable);
  SetProtoMethod(isolate, category_set, "disable", NodeCategorySet::Disable);

  SetConstructorFunction(context, target, "CategorySet", category_set);

  Local<String> isTraceCategoryEnabled =
      FIXED_ONE_BYTE_STRING(env->isolate(), "isTraceCategoryEnabled");
  Local<String> trace = FIXED_ONE_BYTE_STRING(env->isolate(), "trace");

  // Grab the trace and isTraceCategoryEnabled intrinsics from the binding
  // object and expose those to our binding layer.
  Local<Object> binding = context->GetExtrasBindingObject();
  target->Set(context, isTraceCategoryEnabled,
              binding->Get(context, isTraceCategoryEnabled).ToLocalChecked())
                  .Check();
  target->Set(context, trace,
              binding->Get(context, trace).ToLocalChecked()).Check();
}

void NodeCategorySet::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetEnabledCategories);
  registry->Register(SetTraceCategoryStateUpdateHandler);
  registry->Register(NodeCategorySet::New);
  registry->Register(NodeCategorySet::Enable);
  registry->Register(NodeCategorySet::Disable);
}

}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(trace_events,
                                    node::NodeCategorySet::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    trace_events, node::NodeCategorySet::RegisterExternalReferences)
