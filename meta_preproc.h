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

/* TNFA build-time limits */
#define PREPROC_MAX_TNFA_TAGS META_MAX_TNFA_TAGS
#define PREPROC_MAX_TNFA_STATES META_MAX_TNFA_STATES
#define PREPROC_MAX_TNFA_TRANSITIONS META_MAX_TNFA_TRANSITIONS

/* TDFA build-time limits */
#define PREPROC_MAX_TDFA_STATES META_MAX_TDFA_STATES
#define PREPROC_MAX_TDFA_TRANSITIONS META_MAX_TDFA_TRANSITIONS
#define PREPROC_MAX_TDFA_REGOPS META_MAX_TDFA_REGOPS
#define PREPROC_MAX_TDFA_REGISTERS META_MAX_TDFA_REGISTERS
#define PREPROC_MAX_TDFA_WORK_CONFIGS \
    (PREPROC_MAX_TNFA_STATES + PREPROC_MAX_TNFA_TRANSITIONS + 1)

#define ENUM_PREFIX_ PREPROC_FAIL_
#define ENUM_NAME PreprocFailReason
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(ITEMS_EXCEEDED) \
    X(STATES_EXCEEDED) \
    X(BRANCHES_EXCEEDED) \
    X(DFA_STATES_EXCEEDED) \
    X(TNFA_TAGS_EXCEEDED) \
    X(TNFA_TRANSITIONS_EXCEEDED) \
    X(TDFA_STATES_EXCEEDED) \
    X(TDFA_TRANSITIONS_EXCEEDED) \
    X(TDFA_REGISTERS_EXCEEDED) \
    X(TDFA_REGOPS_EXCEEDED) \
    X(TDFA_UNSUPPORTED_ASSERTION) \
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

/*
    Build-time TNFA storage.

    This owns fixed-size backing arrays. A generated/runtime MetaTnfa can
    point to these arrays, or the source generator can emit equivalent static
    arrays and a MetaTnfa wrapper.
*/
typedef struct ParsedTnfa {
    MetaTnfaTag tags[PREPROC_MAX_TNFA_TAGS];
    MetaTnfaState states[PREPROC_MAX_TNFA_STATES];
    MetaTnfaTransition transitions[PREPROC_MAX_TNFA_TRANSITIONS];

    int32 num_tags;
    int32 num_states;
    int32 num_transitions;

    int32 start_state;
    int32 final_state;
} ParsedTnfa;

/*
    Build-time single-pass TDFA storage.
*/
typedef struct ParsedTdfa {
    MetaTnfaTag tags[PREPROC_MAX_TNFA_TAGS];
    MetaTdfaState states[PREPROC_MAX_TDFA_STATES];
    MetaTdfaTransition transitions[PREPROC_MAX_TDFA_TRANSITIONS];
    MetaTdfaRegOp ops[PREPROC_MAX_TDFA_REGOPS];

    int32 num_tags;
    int32 num_states;
    int32 num_transitions;
    int32 num_registers;
    int32 num_ops;

    int32 start_state;
    int32 start_state_nw_nw;
    int32 start_state_nw_w;
    int32 start_state_w_nw;
    int32 start_state_w_w;
    int32 final_register_base;
} ParsedTdfa;

// Intermediate Representation for the extracted regex
typedef struct ExtractedRegex {
    int64 source_start_offset;
    int64 source_end_offset;
    
    bool is_null_macro;

    int64 quote_start_offset;
    int32 original_string_length;
    
    ParsedOp temp_ops[PREPROC_MAX_TEMP_OPS];
    int32 temp_ops_count;

    /*
        Optional parsed TNFA.

        NULL means this regex has not been lowered to a TNFA yet, or TNFA
        generation failed/was skipped.
    */
    ParsedTnfa *tnfa;

    /*
        Optional single-pass TDFA generated from the parsed TNFA.

        NULL means TDFA determinization failed or was skipped.
    */
    ParsedTdfa *tdfa;
    
    bool has_start;
    bool has_end;
    int32 group_counter;
    bool can_be_null;
    uint32 used_ops;
    uint8 fastmap[META_FASTMAP_SIZE];
    
    // Cached op sequence generated in Phase 1
    char op_buffer[PREPROC_OP_BUFFER_SIZE];
} ExtractedRegex;

typedef struct RegexList {
    ExtractedRegex *items;
    int32 count;
    int32 capacity;
} RegexList;

// Inter-phase APIs
RegexList parse_source_code(char *buffer, int64 source_len);
void generate_source_code(char *source, int64 source_len, RegexList *list, FILE *out);

#endif /* META_PREPROC_H */
