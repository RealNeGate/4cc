#pragma once

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#if __cplusplus
extern "C" {
    #endif

    enum {
        DEGREE   = 84,
        MAX_KEYS = DEGREE*2,
        MAX_KIDS = DEGREE*2 + 1
    };

    typedef struct PT_Val {
        // if 0, the slice points to the original buffer, if 1 then it's
        // the added buffer.
        uint64_t added  : 1;
        // this is either the line number or buffer offset
        // depending on the kind of table.
        uint64_t offset : 63;
        uint64_t length;
    } PT_Val;

    // B+ Tree node which uses relative offsets for the keys, this
    // means we can insert and lookup using string/file offsets
    // without needing to update every thing when there's an insertion.
    typedef struct PT_Node PT_Node;
    struct PT_Node {
        PT_Node* parent;
        PT_Node* next;

        // Internal nodes have keys between the values, leaf
        // nodes have one key per value.
        //
        // Internal:    [  *, 5,  * ]  (1 key, 2 vals)
        // Leaf:        [ 1:*,  2:* ]  (2 keys, 2 vals)
        //
        uint32_t is_leaf : 1;
        uint32_t count   : 31;

        // total length of all children in this node
        uint64_t length;
        // total cont of all leaf entries in this node
        uint64_t sum;
        uint64_t keys[MAX_KEYS];

        // PT_Node* kids[MAX_VALS];
        // uint64_t vals[MAX_VALS];
    };

    typedef struct {
        PT_Node* node;
        int index;
        uint64_t key;
    } PT_Cursor;

    typedef struct {
        int degree;
        int val_size;
        PT_Node* root;
    } PT_Table;

    // Cursor API
    PT_Cursor pt_lookup(PT_Table* table, uint64_t k);
    PT_Val* pt_get_val(PT_Cursor c);
    bool pt_next_cursor(PT_Cursor* it);

    // Removes entry at the cursor, doesn't touch c->key
    // but it does advance the cursor to the next entry
    bool pt_remove_at_cursor(PT_Table* table, PT_Cursor* c);

    // Inserts at k, shifting all entries >= k up by v.length
    PT_Cursor pt_insert(PT_Table* table, uint64_t k, PT_Val v);

    PT_Cursor pt_lookup_by_abs_index(PT_Table* table, uint64_t i);
    uint64_t pt_get_absolute_index(PT_Node* n, int i);

    ////////////////////////////////
    // Text edit API
    ////////////////////////////////
    // Virtual offsets refer to the visible file's offsets btw.
    typedef struct PieceTable PieceTable;
    struct PieceTable {
        // This buffer is immutable and ideally file mapped
        size_t og_buffer_size;
        const char* og_buffer;

        // This buffer represents any added
        size_t added_buffer_used;
        size_t added_buffer_cap;
        char* added_buffer;

        // virtual offset -> line number
        PT_Table line_starts;
        PT_Table piece_tables;
    };

    bool pt_alloc2(PieceTable* pt, size_t length, const char* data);
    bool pt_alloc(PieceTable* pt, const char* path);
    void pt_free(PieceTable* pt);

    void pt_replace_range(PieceTable* pt, uint64_t start, uint64_t end, size_t length, const char* text);
    size_t pt_read_range(PieceTable* pt, uint64_t start, uint64_t end, char* dst);
    size_t pt_total_size(PieceTable* pt);
    uint64_t pt_get_line_count(PieceTable* pt);

    #if __cplusplus
}
#endif
