#include "cbase.h"

static reg_errcode_t __attribute_warn_unused_result__
re_string_allocate(re_string_t *pstr, char *str, Idx len, Idx init_len,
                   RE_TRANSLATE_TYPE trans, bool icase, re_dfa_t *dfa) {
    reg_errcode_t ret;
    Idx init_buf_len;

    if (init_len < dfa->mb_cur_max) {
        init_len = dfa->mb_cur_max;
    }
    init_buf_len = (len + 1 < init_len) ? len + 1 : init_len;
    re_string_construct_common(str, len, pstr, trans, icase, dfa);

    ret = re_string_realloc_buffers(pstr, init_buf_len);
    if (__glibc_unlikely(ret != REG_NOERROR)) {
        return ret;
    }

    pstr->word_char = dfa->word_char;
    pstr->word_ops_used = dfa->word_ops_used;
    pstr->mbs = pstr->mbs_allocated ? pstr->mbs : (uint8 *)str;
    pstr->valid_len = (pstr->mbs_allocated || dfa->mb_cur_max > 1) ? 0 : len;
    pstr->valid_raw_len = pstr->valid_len;
    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_string_construct(re_string_t *pstr, char *str, Idx len,
                    RE_TRANSLATE_TYPE trans, bool icase, re_dfa_t *dfa) {
    reg_errcode_t ret;
    memset64(pstr, '\0', sizeof(re_string_t));
    re_string_construct_common(str, len, pstr, trans, icase, dfa);

    if (len > 0) {
        ret = re_string_realloc_buffers(pstr, len + 1);
        if (__glibc_unlikely(ret != REG_NOERROR)) {
            return ret;
        }
    }
    pstr->mbs = pstr->mbs_allocated ? pstr->mbs : (uint8 *)str;

    if (icase) {
        build_upper_buffer(pstr);
    } else {

        {
            if (trans != NULL) {
                re_string_translate_buffer(pstr);
            } else {
                pstr->valid_len = pstr->bufs_len;
                pstr->valid_raw_len = pstr->bufs_len;
            }
        }
    }

    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_string_realloc_buffers(re_string_t *pstr, Idx new_buf_len) {
    if (pstr->mbs_allocated) {
        uint8 *new_mbs = re_realloc(pstr->mbs, uint8, new_buf_len);
        if (__glibc_unlikely(new_mbs == NULL)) {
            return REG_ESPACE;
        }
        pstr->mbs = new_mbs;
    }
    pstr->bufs_len = new_buf_len;
    return REG_NOERROR;
}

static void
re_string_construct_common(char *str, Idx len, re_string_t *pstr,
                           RE_TRANSLATE_TYPE trans, bool icase, re_dfa_t *dfa) {
    pstr->raw_mbs = (uint8 *)str;
    pstr->len = len;
    pstr->raw_len = len;
    pstr->trans = trans;
    pstr->icase = icase;
    pstr->mbs_allocated = (trans != NULL || icase);
    pstr->mb_cur_max = dfa->mb_cur_max;
    pstr->is_utf8 = dfa->is_utf8;
    pstr->map_notascii = dfa->map_notascii;
    pstr->stop = pstr->len;
    pstr->raw_stop = pstr->stop;
}
static void
build_upper_buffer(re_string_t *pstr) {
    Idx char_idx, end_idx;
    end_idx = (pstr->bufs_len > pstr->len) ? pstr->len : pstr->bufs_len;

    for (char_idx = pstr->valid_len; char_idx < end_idx; ++char_idx) {
        int32 ch = pstr->raw_mbs[pstr->raw_mbs_idx + char_idx];
        if (__glibc_unlikely(pstr->trans != NULL)) {
            ch = pstr->trans[ch];
        }
        pstr->mbs[char_idx] = toupper(ch);
    }
    pstr->valid_len = char_idx;
    pstr->valid_raw_len = char_idx;
}

static void
re_string_translate_buffer(re_string_t *pstr) {
    Idx buf_idx, end_idx;
    end_idx = (pstr->bufs_len > pstr->len) ? pstr->len : pstr->bufs_len;

    for (buf_idx = pstr->valid_len; buf_idx < end_idx; ++buf_idx) {
        int32 ch = pstr->raw_mbs[pstr->raw_mbs_idx + buf_idx];
        pstr->mbs[buf_idx] = pstr->trans[ch];
    }

    pstr->valid_len = buf_idx;
    pstr->valid_raw_len = buf_idx;
}

static reg_errcode_t __attribute_warn_unused_result__
re_string_reconstruct(re_string_t *pstr, Idx idx, int32 eflags) {
    Idx offset;

    if (__glibc_unlikely(pstr->raw_mbs_idx <= idx)) {
        offset = idx - pstr->raw_mbs_idx;
    } else {

        pstr->len = pstr->raw_len;
        pstr->stop = pstr->raw_stop;
        pstr->valid_len = 0;
        pstr->raw_mbs_idx = 0;
        pstr->valid_raw_len = 0;
        pstr->offsets_needed = 0;
        pstr->tip_context
            = ((eflags & REG_NOTBOL) ? CONTEXT_BEGBUF
                                     : CONTEXT_NEWLINE | CONTEXT_BEGBUF);
        if (!pstr->mbs_allocated) {
            pstr->mbs = (uint8 *)pstr->raw_mbs;
        }
        offset = idx;
    }

    if (__glibc_likely(offset != 0)) {

        if (__glibc_likely(offset < pstr->valid_raw_len)) {
            {
                pstr->tip_context
                    = re_string_context_at(pstr, offset - 1, eflags);

                if (__glibc_unlikely(pstr->mbs_allocated)) {
                    memmove64(pstr->mbs, pstr->mbs + offset,
                            pstr->valid_len - offset);
                }
                pstr->valid_len -= offset;
                pstr->valid_raw_len -= offset;
                DEBUG_ASSERT(pstr->valid_len > 0);
            }
        } else {
            pstr->valid_len = 0;
            {
                int32 c = pstr->raw_mbs[pstr->raw_mbs_idx + offset - 1];
                pstr->valid_raw_len = 0;
                if (pstr->trans) {
                    c = pstr->trans[c];
                }
                pstr->tip_context
                    = (bitset_contain(pstr->word_char, c)
                           ? CONTEXT_WORD
                           : ((IS_NEWLINE(c) && pstr->newline_anchor)
                                  ? CONTEXT_NEWLINE
                                  : 0));
            }
        }
        if (!__glibc_unlikely(pstr->mbs_allocated)) {
            pstr->mbs += offset;
        }
    }
    pstr->raw_mbs_idx = idx;
    pstr->len -= offset;
    pstr->stop -= offset;
    if (__glibc_unlikely(pstr->mbs_allocated)) {
        if (pstr->icase) {
            build_upper_buffer(pstr);
        } else if (pstr->trans != NULL) {
            re_string_translate_buffer(pstr);
        }
    } else {
        pstr->valid_len = pstr->len;
    }

    pstr->cur_idx = 0;
    return REG_NOERROR;
}

static uint8 __attribute__((pure))
re_string_peek_byte_case(re_string_t *pstr, Idx idx) {
    int32 ch;
    Idx off;

    if (__glibc_likely(!pstr->mbs_allocated)) {
        return re_string_peek_byte(pstr, idx);
    }
    off = pstr->cur_idx + idx;

    ch = pstr->raw_mbs[pstr->raw_mbs_idx + off];
    return ch;
}

static uint8
re_string_fetch_byte_case(re_string_t *pstr) {
    if (__glibc_likely(!pstr->mbs_allocated)) {
        return re_string_fetch_byte(pstr);
    }
    return pstr->raw_mbs[pstr->raw_mbs_idx + pstr->cur_idx++];
}

static void
re_string_destruct(re_string_t *pstr) {

    if (pstr->mbs_allocated) {
        re_free(pstr->mbs);
    }
}

static uint32
re_string_context_at(re_string_t *input, Idx idx, int32 eflags) {
    int32 c;
    if (__glibc_unlikely(idx < 0)) {

        return input->tip_context;
    }
    if (__glibc_unlikely(idx == input->len)) {
        return ((eflags & REG_NOTEOL) ? CONTEXT_ENDBUF
                                      : CONTEXT_NEWLINE | CONTEXT_ENDBUF);
    }
    {
        c = re_string_byte_at(input, idx);
        if (bitset_contain(input->word_char, c)) {
            return CONTEXT_WORD;
        }
        return IS_NEWLINE(c) && input->newline_anchor ? CONTEXT_NEWLINE : 0;
    }
}

static reg_errcode_t __attribute_warn_unused_result__
re_node_set_alloc(re_node_set *set, Idx size) {
    set->alloc = size;
    set->nelem = 0;
    set->elems = re_malloc(Idx, size);
    if (__glibc_unlikely(set->elems == NULL)
        && (MALLOC_0_IS_NONNULL || size != 0)) {
        return REG_ESPACE;
    }
    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_node_set_init_1(re_node_set *set, Idx elem) {
    set->alloc = 1;
    set->nelem = 1;
    set->elems = re_malloc(Idx, 1);
    if (__glibc_unlikely(set->elems == NULL)) {
        set->alloc = set->nelem = 0;
        return REG_ESPACE;
    }
    set->elems[0] = elem;
    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_node_set_init_2(re_node_set *set, Idx elem1, Idx elem2) {
    set->alloc = 2;
    set->elems = re_malloc(Idx, 2);
    if (__glibc_unlikely(set->elems == NULL)) {
        return REG_ESPACE;
    }
    if (elem1 == elem2) {
        set->nelem = 1;
        set->elems[0] = elem1;
    } else {
        set->nelem = 2;
        if (elem1 < elem2) {
            set->elems[0] = elem1;
            set->elems[1] = elem2;
        } else {
            set->elems[0] = elem2;
            set->elems[1] = elem1;
        }
    }
    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_node_set_init_copy(re_node_set *dest, re_node_set *src) {
    dest->nelem = src->nelem;
    if (src->nelem > 0) {
        dest->alloc = dest->nelem;
        dest->elems = re_malloc(Idx, dest->alloc);
        if (__glibc_unlikely(dest->elems == NULL)) {
            dest->alloc = dest->nelem = 0;
            return REG_ESPACE;
        }
        memcpy64(dest->elems, src->elems, src->nelem*sizeof(Idx));
    } else {
        re_node_set_init_empty(dest);
    }
    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_node_set_add_intersect(re_node_set *dest, re_node_set *src1,
                          re_node_set *src2) {
    Idx i1, i2, is, id, delta, sbase;
    if (src1->nelem == 0 || src2->nelem == 0) {
        return REG_NOERROR;
    }

    if (src1->nelem + src2->nelem + dest->nelem > dest->alloc) {
        Idx new_alloc = src1->nelem + src2->nelem + dest->alloc;
        Idx *new_elems = re_realloc(dest->elems, Idx, new_alloc);
        if (__glibc_unlikely(new_elems == NULL)) {
            return REG_ESPACE;
        }
        dest->elems = new_elems;
        dest->alloc = new_alloc;
    }

    sbase = dest->nelem + src1->nelem + src2->nelem;
    i1 = src1->nelem - 1;
    i2 = src2->nelem - 1;
    id = dest->nelem - 1;
    for (;;) {
        if (src1->elems[i1] == src2->elems[i2]) {

            while (id >= 0 && dest->elems[id] > src1->elems[i1]) {
                --id;
            }

            if (id < 0 || dest->elems[id] != src1->elems[i1]) {
                dest->elems[--sbase] = src1->elems[i1];
            }

            if (--i1 < 0 || --i2 < 0) {
                break;
            }
        }

        else if (src1->elems[i1] < src2->elems[i2]) {
            if (--i2 < 0) {
                break;
            }
        } else {
            if (--i1 < 0) {
                break;
            }
        }
    }

    id = dest->nelem - 1;
    is = dest->nelem + src1->nelem + src2->nelem - 1;
    delta = is - sbase + 1;

    dest->nelem += delta;
    if (delta > 0 && id >= 0) {
        for (;;) {
            if (dest->elems[is] > dest->elems[id]) {

                dest->elems[id + delta--] = dest->elems[is--];
                if (delta == 0) {
                    break;
                }
            } else {

                dest->elems[id + delta] = dest->elems[id];
                if (--id < 0) {
                    break;
                }
            }
        }
    }

    memcpy64(dest->elems, dest->elems + sbase, delta*sizeof(Idx));

    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_node_set_init_union(re_node_set *dest, re_node_set *src1,
                       re_node_set *src2) {
    Idx i1, i2, id;
    if (src1 != NULL && src1->nelem > 0 && src2 != NULL && src2->nelem > 0) {
        dest->alloc = src1->nelem + src2->nelem;
        dest->elems = re_malloc(Idx, dest->alloc);
        if (__glibc_unlikely(dest->elems == NULL)) {
            return REG_ESPACE;
        }
    } else {
        if (src1 != NULL && src1->nelem > 0) {
            return re_node_set_init_copy(dest, src1);
        } else if (src2 != NULL && src2->nelem > 0) {
            return re_node_set_init_copy(dest, src2);
        } else {
            re_node_set_init_empty(dest);
        }
        return REG_NOERROR;
    }
    for (i1 = i2 = id = 0; i1 < src1->nelem && i2 < src2->nelem;) {
        if (src1->elems[i1] > src2->elems[i2]) {
            dest->elems[id++] = src2->elems[i2++];
            continue;
        }
        if (src1->elems[i1] == src2->elems[i2]) {
            ++i2;
        }
        dest->elems[id++] = src1->elems[i1++];
    }
    if (i1 < src1->nelem) {
        memcpy64(dest->elems + id, src1->elems + i1,
               (src1->nelem - i1)*sizeof(Idx));
        id += src1->nelem - i1;
    } else if (i2 < src2->nelem) {
        memcpy64(dest->elems + id, src2->elems + i2,
               (src2->nelem - i2)*sizeof(Idx));
        id += src2->nelem - i2;
    }
    dest->nelem = id;
    return REG_NOERROR;
}

static reg_errcode_t __attribute_warn_unused_result__
re_node_set_merge(re_node_set *dest, re_node_set *src) {
    Idx is, id, sbase, delta;
    if (src == NULL || src->nelem == 0) {
        return REG_NOERROR;
    }
    if (dest->alloc < 2*src->nelem + dest->nelem) {
        Idx new_alloc = 2*(src->nelem + dest->alloc);
        Idx *new_buffer = re_realloc(dest->elems, Idx, new_alloc);
        if (__glibc_unlikely(new_buffer == NULL)) {
            return REG_ESPACE;
        }
        dest->elems = new_buffer;
        dest->alloc = new_alloc;
    }

    if (__glibc_unlikely(dest->nelem == 0)) {

        DEBUG_ASSERT(dest->elems);
        dest->nelem = src->nelem;
        memcpy64(dest->elems, src->elems, src->nelem*sizeof(Idx));
        return REG_NOERROR;
    }

    for (sbase = dest->nelem + 2*src->nelem, is = src->nelem - 1,
        id = dest->nelem - 1;
         is >= 0 && id >= 0;) {
        if (dest->elems[id] == src->elems[is]) {
            is--, id--;
        } else if (dest->elems[id] < src->elems[is]) {
            dest->elems[--sbase] = src->elems[is--];
        } else {
            --id;
        }
    }

    if (is >= 0) {

        sbase -= is + 1;
        memcpy64(dest->elems + sbase, src->elems, (is + 1)*sizeof(Idx));
    }

    id = dest->nelem - 1;
    is = dest->nelem + 2*src->nelem - 1;
    delta = is - sbase + 1;
    if (delta == 0) {
        return REG_NOERROR;
    }

    dest->nelem += delta;
    for (;;) {
        if (dest->elems[is] > dest->elems[id]) {

            dest->elems[id + delta--] = dest->elems[is--];
            if (delta == 0) {
                break;
            }
        } else {

            dest->elems[id + delta] = dest->elems[id];
            if (--id < 0) {

                memcpy64(dest->elems, dest->elems + sbase, delta*sizeof(Idx));
                break;
            }
        }
    }

    return REG_NOERROR;
}

static bool __attribute_warn_unused_result__
re_node_set_insert(re_node_set *set, Idx elem) {
    Idx idx;

    if (set->alloc == 0) {
        return __glibc_likely(re_node_set_init_1(set, elem) == REG_NOERROR);
    }

    if (__glibc_unlikely(set->nelem) == 0) {

        DEBUG_ASSERT(set->elems);
        set->elems[0] = elem;
        ++set->nelem;
        return true;
    }

    if (set->alloc == set->nelem) {
        Idx *new_elems;
        set->alloc = set->alloc*2;
        new_elems = re_realloc(set->elems, Idx, set->alloc);
        if (__glibc_unlikely(new_elems == NULL)) {
            return false;
        }
        set->elems = new_elems;
    }

    if (elem < set->elems[0]) {
        for (idx = set->nelem; idx > 0; idx--) {
            set->elems[idx] = set->elems[idx - 1];
        }
    } else {
        for (idx = set->nelem; set->elems[idx - 1] > elem; idx--) {
            set->elems[idx] = set->elems[idx - 1];
        }
        DEBUG_ASSERT(set->elems[idx - 1] < elem);
    }

    set->elems[idx] = elem;
    ++set->nelem;
    return true;
}

static bool __attribute_warn_unused_result__
re_node_set_insert_last(re_node_set *set, Idx elem) {

    if (set->alloc == set->nelem) {
        Idx *new_elems;
        set->alloc = (set->alloc + 1)*2;
        new_elems = re_realloc(set->elems, Idx, set->alloc);
        if (__glibc_unlikely(new_elems == NULL)) {
            return false;
        }
        set->elems = new_elems;
    }

    set->elems[set->nelem++] = elem;
    return true;
}

static bool __attribute__((pure))
re_node_set_compare(re_node_set *set1, re_node_set *set2) {
    Idx i;
    if (set1 == NULL || set2 == NULL || set1->nelem != set2->nelem) {
        return false;
    }
    for (i = set1->nelem; --i >= 0;) {
        if (set1->elems[i] != set2->elems[i]) {
            return false;
        }
    }
    return true;
}

static Idx __attribute__((pure))
re_node_set_contains(re_node_set *set, Idx elem) {
    int64 idx, right, mid;
    if (set->nelem <= 0) {
        return 0;
    }

    idx = 0;
    right = set->nelem - 1;
    while (idx < right) {
        mid = (idx + right) / 2;
        if (set->elems[mid] < elem) {
            idx = mid + 1;
        } else {
            right = mid;
        }
    }
    return set->elems[idx] == elem ? idx + 1 : 0;
}

static void
re_node_set_remove_at(re_node_set *set, Idx idx) {
    if (idx < 0 || idx >= set->nelem) {
        return;
    }
    --set->nelem;
    for (; idx < set->nelem; idx++) {
        set->elems[idx] = set->elems[idx + 1];
    }
}

static Idx
re_dfa_add_node(re_dfa_t *dfa, re_token_t token) {
    if (__glibc_unlikely(dfa->nodes_len >= dfa->nodes_alloc)) {
        int64 new_nodes_alloc = dfa->nodes_alloc*2;
        Idx *new_nexts, *new_indices;
        re_node_set *new_edests, *new_eclosures;
        re_token_t *new_nodes;

        int64 max_object_size
            = MAX(sizeof(re_token_t), MAX(sizeof(re_node_set), sizeof(Idx)));
        if (__glibc_unlikely(MIN(IDX_MAX, SIZE_MAX / max_object_size)
                             < new_nodes_alloc)) {
            return -1;
        }

        new_nodes = re_realloc(dfa->nodes, re_token_t, new_nodes_alloc);
        if (__glibc_unlikely(new_nodes == NULL)) {
            return -1;
        }
        dfa->nodes = new_nodes;
        new_nexts = re_realloc(dfa->nexts, Idx, new_nodes_alloc);
        new_indices = re_realloc(dfa->org_indices, Idx, new_nodes_alloc);
        new_edests = re_realloc(dfa->edests, re_node_set, new_nodes_alloc);
        new_eclosures
            = re_realloc(dfa->eclosures, re_node_set, new_nodes_alloc);
        if (__glibc_unlikely(new_nexts == NULL || new_indices == NULL
                             || new_edests == NULL || new_eclosures == NULL)) {
            re_free(new_nexts);
            re_free(new_indices);
            re_free(new_edests);
            re_free(new_eclosures);
            return -1;
        }
        dfa->nexts = new_nexts;
        dfa->org_indices = new_indices;
        dfa->edests = new_edests;
        dfa->eclosures = new_eclosures;
        dfa->nodes_alloc = new_nodes_alloc;
    }
    dfa->nodes[dfa->nodes_len] = token;
    dfa->nodes[dfa->nodes_len].constraint = 0;

    dfa->nexts[dfa->nodes_len] = -1;
    re_node_set_init_empty(dfa->edests + dfa->nodes_len);
    re_node_set_init_empty(dfa->eclosures + dfa->nodes_len);
    return dfa->nodes_len++;
}

static re_hashval_t
calc_state_hash(re_node_set *nodes, uint32 context) {
    re_hashval_t hash = nodes->nelem + context;
    Idx i;
    for (i = 0; i < nodes->nelem; i++) {
        hash += nodes->elems[i];
    }
    return hash;
}
static re_dfastate_t *__attribute_warn_unused_result__
re_acquire_state(reg_errcode_t *err, re_dfa_t *dfa, re_node_set *nodes) {
    re_hashval_t hash;
    re_dfastate_t *new_state;
    struct re_state_table_entry *spot;
    Idx i;

    if (__glibc_unlikely(nodes->nelem == 0)) {
        *err = REG_NOERROR;
        return NULL;
    }
    hash = calc_state_hash(nodes, 0);
    spot = dfa->state_table + (hash & dfa->state_hash_mask);

    for (i = 0; i < spot->num; i++) {
        re_dfastate_t *state = spot->array[i];
        if (hash != state->hash) {
            continue;
        }
        if (re_node_set_compare(&state->nodes, nodes)) {
            return state;
        }
    }

    new_state = create_ci_newstate(dfa, nodes, hash);
    if (__glibc_unlikely(new_state == NULL)) {
        *err = REG_ESPACE;
    }

    return new_state;
}
static re_dfastate_t *__attribute_warn_unused_result__
re_acquire_state_context(reg_errcode_t *err, re_dfa_t *dfa, re_node_set *nodes,
                         uint32 context) {
    re_hashval_t hash;
    re_dfastate_t *new_state;
    struct re_state_table_entry *spot;
    Idx i;

    if (nodes->nelem == 0) {
        *err = REG_NOERROR;
        return NULL;
    }
    hash = calc_state_hash(nodes, context);
    spot = dfa->state_table + (hash & dfa->state_hash_mask);

    for (i = 0; i < spot->num; i++) {
        re_dfastate_t *state = spot->array[i];
        if (state->hash == hash && state->context == context
            && re_node_set_compare(state->entrance_nodes, nodes)) {
            return state;
        }
    }

    new_state = create_cd_newstate(dfa, nodes, context, hash);
    if (__glibc_unlikely(new_state == NULL)) {
        *err = REG_ESPACE;
    }

    return new_state;
}

static reg_errcode_t __attribute_warn_unused_result__
register_state(re_dfa_t *dfa, re_dfastate_t *newstate, re_hashval_t hash) {
    struct re_state_table_entry *spot;
    reg_errcode_t err;
    Idx i;

    newstate->hash = hash;
    err = re_node_set_alloc(&newstate->non_eps_nodes, newstate->nodes.nelem);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        return REG_ESPACE;
    }
    for (i = 0; i < newstate->nodes.nelem; i++) {
        Idx elem = newstate->nodes.elems[i];
        if (!IS_EPSILON_NODE(dfa->nodes[elem].type)) {
            if (!re_node_set_insert_last(&newstate->non_eps_nodes, elem)) {
                return REG_ESPACE;
            }
        }
    }

    spot = dfa->state_table + (hash & dfa->state_hash_mask);
    if (__glibc_unlikely(spot->alloc <= spot->num)) {
        Idx new_alloc = 2*spot->num + 2;
        re_dfastate_t **new_array
            = re_realloc(spot->array, re_dfastate_t *, new_alloc);
        if (__glibc_unlikely(new_array == NULL)) {
            return REG_ESPACE;
        }
        spot->array = new_array;
        spot->alloc = new_alloc;
    }
    spot->array[spot->num++] = newstate;
    return REG_NOERROR;
}

static void
free_state(re_dfastate_t *state) {
    re_node_set_free(&state->non_eps_nodes);
    re_node_set_free(&state->inveclosure);
    if (state->entrance_nodes != &state->nodes) {
        re_node_set_free(state->entrance_nodes);
        re_free(state->entrance_nodes);
    }
    re_node_set_free(&state->nodes);
    re_free(state->word_trtable);
    re_free(state->trtable);
    re_free(state);
}

static re_dfastate_t *__attribute_warn_unused_result__
create_ci_newstate(re_dfa_t *dfa, re_node_set *nodes, re_hashval_t hash) {
    Idx i;
    reg_errcode_t err;
    re_dfastate_t *newstate;

    newstate = (re_dfastate_t *)calloc(sizeof(re_dfastate_t), 1);
    if (__glibc_unlikely(newstate == NULL)) {
        return NULL;
    }
    err = re_node_set_init_copy(&newstate->nodes, nodes);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        re_free(newstate);
        return NULL;
    }

    newstate->entrance_nodes = &newstate->nodes;
    for (i = 0; i < nodes->nelem; i++) {
        re_token_t *node = dfa->nodes + nodes->elems[i];
        re_token_type_t type = node->type;
        if (type == CHARACTER && !node->constraint) {
            continue;
        }

        if (type == END_OF_RE) {
            newstate->halt = 1;
        } else if (type == OP_BACK_REF) {
            newstate->has_backref = 1;
        } else if (type == ANCHOR || node->constraint) {
            newstate->has_constraint = 1;
        }
    }
    err = register_state(dfa, newstate, hash);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        free_state(newstate);
        newstate = NULL;
    }
    return newstate;
}

static re_dfastate_t *__attribute_warn_unused_result__
create_cd_newstate(re_dfa_t *dfa, re_node_set *nodes, uint32 context,
                   re_hashval_t hash) {
    Idx i, nctx_nodes = 0;
    reg_errcode_t err;
    re_dfastate_t *newstate;

    newstate = (re_dfastate_t *)calloc(sizeof(re_dfastate_t), 1);
    if (__glibc_unlikely(newstate == NULL)) {
        return NULL;
    }
    err = re_node_set_init_copy(&newstate->nodes, nodes);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        re_free(newstate);
        return NULL;
    }

    newstate->context = context;
    newstate->entrance_nodes = &newstate->nodes;

    for (i = 0; i < nodes->nelem; i++) {
        re_token_t *node = dfa->nodes + nodes->elems[i];
        re_token_type_t type = node->type;
        uint32 constraint = node->constraint;

        if (type == CHARACTER && !constraint) {
            continue;
        }

        if (type == END_OF_RE) {
            newstate->halt = 1;
        } else if (type == OP_BACK_REF) {
            newstate->has_backref = 1;
        }

        if (constraint) {
            if (newstate->entrance_nodes == &newstate->nodes) {
                re_node_set *entrance_nodes = re_malloc(re_node_set, 1);
                if (__glibc_unlikely(entrance_nodes == NULL)) {
                    free_state(newstate);
                    return NULL;
                }
                newstate->entrance_nodes = entrance_nodes;
                if (re_node_set_init_copy(newstate->entrance_nodes, nodes)
                    != REG_NOERROR) {
                    free_state(newstate);
                    return NULL;
                }
                nctx_nodes = 0;
                newstate->has_constraint = 1;
            }

            if (NOT_SATISFY_PREV_CONSTRAINT(constraint, context)) {
                re_node_set_remove_at(&newstate->nodes, i - nctx_nodes);
                ++nctx_nodes;
            }
        }
    }
    err = register_state(dfa, newstate, hash);
    if (__glibc_unlikely(err != REG_NOERROR)) {
        free_state(newstate);
        newstate = NULL;
    }
    return newstate;
}
