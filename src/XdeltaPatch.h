#ifndef XDELTAPATCH_H_INCLUDE
#define XDELTAPATCH_H_INCLUDE

#include "internal/XdeltaOp.h"
#include <v8.h>

class XdeltaPatch : public XdeltaOp 
{
public:
  virtual ~XdeltaPatch();
  static void Init(v8::Handle<v8::Object> exports);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

protected:
  XdeltaPatch(int s, int d, v8::Local<v8::Object> cfg);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PatchChunked(const v8::FunctionCallbackInfo<v8::Value>& args);
  bool loadSecondaryFile() /* overrride */;
  bool generateResult() /*override */;

  void FinishAsync();
  v8::Persistent<v8::Object> mBufferObj;
};

#endif //XDELTAPATCH_H_INCLUDE