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

#define NBUCKET 13
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf bucket[NBUCKET];
  struct spinlock bucketlock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  char name[10];
  // Create linked list of buffers
  for(int i = 0 ; i < NBUCKET ; i++){
    snprintf(name, 10, "bucket%d", i);
    initlock(&bcache.bucketlock[i], name);
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
  }
  
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bucket[0].next;
    b->prev = &bcache.bucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].next->prev = b;
    bcache.bucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint64 hashcode = blockno % NBUCKET;
  acquire(&bcache.bucketlock[hashcode]);

  // Is the block already cached?
  for(b = bcache.bucket[hashcode].next; b != &bcache.bucket[hashcode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketlock[hashcode]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Attention! Deadlock!
  // assume that a thread holds bucketlock[0] and acquires bcache.lock while
  // another thread holds bcache.lock and acquires bucketlock[0], then deadlock occurs
  // so we release all locks first and then acquire locks we need
  // acquire(&bcache.lock);

  release(&bcache.bucketlock[hashcode]);

  // 有序申请资源
  acquire(&bcache.lock);
  acquire(&bcache.bucketlock[hashcode]);
  // 必须重新检查，比如 thread1 和 thread2 都访问同一个blockno，并且 no cached，那接下来都会申请buf，这样
  // 存在两个buf缓存同一块，错误错误错误！因此需要重新检查，一方缓存了，另一方不应该继续执行申请buf的代码
  for(b = bcache.bucket[hashcode].next; b != &bcache.bucket[hashcode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketlock[hashcode]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(uint64 subcode = 0 ; subcode < NBUCKET ; subcode++){
    if(subcode != hashcode){
      acquire(&bcache.bucketlock[subcode]);
    }
    for(b = bcache.bucket[subcode].next ; b != &bcache.bucket[subcode] ; b = b->next){
      if(0 == b->refcnt){
        b->prev->next = b->next;
        b->next->prev = b->prev;
        b->next = bcache.bucket[hashcode].next;
        b->prev = &bcache.bucket[hashcode];
        bcache.bucket[hashcode].next->prev = b;
        bcache.bucket[hashcode].next = b;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        
        release(&bcache.lock);
        release(&bcache.bucketlock[hashcode]);
        if(hashcode != subcode){
          release(&bcache.bucketlock[subcode]);
        }
        acquiresleep(&b->lock);
        return b;
      }
    }
    if(subcode != hashcode){
      release(&bcache.bucketlock[subcode]);
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

  uint64 hashcode = b->blockno % NBUCKET;
  releasesleep(&b->lock);

  acquire(&bcache.bucketlock[hashcode]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[hashcode].next;
    b->prev = &bcache.bucket[hashcode];
    bcache.bucket[hashcode].next->prev = b;
    bcache.bucket[hashcode].next = b;
  }
  
  release(&bcache.bucketlock[hashcode]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.bucketlock[b->blockno % NBUCKET]);
  b->refcnt++;
  release(&bcache.bucketlock[b->blockno % NBUCKET]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.bucketlock[b->blockno % NBUCKET]);
  b->refcnt--;
  release(&bcache.bucketlock[b->blockno % NBUCKET]);
}


