static char __re_error_msgid[] = {

    gettext_noop("Success") "\0"

    gettext_noop("No match") "\0"

    gettext_noop("Invalid regular expression") "\0"

    gettext_noop("Invalid collation character") "\0"

    gettext_noop("Invalid character class name") "\0"

    gettext_noop("Trailing backslash") "\0"

    gettext_noop("Invalid back reference") "\0"

    gettext_noop("Unmatched [, [^, [:, [., or [=") "\0"

    gettext_noop("Unmatched ( or \\(") "\0"

    gettext_noop("Unmatched \\{") "\0"

    gettext_noop("Invalid content of \\{\\}") "\0"

    gettext_noop("Invalid range end") "\0"

    gettext_noop("Memory exhausted") "\0"

    gettext_noop("Invalid preceding regular expression") "\0"

    gettext_noop("Premature end of regular expression") "\0"

    gettext_noop("Regular expression too big") "\0"

    gettext_noop("Unmatched ) or \\)")};

static int64 __re_error_msgid_idx[]
    = {0,
       (0 + sizeof "Success"),
       ((0 + sizeof "Success") + sizeof "No match"),
       (((0 + sizeof "Success") + sizeof "No match")
        + sizeof "Invalid regular expression"),
       ((((0 + sizeof "Success") + sizeof "No match")
         + sizeof "Invalid regular expression")
        + sizeof "Invalid collation character"),
       (((((0 + sizeof "Success") + sizeof "No match")
          + sizeof "Invalid regular expression")
         + sizeof "Invalid collation character")
        + sizeof "Invalid character class name"),
       ((((((0 + sizeof "Success") + sizeof "No match")
           + sizeof "Invalid regular expression")
          + sizeof "Invalid collation character")
         + sizeof "Invalid character class name")
        + sizeof "Trailing backslash"),
       (((((((0 + sizeof "Success") + sizeof "No match")
            + sizeof "Invalid regular expression")
           + sizeof "Invalid collation character")
          + sizeof "Invalid character class name")
         + sizeof "Trailing backslash")
        + sizeof "Invalid back reference"),
       ((((((((0 + sizeof "Success") + sizeof "No match")
             + sizeof "Invalid regular expression")
            + sizeof "Invalid collation character")
           + sizeof "Invalid character class name")
          + sizeof "Trailing backslash")
         + sizeof "Invalid back reference")
        + sizeof "Unmatched [, [^, [:, [., or [="),
       (((((((((0 + sizeof "Success") + sizeof "No match")
              + sizeof "Invalid regular expression")
             + sizeof "Invalid collation character")
            + sizeof "Invalid character class name")
           + sizeof "Trailing backslash")
          + sizeof "Invalid back reference")
         + sizeof "Unmatched [, [^, [:, [., or [=")
        + sizeof "Unmatched ( or \\("),
       ((((((((((0 + sizeof "Success") + sizeof "No match")
               + sizeof "Invalid regular expression")
              + sizeof "Invalid collation character")
             + sizeof "Invalid character class name")
            + sizeof "Trailing backslash")
           + sizeof "Invalid back reference")
          + sizeof "Unmatched [, [^, [:, [., or [=")
         + sizeof "Unmatched ( or \\(")
        + sizeof "Unmatched \\{"),
       (((((((((((0 + sizeof "Success") + sizeof "No match")
                + sizeof "Invalid regular expression")
               + sizeof "Invalid collation character")
              + sizeof "Invalid character class name")
             + sizeof "Trailing backslash")
            + sizeof "Invalid back reference")
           + sizeof "Unmatched [, [^, [:, [., or [=")
          + sizeof "Unmatched ( or \\(")
         + sizeof "Unmatched \\{")
        + sizeof "Invalid content of \\{\\}"),
       ((((((((((((0 + sizeof "Success") + sizeof "No match")
                 + sizeof "Invalid regular expression")
                + sizeof "Invalid collation character")
               + sizeof "Invalid character class name")
              + sizeof "Trailing backslash")
             + sizeof "Invalid back reference")
            + sizeof "Unmatched [, [^, [:, [., or [=")
           + sizeof "Unmatched ( or \\(")
          + sizeof "Unmatched \\{")
         + sizeof "Invalid content of \\{\\}")
        + sizeof "Invalid range end"),
       (((((((((((((0 + sizeof "Success") + sizeof "No match")
                  + sizeof "Invalid regular expression")
                 + sizeof "Invalid collation character")
                + sizeof "Invalid character class name")
               + sizeof "Trailing backslash")
              + sizeof "Invalid back reference")
             + sizeof "Unmatched [, [^, [:, [., or [=")
            + sizeof "Unmatched ( or \\(")
           + sizeof "Unmatched \\{")
          + sizeof "Invalid content of \\{\\}")
         + sizeof "Invalid range end")
        + sizeof "Memory exhausted"),
       ((((((((((((((0 + sizeof "Success") + sizeof "No match")
                   + sizeof "Invalid regular expression")
                  + sizeof "Invalid collation character")
                 + sizeof "Invalid character class name")
                + sizeof "Trailing backslash")
               + sizeof "Invalid back reference")
              + sizeof "Unmatched [, [^, [:, [., or [=")
             + sizeof "Unmatched ( or \\(")
            + sizeof "Unmatched \\{")
           + sizeof "Invalid content of \\{\\}")
          + sizeof "Invalid range end")
         + sizeof "Memory exhausted")
        + sizeof "Invalid preceding regular expression"),
       (((((((((((((((0 + sizeof "Success") + sizeof "No match")
                    + sizeof "Invalid regular expression")
                   + sizeof "Invalid collation character")
                  + sizeof "Invalid character class name")
                 + sizeof "Trailing backslash")
                + sizeof "Invalid back reference")
               + sizeof "Unmatched [, [^, [:, [., or [=")
              + sizeof "Unmatched ( or \\(")
             + sizeof "Unmatched \\{")
            + sizeof "Invalid content of \\{\\}")
           + sizeof "Invalid range end")
          + sizeof "Memory exhausted")
         + sizeof "Invalid preceding regular expression")
        + sizeof "Premature end of regular expression"),
       ((((((((((((((((0 + sizeof "Success") + sizeof "No match")
                     + sizeof "Invalid regular expression")
                    + sizeof "Invalid collation character")
                   + sizeof "Invalid character class name")
                  + sizeof "Trailing backslash")
                 + sizeof "Invalid back reference")
                + sizeof "Unmatched [, [^, [:, [., or [=")
               + sizeof "Unmatched ( or \\(")
              + sizeof "Unmatched \\{")
             + sizeof "Invalid content of \\{\\}")
            + sizeof "Invalid range end")
           + sizeof "Memory exhausted")
          + sizeof "Invalid preceding regular expression")
         + sizeof "Premature end of regular expression")
        + sizeof "Regular expression too big")};
char *
re_compile_pattern(char *pattern, int64 length,
                   struct re_pattern_buffer *bufp) {
    reg_errcode_t ret;

    bufp->no_sub = !!(re_syntax_options & RE_NO_SUB);

    bufp->newline_anchor = 1;

    ret = re_compile_internal(bufp, pattern, length, re_syntax_options);

    if (!ret) {
        return NULL;
    }
    return gettext(__re_error_msgid + __re_error_msgid_idx[(int32)ret]);
}
weak_alias(__re_compile_pattern, re_compile_pattern)

    reg_syntax_t re_syntax_options;
reg_syntax_t
re_set_syntax(reg_syntax_t syntax) {
    reg_syntax_t ret = re_syntax_options;

    re_syntax_options = syntax;
    return ret;
}
weak_alias(__re_set_syntax, re_set_syntax)

    int32 re_compile_fastmap(struct re_pattern_buffer *bufp) {
    re_dfa_t *dfa = bufp->buffer;
    char *fastmap = bufp->fastmap;

    memset64(fastmap, '\0', sizeof(char)*SBC_MAX);
    re_compile_fastmap_iter(bufp, dfa->init_state, fastmap);
    if (dfa->init_state != dfa->init_state_word) {
        re_compile_fastmap_iter(bufp, dfa->init_state_word, fastmap);
    }
    if (dfa->init_state != dfa->init_state_nl) {
        re_compile_fastmap_iter(bufp, dfa->init_state_nl, fastmap);
    }
    if (dfa->init_state != dfa->init_state_begbuf) {
        re_compile_fastmap_iter(bufp, dfa->init_state_begbuf, fastmap);
    }
    bufp->fastmap_accurate = 1;
    return 0;
}
weak_alias(__re_compile_fastmap, re_compile_fastmap)

    static inline void __attribute__((always_inline))
    re_set_fastmap(char *fastmap, bool icase, int32 ch) {
    fastmap[ch] = 1;
    if (icase) {
        fastmap[tolower(ch)] = 1;
    }
}

static void
re_compile_fastmap_iter(regex_t *bufp, re_dfastate_t *init_state,
                        char *fastmap) {
    re_dfa_t *dfa = bufp->buffer;
    Idx node_cnt;
    bool icase = (dfa->mb_cur_max == 1 && (bufp->syntax & RE_ICASE));
    for (node_cnt = 0; node_cnt < init_state->nodes.nelem; ++node_cnt) {
        Idx node = init_state->nodes.elems[node_cnt];
        re_token_type_t type = dfa->nodes[node].type;

        if (type == CHARACTER) {
            re_set_fastmap(fastmap, icase, dfa->nodes[node].opr.c);
        } else if (type == SIMPLE_BRACKET) {
            int32 i, ch;
            for (i = 0, ch = 0; i < BITSET_WORDS; ++i) {
                int32 j;
                bitset_word_t w = dfa->nodes[node].opr.sbcset[i];
                for (j = 0; j < BITSET_WORD_BITS; ++j, ++ch) {
                    if (w & ((bitset_word_t)1 << j)) {
                        re_set_fastmap(fastmap, icase, ch);
                    }
                }
            }
        } else if (type == OP_PERIOD

                   || type == END_OF_RE) {
            memset64(fastmap, '\1', sizeof(char)*SBC_MAX);
            if (type == END_OF_RE) {
                bufp->can_be_null = 1;
            }
            return;
        }
    }
}
int32
regcomp(regex_t *__restrict preg, char *__restrict pattern, int32 cflags) {
    reg_errcode_t ret;
    reg_syntax_t syntax = ((cflags & REG_EXTENDED) ? RE_SYNTAX_POSIX_EXTENDED
                                                   : RE_SYNTAX_POSIX_BASIC);

    preg->buffer = NULL;
    preg->allocated = 0;
    preg->used = 0;

    preg->fastmap = re_malloc(char, SBC_MAX);
    if (__glibc_unlikely(preg->fastmap == NULL)) {
        return REG_ESPACE;
    }

    syntax |= (cflags & REG_ICASE) ? RE_ICASE : 0;

    if (cflags & REG_NEWLINE) {

        syntax &= ~RE_DOT_NEWLINE;
        syntax |= RE_HAT_LISTS_NOT_NEWLINE;

        preg->newline_anchor = 1;
    } else {
        preg->newline_anchor = 0;
    }
    preg->no_sub = !!(cflags & REG_NOSUB);
    preg->translate = NULL;

    ret = re_compile_internal(preg, pattern, strlen32(pattern), syntax);

    if (ret == REG_ERPAREN) {
        ret = REG_EPAREN;
    }

    if (__glibc_likely(ret == REG_NOERROR)) {

        (void)re_compile_fastmap(preg);
    } else {

        re_free(preg->fastmap);
        preg->fastmap = NULL;
    }

    return (int32)ret;
}
libc_hidden_def(__regcomp) weak_alias(__regcomp, regcomp)

    int64 regerror(int32 errcode, regex_t *__restrict preg,
                   char *__restrict errbuf, int64 errbuf_size) {
    char *msg;
    int64 msg_size;
    int32 nerrcodes
        = sizeof __re_error_msgid_idx / sizeof __re_error_msgid_idx[0];

    if (__glibc_unlikely(errcode < 0 || errcode >= nerrcodes)) {

        abort();
    }

    msg = gettext(__re_error_msgid + __re_error_msgid_idx[errcode]);

    msg_size = strlen32(msg) + 1;

    if (__glibc_likely(errbuf_size != 0)) {
        int64 cpy_size = msg_size;
        if (__glibc_unlikely(msg_size > errbuf_size)) {
            cpy_size = errbuf_size - 1;
            errbuf[cpy_size] = '\0';
        }
        memcpy64(errbuf, msg, cpy_size);
    }

    return msg_size;
}
weak_alias(__regerror, regerror) static void free_dfa_content(re_dfa_t *dfa) {
    Idx i, j;

    if (dfa->nodes) {
        for (i = 0; i < dfa->nodes_len; ++i) {
            free_token(dfa->nodes + i);
        }
    }
    re_free(dfa->nexts);
    for (i = 0; i < dfa->nodes_len; ++i) {
        if (dfa->eclosures != NULL) {
            re_node_set_free(dfa->eclosures + i);
        }
        if (dfa->inveclosures != NULL) {
            re_node_set_free(dfa->inveclosures + i);
        }
        if (dfa->edests != NULL) {
            re_node_set_free(dfa->edests + i);
        }
    }
    re_free(dfa->edests);
    re_free(dfa->eclosures);
    re_free(dfa->inveclosures);
    re_free(dfa->nodes);

    if (dfa->state_table) {
        for (i = 0; i <= dfa->state_hash_mask; ++i) {
            struct re_state_table_entry *entry = dfa->state_table + i;
            for (j = 0; j < entry->num; ++j) {
                re_dfastate_t *state = entry->array[j];
                free_state(state);
            }
            re_free(entry->array);
        }
    }
    re_free(dfa->state_table);

    re_free(dfa->subexp_map);

    re_free(dfa);
}

void
regfree(regex_t *preg) {
    re_dfa_t *dfa = preg->buffer;
    if (__glibc_likely(dfa != NULL)) {
        lock_fini(dfa->lock);
        free_dfa_content(dfa);
    }
    preg->buffer = NULL;
    preg->allocated = 0;

    re_free(preg->fastmap);
    preg->fastmap = NULL;

    re_free(preg->translate);
    preg->translate = NULL;
}
libc_hidden_def(__regfree) weak_alias(__regfree, regfree)

    static struct re_pattern_buffer re_comp_buf;

char *

    weak_function

    re_comp(char *s) {
    reg_errcode_t ret;
    char *fastmap;

    if (!s) {
        if (!re_comp_buf.buffer) {
            return gettext("No previous regular expression");
        }
        return NULL;
    }

    if (re_comp_buf.buffer) {
        fastmap = re_comp_buf.fastmap;
        re_comp_buf.fastmap = NULL;
        __regfree(&re_comp_buf);
        memset64(&re_comp_buf, '\0', sizeof(re_comp_buf));
        re_comp_buf.fastmap = fastmap;
    }

    if (re_comp_buf.fastmap == NULL) {
        re_comp_buf.fastmap = re_malloc(char, SBC_MAX);
        if (re_comp_buf.fastmap == NULL) {
            return (char *)gettext(__re_error_msgid
                                   + __re_error_msgid_idx[(int32)REG_ESPACE]);
        }
    }

    re_comp_buf.newline_anchor = 1;

    ret = re_compile_internal(&re_comp_buf, s, strlen32(s), re_syntax_options);

    if (!ret) {
        return NULL;
    }

    return (char *)gettext(__re_error_msgid + __re_error_msgid_idx[(int32)ret]);
}

void
__libc_regcomp_freemem(void) {
    __regfree(&re_comp_buf);
}
static reg_errcode_t
re_compile_internal(regex_t *preg, char *pattern, int64 length,
                    reg_syntax_t syntax) {
    reg_errcode_t err = REG_NOERROR;
    re_dfa_t *dfa;
    re_string_t regexp;

    preg->fastmap_accurate = 0;
    preg->syntax = syntax;
    preg->not_bol = preg->not_eol = 0;
    preg->used = 0;
    preg->re_nsub = 0;
    preg->can_be_null = 0;
    preg->regs_allocated = REGS_UNALLOCATED;

    dfa = preg->buffer;
    if (__glibc_unlikely(preg->allocated < sizeof(re_dfa_t))) {

        dfa = re_realloc(preg->buffer, re_dfa_t, 1);
        if (dfa == NULL) {
            return REG_ESPACE;
        }
        preg->allocated = sizeof(re_dfa_t);
        preg->buffer = dfa;
    }
    preg->used = sizeof(re_dfa_t);

    err = init_dfa(dfa, length);
    if (__glibc_unlikely(err == REG_NOERROR && lock_init(dfa->lock) != 0)) {
        err = REG_ESPACE;
    }
    if (__glibc_unlikely(err != REG_NOERROR)) {
        free_dfa_content(dfa);
        preg->buffer = NULL;
        preg->allocated = 0;
        return err;
    }

    err = re_string_construct(&regexp, pattern, length, preg->translate,
                              (syntax & RE_ICASE) != 0, dfa);
    if (__glibc_unlikely(err != REG_NOERROR)) {
    re_compile_internal_free_return:
        free_workarea_compile(preg);
        re_string_destruct(&regexp);
        lock_fini(dfa->lock);
        free_dfa_content(dfa);
        preg->buffer = NULL;
        preg->allocated = 0;
        return err;
    }

    preg->re_nsub = 0;
    dfa->str_tree = parse(&regexp, preg, syntax, &err);
    if (__glibc_unlikely(dfa->str_tree == NULL)) {
        goto re_compile_internal_free_return;
    }

    err = analyze(preg);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        goto re_compile_internal_free_return;
    }
    err = create_initial_state(dfa);

    free_workarea_compile(preg);
    re_string_destruct(&regexp);

    if (__glibc_unlikely(err != REG_NOERROR)) {
        lock_fini(dfa->lock);
        free_dfa_content(dfa);
        preg->buffer = NULL;
        preg->allocated = 0;
    }

    return err;
}

static reg_errcode_t
init_dfa(re_dfa_t *dfa, int64 pat_len) {
    int64 table_size;

    int64 max_i18n_object_size = 0;

    int64 max_object_size
        = MAX(sizeof(struct re_state_table_entry),
              MAX(sizeof(re_token_t),
                  MAX(sizeof(re_node_set),
                      MAX(sizeof(regmatch_t), max_i18n_object_size))));

    memset64(dfa, '\0', sizeof(re_dfa_t));

    dfa->str_tree_storage_idx = BIN_TREE_STORAGE_SIZE;

    if (__glibc_unlikely(MIN(IDX_MAX, SIZE_MAX / max_object_size) / 2
                         <= pat_len)) {
        return REG_ESPACE;
    }

    dfa->nodes_alloc = pat_len + 1;
    dfa->nodes = re_malloc(re_token_t, dfa->nodes_alloc);

    for (table_size = 1;; table_size <<= 1) {
        if (table_size > pat_len) {
            break;
        }
    }

    dfa->state_table = calloc(sizeof(struct re_state_table_entry), table_size);
    dfa->state_hash_mask = table_size - 1;

    dfa->mb_cur_max = MB_CUR_MAX;

    if (dfa->mb_cur_max == 6
        && strcmp(_NL_CURRENT(LC_CTYPE, _NL_CTYPE_CODESET_NAME), "UTF-8") == 0) {
        dfa->is_utf8 = 1;
    }
    dfa->map_notascii
        = (_NL_CURRENT_WORD(LC_CTYPE, _NL_CTYPE_MAP_TO_NONASCII) != 0);
    if (__glibc_unlikely(dfa->nodes == NULL || dfa->state_table == NULL)) {
        return REG_ESPACE;
    }
    return REG_NOERROR;
}

static void
init_word_char(re_dfa_t *dfa) {
    int32 i = 0;
    int32 j;
    int32 ch = 0;
    dfa->word_ops_used = 1;
    if (__glibc_likely(dfa->map_notascii == 0)) {

        bitset_word_t bits0 = 0x00000000;
        bitset_word_t bits1 = 0x03ff0000;
        bitset_word_t bits2 = 0x87fffffe;
        bitset_word_t bits3 = 0x07fffffe;
        if (BITSET_WORD_BITS == 64) {

            dfa->word_char[0] = bits1 << 31 << 1 | bits0;
            dfa->word_char[1] = bits3 << 31 << 1 | bits2;
            i = 2;
        } else if (BITSET_WORD_BITS == 32) {
            dfa->word_char[0] = bits0;
            dfa->word_char[1] = bits1;
            dfa->word_char[2] = bits2;
            dfa->word_char[3] = bits3;
            i = 4;
        } else {
            goto general_case;
        }
        ch = 128;

        if (__glibc_likely(dfa->is_utf8)) {
            memset64(&dfa->word_char[i], '\0', (SBC_MAX - ch) / 8);
            return;
        }
    }

general_case:
    for (; i < BITSET_WORDS; ++i) {
        for (j = 0; j < BITSET_WORD_BITS; ++j, ++ch) {
            if (isalnum(ch) || ch == '_') {
                dfa->word_char[i] |= (bitset_word_t)1 << j;
            }
        }
    }
}

static void
free_workarea_compile(regex_t *preg) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_storage_t *storage, *next;
    for (storage = dfa->str_tree_storage; storage; storage = next) {
        next = storage->next;
        re_free(storage);
    }
    dfa->str_tree_storage = NULL;
    dfa->str_tree_storage_idx = BIN_TREE_STORAGE_SIZE;
    dfa->str_tree = NULL;
    re_free(dfa->org_indices);
    dfa->org_indices = NULL;
}

static reg_errcode_t
create_initial_state(re_dfa_t *dfa) {
    Idx first, i;
    reg_errcode_t err;
    re_node_set init_nodes;

    first = dfa->str_tree->first->node_idx;
    dfa->init_node = first;
    err = re_node_set_init_copy(&init_nodes, dfa->eclosures + first);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        return err;
    }

    if (dfa->nbackref > 0) {
        for (i = 0; i < init_nodes.nelem; ++i) {
            Idx node_idx = init_nodes.elems[i];
            re_token_type_t type = dfa->nodes[node_idx].type;

            Idx clexp_idx;
            if (type != OP_BACK_REF) {
                continue;
            }
            for (clexp_idx = 0; clexp_idx < init_nodes.nelem; ++clexp_idx) {
                re_token_t *clexp_node;
                clexp_node = dfa->nodes + init_nodes.elems[clexp_idx];
                if (clexp_node->type == OP_CLOSE_SUBEXP
                    && clexp_node->opr.idx == dfa->nodes[node_idx].opr.idx) {
                    break;
                }
            }
            if (clexp_idx == init_nodes.nelem) {
                continue;
            }

            if (type == OP_BACK_REF) {
                Idx dest_idx = dfa->edests[node_idx].elems[0];
                if (!re_node_set_contains(&init_nodes, dest_idx)) {
                    reg_errcode_t merge_err = re_node_set_merge(
                        &init_nodes, dfa->eclosures + dest_idx);
                    if (merge_err != REG_NOERROR) {
                        return merge_err;
                    }
                    i = 0;
                }
            }
        }
    }

    dfa->init_state = re_acquire_state_context(&err, dfa, &init_nodes, 0);

    if (__glibc_unlikely(dfa->init_state == NULL)) {
        return err;
    }
    if (dfa->init_state->has_constraint) {
        dfa->init_state_word
            = re_acquire_state_context(&err, dfa, &init_nodes, CONTEXT_WORD);
        dfa->init_state_nl
            = re_acquire_state_context(&err, dfa, &init_nodes, CONTEXT_NEWLINE);
        dfa->init_state_begbuf = re_acquire_state_context(
            &err, dfa, &init_nodes, CONTEXT_NEWLINE | CONTEXT_BEGBUF);
        if (__glibc_unlikely(dfa->init_state_word == NULL
                             || dfa->init_state_nl == NULL
                             || dfa->init_state_begbuf == NULL)) {
            return err;
        }
    } else {
        dfa->init_state_word = dfa->init_state_nl = dfa->init_state_begbuf
            = dfa->init_state;
    }

    re_node_set_free(&init_nodes);
    return REG_NOERROR;
}
static reg_errcode_t
analyze(regex_t *preg) {
    re_dfa_t *dfa = preg->buffer;
    reg_errcode_t ret;

    dfa->nexts = re_malloc(Idx, dfa->nodes_alloc);
    dfa->org_indices = re_malloc(Idx, dfa->nodes_alloc);
    dfa->edests = re_malloc(re_node_set, dfa->nodes_alloc);
    dfa->eclosures = re_malloc(re_node_set, dfa->nodes_alloc);
    if (__glibc_unlikely(dfa->nexts == NULL || dfa->org_indices == NULL
                         || dfa->edests == NULL || dfa->eclosures == NULL)) {
        return REG_ESPACE;
    }

    dfa->subexp_map = re_malloc(Idx, preg->re_nsub);
    if (dfa->subexp_map != NULL) {
        Idx i;
        for (i = 0; i < preg->re_nsub; i++) {
            dfa->subexp_map[i] = i;
        }
        preorder(dfa->str_tree, optimize_subexps, dfa);
        for (i = 0; i < preg->re_nsub; i++) {
            if (dfa->subexp_map[i] != i) {
                break;
            }
        }
        if (i == preg->re_nsub) {
            re_free(dfa->subexp_map);
            dfa->subexp_map = NULL;
        }
    }

    ret = postorder(dfa->str_tree, lower_subexps, preg);
    if (__glibc_unlikely(ret != REG_NOERROR)) {
        return ret;
    }
    ret = postorder(dfa->str_tree, calc_first, dfa);
    if (__glibc_unlikely(ret != REG_NOERROR)) {
        return ret;
    }
    preorder(dfa->str_tree, calc_next, dfa);
    ret = preorder(dfa->str_tree, link_nfa_nodes, dfa);
    if (__glibc_unlikely(ret != REG_NOERROR)) {
        return ret;
    }
    ret = calc_eclosure(dfa);
    if (__glibc_unlikely(ret != REG_NOERROR)) {
        return ret;
    }

    if ((!preg->no_sub && preg->re_nsub > 0 && dfa->has_plural_match)
        || dfa->nbackref) {
        dfa->inveclosures = re_malloc(re_node_set, dfa->nodes_len);
        if (__glibc_unlikely(dfa->inveclosures == NULL)) {
            return REG_ESPACE;
        }
        ret = calc_inveclosure(dfa);
    }

    return ret;
}

static reg_errcode_t
postorder(bin_tree_t *root, reg_errcode_t(fn(void *, bin_tree_t *)),
          void *extra) {
    bin_tree_t *node, *prev;

    for (node = root;;) {

        while (node->left || node->right) {
            if (node->left) {
                node = node->left;
            } else {
                node = node->right;
            }
        }

        do {
            reg_errcode_t err = fn(extra, node);
            if (__glibc_unlikely(err != REG_NOERROR)) {
                return err;
            }
            if (node->parent == NULL) {
                return REG_NOERROR;
            }
            prev = node;
            node = node->parent;
        }

        while (node->right == prev || node->right == NULL);
        node = node->right;
    }
}

static reg_errcode_t
preorder(bin_tree_t *root, reg_errcode_t(fn(void *, bin_tree_t *)),
         void *extra) {
    bin_tree_t *node;

    for (node = root;;) {
        reg_errcode_t err = fn(extra, node);
        if (__glibc_unlikely(err != REG_NOERROR)) {
            return err;
        }

        if (node->left) {
            node = node->left;
        } else {
            bin_tree_t *prev = NULL;
            while (node->right == prev || node->right == NULL) {
                prev = node;
                node = node->parent;
                if (!node) {
                    return REG_NOERROR;
                }
            }
            node = node->right;
        }
    }
}

static reg_errcode_t
optimize_subexps(void *extra, bin_tree_t *node) {
    re_dfa_t *dfa = (re_dfa_t *)extra;

    if (node->token.type == OP_BACK_REF && dfa->subexp_map) {
        int32 idx = node->token.opr.idx;
        node->token.opr.idx = dfa->subexp_map[idx];
        dfa->used_bkref_map |= 1 << node->token.opr.idx;
    }

    else if (node->token.type == SUBEXP && node->left
             && node->left->token.type == SUBEXP) {
        Idx other_idx = node->left->token.opr.idx;

        node->left = node->left->left;
        if (node->left) {
            node->left->parent = node;
        }

        dfa->subexp_map[other_idx] = dfa->subexp_map[node->token.opr.idx];
        if (other_idx < BITSET_WORD_BITS) {
            dfa->used_bkref_map &= ~((bitset_word_t)1 << other_idx);
        }
    }

    return REG_NOERROR;
}

static reg_errcode_t
lower_subexps(void *extra, bin_tree_t *node) {
    regex_t *preg = (regex_t *)extra;
    reg_errcode_t err = REG_NOERROR;

    if (node->left && node->left->token.type == SUBEXP) {
        node->left = lower_subexp(&err, preg, node->left);
        if (node->left) {
            node->left->parent = node;
        }
    }
    if (node->right && node->right->token.type == SUBEXP) {
        node->right = lower_subexp(&err, preg, node->right);
        if (node->right) {
            node->right->parent = node;
        }
    }

    return err;
}

static bin_tree_t *
lower_subexp(reg_errcode_t *err, regex_t *preg, bin_tree_t *node) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *body = node->left;
    bin_tree_t *op, *cls, *tree1, *tree;

    if (preg->no_sub

        && node->left != NULL
        && (node->token.opr.idx >= BITSET_WORD_BITS
            || !(dfa->used_bkref_map
                 & ((bitset_word_t)1 << node->token.opr.idx)))) {
        return node->left;
    }

    op = create_tree(dfa, NULL, NULL, OP_OPEN_SUBEXP);
    cls = create_tree(dfa, NULL, NULL, OP_CLOSE_SUBEXP);
    tree1 = body ? create_tree(dfa, body, cls, CONCAT) : cls;
    tree = create_tree(dfa, op, tree1, CONCAT);
    if (__glibc_unlikely(tree == NULL || tree1 == NULL || op == NULL
                         || cls == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }

    op->token.opr.idx = cls->token.opr.idx = node->token.opr.idx;
    op->token.opt_subexp = cls->token.opt_subexp = node->token.opt_subexp;
    return tree;
}

static reg_errcode_t
calc_first(void *extra, bin_tree_t *node) {
    re_dfa_t *dfa = (re_dfa_t *)extra;
    if (node->token.type == CONCAT) {
        node->first = node->left->first;
        node->node_idx = node->left->node_idx;
    } else {
        node->first = node;
        node->node_idx = re_dfa_add_node(dfa, node->token);
        if (__glibc_unlikely(node->node_idx == -1)) {
            return REG_ESPACE;
        }
        if (node->token.type == ANCHOR) {
            dfa->nodes[node->node_idx].constraint = node->token.opr.ctx_type;
        }
    }
    return REG_NOERROR;
}

static reg_errcode_t
calc_next(void *extra, bin_tree_t *node) {
    switch (node->token.type) {
    case OP_DUP_ASTERISK:
        node->left->next = node;
        break;
    case CONCAT:
        node->left->next = node->right->first;
        node->right->next = node->next;
        break;
    default:
        if (node->left) {
            node->left->next = node->next;
        }
        if (node->right) {
            node->right->next = node->next;
        }
        break;
    }
    return REG_NOERROR;
}

static reg_errcode_t
link_nfa_nodes(void *extra, bin_tree_t *node) {
    re_dfa_t *dfa = (re_dfa_t *)extra;
    Idx idx = node->node_idx;
    reg_errcode_t err = REG_NOERROR;

    switch (node->token.type) {
    case CONCAT:
        break;

    case END_OF_RE:
        DEBUG_ASSERT(node->next == NULL);
        break;

    case OP_DUP_ASTERISK:
    case OP_ALT: {
        Idx left, right;
        dfa->has_plural_match = 1;
        if (node->left != NULL) {
            left = node->left->first->node_idx;
        } else {
            left = node->next->node_idx;
        }
        if (node->right != NULL) {
            right = node->right->first->node_idx;
        } else {
            right = node->next->node_idx;
        }
        DEBUG_ASSERT(left > -1);
        DEBUG_ASSERT(right > -1);
        err = re_node_set_init_2(dfa->edests + idx, left, right);
    } break;

    case ANCHOR:
    case OP_OPEN_SUBEXP:
    case OP_CLOSE_SUBEXP:
        err = re_node_set_init_1(dfa->edests + idx, node->next->node_idx);
        break;

    case OP_BACK_REF:
        dfa->nexts[idx] = node->next->node_idx;
        if (node->token.type == OP_BACK_REF) {
            err = re_node_set_init_1(dfa->edests + idx, dfa->nexts[idx]);
        }
        break;

    default:
        DEBUG_ASSERT(!IS_EPSILON_NODE(node->token.type));
        dfa->nexts[idx] = node->next->node_idx;
        break;
    }

    return err;
}

static reg_errcode_t
duplicate_node_closure(re_dfa_t *dfa, Idx top_org_node, Idx top_clone_node,
                       Idx root_node, uint32 init_constraint) {
    Idx org_node, clone_node;
    bool ok;
    uint32 constraint = init_constraint;
    for (org_node = top_org_node, clone_node = top_clone_node;;) {
        Idx org_dest, clone_dest;
        if (dfa->nodes[org_node].type == OP_BACK_REF) {

            org_dest = dfa->nexts[org_node];
            re_node_set_empty(dfa->edests + clone_node);
            clone_dest = duplicate_node(dfa, org_dest, constraint);
            if (__glibc_unlikely(clone_dest == -1)) {
                return REG_ESPACE;
            }
            dfa->nexts[clone_node] = dfa->nexts[org_node];
            ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
            if (__glibc_unlikely(!ok)) {
                return REG_ESPACE;
            }
        } else if (dfa->edests[org_node].nelem == 0) {

            dfa->nexts[clone_node] = dfa->nexts[org_node];
            break;
        } else if (dfa->edests[org_node].nelem == 1) {

            org_dest = dfa->edests[org_node].elems[0];
            re_node_set_empty(dfa->edests + clone_node);

            if (org_node == root_node && clone_node != org_node) {
                ok = re_node_set_insert(dfa->edests + clone_node, org_dest);
                if (__glibc_unlikely(!ok)) {
                    return REG_ESPACE;
                }
                break;
            }

            constraint |= dfa->nodes[org_node].constraint;
            clone_dest = duplicate_node(dfa, org_dest, constraint);
            if (__glibc_unlikely(clone_dest == -1)) {
                return REG_ESPACE;
            }
            ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
            if (__glibc_unlikely(!ok)) {
                return REG_ESPACE;
            }
        } else {

            org_dest = dfa->edests[org_node].elems[0];
            re_node_set_empty(dfa->edests + clone_node);

            clone_dest = search_duplicated_node(dfa, org_dest, constraint);
            if (clone_dest == -1) {

                reg_errcode_t err;
                clone_dest = duplicate_node(dfa, org_dest, constraint);
                if (__glibc_unlikely(clone_dest == -1)) {
                    return REG_ESPACE;
                }
                ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
                if (__glibc_unlikely(!ok)) {
                    return REG_ESPACE;
                }
                err = duplicate_node_closure(dfa, org_dest, clone_dest,
                                             root_node, constraint);
                if (__glibc_unlikely(err != REG_NOERROR)) {
                    return err;
                }
            } else {

                ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
                if (__glibc_unlikely(!ok)) {
                    return REG_ESPACE;
                }
            }

            org_dest = dfa->edests[org_node].elems[1];
            clone_dest = duplicate_node(dfa, org_dest, constraint);
            if (__glibc_unlikely(clone_dest == -1)) {
                return REG_ESPACE;
            }
            ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
            if (__glibc_unlikely(!ok)) {
                return REG_ESPACE;
            }
        }
        org_node = org_dest;
        clone_node = clone_dest;
    }
    return REG_NOERROR;
}

static Idx
search_duplicated_node(re_dfa_t *dfa, Idx org_node, uint32 constraint) {
    Idx idx;
    for (idx = dfa->nodes_len - 1; dfa->nodes[idx].duplicated && idx > 0;
         --idx) {
        if (org_node == dfa->org_indices[idx]
            && constraint == dfa->nodes[idx].constraint) {
            return idx;
        }
    }
    return -1;
}

static Idx
duplicate_node(re_dfa_t *dfa, Idx org_idx, uint32 constraint) {
    Idx dup_idx = re_dfa_add_node(dfa, dfa->nodes[org_idx]);
    if (__glibc_likely(dup_idx != -1)) {
        dfa->nodes[dup_idx].constraint = constraint;
        dfa->nodes[dup_idx].constraint |= dfa->nodes[org_idx].constraint;
        dfa->nodes[dup_idx].duplicated = 1;

        dfa->org_indices[dup_idx] = org_idx;
    }
    return dup_idx;
}

static reg_errcode_t
calc_inveclosure(re_dfa_t *dfa) {
    Idx src, idx;
    bool ok;
    for (idx = 0; idx < dfa->nodes_len; ++idx) {
        re_node_set_init_empty(dfa->inveclosures + idx);
    }

    for (src = 0; src < dfa->nodes_len; ++src) {
        Idx *elems = dfa->eclosures[src].elems;
        for (idx = 0; idx < dfa->eclosures[src].nelem; ++idx) {
            ok = re_node_set_insert_last(dfa->inveclosures + elems[idx], src);
            if (__glibc_unlikely(!ok)) {
                return REG_ESPACE;
            }
        }
    }

    return REG_NOERROR;
}

static reg_errcode_t
calc_eclosure(re_dfa_t *dfa) {
    Idx node_idx;
    bool incomplete;
    DEBUG_ASSERT(dfa->nodes_len > 0);
    incomplete = false;

    for (node_idx = 0;; ++node_idx) {
        reg_errcode_t err;
        re_node_set eclosure_elem;
        if (node_idx == dfa->nodes_len) {
            if (!incomplete) {
                break;
            }
            incomplete = false;
            node_idx = 0;
        }

        DEBUG_ASSERT(dfa->eclosures[node_idx].nelem != -1);

        if (dfa->eclosures[node_idx].nelem != 0) {
            continue;
        }

        err = calc_eclosure_iter(&eclosure_elem, dfa, node_idx, true);
        if (__glibc_unlikely(err != REG_NOERROR)) {
            return err;
        }

        if (dfa->eclosures[node_idx].nelem == 0) {
            incomplete = true;
            re_node_set_free(&eclosure_elem);
        }
    }
    return REG_NOERROR;
}

static reg_errcode_t
calc_eclosure_iter(re_node_set *new_set, re_dfa_t *dfa, Idx node, bool root) {
    reg_errcode_t err;
    Idx i;
    re_node_set eclosure;
    bool incomplete = false;
    err = re_node_set_alloc(&eclosure, dfa->edests[node].nelem + 1);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        return err;
    }

    eclosure.elems[eclosure.nelem++] = node;

    dfa->eclosures[node].nelem = -1;

    if (dfa->nodes[node].constraint && dfa->edests[node].nelem
        && !dfa->nodes[dfa->edests[node].elems[0]].duplicated) {
        err = duplicate_node_closure(dfa, node, node, node,
                                     dfa->nodes[node].constraint);
        if (__glibc_unlikely(err != REG_NOERROR)) {
            return err;
        }
    }

    if (IS_EPSILON_NODE(dfa->nodes[node].type)) {
        for (i = 0; i < dfa->edests[node].nelem; ++i) {
            re_node_set eclosure_elem;
            Idx edest = dfa->edests[node].elems[i];

            if (dfa->eclosures[edest].nelem == -1) {
                incomplete = true;
                continue;
            }

            if (dfa->eclosures[edest].nelem == 0) {
                err = calc_eclosure_iter(&eclosure_elem, dfa, edest, false);
                if (__glibc_unlikely(err != REG_NOERROR)) {
                    return err;
                }
            } else {
                eclosure_elem = dfa->eclosures[edest];
            }

            err = re_node_set_merge(&eclosure, &eclosure_elem);
            if (__glibc_unlikely(err != REG_NOERROR)) {
                return err;
            }

            if (dfa->eclosures[edest].nelem == 0) {
                incomplete = true;
                re_node_set_free(&eclosure_elem);
            }
        }
    }

    if (incomplete && !root) {
        dfa->eclosures[node].nelem = 0;
    } else {
        dfa->eclosures[node] = eclosure;
    }
    *new_set = eclosure;
    return REG_NOERROR;
}

static void
fetch_token(re_token_t *result, re_string_t *input, reg_syntax_t syntax) {
    re_string_skip_bytes(input, peek_token(result, input, syntax));
}

static int32
peek_token(re_token_t *token, re_string_t *input, reg_syntax_t syntax) {
    uint8 c;

    if (re_string_eoi(input)) {
        token->type = END_OF_RE;
        return 0;
    }

    c = re_string_peek_byte(input, 0);
    token->opr.c = c;

    token->word_char = 0;
    if (c == '\\') {
        uint8 c2;
        if (re_string_cur_idx(input) + 1 >= re_string_length(input)) {
            token->type = BACK_SLASH;
            return 1;
        }

        c2 = re_string_peek_byte_case(input, 1);
        token->opr.c = c2;
        token->type = CHARACTER;

        token->word_char = IS_WORD_CHAR(c2) != 0;

        switch (c2) {
        case '|':
            if (!(syntax & RE_LIMITED_OPS) && !(syntax & RE_NO_BK_VBAR)) {
                token->type = OP_ALT;
            }
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (!(syntax & RE_NO_BK_REFS)) {
                token->type = OP_BACK_REF;
                token->opr.idx = c2 - '1';
            }
            break;
        case '<':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = ANCHOR;
                token->opr.ctx_type = WORD_FIRST;
            }
            break;
        case '>':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = ANCHOR;
                token->opr.ctx_type = WORD_LAST;
            }
            break;
        case 'b':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = ANCHOR;
                token->opr.ctx_type = WORD_DELIM;
            }
            break;
        case 'B':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = ANCHOR;
                token->opr.ctx_type = NOT_WORD_DELIM;
            }
            break;
        case 'w':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = OP_WORD;
            }
            break;
        case 'W':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = OP_NOTWORD;
            }
            break;
        case 's':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = OP_SPACE;
            }
            break;
        case 'S':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = OP_NOTSPACE;
            }
            break;
        case '`':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = ANCHOR;
                token->opr.ctx_type = BUF_FIRST;
            }
            break;
        case '\'':
            if (!(syntax & RE_NO_GNU_OPS)) {
                token->type = ANCHOR;
                token->opr.ctx_type = BUF_LAST;
            }
            break;
        case '(':
            if (!(syntax & RE_NO_BK_PARENS)) {
                token->type = OP_OPEN_SUBEXP;
            }
            break;
        case ')':
            if (!(syntax & RE_NO_BK_PARENS)) {
                token->type = OP_CLOSE_SUBEXP;
            }
            break;
        case '+':
            if (!(syntax & RE_LIMITED_OPS) && (syntax & RE_BK_PLUS_QM)) {
                token->type = OP_DUP_PLUS;
            }
            break;
        case '?':
            if (!(syntax & RE_LIMITED_OPS) && (syntax & RE_BK_PLUS_QM)) {
                token->type = OP_DUP_QUESTION;
            }
            break;
        case '{':
            if ((syntax & RE_INTERVALS) && (!(syntax & RE_NO_BK_BRACES))) {
                token->type = OP_OPEN_DUP_NUM;
            }
            break;
        case '}':
            if ((syntax & RE_INTERVALS) && (!(syntax & RE_NO_BK_BRACES))) {
                token->type = OP_CLOSE_DUP_NUM;
            }
            break;
        default:
            break;
        }
        return 2;
    }

    token->type = CHARACTER;

    token->word_char = IS_WORD_CHAR(token->opr.c);

    switch (c) {
    case '\n':
        if (syntax & RE_NEWLINE_ALT) {
            token->type = OP_ALT;
        }
        break;
    case '|':
        if (!(syntax & RE_LIMITED_OPS) && (syntax & RE_NO_BK_VBAR)) {
            token->type = OP_ALT;
        }
        break;
    case '*':
        token->type = OP_DUP_ASTERISK;
        break;
    case '+':
        if (!(syntax & RE_LIMITED_OPS) && !(syntax & RE_BK_PLUS_QM)) {
            token->type = OP_DUP_PLUS;
        }
        break;
    case '?':
        if (!(syntax & RE_LIMITED_OPS) && !(syntax & RE_BK_PLUS_QM)) {
            token->type = OP_DUP_QUESTION;
        }
        break;
    case '{':
        if ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES)) {
            token->type = OP_OPEN_DUP_NUM;
        }
        break;
    case '}':
        if ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES)) {
            token->type = OP_CLOSE_DUP_NUM;
        }
        break;
    case '(':
        if (syntax & RE_NO_BK_PARENS) {
            token->type = OP_OPEN_SUBEXP;
        }
        break;
    case ')':
        if (syntax & RE_NO_BK_PARENS) {
            token->type = OP_CLOSE_SUBEXP;
        }
        break;
    case '[':
        token->type = OP_OPEN_BRACKET;
        break;
    case '.':
        token->type = OP_PERIOD;
        break;
    case '^':
        if (!(syntax & (RE_CONTEXT_INDEP_ANCHORS | RE_CARET_ANCHORS_HERE))
            && re_string_cur_idx(input) != 0) {
            char prev = re_string_peek_byte(input, -1);
            if (!(syntax & RE_NEWLINE_ALT) || prev != '\n') {
                break;
            }
        }
        token->type = ANCHOR;
        token->opr.ctx_type = LINE_FIRST;
        break;
    case '$':
        if (!(syntax & RE_CONTEXT_INDEP_ANCHORS)
            && re_string_cur_idx(input) + 1 != re_string_length(input)) {
            re_token_t next;
            re_string_skip_bytes(input, 1);
            peek_token(&next, input, syntax);
            re_string_skip_bytes(input, -1);
            if (next.type != OP_ALT && next.type != OP_CLOSE_SUBEXP) {
                break;
            }
        }
        token->type = ANCHOR;
        token->opr.ctx_type = LINE_LAST;
        break;
    default:
        break;
    }
    return 1;
}

static int32
peek_token_bracket(re_token_t *token, re_string_t *input, reg_syntax_t syntax) {
    uint8 c;
    if (re_string_eoi(input)) {
        token->type = END_OF_RE;
        return 0;
    }
    c = re_string_peek_byte(input, 0);
    token->opr.c = c;
    if (c == '\\' && (syntax & RE_BACKSLASH_ESCAPE_IN_LISTS)
        && re_string_cur_idx(input) + 1 < re_string_length(input)) {

        uint8 c2;
        re_string_skip_bytes(input, 1);
        c2 = re_string_peek_byte(input, 0);
        token->opr.c = c2;
        token->type = CHARACTER;
        return 1;
    }
    if (c == '[') {
        uint8 c2;
        int32 token_len;
        if (re_string_cur_idx(input) + 1 < re_string_length(input)) {
            c2 = re_string_peek_byte(input, 1);
        } else {
            c2 = 0;
        }
        token->opr.c = c2;
        token_len = 2;
        switch (c2) {
        case '.':
            token->type = OP_OPEN_COLL_ELEM;
            break;

        case '=':
            token->type = OP_OPEN_EQUIV_CLASS;
            break;

        case ':':
            if (syntax & RE_CHAR_CLASSES) {
                token->type = OP_OPEN_CHAR_CLASS;
                break;
            }
            FALLTHROUGH;
        default:
            token->type = CHARACTER;
            token->opr.c = c;
            token_len = 1;
            break;
        }
        return token_len;
    }
    switch (c) {
    case '-':
        token->type = OP_CHARSET_RANGE;
        break;
    case ']':
        token->type = OP_CLOSE_BRACKET;
        break;
    case '^':
        token->type = OP_NON_MATCH_LIST;
        break;
    default:
        token->type = CHARACTER;
    }
    return 1;
}
static bin_tree_t *
parse(re_string_t *regexp, regex_t *preg, reg_syntax_t syntax,
      reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree, *eor, *root;
    re_token_t current_token;
    dfa->syntax = syntax;
    fetch_token(&current_token, regexp, syntax | RE_CARET_ANCHORS_HERE);
    tree = parse_reg_exp(regexp, preg, &current_token, syntax, 0, err);
    if (__glibc_unlikely(*err != REG_NOERROR && tree == NULL)) {
        return NULL;
    }
    eor = create_tree(dfa, NULL, NULL, END_OF_RE);
    if (tree != NULL) {
        root = create_tree(dfa, tree, eor, CONCAT);
    } else {
        root = eor;
    }
    if (__glibc_unlikely(eor == NULL || root == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }
    return root;
}
static bin_tree_t *
parse_reg_exp(re_string_t *regexp, regex_t *preg, re_token_t *token,
              reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree, *branch = NULL;
    bitset_word_t initial_bkref_map = dfa->completed_bkref_map;
    tree = parse_branch(regexp, preg, token, syntax, nest, err);
    if (__glibc_unlikely(*err != REG_NOERROR && tree == NULL)) {
        return NULL;
    }

    while (token->type == OP_ALT) {
        fetch_token(token, regexp, syntax | RE_CARET_ANCHORS_HERE);
        if (token->type != OP_ALT && token->type != END_OF_RE
            && (nest == 0 || token->type != OP_CLOSE_SUBEXP)) {
            bitset_word_t accumulated_bkref_map = dfa->completed_bkref_map;
            dfa->completed_bkref_map = initial_bkref_map;
            branch = parse_branch(regexp, preg, token, syntax, nest, err);
            if (__glibc_unlikely(*err != REG_NOERROR && branch == NULL)) {
                if (tree != NULL) {
                    postorder(tree, free_tree, NULL);
                }
                return NULL;
            }
            dfa->completed_bkref_map |= accumulated_bkref_map;
        } else {
            branch = NULL;
        }
        tree = create_tree(dfa, tree, branch, OP_ALT);
        if (__glibc_unlikely(tree == NULL)) {
            *err = REG_ESPACE;
            return NULL;
        }
    }
    return tree;
}
static bin_tree_t *
parse_branch(re_string_t *regexp, regex_t *preg, re_token_t *token,
             reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    bin_tree_t *tree, *expr;
    re_dfa_t *dfa = preg->buffer;
    tree = parse_expression(regexp, preg, token, syntax, nest, err);
    if (__glibc_unlikely(*err != REG_NOERROR && tree == NULL)) {
        return NULL;
    }

    while (token->type != OP_ALT && token->type != END_OF_RE
           && (nest == 0 || token->type != OP_CLOSE_SUBEXP)) {
        expr = parse_expression(regexp, preg, token, syntax, nest, err);
        if (__glibc_unlikely(*err != REG_NOERROR && expr == NULL)) {
            if (tree != NULL) {
                postorder(tree, free_tree, NULL);
            }
            return NULL;
        }
        if (tree != NULL && expr != NULL) {
            bin_tree_t *newtree = create_tree(dfa, tree, expr, CONCAT);
            if (newtree == NULL) {
                postorder(expr, free_tree, NULL);
                postorder(tree, free_tree, NULL);
                *err = REG_ESPACE;
                return NULL;
            }
            tree = newtree;
        } else if (tree == NULL) {
            tree = expr;
        }
    }
    return tree;
}

static bin_tree_t *
parse_expression(re_string_t *regexp, regex_t *preg, re_token_t *token,
                 reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree;
    switch (token->type) {
    case CHARACTER:
        tree = create_token_tree(dfa, NULL, NULL, token);
        if (__glibc_unlikely(tree == NULL)) {
            *err = REG_ESPACE;
            return NULL;
        }
        break;

    case OP_OPEN_SUBEXP:
        tree = parse_sub_exp(regexp, preg, token, syntax, nest + 1, err);
        if (__glibc_unlikely(*err != REG_NOERROR && tree == NULL)) {
            return NULL;
        }
        break;

    case OP_OPEN_BRACKET:
        tree = parse_bracket_exp(regexp, dfa, token, syntax, err);
        if (__glibc_unlikely(*err != REG_NOERROR && tree == NULL)) {
            return NULL;
        }
        break;

    case OP_BACK_REF:
        if (!__glibc_likely(dfa->completed_bkref_map & (1 << token->opr.idx))) {
            *err = REG_ESUBREG;
            return NULL;
        }
        dfa->used_bkref_map |= 1 << token->opr.idx;
        tree = create_token_tree(dfa, NULL, NULL, token);
        if (__glibc_unlikely(tree == NULL)) {
            *err = REG_ESPACE;
            return NULL;
        }
        ++dfa->nbackref;
        dfa->has_mb_node = 1;
        break;

    case OP_OPEN_DUP_NUM:
        if (syntax & RE_CONTEXT_INVALID_DUP) {
            *err = REG_BADRPT;
            return NULL;
        }
        FALLTHROUGH;
    case OP_DUP_ASTERISK:
    case OP_DUP_PLUS:
    case OP_DUP_QUESTION:
        if (syntax & RE_CONTEXT_INVALID_OPS) {
            *err = REG_BADRPT;
            return NULL;
        } else if (syntax & RE_CONTEXT_INDEP_OPS) {
            fetch_token(token, regexp, syntax);
            return parse_expression(regexp, preg, token, syntax, nest, err);
        }
        FALLTHROUGH;
    case OP_CLOSE_SUBEXP:
        if ((token->type == OP_CLOSE_SUBEXP)
            && !(syntax & RE_UNMATCHED_RIGHT_PAREN_ORD)) {
            *err = REG_ERPAREN;
            return NULL;
        }
        FALLTHROUGH;
    case OP_CLOSE_DUP_NUM:

        token->type = CHARACTER;

        tree = create_token_tree(dfa, NULL, NULL, token);
        if (__glibc_unlikely(tree == NULL)) {
            *err = REG_ESPACE;
            return NULL;
        }
        break;

    case ANCHOR:
        if ((token->opr.ctx_type
             & (WORD_DELIM | NOT_WORD_DELIM | WORD_FIRST | WORD_LAST))
            && dfa->word_ops_used == 0) {
            init_word_char(dfa);
        }
        if (token->opr.ctx_type == WORD_DELIM
            || token->opr.ctx_type == NOT_WORD_DELIM) {
            bin_tree_t *tree_first, *tree_last;
            if (token->opr.ctx_type == WORD_DELIM) {
                token->opr.ctx_type = WORD_FIRST;
                tree_first = create_token_tree(dfa, NULL, NULL, token);
                token->opr.ctx_type = WORD_LAST;
            } else {
                token->opr.ctx_type = INSIDE_WORD;
                tree_first = create_token_tree(dfa, NULL, NULL, token);
                token->opr.ctx_type = INSIDE_NOTWORD;
            }
            tree_last = create_token_tree(dfa, NULL, NULL, token);
            tree = create_tree(dfa, tree_first, tree_last, OP_ALT);
            if (__glibc_unlikely(tree_first == NULL || tree_last == NULL
                                 || tree == NULL)) {
                *err = REG_ESPACE;
                return NULL;
            }
        } else {
            tree = create_token_tree(dfa, NULL, NULL, token);
            if (__glibc_unlikely(tree == NULL)) {
                *err = REG_ESPACE;
                return NULL;
            }
        }

        fetch_token(token, regexp, syntax);
        return tree;

    case OP_PERIOD:
        tree = create_token_tree(dfa, NULL, NULL, token);
        if (__glibc_unlikely(tree == NULL)) {
            *err = REG_ESPACE;
            return NULL;
        }
        if (dfa->mb_cur_max > 1) {
            dfa->has_mb_node = 1;
        }
        break;

    case OP_WORD:
    case OP_NOTWORD:
        tree = build_charclass_op(dfa, regexp->trans, "alnum", "_",
                                  token->type == OP_NOTWORD, err);
        if (__glibc_unlikely(*err != REG_NOERROR && tree == NULL)) {
            return NULL;
        }
        break;

    case OP_SPACE:
    case OP_NOTSPACE:
        tree = build_charclass_op(dfa, regexp->trans, "space", "",
                                  token->type == OP_NOTSPACE, err);
        if (__glibc_unlikely(*err != REG_NOERROR && tree == NULL)) {
            return NULL;
        }
        break;

    case OP_ALT:
    case END_OF_RE:
        return NULL;

    case BACK_SLASH:
        *err = REG_EESCAPE;
        return NULL;

    default:

        DEBUG_ASSERT(false);
        return NULL;
    }
    fetch_token(token, regexp, syntax);

    while (token->type == OP_DUP_ASTERISK || token->type == OP_DUP_PLUS
           || token->type == OP_DUP_QUESTION
           || token->type == OP_OPEN_DUP_NUM) {
        bin_tree_t *dup_tree
            = parse_dup_op(tree, regexp, dfa, token, syntax, err);
        if (__glibc_unlikely(*err != REG_NOERROR && dup_tree == NULL)) {
            if (tree != NULL) {
                postorder(tree, free_tree, NULL);
            }
            return NULL;
        }
        tree = dup_tree;

        if ((syntax & RE_CONTEXT_INVALID_DUP)
            && (token->type == OP_DUP_ASTERISK
                || token->type == OP_OPEN_DUP_NUM)) {
            if (tree != NULL) {
                postorder(tree, free_tree, NULL);
            }
            *err = REG_BADRPT;
            return NULL;
        }
    }

    return tree;
}
static bin_tree_t *
parse_sub_exp(re_string_t *regexp, regex_t *preg, re_token_t *token,
              reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree;
    int64 cur_nsub;
    cur_nsub = preg->re_nsub++;

    fetch_token(token, regexp, syntax | RE_CARET_ANCHORS_HERE);

    if (token->type == OP_CLOSE_SUBEXP) {
        tree = NULL;
    } else {
        tree = parse_reg_exp(regexp, preg, token, syntax, nest, err);
        if (__glibc_unlikely(*err == REG_NOERROR
                             && token->type != OP_CLOSE_SUBEXP)) {
            if (tree != NULL) {
                postorder(tree, free_tree, NULL);
            }
            *err = REG_EPAREN;
        }
        if (__glibc_unlikely(*err != REG_NOERROR)) {
            return NULL;
        }
    }

    if (cur_nsub <= '9' - '1') {
        dfa->completed_bkref_map |= 1 << cur_nsub;
    }

    tree = create_tree(dfa, tree, NULL, SUBEXP);
    if (__glibc_unlikely(tree == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }
    tree->token.opr.idx = cur_nsub;
    return tree;
}

static bin_tree_t *
parse_dup_op(bin_tree_t *elem, re_string_t *regexp, re_dfa_t *dfa,
             re_token_t *token, reg_syntax_t syntax, reg_errcode_t *err) {
    bin_tree_t *tree = NULL, *old_tree = NULL;
    Idx i, start, end, start_idx = re_string_cur_idx(regexp);
    re_token_t start_token = *token;

    if (token->type == OP_OPEN_DUP_NUM) {
        end = 0;
        start = fetch_number(regexp, token, syntax);
        if (start == -1) {
            if (token->type == CHARACTER && token->opr.c == ',') {
                start = 0;
            } else {
                *err = REG_BADBR;
                return NULL;
            }
        }
        if (__glibc_likely(start != -2)) {

            end = ((token->type == OP_CLOSE_DUP_NUM)
                       ? start
                       : ((token->type == CHARACTER && token->opr.c == ',')
                              ? fetch_number(regexp, token, syntax)
                              : -2));
        }
        if (__glibc_unlikely(start == -2 || end == -2)) {

            if (__glibc_unlikely(!(syntax & RE_INVALID_INTERVAL_ORD))) {
                if (token->type == END_OF_RE) {
                    *err = REG_EBRACE;
                } else {
                    *err = REG_BADBR;
                }

                return NULL;
            }

            re_string_set_index(regexp, start_idx);
            *token = start_token;
            token->type = CHARACTER;

            return elem;
        }

        if (__glibc_unlikely((end != -1 && start > end)
                             || token->type != OP_CLOSE_DUP_NUM)) {

            *err = REG_BADBR;
            return NULL;
        }

        if (__glibc_unlikely(RE_DUP_MAX < (end == -1 ? start : end))) {
            *err = REG_ESIZE;
            return NULL;
        }
    } else {
        start = (token->type == OP_DUP_PLUS) ? 1 : 0;
        end = (token->type == OP_DUP_QUESTION) ? 1 : -1;
    }

    fetch_token(token, regexp, syntax);

    if (__glibc_unlikely(elem == NULL)) {
        return NULL;
    }
    if (__glibc_unlikely(start == 0 && end == 0)) {
        postorder(elem, free_tree, NULL);
        return NULL;
    }

    if (__glibc_unlikely(start > 0)) {
        tree = elem;
        for (i = 2; i <= start; ++i) {
            elem = duplicate_tree(elem, dfa);
            tree = create_tree(dfa, tree, elem, CONCAT);
            if (__glibc_unlikely(elem == NULL || tree == NULL)) {
                goto parse_dup_op_espace;
            }
        }

        if (start == end) {
            return tree;
        }

        elem = duplicate_tree(elem, dfa);
        if (__glibc_unlikely(elem == NULL)) {
            goto parse_dup_op_espace;
        }
        old_tree = tree;
    } else {
        old_tree = NULL;
    }

    if (elem->token.type == SUBEXP) {
        uintptr_t subidx = elem->token.opr.idx;
        postorder(elem, mark_opt_subexp, (void *)subidx);
    }

    tree = create_tree(dfa, elem, NULL, (end == -1 ? OP_DUP_ASTERISK : OP_ALT));
    if (__glibc_unlikely(tree == NULL)) {
        goto parse_dup_op_espace;
    }

    if (TYPE_SIGNED(Idx) || end != -1) {
        for (i = start + 2; i <= end; ++i) {
            elem = duplicate_tree(elem, dfa);
            tree = create_tree(dfa, tree, elem, CONCAT);
            if (__glibc_unlikely(elem == NULL || tree == NULL)) {
                goto parse_dup_op_espace;
            }

            tree = create_tree(dfa, tree, NULL, OP_ALT);
            if (__glibc_unlikely(tree == NULL)) {
                goto parse_dup_op_espace;
            }
        }
    }

    if (old_tree) {
        tree = create_tree(dfa, old_tree, tree, CONCAT);
    }

    return tree;

parse_dup_op_espace:
    *err = REG_ESPACE;
    return NULL;
}
static inline int32_t __attribute__((always_inline))
seek_collating_symbol_entry(uint8 *name, int64 name_len, int32_t *symb_table,
                            int32_t table_size, uint8 *extra) {
    int32_t elem;

    for (elem = 0; elem < table_size; elem++) {
        if (symb_table[2*elem] != 0) {
            int32_t idx = symb_table[2*elem + 1];

            idx += 1 + extra[idx];
            if (name_len == extra[idx]

                && memcmp(name, &extra[idx + 1], name_len) == 0) {

                return elem;
            }
        }
    }
    return -1;
}

static inline uint32 __attribute__((always_inline))
lookup_collation_sequence_value(bracket_elem_t *br_elem, uint32_t nrules,
                                uint8 *collseqmb, char *collseqwc,
                                int32_t table_size, int32_t *symb_table,
                                uint8 *extra) {
    if (br_elem->type == SB_CHAR) {

        if (nrules == 0) {
            return collseqmb[br_elem->opr.ch];
        } else {
            wint_t wc = __btowc(br_elem->opr.ch);
            return __collseq_table_lookup(collseqwc, wc);
        }
    } else if (br_elem->type == MB_CHAR) {
        if (nrules != 0) {
            return __collseq_table_lookup(collseqwc, br_elem->opr.wch);
        }
    } else if (br_elem->type == COLL_SYM) {
        int64 sym_name_len = strlen32((char *)br_elem->opr.name);
        if (nrules != 0) {
            int32_t elem, idx;
            elem = seek_collating_symbol_entry(br_elem->opr.name, sym_name_len,
                                               symb_table, table_size, extra);
            if (elem != -1) {

                idx = symb_table[2*elem + 1];

                idx += 1 + extra[idx];

                idx += 1 + extra[idx];

                idx = (idx + 3) & ~3;

                idx += sizeof(uint32);

                idx += sizeof(uint32)*(1 + *(uint32 *)(extra + idx));

                return *(uint32 *)(extra + idx);
            } else if (sym_name_len == 1) {

                return collseqmb[br_elem->opr.name[0]];
            }
        } else if (sym_name_len == 1) {
            return collseqmb[br_elem->opr.name[0]];
        }
    }
    return UINT_MAX;
}
static inline reg_errcode_t __attribute__((always_inline))
build_range_exp(bitset_t sbcset, re_charset_t *mbcset, Idx *range_alloc,
                bracket_elem_t *start_elem, bracket_elem_t *end_elem,
                re_dfa_t *dfa, reg_syntax_t syntax, uint32_t nrules,
                uint8 *collseqmb, char *collseqwc, int32_t table_size,
                int32_t *symb_table, uint8 *extra) {
    uint32 ch;
    uint32_t start_collseq;
    uint32_t end_collseq;

    if (__glibc_unlikely(
            start_elem->type == EQUIV_CLASS || start_elem->type == CHAR_CLASS
            || end_elem->type == EQUIV_CLASS || end_elem->type == CHAR_CLASS)) {
        return REG_ERANGE;
    }

    start_collseq = lookup_collation_sequence_value(
        start_elem, nrules, collseqmb, collseqwc, table_size, symb_table,
        extra);
    end_collseq = lookup_collation_sequence_value(
        end_elem, nrules, collseqmb, collseqwc, table_size, symb_table, extra);

    if (__glibc_unlikely(start_collseq == UINT_MAX
                         || end_collseq == UINT_MAX)) {
        return REG_ECOLLATE;
    }
    if (__glibc_unlikely((syntax & RE_NO_EMPTY_RANGES)
                         && start_collseq > end_collseq)) {
        return REG_ERANGE;
    }

    if (nrules > 0 || dfa->mb_cur_max > 1) {

        if (__glibc_unlikely(*range_alloc == mbcset->nranges)) {

            uint32_t *new_array_start;
            uint32_t *new_array_end;
            Idx new_nranges;

            new_nranges = 2*mbcset->nranges + 1;
            new_array_start
                = re_realloc(mbcset->range_starts, uint32_t, new_nranges);
            new_array_end
                = re_realloc(mbcset->range_ends, uint32_t, new_nranges);

            if (__glibc_unlikely(new_array_start == NULL
                                 || new_array_end == NULL)) {
                return REG_ESPACE;
            }

            mbcset->range_starts = new_array_start;
            mbcset->range_ends = new_array_end;
            *range_alloc = new_nranges;
        }

        mbcset->range_starts[mbcset->nranges] = start_collseq;
        mbcset->range_ends[mbcset->nranges++] = end_collseq;
    }

    for (ch = 0; ch < SBC_MAX; ch++) {
        uint32_t ch_collseq;

        if (nrules == 0) {
            ch_collseq = collseqmb[ch];
        } else {
            ch_collseq = __collseq_table_lookup(collseqwc, __btowc(ch));
        }
        if (start_collseq <= ch_collseq && ch_collseq <= end_collseq) {
            bitset_set(sbcset, ch);
        }
    }
    return REG_NOERROR;
}

static inline reg_errcode_t __attribute__((always_inline))
build_collating_symbol(bitset_t sbcset, re_charset_t *mbcset,
                       Idx *coll_sym_alloc, uint8 *name, uint32_t nrules,
                       int32_t table_size, int32_t *symb_table, uint8 *extra) {
    int32_t elem, idx;
    int64 name_len = strlen32((char *)name);
    if (nrules != 0) {
        elem = seek_collating_symbol_entry(name, name_len, symb_table,
                                           table_size, extra);
        if (elem != -1) {

            idx = symb_table[2*elem + 1];

            idx += 1 + extra[idx];
        } else if (name_len == 1) {

            bitset_set(sbcset, name[0]);
            return REG_NOERROR;
        } else {
            return REG_ECOLLATE;
        }

        if (__glibc_unlikely(*coll_sym_alloc == mbcset->ncoll_syms)) {

            Idx new_coll_sym_alloc = 2*mbcset->ncoll_syms + 1;

            int32_t *new_coll_syms
                = re_realloc(mbcset->coll_syms, int32_t, new_coll_sym_alloc);
            if (__glibc_unlikely(new_coll_syms == NULL)) {
                return REG_ESPACE;
            }
            mbcset->coll_syms = new_coll_syms;
            *coll_sym_alloc = new_coll_sym_alloc;
        }
        mbcset->coll_syms[mbcset->ncoll_syms++] = idx;
        return REG_NOERROR;
    } else {
        if (__glibc_unlikely(name_len != 1)) {
            return REG_ECOLLATE;
        } else {
            bitset_set(sbcset, name[0]);
            return REG_NOERROR;
        }
    }
}

static bin_tree_t *
parse_bracket_exp(re_string_t *regexp, re_dfa_t *dfa, re_token_t *token,
                  reg_syntax_t syntax, reg_errcode_t *err) {

    uint8 *collseqmb;
    char *collseqwc = NULL;
    uint32_t nrules;
    int32_t table_size = 0;
    int32_t *symb_table = NULL;
    uint8 *extra = NULL;

    re_token_t br_token;
    re_bitset_ptr_t sbcset;

    bool non_match = false;
    bin_tree_t *work_tree;
    int32 token_len;
    bool first_round = true;

    collseqmb = (uint8 *)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_COLLSEQMB);
    nrules = _NL_CURRENT_WORD(LC_COLLATE, _NL_COLLATE_NRULES);
    if (nrules) {

        collseqwc = _NL_CURRENT(LC_COLLATE, _NL_COLLATE_COLLSEQWC);
        table_size = _NL_CURRENT_WORD(LC_COLLATE, _NL_COLLATE_SYMB_HASH_SIZEMB);
        symb_table
            = (int32_t *)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_SYMB_TABLEMB);
        extra = (uint8 *)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_SYMB_EXTRAMB);
    }

    sbcset = (re_bitset_ptr_t)calloc(sizeof(bitset_t), 1);

    if (__glibc_unlikely(sbcset == NULL))

    {
        re_free(sbcset);

        *err = REG_ESPACE;
        return NULL;
    }

    token_len = peek_token_bracket(token, regexp, syntax);
    if (__glibc_unlikely(token->type == END_OF_RE)) {
        *err = REG_BADPAT;
        goto parse_bracket_exp_free_return;
    }
    if (token->type == OP_NON_MATCH_LIST) {

        non_match = true;
        if (syntax & RE_HAT_LISTS_NOT_NEWLINE) {
            bitset_set(sbcset, '\n');
        }
        re_string_skip_bytes(regexp, token_len);
        token_len = peek_token_bracket(token, regexp, syntax);
        if (__glibc_unlikely(token->type == END_OF_RE)) {
            *err = REG_BADPAT;
            goto parse_bracket_exp_free_return;
        }
    }

    if (token->type == OP_CLOSE_BRACKET) {
        token->type = CHARACTER;
    }

    while (1) {
        bracket_elem_t start_elem, end_elem;
        uint8 start_name_buf[32];
        uint8 end_name_buf[32];
        reg_errcode_t ret;
        int32 token_len2 = 0;
        bool is_range_exp = false;
        re_token_t token2;

        start_elem.opr.name = start_name_buf;
        start_elem.type = COLL_SYM;
        ret = parse_bracket_element(&start_elem, regexp, token, token_len, dfa,
                                    syntax, first_round);
        if (__glibc_unlikely(ret != REG_NOERROR)) {
            *err = ret;
            goto parse_bracket_exp_free_return;
        }
        first_round = false;

        token_len = peek_token_bracket(token, regexp, syntax);

        if (start_elem.type != CHAR_CLASS && start_elem.type != EQUIV_CLASS) {
            if (__glibc_unlikely(token->type == END_OF_RE)) {
                *err = REG_EBRACK;
                goto parse_bracket_exp_free_return;
            }
            if (token->type == OP_CHARSET_RANGE) {
                re_string_skip_bytes(regexp, token_len);
                token_len2 = peek_token_bracket(&token2, regexp, syntax);
                if (__glibc_unlikely(token2.type == END_OF_RE)) {
                    *err = REG_EBRACK;
                    goto parse_bracket_exp_free_return;
                }
                if (token2.type == OP_CLOSE_BRACKET) {

                    re_string_skip_bytes(regexp, -token_len);
                    token->type = CHARACTER;
                } else {
                    is_range_exp = true;
                }
            }
        }

        if (is_range_exp == true) {
            end_elem.opr.name = end_name_buf;
            end_elem.type = COLL_SYM;
            ret = parse_bracket_element(&end_elem, regexp, &token2, token_len2,
                                        dfa, syntax, true);
            if (__glibc_unlikely(ret != REG_NOERROR)) {
                *err = ret;
                goto parse_bracket_exp_free_return;
            }

            token_len = peek_token_bracket(token, regexp, syntax);

            *err = build_range_exp(sbcset, mbcset, &range_alloc, &start_elem,
                                   &end_elem, dfa, syntax, nrules, collseqmb,
                                   collseqwc, table_size, symb_table, extra);
            if (__glibc_unlikely(*err != REG_NOERROR)) {
                goto parse_bracket_exp_free_return;
            }
        } else {
            switch (start_elem.type) {
            case SB_CHAR:
                bitset_set(sbcset, start_elem.opr.ch);
                break;
            case EQUIV_CLASS:
                *err = build_equiv_class(sbcset,

                                         start_elem.opr.name);
                if (__glibc_unlikely(*err != REG_NOERROR)) {
                    goto parse_bracket_exp_free_return;
                }
                break;
            case COLL_SYM:
                *err = build_collating_symbol(sbcset,

                                              start_elem.opr.name

                                              ,
                                              nrules, table_size, symb_table,
                                              extra

                );
                if (__glibc_unlikely(*err != REG_NOERROR)) {
                    goto parse_bracket_exp_free_return;
                }
                break;
            case CHAR_CLASS:
                *err = build_charclass(regexp->trans, sbcset,

                                       (char *)start_elem.opr.name, syntax);
                if (__glibc_unlikely(*err != REG_NOERROR)) {
                    goto parse_bracket_exp_free_return;
                }
                break;
            default:
                DEBUG_ASSERT(false);
                break;
            }
        }
        if (__glibc_unlikely(token->type == END_OF_RE)) {
            *err = REG_EBRACK;
            goto parse_bracket_exp_free_return;
        }
        if (token->type == OP_CLOSE_BRACKET) {
            break;
        }
    }

    re_string_skip_bytes(regexp, token_len);

    if (non_match) {
        bitset_not(sbcset);
    }
    {

        br_token.type = SIMPLE_BRACKET;
        br_token.opr.sbcset = sbcset;
        work_tree = create_token_tree(dfa, NULL, NULL, &br_token);
        if (__glibc_unlikely(work_tree == NULL)) {
            goto parse_bracket_exp_espace;
        }
    }
    return work_tree;

parse_bracket_exp_espace:
    *err = REG_ESPACE;
parse_bracket_exp_free_return:
    re_free(sbcset);

    return NULL;
}

static reg_errcode_t
parse_bracket_element(bracket_elem_t *elem, re_string_t *regexp,
                      re_token_t *token, int32 token_len, re_dfa_t *dfa,
                      reg_syntax_t syntax, bool accept_hyphen) {
    re_string_skip_bytes(regexp, token_len);
    if (token->type == OP_OPEN_COLL_ELEM || token->type == OP_OPEN_CHAR_CLASS
        || token->type == OP_OPEN_EQUIV_CLASS) {
        return parse_bracket_symbol(elem, regexp, token);
    }
    if (__glibc_unlikely(token->type == OP_CHARSET_RANGE) && !accept_hyphen) {

        re_token_t token2;
        (void)peek_token_bracket(&token2, regexp, syntax);
        if (token2.type != OP_CLOSE_BRACKET) {

            return REG_ERANGE;
        }
    }
    elem->type = SB_CHAR;
    elem->opr.ch = token->opr.c;
    return REG_NOERROR;
}

static reg_errcode_t
parse_bracket_symbol(bracket_elem_t *elem, re_string_t *regexp,
                     re_token_t *token) {
    uint8 ch, delim = token->opr.c;
    int32 i = 0;
    if (re_string_eoi(regexp)) {
        return REG_EBRACK;
    }
    for (;; ++i) {
        if (i >= 32) {
            return REG_EBRACK;
        }
        if (token->type == OP_OPEN_CHAR_CLASS) {
            ch = re_string_fetch_byte_case(regexp);
        } else {
            ch = re_string_fetch_byte(regexp);
        }
        if (re_string_eoi(regexp)) {
            return REG_EBRACK;
        }
        if (ch == delim && re_string_peek_byte(regexp, 0) == ']') {
            break;
        }
        elem->opr.name[i] = ch;
    }
    re_string_skip_bytes(regexp, 1);
    elem->opr.name[i] = '\0';
    switch (token->type) {
    case OP_OPEN_COLL_ELEM:
        elem->type = COLL_SYM;
        break;
    case OP_OPEN_EQUIV_CLASS:
        elem->type = EQUIV_CLASS;
        break;
    case OP_OPEN_CHAR_CLASS:
        elem->type = CHAR_CLASS;
        break;
    default:
        break;
    }
    return REG_NOERROR;
}

static reg_errcode_t

build_equiv_class(bitset_t sbcset, uint8 *name)

{

    uint32_t nrules = _NL_CURRENT_WORD(LC_COLLATE, _NL_COLLATE_NRULES);
    if (nrules != 0) {
        int32_t *table, *indirect;
        uint8 *weights, *extra, *cp;
        uint8 char_buf[2];
        int32_t idx1, idx2;
        uint32 ch;
        int64 len;

        cp = name;
        table = (int32_t *)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_TABLEMB);
        weights = (uint8 *)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_WEIGHTMB);
        extra = (uint8 *)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_EXTRAMB);
        indirect = (int32_t *)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_INDIRECTMB);
        idx1 = findidx(table, indirect, extra, &cp, -1);
        if (__glibc_unlikely(idx1 == 0 || *cp != '\0')) {

            return REG_ECOLLATE;
        }

        len = weights[idx1 & 0xffffff];
        for (ch = 0; ch < SBC_MAX; ++ch) {
            char_buf[0] = ch;
            cp = char_buf;
            idx2 = findidx(table, indirect, extra, &cp, 1);

            if (idx2 == 0) {

                continue;
            }

            if (len == weights[idx2 & 0xffffff] && (idx1 >> 24) == (idx2 >> 24)
                && memcmp(weights + (idx1 & 0xffffff) + 1,
                          weights + (idx2 & 0xffffff) + 1, len)
                       == 0) {
                bitset_set(sbcset, ch);
            }
        }

        if (__glibc_unlikely(*equiv_class_alloc == mbcset->nequiv_classes)) {

            Idx new_equiv_class_alloc = 2*mbcset->nequiv_classes + 1;

            int32_t *new_equiv_classes = re_realloc(
                mbcset->equiv_classes, int32_t, new_equiv_class_alloc);
            if (__glibc_unlikely(new_equiv_classes == NULL)) {
                return REG_ESPACE;
            }
            mbcset->equiv_classes = new_equiv_classes;
            *equiv_class_alloc = new_equiv_class_alloc;
        }
        mbcset->equiv_classes[mbcset->nequiv_classes++] = idx1;
    } else

    {
        if (__glibc_unlikely(strlen32((char *)name) != 1)) {
            return REG_ECOLLATE;
        }
        bitset_set(sbcset, *name);
    }
    return REG_NOERROR;
}

static reg_errcode_t

build_charclass(RE_TRANSLATE_TYPE trans, bitset_t sbcset, char *class_name,
                reg_syntax_t syntax)

{
    int32 i;
    char *name = class_name;

    if ((syntax & RE_ICASE)
        && (strcmp(name, "upper") == 0 || strcmp(name, "lower") == 0)) {
        name = "alpha";
    }
    if (strcmp(name, "alnum") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isalnum(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isalnum(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "cntrl") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (iscntrl(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (iscntrl(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "lower") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (islower(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (islower(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "space") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isspace(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isspace(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "alpha") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isalpha(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isalpha(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "digit") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isdigit(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isdigit(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "print") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isprint(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isprint(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "upper") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isupper(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isupper(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "blank") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isblank(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isblank(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "graph") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isgraph(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isgraph(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "punct") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (ispunct(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (ispunct(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else if (strcmp(name, "xdigit") == 0) {
        do {
            if (__glibc_unlikely(trans != NULL)) {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isxdigit(i)) {
                        bitset_set(sbcset, trans[i]);
                    }
                }
            } else {
                for (i = 0; i < SBC_MAX; ++i) {
                    if (isxdigit(i)) {
                        bitset_set(sbcset, i);
                    }
                }
            }
        } while (0);
    } else {
        return REG_ECTYPE;
    }

    return REG_NOERROR;
}

static bin_tree_t *
build_charclass_op(re_dfa_t *dfa, RE_TRANSLATE_TYPE trans, char *class_name,
                   char *extra, bool non_match, reg_errcode_t *err) {
    re_bitset_ptr_t sbcset;

    reg_errcode_t ret;
    bin_tree_t *tree;

    sbcset = (re_bitset_ptr_t)calloc(sizeof(bitset_t), 1);
    if (__glibc_unlikely(sbcset == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }
    ret = build_charclass(trans, sbcset,

                          class_name, 0);

    if (__glibc_unlikely(ret != REG_NOERROR)) {
        re_free(sbcset);

        *err = ret;
        return NULL;
    }

    for (; *extra; extra++) {
        bitset_set(sbcset, *extra);
    }

    if (non_match) {
        bitset_not(sbcset);
    }
    re_token_t br_token = {.type = SIMPLE_BRACKET, .opr.sbcset = sbcset};
    tree = create_token_tree(dfa, NULL, NULL, &br_token);
    if (__glibc_unlikely(tree == NULL)) {
        goto build_word_op_espace;
    }
    return tree;

build_word_op_espace:
    re_free(sbcset);

    *err = REG_ESPACE;
    return NULL;
}

static Idx
fetch_number(re_string_t *input, re_token_t *token, reg_syntax_t syntax) {
    Idx num = -1;
    uint8 c;
    while (1) {
        fetch_token(token, input, syntax);
        c = token->opr.c;
        if (__glibc_unlikely(token->type == END_OF_RE)) {
            return -2;
        }
        if (token->type == OP_CLOSE_DUP_NUM || c == ',') {
            break;
        }
        num = ((token->type != CHARACTER || c < '0' || '9' < c || num == -2)
                   ? -2
               : num == -1 ? c - '0'
                           : MIN(RE_DUP_MAX + 1, num*10 + c - '0'));
    }
    return num;
}
static bin_tree_t *
create_tree(re_dfa_t *dfa, bin_tree_t *left, bin_tree_t *right,
            re_token_type_t type) {
    re_token_t t = {.type = type};
    return create_token_tree(dfa, left, right, &t);
}

static bin_tree_t *
create_token_tree(re_dfa_t *dfa, bin_tree_t *left, bin_tree_t *right,
                  re_token_t *token) {
    bin_tree_t *tree;
    if (__glibc_unlikely(dfa->str_tree_storage_idx == BIN_TREE_STORAGE_SIZE)) {
        bin_tree_storage_t *storage = re_malloc(bin_tree_storage_t, 1);

        if (storage == NULL) {
            return NULL;
        }
        storage->next = dfa->str_tree_storage;
        dfa->str_tree_storage = storage;
        dfa->str_tree_storage_idx = 0;
    }
    tree = &dfa->str_tree_storage->data[dfa->str_tree_storage_idx++];

    tree->parent = NULL;
    tree->left = left;
    tree->right = right;
    tree->token = *token;
    tree->token.duplicated = 0;
    tree->token.opt_subexp = 0;
    tree->first = NULL;
    tree->next = NULL;
    tree->node_idx = -1;

    if (left != NULL) {
        left->parent = tree;
    }
    if (right != NULL) {
        right->parent = tree;
    }
    return tree;
}

static reg_errcode_t
mark_opt_subexp(void *extra, bin_tree_t *node) {
    Idx idx = (uintptr_t)extra;
    if (node->token.type == SUBEXP && node->token.opr.idx == idx) {
        node->token.opt_subexp = 1;
    }

    return REG_NOERROR;
}

static void
free_token(re_token_t *node) {

    if (node->type == SIMPLE_BRACKET && node->duplicated == 0) {
        re_free(node->opr.sbcset);
    }
}

static reg_errcode_t
free_tree(void *extra, bin_tree_t *node) {
    free_token(&node->token);
    return REG_NOERROR;
}

static bin_tree_t *
duplicate_tree(bin_tree_t *root, re_dfa_t *dfa) {
    bin_tree_t *node;
    bin_tree_t *dup_root;
    bin_tree_t **p_new = &dup_root, *dup_node = root->parent;

    for (node = root;;) {

        *p_new = create_token_tree(dfa, NULL, NULL, &node->token);
        if (*p_new == NULL) {
            return NULL;
        }
        (*p_new)->parent = dup_node;
        (*p_new)->token.duplicated = 1;
        dup_node = *p_new;

        if (node->left) {
            node = node->left;
            p_new = &dup_node->left;
        } else {
            bin_tree_t *prev = NULL;
            while (node->right == prev || node->right == NULL) {
                prev = node;
                node = node->parent;
                dup_node = dup_node->parent;
                if (!node) {
                    return dup_root;
                }
            }
            node = node->right;
            p_new = &dup_node->right;
        }
    }
}
