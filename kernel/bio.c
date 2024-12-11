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

#define NHTBL 17

struct {
  struct spinlock hlock[NHTBL];
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf *htabl[NHTBL];
} bcache;

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < NHTBL; i ++){
    initlock(&bcache.hlock[i], "bcache");
  }
  initlock(&bcache.lock, "bcache all");

  for(int i = 0; i < NHTBL; i ++){
    bcache.htabl[i] = 0;
  }
  for(b = bcache.buf; b < bcache.buf + NBUF; b ++){
    initsleeplock(&b->lock, "buffer");
    b->hnxt = 0;
  }
  bcache.buf[NBUF - 1].znxt = 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint h = blockno % NHTBL;
  acquire(&bcache.hlock[h]);

  // Is the block already cached?
  for(b = bcache.htabl[h]; b != 0; b = b->hnxt){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hlock[h]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  int ok = 0;
  acquire(&bcache.lock);
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    if (b->refcnt == 0) {
      ok = 1;
      b->refcnt = 1;
      release(&bcache.lock);
      break;
    }
  }
  if(ok == 0){
    panic("bget: no buffers");
  }
  // insert to hash table
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->hnxt = bcache.htabl[h];
  bcache.htabl[h] = b;
  release(&bcache.hlock[h]);
  acquiresleep(&b->lock);
  return b;
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

  uint h = b->blockno % NHTBL;
  acquire(&bcache.hlock[h]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // delete from hash table
    if(bcache.htabl[h]->blockno == b->blockno){
      bcache.htabl[h] = b->hnxt;
    } else {
      for(struct buf *p = bcache.htabl[h]; p->hnxt; p = p->hnxt){
        if (p->hnxt->blockno == b->blockno) {
          p->hnxt = p->hnxt->hnxt;
          break;
        }
      }
    }
    release(&bcache.hlock[h]);
  }else{
    release(&bcache.hlock[h]);
  }
  
}

void
bpin(struct buf *b) {
  uint h = b->blockno % NHTBL;
  acquire(&bcache.hlock[h]);
  b->refcnt++;
  release(&bcache.hlock[h]);
}

void
bunpin(struct buf *b) {
  uint h = b->blockno % NHTBL;
  acquire(&bcache.hlock[h]);
  b->refcnt--;
  release(&bcache.hlock[h]);
}


