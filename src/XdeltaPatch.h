#ifndef XDELTAPATCH_H_INCLUDE
#define XDELTAPATCH_H_INCLUDE

#include "internal/XdeltaOp.h"
#include <v8.h>

class XdeltaPatch : public XdeltaOp 
{
public:
  virtual ~XdeltaPatch();
  static void Init(v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> PatchChunked(const v8::Arguments& args);
  bool loadSecondaryFile() /* overrride */;

  XdeltaPatch(int s, int d, v8::Local<v8::Object> cfg);
  void FinishAsync();
  v8::Persistent<v8::Object> mBufferObj;
};

#endif //XDELTAPATCH_H_INCLUDE