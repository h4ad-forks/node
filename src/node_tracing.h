#ifndef SRC_NODE_TRACING_H_
#define SRC_NODE_TRACING_H_

#include <cinttypes>
#include "aliased_buffer.h"
#include "node.h"
#include "node_snapshotable.h"
#include "util.h"
#include "v8-fast-api-calls.h"
#include "v8.h"

#include <string>

namespace node {
class ExternalReferenceRegistry;

namespace tracing {

class BindingData : public SnapshotableObject {
 public:
  BindingData(Realm* realm, v8::Local<v8::Object> obj);

  using InternalFieldInfo = InternalFieldInfoBase;

  SERIALIZABLE_OBJECT_METHODS()
  SET_BINDING_ID(tracing_binding_data)

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)

  static void CreatePerIsolateProperties(IsolateData* isolate_data,
                                         v8::Local<v8::FunctionTemplate> ctor);
  static void CreatePerContextProperties(v8::Local<v8::Object> target,
                                         v8::Local<v8::Value> unused,
                                         v8::Local<v8::Context> context,
                                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
};

}  // namespace tracing

}  // namespace node

#endif // SRC_NODE_TRACING_H_
