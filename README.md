Vorbisenc.js
=========
JavaScript Ogg Vorbis encoder.

This is a JavaScript Ogg Vorbis encoder. It has been built using Emscripten. It runs in a browser environment as a web worker.

This project may be built by executing build.sh. This builds Ogg, Vorbis and the root project. This script has only been tested in a Linux environment. It should also work on other *nix systems.

Workflow
---

1. Create the worker.

    ```javascript
    var worker = new Worker("/path/to/worker.js");
    ```

2. Start the worker. The given buffer size determines when the worker will respond with data. If set to 0, worker will respond with an encoded buffer for every input buffer. If set to a larger value, worker will respond every time the buffer gets full. The buffer size is limited but may be adjusted, see TOTAL_MEMORY emcc parameter.

    ```javascript
    // sampleRate : The sampling rate of the source audio
    // quality    : Desired quality level, from -0.1 to 1.0 (lo to hi)
    // bufferSize : Internal encoded buffer size, in bytes from 0 to N
    worker.postMessage({
        "cmd"  : "start",
        "args" : [sampleRate,quality,bufferSize]
    });
    ```

3. Wait for a "started" message from the worker. This is necessary because, internally, an initialization procedure takes place and because workers run in their own threads.

    ```javascript
    worker.onmessage = function(e) {
        switch(e.data.message) {
            case "started":
                // Do stuff
            break;
        }
    }
    ```

4. Send data as it becomes available. Currently, only two channels are supported.

    ```javascript
    // inputBufferLeft  : left channel, non-interleaved floating point samples
    // inputBufferRight : right channel, non-interleaved floating point samples
    worker.postMessage({
        "cmd"  : "write",
        "args" : [inputBufferLeft,inputBufferRight]
    });
    ```

5. Receive encoded data as it becomes available. Data is a Uint8Array, an array of 8-bit unsigned integers.

    ```javascript
    worker.onmessage = function(e) {
        switch(e.data.message) {
            case "buffer":
                // Come up with a better name
                var buffer = e.data.data;
            break;
        }
    }
    ```

6. Stop the encoder when done.

    ```javascript
    worker.postMessage({ "cmd" : "finish" });
    ```

7. Wait for a finished message. Note that, between this step and the previous one, you may receive more output buffers.

    ```javascript
    worker.onmessage = function(e) {
        switch(e.data.message) {
            case "finished":
                // Do stuff
            break;
        }
    }
    ```

8. Worker may be reused. Go back to step 1 if necessary.


To Do
---
* Minify & optimize. Currently, the generated JavaScript is almost 3 megabytes.
* Currently, during my own testing, the encoded audio has some artifacts. Figure out what the issue is.
* Add an example.
* Document/remove node_encoder_example.js, a Node.js encoding command line utility. An initial sample project.


Credits
---
* Myself, Mateusz Wielgos.
* [halfvector] here on GitHub - most of the C/C++ code was taken from his [jist].
* [ogv.js] and its forks - help with build script.

License
---
BSD

[jist]:https://gist.github.com/halfvector/9105335#file-ogg-vorbis-encoder-cpp
[halfvector]:https://github.com/halfvector
[ogv.js]:https://github.com/brion/ogv.js/tree/master