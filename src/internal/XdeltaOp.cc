#include "XdeltaOp.h"
#include <node_buffer.h>

using namespace v8;
using namespace node;

XdeltaOp::XdeltaOp(int s, int d, Local<Object> cfg, OpType op) : 
    ObjectWrap(), 
    mOpType(op), 
    mSrc(s), 
    mDst(d), 
    mBusy(false), 
    mErrType(eErrNone)
{
    memset (&mStream, 0, sizeof mStream);
    memset (&mSource, 0, sizeof mSource);
    memset (&mConfig, 0, sizeof mConfig);

    SetCfg(cfg);

    mSource.curblk = reinterpret_cast<const uint8_t*>( new char[mSource.blksize] );
    mInputBuf = new char[mSource.blksize];

    xd3_config_stream(&mStream, &mConfig);
    mState = eStart;
    mWroteFromStream = 0;
    mInputBufRead = 0;
    mConsumedInput = false;
    mFileOffset = 0; 
    mBuffMaxSize = 0;
}

XdeltaOp::~XdeltaOp() 
{
    delete[] mSource.curblk;
    delete[] mInputBuf;
    xd3_close_stream(&mStream);
    xd3_free_stream(&mStream);
}

void XdeltaOp::SetCfg(Local<Object> cfg) 
{
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

int XdeltaOp::GetInt32CfgValue(Local<Object> cfg, const char* str, int def) 
{
    Local<String> aKey;
    Local<Value> aVal;
    Isolate* isolate = Isolate::GetCurrent();

    if ( cfg->Has( aKey = String::NewFromUtf8(isolate, str) ) )
    {
      aVal = cfg->Get(aKey);
      if (aVal->IsInt32())
      {
        return aVal->Int32Value();
      }
    }
    return def;
}

void XdeltaOp::StartAsync(Handle<Function> fn) 
{
    Isolate* isolate = Isolate::GetCurrent();
    mCallback.Reset(isolate, fn);
    mBusy = true;
    this->Ref();
    uv_work_t* aReq = new uv_work_t;
    aReq->data = this;
    uv_queue_work(uv_default_loop(), aReq, Work_pool, Work_done);
}

void XdeltaOp::FinishAsync() 
{
    this->Unref();
    mBusy = false;
    mCallback.Reset();
}

bool XdeltaOp::loadSourceFile()
{
    int aBytesRead = mReader.read(mSrc, 
                                  reinterpret_cast<const char*>( mSource.curblk ),
                                  mSource.blksize, 
                                  ( mSource.blksize * mSource.getblkno ));

    if (mReader.isError())
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

void XdeltaOp::Pool() 
{
  int aAct = ( mState == eStart ) ? XD3_GETSRCBLK : 
                                    ( mOpType == eOpPatch ) ? XD3_INPUT : xd3_encode_input(&mStream);
  do 
  {
    switch (aAct) 
    {
    case XD3_GETSRCBLK: 
    {
      if ( !loadSourceFile() ) return;

      if (mState == eStart) 
      {
        aAct = XD3_INPUT;
        continue;
      }
      break; 
    }
    case XD3_INPUT: 
    {
     mState = eRun;
     if ( !loadSecondaryFile() ) return;
     break;
    }
    case XD3_OUTPUT: 
    {
      if ( !generateResult() )
      {
        return;
      }
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

void XdeltaOp::Done() 
{
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  Local<Function> aCallback(Local<Function>::New(isolate,mCallback));
  FinishAsync();
  Callback(aCallback);
}

void XdeltaOp::Callback(Handle<Function> callback) 
{
  Isolate* isolate = callback->GetIsolate();
  Handle<Value> aArgv[2];
  int aArgc = 0;

  if (mErrType != eErrNone) 
  {
    aArgv[aArgc++] = String::NewFromUtf8(isolate, ( mErrType == eErrUv ? uv_strerror(mUvErr) : mXdErr.c_str() ) );
  } 
  else if ( (mState == eDone) && (mBuffLen == 0) ) 
  {
    aArgv[aArgc++] = Undefined(isolate);
    aArgv[aArgc++] = Null(isolate);
  } 
  else 
  {
    aArgv[aArgc++] = Undefined(isolate);
    MaybeLocal<Object> b = Buffer::Copy(isolate, mBuff, mBuffLen);
    aArgv[aArgc++] = b.ToLocalChecked();
  }

  TryCatch try_catch(isolate);

  callback->Call(isolate->GetCurrentContext()->Global(), aArgc, aArgv);

  if (try_catch.HasCaught())
  {
    FatalException(isolate, try_catch);
  }
}

void XdeltaOp::Work_pool(uv_work_t* req) 
{ 
    if ( req == NULL ) return;

    XdeltaOp* op = static_cast<XdeltaOp*>( req->data );
    if ( op != NULL )
    {
        op->Pool();
    } 
};

void XdeltaOp::Work_done(uv_work_t* req, int ) 
{ 
    if ( req == NULL ) return;

    XdeltaOp* op = static_cast<XdeltaOp*>( req->data );
    if ( op != NULL )
    {
        op->Done();
    }  

    delete req; 
};
