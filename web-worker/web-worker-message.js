onmessage = function(msg) {
	switch(msg.data['cmd']) {
		case "start":
			Module['_vorbisenc_start'].apply(this,msg.data['args']);
		break;
		case "finish":
			Module['_vorbisenc_finish']();
		break;
		case "write":
			var leftChannel  = msg.data['args'][0],
			    rightChannel = msg.data['args'][1],
			    length       = msg.data['args'][0].length,
			    bytesPerEl   = leftChannel.BYTES_PER_ELEMENT,
			    bufLeft      = Module._malloc(length*bytesPerEl*2),
			    bufRight     = bufLeft + (length*bytesPerEl);
			
			Module.HEAPF32.set(leftChannel,bufLeft);
			Module.HEAPF32.set(rightChannel,bufRight);
			Module.ccall('vorbisenc_write','number',['number','number','number'],[bufLeft<<2,bufRight<<2,length]);
			Module._free(bufLeft);
		break;
	}
}