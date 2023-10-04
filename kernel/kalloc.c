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


int ref_arr[(PHYSTOP - KERNBASE) / PGSIZE];


// get index in ref_arr by physical address
// return -1 on failure
int get_index_by_pa(void *pa)
{
  uint64 p = (uint64) pa;
  if (p > (PHYSTOP - PGSIZE) || p < KERNBASE) {
    panic("pa is not in the range");
    return -1;
  }

  return (p - KERNBASE) / PGSIZE;    
}


// used by uvmcopy_new()
void increment_pa_ref(void *pa)
{
  acquire(&kmem.lock);
  int i = get_index_by_pa(pa);
  if (i == -1) {
    panic("increment_pa_ref");
  }

  ref_arr[i]++;
  release(&kmem.lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_init(p);
}


void kfree_init(void *pa)
{
  int i;
  if ((i = get_index_by_pa(pa)) == -1) {
    panic("kfree_init");
  }

  ref_arr[i] = 0;

  kfree_old(pa);
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


void kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree_new");

  /*
  为什么下面这行代码会使程序出错？
    假如现在有一个 PTE_COW page 的引用次数是2，当一个进程要去解绑这个COW页，创建新的页时，
    在copy_on_write()函数中调用kfree()不仅会使ref--，同时下面这行代码覆盖了原有的COW-page内存，
    但是COW-page还被其他进程使用啊，所以就出错了

  这个傻逼bug折磨了我得有10几个小时

  这种错误看似不难，可一旦发生，这种错误不好排查（因为没有异常检测机制）

  这个bug在 usertrap() 里面报了一个 Instruction page fault($scause = 12)错误，拿gdb调都调不了，断点都不知道往哪打

  思考：下次遇到这种问题怎么调？
  */
  // Fill with junk to catch dangling refs.
  // memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  int i = get_index_by_pa(r);
  if (i == -1) {
    panic("kfree_new");
  }

  if (ref_arr[i] < 1) {
    panic("kfree_new");
  }

  ref_arr[i]--;
  if (ref_arr[i] == 0) {
    r->next = kmem.freelist;
    kmem.freelist = r;
  }

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
  if(r) {

    if(((uint64)r % PGSIZE) != 0 || (char*)r < end || (uint64)r >= PHYSTOP)
      panic("kalloc");

    kmem.freelist = r->next;

    int i = get_index_by_pa(r);
    if (i == -1) {
      panic("kalloc_new");
    }
    if (ref_arr[i] != 0) {
      panic("kalloc_new");
    }

    ref_arr[i]++;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
