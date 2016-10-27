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

XdeltaOp::~XdeltaOp() 
{
    delete[] (char*)mSource.curblk;
    delete[] (char*)mInputBuf;
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
    if (cfg->Has(aKey = String::New(str))) 
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
    mCallback = Persistent<Function>::New(fn);
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
    mCallback.Dispose();
}

bool XdeltaOp::loadSourceFile()
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

bool XdeltaOp::loadSecondaryFile()
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

        if ( (mInputBufRead != mWinSize) && (mBuffLen != 0) )
        {
          int bytesRead = mWinSize - mInputBufRead;
          if ( bytesRead < 0 ) bytesRead = 0;

          int aReadSize = (mBuffLen < static_cast<unsigned int>( bytesRead ) ) ? mBuffLen : bytesRead;
          if (aReadSize != 0)
          {
            memcpy((char*) mInputBuf + mInputBufRead, ( mBuff + mBuffMaxSize - mBuffLen ), aReadSize);
            mBuffLen -= aReadSize;
            mInputBufRead += aReadSize;
          }

          if ( (mInputBufRead != mWinSize) || (mBuffLen == 0) ) return false;
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

void XdeltaOp::Pool() 
{
  if (mOpType == eOpDiff && mWroteFromStream == mStream.avail_out)
  {
    xd3_consume_output(&mStream);
  }

  int aAct = mState == eStart ? XD3_GETSRCBLK : mOpType == eOpPatch ? XD3_INPUT : xd3_encode_input(&mStream);
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
      if (mOpType == eOpDiff) 
      {
        unsigned int availableSpace = mBuffMaxSize - mBuffLen;
        int aWriteSize = (mStream.avail_out > availableSpace ) ? availableSpace : mStream.avail_out;
        memcpy(mBuff + mBuffLen, mStream.next_out, aWriteSize);
        mBuffLen += aWriteSize;
        mWroteFromStream = aWriteSize;
        if (mWroteFromStream < mStream.avail_out) //diff buffer is full
        {
          return;
        }
      } 
      else 
      {
        if ( !mWriter.write(mDst, mStream.next_out, (int)mStream.avail_out, mFileOffset) )
        {
          mErrType = eErrUv;
          mUvErr = mWriter.writeError();
          return;
        }

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

void XdeltaOp::Done() 
{
  HandleScope scope;

  Local<Function> aCallback(Local<Function>::New(mCallback));

  FinishAsync();

  Callback(aCallback);
}

void XdeltaOp::Callback(Handle<Function> callback) 
{
  Handle<Value> aArgv[2];
  int aArgc = 0;

  if (mErrType != eErrNone) 
  {
    aArgv[aArgc++] = String::New(mErrType == eErrUv ? uv_strerror(mUvErr) : mXdErr.c_str());
  } 
  else if ( (mState == eDone) && (mBuffLen == 0) ) 
  {
    aArgv[aArgc++] = Undefined();
    aArgv[aArgc++] = Null();
  } 
  else 
  {
    aArgv[aArgc++] = Undefined();
    aArgv[aArgc++] = Buffer::New(mBuff, mBuffLen)->handle_;
  }

  TryCatch try_catch;
  callback->Call(Context::GetCurrent()->Global(), aArgc, aArgv);
  if (try_catch.HasCaught())
  {
    FatalException(try_catch);
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
