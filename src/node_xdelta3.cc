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
  XdeltaOp(int s, int d, int ws, OpType op)
  : ObjectWrap(), mOpType(op), mSrc(s), mDst(d), mWinSize(ws), mBusy(false), mErrType(eErrNone)
  {
    memset (&mStream, 0, sizeof mStream);
    memset (&mSource, 0, sizeof mSource);
    
    mConfig.winsize = mWinSize;
    mSource.blksize = mWinSize;
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
    xd3_close_stream(&mStream);
    xd3_free_stream(&mStream);
  }
  void StartAsync(Handle<Function> fn) {
    mCallback = Persistent<Function>::New(fn);
    mBusy = true;
    this->Ref();
    uv_work_t* aReq = new uv_work_t;
    aReq->data = this;
    uv_queue_work(uv_default_loop(), aReq, Work_pool, Work_done);
  }
  virtual void FinishAsync() {
    this->Unref();
    mBusy = false;
    mCallback.Dispose();
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

  static void Work_pool(uv_work_t* req) { ((XdeltaOp*) req->data)->Pool(); };
  static void Work_done(uv_work_t* req, int ) { ((XdeltaOp*) req->data)->Done();  delete req; };

  void Pool();
  void Done();
  void Callback(Handle<Function> callback);

  OpType mOpType;
  int mSrc, mDst;
  Persistent<Function> mCallback;

  int mWinSize;

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
  static void Init(Handle<Object> target);
  static Persistent<FunctionTemplate> constructor_template;

protected:
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> DiffChunked(const Arguments& args);

  XdeltaDiff(int s, int d, int ws) : XdeltaOp(s, d, ws, eOpDiff), mBuffMemSize(0) {}
  ~XdeltaDiff() {
    if (mBuffMemSize > 0)
      delete[] mBuff;
  }

  int mBuffMemSize;
};

class XdeltaPatch : public XdeltaOp {
public:
  static void Init(Handle<Object> target);
  static Persistent<FunctionTemplate> constructor_template;

protected:
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> PatchChunked(const Arguments& args);

  XdeltaPatch(int s, int d, int ws) : XdeltaOp(s, d, ws, eOpPatch) { };

  void FinishAsync() {
    XdeltaOp::FinishAsync();
    mBufferObj.Dispose();
  }

  Persistent<Object> mBufferObj;
};

void init(Handle<Object> exports) {
  XdeltaDiff::Init(exports);
  XdeltaPatch::Init(exports);

  exports->Set(String::NewSymbol("DEFAULT_WINSIZE"), Integer::New(XD3_DEFAULT_WINSIZE), ReadOnly);
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

  if (args.Length() < 3 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsInt32())
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd, uint)")));

  XdeltaDiff* aXD = new XdeltaDiff(args[0]->Uint32Value(), args[1]->Uint32Value(), args[2]->Uint32Value());

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

  aXd->mBuffMaxSize = args[0]->Uint32Value();
  if (aXd->mBuffMaxSize > aXd->mBuffMemSize) {
    if (aXd->mBuffMemSize > 0)
      delete[] aXd->mBuff;
    aXd->mBuff = new char[aXd->mBuffMemSize = aXd->mBuffMaxSize];
  }
  aXd->mBuffLen = 0;

  if (aXd->mWroteFromStream < aXd->mStream.avail_out) { //if there is something left in the out stream to copy to aXd->mBuff
    int aWriteSize = (aXd->mStream.avail_out - aXd->mWroteFromStream > aXd->mBuffMaxSize) ? aXd->mBuffMaxSize : aXd->mStream.avail_out - aXd->mWroteFromStream;
    memcpy(aXd->mBuff, aXd->mStream.next_out + aXd->mWroteFromStream, aWriteSize);
    aXd->mBuffLen += aWriteSize;
    aXd->mWroteFromStream += aWriteSize;
  }

  if (aXd->mErrType != eErrNone || aXd->mState == eDone)
    aXd->Callback(Local<Function>::Cast(args[1]));
  else if (aXd->mWroteFromStream < aXd->mStream.avail_out)
    aXd->Callback(Local<Function>::Cast(args[1]));
  else
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

  if (args.Length() < 3 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsInt32())
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd, uint)")));

  XdeltaPatch* aXD = new XdeltaPatch(args[0]->Uint32Value(), args[1]->Uint32Value(), args[2]->Uint32Value());

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

  if (aXd->mErrType != eErrNone || aXd->mState == eDone) {
    aXd->Callback(Local<Function>::Cast(args[args.Length()-1]));
    return args.This();
  }
    
  if (args.Length() == 1) {
    aXd->mBuffMaxSize = 0;
  } else {
    aXd->mBufferObj = Persistent<Object>::New(args[0]->ToObject());
    aXd->mBuffMaxSize = Buffer::Length(aXd->mBufferObj);
    aXd->mBuff = Buffer::Data(aXd->mBufferObj);
  }
  aXd->mBuffLen = aXd->mBuffMaxSize;

  aXd->StartAsync(Local<Function>::Cast(args[args.Length()-1]));

  return args.This();
}

void XdeltaOp::Pool() {
  if (mOpType == eOpDiff && mWroteFromStream == mStream.avail_out)
    xd3_consume_output(&mStream);

  int aAct = mState == eStart ? XD3_GETSRCBLK : mOpType == eOpPatch ? XD3_INPUT : xd3_encode_input(&mStream);
  do {
    switch (aAct) {
    case XD3_GETSRCBLK: {
      int aBytesRead = Read(mSrc, (void*) mSource.curblk, mSource.blksize, mSource.blksize * mSource.getblkno);
      if (aBytesRead < 0)
        return;
      mSource.onblk = aBytesRead;
      mSource.curblkno = mSource.getblkno;
      if (mState == eStart) {
        xd3_set_source(&mStream, &mSource);
        aAct = XD3_INPUT;
        continue;
      }
      break; 
    }
    case XD3_INPUT: {
     mState = eRun;
     if (mOpType == eOpDiff) {
        mInputBufRead = Read(mDst, mInputBuf, mWinSize, mFileOffset);
        mFileOffset += mWinSize;
        if (mInputBufRead < 0)
          return;
      } else {
        if (mConsumedInput) {
          mInputBufRead = 0;
          mConsumedInput = false;
        }
        if (mInputBufRead != mWinSize && mBuffLen != 0) {
          int aReadSize = (mBuffLen < (int) mWinSize - mInputBufRead) ? mBuffLen : mWinSize - mInputBufRead;
          if (aReadSize != 0) {
            memcpy((char*) mInputBuf + mInputBufRead, mBuff + mBuffMaxSize - mBuffLen, aReadSize);
            mBuffLen -= aReadSize;
            mInputBufRead += aReadSize;
          }
          if (mInputBufRead != mWinSize || mBuffLen == 0) 
            return;
        }
      }
      if (mInputBufRead < (int) mWinSize)
        xd3_set_flags(&mStream, XD3_FLUSH | mStream.flags);
      xd3_avail_input(&mStream, (const uint8_t*) mInputBuf, mInputBufRead);
      mConsumedInput = true;
      break;
    }
    case XD3_OUTPUT: {
      if (mOpType == eOpDiff) {
        int aWriteSize = ((int)mStream.avail_out > mBuffMaxSize - mBuffLen) ? mBuffMaxSize - mBuffLen : mStream.avail_out;
        memcpy(mBuff + mBuffLen, mStream.next_out, aWriteSize);
        mBuffLen += aWriteSize;
        mWroteFromStream = aWriteSize;
        if (mWroteFromStream < mStream.avail_out) //diff buffer is full
          return;
      } else {
        if (Write(mDst, mStream.next_out, (int)mStream.avail_out, mFileOffset) < 0)
          return;
        mFileOffset += (int)mStream.avail_out;
      }
      xd3_consume_output(&mStream);
      break;
    }
    case XD3_GOTHEADER:
    case XD3_WINSTART:
    case XD3_WINFINISH:
      break;
    default:
      mErrType = eErrXd;
      mXdErr = mStream.msg;
      return;
    }
    aAct = mOpType == eOpDiff ? xd3_encode_input(&mStream) : xd3_decode_input(&mStream);
  } while (mInputBufRead == mWinSize || aAct != XD3_INPUT || mState == eStart);

  mState = eDone;
}

void XdeltaOp::Done() {
  HandleScope scope;

  Local<Function> aCallback(Local<Function>::New(mCallback));

  FinishAsync();

  Callback(aCallback);
}

void XdeltaOp::Callback(Handle<Function> callback) {
  Handle<Value> aArgv[2];
  int aArgc = 0;

  if (mErrType != eErrNone) {
    aArgv[aArgc++] = String::New(mErrType == eErrUv ? uv_strerror(mUvErr) : mXdErr.c_str());
  } else if (mState == eDone && mBuffLen == 0) {
    aArgv[aArgc++] = Undefined();
    aArgv[aArgc++] = Null();
  } else {
    aArgv[aArgc++] = Undefined();
    aArgv[aArgc++] = Buffer::New(mBuff, mBuffLen)->handle_;
  }

  TryCatch try_catch;
  callback->Call(Context::GetCurrent()->Global(), aArgc, aArgv);
  if (try_catch.HasCaught())
    FatalException(try_catch);
}

