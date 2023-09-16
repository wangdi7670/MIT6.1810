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
void fill_mems();
void test_fill_mem();
void kfree_old (void *pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct Kmem {
  struct spinlock lock;
  struct run *freelist;
  uint32 n;  // the number of freelist
} kmem;

struct Kmem mems[NCPU];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  fill_mems();
  // test_fill_mem();
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree_old(p);
    kmem.n++;
  }
}

// fill mems
void fill_mems()
{
  uint32 average_pages = kmem.n / NCPU;
  struct run *temp = kmem.freelist;

  for (int i = 0; i < NCPU; i++) {
    char name[6] = "kmem";
    name[4] = '0' + i;
    name[5] = '\0';
    initlock(&mems[i].lock, name);

    mems[i].n = average_pages;
    mems[i].freelist = temp;

    // deal with last one
    if (i == NCPU - 1) {
      mems[i].n = 0;

      while (temp) {
        mems[i].n++;
        temp = temp->next;
      }
      break;
    }

    struct run *end = (struct run*) 0;
    for(int j = 0; j < average_pages; j++) {
      if (j == average_pages - 1) {
        end = temp;
      }
      temp = temp->next;
    }
    if (end) {
      end->next = (struct run *)0;
    }
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree_old(void *pa)
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


// kfree_new
void kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree_new");

  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;


  push_off();

  int id = cpuid();
  if (&mems[id].n < 0) {
    panic("kfree_new, n < 0");
  }

  acquire(&mems[id].lock);
  r->next = mems[id].freelist;
  mems[id].freelist = r;
  mems[id].n++;
  release(&mems[id].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc_old(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


// kalloc_new
void *kalloc ()
{
  push_off();

  int id = cpuid();
  struct run *r = (struct run *) 0;
  int flag = 0;

  if (&mems[id].n < 0) {
    panic("kfree_new, n < 0");
  }

  if (mems[id].n) {
    if (!mems[id].freelist) {
      panic("wrong mems[id].freelist");
    }

    acquire(&mems[id].lock);
    if (mems[id].n) {
      r = mems[id].freelist;
      mems[id].freelist = r->next;
      mems[id].n--;
      flag = 1;
    } 
    release(&mems[id].lock);
  }

  // steal other cpu's
  if (flag == 0) {
    for (int i = 0; i < NCPU && flag == 0; i++) {
      if (i == id) {
        continue;
      }

      if (&mems[i].n < 0) {
        panic("kfree_new, n < 0");
      }

      if (mems[i].n) {
        if (!mems[i].freelist) {
          panic("wrong mems[i].freelist");
        }
      
        acquire(&mems[i].lock);

        if (!mems[i].n) {
          continue;
        }

        r = mems[i].freelist;
        mems[i].freelist = r->next;
        mems[i].n--;
        flag = 1;
        release(&mems[i].lock);
      }
    }
  } 

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void *) r;
}

void test_fill_mem()
{
  uint32 total = kmem.n;
  uint32 average = total / NCPU;
  for (int i = 0; i <= NCPU - 2; i++)
  {
    if (mems[i].n != average) {
      panic("wrong n");
    }

    uint32 count = 0;
    struct run *temp = mems[i].freelist;
    while (temp) {
      count += 1;
      temp = temp->next;
    }

    if (count != average)
    {
      panic("wrong count");
    }
  }

  if (total != average * (NCPU-1) + mems[NCPU-1].n) {
    panic("wrong total");
  }
/*   printf("average = %d\n", average);
  printf("mems[ncpu-1].n = %d\n", mems[ncpu-1].n); */

  printf("fill_mem test passed!======\n");
}