/**@file 
 * Copyright (C) 2013, Sinovision Tech Ltd.
 * All rights reserved.
 *
 * @brief   This file defines the event service interface
 *    the event service handles the main thread of event service
 *
 * @author  
 * @date 2013-4-19
 *
 */
#ifndef _bufferl_h_
#define _bufferl_h_
//-------------------------add by wjw copy from config.hpp---------------------------


#if defined(_MSC_VER)

# pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

#  if defined(FEBIRD_CREATE_DLL)
#    pragma warning(disable: 4251)
#    define FEBIRD_DLL_EXPORT __declspec(dllexport)      // creator of dll
#  elif defined(FEBIRD_USE_DLL)
#    pragma warning(disable: 4251)
#    define FEBIRD_DLL_EXPORT __declspec(dllimport)      // user of dll
#    ifdef _DEBUG
#	   pragma comment(lib, "febird-d.lib")
#    else
#	   pragma comment(lib, "febird.lib")
#    endif
#  else
#    define FEBIRD_DLL_EXPORT                            // static lib creator or user
#  endif

#else /* _MSC_VER */

#  define FEBIRD_DLL_EXPORT

#endif /* _MSC_VER */


#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)

#  define febird_likely(x)    __builtin_expect(x, 1)
#  define febird_unlikely(x)  __builtin_expect(x, 0)
#  define febird_no_return    __attribute__((noreturn))

#else

#  define febird_likely(x)    x
#  define febird_unlikely(x)  x

#endif

/* The ISO C99 standard specifies that in C++ implementations these
 *    should only be defined if explicitly requested __STDC_CONSTANT_MACROS
 */
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif



//----------------------------------------------copy end -------------------





//-------------------------add by wjw copy from config.h---------------------------


#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

//del by wjw #include "../config.hpp"
//#define c_algo_inline


#define C_ISORT_MAX 16


#define FEBIRD_C_MAX_VALUE_SIZE 64

// #define FEBIRD_C_LONG_DOUBLE_SIZE 10


//----------------------------------------------copy end -------------------


#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

//del by wjw #include "config.h"
#include <stddef.h> // for size_t

#include <pthread.h>//add by wjw for using lock

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------------------
struct sallocator
{
	void* (*salloc  )(struct sallocator* self, size_t size);
	void  (*sfree   )(struct sallocator* self, void* block, size_t size);
	void* (*srealloc)(struct sallocator* self, void* block, size_t old_size, size_t new_size);
};
typedef void* (*salloc_ft  )(struct sallocator* self, size_t size);
typedef void  (*sfree_ft   )(struct sallocator* self, void* block, size_t size);
typedef void* (*srealloc_ft)(struct sallocator* self, void* block, size_t old_size, size_t new_size);

struct fixed_mpool
{
	struct sallocator*  sa;
	struct mpool_chunk* pChunks;
	struct mpool_cell*  head;
	size_t iNextChunk;
	size_t nChunks;
	size_t cell_size;
	size_t chunk_size;
	size_t used_cells;
	size_t used_max;
	pthread_mutex_t lock;
};

struct sfixed_mpool
{
//------------------------------------------------------------------------------------------
/// export a sallocator interface
	void* (*salloc  )(struct sallocator* self, size_t size);
	void  (*sfree   )(struct sallocator* self, void* block, size_t size);
	void* (*srealloc)(struct sallocator* self, void* block, size_t old_size, size_t new_size);
//------------------------------------------------------------------------------------------
	struct fixed_mpool fmp;
};



//must be equal struct cell_attr
struct big_block_header
{
	struct big_block_header *next, *prev;
	size_t size;
	size_t used; // for alignment
};

struct mpool
{
//------------------------------------------------------------------------------------------
/// export a sallocator interface
	void* (*salloc  )(struct sallocator* self, size_t size);
	void  (*sfree   )(struct sallocator* self, void* block, size_t size);
	void* (*srealloc)(struct sallocator* self, void* block, size_t old_size, size_t new_size);
//------------------------------------------------------------------------------------------

	/// sallocator for this mpool self
	struct sallocator* sa;

	struct fixed_mpool* fixed;
	size_t max_cell_size;
	size_t chunk_size;

/// 是否允许 mpool 分配超出 max_cell_size 的内存块
/// allow alloc memory block bigger than max_cell_size
#define FEBIRD_MPOOL_ALLOW_BIG_BLOCK	1

/// when destroy, auto free big block or not
#define FEBIRD_MPOOL_AUTO_FREE_BIG		2

/// use malloc or this->salloc to alloc big block
#define FEBIRD_MPOOL_MALLOC_BIG			4

	size_t big_flags;
	size_t big_blocks; // size > max_cell_size, use malloc, this is rare case
	pthread_mutex_t big_lock;
	// pthread_mutex_t check_ptr_lock;
	struct big_block_header big_list;
};

/***********************************************************************/
FEBIRD_DLL_EXPORT void*  default_srealloc(struct sallocator* sa, void* ptr, size_t old_size, size_t new_size);
/***********************************************************************/


/***********************************************************************/
FEBIRD_DLL_EXPORT void fixed_mpool_init   (struct fixed_mpool* mpf);
FEBIRD_DLL_EXPORT void fixed_mpool_destroy(struct fixed_mpool* mpf);

FEBIRD_DLL_EXPORT void* fixed_mpool_alloc(struct fixed_mpool* mpf);
FEBIRD_DLL_EXPORT void  fixed_mpool_free (struct fixed_mpool* mpf, void* ptr);
/***********************************************************************/


/***********************************************************************/
/**
 * sfixed_mpool_{salloc|sfree} should only called by sallocator interface
 * sfixed_mpool_srealloc is an assert only hook.
 */
FEBIRD_DLL_EXPORT void sfixed_mpool_init   (struct sfixed_mpool* mp);
FEBIRD_DLL_EXPORT void sfixed_mpool_destroy(struct sfixed_mpool* mp);
/***********************************************************************/


/***********************************************************************/
/**
 * mpool_{salloc|sfree} may called by function name, or by interface
 */
FEBIRD_DLL_EXPORT void mpool_init   (struct mpool* mp);
FEBIRD_DLL_EXPORT void mpool_destroy(struct mpool* mp);

FEBIRD_DLL_EXPORT void* mpool_salloc(struct mpool* mp, size_t size);
FEBIRD_DLL_EXPORT void  mpool_sfree (struct mpool* mp, void* ptr, size_t size);

FEBIRD_DLL_EXPORT size_t mpool_used_cells(const struct mpool* mp);
FEBIRD_DLL_EXPORT size_t mpool_used_bytes(const struct mpool* mp);
/***********************************************************************/


/***********************************************************************/
FEBIRD_DLL_EXPORT struct mpool* mpool_get_global(void);

FEBIRD_DLL_EXPORT void* gsalloc(size_t size);
FEBIRD_DLL_EXPORT void gsfree(void* ptr, size_t size);
/***********************************************************************/

#ifdef __cplusplus
}
#endif


//just for unify for buffer

/**  
 * buffer init you want to use ,using it before
 * @param void
 * @return void
 */ 
 
void buffer_init();

/**  
 * you don't use it ,using it after
 * @param void
 * @return void
 */ 
void buffer_destroy();

/**  
 * apply a new buffer
 * @param n the buffer size required 
 * @return the pointer to data of the buffer
 */ 
char * buffer_get(size_t n);

/**  
 * release a new buffer memory
 * @param data the pointer to data of the buffer
 */ 
void buffer_release(char *data);

/**  
 * get the size of the buffer
 * @param data the pointer to data of the buffer
 * @return the size of the buffer
 */ 
size_t buffer_size(char *data);

/**  
 * expand a buffer memory
 * @param data the pointer to data of original buffer
 * @param n  size required for the new buffer
 * @return the pointer to data of the new buffer
 */ 
char * buffer_expand(char * data, size_t n);

/** display the current buffer infomation
 * @param argc para num
 * @param argv para content
 * @return status of the display
 */
int	disp_buffer_info(int argc, char * const argv[]);

/** test the buffer using whether OK or not.
 * @param argc para num
 * @param argv para content
 * @return status of the test
 */
int	buffer_test(int argc, char * const argv[]);
//add by wjw for test

//#define WJW_DEBUG

#endif // _bufferl_h_


