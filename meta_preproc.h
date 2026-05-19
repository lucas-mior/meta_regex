#ifndef META_PREPROC_H
#define META_PREPROC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cbase/util.c"
#include "meta.h"

#define PREPROC_OP_BUFFER_SIZE 16384
#define PREPROC_MAX_STRING_LEN 256
#define PREPROC_MAX_GROUP_STACK 32
#define PREPROC_MAX_TEMP_OPS 1024
#define PREPROC_MAX_CLASS_NAME 16
#define PREPROC_MAX_NFA_ITEMS 1024
#define PREPROC_MAX_BRANCHES 128
#define PREPROC_NFA_BITSET_WORDS 64
#define PREPROC_MAX_NFA_STATES (PREPROC_NFA_BITSET_WORDS*32)

#define ENUM_PREFIX_ PREPROC_FAIL_
#define ENUM_NAME PreprocFailReason
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(ITEMS_EXCEEDED) \
    X(STATES_EXCEEDED) \
    X(BRANCHES_EXCEEDED) \
    X(DFA_STATES_EXCEEDED) \
    X(NO_BRANCHES)
#include "xenums.c"

enum NfaStateType {
    NFA_STATE_ACCEPT,
    NFA_STATE_LITERAL,
    NFA_STATE_CLASS,
    NFA_STATE_ANY,
    NFA_STATE_SPLIT,
    NFA_STATE_EMPTY,
    NFA_STATE_WORD_BOUNDARY,
    NFA_STATE_NON_WORD_BOUNDARY,
    NFA_STATE_WORD_START,
    NFA_STATE_WORD_END,
};

typedef struct ParsedOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    uint32 mask[META_CHAR_BITMASK_WORDS];
} ParsedOp;

typedef struct NfaState {
    int32 type;
    int32 c;
    uint32 mask[META_CHAR_BITMASK_WORDS];
    int32 next1;
    int32 next2;
} NfaState;

typedef struct NfaItem {
    ParsedOp base_op;
    int32 quant;
    int32 min;
    int32 max;
} NfaItem;

typedef struct DfaSet {
    uint32 bits[PREPROC_NFA_BITSET_WORDS];
    int32 prev_is_w;
} DfaSet;

// Intermediate Representation for the extracted regex
typedef struct ExtractedRegex {
    size_t source_start_offset;
    size_t source_end_offset;
    
    bool is_null_macro;
    bool is_invalid_macro;

    size_t quote_start_offset;
    int32 original_string_length;
    
    ParsedOp temp_ops[PREPROC_MAX_TEMP_OPS];
    int32 temp_ops_count;
    
    bool has_start;
    bool has_end;
    int32 group_counter;
    bool can_be_null;
    uint32 used_ops;
    uint8 fastmap[META_FASTMAP_SIZE];
    bool unsupported;
    
    // Cached op sequence generated in Phase 1
    char op_buffer[PREPROC_OP_BUFFER_SIZE];
} ExtractedRegex;

typedef struct RegexList {
    ExtractedRegex *items;
    int32 count;
    int32 capacity;
} RegexList;

// Inter-phase APIs
RegexList parse_source_code(const char *buffer, size_t source_len);
void generate_source_code(const char *source, size_t source_len, RegexList *list, FILE *out);

#endif /* META_PREPROC_H */
