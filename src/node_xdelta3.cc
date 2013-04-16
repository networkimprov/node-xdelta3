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
  XdeltaOp(int s, int d, int op)
  : ObjectWrap(), mOpType((OpType)op), mSrc(s), mDst(d), mBusy(false), mFirstTime(true), mFinishedProcessing(false),
    mDiffBuffMaxSize(0), mDiffBuffSize(0), mWroteFromStream(0), mInputBufRead(0), mConsumedInput(false), mReadDstN(0), mErrType(eErrNone)
  {
    memset (&mStream, 0, sizeof (mStream));
    memset (&mSource, 0, sizeof (mSource));
    mConfig.winsize = XD3_ALLOCSIZE;
    mSource.blksize = XD3_ALLOCSIZE;
    mSource.curblk = (const uint8_t*) new char[mSource.blksize];
    mInputBuf = (void*) new char[mSource.blksize];
  }
  virtual ~XdeltaOp() {
    delete[] (char*)mSource.curblk;
    delete[] (char*)mInputBuf;
    if (mDiffBuff) delete[] mDiffBuff;
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
    if (aBytesWrote != size) {
      mErrType = eErrUv;
      mUvErr = uv_last_error(uv_default_loop());
    }
    return aBytesWrote - size;
  }

  static void OpChunked_pool(uv_work_t* req);
  static void OpChunked_done(uv_work_t* req, int );

  enum OpType { eOpDiff, eOpPatch } mOpType;
  int mSrc, mDst;
  Persistent<Function> mCallback;
  bool mBusy;

  bool mFirstTime; //fix replace these with enum { eStart, eRun, eDone } mState;
  bool mFinishedProcessing;

  int mDiffBuffMaxSize;
  char* mDiffBuff;
  int mDiffBuffSize;
  unsigned mWroteFromStream; //fix unsigned int or size_t

  void* mInputBuf;
  int mInputBufRead;
  bool mConsumedInput;
  int mReadDstN;
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

  int aSize = args[0]->Uint32Value(); //fix move to XdeltaOp function
  if (aSize > aXd->mDiffBuffMaxSize) {
    if (aXd->mDiffBuffMaxSize != 0)
      delete[] aXd->mDiffBuff;
    aXd->mDiffBuffMaxSize = aSize;
    aXd->mDiffBuff = new char[aXd->mDiffBuffMaxSize];
  }

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

  if (args.Length() < 1 || args.Length() > 1 && !Buffer::HasInstance(args[0]) || !args[args.Length()-1]->IsFunction())
    return ThrowException(Exception::TypeError(String::New("arguments are ([buffer], function)")));

  XdeltaPatch* aXd = ObjectWrap::Unwrap<XdeltaPatch>(args.This());

  if (aXd->mBusy)
    return ThrowException(Exception::TypeError(String::New("object busy with async op")));

  if (args.Length() == 1) {
    aXd->mDiffBuffMaxSize = 0;
  } else {
    Local<Object> aBuffer = args[0]->ToObject();
    int aSize = Buffer::Length(aBuffer); //fix move to XdeltaOp function
    if (aSize > aXd->mDiffBuffMaxSize) {
      if (aXd->mDiffBuffMaxSize != 0)
        delete[] aXd->mDiffBuff;
      aXd->mDiffBuffMaxSize = aSize;
      aXd->mDiffBuff = new char[aXd->mDiffBuffMaxSize];
    }
    memcpy(aXd->mDiffBuff, Buffer::Data(aBuffer), aXd->mDiffBuffMaxSize); //fix can mDiffBuff point into Buffer member?
  }
  aXd->StartAsync(Local<Function>::Cast(args[args.Length()-1]));

  return args.This();
}

void XdeltaOp::OpChunked_pool(uv_work_t* req) {
  XdeltaDiff* aXd = (XdeltaDiff*) req->data;

  if (aXd->mOpType == eOpDiff)
    aXd->mDiffBuffSize = 0;
  else
    aXd->mDiffBuffSize = aXd->mDiffBuffMaxSize;
  if (aXd->mFirstTime) {
    xd3_init_config(&aXd->mConfig, XD3_ADLER32);
    xd3_config_stream(&aXd->mStream, &aXd->mConfig);
    
    int aBytesRead = aXd->Read(aXd->mSrc, (void*)aXd->mSource.curblk, aXd->mSource.blksize, 0);
    if (aBytesRead < 0) { //fix way to make xd request this read with getsrcblk?
      xd3_close_stream(&aXd->mStream);
      xd3_free_stream(&aXd->mStream);  //fix config stream after read? would make cleanup unnecessary
      return;
    }
    aXd->mSource.onblk = aBytesRead;
    aXd->mSource.curblkno = 0;
    xd3_set_source(&aXd->mStream, &aXd->mSource);
  }

  if (aXd->mOpType == eOpDiff && aXd->mWroteFromStream < aXd->mStream.avail_out) { //if there is something left in the out stream to emit for a readable buffer
    int aWriteSize = (aXd->mStream.avail_out - aXd->mWroteFromStream > aXd->mDiffBuffMaxSize) ? aXd->mDiffBuffMaxSize : aXd->mStream.avail_out - aXd->mWroteFromStream;
    memcpy(aXd->mDiffBuff, aXd->mStream.next_out + aXd->mWroteFromStream, aWriteSize);
    aXd->mDiffBuffSize += aWriteSize;
    aXd->mWroteFromStream += aWriteSize;
    if (aXd->mWroteFromStream < aXd->mStream.avail_out)
      return;
    xd3_consume_output(&aXd->mStream);
  }

  if (aXd->mFinishedProcessing)
    return;


  bool aRead = aXd->mFirstTime || aXd->mOpType == eOpPatch;
  aXd->mFirstTime = false;
  do {
    if (aRead) {
      aRead = false;
      if (aXd->mOpType == eOpDiff) {
        aXd->mInputBufRead = aXd->Read(aXd->mDst, aXd->mInputBuf, XD3_ALLOCSIZE, aXd->mReadDstN * XD3_ALLOCSIZE);
        aXd->mReadDstN++;
        if (aXd->mInputBufRead < 0)
          return;
      } else {
        if (aXd->mConsumedInput) {
          aXd->mInputBufRead = 0;
          aXd->mConsumedInput = false;
        }
        if (aXd->mInputBufRead != XD3_ALLOCSIZE && aXd->mDiffBuffSize != 0) {
          int aReadSize = (aXd->mDiffBuffSize < XD3_ALLOCSIZE - aXd->mInputBufRead) ? aXd->mDiffBuffSize : XD3_ALLOCSIZE - aXd->mInputBufRead;
          if (aReadSize != 0) {
            memcpy(aXd->mInputBuf + aXd->mInputBufRead, aXd->mDiffBuff + aXd->mDiffBuffMaxSize - aXd->mDiffBuffSize, aReadSize);
            aXd->mDiffBuffSize -= aReadSize;
            aXd->mInputBufRead += aReadSize;
          }
          if (aXd->mInputBufRead != XD3_ALLOCSIZE || aXd->mDiffBuffSize == 0) 
            return;
        }
      }
      if (aXd->mInputBufRead < (int) XD3_ALLOCSIZE)
        xd3_set_flags(&aXd->mStream, XD3_FLUSH | aXd->mStream.flags);
      xd3_avail_input(&aXd->mStream, (const uint8_t*) aXd->mInputBuf, aXd->mInputBufRead);
      aXd->mConsumedInput = true;
    }

    do {
      int aRet = aXd->mOpType == eOpDiff ? xd3_encode_input(&aXd->mStream) : xd3_decode_input(&aXd->mStream);
      switch (aRet) {
      case XD3_INPUT:
        aRead = true; //fix move contents of if(aRead) here?
        break;
      case XD3_OUTPUT: {
        if (aXd->mOpType == eOpDiff) {
          int aWriteSize = ((int)aXd->mStream.avail_out > aXd->mDiffBuffMaxSize - aXd->mDiffBuffSize) ? aXd->mDiffBuffMaxSize - aXd->mDiffBuffSize : aXd->mStream.avail_out;
          memcpy(aXd->mDiffBuff + aXd->mDiffBuffSize, aXd->mStream.next_out, aWriteSize);
          aXd->mDiffBuffSize += aWriteSize;
          aXd->mWroteFromStream = aWriteSize;
          if (aXd->mWroteFromStream < aXd->mStream.avail_out) //diff buffer is full
             return;
          xd3_consume_output(&aXd->mStream);
        } else {
          if (aXd->Write(aXd->mDst, aXd->mStream.next_out, (int)aXd->mStream.avail_out, aXd->mReadDstN) < 0)
            return;
          aXd->mReadDstN += (int)aXd->mStream.avail_out;
          xd3_consume_output(&aXd->mStream);
        }
        break;
      }
      case XD3_GETSRCBLK: {
        int aBytesRead = aXd->Read(aXd->mSrc, (void*) aXd->mSource.curblk, aXd->mSource.blksize, aXd->mSource.blksize * aXd->mSource.getblkno);
        if (aBytesRead < 0) {
          xd3_close_stream(&aXd->mStream); //fix let end of function cleanup
          xd3_free_stream(&aXd->mStream);
          return;
        }
        aXd->mSource.onblk = aBytesRead;
        aXd->mSource.curblkno = aXd->mSource.getblkno;
        break; 
      }
      case XD3_GOTHEADER:
      case XD3_WINSTART:
      case XD3_WINFINISH:
        break;
      default:
        aXd->mErrType = eErrXd;
        aXd->mXdErr = aXd->mStream.msg;
        xd3_close_stream(&aXd->mStream); //fix let end of function cleanup
        xd3_free_stream(&aXd->mStream);
        return;
      }
    } while (!aRead); //fix inner loop not required; test other conditions in main while()
  } while (aXd->mInputBufRead == XD3_ALLOCSIZE);

  xd3_close_stream(&aXd->mStream);
  xd3_free_stream(&aXd->mStream);

  aXd->mFinishedProcessing = true;
}

void XdeltaOp::OpChunked_done(uv_work_t* req, int ) {
  HandleScope scope;
  XdeltaDiff* aXd = (XdeltaDiff*) req->data;

  Handle<Value> aArgv[2];
  int aArgc = 2;

  if (aXd->mErrType != eErrNone) {
    aArgv[0] = String::New(aXd->mErrType == eErrUv ? uv_strerror(aXd->mUvErr) : aXd->mXdErr.c_str());
    aArgc = 1;
  } else if (aXd->mFinishedProcessing && aXd->mDiffBuffSize == 0) {
    aArgc = 0;
  } else {
    aArgv[0] = Undefined();
    aArgv[1] = node::Buffer::New(aXd->mDiffBuff, aXd->mDiffBuffSize)->handle_;
  }
  aXd->Unref();
  aXd->mBusy = false;
  Local<Function> aCallback(aXd->mCallback);
  aXd->mCallback.Dispose();
  
  TryCatch try_catch;
  aCallback->Call(Context::GetCurrent()->Global(), aArgc, aArgv);
  if (try_catch.HasCaught())
    FatalException(try_catch);

  delete req;
}

