var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1dst'), 'r');

var aDiffBufferList = [];

var aDelta = new lXdelta3.DiffStream(aSrcFd, aDstFd);
aDelta.on('error', function(err) { throw err; });
aDelta.on('data', function(bufferChunk) {
  aDiffBufferList.push(bufferChunk);
});
aDelta.on('end', function() {
  var aDiff = Buffer.concat(aDiffBufferList);
  fs.closeSync(aSrcFd);
  fs.closeSync(aDstFd);
  var aDiffOk = fs.readFileSync(path.resolve(__dirname, 'files/txt1gent'));
  if (aDiff.toString('hex') == aDiffOk.toString('hex'))
    console.log('OK:   Basic Diff');
  else
    console.log('FAIL: Basic Diff');
});
