#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <utility>
#include <string>
#include <stdint.h>

struct cacheStats {
    uint32_t loads, stores, cycles;
    uint32_t l_hits, l_misses, s_hits, s_misses;
};

struct cacheInfo {
    unsigned index, assoc, offset;
    std::string write_alloc, write_through, evic;
};

//Data Structure for the Cache
//Can be improved by adding an "index" to map tags to valid blocks

struct Slot {
    uint32_t tag;
    bool valid, dirty;
    uint32_t load_ts, access_ts;
};

struct Set {
    std::vector<Slot> slots;
};

struct Cache {
    std::vector<Set> sets;
};

void load(Cache& c, const cacheInfo& info, uint32_t addr);

void store(Cache& c, const cacheInfo& info, uint32_t addr);

//Print the Stats of the Cache
void printCacheStats();

std::pair<bool, std::pair<unsigned, unsigned>> 
checkHit(Cache& c, const cacheInfo& info, uint32_t addr);

unsigned lru_evict(Set& set);

unsigned fifo_evict(Set& set);






#endif