#include "node_tracing.h"
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
#include <numeric>

namespace node {
namespace tracing {

using v8::Context;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Value;
using v8::Object;
using v8::String;
using v8::Uint8Array;

void BindingData::MemoryInfo(MemoryTracker* tracker) const {}

BindingData::BindingData(Realm* realm, v8::Local<v8::Object> object)
    : SnapshotableObject(realm, object, type_int) {

  // get the pointer of the memory for the flag that store if trace is enabled for http
  // the pointer will always be the same and if the category does not exist, it creates: https://github.com/nodejs/node/blob/6bbf2a57fcf33266c5859497f8cc32e1389a358a/deps/v8/src/libplatform/tracing/tracing-controller.cc#L322-L342
  auto p = const_cast<uint8_t*>(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("node.http"));

  auto nop = [](void*, size_t, void*) {}; // no-op deleter
  auto bs = v8::ArrayBuffer::NewBackingStore(p, 1, nop, nullptr);
  auto ab = v8::ArrayBuffer::New(realm->isolate(), std::move(bs));
  v8::Local<Uint8Array> u8 = v8::Uint8Array::New(ab, 0, 1);

  object
      ->Set(realm->context(),
            FIXED_ONE_BYTE_STRING(realm->isolate(), "tracingCategories"),
            u8)
      .Check();
}

bool BindingData::PrepareForSerialization(v8::Local<v8::Context> context,
                                          v8::SnapshotCreator* creator) {
  return true;
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_EQ(index, BaseObject::kEmbedderType);
  InternalFieldInfo* info =
      InternalFieldInfoBase::New<InternalFieldInfo>(type());
  return info;
}

void BindingData::Deserialize(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_EQ(index, BaseObject::kEmbedderType);
  v8::HandleScope scope(context->GetIsolate());
  Realm* realm = Realm::GetCurrent(context);
  BindingData* binding = realm->AddBindingData<BindingData>(context, holder);
  CHECK_NOT_NULL(binding);
}

void BindingData::CreatePerIsolateProperties(IsolateData* isolate_data,
                                             Local<FunctionTemplate> ctor) {}

void BindingData::CreatePerContextProperties(Local<Object> target,
                                             Local<Value> unused,
                                             Local<Context> context,
                                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  realm->AddBindingData<BindingData>(context, target);
}

void BindingData::RegisterExternalReferences(ExternalReferenceRegistry* registry) {}

}  // namespace tracing

} // node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    tracing, node::tracing::BindingData::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    tracing, node::tracing::BindingData::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    tracing, node::tracing::BindingData::RegisterExternalReferences)
