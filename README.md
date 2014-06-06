Vorbisenc.js
=========
JavaScript Vorbis encoder.

The goal of this project is to create a Vorbis encoder that can run in a browser. Currently, Ogg and Vorbis projects build successfully with Emscripten. A build script, build.sh, is included.

There is a sample Node.js command line tool that encodes a wav file and outputs it as a Vorbis bitstream file. The usage is:
```
node node_encoder_example.js inputFile.wav outputFile.ogg
```
This example file is based heavily on encoder_example.c from Vorbis examples. Additional work had to be done as reading / writing binary data from stdin / stdout doesn't seem to work too well in Emscripten.