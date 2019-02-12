/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 24.03.2018
 *
 * Working_Set data structure
 *
 */

// TOP

#if !defined(FRED_WORKING_SET_H)
#define FRED_WORKING_SET_H

struct Non_File_Table_Entry{
    String name;
    Buffer_Slot_ID id;
};

struct File_Array{
    Editing_File *files;
    i32 size;
};

struct Working_Set{
    File_Array *file_arrays;
    i32 file_count;
    i32 file_max;
    i16 array_count;
    i16 array_max;
    
    Node free_sentinel;
    Node used_sentinel;
    
    Node edit_finished_list;
    u64 time_of_next_edit_finished_signal;
    Plat_Handle edit_finished_timer;
    b32 do_not_mark_edits;
    
    Table canon_table;
    Table name_table;
    
    // TODO(allen): do(update clipboard system to exist fully in the custom layer)
    String clipboards[64];
    i32 clipboard_size;
    i32 clipboard_max_size;
    i32 clipboard_current;
    i32 clipboard_rolling;
    
    Node *sync_check_iter;
    
    i32 default_display_width;
    i32 default_minimum_base_display_width;
};

struct File_Name_Entry{
    String name;
    Buffer_Slot_ID id;
};

internal void
file_mark_edit_finished(Working_Set *working_set, Editing_File *file);

internal b32
file_unmark_edit_finished(Editing_File *file);

#endif

// BOTTOM

