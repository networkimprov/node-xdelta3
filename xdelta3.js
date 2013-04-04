var Stream = require('stream');
var xdelta = require('./build/Release/node_xdelta3.node');

function diff(src, dst) {
  var aStream = new Stream;
  aStream.readable = true;
  xdelta.diff_chunked( src, dst,
    function (buf) {
      aStream.emit('data', buf);
    },
    function (err) {
      if (typeof(err) == 'undefined')
        aStream.emit('end');
      else
        aStream.emit('error');
    }
  );
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
