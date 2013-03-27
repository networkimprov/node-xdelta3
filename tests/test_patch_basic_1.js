var lXdelta3 = require('../xdelta3');
var fs = require('fs');
var path = require('path');

var aSrcFd = fs.openSync(path.resolve(__dirname, 'files/txt1src'), 'r');
var aDstFd = fs.openSync(path.resolve(__dirname, 'files/txt1gent'), 'w');


var aPatch = lXdelta3.patch(aSrcFd, aDstFd, function() {
  console.log('patch finished');
  fs.closeSync(aSrcFd);
  fs.closeSync(aDstFd);
});
aPatch.write(new Buffer('some diff chunk'));
aPatch.end();
