struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;  // refcnt > 0 indicates that there is at least one process using it(get the sleep-lock) or waiting for it(not get the sleep-lock)
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

