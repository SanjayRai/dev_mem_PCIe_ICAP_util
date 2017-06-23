/* Sanjay Rai - Test routing to access PCIe via  dev.mem mmap  */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <fstream>
#include <chrono>
#include "pcie_memio.h" 

using namespace std;

#define BRAM_BASE 0x20000
#define ICAP_BASE 0x10000

#define ICAP_CNTRL_REG           ICAP_BASE + 0x10C
#define ICAP_STATUS_REG          ICAP_BASE + 0x110
#define ICAP_WR_FIFO_REG         ICAP_BASE + 0x100
#define ICAP_WR_FIFO_VACANCY_REG ICAP_BASE + 0x114

int ICAP_prog_binfile (fpga_mmio *fpga_ICAP, uint32_t *data_buffer, unsigned int binfile_length );
int ICAP_acess (fpga_mmio *fpga_ICAP);
int BRAM_acess (fpga_mmio *fpga_BRAM);

int main(int argc, char **argv) {

    uint32_t base_address;
    clock_t elspsed_time;

    fstream fpga_bin_file;

    if (argc != 3) {
        printf("usage: pcie_mmio base_addr fpga_bin_file\n");
        return -1;
    }

    elspsed_time = clock();
    auto start_t = chrono::high_resolution_clock::now();
    // ------------ Init -----------------------
    fpga_mmio *my_fpga_ptr = new fpga_mmio;
    base_address = strtoll(argv[1], 0, 0);
    my_fpga_ptr->fpga_mmio_init(base_address);

    fpga_bin_file.open(argv[2], ios::in | ios::binary);

    fpga_bin_file.seekg(0, fpga_bin_file.end);
    int file_size = fpga_bin_file.tellg();
    fpga_bin_file.seekg(0, fpga_bin_file.beg);
    cout << "File Size " << file_size << endl;

    char *bitStream_buffer = new char [file_size];
    uint32_t *bitstream_ptr;
    bitstream_ptr = (uint32_t *)bitStream_buffer;
    fpga_bin_file.read(bitStream_buffer, file_size);
    fpga_bin_file.close();

    ICAP_prog_binfile (my_fpga_ptr, bitstream_ptr, file_size );
    BRAM_acess(my_fpga_ptr);

    // ------------ Clean -----------------------
    my_fpga_ptr->fpga_mmio_clean();
    auto stop_t = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed_hi_res = stop_t - start_t ;
    cout << "High _Res count  =  " << elapsed_hi_res.count() << "s\n";
    elspsed_time = (clock() - elspsed_time);
    printf ("Elapsede time = %f secs\n", (double)elspsed_time/CLOCKS_PER_SEC);

    return 0;
}

int ICAP_prog_binfile (fpga_mmio *fpga_ICAP, uint32_t *data_buffer, unsigned int binfile_length ) {

    uint32_t ret_data;
    uint32_t itn_count;
    uint32_t byte_swapped;
    // Reset the ICAP
    fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_CNTRL_REG, 0x8); 
    // Check if the ICAP is ready
    ret_data = fpga_ICAP->fpga_peek<uint32_t, uint32_t>(ICAP_STATUS_REG); 
    printf (" Status Reg = %x\n", ret_data);
    itn_count = 0;
    while (ret_data != 0x5) {
        ret_data = fpga_ICAP->fpga_peek<uint32_t, uint32_t>(ICAP_STATUS_REG); 
        itn_count++; 
        if (itn_count > 1000) FAUT_CONDITION;
    }

    // ICAP Data File processing
    for (unsigned int i = 0 ; i < (binfile_length/4); i++) {
        byte_swapped = ((*data_buffer>>24)&0x000000ff) | \
                       ((*data_buffer>>8) &0x0000ff00) | \
                       ((*data_buffer<<8) &0x00ff0000) | \
                       ((*data_buffer<<24)&0xff000000);
        data_buffer++;
    
        //cout << "Writing =  " << hex << byte_swapped << endl;
        fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, byte_swapped); 
        if (((i+1) % 60) == 0) {
            // Write to the COntrol register to drain the Data FIFO every 60 or writes 
            ret_data = fpga_ICAP->fpga_peek<uint32_t, uint32_t>(ICAP_WR_FIFO_VACANCY_REG); 
            fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_CNTRL_REG, 0x1); 
            itn_count = 0;
            // Wait till Fifo is drained
            while (ret_data != 0x3F) {
                ret_data = fpga_ICAP->fpga_peek<uint32_t, uint32_t>(ICAP_WR_FIFO_VACANCY_REG); 
                itn_count++; 
                if (itn_count > 1000) FAUT_CONDITION;
            }
        }
    }
    // Final ICAP Fifo Flush
    fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_CNTRL_REG, 0x1); 

   return 0;
}


int BRAM_acess (fpga_mmio *fpga_BRAM) {

    uint32_t tmp_addr;

    tmp_addr = BRAM_BASE;
    for (unsigned int i = 0 ; i < 20; i++) {
        fpga_BRAM->fpga_poke<uint32_t, uint8_t>(tmp_addr, i); 
        tmp_addr = tmp_addr +1;
    }
   return 0;
}

int ICAP_IPROG_reset (fpga_mmio *fpga_ICAP) {
   printf ("Initiatig IPROG...............\n");
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0xFFFFFFFFUL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0xFFFFFFFFUL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0xAA995566UL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0x20000000UL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0x30020001UL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0x00000000UL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0x30080001UL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0x0000000FUL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0x20000000UL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_WR_FIFO_REG, 0x20000000UL); 
   fpga_ICAP->fpga_poke<uint32_t, uint32_t>(ICAP_CNTRL_REG, 0x1); 

   return 0;
}
