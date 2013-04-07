var Stream = require('stream');
var xdelta = require('./build/Release/node_xdelta3.node');

function diff(src, dst) {
  var aStream = new Stream.Readable();
  aStream._read = function(size) { /*TODO: pause and resume diff_chunked*/};
  xdelta.diff_chunked( src, dst,
    function (buf) {
      aStream.push(buf);
    },
    function (err) {
      if (typeof(err) == 'undefined')
        aStream.push(null);
      else
        aStream.emit('error', err);
    }
  );
  return aStream;
}

function patch(src, dst, callback) {
  var aWritableStream = new Stream.Writable();

  aWritableStream._write = function (chunk, encoding, callback) {
    //TODO: call C++ method which processes a diff chunk
    console.log('patch buf: ' + buf.toString());
  }

  return aWritableStream;
}

module.exports = {
  diff: diff,
  patch: patch
};
