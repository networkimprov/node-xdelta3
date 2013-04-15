module.exports = {
  diff: diff,
  patchFile: patchFile
};

var stream = require('stream');
var util = require('util');
var xdelta = require('./build/Release/node_xdelta3.node');


function DiffStream(diffObj) { stream.Readable.call(this); this.diffObj = diffObj; }
util.inherits(DiffStream, stream.Readable);
DiffStream.prototype._read = function(size) {
  var aThis = this;
  this.diffObj.diff_chunked(size, function(err, data) {
    if (err)
      aThis.emit('error', err);
    else if (typeof(data) != 'undefined') 
      aThis.push(data);
    else
      aThis.push(null);
  });
};

function diff(src, dst) {
  var aDiffObj = new xdelta.XdeldaDiff(src, dst);
  var aStream = new DiffStream(aDiffObj);
  return aStream;
}

function PatchStream(patchObj) { stream.Writable.call(this); this.patchObj = patchObj; }
util.inherits(PatchStream, stream.Writable);
PatchStream.prototype._write = function (chunk, encoding, callback) {
  //TODO: call C++ method which processes a diff chunk
}

function patchFile(src, dst) {
  var aPatchObj = {};
  var aWritableStream = PatchStream(aPatchObj);
  return aWritableStream;
}
