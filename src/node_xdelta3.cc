extern "C" {
  #include "../include/xdelta3/xdelta3.h"
}

#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <string>

using namespace v8;
using namespace node;


class XdeltaOp : public ObjectWrap {
protected:
  enum OpType { eOpDiff, eOpPatch };
  XdeltaOp(int s, int d, OpType op)
  : ObjectWrap(), mOpType(op), mSrc(s), mDst(d), mBusy(false), mErrType(eErrNone)
  {
    memset (&mStream, 0, sizeof (mStream));
    memset (&mSource, 0, sizeof (mSource));
    mConfig.winsize = XD3_ALLOCSIZE;
    mSource.blksize = XD3_ALLOCSIZE;
    mSource.curblk = (const uint8_t*) new char[mSource.blksize];
    mInputBuf = (void*) new char[mSource.blksize];
    xd3_init_config(&mConfig, XD3_ADLER32);
    xd3_config_stream(&mStream, &mConfig);
    mState = eStart;
    mWroteFromStream = 0;
    mInputBufRead = 0;
    mConsumedInput = false;
    mFileOffset = 0; 
    mBuffMaxSize = 0;
  }
  virtual ~XdeltaOp() {
    delete[] (char*)mSource.curblk;
    delete[] (char*)mInputBuf;
    if (mBuff) delete[] mBuff;
    xd3_close_stream(&mStream);
    xd3_free_stream(&mStream);
  }
  void StartAsync(Handle<Function> fn) {
    mCallback = Persistent<Function>::New(fn);
    mBusy = true;
    this->Ref();
    uv_work_t* aReq = new uv_work_t;
    aReq->data = this;
    uv_queue_work(uv_default_loop(), aReq, OpChunked_pool, OpChunked_done);
  }
  int Read(int fd, void* buf, size_t size, size_t offset) {
    uv_fs_t aUvReq;
    int aBytesRead = uv_fs_read(uv_default_loop(), &aUvReq, fd, buf, size, offset, NULL);
    if (aBytesRead < 0) {
      mErrType = eErrUv;
      mUvErr = uv_last_error(uv_default_loop());
    }
    return aBytesRead;
  }
  int Write(int fd, void* buf, size_t size, size_t offset) { 
    uv_fs_t aUvReq;
    int aBytesWrote = uv_fs_write(uv_default_loop(), &aUvReq, fd, buf, size, offset, NULL);
    if (aBytesWrote != (int) size) {
      mErrType = eErrUv;
      mUvErr = uv_last_error(uv_default_loop());
    }
    return aBytesWrote - size;
  }

  static void OpChunked_pool(uv_work_t* req);
  static void OpChunked_done(uv_work_t* req, int );

  OpType mOpType;
  int mSrc, mDst;
  Persistent<Function> mCallback;

  bool mBusy;
  enum { eStart, eRun, eDone } mState;

  int mBuffMaxSize;
  char* mBuff;
  int mBuffLen;
  unsigned int mWroteFromStream;

  void* mInputBuf;
  int mInputBufRead;
  bool mConsumedInput;
  int mFileOffset;
  xd3_stream mStream;
  xd3_config mConfig;
  xd3_source mSource;

  enum { eErrNone, eErrUv, eErrXd } mErrType;
  uv_err_t mUvErr;
  std::string mXdErr;
};

class XdeltaDiff : public XdeltaOp {
public:
  static Persistent<FunctionTemplate> constructor_template;
  static void Init(Handle<Object> target);

protected:
  XdeltaDiff(int s, int d) : XdeltaOp(s, d, eOpDiff) { };

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> DiffChunked(const Arguments& args);
};

class XdeltaPatch : public XdeltaOp {
public:
  static Persistent<FunctionTemplate> constructor_template;
  static void Init(Handle<Object> target);

protected:
  XdeltaPatch(int s, int d) : XdeltaOp(s, d, eOpPatch) { };

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> PatchChunked(const Arguments& args);
};

void init(Handle<Object> exports) {
  XdeltaDiff::Init(exports);
  XdeltaPatch::Init(exports);
}

NODE_MODULE(node_xdelta3, init);

Persistent<FunctionTemplate> XdeltaDiff::constructor_template;

void XdeltaDiff::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("XdeltaDiff"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "diffChunked", DiffChunked);

  target->Set(String::NewSymbol("XdeltaDiff"), constructor_template->GetFunction());
}

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

  int aSize = args[0]->Uint32Value();
  if (aSize > aXd->mBuffMaxSize) { //fix looks like we need mDiffBuffSize so we don't return more than requested
    if (aXd->mBuffMaxSize != 0)
      delete[] aXd->mBuff;
    aXd->mBuffMaxSize = aSize;
    aXd->mBuff = new char[aXd->mBuffMaxSize];
  }
  aXd->mBuffLen = 0;

  if ((int) aXd->mStream.avail_out - (int) aXd->mWroteFromStream > aXd->mBuffMaxSize) { //if there is a full chunk left in the out stream to emit
    memcpy(aXd->mBuff, aXd->mStream.next_out + aXd->mWroteFromStream, aXd->mBuffMaxSize);
    aXd->mBuffLen += aXd->mBuffMaxSize;
    aXd->mWroteFromStream += aXd->mBuffMaxSize;

    //fix call _done directly here to avoid duplication?
    Handle<Value> aArgv[3];
    aArgv[0] = Undefined();
    aArgv[1] = Buffer::New(aXd->mBuff, aXd->mBuffLen)->handle_;
    aArgv[2] = v8::True();

    TryCatch try_catch;  
    Local<Function>::Cast(args[1])->Call(Context::GetCurrent()->Global(), 3, aArgv);
    if (try_catch.HasCaught())
      FatalException(try_catch);
  } else
    aXd->StartAsync(Local<Function>::Cast(args[1]));

  return args.This();
}

Persistent<FunctionTemplate> XdeltaPatch::constructor_template;

void XdeltaPatch::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("XdeltaPatch"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "patchChunked", PatchChunked);

  target->Set(String::NewSymbol("XdeltaPatch"), constructor_template->GetFunction());
}

Handle<Value> XdeltaPatch::New(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsInt32())
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd)")));

  XdeltaPatch* aXD = new XdeltaPatch(args[0]->Uint32Value(), args[1]->Uint32Value());

  aXD->Wrap(args.This());

  return args.This();
}

Handle<Value> XdeltaPatch::PatchChunked(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || (args.Length() > 1 && !Buffer::HasInstance(args[0])) || !args[args.Length()-1]->IsFunction())
    return ThrowException(Exception::TypeError(String::New("arguments are ([buffer], function)")));

  XdeltaPatch* aXd = ObjectWrap::Unwrap<XdeltaPatch>(args.This());

  if (aXd->mBusy)
    return ThrowException(Exception::TypeError(String::New("object busy with async op")));
  if (args.Length() == 1) {
    aXd->mBuffMaxSize = 0;
  } else {
    //fix don't alloc/copy. mPatchBuff = Unwrap(), Ref(), mBuff = Data(), Unref() & clear mBuff in _done
    Local<Object> aBuffer = args[0]->ToObject();
    int aSize = Buffer::Length(aBuffer);
    if (aSize > aXd->mBuffMaxSize) {
      if (aXd->mBuffMaxSize != 0)
        delete[] aXd->mBuff;
      aXd->mBuffMaxSize = aSize;
      aXd->mBuff = new char[aXd->mBuffMaxSize];
    }
    aXd->mBuffMaxSize = aSize;
    memcpy(aXd->mBuff, Buffer::Data(aBuffer), aXd->mBuffMaxSize);
  }
  aXd->mBuffLen = aXd->mBuffMaxSize;
  aXd->StartAsync(Local<Function>::Cast(args[args.Length()-1]));

  return args.This();
}

void XdeltaOp::OpChunked_pool(uv_work_t* req) {
  XdeltaDiff* aXd = (XdeltaDiff*) req->data;

  if (aXd->mOpType == eOpDiff && aXd->mWroteFromStream < aXd->mStream.avail_out) { //if there is something left in the out stream to copy to aXd->mBuff
    //fix do this in DiffChunked before StartAsync?
    int aWriteSize = aXd->mStream.avail_out - aXd->mWroteFromStream;
    memcpy(aXd->mBuff, aXd->mStream.next_out + aXd->mWroteFromStream, aWriteSize);
    aXd->mBuffLen += aWriteSize;
    aXd->mWroteFromStream += aWriteSize;
  }

  if (aXd->mOpType == eOpDiff && aXd->mWroteFromStream == aXd->mStream.avail_out)
      xd3_consume_output(&aXd->mStream);

  if (aXd->mState == eDone)
    return;

  int aAct = aXd->mState == eStart ? XD3_GETSRCBLK : aXd->mOpType == eOpPatch ? XD3_INPUT : xd3_encode_input(&aXd->mStream);
  do {
    switch (aAct) {
    case XD3_GETSRCBLK: {
      int aBytesRead = aXd->Read(aXd->mSrc, (void*) aXd->mSource.curblk, aXd->mSource.blksize, aXd->mSource.blksize * aXd->mSource.getblkno);
      if (aBytesRead < 0)
        return;
      aXd->mSource.onblk = aBytesRead;
      aXd->mSource.curblkno = aXd->mSource.getblkno;
      if (aXd->mState == eStart) {
        xd3_set_source(&aXd->mStream, &aXd->mSource);
        aAct = XD3_INPUT;
        continue;
      }
      break; 
    }
    case XD3_INPUT: {
     aXd->mState = eRun;
     if (aXd->mOpType == eOpDiff) {
        aXd->mInputBufRead = aXd->Read(aXd->mDst, aXd->mInputBuf, XD3_ALLOCSIZE, aXd->mFileOffset);
        aXd->mFileOffset += XD3_ALLOCSIZE;
        if (aXd->mInputBufRead < 0)
          return;
      } else {
        if (aXd->mConsumedInput) {
          aXd->mInputBufRead = 0;
          aXd->mConsumedInput = false;
        }
        if (aXd->mInputBufRead != XD3_ALLOCSIZE && aXd->mBuffLen != 0) {
          int aReadSize = (aXd->mBuffLen < (int) XD3_ALLOCSIZE - aXd->mInputBufRead) ? aXd->mBuffLen : XD3_ALLOCSIZE - aXd->mInputBufRead;
          if (aReadSize != 0) {
            memcpy((char*) aXd->mInputBuf + aXd->mInputBufRead, aXd->mBuff + aXd->mBuffMaxSize - aXd->mBuffLen, aReadSize);
            aXd->mBuffLen -= aReadSize;
            aXd->mInputBufRead += aReadSize;
          }
          if (aXd->mInputBufRead != XD3_ALLOCSIZE || aXd->mBuffLen == 0) 
            return;
        }
      }
      if (aXd->mInputBufRead < (int) XD3_ALLOCSIZE)
        xd3_set_flags(&aXd->mStream, XD3_FLUSH | aXd->mStream.flags);
      xd3_avail_input(&aXd->mStream, (const uint8_t*) aXd->mInputBuf, aXd->mInputBufRead);
      aXd->mConsumedInput = true;
      break;
    }
    case XD3_OUTPUT: {
      if (aXd->mOpType == eOpDiff) {
        int aWriteSize = ((int)aXd->mStream.avail_out > aXd->mBuffMaxSize - aXd->mBuffLen) ? aXd->mBuffMaxSize - aXd->mBuffLen : aXd->mStream.avail_out;
        memcpy(aXd->mBuff + aXd->mBuffLen, aXd->mStream.next_out, aWriteSize);
        aXd->mBuffLen += aWriteSize;
        aXd->mWroteFromStream = aWriteSize;
        if (aXd->mWroteFromStream < aXd->mStream.avail_out) //diff buffer is full
          return;
      } else {
        if (aXd->Write(aXd->mDst, aXd->mStream.next_out, (int)aXd->mStream.avail_out, aXd->mFileOffset) < 0)
          return;
        aXd->mFileOffset += (int)aXd->mStream.avail_out;
      }
      xd3_consume_output(&aXd->mStream);
      break;
    }
    case XD3_GOTHEADER:
    case XD3_WINSTART:
    case XD3_WINFINISH:
      break;
    default:
      aXd->mErrType = eErrXd;
      aXd->mXdErr = aXd->mStream.msg;
      return;
    }
    aAct = aXd->mOpType == eOpDiff ? xd3_encode_input(&aXd->mStream) : xd3_decode_input(&aXd->mStream);
  } while (aXd->mInputBufRead == XD3_ALLOCSIZE || aAct != XD3_INPUT || aXd->mState == eStart);

  aXd->mState = eDone;
}

void XdeltaOp::OpChunked_done(uv_work_t* req, int ) {
  HandleScope scope;
  XdeltaDiff* aXd = (XdeltaDiff*) req->data;

  Handle<Value> aArgv[3];
  int aArgc;

  if (aXd->mErrType != eErrNone) {
    aArgv[0] = String::New(aXd->mErrType == eErrUv ? uv_strerror(aXd->mUvErr) : aXd->mXdErr.c_str());
    aArgc = 1;
  } else if (aXd->mState == eDone && aXd->mBuffLen == 0) {
    aArgc = 0;
  } else {
    aArgc = 3;
    aArgv[0] = Undefined();
    aArgv[1] = Buffer::New(aXd->mBuff, aXd->mBuffLen)->handle_;
    aArgv[2] = v8::False();
  }
  aXd->Unref();
  aXd->mBusy = false;  
  Local<Function> aCallback(Local<Function>::New(aXd->mCallback));
  aXd->mCallback.Dispose(); 

  TryCatch try_catch;
  aCallback->Call(Context::GetCurrent()->Global(), aArgc, aArgv);
  if (try_catch.HasCaught())
    FatalException(try_catch);

  delete req;
}

