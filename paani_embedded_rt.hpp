#ifndef PAANI_EMBEDDED_RT_HPP
#define PAANI_EMBEDDED_RT_HPP

// Auto-generated embedded paani SoA runtime
// Archetype-Based SoA ECS with cache-friendly data layout

constexpr const char* PAANI_ECS_H_CONTENT = R"SOA_HEADER(
// paani_ecs_soa.h - Paani ECS Runtime with Archetype-Based SoA Storage
// High-performance ECS with cache-friendly data layout
// API compatible with paani_ecs.h
// Copyright 2026

#ifndef PAANI_ECS_SOA_H
#define PAANI_ECS_SOA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============ Types (compatible with original) ============

typedef uint64_t paani_entity_t;
typedef uint32_t paani_ctype_t;
typedef uint32_t paani_trait_t;
typedef uint32_t paani_system_t;

#define PAANI_ENTITY_INDEX(e) ((uint32_t)(e))
#define PAANI_ENTITY_GEN(e)   ((uint32_t)((e) >> 32))
#define PAANI_ENTITY_MAKE(idx, gen) (((uint64_t)(gen) << 32) | (uint32_t)(idx))
#define PAANI_ENTITY_NULL ((paani_entity_t)-1)
#define PAANI_INVALID_CTYPE ((paani_ctype_t)-1)
#define PAANI_INVALID_TRAIT ((paani_trait_t)-1)
#define PAANI_INVALID_SYSTEM ((paani_system_t)-1)

// Forward declarations
typedef struct paani_world paani_world_t;
typedef struct paani_query paani_query_t;

// Component lifecycle callbacks
typedef void (*paani_ctor_fn)(void* ptr, const void* init_data);
typedef void (*paani_dtor_fn)(void* ptr);
typedef void (*paani_system_fn)(paani_world_t* world, float dt, void* userdata);

// ============ Error Handling ============

typedef enum {
    PAANI_OK = 0,
    PAANI_ERR_NULL_POINTER = -1,
    PAANI_ERR_INVALID_ENTITY = -2,
    PAANI_ERR_INVALID_COMPONENT = -3,
    PAANI_ERR_INVALID_TRAIT = -4,
    PAANI_ERR_COMPONENT_NOT_FOUND = -5,
    PAANI_ERR_TRAIT_NOT_FOUND = -6,
    PAANI_ERR_OUT_OF_MEMORY = -7,
    PAANI_ERR_ENTITY_NOT_ALIVE = -8,
    PAANI_ERR_COMPONENT_ALREADY_EXISTS = -9,
    PAANI_ERR_TRAIT_REQUIREMENTS_NOT_MET = -10,
} paani_error_t;

const char* paani_get_error(void);
void paani_clear_error(void);

// ============ World ============

paani_world_t* paani_world_create(void);
void paani_world_destroy(paani_world_t* world);
void paani_world_clear(paani_world_t* world);
uint32_t paani_world_entity_count(const paani_world_t* world);

// ============ Entity ============

paani_entity_t paani_entity_create(paani_world_t* world);
void paani_entity_destroy(paani_world_t* world, paani_entity_t entity);
int paani_entity_alive(const paani_world_t* world, paani_entity_t entity);
paani_entity_t paani_entity_clone(paani_world_t* world, paani_entity_t source);

// ============ Deferred Entity Destruction ============

void paani_entity_destroy_deferred(paani_world_t* world, paani_entity_t entity);
void paani_entity_process_deferred_destroys(paani_world_t* world);
void paani_entity_clear_deferred_destroys(paani_world_t* world);

// ============ Component ============

paani_ctype_t paani_component_register(paani_world_t* world, 
                                        uint32_t size,
                                        paani_ctor_fn ctor,
                                        paani_dtor_fn dtor);

void paani_component_add(paani_world_t* world, 
                         paani_entity_t entity,
                         paani_ctype_t type, 
                         const void* data);

void paani_component_remove(paani_world_t* world, 
                            paani_entity_t entity,
                            paani_ctype_t type);

void* paani_component_get(paani_world_t* world, 
                          paani_entity_t entity,
                          paani_ctype_t type);

int paani_component_has(const paani_world_t* world, 
                        paani_entity_t entity,
                        paani_ctype_t type);

// ============ Trait ============

paani_trait_t paani_trait_register(paani_world_t* world,
                                   const paani_ctype_t* components,
                                   uint32_t num_components);

void paani_trait_add(paani_world_t* world, 
                     paani_entity_t entity,
                     paani_trait_t trait);

void paani_trait_remove(paani_world_t* world, 
                        paani_entity_t entity,
                        paani_trait_t trait);

int paani_entity_has_trait(const paani_world_t* world, 
                           paani_entity_t entity,
                           paani_trait_t trait);

// ============ Query (Standard) ============

typedef struct {
    paani_entity_t entity;
    void** components;
    uint32_t capacity;
} paani_query_result_t;

paani_query_t* paani_query_create(paani_world_t* world,
                                   const paani_ctype_t* types,
                                   uint32_t num_types);

paani_query_t* paani_query_trait(paani_world_t* world,
                                  paani_trait_t trait);

void paani_query_destroy(paani_query_t* query);
uint32_t paani_query_count(const paani_query_t* query);
int paani_query_next(paani_query_t* query, paani_query_result_t* out_result);
void paani_query_reset(paani_query_t* query);

// ============ Batch Iteration API (Faster than iterator) ============
// Get total entity count for batch iteration
uint32_t paani_query_entity_count(paani_query_t* query);

// Get entity and components by index (for batch iteration)
// Returns 1 on success, 0 if index out of bounds
int paani_query_get(paani_query_t* query, 
                    uint32_t index,
                    paani_entity_t* out_entity,
                    void** out_components,
                    uint32_t max_components);

// ============ SoA Query (High Performance) ============
// Direct access to contiguous component arrays

typedef struct {
    void** component_columns;  // [type_index] -> pointer to contiguous array
    uint32_t count;
    uint32_t num_types;
} paani_query_soa_t;

// Get SoA view for components (fastest, contiguous memory)
int paani_query_soa_get(paani_world_t* world,
                        const paani_ctype_t* types,
                        uint32_t num_types,
                        paani_query_soa_t* out_soa);

// Release SoA view
void paani_query_soa_release(paani_query_soa_t* soa);

// ============ System ============

paani_system_t paani_system_register(paani_world_t* world,
                                     const char* name,
                                     paani_system_fn fn,
                                     void* userdata);

void paani_system_lock(paani_world_t* world, paani_system_t system);
void paani_system_unlock(paani_world_t* world, paani_system_t system);
int paani_system_is_locked(paani_world_t* world, paani_system_t system);

void paani_system_depend(paani_world_t* world,
                         paani_system_t before,
                         paani_system_t after);

// Find a system by name (returns PAANI_INVALID_SYSTEM if not found)
paani_system_t paani_system_find(paani_world_t* world, const char* name);

void paani_system_run_all(paani_world_t* world, float dt);

// Get all systems (for iteration)
typedef struct {
    paani_system_t* systems;
    uint32_t count;
} paani_system_list_t;

paani_system_list_t* paani_system_get_all(paani_world_t* world, uint32_t* out_count);
void paani_free_system_list(paani_system_list_t* list);

// ============ Exit Flag ============

void paani_exit(paani_world_t* world);
int paani_should_exit(paani_world_t* world);
void paani_clear_exit(paani_world_t* world);

#ifdef __cplusplus
}
#endif

#endif // PAANI_ECS_SOA_H
)SOA_HEADER";

constexpr const char* PAANI_ECS_C_CONTENT = R"SOA_SOURCE(
// paani_ecs_soa.c - Archetype-Based SoA ECS Runtime Implementation
// High-performance with cache-friendly data layout
// API compatible with paani_ecs.c

#include "paani_ecs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#ifdef _MSC_VER
    #define PAANI_THREAD_LOCAL __declspec(thread)
#else
    #define PAANI_THREAD_LOCAL __thread
#endif

// ============ Error Handling ============

static PAANI_THREAD_LOCAL char paani_error_msg[256] = {0};

static void paani_set_error(const char* msg) {
    strncpy(paani_error_msg, msg, sizeof(paani_error_msg) - 1);
    paani_error_msg[sizeof(paani_error_msg) - 1] = '\0';
}

const char* paani_get_error(void) {
    return paani_error_msg[0] ? paani_error_msg : NULL;
}

void paani_clear_error(void) {
    paani_error_msg[0] = '\0';
}

#define PAANI_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        paani_set_error(msg); \
        return; \
    } \
} while(0)

#define PAANI_ASSERT_RET(cond, msg, ret) do { \
    if (!(cond)) { \
        paani_set_error(msg); \
        return (ret); \
    } \
} while(0)

// ============ Constants ============

#define PAANI_INITIAL_ENTITY_CAPACITY 1024
#define PAANI_INITIAL_ARCHETYPE_CAPACITY 16
#define PAANI_ARCHETYPE_INITIAL_ROWS 64

// Chunked array initial sizes (will grow dynamically)
#define PAANI_INITIAL_COMPONENT_CAPACITY 64
#define PAANI_INITIAL_TRAIT_CAPACITY 32
#define PAANI_INITIAL_SYSTEM_CAPACITY 128

// ============ Forward Declarations ============

typedef struct paani_archetype paani_archetype_t;
typedef struct paani_component_meta paani_component_meta_t;

// ============ Core Structures ============

typedef struct {
    uint32_t generation;
    int alive;
    paani_archetype_t* archetype;
    uint32_t row_index;
    uint64_t trait_set;  // Bitmask of trait membership (each bit = one trait)
} paani_entity_record_t;

typedef struct paani_component_meta {
    uint32_t size;
    paani_ctor_fn ctor;
    paani_dtor_fn dtor;
    char name[32];
} paani_component_meta_t;

typedef struct paani_trait_def {
    paani_ctype_t* components;
    uint32_t num_components;
    char name[32];
} paani_trait_def_t;

typedef struct paani_system_record {
    char name[64];
    paani_system_fn fn;
    void* userdata;
    uint32_t* dependencies;
    uint32_t num_dependencies;
    uint32_t dependency_capacity;
    int locked;
} paani_system_record_t;

// ============ Hybrid Dynamic Array (Scheme C + Geometric Block Growth) ============
// Strategy:
//   - First 1024 elements: contiguous array (cache-friendly for common case)
//   - Beyond 1024: chunked linked list with geometric growth (1.5x ratio)
// Block sizes: 1024, 1536, 2304, 3456, 5184...
// This balances cache locality with unlimited growth capability

#define PAANI_INLINE_THRESHOLD 1024  // Threshold for inline contiguous storage
#define PAANI_GROWTH_RATIO_NUM 3     // Growth ratio numerator (3/2 = 1.5x)
#define PAANI_GROWTH_RATIO_DEN 2

typedef struct {
    // Inline contiguous storage for small arrays (first 1024 elements)
    void* inline_data;       // Contiguous buffer up to inline_threshold
    uint32_t inline_capacity; // Capacity of inline buffer
    
    // Chunked storage for large arrays (beyond threshold)
    void** chunks;           // Array of chunk pointers
    uint32_t num_chunks;     // Number of allocated chunks
    uint32_t chunk_capacity; // Capacity of chunks array
    uint32_t chunk_start_index; // Index where chunks start (PAANI_INLINE_THRESHOLD)
    
    // Common metadata
    uint32_t total_count;    // Total number of elements
    uint32_t element_size;   // Size of each element
    uint32_t first_chunk_size; // Size of first chunk after inline
} paani_hybrid_array_t;

// Calculate chunk size with geometric growth (1.5x ratio)
static uint32_t paani_hybrid_chunk_size(uint32_t chunk_index, uint32_t first_size) {
    // chunk_index 0 corresponds to the first chunk after inline storage
    // Sizes: first_size, first_size*1.5, first_size*1.5^2, ...
    uint32_t size = first_size;
    for (uint32_t i = 0; i < chunk_index; i++) {
        size = (size * PAANI_GROWTH_RATIO_NUM) / PAANI_GROWTH_RATIO_DEN;
        // Ensure minimum growth
        if (size < first_size + 64) size = first_size + 64;
    }
    return size;
}

static paani_hybrid_array_t* paani_hybrid_array_create(uint32_t element_size, uint32_t initial_hint) {
    paani_hybrid_array_t* arr = (paani_hybrid_array_t*)calloc(1, sizeof(paani_hybrid_array_t));
    if (!arr) return NULL;
    
    arr->element_size = element_size;
    arr->inline_capacity = PAANI_INLINE_THRESHOLD;
    arr->chunk_start_index = PAANI_INLINE_THRESHOLD;
    
    // Allocate inline buffer
    arr->inline_data = calloc(PAANI_INLINE_THRESHOLD, element_size);
    if (!arr->inline_data) {
        free(arr);
        return NULL;
    }
    
    // Setup for chunked storage (initially empty)
    arr->chunk_capacity = 8;
    arr->chunks = (void**)calloc(arr->chunk_capacity, sizeof(void*));
    if (!arr->chunks) {
        free(arr->inline_data);
        free(arr);
        return NULL;
    }
    
    // First chunk size based on element size (at least 1024 elements worth)
    arr->first_chunk_size = initial_hint > PAANI_INLINE_THRESHOLD ? initial_hint : PAANI_INLINE_THRESHOLD;
    
    return arr;
}

static void paani_hybrid_array_destroy(paani_hybrid_array_t* arr) {
    if (!arr) return;
    free(arr->inline_data);
    for (uint32_t i = 0; i < arr->num_chunks; i++) {
        free(arr->chunks[i]);
    }
    free(arr->chunks);
    free(arr);
}

static void* paani_hybrid_array_get(paani_hybrid_array_t* arr, uint32_t index) {
    if (!arr || index >= arr->total_count) return NULL;
    
    // Fast path: inline storage
    if (index < PAANI_INLINE_THRESHOLD) {
        return (char*)arr->inline_data + index * arr->element_size;
    }
    
    // Slow path: chunked storage
    uint32_t chunk_index = 0;
    uint32_t elements_before = PAANI_INLINE_THRESHOLD;
    uint32_t chunk_size = paani_hybrid_chunk_size(0, arr->first_chunk_size);
    
    while (index >= elements_before + chunk_size) {
        elements_before += chunk_size;
        chunk_index++;
        chunk_size = paani_hybrid_chunk_size(chunk_index, arr->first_chunk_size);
    }
    
    // Ensure chunk is allocated
    if (chunk_index >= arr->num_chunks) return NULL;
    
    uint32_t offset_in_chunk = index - elements_before;
    return (char*)arr->chunks[chunk_index] + offset_in_chunk * arr->element_size;
}

static int paani_hybrid_array_ensure_capacity(paani_hybrid_array_t* arr, uint32_t min_capacity) {
    if (!arr) return 0;
    if (min_capacity <= arr->total_count) return 1;
    
    // Check if we need chunked storage
    if (min_capacity > PAANI_INLINE_THRESHOLD) {
        uint32_t needed_in_chunks = min_capacity - PAANI_INLINE_THRESHOLD;
        uint32_t current_chunk_capacity = 0;
        
        // Calculate current capacity in chunks
        for (uint32_t i = 0; i < arr->num_chunks; i++) {
            current_chunk_capacity += paani_hybrid_chunk_size(i, arr->first_chunk_size);
        }
        
        // Allocate new chunks as needed
        while (current_chunk_capacity < needed_in_chunks) {
            // Grow chunks array if needed
            if (arr->num_chunks >= arr->chunk_capacity) {
                arr->chunk_capacity *= 2;
                void** new_chunks = (void**)realloc(arr->chunks, arr->chunk_capacity * sizeof(void*));
                if (!new_chunks) return 0;
                arr->chunks = new_chunks;
            }
            
            // Calculate and allocate new chunk
            uint32_t chunk_size = paani_hybrid_chunk_size(arr->num_chunks, arr->first_chunk_size);
            arr->chunks[arr->num_chunks] = calloc(chunk_size, arr->element_size);
            if (!arr->chunks[arr->num_chunks]) return 0;
            
            current_chunk_capacity += chunk_size;
            arr->num_chunks++;
        }
    }
    
    return 1;
}

static void* paani_hybrid_array_push(paani_hybrid_array_t* arr) {
    if (!arr) return NULL;
    
    uint32_t index = arr->total_count;
    if (!paani_hybrid_array_ensure_capacity(arr, index + 1)) return NULL;
    
    arr->total_count++;
    return paani_hybrid_array_get(arr, index);
}

static uint32_t paani_hybrid_array_count(const paani_hybrid_array_t* arr) {
    return arr ? arr->total_count : 0;
}

// Backward compatibility: old chunked_array API maps to hybrid_array
typedef paani_hybrid_array_t paani_chunked_array_t;

static paani_chunked_array_t* paani_chunked_array_create(uint32_t element_size, uint32_t initial_hint) {
    return paani_hybrid_array_create(element_size, initial_hint);
}

static void paani_chunked_array_destroy(paani_chunked_array_t* arr) {
    paani_hybrid_array_destroy(arr);
}

static void* paani_chunked_array_get(paani_chunked_array_t* arr, uint32_t index) {
    return paani_hybrid_array_get(arr, index);
}

static void* paani_chunked_array_push(paani_chunked_array_t* arr) {
    return paani_hybrid_array_push(arr);
}

// Archetype: SoA storage for entities with the same component signature
typedef struct paani_archetype {
    uint64_t signature;              // Bitmask of component types
    paani_ctype_t* component_types;  // Sorted list of component types
    uint32_t num_types;
    
    // SoA storage: each component type has a contiguous array
    uint8_t** component_data;        // [type_index][row_index * component_size]
    uint32_t* component_sizes;       // Size of each component type
    uint32_t capacity;               // Maximum rows
    uint32_t count;                  // Current rows (entities)
    
    // Entity IDs (corresponds to rows)
    paani_entity_t* entities;
    
    // Archetype graph edges for fast component add/remove (dynamically sized)
    paani_archetype_t** add_edges;      // [ctype] -> archetype after adding
    paani_archetype_t** remove_edges;   // [ctype] -> archetype after removing
    uint32_t edge_capacity;             // Capacity of edge arrays (grows dynamically)
} paani_archetype_t;

// Batch cache structure (defined early for use in paani_query)
typedef struct paani_batch_cache {
    uint32_t* cumulative_counts;
    uint32_t num_archetypes;
    uint32_t version;
} paani_batch_cache_t;

typedef struct paani_query {
    paani_world_t* world;
    paani_ctype_t* types;
    uint32_t num_types;
    paani_trait_t trait;
    
    // Matching archetypes
    paani_archetype_t** archetypes;
    uint32_t num_archetypes;
    uint32_t current_archetype;
    uint32_t current_row;
    
    // Pre-computed component indices for O(1) access
    // component_indices[i][j] = index of types[j] in archetypes[i]
    uint32_t** component_indices;
    
    uint64_t version;
    
    // Cache for batch iteration (lazy initialized)
    paani_batch_cache_t* batch_cache;
} paani_query_t;

// ============ Query Cache with Incremental Update ============
// Provides O(1) query performance with smart cache invalidation
// Only updates affected queries when archetypes change, not entire cache

typedef struct {
    uint64_t signature;              // Query signature (hash of component types)
    paani_archetype_t** matches;     // Cached matching archetypes
    uint32_t num_matches;
    uint32_t capacity;
    uint64_t required_sig;           // Bitmask of required components
    uint32_t num_types;              // Number of component types in query
    int is_valid;                    // 1 if cache entry is valid, 0 if needs rebuild
} paani_cached_query_t;

typedef struct {
    paani_cached_query_t* entries;   // Cache entries
    uint32_t num_entries;
    uint32_t capacity;
} paani_query_cache_t;

static uint64_t paani_query_signature_compute(const paani_ctype_t* types, uint32_t num_types) {
    // FNV-1a hash for query signature
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t i = 0; i < num_types; i++) {
        hash ^= types[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static paani_query_cache_t* paani_query_cache_create(void) {
    paani_query_cache_t* cache = (paani_query_cache_t*)calloc(1, sizeof(paani_query_cache_t));
    if (!cache) return NULL;
    
    cache->capacity = 64;
    cache->entries = (paani_cached_query_t*)calloc(cache->capacity, sizeof(paani_cached_query_t));
    if (!cache->entries) {
        free(cache);
        return NULL;
    }
    
    return cache;
}

static void paani_query_cache_destroy(paani_query_cache_t* cache) {
    if (!cache) return;
    
    for (uint32_t i = 0; i < cache->num_entries; i++) {
        free(cache->entries[i].matches);
    }
    free(cache->entries);
    free(cache);
}

// Legacy full invalidate (rarely used now)
static void paani_query_cache_invalidate_all(paani_query_cache_t* cache) {
    if (!cache) return;
    for (uint32_t i = 0; i < cache->num_entries; i++) {
        cache->entries[i].is_valid = 0;
    }
}

// Find cached query, returns NULL if not found or stale
static paani_cached_query_t* paani_query_cache_find(paani_query_cache_t* cache, 
                                                     uint64_t signature) {
    if (!cache) return NULL;
    
    for (uint32_t i = 0; i < cache->num_entries; i++) {
        if (cache->entries[i].signature == signature && cache->entries[i].is_valid) {
            return &cache->entries[i];
        }
    }
    
    return NULL;
}

// Incremental update: Add new archetype to matching cached queries
static void paani_query_cache_add_archetype(paani_query_cache_t* cache, 
                                             paani_archetype_t* arch,
                                             uint32_t arch_index) {
    if (!cache || !arch) return;
    
    // For each cached query, check if it matches the new archetype
    for (uint32_t i = 0; i < cache->num_entries; i++) {
        paani_cached_query_t* entry = &cache->entries[i];
        if (!entry->is_valid) continue;
        
        // Check if archetype has all required components for this query
        if ((arch->signature & entry->required_sig) == entry->required_sig) {
            // Match! Add archetype to this query's cache
            if (entry->num_matches >= entry->capacity) {
                entry->capacity = entry->capacity > 0 ? entry->capacity * 2 : 4;
                paani_archetype_t** new_matches = (paani_archetype_t**)realloc(entry->matches, 
                                                                                entry->capacity * sizeof(paani_archetype_t*));
                if (!new_matches) continue;
                entry->matches = new_matches;
            }
            entry->matches[entry->num_matches++] = arch;
        }
    }
}

// Add query result to cache
static paani_cached_query_t* paani_query_cache_add(paani_query_cache_t* cache,
                                                    uint64_t signature,
                                                    uint64_t required_sig,
                                                    uint32_t num_types,
                                                    paani_archetype_t** matches,
                                                    uint32_t num_matches) {
    if (!cache) return NULL;
    
    // Grow cache if needed
    if (cache->num_entries >= cache->capacity) {
        uint32_t new_capacity = cache->capacity * 2;
        paani_cached_query_t* new_entries = (paani_cached_query_t*)realloc(cache->entries, 
                                                                           new_capacity * sizeof(paani_cached_query_t));
        if (!new_entries) return NULL;
        
        memset(&new_entries[cache->capacity], 0, (new_capacity - cache->capacity) * sizeof(paani_cached_query_t));
        cache->entries = new_entries;
        cache->capacity = new_capacity;
    }
    
    paani_cached_query_t* entry = &cache->entries[cache->num_entries++];
    entry->signature = signature;
    entry->required_sig = required_sig;
    entry->num_types = num_types;
    entry->num_matches = num_matches;
    entry->capacity = num_matches;
    entry->is_valid = 1;
    
    // Copy matches
    entry->matches = (paani_archetype_t**)malloc(num_matches * sizeof(paani_archetype_t*));
    if (entry->matches) {
        memcpy(entry->matches, matches, num_matches * sizeof(paani_archetype_t*));
    }
    
    return entry;
}

// ============ Bitset Index for Fast Query ============
// Each component type has a bitset where bit i = 1 means archetype i has this component
// Provides O(A/64) query performance instead of O(A)

typedef struct {
    uint64_t* bits;        // Array of 64-bit words
    uint32_t num_bits;     // Number of archetypes tracked
    uint32_t capacity;     // Capacity in 64-bit words
} paani_bitset_index_t;

static void paani_bitset_init(paani_bitset_index_t* index) {
    index->bits = NULL;
    index->num_bits = 0;
    index->capacity = 0;
}

static void paani_bitset_free(paani_bitset_index_t* index) {
    free(index->bits);
    index->bits = NULL;
    index->num_bits = 0;
    index->capacity = 0;
}

static int paani_bitset_ensure_capacity(paani_bitset_index_t* index, uint32_t min_bits) {
    uint32_t required_words = (min_bits + 63) / 64;
    if (required_words <= index->capacity) return 1;
    
    uint64_t* new_bits = (uint64_t*)realloc(index->bits, required_words * sizeof(uint64_t));
    if (!new_bits) return 0;
    
    // Zero new memory
    for (uint32_t i = index->capacity; i < required_words; i++) {
        new_bits[i] = 0;
    }
    
    index->bits = new_bits;
    index->capacity = required_words;
    return 1;
}

static void paani_bitset_set(paani_bitset_index_t* index, uint32_t bit) {
    if (bit >= index->num_bits) index->num_bits = bit + 1;
    if (!paani_bitset_ensure_capacity(index, index->num_bits)) return;
    index->bits[bit / 64] |= (1ULL << (bit % 64));
}

static void paani_bitset_clear(paani_bitset_index_t* index, uint32_t bit) {
    if (bit >= index->num_bits) return;
    index->bits[bit / 64] &= ~(1ULL << (bit % 64));
}

static int paani_bitset_get(const paani_bitset_index_t* index, uint32_t bit) {
    if (bit >= index->num_bits) return 0;
    return (index->bits[bit / 64] >> (bit % 64)) & 1ULL;
}

#ifdef _MSC_VER
    #include <intrin.h>
#endif

// Cross-platform count trailing zeros
static inline uint32_t paani_ctz64(uint64_t x) {
    if (x == 0) return 64;
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward64(&index, x);
    return (uint32_t)index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(x);
#else
    // Fallback for other compilers
    uint32_t n = 0;
    while (((x >> n) & 1) == 0 && n < 64) n++;
    return n;
#endif
}

// AND multiple bitsets together, result stored in out_indices (list of set bit positions)
static uint32_t paani_bitset_and_query(paani_bitset_index_t** indices, uint32_t num_indices,
                                        uint32_t max_archetypes,
                                        uint32_t* out_matching_archetypes, uint32_t max_results) {
    if (num_indices == 0 || !out_matching_archetypes) return 0;
    
    uint32_t num_words = (max_archetypes + 63) / 64;
    uint32_t result_count = 0;
    
    // For each word
    for (uint32_t word_idx = 0; word_idx < num_words && result_count < max_results; word_idx++) {
        // Start with all bits set (or first index's bits)
        uint64_t result = indices[0]->bits[word_idx];
        
        // AND with remaining indices
        for (uint32_t i = 1; i < num_indices; i++) {
            if (word_idx < indices[i]->capacity) {
                result &= indices[i]->bits[word_idx];
            } else {
                result = 0;  // Beyond this index's capacity, no match
                break;
            }
        }
        
        // Extract set bits from result
        while (result && result_count < max_results) {
            uint32_t bit_in_word = paani_ctz64(result);  // Count trailing zeros
            uint32_t archetype_idx = word_idx * 64 + bit_in_word;
            if (archetype_idx < max_archetypes) {
                out_matching_archetypes[result_count++] = archetype_idx;
            }
            result &= result - 1;  // Clear lowest set bit
        }
    }
    
    return result_count;
}

struct paani_world {
    // Entity management
    paani_entity_record_t* entities;
    uint32_t* free_entities;
    uint32_t num_entities;
    uint32_t entity_capacity;
    uint32_t free_count;
    
    // Component metadata (chunked array - dynamic growth)
    paani_chunked_array_t* component_meta;
    uint32_t num_component_types;
    
    // Archetype storage
    paani_archetype_t** archetypes;
    uint32_t num_archetypes;
    uint32_t archetype_capacity;
    paani_archetype_t* empty_archetype;  // Archetype with no components
    
    // Trait definitions (chunked array - dynamic growth)
    paani_chunked_array_t* traits;
    uint32_t num_traits;
    
    // Systems (chunked array - dynamic growth)
    paani_chunked_array_t* systems;
    uint32_t num_systems;
    
    // Deferred destruction
    paani_entity_t* deferred_destroys;
    uint32_t deferred_count;
    uint32_t deferred_capacity;
    
    // Version tracking
    uint64_t version;
    
    // Exit flag
    int should_exit;
    
    // Bitset index for fast query (one per component type, using chunked array)
    paani_chunked_array_t* component_bitset_indices;  // paani_bitset_index_t per component type
    
    // Query cache for O(1) query performance
    paani_query_cache_t* query_cache;
};

// ============ Archetype Management ============

static uint64_t paani_signature_compute(const paani_ctype_t* types, uint32_t num_types) {
    uint64_t sig = 0;
    for (uint32_t i = 0; i < num_types; i++) {
        if (types[i] < 64) {
            sig |= (1ULL << types[i]);
        }
    }
    return sig;
}

static int paani_signature_contains(uint64_t sig, paani_ctype_t type) {
    return (type < 64) && (sig & (1ULL << type));
}

// Ensure edge arrays can accommodate the given component type
static int paani_archetype_ensure_edge_capacity(paani_archetype_t* arch, paani_ctype_t ctype) {
    if (!arch) return 0;
    
    uint32_t required_capacity = ctype + 1;
    if (required_capacity <= arch->edge_capacity) return 1;
    
    // Grow edge arrays with geometric growth
    uint32_t new_capacity = arch->edge_capacity;
    if (new_capacity == 0) new_capacity = PAANI_INITIAL_COMPONENT_CAPACITY;
    while (new_capacity < required_capacity) {
        new_capacity = (new_capacity * PAANI_GROWTH_RATIO_NUM) / PAANI_GROWTH_RATIO_DEN;
        if (new_capacity < required_capacity + 16) new_capacity = required_capacity + 16;
    }
    
    // Reallocate add_edges
    paani_archetype_t** new_add = (paani_archetype_t**)realloc(arch->add_edges, 
                                                                new_capacity * sizeof(paani_archetype_t*));
    if (!new_add) return 0;
    arch->add_edges = new_add;
    
    // Reallocate remove_edges
    paani_archetype_t** new_remove = (paani_archetype_t**)realloc(arch->remove_edges, 
                                                                   new_capacity * sizeof(paani_archetype_t*));
    if (!new_remove) return 0;
    arch->remove_edges = new_remove;
    
    // Zero new memory
    for (uint32_t i = arch->edge_capacity; i < new_capacity; i++) {
        arch->add_edges[i] = NULL;
        arch->remove_edges[i] = NULL;
    }
    
    arch->edge_capacity = new_capacity;
    return 1;
}

static paani_archetype_t* paani_archetype_find_or_create(paani_world_t* world, 
                                                          const paani_ctype_t* types,
                                                          uint32_t num_types);

static paani_archetype_t* paani_archetype_create(paani_world_t* world,
                                                  const paani_ctype_t* types,
                                                  uint32_t num_types) {
    paani_archetype_t* arch = (paani_archetype_t*)calloc(1, sizeof(paani_archetype_t));
    if (!arch) return NULL;
    
    arch->signature = paani_signature_compute(types, num_types);
    arch->num_types = num_types;
    arch->capacity = PAANI_ARCHETYPE_INITIAL_ROWS;
    arch->count = 0;
    
    if (num_types > 0) {
        arch->component_types = (paani_ctype_t*)malloc(sizeof(paani_ctype_t) * num_types);
        arch->component_data = (uint8_t**)calloc(num_types, sizeof(uint8_t*));
        arch->component_sizes = (uint32_t*)malloc(sizeof(uint32_t) * num_types);
        
        // Sort types for consistent signatures
        memcpy(arch->component_types, types, sizeof(paani_ctype_t) * num_types);
        // Simple bubble sort for small arrays
        for (uint32_t i = 0; i < num_types; i++) {
            for (uint32_t j = i + 1; j < num_types; j++) {
                if (arch->component_types[i] > arch->component_types[j]) {
                    paani_ctype_t tmp = arch->component_types[i];
                    arch->component_types[i] = arch->component_types[j];
                    arch->component_types[j] = tmp;
                }
            }
        }
        
        // Allocate component columns
        for (uint32_t i = 0; i < num_types; i++) {
            paani_ctype_t ctype = arch->component_types[i];
            paani_component_meta_t* meta = (paani_component_meta_t*)paani_chunked_array_get(world->component_meta, ctype);
            arch->component_sizes[i] = meta ? meta->size : 0;
            arch->component_data[i] = (uint8_t*)calloc(arch->capacity, arch->component_sizes[i]);
        }
    }
    
    arch->entities = (paani_entity_t*)malloc(sizeof(paani_entity_t) * arch->capacity);
    
    // Allocate edge arrays (dynamically sized, will grow as needed)
    arch->edge_capacity = PAANI_INITIAL_COMPONENT_CAPACITY;
    arch->add_edges = (paani_archetype_t**)calloc(arch->edge_capacity, sizeof(paani_archetype_t*));
    arch->remove_edges = (paani_archetype_t**)calloc(arch->edge_capacity, sizeof(paani_archetype_t*));
    
    // Add to world
    uint32_t archetype_index = world->num_archetypes;
    if (world->num_archetypes >= world->archetype_capacity) {
        world->archetype_capacity *= 2;
        world->archetypes = (paani_archetype_t**)realloc(world->archetypes, 
                                                         sizeof(paani_archetype_t*) * world->archetype_capacity);
    }
    world->archetypes[world->num_archetypes++] = arch;
    
    // Update bitset indices for each component type in this archetype
    for (uint32_t i = 0; i < num_types; i++) {
        paani_ctype_t ctype = arch->component_types[i];
        paani_bitset_index_t* index = (paani_bitset_index_t*)paani_chunked_array_get(world->component_bitset_indices, ctype);
        if (!index) {
            // First time seeing this component type, initialize it
            index = (paani_bitset_index_t*)paani_chunked_array_push(world->component_bitset_indices);
            if (index) paani_bitset_init(index);
        }
        if (index) {
            paani_bitset_set(index, archetype_index);
        }
    }
    
    // Incremental update: Only add new archetype to matching cached queries
    // This is O(num_cached_queries) instead of invalidating entire cache
    paani_query_cache_add_archetype(world->query_cache, arch, archetype_index);
    
    return arch;
}

static void paani_archetype_grow(paani_archetype_t* arch, uint32_t new_capacity) {
    if (new_capacity <= arch->capacity) return;
    
    for (uint32_t i = 0; i < arch->num_types; i++) {
        arch->component_data[i] = (uint8_t*)realloc(arch->component_data[i], 
                                                     new_capacity * arch->component_sizes[i]);
        // Zero new memory
        memset(arch->component_data[i] + arch->capacity * arch->component_sizes[i], 0,
               (new_capacity - arch->capacity) * arch->component_sizes[i]);
    }
    
    arch->entities = (paani_entity_t*)realloc(arch->entities, sizeof(paani_entity_t) * new_capacity);
    arch->capacity = new_capacity;
}

static uint32_t paani_archetype_alloc_row(paani_archetype_t* arch) {
    if (arch->count >= arch->capacity) {
        paani_archetype_grow(arch, arch->capacity * 2);
    }
    return arch->count++;
}

static void* paani_archetype_get_component(paani_archetype_t* arch, uint32_t row, paani_ctype_t type) {
    for (uint32_t i = 0; i < arch->num_types; i++) {
        if (arch->component_types[i] == type) {
            return arch->component_data[i] + row * arch->component_sizes[i];
        }
    }
    return NULL;
}

static uint32_t paani_archetype_get_type_index(paani_archetype_t* arch, paani_ctype_t type) {
    for (uint32_t i = 0; i < arch->num_types; i++) {
        if (arch->component_types[i] == type) {
            return i;
        }
    }
    return (uint32_t)-1;
}

// Move entity from one archetype to another (used for add/remove component)
static uint32_t paani_archetype_move_entity(paani_world_t* world,
                                             paani_entity_t entity,
                                             paani_archetype_t* from,
                                             paani_archetype_t* to,
                                             uint32_t from_row,
                                             const void* new_component_data,
                                             paani_ctype_t new_component_type) {
    uint32_t to_row = paani_archetype_alloc_row(to);
    to->entities[to_row] = entity;
    
    // Copy existing components
    for (uint32_t i = 0; i < to->num_types; i++) {
        paani_ctype_t ctype = to->component_types[i];
        void* dst = to->component_data[i] + to_row * to->component_sizes[i];
        
        if (ctype == new_component_type && new_component_data) {
            // New component
            memcpy(dst, new_component_data, to->component_sizes[i]);
            // Call constructor if any
            paani_component_meta_t* meta = (paani_component_meta_t*)paani_chunked_array_get(world->component_meta, ctype);
            if (meta && meta->ctor) {
                meta->ctor(dst, new_component_data);
            }
        } else if (from && paani_signature_contains(from->signature, ctype)) {
            // Copy from old archetype
            void* src = paani_archetype_get_component(from, from_row, ctype);
            memcpy(dst, src, to->component_sizes[i]);
        }
    }
    
    // Remove from old archetype (swap with last)
    if (from && from->count > 0) {
        uint32_t last_row = from->count - 1;
        if (from_row != last_row) {
            // Move last row to removed position
            paani_entity_t last_entity = from->entities[last_row];
            from->entities[from_row] = last_entity;
            
            // Update entity record for swapped entity
            world->entities[PAANI_ENTITY_INDEX(last_entity)].row_index = from_row;
            
            // Move component data
            for (uint32_t i = 0; i < from->num_types; i++) {
                void* src = from->component_data[i] + last_row * from->component_sizes[i];
                void* dst = from->component_data[i] + from_row * from->component_sizes[i];
                memcpy(dst, src, from->component_sizes[i]);
            }
        }
        from->count--;
    }
    
    return to_row;
}

static paani_archetype_t* paani_archetype_find_or_create(paani_world_t* world,
                                                          const paani_ctype_t* types,
                                                          uint32_t num_types) {
    uint64_t sig = paani_signature_compute(types, num_types);
    
    // Search existing archetypes
    for (uint32_t i = 0; i < world->num_archetypes; i++) {
        if (world->archetypes[i]->signature == sig) {
            // Verify types match exactly (handle hash collision)
            if (world->archetypes[i]->num_types == num_types) {
                int match = 1;
                for (uint32_t j = 0; j < num_types; j++) {
                    if (world->archetypes[i]->component_types[j] != types[j]) {
                        match = 0;
                        break;
                    }
                }
                if (match) return world->archetypes[i];
            }
        }
    }
    
    // Create new archetype
    return paani_archetype_create(world, types, num_types);
}

// ============ World ============

paani_world_t* paani_world_create(void) {
    paani_world_t* world = (paani_world_t*)calloc(1, sizeof(paani_world_t));
    if (!world) return NULL;
    
    world->entity_capacity = PAANI_INITIAL_ENTITY_CAPACITY;
    world->entities = (paani_entity_record_t*)calloc(world->entity_capacity, sizeof(paani_entity_record_t));
    world->free_entities = (uint32_t*)malloc(sizeof(uint32_t) * world->entity_capacity);
    
    world->archetype_capacity = PAANI_INITIAL_ARCHETYPE_CAPACITY;
    world->archetypes = (paani_archetype_t**)malloc(sizeof(paani_archetype_t*) * world->archetype_capacity);
    
    // Initialize chunked arrays for dynamic growth
    world->component_meta = paani_chunked_array_create(sizeof(paani_component_meta_t), PAANI_INITIAL_COMPONENT_CAPACITY);
    world->traits = paani_chunked_array_create(sizeof(paani_trait_def_t), PAANI_INITIAL_TRAIT_CAPACITY);
    world->systems = paani_chunked_array_create(sizeof(paani_system_record_t), PAANI_INITIAL_SYSTEM_CAPACITY);
    world->component_bitset_indices = paani_chunked_array_create(sizeof(paani_bitset_index_t), PAANI_INITIAL_COMPONENT_CAPACITY);
    world->query_cache = paani_query_cache_create();
    
    if (!world->component_meta || !world->traits || !world->systems || !world->component_bitset_indices || !world->query_cache) {
        paani_chunked_array_destroy(world->component_meta);
        paani_chunked_array_destroy(world->traits);
        paani_chunked_array_destroy(world->systems);
        paani_chunked_array_destroy(world->component_bitset_indices);
        paani_query_cache_destroy(world->query_cache);
        free(world->archetypes);
        free(world->entities);
        free(world->free_entities);
        free(world);
        return NULL;
    }
    
    // Create empty archetype (no components)
    world->empty_archetype = paani_archetype_create(world, NULL, 0);
    
    world->deferred_capacity = 256;
    world->deferred_destroys = (paani_entity_t*)malloc(sizeof(paani_entity_t) * world->deferred_capacity);
    
    return world;
}

void paani_world_destroy(paani_world_t* world) {
    if (!world) return;
    
    // Call destructors for all entities before destroying archetypes
    // Iterate through all entities and call component dtors
    for (uint32_t i = 0; i < world->num_entities; i++) {
        paani_entity_record_t* record = &world->entities[i];
        if (record->alive && record->archetype) {
            paani_archetype_t* arch = record->archetype;
            uint32_t row = record->row_index;
            
            // Call dtor for each component type in this archetype
            for (uint32_t j = 0; j < arch->num_types; j++) {
                paani_ctype_t ctype = arch->component_types[j];
                paani_component_meta_t* meta = (paani_component_meta_t*)paani_chunked_array_get(
                    world->component_meta, ctype);
                
                if (meta && meta->dtor) {
                    void* comp_data = arch->component_data[j] + row * arch->component_sizes[j];
                    meta->dtor(comp_data);
                }
            }
        }
    }
    
    // Destroy all archetypes
    for (uint32_t i = 0; i < world->num_archetypes; i++) {
        paani_archetype_t* arch = world->archetypes[i];
        if (arch) {
            for (uint32_t j = 0; j < arch->num_types; j++) {
                free(arch->component_data[j]);
            }
            free(arch->component_data);
            free(arch->component_sizes);
            free(arch->component_types);
            free(arch->entities);
            free(arch->add_edges);
            free(arch->remove_edges);
            free(arch);
        }
    }
    
    free(world->archetypes);
    free(world->entities);
    free(world->free_entities);
    free(world->deferred_destroys);
    
    // Clean up trait component arrays (iterate through chunked array)
    for (uint32_t i = 0; i < world->num_traits; i++) {
        paani_trait_def_t* trait = (paani_trait_def_t*)paani_chunked_array_get(world->traits, i);
        if (trait) free(trait->components);
    }
    
    // Clean up system dependencies (iterate through chunked array)
    for (uint32_t i = 0; i < world->num_systems; i++) {
        paani_system_record_t* sys = (paani_system_record_t*)paani_chunked_array_get(world->systems, i);
        if (sys) free(sys->dependencies);
    }
    
    // Clean up bitset indices (iterate through chunked array)
    for (uint32_t i = 0; i < world->num_component_types; i++) {
        paani_bitset_index_t* index = (paani_bitset_index_t*)paani_chunked_array_get(world->component_bitset_indices, i);
        if (index) paani_bitset_free(index);
    }
    
    // Destroy chunked arrays
    paani_chunked_array_destroy(world->component_meta);
    paani_chunked_array_destroy(world->traits);
    paani_chunked_array_destroy(world->systems);
    paani_chunked_array_destroy(world->component_bitset_indices);
    
    // Destroy query cache
    paani_query_cache_destroy(world->query_cache);
    
    free(world);
}

void paani_world_clear(paani_world_t* world) {
    if (!world) return;
    
    // Mark all entities as not alive and clear trait sets
    for (uint32_t i = 0; i < world->num_entities; i++) {
        world->entities[i].alive = 0;
        world->entities[i].trait_set = 0;
    }
    
    // Clear all archetypes
    for (uint32_t i = 0; i < world->num_archetypes; i++) {
        world->archetypes[i]->count = 0;
    }
    
    world->num_entities = 0;
    world->free_count = 0;
    world->deferred_count = 0;
    world->version++;
}

uint32_t paani_world_entity_count(const paani_world_t* world) {
    if (!world) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < world->num_entities; i++) {
        if (world->entities[i].alive) count++;
    }
    return count;
}

// ============ Entity ============

paani_entity_t paani_entity_create(paani_world_t* world) {
    if (!world) return PAANI_ENTITY_NULL;
    
    uint32_t index;
    uint32_t generation;
    
    if (world->free_count > 0) {
        // Reuse free entity slot
        index = world->free_entities[--world->free_count];
        generation = world->entities[index].generation;
    } else {
        // Allocate new entity slot
        if (world->num_entities >= world->entity_capacity) {
            world->entity_capacity *= 2;
            world->entities = (paani_entity_record_t*)realloc(world->entities, 
                                                             sizeof(paani_entity_record_t) * world->entity_capacity);
            memset(&world->entities[world->num_entities], 0, 
                   sizeof(paani_entity_record_t) * (world->entity_capacity - world->num_entities));
            world->free_entities = (uint32_t*)realloc(world->free_entities, 
                                                      sizeof(uint32_t) * world->entity_capacity);
        }
        index = world->num_entities++;
        generation = 0;
    }
    
    world->entities[index].generation = generation;
    world->entities[index].alive = 1;
    world->entities[index].archetype = world->empty_archetype;
    world->entities[index].row_index = 0;
    world->entities[index].trait_set = 0;  // Clear trait membership
    
    world->version++;
    
    return PAANI_ENTITY_MAKE(index, generation);
}

void paani_entity_destroy(paani_world_t* world, paani_entity_t entity) {
    if (!world) return;

    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return;

    paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) return;

    // Call destructors for all components (in reverse order)
    paani_archetype_t* arch = record->archetype;
    if (arch) {
        for (int32_t i = (int32_t)arch->num_types - 1; i >= 0; i--) {
            paani_ctype_t ctype = arch->component_types[i];
            paani_component_meta_t* meta = (paani_component_meta_t*)paani_chunked_array_get(world->component_meta, ctype);
            if (meta && meta->dtor) {
                void* comp_data = arch->component_data[i] + record->row_index * arch->component_sizes[i];
                meta->dtor(comp_data);
            }
        }
    }

    // Remove from archetype
    uint32_t row = record->row_index;    
    if (arch && arch->count > 0) {
        uint32_t last_row = arch->count - 1;
        if (row != last_row) {
            // Swap with last
            paani_entity_t last_entity = arch->entities[last_row];
            arch->entities[row] = last_entity;
            world->entities[PAANI_ENTITY_INDEX(last_entity)].row_index = row;
            
            for (uint32_t i = 0; i < arch->num_types; i++) {
                void* src = arch->component_data[i] + last_row * arch->component_sizes[i];
                void* dst = arch->component_data[i] + row * arch->component_sizes[i];
                memcpy(dst, src, arch->component_sizes[i]);
            }
        }
        arch->count--;
    }
    
    record->alive = 0;
    record->generation++;
    record->archetype = NULL;
    record->trait_set = 0;
    
    if (world->free_count < world->entity_capacity) {
        world->free_entities[world->free_count++] = index;
    }
    
    world->version++;
}

int paani_entity_alive(const paani_world_t* world, paani_entity_t entity) {
    if (!world) return 0;
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return 0;
    
    const paani_entity_record_t* record = &world->entities[index];
    return record->alive && record->generation == PAANI_ENTITY_GEN(entity);
}

paani_entity_t paani_entity_clone(paani_world_t* world, paani_entity_t source) {
    if (!world || !paani_entity_alive(world, source)) return PAANI_ENTITY_NULL;
    
    paani_entity_t clone = paani_entity_create(world);
    if (clone == PAANI_ENTITY_NULL) return PAANI_ENTITY_NULL;
    
    paani_entity_record_t* src_record = &world->entities[PAANI_ENTITY_INDEX(source)];
    paani_archetype_t* src_arch = src_record->archetype;
    uint32_t src_row = src_record->row_index;
    
    // Copy all components
    for (uint32_t i = 0; i < src_arch->num_types; i++) {
        paani_ctype_t ctype = src_arch->component_types[i];
        void* src_data = paani_archetype_get_component(src_arch, src_row, ctype);
        paani_component_add(world, clone, ctype, src_data);
    }
    
    // Copy trait membership
    paani_entity_record_t* clone_record = &world->entities[PAANI_ENTITY_INDEX(clone)];
    clone_record->trait_set = src_record->trait_set;
    
    return clone;
}

// ============ Deferred Destruction ============

void paani_entity_destroy_deferred(paani_world_t* world, paani_entity_t entity) {
    if (!world) return;
    
    if (world->deferred_count >= world->deferred_capacity) {
        world->deferred_capacity *= 2;
        world->deferred_destroys = (paani_entity_t*)realloc(world->deferred_destroys,
                                                           sizeof(paani_entity_t) * world->deferred_capacity);
    }
    world->deferred_destroys[world->deferred_count++] = entity;
}

void paani_entity_process_deferred_destroys(paani_world_t* world) {
    if (!world) return;
    
    for (uint32_t i = 0; i < world->deferred_count; i++) {
        paani_entity_destroy(world, world->deferred_destroys[i]);
    }
    world->deferred_count = 0;
}

void paani_entity_clear_deferred_destroys(paani_world_t* world) {
    if (!world) return;
    world->deferred_count = 0;
}

// ============ Component ============

paani_ctype_t paani_component_register(paani_world_t* world,
                                        uint32_t size,
                                        paani_ctor_fn ctor,
                                        paani_dtor_fn dtor) {
    if (!world || size == 0) return PAANI_INVALID_CTYPE;

    paani_ctype_t ctype = world->num_component_types++;
    paani_component_meta_t* meta = (paani_component_meta_t*)paani_chunked_array_push(world->component_meta);
    if (!meta) {
        paani_set_error("Failed to allocate component metadata");
        world->num_component_types--;
        return PAANI_INVALID_CTYPE;
    }

    meta->size = size;
    meta->ctor = ctor;
    meta->dtor = dtor;
    snprintf(meta->name, sizeof(meta->name), "Component_%u", ctype);

    return ctype;
}

void paani_component_add(paani_world_t* world, 
                         paani_entity_t entity,
                         paani_ctype_t type, 
                         const void* data) {
    if (!world) { PAANI_ASSERT(0, "World is NULL"); return; }
    if (type >= world->num_component_types) { PAANI_ASSERT(0, "Invalid component type"); return; }
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) { PAANI_ASSERT(0, "Invalid entity"); return; }
    
    paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) {
        PAANI_ASSERT(0, "Entity is not alive");
        return;
    }
    
    if (paani_signature_contains(record->archetype->signature, type)) {
        PAANI_ASSERT(0, "Component already exists on entity");
        return;
    }
    
    // Ensure edge arrays can accommodate this component type
    if (!paani_archetype_ensure_edge_capacity(record->archetype, type)) {
        paani_set_error("Failed to expand edge arrays");
        return;
    }
    if (!paani_archetype_ensure_edge_capacity(record->archetype, type)) {
        paani_set_error("Failed to expand edge arrays");
        return;
    }

    // Find or create target archetype
    paani_archetype_t* target = record->archetype->add_edges[type];
    if (!target) {
        // Build new component list (dynamically allocated)
        uint32_t num_new_types = record->archetype->num_types + 1;
        paani_ctype_t* new_types = (paani_ctype_t*)malloc(sizeof(paani_ctype_t) * num_new_types);
        if (!new_types) return;

        // Copy existing types
        for (uint32_t i = 0; i < record->archetype->num_types; i++) {
            new_types[i] = record->archetype->component_types[i];
        }
        // Add new type
        new_types[record->archetype->num_types] = type;

        target = paani_archetype_find_or_create(world, new_types, num_new_types);
        free(new_types);

        // Ensure target's edge arrays are also large enough
        if (!paani_archetype_ensure_edge_capacity(target, type)) {
            paani_set_error("Failed to expand target edge arrays");
            return;
        }

        record->archetype->add_edges[type] = target;
        target->remove_edges[type] = record->archetype;
    }
    
    // Move entity to new archetype
    uint32_t new_row = paani_archetype_move_entity(world, entity, record->archetype, target,
                                                    record->row_index, data, type);
    record->archetype = target;
    record->row_index = new_row;
    
    world->version++;
}

void paani_component_remove(paani_world_t* world, 
                            paani_entity_t entity,
                            paani_ctype_t type) {
    if (!world) return;
    if (type >= world->num_component_types) return;
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return;
    
    paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) return;
    
    if (!paani_signature_contains(record->archetype->signature, type)) return;
    
    // Call destructor for this component before removing
    paani_component_meta_t* meta = (paani_component_meta_t*)paani_chunked_array_get(world->component_meta, type);
    if (meta && meta->dtor) {
        void* comp_data = paani_archetype_get_component(record->archetype, record->row_index, type);
        if (comp_data) {
            meta->dtor(comp_data);
        }
    }
    
    // Find or create target archetype (without this component)
    // Ensure edge arrays can accommodate this component type
    if (!paani_archetype_ensure_edge_capacity(record->archetype, type)) {
        paani_set_error("Failed to expand edge arrays");
        return;
    }

    paani_archetype_t* target = record->archetype->remove_edges[type];
    if (!target) {
        // Dynamically allocate new types array
        uint32_t num_new_types = record->archetype->num_types - 1;
        paani_ctype_t* new_types = (paani_ctype_t*)malloc(sizeof(paani_ctype_t) * num_new_types);
        if (!new_types) return;

        uint32_t idx = 0;
        for (uint32_t i = 0; i < record->archetype->num_types; i++) {
            if (record->archetype->component_types[i] != type) {
                new_types[idx++] = record->archetype->component_types[i];
            }
        }

        target = paani_archetype_find_or_create(world, new_types, num_new_types);
        free(new_types);

        // Ensure target's edge arrays are also large enough
        if (!paani_archetype_ensure_edge_capacity(target, type)) {
            paani_set_error("Failed to expand target edge arrays");
            return;
        }

        record->archetype->remove_edges[type] = target;
        target->add_edges[type] = record->archetype;
    }
    
    // Move entity to new archetype
    uint32_t new_row = paani_archetype_move_entity(world, entity, record->archetype, target,
                                                    record->row_index, NULL, PAANI_INVALID_CTYPE);
    record->archetype = target;
    record->row_index = new_row;
    
    world->version++;
}

void* paani_component_get(paani_world_t* world, 
                          paani_entity_t entity,
                          paani_ctype_t type) {
    if (!world) return NULL;
    if (type >= world->num_component_types) return NULL;
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return NULL;
    
    paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) return NULL;
    
    return paani_archetype_get_component(record->archetype, record->row_index, type);
}

int paani_component_has(const paani_world_t* world, 
                        paani_entity_t entity,
                        paani_ctype_t type) {
    if (!world) return 0;
    if (type >= world->num_component_types) return 0;
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return 0;
    
    const paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) return 0;
    
    return paani_signature_contains(record->archetype->signature, type);
}

// ============ Trait ============

paani_trait_t paani_trait_register(paani_world_t* world,
                                   const paani_ctype_t* components,
                                   uint32_t num_components) {
    if (!world || !components || num_components == 0) return PAANI_INVALID_TRAIT;

    paani_trait_t trait = world->num_traits++;
    paani_trait_def_t* def = (paani_trait_def_t*)paani_chunked_array_push(world->traits);
    if (!def) {
        world->num_traits--;
        return PAANI_INVALID_TRAIT;
    }

    def->num_components = num_components;
    def->components = (paani_ctype_t*)malloc(sizeof(paani_ctype_t) * num_components);
    memcpy(def->components, components, sizeof(paani_ctype_t) * num_components);
    snprintf(def->name, sizeof(def->name), "Trait_%u", trait);

    return trait;
}

void paani_trait_add(paani_world_t* world,
                     paani_entity_t entity,
                     paani_trait_t trait) {
    if (!world || trait >= world->num_traits) return;
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return;
    
    paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) return;
    
    paani_trait_def_t* def = (paani_trait_def_t*)paani_chunked_array_get(world->traits, trait);
    if (!def) return;

    // Check if entity has all required components before tagging
    for (uint32_t i = 0; i < def->num_components; i++) {
        if (!paani_component_has(world, entity, def->components[i])) {
            paani_set_error("Cannot tag entity with trait: missing required component");
            return;
        }
    }

    // Set trait membership bit
    if (trait < 64) {
        record->trait_set |= (1ULL << trait);
    }
}

void paani_trait_remove(paani_world_t* world, 
                        paani_entity_t entity,
                        paani_trait_t trait) {
    if (!world || trait >= world->num_traits) return;
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return;
    
    paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) return;
    
    // Clear trait membership bit (trait is just a tag, components remain)
    if (trait < 64) {
        record->trait_set &= ~(1ULL << trait);
    }
}

int paani_entity_has_trait(const paani_world_t* world,
                           paani_entity_t entity,
                           paani_trait_t trait) {
    if (!world || trait >= world->num_traits) return 0;
    
    uint32_t index = PAANI_ENTITY_INDEX(entity);
    if (index >= world->num_entities) return 0;
    
    const paani_entity_record_t* record = &world->entities[index];
    if (!record->alive || record->generation != PAANI_ENTITY_GEN(entity)) return 0;
    
    // Check trait membership bit (trait is a tag, independent of components)
    if (trait < 64) {
        return (record->trait_set & (1ULL << trait)) != 0;
    }
    return 0;
}

// ============ Query (Standard) ============

paani_query_t* paani_query_create(paani_world_t* world,
                                   const paani_ctype_t* types,
                                   uint32_t num_types) {
    if (!world || !types || num_types == 0) return NULL;
    
    paani_query_t* query = (paani_query_t*)calloc(1, sizeof(paani_query_t));
    if (!query) return NULL;
    
    query->world = world;
    query->num_types = num_types;
    query->types = (paani_ctype_t*)malloc(sizeof(paani_ctype_t) * num_types);
    memcpy(query->types, types, sizeof(paani_ctype_t) * num_types);
    query->trait = PAANI_INVALID_TRAIT;
    
    // Compute query signature and required component signature
    uint64_t signature = paani_query_signature_compute(types, num_types);
    uint64_t required_sig = paani_signature_compute(types, num_types);
    
    // Try to find cached result (O(1) lookup)
    paani_cached_query_t* cached = paani_query_cache_find(world->query_cache, signature);
    
    if (cached && cached->num_matches > 0) {
        // Cache hit! O(1) performance - cache is kept up-to-date incrementally
        query->archetypes = (paani_archetype_t**)malloc(cached->num_matches * sizeof(paani_archetype_t*));
        memcpy(query->archetypes, cached->matches, cached->num_matches * sizeof(paani_archetype_t*));
        query->num_archetypes = cached->num_matches;
    } else {
        // Cache miss: use bitset index for O(A/64) performance
        query->archetypes = (paani_archetype_t**)malloc(sizeof(paani_archetype_t*) * world->num_archetypes);
        query->num_archetypes = 0;
        
        // Collect bitset indices for all query types
        paani_bitset_index_t** indices = (paani_bitset_index_t**)malloc(sizeof(paani_bitset_index_t*) * num_types);
        uint32_t num_valid_indices = 0;
        
        for (uint32_t i = 0; i < num_types; i++) {
            paani_ctype_t ctype = types[i];
            if (ctype < world->num_component_types) {
                paani_bitset_index_t* index = (paani_bitset_index_t*)paani_chunked_array_get(world->component_bitset_indices, ctype);
                if (index && index->bits) {
                    indices[num_valid_indices++] = index;
                }
            }
        }
        
        if (num_valid_indices == num_types && num_types > 0) {
            // Use bitset AND query for fast matching (O(A/64))
            uint32_t* matching_indices = (uint32_t*)malloc(sizeof(uint32_t) * world->num_archetypes);
            uint32_t num_matches = paani_bitset_and_query(indices, num_valid_indices, world->num_archetypes,
                                                           matching_indices, world->num_archetypes);
            
            // Convert indices to archetype pointers
            for (uint32_t i = 0; i < num_matches; i++) {
                uint32_t arch_idx = matching_indices[i];
                if (arch_idx < world->num_archetypes) {
                    query->archetypes[query->num_archetypes++] = world->archetypes[arch_idx];
                }
            }
            
            free(matching_indices);
        } else {
            // Fallback: linear scan if bitset not available
            for (uint32_t i = 0; i < world->num_archetypes; i++) {
                paani_archetype_t* arch = world->archetypes[i];
                if ((arch->signature & required_sig) == required_sig) {
                    query->archetypes[query->num_archetypes++] = arch;
                }
            }
        }
        
        free(indices);
        
        // Cache the result for future O(1) lookups
        // The cache will be incrementally updated when new archetypes are created
        paani_query_cache_add(world->query_cache, signature, required_sig, num_types,
                              query->archetypes, query->num_archetypes);
    }
    
    // Pre-compute component indices for O(1) access during iteration
    query->component_indices = (uint32_t**)calloc(query->num_archetypes, sizeof(uint32_t*));
    for (uint32_t i = 0; i < query->num_archetypes; i++) {
        paani_archetype_t* arch = query->archetypes[i];
        query->component_indices[i] = (uint32_t*)malloc(sizeof(uint32_t) * query->num_types);
        
        for (uint32_t j = 0; j < query->num_types; j++) {
            // Find index of query->types[j] in arch->component_types
            query->component_indices[i][j] = (uint32_t)-1;
            for (uint32_t k = 0; k < arch->num_types; k++) {
                if (arch->component_types[k] == query->types[j]) {
                    query->component_indices[i][j] = k;
                    break;
                }
            }
        }
    }
    
    query->version = world->version;
    return query;
}

paani_query_t* paani_query_trait(paani_world_t* world,
                                  paani_trait_t trait) {
    if (!world || trait >= world->num_traits) return NULL;

    paani_trait_def_t* def = (paani_trait_def_t*)paani_chunked_array_get(world->traits, trait);
    if (!def) return NULL;

    paani_query_t* query = paani_query_create(world, def->components, def->num_components);
    if (query) {
        query->trait = trait;
    }
    return query;
}

void paani_query_destroy(paani_query_t* query) {
    if (!query) return;
    
    // Free pre-computed component indices
    if (query->component_indices) {
        for (uint32_t i = 0; i < query->num_archetypes; i++) {
            free(query->component_indices[i]);
        }
        free(query->component_indices);
    }
    
    // Free batch iteration cache
    if (query->batch_cache) {
        free(query->batch_cache->cumulative_counts);
        free(query->batch_cache);
    }
    
    free(query->types);
    free(query->archetypes);
    free(query);
}

uint32_t paani_query_count(const paani_query_t* query) {
    if (!query) return 0;
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->num_archetypes; i++) {
        count += query->archetypes[i]->count;
    }
    return count;
}

// Branch prediction hints
#ifdef __GNUC__
    #define PAANI_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define PAANI_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define PAANI_LIKELY(x)   (x)
    #define PAANI_UNLIKELY(x) (x)
#endif

int paani_query_next(paani_query_t* query, paani_query_result_t* out_result) {
    if (PAANI_UNLIKELY(!query || !out_result)) return 0;
    if (PAANI_UNLIKELY(query->version != query->world->version)) return 0;  // Stale query
    
    // Fast path: single archetype (most common case)
    if (PAANI_LIKELY(query->num_archetypes == 1)) {
        paani_archetype_t* arch = query->archetypes[0];
        uint32_t row = query->current_row;
        
        if (PAANI_LIKELY(row < arch->count)) {
            query->current_row = row + 1;
            out_result->entity = arch->entities[row];
            
            // Unroll loop for common cases (1-2 components)
            if (query->num_types == 1) {
                uint32_t type_idx = query->component_indices[0][0];
                out_result->components[0] = arch->component_data[type_idx] + row * arch->component_sizes[type_idx];
            } else if (query->num_types == 2) {
                uint32_t type_idx0 = query->component_indices[0][0];
                uint32_t type_idx1 = query->component_indices[0][1];
                out_result->components[0] = arch->component_data[type_idx0] + row * arch->component_sizes[type_idx0];
                out_result->components[1] = arch->component_data[type_idx1] + row * arch->component_sizes[type_idx1];
            } else {
                // Generic case
                for (uint32_t i = 0; i < query->num_types; i++) {
                    uint32_t type_idx = query->component_indices[0][i];
                    out_result->components[i] = arch->component_data[type_idx] + row * arch->component_sizes[type_idx];
                }
            }
            return 1;
        }
        return 0;
    }
    
    // Slow path: multiple archetypes
    while (query->current_archetype < query->num_archetypes) {
        uint32_t arch_idx = query->current_archetype;
        paani_archetype_t* arch = query->archetypes[arch_idx];
        
        if (query->current_row < arch->count) {
            uint32_t row = query->current_row++;
            out_result->entity = arch->entities[row];
            
            // Fill component pointers using pre-computed indices (O(1) per component)
            for (uint32_t i = 0; i < query->num_types; i++) {
                uint32_t type_idx = query->component_indices[arch_idx][i];
                if (type_idx != (uint32_t)-1) {
                    out_result->components[i] = arch->component_data[type_idx] + row * arch->component_sizes[type_idx];
                } else {
                    out_result->components[i] = NULL;
                }
            }
            return 1;
        }
        
        // Move to next archetype
        query->current_archetype++;
        query->current_row = 0;
    }
    
    return 0;  // No more entities
}

void paani_query_reset(paani_query_t* query) {
    if (!query) return;
    query->current_archetype = 0;
    query->current_row = 0;
}

// ============ Batch Iteration API (Faster than iterator) ============

uint32_t paani_query_entity_count(paani_query_t* query) {
    if (!query) return 0;
    if (query->version != query->world->version) return 0;
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < query->num_archetypes; i++) {
        count += query->archetypes[i]->count;
    }
    return count;
}

// Build cache for batch iteration O(1) lookup
static void paani_query_build_cache(paani_query_t* query) {
    if (!query) return;
    
    // Check if cache is still valid
    if (query->batch_cache && query->batch_cache->version == query->world->version) {
        return;
    }
    
    // Allocate or resize cache
    if (!query->batch_cache) {
        query->batch_cache = (paani_batch_cache_t*)malloc(sizeof(paani_batch_cache_t));
        query->batch_cache->cumulative_counts = NULL;
    }
    
    if (query->batch_cache->num_archetypes < query->num_archetypes) {
        free(query->batch_cache->cumulative_counts);
        query->batch_cache->cumulative_counts = (uint32_t*)malloc(sizeof(uint32_t) * query->num_archetypes);
        query->batch_cache->num_archetypes = query->num_archetypes;
    }
    
    // Build cumulative counts
    uint32_t cumulative = 0;
    for (uint32_t i = 0; i < query->num_archetypes; i++) {
        query->batch_cache->cumulative_counts[i] = cumulative;
        cumulative += query->archetypes[i]->count;
    }
    
    query->batch_cache->version = query->world->version;
}

int paani_query_get(paani_query_t* query,
                    uint32_t index,
                    paani_entity_t* out_entity,
                    void** out_components,
                    uint32_t max_components) {
    if (PAANI_UNLIKELY(!query || !out_entity || !out_components)) return 0;
    if (PAANI_UNLIKELY(query->version != query->world->version)) return 0;
    if (PAANI_UNLIKELY(max_components < query->num_types)) return 0;
    if (PAANI_UNLIKELY(query->num_archetypes == 0)) return 0;
    if (PAANI_UNLIKELY(!query->component_indices)) return 0;
    
    // Fast path: single archetype (most common case)
    if (PAANI_LIKELY(query->num_archetypes == 1)) {
        paani_archetype_t* arch = query->archetypes[0];
        if (PAANI_UNLIKELY(index >= arch->count)) return 0;
        
        *out_entity = arch->entities[index];
        
        // Fill component pointers
        for (uint32_t i = 0; i < query->num_types; i++) {
            uint32_t type_idx = query->component_indices[0][i];
            if (type_idx != (uint32_t)-1 && type_idx < arch->num_types) {
                out_components[i] = (char*)arch->component_data[type_idx] + index * arch->component_sizes[type_idx];
            } else {
                out_components[i] = NULL;
            }
        }
        return 1;
    }
    
    // Multiple archetypes: linear search (cache building overhead not worth it for small N)
    uint32_t remaining = index;
    for (uint32_t arch_idx = 0; arch_idx < query->num_archetypes; arch_idx++) {
        paani_archetype_t* arch = query->archetypes[arch_idx];
        
        if (remaining < arch->count) {
            // Found it
            uint32_t row = remaining;
            *out_entity = arch->entities[row];
            
            for (uint32_t i = 0; i < query->num_types; i++) {
                uint32_t type_idx = query->component_indices[arch_idx][i];
                if (type_idx != (uint32_t)-1 && type_idx < arch->num_types) {
                    out_components[i] = (char*)arch->component_data[type_idx] + row * arch->component_sizes[type_idx];
                } else {
                    out_components[i] = NULL;
                }
            }
            return 1;
        }
        
        remaining -= arch->count;
    }
    
    return 0;  // Index out of bounds
}

// ============ SoA Query (High Performance) ============

int paani_query_soa_get(paani_world_t* world,
                        const paani_ctype_t* types,
                        uint32_t num_types,
                        paani_query_soa_t* out_soa) {
    if (!world || !types || num_types == 0 || !out_soa) return 0;
    
    // For now, we return the first matching archetype's SoA data
    // In a full implementation, we'd need to handle multiple archetypes
    uint64_t required_sig = paani_signature_compute(types, num_types);
    
    for (uint32_t i = 0; i < world->num_archetypes; i++) {
        paani_archetype_t* arch = world->archetypes[i];
        if ((arch->signature & required_sig) == required_sig && arch->count > 0) {
            out_soa->component_columns = (void**)malloc(sizeof(void*) * num_types);
            out_soa->num_types = num_types;
            out_soa->count = arch->count;
            
            for (uint32_t t = 0; t < num_types; t++) {
                uint32_t type_idx = paani_archetype_get_type_index(arch, types[t]);
                if (type_idx != (uint32_t)-1) {
                    out_soa->component_columns[t] = arch->component_data[type_idx];
                } else {
                    out_soa->component_columns[t] = NULL;
                }
            }
            return 1;
        }
    }
    
    return 0;
}

void paani_query_soa_release(paani_query_soa_t* soa) {
    if (!soa) return;
    free(soa->component_columns);
    soa->component_columns = NULL;
    soa->count = 0;
}

// ============ System ============

paani_system_t paani_system_register(paani_world_t* world,
                                     const char* name,
                                     paani_system_fn fn,
                                     void* userdata) {
    if (!world || !name || !fn) return PAANI_INVALID_SYSTEM;

    paani_system_t sys = world->num_systems++;
    paani_system_record_t* record = (paani_system_record_t*)paani_chunked_array_push(world->systems);
    if (!record) {
        world->num_systems--;
        return PAANI_INVALID_SYSTEM;
    }

    strncpy(record->name, name, sizeof(record->name) - 1);
    record->name[sizeof(record->name) - 1] = '\0';
    record->fn = fn;
    record->userdata = userdata;
    record->locked = 0;
    record->num_dependencies = 0;
    record->dependency_capacity = 4;
    record->dependencies = (uint32_t*)malloc(sizeof(uint32_t) * record->dependency_capacity);

    return sys;
}

void paani_system_lock(paani_world_t* world, paani_system_t system) {
    if (!world || system >= world->num_systems) return;
    paani_system_record_t* record = (paani_system_record_t*)paani_chunked_array_get(world->systems, system);
    if (record) record->locked = 1;
}

void paani_system_unlock(paani_world_t* world, paani_system_t system) {
    if (!world || system >= world->num_systems) return;
    paani_system_record_t* record = (paani_system_record_t*)paani_chunked_array_get(world->systems, system);
    if (record) record->locked = 0;
}

int paani_system_is_locked(paani_world_t* world, paani_system_t system) {
    if (!world || system >= world->num_systems) return 0;
    paani_system_record_t* record = (paani_system_record_t*)paani_chunked_array_get(world->systems, system);
    return record ? record->locked : 0;
}

void paani_system_depend(paani_world_t* world,
                         paani_system_t before,
                         paani_system_t after) {
    if (!world || before >= world->num_systems || after >= world->num_systems) return;

    paani_system_record_t* sys = (paani_system_record_t*)paani_chunked_array_get(world->systems, after);
    if (!sys) return;

    if (sys->num_dependencies >= sys->dependency_capacity) {
        sys->dependency_capacity *= 2;
        sys->dependencies = (uint32_t*)realloc(sys->dependencies,
                                               sizeof(uint32_t) * sys->dependency_capacity);
    }
    sys->dependencies[sys->num_dependencies++] = before;
}

paani_system_t paani_system_find(paani_world_t* world, const char* name) {
    if (!world || !name) return PAANI_INVALID_SYSTEM;

    for (uint32_t i = 0; i < world->num_systems; i++) {
        paani_system_record_t* sys = (paani_system_record_t*)paani_chunked_array_get(world->systems, i);
        if (sys && sys->name && strcmp(sys->name, name) == 0) {
            return i;
        }
    }
    return PAANI_INVALID_SYSTEM;
}

// Simple topological sort for system execution
static void paani_system_run_sorted(paani_world_t* world, float dt) {
    uint32_t n = world->num_systems;
    if (n == 0) return;

    // Build in-degree array
    uint32_t* in_degree = (uint32_t*)calloc(n, sizeof(uint32_t));
    for (uint32_t i = 0; i < n; i++) {
        paani_system_record_t* sys = (paani_system_record_t*)paani_chunked_array_get(world->systems, i);
        if (sys) {
            for (uint32_t j = 0; j < sys->num_dependencies; j++) {
                in_degree[i]++;
            }
        }
    }

    // Find all systems with no dependencies (and not locked)
    uint32_t* queue = (uint32_t*)malloc(sizeof(uint32_t) * n);
    uint32_t queue_head = 0, queue_tail = 0;

    for (uint32_t i = 0; i < n; i++) {
        paani_system_record_t* sys = (paani_system_record_t*)paani_chunked_array_get(world->systems, i);
        if (sys && in_degree[i] == 0 && !sys->locked) {
            queue[queue_tail++] = i;
        }
    }

    // Process systems in topological order
    uint32_t processed = 0;
    while (queue_head < queue_tail) {
        uint32_t sys_idx = queue[queue_head++];
        paani_system_record_t* sys = (paani_system_record_t*)paani_chunked_array_get(world->systems, sys_idx);
        if (!sys) continue;

        // Run system
        sys->fn(world, dt, sys->userdata);
        processed++;

        // Reduce in-degree of dependent systems
        for (uint32_t i = 0; i < n; i++) {
            paani_system_record_t* dependent = (paani_system_record_t*)paani_chunked_array_get(world->systems, i);
            if (!dependent) continue;
            for (uint32_t j = 0; j < dependent->num_dependencies; j++) {
                if (dependent->dependencies[j] == sys_idx) {
                    in_degree[i]--;
                    if (in_degree[i] == 0 && !dependent->locked) {
                        queue[queue_tail++] = i;
                    }
                }
            }
        }
    }

    free(in_degree);
    free(queue);
}

void paani_system_run_all(paani_world_t* world, float dt) {
    if (!world) return;
    paani_system_run_sorted(world, dt);
}

paani_system_list_t* paani_system_get_all(paani_world_t* world, uint32_t* out_count) {
    if (!world) return NULL;
    if (out_count) *out_count = world->num_systems;
    
    paani_system_list_t* list = (paani_system_list_t*)malloc(sizeof(paani_system_list_t));
    list->systems = (paani_system_t*)malloc(sizeof(paani_system_t) * world->num_systems);
    list->count = world->num_systems;
    
    for (uint32_t i = 0; i < world->num_systems; i++) {
        list->systems[i] = i;
    }
    
    return list;
}

void paani_free_system_list(paani_system_list_t* list) {
    if (!list) return;
    free(list->systems);
    free(list);
}

// ============ Exit Flag ============

void paani_exit(paani_world_t* world) {
    if (world) world->should_exit = 1;
}

int paani_should_exit(paani_world_t* world) {
    return world ? world->should_exit : 0;
}

void paani_clear_exit(paani_world_t* world) {
    if (world) world->should_exit = 0;
}
)SOA_SOURCE";

#endif // PAANI_EMBEDDED_RT_HPP
