#include "XdeltaPatch.h"
#include <node_buffer.h>

using namespace v8;
using namespace node;

XdeltaPatch::XdeltaPatch(int s, int d, Local<Object> cfg) : 
    XdeltaOp(s, d, cfg, eOpPatch)
{

}

XdeltaPatch::~XdeltaPatch()
{}

void XdeltaPatch::Init(Handle<Object> exports)
{
  Isolate* isolate = exports->GetIsolate();
  HandleScope scope(isolate);

  Local<FunctionTemplate> t = FunctionTemplate::New(isolate, New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewFromUtf8(isolate,"XdeltaPatch", String::kInternalizedString));
  constructor_template.Reset(isolate,t);

  NODE_SET_PROTOTYPE_METHOD(t, "patchChunked", PatchChunked);

  exports->Set(String::NewFromUtf8(isolate, "XdeltaPatch", String::kInternalizedString), t->GetFunction());
}

void XdeltaPatch::New(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsObject())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"arguments are (fd, fd, obj)")));
    return;
  }

  XdeltaPatch* aXD = new XdeltaPatch(args[0]->Uint32Value(), args[1]->Uint32Value(), args[2]->ToObject());

  aXD->Wrap(args.This());

  args.GetReturnValue().Set( args.This() );
}

void XdeltaPatch::PatchChunked(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);

  if (args.Length() < 1 || (args.Length() > 1 && !Buffer::HasInstance(args[0])) || !args[args.Length()-1]->IsFunction())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"arguments are ([buffer], function)")));
    return;
  }

  XdeltaPatch* aXd = ObjectWrap::Unwrap<XdeltaPatch>(args.This());

  if (aXd->mBusy)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"object busy with async op")));
    return;
  }

  if (aXd->mErrType != eErrNone || aXd->mState == eDone)
  {
    aXd->Callback(Local<Function>::Cast(args[args.Length()-1]));
    args.GetReturnValue().Set( args.This() );
    return;
  }
    
  if (args.Length() == 1) 
  {
    aXd->mBuffMaxSize = 0;
  } 
  else 
  {
    Local<Object> obj = args[0]->ToObject();
    aXd->mBufferObj.Reset( isolate, obj );

    int bufferSize = Buffer::Length(obj);
    if ( bufferSize < 0 ) 
    {
      bufferSize = 0;
    }

    aXd->mBuffMaxSize = static_cast<unsigned int>( bufferSize );
    aXd->mBuff = Buffer::Data(obj);
  }
  aXd->mBuffLen = aXd->mBuffMaxSize;

  aXd->StartAsync(Local<Function>::Cast(args[args.Length()-1]));

  args.GetReturnValue().Set( args.This() );
}

void XdeltaPatch::FinishAsync() 
{
    XdeltaOp::FinishAsync();
    mBufferObj.Reset();
}

bool XdeltaPatch::loadSecondaryFile()
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

      if (mInputBufRead < (int) mWinSize)
      {
        xd3_set_flags(&mStream, XD3_FLUSH | mStream.flags);
      }

      xd3_avail_input(&mStream, (const uint8_t*) mInputBuf, mInputBufRead);
      mConsumedInput = true;

      return true;
}

bool XdeltaPatch::generateResult()
{
    if ( !mWriter.write(mDst, mStream.next_out, (int)mStream.avail_out, mFileOffset) )
    {
        mErrType = eErrUv;
        mUvErr = mWriter.writeError();
        return false;
    }

    mFileOffset += (int)mStream.avail_out;
    xd3_consume_output(&mStream);

    return true;
}