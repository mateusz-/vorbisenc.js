// Chrome GC bug? https://code.google.com/p/chromium/issues/detail?id=82795
window.processor;

(function() {
	
	var running       = false;
	var buffer        = new Uint8Array();
	var encoderWorker = new Worker("web-worker/web-worker.js");

	var shim = {
		AudioContext: (window.AudioContext || window.webkitAudioContext),
		getUserMedia: (navigator.getUserMedia || navigator.webkitGetUserMedia || navigator.mozGetUserMedia || navigator.msGetUserMedia).bind(navigator)
	};
	
	function startRecording() {
		if(running) { return; };
		
		buffer = new Uint8Array();
		var ac = new shim.AudioContext;
		
		if(!ac) {
			return alert("No web audio support in this browser!");
		};
		
		console.log("Audio context set up.");
		console.log("navigator.getUserMedia " + (shim.getUserMedia ? "available." : "not present!"));
		
		shim.getUserMedia({ "audio" : true },function(stream) {
			var input = ac.createMediaStreamSource(stream);
			console.log("Media stream created.");
			console.log("input sample rate " + input.context.sampleRate);
			
			encoderWorker.onmessage = function (e) {
				switch(e.data.message) {
					case "buffer":
						console.log("got buffer");
						var newBuffer = new Uint8Array(e.data.data.length + buffer.length);
						newBuffer.set(buffer,0);
						newBuffer.set(e.data.data,buffer.length);
						buffer = newBuffer;
					break;
					case "finished":
						function encode64(buffer) {
							var binary = '';
							for(var i=0; i<buffer.byteLength;i++) {
								binary += String.fromCharCode(buffer[i]);
							}
							return window.btoa(binary);
						};
						var mp3Blob = new Blob([buffer], { "type" : "audio/ogg" });
					
						var url = 'data:audio/ogg;base64,'+encode64(buffer);
						var li = document.createElement('li');
						var au = document.createElement('audio');
						var hf = document.createElement('a');
						
						var recordingsList = document.getElementById("recordings-list");
						
						au.controls = true;
						au.src = url;
						hf.href = url;
						hf.download = 'audio_recording_' + new Date().getTime() + '.ogg';
						hf.innerHTML = hf.download;
						li.appendChild(au);
						li.appendChild(hf);
						recordingsList.appendChild(li);
						running = false;
					break;
					case "started":
						processor = (input.context.createScriptProcessor || input.context.createJavaScriptNode).call(input.context,4096,2,2);
						running   = true;
						processor.onaudioprocess = function(e) {
							if(!running) { return; };
							encoderWorker.postMessage({
								"cmd"  : "write",
								"args" : [e.inputBuffer.getChannelData(0),e.inputBuffer.getChannelData(1)]
							});
						}
						input.connect(processor);
						processor.connect(input.context.destination);
					break;
				}
			};
			
			encoderWorker.postMessage({
				"cmd"  : "start",
				"args" : [ac.sampleRate,0.4,0]
			});
			
		},function(e) {
			console.log('No live audio input: ' + e);
			running = false;
		});
	};
	
	function stopRecording() {
		if(!running) { return; };
		encoderWorker.postMessage({ "cmd" : "finish" });
	};
	
	/* DOM callbacks */
	window.onload = function() {
		document.getElementById("start-btn").onclick = startRecording;
		document.getElementById("stop-btn").onclick  = stopRecording;
	};
	
})();