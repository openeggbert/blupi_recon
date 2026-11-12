// Microsoft C runtime behaviours for the historical TUs (msvc_crt.h).
#define BLUPI_MSVC_CRT_IMPLEMENTATION
#include "msvc_crt.h"

#include <new>

static unsigned int g_randSeed = 1;

extern "C" void blupi_msvc_srand(unsigned int seed)
{
	g_randSeed = seed;
}

extern "C" int blupi_msvc_rand(void)
{
	g_randSeed = g_randSeed * 214013u + 2531011u;
	return (int)((g_randSeed >> 16) & 0x7fff);
}

extern "C" BlupiMsvcFile* blupi_msvc_fopen(const char* path, const char* mode)
{
	FILE* file = fopen(path, mode);   // free_api_fopen (windows.h)
	if (file == NULL) return NULL;
	BlupiMsvcFile* stream = new (std::nothrow) BlupiMsvcFile{file, 0};
	if (stream == NULL) fclose(file);
	return stream;
}

extern "C" int blupi_msvc_fclose(BlupiMsvcFile* stream)
{
	if (stream == NULL) return EOF;
	int result = fclose(stream->file);
	delete stream;
	return result;
}

extern "C" size_t blupi_msvc_fread(void* buffer, size_t size, size_t count, BlupiMsvcFile* stream)
{
	size_t n = fread(buffer, size, count, stream->file);
	if (ferror(stream->file)) stream->_flag |= _IOERR;
	return n;
}

extern "C" size_t blupi_msvc_fwrite(const void* buffer, size_t size, size_t count, BlupiMsvcFile* stream)
{
	size_t n = fwrite(buffer, size, count, stream->file);
	if (ferror(stream->file)) stream->_flag |= _IOERR;
	return n;
}

extern "C" int blupi_msvc_fseek(BlupiMsvcFile* stream, long offset, int origin)
{
	return fseek(stream->file, offset, origin);
}
