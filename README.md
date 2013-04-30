# node-xdelta3

Asynchronous, non-blocking Node.js binding for Xdelta3 binary diff utility

# API

**xdelta3.DiffStream(fd src, fd dst, [obj opt])**  
src - original file  
dst - file to generate diff with  
opt - stream options  
creates a readable stream  

**xdelta3.PatchStream(fd src, fd dest, [obj opt]);**  
src - original file  
dest - file generated from diff  
opt - stream options  
creates a writable stream  

# USAGE

``` js
var aDelta = new xdelta3.DiffStream(src, dst);
aDelta.on('data', function(bufferChunk) {
  console.log('chunk of diff received');
});
aDelta.on('end', function() {
  console.log('diff finished');
});
aDelta.on('error', function(err) {
  console.log('error: ' + err);
});

var aPatch = new xdelta3.PatchStream(src, dst);
for (var N = 0; N < aDiffBufferChunks.length; N++)
  aPatch.write(aDiffBufferChunks[i]);
aPatch.end();
aPatch.on('error', function(err) {
  console.log('error: ' + err);
});
aPatch.on('close', function() {
  console.log('patch finished');
});

```

# DEPENDENCIES

This binding requires the existence of libxdelta3 in a shared library path:

* download the sources for a stable release - [3.0.6 here](https://code.google.com/p/xdelta/downloads/detail?name=xdelta3-3.0.6.tar.gz)
* build the command line interface
* create the library from the existing objects:
```
gcc -shared xdelta3-xdelta3.o -o libxdelta3.so
```
* move the library to a shared lib path (ex /usr/local/lib)

