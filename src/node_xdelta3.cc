#include <node.h>
#include <v8.h>

#include "../include/xdelta3/xdelta3.h"

using namespace v8;

Handle<Value> DiffChunked(const Arguments& args) {
  HandleScope scope;

  xd3_config config;
  xd3_init_config(&config, XD3_ADLER32);

  return scope.Close(String::New("not developed"));
}

void init(Handle<Object> exports) {
  exports->Set(String::NewSymbol("diff_chunked"), FunctionTemplate::New(DiffChunked)->GetFunction());
}

NODE_MODULE(node_xdelta3, init)
