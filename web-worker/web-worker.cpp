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
	
	int packet_id;
	int rate;
	int num_channels;
	int sample_rate;
	int granulepos;
	
	int encoded_max_size;
	int encoded_length;
	
	unsigned char* encoded_buffer;
};

tEncoderState *state;

extern "C" {

	// write encoded ogg page to a file or buffer
	int write_page(ogg_page* page) {
		memcpy(state->encoded_buffer + state->encoded_length, page->header, page->header_len);
		state->encoded_length += page->header_len;

		memcpy(state->encoded_buffer + state->encoded_length, page->body, page->body_len);
		state->encoded_length += page->body_len;

		return 0;
	}

	// preps encoder, allocates output buffer
	void lexy_encoder_start(int sample_rate = 48000, float vbr_quality = 0.4f) {
		state = new tEncoderState();
		state->packet_id = 0;
		state->granulepos = 0;
		
		srand(time(NULL));
		ogg_stream_init(&state->os, rand());
		
		int size, error;
		
		state->num_channels = 2;
		state->sample_rate = sample_rate;
		
		// max duration. 3 mins = 180 sec @ 128kbit/s = ~3MB
		state->encoded_buffer = new unsigned char[3 * 1024 * 1024]; // final encoded-audio buffer
		
		printf("lexy_encoder_start(); initializing vorbis encoder with sample_rate = %i Hz and vbr quality = %3.2f\n", state->sample_rate, vbr_quality);
		
		state->encoded_max_size = 0;
		state->encoded_length = 0;
		
		// initialize vorbis
		vorbis_info_init(&state->vi);
		if(vorbis_encode_init_vbr(&state->vi, 2, state->sample_rate, vbr_quality)) {
			printf("lexy_encoder_start(); error initializing vorbis encoder\n");
			return;
		}
		
		vorbis_comment_init(&state->vc);
		vorbis_comment_add_tag(&state->vc, "ENCODER", "lexy-coder");
		
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
		while(ogg_stream_flush(&state->os, &og))
			write_page(&og);
		
		EM_ASM_INT({
			postMessage({ 'message' : 'buffer', 'data' : HEAPU8.subarray($0,$1) });
		},(int)&state->encoded_buffer[0],(int)&state->encoded_buffer[state->encoded_length]);
		EM_ASM({
			postMessage({ 'message' : 'initialized' });
		});
	}
	
	// input should be more than 10ms long
	void lexy_encoder_write(float* input_buffer_left, float* input_buffer_right, int num_samples) {
		int previous_encoded_length = state->encoded_length;

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
					// printf("lexy_encoder_write(); writing ogg samples page after packet %i\n", num_packets);
					write_page(&og);
				}
			}
		}
		
		if(state->encoded_length > previous_encoded_length) {
			EM_ASM_INT({
				postMessage({ 'message' : 'buffer', 'data' : HEAPU8.subarray($0,$1) });
			},(int)&state->encoded_buffer[previous_encoded_length],(int)&state->encoded_buffer[state->encoded_length]);
		}
	}

	// finish encoding
	void lexy_encoder_finish() {
		printf("lexy_encoder_finish(); ending stream\n");
		
		// write an end-of-stream packet
		vorbis_analysis_wrote(&state->vd, 0);
			
		ogg_page og;
			
		while(vorbis_analysis_blockout(&state->vd, &state->vb) == 1) {
			vorbis_analysis(&state->vb, NULL);
			vorbis_bitrate_addblock(&state->vb);
			
			while(vorbis_bitrate_flushpacket(&state->vd, &state->op)) {
				ogg_stream_packetin(&state->os, &state->op);
				
				while(ogg_stream_flush(&state->os, &og)) write_page(&og);
			}
		}

		printf("lexy_encoder_finish(); cleaning up\n");
			
		ogg_stream_clear(&state->os);
		vorbis_block_clear(&state->vb);
		vorbis_dsp_clear(&state->vd);
		vorbis_comment_clear(&state->vc);
		vorbis_info_clear(&state->vi);
	}

}