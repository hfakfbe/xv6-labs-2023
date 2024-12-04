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
uint16 cow_map[PHYSTOP / PGSIZE];
struct spinlock cow_map_lock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&cow_map_lock, "cow_map");
  for(int i = 0; i < PHYSTOP / PGSIZE; i ++){
    cow_map[i] = 1;
  }
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
  
  acquire(&cow_map_lock);
  if(-- cow_map[(uint64)pa / PGSIZE] > 0){
    release(&cow_map_lock);
    return;
  }
  release(&cow_map_lock);

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
  if(r){
    kmem.freelist = r->next;
    cow_map[(uint64)r / PGSIZE] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void 
kinc_cow_map(void *pa)
{
  acquire(&cow_map_lock);
  cow_map[(uint64)pa / PGSIZE] ++;
  release(&cow_map_lock);
}

void 
kdec_cow_map(void *pa)
{
  kfree(pa);
}

// pte must have PTE_V and PTE_C flag
void 
kcopy_cow(pte_t *pte)
{
  uint64 pa0 = PTE2PA(*pte), pa;
  if((pa = (uint64) kalloc()) == 0){
    kdec_cow_map((void *) pa0);
    printf("kcopy_cow(): kalloc failed\n");
    setkilled(myproc());
    exit(-1);
  }
  memmove((char*)pa, (char*)pa0, PGSIZE);
  *pte &= ~PTE_C;
  *pte |= PTE_W;
  *pte = PA2PTE(pa) | PTE_FLAGS(*pte);
  kdec_cow_map((void *) pa0);
}