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

void XdeltaDiff::Init(Handle<Object> target) 
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("XdeltaDiff"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "diffChunked", DiffChunked);

  target->Set(String::NewSymbol("XdeltaDiff"), constructor_template->GetFunction());
}

Handle<Value> XdeltaDiff::New(const Arguments& args) 
{
  HandleScope scope;

  if (args.Length() < 3 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[2]->IsObject())
  {
    return ThrowException(Exception::TypeError(String::New("arguments are (fd, fd, obj)")));
  }

  XdeltaDiff* aXD = new XdeltaDiff(args[0]->Uint32Value(), args[1]->Uint32Value(), args[2]->ToObject());

  aXD->Wrap(args.This());

  return args.This();
}

Handle<Value> XdeltaDiff::DiffChunked(const Arguments& args) 
{
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsFunction())
  {
    return ThrowException(Exception::TypeError(String::New("arguments are (number, function)")));
  }

  XdeltaDiff* aXd = ObjectWrap::Unwrap<XdeltaDiff>(args.This());

  if (aXd->mBusy)
  {
    return ThrowException(Exception::TypeError(String::New("object busy with async op")));
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
    
  return args.This();
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

    if (mInputBufRead < 0)
    {
        mErrType = eErrUv;
        mUvErr = mReader.readError();
        return false;
    }

    if (mInputBufRead < (int) mWinSize)
    {
      xd3_set_flags(&mStream, XD3_FLUSH | mStream.flags);
    }

    xd3_avail_input(&mStream, (const uint8_t*) mInputBuf, mInputBufRead);
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