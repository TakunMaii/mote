/*
================================================================================
    miecs.h - Single-header, pure C, minimal ECS (Entity Component System)
    stb-style, public-domain-ish implementation.

    Author:   Maii
    Version:  0.1
    Language: C99 (or later)
    Name:     miecs

--------------------------------------------------------------------------------
USAGE
--------------------------------------------------------------------------------

    // In exactly *one* C or C++ translation unit do:
        #define MIECS_IMPLEMENTATION
        #include "miecs.h"

    // In all other translation units that use miecs, simply:
        #include "miecs.h"

--------------------------------------------------------------------------------
OVERVIEW
--------------------------------------------------------------------------------

    This is a minimal ECS implementation with the following properties:

    - Entities are integer IDs.
    - Components are user-defined structs.
    - Each component type is stored in a sparse set.
    - Simple API:
        - miecs_world_create / miecs_world_destroy
        - miecs_entity_create / miecs_entity_destroy
        - miecs_component_register
        - miecs_component_add / miecs_component_get / miecs_component_has /
          miecs_component_remove
        - miecs_view_begin / miecs_view_next (iterate entities with a set of
          components)

    - No dynamic code generation / no macros to define components.
    - No threading, no job system, no advanced features.
    - Designed to be small and understandable rather than maximally fast.

--------------------------------------------------------------------------------
LICENSE
--------------------------------------------------------------------------------

    This file is released into the public domain. You may use it for any
    purpose, with or without modification, without restriction.

    If your jurisdiction does not recognize the public domain, you may treat
    this file as licensed under the MIT license:

        Copyright (c) 2024 Maii

        Permission is hereby granted, free of charge, to any person obtaining a
        copy of this software and associated documentation files (the
        "Software"), to deal in the Software without restriction, including
        without limitation the rights to use, copy, modify, merge, publish,
        distribute, sublicense, and/or sell copies of the Software, and to
        permit persons to whom the Software is furnished to do so, subject to
        the following conditions:

        The above copyright notice and this permission notice shall be included
        in all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
        OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
        MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
        IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
        CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
        TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
        SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

================================================================================
*/

#ifndef MIECS_H_INCLUDED
#define MIECS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint32_t, uint64_t, etc. */

/*-----------------------------------------------------------------------------
    Configuration (override by defining before including)
-----------------------------------------------------------------------------*/

/* Maximum number of different component types in one world. */
#ifndef MIECS_MAX_COMPONENT_TYPES
#define MIECS_MAX_COMPONENT_TYPES 64
#endif

/* Initial capacity for entity storage (will grow as needed). */
#ifndef MIECS_INITIAL_ENTITY_CAPACITY
#define MIECS_INITIAL_ENTITY_CAPACITY 1024
#endif

/* Initial capacity for each component storage (will grow as needed). */
#ifndef MIECS_INITIAL_COMPONENT_CAPACITY
#define MIECS_INITIAL_COMPONENT_CAPACITY 1024
#endif

/* Whether to include simple runtime asserts. Define to 0 to disable. */
#ifndef MIECS_ENABLE_ASSERT
#define MIECS_ENABLE_ASSERT 1
#endif

#if MIECS_ENABLE_ASSERT
#include <assert.h>
#define MIECS_ASSERT(x) assert(x)
#else
#define MIECS_ASSERT(x) ((void)0)
#endif

/*-----------------------------------------------------------------------------
    Types
-----------------------------------------------------------------------------*/

/* Entity handle: 32-bit id enough in many cases */
typedef uint32_t miecs_entity;

/* Component type id (index into world arrays). */
typedef uint16_t miecs_component_type;

/* Invalid constants. */
#define MIECS_INVALID_ENTITY       ((miecs_entity)0)
#define MIECS_INVALID_COMPONENT    ((miecs_component_type)0xFFFF)

/* Forward declaration of world. */
typedef struct miecs_world miecs_world;

/* Opaque iterator for views. */
typedef struct miecs_view_iter {
    miecs_world         *world;
    size_t               comp_count;
    miecs_component_type comp_types[8]; /* simple fixed upper bound per view */
    size_t               index;         /* current index in the "primary" array */
} miecs_view_iter;

/*-----------------------------------------------------------------------------
    Public API
-----------------------------------------------------------------------------*/

/* World management */
miecs_world *miecs_world_create(void);
void         miecs_world_destroy(miecs_world *world);

/* Entities */
miecs_entity miecs_entity_create(miecs_world *world);
void         miecs_entity_destroy(miecs_world *world, miecs_entity e);
int          miecs_entity_alive(miecs_world *world, miecs_entity e);

/* Components registration

   Each unique C struct type you want to store should correspond to exactly
   one miecs_component_type. Register it once with its size:

      miecs_component_type Position_type =
          miecs_component_register(world, "Position", sizeof(Position));

   'name' is optional and only for debugging; may be NULL.
*/
miecs_component_type miecs_component_register(miecs_world *world,
                                              const char  *name,
                                              size_t       size);

/* Component operations

   Add / get / has / remove
*/
void *miecs_component_add(miecs_world *world,
                          miecs_entity e,
                          miecs_component_type ct);

void *miecs_component_get(miecs_world *world,
                          miecs_entity e,
                          miecs_component_type ct);

int   miecs_component_has(miecs_world *world,
                          miecs_entity e,
                          miecs_component_type ct);

void  miecs_component_remove(miecs_world *world,
                             miecs_entity e,
                             miecs_component_type ct);

/* Views / iteration

   A view lets you iterate over all entities that have a given set of
   components.

   Usage:

       miecs_view_iter it;
       miecs_view_begin(&it, world, 2, Position_type, Velocity_type);
       while (miecs_view_next(&it, &e)) {
           Position *p = miecs_component_get(world, e, Position_type);
           Velocity *v = miecs_component_get(world, e, Velocity_type);
           ...
       }

   Note: This is a simple implementation; view creation is O(#entities).
*/
void miecs_view_begin(miecs_view_iter *it,
                      miecs_world     *world,
                      size_t           comp_count,
                      ... /* miecs_component_type ct0, ct1, ... */);

int miecs_view_next(miecs_view_iter *it, miecs_entity *out_entity);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MIECS_H_INCLUDED */

/*==============================================================================
    IMPLEMENTATION
==============================================================================*/

#ifdef MIECS_IMPLEMENTATION
#ifndef MIECS_IMPLEMENTATION_ONCE
#define MIECS_IMPLEMENTATION_ONCE

#include <stdlib.h>  /* malloc, free, realloc */
#include <string.h>  /* memset, memcpy */
#include <stdarg.h>  /* va_list */

/*-----------------------------------------------------------------------------
    Internal data structures
-----------------------------------------------------------------------------*/

typedef struct miecs_sparse_set {
    miecs_entity *dense;  /* dense array of entities */
    uint32_t     *sparse; /* sparse index: entity -> index in dense */
    uint8_t      *data;   /* component data, tightly packed */
    size_t        count;  /* number of live entities with this component */
    size_t        capacity;
    size_t        elem_size;
    size_t        sparse_capacity; /* how many entries in sparse[] */
} miecs_sparse_set;

typedef struct miecs_world {
    /* Entity management */
    miecs_entity *entities;      /* index -> entity id (if alive) or 0 if free */
    uint8_t      *alive;         /* boolean: 1 if entity alive */
    size_t        entity_count;
    size_t        entity_capacity;

    /* Free list for entity IDs */
    miecs_entity *free_list;
    size_t        free_count;
    size_t        free_capacity;

    /* Components */
    miecs_sparse_set components[MIECS_MAX_COMPONENT_TYPES];
    const char      *component_names[MIECS_MAX_COMPONENT_TYPES];
    size_t           component_count;
} miecs_world;

/*-----------------------------------------------------------------------------
    Utility
-----------------------------------------------------------------------------*/

static void *miecs__realloc(void *p, size_t old_size, size_t new_size)
{
    (void)old_size;
    if (new_size == 0) {
        free(p);
        return NULL;
    }
    void *np = realloc(p, new_size);
    return np;
}

static void miecs__ensure_entity_capacity(miecs_world *world, size_t min_cap)
{
    if (world->entity_capacity >= min_cap) return;

    size_t new_cap = world->entity_capacity ? world->entity_capacity : MIECS_INITIAL_ENTITY_CAPACITY;
    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    world->entities = (miecs_entity *)miecs__realloc(
        world->entities,
        world->entity_capacity * sizeof(miecs_entity),
        new_cap * sizeof(miecs_entity)
    );
    world->alive = (uint8_t *)miecs__realloc(
        world->alive,
        world->entity_capacity * sizeof(uint8_t),
        new_cap * sizeof(uint8_t)
    );

    for (size_t i = world->entity_capacity; i < new_cap; ++i) {
        world->entities[i] = MIECS_INVALID_ENTITY;
        world->alive[i] = 0;
    }

    world->entity_capacity = new_cap;
}

/* Ensure sparse table can hold entity id e (1-based id). */
static void miecs__sparse_set_ensure_sparse(miecs_sparse_set *set, miecs_entity e)
{
    if (e == 0) return;
    if (set->sparse_capacity > e) return;

    size_t new_cap = set->sparse_capacity ? set->sparse_capacity : 1024;
    while (new_cap <= e) {
        new_cap *= 2;
    }

    uint32_t *new_sparse = (uint32_t *)miecs__realloc(
        set->sparse,
        set->sparse_capacity * sizeof(uint32_t),
        new_cap * sizeof(uint32_t)
    );
    /* Initialize newly added part with an invalid index (0xFFFFFFFF). */
    for (size_t i = set->sparse_capacity; i < new_cap; ++i) {
        new_sparse[i] = 0xFFFFFFFFu;
    }

    set->sparse = new_sparse;
    set->sparse_capacity = new_cap;
}

/* Ensure dense capacity. */
static void miecs__sparse_set_ensure_dense(miecs_sparse_set *set, size_t min_cap)
{
    if (set->capacity >= min_cap) return;

    size_t new_cap = set->capacity ? set->capacity : MIECS_INITIAL_COMPONENT_CAPACITY;
    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    set->dense = (miecs_entity *)miecs__realloc(
        set->dense,
        set->capacity * sizeof(miecs_entity),
        new_cap * sizeof(miecs_entity)
    );
    set->data = (uint8_t *)miecs__realloc(
        set->data,
        set->capacity * set->elem_size,
        new_cap * set->elem_size
    );

    set->capacity = new_cap;
}

/* Insert entity into sparse set, returns pointer to its data. */
static void *miecs__sparse_set_add(miecs_sparse_set *set, miecs_entity e)
{
    miecs__sparse_set_ensure_sparse(set, e);
    /* If already present, just return existing pointer */
    if (set->sparse[e] != 0xFFFFFFFFu) {
        size_t idx = set->sparse[e];
        return set->data + idx * set->elem_size;
    }

    miecs__sparse_set_ensure_dense(set, set->count + 1);

    size_t idx = set->count;
    set->dense[idx] = e;
    set->sparse[e] = (uint32_t)idx;
    ++set->count;

    uint8_t *ptr = set->data + idx * set->elem_size;
    memset(ptr, 0, set->elem_size);
    return ptr;
}

static void *miecs__sparse_set_get(miecs_sparse_set *set, miecs_entity e)
{
    if (e == 0 || e >= set->sparse_capacity) return NULL;
    uint32_t idx = set->sparse[e];
    if (idx == 0xFFFFFFFFu || idx >= set->count) return NULL;
    return set->data + (size_t)idx * set->elem_size;
}

static int miecs__sparse_set_has(miecs_sparse_set *set, miecs_entity e)
{
    if (e == 0 || e >= set->sparse_capacity) return 0;
    uint32_t idx = set->sparse[e];
    if (idx == 0xFFFFFFFFu || idx >= set->count) return 0;
    return 1;
}

static void miecs__sparse_set_remove(miecs_sparse_set *set, miecs_entity e)
{
    if (e == 0 || e >= set->sparse_capacity) return;
    uint32_t idx = set->sparse[e];
    if (idx == 0xFFFFFFFFu || idx >= set->count) return;

    /* Swap with last */
    size_t last_idx = set->count - 1;
    if (idx != last_idx) {
        miecs_entity last_e = set->dense[last_idx];
        /* move last entry into idx */
        set->dense[idx] = last_e;
        memcpy(set->data + idx * set->elem_size,
               set->data + last_idx * set->elem_size,
               set->elem_size);
        set->sparse[last_e] = idx;
    }

    /* mark e absent */
    set->sparse[e] = 0xFFFFFFFFu;
    --set->count;
}

/*-----------------------------------------------------------------------------
    World implementation
-----------------------------------------------------------------------------*/

miecs_world *miecs_world_create(void)
{
    miecs_world *w = (miecs_world *)malloc(sizeof(miecs_world));
    if (!w) return NULL;
    memset(w, 0, sizeof(*w));

    /* allocate initial for entities */
    miecs__ensure_entity_capacity(w, MIECS_INITIAL_ENTITY_CAPACITY);

    /* mark all sparse sets as empty */
    for (size_t i = 0; i < MIECS_MAX_COMPONENT_TYPES; ++i) {
        w->components[i].dense = NULL;
        w->components[i].sparse = NULL;
        w->components[i].data = NULL;
        w->components[i].count = 0;
        w->components[i].capacity = 0;
        w->components[i].elem_size = 0;
        w->components[i].sparse_capacity = 0;
        w->component_names[i] = NULL;
    }

    w->component_count = 0;

    /* free list initial */
    w->free_list = NULL;
    w->free_count = 0;
    w->free_capacity = 0;

    return w;
}

void miecs_world_destroy(miecs_world *world)
{
    if (!world) return;

    free(world->entities);
    free(world->alive);

    free(world->free_list);

    for (size_t i = 0; i < MIECS_MAX_COMPONENT_TYPES; ++i) {
        miecs_sparse_set *set = &world->components[i];
        free(set->dense);
        free(set->sparse);
        free(set->data);
    }

    free(world);
}

/*-----------------------------------------------------------------------------
    Entities
-----------------------------------------------------------------------------*/

miecs_entity miecs_entity_create(miecs_world *world)
{
    MIECS_ASSERT(world);

    miecs_entity e;

    if (world->free_count > 0) {
        /* Reuse from free list */
        e = world->free_list[--world->free_count];
        MIECS_ASSERT(e > 0 && e < (miecs_entity)world->entity_capacity);
        world->alive[e] = 1;
    } else {
        /* allocate new id = next index */
        size_t idx = ++world->entity_count;   /* use 1-based entity id */
        miecs__ensure_entity_capacity(world, idx + 1);
        e = (miecs_entity)idx;
        world->entities[e] = e;
        world->alive[e] = 1;
    }

    return e;
}

void miecs_entity_destroy(miecs_world *world, miecs_entity e)
{
    if (!world || e == 0 || e >= (miecs_entity)world->entity_capacity) return;
    if (!world->alive[e]) return;

    /* Remove all components of this entity */
    for (size_t i = 0; i < world->component_count; ++i) {
        miecs_sparse_set *set = &world->components[i];
        if (set->elem_size == 0) continue;
        if (miecs__sparse_set_has(set, e)) {
            miecs__sparse_set_remove(set, e);
        }
    }

    world->alive[e] = 0;

    /* add to free list */
    if (world->free_count == world->free_capacity) {
        size_t new_cap = world->free_capacity ? world->free_capacity * 2 : 128;
        world->free_list = (miecs_entity *)miecs__realloc(
            world->free_list,
            world->free_capacity * sizeof(miecs_entity),
            new_cap * sizeof(miecs_entity)
        );
        world->free_capacity = new_cap;
    }

    world->free_list[world->free_count++] = e;
}

int miecs_entity_alive(miecs_world *world, miecs_entity e)
{
    if (!world || e == 0 || e >= (miecs_entity)world->entity_capacity) return 0;
    return world->alive[e] ? 1 : 0;
}

/*-----------------------------------------------------------------------------
    Components
-----------------------------------------------------------------------------*/

miecs_component_type miecs_component_register(miecs_world *world,
                                              const char  *name,
                                              size_t       size)
{
    MIECS_ASSERT(world);
    if (world->component_count >= MIECS_MAX_COMPONENT_TYPES) {
        return MIECS_INVALID_COMPONENT;
    }
    miecs_component_type id = (miecs_component_type)world->component_count++;
    miecs_sparse_set *set = &world->components[id];

    set->elem_size = size;
    set->dense = NULL;
    set->sparse = NULL;
    set->data = NULL;
    set->count = 0;
    set->capacity = 0;
    set->sparse_capacity = 0;

    world->component_names[id] = name;

    return id;
}

void *miecs_component_add(miecs_world *world,
                          miecs_entity e,
                          miecs_component_type ct)
{
    MIECS_ASSERT(world);
    if (ct >= world->component_count) return NULL;
    if (!miecs_entity_alive(world, e)) return NULL;

    miecs_sparse_set *set = &world->components[ct];
    return miecs__sparse_set_add(set, e);
}

void *miecs_component_get(miecs_world *world,
                          miecs_entity e,
                          miecs_component_type ct)
{
    MIECS_ASSERT(world);
    if (ct >= world->component_count) return NULL;
    if (!miecs_entity_alive(world, e)) return NULL;

    miecs_sparse_set *set = &world->components[ct];
    return miecs__sparse_set_get(set, e);
}

int miecs_component_has(miecs_world *world,
                        miecs_entity e,
                        miecs_component_type ct)
{
    MIECS_ASSERT(world);
    if (ct >= world->component_count) return 0;
    if (!miecs_entity_alive(world, e)) return 0;

    miecs_sparse_set *set = &world->components[ct];
    return miecs__sparse_set_has(set, e);
}

void miecs_component_remove(miecs_world *world,
                            miecs_entity e,
                            miecs_component_type ct)
{
    MIECS_ASSERT(world);
    if (ct >= world->component_count) return;
    if (!miecs_entity_alive(world, e)) return;

    miecs_sparse_set *set = &world->components[ct];
    miecs__sparse_set_remove(set, e);
}

/*-----------------------------------------------------------------------------
    View / Iteration
-----------------------------------------------------------------------------*/

void miecs_view_begin(miecs_view_iter *it,
                      miecs_world     *world,
                      size_t           comp_count,
                      ...)
{
    MIECS_ASSERT(it && world);
    MIECS_ASSERT(comp_count <= 8);

    memset(it, 0, sizeof(*it));
    it->world = world;
    it->comp_count = comp_count;

    va_list args;
    va_start(args, comp_count);
    for (size_t i = 0; i < comp_count; ++i) {
        miecs_component_type ct = (miecs_component_type)va_arg(args, int);
        it->comp_types[i] = ct;
    }
    va_end(args);

    it->index = 1; /* entities are 1-based */
}

int miecs_view_next(miecs_view_iter *it, miecs_entity *out_entity)
{
    MIECS_ASSERT(it && out_entity);
    miecs_world *world = it->world;
    if (!world) return 0;

    size_t cap = world->entity_capacity;

    for (; it->index < cap; ++it->index) {
        miecs_entity e = (miecs_entity)it->index;
        if (!miecs_entity_alive(world, e)) continue;

        int ok = 1;
        for (size_t j = 0; j < it->comp_count; ++j) {
            miecs_component_type ct = it->comp_types[j];
            if (ct >= world->component_count) {
                ok = 0;
                break;
            }
            miecs_sparse_set *set = &world->components[ct];
            if (!miecs__sparse_set_has(set, e)) {
                ok = 0;
                break;
            }
        }

        if (ok) {
            *out_entity = e;
            ++it->index;
            return 1;
        }
    }

    return 0;
}

#endif /* MIECS_IMPLEMENTATION_ONCE */
#endif /* MIECS_IMPLEMENTATION */


/*==============================================================================
    EXAMPLE (put into your own .c file, not in header)
==============================================================================

#define MIECS_IMPLEMENTATION
#include "miecs.h"

typedef struct {
    float x, y;
} Position;

typedef struct {
    float vx, vy;
} Velocity;

int main(void)
{
    miecs_world *w = miecs_world_create();

    miecs_component_type Position_type =
        miecs_component_register(w, "Position", sizeof(Position));
    miecs_component_type Velocity_type =
        miecs_component_register(w, "Velocity", sizeof(Velocity));

    for (int i = 0; i < 10; ++i) {
        miecs_entity e = miecs_entity_create(w);
        Position *p = (Position *)miecs_component_add(w, e, Position_type);
        Velocity *v = (Velocity *)miecs_component_add(w, e, Velocity_type);

        p->x = (float)i;
        p->y = (float)i * 2.0f;
        v->vx = 1.0f;
        v->vy = 0.5f;
    }

    miecs_view_iter it;
    miecs_entity e;
    miecs_view_begin(&it, w, 2, Position_type, Velocity_type);
    while (miecs_view_next(&it, &e)) {
        Position *p = (Position *)miecs_component_get(w, e, Position_type);
        Velocity *v = (Velocity *)miecs_component_get(w, e, Velocity_type);

        p->x += v->vx;
        p->y += v->vy;
    }

    miecs_world_destroy(w);
    return 0;
}

==============================================================================*/