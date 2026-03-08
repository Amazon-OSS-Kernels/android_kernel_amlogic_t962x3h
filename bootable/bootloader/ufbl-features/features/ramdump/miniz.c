/* miniz.c v1.15 - public domain deflate/inflate, zlib-subset, ZIP reading/writing/appending, PNG writing
   See "unlicense" statement at the end of this file.
   Rich Geldreich <richgel99@gmail.com>, last updated Oct. 13, 2013
   Implements RFC 1950: http://www.ietf.org/rfc/rfc1950.txt and RFC 1951: http://www.ietf.org/rfc/rfc1951.txt

   Most API's defined in miniz.c are optional. For example, to disable the archive related functions just define
   MINIZ_NO_ARCHIVE_APIS, or to get rid of all stdio usage define MINIZ_NO_STDIO (see the list below for more macros).

   * Change History
     10/13/13 v1.15 r4 - Interim bugfix release while I work on the next major release with Zip64 support (almost there!):
       - Critical fix for the MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY bug (thanks kahmyong.moon@hp.com) which could cause locate files to not find files. This bug
        would only have occured in earlier versions if you explicitly used this flag, OR if you used mz_zip_extract_archive_file_to_heap() or mz_zip_add_mem_to_archive_file_in_place()
        (which used this flag). If you can't switch to v1.15 but want to fix this bug, just remove the uses of this flag from both helper funcs (and of course don't use the flag).
       - Bugfix in mz_zip_reader_extract_to_mem_no_alloc() from kymoon when pUser_read_buf is not NULL and compressed size is > uncompressed size
       - Fixing mz_zip_reader_extract_*() funcs so they don't try to extract compressed data from directory entries, to account for weird zipfiles which contain zero-size compressed data on dir entries.
         Hopefully this fix won't cause any issues on weird zip archives, because it assumes the low 16-bits of zip external attributes are DOS attributes (which I believe they always are in practice).
       - Fixing mz_zip_reader_is_file_a_directory() so it doesn't check the internal attributes, just the filename and external attributes
       - mz_zip_reader_init_file() - missing MZ_FCLOSE() call if the seek failed
       - Added cmake support for Linux builds which builds all the examples, tested with clang v3.3 and gcc v4.6.
       - Clang fix for tdefl_write_image_to_png_file_in_memory() from toffaletti
       - Merged MZ_FORCEINLINE fix from hdeanclark
       - Fix <time.h> include before config #ifdef, thanks emil.brink
       - Added tdefl_write_image_to_png_file_in_memory_ex(): supports Y flipping (super useful for OpenGL apps), and explicit control over the compression level (so you can
        set it to 1 for real-time compression).
       - Merged in some compiler fixes from paulharris's github repro.
       - Retested this build under Windows (VS 2010, including static analysis), tcc  0.9.26, gcc v4.6 and clang v3.3.
       - Added example6.c, which dumps an image of the mandelbrot set to a PNG file.
       - Modified example2 to help test the MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY flag more.
       - In r3: Bugfix to mz_zip_writer_add_file() found during merge: Fix possible src file fclose() leak if alignment bytes+local header file write faiiled
       - In r4: Minor bugfix to mz_zip_writer_add_from_zip_reader(): Was pushing the wrong central dir header offset, appears harmless in this release, but it became a problem in the zip64 branch
     5/20/12 v1.14 - MinGW32/64 GCC 4.6.1 compiler fixes: added MZ_FORCEINLINE, #include <time.h> (thanks fermtect).
     5/19/12 v1.13 - From jason@cornsyrup.org and kelwert@mtu.edu - Fix mz_crc32() so it doesn't compute the wrong CRC-32's when mz_ulong is 64-bit.
       - Temporarily/locally slammed in "typedef unsigned long mz_ulong" and re-ran a randomized regression test on ~500k files.
       - Eliminated a bunch of warnings when compiling with GCC 32-bit/64.
       - Ran all examples, miniz.c, and tinfl.c through MSVC 2008's /analyze (static analysis) option and fixed all warnings (except for the silly
        "Use of the comma-operator in a tested expression.." analysis warning, which I purposely use to work around a MSVC compiler warning).
       - Created 32-bit and 64-bit Codeblocks projects/workspace. Built and tested Linux executables. The codeblocks workspace is compatible with Linux+Win32/x64.
       - Added miniz_tester solution/project, which is a useful little app derived from LZHAM's tester app that I use as part of the regression test.
       - Ran miniz.c and tinfl.c through another series of regression testing on ~500,000 files and archives.
       - Modified example5.c so it purposely disables a bunch of high-level functionality (MINIZ_NO_STDIO, etc.). (Thanks to corysama for the MINIZ_NO_STDIO bug report.)
       - Fix ftell() usage in examples so they exit with an error on files which are too large (a limitation of the examples, not miniz itself).
     4/12/12 v1.12 - More comments, added low-level example5.c, fixed a couple minor level_and_flags issues in the archive API's.
      level_and_flags can now be set to MZ_DEFAULT_COMPRESSION. Thanks to Bruce Dawson <bruced@valvesoftware.com> for the feedback/bug report.
     5/28/11 v1.11 - Added statement from unlicense.org
     5/27/11 v1.10 - Substantial compressor optimizations:
      - Level 1 is now ~4x faster than before. The L1 compressor's throughput now varies between 70-110MB/sec. on a
      - Core i7 (actual throughput varies depending on the type of data, and x64 vs. x86).
      - Improved baseline L2-L9 compression perf. Also, greatly improved compression perf. issues on some file types.
      - Refactored the compression code for better readability and maintainability.
      - Added level 10 compression level (L10 has slightly better ratio than level 9, but could have a potentially large
       drop in throughput on some files).
     5/15/11 v1.09 - Initial stable release.

   * Low-level Deflate/Inflate implementation notes:

     Compression: Use the "tdefl" API's. The compressor supports raw, static, and dynamic blocks, lazy or
     greedy parsing, match length filtering, RLE-only, and Huffman-only streams. It performs and compresses
     approximately as well as zlib.

     Decompression: Use the "tinfl" API's. The entire decompressor is implemented as a single function
     coroutine: see tinfl_decompress(). It supports decompression into a 32KB (or larger power of 2) wrapping buffer, or into a memory
     block large enough to hold the entire file.

     The low-level tdefl/tinfl API's do not make any use of dynamic memory allocation.

   * zlib-style API notes:

     miniz.c implements a fairly large subset of zlib. There's enough functionality present for it to be a drop-in
     zlib replacement in many apps:
        The z_stream struct, optional memory allocation callbacks
        deflateInit/deflateInit2/deflate/deflateReset/deflateEnd/deflateBound
        inflateInit/inflateInit2/inflate/inflateEnd
        compress, compress2, compressBound, uncompress
        CRC-32, Adler-32 - Using modern, minimal code size, CPU cache friendly routines.
        Supports raw deflate streams or standard zlib streams with adler-32 checking.

     Limitations:
      The callback API's are not implemented yet. No support for gzip headers or zlib static dictionaries.
      I've tried to closely emulate zlib's various flavors of stream flushing and return status codes, but
      there are no guarantees that miniz.c pulls this off perfectly.

   * PNG writing: See the tdefl_write_image_to_png_file_in_memory() function, originally written by
     Alex Evans. Supports 1-4 bytes/pixel images.

   * ZIP archive API notes:

     The ZIP archive API's where designed with simplicity and efficiency in mind, with just enough abstraction to
     get the job done with minimal fuss. There are simple API's to retrieve file information, read files from
     existing archives, create new archives, append new files to existing archives, or clone archive data from
     one archive to another. It supports archives located in memory or the heap, on disk (using stdio.h),
     or you can specify custom file read/write callbacks.

     - Archive reading: Just call this function to read a single file from a disk archive:

      void *mz_zip_extract_archive_file_to_heap(const char *pZip_filename, const char *pArchive_name,
        size_t *pSize, mz_uint zip_flags);

     For more complex cases, use the "mz_zip_reader" functions. Upon opening an archive, the entire central
     directory is located and read as-is into memory, and subsequent file access only occurs when reading individual files.

     - Archives file scanning: The simple way is to use this function to scan a loaded archive for a specific file:

     int mz_zip_reader_locate_file(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags);

     The locate operation can optionally check file comments too, which (as one example) can be used to identify
     multiple versions of the same file in an archive. This function uses a simple linear search through the central
     directory, so it's not very fast.

     Alternately, you can iterate through all the files in an archive (using mz_zip_reader_get_num_files()) and
     retrieve detailed info on each file by calling mz_zip_reader_file_stat().

     - Archive creation: Use the "mz_zip_writer" functions. The ZIP writer immediately writes compressed file data
     to disk and builds an exact image of the central directory in memory. The central directory image is written
     all at once at the end of the archive file when the archive is finalized.

     The archive writer can optionally align each file's local header and file data to any power of 2 alignment,
     which can be useful when the archive will be read from optical media. Also, the writer supports placing
     arbitrary data blobs at the very beginning of ZIP archives. Archives written using either feature are still
     readable by any ZIP tool.

     - Archive appending: The simple way to add a single file to an archive is to call this function:

      mz_bool mz_zip_add_mem_to_archive_file_in_place(const char *pZip_filename, const char *pArchive_name,
        const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags);

     The archive will be created if it doesn't already exist, otherwise it'll be appended to.
     Note the appending is done in-place and is not an atomic operation, so if something goes wrong
     during the operation it's possible the archive could be left without a central directory (although the local
     file headers and file data will be fine, so the archive will be recoverable).

     For more complex archive modification scenarios:
     1. The safest way is to use a mz_zip_reader to read the existing archive, cloning only those bits you want to
     preserve into a new archive using using the mz_zip_writer_add_from_zip_reader() function (which compiles the
     compressed file data as-is). When you're done, delete the old archive and rename the newly written archive, and
     you're done. This is safe but requires a bunch of temporary disk space or heap memory.

     2. Or, you can convert an mz_zip_reader in-place to an mz_zip_writer using mz_zip_writer_init_from_reader(),
     append new files as needed, then finalize the archive which will write an updated central directory to the
     original archive. (This is basically what mz_zip_add_mem_to_archive_file_in_place() does.) There's a
     possibility that the archive's central directory could be lost with this method if anything goes wrong, though.

     - ZIP archive support limitations:
     No zip64 or spanning support. Extraction functions can only handle unencrypted, stored or deflated files.
     Requires streams capable of seeking.

   * This is a header file library, like stb_image.c. To get only a header file, either cut and paste the
     below header, or create miniz.h, #define MINIZ_HEADER_FILE_ONLY, and then include miniz.c from it.

   * Important: For best perf. be sure to customize the below macros for your target platform:
     #define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
     #define MINIZ_LITTLE_ENDIAN 1
     #define MINIZ_HAS_64BIT_REGISTERS 1

   * On platforms using glibc, Be sure to "#define _LARGEFILE64_SOURCE 1" before including miniz.c to ensure miniz
     uses the 64-bit variants: fopen64(), stat64(), etc. Otherwise you won't be able to process large files
     (i.e. 32-bit stat() fails for me on files > 0x7FFFFFFF bytes).
*/

/*
   This code is modified by Wei Chen to fit into bootloader usage scenarios. The functionalities to deal with zip
   files are all stripped out. Only the core compression and uncompression operations are kept.

   Wei Chen  <weichew@amazon.com>, last updated Oct. 29, 2014
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <debug.h>
#include "miniz.h"

extern void mtk_wdt_restart(void);

// Purposely making these tables static for faster init and thread safety.
static const mz_uint16 s_tdefl_len_sym[256] = {
    257,258,259,260,261,262,263,264,265,265,266,266,267,267,268,268,269,269,269,269,270,270,270,270,271,271,271,271,272,272,272,272,
    273,273,273,273,273,273,273,273,274,274,274,274,274,274,274,274,275,275,275,275,275,275,275,275,276,276,276,276,276,276,276,276,
    277,277,277,277,277,277,277,277,277,277,277,277,277,277,277,277,278,278,278,278,278,278,278,278,278,278,278,278,278,278,278,278,
    279,279,279,279,279,279,279,279,279,279,279,279,279,279,279,279,280,280,280,280,280,280,280,280,280,280,280,280,280,280,280,280,
    281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,281,
    282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,282,
    283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,283,
    284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,284,285
};


static const mz_uint8 s_tdefl_len_extra[256] = {
    0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,0
};

static const mz_uint8 s_tdefl_small_dist_sym[512] = {
    0,1,2,3,4,4,5,5,6,6,6,6,7,7,7,7,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,
    13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,
    14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,
    14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17
};

static const mz_uint8 s_tdefl_small_dist_extra[512] = {
    0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7
};

static const mz_uint8 s_tdefl_large_dist_sym[128] = {
    0,0,18,19,20,20,21,21,22,22,22,22,23,23,23,23,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,
    26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,
    28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29
};

static const mz_uint8 s_tdefl_large_dist_extra[128] = {
    0,0,8,8,9,9,9,9,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
    13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13
};

static const mz_uint s_tdefl_num_probes[11] = { 0, 1, 6, 32,  16, 32, 128, 256,  512, 768, 1500 };

static mz_uint32 s_base_phyaddr = 0;
static mz_uint32 s_base_size_limit = 0;

static void *def_alloc_func(void *opaque, size_t items, void *base, size_t size) {
    void *p;
    (void)opaque, (void)items, (void)base, (void)size;
    if(size > s_base_size_limit) {
        return NULL;
    }
    p = base;
    memset(p, 0, size);
    return p;
}

static void def_free_func(void *opaque, void *address) {
    (void)opaque, (void)address;
    // no action needed since we are not using any heap
}

#define TDEFL_PUT_BITS(b, l) do { \
    mz_uint bits = b; mz_uint len = l; \
    d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; \
    while (d->m_bits_in >= 8) { \
        if (d->m_pOutput_buf < d->m_pOutput_buf_end) \
            *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); \
        d->m_bit_buffer >>= 8; \
        d->m_bits_in -= 8; \
    } \
} while(0)

#define TDEFL_RLE_PREV_CODE_SIZE() { if (rle_repeat_count) { \
    if (rle_repeat_count < 3) { \
        d->m_huff_count[2][prev_code_size] = (mz_uint16)(d->m_huff_count[2][prev_code_size] + rle_repeat_count); \
        while (rle_repeat_count--) packed_code_sizes[num_packed_code_sizes++] = prev_code_size; \
    } else { \
        d->m_huff_count[2][16] = (mz_uint16)(d->m_huff_count[2][16] + 1); packed_code_sizes[num_packed_code_sizes++] = 16; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_repeat_count - 3); \
} rle_repeat_count = 0; } }

#define TDEFL_RLE_ZERO_CODE_SIZE() { if (rle_z_count) { \
  if (rle_z_count < 3) { \
    d->m_huff_count[2][0] = (mz_uint16)(d->m_huff_count[2][0] + rle_z_count); while (rle_z_count--) packed_code_sizes[num_packed_code_sizes++] = 0; \
  } else if (rle_z_count <= 10) { \
    d->m_huff_count[2][17] = (mz_uint16)(d->m_huff_count[2][17] + 1); packed_code_sizes[num_packed_code_sizes++] = 17; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 3); \
  } else { \
    d->m_huff_count[2][18] = (mz_uint16)(d->m_huff_count[2][18] + 1); packed_code_sizes[num_packed_code_sizes++] = 18; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 11); \
} rle_z_count = 0; } }


mz_ulong mz_adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len) {
    mz_uint32 i, s1 = (mz_uint32)(adler & 0xffff), s2 = (mz_uint32)(adler >> 16);
    size_t block_len = buf_len % 5552;

    if (!ptr) return MZ_ADLER32_INIT;
    while (buf_len) {
        for (i = 0; i + 7 < block_len; i += 8, ptr += 8) {
            s1 += ptr[0], s2 += s1;
            s1 += ptr[1], s2 += s1;
            s1 += ptr[2], s2 += s1;
            s1 += ptr[3], s2 += s1;
            s1 += ptr[4], s2 += s1;
            s1 += ptr[5], s2 += s1;
            s1 += ptr[6], s2 += s1;
            s1 += ptr[7], s2 += s1;
        }

        for ( ; i < block_len; ++i) s1 += *ptr++, s2 += s1;
        s1 %= 65521U, s2 %= 65521U;
        buf_len -= block_len;
        block_len = 5552;
    }
    return (s2 << 16) + s1;
}

tdefl_status tdefl_init(tdefl_compressor *d, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags) {
    d->m_pPut_buf_func = pPut_buf_func;
    d->m_pPut_buf_user = pPut_buf_user;
    d->m_flags = (mz_uint)(flags);
    d->m_max_probes[0] = 1 + ((flags & 0xFFF) + 2) / 3;
    d->m_greedy_parsing = (flags & TDEFL_GREEDY_PARSING_FLAG) != 0;
    d->m_max_probes[1] = 1 + (((flags & 0xFFF) >> 2) + 2) / 3;

    if (!(flags & TDEFL_NONDETERMINISTIC_PARSING_FLAG))
        MZ_CLEAR_OBJ(d->m_hash);

    d->m_lookahead_pos = 0;
    d->m_lookahead_size = 0;
    d->m_dict_size = 0;
    d->m_total_lz_bytes = 0;
    d->m_lz_code_buf_dict_pos = 0;
    d->m_bits_in = 0;
    d->m_output_flush_ofs = 0;
    d->m_output_flush_remaining = 0;
    d->m_finished = 0;
    d->m_block_index = 0;
    d->m_bit_buffer = 0;
    d->m_wants_to_finish = 0;
    d->m_pLZ_code_buf = d->m_lz_code_buf + 1;
    d->m_pLZ_flags = d->m_lz_code_buf;
    d->m_num_flags_left = 8;
    d->m_pOutput_buf = d->m_output_buf;
    d->m_pOutput_buf_end = d->m_output_buf;
    d->m_prev_return_status = TDEFL_STATUS_OKAY;
    d->m_saved_match_dist = 0;
    d->m_saved_match_len = 0;
    d->m_saved_lit = 0;
    d->m_adler32 = 1;
    d->m_pIn_buf = NULL;
    d->m_pOut_buf = NULL;
    d->m_pIn_buf_size = NULL;
    d->m_pOut_buf_size = NULL;
    d->m_flush = TDEFL_NO_FLUSH;
    d->m_pSrc = NULL;
    d->m_src_buf_left = 0;
    d->m_out_buf_ofs = 0;

    memset(&d->m_huff_count[0][0], 0, sizeof(d->m_huff_count[0][0]) * TDEFL_MAX_HUFF_SYMBOLS_0);
    memset(&d->m_huff_count[1][0], 0, sizeof(d->m_huff_count[1][0]) * TDEFL_MAX_HUFF_SYMBOLS_1);

    return TDEFL_STATUS_OKAY;
}

tdefl_status tdefl_get_prev_return_status(tdefl_compressor *d) {
    return d->m_prev_return_status;
}

mz_uint32 tdefl_get_adler32(tdefl_compressor *d) {
    return d->m_adler32;
}

// Radix sorts tdefl_sym_freq[] array by 16-bit key m_key. Returns ptr to sorted values.
typedef struct {
    mz_uint16 m_key,
    m_sym_index;
} tdefl_sym_freq;

static tdefl_sym_freq* tdefl_radix_sort_syms(mz_uint num_syms, tdefl_sym_freq* pSyms0, tdefl_sym_freq* pSyms1) {
    mz_uint32 total_passes = 2, pass_shift, pass, i, hist[256 * 2];
    tdefl_sym_freq* pCur_syms = pSyms0, *pNew_syms = pSyms1;
    tdefl_sym_freq* t;
    MZ_CLEAR_OBJ(hist);

    for (i = 0; i < num_syms; i++) {
        mz_uint freq = pSyms0[i].m_key;
        hist[freq & 0xFF]++;
        hist[256 + ((freq >> 8) & 0xFF)]++;
    }
    while ((total_passes > 1) && (num_syms == hist[(total_passes - 1) * 256]))
        total_passes--;

    for (pass_shift = 0, pass = 0; pass < total_passes; pass++, pass_shift += 8) {
        const mz_uint32* pHist = &hist[pass << 8];
        mz_uint offsets[256], cur_ofs = 0;
        for (i = 0; i < 256; i++) {
            offsets[i] = cur_ofs;
            cur_ofs += pHist[i];
        }
        for (i = 0; i < num_syms; i++)
            pNew_syms[offsets[(pCur_syms[i].m_key >> pass_shift) & 0xFF]++] = pCur_syms[i];
        t = pCur_syms;
        pCur_syms = pNew_syms; pNew_syms = t;
    }
    return pCur_syms;
}

// tdefl_calculate_minimum_redundancy() originally written by: Alistair Moffat, alistair@cs.mu.oz.au, Jyrki Katajainen, jyrki@diku.dk, November 1996.
static void tdefl_calculate_minimum_redundancy(tdefl_sym_freq *A, int n) {
    int root, leaf, next, avbl, used, dpth;
    if (n==0) return;
    else if (n==1) {
        A[0].m_key = 1;
        return;
    }

    A[0].m_key += A[1].m_key;
    root = 0;
    leaf = 2;
    for (next=1; next < n-1; next++) {
        if (leaf>=n || A[root].m_key<A[leaf].m_key) {
            A[next].m_key = A[root].m_key;
            A[root++].m_key = (mz_uint16)next;
        }
        else
            A[next].m_key = A[leaf++].m_key;

        if (leaf>=n || (root<next && A[root].m_key<A[leaf].m_key)) {
            A[next].m_key = (mz_uint16)(A[next].m_key + A[root].m_key);
            A[root++].m_key = (mz_uint16)next;
        } else
            A[next].m_key = (mz_uint16)(A[next].m_key + A[leaf++].m_key);
    }
    A[n-2].m_key = 0;
    for (next=n-3; next>=0; next--)
        A[next].m_key = A[A[next].m_key].m_key+1;

    avbl = 1;
    used = dpth = 0;
    root = n-2;
    next = n-1;

    while (avbl>0) {
        while (root>=0 && (int)A[root].m_key==dpth) {
            used++;
            root--;
        }
        while (avbl>used) {
            A[next--].m_key = (mz_uint16)(dpth);
            avbl--;
        }
        avbl = 2*used;
        dpth++;
        used = 0;
    }
}

// Limits canonical Huffman code table's max code size.
enum {
    TDEFL_MAX_SUPPORTED_HUFF_CODESIZE = 32
};

static void tdefl_huffman_enforce_max_code_size(int *pNum_codes, int code_list_len, int max_code_size) {
    int i;
    mz_uint32 total = 0;
    if (code_list_len <= 1) return;
    for (i = max_code_size + 1; i <= TDEFL_MAX_SUPPORTED_HUFF_CODESIZE; i++)
        pNum_codes[max_code_size] += pNum_codes[i];
    for (i = max_code_size; i > 0; i--)
        total += (((mz_uint32)pNum_codes[i]) << (max_code_size - i));
    while (total != (1UL << max_code_size)) {
        pNum_codes[max_code_size]--;
        for (i = max_code_size - 1; i > 0; i--)
            if (pNum_codes[i]) {
                pNum_codes[i]--;
                pNum_codes[i + 1] += 2;
                break;
            }
        total--;
    }
}

static void tdefl_optimize_huffman_table(tdefl_compressor *d, int table_num, int table_len,
                                       int code_size_limit, int static_table) {
    int i, j, l, num_codes[1 + TDEFL_MAX_SUPPORTED_HUFF_CODESIZE];
    mz_uint next_code[TDEFL_MAX_SUPPORTED_HUFF_CODESIZE + 1];
    MZ_CLEAR_OBJ(num_codes);

    if (static_table) {
        for (i = 0; i < table_len; i++)
            num_codes[d->m_huff_code_sizes[table_num][i]]++;
    }
    else {
        tdefl_sym_freq syms0[TDEFL_MAX_HUFF_SYMBOLS], syms1[TDEFL_MAX_HUFF_SYMBOLS], *pSyms;
        int num_used_syms = 0;
        const mz_uint16 *pSym_count = &d->m_huff_count[table_num][0];

        for (i = 0; i < table_len; i++)
             if (pSym_count[i]) {
                 syms0[num_used_syms].m_key = (mz_uint16)pSym_count[i];
                 syms0[num_used_syms++].m_sym_index = (mz_uint16)i;
             }

        pSyms = tdefl_radix_sort_syms(num_used_syms, syms0, syms1);
        tdefl_calculate_minimum_redundancy(pSyms, num_used_syms);

        for (i = 0; i < num_used_syms; i++)
            num_codes[pSyms[i].m_key]++;

        tdefl_huffman_enforce_max_code_size(num_codes, num_used_syms, code_size_limit);

        MZ_CLEAR_OBJ(d->m_huff_code_sizes[table_num]);
        MZ_CLEAR_OBJ(d->m_huff_codes[table_num]);
        for (i = 1, j = num_used_syms; i <= code_size_limit; i++)
             for (l = num_codes[i]; l > 0; l--)
                  d->m_huff_code_sizes[table_num][pSyms[--j].m_sym_index] = (mz_uint8)(i);
    }

    next_code[1] = 0;
    for (j = 0, i = 2; i <= code_size_limit; i++)
        next_code[i] = j = ((j + num_codes[i - 1]) << 1);

    for (i = 0; i < table_len; i++) {
        mz_uint rev_code = 0, code, code_size;
        if ((code_size = d->m_huff_code_sizes[table_num][i]) == 0)
            continue;
        code = next_code[code_size]++;
        for (l = code_size; l > 0; l--, code >>= 1)
            rev_code = (rev_code << 1) | (code & 1);
        d->m_huff_codes[table_num][i] = (mz_uint16)rev_code;
    }
}

static mz_uint8 s_tdefl_packed_code_size_syms_swizzle[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static void tdefl_start_dynamic_block(tdefl_compressor *d) {
    int num_lit_codes, num_dist_codes, num_bit_lengths; mz_uint i, total_code_sizes_to_pack, num_packed_code_sizes, rle_z_count, rle_repeat_count, packed_code_sizes_index;
    mz_uint8 code_sizes_to_pack[TDEFL_MAX_HUFF_SYMBOLS_0 + TDEFL_MAX_HUFF_SYMBOLS_1], packed_code_sizes[TDEFL_MAX_HUFF_SYMBOLS_0 + TDEFL_MAX_HUFF_SYMBOLS_1], prev_code_size = 0xFF;

    d->m_huff_count[0][256] = 1;

    tdefl_optimize_huffman_table(d, 0, TDEFL_MAX_HUFF_SYMBOLS_0, 15, MZ_FALSE);
    tdefl_optimize_huffman_table(d, 1, TDEFL_MAX_HUFF_SYMBOLS_1, 15, MZ_FALSE);

    for (num_lit_codes = 286; num_lit_codes > 257; num_lit_codes--) if (d->m_huff_code_sizes[0][num_lit_codes - 1]) break;
    for (num_dist_codes = 30; num_dist_codes > 1; num_dist_codes--) if (d->m_huff_code_sizes[1][num_dist_codes - 1]) break;

    memcpy(code_sizes_to_pack, &d->m_huff_code_sizes[0][0], num_lit_codes);
    memcpy(code_sizes_to_pack + num_lit_codes, &d->m_huff_code_sizes[1][0], num_dist_codes);
    total_code_sizes_to_pack = num_lit_codes + num_dist_codes;
    num_packed_code_sizes = 0;
    rle_z_count = 0;
    rle_repeat_count = 0;

    memset(&d->m_huff_count[2][0], 0, sizeof(d->m_huff_count[2][0]) * TDEFL_MAX_HUFF_SYMBOLS_2);
    for (i = 0; i < total_code_sizes_to_pack; i++) {
        mz_uint8 code_size = code_sizes_to_pack[i];
        if (!code_size) {
            TDEFL_RLE_PREV_CODE_SIZE();
            if (++rle_z_count == 138) {
                TDEFL_RLE_ZERO_CODE_SIZE();
            }
        }
        else {
            TDEFL_RLE_ZERO_CODE_SIZE();
            if (code_size != prev_code_size) {
                TDEFL_RLE_PREV_CODE_SIZE();
                d->m_huff_count[2][code_size] = (mz_uint16)(d->m_huff_count[2][code_size] + 1);
                packed_code_sizes[num_packed_code_sizes++] = code_size;
            }
            else if (++rle_repeat_count == 6) {
                TDEFL_RLE_PREV_CODE_SIZE();
            }
        }
        prev_code_size = code_size;
    }
    if (rle_repeat_count) {
        TDEFL_RLE_PREV_CODE_SIZE();
    } else {
        TDEFL_RLE_ZERO_CODE_SIZE();
    }

    tdefl_optimize_huffman_table(d, 2, TDEFL_MAX_HUFF_SYMBOLS_2, 7, MZ_FALSE);

    TDEFL_PUT_BITS(2, 2);
    TDEFL_PUT_BITS(num_lit_codes - 257, 5);
    TDEFL_PUT_BITS(num_dist_codes - 1, 5);

    for (num_bit_lengths = 18; num_bit_lengths >= 0; num_bit_lengths--)
        if (d->m_huff_code_sizes[2][s_tdefl_packed_code_size_syms_swizzle[num_bit_lengths]])
            break;
    num_bit_lengths = MZ_MAX(4, (num_bit_lengths + 1));
    TDEFL_PUT_BITS(num_bit_lengths - 4, 4);
    for (i = 0; (int)i < num_bit_lengths; i++)
        TDEFL_PUT_BITS(d->m_huff_code_sizes[2][s_tdefl_packed_code_size_syms_swizzle[i]], 3);

    for (packed_code_sizes_index = 0; packed_code_sizes_index < num_packed_code_sizes; ) {
        mz_uint code = packed_code_sizes[packed_code_sizes_index++];
        // MZ_ASSERT(code < TDEFL_MAX_HUFF_SYMBOLS_2);
        TDEFL_PUT_BITS(d->m_huff_codes[2][code], d->m_huff_code_sizes[2][code]);
        if (code >= 16)
            TDEFL_PUT_BITS(packed_code_sizes[packed_code_sizes_index++], "\02\03\07"[code - 16]);
    }
}

static void tdefl_start_static_block(tdefl_compressor *d) {
    mz_uint i;
    mz_uint8 *p = &d->m_huff_code_sizes[0][0];

    for (i = 0; i <= 143; ++i) *p++ = 8;
    for ( ; i <= 255; ++i) *p++ = 9;
    for ( ; i <= 279; ++i) *p++ = 7;
    for ( ; i <= 287; ++i) *p++ = 8;

    memset(d->m_huff_code_sizes[1], 5, 32);

    tdefl_optimize_huffman_table(d, 0, 288, 15, MZ_TRUE);
    tdefl_optimize_huffman_table(d, 1, 32, 15, MZ_TRUE);

    TDEFL_PUT_BITS(1, 2);
}

static const mz_uint mz_bitmasks[17] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF, 0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

static mz_bool tdefl_compress_lz_codes(tdefl_compressor *d) {
    mz_uint flags;
    mz_uint8 *pLZ_codes;

    flags = 1;
    for (pLZ_codes = d->m_lz_code_buf; pLZ_codes < d->m_pLZ_code_buf; flags >>= 1) {
        if (flags == 1)
            flags = *pLZ_codes++ | 0x100;
        if (flags & 1) {
            mz_uint sym, num_extra_bits;
            mz_uint match_len = pLZ_codes[0], match_dist = (pLZ_codes[1] | (pLZ_codes[2] << 8));
            pLZ_codes += 3;

            // MZ_ASSERT(d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]);
            TDEFL_PUT_BITS(d->m_huff_codes[0][s_tdefl_len_sym[match_len]], d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]);
            TDEFL_PUT_BITS(match_len & mz_bitmasks[s_tdefl_len_extra[match_len]], s_tdefl_len_extra[match_len]);

            if (match_dist < 512) {
                sym = s_tdefl_small_dist_sym[match_dist]; num_extra_bits = s_tdefl_small_dist_extra[match_dist];
            }
            else {
            sym = s_tdefl_large_dist_sym[match_dist >> 8]; num_extra_bits = s_tdefl_large_dist_extra[match_dist >> 8];
            }
            // MZ_ASSERT(d->m_huff_code_sizes[1][sym]);
            TDEFL_PUT_BITS(d->m_huff_codes[1][sym], d->m_huff_code_sizes[1][sym]);
            TDEFL_PUT_BITS(match_dist & mz_bitmasks[num_extra_bits], num_extra_bits);
        }
        else {
            mz_uint lit = *pLZ_codes++;
            // MZ_ASSERT(d->m_huff_code_sizes[0][lit]);
            TDEFL_PUT_BITS(d->m_huff_codes[0][lit], d->m_huff_code_sizes[0][lit]);
        }
    }

    TDEFL_PUT_BITS(d->m_huff_codes[0][256], d->m_huff_code_sizes[0][256]);

    return (d->m_pOutput_buf < d->m_pOutput_buf_end);
}


static mz_bool tdefl_compress_block(tdefl_compressor *d, mz_bool static_block) {
    if (static_block)
        tdefl_start_static_block(d);
    else
        tdefl_start_dynamic_block(d);

    return tdefl_compress_lz_codes(d);
}

static int tdefl_flush_block(tdefl_compressor *d, int flush) {
    mz_uint saved_bit_buf, saved_bits_in;
    mz_uint8 *pSaved_output_buf;
    mz_bool comp_block_succeeded = MZ_FALSE;
    int n, use_raw_block = ((d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS) != 0) && (d->m_lookahead_pos - d->m_lz_code_buf_dict_pos) <= d->m_dict_size;
    mz_uint8 *pOutput_buf_start = ((d->m_pPut_buf_func == NULL) && ((*d->m_pOut_buf_size - d->m_out_buf_ofs) >= TDEFL_OUT_BUF_SIZE)) ? ((mz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs) : d->m_output_buf;

    d->m_pOutput_buf = pOutput_buf_start;
    d->m_pOutput_buf_end = d->m_pOutput_buf + TDEFL_OUT_BUF_SIZE - 16;

    //MZ_ASSERT(!d->m_output_flush_remaining);

    d->m_output_flush_ofs = 0;
    d->m_output_flush_remaining = 0;
    *d->m_pLZ_flags = (mz_uint8)(*d->m_pLZ_flags >> d->m_num_flags_left);
    d->m_pLZ_code_buf -= (d->m_num_flags_left == 8);

    if ((d->m_flags & TDEFL_WRITE_ZLIB_HEADER) && (!d->m_block_index)) {
        TDEFL_PUT_BITS(0x78, 8); TDEFL_PUT_BITS(0x01, 8);
    }

    TDEFL_PUT_BITS(flush == TDEFL_FINISH, 1);
    pSaved_output_buf = d->m_pOutput_buf;
    saved_bit_buf = d->m_bit_buffer;
    saved_bits_in = d->m_bits_in;

    if (!use_raw_block)
        comp_block_succeeded = tdefl_compress_block(d, (d->m_flags & TDEFL_FORCE_ALL_STATIC_BLOCKS) || (d->m_total_lz_bytes < 48));

    // If the block gets expanded, forget the current contents of the output
    // buffer and send a raw block instead.
    if ( ((use_raw_block) || ((d->m_total_lz_bytes) && ((d->m_pOutput_buf - pSaved_output_buf + 1U) >= d->m_total_lz_bytes))) &&
       ((d->m_lookahead_pos - d->m_lz_code_buf_dict_pos) <= d->m_dict_size) ) {
        mz_uint i;
        d->m_pOutput_buf = pSaved_output_buf;
        d->m_bit_buffer = saved_bit_buf;
        d->m_bits_in = saved_bits_in;

        TDEFL_PUT_BITS(0, 2);

        if (d->m_bits_in) {
            TDEFL_PUT_BITS(0, 8 - d->m_bits_in);
        }

        for (i = 2; i; --i, d->m_total_lz_bytes ^= 0xFFFF) {
            TDEFL_PUT_BITS(d->m_total_lz_bytes & 0xFFFF, 16);
        }

        for (i = 0; i < d->m_total_lz_bytes; ++i) {
            TDEFL_PUT_BITS(d->m_dict[(d->m_lz_code_buf_dict_pos + i)
                                            & TDEFL_LZ_DICT_SIZE_MASK], 8);
        }
    }
    // Check for the extremely unlikely (if not impossible) case of the compressed block not fitting into the output buffer when using dynamic codes.
    else if (!comp_block_succeeded) {
        d->m_pOutput_buf = pSaved_output_buf;
        d->m_bit_buffer = saved_bit_buf,
        d->m_bits_in = saved_bits_in;
        tdefl_compress_block(d, MZ_TRUE);
    }

    if (flush) {
        if (flush == TDEFL_FINISH) {
            if (d->m_bits_in) {
                TDEFL_PUT_BITS(0, 8 - d->m_bits_in);
            }
            if (d->m_flags & TDEFL_WRITE_ZLIB_HEADER) {
                mz_uint i, a = d->m_adler32;
                for (i = 0; i < 4; i++) {
                    TDEFL_PUT_BITS((a >> 24) & 0xFF, 8); a <<= 8;
                }
            }
        }
        else {
            mz_uint i, z = 0;
            TDEFL_PUT_BITS(0, 3);
            if (d->m_bits_in) {
                TDEFL_PUT_BITS(0, 8 - d->m_bits_in);
            }
            for (i = 2; i; --i, z ^= 0xFFFF) {
                TDEFL_PUT_BITS(z & 0xFFFF, 16);
            }
        }
    }

    // MZ_ASSERT(d->m_pOutput_buf < d->m_pOutput_buf_end);
    memset(&d->m_huff_count[0][0], 0,
               sizeof(d->m_huff_count[0][0]) * TDEFL_MAX_HUFF_SYMBOLS_0);
    memset(&d->m_huff_count[1][0], 0,
               sizeof(d->m_huff_count[1][0]) * TDEFL_MAX_HUFF_SYMBOLS_1);

    d->m_pLZ_code_buf = d->m_lz_code_buf + 1;
    d->m_pLZ_flags = d->m_lz_code_buf;
    d->m_num_flags_left = 8;
    d->m_lz_code_buf_dict_pos += d->m_total_lz_bytes;
    d->m_total_lz_bytes = 0;
    d->m_block_index++;

    if ((n = (int)(d->m_pOutput_buf - pOutput_buf_start)) != 0) {
        if (d->m_pPut_buf_func) {
            *d->m_pIn_buf_size = d->m_pSrc - (const mz_uint8 *)d->m_pIn_buf;
             if (!(*d->m_pPut_buf_func)(d->m_output_buf, n, d->m_pPut_buf_user))
                 return (d->m_prev_return_status = TDEFL_STATUS_PUT_BUF_FAILED);
        }
        else if (pOutput_buf_start == d->m_output_buf) {
            int bytes_to_copy = (int)MZ_MIN((size_t)n,
                             (size_t)(*d->m_pOut_buf_size - d->m_out_buf_ofs));

            memcpy((mz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs, d->m_output_buf, bytes_to_copy);
            d->m_out_buf_ofs += bytes_to_copy;

            if ((n -= bytes_to_copy) != 0) {
                d->m_output_flush_ofs = bytes_to_copy;
                d->m_output_flush_remaining = n;
            }
        }
        else {
            d->m_out_buf_ofs += n;
        }
    }

    return d->m_output_flush_remaining;
}


static __inline void tdefl_record_literal(tdefl_compressor *d, mz_uint8 lit) {
    d->m_total_lz_bytes++;
    *d->m_pLZ_code_buf++ = lit;
    *d->m_pLZ_flags = (mz_uint8)(*d->m_pLZ_flags >> 1);
    if (--d->m_num_flags_left == 0) {
        d->m_num_flags_left = 8;
        d->m_pLZ_flags = d->m_pLZ_code_buf++;
    }
    d->m_huff_count[0][lit]++;
}

static __inline void tdefl_record_match(tdefl_compressor *d, mz_uint match_len,
                                                                   mz_uint match_dist) {
    mz_uint32 s0, s1;
    //MZ_ASSERT((match_len >= TDEFL_MIN_MATCH_LEN) && (match_dist >= 1) && (match_dist <= TDEFL_LZ_DICT_SIZE));

    d->m_total_lz_bytes += match_len;
    d->m_pLZ_code_buf[0] = (mz_uint8)(match_len - TDEFL_MIN_MATCH_LEN);
    match_dist -= 1;
    d->m_pLZ_code_buf[1] = (mz_uint8)(match_dist & 0xFF);
    d->m_pLZ_code_buf[2] = (mz_uint8)(match_dist >> 8);
    d->m_pLZ_code_buf += 3;
    *d->m_pLZ_flags = (mz_uint8)((*d->m_pLZ_flags >> 1) | 0x80);
    if (--d->m_num_flags_left == 0) {
        d->m_num_flags_left = 8;
        d->m_pLZ_flags = d->m_pLZ_code_buf++;
    }

    s0 = s_tdefl_small_dist_sym[match_dist & 511];
    s1 = s_tdefl_large_dist_sym[(match_dist >> 8) & 127];
    d->m_huff_count[1][(match_dist < 512) ? s0 : s1]++;

    if (match_len >= TDEFL_MIN_MATCH_LEN)
        d->m_huff_count[0][s_tdefl_len_sym[match_len - TDEFL_MIN_MATCH_LEN]]++;
}

#define TDEFL_READ_UNALIGNED_WORD(p) *(const mz_uint16*)(p)
#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
static inline void tdefl_find_match(tdefl_compressor *d, mz_uint lookahead_pos, mz_uint max_dist, mz_uint max_match_len, mz_uint *pMatch_dist, mz_uint *pMatch_len)
{
  mz_uint dist, pos = lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK, match_len = *pMatch_len, probe_pos = pos, next_probe_pos, probe_len;
  mz_uint num_probes_left = d->m_max_probes[match_len >= 32];
  const mz_uint16 *s = (const mz_uint16*)(d->m_dict + pos), *p, *q, *tr;
  const mz_uint16 *c = (const mz_uint16*)(d->m_dict + pos + match_len - 1);
  mz_uint16 c01 = TDEFL_READ_UNALIGNED_WORD(c);
  mz_uint16 s01 = TDEFL_READ_UNALIGNED_WORD(s);

  // MZ_ASSERT(max_match_len <= TDEFL_MAX_MATCH_LEN);
  if (max_match_len <= match_len) return;
  for ( ; ; )
  {
    for ( ; ; )
    {
      if (--num_probes_left == 0) return;
      #define TDEFL_PROBE \
        next_probe_pos = d->m_next[probe_pos]; \
        if ((!next_probe_pos) || ((dist = (mz_uint16)(lookahead_pos - next_probe_pos)) > max_dist)) return; \
        probe_pos = next_probe_pos & TDEFL_LZ_DICT_SIZE_MASK; \
        tr = (const mz_uint16*)(d->m_dict + probe_pos + match_len -1);	\
        if (TDEFL_READ_UNALIGNED_WORD(tr) == c01) break;
      TDEFL_PROBE; TDEFL_PROBE; TDEFL_PROBE;
    }
    if (!dist) break; q = (const mz_uint16*)(d->m_dict + probe_pos); if (TDEFL_READ_UNALIGNED_WORD(q) != s01) continue; p = s; probe_len = 32;
    do { } while ( (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) && (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) &&
                   (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) && (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) && (--probe_len > 0) );
    if (!probe_len)
    {
      *pMatch_dist = dist; *pMatch_len = MZ_MIN(max_match_len, TDEFL_MAX_MATCH_LEN); break;
    }
    else if ((probe_len = ((mz_uint)(p - s) * 2) + (mz_uint)(*(const mz_uint8*)p == *(const mz_uint8*)q)) > match_len)
    {
      *pMatch_dist = dist; if ((*pMatch_len = match_len = MZ_MIN(max_match_len, probe_len)) == max_match_len) break;
      c = (const mz_uint16*)(d->m_dict + pos + match_len - 1);
      c01 = TDEFL_READ_UNALIGNED_WORD(c);
    }
  }
}
#else

static __inline void tdefl_find_match(tdefl_compressor *d, mz_uint lookahead_pos, mz_uint max_dist, mz_uint max_match_len, mz_uint *pMatch_dist, mz_uint *pMatch_len) {
    mz_uint dist, pos = lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK, match_len = *pMatch_len, probe_pos = pos, next_probe_pos, probe_len;
    mz_uint num_probes_left = d->m_max_probes[match_len >= 32];
    const mz_uint8 *s = d->m_dict + pos, *p, *q;
    mz_uint8 c0 = d->m_dict[pos + match_len], c1 = d->m_dict[pos + match_len - 1];

    // MZ_ASSERT(max_match_len <= TDEFL_MAX_MATCH_LEN);
    if (max_match_len <= match_len) return;
    for ( ; ; ) {
        for ( ; ; ) {
            if (--num_probes_left == 0) return;
            #define TDEFL_PROBE \
                    next_probe_pos = d->m_next[probe_pos]; \
                    if ((!next_probe_pos) || ((dist = (mz_uint16)(lookahead_pos - next_probe_pos)) > max_dist)) return; \
                    probe_pos = next_probe_pos & TDEFL_LZ_DICT_SIZE_MASK; \
                    if ((d->m_dict[probe_pos + match_len] == c0) && (d->m_dict[probe_pos + match_len - 1] == c1)) break;
            TDEFL_PROBE;
            TDEFL_PROBE;
            TDEFL_PROBE;
        }

        if (!dist) break;
        p = s;
        q = d->m_dict + probe_pos;
        for (probe_len = 0; probe_len < max_match_len; probe_len++)
            if (*p++ != *q++) break;

        if (probe_len > match_len) {
            *pMatch_dist = dist;
            if ((*pMatch_len = match_len = probe_len) == max_match_len)
                return;
            c0 = d->m_dict[pos + match_len];
            c1 = d->m_dict[pos + match_len - 1];
        }
    }
}
#endif

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
static mz_bool tdefl_compress_fast(tdefl_compressor *d)
{
  // Faster, minimally featured LZRW1-style match+parse loop with better register utilization. Intended for applications where raw throughput is valued more highly than ratio.
  mz_uint lookahead_pos = d->m_lookahead_pos, lookahead_size = d->m_lookahead_size, dict_size = d->m_dict_size, total_lz_bytes = d->m_total_lz_bytes, num_flags_left = d->m_num_flags_left;
  mz_uint8 *pLZ_code_buf = d->m_pLZ_code_buf, *pLZ_flags = d->m_pLZ_flags;
  mz_uint cur_pos = lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK;
  while ((d->m_src_buf_left) || ((d->m_flush) && (lookahead_size)))
  {
    const mz_uint TDEFL_COMP_FAST_LOOKAHEAD_SIZE = 4096;
    mz_uint dst_pos = (lookahead_pos + lookahead_size) & TDEFL_LZ_DICT_SIZE_MASK;
    mz_uint num_bytes_to_process = (mz_uint)MZ_MIN(d->m_src_buf_left, TDEFL_COMP_FAST_LOOKAHEAD_SIZE - lookahead_size);
    d->m_src_buf_left -= num_bytes_to_process;
    lookahead_size += num_bytes_to_process;

    while (num_bytes_to_process)
    {
      mz_uint32 n = MZ_MIN(TDEFL_LZ_DICT_SIZE - dst_pos, num_bytes_to_process);
      memcpy(d->m_dict + dst_pos, d->m_pSrc, n);
      if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
        memcpy(d->m_dict + TDEFL_LZ_DICT_SIZE + dst_pos, d->m_pSrc, MZ_MIN(n, (TDEFL_MAX_MATCH_LEN - 1) - dst_pos));
      d->m_pSrc += n;
      dst_pos = (dst_pos + n) & TDEFL_LZ_DICT_SIZE_MASK;
      num_bytes_to_process -= n;
    }

    dict_size = MZ_MIN(TDEFL_LZ_DICT_SIZE - lookahead_size, dict_size);
    if ((!d->m_flush) && (lookahead_size < TDEFL_COMP_FAST_LOOKAHEAD_SIZE)) break;

    while (lookahead_size >= 4)
    {
      mz_uint cur_match_dist, cur_match_len = 1;
      mz_uint8 *pCur_dict = d->m_dict + cur_pos;
      // mz_uint first_trigram = (*(const mz_uint32 *)pCur_dict) & 0xFFFFFF;
      mz_uint first_trigram;
      memcpy((mz_uint8 *)&first_trigram, pCur_dict, sizeof(mz_uint32));
      first_trigram = first_trigram & 0xFFFFFF;
      mz_uint hash = (first_trigram ^ (first_trigram >> (24 - (TDEFL_LZ_HASH_BITS - 8)))) & TDEFL_LEVEL1_HASH_SIZE_MASK;
      mz_uint probe_pos = d->m_hash[hash];
      d->m_hash[hash] = (mz_uint16)lookahead_pos;
      mz_uint test;
      memcpy((mz_uint8 *)&test, (mz_uint8 *)(d->m_dict + (probe_pos &= TDEFL_LZ_DICT_SIZE_MASK)), sizeof(mz_uint32));
      // tr = (const mz_uint32 *)(d->m_dict + (probe_pos &= TDEFL_LZ_DICT_SIZE_MASK));
//      if (((cur_match_dist = (mz_uint16)(lookahead_pos - probe_pos)) <= dict_size) && ((*(const mz_uint32 *)(d->m_dict + (probe_pos &= TDEFL_LZ_DICT_SIZE_MASK)) & 0xFFFFFF) == first_trigram))
      if (((cur_match_dist = (mz_uint16)(lookahead_pos - probe_pos)) <= dict_size) && ((test & 0xFFFFFF) == first_trigram))
      {
        const mz_uint16 *p = (const mz_uint16 *)pCur_dict;
        const mz_uint16 *q = (const mz_uint16 *)(d->m_dict + probe_pos);
        mz_uint32 probe_len = 32;
//        do { } while ( (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) && (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) &&
//          (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) && (TDEFL_READ_UNALIGNED_WORD(++p) == TDEFL_READ_UNALIGNED_WORD(++q)) && (--probe_len > 0) );
        do { } while ( !memcmp((mz_uint8 *)(++p), (mz_uint8 *)(++q), sizeof(mz_uint16)) && !memcmp((mz_uint8 *)(++p), (mz_uint8 *)(++q), sizeof(mz_uint16))  &&
          !memcmp((mz_uint8 *)(++p), (mz_uint8 *)(++q), sizeof(mz_uint16))  && !memcmp((mz_uint8 *)(++p), (mz_uint8 *)(++q), sizeof(mz_uint16))  && (--probe_len > 0) );
        cur_match_len = ((mz_uint)(p - (const mz_uint16 *)pCur_dict) * 2) + (mz_uint)(*(const mz_uint8 *)p == *(const mz_uint8 *)q);
        if (!probe_len)
          cur_match_len = cur_match_dist ? TDEFL_MAX_MATCH_LEN : 0;
        if ((cur_match_len < TDEFL_MIN_MATCH_LEN) || ((cur_match_len == TDEFL_MIN_MATCH_LEN) && (cur_match_dist >= 8U*1024U)))
        {
          cur_match_len = 1;
          *pLZ_code_buf++ = (mz_uint8)first_trigram;
          *pLZ_flags = (mz_uint8)(*pLZ_flags >> 1);
          d->m_huff_count[0][(mz_uint8)first_trigram]++;
        }
        else
        {
          mz_uint32 s0, s1;
          cur_match_len = MZ_MIN(cur_match_len, lookahead_size);

          //MZ_ASSERT((cur_match_len >= TDEFL_MIN_MATCH_LEN) && (cur_match_dist >= 1) && (cur_match_dist <= TDEFL_LZ_DICT_SIZE));

          cur_match_dist--;

          pLZ_code_buf[0] = (mz_uint8)(cur_match_len - TDEFL_MIN_MATCH_LEN);

          //*(mz_uint16 *)(&pLZ_code_buf[1]) = (mz_uint16)cur_match_dist;
          memcpy((mz_uint8 *)(&pLZ_code_buf[1]), (mz_uint8 *)&cur_match_dist, sizeof(mz_uint16));
          pLZ_code_buf += 3;
          *pLZ_flags = (mz_uint8)((*pLZ_flags >> 1) | 0x80);

          s0 = s_tdefl_small_dist_sym[cur_match_dist & 511];
          s1 = s_tdefl_large_dist_sym[cur_match_dist >> 8];
          d->m_huff_count[1][(cur_match_dist < 512) ? s0 : s1]++;

          d->m_huff_count[0][s_tdefl_len_sym[cur_match_len - TDEFL_MIN_MATCH_LEN]]++;
        }
      }
      else
      {
        *pLZ_code_buf++ = (mz_uint8)first_trigram;
        *pLZ_flags = (mz_uint8)(*pLZ_flags >> 1);
        d->m_huff_count[0][(mz_uint8)first_trigram]++;
      }

      if (--num_flags_left == 0) { num_flags_left = 8; pLZ_flags = pLZ_code_buf++; }

      total_lz_bytes += cur_match_len;
      lookahead_pos += cur_match_len;
      dict_size = MZ_MIN(dict_size + cur_match_len, TDEFL_LZ_DICT_SIZE);
      cur_pos = (cur_pos + cur_match_len) & TDEFL_LZ_DICT_SIZE_MASK;
      //MZ_ASSERT(lookahead_size >= cur_match_len);
      lookahead_size -= cur_match_len;
      if (pLZ_code_buf > &d->m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE - 8])
      {
        int n;
        d->m_lookahead_pos = lookahead_pos; d->m_lookahead_size = lookahead_size; d->m_dict_size = dict_size;
        d->m_total_lz_bytes = total_lz_bytes; d->m_pLZ_code_buf = pLZ_code_buf; d->m_pLZ_flags = pLZ_flags; d->m_num_flags_left = num_flags_left;

        if ((n = tdefl_flush_block(d, 0)) != 0)
          return (n < 0) ? MZ_FALSE : MZ_TRUE;
        total_lz_bytes = d->m_total_lz_bytes; pLZ_code_buf = d->m_pLZ_code_buf; pLZ_flags = d->m_pLZ_flags; num_flags_left = d->m_num_flags_left;
      }
    }

    while (lookahead_size)
    {
      mz_uint8 lit = d->m_dict[cur_pos];

      total_lz_bytes++;
      *pLZ_code_buf++ = lit;
      *pLZ_flags = (mz_uint8)(*pLZ_flags >> 1);
      if (--num_flags_left == 0) { num_flags_left = 8; pLZ_flags = pLZ_code_buf++; }

      d->m_huff_count[0][lit]++;

      lookahead_pos++;
      dict_size = MZ_MIN(dict_size + 1, TDEFL_LZ_DICT_SIZE);
      cur_pos = (cur_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK;
      lookahead_size--;

      if (pLZ_code_buf > &d->m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE - 8])
      {
        int n;
        d->m_lookahead_pos = lookahead_pos; d->m_lookahead_size = lookahead_size; d->m_dict_size = dict_size;
        d->m_total_lz_bytes = total_lz_bytes; d->m_pLZ_code_buf = pLZ_code_buf; d->m_pLZ_flags = pLZ_flags; d->m_num_flags_left = num_flags_left;
        if ((n = tdefl_flush_block(d, 0)) != 0)
          return (n < 0) ? MZ_FALSE : MZ_TRUE;
        total_lz_bytes = d->m_total_lz_bytes; pLZ_code_buf = d->m_pLZ_code_buf; pLZ_flags = d->m_pLZ_flags; num_flags_left = d->m_num_flags_left;
      }
    }
  }

  d->m_lookahead_pos = lookahead_pos; d->m_lookahead_size = lookahead_size; d->m_dict_size = dict_size;
  d->m_total_lz_bytes = total_lz_bytes; d->m_pLZ_code_buf = pLZ_code_buf; d->m_pLZ_flags = pLZ_flags; d->m_num_flags_left = num_flags_left;
  return MZ_TRUE;
}
#endif // MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN


static mz_bool tdefl_compress_normal(tdefl_compressor *d) {

    const mz_uint8 *pSrc = d->m_pSrc;
    size_t src_buf_left = d->m_src_buf_left;
    tdefl_flush flush = d->m_flush;
    while ((src_buf_left) || ((flush) && (d->m_lookahead_size))) {
        mz_uint len_to_move, cur_match_dist, cur_match_len, cur_pos;

        // Update dictionary and hash chains. Keeps the lookahead size equal to TDEFL_MAX_MATCH_LEN.
        if ((d->m_lookahead_size + d->m_dict_size) >= (TDEFL_MIN_MATCH_LEN - 1)) {
            mz_uint dst_pos = (d->m_lookahead_pos + d->m_lookahead_size) & TDEFL_LZ_DICT_SIZE_MASK;
            mz_uint ins_pos = d->m_lookahead_pos + d->m_lookahead_size - 2;
            mz_uint hash = (d->m_dict[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] << TDEFL_LZ_HASH_SHIFT)
                                                         ^ d->m_dict[(ins_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK];

            mz_uint num_bytes_to_process = (mz_uint)MZ_MIN(src_buf_left, TDEFL_MAX_MATCH_LEN - d->m_lookahead_size);

            const mz_uint8 *pSrc_end = pSrc + num_bytes_to_process;

            src_buf_left -= num_bytes_to_process;
            d->m_lookahead_size += num_bytes_to_process;

            while (pSrc != pSrc_end) {
                mz_uint8 c = *pSrc++; d->m_dict[dst_pos] = c;
                if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
                    d->m_dict[TDEFL_LZ_DICT_SIZE + dst_pos] = c;
                hash = ((hash << TDEFL_LZ_HASH_SHIFT) ^ c) & (TDEFL_LZ_HASH_SIZE - 1);
                d->m_next[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] = d->m_hash[hash];
                d->m_hash[hash] = (mz_uint16)(ins_pos);
                dst_pos = (dst_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK;
                ins_pos++;
            }
        }
        else {
            while ((src_buf_left) && (d->m_lookahead_size < TDEFL_MAX_MATCH_LEN)) {
                mz_uint8 c = *pSrc++;
                mz_uint dst_pos = (d->m_lookahead_pos + d->m_lookahead_size) & TDEFL_LZ_DICT_SIZE_MASK;
                src_buf_left--;
                d->m_dict[dst_pos] = c;
                if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
                    d->m_dict[TDEFL_LZ_DICT_SIZE + dst_pos] = c;
                if ((++d->m_lookahead_size + d->m_dict_size) >= TDEFL_MIN_MATCH_LEN) {
                    mz_uint ins_pos = d->m_lookahead_pos + (d->m_lookahead_size - 1) - 2;
                    mz_uint hash = ((d->m_dict[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] \
                                     << (TDEFL_LZ_HASH_SHIFT * 2)) ^ (d->m_dict[(ins_pos + 1) \
                                     & TDEFL_LZ_DICT_SIZE_MASK] << TDEFL_LZ_HASH_SHIFT) ^ c) \
                                     & (TDEFL_LZ_HASH_SIZE - 1);
                    d->m_next[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] = d->m_hash[hash];
                    d->m_hash[hash] = (mz_uint16)(ins_pos);
                }
            }
        }

        d->m_dict_size = MZ_MIN(TDEFL_LZ_DICT_SIZE - d->m_lookahead_size, d->m_dict_size);
        if ((!flush) && (d->m_lookahead_size < TDEFL_MAX_MATCH_LEN))
            break;

        // Simple lazy/greedy parsing state machine.
        len_to_move = 1;
        cur_match_dist = 0;
        cur_match_len = d->m_saved_match_len ? d->m_saved_match_len : (TDEFL_MIN_MATCH_LEN - 1);
        cur_pos = d->m_lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK;
        if (d->m_flags & (TDEFL_RLE_MATCHES | TDEFL_FORCE_ALL_RAW_BLOCKS)) {
            if ((d->m_dict_size) && (!(d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS))) {
                mz_uint8 c = d->m_dict[(cur_pos - 1) & TDEFL_LZ_DICT_SIZE_MASK];
                cur_match_len = 0;
                while (cur_match_len < d->m_lookahead_size) {
                    if (d->m_dict[cur_pos + cur_match_len] != c)
                        break;
                    cur_match_len++;
                }
                if (cur_match_len < TDEFL_MIN_MATCH_LEN)
                    cur_match_len = 0;
               else
                   cur_match_dist = 1;
            }
        }
        else {
            tdefl_find_match(d, d->m_lookahead_pos, d->m_dict_size, d->m_lookahead_size, &cur_match_dist, &cur_match_len);
        }
        if (((cur_match_len == TDEFL_MIN_MATCH_LEN) && (cur_match_dist >= 8U*1024U))
                                         || (cur_pos == cur_match_dist)
                                         || ((d->m_flags & TDEFL_FILTER_MATCHES) && (cur_match_len <= 5))) {
           cur_match_dist = cur_match_len = 0;
        }

        if (d->m_saved_match_len) {
            if (cur_match_len > d->m_saved_match_len) {
                tdefl_record_literal(d, (mz_uint8)d->m_saved_lit);
                if (cur_match_len >= 128) {
                    tdefl_record_match(d, cur_match_len, cur_match_dist);
                    d->m_saved_match_len = 0; len_to_move = cur_match_len;
                }
                else {
                    d->m_saved_lit = d->m_dict[cur_pos];
                    d->m_saved_match_dist = cur_match_dist;
                    d->m_saved_match_len = cur_match_len;
                }
            }
            else {
                tdefl_record_match(d, d->m_saved_match_len, d->m_saved_match_dist);
                len_to_move = d->m_saved_match_len - 1;
                d->m_saved_match_len = 0;
            }
        }
        else if (!cur_match_dist)
            tdefl_record_literal(d, d->m_dict[MZ_MIN(cur_pos, sizeof(d->m_dict) - 1)]);
        else if ((d->m_greedy_parsing) || (d->m_flags & TDEFL_RLE_MATCHES)
                                                || (cur_match_len >= 128)) {
            tdefl_record_match(d, cur_match_len, cur_match_dist);
            len_to_move = cur_match_len;
        }
        else {
            d->m_saved_lit = d->m_dict[MZ_MIN(cur_pos, sizeof(d->m_dict) - 1)];
            d->m_saved_match_dist = cur_match_dist;
            d->m_saved_match_len = cur_match_len;
        }

        // Move the lookahead forward by len_to_move bytes.
        d->m_lookahead_pos += len_to_move;
        // MZ_ASSERT(d->m_lookahead_size >= len_to_move);
        d->m_lookahead_size -= len_to_move;
        d->m_dict_size = MZ_MIN(d->m_dict_size + len_to_move, TDEFL_LZ_DICT_SIZE);

        // Check if it's time to flush the current LZ codes to the internal output buffer.
        if ( (d->m_pLZ_code_buf > &d->m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE - 8])
                   || ((d->m_total_lz_bytes > 31*1024)
                   && (((((mz_uint)(d->m_pLZ_code_buf - d->m_lz_code_buf) * 115) >> 7) >= d->m_total_lz_bytes) \
                   || (d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS)))) {
            int n;
            d->m_pSrc = pSrc;
            d->m_src_buf_left = src_buf_left;
            if ((n = tdefl_flush_block(d, 0)) != 0)
                return (n < 0) ? MZ_FALSE : MZ_TRUE;
        }
    }

    d->m_pSrc = pSrc; d->m_src_buf_left = src_buf_left;

    return MZ_TRUE;
}


static tdefl_status tdefl_flush_output_buffer(tdefl_compressor *d) {
    if (d->m_pIn_buf_size) {
        *d->m_pIn_buf_size = d->m_pSrc - (const mz_uint8 *)d->m_pIn_buf;
    }

    if (d->m_pOut_buf_size) {
        size_t n = MZ_MIN(*d->m_pOut_buf_size - d->m_out_buf_ofs, d->m_output_flush_remaining);
        memcpy((mz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs,
                           d->m_output_buf + d->m_output_flush_ofs, n);
        d->m_output_flush_ofs += (mz_uint)n;
        d->m_output_flush_remaining -= (mz_uint)n;
        d->m_out_buf_ofs += n;
        *d->m_pOut_buf_size = d->m_out_buf_ofs;
    }

    return (d->m_finished && !d->m_output_flush_remaining) ? \
                              TDEFL_STATUS_DONE : TDEFL_STATUS_OKAY;
}


tdefl_status tdefl_compress(tdefl_compressor *d, const void *pIn_buf,
             size_t *pIn_buf_size, void *pOut_buf, size_t *pOut_buf_size, tdefl_flush flush) {

    if (!d) {
        if (pIn_buf_size) *pIn_buf_size = 0;
        if (pOut_buf_size) *pOut_buf_size = 0;
        return TDEFL_STATUS_BAD_PARAM;
    }

    d->m_pIn_buf = pIn_buf;
    d->m_pIn_buf_size = pIn_buf_size;
    d->m_pOut_buf = pOut_buf;
    d->m_pOut_buf_size = pOut_buf_size;
    d->m_pSrc = (const mz_uint8 *)(pIn_buf);
    d->m_src_buf_left = pIn_buf_size ? *pIn_buf_size : 0;
    d->m_out_buf_ofs = 0;
    d->m_flush = flush;

    if ( ((d->m_pPut_buf_func != NULL) == ((pOut_buf != NULL) || (pOut_buf_size != NULL)))
                                         || (d->m_prev_return_status != TDEFL_STATUS_OKAY)
                                         || (d->m_wants_to_finish && (flush != TDEFL_FINISH))
                                         || (pIn_buf_size && *pIn_buf_size && !pIn_buf)
                                         || (pOut_buf_size && *pOut_buf_size && !pOut_buf) ) {
        if (pIn_buf_size) *pIn_buf_size = 0;
        if (pOut_buf_size) *pOut_buf_size = 0;
        return (d->m_prev_return_status = TDEFL_STATUS_BAD_PARAM);
    }

    d->m_wants_to_finish |= (flush == TDEFL_FINISH);

    if ((d->m_output_flush_remaining) || (d->m_finished))
        return (d->m_prev_return_status = tdefl_flush_output_buffer(d));
#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
  if (((d->m_flags & TDEFL_MAX_PROBES_MASK) == 1) &&
      ((d->m_flags & TDEFL_GREEDY_PARSING_FLAG) != 0) &&
      ((d->m_flags & (TDEFL_FILTER_MATCHES | TDEFL_FORCE_ALL_RAW_BLOCKS | TDEFL_RLE_MATCHES)) == 0))
  {
    if (!tdefl_compress_fast(d))
      return d->m_prev_return_status;
  }
  else
#endif // #if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
  {
    if (!tdefl_compress_normal(d)) {
        return d->m_prev_return_status;
    }
  }
    if ((d->m_flags & (TDEFL_WRITE_ZLIB_HEADER | TDEFL_COMPUTE_ADLER32)) && (pIn_buf))
        d->m_adler32 = (mz_uint32)mz_adler32(d->m_adler32, (const mz_uint8 *)pIn_buf,
                                                            d->m_pSrc - (const mz_uint8 *)pIn_buf);

    if ((flush) && (!d->m_lookahead_size) && (!d->m_src_buf_left) && (!d->m_output_flush_remaining)) {
        if (tdefl_flush_block(d, flush) < 0)
            return d->m_prev_return_status;

        d->m_finished = (flush == TDEFL_FINISH);
        if (flush == TDEFL_FULL_FLUSH) {
            MZ_CLEAR_OBJ(d->m_hash);
            MZ_CLEAR_OBJ(d->m_next);
            d->m_dict_size = 0;
        }
    }

    return (d->m_prev_return_status = tdefl_flush_output_buffer(d));
}


// level may actually range from [0,10] (10 is a "hidden" max level, where we want a bit more compression and it's fine if throughput to fall off a cliff on some files).
mz_uint tdefl_create_comp_flags_from_zip_params(int level, int window_bits, int strategy) {
    mz_uint comp_flags = s_tdefl_num_probes[(level >= 0) ? MZ_MIN(10, level) \
                         : MZ_DEFAULT_LEVEL] | ((level <= 3) ? TDEFL_GREEDY_PARSING_FLAG : 0);

    if (window_bits > 0)
        comp_flags |= TDEFL_WRITE_ZLIB_HEADER;

    if (!level)
        comp_flags |= TDEFL_FORCE_ALL_RAW_BLOCKS;
    else if (strategy == MZ_FILTERED)
        comp_flags |= TDEFL_FILTER_MATCHES;
    else if (strategy == MZ_HUFFMAN_ONLY)
        comp_flags &= ~TDEFL_MAX_PROBES_MASK;
    else if (strategy == MZ_FIXED)
        comp_flags |= TDEFL_FORCE_ALL_STATIC_BLOCKS;
    else if (strategy == MZ_RLE)
        comp_flags |= TDEFL_RLE_MATCHES;

    return comp_flags;
}

int mz_deflateEnd(mz_streamp pStream) {
    if (!pStream)
        return MZ_STREAM_ERROR;

    if (pStream->state) {
        pStream->zfree(pStream->opaque, pStream->state);
        pStream->state = NULL;
    }

    return MZ_OK;
}


#define DEFAULT_MEM_LEVEL 9
int mz_deflateInit(mz_streamp pStream, int level) {

    tdefl_compressor *pComp;
    mz_uint comp_flags = TDEFL_COMPUTE_ADLER32 |
              tdefl_create_comp_flags_from_zip_params(level, MZ_DEFAULT_WINDOW_BITS, MZ_DEFAULT_STRATEGY);

    if (!pStream) return MZ_STREAM_ERROR;

    pStream->data_type = 0;
    pStream->adler = MZ_ADLER32_INIT;
    pStream->msg = NULL;
    pStream->reserved = 0;
    pStream->total_in = 0;
    pStream->total_out = 0;
    if (!pStream->zalloc) pStream->zalloc = def_alloc_func;
    if (!pStream->zfree) pStream->zfree = def_free_func;

    pComp = (tdefl_compressor *)pStream->zalloc(pStream->opaque, 1, (void *)s_base_phyaddr, sizeof(tdefl_compressor));

    if (!pComp) return MZ_MEM_ERROR;

    pStream->state = (struct mz_internal_state *)pComp;

    if (tdefl_init(pComp, NULL, NULL, comp_flags) != TDEFL_STATUS_OKAY) {
        mz_deflateEnd(pStream);
        return MZ_PARAM_ERROR;
    }

    return MZ_OK;
}

int mz_deflate(mz_streamp pStream, int flush) {
    size_t in_bytes, out_bytes;
    mz_ulong orig_total_in, orig_total_out;
    int mz_status = MZ_OK;

    if ((!pStream) || (!pStream->state) || (flush < 0) || (flush > MZ_FINISH) || (!pStream->next_out))  {
        return MZ_STREAM_ERROR;
    }

    if (!pStream->avail_out)
        return MZ_BUF_ERROR;

    if (flush == MZ_PARTIAL_FLUSH)
        flush = MZ_SYNC_FLUSH;

    if (((tdefl_compressor*)pStream->state)->m_prev_return_status == TDEFL_STATUS_DONE)
        return (flush == MZ_FINISH) ? MZ_STREAM_END : MZ_BUF_ERROR;

    orig_total_in = pStream->total_in;
    orig_total_out = pStream->total_out;

    for ( ; ; ) {
        tdefl_status defl_status;
        in_bytes = pStream->avail_in;
        out_bytes = pStream->avail_out;

        defl_status = tdefl_compress((tdefl_compressor*)pStream->state, pStream->next_in, &in_bytes,
                                                        pStream->next_out, &out_bytes, (tdefl_flush)flush);

        pStream->next_in += (mz_uint)in_bytes;
        pStream->avail_in -= (mz_uint)in_bytes;
        pStream->total_in += (mz_uint)in_bytes;
        pStream->adler = tdefl_get_adler32((tdefl_compressor*)pStream->state);
        pStream->next_out += (mz_uint)out_bytes;
        pStream->avail_out -= (mz_uint)out_bytes;
        pStream->total_out += (mz_uint)out_bytes;

        if (defl_status < 0) {
            mz_status = MZ_STREAM_ERROR;
            break;
        }
        else if (defl_status == TDEFL_STATUS_DONE) {
            mz_status = MZ_STREAM_END;
            break;
        }
        else if (!pStream->avail_out) {
            break;
        }
        else if ((!pStream->avail_in) && (flush != MZ_FINISH)) {
            if ((flush) || (pStream->total_in != orig_total_in) || (pStream->total_out != orig_total_out))
                break;
            return MZ_BUF_ERROR; // Can't make forward progress without some input.
        }
    }

    return mz_status;
}

void mz_setbase(mz_uint32 base, mz_uint32 size)
{
    if(base == 0 || size == 0) return;
    s_base_phyaddr = base;
    s_base_size_limit = size;
}

int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len)
{
    int status;
    mz_stream stream;
    memset(&stream, 0, sizeof(stream));

    // In case mz_ulong is 64-bits (argh I hate longs).
    // if ((source_len | *pDest_len) > 0xFFFFFFFFU) return MZ_PARAM_ERROR;

    stream.next_in = pSource;
    stream.avail_in = (mz_uint32)source_len;
    stream.next_out = pDest;
    stream.avail_out = (mz_uint32)*pDest_len;

    status = mz_deflateInit(&stream, MZ_BEST_SPEED); //MZ_DEFAULT_COMPRESSION);

    if (status != MZ_OK)
        return status;

    status = mz_deflate(&stream, MZ_FINISH);

    if (status != MZ_STREAM_END) {
        mz_deflateEnd(&stream);
        return (status == MZ_OK) ? MZ_BUF_ERROR : status;
    }

    *pDest_len = stream.total_out;

    return mz_deflateEnd(&stream);
}


/*
===========================

UNCOMPRESS FUNCTIONS BELOW

===========================
*/


// Initializes the decompressor to its initial state.
#define tinfl_init(r) do { (r)->m_state = 0; } while(0)
#define tinfl_get_adler32(r) (r)->m_check_adler32

#define TINFL_MEMCPY(d, s, l) memcpy(d, s, l)
#define TINFL_MEMSET(p, c, l) memset(p, c, l)

#define TINFL_CR_BEGIN switch(r->m_state) { case 0:
#define TINFL_CR_RETURN(state_index, result) do { status = result; r->m_state = state_index; goto common_exit; case state_index:; } while(0)
#define TINFL_CR_RETURN_FOREVER(state_index, result) do { for ( ; ; ) { TINFL_CR_RETURN(state_index, result); } } while(0)
#define TINFL_CR_FINISH }

// TODO: If the caller has indicated that there's no more input, and we attempt to read beyond the input buf, then something is wrong with the input because the inflator never
// reads ahead more than it needs to. Currently TINFL_GET_BYTE() pads the end of the stream with 0's in this scenario.
#define TINFL_GET_BYTE(state_index, c) do { \
  if (pIn_buf_cur >= pIn_buf_end) { \
    for ( ; ; ) { \
      if (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) { \
        TINFL_CR_RETURN(state_index, TINFL_STATUS_NEEDS_MORE_INPUT); \
        if (pIn_buf_cur < pIn_buf_end) { \
          c = *pIn_buf_cur++; \
          break; \
        } \
      } else { \
        c = 0; \
        break; \
      } \
    } \
  } else c = *pIn_buf_cur++; } while(0)

#define TINFL_NEED_BITS(state_index, n) do { mz_uint c; TINFL_GET_BYTE(state_index, c); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(n))
#define TINFL_SKIP_BITS(state_index, n) do { if (num_bits < (mz_uint)(n)) { TINFL_NEED_BITS(state_index, n); } bit_buf >>= (n); num_bits -= (n); } while(0)
#define TINFL_GET_BITS(state_index, b, n) do { if (num_bits < (mz_uint)(n)) { TINFL_NEED_BITS(state_index, n); } b = bit_buf & ((1 << (n)) - 1); bit_buf >>= (n); num_bits -= (n); } while(0)

// TINFL_HUFF_BITBUF_FILL() is only used rarely, when the number of bytes remaining in the input buffer falls below 2.
// It reads just enough bytes from the input stream that are needed to decode the next Huffman code (and absolutely no more). It works by trying to fully decode a
// Huffman code by using whatever bits are currently present in the bit buffer. If this fails, it reads another byte, and tries again until it succeeds or until the
// bit buffer contains >=15 bits (deflate's max. Huffman code size).
#define TINFL_HUFF_BITBUF_FILL(state_index, pHuff) \
  do { \
    temp = (pHuff)->m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]; \
    if (temp >= 0) { \
      code_len = temp >> 9; \
      if ((code_len) && (num_bits >= code_len)) \
      break; \
    } else if (num_bits > TINFL_FAST_LOOKUP_BITS) { \
       code_len = TINFL_FAST_LOOKUP_BITS; \
       do { \
          temp = (pHuff)->m_tree[~temp + ((bit_buf >> code_len++) & 1)]; \
       } while ((temp < 0) && (num_bits >= (code_len + 1))); if (temp >= 0) break; \
    } TINFL_GET_BYTE(state_index, c); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; \
  } while (num_bits < 15);

// TINFL_HUFF_DECODE() decodes the next Huffman coded symbol. It's more complex than you would initially expect because the zlib API expects the decompressor to never read
// beyond the final byte of the deflate stream. (In other words, when this macro wants to read another byte from the input, it REALLY needs another byte in order to fully
// decode the next Huffman code.) Handling this properly is particularly important on raw deflate (non-zlib) streams, which aren't followed by a byte aligned adler-32.
// The slow path is only executed at the very end of the input buffer.
#define TINFL_HUFF_DECODE(state_index, sym, pHuff) do { \
  int temp; mz_uint code_len, c; \
  if (num_bits < 15) { \
    if ((pIn_buf_end - pIn_buf_cur) < 2) { \
       TINFL_HUFF_BITBUF_FILL(state_index, pHuff); \
    } else { \
       bit_buf |= (((tinfl_bit_buf_t)pIn_buf_cur[0]) << num_bits) | (((tinfl_bit_buf_t)pIn_buf_cur[1]) << (num_bits + 8)); pIn_buf_cur += 2; num_bits += 16; \
    } \
  } \
  if ((temp = (pHuff)->m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) \
    code_len = temp >> 9, temp &= 511; \
  else { \
    code_len = TINFL_FAST_LOOKUP_BITS; do { temp = (pHuff)->m_tree[~temp + ((bit_buf >> code_len++) & 1)]; } while (temp < 0); \
  } sym = temp; bit_buf >>= code_len; num_bits -= code_len; } while(0)

tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size,
        mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const mz_uint32 decomp_flags) {
    static const int s_length_base[31] = { 3,4,5,6,7,8,9,10,11,13, 15,17,19,23,27,31,35,43,51,59, 67,83,99,115,131,163,195,227,258,0,0 };
    static const int s_length_extra[31]= { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0 };
    static const int s_dist_base[32] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193, 257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};
    static const int s_dist_extra[32] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const mz_uint8 s_length_dezigzag[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
    static const int s_min_table_sizes[3] = { 257, 1, 4 };

    tinfl_status status = TINFL_STATUS_FAILED;
    mz_uint32 num_bits, dist, counter, num_extra;
    tinfl_bit_buf_t bit_buf;
    const mz_uint8 *pIn_buf_cur = pIn_buf_next, *const pIn_buf_end = pIn_buf_next + *pIn_buf_size;
    mz_uint8 *pOut_buf_cur = pOut_buf_next, *const pOut_buf_end = pOut_buf_next + *pOut_buf_size;
    size_t out_buf_size_mask = (decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) ? (size_t)-1 : ((pOut_buf_next - pOut_buf_start) + *pOut_buf_size) - 1, dist_from_out_buf_start;

    // Ensure the output buffer's size is a power of 2, unless the output buffer is large enough to hold the entire output file (in which case it doesn't matter).
    if (((out_buf_size_mask + 1) & out_buf_size_mask) || (pOut_buf_next < pOut_buf_start)) {
        *pIn_buf_size = *pOut_buf_size = 0;
        return TINFL_STATUS_BAD_PARAM;
    }

    num_bits = r->m_num_bits;
    bit_buf = r->m_bit_buf;
    dist = r->m_dist;
    counter = r->m_counter;
    num_extra = r->m_num_extra;
    dist_from_out_buf_start = r->m_dist_from_out_buf_start;

    TINFL_CR_BEGIN

    bit_buf = num_bits = dist = counter = num_extra = r->m_zhdr0 = r->m_zhdr1 = 0;
    r->m_z_adler32 = r->m_check_adler32 = 1;
    if (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER) {
        TINFL_GET_BYTE(1, r->m_zhdr0); TINFL_GET_BYTE(2, r->m_zhdr1);
        counter = (((r->m_zhdr0 * 256 + r->m_zhdr1) % 31 != 0) || (r->m_zhdr1 & 32)
                                                      || ((r->m_zhdr0 & 15) != 8));
        if (!(decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF))
            counter |= (((1U << (8U + (r->m_zhdr0 >> 4))) > 32768U)
                       || ((out_buf_size_mask + 1) < (size_t)(1U << (8U + (r->m_zhdr0 >> 4)))));
        if (counter) {
            TINFL_CR_RETURN_FOREVER(36, TINFL_STATUS_FAILED);
        }
    }

    do {
        TINFL_GET_BITS(3, r->m_final, 3);
        r->m_type = r->m_final >> 1;
        if (r->m_type == 0) {
            TINFL_SKIP_BITS(5, num_bits & 7);
            for (counter = 0; counter < 4; ++counter) {
                if (num_bits) TINFL_GET_BITS(6, r->m_raw_header[counter], 8);
                else TINFL_GET_BYTE(7, r->m_raw_header[counter]);
            }

            if ((counter = (r->m_raw_header[0] | (r->m_raw_header[1] << 8)))
                           != (mz_uint)(0xFFFF ^ (r->m_raw_header[2] | (r->m_raw_header[3] << 8)))) {
                TINFL_CR_RETURN_FOREVER(39, TINFL_STATUS_FAILED);
            }
            while ((counter) && (num_bits)) {
                TINFL_GET_BITS(51, dist, 8);
                while (pOut_buf_cur >= pOut_buf_end) {
                    TINFL_CR_RETURN(52, TINFL_STATUS_HAS_MORE_OUTPUT);
                }
                *pOut_buf_cur++ = (mz_uint8)dist;
                counter--;
            }
            while (counter) {
                size_t n;
                while (pOut_buf_cur >= pOut_buf_end) {
                    TINFL_CR_RETURN(9, TINFL_STATUS_HAS_MORE_OUTPUT);
                }

                while (pIn_buf_cur >= pIn_buf_end) {
                    if (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) {
                        TINFL_CR_RETURN(38, TINFL_STATUS_NEEDS_MORE_INPUT);
                    }
                    else {
                        TINFL_CR_RETURN_FOREVER(40, TINFL_STATUS_FAILED);
                    }
                }
                n = MZ_MIN(MZ_MIN((size_t)(pOut_buf_end - pOut_buf_cur),
                                  (size_t)(pIn_buf_end - pIn_buf_cur)), counter);
                TINFL_MEMCPY(pOut_buf_cur, pIn_buf_cur, n);
                pIn_buf_cur += n;
                pOut_buf_cur += n;
                counter -= (mz_uint)n;
            }
        }
        else if (r->m_type == 3) {
            TINFL_CR_RETURN_FOREVER(10, TINFL_STATUS_FAILED);
        }
        else {
            if (r->m_type == 1) {
                mz_uint8 *p = r->m_tables[0].m_code_size;
                mz_uint i;
                r->m_table_sizes[0] = 288;
                r->m_table_sizes[1] = 32;
                TINFL_MEMSET(r->m_tables[1].m_code_size, 5, 32);
                for ( i = 0; i <= 143; ++i) *p++ = 8;
                for ( ; i <= 255; ++i) *p++ = 9;
                for ( ; i <= 279; ++i) *p++ = 7;
                for ( ; i <= 287; ++i) *p++ = 8;
            }
            else {
                for (counter = 0; counter < 3; counter++) {
                    TINFL_GET_BITS(11, r->m_table_sizes[counter], "\05\05\04"[counter]);
                    r->m_table_sizes[counter] += s_min_table_sizes[counter];
                }

                MZ_CLEAR_OBJ(r->m_tables[2].m_code_size);
                for (counter = 0; counter < r->m_table_sizes[2]; counter++) {
                    mz_uint s;
                    TINFL_GET_BITS(14, s, 3);
                    r->m_tables[2].m_code_size[s_length_dezigzag[counter]] = (mz_uint8)s;
                }
                r->m_table_sizes[2] = 19;
            }
            for ( ; (int)r->m_type >= 0; r->m_type--) {
                int tree_next, tree_cur;
                tinfl_huff_table *pTable;
                mz_uint i, j, used_syms, total, sym_index, next_code[17], total_syms[16];

                pTable = &r->m_tables[r->m_type];
                MZ_CLEAR_OBJ(total_syms);
                MZ_CLEAR_OBJ(pTable->m_look_up);
                MZ_CLEAR_OBJ(pTable->m_tree);
                for (i = 0; i < r->m_table_sizes[r->m_type]; ++i)
                    total_syms[pTable->m_code_size[i]]++;
                used_syms = 0, total = 0;
                next_code[0] = next_code[1] = 0;
                for (i = 1; i <= 15; ++i) {
                    used_syms += total_syms[i];
                    next_code[i + 1] = (total = ((total + total_syms[i]) << 1));
                }
                if ((65536 != total) && (used_syms > 1)) {
                    TINFL_CR_RETURN_FOREVER(35, TINFL_STATUS_FAILED);
                }
                for (tree_next = -1, sym_index = 0; sym_index < r->m_table_sizes[r->m_type];
                                                                            ++sym_index) {
                    mz_uint rev_code = 0, l, cur_code, code_size = pTable->m_code_size[sym_index];
                    if (!code_size) continue;
                    cur_code = next_code[code_size]++;
                    for (l = code_size; l > 0; l--, cur_code >>= 1)
                        rev_code = (rev_code << 1) | (cur_code & 1);
                    if (code_size <= TINFL_FAST_LOOKUP_BITS) {
                        mz_int16 k = (mz_int16)((code_size << 9) | sym_index);
                        while (rev_code < TINFL_FAST_LOOKUP_SIZE) {
                            pTable->m_look_up[rev_code] = k;
                            rev_code += (1 << code_size);
                        }
                        continue;
                    }
                    if (0 == (tree_cur = pTable->m_look_up[rev_code & (TINFL_FAST_LOOKUP_SIZE - 1)])) {
                        pTable->m_look_up[rev_code & (TINFL_FAST_LOOKUP_SIZE - 1)] = (mz_int16)tree_next;
                        tree_cur = tree_next;
                        tree_next -= 2;
                    }
                    rev_code >>= (TINFL_FAST_LOOKUP_BITS - 1);
                    for (j = code_size; j > (TINFL_FAST_LOOKUP_BITS + 1); j--) {
                        tree_cur -= ((rev_code >>= 1) & 1);
                        if (!pTable->m_tree[-tree_cur - 1]) {
                            pTable->m_tree[-tree_cur - 1] = (mz_int16)tree_next;
                            tree_cur = tree_next;
                            tree_next -= 2;
                        }
                        else
                            tree_cur = pTable->m_tree[-tree_cur - 1];
                    }
                    tree_cur -= ((rev_code >>= 1) & 1);
                    pTable->m_tree[-tree_cur - 1] = (mz_int16)sym_index;
                }
                if (r->m_type == 2) {
                    for (counter = 0; counter < (r->m_table_sizes[0] + r->m_table_sizes[1]); ) {
                        mz_uint s;
                        TINFL_HUFF_DECODE(16, dist, &r->m_tables[2]);
                        if (dist < 16) {
                            r->m_len_codes[counter++] = (mz_uint8)dist;
                            continue;
                        }
                        if ((dist == 16) && (!counter)) {
                            TINFL_CR_RETURN_FOREVER(17, TINFL_STATUS_FAILED);
                        }
                        num_extra = "\02\03\07"[dist - 16];
                        TINFL_GET_BITS(18, s, num_extra);
                        s += "\03\03\013"[dist - 16];
                        TINFL_MEMSET(r->m_len_codes + counter, (dist == 16) ?
                                                           r->m_len_codes[counter - 1] : 0, s);
                        counter += s;
                    }
                    if ((r->m_table_sizes[0] + r->m_table_sizes[1]) != counter) {
                        TINFL_CR_RETURN_FOREVER(21, TINFL_STATUS_FAILED);
                    }
                    TINFL_MEMCPY(r->m_tables[0].m_code_size, r->m_len_codes, r->m_table_sizes[0]);
                    TINFL_MEMCPY(r->m_tables[1].m_code_size, r->m_len_codes + r->m_table_sizes[0],
                                                                          r->m_table_sizes[1]);
                }
            }
            for ( ; ; ) {
                mz_uint8 *pSrc;
                for ( ; ; ) {
                    if (((pIn_buf_end - pIn_buf_cur) < 4) || ((pOut_buf_end - pOut_buf_cur) < 2)) {
                        TINFL_HUFF_DECODE(23, counter, &r->m_tables[0]);
                        if (counter >= 256) break;
                        while (pOut_buf_cur >= pOut_buf_end) {
                            TINFL_CR_RETURN(24, TINFL_STATUS_HAS_MORE_OUTPUT);
                        }
                        *pOut_buf_cur++ = (mz_uint8)counter;
                    }
                    else {
                        int sym2; mz_uint code_len;
#if TINFL_USE_64BIT_BITBUF
                        if (num_bits < 30) {
                            bit_buf |= (((tinfl_bit_buf_t)MZ_READ_LE32(pIn_buf_cur)) << num_bits);
                            pIn_buf_cur += 4;
                            num_bits += 32;
                        }
#else
                        if (num_bits < 15) {
                            bit_buf |= (((tinfl_bit_buf_t)MZ_READ_LE16(pIn_buf_cur)) << num_bits);
                            pIn_buf_cur += 2;
                            num_bits += 16;
                        }
#endif
                        if ((sym2 = r->m_tables[0].m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0)
                            code_len = sym2 >> 9;
                        else {
                            code_len = TINFL_FAST_LOOKUP_BITS;
                            do {
                                sym2 = r->m_tables[0].m_tree[~sym2 + ((bit_buf >> code_len++) & 1)];
                            } while (sym2 < 0);
                        }
                        counter = sym2;
                        bit_buf >>= code_len;
                        num_bits -= code_len;
                        if (counter & 256) break;
#if !TINFL_USE_64BIT_BITBUF
                        if (num_bits < 15) {
                            bit_buf |= (((tinfl_bit_buf_t)MZ_READ_LE16(pIn_buf_cur)) << num_bits);
                            pIn_buf_cur += 2;
                            num_bits += 16;
                        }
#endif
                        if ((sym2 = r->m_tables[0].m_look_up[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0)
                            code_len = sym2 >> 9;
                        else {
                            code_len = TINFL_FAST_LOOKUP_BITS;
                            do {
                                sym2 = r->m_tables[0].m_tree[~sym2 + ((bit_buf >> code_len++) & 1)];
                            } while (sym2 < 0);
                        }
                        bit_buf >>= code_len;
                        num_bits -= code_len;

                        pOut_buf_cur[0] = (mz_uint8)counter;
                        if (sym2 & 256) {
                            pOut_buf_cur++;
                            counter = sym2;
                            break;
                        }
                        pOut_buf_cur[1] = (mz_uint8)sym2;
                        pOut_buf_cur += 2;
                    }
                }
                if ((counter &= 511) == 256) break;

                num_extra = s_length_extra[counter - 257];
                counter = s_length_base[counter - 257];
                if (num_extra) {
                    mz_uint extra_bits;
                    TINFL_GET_BITS(25, extra_bits, num_extra);
                    counter += extra_bits;
                }

                TINFL_HUFF_DECODE(26, dist, &r->m_tables[1]);
                num_extra = s_dist_extra[dist];
                dist = s_dist_base[dist];
                if (num_extra) {
                    mz_uint extra_bits;
                    TINFL_GET_BITS(27, extra_bits, num_extra);
                    dist += extra_bits;
                }

                dist_from_out_buf_start = pOut_buf_cur - pOut_buf_start;
                if ((dist > dist_from_out_buf_start)
                     && (decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)) {
                    TINFL_CR_RETURN_FOREVER(37, TINFL_STATUS_FAILED);
                }
                pSrc = pOut_buf_start + ((dist_from_out_buf_start - dist) & out_buf_size_mask);

                if ((MZ_MAX(pOut_buf_cur, pSrc) + counter) > pOut_buf_end) {
                    while (counter--) {
                        while (pOut_buf_cur >= pOut_buf_end) {
                            TINFL_CR_RETURN(53, TINFL_STATUS_HAS_MORE_OUTPUT);
                        }
                        *pOut_buf_cur++ = pOut_buf_start[(dist_from_out_buf_start++ - dist)
                                                                  & out_buf_size_mask];
                    }
                    continue;
                }
                do {
                    pOut_buf_cur[0] = pSrc[0];
                    pOut_buf_cur[1] = pSrc[1];
                    pOut_buf_cur[2] = pSrc[2];
                    pOut_buf_cur += 3;
                    pSrc += 3;
                } while ((int)(counter -= 3) > 2);
                if ((int)counter > 0) {
                    pOut_buf_cur[0] = pSrc[0];
                    if ((int)counter > 1)
                        pOut_buf_cur[1] = pSrc[1];
                    pOut_buf_cur += counter;
                }
            }
        }
    } while (!(r->m_final & 1));

    if (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER) {
        TINFL_SKIP_BITS(32, num_bits & 7);
        for (counter = 0; counter < 4; ++counter) {
            mz_uint s;
            if (num_bits) TINFL_GET_BITS(41, s, 8);
            else TINFL_GET_BYTE(42, s);
            r->m_z_adler32 = (r->m_z_adler32 << 8) | s;
        }
    }
    TINFL_CR_RETURN_FOREVER(34, TINFL_STATUS_DONE);
    TINFL_CR_FINISH

common_exit:
    r->m_num_bits = num_bits;
    r->m_bit_buf = bit_buf;
    r->m_dist = dist;
    r->m_counter = counter;
    r->m_num_extra = num_extra;
    r->m_dist_from_out_buf_start = dist_from_out_buf_start;
    *pIn_buf_size = pIn_buf_cur - pIn_buf_next;
    *pOut_buf_size = pOut_buf_cur - pOut_buf_next;
    if ((decomp_flags & (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_COMPUTE_ADLER32))
                      && (status >= 0)) {
        const mz_uint8 *ptr = pOut_buf_next;
        size_t buf_len = *pOut_buf_size;
        mz_uint32 i, s1 = r->m_check_adler32 & 0xffff, s2 = r->m_check_adler32 >> 16;
        size_t block_len = buf_len % 5552;
        while (buf_len) {
            for (i = 0; i + 7 < block_len; i += 8, ptr += 8) {
                s1 += ptr[0], s2 += s1;
                s1 += ptr[1], s2 += s1;
                s1 += ptr[2], s2 += s1;
                s1 += ptr[3], s2 += s1;
                s1 += ptr[4], s2 += s1;
                s1 += ptr[5], s2 += s1;
                s1 += ptr[6], s2 += s1;
                s1 += ptr[7], s2 += s1;
            }
            for ( ; i < block_len; ++i)
                s1 += *ptr++, s2 += s1;
            s1 %= 65521U, s2 %= 65521U;
            buf_len -= block_len;
            block_len = 5552;
        }
        r->m_check_adler32 = (s2 << 16) + s1;
        if ((status == TINFL_STATUS_DONE) && (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER)
                                          && (r->m_check_adler32 != r->m_z_adler32))
            status = TINFL_STATUS_ADLER32_MISMATCH;
    }
    return status;
}



int mz_inflateInit2(mz_streamp pStream, int window_bits) {
    inflate_state *pDecomp;
    if (!pStream)
        return MZ_STREAM_ERROR;

    if ((window_bits != MZ_DEFAULT_WINDOW_BITS) && (-window_bits != MZ_DEFAULT_WINDOW_BITS))
        return MZ_PARAM_ERROR;

    pStream->data_type = 0;
    pStream->adler = 0;
    pStream->msg = NULL;
    pStream->total_in = 0;
    pStream->total_out = 0;
    pStream->reserved = 0;
    if (!pStream->zalloc)
        pStream->zalloc = def_alloc_func;
    if (!pStream->zfree)
        pStream->zfree = def_free_func;

    pDecomp = (inflate_state*)pStream->zalloc(pStream->opaque, 1, (void *)s_base_phyaddr, sizeof(inflate_state));
    if (!pDecomp) return MZ_MEM_ERROR;

    pStream->state = (struct mz_internal_state *)pDecomp;

    tinfl_init(&pDecomp->m_decomp);
    pDecomp->m_dict_ofs = 0;
    pDecomp->m_dict_avail = 0;
    pDecomp->m_last_status = TINFL_STATUS_NEEDS_MORE_INPUT;
    pDecomp->m_first_call = 1;
    pDecomp->m_has_flushed = 0;
    pDecomp->m_window_bits = window_bits;

    return MZ_OK;
}

int mz_inflateInit(mz_streamp pStream) {
    return mz_inflateInit2(pStream, MZ_DEFAULT_WINDOW_BITS);
}

int mz_inflate(mz_streamp pStream, int flush) {
    inflate_state* pState;
    mz_uint n, first_call, decomp_flags = TINFL_FLAG_COMPUTE_ADLER32;
    size_t in_bytes, out_bytes, orig_avail_in;
    tinfl_status status;

    if ((!pStream) || (!pStream->state))
        return MZ_STREAM_ERROR;

    if (flush == MZ_PARTIAL_FLUSH)
        flush = MZ_SYNC_FLUSH;
    if ((flush) && (flush != MZ_SYNC_FLUSH) && (flush != MZ_FINISH))
        return MZ_STREAM_ERROR;

    pState = (inflate_state*)pStream->state;
    if (pState->m_window_bits > 0)
        decomp_flags |= TINFL_FLAG_PARSE_ZLIB_HEADER;
    orig_avail_in = pStream->avail_in;

    first_call = pState->m_first_call;
    pState->m_first_call = 0;

    if (pState->m_last_status < 0) {
        //uart_printlog_t("before uncomp, already failed! last_status = %d \n", pState->m_last_status);
        return MZ_DATA_ERROR;
    }

    if (pState->m_has_flushed && (flush != MZ_FINISH))
        return MZ_STREAM_ERROR;

    pState->m_has_flushed |= (flush == MZ_FINISH);

    if ((flush == MZ_FINISH) && (first_call)) {
        // MZ_FINISH on the first call implies that the input
        // and output buffers are large enough to hold the entire
        // compressed/decompressed file.
        decomp_flags |= TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
        in_bytes = pStream->avail_in; out_bytes = pStream->avail_out;
        status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes,
                            pStream->next_out, pStream->next_out, &out_bytes, decomp_flags);
        pState->m_last_status = status;
        pStream->next_in += (mz_uint)in_bytes;
        pStream->avail_in -= (mz_uint)in_bytes;
        pStream->total_in += (mz_uint)in_bytes;
        pStream->adler = tinfl_get_adler32(&pState->m_decomp);
        pStream->next_out += (mz_uint)out_bytes;
        pStream->avail_out -= (mz_uint)out_bytes;
        pStream->total_out += (mz_uint)out_bytes;
        if (status < 0) {
            //uart_printlog_t("before uncomp, first call failed! last_status = %d \n", status);
            return MZ_DATA_ERROR;
        }
        else if (status != TINFL_STATUS_DONE) {
            pState->m_last_status = TINFL_STATUS_FAILED;
            return MZ_BUF_ERROR;
        }
        return MZ_STREAM_END;
    }

    // flush != MZ_FINISH then we must assume there's more input.
    if (flush != MZ_FINISH)
        decomp_flags |= TINFL_FLAG_HAS_MORE_INPUT;

    if (pState->m_dict_avail) {
        n = MZ_MIN(pState->m_dict_avail, pStream->avail_out);
        memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
        pStream->next_out += n;
        pStream->avail_out -= n;
        pStream->total_out += n;
        pState->m_dict_avail -= n;
        pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);
        return ((pState->m_last_status == TINFL_STATUS_DONE) && (!pState->m_dict_avail))
                                                             ? MZ_STREAM_END : MZ_OK;
    }

    for ( ; ; ) {
        in_bytes = pStream->avail_in;
        out_bytes = TINFL_LZ_DICT_SIZE - pState->m_dict_ofs;

        status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes,
                   pState->m_dict, pState->m_dict + pState->m_dict_ofs, &out_bytes, decomp_flags);
        pState->m_last_status = status;

        pStream->next_in += (mz_uint)in_bytes;
        pStream->avail_in -= (mz_uint)in_bytes;
        pStream->total_in += (mz_uint)in_bytes;
        pStream->adler = tinfl_get_adler32(&pState->m_decomp);

        pState->m_dict_avail = (mz_uint)out_bytes;

        n = MZ_MIN(pState->m_dict_avail, pStream->avail_out);
        memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
        pStream->next_out += n;
        pStream->avail_out -= n;
        pStream->total_out += n;
        pState->m_dict_avail -= n;
        pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);
        //uart_printlog_t("uncomp: next_out: %d, avalout %d, totalout %d, m_dictav %d \r\n",
        //                                 pStream->next_out, pStream->avail_out,
        //                               pStream->total_out, pState->m_dict_avail);

        if (status < 0) {
             // Stream is corrupted (there could be some uncompressed data left in the output dictionary - oh well).
           // uart_printlog_t("uncomp failed with status = %d \n", status);
           return MZ_DATA_ERROR;
        }
        else if ((status == TINFL_STATUS_NEEDS_MORE_INPUT) && (!orig_avail_in))
            // Signal caller that we can't make forward progress without supplying more input or by setting flush to MZ_FINISH.
            return MZ_BUF_ERROR;
        else if (flush == MZ_FINISH) {
            // The output buffer MUST be large to hold the remaining uncompressed data when flush==MZ_FINISH.
            if (status == TINFL_STATUS_DONE)
                 return pState->m_dict_avail ? MZ_BUF_ERROR : MZ_STREAM_END;
                 // status here must be TINFL_STATUS_HAS_MORE_OUTPUT, which means there's at least 1 more byte on the way. If there's no more room left in the output buffer then something is wrong.
            else if (!pStream->avail_out)
                return MZ_BUF_ERROR;
        }
        else if ((status == TINFL_STATUS_DONE) || (!pStream->avail_in)
                  || (!pStream->avail_out) || (pState->m_dict_avail))
            break;
    }

    return ((status == TINFL_STATUS_DONE) && (!pState->m_dict_avail))
                                             ? MZ_STREAM_END : MZ_OK;
}

int mz_inflateEnd(mz_streamp pStream) {
    if (!pStream)
        return MZ_STREAM_ERROR;

    if (pStream->state) {
        pStream->zfree(pStream->opaque, pStream->state);
        pStream->state = NULL;
    }

    return MZ_OK;
}

int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len) {
    mz_stream stream;
    int status;
    memset(&stream, 0, sizeof(stream));

    // In case mz_ulong is 64-bits (argh I hate longs).
    if ((source_len | *pDest_len) > 0xFFFFFFFFU) return MZ_PARAM_ERROR;

    stream.next_in = pSource;
    stream.avail_in = (mz_uint32)source_len;
    stream.next_out = pDest;
    stream.avail_out = (mz_uint32)*pDest_len;

    status = mz_inflateInit(&stream);
    if (status != MZ_OK)
        return status;

    status = mz_inflate(&stream, MZ_FINISH);
    if (status != MZ_STREAM_END) {
        mz_inflateEnd(&stream);
        return ((status == MZ_BUF_ERROR) && (!stream.avail_in)) ? MZ_DATA_ERROR : status;
    }

    *pDest_len = stream.total_out;

    return mz_inflateEnd(&stream);
}

/*
  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>
*/

