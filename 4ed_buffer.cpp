/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 06.01.2017
 *
 * The 4coder base buffer data structure.
 *
 */

// TOP

//
// Buffer low level operations
//

#include "cbt.h"

internal void
write_cursor_with_index(Cursor_With_Index *positions, i32 *count, i64 pos){
    positions[*count].index = *count;
    positions[*count].pos = pos;
    ++(*count);
}

internal void
buffer_quick_sort_cursors(Cursor_With_Index *positions, i32 first, i32 one_past_last){
    if (first + 1 < one_past_last){
        i32 pivot = one_past_last - 1;
        i64 pivot_pos = positions[pivot].pos;
        i32 j = first;
        for (i32 i = first; i < pivot; i += 1){
            i64 pos = positions[i].pos;
            if (pos < pivot_pos){
                Swap(Cursor_With_Index, positions[j], positions[i]);
                j += 1;
            }
        }
        Swap(Cursor_With_Index, positions[j], positions[pivot]);
        buffer_quick_sort_cursors(positions, first, j);
        buffer_quick_sort_cursors(positions, j + 1, one_past_last);
    }
}

internal void
buffer_sort_cursors(Cursor_With_Index *positions, i32 count){
    if (count > 0){
        buffer_quick_sort_cursors(positions, 0, count);
    }
}

internal void
buffer_unsort_cursors(Cursor_With_Index *positions, i32 count){
    if (count > 0){
        i32 i = 0;
        for (;;){
            if (positions[i].index == i){
                i += 1;
                if (i >= count){
                    break;
                }
            }
            else{
                i32 j = positions[i].index;
                Swap(Cursor_With_Index, positions[i], positions[j]);
            }
        }
    }
}

internal void
buffer_update_cursors_lean_l(Cursor_With_Index *sorted_positions, i32 count,
    Batch_Edit *batch){
    Cursor_With_Index *pos = sorted_positions;
    Cursor_With_Index *end_pos = sorted_positions + count;
    i64 shift_amount = 0;
    for (; batch != 0 && pos < end_pos;
        batch = batch->next){
        Range_i64 range = batch->edit.range;
        i64 len = batch->edit.text.size;
        if (shift_amount != 0){
            for (;pos < end_pos && pos->pos < range.first; pos += 1){
                pos->pos += shift_amount;
            }
        }
        else{
            for (;pos < end_pos && pos->pos < range.first; pos += 1);
        }
        i64 new_pos = range.first + shift_amount;
        for (;pos < end_pos && pos->pos <= range.one_past_last; pos += 1){
            pos->pos = new_pos;
        }
        shift_amount += len - (range.one_past_last - range.first);
    }
    if (shift_amount != 0){
        for (;pos < end_pos; pos += 1){
            pos->pos += shift_amount;
        }
    }
}

internal void
buffer_update_cursors_lean_r(Cursor_With_Index *sorted_positions, i32 count,
    Batch_Edit *batch){
    Cursor_With_Index *pos = sorted_positions;
    Cursor_With_Index *end_pos = sorted_positions + count;
    i64 shift_amount = 0;
    for (; batch != 0 && pos < end_pos;
        batch = batch->next){
        Range_i64 range = batch->edit.range;
        i64 len = batch->edit.text.size;
        if (shift_amount != 0){
            for (;pos < end_pos && pos->pos < range.first; pos += 1){
                pos->pos += shift_amount;
            }
        }
        else{
            for (;pos < end_pos && pos->pos < range.first; pos += 1);
        }
        i64 new_pos = range.first + len + shift_amount;
        for (;pos < end_pos && pos->pos < range.one_past_last; pos += 1){
            pos->pos = new_pos;
        }
        shift_amount += len - (range.one_past_last - range.first);
    }
    if (shift_amount != 0){
        for (;pos < end_pos; pos += 1){
            pos->pos += shift_amount;
        }
    }
}

//////////////////////////////////////

internal i64
buffer_size(PieceTable *buffer){
    return pt_total_size(buffer);
}

internal i64
buffer_line_count(PieceTable *buffer){
    return pt_get_line_count(buffer);
}

internal void
buffer_init(PieceTable *buffer, u8 *data, u64 size, Base_Allocator *allocator){
    block_zero_struct(buffer);

    #if 0
    buffer->allocator = allocator;

    u64 capacity = round_up_u64(size*2, KB(4));
    String_Const_u8 memory = base_allocate(allocator, capacity);
    buffer->data = (u8*)memory.str;
    buffer->size1 = size/2;
    buffer->gap_size = capacity - size;
    buffer->size2 = size - buffer->size1;
    buffer->max = capacity;

    block_copy(buffer->data, data, buffer->size1);
    block_copy(buffer->data + buffer->size1 + buffer->gap_size, data + buffer->size1, buffer->size2);
    #endif

    String_Const_u8 memory = base_allocate(allocator, size);
    block_copy(memory.str, data, size);
    pt_alloc2(buffer, size, (const char*) memory.str);
}

internal b32
buffer_replace_range(PieceTable *buffer, Range_i64 range, String_Const_u8 text, i64 shift_amount){
    pt_replace_range(buffer, range.start, range.end, text.size, (const char*) text.str);
    return false;
}

////////////////////////////////

internal void
buffer_chunks_clamp(List_String_Const_u8 *chunks, Range_i64 range){
    i64 p = 0;
    List_String_Const_u8 list = {};
    for (Node_String_Const_u8 *node = chunks->first, *next = 0;
        node != 0;
        node = next){
        next = node->next;
        Range_i64 node_range = Ii64(p, p + node->string.size);
        if (range_overlap(range, node_range)){
            i64 first = Max(node_range.first, range.first) - node_range.first;
            i64 one_past_last = Min(node_range.one_past_last, range.one_past_last) - node_range.first;
            String_Const_u8 s = string_prefix(node->string, one_past_last);
            node->string = string_skip(s, first);
            sll_queue_push(list.first, list.last, node);
            list.total_size += node->string.size;
            list.node_count += 1;
        }
        p = node_range.one_past_last;
    }
    *chunks = list;
}

internal String_Const_u8
buffer_stringify(Arena *arena, PieceTable *buffer, Range_i64 range){
    char *buf = push_array(arena, char, range.end - range.start);
    size_t written = pt_read_range(buffer, range.start, range.end, buf);
    assert(written == range.end - range.start);
    return SCu8((u8*)buf, written);
}

internal i64
buffer_get_line_index(PieceTable *buffer, i64 pos){
    if (pos == 0) {
        return 0;
    }

    PT_Cursor left = pt_lookup(&buffer->line_starts, pos - 1);
    return left.node ? pt_get_absolute_index(left.node, left.index) : 0;
}

Line_Move*
push_line_move(Arena *arena, Line_Move *moves, i64 new_line_first,
    i64 old_line_first, i64 old_line_opl, i64 text_shift){
    Line_Move *move = push_array(arena, Line_Move, 1);
    move->next = moves;
    move->kind = LineMove_ShiftOldValues;
    move->new_line_first = new_line_first;
    move->old_line_first = old_line_first;
    move->old_line_opl = old_line_opl;
    move->text_shift = text_shift;
    return(move);
}

Line_Move*
push_line_move(Arena *arena, Line_Move *moves, i64 new_line_first,
    String_Const_u8 string, i64 text_base){
    Line_Move *move = push_array(arena, Line_Move, 1);
    move->next = moves;
    move->kind = LineMove_MeasureString;
    move->new_line_first = new_line_first;
    move->string = string;
    move->text_base = text_base;
    return(move);
}

internal Buffer_Cursor
buffer_cursor_from_pos(PieceTable *buffer, i64 pos);

internal Range_i64
buffer_get_pos_range_from_line_number(PieceTable *buffer, i64 line_number){
    Range_i64 result = {};
    if (1 <= line_number && line_number <= buffer_line_count(buffer)){
        PT_Cursor left = pt_lookup_by_abs_index(&buffer->line_starts, line_number - 1);
        result.first = line_number > 1 ? left.key + 1 : 0;
        result.one_past_last = left.key + pt_get_val(left)->length + 1;
        result.one_past_last = clamp_top(result.one_past_last, buffer_size(buffer));

        // printf("R %lu => %lu %lu %lu\n", line_number, result.first, result.one_past_last, pt_get_val(left)->length);
        // Buffer_Cursor r = buffer_cursor_from_pos(buffer, result.first);
        // printf("  %lu %lu\n", r.line, r.col);
    }
    return(result);
}

internal i64
buffer_get_first_pos_from_line_number(PieceTable *buffer, i64 line_number){
    i64 result = 0;
    if (line_number < 1){
        result = 0;
    }
    else if (line_number > buffer_line_count(buffer)){
        result = buffer_size(buffer);
    }
    else{
        PT_Cursor left = pt_lookup_by_abs_index(&buffer->line_starts, line_number - 1);
        assert(left.node);
        result = left.key;
    }
    return(result);
}

internal i64
buffer_get_last_pos_from_line_number(PieceTable *buffer, i64 line_number){
    i64 result = 0;
    if (line_number < 1){
        result = 0;
    }
    else if (line_number >= buffer_line_count(buffer)){
        result = buffer_size(buffer);
    }
    else{
        PT_Cursor left = pt_lookup_by_abs_index(&buffer->line_starts, line_number);
        assert(left.node);
        result = left.key;
    }
    return(result);
}

internal Buffer_Cursor
buffer_cursor_from_pos(PieceTable *buffer, i64 pos){
    i64 size = buffer_size(buffer);
    pos = clamp(0, pos, size);

    PT_Cursor left = pt_lookup(&buffer->line_starts, pos ? pos - 1 : 0);
    i64 line_index = left.node ? pt_get_absolute_index(left.node, left.index) : 0;

    Buffer_Cursor result = {};
    result.pos  = pos;
    result.line = line_index + 1;
    result.col  = (pos - (left.key ? left.key + 1 : 0)) + 1;
    return(result);
}

internal Buffer_Cursor
buffer_cursor_from_line_col(PieceTable *buffer, i64 line, i64 col){
    i64 size = buffer_size(buffer);
    i64 line_index = line - 1;
    i64 line_count = buffer_line_count(buffer);
    line_index = clamp(0, line_index, line_count - 1);

    PT_Cursor left = pt_lookup_by_abs_index(&buffer->line_starts, line_index);
    i64 this_start = left.key ? left.key + 1 : 0;
    i64 max_col    = pt_get_val(left)->length - !!left.key;
    if (line_index + 1 == line_count){
        max_col += 1;
    }
    max_col = clamp_bot(1, max_col);

    if (col < 0){
        if (-col > max_col){
            col = 1;
        }
        else{
            col = max_col + col + 1;
        }
    }
    else if (col == 0){
        col = 1;
    }
    else{
        col = clamp_top(col, max_col);
    }
    Assert(col > 0);
    i64 adjusted_pos = col - 1;

    i64 pos = this_start + adjusted_pos;

    Buffer_Cursor result = {};
    result.pos = pos;
    result.line = line_index + 1;
    result.col = col;
    return(result);
}

internal Buffer_Chunk_Position
buffer_get_chunk_position(String_Const_u8_Array chunks, i64 buffer_size, i64 real_pos){
    Buffer_Chunk_Position pos = {};
    pos.real_pos = real_pos;
    pos.chunk_pos = real_pos;
    if (pos.real_pos != buffer_size){
        for (;(i64)(chunks.vals[pos.chunk_index].size) <= pos.chunk_pos;){
            Assert(pos.chunk_index < chunks.count);
            pos.chunk_pos -= (i32)chunks.vals[pos.chunk_index].size;
            pos.chunk_index += 1;
        }
    }
    else{
        pos.chunk_index = chunks.count - 1;
        pos.chunk_pos = (i32)chunks.vals[pos.chunk_index].size;
    }
    return(pos);
}

internal i32
buffer_chunk_position_iterate(String_Const_u8_Array chunks, Buffer_Chunk_Position *pos, Scan_Direction direction){
    i32 past_end = 0;
    pos->real_pos += direction;
    pos->chunk_pos += direction;
    if (pos->chunk_pos < 0){
        if (pos->chunk_index == 0){
            past_end = -1;
        }
        else{
            pos->chunk_index -= 1;
            pos->chunk_pos = (i32)chunks.vals[pos->chunk_index].size - 1;
        }
    }
    else if (pos->chunk_pos >= (i64)(chunks.vals[pos->chunk_index].size)){
        pos->chunk_index += 1;
        if (pos->chunk_index == chunks.count){
            past_end = 1;
        }
        else{
            pos->chunk_pos = 0;
        }
    }
    return(past_end);
}

// BOTTOM

