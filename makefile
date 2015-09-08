
CFLAGS := -g -Wall -I/usr/include -I/usr/local/include -DPIXEL_LUA
LDFLAGS := -L/usr/local/bin -L/usr/bin -llua53 -lopenal32
SHARED := --shared

all : audio.dll

audio.dll : laudio.c stb_vorbis.c readfile.c
	$(CC) $(CFLAGS) $(SHARED) $(LDFLAGS) $^ -o $@

clean :
	-rm audio.dll