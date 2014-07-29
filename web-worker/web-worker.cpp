#include <emscripten.h>

#include <stdio.h>  // printf
#include <math.h>   // sin
#include <time.h>   // time
#include <stdlib.h> // srand
#include <string.h> // memcpy

#include <vorbis/vorbisenc.h>

struct tEncoderState {
	ogg_stream_state os;
	
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
	ogg_packet op;
	
	int num_channels;
	int sample_rate;
	
	int encoded_max_size;
	int encoded_length;
	
	unsigned char* encoded_buffer;
	
	bool running = false;
};

tEncoderState *state;

extern "C" {

	// write encoded ogg page to a file or buffer
	int vorbisenc_write_page(ogg_page* page) {
		// let's check if the page can fit into the buffer
		if(state->encoded_length + page->header_len + page->body_len <= state->encoded_max_size) {
			// page can fit
			memcpy(state->encoded_buffer + state->encoded_length, page->header, page->header_len);
			state->encoded_length += page->header_len;
			memcpy(state->encoded_buffer + state->encoded_length, page->body, page->body_len);
			state->encoded_length += page->body_len;
		} else if(page->header_len + page->body_len <= state->encoded_max_size) {
			// page can fit but there is not enough space remaining in this buffer
			EM_ASM_INT({
				postMessage({ 'message' : 'buffer', 'data' : HEAPU8.subarray($0,$1) });
			}, state->encoded_buffer, &state->encoded_buffer[state->encoded_length]);
			state->encoded_length = 0;
			vorbisenc_write_page(page);
		} else {
			// page can't fit
			long size = page->header_len + page->body_len + state->encoded_length;
			unsigned char* encoded_buffer = (unsigned char*)malloc(size);
			memcpy(encoded_buffer, state->encoded_buffer, state->encoded_length);
			memcpy(encoded_buffer + state->encoded_length, page->header, page->header_len);
			memcpy(encoded_buffer + state->encoded_length + page->header_len, page->body, page->body_len);
			EM_ASM_INT({
				postMessage({ 'message' : 'buffer', 'data' : HEAPU8.subarray($0,$1) });
			}, encoded_buffer, &encoded_buffer[size]);
			state->encoded_length = 0;
			free(encoded_buffer);
		}
		return 0;
	}

	// preps encoder, allocates output buffer
	// 3145728 = 3 * 1024 * 1024 // max duration. 3 mins = 180 sec @ 128kbit/s = ~3MB
	void vorbisenc_start(int sample_rate = 48000, float vbr_quality = 0.4f, int size = 3145728) {
		if(state) {
			printf("vorbisenc.js : encoder already running\n");
			return;
		}
		
		state = new tEncoderState();
		state->running = true;
		
		srand(time(NULL));
		ogg_stream_init(&state->os, rand());
		
		state->num_channels     = 2;
		state->sample_rate      = sample_rate;
		state->encoded_max_size = size;
		state->encoded_length   = 0;
		
		state->encoded_buffer = (unsigned char*)malloc(state->encoded_max_size);
		
		printf("vorbisenc.js : initializing vorbis encoder with sample_rate = %i Hz and vbr quality = %3.2f\n", state->sample_rate, vbr_quality);
		
		// initialize vorbis
		vorbis_info_init(&state->vi);
		if(vorbis_encode_init_vbr(&state->vi, 2, state->sample_rate, vbr_quality)) {
			printf("vorbisenc.js : error initializing vorbis encoder\n");
			return;
		}
		
		vorbis_comment_init(&state->vc);
		vorbis_comment_add_tag(&state->vc, "ENCODER", "vorbisenc.js");
		
		vorbis_analysis_init(&state->vd, &state->vi);
		vorbis_block_init(&state->vd, &state->vb);
		
		ogg_packet vorbis_header, vorbis_header_comment, vorbis_header_code;
		
		// write out vorbis's headers
		vorbis_analysis_headerout(&state->vd, &state->vc, &vorbis_header, &vorbis_header_comment, &vorbis_header_code);
		
		ogg_stream_packetin(&state->os, &vorbis_header);
		ogg_stream_packetin(&state->os, &vorbis_header_comment);
		ogg_stream_packetin(&state->os, &vorbis_header_code);

		ogg_page og;

		// flush packet into its own page
		while(ogg_stream_flush(&state->os, &og)) {
			vorbisenc_write_page(&og);
		}

		EM_ASM({
			postMessage({ 'message' : 'started' });
		});
	}
	
	// input should be more than 10ms long
	void vorbisenc_write(float* input_buffer_left, float* input_buffer_right, int num_samples) {
		if(!state->running) { return; }
		// get space in which to copy uncompressed data
		float** buffer = vorbis_analysis_buffer(&state->vd, num_samples);

		// copy non-interleaved channels
		for(int i = 0; i < num_samples; i ++) {
			buffer[0][i] = input_buffer_left[i];
			buffer[1][i] = input_buffer_right[i];
		}

		vorbis_analysis_wrote(&state->vd, num_samples);

		ogg_page og;
		int num_packets = 0;

		while(vorbis_analysis_blockout(&state->vd, &state->vb) == 1) {
			vorbis_analysis(&state->vb, NULL);
			vorbis_bitrate_addblock(&state->vb);
			
			while(vorbis_bitrate_flushpacket(&state->vd, &state->op)) {
				// push packet into ogg
				ogg_stream_packetin(&state->os, &state->op);
				num_packets++;
				
				// fetch page from ogg
				while(ogg_stream_pageout(&state->os, &og) || (state->op.e_o_s && ogg_stream_flush(&state->os, &og))) {
					vorbisenc_write_page(&og);
				}
			}
		}

	}

	// finish encoding
	void vorbisenc_finish() {
		if(!state->running) { return; }
		state->running = false;
		
		printf("vorbisenc.js : ending stream\n");
		
		// write an end-of-stream packet
		vorbis_analysis_wrote(&state->vd, 0);
			
		ogg_page og;
			
		while(vorbis_analysis_blockout(&state->vd, &state->vb) == 1) {
			vorbis_analysis(&state->vb, NULL);
			vorbis_bitrate_addblock(&state->vb);
			
			while(vorbis_bitrate_flushpacket(&state->vd, &state->op)) {
				ogg_stream_packetin(&state->os, &state->op);
				
				while(ogg_stream_flush(&state->os, &og)) vorbisenc_write_page(&og);
			}
		}

		printf("vorbisenc.js : cleaning up\n");
		
		free(state->encoded_buffer);
		
		ogg_stream_clear(&state->os);
		vorbis_block_clear(&state->vb);
		vorbis_dsp_clear(&state->vd);
		vorbis_comment_clear(&state->vc);
		vorbis_info_clear(&state->vi);
		
		delete state;
		state = NULL;
		
		EM_ASM({
			postMessage({ 'message' : 'finished' });
		});
	}

}