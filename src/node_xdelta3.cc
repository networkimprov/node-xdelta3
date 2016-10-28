extern "C" {
  #include "../include/xdelta3/xdelta3.h"
}

#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <string>
#include "XdeltaPatch.h"
#include "XdeltaDiff.h"

using namespace v8;
using namespace node;

void init(Handle<Object> exports) {
  XdeltaDiff::Init(exports);
  XdeltaPatch::Init(exports);

  Isolate* isolate = exports->GetIsolate();

  Local<Object> aObj = Object::New(isolate);
  aObj->Set(String::NewFromUtf8(isolate, "SMATCH_SLOW", String::kInternalizedString), Integer::New(isolate, XD3_SMATCH_SLOW));

  aObj->Set(String::NewFromUtf8(isolate, "DEFAULT_IOPT_SIZE", String::kInternalizedString), Integer::New(isolate, XD3_DEFAULT_IOPT_SIZE));
  aObj->Set(String::NewFromUtf8(isolate, "DEFAULT_SPREVSZ", String::kInternalizedString), Integer::New(isolate, XD3_DEFAULT_SPREVSZ));
  aObj->Set(String::NewFromUtf8(isolate, "DEFAULT_WINSIZE", String::kInternalizedString), Integer::New(isolate, XD3_DEFAULT_WINSIZE));

  aObj->Set(String::NewFromUtf8(isolate, "SMATCH_DEFAULT", String::kInternalizedString), Integer::New(isolate, XD3_SMATCH_DEFAULT));
  aObj->Set(String::NewFromUtf8(isolate, "SMATCH_SLOW", String::kInternalizedString), Integer::New(isolate, XD3_SMATCH_SLOW));
  aObj->Set(String::NewFromUtf8(isolate, "SMATCH_FAST", String::kInternalizedString), Integer::New(isolate, XD3_SMATCH_FAST));
  aObj->Set(String::NewFromUtf8(isolate, "SMATCH_FASTER", String::kInternalizedString), Integer::New(isolate, XD3_SMATCH_FASTER));
  aObj->Set(String::NewFromUtf8(isolate, "SMATCH_FASTEST", String::kInternalizedString), Integer::New(isolate, XD3_SMATCH_FASTEST));
  aObj->Set(String::NewFromUtf8(isolate, "SMATCH_SOFT", String::kInternalizedString), Integer::New(isolate, XD3_SMATCH_SOFT));

  aObj->Set(String::NewFromUtf8(isolate, "SEC_DJW", String::kInternalizedString), Integer::New(isolate, XD3_SEC_DJW));
  aObj->Set(String::NewFromUtf8(isolate, "SEC_FGK", String::kInternalizedString), Integer::New(isolate, XD3_SEC_FGK));
  aObj->Set(String::NewFromUtf8(isolate, "SEC_LZMA", String::kInternalizedString), Integer::New(isolate, XD3_SEC_LZMA));
  aObj->Set(String::NewFromUtf8(isolate, "SEC_NODATA", String::kInternalizedString), Integer::New(isolate, XD3_SEC_NODATA));
  aObj->Set(String::NewFromUtf8(isolate, "SEC_NOINST", String::kInternalizedString), Integer::New(isolate, XD3_SEC_NOINST));
  aObj->Set(String::NewFromUtf8(isolate, "SEC_NOADDR", String::kInternalizedString), Integer::New(isolate, XD3_SEC_NOADDR));
  aObj->Set(String::NewFromUtf8(isolate, "ADLER32", String::kInternalizedString), Integer::New(isolate, XD3_ADLER32));
  aObj->Set(String::NewFromUtf8(isolate, "ADLER32_NOVER", String::kInternalizedString), Integer::New(isolate, XD3_ADLER32_NOVER));
  aObj->Set(String::NewFromUtf8(isolate, "ALT_CODE_TABLE", String::kInternalizedString), Integer::New(isolate,  XD3_ALT_CODE_TABLE));
  aObj->Set(String::NewFromUtf8(isolate, "NOCOMPRESS", String::kInternalizedString), Integer::New(isolate, XD3_NOCOMPRESS));
  aObj->Set(String::NewFromUtf8(isolate, "BEGREEDY", String::kInternalizedString), Integer::New(isolate, XD3_BEGREEDY));
  aObj->Set(String::NewFromUtf8(isolate, "ADLER32_RECODE", String::kInternalizedString), Integer::New(isolate, XD3_ADLER32_RECODE));
  aObj->Set(String::NewFromUtf8(isolate, "COMPLEVEL_1", String::kInternalizedString), Integer::New(isolate, XD3_COMPLEVEL_1));
  aObj->Set(String::NewFromUtf8(isolate, "COMPLEVEL_2", String::kInternalizedString), Integer::New(isolate, XD3_COMPLEVEL_2));
  aObj->Set(String::NewFromUtf8(isolate, "COMPLEVEL_3", String::kInternalizedString), Integer::New(isolate, XD3_COMPLEVEL_3));
  aObj->Set(String::NewFromUtf8(isolate, "COMPLEVEL_6", String::kInternalizedString), Integer::New(isolate, XD3_COMPLEVEL_6));
  aObj->Set(String::NewFromUtf8(isolate, "COMPLEVEL_9", String::kInternalizedString), Integer::New(isolate, XD3_COMPLEVEL_9));

  exports->Set(String::NewFromUtf8(isolate, "constants", String::kInternalizedString), aObj);
}

NODE_MODULE(node_xdelta3, init);

Persistent<FunctionTemplate> XdeltaDiff::constructor_template;
Persistent<FunctionTemplate> XdeltaPatch::constructor_template;



