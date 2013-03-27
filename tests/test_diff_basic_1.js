var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1dst'), 'r');

var aDiffBufferList = [];

var aDelta = lXdelta3.diff(aSrcFd, aDstFd);
aDelta.on('error', function(err) { throw err; });
aDelta.on('data', function(bufferChunk) {
  aDiffBufferList.push(bufferChunk);
});
aDelta.on('end', function() {
  var aDiff = Buffer.concat(aDiffBufferList);
  console.log(aDiff.toString());
  fs.closeSync(aSrcFd);
  fs.closeSync(aDstFd);
});
