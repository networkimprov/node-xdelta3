#ifndef XDELTAOP_H_INCLUDE
#define XDELTAOP_H_INCLUDE

#include <v8.h>
#include <node.h>
#include "FileReader.h"
#include <string>

extern "C" {
  #include "../../include/xdelta3/xdelta3.h"
}

class XdeltaOp : public node::ObjectWrap 
{
protected:
  enum OpType 
  { 
      eOpDiff, 
      eOpPatch 
  };
  enum OperationState 
  { 
      eStart, 
      eRun, 
      eDone 
  };
  enum OperationErrorType 
  { 
      eErrNone, 
      eErrUv, 
      eErrXd 
  };

  XdeltaOp(int s, int d, v8::Local<v8::Object> cfg, OpType op);
  virtual ~XdeltaOp();
  void SetCfg(v8::Local<v8::Object> cfg);
  static int GetInt32CfgValue(v8::Local<v8::Object> cfg, const char* str, int def);
  void StartAsync(v8::Handle<v8::Function> fn);
  virtual void FinishAsync();
  int Write(int fd, void* buf, size_t size, size_t offset);
  bool loadSourceFile();
  bool loadSecondaryFile();

  static void Work_pool(uv_work_t* req);
  static void Work_done(uv_work_t* req, int );

  void Pool();
  void Done();
  void Callback(v8::Handle<v8::Function> callback);

  OpType mOpType;
  int mSrc, mDst;
  v8::Persistent<v8::Function> mCallback;
  int mWinSize;
  FileReader mReader;
  bool mBusy;
  OperationState mState;
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
  OperationErrorType mErrType;
  uv_err_t mUvErr;
  std::string mXdErr;
};

#endif //XDELTAOP_H_INCLUDE