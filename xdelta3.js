module.exports = {
  DiffStream: DiffStream,
  PatchStream: PatchStream
};

var stream = require('stream');
var util = require('util');
var xdelta = require('./build/Release/node_xdelta3.node');


function DiffStream(src, dst) { stream.Readable.call(this); this.diffObj = new xdelta.XdeltaDiff(src, dst); }
util.inherits(DiffStream, stream.Readable);
DiffStream.prototype._read = function(size) {
  var aThis = this;
  this.diffObj.diffChunked(size, function(err, data) {
    if (err)
      aThis.emit('error', err);
    else if (typeof(data) != 'undefined') 
      aThis.push(data);
    else
      aThis.push(null);
  });
};

function PatchStream(src, dst) {
  stream.Writable.call(this);
  this.patchObj = new xdelta.XdeltaPatch(src, dst);
  this.on('finish', function () { 
    var aThis = this;
    this._end(function() {
      aThis.emit('close');
    }); 
  });
}
util.inherits(PatchStream, stream.Writable);
PatchStream.prototype._write = function (chunk, encoding, callback) {
  this.patchObj.patchChunked(chunk, callback);
}
PatchStream.prototype._end = function (callback) {
  callback();
}
