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

var aDelta = new lXdelta3.DiffStream(aSrcFd, aDstFd);
aDelta.on('error', function(err) { throw err; });
aDelta.on('data', function(bufferChunk) {
  aDiffBufferList.push(bufferChunk);
});
aDelta.on('end', function() {
  var aDiff = Buffer.concat(aDiffBufferList);
  fs.closeSync(aSrcFd);
  fs.closeSync(aDstFd);

  if (aDiff.length != 630634)
    console.log('FAIL: Basic Large Diff');
  else {
    console.log('OK:   Basic Large Diff');

    var aSrcFd2 = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
    var aDstFd2 = fs.openSync(path.resolve(__dirname, 'files/txt1result'), 'w');

    var aPatch = new lXdelta3.PatchStream(aSrcFd2, aDstFd2);

    for (var N = 0; N < aDiffBufferList.length; N++)
      aPatch.write(aDiffBufferList[N]);
    aPatch.end();
    aPatch.on('error', function(err) { console.log(err); });
    aPatch.on('close', function() {
      fs.closeSync(aSrcFd);
      fs.closeSync(aDstFd);
      if (aBuffer.toString() == fs.readFileSync(path.resolve(__dirname, 'files/txt1result')).toString())
        console.log('OK:   Basic Large Patch');
      else
        console.log('FAIL: Basic Large Patch');
    });

  }

});




