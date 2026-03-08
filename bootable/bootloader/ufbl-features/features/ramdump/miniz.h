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

/* miniz.c v1.15 - public domain deflate/inflate, zlib-subset, ZIP reading/writing/appending, PNG writing
   See "unlicense" statement at the end of this file.
   Rich Geldreich <richgel99@gmail.com>, last updated Oct. 13, 2013
   Implements RFC 1950: http://www.ietf.org/rfc/rfc1950.txt and RFC 1951: http://www.ietf.org/rfc/rfc1951.txt

   Most API's defined in miniz.c are optional. For example, to disable the archive related functions just define
   MINIZ_NO_ARCHIVE_APIS, or to get rid of all stdio usage define MINIZ_NO_STDIO (see the list below for more macros).
*/


/*
   This code is modified by Wei Chen to fit into bootloader usage scenarios. The functionalities to deal with zip
   files are all stripped out. Only the core compression and uncompression operations are kept.

   Wei Chen  <weichew@amazon.com>, last updated Nov. 19th, 2014
*/

#ifndef _MINIZ_H
#define _MINIZ_H

// Heap allocation callbacks.
// Note that mz_alloc_func parameter types purpsosely differ from
// zlib's: items/size is size_t, not unsigned long.
typedef void *(*mz_alloc_func)(void *opaque, size_t items, void *base, size_t size);
typedef void (*mz_free_func)(void *opaque, void *address);
typedef void *(*mz_realloc_func)(void *opaque, void *address, size_t items, size_t size);

// Flush values. For typical usage you only need MZ_NO_FLUSH and MZ_FINISH.
// The other values are for advanced use (refer to the zlib docs).

enum {
    MZ_NO_FLUSH = 0,
    MZ_PARTIAL_FLUSH = 1,
    MZ_SYNC_FLUSH = 2,
    MZ_FULL_FLUSH = 3,
    MZ_FINISH = 4,
    MZ_BLOCK = 5
};

// Return status codes. MZ_PARAM_ERROR is non-standard.
enum {
    MZ_OK = 0,
    MZ_STREAM_END = 1,
    MZ_NEED_DICT = 2,
    MZ_ERRNO = -1,
    MZ_STREAM_ERROR = -2,
    MZ_DATA_ERROR = -3,
    MZ_MEM_ERROR = -4,
    MZ_BUF_ERROR = -5,
    MZ_VERSION_ERROR = -6,
    MZ_PARAM_ERROR = -10000
};

// Compression levels: 0-9 are the standard zlib-style levels,
// 10 is best possible compression (not zlib compatible,
// and may be very slow), MZ_DEFAULT_COMPRESSION=MZ_DEFAULT_LEVEL.
enum {
    MZ_NO_COMPRESSION = 0,
    MZ_BEST_SPEED = 1,
    MZ_BEST_COMPRESSION = 9,
    MZ_UBER_COMPRESSION = 10,
    MZ_DEFAULT_LEVEL = 6,
    MZ_DEFAULT_COMPRESSION = -1
};

// Compression strategies.
enum {
    MZ_DEFAULT_STRATEGY = 0,
    MZ_FILTERED = 1,
    MZ_HUFFMAN_ONLY = 2,
    MZ_RLE = 3,
    MZ_FIXED = 4
};

typedef unsigned char mz_uint8;
typedef signed short mz_int16;
typedef unsigned short mz_uint16;
typedef unsigned int mz_uint32;
typedef unsigned int mz_uint;
typedef long long mz_int64;
typedef unsigned long long mz_uint64;
typedef int mz_bool;

#define MZ_FALSE (0)
#define MZ_TRUE (1)

#define MZ_ADLER32_INIT (1)
#define MZ_CRC32_INIT (0)
#define MZ_DEFLATED 8

// Window bits
#define MZ_DEFAULT_WINDOW_BITS 15

#define MZ_ASSERT(x) assert(x)
/*
#ifdef MINIZ_NO_MALLOC
  #define MZ_MALLOC(x) NULL
  #define MZ_FREE(x) (void)x, ((void)0)
  #define MZ_REALLOC(p, x) NULL
#else
  #define MZ_MALLOC(x) malloc(x)
  #define MZ_FREE(x) free(x)
  #define MZ_REALLOC(p, x) realloc(p, x)
#endif
*/

#define MZ_MAX(a,b) (((a)>(b))?(a):(b))
#define MZ_MIN(a,b) (((a)<(b))?(a):(b))
#define MZ_CLEAR_OBJ(obj) memset(&(obj), 0, sizeof(obj))

#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
#define MINIZ_LITTLE_ENDIAN 1
#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
  #define MZ_READ_LE16(p) *((const mz_uint16 *)(p))
  #define MZ_READ_LE32(p) *((const mz_uint32 *)(p))
#else
  #define MZ_READ_LE16(p) ((mz_uint32)(((const mz_uint8 *)(p))[0]) | ((mz_uint32)(((const mz_uint8 *)(p))[1]) << 8U))
  #define MZ_READ_LE32(p) ((mz_uint32)(((const mz_uint8 *)(p))[0]) | ((mz_uint32)(((const mz_uint8 *)(p))[1]) << 8U) | ((mz_uint32)(((const mz_uint8 *)(p))[2]) << 16U) | ((mz_uint32)(((const mz_uint8 *)(p))[3]) << 24U))
#endif


// For more compatibility with zlib, miniz.c uses unsigned
// long for some parameters/struct members.
// Beware: mz_ulong can be either 32 or 64-bits!
typedef unsigned long mz_ulong;

// Compression/decompression stream struct.
typedef struct mz_stream_s {
    const unsigned char *next_in;     // pointer to next byte to read
    unsigned int avail_in;            // number of bytes available at next_in
    mz_ulong total_in;                // total number of bytes consumed so far
    unsigned char *next_out;          // pointer to next byte to write
    unsigned int avail_out;           // number of bytes that can be written to next_out
    mz_ulong total_out;               // total number of bytes produced so far
    char *msg;                        // error msg (unused)
    struct mz_internal_state *state;  // internal state, allocated by zalloc/zfree
    mz_alloc_func zalloc;             // optional heap allocation function (defaults to malloc)
    mz_free_func zfree;               // optional heap free function (defaults to free)
    void *opaque;                     // heap alloc function user pointer
    int data_type;                    // data_type (unused)
    mz_ulong adler;                   // adler32 of the source or uncompressed data
    mz_ulong reserved;                // not used
} mz_stream;

typedef mz_stream *mz_streamp;

// The low-level tdefl functions below may be used directly if the
// above helper functions aren't flexible enough. The low-level
// functions don't make any heap allocations,
// unlike the above helper functions.
typedef enum {
    TDEFL_STATUS_BAD_PARAM = -2,
    TDEFL_STATUS_PUT_BUF_FAILED = -1,
    TDEFL_STATUS_OKAY = 0,
    TDEFL_STATUS_DONE = 1,
} tdefl_status;

// Set TDEFL_LESS_MEMORY to 1 to use less memory
// (compression will be slightly slower, and
// raw/dynamic blocks will be output more frequently).
#define TDEFL_LESS_MEMORY 0

// tdefl_init() compression flags logically OR'd together
// low 12 bits contain the max. number of probes per dictionary search):
// TDEFL_DEFAULT_MAX_PROBES: The compressor defaults to 128 dictionary
// probes per dictionary search. 0=Huffman only, 1=Huffman+LZ
// (fastest/crap compression), 4095=Huffman+LZ (slowest/best compression).
enum {
    TDEFL_HUFFMAN_ONLY = 0,
    TDEFL_DEFAULT_MAX_PROBES = 128,
    TDEFL_MAX_PROBES_MASK = 0xFFF
};

// TDEFL_WRITE_ZLIB_HEADER: If set, the compressor outputs a
// zlib header before the deflate data, and the Adler-32 of the source
// data at the end. Otherwise, you'll get raw deflate data.
// TDEFL_COMPUTE_ADLER32: Always compute the adler-32 of the input
// data (even when not writing zlib headers).
// TDEFL_GREEDY_PARSING_FLAG: Set to use faster greedy parsing,
// instead of more efficient lazy parsing.
// TDEFL_NONDETERMINISTIC_PARSING_FLAG: Enable to decrease the
// compressor's initialization time to the minimum, but the output
// may vary from run to run given the same input (depending on the contents of memory).
// TDEFL_RLE_MATCHES: Only look for RLE matches (matches with a distance of 1)
// TDEFL_FILTER_MATCHES: Discards matches <= 5 chars if enabled.
// TDEFL_FORCE_ALL_STATIC_BLOCKS: Disable usage of optimized Huffman tables.
// TDEFL_FORCE_ALL_RAW_BLOCKS: Only use raw (uncompressed) deflate blocks.
// The low 12 bits are reserved to control the max # of hash probes per
// dictionary lookup (see TDEFL_MAX_PROBES_MASK).
enum {
    TDEFL_WRITE_ZLIB_HEADER             = 0x01000,
    TDEFL_COMPUTE_ADLER32               = 0x02000,
    TDEFL_GREEDY_PARSING_FLAG           = 0x04000,
    TDEFL_NONDETERMINISTIC_PARSING_FLAG = 0x08000,
    TDEFL_RLE_MATCHES                   = 0x10000,
    TDEFL_FILTER_MATCHES                = 0x20000,
    TDEFL_FORCE_ALL_STATIC_BLOCKS       = 0x40000,
    TDEFL_FORCE_ALL_RAW_BLOCKS          = 0x80000
};

enum {
    TDEFL_MAX_HUFF_TABLES = 3,
    TDEFL_MAX_HUFF_SYMBOLS_0 = 288,
    TDEFL_MAX_HUFF_SYMBOLS_1 = 32,
    TDEFL_MAX_HUFF_SYMBOLS_2 = 19,
    TDEFL_LZ_DICT_SIZE = 32768,
    TDEFL_LZ_DICT_SIZE_MASK = TDEFL_LZ_DICT_SIZE - 1,
    TDEFL_MIN_MATCH_LEN = 3,
    TDEFL_MAX_MATCH_LEN = 258
};

// TDEFL_OUT_BUF_SIZE MUST be large enough to hold a single entire
// compressed output block (using static/fixed Huffman codes).

#if TDEFL_LESS_MEMORY
enum {
    TDEFL_LZ_CODE_BUF_SIZE = 24 * 1024,
    TDEFL_OUT_BUF_SIZE = (TDEFL_LZ_CODE_BUF_SIZE * 13 ) / 10,
    TDEFL_MAX_HUFF_SYMBOLS = 288,
    TDEFL_LZ_HASH_BITS = 12,
    TDEFL_LEVEL1_HASH_SIZE_MASK = 4095,
    TDEFL_LZ_HASH_SHIFT = (TDEFL_LZ_HASH_BITS + 2) / 3,
    TDEFL_LZ_HASH_SIZE = 1 << TDEFL_LZ_HASH_BITS
};
#else
enum {
    TDEFL_LZ_CODE_BUF_SIZE = 64 * 1024,
    TDEFL_OUT_BUF_SIZE = (TDEFL_LZ_CODE_BUF_SIZE * 13 ) / 10,
    TDEFL_MAX_HUFF_SYMBOLS = 288, TDEFL_LZ_HASH_BITS = 15,
    TDEFL_LEVEL1_HASH_SIZE_MASK = 4095,
    TDEFL_LZ_HASH_SHIFT = (TDEFL_LZ_HASH_BITS + 2) / 3,
    TDEFL_LZ_HASH_SIZE = 1 << TDEFL_LZ_HASH_BITS
};
#endif

// Must map to MZ_NO_FLUSH, MZ_SYNC_FLUSH, etc. enums
typedef enum {
    TDEFL_NO_FLUSH = 0,
    TDEFL_SYNC_FLUSH = 2,
    TDEFL_FULL_FLUSH = 3,
    TDEFL_FINISH = 4
} tdefl_flush;

// Output stream interface. The compressor uses this interface to write
// compressed data. It'll typically be called TDEFL_OUT_BUF_SIZE at a time.
typedef mz_bool (*tdefl_put_buf_func_ptr)(const void* pBuf, int len, void *pUser);

// tdefl's compression state structure.
typedef struct {
    tdefl_put_buf_func_ptr m_pPut_buf_func;
    void *m_pPut_buf_user;
    mz_uint m_flags, m_max_probes[2];
    int m_greedy_parsing;
    mz_uint m_adler32, m_lookahead_pos, m_lookahead_size, m_dict_size;
    mz_uint8 *m_pLZ_code_buf, *m_pLZ_flags, *m_pOutput_buf, *m_pOutput_buf_end;
    mz_uint m_num_flags_left, m_total_lz_bytes, m_lz_code_buf_dict_pos, m_bits_in, m_bit_buffer;
    mz_uint m_saved_match_dist, m_saved_match_len, m_saved_lit, m_output_flush_ofs, m_output_flush_remaining, m_finished, m_block_index, m_wants_to_finish;
    tdefl_status m_prev_return_status;
    const void *m_pIn_buf;
    void *m_pOut_buf;
    size_t *m_pIn_buf_size, *m_pOut_buf_size;
    tdefl_flush m_flush;
    const mz_uint8 *m_pSrc;
    size_t m_src_buf_left, m_out_buf_ofs;
    mz_uint8 m_dict[TDEFL_LZ_DICT_SIZE + TDEFL_MAX_MATCH_LEN - 1];
    mz_uint16 m_huff_count[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
    mz_uint16 m_huff_codes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
    mz_uint8 m_huff_code_sizes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
    mz_uint8 m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE];
    mz_uint16 m_next[TDEFL_LZ_DICT_SIZE];
    mz_uint16 m_hash[TDEFL_LZ_HASH_SIZE];
    mz_uint8 m_output_buf[TDEFL_OUT_BUF_SIZE];
} tdefl_compressor;

// Decompression flags used by tinfl_decompress().
// TINFL_FLAG_PARSE_ZLIB_HEADER: If set, the input has a valid zlib header
// and ends with an adler32 checksum (it's a valid zlib stream). Otherwise, the
// input is a raw deflate stream.
// TINFL_FLAG_HAS_MORE_INPUT: If set, there are more input bytes available beyond
// the end of the supplied input buffer. If clear, the input buffer contains
// all remaining input.
// TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF: If set, the output buffer is large
// enough to hold the entire decompressed stream. If clear, the output buffer
// is at least the size of the dictionary (typically 32KB).
// TINFL_FLAG_COMPUTE_ADLER32: Force adler-32 checksum computation of the decompressed bytes.
enum {
    TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
    TINFL_FLAG_HAS_MORE_INPUT = 2,
    TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
    TINFL_FLAG_COMPUTE_ADLER32 = 8
};

// Max size of LZ dictionary.
#define TINFL_LZ_DICT_SIZE 32768

// Return status.
typedef enum {
    TINFL_STATUS_BAD_PARAM = -3,
    TINFL_STATUS_ADLER32_MISMATCH = -2,
    TINFL_STATUS_FAILED = -1,
    TINFL_STATUS_DONE = 0,
    TINFL_STATUS_NEEDS_MORE_INPUT = 1,
    TINFL_STATUS_HAS_MORE_OUTPUT = 2
} tinfl_status;

struct tinfl_decompressor_tag;
typedef struct tinfl_decompressor_tag tinfl_decompressor;

// Internal/private bits follow.
enum {
    TINFL_MAX_HUFF_TABLES = 3,
    TINFL_MAX_HUFF_SYMBOLS_0 = 288,
    TINFL_MAX_HUFF_SYMBOLS_1 = 32,
    TINFL_MAX_HUFF_SYMBOLS_2 = 19,
    TINFL_FAST_LOOKUP_BITS = 10,
    TINFL_FAST_LOOKUP_SIZE = 1 << TINFL_FAST_LOOKUP_BITS
};

typedef struct {
    mz_uint8 m_code_size[TINFL_MAX_HUFF_SYMBOLS_0];
    mz_int16 m_look_up[TINFL_FAST_LOOKUP_SIZE], m_tree[TINFL_MAX_HUFF_SYMBOLS_0 * 2];
} tinfl_huff_table;

#if MINIZ_HAS_64BIT_REGISTERS
    #define TINFL_USE_64BIT_BITBUF 1
#endif

#if TINFL_USE_64BIT_BITBUF
    typedef mz_uint64 tinfl_bit_buf_t;
    #define TINFL_BITBUF_SIZE (64)
#else
    typedef mz_uint32 tinfl_bit_buf_t;
    #define TINFL_BITBUF_SIZE (32)
#endif

struct tinfl_decompressor_tag {
    mz_uint32 m_state, m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32, m_final, m_type, m_check_adler32, m_dist, m_counter, m_num_extra, m_table_sizes[TINFL_MAX_HUFF_TABLES];
    tinfl_bit_buf_t m_bit_buf;
    size_t m_dist_from_out_buf_start;
    tinfl_huff_table m_tables[TINFL_MAX_HUFF_TABLES];
    mz_uint8 m_raw_header[4], m_len_codes[TINFL_MAX_HUFF_SYMBOLS_0 + TINFL_MAX_HUFF_SYMBOLS_1 + 137];
};


typedef struct {
    tinfl_decompressor m_decomp;
    mz_uint m_dict_ofs, m_dict_avail, m_first_call, m_has_flushed;
    int m_window_bits;
    mz_uint8 m_dict[TINFL_LZ_DICT_SIZE];
    tinfl_status m_last_status;
} inflate_state;

void mz_setbase(mz_uint32 base, mz_uint32 size);
int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len);
int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len);
#endif
