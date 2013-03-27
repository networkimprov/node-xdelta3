var Stream = require('stream');

function diff(src, dst) {
  var aStream = new Stream;
  aStream.readable = true;

  //TODO: call C++ method which will emit chunks as diff gets to a certain size
  setTimeout(function (){
    aStream.emit('data', new Buffer('not yet developed'));
    aStream.emit('end');
  }, 1000);

  return aStream;
}

function patch(src, dst, callback) {
  var aWritableStream = new Stream;
  aWritableStream.writable = true;

  aWritableStream.write = function (buf) {
    //TODO: call C++ method which processes a diff chunk
    console.log('patch buf: ' + buf.toString());
  }

  aWritableStream.end = function() {
    //TODO: call C++ method which signals the end of a patch
    callback();
  }

  return aWritableStream;
}

module.exports = {
  diff: diff,
  patch: patch
};
