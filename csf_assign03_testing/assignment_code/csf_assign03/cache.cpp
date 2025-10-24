#include "cache.h"
#include <iostream>
#include "math.h"

cacheStats stats = {0}; //initialize stats struct with all zeros 

std::pair<bool, std::pair<unsigned, unsigned>> 
checkHit(Cache& c, const cacheInfo& info, uint32_t addr) {
    std::pair<bool, std::pair<unsigned, unsigned>> cache_hit;

    uint32_t ind_bits = log2(info.index);
    uint32_t off_bits = log2(info.offset);

    uint32_t index_mask = (1 << ind_bits) -1;

    uint32_t index = (addr >> off_bits) & index_mask;
    uint32_t tag = (addr >> (off_bits + ind_bits));

    //from the cache, go to the computed index
    Set& set = c.sets[index];

    //check each slots (ways) and see if theres matching
    unsigned slot_to_replace = info.assoc; //full to mean no invalid lines
    cache_hit.first = false;

    for (unsigned i = 0; i < info.assoc; i++) {
        uint32_t tag_comp = (set.slots[i].tag >> (off_bits + ind_bits));
        if (tag_comp == tag && set.slots[i].valid) {
            cache_hit.first = true;
            cache_hit.second = std::make_pair(index, i); //issues with autocast?
            set.slots[i].access_ts = stats.cycles; //need to check for cycles
            return cache_hit; 
        } 

        if (!set.slots[i].valid && slot_to_replace == info.assoc) {
            slot_to_replace = i;
        }

    }
    //If none-matching, either store in non-valid or evict
    //Store the slot to repalce in the second, no slots if max value
    cache_hit.second = std::make_pair(index, slot_to_replace);

    //Update all of the stats accordingly throughout

    return cache_hit;
}

unsigned lru_evict(Set& set) {
    unsigned min_index = 0;
    uint32_t min_ts = set.slots[0].access_ts;

    for (unsigned i = 0; i < set.slots.size(); i++) {
        if(set.slots[i].access_ts < min_ts) {
            min_ts = set.slots[i].access_ts;
            min_index = i;
        }
    }

    return min_index;
}

unsigned fifo_evict(Set& set) {
    unsigned min_index = 0;
    uint32_t min_ts = set.slots[0].load_ts;

    for (unsigned i = 0; i < set.slots.size(); i++) {
        if(set.slots[i].load_ts < min_ts) {
            min_ts = set.slots[i].load_ts;
            min_index = i;
        }
    }

    return min_index;
}


void load(Cache& c, const cacheInfo& info, uint32_t addr) {

    stats.loads++;
    //Determine the Tag of the addr
    //Sequential search to determine Tag hit
    std::pair<bool, std::pair<unsigned, unsigned>> cache_hit;

    stats.cycles++; //single cache load
    cache_hit = checkHit(c, info, addr);
    std::pair<unsigned, unsigned>& indexes = cache_hit.second;

    //If cache hit, update cacheStats
    if (cache_hit.first == true) {
        // update l_hits
        stats.l_hits++;
        //additional updates?        
    } else {
        //If cache miss, update cacheStats, bring/evict the new addr based on FIFO/LRU
        //update l_misses
        stats.l_misses++;
        //cache_update mechanism based on LRU or FIFO
        //  1) go to the index of the addr
        //  2) see if theres any non-valid slots - fill them
        //  3) if everything full, determine based on LRU or FIFO
        // Update the load_ts and acces_ts, and current cycles
        Set& set = c.sets[indexes.first]; //get the respective set
        stats.cycles += ((info.offset/4) * 100); //to access main memory
        if (indexes.second != info.assoc) {
            set.slots[indexes.second].tag = addr;
            set.slots[indexes.second].valid = true;
            set.slots[indexes.second].load_ts = stats.cycles;
        } else {
            //eviction differes based on LRU or FIFO - seperate here?
            std::string lru_fifo = info.evic;
            unsigned to_replace = info.assoc;
            if (lru_fifo == "lru") {
                //LRU eviction logic, look for the first access_ts
                to_replace = lru_evict(set);
            } else if (lru_fifo == "fifo") {
                //FIFO eviction logic, look for the first load_ts
                to_replace = fifo_evict(set);
            } else {
                std::cerr << "wrong parameter for fifo / lru" << std::endl;
            }
            set.slots[to_replace].tag = addr;
            set.slots[to_replace].valid = true;
            set.slots[to_replace].load_ts = stats.cycles;
        }
        
    }
    
}


//having a general function for writing to the cache will be useful

void store(Cache& c, const cacheInfo& info, uint32_t addr) {
    
    stats.stores++;
    //Determine the Tag of the Addr
    //Sequential search to determine Tag hit
    std::pair<bool, std::pair<unsigned, unsigned>> cache_hit;

    cache_hit = checkHit(c, info, addr);
    //If Cache hit, update cacheStats , and based on write-through or back
    if (cache_hit.first == true) {
        // update s_hits;
        // update the cache for write-through (both cache and memory)
        // or non-write through (only cache, marks block dirty)
    } else {
        //If Cache miss, update cacheStats, and determine write-allocate or no
        // update s_misses;
        // update the caceh for write-allocate (write relvant memroy to cache)
        // no-write allocate will not modify the cache here
    }

    

}

void printCacheStats() {
    std::cout << "Total loads: " << stats.loads << std::endl;
    std::cout << "Total stores: " << stats.stores << std::endl;
    std::cout << "Load hits: " << stats.l_hits << std::endl;
    std::cout << "Load misses: " << stats.l_misses << std::endl;
    std::cout << "Store hits: " << stats.s_hits << std::endl;
    std::cout << "Store misses: " << stats.s_misses << std::endl;
    std::cout << "Total cycles: " << stats.cycles << std::endl;
}