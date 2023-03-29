#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>

const size_t bar_size = 256ul*1024*1024;
const off_t bar_addr = 0x6160000000; // from lspci
const off_t dma_off_in_bar = 0xff00000; // from dmesg device_addr

unsigned char *bar;

void test_fault(off_t fault_off) {
    unsigned char test = rand()%256;
    bar[fault_off] = test;
    volatile unsigned char read = bar[fault_off];
    if (test != read) printf("fault test ok! check axi ila\n");
    else printf("fault test error!\n");
}

void test_dma() {
    unsigned char test = rand()%256;
    bar[dma_off_in_bar] = test;
    volatile unsigned char read = bar[dma_off_in_bar];
    if (test == read) printf("dma test ok!\n");
    else printf("Read after write error! Expect %x found %x\n", test, read);
}

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    bar = mmap(0, bar_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, bar_addr);

    if (bar == NULL) {
        printf("Unable to mmap\n");
        return 1;
    }

    test_fault(0);
    test_dma();
    return 0;
}