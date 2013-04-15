var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aBuffer = '';
for (var N = 0; N < 100000; N++)
  aBuffer += N + ' xx ' + (N+2);
fs.writeFileSync(path.resolve(__dirname, 'files/txt1dstlarge'), aBuffer);

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1dstlarge'), 'r');

var aDiffBufferList = [];

var aDelta = lXdelta3.diff(aSrcFd, aDstFd);
aDelta.on('error', function(err) { throw err; });
aDelta.on('data', function(bufferChunk) {
  aDiffBufferList.push(bufferChunk);
});
aDelta.on('end', function() {
  var aDiff = Buffer.concat(aDiffBufferList);
  fs.closeSync(aSrcFd);
  fs.closeSync(aDstFd);
  if (aDiff.length == 630125)
    console.log('OK:   Basic Large Diff');
  else
    console.log('FAIL: Basic Large Diff');
});
