#if !defined(META_REGEX_H)
#define META_REGEX_H

#include "cbase/util.c"

#define META_ALPHABET_SIZE 256
#define META_MAX_STATIC_DFA_STATES 256
#define META_MAX_OPS 512
#define META_FASTMAP_SIZE 32
#define META_CHAR_BITMASK_WORDS 8
#define META_MAX_LAZY_DFA_STATES 2048
#define META_PC_WORDS (META_MAX_OPS / 32)

#define ENUM_PREFIX_ META_RE_
#define ENUM_NAME MetaRegexFlags
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(YESSUB) \
    X(NOSUB)
#include "xenums.c"

/* TNFA limits */
#define META_MAX_TNFA_TAGS 256
#define META_MAX_TNFA_STATES 1024
#define META_MAX_TNFA_TRANSITIONS 4096
#define META_TNFA_STATE_WORDS ((META_MAX_TNFA_STATES + 31) / 32)

/*
    TNFA tag encoding.

    0   => no tag / plain epsilon
    > 0 => positive tag t
    < 0 => negative tag -t

    Tag ids are 1-based so that 0 can be reserved for "no tag".
*/
#define META_TNFA_TAG_NONE 0
#define META_TNFA_POS_TAG(t) ((int32)(t))
#define META_TNFA_NEG_TAG(t) (-(int32)(t))
#define META_TNFA_TAG_ID(t) ((t) < 0 ? -(t) : (t))
#define META_TNFA_TAG_IS_NEGATIVE(t) ((t) < 0)
#define META_TNFA_TAG_IS_POSITIVE(t) ((t) > 0)

#define ENUM_PREFIX_ META_OP_
#define ENUM_NAME MetaOpType
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(END) \
    X(LITERAL) \
    X(ANY) \
    X(CLASS) \
    X(GROUP_START) \
    X(GROUP_END) \
    X(STAR) \
    X(PLUS) \
    X(OPTIONAL) \
    X(ALTERNATION) \
    X(BOUNDED) \
    X(SPLIT) \
    X(JUMP) \
    X(WORD_START) \
    X(WORD_END) \
    X(WORD_BOUNDARY) \
    X(NON_WORD_BOUNDARY) \
    X(BACKREF)
#include "xenums.c"

typedef struct MetaOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    uint32 mask[META_CHAR_BITMASK_WORDS];
} MetaOp;

typedef struct StaticDfaState {
    uint8 is_accepting[META_ALPHABET_SIZE];
    int32 next[META_ALPHABET_SIZE];
} StaticDfaState;

typedef struct StaticDfa {
    int32 num_states;
    int32 start_state_w;
    int32 start_state_nw;
    StaticDfaState states[];
} StaticDfa;

typedef struct NfaStateSet {
    uint32 bits[META_PC_WORDS];
} NfaStateSet;

typedef struct MetaTnfaStateSet {
    uint32 bits[META_TNFA_STATE_WORDS];
} MetaTnfaStateSet;

enum MetaTnfaTransitionKind {
    META_TNFA_TRANS_EPSILON,
    META_TNFA_TRANS_LITERAL,
    META_TNFA_TRANS_CLASS,
    META_TNFA_TRANS_ANY,
    META_TNFA_TRANS_WORD_START,
    META_TNFA_TRANS_WORD_END,
    META_TNFA_TRANS_WORD_BOUNDARY,
    META_TNFA_TRANS_NON_WORD_BOUNDARY,
};

enum MetaTnfaTagRole {
    META_TNFA_TAG_GENERIC,
    META_TNFA_TAG_GROUP_START,
    META_TNFA_TAG_GROUP_END,
    META_TNFA_TAG_POSIX_AUX,
};

typedef struct MetaTnfaTag {
    /*
        1-based positive tag id.

        Negative tags are not stored as separate tags. They are represented
        on transitions by using -id.
    */
    int32 id;

    /*
        Capture group associated with this tag.

        group == -1 means the tag is internal / not directly tied to a
        public capture group.
    */
    int32 group;

    enum MetaTnfaTagRole role;

    /*
        Nonzero when this tag may have multiple values, for example under
        repetition. TDFA construction can use this to decide whether a tag
        needs history/list storage instead of a single offset.
    */
    int32 is_multivalued;

    /*
        Nonzero when this tag is not tracked directly by TDFA registers.
        In that case its value is derived as:

            tag[id] = tag[fixed_base_tag] + fixed_offset

        fixed_base_tag is a 1-based tag id. 0 means "not fixed".
    */
    int32 fixed_base_tag;
    int32 fixed_offset;
} MetaTnfaTag;

typedef struct MetaTnfaState {
    /*
        Optional adjacency index.

        If transitions are kept grouped by source state, first_transition and
        transition_count give a compact outgoing-transition slice.

        If the builder does not group transitions, set first_transition = -1
        and scan MetaTnfa.transitions by .from.
    */
    int32 first_transition;
    int32 transition_count;
} MetaTnfaState;

typedef struct MetaTnfaTransition {
    enum MetaTnfaTransitionKind kind;

    int32 from;
    int32 to;

    /*
        Used by META_TNFA_TRANS_LITERAL.
    */
    int32 value;

    /*
        Used by META_TNFA_TRANS_CLASS.
    */
    uint32 mask[META_CHAR_BITMASK_WORDS];

    /*
        Used by META_TNFA_TRANS_EPSILON.

        Lower values should be visited first during epsilon-closure
        construction.
    */
    int32 priority;

    /*
        Used by META_TNFA_TRANS_EPSILON.

        0   => untagged epsilon
        > 0 => positive tag
        < 0 => negative tag
    */
    int32 tag;
} MetaTnfaTransition;

typedef struct MetaTnfa {
    int32 num_tags;
    int32 num_states;
    int32 num_transitions;

    int32 start_state;
    int32 final_state;

    MetaTnfaTag *tags;
    MetaTnfaState *states;
    MetaTnfaTransition *transitions;
} MetaTnfa;


/* Single-pass TDFA limits */
#define META_MAX_TDFA_STATES 1024
#define META_MAX_TDFA_TRANSITIONS 65536
#define META_MAX_TDFA_REGOPS 65536
#define META_MAX_TDFA_REGISTERS 65536

/*
    Single-pass TDFA register operation.

    Registers are 1-based. Register 0 means "no register".

    SET_NIL writes -1.
    SET_POS writes the current input offset.
    COPY copies another register.
*/
enum MetaTdfaRegOpKind {
    META_TDFA_REGOP_SET_NIL,
    META_TDFA_REGOP_SET_POS,
    META_TDFA_REGOP_COPY,
};

typedef struct MetaTdfaRegOp {
    enum MetaTdfaRegOpKind kind;
    int32 dst;
    int32 src;
} MetaTdfaRegOp;

typedef struct MetaTdfaTransition {
    int32 from;
    int32 to;

    /* Input byte consumed by this transition. */
    int32 symbol;

    /*
        Word-context selector for the target state.

        -1 => transition does not care about the next byte word class.
         0 => transition is valid when the next byte is not a word char,
              including end of input.
         1 => transition is valid when the next byte is a word char.
    */
    int32 next_is_word;

    int32 first_op;
    int32 op_count;
} MetaTdfaTransition;

typedef struct MetaTdfaState {
    int32 is_accepting;

    int32 first_transition;
    int32 transition_count;

    /* Final quasi-transition operations, executed at end of match. */
    int32 first_final_op;
    int32 final_op_count;
} MetaTdfaState;

typedef struct MetaTdfa {
    int32 num_tags;
    int32 num_states;
    int32 num_transitions;
    int32 num_registers;
    int32 num_ops;

    int32 start_state;

    /*
        Context-specialized start states. The first suffix is previous byte
        wordness, the second suffix is current byte wordness. End-of-input is
        treated as non-word for current byte wordness.
    */
    int32 start_state_nw_nw;
    int32 start_state_nw_w;
    int32 start_state_w_nw;
    int32 start_state_w_w;

    /* Final registers are final_register_base + tag_id - 1. */
    int32 final_register_base;

    /*
        Nonzero if this TDFA has word-context assertions and therefore
        transition lookup must distinguish the next byte wordness.
    */
    int32 uses_context;

    /*
        Optional direct transition lookup table.

        If non-NULL, transition_index is indexed by:
            state*transition_index_stride + byte
        for context-free TDFA, and by:
            state*transition_index_stride + next_is_word*256 + byte
        for context-sensitive TDFA. Values are transition indices, or -1.
    */
    int32 transition_index_stride;
    int32 *transition_index;

    MetaTnfaTag *tags;
    MetaTdfaState *states;
    MetaTdfaTransition *transitions;
    MetaTdfaRegOp *ops;
} MetaTdfa;

typedef struct LazyDfa LazyDfa;

typedef struct MetaRegex {
    char *string;
    MetaOp ops[META_MAX_OPS];
    int32 has_start_anchor;
    int32 has_end_anchor;
    int32 re_nsub;
    int32 can_be_null;
    int32 min_match_len;
    enum MetaRegexFlags flags;
    enum MetaOpType used_ops;
    uint8 fastmap[META_FASTMAP_SIZE];

    /*
        Optional tagged NFA representation.

        NULL means no TNFA was generated/stored for this regex.
    */
    MetaTnfa *tnfa;

    /*
        Optional single-pass tagged DFA generated from the TNFA.

        NULL means TDFA determinization failed or was skipped.
    */
    MetaTdfa *tdfa;

    StaticDfa *static_dfa;
    LazyDfa *lazy_dfa;
} MetaRegex;

typedef struct MatcherFeatures {
    enum MetaOpType supports;
    bool extracts;
} MatcherFeatures;

#define META_R_SELECT(_1, _2, NAME, ...) NAME
#define META_R_1(STR) (&(MetaRegex){ .string = (STR), .flags = META_RE_NONE })
#define META_R_2(STR, FLAGS) (&(MetaRegex){ .string = (STR), .flags = (enum MetaRegexFlags)(FLAGS) })
#define R(...) META_R_SELECT(__VA_ARGS__, META_R_2, META_R_1)(__VA_ARGS__)

#endif /* META_REGEX_H */
