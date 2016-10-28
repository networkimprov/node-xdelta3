module.exports = {
  DiffStream: DiffStream,
  PatchStream: PatchStream
};

var stream = require('stream');
var fs = require('fs');
var util = require('util');
var xdelta = require('./build/Release/node_xdelta3.node');

for (var aOpt in xdelta.constants)
  module.exports[aOpt] = xdelta.constants[aOpt];

function DiffStream(src, dst, opt) {
  stream.Readable.call(this, opt || {});
  this.diffObj = new xdelta.XdeltaDiff(src, dst, opt || {});
}
util.inherits(DiffStream, stream.Readable);

DiffStream.prototype._read = function(size) {
  var that = this;
  that.diffObj.diffChunked(size, function(err, data) {
    if (err)
      that.emit('error', err);
    else
      that.push(data);
  });
};


function PatchStream(src, dst, opt) {
  stream.Writable.call(this, opt || {});
  this.patchObj = new xdelta.XdeltaPatch(src, dst, opt || {});
  this.on('finish', function () { 
    var that = this;

    that.patchObj.patchChunked(function(err) {
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
