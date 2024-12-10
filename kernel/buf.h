struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *znxt; // zero-list next
  struct buf *hnxt; // hash next
  uchar data[BSIZE];
};

