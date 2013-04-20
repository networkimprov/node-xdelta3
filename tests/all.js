
var aCmd = '\
  node diff_basic_1.js; \
  node diffpatch_large_1.js; \
  node patch_basic_1.js';

run(aCmd, 1, function(err) {
  if (err) throw new Exception('halted');
  console.log('tests complete');
});

var nChild = require('child_process');

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
