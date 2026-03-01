LDLIBS += -lkahto -lm
CFLAGS += -gdwarf-2 -g3

animation: animation.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS) -lkahto-ffmpeg

animation_async: animation_async.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS) -lkahto-ffmpeg

animation_sync: animation_sync.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS) -lkahto-ffmpeg

clean:
	find ! -regex ".*\.[^/]+" -perm -700 -type f -delete
