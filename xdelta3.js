module.exports = {
  DiffStream: DiffStream,
  PatchStream: PatchStream
};

var stream = require('stream');
var util = require('util');
var xdelta = require('./build/Release/node_xdelta3.node');


function DiffStream(src, dst) {
  stream.Readable.call(this);
  this.diffObj = new xdelta.XdeltaDiff(src, dst);
}
util.inherits(DiffStream, stream.Readable);

DiffStream.prototype._read = function(size) {
  var that = this;
  that.diffObj.diffChunked(size, function(err, data, nextTick) {
    if (err)
      that.emit('error', err);
    else
      that.push(typeof(data) === 'undefined' ? null : data);
  });
};


function PatchStream(src, dst) {
  stream.Writable.call(this);
  this.patchObj = new xdelta.XdeltaPatch(src, dst);
  this.on('finish', function () { 
    var that = this;
    that._end(function(err) {
      if (err)
        that.emit('error', err);
      else
        that.emit('close');
    }); 
  });
}
util.inherits(PatchStream, stream.Writable);

PatchStream.prototype._write = function (chunk, encoding, callback) {
  this.patchObj.patchChunked(chunk, callback);
};

PatchStream.prototype._end = function (callback) {
  this.patchObj.patchChunked(callback);
};
