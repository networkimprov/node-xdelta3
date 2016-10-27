extern "C" {
  #include "../include/xdelta3/xdelta3.h"
}

#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include "internal/FileReader.h"
#include <string>
#include "XdeltaPatch.h"
#include "XdeltaDiff.h"

using namespace v8;
using namespace node;

void init(Handle<Object> exports) {
  XdeltaDiff::Init(exports);
  XdeltaPatch::Init(exports);

  Local<Object> aObj = Object::New();
  aObj->Set(String::NewSymbol("SMATCH_SLOW"), Integer::New(XD3_SMATCH_SLOW), ReadOnly);

  aObj->Set(String::NewSymbol("DEFAULT_IOPT_SIZE"), Integer::New(XD3_DEFAULT_IOPT_SIZE), ReadOnly);
  aObj->Set(String::NewSymbol("DEFAULT_SPREVSZ"), Integer::New(XD3_DEFAULT_SPREVSZ), ReadOnly);
  aObj->Set(String::NewSymbol("DEFAULT_WINSIZE"), Integer::New(XD3_DEFAULT_WINSIZE), ReadOnly);

  aObj->Set(String::NewSymbol("SMATCH_DEFAULT"), Integer::New(XD3_SMATCH_DEFAULT), ReadOnly);
  aObj->Set(String::NewSymbol("SMATCH_SLOW"), Integer::New(XD3_SMATCH_SLOW), ReadOnly);
  aObj->Set(String::NewSymbol("SMATCH_FAST"), Integer::New(XD3_SMATCH_FAST), ReadOnly);
  aObj->Set(String::NewSymbol("SMATCH_FASTER"), Integer::New(XD3_SMATCH_FASTER), ReadOnly);
  aObj->Set(String::NewSymbol("SMATCH_FASTEST"), Integer::New(XD3_SMATCH_FASTEST), ReadOnly);
  aObj->Set(String::NewSymbol("SMATCH_SOFT"), Integer::New(XD3_SMATCH_SOFT), ReadOnly);

  aObj->Set(String::NewSymbol("SEC_DJW"), Integer::New(XD3_SEC_DJW), ReadOnly);
  aObj->Set(String::NewSymbol("SEC_FGK"), Integer::New(XD3_SEC_FGK), ReadOnly);
  aObj->Set(String::NewSymbol("SEC_LZMA"), Integer::New(XD3_SEC_LZMA), ReadOnly);
  aObj->Set(String::NewSymbol("SEC_NODATA"), Integer::New(XD3_SEC_NODATA), ReadOnly);
  aObj->Set(String::NewSymbol("SEC_NOINST"), Integer::New(XD3_SEC_NOINST), ReadOnly);
  aObj->Set(String::NewSymbol("SEC_NOADDR"), Integer::New(XD3_SEC_NOADDR), ReadOnly);
  aObj->Set(String::NewSymbol("ADLER32"), Integer::New(XD3_ADLER32), ReadOnly);
  aObj->Set(String::NewSymbol("ADLER32_NOVER"), Integer::New(XD3_ADLER32_NOVER), ReadOnly);
  aObj->Set(String::NewSymbol("ALT_CODE_TABLE"), Integer::New( XD3_ALT_CODE_TABLE), ReadOnly);
  aObj->Set(String::NewSymbol("NOCOMPRESS"), Integer::New(XD3_NOCOMPRESS), ReadOnly);
  aObj->Set(String::NewSymbol("BEGREEDY"), Integer::New(XD3_BEGREEDY), ReadOnly);
  aObj->Set(String::NewSymbol("ADLER32_RECODE"), Integer::New(XD3_ADLER32_RECODE), ReadOnly);
  aObj->Set(String::NewSymbol("COMPLEVEL_1"), Integer::New(XD3_COMPLEVEL_1), ReadOnly);
  aObj->Set(String::NewSymbol("COMPLEVEL_2"), Integer::New(XD3_COMPLEVEL_2), ReadOnly);
  aObj->Set(String::NewSymbol("COMPLEVEL_3"), Integer::New(XD3_COMPLEVEL_3), ReadOnly);
  aObj->Set(String::NewSymbol("COMPLEVEL_6"), Integer::New(XD3_COMPLEVEL_6), ReadOnly);
  aObj->Set(String::NewSymbol("COMPLEVEL_9"), Integer::New(XD3_COMPLEVEL_9), ReadOnly);

  exports->Set(String::NewSymbol("constants"), aObj);
}

NODE_MODULE(node_xdelta3, init);

Persistent<FunctionTemplate> XdeltaDiff::constructor_template;
Persistent<FunctionTemplate> XdeltaPatch::constructor_template;



