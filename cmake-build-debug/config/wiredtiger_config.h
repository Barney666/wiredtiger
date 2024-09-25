/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#ifndef __WIREDTIGER_CONFIG_H_
#define __WIREDTIGER_CONFIG_H_

/* Define to 1 to pause for debugger attach on failure. */
/* #undef HAVE_ATTACH */

/* LZ4 support automatically loaded. */
/* #undef HAVE_BUILTIN_EXTENSION_LZ4 */

/* Snappy support automatically loaded. */
/* #undef HAVE_BUILTIN_EXTENSION_SNAPPY */

/* ZLIB support automatically loaded. */
/* #undef HAVE_BUILTIN_EXTENSION_ZLIB */

/* ZSTD support automatically loaded. */
/* #undef HAVE_BUILTIN_EXTENSION_ZSTD */

/* libsodium support automatically loaded. */
/* #undef HAVE_BUILTIN_EXTENSION_SODIUM */

/* Define to 1 if you have the `clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 for diagnostic tests. */
#define HAVE_DIAGNOSTIC 1

/* Define to 1 to remove full memory barriers on diagnostic yields. */
/* #undef NON_BARRIER_DIAGNOSTIC_YIELDS */

/* Define to 1 for unit tests. */
/* #undef HAVE_UNITTEST */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `fallocate' function. */
/* #undef HAVE_FALLOCATE */

/* Define to 1 if you have the `fdatasync' function. */
/* #undef HAVE_FDATASYNC */

/* Define to 1 if you have the `ftruncate' function. */
#define HAVE_FTRUNCATE 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `dl' library (-ldl). */
#define HAVE_LIBDL 1

/* Define to 1 if you have the `lz4' library (-llz4). */
#define HAVE_LIBLZ4 1

/* Define to 1 if you have the `memkind' library (-lmemkind). */
/* #undef HAVE_LIBMEMKIND */

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the `rt' library (-lrt). */
/* #undef HAVE_LIBRT */

/* Define to 1 if you have the `snappy' library (-lsnappy). */
/* #undef HAVE_LIBSNAPPY */

/* Define to 1 if the user has explictly enabled TCMalloc builds. */
/* #undef ENABLE_TCMALLOC */

/*
 * To remain compatible with autoconf & scons builds, we
 * define HAVE_LIBTCMALLOC for configuring our sources to actually
 * include the tcmalloc headers, as opposed to the sources
 * using ENABLE_TCMALLOC.
 */
#if defined ENABLE_TCMALLOC
  /* Define to 1 if you have the `tcmalloc' library (-ltcmalloc). */
/* #undef HAVE_LIBTCMALLOC */
#endif

/* Define to 1 if you have the `z' library (-lz). */
#define HAVE_LIBZ 1

/* Define to 1 if you have the `zstd' library (-lzstd). */
#define HAVE_LIBZSTD 1

/* Define to 1 if you have the `sodium' library (-lsodium). */
/* #undef HAVE_LIBSODIUM */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 to disable any crc32 hardware support. */
/* #undef HAVE_NO_CRC32_HARDWARE */

/* Define to 1 if you have the `posix_fadvise' function. */
/* #undef HAVE_POSIX_FADVISE */

/* Define to 1 if you have the `posix_fallocate' function. */
/* #undef HAVE_POSIX_FALLOCATE */

/* Define to 1 if you have the `posix_madvise' function. */
#define HAVE_POSIX_MADVISE 1

/* Define to 1 if `posix_memalign' works. */
#define HAVE_POSIX_MEMALIGN 1

/* Define to 1 if pthread condition variables support monotonic clocks. */
/* #undef HAVE_PTHREAD_COND_MONOTONIC */

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strtouq' function. */
#define HAVE_STRTOUQ 1

/* Define to 1 if you have the `sync_file_range' function. */
/* #undef HAVE_SYNC_FILE_RANGE */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the `timer_create' function. */
/* #undef HAVE_TIMER_CREATE */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <x86intrin.h> header file. */
/* #undef HAVE_X86INTRIN_H */

/* Spinlock type from mutex.h. */
#define SPINLOCK_TYPE SPINLOCK_PTHREAD_MUTEX

/* Define to 1 if the target system is big endian */
/* #undef WORDS_BIGENDIAN */

/* Version number of package */
#define VERSION "11.3.0"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Default alignment of buffers used for I/O. */
#define WT_BUFFER_ALIGNMENT_DEFAULT 0

/* Define to 1 to support standalone build. */
#define WT_STANDALONE_BUILD 1

#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

#endif
