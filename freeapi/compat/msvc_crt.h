// Forced include for the historical TUs in the Free API build: the two
// behaviours of the Microsoft C runtime (VC5 LIBC) that the unchanged source
// depends on and that Free API leaves to the host C library.
//
//  * rand/srand: the VC5 linear congruential generator, so level decoration
//    and the recorded demos replay as in the original on every C library.
//  * FILE: CEvent::CopyMission tests a stream's error bit directly
//    (`file->_flag & _IOERR`, what VC5's ferror macro expanded to). glibc,
//    bionic and musl FILE have no such member, so the historical TUs use a
//    stream type that has it, over Free API's fopen (path normalisation).
//
// Same approach as the SDL3 port (port/common/compat/include/port_forced.h,
// port_hist_stdio.h).
#ifndef BLUPI_FREEAPI_MSVC_CRT_H
#define BLUPI_FREEAPI_MSVC_CRT_H

// Free API's windows.h redefines fopen; include it first so that its include
// guard keeps the macros below in force.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
#include <algorithm>   // std::random_shuffle names std::rand
#include <cstdio>
#include <cstdlib>
#endif

struct BlupiMsvcFile
{
	FILE* file;
	int   _flag;     // _IOERR once a read or write failed (VC5 ferror)
};

#define _IOERR 0x0020    // VC5 stdio.h

#ifdef __cplusplus
extern "C" {
#endif
int    blupi_msvc_rand(void);
void   blupi_msvc_srand(unsigned int seed);
struct BlupiMsvcFile* blupi_msvc_fopen(const char* path, const char* mode);
int    blupi_msvc_fclose(struct BlupiMsvcFile* stream);
size_t blupi_msvc_fread(void* buffer, size_t size, size_t count, struct BlupiMsvcFile* stream);
size_t blupi_msvc_fwrite(const void* buffer, size_t size, size_t count, struct BlupiMsvcFile* stream);
int    blupi_msvc_fseek(struct BlupiMsvcFile* stream, long offset, int origin);
#ifdef __cplusplus
}
#endif

#ifndef BLUPI_MSVC_CRT_IMPLEMENTATION
#undef fopen
#define rand    blupi_msvc_rand
#define srand   blupi_msvc_srand
#define FILE    BlupiMsvcFile
#define fopen   blupi_msvc_fopen
#define fclose  blupi_msvc_fclose
#define fread   blupi_msvc_fread
#define fwrite  blupi_msvc_fwrite
#define fseek   blupi_msvc_fseek
#endif

#endif
