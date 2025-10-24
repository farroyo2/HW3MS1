#include "cache.h"
#include <iostream>
#include "math.h"

cacheStats stats = {0}; //initialize stats struct with all zeros 

std::pair<bool, std::pair<unsigned, unsigned>> 
checkHit(Cache& c, const cacheInfo& info, uint32_t addr) {
    std::pair<bool, std::pair<unsigned, unsigned>> cache_hit;

    uint32_t ind_bits = log2(info.index);
    uint32_t off_bits = log2(info.offset);

    uint32_t index_mask = (1u << ind_bits) -1u;

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
            // set.slots[i].access_ts = stats.cycles; //need to check for cycles
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

unsigned lru_evict(Set& set, std::string write_through, unsigned offset) {
    unsigned min_index = 0;
    uint32_t min_ts = set.slots[0].access_ts;

    for (unsigned i = 0; i < set.slots.size(); i++) {
        if(set.slots[i].access_ts < min_ts) {
            min_ts = set.slots[i].access_ts;
            min_index = i;
        }
    }

    if(write_through == "write-back" && set.slots[min_index].dirty) {
        stats.cycles += ((offset/4) * 100); //store the modified
    }

    return min_index;
}

unsigned fifo_evict(Set& set, std::string write_through, unsigned offset) {
    unsigned min_index = 0;
    uint32_t min_ts = set.slots[0].load_ts;

    for (unsigned i = 0; i < set.slots.size(); i++) {
        if(set.slots[i].load_ts < min_ts) {
            min_ts = set.slots[i].load_ts;
            min_index = i;
        }
    }

     if(write_through == "write-back" && set.slots[min_index].dirty) {
        stats.cycles += ((offset/4) * 100); //store the modified
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
    Set& set = c.sets[indexes.first]; //get the respective set
    //set.slots[indexes.second].access_ts = stats.cycles;
    //If cache hit, update cacheStats
    if (cache_hit.first == true) {
        // update l_hits
        stats.l_hits++;
        set.slots[indexes.second].access_ts = stats.cycles;
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
        stats.cycles += ((info.offset/4) * 100); //to access main memory
        if (indexes.second != info.assoc) {
            //means replacement can happen without eviction
        } else {
            //eviction differes based on LRU or FIFO - seperate here?
            std::string lru_fifo = info.evic;
            unsigned to_replace;
            if (lru_fifo == "lru") {
                //LRU eviction logic, look for the first access_ts
                to_replace = lru_evict(set, info.write_through, info.offset);
            } else if (lru_fifo == "fifo") {
                //FIFO eviction logic, look for the first load_ts
                to_replace = fifo_evict(set, info.write_through, info.offset);
            } else {
                std::cerr << "wrong parameter for fifo / lru" << std::endl;
            }
            indexes.second = to_replace;
        }
        set.slots[indexes.second].tag = addr;
        set.slots[indexes.second].valid = true;
        set.slots[indexes.second].dirty = false;
        set.slots[indexes.second].load_ts = stats.cycles;
        set.slots[indexes.second].access_ts = stats.cycles;
    }
    
}


//having a general function for writing to the cache will be useful

void store(Cache& c, const cacheInfo& info, uint32_t addr) {
    
    stats.stores++;
    //Sequential search to determine Tag hit
    std::pair<bool, std::pair<unsigned, unsigned>> cache_hit;

    stats.cycles++; //single cache load
    cache_hit = checkHit(c, info, addr);
    std::pair<unsigned, unsigned>& indexes = cache_hit.second;
    Set& set = c.sets[indexes.first]; //get the respective set

    //If Cache hit, update cacheStats , and based on write-through or back
    if (cache_hit.first == true) {
        // update s_hits;
        stats.s_hits++;
        
        // update the cache for write-through (both cache and memory)
        if (info.write_through == "write-through") {
            stats.cycles += 100;//((info.offset/4) * 100); //for main mem access
            stats.cycles++; //for cache store? check this
        } else if (info.write_through == "write-back") {
            // or non-write through (only cache, marks block dirty)
            set.slots[indexes.second].dirty = true; //the cache contents have changed locally
            stats.cycles++;
        } else {
            std::cerr << "write_through error doesn't exist" << std::endl;
        }
        set.slots[indexes.second].access_ts = stats.cycles;
    } else {
        //If Cache miss, update cacheStats, and determine write-allocate or no
        // update s_misses;
        stats.s_misses++;
        // update the caceh for write-allocate (write relvant memroy to cache)
        if (info.write_alloc == "write-allocate") {
            if (indexes.second != info.assoc) {
                //stats.cycles++; //we update the cache where its empty -- necessary addition?
            } else {
                //eviction differes based on LRU or FIFO - seperate here?
                std::string lru_fifo = info.evic;
                unsigned to_replace;
                if (lru_fifo == "lru") {
                    //LRU eviction logic, look for the first access_ts
                    to_replace = lru_evict(set, info.write_through, info.offset);
                } else if (lru_fifo == "fifo") {
                    //FIFO eviction logic, look for the first load_ts
                    to_replace = fifo_evict(set, info.write_through, info.offset);
                } else {
                    std::cerr << "wrong parameter for fifo / lru" << std::endl;
                }
                indexes.second = to_replace;
            }
            set.slots[indexes.second].tag = addr;
            set.slots[indexes.second].valid = true;
            set.slots[indexes.second].dirty = false;
            set.slots[indexes.second].load_ts = stats.cycles;
            set.slots[indexes.second].access_ts = stats.cycles;
            if ( info.write_through == "write-through" ) {
                stats.cycles += 100; //we write to the main mem
            }
            stats.cycles++; //single cache store
            stats.cycles += ((info.offset/4) * 100); //load from the main memory to the cache
        } else if (info.write_alloc == "no-write-allocate") {
            // no-write allocate will not modify the cache here
            stats.cycles += 100; //write straight to memory
        } else {
            std::cerr << "write_alloc error: doesn't exist" << std::endl;
        }
        
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