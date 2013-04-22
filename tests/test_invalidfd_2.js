var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1result'), 'w');


var aPatch = new lXdelta3.PatchStream(aSrcFd, aDstFd);
aPatch.write(fs.readFileSync(path.resolve(__dirname, 'files/txt1gent')));
fs.closeSync(aDstFd);
aPatch.end();
aPatch.on('close', function() {
  console.log('FAIL: Close fd 2');
});
aPatch.on('error', function() {
  fs.closeSync(aSrcFd);
  console.log('OK:   Close fd 2');
});

