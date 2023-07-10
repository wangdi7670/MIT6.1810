// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int arr[(PHYSTOP - KERNBASE) / PGSIZE];
} reference_count;


void increment(uint64 pa) {
  acquire(&reference_count.lock);
  reference_count.arr[PA2RCID(pa)]++;
  release(&reference_count.lock);
}


void decrement(uint64 pa) {
  acquire(&reference_count.lock);
  reference_count.arr[PA2RCID(pa)]--;
  release(&reference_count.lock);
}

int get_ref_count(uint64 pa) {
  acquire(&reference_count.lock);
  int a = reference_count.arr[PA2RCID(pa)];
  release(&reference_count.lock);
  return a;
}


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&reference_count.lock, "reference_count");

/*   for (int i = 0; i < (PHYSTOP - KERNBASE) / PGSIZE; i++) {
    reference_count.arr[i] = PHYSTOP;
  } */

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  uint64 pa = (uint64) r;

  if (pa % PGSIZE != 0 || (char *)pa < end || pa >= PHYSTOP) {
    if (pa % PGSIZE != 0) {
      printf("pa % PGSIZE != 0\n");
    }
    else if ((char*) pa < end) {
      printf("pa < end, pa = %p\n", pa);
    }
    else if (pa >= PHYSTOP) {
      printf("pa >= PHYSTOP\n");
    }
    panic("kalloc");
  }

  // Set a page's reference count to 1 when kalloc() allocates it
  acquire(&reference_count.lock);
  reference_count.arr[PA2RCID(pa)] = 1;
  release(&reference_count.lock);

  return (void*)r;
}
