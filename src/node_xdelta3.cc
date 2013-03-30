#include <node.h>

#include <v8.h>

#include "../include/xdelta3/xdelta3.h"

#define DEFAULT_CHUNK_SIZE 16384

using namespace v8;

struct DiffChunked_data {
  DiffChunked_data(int s, int d, Handle<Function> cbData, Handle<Function> cbEnd) : src(s), dst(d), callbackData(cbData), callbackEnd(cbEnd), errCode(0) {}
  int src, dst;
  Persistent<Function> callbackData, callbackEnd;
  int bytesRead;
  int errCode;
};

void DiffChunked_pool(uv_work_t* req) {
  DiffChunked_data* aData = (DiffChunked_data*) req->data;

  char tmp[DEFAULT_CHUNK_SIZE];

  uv_fs_t uvReq;
  aData->bytesRead = uv_fs_read(uv_default_loop(), &uvReq, aData->src, tmp, DEFAULT_CHUNK_SIZE, 0, NULL);
  if (aData->bytesRead < 0) {
    aData->errCode = uv_last_error(uv_default_loop()).code;             
  }

  aData->bytesRead = 0;
}

void DiffChunked_done(uv_work_t* req, int ) {
  HandleScope scope;
  DiffChunked_data* aData = (DiffChunked_data*) req->data;
  
  if (aData->bytesRead == 0) {
    //emit the end
    TryCatch try_catch;
    aData->callbackEnd->Call(Context::GetCurrent()->Global(), 0, NULL);
    if (try_catch.HasCaught())
      node::FatalException(try_catch);
    aData->callbackData.Dispose();
    aData->callbackEnd.Dispose();
    delete req;  
    delete aData;
  } else if (aData->bytesRead > 0){
    //TODO: emit the buffer chunk
    //keep on working
    uv_queue_work(uv_default_loop(), req, DiffChunked_pool, DiffChunked_done);
  } else {
    //TODO: error handling
    //UVException(code, #func, "", path);
  }
}

Handle<Value> DiffChunked(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsFunction() || !args[3]->IsFunction()) {
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd, function, function)")));
  }

  DiffChunked_data* aData;
  aData = new DiffChunked_data(args[0]->Uint32Value(), args[1]->Uint32Value(), Persistent<Function>::New(Local<Function>::Cast(args[2])), Persistent<Function>::New(Local<Function>::Cast(args[3])));
  uv_work_t* aReq = new uv_work_t();
  aReq->data = aData;

  uv_queue_work(uv_default_loop(), aReq, DiffChunked_pool, DiffChunked_done);
  
  return scope.Close(String::New("not developed"));
}

void init(Handle<Object> exports) {
  exports->Set(String::NewSymbol("diff_chunked"), FunctionTemplate::New(DiffChunked)->GetFunction());
}

NODE_MODULE(node_xdelta3, init)

