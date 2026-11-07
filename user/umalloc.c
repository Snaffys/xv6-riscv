#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

/*
// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
  Header *bp, *p;

  bp = (Header *)ap - 1;
  for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if (bp + bp->s.size == p->s.ptr) {
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if (p + p->s.size == bp) {
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header *
morecore(uint nu)
{
  char *p;
  Header *hp;

  if (nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if (p == SBRK_ERROR)
    return 0;
  hp = (Header *)p;
  hp->s.size = nu;
  free((void *)(hp + 1));
  return freep;
}

void *
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;
  if ((prevp = freep) == 0) {
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr) {
    if (p->s.size >= nunits) {
      if (p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void *)(p + 1);
    }
    if (p == freep)
      if ((p = morecore(nunits)) == 0)
        return 0;
  }
}
*/


const int sfl_debug = 1;

#define USED 1
#define NLISTS 12

typedef struct block {
  uint size;
  uint prev_size;
} block;

typedef struct free_block {
  block block;
  struct free_block *prev, *next;
} free_block;

#define MINBLOCKSIZE sizeof(free_block)

char *heap_start, *heap_end;
block *last_block;
free_block *free_block_lists[NLISTS];

uint block_size(block *block) {
  return block->size & ~USED;
}

int used(block *block) {
  return block->size & USED;
}

block *next_block(block *block) {
  char *next_block_ptr = (char *)block + block_size(block);
    
  if (next_block_ptr < heap_end) {
    return (block *)next_block_ptr;
  } else {
    return 0;
  }
}

block *prev_block(block *b) {
  if (b->prev_size != 0) {
    return (block *)((char *)b - b->prev_size);
  } else {
    return 0;
  }
}

static int list_for_block(uint block_size) {
  int list = 0;

  while (block_size >>= 1) {
    ++list;
  }

  if (list >= NLISTS) {
    list = NLISTS - 1;
  }
  return list;
}

static void add_block_to_free_lists(block *b) {
  free_block *fb = (free_block *)b;
  int list_idx = list_for_block(block_size(b));
  fb->prev = 0;
  fb->next = free_block_lists[list_idx];
  if (fb->next) { 
    fb->next->prev = fb;
  }
  free_block_lists[list_idx] = fb;
}

static void remove_block_from_free_lists(block *block) {
  free_block *fb = (free_block *)block;
  
  if (fb->prev) {
    fb->prev->next = fb->next; 
  } else {
    int list_idx = list_for_block(block_size(block));
    free_block_lists[list_idx] = fb->next;
  }

  if (fb->next) {
    fb->next->prev = fb->prev;
  }
}

static block *find_block(uint needed_size) {
  for (int list_idx = list_for_block(needed_size); list_idx < NLISTS; ++list_idx) {
    for (free_block *fb = free_block_lists[list_idx]; fb; fb = fb->next) {
      if (block_size(&fb->block) >= needed_size) {
        return &fb->block;
      }
    }
  }
  return 0;
}

static int more_heap(uint needed_size) {
  uint requested_size;

  if (last_block && !used(last_block)) {
    requested_size = needed_size - block_size(last_block);
  } else {
    requested_size = needed_size;
  }

  uint allocated_bytes = PGROUNDUP(requested_size);

  char *allocated_ptr = sbrk(allocated_bytes);
  if (allocated_ptr == SBRK_ERROR) {
    return -1;
  }
  if (allocated_ptr != heap_end) {           
    sbrk(-(int)allocated_bytes);
    return -1;
  }
  heap_end += allocated_bytes;

  if (last_block && !used(last_block)) {
    remove_block_from_free_lists(last_block);
    last_block->size += allocated_bytes;
    add_block_to_free_lists(last_block);
  } else {
    block *b = (block *)allocated_ptr;
    b->size = allocated_bytes;
    if (last_block) {
      b->prev_size = block_size(last_block);
    } else {
      b->prev_size = 0;
    }
    last_block = b;
    add_block_to_free_lists(b);
  }
  printk("allocated bytes: %d heap_end%p\n", allocated_bytes, heap_end);
  return 0;
}

void *malloc(uint size) {
  if (size == 0) { 
    return 0;
  }
  uint needed_block_size = (size + sizeof(block) + 7) & ~7;
  
  
  if (needed_block_size < MINBLOCKSIZE) {
    needed_block_size = MINBLOCKSIZE;
  }

  if (!heap_start) {
    heap_start = heap_end = sbrk(0);
  }

  block *b = find_block(needed_block_size);
  if (!b) {
    if (more_heap(needed_block_size) == -1) {
      return 0;
    }
    b = find_block(needed_block_size);
  }
  remove_block_from_free_lists(b);

  uint unused_block_part = block_size(b) - needed_block_size;
  if (unused_block_part >= MINBLOCKSIZE) {
    block *ubp = (block *)((char *)b + needed_block_size);
    ubp->size = unused_block_part;
    ubp->prev_size = needed_block_size;
    block *next_b = next_block(ubp);
    if (next_b) {
      next->prevPsize = unused_bllock_part;
    } else {
      last_block = ubp;
    }
    b->size = needed_block_size | USED;
    add_block_to_free_lists(ubp);
  } else {
    b->size |= USED;
  }

  // info
  return (char *)b + sizeof(block);
}

void free(void *ap) {
  if (!ap) {
    return;
  }
  block *b = (block *)((char *)ap - sizeof(block));
  if ((char *)b < heap_start || (char *)ap >= heap_end || !used(b)) {
    printk("wrong pointer: %p\n", ap);
    return;
  }
  b->size &= ~USED;

  block *next_b = next_block(b);
  if (next_b && !used(next_b)) {
    remove_block_from_free_lists(next_b);
    b->size += block_size(next_b);
    if (next_b == last_block) {
      last_block = b;
    }
  }
  block *prev_b = prev_block(b);
  if (prev_b && !used(prev_b)) {
    remove_block_from_free_lists(prev_b);
    prev_b->size += block_size(b);
    if (b == last_block) {
      last_block = prev_b;
    }
    b = prev_b;
  }
  next_b = next_block(b);
  if (next_b) {
    next_b->prev_size = block_size(b);
  }

  if (b == last_block) {                
    char *keep;
    if (((uint64)b & (PGSIZE - 1)) == 0) {
      keep = (char *)b;           
    } else {
      keep = (char *)PGROUNDUP((uint64)b + MINBLOCKSIZE);
    }
    if (keep < heap_end) {
      uint rest_mem = heap_end - keep;
      if (sbrk(-(int)rest_mem) != SBRK_ERROR) {
        heap_end = keep;
        printk("freed memory: %d  heap end: %p\n", rest_mem, heap_end);
        if (keep == (char *)b) {
          last_block = prev_block(b);
          // info
          return;
        }
        
        b->size = keep - (char *)b;
      }
    }
  }
  add_block_to_free_lists(b);
  //info
}
