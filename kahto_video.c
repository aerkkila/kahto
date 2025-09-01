#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h> // av_opt_set
#include <stdio.h>
#include <err.h>

#define error(fun) fprintf(stderr, "%s (%i): %s, %s\n", __FILE__, __LINE__, fun, strerror(errno))

#define boolfun(ret, fun, ...)	\
	ret = fun(__VA_ARGS__);	\
	if (!ret) return error(#fun), 1

#define negfun(fun, ...)	\
	if (fun(__VA_ARGS__) < 0) return error(#fun), 1

struct kahto_video {
	int w, h, iframe;
	float fps;
	AVFormatContext *fcontext;
	AVCodecContext *ctx;
	AVPacket *packet;
	AVStream *stream;
	AVFrame *frame;
};

int kahto_init_video(struct kahto_video *out, const char *filename, int width, int height, float fps) {
	AVFormatContext *fcontext;
	negfun(avformat_alloc_output_context2, &fcontext, NULL, NULL, filename);

	const AVCodec *boolfun(videocodec, avcodec_find_encoder, AV_CODEC_ID_H264);
	AVCodecContext *boolfun(videoctx, avcodec_alloc_context3, videocodec);
	AVPacket *boolfun(videopacket, av_packet_alloc);
	AVStream *boolfun(videostream, avformat_new_stream, fcontext, NULL);
	AVFrame *boolfun(videoframe, av_frame_alloc);

	videoctx->width = videoframe->width  = width;
	videoctx->height = videoframe->height = height;
	videoctx->pix_fmt = videoframe->format = AV_PIX_FMT_YUV420P;
	videoctx->time_base = videostream->time_base = (AVRational){1, fps};
	videoctx->framerate = videostream->avg_frame_rate = (AVRational){fps, 1};
	av_opt_set(videoctx->priv_data, "preset", "slow", 0);
	videostream->id = fcontext->nb_streams-1;

	negfun(avcodec_open2, videoctx, videocodec, NULL);
	negfun(avcodec_parameters_from_context, videostream->codecpar, videoctx);
	negfun(av_frame_get_buffer, videoframe, 0); // allocates frame->data

	negfun(avio_open, &fcontext->pb, filename, AVIO_FLAG_WRITE);
	negfun(avformat_write_header, fcontext, NULL);

	struct kahto_video video = {
		.w = width,
		.h = height,
		.iframe = 0,
		.fps = fps,
		.fcontext = fcontext,
		.ctx = videoctx,
		.packet = videopacket,
		.stream = videostream,
		.frame = videoframe,
	};
	*out = video;
	return 0;
}

int kahto_destroy_video(struct kahto_video *video) {
	av_write_trailer(video->fcontext);

	av_frame_free(&video->frame);
	av_packet_free(&video->packet);
	avcodec_free_context(&video->ctx);
	negfun(avio_closep, &video->fcontext->pb);
	avformat_free_context(video->fcontext);
	return 0;
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

static int write_frame(struct kahto_video *video, uint32_t *argb) {
	negfun(av_frame_make_writable, video->frame);
	video->frame->pts = video->iframe;

	int w = video->w, h = video->h,
		W = video->frame->linesize[0];
	int w05 = w/2, h05 = h/2;
	uint8_t *luma = video->frame->data[0];
	int ind = 0;
	for (int i=0; i<h; i++) {
		int indW = i * W;
		for (int ii=0; ii<w; ii++)
			luma[indW++] = get_luma(argb[ind++]);
	}
	uint8_t *u = video->frame->data[1],
	*v = video->frame->data[2];
	ind = 0;
	W = video->frame->linesize[1];
	for (int i=0; i<h05; i++) {
		int ind0 = i * 2 * w;
		int ind1 = i * W;
		for (int ii=0; ii<w05; ii++) {
			u[ind1] = get_u(argb[ind0 + ii*2]);
			v[ind1++] = get_v(argb[ind0 + ii*2]);
		}
	}

	return encode(video->fcontext, video->ctx, video->stream, video->frame, video->packet);
}

static int video_async_update(struct kahto_figure *fig, uint32_t *canvas, int ystride, long count, double elapsed) {
	return async_update(fig->async, canvas, ystride);
}

struct kahto_figure* kahto_write_mp4_preserve(struct kahto_figure *fig, const char *name, float fps) {
	fig->wh[0] -= fig->wh[0] % 2; // has to be a multiple of 2
	fig->wh[1] -= fig->wh[1] % 2; // has to be a multiple of 2
	int w = fig->wh[0], h = fig->wh[1];
	uint32_t *argb = malloc(w * h * sizeof(uint32_t));
	kahto_draw(fig, argb, w);
	if (!name)
		name = fig->name;
	mkdir_file(name);

	struct kahto_video video;
	if (kahto_init_video(&video, name, w, h, fps))
		return fig;

	if (!fig->update)
		fig->update = video_async_update;

	long updatecount = -1;
	double interval = 1 / fps;
	while ((++updatecount, fig->update((void*)fig, argb, w, updatecount, updatecount * interval)) >= 0) {
		write_frame(&video, argb);
		video.iframe++;
	}
	encode(video.fcontext, video.ctx, video.stream, NULL, video.packet);
	kahto_destroy_video(&video);

	return fig;
}
