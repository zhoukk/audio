

#ifndef WAV_IMPLEMENTATION

int wav_decode_memory(const char *data, int size, int *channel, int *rate, int *bits, char **pbuffer);

#else

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct riff_chunk {
	uint8_t chunk_id[4];
	uint32_t chunk_sz;
	uint8_t format[4];
};

struct format_chunk {
	uint8_t chunk_id[4];
	uint32_t chunk_sz;
	uint16_t format;
	uint16_t channel;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
};

struct data_chunk {
	uint8_t chunk_id[4];
	uint32_t chunk_sz;
};


static uint8_t *read_uint16_t(uint8_t *buff, int *size, uint16_t *o) {
	if (*size < 2) return 0;
	*o = ((uint16_t)(buff[1])) << 8;
	*o |= (uint16_t)(buff[0]);
	*size -= 2;
	return buff+2;
}

static uint8_t *read_uint32_t(uint8_t *buff, int *size, uint32_t *o) {
	if (*size < 4) return 0;
	*o = ((uint32_t)(buff[3])) << 24;
	*o |= ((uint32_t)(buff[2])) << 16;
	*o |= ((uint32_t)(buff[1])) << 8;
	*o |= (uint32_t)(buff[0]);
	*size -= 4;
	return buff+4;
}

static uint8_t *read_buff(uint8_t *buff, int *size, int o_size, uint8_t *o) {
	if (*size < o_size) return 0;
	memcpy(o, buff, o_size);
	*size -= o_size;
	return buff+o_size;
}

static uint8_t *read_riff_chunk(uint8_t *buff, int *size, struct riff_chunk *riff) {
	buff = read_buff(buff, size, 4, riff->chunk_id);
	if (!buff) return 0;
	buff = read_uint32_t(buff, size, &riff->chunk_sz);
	if (!buff) return 0;
	buff = read_buff(buff, size, 4, riff->format);
	if (!buff) return 0;
	if (memcmp(riff->chunk_id, "RIFF", 4) ||
		memcmp(riff->format, "WAVE", 4))
		return 0;
	return buff;
}

static uint8_t *read_format_chunk(uint8_t *buff, int *size, struct format_chunk *format) {
	buff = read_buff(buff, size, 4, format->chunk_id);
	if (!buff) return 0;
	buff = read_uint32_t(buff, size, &format->chunk_sz);
	if (!buff) return 0;
	buff = read_uint16_t(buff, size, &format->format);
	if (!buff) return 0;
	buff = read_uint16_t(buff, size, &format->channel);
	if (!buff) return 0;
	buff = read_uint32_t(buff, size, &format->sample_rate);
	if (!buff) return 0;
	buff = read_uint32_t(buff, size, &format->byte_rate);
	if (!buff) return 0;
	buff = read_uint16_t(buff, size, &format->block_align);
	if (!buff) return 0;
	buff = read_uint16_t(buff, size, &format->bits_per_sample);
	if (!buff) return 0;
	if (memcmp(format->chunk_id, "fmt ", 4))
		return 0;
	return buff;
}

static uint8_t *read_data_chunk(uint8_t *buff, int *size, struct data_chunk *data) {
	buff = read_buff(buff, size, 4, data->chunk_id);
	if (!buff) return 0;
	buff = read_uint32_t(buff, size, &data->chunk_sz);
	return buff;
}

int wav_decode_memory(const char *data, int size, int *channel, int *rate, int *bits, char **pbuffer) {
	uint8_t *buffer;
	uint8_t *buff = (uint8_t *)data;
	struct riff_chunk riff_chunk;
	buff = read_riff_chunk(buff, &size, &riff_chunk);
	if (!buff) return -1;

	struct format_chunk format_chunk;
	buff = read_format_chunk(buff, &size, &format_chunk);
	if (!buff) return -1;

	*channel = format_chunk.channel;
	*rate = format_chunk.sample_rate;
	*bits = format_chunk.bits_per_sample;

	struct data_chunk data_chunk;
	for (;;) {
		buff = read_data_chunk(buff, &size, &data_chunk);
		if (!buff) return -1;
		if (memcmp(data_chunk.chunk_id, "data", 4))
			buff += data_chunk.chunk_sz;
		else break;
	}
	buffer = malloc(data_chunk.chunk_sz);
	buff = read_buff(buff, &size, data_chunk.chunk_sz, buffer);
	if (!buff) {
		free(buffer);
		return -1;
	}
	*pbuffer = (char *)buffer;
	return data_chunk.chunk_sz;
}

#endif // WAV_IMPLEMENTATION