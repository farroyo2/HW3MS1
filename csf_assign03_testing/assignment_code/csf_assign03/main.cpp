#include <iostream>
#include <string>
#include <utility>
#include <stdint.h>
#include "cache.h"

int main( int argc, char **argv ) {
  //std::cout << "hello, this is a Cache Simulator by FA and SL" << std::endl;

  //argv must be checked
  /* 
  *   1: number of sets in the cache (+ power of 2)
  *   2: number of blocks in each set (+ power of 2)
  *   3: number of bytes in each block (+ power of 2, at least 4)
  *   4: write-allocate or no-write-allocate
  *   5: write-through or write-back
  *   6: lru or fifo
  */

  if (argc != 7) {
    //some kind of error checking
  }

  unsigned index = std::stoul(argv[1]);
  unsigned assoc = std::stoul(argv[2]);
  unsigned offset = std::stoul(argv[3]);

  std::string write_alloc = argv[4];
  std::string write_through = argv[5];
  std::string evic = argv[6];

  cacheInfo info {
    index, assoc, offset, write_alloc, write_through, evic
  };

  Cache c{ 
    std::vector<Set>(index, Set{std::vector<Slot>(assoc)}) 
  };

  char op;
  std::string addr_str;
  unsigned dummy;

  //Start a while loop to read each line in a trace file
  while ( std::cin >> op >> addr_str >> dummy) {

    //Separate function for reading each line

    uint32_t addr = std::stoul(addr_str, nullptr, 16); //base arg can be left as zero (0x)

    //Depending on the given arguments, call approrpiate functions

    //std::cout << op << " " << addr << '\n'; //for debugging

    switch (op) {
      case 's':
        //call cache store
        store(c, info, addr);
        break;
      case 'l':
        //call cache load
        load(c, info, addr);
        break;
      default:
        std::cerr << "invalid operation argument" << std::endl;
        break;
    }
  } 
  //End when @ EOF

  //Print cycles/hits/other stats before existing the simulation
  printCacheStats();

  return 0;
}