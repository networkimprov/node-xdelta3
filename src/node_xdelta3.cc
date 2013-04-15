extern "C" {
  #include "../include/xdelta3/xdelta3.h"
}

#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <string>

using namespace v8;
using namespace node;


class XdeltaDiff : public ObjectWrap {

public:
  static Persistent<FunctionTemplate> constructor_template;
  static void Init(Handle<Object> target);

protected:

  XdeltaDiff(int s, int d)
    : ObjectWrap(), mBusy(false), mSrc(s), mDst(d), mCallbackSet(false), mFirstTime(true), mFinishedProcessing(false),
    mDiffBuffMemSize(0), mDiffBuffReadMaxSize(0), mDiffBuffSize(0), mWroteFromStream(0), mReadDstN(0), mErrType(eErrNone)
  {
    memset (&mStream, 0, sizeof (mStream));
    memset (&mSource, 0, sizeof (mSource));
    mConfig.winsize = XD3_ALLOCSIZE;
    mSource.blksize = XD3_ALLOCSIZE;
    mSource.curblk = (const uint8_t*) new char[mSource.blksize];
    mInputBuf = (void*) new char[mSource.blksize];
  };
  ~XdeltaDiff() {
    delete[] (char*)mSource.curblk;
    delete[] (char*)mInputBuf;
    if (mDiffBuff) delete[] mDiffBuff;
  }

  static Handle<Value> New(const Arguments& args);

  bool mBusy;

  int mSrc, mDst;
  Persistent<Function> mCallback;
  bool mCallbackSet;

  bool mFirstTime;
  bool mFinishedProcessing;
  int mDiffBuffMemSize;
  int mDiffBuffReadMaxSize;
  char* mDiffBuff;
  int mDiffBuffSize;
  unsigned mWroteFromStream;
  int mReadDstN;

  enum { eErrNone, eErrUv, eErrXd } mErrType; // 0 - none, -1 - libuv, -2 - xdelta3
  uv_err_t mUvErr;
  std::string mXdeltaErr;

  struct stat mStatbuf;
  void* mInputBuf;
  xd3_stream mStream;
  xd3_config mConfig;
  xd3_source mSource;

  void StartAsync() {
    mBusy = true;
    Ref();
  }
  void FinishAsync() {
    mBusy = false;
    Unref();
  }

  static Handle<Value> DiffChunked(const Arguments& args);
  static void DiffChunked_pool(uv_work_t* req);
  static void DiffChunked_done(uv_work_t* req, int );
};

Persistent<FunctionTemplate> XdeltaDiff::constructor_template;

void XdeltaDiff::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("XdeldaDiff"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "diff_chunked", DiffChunked);

  target->Set(String::NewSymbol("XdeldaDiff"), constructor_template->GetFunction());
}

void init(Handle<Object> exports) {
  XdeltaDiff::Init(exports);
}

NODE_MODULE(node_xdelta3, init);


Handle<Value> XdeltaDiff::New(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsInt32())
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd)")));

  XdeltaDiff* aXD = new XdeltaDiff(args[0]->Uint32Value(), args[1]->Uint32Value());

  aXD->Wrap(args.This());

  return args.This();
}

Handle<Value> XdeltaDiff::DiffChunked(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsFunction())
    return ThrowException(Exception::TypeError(String::New("arguments are (number, function)")));

  XdeltaDiff* aXd = ObjectWrap::Unwrap<XdeltaDiff>(args.This());

  if (aXd->mBusy)
    return ThrowException(Exception::TypeError(String::New("object busy with async op")));


  aXd->mDiffBuffReadMaxSize = args[0]->Uint32Value();
  if (aXd->mDiffBuffReadMaxSize > aXd->mDiffBuffMemSize) {
    if (aXd->mDiffBuffMemSize != 0) delete[] aXd->mDiffBuff;
    aXd->mDiffBuffMemSize = aXd->mDiffBuffReadMaxSize;
    aXd->mDiffBuff = new char[aXd->mDiffBuffReadMaxSize];
  }
   
  if (aXd->mCallbackSet)
    aXd->mCallback.Dispose();
  aXd->mCallback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  aXd->mCallbackSet = true;

  aXd->StartAsync();

  uv_work_t* aReq = new uv_work_t();
  aReq->data = aXd;

  uv_queue_work(uv_default_loop(), aReq, DiffChunked_pool, DiffChunked_done);

  return args.This();
}

void XdeltaDiff::DiffChunked_pool(uv_work_t* req) {
  XdeltaDiff* aXd = (XdeltaDiff*) req->data;
  aXd->mDiffBuffSize = 0;

  if (aXd->mFirstTime) {
    xd3_init_config(&aXd->mConfig, XD3_ADLER32);
    xd3_config_stream(&aXd->mStream, &aXd->mConfig);

    uv_fs_t aUvReq;
    int aBytesRead;
    aBytesRead = uv_fs_read(uv_default_loop(), &aUvReq, aXd->mSrc, (void*)aXd->mSource.curblk, aXd->mSource.blksize, 0, NULL);
    if (aBytesRead < 0) {
      aXd->mErrType = eErrUv;
      aXd->mUvErr = uv_last_error(uv_default_loop());
      xd3_close_stream(&aXd->mStream);
      xd3_free_stream(&aXd->mStream);
      return;
    }
    aXd->mSource.onblk = aBytesRead;
    aXd->mSource.curblkno = 0;

    xd3_set_source(&aXd->mStream, &aXd->mSource);
  }

  if (aXd->mWroteFromStream < aXd->mStream.avail_out) { //if there is something left in the out stream to emit
    int aWriteSize = (aXd->mStream.avail_out - aXd->mWroteFromStream > aXd->mDiffBuffReadMaxSize) ? aXd->mDiffBuffReadMaxSize : aXd->mStream.avail_out - aXd->mWroteFromStream;
    memcpy(aXd->mDiffBuff, aXd->mStream.next_out + aXd->mWroteFromStream, aWriteSize);
    aXd->mDiffBuffSize += aWriteSize;
    aXd->mWroteFromStream += aWriteSize;
    if (aXd->mWroteFromStream < aXd->mStream.avail_out)
      return;
    xd3_consume_output(&aXd->mStream);
  }

  if (aXd->mFinishedProcessing)
    return;

  int aInputBufRead = 0;
  bool aRead = aXd->mFirstTime; //if it is not firstTime, XD3_OUTPUT was called last, so read is not necessary
  aXd->mFirstTime = false;
  do {
    if (aRead) {
      aRead = false;
      uv_fs_t aUvReq;
      aInputBufRead = uv_fs_read(uv_default_loop(), &aUvReq, aXd->mDst, aXd->mInputBuf, XD3_ALLOCSIZE, aXd->mReadDstN * XD3_ALLOCSIZE, NULL);
      aXd->mReadDstN++;
      if (aInputBufRead < 0) {
        aXd->mErrType = eErrUv;
        aXd->mUvErr = uv_last_error(uv_default_loop());
        return;
      }
      if (aInputBufRead < (int) XD3_ALLOCSIZE)
        xd3_set_flags(&aXd->mStream, XD3_FLUSH | aXd->mStream.flags);
      xd3_avail_input(&aXd->mStream, (const uint8_t*) aXd->mInputBuf, aInputBufRead);
    }

    process:

    int aRet = xd3_encode_input(&aXd->mStream);
    switch (aRet) {
    case XD3_INPUT:
      aRead = true;
      continue;

    case XD3_OUTPUT: {
      int aWriteSize = ((int)aXd->mStream.avail_out > aXd->mDiffBuffReadMaxSize - aXd->mDiffBuffSize) ? aXd->mDiffBuffReadMaxSize - aXd->mDiffBuffSize : aXd->mStream.avail_out;
      memcpy(aXd->mDiffBuff + aXd->mDiffBuffSize, aXd->mStream.next_out, aWriteSize);
      aXd->mDiffBuffSize += aWriteSize;
      aXd->mWroteFromStream = aWriteSize;

      if (aXd->mWroteFromStream < aXd->mStream.avail_out) //diff buffer is full
        return;
      xd3_consume_output(&aXd->mStream);

      goto process;
    }
    case XD3_GETSRCBLK: {
      uv_fs_t aUvReq;
      int aBytesRead;
      aBytesRead = uv_fs_read(uv_default_loop(), &aUvReq, aXd->mSrc, (void*) aXd->mSource.curblk, aXd->mSource.blksize, aXd->mSource.blksize * aXd->mSource.getblkno, NULL);
      if (aBytesRead < 0) {
        aXd->mErrType = eErrUv;
        aXd->mUvErr = uv_last_error(uv_default_loop());
        xd3_close_stream(&aXd->mStream);
        xd3_free_stream(&aXd->mStream);
        return;
      }
      aXd->mSource.onblk = aBytesRead;
      aXd->mSource.curblkno = aXd->mSource.getblkno;
      goto process;  
    }
    case XD3_GOTHEADER:
    case XD3_WINSTART:
    case XD3_WINFINISH:
      goto process;
    default:
      aXd->mErrType = eErrXd;
      aXd->mXdeltaErr = aXd->mStream.msg;
      xd3_close_stream(&aXd->mStream);
      xd3_free_stream(&aXd->mStream);
      return;
    }

  } while (aInputBufRead == XD3_ALLOCSIZE);

  xd3_close_stream(&aXd->mStream);
  xd3_free_stream(&aXd->mStream);

  aXd->mFinishedProcessing = true;

}

void XdeltaDiff::DiffChunked_done(uv_work_t* req, int ) {
  HandleScope scope;
  XdeltaDiff* aXd = (XdeltaDiff*) req->data;

  aXd->FinishAsync();

  TryCatch try_catch;

  if (aXd->mErrType != eErrNone || (aXd->mFinishedProcessing && aXd->mDiffBuffSize == 0)) {

    if (aXd->mErrType != eErrNone) {
      Local<Value> argv[1];
      if (aXd->mErrType == eErrUv)
        argv[0] = String::New(uv_strerror(aXd->mUvErr));
      else
        argv[0] = String::New(aXd->mXdeltaErr.c_str());
      aXd->mCallback->Call(Context::GetCurrent()->Global(), 1, argv);
    } else
      aXd->mCallback->Call(Context::GetCurrent()->Global(), 0, NULL);
  } else {
    //emit the data
    node::Buffer *aSlowBuffer = node::Buffer::New(aXd->mDiffBuff, aXd->mDiffBuffSize);
    Local<Function> aBufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(String::New("Buffer")));
    Handle<Value> aConstructorArgs[3] = { aSlowBuffer->handle_, Integer::New(aXd->mDiffBuffSize), Integer::New(0) };
    Local<Object> aActualBuffer = aBufferConstructor->NewInstance(3, aConstructorArgs);
    Handle<Value> aArgv[2];
    aArgv[0] = Undefined();
    aArgv[1] = aActualBuffer;

    aXd->mCallback->Call(Context::GetCurrent()->Global(), 2, aArgv);
  }
  if (try_catch.HasCaught())
    node::FatalException(try_catch);
  delete req;
}

