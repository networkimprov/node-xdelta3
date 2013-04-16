var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1result'), 'w');




var aPatch = new lXdelta3.PatchStream(aSrcFd, aDstFd);
aPatch.write(fs.readFileSync(path.resolve(__dirname, 'files/txt1gent')));
aPatch.end();
aPatch.on('close', function() {
  fs.closeSync(aSrcFd);
  fs.closeSync(aDstFd);
  if (fs.readFileSync(path.resolve(__dirname, 'files/txt1dst')) == fs.readFileSync(path.resolve(__dirname, 'files/txt1result')))
    console.log('OK:   Basic Patch');
  else
    console.log('FAIL: Basic Patch');
});

