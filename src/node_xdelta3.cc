extern "C" {
  #include "../include/xdelta3/xdelta3.h"
}

#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include "internal/FileReader.h"
#include <string>

using namespace v8;
using namespace node;


class XdeltaOp : public ObjectWrap {
protected:
  enum OpType { eOpDiff, eOpPatch };
  XdeltaOp(int s, int d, Local<Object> cfg, OpType op)
  : ObjectWrap(), mOpType(op), mSrc(s), mDst(d), mBusy(false), mErrType(eErrNone)
  {
    memset (&mStream, 0, sizeof mStream);
    memset (&mSource, 0, sizeof mSource);
    memset (&mConfig, 0, sizeof mConfig);

    SetCfg(cfg);

    mSource.curblk = (const uint8_t*) new char[mSource.blksize];
    mInputBuf = (void*) new char[mSource.blksize];

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
  void SetCfg(Local<Object> cfg) {
    mConfig.iopt_size = GetInt32CfgValue(cfg, "iopt_size", XD3_DEFAULT_IOPT_SIZE);
    mConfig.sprevsz = GetInt32CfgValue(cfg, "sprevsz", XD3_DEFAULT_SPREVSZ);
    mConfig.smatch_cfg = (xd3_smatch_cfg) GetInt32CfgValue(cfg, "smatch_cfg", XD3_SMATCH_DEFAULT);
    mConfig.winsize = GetInt32CfgValue(cfg,"winsize", XD3_DEFAULT_WINSIZE);
    mConfig.flags = GetInt32CfgValue(cfg, "flags", 0);
    mConfig.sec_data.ngroups = GetInt32CfgValue(cfg, "sec_data_ngroups", 0);
    mConfig.sec_inst.ngroups = GetInt32CfgValue(cfg, "sec_inst_ngroups", 0);
    mConfig.sec_addr.ngroups = GetInt32CfgValue(cfg, "sec_addr_ngroups", 0);
    mWinSize = mConfig.winsize;
    mSource.blksize = mConfig.winsize;
  }
  static int GetInt32CfgValue(Local<Object> cfg, const char* str, int def) {
    Local<String> aKey;
    Local<Value> aVal;
    if (cfg->Has(aKey = String::New(str))) {
      aVal = cfg->Get(aKey);
      if (aVal->IsInt32())
        return aVal->Int32Value();
    }
    return def;
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
  int Write(int fd, void* buf, size_t size, size_t offset) { 
    uv_fs_t aUvReq;
    int aBytesWrote = uv_fs_write(uv_default_loop(), &aUvReq, fd, buf, size, offset, NULL);
    if (aBytesWrote != (int) size) {
      mErrType = eErrUv;
      mUvErr = uv_last_error(uv_default_loop());
    }
    return aBytesWrote - size;
  }
  bool loadSourceFile()
  {
    int aBytesRead = mReader.read(mSrc, 
                                  const_cast<void*>( static_cast<const void*>( mSource.curblk ) ),
                                  mSource.blksize, 
                                  ( mSource.blksize * mSource.getblkno ));

    if (aBytesRead < 0) 
    {
      mErrType = eErrUv;
      mUvErr = mReader.readError();
      return false;
    }

    mSource.onblk = aBytesRead;
    mSource.curblkno = mSource.getblkno;
    if (mState == eStart) 
    {
      xd3_set_source(&mStream, &mSource);
    }

    return true;
  }
  bool loadSecondaryFile()
  {
    if (mOpType == eOpDiff)
    {
        mInputBufRead = mReader.read(mDst, mInputBuf, mWinSize, mFileOffset);
        mFileOffset += mWinSize;

        if (mInputBufRead < 0)
        {
          mErrType = eErrUv;
          mUvErr = mReader.readError();
          return false;
        }
    }
    else
    {
        if (mConsumedInput)
        {
          mInputBufRead = 0;
          mConsumedInput = false;
        }

        if (mInputBufRead != mWinSize && mBuffLen != 0)
        {
          int aReadSize = (mBuffLen < (int) mWinSize - mInputBufRead) ? mBuffLen : mWinSize - mInputBufRead;
          if (aReadSize != 0)
          {
            memcpy((char*) mInputBuf + mInputBufRead, mBuff + mBuffMaxSize - mBuffLen, aReadSize);
            mBuffLen -= aReadSize;
            mInputBufRead += aReadSize;
          }

          if (mInputBufRead != mWinSize || mBuffLen == 0) return false;
        }
      }

      if (mInputBufRead < (int) mWinSize)
      {
        xd3_set_flags(&mStream, XD3_FLUSH | mStream.flags);
      }

      xd3_avail_input(&mStream, (const uint8_t*) mInputBuf, mInputBufRead);
      mConsumedInput = true;

      return true;
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
  FileReader mReader;

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

  XdeltaDiff(int s, int d, Local<Object> cfg) : XdeltaOp(s, d, cfg, eOpDiff), mBuffMemSize(0) {}
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

  XdeltaPatch(int s, int d, Local<Object> cfg) : XdeltaOp(s, d, cfg, eOpPatch) { };

  void FinishAsync() {
    XdeltaOp::FinishAsync();
    mBufferObj.Dispose();
  }

  Persistent<Object> mBufferObj;
};

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

  if (args.Length() < 3 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsObject())
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd, obj)")));

  XdeltaDiff* aXD = new XdeltaDiff(args[0]->Uint32Value(), args[1]->Uint32Value(), args[2]->ToObject());

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

  if (args.Length() < 3 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsObject())
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd, obj)")));

  XdeltaPatch* aXD = new XdeltaPatch(args[0]->Uint32Value(), args[1]->Uint32Value(), args[2]->ToObject());

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
      if ( !loadSourceFile() ) return;
      if (mState == eStart) 
      {
        aAct = XD3_INPUT;
        continue;
      }
      break; 
    }
    case XD3_INPUT: {
     mState = eRun;
     if ( !loadSecondaryFile() ) return;
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

