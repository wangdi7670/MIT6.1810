#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(int recovering)
{
  for (int tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start + 1 + tail);
    struct buf *rbuf = bread(log.dev, log.lh.block[tail]);
    memmove(rbuf->data, lbuf->data, BSIZE);
    bwrite(rbuf);

    // because logwrite() add ref for buffer cache previously
    if (recovering == 0) {
      bunpin(rbuf);
    }

    brelse(lbuf);
    brelse(rbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *b = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (b->data);
  log.lh.n = lh->n;
  for (int i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }

  brelse(b);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *b = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (b->data);
  lh->n = log.lh.n;
  for (int i = 0; i < log.lh.n; i++) {
    lh->block[i] = log.lh.block[i];
  }

  bwrite(b);
  brelse(b);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);

  while (1) {
    if (log.committing == 1) {
      sleep(&log, &log.lock);

    } else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE) {
      sleep(&log, &log.lock);

    } else {
      log.outstanding++;
      break;
    }
  }

  release(&log.lock);
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  if (!(log.outstanding > 0)) {
    panic("outstanding should > 0");
  }
  if (log.committing != 0) {
    panic("committing should be 0");
  }

  log.outstanding--;
  wakeup(&log);

  if (log.outstanding == 0) {
    log.committing = 1;
    do_commit = 1;
  }

  release(&log.lock);

  if (do_commit == 1) {
    commit();

    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  for (int i = 0; i < log.lh.n; i++) {
    // because logwrite() add ref for buffer cache previously, so this read is directly read rather than read from disk
    struct buf *from = bread(log.dev, log.lh.block[i]);  
    struct buf *to = bread(log.dev, log.start + 1 + i);  // log block

    memmove(to->data, from->data, BSIZE);
    bwrite(to);

    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(0); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  acquire(&log.lock);
  if (log.lh.n == log.size - 1) {
    panic("too big a transaction");
  }
  if (!(log.outstanding >= 1)) {
    panic("it should be in a transaction");
  }
  if (log.committing == 1) {
    panic("it should not in commiting");
  }

  int find = 0;
  for (int i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno) {
      find = 1;
      break;
    }
  }

  if (!find) {
    bpin(b);
    log.lh.block[log.lh.n] = b->blockno; 
    log.lh.n++;
  }

  release(&log.lock);
}

