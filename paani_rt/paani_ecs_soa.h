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

// Fast SoA view from existing query (no malloc/lookup overhead)
int paani_query_trait_soa(paani_query_t* query, paani_query_soa_t* out_soa);

// Release SoA view
void paani_query_soa_release(paani_query_soa_t* soa);

// ============ Direct Batch Access (Maximum Performance) ============
// Get archetype count for batch-optimized systems
uint32_t paani_query_batch_count(paani_query_t* query);

// Get component column pointer directly (zero overhead)
void* paani_query_batch_column(paani_query_t* query, uint32_t component_index);

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
