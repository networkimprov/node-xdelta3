#ifndef XDELTADIFF_H_INCLUDE
#define XDELTADIFF_H_INCLUDE

#include "internal/XdeltaOp.h"
#include <v8.h>

class XdeltaDiff : public XdeltaOp 
{
public:
  virtual ~XdeltaDiff();
  static void Init(v8::Handle<v8::Object> exports);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

protected:
  XdeltaDiff(int s, int d, v8::Local<v8::Object> cfg);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DiffChunked(const v8::FunctionCallbackInfo<v8::Value>& args);
  bool loadSecondaryFile() /* overrride */;
  bool generateResult() /*override */;
  void Pool() /* override */;

  unsigned int mBuffMemSize;
};

#endif //XDELTADIFF_H_INCLUDE