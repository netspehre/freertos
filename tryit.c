#include <stdio.h>
#include <stdlib.h>
#include <fio.h>

#define ENABLE_DEBUG
#include "FreeRTOS.h"
#define DBGPRINTF1(x,a)       printf("\r%s %d: ", __FILE__, __LINE__); printf(x, a)
#define DBGPRINTF2(x,a,b)     printf("\r%s %d: ", __FILE__, __LINE__); printf(x, a, b)
/* heap_ww.c */
void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void *pv);
size_t xPortGetFreeHeapSize(void);

struct slot {
    void *pointer;
    unsigned int size;
    unsigned int lfsr;
};

#define CIRCBUFSIZE 500
unsigned int write_pointer, read_pointer;
static struct slot slots[CIRCBUFSIZE];


static unsigned int circbuf_size(void)
{
    return (write_pointer + CIRCBUFSIZE - read_pointer) % CIRCBUFSIZE;
}

static void write_cb(struct slot foo)
{
    if (circbuf_size() == CIRCBUFSIZE - 1) {
        printf( "circular buffer overflow\n");
        //exit(1);
	return ;
    }
    slots[write_pointer++] = foo;
    write_pointer %= CIRCBUFSIZE;
}

static struct slot read_cb(void)
{
    struct slot foo;
    if (write_pointer == read_pointer) {
        // circular buffer is empty
        return (struct slot){ .pointer=NULL, .size=0, .lfsr=0 };
    }
    foo = slots[read_pointer++];
    read_pointer %= CIRCBUFSIZE;
    return foo;
} 
// Get a pseudorandom number generator from Wikipedia
static unsigned int lfsr = 0xace1;
unsigned int bit;
int prng(void)
{
    /* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    //bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    __asm__("ldr r0,=bit");  //&bit
    __asm__("ldr r1,=lfsr"); //&lfsr
    __asm__("ldr r2,[r0]");   //bit
    __asm__("ldr r3,[r1]");   //lfsr
    __asm__("lsr r4,r3,#2");  //(lfsr >> 2)
    __asm__("eor r5,r4,r3");  //(lfsr >> 0) ^ (lfsr >> 2)
    __asm__("lsr r4,r3,#3");  //(lfsr >> 3)
    __asm__("eor r5,r5,r4");  //^(lfsr >> 3)
    __asm__("lsr r4,r3,#5");  //(lfsr >> 5)
    __asm__("eor r5,r5,r4");  //^(lfsr >> 5)
    __asm__("and r5,r5,#1");  //& 1

    //lfsr =  (lfsr >> 1) | (bit << 15);
    __asm__("lsr r4,r3,#1");  //(lfsr >> 1)
    __asm__("lsl r6,r5,#15"); //(bit << 15)
    __asm__("orr r0,r6,r4");  //(lfsr >> 1) | (bit << 15);
    __asm__("str r0,[r1]");

    return lfsr & 0xffff;
}

int mmtest(void)
{
    int i, size;
    char *p;
    char ch;
    while (1) {
        size = prng() & 0x7FF;
        DBGPRINTF1("try to allocate %d bytes\n", size);
        p = (char *) pvPortMalloc(size);
        DBGPRINTF1("malloc returned %p\n", p);
        if (p == NULL) {
            // can't do new allocations until we free some older ones
            while (circbuf_size() > 0) {
                // confirm that data didn't get trampled before freeing
                struct slot foo = read_cb();
                p = foo.pointer;
                lfsr = foo.lfsr;  // reset the PRNG to its earlier state
                size = foo.size;
                printf("free a block, size %d\n", size);
                for (i = 0; i < size; i++) {
                    unsigned char u = p[i];
                    unsigned char v = (unsigned char) prng();
                    if (u != v) {
                        DBGPRINTF2("OUCH: u=%02X, v=%02X\n", u, v);
                        return 1;
                    }
                }
                vPortFree(p);
                if ((prng() & 1) == 0) break;
            }
        } else {
            printf("allocate a block, size %d\n", size);
            write_cb((struct slot){.pointer=p, .size=size, .lfsr=lfsr});
            for (i = 0; i < size; i++) {
                p[i] = (unsigned char) prng();
            }
        }
        fio_read(0, &ch, 1);
    }

    return 0;
}
