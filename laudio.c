#include "AL/al.h"
#include "AL/alc.h"

#include "readfile.h"

#define WAV_IMPLEMENTATION
#include "wav.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <stdio.h>
#include <stdlib.h>

enum AUDIO_FORMAT {
	MONO8 = 0,
	MONO16 = 1,
	STEREO8 = 2,
	STEREO16 = 3,
};

struct audio {
	ALCdevice *device;
	ALCcontext *context;
};

static struct audio A;

const char *audio_error(void) {
	ALenum err = alGetError();
	if (err != AL_NO_ERROR)
		return alGetString(err);
	return 0;
}

int audio_init(void) {
	A.device = alcOpenDevice(0);
	if (!A.device) return -1;
	A.context = alcCreateContext(A.device, 0);
	if (!A.context) return -1;
	if (alcMakeContextCurrent(A.context) == AL_FALSE)
		return -1;
	return 0;
}

void audio_unload(ALuint buffer) {
	alDeleteBuffers(1, &buffer);
}

void audio_unit(void) {
	alcMakeContextCurrent(0);
	alcDestroyContext(A.context);
	alcCloseDevice(A.device);
}

int audio_load(enum AUDIO_FORMAT t, void *data, int size, int freq) {
	ALuint buffer;
	ALenum fmt;
	alGenBuffers(1, &buffer);
	switch (t) {
		case MONO8:
			fmt = AL_FORMAT_MONO8;
			break;
		case MONO16:
			fmt = AL_FORMAT_MONO16;
			break;
		case STEREO8:
			fmt = AL_FORMAT_STEREO8;
			break;
		case STEREO16:
			fmt = AL_FORMAT_STEREO16;
			break;
	}
	alBufferData(buffer, fmt, (ALshort *)data, (ALsizei)size, (ALsizei)freq);
	ALenum err = alGetError();
	if (err != AL_NO_ERROR) {
		fprintf(stderr, "audio_load: %s\n", alGetString(err));
		return -1;
	}
	return buffer;
}

int audio_loadfile(const char *filename) {
	char *data, *out;
	int size, channel, rate, bits;
	enum AUDIO_FORMAT t;
	data = readfile(filename, &size);
	if (!data) {
		return -1;
	}
	if (strstr(filename, ".wav"))
		size = wav_decode_memory(data, size, &channel, &rate, &bits, &out);
	else if (strstr(filename, ".ogg")) {
		short *s;
		size = stb_vorbis_decode_memory((unsigned char *)data, size, &channel, &rate, &s);
		out = (char *)s;
		size *= channel * sizeof(short);
		bits = 16;
	} else {
		free(data);
		return -1;
	}
	free(data);
	if (channel == 1)
		if (bits == 8) t = MONO8;
		else if (bits == 16) t = MONO16;
		else return -1;
	else if (channel == 2)
		if (bits == 8) t = STEREO8;
		else if (bits == 16) t = STEREO16;
		else return -1;
	else return -1;
	ALuint buffer = audio_load(t, out, size, rate);
	free(out);
	return buffer;
}

int audio_play(ALuint buffer, int loop, float volume, float pitch) {
	ALuint source;
	alGenSources(1, &source);
	alSourcei(source, AL_BUFFER, buffer);
	alSourcef(source, AL_GAIN, volume);
	alSourcef(source, AL_PITCH, pitch);
	if (loop == 1)
		alSourcei(source, AL_LOOPING, AL_TRUE);
	alSourcePlay(source);
	ALenum err = alGetError();
	if (err != AL_NO_ERROR) {
		fprintf(stderr, "audio_play: %s\n", alGetString(err));
		return -1;
	}
	return source;
}

float audio_volume(ALuint source, float volume) {
	if (volume == -1.0f) {
		alGetSourcef(source, AL_GAIN, &volume);
		return volume;
	}
	alSourcef(source, AL_GAIN, (ALfloat)volume);
	return 0;
}

void audio_pause(ALuint source) {
	alSourcePause(source);
}

void audio_stop(ALuint source) {
	alSourceStop(source);
	alSourcei(source, AL_BUFFER, 0);
	alDeleteSources(1, &source);
}

void audio_rewind(ALuint source) {
	alSourceRewind(source);
}

int audio_playing(ALuint source) {
	ALint state;
	alGetSourcei(source, AL_SOURCE_STATE, &state);
	return state == AL_PLAYING;
}


#ifdef PIXEL_LUA

#include "lua.h"
#include "lauxlib.h"

static int lload(lua_State *L) {
	const char *file = luaL_checkstring(L, 1);
	int buffer = audio_loadfile(file);
	if (-1 == buffer) return luaL_error(L, audio_error());
	lua_pushinteger(L, buffer);
	return 1;
}

static int lunload(lua_State *L) {
	int buffer = (int)luaL_checkinteger(L, 1);
	audio_unload(buffer);
	return 0;
}

static int lplay(lua_State *L) {
	int buffer = (int)luaL_checkinteger(L, 1);
	int loop = lua_toboolean(L, 2);
	float volume = luaL_optnumber(L, 3, 1.0f);
	float pitch = luaL_optnumber(L, 4, 1.0f);
	lua_pushinteger(L, audio_play(buffer, loop, volume, pitch));
	return 1;
}

static int lstop(lua_State *L) {
	int source = (int)luaL_checkinteger(L, 1);
	audio_stop(source);
	return 0;
}

static int lrewind(lua_State *L) {
	int source = (int)luaL_checkinteger(L, 1);
	audio_rewind(source);
	return 0;
}

static int lpause(lua_State *L) {
	int source = (int)luaL_checkinteger(L, 1);
	audio_pause(source);
	return 0;
}

static int lvolume(lua_State *L) {
	int source = (int)luaL_checkinteger(L, 1);
	float volume = -1.0f;
	if (lua_gettop(L) > 1) {
		volume = (float)lua_tonumber(L, 2);
	}
	lua_pushnumber(L, audio_volume(source, volume));
	return 1;
}

static int lplaying(lua_State *L) {
	int source = (int)luaL_checkinteger(L, 1);
	lua_pushboolean(L, audio_playing(source));
	return 1;
}

static int lunit(lua_State *L) {
	audio_unit();
	return 0;
}

int luaopen_audio(lua_State *L) {
	luaL_Reg l[] = {
		{"load", lload},
		{"unload", lunload},
		{"play", lplay},
		{"stop", lstop},
		{"pause", lpause},
		{"rewind", lrewind},
		{"volume", lvolume},
		{"playing", lplaying},
		{0, 0},
	};
	if (0 != audio_init()) {
		fprintf(stderr, "audio init err:%s\n", audio_error());
		return luaL_error(L, "audio init failed");
	}
	luaL_newlib(L, l);
	if (luaL_newmetatable(L, "audio")) {
		lua_pushcfunction(L, lunit);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	return 1;
}

#endif // PIXEL_LUA
