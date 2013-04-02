#include <node.h>
#include <node_buffer.h>
#include <v8.h>

extern "C" {
  #include "../include/xdelta3/xdelta3.h"
}

#define DEFAULT_CHUNK_SIZE 16384
#define BLOCK_SIZE_XDELTA3 XD3_ALLOCSIZE

using namespace v8;

struct DiffChunked_data {
  DiffChunked_data(int s, int d, Handle<Function> cbData, Handle<Function> cbEnd) : src(s), dst(d), callbackData(cbData), callbackEnd(cbEnd), diffBuffSize(0),
      wroteFromStream(0), readDstN(0), errType(0), errCode(0), firstTime(true) {
    memset (&stream, 0, sizeof (stream));
    memset (&source, 0, sizeof (source));
    config.winsize = BLOCK_SIZE_XDELTA3;
    source.blksize = BLOCK_SIZE_XDELTA3;
    source.curblk = (const uint8_t*) new char[source.blksize];
    inputBuf = (void*) new char[source.blksize];
    diffBuff = new char[DEFAULT_CHUNK_SIZE];
  };
  ~DiffChunked_data() {
    delete[] (char*)source.curblk;
    delete[] (char*)inputBuf;
    delete[] diffBuff;
    callbackData.Dispose();
    callbackEnd.Dispose();
  };

  bool firstTime;
  int src, dst;
  Persistent<Function> callbackData, callbackEnd;
  int diffBuffSize;
  char* diffBuff;
  int wroteFromStream;
  int readDstN;

  int errType; // 0 - none, -1 - libuv, -2 - xdelta3
  int errCode;

  struct stat statbuf;
  void* inputBuf;
  xd3_stream stream;
  xd3_config config;
  xd3_source source;
};

void DiffChunked_pool(uv_work_t* req) {
  DiffChunked_data* aData = (DiffChunked_data*) req->data;

  aData->diffBuffSize = 0;

  if (aData->firstTime) {
    aData->firstTime = false;
    xd3_init_config(&aData->config, XD3_ADLER32);
    xd3_config_stream(&aData->stream, &aData->config);

    uv_fs_t uvReq;
    int aBytesRead;
    aBytesRead = uv_fs_read(uv_default_loop(), &uvReq, aData->src, (void*)aData->source.curblk, aData->source.blksize, 0, NULL);
    if (aBytesRead < 0) {
      aData->errType = -1;
      aData->errCode = uv_last_error(uv_default_loop()).code;
      return;
    }
    aData->source.onblk = aBytesRead;
    aData->source.curblkno = 0;

    xd3_set_source(&aData->stream, &aData->source);
  }

  //TODO: write the rest of aData->stream.avail_out to diffBuff
  if (aData->wroteFromStream < aData->stream.avail_out)
    xd3_consume_output(&aData->stream);

  int aInputBufRead;
  bool aRead = aData->firstTime; //if it is not firstTime we are coming from a XD3_OUTPUT
  do {
    if (aRead) {
      aRead = false;
      uv_fs_t uvReq;
      aInputBufRead = uv_fs_read(uv_default_loop(), &uvReq, aData->dst, aData->inputBuf, BLOCK_SIZE_XDELTA3, aData->readDstN * BLOCK_SIZE_XDELTA3, NULL);
      aData->readDstN++;
      if (aInputBufRead < 0) {
        aData->errType = -1;
        aData->errCode = uv_last_error(uv_default_loop()).code;
        return;
      }
      if (aInputBufRead < BLOCK_SIZE_XDELTA3)
      {
        xd3_set_flags(&aData->stream, XD3_FLUSH | aData->stream.flags);
      }
      xd3_avail_input(&aData->stream, (const uint8_t*) aData->inputBuf, aInputBufRead);
    }

    int ret = xd3_encode_input(&aData->stream);
    switch (ret)
    {
    case XD3_INPUT:
      aRead = true;
      continue;

    case XD3_OUTPUT:
    {
      int aWriteSize = (aData->stream.avail_out > DEFAULT_CHUNK_SIZE - aData->diffBuffSize) ? DEFAULT_CHUNK_SIZE - aData->diffBuffSize : aData->stream.avail_out;
      memcpy(aData->diffBuff + aData->diffBuffSize, aData->stream.next_out, aWriteSize);
      aData->diffBuffSize += aWriteSize;
      aData->wroteFromStream = aWriteSize;

      if (aData->wroteFromStream < aData->stream.avail_out) { //diff buffer is full
        return;
      } else
        xd3_consume_output(&aData->stream);

      continue;
    }
    case XD3_GETSRCBLK:
    {
      uv_fs_t uvReq;
      int aBytesRead;
      aBytesRead = uv_fs_read(uv_default_loop(), &uvReq, aData->src, (void*) aData->source.curblk, aData->source.blksize, aData->source.blksize * aData->source.getblkno, NULL);
      if (aBytesRead < 0) {
        aData->errType = -1;
        aData->errCode = uv_last_error(uv_default_loop()).code;
        return;
      }
      aData->source.onblk = aBytesRead;
      aData->source.curblkno = aData->source.getblkno;
      continue;  
    }
    case XD3_GOTHEADER:
    case XD3_WINSTART:
    case XD3_WINFINISH:
      continue;
    default:
      aData->errType = -2;
      //TODO: error handling
      return;
    }


  } while (aInputBufRead == BLOCK_SIZE_XDELTA3);
}

void DiffChunked_done(uv_work_t* req, int ) {
  HandleScope scope;
  DiffChunked_data* aData = (DiffChunked_data*) req->data;
  
  if (aData->errType != 0) {
    //TODO: error handling
    //UVException(code, #func, "", path);
  } else if (aData->diffBuffSize == 0) {
    //emit the end
    TryCatch try_catch;
    aData->callbackEnd->Call(Context::GetCurrent()->Global(), 0, NULL);
    if (try_catch.HasCaught())
      node::FatalException(try_catch);
    delete req;
    delete aData;
  } else {
    //emit the data

    node::Buffer *aSlowBuffer = node::Buffer::New(aData->diffBuff, aData->diffBuffSize);
    Local<Function> aBufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(String::New("Buffer")));
    Handle<Value> aConstructorArgs[3] = { aSlowBuffer->handle_, Integer::New(aData->diffBuffSize), Integer::New(0) };
    Local<Object> aActualBuffer = aBufferConstructor->NewInstance(3, aConstructorArgs);

    Local<Value> aArgv[1];
    aArgv[0] = aActualBuffer;
    TryCatch try_catch;
    aData->callbackData->Call(Context::GetCurrent()->Global(), 1, aArgv);
    if (try_catch.HasCaught())
      node::FatalException(try_catch);

    //keep on working
    uv_queue_work(uv_default_loop(), req, DiffChunked_pool, DiffChunked_done);
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

