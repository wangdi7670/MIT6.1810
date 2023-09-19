// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


// #define BCACHE
#ifdef BCACHE


struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }

  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}

#else

#define TABLE_SIZE 15
#define BUCKET_SIZE 2


struct bucket {
  struct spinlock lock;
  struct buf buf[BUCKET_SIZE];
  struct buf head;
};

struct {
  struct spinlock lock;
  struct bucket buf_table[TABLE_SIZE];
} bcache;

void test_init();
int bucket_length(struct bucket *bkt);


void bucket_init(struct bucket *bkt)  // 2. 
{
  struct buf *b;

  initlock(&bkt->lock, "bcache_bkt");

  // Create linked list of buffers
  bkt->head.prev = &bkt->head;
  bkt->head.next = &bkt->head;

  for(b = bkt->buf; b < bkt->buf + BUCKET_SIZE; b++){
    b->next = bkt->head.next;
    b->prev = &bkt->head;
    initsleeplock(&b->lock, "buffer");
    bkt->head.next->prev = b;
    bkt->head.next = b;
  }
}


void binit()
{
  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < TABLE_SIZE; i++) {
    bucket_init(&bcache.buf_table[i]);
  }

  // test_init();
}


int bucket_length(struct bucket *bkt)
{
  int count = 0;
  struct buf *b = bkt->head.next;
  while (b != &bkt->head)
  {
    if (b->refcnt != 0 || b->blockno != 0 || b->dev != 0 || b->valid != 0) {
      panic("wrong refcnt");
    }

    count++;
    b = b->next;
  }

  return count;
}


void test_init()
{
  int count = 0;
  for (int i = 0; i < TABLE_SIZE; i++) {
    struct bucket *bkt = &bcache.buf_table[i];
    count += bucket_length(bkt);
  }

  if (count != TABLE_SIZE * BUCKET_SIZE) {
    panic("wrong init");
  }

  printf("test init: OK\n");
}


// move buf to the bkt's head
void move_buf(struct buf* b, struct bucket *bkt)
{
  b->prev->next = b->next;
  b->next->prev = b->prev;

  b->next = bkt->head.next;
  b->prev = &bkt->head;
  bkt->head.next->prev = b;
  bkt->head.next = b;
}


static struct buf* bget(uint dev, uint blockno)
{
  if (dev != 1) {
    panic("wrong dev");
  }

  // acquire(&bcache.lock);
  
  int id = blockno % TABLE_SIZE;
  struct bucket *bkt = &bcache.buf_table[id];
  struct buf *b;

  acquire(&bkt->lock);
  // Is the block cached?
  for (b = bkt->head.next; b != &bkt->head; b = b->next) {
    if (b->blockno == blockno && b->dev == dev) {
      b->refcnt++;
      // release(&bcache.lock);
      release(&bkt->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached
  for (int i = 0; i < TABLE_SIZE; i++) {
    if (i == id) {
      continue;
    }
    struct bucket *temp = &bcache.buf_table[i];
    acquire(&temp->lock);

    for (b = temp->head.next; b != &temp->head; b = b->next) {
      if (b->refcnt == 0) {
        b->refcnt++;
        b->blockno = blockno;
        b->dev = dev;
        b->valid = 0;

        // move the buf from one bucket to another bucket
        move_buf(b, bkt);
      
        // release(&bcache.lock);
        release(&bkt->lock);
        release(&temp->lock);
        acquiresleep(&b->lock);
        return b;
      }
    }

    release(&temp->lock);
  }

  panic("no buffer");
}


void brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock)) {
    panic("brelse");
  }

  releasesleep(&b->lock);

  int id = b->blockno % TABLE_SIZE;
  struct bucket *bkt = &bcache.buf_table[id];

  acquire(&bkt->lock);
  b->refcnt--;
  release(&bkt->lock);
}


void bpin(struct buf *b) {
  int id = b->blockno % TABLE_SIZE;
  struct bucket *bkt = &bcache.buf_table[id];

  acquire(&bkt->lock);
  b->refcnt++;
  release(&bkt->lock);
}


void bunpin(struct buf *b) {
  int id = b->blockno % TABLE_SIZE;
  struct bucket *bkt = &bcache.buf_table[id];

  acquire(&bkt->lock);
  b->refcnt--;
  release(&bkt->lock);
}

// Return a locked buf with the contents of the indicated block.
struct buf* bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

#endif