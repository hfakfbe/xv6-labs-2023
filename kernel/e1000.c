#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  acquire(&e1000_lock);//加🔓
  int ring_idx = regs[E1000_TDT];//获取下一个环索引
  if(tx_ring[ring_idx].status & E1000_TXD_STAT_DD){//检查索引到的位置E1000有没有完成先前的传输请求
    if(tx_mbufs[ring_idx])//检查用于传输上一个包的缓冲区是否被释放
      mbuffree(tx_mbufs[ring_idx]);//释放缓冲区
    tx_ring[ring_idx].addr = (uint64) m->head;//填写描述符addr字段使其指向当前缓冲区
    tx_ring[ring_idx].length = m->len;//填写描述符length字段
    tx_ring[ring_idx].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;//查看e1000开发者手册3.3.3.1的Notes部分
    tx_mbufs[ring_idx] = m;
    __sync_synchronize();//内存屏障，防止指令重排
    regs[E1000_TDT] = (regs[E1000_TDT]+1) % TX_RING_SIZE;
    release(&e1000_lock);//解🔓
    return 0;
  } else {
    release(&e1000_lock);//解🔓
    return -1;//error
  }
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  while(1){//一次处理多个包,防止丢失
    int ring_idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;//获取下一个环索引
    if(rx_ring[ring_idx].status & E1000_TXD_STAT_DD){//检查索引到的位置是否有数据包待接受
      rx_mbufs[ring_idx]->len = rx_ring[ring_idx].length;//将数据长度存入缓冲区结构体
      net_rx(rx_mbufs[ring_idx]);//将数据包交给net_rx解析
      rx_mbufs[ring_idx] = mbufalloc(0);//申请新缓冲区
      if (!rx_mbufs[ring_idx])//判断是否申请成功
        panic("e1000");
      rx_ring[ring_idx].addr = (uint64) rx_mbufs[ring_idx]->head;//缓冲区首地址存入描述符结构体
      rx_ring[ring_idx].status = 0;//将描述符的状态位清除为零
      __sync_synchronize();//内存屏障，防止指令重排
      regs[E1000_RDT] = ring_idx;//修改尾指针寄存器
    }
    else return;
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
