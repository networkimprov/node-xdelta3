#include "XdeltaDiff.h"

using namespace v8;

XdeltaDiff::XdeltaDiff(int s, int d, Local<Object> cfg) :
    XdeltaOp(s, d, cfg, eOpDiff), 
    mBuffMemSize(0) 
{

}

XdeltaDiff::~XdeltaDiff()
{
    if (mBuffMemSize > 0)
    {
      delete[] mBuff;
    }
}

void XdeltaDiff::Init(Handle<Object> exports)
{
  Isolate* isolate = exports->GetIsolate();
  HandleScope scope(isolate);

  Local<FunctionTemplate> t = FunctionTemplate::New(isolate, New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewFromUtf8(isolate,"XdeltaDiff", String::kInternalizedString));
  constructor_template.Reset(isolate,t);

  NODE_SET_PROTOTYPE_METHOD(t, "diffChunked", DiffChunked);

  exports->Set(String::NewFromUtf8(isolate, "XdeltaDiff", String::kInternalizedString), t->GetFunction());
}

void XdeltaDiff::New(const FunctionCallbackInfo<Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsObject())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"arguments are (fd, fd, obj)")));
    return;
  }

  XdeltaDiff* aXD = new XdeltaDiff(args[0]->Uint32Value(), args[1]->Uint32Value(), args[2]->ToObject());

  aXD->Wrap(args.This());

  args.GetReturnValue().Set(args.This());
}

void XdeltaDiff::DiffChunked(const FunctionCallbackInfo<Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsFunction())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"arguments are (number, function)")));
    return;
  }

  XdeltaDiff* aXd = ObjectWrap::Unwrap<XdeltaDiff>(args.This());

  if (aXd->mBusy)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate,"object busy with async op")));
    return;
  }

  aXd->mBuffMaxSize = args[0]->Uint32Value();
  if (aXd->mBuffMaxSize > aXd->mBuffMemSize) 
  {
    if (aXd->mBuffMemSize > 0)
    {
      delete[] aXd->mBuff;
    }

    aXd->mBuff = new char[aXd->mBuffMemSize = aXd->mBuffMaxSize];
  }
  aXd->mBuffLen = 0;

  if (aXd->mWroteFromStream < aXd->mStream.avail_out) //if there is something left in the out stream to copy to aXd->mBuff
  {
    unsigned int availableSpace = aXd->mStream.avail_out - aXd->mWroteFromStream;
    int aWriteSize = ( availableSpace > aXd->mBuffMaxSize ) ? aXd->mBuffMaxSize : availableSpace;
    memcpy(aXd->mBuff, aXd->mStream.next_out + aXd->mWroteFromStream, aWriteSize);
    aXd->mBuffLen += aWriteSize;
    aXd->mWroteFromStream += aWriteSize;
  }

  if (aXd->mErrType != eErrNone || aXd->mState == eDone)
  {
    aXd->Callback(Local<Function>::Cast(args[1]));
  }
  else if (aXd->mWroteFromStream < aXd->mStream.avail_out) 
  {
    aXd->Callback(Local<Function>::Cast(args[1]));
  }
  else
  {
    aXd->StartAsync(Local<Function>::Cast(args[1]));
  }

  args.GetReturnValue().Set(args.This());
}

void XdeltaDiff::Pool()
{
  if ( mWroteFromStream == mStream.avail_out )
  {
    xd3_consume_output(&mStream);
  }

  XdeltaOp::Pool();
}

bool XdeltaDiff::loadSecondaryFile()
{
    mInputBufRead = mReader.read(mDst, mInputBuf, mWinSize, mFileOffset);
    mFileOffset += mWinSize;

    if (mReader.isError())
    {
        mErrType = eErrUv;
        mUvErr = mReader.readError();
        return false;
    }

    if (mInputBufRead < (int) mWinSize)
    {
      xd3_set_flags(&mStream, XD3_FLUSH | mStream.flags);
    }

    xd3_avail_input(&mStream, reinterpret_cast<const uint8_t*>( mInputBuf ), mInputBufRead);
    mConsumedInput = true;

    return true;
}

bool XdeltaDiff::generateResult()
{
  unsigned int availableSpace = mBuffMaxSize - mBuffLen;
  int aWriteSize = (mStream.avail_out > availableSpace ) ? availableSpace : mStream.avail_out;

  memcpy(mBuff + mBuffLen, mStream.next_out, aWriteSize);
  mBuffLen += aWriteSize;
  mWroteFromStream = aWriteSize;

  if (mWroteFromStream < mStream.avail_out) //diff buffer is full
  {
     return false;
  }

  xd3_consume_output(&mStream);
  return true;
}