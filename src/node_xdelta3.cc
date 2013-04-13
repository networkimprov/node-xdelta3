extern "C" {
  #include "../include/xdelta3/xdelta3.h"
}

#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <string>

#define DEFAULT_CHUNK_SIZE 16384
#define BLOCK_SIZE_XDELTA3 XD3_ALLOCSIZE

using namespace v8;

void init(Handle<Object> exports) {
  exports->Set(String::NewSymbol("diff_chunked"), FunctionTemplate::New(DiffChunked)->GetFunction());
}

NODE_MODULE(node_xdelta3, init);

Handle<Value> DiffChunked(const Arguments& args);

struct DiffChunked_data {
  DiffChunked_data(int s, int d, Local<Function> cbData, Local<Function> cbEnd)
    : src(s), dst(d), firstTime(true), finishedProcessing(false), diffBuffSize(0), wroteFromStream(0), readDstN(0), errType(0)
  {
    callbackData = Persistent<Function>::New(cbData);
    callbackEnd = Persistent<Function>::New(cbEnd);
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

  int src, dst;
  Persistent<Function> callbackData, callbackEnd;

  bool firstTime;
  bool finishedProcessing;
  char* diffBuff;
  int diffBuffSize;
  unsigned wroteFromStream;
  int readDstN;

  int errType; // 0 - none, -1 - libuv, -2 - xdelta3
  uv_err_t uvErr;
  std::string xdeltaErr;

  struct stat statbuf;
  void* inputBuf;
  xd3_stream stream;
  xd3_config config;
  xd3_source source;
};

void DiffChunked_pool(uv_work_t* req);
void DiffChunked_done(uv_work_t* req, int );

Handle<Value> DiffChunked(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsFunction() || !args[3]->IsFunction()) {
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd, function, function)")));
  }

  DiffChunked_data* aData;
  aData = new DiffChunked_data(args[0]->Uint32Value(), args[1]->Uint32Value(), Local<Function>::Cast(args[2]), Local<Function>::Cast(args[3]));
  uv_work_t* aReq = new uv_work_t();
  aReq->data = aData;

  uv_queue_work(uv_default_loop(), aReq, DiffChunked_pool, DiffChunked_done);
  
  return scope.Close(String::New("not developed"));
}

void DiffChunked_pool(uv_work_t* req) {
  DiffChunked_data* aData = (DiffChunked_data*) req->data;
  aData->diffBuffSize = 0;

  if (aData->firstTime) {
    xd3_init_config(&aData->config, XD3_ADLER32);
    xd3_config_stream(&aData->stream, &aData->config);

    uv_fs_t aUvReq;
    int aBytesRead;
    aBytesRead = uv_fs_read(uv_default_loop(), &aUvReq, aData->src, (void*)aData->source.curblk, aData->source.blksize, 0, NULL);
    if (aBytesRead < 0) {
      aData->errType = -1;
      aData->uvErr = uv_last_error(uv_default_loop());
      return;
    }
    aData->source.onblk = aBytesRead;
    aData->source.curblkno = 0;

    xd3_set_source(&aData->stream, &aData->source);
  }

  if (aData->wroteFromStream < aData->stream.avail_out) { //if there is something left in the out stream to emit
    int aWriteSize = (aData->stream.avail_out - aData->wroteFromStream > DEFAULT_CHUNK_SIZE) ? DEFAULT_CHUNK_SIZE : aData->stream.avail_out - aData->wroteFromStream;
    memcpy(aData->diffBuff, aData->stream.next_out + aData->wroteFromStream, aWriteSize);
    aData->diffBuffSize += aWriteSize;
    aData->wroteFromStream += aWriteSize;
    if (aData->wroteFromStream < aData->stream.avail_out)
      return;
    else
      xd3_consume_output(&aData->stream);
  }

  if (aData->finishedProcessing)
    return;

  int aInputBufRead = 0;
  bool aRead = aData->firstTime; //if it is not firstTime, XD3_OUTPUT was called last, so read is not necessary
  aData->firstTime = false;
  do {
    if (aRead) {
      aRead = false;
      uv_fs_t aUvReq;
      aInputBufRead = uv_fs_read(uv_default_loop(), &aUvReq, aData->dst, aData->inputBuf, BLOCK_SIZE_XDELTA3, aData->readDstN * BLOCK_SIZE_XDELTA3, NULL);
      aData->readDstN++;
      if (aInputBufRead < 0) {
        aData->errType = -1;
        aData->uvErr = uv_last_error(uv_default_loop());
        return;
      }
      if (aInputBufRead < (int) BLOCK_SIZE_XDELTA3)
      {
        xd3_set_flags(&aData->stream, XD3_FLUSH | aData->stream.flags);
      }
      xd3_avail_input(&aData->stream, (const uint8_t*) aData->inputBuf, aInputBufRead);
    }

    process:

    int aRet = xd3_encode_input(&aData->stream);
    switch (aRet)
    {
    case XD3_INPUT:
      aRead = true;
      continue;

    case XD3_OUTPUT:
    {
      int aWriteSize = ((int)aData->stream.avail_out > DEFAULT_CHUNK_SIZE - aData->diffBuffSize) ? DEFAULT_CHUNK_SIZE - aData->diffBuffSize : aData->stream.avail_out;
      memcpy(aData->diffBuff + aData->diffBuffSize, aData->stream.next_out, aWriteSize);
      aData->diffBuffSize += aWriteSize;
      aData->wroteFromStream = aWriteSize;

      if (aData->wroteFromStream < aData->stream.avail_out) //diff buffer is full
        return;
      else
        xd3_consume_output(&aData->stream);

      goto process;
    }
    case XD3_GETSRCBLK:
    {
      uv_fs_t aUvReq;
      int aBytesRead;
      aBytesRead = uv_fs_read(uv_default_loop(), &aUvReq, aData->src, (void*) aData->source.curblk, aData->source.blksize, aData->source.blksize * aData->source.getblkno, NULL);
      if (aBytesRead < 0) {
        aData->errType = -1;
        aData->uvErr = uv_last_error(uv_default_loop());
        return;
      }
      aData->source.onblk = aBytesRead;
      aData->source.curblkno = aData->source.getblkno;
      goto process;  
    }
    case XD3_GOTHEADER:
    case XD3_WINSTART:
    case XD3_WINFINISH:
      goto process;
    default:
      aData->errType = -2;
      aData->xdeltaErr = aData->stream.msg;
      xd3_close_stream(&aData->stream);
      xd3_free_stream(&aData->stream);
      return;
    }

  } while (aInputBufRead == BLOCK_SIZE_XDELTA3);

  xd3_close_stream(&aData->stream);
  xd3_free_stream(&aData->stream);

  aData->finishedProcessing = true;

}

void DiffChunked_done(uv_work_t* req, int ) {
  HandleScope scope;
  DiffChunked_data* aData = (DiffChunked_data*) req->data;
  
  if (aData->errType != 0 || (aData->finishedProcessing && aData->diffBuffSize == 0)) {
    TryCatch try_catch;

    if (aData->errType != 0) {
      Local<Value> argv[1];
      if (aData->errType == -1)
        argv[0] = String::New(uv_strerror(aData->uvErr));
      else
        argv[0] = String::New(aData->xdeltaErr.c_str());
      aData->callbackEnd->Call(Context::GetCurrent()->Global(), 1, argv);
    } else
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





