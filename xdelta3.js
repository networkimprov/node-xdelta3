module.exports = {
  diff: diff,
  patchFile: patchFile
};

var Stream = require('stream');
var util = require("util");
var xdelta = require('./build/Release/node_xdelta3.node');


function DiffStream(diffObj) { Stream.Readable.call(this); this.diffObj = diffObj; }
util.inherits(DiffStream, Stream.Readable);
DiffStream.prototype._read = function(size) {
  /*TODO: pause and resume diff_chunked using diffObj*/
};

function diff(src, dst) {
  var aDiffObj = {};
  var aStream = new DiffStream(aDiffObj);
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

function PatchStream(patchObj) { Stream.Writable.call(this); this.patchObj = patchObj; }
util.inherits(PatchStream, Stream.Writable);
PatchStream.prototype._write = function (chunk, encoding, callback) {
  //TODO: call C++ method which processes a diff chunk
}

function patchFile(src, dst) {
  var aPatchObj = {};
  var aWritableStream = PatchStream(aPatchObj);
  return aWritableStream;
}
