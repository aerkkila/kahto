#include <kahto.h>
#include <math.h>

int main() {
	struct kahto_figure *fig = kahto_figure_new();
	struct kahto_text text = {
		.text = "hello, world",
	};
	kahto_add_text(fig, &text);
	kahto_set_wh(fig, 1920, 1080);
	struct kahto_videoargs va = {
		.fps = 30,
		.preset = "fast",
	};
	struct kahto_audioargs aa = {
		.samplerate = 48000,
		.nchannels = 2,
	};
	struct kahto_videohelper *vid = kahto_start_video(fig, "testi.mp4", &va, &aa);

	kahto_draw_figure(vid->fig, vid->canvas, vid->ystride);
	float nsec = 2;
	int end = nsec * va.fps;

	for (int i=0; i<end; i++) {
		fig->texts[0].xy[1] = 0.8*i/end;
		kahto_layout(fig);
		kahto_draw_figure(vid->fig, vid->canvas, vid->ystride);
		kahto_write_videoframe(vid->video, vid->canvas);
	}

	end = nsec * aa.samplerate;
	short *audio[] = {
		malloc(end * 2),
		malloc(end * 2),
	};
	const double pi = 3.14159265358979;
	for (int i=0; i<end; i++) {
		float freq = 440;
		for (int ii=0; ii<2; ii++) {
			float rad_per_s = freq*2*pi;
			float s_per_samp = 1. / aa.samplerate;
			float rad_per_samp = rad_per_s * s_per_samp;
			audio[ii][i] = sin(i*rad_per_samp) * 0x7fff;
			freq = 880;
		}
	}
	kahto_write_audio(vid->video, audio, end);
	free(audio[0]);
	free(audio[1]);

	kahto_end_video(vid);
}
