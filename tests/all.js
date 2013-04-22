var nPath = require('path');
var nFs = require('fs');
var nChild = require('child_process');

var aCmd = '';

var aAllFiles = nFs.readdirSync(nPath.dirname(module.filename));
for (var i=0; i < aAllFiles.length; i++)
  if (aAllFiles[i].match(/^test_.*\.js$/g))
    aCmd += 'node ' + nPath.resolve(__dirname, aAllFiles[i]) + ';';

run(aCmd, 1, function(err) {
  if (err) throw new Error('halted');
  console.log('tests complete');
});

function run(iCommand, iErrValue, iCallback) {
  if (arguments.length < 3) {
    iCallback = iErrValue;
    iErrValue = undefined;
  }
  var aLine = iCommand.split(/\s*;\s*/);
  fRun(0);
  function fRun(aN) {
    for (; aN < aLine.length; ++aN) {
      var aArg = aLine[aN].split(/\s+/);
      if (aArg[0] === '')
        aArg.shift();
      if (aArg.length)
        break;
    }
    if (aN === aLine.length)
      return process.nextTick(iCallback);
    if (aArg[aArg.length-1] === '')
      aArg.pop();
    var aCmd = aArg.shift();
    var aC = nChild.spawn(aCmd, aArg, {customFds:[0,1,2], env:process.env});
    aC.on('exit', function(code) {
      if (code) return iCallback(iErrValue);
      fRun(++aN);
    });
  }
}
