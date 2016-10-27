#ifndef XDELTADIFF_H_INCLUDE
#define XDELTADIFF_H_INCLUDE

#include "internal/XdeltaOp.h"
#include <v8.h>

class XdeltaDiff : public XdeltaOp 
{
public:
  virtual ~XdeltaDiff();
  static void Init(v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> DiffChunked(const v8::Arguments& args);

  XdeltaDiff(int s, int d, v8::Local<v8::Object> cfg);
  int mBuffMemSize;
};

#endif //XDELTADIFF_H_INCLUDE