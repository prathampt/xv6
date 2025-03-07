#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file

void print_gdt(void) {
    struct segdesc *gdt;
    int num_entries, i;
    uint base, limit;

    struct {
            ushort limit;
            uint base;
    } __attribute__((packed)) gdtr;

    // Get GDT register
    asm volatile("sgdt %0" : "=m" (gdtr));

    // Validate base address and number of entries
    if (gdtr.base == 0 || gdtr.limit > 8191 * sizeof(struct segdesc)) {
        cprintf("Invalid GDTR base or limit!\n");
        return;
    }

    // Get pointer to GDT
    gdt = (struct segdesc*)gdtr.base;
    num_entries = (gdtr.limit + 1) / sizeof(struct segdesc);

    cprintf("\n=== GLOBAL DESCRIPTOR TABLE ===\n");
    cprintf("GDT Base Address: 0x%x\n", gdtr.base);
    cprintf("GDT Limit: %d bytes (%d entries)\n", gdtr.limit, num_entries);
    cprintf("\n");

    // Print header
    cprintf("Index | BA | Limit | P | DPL | S | Type | G | DB | Description\n");
    cprintf("------+---------------+------------+---+-----+---+------+---+----+------------\n");

    for (i = 0; i < num_entries && i<10; i++) {
        base = (uint)gdt[i].base_15_0 |
               ((uint)gdt[i].base_23_16 << 16) |
               ((uint)gdt[i].base_31_24 << 24);

        limit = (ushort)gdt[i].lim_15_0 | ((ushort)gdt[i].lim_19_16 << 16);

        // Apply granularity bit (G bit)
        if (gdt[i].g)
            limit = (limit << 12) | 0xFFF;

        cprintf("%d | 0x%x | 0x%x | %d | %d | %d | 0x%x | %d | %d | ",
               i, base, limit, gdt[i].p, gdt[i].dpl, gdt[i].s, gdt[i].type,
               gdt[i].g, gdt[i].db);

        // Print segment type description
        if (gdt[i].s == 0) {
            switch(gdt[i].type) {
                case 0x1: cprintf("16-bit TSS (Available)"); break;
                case 0x2: cprintf("LDT"); break;
                case 0x3: cprintf("16-bit TSS (Busy)"); break;
                case 0x4: cprintf("16-bit Call Gate"); break;
                case 0x5: cprintf("Task Gate"); break;
                case 0x6: cprintf("16-bit Interrupt Gate"); break;
                case 0x7: cprintf("16-bit Trap Gate"); break;
                case 0x9: cprintf("32-bit TSS (Available)"); break;
                case 0xB: cprintf("32-bit TSS (Busy)"); break;
                case 0xC: cprintf("32-bit Call Gate"); break;
                case 0xE: cprintf("32-bit Interrupt Gate"); break;
                case 0xF: cprintf("32-bit Trap Gate"); break;
                default:  cprintf("Unknown System Segment"); break;
            }
        } else {  // Code or Data segment
            if (gdt[i].type & 0x8)
                cprintf("Code Segment");
            else
                cprintf("Data Segment");
        }
        cprintf("\n");
    }
    cprintf("=================================\n\n");
}   
// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  kinit1(end, P2V(8*1024*1024)); // phys page allocator
  kvmalloc();      // kernel page table
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  print_gdt();
  uartinit();      // serial port
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk 
  startothers();   // start other processors
  kinit2(P2V(8*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  userinit();      // first user process
  mpmain();        // finish this processor's setup
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  [1] = (1 << 22) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
  [(KERNBASE>>PDXSHIFT) + 1] = (1 << 22) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

