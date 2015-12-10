#include <mem.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/*
    2014 Leonard Kevin McGuire Jr (www.kmcg3413.net) (kmcg3413@gmail.com)
*/
typedef struct _KHEAPBLOCKBM {
	struct _KHEAPBLOCKBM	                *next;
	uint32_t					size;
	uint32_t					used;
	uint32_t					bsize;
  uint32_t                                  lfb;
} KHEAPBLOCKBM;

typedef struct _KHEAPBM {
	KHEAPBLOCKBM			*fblock;
} KHEAPBM;

void k_heapBMInit(KHEAPBM *heap) {
	heap->fblock = 0;
}

int k_heapBMAddBlock(KHEAPBM *heap, uint32_t* addr, uint32_t size, uint32_t bsize) {
	KHEAPBLOCKBM		*b;
	uint32_t				bcnt;
	uint32_t				x;
	uint8_t				*bm;

	b = (KHEAPBLOCKBM*)addr;
	b->size = size - sizeof(KHEAPBLOCKBM);
	b->bsize = bsize;

	b->next = heap->fblock;
	heap->fblock = b;

	bcnt = size / bsize;
	bm = (uint8_t*)&b[1];
	/* clear bitmap */
	for (x = 0; x < bcnt; ++x) {
			bm[x] = 0;
	}
	/* reserve room for bitmap */
	bcnt = (bcnt / bsize) * bsize < bcnt ? bcnt / bsize + 1 : bcnt / bsize;
	for (x = 0; x < bcnt; ++x) {
			bm[x] = 5;
	}
	b->lfb = bcnt - 1;

	b->used = bcnt;

	return 1;
}

static uint8_t k_heapBMGetNID(uint8_t a, uint8_t b) {
	uint8_t		c;
	for (c = a + 1; c == b || c == 0; ++c);
	return c;
}

void *k_heapBMAlloc(KHEAPBM *heap, uint32_t size) {
	KHEAPBLOCKBM		*b;
	uint8_t				*bm;
	uint32_t				bcnt;
	uint32_t				x, y, z;
	uint32_t				bneed;
	uint8_t				nid;

	/* iterate blocks */
	for (b = heap->fblock; b; b = b->next) {
		/* check if block has enough room */
		if (b->size - (b->used * b->bsize) >= size) {

			bcnt = b->size / b->bsize;
			bneed = (size / b->bsize) * b->bsize < size ? size / b->bsize + 1 : size / b->bsize;
			bm = (uint8_t*)&b[1];

			for (x = (b->lfb + 1 >= bcnt ? 0 : b->lfb + 1); x != b->lfb; ++x) {
				/* just wrap around */
				if (x >= bcnt) {
					x = 0;
				}

				if (bm[x] == 0) {
					/* count free blocks */
					for (y = 0; bm[x + y] == 0 && y < bneed && (x + y) < bcnt; ++y);

					/* we have enough, now allocate them */
					if (y == bneed) {
						/* find ID that does not match left or right */
						nid = k_heapBMGetNID(bm[x - 1], bm[x + y]);

						/* allocate by setting id */
						for (z = 0; z < y; ++z) {
							bm[x + z] = nid;
						}

						/* optimization */
						b->lfb = (x + bneed) - 2;

						/* count used blocks NOT bytes */
						b->used += y;

						return (void*)(x * b->bsize + (uint32_t*)&b[1]);
					}

					/* x will be incremented by one ONCE more in our FOR loop */
					x += (y - 1);
					continue;
				}
			}
		}
	}

	return 0;
}

void k_heapBMFree(KHEAPBM *heap, void *ptr) {
	KHEAPBLOCKBM		*b;
	uint32_t*				ptroff;
	uint32_t				bi, x;
	uint8_t				*bm;
	uint8_t				id;
	uint32_t				max;

	for (b = heap->fblock; b; b = b->next) {
		if ((uint32_t*)ptr > (uint32_t*)b && (uint32_t*)ptr < (uint32_t*)b + b->size) {
			/* found block */
			ptroff = (uint32_t*)((uint32_t*)ptr - (uint32_t*)&b[1]);  /* get offset to get block */
			/* block offset in BM */
			bi = (uint32_t)ptroff / b->bsize;
			/* .. */
			bm = (uint8_t*)&b[1];
			/* clear allocation */
			id = bm[bi];
			/* oddly.. GCC did not optimize this */
			max = b->size / b->bsize;
			for (x = bi; bm[x] == id && x < max; ++x) {
				bm[x] = 0;
			}
			/* update free block count */
			b->used -= x - bi;
			return;
		}
	}

	/* this error needs to be raised or reported somehow */
	return;
}

KHEAPBM kheap;

void memmgmt_init(struct multiboot_mmap_entry* mmap, int mmap_size)
{
  puts("Heap initialization...");
  if(kheap.fblock == NULL)
  {
    k_heapBMInit(&kheap);
  }
  for(int i = 0; i < mmap_size; i++)
  {
    if(mmap[i].type == 1)
    {
      k_heapBMAddBlock(&kheap, (uint32_t*)(uint32_t)mmap[i].addr, mmap[i].len, 16);
      putc('.');
      break;
    }
  }
  puts("done!\n");
  /*int found = 0;
  for(int i = 0; i < mmap_size; i++)
  {
    if(mmap[i].type == 1)
    {
      if(found)
      {
        mmgmt_conf.address = mmap[i].addr;
        mmgmt_conf.blocks_n = mmap[i].len / 16384;
        if(mmgmt_conf.blocks_n > 2048)
        {
          mmgmt_conf.blocks_n = 2048;
        }
      }
      else
      {
        found = 1;
      }
    }
    //mmap_entry += (mmap_entry->len + sizeof(struct multiboot_mmap_entry));
  }
  if(found && mmgmt_conf.address == 0)
  {
    puts("Memory Manager init failed, free block not found\n");
  }
  else
  {
    char buffer[64];
    puts("Memory Manager set up with parameters: \n  Address: ");
    itoa(mmgmt_conf.address, buffer, 16);
    puts(buffer);
    puts("\n  Number of 16k blocks: ");
    itoa(mmgmt_conf.blocks_n, buffer, 10);
    puts(buffer);
    putc('\n');
  }*/
}

void* mmgmt_alloc(size_t size)
{
  return k_heapBMAlloc(&kheap, size);
}

void mmgmt_free(void* ptr)
{
  k_heapBMFree(&kheap, ptr);
}
