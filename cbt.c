#include "cbt.h"

#include <time.h>
#include <sys/stat.h>
#if defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define fileno _fileno
#define fstat  _fstat
#define stat   _stat
#define strdup _strdup
#endif

size_t pt_get_sum(PT_Node* n, int i) {
    if (n->is_leaf) {
        return 1;
    } else {
        PT_Node** kids = (PT_Node**) (n + 1);
        return kids[i]->sum;
    }
}

static int pt_bin_search(PT_Node* node, uint64_t key) {
    size_t left = 0, right = node->count;
    if (node->is_leaf) {
        while (left != right) {
            size_t i = (left + right) / 2;
            uint64_t key_at_idx = node->keys[i];
            if (key_at_idx == key) {
                return i;
            } else if (key_at_idx > key) {
                right = i;
            } else {
                left = i + 1;
            }
        }
        return left - 1;
    } else {
        while (left != right) {
            size_t i = (left + right) / 2;
            uint64_t key_at_idx = node->keys[i];
            if (key_at_idx > key) {
                right = i;
            } else {
                left = i + 1;
            }
        }
        return left;
    }
}

void pt_dump_flat(PT_Node* n, uint64_t base) {
    if (n->is_leaf) {
        PT_Val* vals = (PT_Val*) (n + 1);
        for (int i = 0; i < n->count; i++) {
            printf("%lu:%lu ", base + n->keys[i], vals[i].length);
        }
    } else {
        PT_Node** kids = (PT_Node**) (n + 1);
        pt_dump_flat(kids[0], base);
        for (int i = 1; i < n->count + 1; i++) {
            pt_dump_flat(kids[i], base + n->keys[i - 1]);
        }
    }
}

void pt_assert_lengths(PT_Node* n, uint64_t base, int depth) {
    if (n->is_leaf) {
        PT_Val* vals = (PT_Val*) (n + 1);
        size_t length = n->keys[0];
        for (int i = 0; i < n->count; i++) {
            assert(length == n->keys[i]);
            length += vals[i].length;
        }
        assert(length == n->length);
    } else {
        PT_Node** kids = (PT_Node**) (n + 1);
        size_t length = kids[0]->length;
        pt_assert_lengths(kids[0], base, depth + 1);
        for (int i = 1; i < n->count + 1; i++) {
            assert(length == n->keys[i - 1]);
            pt_assert_lengths(kids[i], base + n->keys[i - 1], depth + 1);
            length += kids[i]->length;
        }
        assert(length == n->length);
    }
}

void pt_dump(PT_Node* n, uint64_t base, int depth) {
    if (n == NULL) { return; }
    for (int i = 0; i < depth; i++) { printf("  "); }
    
    if (n->is_leaf) {
        printf("%lu:%lu:[ ", base, n->length);
        PT_Val* vals = (PT_Val*) (n + 1);
        size_t length = n->keys[0];
        for (int i = 0; i < n->count; i++) {
            assert(length == n->keys[i]);
            printf("%lu:%lu ", base + n->keys[i], vals[i].length);
            length += vals[i].length;
        }
        printf("]\n");
        assert(length == n->length);
    } else {
        printf("%lu:%lu:[ ", base, n->length);
        for (int i = 0; i < n->count; i++) {
            printf("* %lu ", base + n->keys[i]);
        }
        printf("* ]\n");
        
        PT_Node** kids = (PT_Node**) (n + 1);
        size_t length = kids[0]->length;
        pt_dump(kids[0], base, depth + 1);
        for (int i = 1; i < n->count + 1; i++) {
            assert(length == n->keys[i - 1]);
            pt_dump(kids[i], base + n->keys[i - 1], depth + 1);
            length += kids[i]->length;
        }
        assert(length == n->length);
    }
}

void pt_assert_sort(PT_Node* n) {
    #ifndef NDEBUG
    for (int i = 1; i < n->count; i++) {
        assert(n->keys[i - 1] <= n->keys[i]);
    }
    #endif
}

static void pt_move_val(PT_Node* n, int dst, PT_Node* n2, int src, int count) {
    assert(n->is_leaf == n2->is_leaf);
    size_t val_size = n->is_leaf ? sizeof(PT_Val) : sizeof(PT_Node*);
    
    char* dst_v = (char*) (n + 1);
    char* src_v = (char*) (n2 + 1);
    memmove(dst_v + dst*val_size, src_v + src*val_size, count * val_size);
}

static void pt_update_parents(PT_Node* n, int start, int count) {
    if (n->is_leaf) { return; }
    
    PT_Node** kids = (PT_Node**) (n + 1);
    for (int i = 0; i < count; i++) {
        kids[start + i]->parent = n;
    }
}

static void pt_split_child(PT_Node* x, PT_Node* y, int idx, uint64_t base) {
    assert(y->count == DEGREE*2 - 1);
    
    size_t payload_size = y->is_leaf ? MAX_KEYS*sizeof(PT_Val) : MAX_KIDS*sizeof(PT_Node*);
    PT_Node* z = calloc(1, sizeof(PT_Node) + payload_size);
    z->is_leaf = y->is_leaf;
    
    // insert into x
    uint64_t middle = y->keys[DEGREE - !y->is_leaf];
    {
        // insert value
        assert(!x->is_leaf);
        PT_Node** x_vals = (PT_Node**) (x + 1);
        for (int i = x->count; i >= idx + 1; i--) {
            x_vals[i + 1] = x_vals[i];
        }
        x_vals[idx + 1] = z;
        
        // insert key
        for (int i = x->count - 1; i >= idx; i--) {
            x->keys[i + 1] = x->keys[i];
        }
        x->keys[idx] = base + middle;
        x->count += 1;
        
        pt_assert_sort(x);
    }
    
    // copy upper half to z
    {
        size_t val_count = DEGREE - y->is_leaf;
        z->count = DEGREE - 1;
        
        // copy keys
        for (int i = 0; i < DEGREE - 1; i++) {
            z->keys[i] = y->keys[DEGREE + i] - middle;
        }
        pt_move_val(z, 0, y, DEGREE, val_count);
        pt_update_parents(z, 0, val_count);
        
        // update sums
        if (y->is_leaf) {
            z->sum =  z->count;
            y->sum -= z->count;
        } else {
            PT_Node** kids = (PT_Node**) (z + 1);
            for (int i = 0; i < val_count; i++) {
                uint64_t sum = kids[i]->sum;
                z->sum += sum;
                y->sum -= sum;
            }
        }
        pt_assert_sort(z);
    }
    
    y->count = DEGREE - !y->is_leaf;
    z->parent = y->parent = x;
    
    z->next = y->next;
    y->next = z;
    
    // split length
    z->length = y->length - middle;
    y->length = middle;
}

PT_Cursor pt_lookup_by_abs_index(PT_Table* table, uint64_t k) {
    PT_Node* node = table->root;
    if (node == NULL) {
        return (PT_Cursor){ 0 };
    }
    
    // layers of binary search
    uint64_t pos = 0;
    for (;;) {
        if (node->is_leaf) {
            return (PT_Cursor){ node, k, pos + node->keys[k] };
        } else {
            int i = 0;
            uint64_t curr = 0;
            PT_Node** kids = (PT_Node**) (node + 1);
            for (; i < node->count; i++) {
                uint64_t next = curr + kids[i]->sum;
                if (k < next) { break; }
                curr = next, pos += kids[i]->length;
            }
            
            k -= curr;
            node = ((PT_Node**) (node + 1))[i];
        }
    }
}

PT_Cursor pt_lookup(PT_Table* table, uint64_t k) {
    uint64_t start_k = k;
    PT_Node* node = table->root;
    if (node == NULL) {
        return (PT_Cursor){ 0 };
    }
    
    // layers of binary search
    for (;;) {
        int index = pt_bin_search(node, k);
        if (node->is_leaf) {
            if (index < 0) {
                return (PT_Cursor){ 0 };
            }
            
            return (PT_Cursor){ node, index, start_k + (node->keys[index] - k) };
        } else {
            k -= index ? node->keys[index - 1] : 0;
            node = ((PT_Node**) (node + 1))[index];
        }
    }
}

static int pt_find_node_in_parent(PT_Node* n) {
    PT_Node* parent = n->parent;
    PT_Node** kids = (PT_Node**) (parent + 1);
    for (int i = 0; i < parent->count + 1; i++) {
        if (kids[i] == n) { return i; }
    }
    return -1;
}

bool pt_next_cursor(PT_Cursor* it) {
    if (it->index == it->node->count - 1) {
        it->node = it->node->next;
        it->index = 0;
        return it->node != NULL;
    } else {
        it->index += 1;
        return true;
    }
}

bool pt_prev_cursor(PT_Cursor* it) {
    if (it->index == 0) {
        // walk up (and left)
        PT_Node* curr = it->node->parent;
        while (curr != NULL) {
            int kid = pt_find_node_in_parent(curr);
            if (kid > 0) {
                PT_Node** kids = (PT_Node**) (curr + 1);
                curr = kids[kid - 1];
                break;
            }
            curr = curr->parent;
        }
        
        if (curr == NULL) {
            it->node = NULL;
            return false;
        }
        
        // walk back down (and right)
        while (!curr->is_leaf) {
            PT_Node** kids = (PT_Node**) (curr + 1);
            curr = kids[curr->count];
        }
        
        it->node  = curr;
        it->index = curr->count - 1;
        return true;
    } else {
        it->index -= 1;
        return true;
    }
}

PT_Val* pt_get_val(PT_Cursor c) {
    PT_Val* vals = (PT_Val*) (c.node + 1);
    return &vals[c.index];
}

size_t pt_get_entry_length(PT_Node* n, int i) {
    if (n->is_leaf) {
        PT_Val* vals = (PT_Val*) (n + 1);
        return vals[i].length;
    } else {
        PT_Node** kids = (PT_Node**) (n + 1);
        return kids[i]->length;
    }
}

void pt_notify_length_change(PT_Node* node, int leaf_i, int64_t delta, int64_t sum_delta) {
    for (int i = leaf_i + 1; i < node->count; i++) {
        node->keys[i] += delta;
    }
    
    // update length
    do {
        PT_Node* parent = node->parent;
        if (parent) {
            PT_Node** kids = (PT_Node**) (parent + 1);
            
            // find self
            int i = 0;
            for (; i < parent->count + 1; i++) {
                if (kids[i] == node) { break; }
            }
            // shift up all keys beyond that point
            for (; i < parent->count; i++) {
                parent->keys[i] += delta;
            }
            pt_assert_sort(parent);
        }
        node->length += delta;
        node->sum += sum_delta;
        node = node->parent;
    } while (node);
}

static void pt_rebalance(PT_Table* table, PT_Node* n, PT_Cursor* c) {
    assert(n->count < DEGREE);
    
    // try to borrow from left or right
    int parent_key_count = n->parent->count;
    assert(parent_key_count != 0);
    
    uint64_t* parent_keys = n->parent->keys;
    PT_Node** siblings = (PT_Node**) (n->parent + 1);
    int j = pt_find_node_in_parent(n);
    
    if (j > 0 && siblings[j-1]->count > DEGREE) {
        // borrow from the left
        PT_Node* prev = siblings[j-1];
        PT_Node* curr = n;
        
        // update cursor
        if (c != NULL && c->node == curr) {
            c->index += 1;
        }
        
        // shift up, insert last of prev into the bottom of curr
        int64_t delta = pt_get_entry_length(prev, prev->count + !prev->is_leaf - 1);
        for (int i = curr->count; i--;) {
            curr->keys[i + 1] = curr->keys[i] + delta;
        }
        curr->keys[0] = prev->is_leaf ? 0 : delta;
        
        // shift up vals, insert prev into curr[0]
        pt_move_val(curr, 1, curr, 0, curr->count + !curr->is_leaf);
        pt_move_val(curr, 0, prev, prev->count + !prev->is_leaf - 1, 1);
        pt_update_parents(curr, 0, 1);
        
        // move lengths around
        prev->length -= delta;
        curr->length += delta;
        
        // move sums around
        size_t sum = pt_get_sum(curr, 0);
        prev->sum -= sum;
        curr->sum += sum;
        
        // update parent's key
        parent_keys[j - 1] -= delta;
        prev->count -= 1;
        curr->count += 1;
        
        if (prev->count < DEGREE - !prev->is_leaf) {
            pt_rebalance(table, prev, NULL);
        }
    } else if (j < parent_key_count && siblings[j+1]->count > DEGREE) {
        // borrow from the right
        PT_Node* curr = n;
        PT_Node* next = siblings[j+1];
        
        // move lengths around
        uint64_t delta = pt_get_entry_length(next, 0);
        uint64_t first_k = curr->length;
        next->length -= delta;
        curr->length += delta;
        
        // move sums around
        size_t sum = pt_get_sum(next, 0);
        next->sum -= sum;
        curr->sum += sum;
        
        // shift down, insert first of next into top of curr
        for (int i = 0; i < next->count; i++) {
            next->keys[i] = next->keys[i + 1] - delta;
        }
        curr->keys[curr->count] = first_k;
        // shift down vals
        pt_move_val(curr, curr->count + !curr->is_leaf, next, 0, 1);
        pt_move_val(next, 0, next, 1, next->count + !next->is_leaf);
        pt_update_parents(curr, 1, curr->count + !curr->is_leaf);
        
        // update parent's key
        parent_keys[j] += delta;
        next->count -= 1;
        curr->count += 1;
        
        if (next->count < DEGREE - !next->is_leaf) {
            pt_rebalance(table, next, NULL);
        }
    } else {
        if (j == parent_key_count) {
            j -= 1;
        }
        
        // merge, [j] and [j+1]
        PT_Node* curr = siblings[j];
        PT_Node* next = siblings[j+1];
        
        // update cursor
        if (c != NULL && c->node == next) {
            c->node   = curr;
            c->index += curr->count;
        }
        
        // copy all [j+1] keys & values into [j]
        assert(curr->is_leaf == next->is_leaf);
        assert(curr->count + next->count + !curr->is_leaf < DEGREE*2);
        uint64_t delta = parent_keys[j] - (j ? parent_keys[j-1] : 0);
        if (curr->is_leaf) {
            for (int i = 0; i < next->count; i++) {
                curr->keys[curr->count + i] = next->keys[i] + delta;
            }
        } else {
            curr->keys[curr->count] = delta;
            for (int i = 0; i < next->count; i++) {
                curr->keys[curr->count + i + 1] = next->keys[i] + delta;
            }
            curr->count += 1;
        }
        pt_move_val(curr, curr->count, next, 0, next->count + !next->is_leaf);
        pt_update_parents(curr, curr->count, next->count + !next->is_leaf);
        curr->count += next->count;
        next->count = 0;
        curr->next = next->next;
        assert(curr->count < 2*DEGREE);
        
        curr->length += next->length;
        next->length = 0;
        
        curr->sum += next->sum;
        next->sum = 0;
        
        // shift down & safely delete it
        for (int i = j + 1; i < parent_key_count; i++) {
            siblings[i] = siblings[i + 1];
            parent_keys[i - 1] = parent_keys[i];
        }
        n->parent->count -= 1;
        
        if (n->parent->count == 0) {
            // no need for internal root node anymore
            table->root = siblings[0];
            table->root->parent = NULL;
        } else if (n->parent != table->root && n->parent->count < DEGREE - 1) {
            pt_rebalance(table, n->parent, NULL);
        }
    }
}

bool pt_remove_at_cursor(PT_Table* table, PT_Cursor* c) {
    PT_Val* vals = (PT_Val*) (c->node + 1);
    size_t i = c->index;
    pt_notify_length_change(c->node, i, -vals[i].length, -1);
    
    // shift down
    PT_Node* n = c->node;
    size_t key_count = --n->count;
    if (i < key_count) {
        for (; i < key_count; i++) {
            n->keys[i] = n->keys[i + 1];
            vals[i] = vals[i + 1];
        }
    } else {
        // we're at the end of the leaf page, the
        // next entry is on the next node.
        c->node  = n->next;
        c->index = 0;
    }
    
    if (key_count < DEGREE && n != NULL && n != table->root) {
        pt_rebalance(table, n, c);
    }
    
    return c->node != NULL;
}

PT_Cursor pt_insert(PT_Table* table, uint64_t k, PT_Val v) {
    if (table->root == NULL) {
        PT_Node* new_node = calloc(1, sizeof(PT_Node) + MAX_KEYS*sizeof(PT_Val));
        new_node->count   = 1;
        new_node->sum     = 1;
        new_node->is_leaf = true;
        new_node->length  = v.length;
        new_node->keys[0] = k;
        ((PT_Val*) (new_node + 1))[0] = v;
        table->root = new_node;
        return (PT_Cursor){ new_node, 0, k };
    } else {
        uint64_t start_k = k;
        size_t max_keys = DEGREE*2 - 1;
        
        // Rotate the root
        PT_Node* node = table->root;
        if (node->count == max_keys) {
            PT_Node* new_node = calloc(1, sizeof(PT_Node) + MAX_KIDS*sizeof(PT_Node*));
            new_node->length = node->length;
            new_node->sum = node->sum;
            
            // Make old root as child of new root
            PT_Node** kids = (PT_Node**) (new_node + 1);
            kids[0] = node;
            
            // Split the old root and move 1 key to the new root
            pt_split_child(new_node, node, 0, 0);
            
            // New root has two children now. Decide which of the
            // two children is going to have new key
            int i = 0;
            if (k > new_node->keys[0]) {
                k -= new_node->keys[0];
                i++;
            }
            
            node = ((PT_Node**) (new_node + 1))[i];
            table->root = new_node;
        }
        
        static int times = 0;
        ++times;
        
        // printf("\n\n\nTIMES %d : %zu\n", times, k);
        
        int left = pt_bin_search(node, k);
        while (!node->is_leaf) {
            uint64_t base = left ? node->keys[left - 1] : 0;
            PT_Node* kid = ((PT_Node**) (node + 1))[left];
            assert(kid);
            
            if (kid->count == max_keys) {
                // If the child is full, then split it
                pt_split_child(node, kid, left, base);
                
                // After split, the middle key of C[left] goes up and
                // C[left] is splitted into two. See which of the two
                // is going to have the new key
                if (k > node->keys[left]) {
                    k -= node->keys[left];
                    node = ((PT_Node**) (node + 1))[left + 1];
                    left = pt_bin_search(node, k);
                    continue;
                }
            }
            
            // Translate key into kid's key
            k -= base;
            node = kid;
            left = pt_bin_search(node, k);
        }
        
        PT_Val* vals = (PT_Val*) (node + 1);
        // if the keys[left] matches k, we're gonna place the entry to the
        // left of it (this "insert" call doesn't replace entries).
        if (left < 0 || node->keys[left] != k) {
            left += 1;
        }
        
        // shift up
        if (left < node->count) {
            memmove(&node->keys[left+1], &node->keys[left], (node->count - left) * sizeof(uint64_t));
            memmove(&vals[left+1], &vals[left], (node->count - left) * sizeof(PT_Val));
        }
        
        assert(node->count < max_keys);
        assert(left < node->count + 1);
        
        node->count += 1;
        node->keys[left] = k;
        vals[left] = v;
        
        pt_notify_length_change(node, left, v.length, 1);
        return (PT_Cursor){ node, left, start_k };
    }
}

void pt_free(PieceTable* pt) {
    // TODO(NeGate)
}

bool pt_alloc(PieceTable* pt, const char* path) {
    // Maybe we can file map this stuff, but the issue is that when we
    // write back into the file system we'd need to preserve a version
    // of the file as the original buffer.
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("Could not read file: %s\n", path);
        return false;
    }
    
    int descriptor = fileno(file);
    
    struct stat file_stats;
    if (fstat(descriptor, &file_stats) == -1) {
        fclose(file);
        abort();
    }
    
    size_t size  = file_stats.st_size;
    char* buffer = malloc(size);
    
    fseek(file, 0, SEEK_SET);
    fread(buffer, 1, size, file);
    fclose(file);
    
    return pt_alloc2(pt, size, buffer);
}

bool pt_alloc2(PieceTable* pt, size_t length, const char* data) {
    memset(pt, 0, sizeof(*pt));
    
    pt->added_buffer_cap = 16*1024;
    pt->added_buffer = malloc(pt->added_buffer_cap);
    
    pt->og_buffer_size = length;
    pt->og_buffer      = data;
    
    if (length > 0) {
        pt_insert(&pt->piece_tables, 0, (PT_Val){ 0, 0, length });
        
        uint64_t last_line = 0, count = 0;
        for (size_t i = 0; i < pt->og_buffer_size; i++) {
            if (pt->og_buffer[i] == '\n') {
                assert(i - last_line >= 0);
                
                size_t one_past_last = i + 1;
                // printf("LINE[%ld] %zu %zu\n", ++count, last_line, one_past_last);
                pt_insert(&pt->line_starts, last_line, (PT_Val){ 0, 0, one_past_last - last_line });
                
                #if 0
                printf("\n\n\nPUT %zu\n", last_line);
                pt_dump(pt->line_starts.root, 0, 0);
                
                // Quadratic validate
                uint64_t last_line2 = 0;
                for (size_t j = 0; j < i; j++) {
                    if (pt->og_buffer[j] == '\n') {
                        PT_Cursor c = pt_lookup(&pt->line_starts, last_line2);
                        PT_Val* piece = pt_get_val(c);
                        printf("VALIDATE %ld => %ld\n", last_line2, c.key);
                        assert(c.key == last_line2);
                        assert(piece->length == (j + 1) - last_line2);
                        last_line2 = j + 1;
                    }
                }
                #endif
                
                last_line = one_past_last;
            }
        }
        
        if (last_line <= pt->og_buffer_size) {
            pt_insert(&pt->line_starts, last_line, (PT_Val){ 0, 0, pt->og_buffer_size - last_line });
        }
    }
    
    return true;
}

int pt_dump_text0(PieceTable* table, PT_Node* n, uint64_t base, int status) {
    if (n->is_leaf) {
        PT_Val* vals = (PT_Val*) (n + 1);
        for (int i = 0; i < n->count; i++) {
            const char* buffer = vals[i].added ? table->added_buffer : table->og_buffer;
            printf("%.*s", (int) vals[i].length, buffer + vals[i].offset);
            printf(status&1 ? "\x1b[41m" : "\x1b[0m"), status += 1;
        }
    } else {
        PT_Node** kids = (PT_Node**) (n + 1);
        status = pt_dump_text0(table, kids[0], base, status);
        for (int i = 1; i < n->count + 1; i++) {
            status = pt_dump_text0(table, kids[i], base + n->keys[i - 1], status);
        }
    }
    return status;
}

int pt_dump_text(PieceTable* table) {
    printf("\x1b[0m");
    int count = pt_dump_text0(table, table->piece_tables.root, 0, 0);
    printf("\x1b[0m\n");
    return count;
}

static uint64_t pt_add_buffer(PieceTable* pt, size_t length, const char* text) {
    if (pt->added_buffer_used + length > pt->added_buffer_cap) {
        pt->added_buffer_cap *= 2;
        pt->added_buffer = realloc(pt->added_buffer, pt->added_buffer_cap);
    }
    size_t buf_pos = pt->added_buffer_used;
    memcpy(pt->added_buffer+buf_pos, text, length);
    pt->added_buffer_used += length;
    return buf_pos;
}

void pt_validate_lines(PieceTable* pt) {
    PT_Cursor left = pt_lookup(&pt->piece_tables, 0);
    uint64_t line_start = 0;
    
    while (left.node) {
        PT_Val* piece = pt_get_val(left);
        const char* buffer = (piece->added ? pt->added_buffer : pt->og_buffer) + piece->offset;
        
        // scan for line starts
        for (size_t i = 0; i < piece->length; i++) {
            size_t pos_i = left.key + i;
            if (buffer[i] == '\n') {
                PT_Cursor q = pt_lookup(&pt->line_starts, line_start);
                PT_Val* qv  = pt_get_val(q);
                assert(q.key == line_start);
                assert(q.key + qv->length == pos_i);
                line_start = pos_i;
            }
        }
        
        pt_next_cursor(&left), left.key += piece->length;
    }
    assert(pt->piece_tables.root->length == pt->line_starts.root->length);
}

static void pt_notify_added_lines(PieceTable* pt, uint64_t pos, size_t length, const char* text) {
    // printf("NOTIFY %.*s\n", (int) length, text);
    // Grow space, this will shift things up
    if (pt->line_starts.root) {
        PT_Cursor left = pt_lookup(&pt->line_starts, pos);
        PT_Val* piece = pt_get_val(left);
        piece->length += length;
        pt_notify_length_change(left.node, left.index, length, 0);
    } else {
        assert(pos == 0);
        pt_insert(&pt->line_starts, 0, (PT_Val){ 0, 0, length });
    }
    
    // Split range accordingly
    for (size_t i = 0; i < length; i++) {
        if (text[i] != '\n') { continue; }
        
        // lazy lookup to avoid slowness on the many many non-newline inserts
        uint64_t pos_i = pos+i+1;
        
        // split left piece at pos_i
        PT_Cursor left = pt_lookup(&pt->line_starts, pos_i);
        PT_Val* piece = pt_get_val(left);
        
        uint64_t split = (left.key + piece->length) - pos_i;
        // printf("  SPLIT %ld %ld %ld\n", pos_i, split, left.key + piece->length);
        assert((left.key + piece->length) >= pos_i);
        
        if (split > 0) {
            piece->length -= split;
            pt_notify_length_change(left.node, left.index, -split, 0);
            
            // Newline at pos_i
            pt_insert(&pt->line_starts, pos_i, (PT_Val){ 0, 0, split });
        } else {
            // Empty line at the end
            pt_insert(&pt->line_starts, pos_i, (PT_Val){ 0, 0, 0 });
        }
    }
}

static uint64_t pt_timings[2], pt_calls[2];
static uint64_t get_nanos(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ((uint64_t)ts.tv_sec*1000000000LL) + ts.tv_nsec;
}

static uint64_t u64_max(uint64_t a, uint64_t b) { return a > b ? a : b; }
static uint64_t u64_min(uint64_t a, uint64_t b) { return a < b ? a : b; }
size_t pt_read_range(PieceTable* pt, uint64_t start, uint64_t end, char* dst) {
    if (pt->piece_tables.root == NULL || start == end) {
        return 0;
    }
    
    // find the first
    PT_Cursor left = pt_lookup(&pt->piece_tables, start);
    size_t curr = 0, count = end - start, clip = start - left.key;
    do {
        PT_Val* piece = pt_get_val(left);
        
        const char* buffer = piece->added ? pt->added_buffer : pt->og_buffer;
        size_t limit = u64_min(count - curr, piece->length - clip);
        memcpy(dst + curr, buffer + piece->offset + clip, limit);
        
        curr += limit, clip = 0;
    } while (curr < count && pt_next_cursor(&left));
    return curr;
}

void pt_replace_range(PieceTable* pt, uint64_t start, uint64_t end, size_t length, const char* text) {
    uint64_t start_time = get_nanos();
    size_t total_length = pt->piece_tables.root ? pt->piece_tables.root->length : 0;
    assert(start <= total_length);

    // printf("replace_range(%ld, %ld, %ld, %p)\n", start, end, length, text);
    
    ////////////////////////////////
    // Delete line range of [start, end), shifts right down
    ////////////////////////////////
    if (start != end && pt->line_starts.root != NULL && pt->line_starts.root->count > 0) {
        PT_Cursor left = pt_lookup(&pt->line_starts, start);
        // pt_dump(pt->line_starts.root, 0, 0);
        
        // if the cursor lands behind start, then we skip ahead
        // and tally how many bytes of that node need to be trimmed.
        uint64_t to_preserve = 0;
        if (left.key <= start) {
            PT_Val* piece = pt_get_val(left);
            to_preserve += u64_min(start - left.key, piece->length);
            to_preserve += piece->length - u64_min(end - left.key, piece->length);
            left.key += piece->length;
            if (!pt_next_cursor(&left)) { goto skip2; }
        }
        
        // Remove lines within the range
        while (left.key < end) {
            PT_Val* piece = pt_get_val(left);
            assert(left.key >= start);
            // tally up all the space we're deleteing because
            // we're gonna add it back in once we're done here.
            to_preserve += piece->length - u64_min(end - left.key, piece->length);
            left.key    += piece->length;
            // we can completely delete this piece
            if (!pt_remove_at_cursor(&pt->line_starts, &left)) {
                break;
            }
        }
        
        skip2:;
        // Find the entry right behind, we're growing it
        left = pt_lookup(&pt->line_starts, start);
        PT_Val* piece = pt_get_val(left);
        int64_t delta = to_preserve - piece->length;
        piece->length = to_preserve;
        pt_notify_length_change(left.node, left.index, delta, 0);
    }
    
    if (pt->piece_tables.root != NULL && pt->piece_tables.root->count > 0) {
        PT_Cursor left = pt_lookup(&pt->piece_tables, start ? start - 1 : 0);
        PT_Val* left_v = pt_get_val(left);
        uint64_t left_end = left.key + left_v->length;
        
        // Optimization, if we're appending characters to a piece which
        // references the end of the append buffer, just grow the piece.
        if (start == end && left_end == start && left_v->added) {
            if (pt->added_buffer_used == left_v->offset + left_v->length) {
                // printf("GROW1! %.*s\n", (int) length, text);

                uint64_t buf_pos = pt_add_buffer(pt, length, text);
                assert(left_v->offset + left_v->length == buf_pos);
                left_v->length += length;
                pt_notify_length_change(left.node, left.index, length, 0);
                pt_notify_added_lines(pt, start, length, text);
                pt_timings[0] += get_nanos() - start_time, pt_calls[0]++;
                return;
            }

            // if the last added buffer insertion matches our current insertion, grow
            // our piece without allocating new added bytes.
            if (pt->added_buffer_used - length == left_v->offset + left_v->length &&
                memcmp(&pt->added_buffer[pt->added_buffer_used - length], text, length) == 0) {
                // grow piece
                // printf("GROW2! %.*s\n", (int) length, text);

                left_v->length += length;
                pt_notify_length_change(left.node, left.index, length, 0);
                pt_notify_added_lines(pt, start, length, text);
                pt_timings[0] += get_nanos() - start_time, pt_calls[0]++;
                return;
            }
        }
        
        if (left.key != start) {
            // If we're splitting at the end of a piece, just don't
            if (left.key+left_v->length == start) {
                if (!pt_next_cursor(&left)) { goto skip; }
            } else {
                // Truncate left entry
                left_v->length = start - left.key;
                pt_notify_length_change(left.node, left.index, start - left_end, 0);
                
                // Split for right entry
                PT_Val new_right_v = *left_v;
                new_right_v.length = left_end - start;
                new_right_v.offset += start - left.key;
                left = pt_insert(&pt->piece_tables, start, new_right_v);
            }
        }
        
        // Iterate the cursor to truncate each piece that's within the deleted range
        uint64_t to_delete = end - start;
        while (to_delete > 0) {
            PT_Val* piece = pt_get_val(left);
            if (to_delete < piece->length) {
                // truncate this node
                piece->offset += to_delete;
                piece->length -= to_delete;
                pt_notify_length_change(left.node, left.index, -to_delete, 0);
                break;
            } else {
                // we can completely delete this piece
                to_delete -= piece->length;
                if (!pt_remove_at_cursor(&pt->piece_tables, &left)) {
                    break;
                }
            }
        }
        
        skip:;
    }
    
    ////////////////////////////////
    // Insert text, shifts right up
    ////////////////////////////////
    if (length > 0) {
        uint64_t buf_pos;
        // We could share the same characters as the last insertion
        if (pt->added_buffer_used >= length && memcmp(&pt->added_buffer[pt->added_buffer_used - length], text, length) == 0) {
            buf_pos = pt->added_buffer_used - length;
        } else {
            buf_pos = pt_add_buffer(pt, length, text);
        }
        pt_insert(&pt->piece_tables, start, (PT_Val){ 1, buf_pos, length });
        pt_notify_added_lines(pt, start, length, text);
    }
    pt_timings[0] += get_nanos() - start_time, pt_calls[0]++;
}

uint64_t pt_get_absolute_index(PT_Node* n, int i) {
    uint64_t start_time = get_nanos();
    uint64_t total = i;
    while (n->parent) {
        // sum of siblings before n
        PT_Node* parent = n->parent;
        PT_Node** kids = (PT_Node**) (parent + 1);
        for (int i = 0; i < parent->count + 1; i++) {
            if (kids[i] == n) { break; }
            total += kids[i]->sum;
        }
        
        n = parent;
    }
    
    pt_timings[1] += get_nanos() - start_time, pt_calls[1]++;
    return total;
}

uint64_t pt_get_line_count(PieceTable* pt) {
    return pt->line_starts.root ? pt->line_starts.root->sum : 0;
}

size_t pt_total_size(PieceTable* pt) {
    return pt->piece_tables.root ? pt->piece_tables.root->length : 0;
}
