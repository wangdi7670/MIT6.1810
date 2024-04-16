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

#define LENGTH 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf *hashtable[LENGTH];
  struct spinlock ht_lock[LENGTH];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }

  //
  // build with hashtable
  // 
  for (int i = 0; i < LENGTH; i++) {
    bcache.hashtable[i] = 0;

    char name[8] = "bcache";
    name[6] = '0' + i;
    name[7] = '\0';
    initlock(&bcache.ht_lock[i], name);
  }

  for (int i = 0; i < NBUF; i++) {
    int j = i % LENGTH;
    bcache.buf[i].next = bcache.hashtable[j];
    bcache.hashtable[j] = &bcache.buf[i];
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int index = blockno % LENGTH;

  acquire(&bcache.ht_lock[index]);

  // Is the block already cached?
  for (b = bcache.hashtable[index]; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.ht_lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.hashtable[index]; b != 0; b = b->next) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.ht_lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.ht_lock[index]);

  // 
  // move
  // 
  for (int i = 0; i < LENGTH; i++) {
    if (i == index) {
      continue;
    }

    if (i < index) {
      acquire(&bcache.ht_lock[i]);
      acquire(&bcache.ht_lock[index]);
    } else {
      acquire(&bcache.ht_lock[index]);
      acquire(&bcache.ht_lock[i]);
    }

    struct buf *last = 0;
    for (b = bcache.hashtable[i]; b != 0; b = b->next) {
      if (b->refcnt == 0) {
        if (last != 0) {
          last->next = b->next;
        } else {
          bcache.hashtable[i] = b->next;
        }
        // move from i => index
        b->next = bcache.hashtable[index];
        bcache.hashtable[index] = b;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // release(&bcache.lock);
        release(&bcache.ht_lock[i]);
        release(&bcache.ht_lock[index]);
        acquiresleep(&b->lock);
        return b;
      }

      last = b;
    }

    release(&bcache.ht_lock[i]);
    release(&bcache.ht_lock[index]);
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

  int index = b->blockno % LENGTH;
  releasesleep(&b->lock);

  acquire(&bcache.ht_lock[index]);
  b->refcnt--;
  
  release(&bcache.ht_lock[index]);
}

void
bpin(struct buf *b) {
  int index = b->blockno % LENGTH;

  acquire(&bcache.ht_lock[index]);
  b->refcnt++;
  release(&bcache.ht_lock[index]);
}

void
bunpin(struct buf *b) {
  int index = b->blockno % LENGTH;

  acquire(&bcache.ht_lock[index]);
  b->refcnt--;
  release(&bcache.ht_lock[index]);
}


