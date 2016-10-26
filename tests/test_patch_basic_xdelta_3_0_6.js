// This test verifies that a diff generated with Xdelta3 version 3.0.6
// can still be used by Xdelta3 version 3.0.11.

var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1result'), 'w');




var aPatch = new lXdelta3.PatchStream(aSrcFd, aDstFd);
aPatch.write(fs.readFileSync(path.resolve(__dirname, 'files/txt1gent.3.0.6')));
aPatch.end();
aPatch.on('close', function() {
  fs.closeSync(aSrcFd);
  fs.closeSync(aDstFd);
  if (fs.readFileSync(path.resolve(__dirname, 'files/txt1dst')).toString() == fs.readFileSync(path.resolve(__dirname, 'files/txt1result')).toString())
    console.log('OK:   Basic Patch Regression 3.0.6');
  else
    console.log('FAIL: Basic Patch Regression 3.0.6');
});

