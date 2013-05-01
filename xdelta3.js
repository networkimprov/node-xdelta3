module.exports = {
  DiffStream: DiffStream,
  PatchStream: PatchStream
};

var stream = require('stream');
var util = require('util');
var xdelta = require('./build/Release/node_xdelta3.node');


function DiffStream(src, dst, opt) {
  stream.Readable.call(this, opt); //fix check opt for winsize; fs.fstat if none
  this.diffObj = new xdelta.XdeltaDiff(src, dst);
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


function PatchStream(src, dst) {
  stream.Writable.call(this, opt); //fix check opt for winsize
  this.patchObj = new xdelta.XdeltaPatch(src, dst);
  this.on('finish', function () { 
    var that = this;
    that._end(function(err) { //fix if only use of _end(), just call patchChunked here
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
