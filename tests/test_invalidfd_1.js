var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1dst'), 'r');

var aDelta = new lXdelta3.DiffStream(aSrcFd, aDstFd);
fs.closeSync(aDstFd);
aDelta.on('error', function(err) {
  fs.closeSync(aSrcFd);
  console.log('OK:   Close fd 1');
});
aDelta.on('data', function(bufferChunk) {
  console.log('FAIL: Close fd 1');   
});
aDelta.on('end', function() {
  console.log('FAIL: Close fd 1'); 
});
