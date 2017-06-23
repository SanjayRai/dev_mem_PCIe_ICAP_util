/* Sanjay Rai - Routine to access PCIe via  dev.mem mmap  */
#ifndef FPGA_MMIO_UTILS_H_
#define  FPGA_MMIO_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>

using namespace std;

#define FAUT_CONDITION do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

#define PAGE_SIZE (256*1024UL)


class fpga_mmio {
    private :
        void *virt_page_base;
        int fp_dev_mem;

    public :

        fpga_mmio (void) {}
        
        template <class addr_type, class data_type>
        int fpga_poke (addr_type register_offset, data_type val) {
            addr_type *tmp_addr;
            tmp_addr = (addr_type *)((uint8_t *)virt_page_base + register_offset);

            *tmp_addr = val;
            //cout << "Addr = " << hex << tmp_addr << " : Val = " << val <<endl;
            return 0;
        }

        template <class addr_type, class data_type>
        data_type fpga_peek (addr_type register_offset) {
            addr_type *tmp_addr;
            tmp_addr = (addr_type *)((uint8_t *)virt_page_base + register_offset);
            return(*tmp_addr);
        }


        int fpga_mmio_init(uint32_t base_address) {


            if((fp_dev_mem = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FAUT_CONDITION;
            fflush(stdout);

            //cout << " Base Address = " << hex << "0x" << base_address << endl;
            virt_page_base = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fp_dev_mem, base_address & ~(PAGE_SIZE-1));
            //cout << " Virtual Base Address = " << hex << virt_page_base << endl;
            if(virt_page_base == (void *) -1) FAUT_CONDITION;

            return 0;
        }

        int fpga_mmio_clean() {
            if(munmap(virt_page_base, PAGE_SIZE) == -1) FAUT_CONDITION;
            close(fp_dev_mem);
            return 0;
        }
};
#endif
