#include "cache.h"
#include "dogfault.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// DO NOT MODIFY THIS FILE. INVOKE AFTER EACH ACCESS FROM runTrace
void print_result(result r) {
  if (r.status == CACHE_EVICT)
    printf(" [status: miss eviction, victim_block: 0x%llx, insert_block: 0x%llx]",
           r.victim_block_addr, r.insert_block_addr);
  if (r.status == CACHE_HIT)
    printf(" [status: hit]");
  if (r.status == CACHE_MISS)
    printf(" [status: miss, insert_block: 0x%llx]", r.insert_block_addr);
}

/* This is the entry point to operate the cache for a given address in the trace file.
 * First, is increments the global lru_clock in the corresponding cache set for the address.
 * Second, it checks if the address is already in the cache using the "probe_cache" function.
 * If yes, it is a cache hit:
 *     1) call the "hit_cacheline" function to update the counters inside the hit cache 
 *        line, including its lru_clock and access_counter.
 *     2) record a hit status in the return "result" struct and update hit_count 
 * Otherwise, it is a cache miss:
 *     1) call the "insert_cacheline" function, trying to find an empty cache line in the
 *        cache set and insert the address into the empty line. 
 *     2) if the "insert_cacheline" function returns true, record a miss status and the
          inserted block address in the return "result" struct and update miss_count
 *     3) otherwise, if the "insert_cacheline" function returns false:
 *          a) call the "victim_cacheline" function to figure which victim cache line to 
 *             replace based on the cache replacement policy (LRU and LFU).
 *          b) call the "replace_cacheline" function to replace the victim cache line with
 *             the new cache line to insert.
 *          c) record an eviction status, the victim block address, and the inserted block
 *             address in the return "result" struct. Update miss_count and eviction_count.
 */
result operateCache(const unsigned long long address, Cache *cache) {
    result r={0};
    unsigned long long set_idx = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    unsigned long long block_addr = address_to_block(address, cache);
  
    Set *set = &cache->sets[set_idx];
    set->lru_clock++;  // Increment global LRU clock

    if (probe_cache(address, cache)) {
        hit_cacheline(address, cache);
        r.status = CACHE_HIT;
        cache->hit_count++;
    } else {
        if (insert_cacheline(address, cache)) {
            r.status = CACHE_MISS;
            r.insert_block_addr = block_addr;
            cache->miss_count++;
        } else {
            unsigned long long victim_addr = victim_cacheline(address, cache);
            replace_cacheline(victim_addr, address, cache);
            r.status = CACHE_EVICT;
            r.victim_block_addr = victim_addr;
            r.insert_block_addr = block_addr;
            cache->miss_count++;
            cache->eviction_count++;
        }
    }
    return r;
}

// HELPER FUNCTIONS USEFUL FOR IMPLEMENTING THE CACHE
// Given an address, return the block (aligned) address,
// i.e., byte offset bits are cleared to 0
unsigned long long address_to_block(const unsigned long long address,
                                    const Cache *cache) {
    return address >> cache->blockBits << cache->blockBits;//return address with a shift right ie removing blockoffset bits,
    //and then shift back left to align starting address
}

// Return the cache tag of an address
unsigned long long cache_tag(const unsigned long long address,
                             const Cache *cache) {
    return address >> (cache->blockBits + cache->setBits);
}

// Return the cache set index of the address
unsigned long long cache_set(const unsigned long long address,
                             const Cache *cache) {
  /* YOUR CODE HERE */
return (address>>cache->blockBits) & ((1ULL<<cache->setBits)-1);
}

// Check if the address is found in the cache. If so, return true. else return false.
bool probe_cache(const unsigned long long address, const Cache *cache) {
    unsigned long long set_idx = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    Set *set = &cache->sets[set_idx];//retrieves the set where data wouldbe stored

    for (int i = 0; i < cache->linesPerSet; i++) {//for loop to check if requested data is present
        Line *line = &set->lines[i];
        if (line->valid && line->tag == tag) {//valid indicates whether line holds actual data and tag indentifies which mmeory block is stored
            return true;
        }
    }
    return false;
}

// Access address in cache. Called only if probe is successful.
// Update the LRU (least recently used) or LFU (least frequently used) counters.
void hit_cacheline(const unsigned long long address, Cache *cache) {
    unsigned long long set_idx = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    Set *set = &cache->sets[set_idx];

    for (int i = 0; i < cache->linesPerSet; i++) {
        Line *line = &set->lines[i];
        if (line->valid && line->tag == tag) {
            line->lru_clock = set->lru_clock;//global counter that increments every time the set is acces(ie any linewithin that set)..then marking said line as recently used 
            line->access_counter++;//incrementing access counter (LFU)
            break;
        }
    }
}


/* This function is only called if probe_cache returns false, i.e., the address is
 * not in the cache. In this function, it will try to find an empty (i.e., invalid)
 * cache line for the address to insert. 
 * If it found an empty one:
 *     1) it inserts the address into that cache line (marking it valid).
 *     2) it updates the cache line's lru_clock based on the global lru_clock 
 *        in the cache set and initiates the cache line's access_counter.
 *     3) it returns true.
 * Otherwise, it returns false.  
 */ 
bool insert_cacheline(const unsigned long long address, Cache *cache) {
    unsigned long long set_idx = cache_set(address, cache);
    unsigned long long tag = cache_tag(address, cache);
    Set *set = &cache->sets[set_idx];

    for (int i = 0; i < cache->linesPerSet; i++) {
        Line *line = &set->lines[i];
        if (!line->valid) {
            line->valid = true;//make said invalid line now valid
            line->tag = tag;//tag the line with the tag 
            line->lru_clock = set->lru_clock;//update lruclock to mark recently used
            line->access_counter = 1;//initiate and set counter value to 1 as it was accesed once (LFU)
            return true;
        }
    }
    return false;
}


// If there is no empty cacheline, this method figures out which cacheline to replace
// depending on the cache replacement policy (LRU and LFU). It returns the block address
// of the victim cacheline; note we no longer have access to the full address of the victim
unsigned long long victim_cacheline(const unsigned long long address,
                                    const Cache *cache) {
    unsigned long long set_idx = cache_set(address, cache);
    Set *set = &cache->sets[set_idx];
    Line *victim = &set->lines[0];//assume first line is victim 

    for (int i = 1; i < cache->linesPerSet; i++) {
        Line *line = &set->lines[i];

        if (cache->lfu == 0) {//what if they have same lru clock values? 
            if (line->lru_clock < victim->lru_clock) {
                victim = line;
            }
        } else if (cache->lfu == 1) {
            if (line->access_counter < victim->access_counter ||
                (line->access_counter == victim->access_counter &&
                 line->lru_clock < victim->lru_clock)) {
                victim = line;
            }
        }
    }

    return (victim->tag << (cache->setBits + cache->blockBits)) | (set_idx << cache->blockBits);
}


/* Replace the victim cacheline with the new address to insert. Note for the victim cachline,
 * we only have its block address. For the new address to be inserted, we have its full address.
 * Remember to update the new cache line's lru_clock based on the global lru_clock in the cache
 * set and initiate the cache line's access_counter.
 */
void replace_cacheline(const unsigned long long victim_block_addr,
                       const unsigned long long insert_addr, Cache *cache) {
    unsigned long long set_idx = cache_set(victim_block_addr, cache);
    unsigned long long victim_tag = cache_tag(victim_block_addr, cache);
    unsigned long long new_tag = cache_tag(insert_addr, cache);
    Set *set = &cache->sets[set_idx];

    for (int i = 0; i < cache->linesPerSet; i++) {
        Line *line = &set->lines[i];
        if (line->valid && line->tag == victim_tag) {
            line->tag = new_tag;
            line->lru_clock = set->lru_clock;
            line->access_counter = 1;
            return;
        }
    }
}


// allocate the memory space for the cache with the given cache parameters
// and initialize the cache sets and lines.
// Initialize the cache name to the given name 
void cacheSetUp(Cache *cache, char *name) {
  
    // Calculate number of sets
    int num_sets= 1 << cache->setBits;

    // Allocate memory for cache sets
    cache->sets = (Set *)malloc(sizeof(Set) * num_sets);
   assert(cache->sets!=NULL);
    for (int i = 0; i < num_sets; i++) {
        cache->sets[i].lines = (Line *)malloc(sizeof(Line) * cache->linesPerSet);
     assert(cache->sets[i].lines!=NULL);
        for (int j = 0; j < cache->linesPerSet; j++) {
            cache->sets[i].lines[j].valid = false;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].lru_clock = 0;
            cache->sets[i].lines[j].access_counter = 0;
        }
           cache->sets[i].lru_clock = 0;

    }

    // Initialize counters
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->eviction_count = 0;

    cache->name=strdup(name);
    assert(cache->name!=NULL);
}
// deallocate the memory space for the cache
void deallocate(Cache *cache) {
    if (!cache || !cache->sets) return;
   int num_sets= 1 << cache->setBits;

    for (int i = 0; i < num_sets; i++) {
        Set *set=&cache->sets[i];
        if(set->lines){
          free(set->lines);
          set->lines=NULL;
        }
    }

    free(cache->sets);
    cache->sets = NULL;
}

// print out summary stats for the cache
void printSummary(const Cache *cache) {
  printf("%s hits: %d, misses: %d, evictions: %d\n", cache->name, cache->hit_count,
         cache->miss_count, cache->eviction_count);
}
