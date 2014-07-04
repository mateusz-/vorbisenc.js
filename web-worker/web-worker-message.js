onmessage = function(msg) {
	switch (msg.data['funcName']) {
		case "lexy_encoder_start":
			var func = Module['_' + msg.data['funcName']];
			if(!func) {
				throw 'invalid worker function to call: ' + msg.data['funcName'];
			};
			func.apply(this,msg.data['data']);
		break;
		case "lexy_encoder_write":
			var leftChannel  = msg.data['data'][0],
			    rightChannel = msg.data['data'][1],
			    length       = msg.data['data'][0].length,
			    bytesPerEl   = leftChannel.BYTES_PER_ELEMENT,
			    bufLeft      = Module._malloc(length*bytesPerEl*2),
			    bufRight     = bufLeft + (length*bytesPerEl);
			
			Module.HEAPF32.set(leftChannel,bufLeft);
			Module.HEAPF32.set(rightChannel,bufRight);
			Module.ccall('lexy_encoder_write','number',['number','number','number'],[bufLeft<<2,bufRight<<2,length]);
			Module._free(bufLeft);
		break;
	}
}