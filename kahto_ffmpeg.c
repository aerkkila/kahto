#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h> // av_opt_set
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <pthread.h>
#include "kahto.h"
#include "kahto_mkdir.c"
#define include_async_staticonly
#include "kahto_async.c"

#define error(fun) fprintf(stderr, "%s (%i): %s, %s\n", __FILE__, __LINE__, fun, strerror(errno))

#define boolfun(ret, fun, ...)	\
	ret = fun(__VA_ARGS__);	\
	if (!ret) return error(#fun), 1

#define negfun(fun, ...)	\
	if (fun(__VA_ARGS__) < 0) return error(#fun), 1

struct kahto_avstream {
	AVCodecContext *ctx;
	AVPacket *packet;
	AVStream *stream;
	AVFrame *frame;
};

struct kahto_video {
	int w, h, iframe /*video*/, samplerate, audiomem;
	char owner, nchannels;
	float fps;
	long isample; // audio
	AVFormatContext *fcontext;
	struct kahto_avstream video, audio;
};

static int init_video(AVFormatContext *fcontext, struct kahto_videoargs *args, struct kahto_video *out) {
	const AVCodec *boolfun(videocodec, avcodec_find_encoder, AV_CODEC_ID_H264);
	AVCodecContext *boolfun(videoctx, avcodec_alloc_context3, videocodec);
	AVPacket *boolfun(videopacket, av_packet_alloc);
	AVStream *boolfun(videostream, avformat_new_stream, fcontext, NULL);
	AVFrame *boolfun(videoframe, av_frame_alloc);

	videoctx->width = videoframe->width = args->w;
	videoctx->height = videoframe->height = args->h;
	videoctx->pix_fmt = videoframe->format = AV_PIX_FMT_YUV420P;
	videoctx->time_base = videostream->time_base = (AVRational){1, args->fps};
	videoctx->framerate = videostream->avg_frame_rate = (AVRational){args->fps, 1};
	av_opt_set(videoctx->priv_data, "preset", args->preset ? args->preset : "slow", 0);
	videostream->id = fcontext->nb_streams-1;

	negfun(avcodec_open2, videoctx, videocodec, NULL);
	negfun(avcodec_parameters_from_context, videostream->codecpar, videoctx);
	negfun(av_frame_get_buffer, videoframe, 0); // allocates frame->data

	out->w = args->w;
	out->h = args->h;
	out->fps = args->fps;
	out->fcontext = fcontext;
	out->video = (struct kahto_avstream) {
		.ctx = videoctx,
		.packet = videopacket,
		.stream = videostream,
		.frame = videoframe,
	};
	return 0;
}

static int init_audio(AVFormatContext *fcontext, struct kahto_audioargs *args, struct kahto_video *out) {
	const AVCodec *boolfun(audiocodec, avcodec_find_encoder, AV_CODEC_ID_MP3);
	AVCodecContext *boolfun(audioctx, avcodec_alloc_context3, audiocodec);
	AVPacket *boolfun(audiopacket, av_packet_alloc);
	AVStream *boolfun(audiostream, avformat_new_stream, fcontext, NULL);
	AVFrame *boolfun(audioframe, av_frame_alloc);

	audioctx->sample_fmt = audioframe->format = AV_SAMPLE_FMT_S16P;
	audioctx->bit_rate = args->bitrate ? args->bitrate : 128000;
	audioctx->sample_rate = audioframe->sample_rate = args->samplerate ? args->samplerate : 48000;
	audioctx->time_base = audiostream->time_base = (AVRational){1, audioctx->sample_rate};
	AVChannelLayout layout = args->nchannels == 2 ?
		(typeof(layout))AV_CHANNEL_LAYOUT_STEREO :
		(typeof(layout))AV_CHANNEL_LAYOUT_MONO;
	av_channel_layout_copy(&audioctx->ch_layout, &layout);
	av_channel_layout_copy(&audioframe->ch_layout, &layout);
	audioframe->pts = 0;

	negfun(avcodec_open2, audioctx, audiocodec, NULL);
	negfun(avcodec_parameters_from_context, audiostream->codecpar, audioctx);
	audioframe->nb_samples = audioctx->frame_size;
	negfun(av_frame_get_buffer, audioframe, 0);

	out->samplerate = audioframe->sample_rate;
	out->fcontext = fcontext;
	out->nchannels = args->nchannels;
	out->audio = (struct kahto_avstream) {
		.ctx = audioctx,
		.packet = audiopacket,
		.stream = audiostream,
		.frame = audioframe,
	};
	return 0;
}

static int kahto_init_video
(struct kahto_video *out, const char *filename,
 struct kahto_videoargs *videoargs, struct kahto_audioargs *audioargs) {
	AVFormatContext *fcontext;
	negfun(avformat_alloc_output_context2, &fcontext, NULL, NULL, filename);

	if (videoargs)
		if (init_video(fcontext, videoargs, out))
			return 1;
	if (audioargs)
		if (init_audio(fcontext, audioargs, out))
			return 1;

	negfun(avio_open, &fcontext->pb, filename, AVIO_FLAG_WRITE);
	negfun(avformat_write_header, fcontext, NULL);
	return 0;
}

struct kahto_videohelper* kahto_start_video
(struct kahto_figure *fig, const char *filename, struct kahto_videoargs *va, struct kahto_audioargs *aa) {
	struct kahto_videohelper *vh = calloc(1, sizeof(*vh));
	vh->lockmem = fig->wh_locked;
	if (va && va->w) fig->wh[0] = va->w;
	if (va && va->h) fig->wh[1] = va->h;

	fig->wh[0] -= fig->wh[0] % 2; // has to be a multiple of 2
	fig->wh[1] -= fig->wh[1] % 2; // has to be a multiple of 2
	kahto_layout(fig); // might change fig->wh
	fig->wh_locked = 1;
	fig->wh[0] += fig->wh[0] % 2; // has to be a multiple of 2
	fig->wh[1] += fig->wh[1] % 2; // has to be a multiple of 2
	if (va) va->w = fig->wh[0];
	if (va) va->h = fig->wh[1];

	vh->canvas = malloc(fig->wh[0] * fig->wh[1] * sizeof(*vh->canvas));
	vh->ystride = fig->wh[0];
	vh->video = kahto_video_new(filename, va, aa);
	vh->fig = fig;
	return vh;
}

void kahto_end_video(struct kahto_videohelper *vh) {
	kahto_destroy_video(vh->video);
	kahto_destroy(vh->fig);
	free(vh->canvas);
	free(vh);
}

static void avstream_destroy(struct kahto_avstream *s) {
	av_frame_free(&s->frame);
	av_packet_free(&s->packet);
	avcodec_free_context(&s->ctx);
}

static int encode(AVFormatContext *fcontext, AVCodecContext *ctx, AVStream *vstream, AVFrame *frame, AVPacket *packet);

static int finish_audio(kahto_video *restrict video) {
	struct kahto_avstream *astream = &video->audio;
	int fullframe = astream->frame->nb_samples;
	int missing = fullframe - video->audiomem;
	for (int ic=0; ic<video->nchannels; ic++) {
		short *dest = (void*)astream->frame->data[ic];
		memset(dest+video->audiomem, 0, missing*sizeof(dest[0]));
	}
	negfun(av_frame_make_writable, astream->frame);
	astream->frame->pts = video->isample;
	encode(video->fcontext, astream->ctx, astream->stream, astream->frame, astream->packet);
	video->isample += fullframe;
	return 0;
}

int kahto_destroy_video(struct kahto_video *video) {
	if (video->audiomem)
		finish_audio(video);
	if (video->isample)
		encode(video->fcontext, video->audio.ctx, video->audio.stream, NULL, video->audio.packet);
	if (video->iframe)
		encode(video->fcontext, video->video.ctx, video->video.stream, NULL, video->video.packet);
	av_write_trailer(video->fcontext);

	if (video->audio.ctx)
		avstream_destroy(&video->audio);
	if (video->video.ctx)
		avstream_destroy(&video->video);

	negfun(avio_closep, &video->fcontext->pb);
	avformat_free_context(video->fcontext);
	if (video->owner)
		free(video);
	return 0;
}

kahto_video* kahto_video_new
(const char *filename, struct kahto_videoargs *videoargs, struct kahto_audioargs *audioargs) {
	kahto_video *video = calloc(sizeof(*video), 1);
	video->owner = 1;
	if (kahto_init_video(video, filename, videoargs, audioargs)) {
		kahto_destroy_video(video);
		return NULL;
	}
	return video;
}

static inline int get_luma_rgb(int r, int g, int b) {
	return (( 66 * r + 129 * g +  25 * b + 128) / 256) +  16;
}

static inline int get_luma(uint32_t argb) {
	unsigned char *c = (void*)&argb;
	return get_luma_rgb(c[2], c[1], c[0]);
}

static inline int get_u(uint32_t argb) {
	int r = argb >> 16 & 0xff;
	int g = argb >> 8 & 0xff;
	int b = argb & 0xff;
	return ((-38 * r -  74 * g + 112 * b + 128) / 256) + 128;
}

static inline int get_v(uint32_t argb) {
	int r = argb >> 16 & 0xff;
	int g = argb >> 8 & 0xff;
	int b = argb & 0xff;
	return ((112 * r -  94 * g -  18 * b + 128) / 256) + 128;
}

static int encode(AVFormatContext *fcontext, AVCodecContext *ctx, AVStream *vstream, AVFrame *frame, AVPacket *packet) {
	negfun(avcodec_send_frame, ctx, frame);
	int ihelp;
	while (1) {
		switch ((ihelp=avcodec_receive_packet(ctx, packet))) {
			case AVERROR(EINVAL):
				return error("codec not opened, or it is a decoder"), 1;
			default:
				fprintf(stderr, "avcodec_receive_packet: %s\n", av_err2str(ihelp));
				return 0;
			case AVERROR(EAGAIN):
			case AVERROR_EOF: return 0;
			case 0: break;
		}
		av_packet_rescale_ts(packet, ctx->time_base, vstream->time_base);
		packet->stream_index = vstream->index;
		negfun(av_interleaved_write_frame, fcontext, packet);
	}
}

int kahto_write_videoframe(kahto_video *video, uint32_t *argb) {
	struct kahto_avstream *vstream = &video->video;
	negfun(av_frame_make_writable, vstream->frame);
	vstream->frame->pts = video->iframe;

	int w = video->w, h = video->h,
		W = vstream->frame->linesize[0];
	int w05 = w/2, h05 = h/2;
	uint8_t *luma = vstream->frame->data[0];
	int ind = 0;
	for (int i=0; i<h; i++) {
		int indW = i * W;
		for (int ii=0; ii<w; ii++)
			luma[indW++] = get_luma(argb[ind++]);
	}
	uint8_t *u = vstream->frame->data[1],
			*v = vstream->frame->data[2];
	ind = 0;
	W = vstream->frame->linesize[1];
	for (int i=0; i<h05; i++) {
		int ind0 = i * 2 * w;
		int ind1 = i * W;
		for (int ii=0; ii<w05; ii++) {
			u[ind1] = get_u(argb[ind0 + ii*2]);
			v[ind1++] = get_v(argb[ind0 + ii*2]);
		}
	}

	++video->iframe;
	return encode(video->fcontext, vstream->ctx, vstream->stream, vstream->frame, vstream->packet);
}

int kahto_write_audio(kahto_video *restrict video, short **indata0, int ndata) {
	struct kahto_avstream *astream = &video->audio;
	short *indata[video->nchannels];
	memcpy(indata, indata0, sizeof(indata));
	int fullframe = astream->frame->nb_samples;
	int missing = fullframe - video->audiomem;

	while (ndata >= missing) {
		for (int ic=0; ic<video->nchannels; ic++) {
			short *dest = (void*)astream->frame->data[ic];
			memcpy(dest + video->audiomem, indata[ic], missing * sizeof(dest[0]));
			indata[ic] += missing;
		}
		ndata -= missing;
		video->audiomem = 0;
		missing = fullframe;

		negfun(av_frame_make_writable, astream->frame);
		astream->frame->pts = video->isample;
		encode(video->fcontext, astream->ctx, astream->stream, astream->frame, astream->packet);
		video->isample += fullframe;
	}

	video->audiomem = ndata;
	for (int ic=0; ic<video->nchannels; ic++) {
		short *data = (void*)astream->frame->data[ic];
		memcpy(data, indata[ic], ndata*sizeof(data[0]));
	}
	return 0;
}

static int video_async_update(struct kahto_figure *fig, uint32_t *canvas, int ystride, long count, double elapsed) {
	return async_update(fig->async, canvas, ystride);
}

struct kahto_figure* kahto_write_mp4_preserve(struct kahto_figure *fig, const char *name, float fps) {
	fig->wh[0] -= fig->wh[0] % 2; // has to be a multiple of 2
	fig->wh[1] -= fig->wh[1] % 2; // has to be a multiple of 2
	kahto_layout(fig); // might change fig->wh
	int lockmem = fig->wh_locked;
	fig->wh_locked = 1;
	fig->wh[0] += fig->wh[0] % 2; // has to be a multiple of 2
	fig->wh[1] += fig->wh[1] % 2; // has to be a multiple of 2
	int w = fig->wh[0], h = fig->wh[1];
	uint32_t *argb = malloc(w * h * sizeof(uint32_t));
	kahto_draw_figures(fig, argb, w);
	if (!name)
		name = fig->name;
	mkdir_file(name);

	struct kahto_video video = {0};
	struct kahto_videoargs videoargs = {
		.w = w,
		.h = h,
		.fps = fps,
	};
	if (kahto_init_video(&video, name, &videoargs, NULL))
		return fig;

	if (!fig->update)
		fig->update = video_async_update;

	long updatecount = -1;
	double interval = 1 / fps;
	do
		kahto_write_videoframe(&video, argb);
	while ((++updatecount, fig->update((void*)fig, argb, w, updatecount, updatecount * interval)) >= 0);
	kahto_destroy_video(&video);
	free(argb);

	fig->wh_locked = lockmem;
	return fig;
}

#undef error
#undef boolfun
#undef negfun

void kahto_write_mp4(struct kahto_figure *fig, const char *name, float fps) {
	kahto_destroy(kahto_write_mp4_preserve(fig, name, fps));
}

static void* async_write_mp4(void *vargs) {
	struct kahto_async *h = vargs;
	kahto_async_unlock_step(h);
	kahto_write_mp4_preserve(h->figure, NULL, h->_fps);
	h->_exit = async_response;
	return NULL;
}

struct kahto_async* kahto_async_write_mp4(struct kahto_figure *fig, const char *name, float fps) {
	struct kahto_async *h = calloc(1, sizeof(*h));
	h->figure = fig;
	h->_fps = fps;
	if (name)
		fig->name = (void*)(intptr_t)name;
	fig->async = h;
	pthread_create(&h->_thread, NULL, async_write_mp4, h);
	return h;
}
