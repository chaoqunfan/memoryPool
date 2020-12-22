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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "buffer.h"
#include "TestFramework.h"

#ifdef _MSC_VER
/* Always compile this module for speed, not size */
#pragma optimize("t", on)
#endif

#ifdef _MSC_VER
#  define FZD "%Id"
#else
#  define FZD "%zd"
#endif

/* this macro not using must be power of 2 and greater than sizeof(void*) */
#define MPOOL_MIN_CELL 8

#define MPOOL_MIN_CHUNK 256//the min of CHUNK num.

//add by wjw for all cell size ,pls note the next must bigger than before. it must equal the struct  big_block_header's size
struct cell_attr
{
	struct fixed_mpool *my_fix; //the cell belong to the fixpool
	struct fixed_mpool *reserved_fix; //not using just for equal the struct big_block_header's size
	size_t infact_size;//infact size of user need
	size_t used;//not using just for equal the struct big_block_header's size
};
#define MAX_CELL_SIZE 2048 //the max size of cell
#define CHUNK_SIZE 4096 //the size of each chunk
size_t all_cell_size[]={64,256,512,1024,MAX_CELL_SIZE};//using it for product fixpool for each content,you can modify it ,pls note all_cell_size[n]<all_cell_size[n+1],and the last one must be macro MAX_CELL_SIZE .



struct mpool_cell
{
	struct cell_attr cellattr;//add by wjw 
	struct mpool_cell* next;
};

struct mpool_chunk
{
	struct mpool_cell* cell; // cell array
	size_t size; // size in bytes;
	char *head;//add by wjw for addr head.
};

/**********************************************************************************/
/**
 * sallocator use malloc/free
 */
static void* malloc_salloc(struct sallocator* sa, size_t size)
{
	void* buffer;
	buffer=malloc(size);
	memset(buffer,0,size);
	return buffer;
}

static void malloc_sfree(struct sallocator* sa, void* block, size_t size)
{
	free(block);
}

static void* malloc_srealloc(struct sallocator* sa, void* block, size_t old_size, size_t new_size)
{
	return realloc(block, new_size);
}
/**********************************************************************************/

void*  default_srealloc(struct sallocator* sa, void* ptr, size_t old_size, size_t new_size)
{
	void* q = sa->salloc(sa, new_size);
	assert(old_size > 0);
	assert(new_size > 0);
	if (NULL == q) return NULL;
	memcpy(q, ptr, old_size < new_size ? old_size : new_size);
	sa->sfree(sa, ptr, old_size);
	return q;
}


void  init_fixed_pool(struct fixed_mpool* fmp);
static void chunk_init(struct mpool_cell* cell, size_t cell_count, size_t cell_size)
{
	size_t i;
	struct mpool_cell* p = cell;
	assert(cell_size % MPOOL_MIN_CELL == 0);
	for (i = 0; i < cell_count-1; ++i)
		p = p->next = (struct mpool_cell*)((char*)p + cell_size);
	p->next = NULL;
}

//add by wjw for cell attr
static void chunk_init_cellattr(struct fixed_mpool* fmp,struct mpool_cell* cell, size_t cell_count, size_t cell_size,size_t infactsize)
{
	size_t i;
	struct mpool_cell* p = cell;
	p->cellattr.my_fix=fmp;
	p->cellattr.infact_size=infactsize;
	p->cellattr.used=0;
	
	assert(cell_size % MPOOL_MIN_CELL == 0);
	for (i = 0; i < cell_count-1; ++i)
	{
		p = p->next = (struct mpool_cell*)((char*)p + cell_size);
		p->cellattr.my_fix=fmp;
		p->cellattr.infact_size=infactsize;
		p->cellattr.used=0;
		
	}
	p->next = NULL;
}



/**********************************************************************************/
static struct sallocator fal = {
	&malloc_salloc,
	&malloc_sfree,
	&malloc_srealloc
};
#if 0
/**********************************************************************************/
static void* sfixed_mpool_salloc  (struct sfixed_mpool* fmp, size_t size);
static void* sfixed_mpool_srealloc(struct sfixed_mpool* fmp, size_t old_size, size_t new_size);
static void  sfixed_mpool_sfree   (struct sfixed_mpool* fmp, void* ptr, size_t size);
#endif
/**
 * require initialized fields:
 *	 cell_size
 *	 size
 *	 sa [0 OR initialized]
 */
void fixed_mpool_init(struct fixed_mpool* fmp)
{
	if (NULL == fmp->sa)
		fmp->sa = &fal;
	else {
		assert(NULL != fmp->sa->salloc);
		assert(NULL != fmp->sa->sfree);
		if (NULL == fmp->sa->srealloc)
			fmp->sa->srealloc = &default_srealloc;
	}
	assert(fmp->chunk_size > 0);
	assert(fmp->cell_size > 0);
	assert(fmp->cell_size < fmp->chunk_size);

	fmp->cell_size = (fmp->cell_size + MPOOL_MIN_CELL - 1) / MPOOL_MIN_CELL * MPOOL_MIN_CELL;
	fmp->chunk_size = (fmp->chunk_size + MPOOL_MIN_CHUNK - 1) / MPOOL_MIN_CHUNK * MPOOL_MIN_CHUNK;

	if (fmp->nChunks < MPOOL_MIN_CHUNK/sizeof(struct mpool_chunk))
		fmp->nChunks = MPOOL_MIN_CHUNK/sizeof(struct mpool_chunk);



	fmp->iNextChunk = 0;
	fmp->pChunks = NULL;
	fmp->head = NULL;
	fmp->used_cells = 0;
	pthread_mutex_init(&fmp->lock,NULL);
	init_fixed_pool(fmp);
}
#if 0
void sfixed_mpool_init(struct sfixed_mpool* sfmp)
{
	fixed_mpool_init(&sfmp->fmp);
	sfmp->salloc   = (salloc_ft  )&sfixed_mpool_salloc;
	sfmp->sfree    = (sfree_ft   )&sfixed_mpool_sfree;
	sfmp->srealloc = (srealloc_ft)&sfixed_mpool_srealloc;
}
#endif
void fixed_mpool_destroy(struct fixed_mpool* fmp)
{
	if (fmp->pChunks) {
		struct sallocator* sa = fmp->sa;
		ptrdiff_t i;
		for (i = fmp->iNextChunk - 1; i >= 0; --i)
			sa->sfree(sa, fmp->pChunks[i].cell, fmp->pChunks[i].size);
		sa->sfree(sa, fmp->pChunks, sizeof(struct mpool_chunk) * fmp->nChunks);

		fmp->iNextChunk = 0;
		fmp->pChunks = 0;
		fmp->head = NULL;
		fmp->used_cells = 0;
		pthread_mutex_destroy(&fmp->lock);
	//	memset(fmp, 0, sizeof(struct fixed_mpool));
	} else {
	//	fprintf(stderr, "warning: fixed_mpool_destroy: already destroyed or not inited");
	}
}

#if 0
void sfixed_mpool_destroy(struct sfixed_mpool* sfmp)
{
	fixed_mpool_destroy(&sfmp->fmp);
}
#endif

//add by wjw for cell attr 
struct mpool_cell* fixed_mpool_add_chunk_cellattr(struct fixed_mpool* fmp,size_t infactsize)
{
	struct mpool_cell* cell;
	
	if (febird_unlikely(NULL == fmp->pChunks)) {
		size_t size = sizeof(struct mpool_chunk) * fmp->nChunks;
		fmp->pChunks = (struct mpool_chunk*)fmp->sa->salloc(fmp->sa, size);
		if (NULL == fmp->pChunks)
			{
				pthread_mutex_unlock(&fmp->lock);	
				return NULL;
			}
	} else if (febird_unlikely(fmp->iNextChunk == fmp->nChunks)) {
		size_t old_size = sizeof(struct mpool_chunk) * fmp->nChunks;
		size_t new_size = 2 * old_size;

		// allocate new chunk array
		struct mpool_chunk* c = (struct mpool_chunk*)
			fmp->sa->srealloc(fmp->sa, fmp->pChunks, old_size, new_size);

		if (NULL == c) 
			{
				pthread_mutex_unlock(&fmp->lock);	
				return NULL;
			}
		fmp->pChunks = c;
		fmp->nChunks *= 2;     // chunk array expanded 2
		 fmp->chunk_size *= 2;  // chunk_size  expanded 2 also
	}

	// allocate a new cell array

	cell = (struct mpool_cell*)fmp->sa->salloc(fmp->sa, fmp->chunk_size);
	if (NULL == cell)
	{
		pthread_mutex_unlock(&fmp->lock);
		return NULL;
	}
	fmp->pChunks[fmp->iNextChunk].cell = cell;
	fmp->pChunks[fmp->iNextChunk].size = fmp->chunk_size;
	fmp->iNextChunk++;
	chunk_init_cellattr(fmp,cell, fmp->chunk_size / fmp->cell_size, fmp->cell_size,infactsize);

	/* alloc cell */
	fmp->used_cells++;
	if(fmp->used_cells>fmp->used_max)
		fmp->used_max=fmp->used_cells;
	fmp->head = cell->next;
	cell->cellattr.used=1;
	pthread_mutex_unlock(&fmp->lock);	

	return (struct mpool_cell*)((char *) cell+sizeof(struct cell_attr));
}


//add by wjw for init the fix pool

void  init_fixed_pool(struct fixed_mpool* fmp)
{
	struct mpool_cell* cell;
	 
	size_t size = sizeof(struct mpool_chunk) * fmp->nChunks;
	fmp->pChunks = (struct mpool_chunk*)fmp->sa->salloc(fmp->sa, size);
	if (NULL == fmp->pChunks)
		return  ;
	 
 
	cell = (struct mpool_cell*)fmp->sa->salloc(fmp->sa, fmp->chunk_size);
	if (NULL == cell) return  ;
	fmp->pChunks[fmp->iNextChunk].cell = cell;
	fmp->pChunks[fmp->iNextChunk].size = fmp->chunk_size;
	fmp->iNextChunk++;
	chunk_init(cell, fmp->chunk_size / fmp->cell_size, fmp->cell_size);

	/* alloc cell */
	fmp->used_cells=0;
	fmp->used_max=0;
	fmp->head = cell;
 
}

//add by wjw for cell attr

#define FIXED_MPOOL_IMPL_ALLOC_CELL_ATTR(fmp,size) 	\
{										\
	struct mpool_cell* cell = fmp->head;\
	if (febird_likely(NULL != cell)) {	\
		fmp->used_cells++;				\
		if(fmp->used_cells>fmp->used_max)	\
			fmp->used_max=fmp->used_cells;	\
		fmp->head = cell->next;			\
		cell->cellattr.my_fix=fmp;	\
		cell->cellattr.infact_size=size;	\
		cell->cellattr.used=1;	\
		pthread_mutex_unlock(&fmp->lock);	\
		return ( struct mpool_cell*)( (char *)cell+sizeof(struct cell_attr));					\
	}									\
	return fixed_mpool_add_chunk_cellattr(fmp,size);	\
}

#define FIXED_MPOOL_IMPL_FREE(fmp, ptr)	\
{										\
	struct mpool_cell* cell = (struct mpool_cell*)ptr; \
	(cell->cellattr).used=0;		\
	cell->next = fmp->head;	\
	fmp->used_cells--;		\
	fmp->head = cell;		\
	pthread_mutex_unlock(&fmp->lock); 	\
}


#if 0



struct mpool_cell* fixed_mpool_add_chunk(struct fixed_mpool* fmp)
{
	struct mpool_cell* cell;
	
	if (febird_unlikely(NULL == fmp->pChunks)) {
		size_t size = sizeof(struct mpool_chunk) * fmp->nChunks;
		fmp->pChunks = (struct mpool_chunk*)fmp->sa->salloc(fmp->sa, size);
		if (NULL == fmp->pChunks)
			return NULL;
	} else if (febird_unlikely(fmp->iNextChunk == fmp->nChunks)) {
		size_t old_size = sizeof(struct mpool_chunk) * fmp->nChunks;
		size_t new_size = 2 * old_size;

		// allocate new chunk array
		struct mpool_chunk* c = (struct mpool_chunk*)
			fmp->sa->srealloc(fmp->sa, fmp->pChunks, old_size, new_size);

		if (NULL == c) return NULL;
		fmp->pChunks = c;
		fmp->nChunks *= 2;     // chunk array expanded 2
		 fmp->chunk_size *= 2;  // chunk_size  expanded 2 also
	}

	// allocate a new cell array

	cell = (struct mpool_cell*)fmp->sa->salloc(fmp->sa, fmp->chunk_size);
	if (NULL == cell) return NULL;
	fmp->pChunks[fmp->iNextChunk].cell = cell;
	fmp->pChunks[fmp->iNextChunk].size = fmp->chunk_size;
	fmp->iNextChunk++;
	chunk_init(cell, fmp->chunk_size / fmp->cell_size, fmp->cell_size);

	/* alloc cell */
	fmp->used_cells++;
	fmp->head = cell->next;

	return cell;
}

#define FIXED_MPOOL_IMPL_ALLOC(fmp) 	\
{										\
	struct mpool_cell* cell = fmp->head;\
	if (febird_likely(NULL != cell)) {	\
		fmp->used_cells++;				\
		fmp->head = cell->next;			\
		return cell;					\
	}									\
	return fixed_mpool_add_chunk(fmp);	\
}
/***************************************************************/



/***************************************************************/
void* fixed_mpool_alloc(struct fixed_mpool* fmp)
FIXED_MPOOL_IMPL_ALLOC(fmp)


void*
sfixed_mpool_salloc(struct sfixed_mpool* sfmp, size_t size)
{
	if (febird_unlikely(size > sfmp->fmp.cell_size)) {
		fprintf(stderr, "fatal: sfixed_mpool_salloc:[cell_size="FZD", request_size="FZD"]\n", sfmp->fmp.cell_size, size);
		abort();
	}
	FIXED_MPOOL_IMPL_ALLOC((&sfmp->fmp))
}


void fixed_mpool_free(struct fixed_mpool* fmp, void* ptr)
FIXED_MPOOL_IMPL_FREE(fmp, ptr)


void sfixed_mpool_sfree(struct sfixed_mpool* sfmp, void* ptr, size_t size)
{
	if (febird_unlikely(size > sfmp->fmp.cell_size)) {
		fprintf(stderr, "fatal: sfixed_mpool_sfree:[cell_size="FZD", request_size="FZD"]\n", sfmp->fmp.cell_size, size);
		abort();
	}
	FIXED_MPOOL_IMPL_FREE((&sfmp->fmp), ptr)
}

static void* sfixed_mpool_srealloc(struct sfixed_mpool* fmp, size_t old_size, size_t new_size)
{
	fprintf(stderr, "fatal: sfixed_mpool_srealloc: this function should not be called\n");
	abort();
	return NULL; // avoid warning
}
#endif
/**********************************************************************************/
/**
 * init mpool and destroy mpool
 *   
 *	 
 *	
 */
void mpool_init(struct mpool* mp)
{
	size_t i, nFixed;
	struct sallocator* al;
	
	assert(mp->max_cell_size < mp->chunk_size);

	if (NULL == mp->salloc)
		al = mp->sa = &fal;
	else {
		al = mp->sa;
		assert(NULL != al->salloc);
		assert(NULL != al->sfree);
		if (NULL == al->srealloc)
			al->srealloc = &default_srealloc;
	}
	mp->salloc = (salloc_ft)&mpool_salloc;
	mp->sfree = (sfree_ft)&mpool_sfree;
	mp->srealloc = (srealloc_ft)&default_srealloc;

	mp->max_cell_size = (mp->max_cell_size + MPOOL_MIN_CELL - 1) / MPOOL_MIN_CELL * MPOOL_MIN_CELL;
	mp->chunk_size = (mp->chunk_size + MPOOL_MIN_CHUNK - 1) / MPOOL_MIN_CHUNK * MPOOL_MIN_CHUNK;
	nFixed = sizeof(all_cell_size)/sizeof(all_cell_size[0]);//modify by wjw nFixed = mp->max_cell_size / MPOOL_MIN_CELL;

	mp->fixed = (struct fixed_mpool*)al->salloc(al, sizeof(struct fixed_mpool) * nFixed);
	if (NULL == mp->fixed) {
		fprintf(stderr, "fatal: febird.mpool_init[max_cell_size="FZD", size="FZD"]\n"
				, mp->max_cell_size, mp->chunk_size);
		abort();
	}


	for (i = 0; i < nFixed; ++i)
	{
		mp->fixed[i].cell_size =all_cell_size[i];
		mp->fixed[i].chunk_size = mp->chunk_size;
		mp->fixed[i].nChunks = 16;
		mp->fixed[i].sa = mp->sa;
		fixed_mpool_init(&mp->fixed[i]);
	}
//	if (mp->big_flags & FEBIRD_MPOOL_ALLOW_BIG_BLOCK) {
//	always init
		mp->big_blocks = 0;
		mp->big_list.prev = mp->big_list.next = &mp->big_list;
		mp->big_list.size = 0;
		mp->big_list.used= 0;
		pthread_mutex_init(&mp->big_lock,NULL);
		// pthread_mutex_init(&mp->check_ptr_lock, NULL);
//	}
}

void mpool_destroy(struct mpool* mp)
{
	size_t i, nFixed;
	if (NULL == mp->fixed) {
		fprintf(stderr, "fatal: febird.mpool_destroy: not inited or has already destroyed\n");
		return;
	}
	if (mp->big_flags & FEBIRD_MPOOL_ALLOW_BIG_BLOCK)
	{
		pthread_mutex_destroy(&mp->big_lock);
		// pthread_mutex_destroy(&mp->check_ptr_lock);
		if (mp->big_flags & FEBIRD_MPOOL_AUTO_FREE_BIG)
		{
			size_t total_size = 0;
			struct big_block_header *p;
			for (i = 0, p = mp->big_list.next; p != &mp->big_list; ++i)
			{
				struct big_block_header *q = p->next;
				total_size += p->size;
				if (mp->big_flags & FEBIRD_MPOOL_MALLOC_BIG)
					free(p);
				else
					mp->sa->sfree(mp->sa, p, p->size);
				p = q;
			}
			if (i != mp->big_blocks || total_size != mp->big_list.size)
			{
				fprintf(stderr
					, "fatal: febird.mpool_destroy: bad track list, big_blocks="FZD", i="FZD"\n"
					, mp->big_blocks, i
					);
			}
		} else {
			if (mp->big_blocks)
				fprintf(stderr
					, "warning: mpool_destroy: memory leak big blocks="FZD"\n"
					, mp->big_blocks
					);
		}
	}
	nFixed=sizeof(all_cell_size)/sizeof(all_cell_size[0]);//modify by wjw nFixed = mp->max_cell_size / MPOOL_MIN_CELL;
	for (i = 0; i < nFixed; ++i)
		fixed_mpool_destroy(&mp->fixed[i]);
	mp->sa->sfree(mp->sa, mp->fixed, sizeof(struct fixed_mpool) * nFixed);

	mp->fixed = NULL;
}


/**********************************************************************************/
/**
 * operation of big block
 *   
 *	 
 *	
 */
static  void* mpool_salloc_big(struct mpool* mp, size_t size)
{
	pthread_mutex_lock(&mp->big_lock);
	if (mp->big_flags & FEBIRD_MPOOL_ALLOW_BIG_BLOCK)
   	{
		// this is rare case
		struct big_block_header *p, *h;
		
		if (mp->big_flags & FEBIRD_MPOOL_AUTO_FREE_BIG)
			size += sizeof(struct big_block_header);
		p = (struct big_block_header*)
			( mp->big_flags & FEBIRD_MPOOL_MALLOC_BIG
			? malloc(size)
			: mp->sa->salloc(mp->sa, size)
			);
		if (p) {
			if (mp->big_flags & FEBIRD_MPOOL_AUTO_FREE_BIG) {
				h = &mp->big_list;
				size-= sizeof(struct big_block_header);//add by wjw 
				p->size = size;
				p->prev = h;
				p->next = h->next;
				h->next->prev = p;
				h->next = p;

				h->size += size; // accumulate size in list header
				mp->big_blocks++;
				p->used=1;
				pthread_mutex_unlock(&mp->big_lock);
				return (p + 1);
			} else
				{
					pthread_mutex_unlock(&mp->big_lock);
					return p;
				}
		} else
			{
				pthread_mutex_unlock(&mp->big_lock);
				return NULL;
			}
	} else {
		fprintf(stderr, "fatal: mpool_salloc: [size="FZD", max_cell_size="FZD"]\n", size, mp->max_cell_size);
		pthread_mutex_unlock(&mp->big_lock);
		abort();
	}
	assert(0);
	return NULL; // avoid warnings
}

static void mpool_sfree_big(struct mpool* mp, void* ptr, size_t size)
{
	pthread_mutex_lock(&mp->big_lock);
	//printf("%s %d ",__FUNCTION__,__LINE__);
	if (mp->big_flags & FEBIRD_MPOOL_ALLOW_BIG_BLOCK)
   	{
		if (mp->big_flags & FEBIRD_MPOOL_AUTO_FREE_BIG)
	   	{
			// this is rare case
			struct big_block_header* bbh = (struct big_block_header*)ptr - 1;
			bbh->prev->next = bbh->next;
			bbh->next->prev = bbh->prev;
			if (size  != bbh->size) {//modify by wjw if (size + sizeof(struct big_block_header) != bbh->size) {
				fprintf(stderr, "fatal: mpool_sfree: size_error[recored="FZD", passed="FZD"]\n"
						, bbh->size, size + sizeof(struct big_block_header));
			}
			size = bbh->size;
			ptr = bbh;
			mp->big_list.size -= size; // accumulate size in list header
		}
		mp->big_blocks--;

		if (mp->big_flags & FEBIRD_MPOOL_MALLOC_BIG)
			free(ptr);
		else
			mp->sa->sfree(mp->sa, ptr, size);
	} else {
		fprintf(stderr, "fatal: mpool_sfree: [size="FZD", max_cell_size="FZD"]\n", size, mp->max_cell_size);
		pthread_mutex_unlock(&mp->big_lock);
		abort();
	}
	pthread_mutex_unlock(&mp->big_lock);
	return;
}


/**********************************************************************************/
/**
 * operation of fixpool's cell
 *   
 *	 
 *	
 */
void* mpool_salloc(struct mpool* mp, size_t size)
{
	assert(size > 0);
	if (febird_likely(size <= (mp->max_cell_size -sizeof(struct cell_attr)))) {
		int i;
		for(i=0;i<sizeof(all_cell_size)/sizeof(all_cell_size[0]);i++)
			{
				if((all_cell_size[i]-sizeof(struct cell_attr))>=(size))
					break;
			}
		struct fixed_mpool* fmp = &mp->fixed[i];
		pthread_mutex_lock(&fmp->lock);
		

		//modify by wjw 
		/*size_t idx = (size - 1) / MPOOL_MIN_CELL;
		struct fixed_mpool* fmp = &mp->fixed[idx];*/
		FIXED_MPOOL_IMPL_ALLOC_CELL_ATTR(fmp,size)//modify by wjw FIXED_MPOOL_IMPL_ALLOC(fmp)
	} else
		return mpool_salloc_big(mp, size);
}

void mpool_sfree(struct mpool* mp, void* ptr, size_t size)
{
	assert(size > 0);
	if (febird_likely(size <= mp->max_cell_size)) {
		int i;
		for(i=0;i<sizeof(all_cell_size)/sizeof(all_cell_size[0]);i++)
			{
				if(all_cell_size[i]>=size)
				break;
			}
		struct fixed_mpool* fmp = &mp->fixed[i];
		
		/*modify by wjw size_t idx = (size - 1) / MPOOL_MIN_CELL;
		struct fixed_mpool* fmp = &mp->fixed[idx];*/
		FIXED_MPOOL_IMPL_FREE(fmp, ptr)
	} else
		mpool_sfree_big(mp, ptr, size);
}

//add by wjw for cell attr 
void mpool_sfree_cellattr(struct mpool* mp, void* ptr)
{
	size_t size;
	struct big_block_header* bbh = (struct big_block_header*)ptr - 1;//using struct cell_attr
	struct cell_attr* attr = (struct cell_attr*) bbh;
	size=bbh->size;
	

	if (febird_likely(size <= (mp->max_cell_size - sizeof(struct cell_attr)))) {

		struct fixed_mpool* fmp =attr->my_fix;
		pthread_mutex_lock(&fmp->lock);
		
	
		
		/*modify by wjw size_t idx = (size - 1) / MPOOL_MIN_CELL;
		struct fixed_mpool* fmp = &mp->fixed[idx];*/
		FIXED_MPOOL_IMPL_FREE(fmp, attr)
	} else
		mpool_sfree_big(mp, ptr, size);
}


/**********************************************************************************/
/**
 * statistics of mpool's cells and bytes
 *   
 *	 
 *	
 */
#if 0
size_t mpool_used_cells(const struct mpool* mp)
{
	size_t i,n = sizeof(all_cell_size)/sizeof(all_cell_size[0]); //modify by wjw n = mp->max_cell_size / MPOOL_MIN_CELL;
	size_t used = 0;
	for (i = 0; i < n; ++i)
		used += mp->fixed[i].used_cells;
	return used;
}

size_t mpool_used_bytes(const struct mpool* mp)
{
	size_t i,n = sizeof(all_cell_size)/sizeof(all_cell_size[0]); //modify by wjw n = mp->max_cell_size / MPOOL_MIN_CELL;
	size_t used = 0;
	for (i = 0; i < n; ++i)
		used += mp->fixed[i].cell_size * mp->fixed[i].used_cells;
	return used;
}

static struct mpool global_mpool = {0};
static void destroy_global_mpool(void)
{
	if (global_mpool.fixed) {
		size_t used = mpool_used_cells(&global_mpool);
		if (used) {
			fprintf(stderr, "warning: memory leak in global_mpool\n");
		}
		mpool_destroy(&global_mpool);
	}
}

struct mpool* mpool_get_global(void)
{
	if (NULL == global_mpool.fixed) {
		global_mpool.chunk_size = 4096;
		global_mpool.max_cell_size = 256;
		global_mpool.sa = &fal;
		global_mpool.big_flags = 0
			|FEBIRD_MPOOL_ALLOW_BIG_BLOCK
			|FEBIRD_MPOOL_AUTO_FREE_BIG
			|FEBIRD_MPOOL_MALLOC_BIG
			;
		mpool_init(&global_mpool);
		atexit(&destroy_global_mpool);
	}
	return &global_mpool;
}

void* gsalloc(size_t size)
{
	return mpool_salloc(&global_mpool, size);
}

void gsfree(void* ptr, size_t size)
{
	mpool_sfree(&global_mpool, ptr, size);
}
#endif
//---------------add by wjw for debug start ---------------------

void  mpool_cells_fixpool(const struct mpool* mp)
{
	size_t i, n =  sizeof(all_cell_size)/sizeof(all_cell_size[0]); //modify by wjw n = mp->max_cell_size / MPOOL_MIN_CELL;
	size_t used = 0,total=0;
	printf("----------------each fixpool's cells content in mpool start ---------------------\n");
	printf("  ID  cell_size  cell_total_num   used_cell_numm  used_cell_max\n");
	for (i = 0; i < n; ++i)
	{
		//used += mp->fixed[i].used_cells;
		total=mp->fixed[i].nChunks*mp->fixed[i].chunk_size/mp->fixed[i].cell_size;
		used=mp->fixed[i].used_cells;
		printf("%4d  %9d  %14d  %13d  %13d\n",i,all_cell_size[i],total,used,mp->fixed[i].used_max);

	}
	//printf("----------------each fixpool's cells content in mpool end ---------------------\n");
	
}


void  mpool_bigs_mem(const struct mpool* mp)
{
	size_t i ;
	struct big_block_header *head, *tmp;

	if((&mp->big_list)==mp->big_list.next)
	{
		printf("no bigger than MAX_CELL_SIZE = %d \n ",MAX_CELL_SIZE);
		return;
	}
	head=(struct big_block_header *)(&(mp->big_list));
	printf("----------------big list in mpool start ---------------------\n");
	printf("  ID  buffer_size \n");
	for (tmp=head->next,i=0; tmp!=(&mp->big_list); tmp=tmp->next)
	{
	
		printf("%4d  %11d \n",i,tmp->size);
		i++;

	}
	printf("total size of list is   %d            ------------end---------- \n",head->size);
	//printf("----------------each fixpool's cells content in mpool end ---------------------\n");
	
}





#if 0
//this function cannot used current.
size_t mpool_bytes_fixpool(const struct mpool* mp)
{
	size_t i, j, n = sizeof(all_cell_size)/sizeof(all_cell_size[0]); //modify by wjw n = mp->max_cell_size / MPOOL_MIN_CELL;
	size_t used = 0;
	for (i = 0; i < n; ++i)
	{
		used += mp->fixed[i].cell_size * mp->fixed[i].used_cells;
		for(j=0;j<=mp->fixed[i].nChunks;j++)
		{
			
		}
	}
	return used;
}
#endif
//---------------add by wjw for debug end ---------------------







static struct mpool static_golbal_mpool; //this is the global mpool.
//extern size_t all_cell_size[];


/*
return 0 is ok,-1 not ok
*/
int check_ptr(char *data)
{
	//size_t size;
	size_t i, j,nFixed,flag;
	flag=0;//0 not in,1 in
	struct big_block_header *p = NULL;
	struct big_block_header* bbh = (struct big_block_header*)data - 1;//using struct cell_attr
	struct cell_attr* attr = (struct cell_attr*) bbh;
	//printf("%s %d ",__FUNCTION__,__LINE__);
	//fflush(stdout);

	nFixed = sizeof(all_cell_size)/sizeof(all_cell_size[0]);
	for(i=0;i<nFixed;i++)
	{

		for(j=0;j<static_golbal_mpool.fixed[i].iNextChunk;j++)
		{
			if((char *)(attr)>=(char *)(static_golbal_mpool.fixed[i].pChunks[j].cell) && 
				(char *)attr<=((char *)(static_golbal_mpool.fixed[i].pChunks[j].cell) +static_golbal_mpool.fixed[i].pChunks[j].size) )
			{
				flag=1;
				break;
			}
		}
		if(flag==1)
			break;
			
	}
	pthread_mutex_lock(&static_golbal_mpool.big_lock);
	p=static_golbal_mpool.big_list.next;
	if(flag==0)
	{
		while(p!=NULL && p!=&(static_golbal_mpool.big_list))
		{
			if((char *)(p+1)==data)
				{
					flag=1;
					break;
				}
			p=p->next;
		}
	}
	pthread_mutex_unlock(&static_golbal_mpool.big_lock);
	if(flag==0)
		return -1;

	if(attr->used !=1)
		{	
			//printf("%s %d ",__FUNCTION__,__LINE__);
			//fflush(stdout);
			return -1;
		}

	//printf("%s %d ",__FUNCTION__,__LINE__);
	//fflush(stdout);
	/*
	nFixed = sizeof(all_cell_size)/sizeof(all_cell_size[0]);
	for(i=0;i<nFixed;i++)
	{
		if(attr->my_fix==&(static_golbal_mpool.fixed[i]))
			{
				printf("%s %d ",__FUNCTION__,__LINE__);
				return 0;
			}
			
	}*/


	return 0;
	

}




/**  
 * apply a new buffer
 * @param n the buffer size required 
 * @return the pointer to data of the buffer
 */ 
char * buffer_get(size_t n)
{
	return mpool_salloc(&static_golbal_mpool,n);
}

/**  
 * release a new buffer memory
 * @param data the pointer to data of the buffer
 */ 
void buffer_release(char *data)
{
	int ret=0;
	//printf("%s %d ",__FUNCTION__,__LINE__);
	//fflush(stdout);
	assert(data!=NULL && "buffer_release:pointer cannot be NULL");
	//printf("%s %d ",__FUNCTION__,__LINE__);
	//fflush(stdout);
	ret=check_ptr(data);
	//printf("%s %d ",__FUNCTION__,__LINE__);
	//fflush(stdout);
	assert(ret==0 && "buffer_release :pointer must  be in memory pool");
	//printf("%s %d ",__FUNCTION__,__LINE__);
	//fflush(stdout);
	mpool_sfree_cellattr(&static_golbal_mpool,data);
}

/**  
 * get the size of the buffer
 * @param data the pointer to data of the buffer
 * @return the size of the buffer
 */ 
size_t buffer_size(char *data)
{
	size_t size;
	struct big_block_header* bbh = (struct big_block_header*)data - 1;//using struct cell_attr is the same.
	size=bbh->size;
	return size;
}

/**  
 * expand a buffer memory
 * @param data the pointer to data of original buffer
 * @param n  size required for the new buffer
 * @return the pointer to data of the new buffer
 */ 
char * buffer_expand(char * data, size_t n)
{

	int ret=0;
	assert(data!=NULL && "buffer_expand :pointer cannot be NULL");
	ret=check_ptr(data);
	assert(ret==0 && "buffer_expand :pointer must  be in memory pool");
	
	size_t cell_size;
	char * tmp;
	struct cell_attr* cellattr = (struct cell_attr*)data - 1;//using struct cell_attr is the same.

	cell_size=cellattr->my_fix->cell_size;
	if((cell_size-sizeof(struct cell_attr))>=n)
	{
		cellattr->infact_size = n;
		return data;
	}
	tmp=buffer_get(n);
	memcpy(tmp,data,cell_size-sizeof(struct cell_attr));
	buffer_release(data);
	return	tmp;
	
}


#define USING_MAX_CELLS 1024 //when you use buffer_test  the cell num must litter this value.

#define CELL_NUM 1000  //using test the num of cell in the mpool
#define CELL_SIZE_1 1000 //the fixpool's cell size will be using when buffer_test
#define CELL_SIZE_2 (MAX_CELL_SIZE+1)//the big block size will be using when buffer_test

/**  
 * buffer init you want to use ,using it before
 * @param void
 * @return void
 */ 
void buffer_init()
{
	static_golbal_mpool.chunk_size = CHUNK_SIZE;
	static_golbal_mpool.max_cell_size = MAX_CELL_SIZE;
	static_golbal_mpool.sa = &fal;
	static_golbal_mpool.big_flags =  0
			|FEBIRD_MPOOL_ALLOW_BIG_BLOCK
			|FEBIRD_MPOOL_AUTO_FREE_BIG
			|FEBIRD_MPOOL_MALLOC_BIG;
	mpool_init(&static_golbal_mpool);
}


/**  
 * you don't use it ,using it after
 * @param void
 * @return void
 */ 
void buffer_destroy()
{
	mpool_destroy(&static_golbal_mpool);
}


/** display the current buffer infomation
 * @param argc para num
 * @param argv para content
 * @return status of the display
 */
int	disp_buffer_info(int argc, char * const argv[])
{
	mpool_cells_fixpool(&static_golbal_mpool);
	mpool_bigs_mem(&static_golbal_mpool);
	return 0;
}

/** test the buffer using whether OK or not.
 * @param argc para num
 * @param argv para content
 * @return status of the test
 * @huyf if this buffer_test will be delete,
 * @huyf this function used in UnitTest and bind to command 'tbuf' in monitor.c
 * @huyf include TestFramework.h
 */
int	buffer_test(int argc, char * const argv[])
{
	TEST_INIT(1);
	TEST_CATEGORY("Buffer Test");
	void *buffer[USING_MAX_CELLS];
	int i;
	//printf("size is %d \n",sizeof(all_cell_size)/sizeof(all_cell_size[0]));
	//printf("struct cell_attr=%d struct big_block_header=%d  \n",sizeof(struct cell_attr),sizeof (struct big_block_header));

	
/*		
	test_mpool.chunk_size = 4096;
	test_mpool.max_cell_size = MAX_CELL_SIZE;
	test_mpool.sa = &fal;
	test_mpool.big_flags =  0
			|FEBIRD_MPOOL_ALLOW_BIG_BLOCK
			|FEBIRD_MPOOL_AUTO_FREE_BIG
			|FEBIRD_MPOOL_MALLOC_BIG;
	//mpool_init(&test_mpool);
*/
#if 0
	for(i=0;i<CELL_NUM;i++)
	{

		if(i<CELL_NUM/2)
		{
			buffer[i]=mpool_salloc(&test_mpool,1000);
			mpool_cells_fixpool(&test_mpool);
			mpool_bigs_mem(&test_mpool);

		}
		else
		{
			buffer[i]=mpool_salloc(&test_mpool,MAX_CELL_SIZE+1);
			mpool_cells_fixpool(&test_mpool);
			mpool_bigs_mem(&test_mpool);
		}
	
	}
	
	for(i=0;i<CELL_NUM;i++)
	{

#if 0
		if(i<CELL_NUM/2)
		{
			//mpool_sfree(&test_mpool,buffer[i],1000);
			mpool_sfree_cellattr(&test_mpool,buffer[i]);
			mpool_cells_fixpool(&test_mpool);
			mpool_bigs_mem(&test_mpool);

		}
		else
		{
			mpool_sfree(&test_mpool,buffer[i],MAX_CELL_SIZE+1);
			mpool_cells_fixpool(&test_mpool);
			mpool_bigs_mem(&test_mpool);
		}
#endif 
		mpool_sfree_cellattr(&test_mpool,buffer[i]);
		mpool_cells_fixpool(&test_mpool);
		mpool_bigs_mem(&test_mpool);
		
	}

#endif 
	for(i=0;i<CELL_NUM;i++)
	{

		if(i<CELL_NUM/2)
		{
			buffer[i]=buffer_get(CELL_SIZE_1);
			//mpool_cells_fixpool(&test_mpool);
			//mpool_bigs_mem(&test_mpool);

			
		}
		else
		{
			buffer[i]=buffer_get(CELL_SIZE_2);
			//mpool_cells_fixpool(&test_mpool);
			//mpool_bigs_mem(&test_mpool);
		}
	
	}
	TEST((CELL_NUM/2) == static_golbal_mpool.fixed[3].used_cells,  "cell number matched");
	TEST((CELL_SIZE_2*CELL_NUM/2) == static_golbal_mpool.big_list.size,  "big block size  matched");
	memcpy(buffer[0],"12345",6);
	memcpy(buffer[1],"7890",5);
	//printf("before buf0 add is %p content is %s \n",buffer[0],(char*)buffer[0]);
	//printf("before buf1 add is %p content is %s \n",buffer[1],(char*)buffer[1]);
	buffer[0]=buffer_expand(buffer[0],CELL_SIZE_1/2*3);
	buffer[1]=buffer_expand(buffer[1],CELL_SIZE_1+8);
	TEST((CELL_NUM/2-1) == static_golbal_mpool.fixed[3].used_cells,  "cell number matched");
	TEST((1) == static_golbal_mpool.fixed[4].used_cells,  "cell number matched");
	
	//printf("after buf0 add is %p content is %s \n",buffer[0],(char*)buffer[0]);
	//printf("after buf1 add is %p content is %s \n",buffer[1],(char*)buffer[1]);
	//mpool_cells_fixpool(&test_mpool);
	//mpool_bigs_mem(&test_mpool);
	

	for(i=0;i<CELL_NUM;i++)
	{

		//printf("%s %d ",__FUNCTION__,__LINE__);
		//fflush(stdout);
		buffer_release(buffer[i]);
		//printf("release %d\n",i);
		//fflush(stdout);
		//mpool_cells_fixpool(&test_mpool);
		//mpool_bigs_mem(&test_mpool);
		
	}

	for(i=0;i<sizeof(all_cell_size)/sizeof(all_cell_size[0]);i++)
	{


		TEST(0 == static_golbal_mpool.fixed[i].used_cells,  "cell number matched");
		//mpool_cells_fixpool(&test_mpool);
		//mpool_bigs_mem(&test_mpool);
		
	}
	TEST(0 == static_golbal_mpool.big_list.size,  "big block size  matched");

	//char * tmp;
//	tmp=(char *)0x12345678;
	//printf("%s %d  ",__FUNCTION__,__LINE__);
	//fflush(stdout);
	

	//buffer_release(tmp);
	

	
	
	//mpool_destroy(&test_mpool);


	TEST_DONE(); 	

	return 0;
		
	


	
}


