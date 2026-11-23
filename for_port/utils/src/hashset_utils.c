/********************************************************************
                    HASH-BASED SET MODULE IMPLEMENTATION
********************************************************************/

#include "hashset_utils.h"

// ------------------------------------ Utilities ------------------------------------------


// ------------------------------------- API -----------------------------------------------

// return new hset se1 - se2
hset             hset_init_minus(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);

    if (hset_getype(se1) != hset_getype(se2) )
         userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1)), value64_typename(hset_getype(se2) ) );

    // simple implementation
    hset res = hset_init(se1->sz - 1, se1->flags);
    for (int i = 0; i < se1->sz; i++){
        const hset_elem *el = se1->table[i];
        while (el){
            if (!hset_get(se2, el->v) )
                hset_set(&res, el->v);
            el = el->next;
        }
    }
    return logsimpleret(res, "Created minus - total %d", se1->count);
}

// just intersect with construct
hset                        hset_init_intersect(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);

    if (hset_getype(se1) != hset_getype(se2) )
          userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1)), value64_typename(hset_getype(se2) ) );

    // simple implementation
    hset res = hset_init(se1->sz - 1, se1->flags);
    if (se1->count > 0 && se2->count > 0)
        for (int i = 0; i < se1->sz; i++){
            const hset_elem *el = se1->table[i];
            while (el){
                if (hset_get(se2, el->v) )  // omg
                    hset_set(&res, el->v);
                el = el->next;
            }
        }
    return logsimpleret(res, "Created intersect - total %d", res.count);
}

// simm diff with construct
hset                        hset_init_symmdiff(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);
    if (hset_getype(se1) != hset_getype(se2) )
         userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1)), value64_typename(hset_getype(se2) ) );
    hset res = hset_init(MAX(se1->sz, se2->sz) - 1, se1->flags);

    for (int i = 0; i < se1->sz; i++){
        const hset_elem *el = se1->table[i];
        while (el){
            if (!hset_get(se2, el->v))
                hset_set(&res, el->v);
            el = el->next;
        }
    }
    for (int i = 0; i < se2->sz; i++){
        const hset_elem *el = se2->table[i];
        while (el){
            if (!hset_get(se1, el->v))
                hset_set(&res, el->v);
            el = el->next;
        }
    }

    return logsimpleret(res, "Created symm diff - total %d", res.count);
}

// union with construct
hset                        hset_init_union(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers");
    if (hset_getype(se1) != hset_getype(se2) )
         userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1)), value64_typename(hset_getype(se2) ) );
    hset res = hset_init(MAX(se1->sz, se2->sz) - 1, se1->flags);

    for (int i = 0; i < se1->sz; i++){
        const hset_elem *el = se1->table[i];
        while (el){
            hset_set(&res, el->v);
            el = el->next;
        }
    }
    for (int i = 0; i < se2->sz; i++){
        const hset_elem *el = se2->table[i];
        while (el){
            hset_set(&res, el->v);
            el = el->next;
        }
    }
    return logsimpleret(res, "Created union - total %d", res.count);
}

// check if all of se2 in se1 strictly or not
bool                        hset_subset_check(const hset *restrict se1, const hset *restrict se2, bool strict){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);

    if (hset_getype(se1) != hset_getype(se2) )
        userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1) ), value64_typename(hset_getype(se2) ) );

    if ( (!strict && hset_cnt(se1) > hset_cnt(se2) ) ||
        (strict && hset_cnt(se1) >= hset_cnt(se2) ) )
        return logsimpleret(false, "Count of se1 %d more (%s) that count of se2 %d", hset_cnt(se1), bool_str(strict), hset_cnt(se2) );
    // check that EVERY element in se1 exists in se2
    for (int i = 0; i < se1->sz; i++){
        const hset_elem *el = se1->table[i];
        while (el){
            if (!hset_get(se2, el->v) )
                return logsimpleret(false, "Not matched as in");
            el = el->next;
        }
    }
    return logsimpleret(true, "Matched %s as in", strict ? "strict": "");
}
// if not exists se2 in se1
bool                        hset_notexists(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);
    // refsctor tha to common
    if (hset_getype(se1) != hset_getype(se2) )
        userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1) ), value64_typename(hset_getype(se2) ) );
    HSET_FOREACH(se2, val){
        if (hset_get(se1, val) ){
            value64_fprint(logfile, val, hset_getype(se2));
            return logsimpleret(false, "Found element in se1");
        }
    }
    return logsimpleret(true, "Not found - OK");
}
// if exists any of se2 in se1
bool                        hset_any(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);
    // refsctor tha to common
    if (hset_getype(se1) != hset_getype(se2) )
        userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1) ), value64_typename(hset_getype(se2) ) );
    HSET_FOREACH(se2, val){
        if (hset_get(se1, val) ){
            value64_fprint(logfile, val, hset_getype(se2));
            return logsimpleret(true, "Element of se2 Found in se1");
        }
    }
    return logsimpleret(false, "All not found - OK");
}

// se1 -= se2 as SET, returns count of deleted element
hset                       *hset_minus(hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);
    // TODO: rework checkers!!! at least move that into check_it()
    if (hset_getype(se1) != hset_getype(se2) )
        userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1) ), value64_typename(hset_getype(se2) ) );

    int     cnt = 0;
    if ( !(hset_cnt(se1) == 0 || hset_cnt(se2) == 0) ){
        // foreach elements in se2 try to delete it from se1
        for (int i = 0; i < se2->sz; i++){
            const hset_elem *el = se2->table[i];
            while (el){
                cnt += (int) hset_del(se1, el->v);    // +1 if reallt deleted
                el = el->next;
            }
        }
    }
    return logsimpleret(se1, "Removed %d elements", cnt);
}

// se1 insersect= se2 as SET
hset                       *hset_intersect(hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);
    // TODO: rework checkers!!! at least move that into check_it()
    if (hset_getype(se1) != hset_getype(se2) )
        userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1) ), value64_typename(hset_getype(se2) ) );

    // foreach elements in se2 try to delete it from se1
    int     delcnt = 0;
    for (int i = 0; i < se1->sz; i++){
        const hset_elem *el = se1->table[i];
        while (el){
            hset_elem *next = el->next;
            if (!hset_get(se2, el->v) )
                delcnt += (int) hset_del(se1, el->v);    // +1 if reallt deleted
            el = next;
        }
    }
    return logsimpleret(se1, "%d elements remains, deleted %d", se1->count, delcnt);
}

// se1 symmdiff= se2 as SET
hset                       *hset_symmdiff(hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);
    // TODO: rework checkers!!! at least move that into check_it()
    if (hset_getype(se1) != hset_getype(se2) )
        userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1) ), value64_typename(hset_getype(se2) ) );

    // done via constructor
    hset tmp = hset_init_symmdiff(se1, se2);
    hset_moveall(se1, &tmp);       // no need to free tmp!!!!
    return logsimpleret(se1, "Done, new cnt = %d", se1->count);
}
// union= as SET
hset            *hset_union(hset *restrict se1, const hset *restrict se2){ 
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);
    // TODO: rework checkers!!! at least move that into check_it()
    if (hset_getype(se1) != hset_getype(se2) )
        userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1) ), value64_typename(hset_getype(se2) ) );

    int         addcnt = 0;
    if (se2->count > 0){
        for (int i = 0; i < se2->sz; i++){
            const hset_elem *el = se2->table[i];
            while (el){
                addcnt += hset_set(se1, el->v);
                el = el->next;
            }
        }
    }
    return  logsimpleret(se1, "%d elements remains, added from se2 %d", se1->count, addcnt);
}

// --------------------------------------- ITERATORS ---------------------------------------

void                        hset_const_foreach(const hset *se, hset_const_proc_t proc) {
    for (int i = 0; i < se->sz; i++) {
        const hset_elem *el = se->table[i];
        while (el) {
            proc(el->v);   // передаём копию
            el = el->next;
        }
    }
}
// ----------------------------------------- REDUCE -----------------------------------------

// Core engine reduce
hset_accum                  hset_reduce(const hset *se, hset_accum init, hset_reduce_func func) {
    hset_accum acc = init;
    HSET_FOREACH(se, var) {
        func(&acc, var);
    }
    return acc;
}
// conditional engine
hset_accum                  hset_reduce_filtered(const hset *restrict se, hset_accum init, hset_reduce_func func,
                                valur64_predicate_t pred, const value64_tarray *restrict params) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");

    // Без предиката – обычная свёртка
    if (!pred)
        return hset_reduce(se, init, func);

    hset_accum acc = init;
    HSET_FOREACH(se, var) {
        if (pred(var, params)) {
            func(&acc, var);
        }
    }
    return acc;
}
// ------------------------------------- REDUCE IMPL -----------------------------------------

void                        hset_sum_int(hset_accum *acc, value64 v) {
    acc->value.ival += v.ival;
    acc->count++;
}
void                        hset_count_int(hset_accum *acc, value64 v) {
    (void) v;
    acc->count++;
}

void                        hset_max_int(hset_accum *acc, value64 v) {
    if (acc->count == 0 || v.ival > acc->value.ival) {
        acc->value.ival = v.ival;
        acc->count = 1;   // помечаем, что значение уже есть
    }
}
void                        hset_min_int(hset_accum *acc, value64 v) {
    if (acc->count == 0 || v.ival < acc->value.ival) {
        acc->value.ival = v.ival;
        acc->count = 1;
    }
}

/*void                        hset_avg_int(hset_accum *acc, value64 v) {
    acc->value.ival += v.ival;
    acc->count++;
}*/
// long

void                        hset_sum_lng(hset_accum *acc, value64 v) {
    acc->value.lval += v.lval;
    acc->count++;
}
void                        hset_max_lng(hset_accum *acc, value64 v) {
    if (acc->count == 0 || v.lval > acc->value.lval) {
        acc->value.lval = v.lval;
        acc->count = 1;   // помечаем, что значение уже есть
    }
}
void                        hset_min_lng(hset_accum *acc, value64 v) {
    if (acc->count == 0 || v.lval < acc->value.lval) {
        acc->value.lval = v.lval;
        acc->count = 1;
    }
}
// double
void                        hset_sum_dbl(hset_accum *acc, value64 v) {
    if (isfinite(v.dval)) {
        acc->value.dval += v.dval;
        acc->count++;
    }
}

void                        hset_count_dbl(hset_accum *acc, value64 v) {
    if (isfinite(v.dval))
        acc->count++;
}

void                        hset_max_dbl(hset_accum *acc, value64 v) {
    if (!isfinite(v.dval))
        return;
    if (acc->count == 0 || v.dval > acc->value.dval) {
        acc->value.dval = v.dval;
        acc->count = 1;   // помечаем, что значение уже есть
    }
}

void                        hset_min_dbl(hset_accum *acc, value64 v) {
    if (!isfinite(v.dval))
        return;
    if (acc->count == 0 || v.dval < acc->value.dval) {
        acc->value.dval = v.dval;
        acc->count = 1;
    }
}

/*void                        hset_avg_dbl(hset_accum *acc, value64 v) {
    if (isfinite(v.dval)) {
        acc->value.dval += v.dval;
        acc->count++;
    }
}*/

// fs
// ======================= FS reduce callbacks =======================
// TEMPORARY, because fs doesn't provide comparation
/*static int                   hset_fs_cmp_wrap(const fs* restrict s1, const fs* restrict s2){
    invraisecode(ERR_NULLABLE_PTR, s1 && s2 && s1->v,
        "Null pointer %p %p %p", s1, s2, s1 ? s1->v: NULL);

    if (!s2->v)
        return 1;   // s1 valuable so > null
    return fs_cmp(s1, s2);
}*/

void                         hset_count_fs(hset_accum *acc, value64 v) {
    const fs *s = value64_fs(v);
    if (!fs_isnull(s) )
        acc->count++;
}
// compare as literal, find max
void                        hset_max_fs(hset_accum *acc, value64 v) {
    const fs    *cur = value64_fs(v);
    if (fs_isnull(cur) )    // ignore null and empry
        return;
    //if (acc->count == 0)
      //  hset_accum_fs(acc) = fs_create(); // body in heap, v isn't allocated
    if (acc->count == 0 || fs_cmp /*hset_fs_cmp_wrap*/(cur, hset_accum_fs(acc) ) > 0)
        fs_cpy(hset_accum_fs(acc), *cur);
    acc->count++;
}

// compare as literal, min (even 0)
void                        hset_min_fs(hset_accum *acc, value64 v) {
    const fs    *cur = value64_fs(v);
    if (fs_isnull(cur) )    // ignore null and empry
        return;
    //if (acc->count == 0)
      //  hset_accum_fs(acc) = fs_create(); // body in heap, v isn't allocated
    if (acc->count == 0 || fs_cmp /*hset_fs_cmp_wrap*/(cur, acc->value.fsval ) < 0)
        fs_cpy(hset_accum_fs(acc), *cur);
    acc->count++;
}
// compare as length, find max
void                        hset_maxlen_fs(hset_accum *acc, value64 v) {
    const fs    *cur = value64_fs(v);
    if (fs_isnull(cur) )    // ignore null and null fs (fs.v == NULL)
        return;
    //if (acc->count == 0)
      //  hset_accum_fs(acc) = fs_create(); // body in heap, v isn't allocated
    if (acc->count == 0 || fs_len(cur) > fs_len(hset_accum_fs(acc) ) )
        fs_cpy(hset_accum_fs(acc), *cur);
    acc->count++;
}

// compare as literal, min (even 0)
void                        hset_minlen_fs(hset_accum *acc, value64 v) {
    const fs    *cur = value64_fs(v);
    if (fs_isnull(cur) )    // ignore null and empry
        return;
    //if (acc->count == 0)
      //  hset_accum_fs(acc) = fs_create(); // body in heap, v isn't allocated
    if (acc->count == 0 || fs_len(cur) < fs_len(hset_accum_fs(acc) ) )
        fs_cpy(hset_accum_fs(acc), *cur);
    acc->count++;
}
// sum over len
void                        hset_sumlen_fs(hset_accum *acc, value64 v) {
    const fs *cur = value64_fs(v);
    if (!fs_isnull(cur))
        hset_accum_long(acc) += fs_len(cur);     // hset_long() += fs_len(cur); 
    acc->count++;
}

void                        hset_agg_fs     (hset_accum *acc, value64 v) {
    const fs *cur = value64_fs(v);
    if (fs_isnull(cur))
        return;     // totally ignore ? OK for now

    if (acc->count > 0 && acc->sep) {   // not optimized, but ok for now
        // добавляем разделитель перед очередным элементом
        fs_cat(hset_accum_fs(acc), fsliteral(acc->sep));
    }
    fs_cat(hset_accum_fs(acc), *cur);   // дописываем саму строку
    acc->count++;
}

// ------------------------------------------ FILTER -----------------------------------------
// Core engine filter 1 parameter
hset                       *hset_filter(hset *restrict se, valur64_predicate_t pred, const value64_tarray *restrict params) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");

    int     cnt = 0;
    HSET_FOREACH_DEL(se, var) {
        if (!pred(var, params) )
            cnt += hset_del(se, var);
    }
    return logsimpleret(se, "Deleted by apply filter %d", cnt);
}
// core engine construct 1 par
hset                        hset_init_filter(const hset *restrict se, valur64_predicate_t pred, const value64_tarray *restrict params) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");

    hset res = hset_init(se->sz, hset_getype(se) );
    HSET_FOREACH(se, var) {
        if (pred(var, params) )
            hset_set(&res, var);   // копирование, оригинал не трогаем
    }
    return logsimpleret(res, "Filtered: %d elements remain in new set", res.count);
}
// Core engine filter 1 parameter
hset                       *hset_filter2(hset *restrict se, hset_predicate2_t pred2, value64 data1, value64 data2) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");

    int     cnt = 0;
    HSET_FOREACH_DEL(se, var) {
        if (!pred2(var, data1, data2) )
            cnt += hset_del(se, var);
    }
    return logsimpleret(se, "Deleted by apply filter %d", cnt);
}
// core engine construct 2 par
extern hset                  hset_init_filter2(const hset *restrict se, value64_predicate_t pred2, /*value64 data1, value64 data2*/
            const value64_tarray *restrict params) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");

    hset res = hset_init(se->sz, hset_getype(se) );
    HSET_FOREACH(se, var) {
        if (pred2(var, params) )
            hset_set(&res, var);   // копирование, оригинал не трогаем
    }
    return logsimpleret(res, "Filtered: %d elements remain in new set", res.count);
}

// -------------------------- simplifyers over filters ----------------------------------
// sql-like create as select:fs where predicate:int
hset                        hset_create_fs_int_filter(const hset *restrict se, valur64_predicate_t filter, int data){
    invraisecode(ERR_NULLABLE_PTR, se != NULL,
        "Null pointers %p", se);
    invraisecode(ERR_UNSUPPORTED_TYPE, hset_getype(se) == VALUE64_INT /*|| hset_getype(se) == VALUE64_LONG */,
        "Only VALUE64_INT/LONG is supported");
    hset tmp = hset_init_filter(se, filter, /*LITERAL64_INT(data) */ V64TYP_INTLIST(data) );
    return logsimpleret(tmp, "Created as int filter %d elems", hset_cnt(&tmp) );
}
// sql-like apply :fs where  predicate:int
hset                       *hset_apply_fs_int_filter(hset *restrict se, valur64_predicate_t filter, int data){
    invraisecode(ERR_NULLABLE_PTR, se != NULL,
        "Null pointers %p", se);
    invraisecode(ERR_UNSUPPORTED_TYPE, hset_getype(se) == VALUE64_INT /*|| hset_getype(se) == VALUE64_LONG */,
        "Only VALUE64_INT/LONG is supported");
    hset_filter(se, filter, /* LITERAL64_INT(data) */ V64TYP_INTLIST(data) );
    return logsimpleret(se, "Remained after int filter %d elems", hset_cnt(se) );
}

// sql-like create as select:fs where predicate:str
hset                        hset_create_fs_str_filter(const hset *restrict se, valur64_predicate_t filter, const char *restrict pattern,) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL && pattern != NULL,
            "Null pointers %p %p", se, pattern);
    invraisecode(ERR_UNSUPPORTED_TYPE, hset_getype(se) == VALUE64_FS,
            "Only VALUE64_FS is supported");
    //fs l = FSLITERAL(pattern);  // no need to free
    hset tmp = hset_init_filter(se, filter, /* LITERAL64_STR(pattern)*/ V64TYP_FSLIST_FROMSTR(pattern) );
    return logsimpleret(tmp, "Created as str filter %d elems", hset_cnt(&tmp) );
}
// sql-like apply :fs where predicate:str
hset                       *hset_apply_fs_str_filter(hset *restrict se, valur64_predicate_t filter, const char *restrict pattern) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL && pattern != NULL,
            "Null pointers %p %p", se, pattern);
    invraisecode(ERR_UNSUPPORTED_TYPE, hset_getype(se) == VALUE64_FS,
            "Only VALUE64_FS is supported");
    hset_filter(se, filter, /*LITERAL64_STR(pattern) */  V64TYP_FSLIST_FROMSTR(pattern) );
    return logsimpleret(se, "Remained after str filter %d elems", hset_cnt(se) );
}
// sql-like create as select:int where predicate:int
hset                        hset_create_int_int_filter(const hset *restrict se, valur64_predicate_t filter, int value) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");
    invraisecode(ERR_UNSUPPORTED_TYPE,
        hset_getype(se) == VALUE64_INT /* || hset_getype(se) == VALUE64_LONG */, // long is diabled for now
        "Only VALUE64_INT supported");
    hset tmp = hset_init_filter(se, filter, V64TYP_INTLIST(value) );
    return logsimpleret(tmp, "Created %d elems", hset_cnt(&tmp) );
}

// sql-like aoply :int where predicate:int
hset                       *hset_apply_int_int_filter(hset *restrict se,  valur64_predicate_t filter, int value) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");
    invraisecode(ERR_UNSUPPORTED_TYPE,
        hset_getype(se) == VALUE64_INT /* || hset_getype(se) == VALUE64_LONG*/, // long is diabled for now
        "Only VALUE64_INT supported");
    hset_filter(se, filter, V64TYP_INTLIST(value) );
    return logsimpleret(se, "Remained %d elems", hset_cnt(se) );
}

// int - (int, int)
// sql-like create as select:int where predicate:(int, int)
hset                        hset_create_int_int_filter2(const hset *restrict se, value64_predicate_t filter2, int value1, int value2) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");
    invraisecode(ERR_UNSUPPORTED_TYPE,
        hset_getype(se) == VALUE64_INT /* || hset_getype(se) == VALUE64_LONG */, // long is diabled for now
        "Only VALUE64_INT supported");
    hset tmp = hset_init_filter2(se, filter2, V64TYP_INTLIST(value1, value2) );
    return logsimpleret(tmp, "Created %d elems", hset_cnt(&tmp) );
}
// sql-like apply:int where predicate:(int, int)
hset                       *hset_apply_int_int_filter2(hset *restrict se, value64_predicate_t filter2, int value1, int value2) {
    invraisecode(ERR_NULLABLE_PTR, se != NULL, "Null pointer");
    invraisecode(ERR_UNSUPPORTED_TYPE,
        hset_getype(se) == VALUE64_INT /* || hset_getype(se) == VALUE64_LONG*/, // long is diabled for now
        "Only VALUE64_INT supported");
    hset_filter2(se, filter2, V64TYP_INTLIST(value1, value2) );
    return logsimpleret(se, "Remained %d elems", hset_cnt(se) );
}

// ---------------------------------- MAPPERS ---------------------------------
// ------------------------------- engine ------------------------------------------
// constructor only! No apply engine
hset                        hset_init_map(const hset *restrict src, hset_map_t map, const value64_params_t *restrict params) {
    invraisecode(ERR_NULLABLE_PTR, src != NULL, "Null pointer");
    hset res = hset_init(src->sz, hset_getype(src));

    HSET_FOREACH(src, var) {
        value64 mapped = map(var, params);  // params is NULL normal!
        hset_move(&res, &mapped);
    }

    return logsimpleret(res, "Mapped (new set) %d elems", res.count);
}

// ---------------------------------------- Testing ------------------------------------------
#ifdef HASHSET_UTILS_TESTING

#include "test.h"
#include "array.h"
#include <time.h>

//types, macro for testing
#define HSET_HAS_FS(se, path) \
    ({ \
        value64 _v = value64_createfs_asstr(path); \
        bool _res = hset_get((se), _v); \
        value64_freefs(&_v); \
        _res; \
    })

#define HSET_HAS_INT(se, val)  hset_get((se), LITERAL64_INT(val))

// Преобразование: добавить число (один параметр)
// v - VALUE64_INT, p->v[0] - VALUE64_INT
static value64                  test_map_int_add_int(value64 v, const value64_params_t* p) {
    return LITERAL64_INT(value64_int(v) + value64_int( value64_getpar1(p) ) );
    // or just v->ival += value64_int( value64_getpar1(p) )
}

// Преобразование: добавить суффикс к пути (один параметр-строка)
// v - VALUE64_FS, p->v[0]  VALUE64_STR
static value64                  test_map_fs_append_str(value64 v, const value64_params_t* p) {
    value64 newv = value64_clone(v, VALUE64_FS);
    const char *suffix = value64_str( value64_getpar1(p) ); // no checking here, p->v[0] MUST be VALUE64_STR
    if (suffix)
        fs_catstr(value64_fs(newv), suffix);
    return newv;
}

// ------------------------- TEST 8 ---------------------------------
static TestStatus
tf8(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: empty in empty", ++subnum);
    {
        hset empty1 = hset_init(10, VALUE64_INT);
        hset empty2 = hset_init(100, VALUE64_INT);

        test_validatefree(
            hset_validate(stdout, &empty1) && hset_validate(stdout, &empty1),
            (hset_free(&empty1), hset_free(&empty2) ), "Validation failed empty"
        );
        test_validatefree(
            hset_in(&empty1, &empty2), (hset_free(&empty1), hset_free(&empty2) ),
            "Empty set should be subset of empty set"
        );
        hset_free(&empty1);
        hset_free(&empty2);
    }
    test_sub("subtest %d: empty in nonempty", ++subnum);
    {
        int vals[] = {1, 3, 5, 7, 9};
        hset empty    = hset_init(10, VALUE64_INT);
        hset nonempty = hset_from_intarr(vals, COUNT(vals) );

        test_validatefree(
            hset_validate(stdout, &empty) && hset_validate(stdout, &nonempty),
            (hset_free(&empty), hset_free(&nonempty) ), "Validation failed empty"
        );
        test_validatefree(
            hset_in(&empty, &nonempty), (hset_free(&empty), hset_free(&nonempty) ),
            "Empty set should be subset of any set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    test_sub("subtest %d: nonempty not in empty", ++subnum);
    {
        int vals[] = {1, 3, 5, 7, 9};
        hset empty    = hset_init(10, VALUE64_INT);
        hset nonempty = hset_from_intarr(vals, COUNT(vals) );

        test_validatefree(
            !hset_in(&nonempty, &empty), (hset_free(&empty), hset_free(&nonempty) ),
            "Non-empty set should NOT be subset of empty set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    test_sub("subtest %d: equal sets", ++subnum);
    {
        int all_vals[] = {1, 3, 5, 7, 9};

        hset superset = hset_from_intarr(all_vals, COUNT(all_vals) );
        hset subset   = hset_from_intarr(all_vals, COUNT(all_vals) );

        test_validatefree(
            hset_in(&subset, &superset), (hset_free(&superset), hset_free(&subset) ),
            "Subset should be strict in equal set"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    test_sub("subtest %d: subset in superset", ++subnum);
    {
        int all_vals[] = {1, 3, 5, 7, 9};
        int sub_vals[] = {1, 5, 9};

        hset superset = hset_from_intarr(all_vals, COUNT(all_vals) );
        hset subset   = hset_from_intarr(sub_vals, COUNT(sub_vals) );

        test_validatefree(
            hset_in(&subset, &superset), (hset_free(&superset), hset_free(&subset) ),
            "Subset should be in superset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }

    test_sub("subtest %d: superset not in subset", ++subnum);
    {
        int all_vals[] = {1, 3, 5, 7, 9};
        int sub_vals[] = {1, 5, 9};

        hset superset = hset_from_intarr(all_vals, COUNT(all_vals) );
        hset subset   = hset_from_intarr(sub_vals, COUNT(sub_vals) );

        test_validatefree(
            !hset_in(&superset, &subset), (hset_free(&superset), hset_free(&subset) ),
            "Superset should NOT be in subset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    test_sub("subtest %d: type mismatch raise SIGINT", ++subnum);
    {
        int int_vals[] = {1, 2, 3};
        hset int_set = hset_from_intarr(int_vals, 3);
        hset dbl_set = hset_init(10, VALUE64_DBL);
        hset_set(&dbl_set, LITERAL64_DBL(1.0));
        hset_set(&dbl_set, LITERAL64_DBL(2.0));
        if (!try () ){
            test_validatefree(
                !hset_in(&int_set, &dbl_set),
                (hset_free(&int_set), hset_free(&dbl_set)),
                "Different types should never be subset"
            );
        } else {
            //err_printstacktrace();
            hset_free(&int_set);
            hset_free(&dbl_set);
            return logret(TEST_PASSED, "done");
        }
        return logret(TEST_FAILED, "done");
    }
    /* // fs
    */
    test_sub("subtest %d: empty FS in empty FS", ++subnum);
    {
        hset empty1 = hset_init_fs(10);
        hset empty2 = hset_init_fs(10);

        test_validatefree(
            hset_validate(stdout, &empty1) && hset_validate(stdout, &empty2),
            (hset_free(&empty1), hset_free(&empty2)),
            "Validation failed empty FS"
        );
        test_validatefree(
            hset_in(&empty1, &empty2),
            (hset_free(&empty1), hset_free(&empty2)),
            "Empty FS set should be subset of empty FS set"
        );
        hset_free(&empty1);
        hset_free(&empty2);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: empty FS in nonempty FS", ++subnum);
    {
        hset empty    = hset_init_fs(10);
        hset nonempty = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b");

        test_validatefree(
            hset_validate(stdout, &empty) && hset_validate(stdout, &nonempty),
            (hset_free(&empty), hset_free(&nonempty)),
            "Validation failed empty FS vs nonempty FS"
        );
        test_validatefree(
            hset_in(&empty, &nonempty),
            (hset_free(&empty), hset_free(&nonempty)),
            "Empty FS set should be subset of any FS set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty FS not in empty FS", ++subnum);
    {
        hset empty    = hset_init_fs(10);
        hset nonempty = HSET_CREATEFS_ASSTR("/tmp/x");

        test_validatefree(
            !hset_in(&nonempty, &empty),
            (hset_free(&empty), hset_free(&nonempty)),
            "Non-empty FS set should NOT be subset of empty FS set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: equal FS sets", ++subnum);
    {
        hset superset = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset subset   = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");

        test_validatefree(
            hset_in(&subset, &superset),
            (hset_free(&superset), hset_free(&subset)),
            "Equal FS sets: subset should be in superset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS subset in FS superset", ++subnum);
    {
        hset superset = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset subset   = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/c");

        test_validatefree(
            hset_in(&subset, &superset),
            (hset_free(&superset), hset_free(&subset)),
            "FS subset should be in superset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS superset not in FS subset", ++subnum);
    {
        hset superset = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset subset   = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/c");

        test_validatefree(
            !hset_in(&superset, &subset),
            (hset_free(&superset), hset_free(&subset)),
            "FS superset should NOT be in subset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: FS vs INT type mismatch raise SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = HSET_CREATE_INT(42);

        if (!try()) {
            test_validatefree(
                !hset_in(&fs_set, &int_set),
                (hset_free(&fs_set), hset_free(&int_set)),
                "FS set should not be subset of INT set (type mismatch)"
            );
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            return logret(TEST_PASSED, "done");
        }
        return logret(TEST_FAILED, "done");
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 9 ---------------------------------
static TestStatus
tf9(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: empty strict in empty", ++subnum);
    {
        hset empty1    = hset_init(10, VALUE64_INT);
        hset empty2 = hset_init(100, VALUE64_INT);

        test_validatefree(
            hset_validate(stdout, &empty1) && hset_validate(stdout, &empty1),
            (hset_free(&empty1), hset_free(&empty2) ), "Validation failed empty"
        );
        test_validatefree(
            hset_strictin(&empty1, &empty2) == false, (hset_free(&empty1), hset_free(&empty2) ),
            "Empty set should NOT be subset of empty set"
        );
        hset_free(&empty1);
        hset_free(&empty2);
    }
    test_sub("subtest %d: empty strict in nonempty", ++subnum);
    {
        int vals[] = {1, 3, 5, 7, 9};
        hset empty    = hset_init(10, VALUE64_INT);
        hset nonempty = hset_from_intarr(vals, COUNT(vals) );

        test_validatefree(
            hset_validate(stdout, &empty) && hset_validate(stdout, &nonempty),
            (hset_free(&empty), hset_free(&nonempty) ), "Validation failed empty"
        );
        test_validatefree(
            hset_strictin(&empty, &nonempty), (hset_free(&empty), hset_free(&nonempty) ),
            "Empty set should be strict subset of any set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    test_sub("subtest %d: nonempty not in empty", ++subnum);
    {
        int vals[] = {1, 3, 5, 7, 9};
        hset empty    = hset_init(10, VALUE64_INT);
        hset nonempty = hset_from_intarr(vals, COUNT(vals) );

        test_validatefree(
            !hset_strictin(&nonempty, &empty), (hset_free(&empty), hset_free(&nonempty) ),
            "Non-empty set should NOT be strict subset of empty set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }

    test_sub("subtest %d: subset in superset", ++subnum);
    {
        int all_vals[] = {1, 3, 5, 7, 9};
        int sub_vals[] = {1, 5, 9};

        hset superset = hset_from_intarr(all_vals, COUNT(all_vals) );
        hset subset = hset_from_intarr(sub_vals, COUNT(sub_vals) );

        test_validatefree(
            hset_strictin(&subset, &superset), (hset_free(&superset), hset_free(&subset) ),
            "Subset should be strict in superset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    test_sub("subtest %d: equal sets", ++subnum);
    {
        int all_vals[] = {1, 3, 5, 7, 9};

        hset superset = hset_from_intarr(all_vals, COUNT(all_vals) );
        hset subset   = hset_from_intarr(all_vals, COUNT(all_vals) );

        test_validatefree(
            hset_strictin(&subset, &superset) == false, (hset_free(&superset), hset_free(&subset) ),
            "Subset should NOT be strict in equal set"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    test_sub("subtest %d: superset not in subset", ++subnum);
    {
        int all_vals[] = {1, 3, 5, 7, 9};
        int sub_vals[] = {1, 5, 9};

        hset superset = hset_from_intarr(all_vals, COUNT(all_vals) );
        hset subset = hset_from_intarr(sub_vals, COUNT(sub_vals) );

        test_validatefree(
            !hset_strictin(&superset, &subset), (hset_free(&superset), hset_free(&subset) ),
            "Superset should NOT be strict in subset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    // fs
    test_sub("subtest %d: empty strict in empty", ++subnum);
    {
        hset empty1 = hset_init_fs(10);
        hset empty2 = hset_init_fs(100);

        test_validatefree(
            hset_validate(stdout, &empty1) && hset_validate(stdout, &empty2),
            (hset_free(&empty1), hset_free(&empty2)),
            "Validation failed empty FS"
        );
        test_validatefree(
            hset_strictin(&empty1, &empty2) == false,
            (hset_free(&empty1), hset_free(&empty2)),
            "Empty FS set should NOT be strict subset of empty FS set"
        );
        hset_free(&empty1);
        hset_free(&empty2);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty strict in nonempty", ++subnum);
    {
        hset empty    = hset_init_fs(10);
        hset nonempty = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b");

        test_validatefree(
            hset_validate(stdout, &empty) && hset_validate(stdout, &nonempty),
            (hset_free(&empty), hset_free(&nonempty)),
            "Validation failed empty FS vs nonempty FS"
        );
        test_validatefree(
            hset_strictin(&empty, &nonempty),
            (hset_free(&empty), hset_free(&nonempty)),
            "Empty FS set should be strict subset of any non‑empty FS set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty not strict in empty", ++subnum);
    {
        hset empty    = hset_init_fs(10);
        hset nonempty = HSET_CREATEFS_ASSTR("/tmp/x");

        test_validatefree(
            !hset_strictin(&nonempty, &empty),
            (hset_free(&empty), hset_free(&nonempty)),
            "Non‑empty FS set should NOT be strict subset of empty FS set"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: strict subset in superset", ++subnum);
    {
        hset superset = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset subset   = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/c");

        test_validatefree(
            hset_strictin(&subset, &superset),
            (hset_free(&superset), hset_free(&subset)),
            "Proper subset should be strict in superset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: equal sets (not strict)", ++subnum);
    {
        hset set1 = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset set2 = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");

        test_validatefree(
            hset_strictin(&set1, &set2) == false,
            (hset_free(&set1), hset_free(&set2)),
            "Equal FS sets should NOT be strict subsets of each other"
        );
        hset_free(&set1);
        hset_free(&set2);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: superset not strict in subset", ++subnum);
    {
        hset superset = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset subset   = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/c");

        test_validatefree(
            !hset_strictin(&superset, &subset),
            (hset_free(&superset), hset_free(&subset)),
            "Superset should NOT be strict in its proper subset"
        );
        hset_free(&superset);
        hset_free(&subset);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raise SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            test_validatefree(
                !hset_strictin(&fs_set, &int_set),
                (hset_free(&fs_set), hset_free(&int_set)),
                "FS set should not be strict subset of INT set (type mismatch)"
            );
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            return logret(TEST_PASSED, "done");
        }
        return logret(TEST_FAILED, "done");
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 10 ---------------------------------
static TestStatus
tf10(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое минус пустое */
    test_sub("subtest %d: empty minus empty", ++subnum);
    {
        hset    se1 = hset_init(10, VALUE64_INT);
        hset    se2 = hset_init(10, VALUE64_INT);
        hset   *res = hset_minus(&se1, &se2);

        int     ok = (res == &se1) && (hset_cnt(&se1) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Empty minus empty: res=%p, cnt=%d (expected res=&se1, cnt=0)",
            (void*)res, hset_cnt(&se1)
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    /* 2. Пустое минус непустое */
    test_sub("subtest %d: empty minus non-empty", ++subnum);
    {
        hset    empty = hset_init(10, VALUE64_INT);
        int     vals[] = {1, 2, 3};
        hset    nonempty = hset_from_intarr(vals, COUNT(vals));
        hset   *res = hset_minus(&empty, &nonempty);

        int     ok = (res == &empty) && (hset_cnt(&empty) == 0) &&
                     (hset_cnt(&nonempty) == COUNT(vals));
        test_validatefree(
            ok,
            (hset_free(&empty), hset_free(&nonempty)),
            "Empty minus non-empty: res=%p, cnt=%d (expected res=&empty, cnt=0)",
            (void*)res, hset_cnt(&empty)
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    /* 3. Непустое минус пустое */
    test_sub("subtest %d: non-empty minus empty", ++subnum);
    {
        int     vals[] = {10, 20, 30};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    empty = hset_init(10, VALUE64_INT);
        int     cnt_before = hset_cnt(&se1);
        hset   *res = hset_minus(&se1, &empty);

        int     ok = (res == &se1) && (hset_cnt(&se1) == cnt_before);
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty)),
            "Non-empty minus empty: res=%p, cnt=%d (expected res=&se1, cnt=%d)",
            (void*)res, hset_cnt(&se1), cnt_before
        );
        hset_free(&se1);
        hset_free(&empty);
    }

    /* 4. Множество минус его копия (все элементы должны быть удалены) */
    test_sub("subtest %d: set minus its copy", ++subnum);
    {
        int     vals[] = {5, 10, 15, 20};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    se2 = hset_from_intarr(vals, COUNT(vals));
        /*hset    se2 = hset_init(10, VALUE64_INT);
        for (int i = 0; i < COUNT(vals); i++)
            hset_set(&se2, LITERAL64_INT(vals[i])); */

        int     cnt_before = hset_cnt(&se1);
        hset   *res = hset_minus(&se1, &se2);

        int     ok = (res == &se1) && (hset_cnt(&se1) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Set minus its copy: res=%p, cnt=%d (expected res=&se1, cnt=0 after %d deleted)",
            (void*)res, hset_cnt(&se1), cnt_before
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 5. Непустое минус строгое подмножество */
    test_sub("subtest %d: non-empty minus subset", ++subnum);
    {
        int     all_vals[] = {1, 2, 3, 4, 5, 6};
        int     sub_vals[] = {2, 5};
        hset    se1 = hset_from_intarr(all_vals, COUNT(all_vals));
        hset    se2 = hset_from_intarr(sub_vals, COUNT(sub_vals));
        /* hset    se2 = hset_init(10, VALUE64_INT);
        for (int i = 0; i < COUNT(sub_vals); i++)
            hset_set(&se2, LITERAL64_INT(sub_vals[i])); */

        hset   *res = hset_minus(&se1, &se2);

        int     expected_cnt = 4;   // {1,3,4,6}
        int     ok = (res == &se1) && (hset_cnt(&se1) == expected_cnt) &&
                     !hset_get(&se1, LITERAL64_INT(2)) &&
                     !hset_get(&se1, LITERAL64_INT(5)) &&
                     hset_get(&se1, LITERAL64_INT(1)) &&
                     hset_get(&se1, LITERAL64_INT(3)) &&
                     hset_get(&se1, LITERAL64_INT(4)) &&
                     hset_get(&se1, LITERAL64_INT(6));
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Non-empty minus subset: res=%p, cnt=%d (expected res=&se1, cnt=%d)",
            (void*)res, hset_cnt(&se1), expected_cnt
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    /* 6. Непустое минус непересекающееся множество */
    test_sub("subtest %d: non-empty minus disjoint", ++subnum);
    {
        int     vals1[] = {7, 8, 9};
        int     vals2[] = {1, 2, 3};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));

        int     cnt_before = hset_cnt(&se1);
        hset   *res = hset_minus(&se1, &se2);

        int     ok = (res == &se1) && (hset_cnt(&se1) == cnt_before);
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals1[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Non-empty minus disjoint: res=%p, cnt=%d (expected res=&se1, cnt=%d)",
            (void*)res, hset_cnt(&se1), cnt_before
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    /* 7. Удаление, когда se2 содержит лишние элементы */
    test_sub("subtest %d: se2 with extra elements", ++subnum);
    {
        int     vals1[] = {100, 200, 300};
        int     vals2[] = {200, 400, 500};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));

        hset   *res = hset_minus(&se1, &se2);

        // Ожидаем, что останутся только 100 и 300
        int     expected_remaining[] = {100, 300};
        int     ok = (res == &se1) && (hset_cnt(&se1) == COUNT(expected_remaining));
        for (int i = 0; ok && i < COUNT(expected_remaining); i++)
            ok = hset_get(&se1, LITERAL64_INT(expected_remaining[i]));
        // Убедимся, что 200 действительно удалён
        if (ok)
            ok = !hset_get(&se1, LITERAL64_INT(200));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "se2 with extra elements: res=%p, cnt=%d (expected res=&se1, cnt=2)",
            (void*)res, hset_cnt(&se1)
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    /* 8. Несовпадение типов – ожидаем аварийный сигнал */
    test_sub("subtest %d: type mismatch raises SIGINT", ++subnum);
    {
        int int_vals[] = {1, 2, 3};
        hset int_set = hset_from_intarr(int_vals, COUNT(int_vals) );
        hset dbl_set = hset_init(10, VALUE64_DBL);
        hset_set(&dbl_set, LITERAL64_DBL(1.0));

        bool signal_caught = try();
        if (signal_caught) {
            hset_free(&int_set);
            hset_free(&dbl_set);
            return logret(TEST_PASSED, "Signal caught as expected");
        } else {
            hset_free(&int_set);
            hset_free(&dbl_set);
            return logerr(TEST_FAILED, "Expected signal for type mismatch, but none was raised");
        }
    }
    // fs
    test_sub("subtest %d: empty minus empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset *res = hset_minus(&a, &b);   // res == &a, a изменён

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Empty minus empty should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty minus nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_minus(&a, &b);

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Empty minus nonempty should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty minus empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset *res = hset_minus(&a, &b);

        test_validatefree(
            res->count == 3 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/b") &&
            HSET_HAS_FS(res, "/tmp/c"),
            (hset_free(res), hset_free(&b)),
            "A minus empty should equal A"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: minus with common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3", "/tmp/4");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset *res = hset_minus(&a, &b);

        test_validatefree(
            res->count == 2 &&
            HSET_HAS_FS(res, "/tmp/1") &&
            HSET_HAS_FS(res, "/tmp/2") &&
            !HSET_HAS_FS(res, "/tmp/3") &&
            !HSET_HAS_FS(res, "/tmp/4") &&
            !HSET_HAS_FS(res, "/tmp/5"),
            (hset_free(res), hset_free(&b)),
            "A \\ B should contain only /tmp/1 and /tmp/2"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: minus with no common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset *res = hset_minus(&a, &b);

        test_validatefree(
            res->count == 2 &&
            HSET_HAS_FS(res, "/tmp/1") &&
            HSET_HAS_FS(res, "/tmp/2"),
            (hset_free(res), hset_free(&b)),
            "A \\ B with no overlap should equal A"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: minus results in empty set", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_minus(&a, &b);

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "A \\ B when A == B should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            hset *res = hset_minus(&fs_set, &int_set);   // должно вызвать ошибку
            hset_free(res);
            hset_free(&int_set);
            test_validate(false, 
                "Type mismatch should have raised SIGINT"
            );
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logmsg("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 11 ---------------------------------
static TestStatus
tf11(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое минус пустое = пустое */
    test_sub("subtest %d: empty minus empty", ++subnum);
    {
        hset se1 = hset_init(10, VALUE64_INT);
        hset se2 = hset_init(10, VALUE64_INT);
        hset res = hset_init_minus(&se1, &se2);

        int ok = (hset_cnt(&res) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Empty minus empty: cnt=%d (expected 0)", hset_cnt(&res)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 2. Непустое минус пустое = копия исходного (все элементы) */
    test_sub("subtest %d: non-empty minus empty", ++subnum);
    {
        int vals[] = {3, 7, 11, 42};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset empty = hset_init(10, VALUE64_INT);
        hset res = hset_init_minus(&se1, &empty);

        int ok = (hset_cnt(&res) == COUNT(vals));
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&res, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty), hset_free(&res)),
            "Non-empty minus empty: cnt=%d (expected %d)", hset_cnt(&res), COUNT(vals)
        );
        hset_free(&se1);
        hset_free(&empty);
        hset_free(&res);
    }

    /* 3. Пустое минус непустое = пустое */
    test_sub("subtest %d: empty minus non-empty", ++subnum);
    {
        int vals[] = {100, 200};
        hset empty = hset_init(10, VALUE64_INT);
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_init_minus(&empty, &se2);

        int ok = (hset_cnt(&res) == 0);
        test_validatefree(
            ok,
            (hset_free(&empty), hset_free(&se2), hset_free(&res)),
            "Empty minus non-empty: cnt=%d (expected 0)", hset_cnt(&res)
        );
        hset_free(&empty);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 4. Множество минус его копия = пустое (все элементы удаляются) */
    test_sub("subtest %d: set minus its copy", ++subnum);
    {
        int vals[] = {5, 10, 15, 20};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset se2 = hset_clone(&se1);  // или ручное заполнение, но clone уже проверен
        hset res = hset_init_minus(&se1, &se2);

        int ok = (hset_cnt(&res) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Set minus its copy: cnt=%d (expected 0)", hset_cnt(&res)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 5. Частичное перекрытие: элементы, отсутствующие в se2, сохраняются */
    test_sub("subtest %d: partial overlap", ++subnum);
    {
        int vals1[] = {1, 2, 3, 4, 5, 6};
        int vals2[] = {2, 5};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_init(10, VALUE64_INT);
        for (int i = 0; i < COUNT(vals2); i++)
            hset_set(&se2, LITERAL64_INT(vals2[i]));

        hset res = hset_init_minus(&se1, &se2);

        // Ожидаем, что останутся {1,3,4,6}
        int expected_vals[] = {1, 3, 4, 6};
        int ok = (hset_cnt(&res) == COUNT(expected_vals));
        for (int i = 0; ok && i < COUNT(expected_vals); i++)
            ok = hset_get(&res, LITERAL64_INT(expected_vals[i]));

        // Дополнительно убедимся, что удалённых нет
        if (ok) {
            ok = !hset_get(&res, LITERAL64_INT(2)) &&
                 !hset_get(&res, LITERAL64_INT(5));
        }

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Partial overlap: cnt=%d (expected %d)", hset_cnt(&res), COUNT(expected_vals)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 6. Непересекающиеся множества: все элементы se1 должны остаться */
    test_sub("subtest %d: disjoint sets", ++subnum);
    {
        int vals1[] = {7, 8, 9};
        int vals2[] = {1, 2, 3};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset res = hset_init_minus(&se1, &se2);

        int ok = (hset_cnt(&res) == COUNT(vals1));
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&res, LITERAL64_INT(vals1[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Disjoint sets: cnt=%d (expected %d)", hset_cnt(&res), COUNT(vals1)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }
    // fs
    test_sub("subtest %d: empty minus empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset res = hset_init_minus(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty minus empty should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty minus nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_minus(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty minus nonempty should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty minus empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset res = hset_init_minus(&a, &b);

        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/b") &&
            HSET_HAS_FS(&res, "/tmp/c"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "A minus empty should equal A"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: minus with common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3", "/tmp/4");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset res = hset_init_minus(&a, &b);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/1") &&
            HSET_HAS_FS(&res, "/tmp/2") &&
            !HSET_HAS_FS(&res, "/tmp/3") &&
            !HSET_HAS_FS(&res, "/tmp/4") &&
            !HSET_HAS_FS(&res, "/tmp/5"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "A \\ B should contain only /tmp/1 and /tmp/2"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: minus with no common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset res = hset_init_minus(&a, &b);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/1") &&
            HSET_HAS_FS(&res, "/tmp/2"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "A \\ B with no overlap should equal A"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: minus results in empty set", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_minus(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "A \\ B when A == B should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            hset res = hset_init_minus(&fs_set, &int_set);   // должно вызвать ошибку
            hset_free(&fs_set);
            hset_free(&int_set);
            hset_free(&res);
            test_validate(false, 
                "Type mismatch should have raised SIGINT"
            );
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logmsg("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 12 ---------------------------------

static TestStatus
tf12(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое пересекается с пустым – остаётся пустое */
    test_sub("subtest %d: empty intersect empty", ++subnum);
    {
        hset    se1 = hset_init(10, VALUE64_INT);
        hset    se2 = hset_init(10, VALUE64_INT);

        hset   *res = hset_intersect(&se1, &se2);

        int     ok = (res == &se1) && (hset_cnt(&se1) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Empty intersect empty: res=%p, cnt=%d (expected res=&se1, cnt=0)",
            (void*)res, hset_cnt(&se1)
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    /* 2. Непустое пересекается с пустым – остаётся пустое */
    test_sub("subtest %d: non-empty intersect empty", ++subnum);
    {
        int     vals[] = {1, 2, 3};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    empty = hset_init(10, VALUE64_INT);
        hset   *res = hset_intersect(&se1, &empty);

        int     ok = (res == &se1) && (hset_cnt(&se1) == 0) &&
                     (hset_cnt(&empty) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty)),
            "Non-empty intersect empty: res=%p, cnt=%d (expected res=&se1, cnt=0)",
            (void*)res, hset_cnt(&se1)
        );
        hset_free(&se1);
        hset_free(&empty);
    }
    /* 3. Пустое пересекается с непустым – остаётся пустое */
    test_sub("subtest %d: empty intersect non-empty", ++subnum);
    {
        int     vals[] = {10, 20};
        hset    empty = hset_init(10, VALUE64_INT);
        hset    se2 = hset_from_intarr(vals, COUNT(vals));
        hset   *res = hset_intersect(&empty, &se2);

        int     ok = (res == &empty) && (hset_cnt(&empty) == 0) &&
                     (hset_cnt(&se2) == COUNT(vals));
        test_validatefree(
            ok,
            (hset_free(&empty), hset_free(&se2)),
            "Empty intersect non-empty: res=%p, cnt=%d (expected res=&empty, cnt=0)",
            (void*)res, hset_cnt(&empty)
        );
        hset_free(&empty);
        hset_free(&se2);
    }
    /* 4. Одинаковые множества – остаются все элементы */
    test_sub("subtest %d: identical sets", ++subnum);
    {
        int     vals[] = {7, 8, 9};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    se2 = hset_from_intarr(vals, COUNT(vals));
        int     cnt_before = hset_cnt(&se1);

        hset   *res = hset_intersect(&se1, &se2);

        int     ok = (res == &se1) && (hset_cnt(&se1) == cnt_before);
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Identical sets: res=%p, cnt=%d (expected res=&se1, cnt=%d)",
            (void*)res, hset_cnt(&se1), cnt_before
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    /* 5. Частичное перекрытие – остаются только общие элементы */
    test_sub("subtest %d: partial overlap", ++subnum);
    {
        int     vals1[] = {1, 2, 3, 4, 5};
        int     vals2[] = {2, 4, 6};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));

        hset   *res = hset_intersect(&se1, &se2);

        int     expected_vals[] = {2, 4};
        int     expected_cnt = COUNT(expected_vals);
        int     ok = (res == &se1) && (hset_cnt(&se1) == expected_cnt);
        for (int i = 0; ok && i < expected_cnt; i++)
            ok = hset_get(&se1, LITERAL64_INT(expected_vals[i]));
        // Убедимся, что остальные отсутствуют
        if (ok) {
            ok = !hset_get(&se1, LITERAL64_INT(1)) &&
                 !hset_get(&se1, LITERAL64_INT(3)) &&
                 !hset_get(&se1, LITERAL64_INT(5));
        }

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Partial overlap: res=%p, cnt=%d (expected res=&se1, cnt=%d)",
            (void*)res, hset_cnt(&se1), expected_cnt
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    /* 6. Непересекающиеся множества – результат пуст */
    test_sub("subtest %d: disjoint sets", ++subnum);
    {
        int     vals1[] = {100, 200};
        int     vals2[] = {300, 400};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));

        hset   *res = hset_intersect(&se1, &se2);

        int     ok = (res == &se1) && (hset_cnt(&se1) == 0) &&
                     (hset_cnt(&se2) == COUNT(vals2));
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Disjoint sets: res=%p, cnt=%d (expected res=&se1, cnt=0)",
            (void*)res, hset_cnt(&se1)
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    // fs
    test_sub("subtest %d: empty intersect empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset *res = hset_intersect(&a, &b);   // res == &a

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Empty intersect empty should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty intersect nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_intersect(&a, &b);

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Empty intersect nonempty should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty intersect empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset *res = hset_intersect(&a, &b);

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Nonempty intersect empty should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: partial overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3", "/tmp/4");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset *res = hset_intersect(&a, &b);

        test_validatefree(
            res->count == 2 &&
            HSET_HAS_FS(res, "/tmp/3") &&
            HSET_HAS_FS(res, "/tmp/4") &&
            !HSET_HAS_FS(res, "/tmp/1") &&
            !HSET_HAS_FS(res, "/tmp/2") &&
            !HSET_HAS_FS(res, "/tmp/5"),
            (hset_free(res), hset_free(&b)),
            "Intersection should contain only common elements"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: no common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset *res = hset_intersect(&a, &b);

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Disjoint sets intersection should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: identical sets", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_intersect(&a, &b);

        test_validatefree(
            res->count == 2 &&
            HSET_HAS_FS(res, "/tmp/x") &&
            HSET_HAS_FS(res, "/tmp/y"),
            (hset_free(res), hset_free(&b)),
            "Intersection of identical sets should equal original"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));   // используем hset_set для int

        if (!try()) {
            hset *res = hset_intersect(&fs_set, &int_set);
            hset_free(res);
            hset_free(&int_set);
            test_validate(false, "Type mismatch should have raised SIGINT");
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logsimple("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 13 ---------------------------------

static TestStatus
tf13(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое с пустым -> пустое */
    test_sub("subtest %d: empty intersect empty", ++subnum);
    {
        hset se1 = hset_init(10, VALUE64_INT);
        hset se2 = hset_init(10, VALUE64_INT);
        hset res = hset_init_intersect(&se1, &se2);

        int ok = (hset_cnt(&res) == 0) && (hset_cnt(&se1) == 0) && (hset_cnt(&se2) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Empty intersect empty: res=%d, se1=%d, se2=%d (expected all 0)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 2. Непустое с пустым -> пустое, se1 не изменилось */
    test_sub("subtest %d: non-empty intersect empty", ++subnum);
    {
        int vals[] = {10, 20, 30};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset empty = hset_init(10, VALUE64_INT);
        hset res = hset_init_intersect(&se1, &empty);

        int ok = (hset_cnt(&res) == 0) && (hset_cnt(&se1) == COUNT(vals)) && (hset_cnt(&empty) == 0);
        // дополнительно убедимся, что элементы se1 на месте
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty), hset_free(&res)),
            "Non-empty intersect empty: res=%d, se1=%d, empty=%d (expected res=0, se1=%d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&empty), COUNT(vals)
        );
        hset_free(&se1);
        hset_free(&empty);
        hset_free(&res);
    }

    /* 3. Пустое с непустым -> пустое, se2 не изменилось */
    test_sub("subtest %d: empty intersect non-empty", ++subnum);
    {
        int vals[] = {5, 7};
        hset empty = hset_init(10, VALUE64_INT);
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_init_intersect(&empty, &se2);

        int ok = (hset_cnt(&res) == 0) && (hset_cnt(&empty) == 0) && (hset_cnt(&se2) == COUNT(vals));
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se2, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&empty), hset_free(&se2), hset_free(&res)),
            "Empty intersect non-empty: res=%d, empty=%d, se2=%d (expected res=0, se2=%d)",
            hset_cnt(&res), hset_cnt(&empty), hset_cnt(&se2), COUNT(vals)
        );
        hset_free(&empty);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 4. Одинаковые множества -> все элементы */
    test_sub("subtest %d: identical sets", ++subnum);
    {
        int vals[] = {1, 2, 3, 4};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_init_intersect(&se1, &se2);

        int expected = COUNT(vals);
        int ok = (hset_cnt(&res) == expected) && (hset_cnt(&se1) == expected) && (hset_cnt(&se2) == expected);
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&res, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Identical sets: res=%d, se1=%d, se2=%d (expected all %d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2), expected
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 5. Частичное перекрытие -> только общие элементы */
    test_sub("subtest %d: partial overlap", ++subnum);
    {
        int vals1[] = {1, 2, 3, 4, 5};
        int vals2[] = {2, 4, 6};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset res = hset_init_intersect(&se1, &se2);

        int common_vals[] = {2, 4};
        int expected = COUNT(common_vals);
        int ok = (hset_cnt(&res) == expected) && (hset_cnt(&se1) == COUNT(vals1)) && (hset_cnt(&se2) == COUNT(vals2));
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&res, LITERAL64_INT(common_vals[i]));
        // убедимся, что не общих элементов нет
        if (ok) {
            ok = !hset_get(&res, LITERAL64_INT(1)) &&
                 !hset_get(&res, LITERAL64_INT(3)) &&
                 !hset_get(&res, LITERAL64_INT(5));
        }

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Partial overlap: res=%d, se1=%d, se2=%d (expected res=%d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2), expected
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 6. Непересекающиеся множества -> пустое */
    test_sub("subtest %d: disjoint sets", ++subnum);
    {
        int vals1[] = {100, 200};
        int vals2[] = {300, 400};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset res = hset_init_intersect(&se1, &se2);

        int ok = (hset_cnt(&res) == 0) && (hset_cnt(&se1) == COUNT(vals1)) && (hset_cnt(&se2) == COUNT(vals2));
        // se1 и se2 не должны потерять элементы
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se2, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Disjoint sets: res=%d, se1=%d, se2=%d (expected res=0)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }
    // fs
    test_sub("subtest %d: empty intersect empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset res = hset_init_intersect(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty intersect empty should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty intersect nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_intersect(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty intersect nonempty should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty intersect empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset res = hset_init_intersect(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Nonempty intersect empty should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: partial overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3", "/tmp/4");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset res = hset_init_intersect(&a, &b);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/3") &&
            HSET_HAS_FS(&res, "/tmp/4") &&
            !HSET_HAS_FS(&res, "/tmp/1") &&
            !HSET_HAS_FS(&res, "/tmp/2") &&
            !HSET_HAS_FS(&res, "/tmp/5"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Init intersect should contain only common elements"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: no common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset res = hset_init_intersect(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Init intersect of disjoint sets should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: identical sets", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_intersect(&a, &b);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/x") &&
            HSET_HAS_FS(&res, "/tmp/y"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Init intersect of identical sets should equal original"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            hset res = hset_init_intersect(&fs_set, &int_set);
            hset_free(&fs_set);
            hset_free(&int_set);
            hset_free(&res);
            test_validate(false, "Type mismatch should have raised SIGINT");
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logsimple("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 14 ---------------------------------
static TestStatus
tf14(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое с пустым -> пустое */
    test_sub("subtest %d: empty symm diff empty", ++subnum);
    {
        hset se1 = hset_init(10, VALUE64_INT);
        hset se2 = hset_init(10, VALUE64_INT);
        hset res = hset_init_symmdiff(&se1, &se2);

        int ok = (hset_cnt(&res) == 0) && (hset_cnt(&se1) == 0) && (hset_cnt(&se2) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Empty symm diff empty: res=%d, se1=%d, se2=%d (expected all 0)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 2. Непустое с пустым -> все элементы непустого */
    test_sub("subtest %d: non-empty symm diff empty", ++subnum);
    {
        int vals[] = {3, 7, 11};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset empty = hset_init(10, VALUE64_INT);
        hset res = hset_init_symmdiff(&se1, &empty);

        int expected = COUNT(vals);
        int ok = (hset_cnt(&res) == expected) && (hset_cnt(&se1) == expected) && (hset_cnt(&empty) == 0);
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&res, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty), hset_free(&res)),
            "Non-empty symm diff empty: res=%d, se1=%d, empty=%d (expected res=%d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&empty), expected
        );
        hset_free(&se1);
        hset_free(&empty);
        hset_free(&res);
    }

    /* 3. Пустое с непустым -> все элементы непустого */
    test_sub("subtest %d: empty symm diff non-empty", ++subnum);
    {
        int vals[] = {5, 9};
        hset empty = hset_init(10, VALUE64_INT);
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_init_symmdiff(&empty, &se2);

        int expected = COUNT(vals);
        int ok = (hset_cnt(&res) == expected) && (hset_cnt(&empty) == 0) && (hset_cnt(&se2) == expected);
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&res, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&empty), hset_free(&se2), hset_free(&res)),
            "Empty symm diff non-empty: res=%d, empty=%d, se2=%d (expected res=%d)",
            hset_cnt(&res), hset_cnt(&empty), hset_cnt(&se2), expected
        );
        hset_free(&empty);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 4. Одинаковые множества -> пустое */
    test_sub("subtest %d: identical sets", ++subnum);
    {
        int vals[] = {1, 2, 3, 4};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_init_symmdiff(&se1, &se2);

        int expected = COUNT(vals);
        int ok = (hset_cnt(&res) == 0) && (hset_cnt(&se1) == expected) && (hset_cnt(&se2) == expected);
        // элементы исходных множеств на месте
        for (int i = 0; ok && i < expected; i++) {
            if (!hset_get(&se1, LITERAL64_INT(vals[i])) ||
                !hset_get(&se2, LITERAL64_INT(vals[i])))
                ok = 0;
        }

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Identical sets: res=%d, se1=%d, se2=%d (expected res=0)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 5. Частичное перекрытие -> только элементы, принадлежащие ровно одному множеству */
    test_sub("subtest %d: partial overlap", ++subnum);
    {
        int vals1[] = {1, 2, 3, 4, 5};
        int vals2[] = {2, 4, 6};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset res = hset_init_symmdiff(&se1, &se2);

        // Ожидаем {1,3,5,6}
        int expected_vals[] = {1, 3, 5, 6};
        int expected_cnt = COUNT(expected_vals);
        int ok = (hset_cnt(&res) == expected_cnt) &&
                 (hset_cnt(&se1) == COUNT(vals1)) &&
                 (hset_cnt(&se2) == COUNT(vals2));
        // Проверяем наличие ожидаемых
        for (int i = 0; ok && i < expected_cnt; i++)
            ok = hset_get(&res, LITERAL64_INT(expected_vals[i]));
        // Проверяем отсутствие общих (2 и 4)
        if (ok) {
            ok = !hset_get(&res, LITERAL64_INT(2)) &&
                 !hset_get(&res, LITERAL64_INT(4));
        }
        // Убедимся, что исходные множества не изменились
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se2, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Partial overlap: res=%d, se1=%d, se2=%d (expected res=%d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2), expected_cnt
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 6. Непересекающиеся множества -> все элементы обоих */
    test_sub("subtest %d: disjoint sets", ++subnum);
    {
        int vals1[] = {100, 200};
        int vals2[] = {300, 400};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset res = hset_init_symmdiff(&se1, &se2);

        int expected_total = COUNT(vals1) + COUNT(vals2);
        int ok = (hset_cnt(&res) == expected_total) &&
                 (hset_cnt(&se1) == COUNT(vals1)) &&
                 (hset_cnt(&se2) == COUNT(vals2));
        // все элементы vals1 и vals2 должны быть в res
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&res, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&res, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Disjoint sets: res=%d, se1=%d, se2=%d (expected res=%d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2), expected_total
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }
    // fs
    test_sub("subtest %d: empty symmdiff empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset res = hset_init_symmdiff(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty Δ empty should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty symmdiff nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_symmdiff(&a, &b);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/x") &&
            HSET_HAS_FS(&res, "/tmp/y"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty Δ nonempty should equal nonempty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty symmdiff empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset res = hset_init_symmdiff(&a, &b);

        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/b") &&
            HSET_HAS_FS(&res, "/tmp/c"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Nonempty Δ empty should equal nonempty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: partial overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3", "/tmp/4");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset res = hset_init_symmdiff(&a, &b);

        // Результат: {1,2,5}
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/1") &&
            HSET_HAS_FS(&res, "/tmp/2") &&
            HSET_HAS_FS(&res, "/tmp/5") &&
            !HSET_HAS_FS(&res, "/tmp/3") &&
            !HSET_HAS_FS(&res, "/tmp/4"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Symmetric difference should contain only /tmp/1, /tmp/2, /tmp/5"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: no common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset res = hset_init_symmdiff(&a, &b);

        // Результат: {1,2,3,4}
        test_validatefree(
            res.count == 4 &&
            HSET_HAS_FS(&res, "/tmp/1") &&
            HSET_HAS_FS(&res, "/tmp/2") &&
            HSET_HAS_FS(&res, "/tmp/3") &&
            HSET_HAS_FS(&res, "/tmp/4"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Disjoint sets symmetric difference should be union"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: identical sets", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_symmdiff(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Symmetric difference of identical sets should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            hset res = hset_init_symmdiff(&fs_set, &int_set);
            hset_free(&fs_set);
            hset_free(&int_set);
            hset_free(&res);
            test_validate(false, "Type mismatch should have raised SIGINT");
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logsimple("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 15 ---------------------------------
static TestStatus
tf15(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое с пустым -> пустое */
    test_sub("subtest %d: empty symmdiff empty", ++subnum);
    {
        hset se1 = hset_init(10, VALUE64_INT);
        hset se2 = hset_init(10, VALUE64_INT);
        hset *res = hset_symmdiff(&se1, &se2);

        int ok = (hset_cnt(&se1) == 0) && (hset_cnt(&se2) == 0) && (res == &se1);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Empty symmdiff empty: se1=%d, se2=%d, res=%p (expected se1=0, se2=0, res==&se1)",
            hset_cnt(&se1), hset_cnt(&se2), (void*)res
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 2. Непустое с пустым -> se1 без изменений */
    test_sub("subtest %d: non-empty symmdiff empty", ++subnum);
    {
        int vals[] = {3, 7, 11};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset empty = hset_init(10, VALUE64_INT);
        int cnt_before = hset_cnt(&se1);
        hset *res = hset_symmdiff(&se1, &empty);

        int ok = (hset_cnt(&se1) == cnt_before) && (hset_cnt(&empty) == 0) && (res == &se1);
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty)),
            "Non-empty symmdiff empty: se1=%d, empty=%d, res=%p (expected se1=%d, empty=0)",
            hset_cnt(&se1), hset_cnt(&empty), (void*)res, cnt_before
        );
        hset_free(&se1);
        hset_free(&empty);
    }

    /* 3. Пустое с непустым -> se1 получает все элементы se2 */
    test_sub("subtest %d: empty symmdiff non-empty", ++subnum);
    {
        int vals[] = {5, 9};
        hset se1 = hset_init(10, VALUE64_INT);
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_symmdiff(&se1, &se2);

        int expected = COUNT(vals);
        int ok = (hset_cnt(&se1) == expected) && (hset_cnt(&se2) == expected) && (res == &se1);
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Empty symmdiff non-empty: se1=%d, se2=%d, res=%p (expected se1=%d, se2=%d)",
            hset_cnt(&se1), hset_cnt(&se2), (void*)res, expected, expected
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 4. Одинаковые множества -> se1 становится пустым */
    test_sub("subtest %d: identical sets", ++subnum);
    {
        int vals[] = {1, 2, 3, 4};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_symmdiff(&se1, &se2);

        int expected_orig = COUNT(vals);
        int ok = (hset_cnt(&se1) == 0) && (hset_cnt(&se2) == expected_orig) && (res == &se1);
        // элементы se2 должны остаться на месте
        for (int i = 0; ok && i < expected_orig; i++)
            ok = hset_get(&se2, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Identical sets: se1=%d, se2=%d, res=%p (expected se1=0, se2=%d)",
            hset_cnt(&se1), hset_cnt(&se2), (void*)res, expected_orig
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 5. Частичное перекрытие -> se1 содержит только элементы, принадлежащие ровно одному множеству */
    test_sub("subtest %d: partial overlap", ++subnum);
    {
        int vals1[] = {1, 2, 3, 4, 5};
        int vals2[] = {2, 4, 6};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset *res = hset_symmdiff(&se1, &se2);

        // Ожидаем в se1: {1,3,5,6}
        int expected_vals[] = {1, 3, 5, 6};
        int expected_cnt = COUNT(expected_vals);
        int ok = (hset_cnt(&se1) == expected_cnt) && (hset_cnt(&se2) == COUNT(vals2)) && (res == &se1);
        // проверяем наличие ожидаемых
        for (int i = 0; ok && i < expected_cnt; i++)
            ok = hset_get(&se1, LITERAL64_INT(expected_vals[i]));
        // проверяем отсутствие общих (2 и 4)
        if (ok) {
            ok = !hset_get(&se1, LITERAL64_INT(2)) &&
                 !hset_get(&se1, LITERAL64_INT(4));
        }
        // se2 не изменился
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se2, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Partial overlap: se1=%d, se2=%d, res=%p (expected se1=%d, se2=%d)",
            hset_cnt(&se1), hset_cnt(&se2), (void*)res, expected_cnt, COUNT(vals2)
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 6. Непересекающиеся множества -> se1 содержит все элементы обоих */
    test_sub("subtest %d: disjoint sets", ++subnum);
    {
        int vals1[] = {100, 200};
        int vals2[] = {300, 400};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset *res = hset_symmdiff(&se1, &se2);

        int expected_total = COUNT(vals1) + COUNT(vals2);
        int ok = (hset_cnt(&se1) == expected_total) && (hset_cnt(&se2) == COUNT(vals2)) && (res == &se1);
        // все элементы vals1 и vals2 должны быть в se1
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals2[i]));
        // se2 не изменился
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se2, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Disjoint sets: se1=%d, se2=%d, res=%p (expected se1=%d, se2=%d)",

            hset_cnt(&se1), hset_cnt(&se2), (void*)res, expected_total, COUNT(vals2)
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    // fs
    test_sub("subtest %d: empty symmdiff empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset *res = hset_symmdiff(&a, &b);   // res == &a

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Empty Δ empty should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty symmdiff nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_symmdiff(&a, &b);

        test_validatefree(
            res->count == 2 &&
            HSET_HAS_FS(res, "/tmp/x") &&
            HSET_HAS_FS(res, "/tmp/y"),
            (hset_free(res), hset_free(&b)),
            "Empty Δ nonempty should equal nonempty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty symmdiff empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset *res = hset_symmdiff(&a, &b);

        test_validatefree(
            res->count == 3 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/b") &&
            HSET_HAS_FS(res, "/tmp/c"),
            (hset_free(res), hset_free(&b)),
            "Nonempty Δ empty should equal nonempty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: partial overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3", "/tmp/4");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset *res = hset_symmdiff(&a, &b);

        // Результат: {1,2,5}
        test_validatefree(
            res->count == 3 &&
            HSET_HAS_FS(res, "/tmp/1") &&
            HSET_HAS_FS(res, "/tmp/2") &&
            HSET_HAS_FS(res, "/tmp/5") &&
            !HSET_HAS_FS(res, "/tmp/3") &&
            !HSET_HAS_FS(res, "/tmp/4"),
            (hset_free(res), hset_free(&b)),
            "Symmetric difference should contain only /tmp/1, /tmp/2, /tmp/5"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: no common elements", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset *res = hset_symmdiff(&a, &b);

        // Результат: {1,2,3,4}
        test_validatefree(
            res->count == 4 &&
            HSET_HAS_FS(res, "/tmp/1") &&
            HSET_HAS_FS(res, "/tmp/2") &&
            HSET_HAS_FS(res, "/tmp/3") &&
            HSET_HAS_FS(res, "/tmp/4"),
            (hset_free(res), hset_free(&b)),
            "Disjoint sets symmetric difference should be union"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: identical sets", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_symmdiff(&a, &b);

        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Symmetric difference of identical sets should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            hset *res = hset_symmdiff(&fs_set, &int_set);
            hset_free(res);
            hset_free(&int_set);
            test_validate(false, "Type mismatch should have raised SIGINT");
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logsimple("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}


// ------------------------- TEST 17 ---------------------------------
static TestStatus
tf17(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое ∪ пустое = пустое */
    test_sub("subtest %d: empty union empty", ++subnum);
    {
        hset    se1 = hset_init(10, VALUE64_INT);
        hset    se2 = hset_init(10, VALUE64_INT);
        hset   *res = hset_union(&se1, &se2);

        int     ok = (hset_cnt(&se1) == 0) && (res == &se1);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Empty union empty: cnt=%d, res=%p (expected cnt=0, res=&se1)",
            hset_cnt(&se1), (void*)res
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 2. Непустое ∪ пустое = исходное непустое */
    test_sub("subtest %d: non-empty union empty", ++subnum);
    {
        int     vals[] = {3, 7, 11};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    empty = hset_init(10, VALUE64_INT);
        int     cnt_before = hset_cnt(&se1);
        hset   *res = hset_union(&se1, &empty);

        int     ok = (hset_cnt(&se1) == cnt_before) && (res == &se1);
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty)),
            "Non-empty union empty: cnt=%d (expected %d), res=%p",
            hset_cnt(&se1), cnt_before, (void*)res
        );
        hset_free(&se1);
        hset_free(&empty);
    }

    /* 3. Пустое ∪ непустое = копия непустого в se1 */
    test_sub("subtest %d: empty union non-empty", ++subnum);
    {
        int     vals[] = {5, 9};
        hset    se1 = hset_init(10, VALUE64_INT);
        hset    se2 = hset_from_intarr(vals, COUNT(vals));
        hset   *res = hset_union(&se1, &se2);

        int     expected = COUNT(vals);
        int     ok = (hset_cnt(&se1) == expected) && (res == &se1);
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Empty union non-empty: cnt=%d (expected %d), res=%p",
            hset_cnt(&se1), expected, (void*)res
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 4. Одинаковые множества: результат не меняется, количество не растёт */
    test_sub("subtest %d: identical sets", ++subnum);
    {
        int     vals[] = {1, 2, 3, 4};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    se2 = hset_from_intarr(vals, COUNT(vals));
        int     cnt_before = hset_cnt(&se1);
        hset   *res = hset_union(&se1, &se2);

        int     ok = (hset_cnt(&se1) == cnt_before) && (res == &se1);
        // все элементы должны остаться на месте
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Identical sets: cnt=%d (expected %d), res=%p",
            hset_cnt(&se1), cnt_before, (void*)res
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 5. Частичное перекрытие: добавляются только недостающие элементы */
    test_sub("subtest %d: partial overlap", ++subnum);
    {
        int     vals1[] = {1, 2, 3, 4, 5};
        int     vals2[] = {3, 5, 6, 7};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset   *res = hset_union(&se1, &se2);

        // Ожидаем {1,2,3,4,5,6,7} – 7 элементов
        int     expected_vals[] = {1, 2, 3, 4, 5, 6, 7};
        int     expected_cnt = COUNT(expected_vals);
        int     ok = (hset_cnt(&se1) == expected_cnt) && (res == &se1);
        for (int i = 0; ok && i < expected_cnt; i++)
            ok = hset_get(&se1, LITERAL64_INT(expected_vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Partial overlap: cnt=%d (expected %d), res=%p",
            hset_cnt(&se1), expected_cnt, (void*)res
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 6. Непересекающиеся множества: все элементы se2 добавляются */
    test_sub("subtest %d: disjoint sets", ++subnum);
    {
        int     vals1[] = {100, 200};
        int     vals2[] = {300, 400};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));
        hset   *res = hset_union(&se1, &se2);

        int     expected_total = COUNT(vals1) + COUNT(vals2);
        int     ok = (hset_cnt(&se1) == expected_total) && (res == &se1);
        // все элементы должны быть доступны
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2)),
            "Disjoint sets: cnt=%d (expected %d), res=%p",
            hset_cnt(&se1), expected_total, (void*)res
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    // fs
    test_sub("subtest %d: empty union empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset *res = hset_union(&a, &b);   // res == &a

        test_validatefree(
            res == &a,
            (hset_free(res), hset_free(&b)),
            "Returned pointer %p must be equal %p", res, &a
        );
        test_validatefree(
            res->count == 0,
            (hset_free(res), hset_free(&b)),
            "Empty ∪ empty should be empty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty union nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_union(&a, &b);

        test_validatefree(
            res->count == 2 &&
            HSET_HAS_FS(res, "/tmp/x") &&
            HSET_HAS_FS(res, "/tmp/y"),
            (hset_free(res), hset_free(&b)),
            "Empty ∪ nonempty should equal nonempty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty union empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset *res = hset_union(&a, &b);

        test_validatefree(
            res->count == 3 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/b") &&
            HSET_HAS_FS(res, "/tmp/c"),
            (hset_free(res), hset_free(&b)),
            "Nonempty ∪ empty should equal nonempty"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: partial overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset *res = hset_union(&a, &b);

        test_validatefree(
            res->count == 5 &&
            HSET_HAS_FS(res, "/tmp/1") &&
            HSET_HAS_FS(res, "/tmp/2") &&
            HSET_HAS_FS(res, "/tmp/3") &&
            HSET_HAS_FS(res, "/tmp/4") &&
            HSET_HAS_FS(res, "/tmp/5"),
            (hset_free(res), hset_free(&b)),
            "Union of partially overlapping sets should contain all unique elements"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: no overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset *res = hset_union(&a, &b);

        test_validatefree(
            res->count == 4 &&
            HSET_HAS_FS(res, "/tmp/1") &&
            HSET_HAS_FS(res, "/tmp/2") &&
            HSET_HAS_FS(res, "/tmp/3") &&
            HSET_HAS_FS(res, "/tmp/4"),
            (hset_free(res), hset_free(&b)),
            "Union of disjoint sets should be the combination"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: identical sets", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset *res = hset_union(&a, &b);

        test_validatefree(
            res->count == 2 &&
            HSET_HAS_FS(res, "/tmp/x") &&
            HSET_HAS_FS(res, "/tmp/y"),
            (hset_free(res), hset_free(&b)),
            "Union of identical sets should equal original"
        );
        hset_free(res);
        hset_free(&b);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            hset *res = hset_union(&fs_set, &int_set);
            hset_free(res);
            hset_free(&int_set);
            test_validate(false, "Type mismatch should have raised SIGINT");
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logsimple("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 18 ---------------------------------

static TestStatus
tf18(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое ∪ пустое = пустое */
    test_sub("subtest %d: empty union empty", ++subnum);
    {
        hset    se1 = hset_init(10, VALUE64_INT);
        hset    se2 = hset_init(10, VALUE64_INT);
        hset    res = hset_init_union(&se1, &se2);

        int     ok = (hset_cnt(&res) == 0) &&
                     (hset_cnt(&se1) == 0) &&
                     (hset_cnt(&se2) == 0);
        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Empty union empty: res=%d, se1=%d, se2=%d (expected all 0)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2)
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 2. Непустое ∪ пустое = копия se1, исходные не изменились */
    test_sub("subtest %d: non-empty union empty", ++subnum);
    {
        int     vals[] = {3, 7, 11};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    empty = hset_init(10, VALUE64_INT);

        int     cnt1_before = hset_cnt(&se1);
        hset    res = hset_init_union(&se1, &empty);
        int     expected = COUNT(vals);

        int     ok = (hset_cnt(&res) == expected) &&
                     (hset_cnt(&se1) == cnt1_before) &&
                     (hset_cnt(&empty) == 0);
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&res, LITERAL64_INT(vals[i]));
        // se1 должен остаться без изменений
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&se1, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&empty), hset_free(&res)),
            "Non-empty union empty: res=%d, se1=%d (expected %d, %d)",
            hset_cnt(&res), hset_cnt(&se1), expected, cnt1_before
        );
        hset_free(&se1);
        hset_free(&empty);
        hset_free(&res);
    }

    /* 3. Пустое ∪ непустое = копия se2, исходные не изменились */
    test_sub("subtest %d: empty union non-empty", ++subnum);
    {
        int     vals[] = {5, 9};
        hset    empty = hset_init(10, VALUE64_INT);
        hset    se2 = hset_from_intarr(vals, COUNT(vals));

        int     cnt2_before = hset_cnt(&se2);
        hset    res = hset_init_union(&empty, &se2);

        int     expected = COUNT(vals);
        int     ok = (hset_cnt(&res) == expected) &&
                     (hset_cnt(&empty) == 0) &&
                     (hset_cnt(&se2) == cnt2_before);
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&res, LITERAL64_INT(vals[i]));
        for (int i = 0; ok && i < expected; i++)
            ok = hset_get(&se2, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            (hset_free(&empty), hset_free(&se2), hset_free(&res)),
            "Empty union non-empty: res=%d, se2=%d (expected %d, %d)",
            hset_cnt(&res), hset_cnt(&se2), expected, cnt2_before
        );
        hset_free(&empty);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 4. Одинаковые множества: результат совпадает с исходными */
    test_sub("subtest %d: identical sets", ++subnum);
    {
        int     vals[] = {1, 2, 3, 4};
        hset    se1 = hset_from_intarr(vals, COUNT(vals));
        hset    se2 = hset_from_intarr(vals, COUNT(vals));

        int     cnt_before = hset_cnt(&se1);
        hset    res = hset_init_union(&se1, &se2);

        int     expected = COUNT(vals);
        int     ok = (hset_cnt(&res) == expected) &&
                     (hset_cnt(&se1) == cnt_before) &&
                     (hset_cnt(&se2) == cnt_before);
        for (int i = 0; ok && i < expected; i++) {
            ok = hset_get(&res, LITERAL64_INT(vals[i])) &&
                 hset_get(&se1, LITERAL64_INT(vals[i])) &&
                 hset_get(&se2, LITERAL64_INT(vals[i]));
        }

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Identical sets: res=%d, se1=%d, se2=%d (expected all %d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2), expected
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 5. Частичное перекрытие: объединение содержит все уникальные элементы */
    test_sub("subtest %d: partial overlap", ++subnum);
    {
        int     vals1[] = {1, 2, 3, 4, 5};
        int     vals2[] = {3, 5, 6, 7};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));

        int     cnt1_before = hset_cnt(&se1);
        int     cnt2_before = hset_cnt(&se2);
        hset    res = hset_init_union(&se1, &se2);

        int     expected_vals[] = {1, 2, 3, 4, 5, 6, 7};
        int     expected_cnt = COUNT(expected_vals);
        int     ok = (hset_cnt(&res) == expected_cnt) &&
                     (hset_cnt(&se1) == cnt1_before) &&
                     (hset_cnt(&se2) == cnt2_before);
        for (int i = 0; ok && i < expected_cnt; i++)
            ok = hset_get(&res, LITERAL64_INT(expected_vals[i]));
        // se1 и se2 не тронуты
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se2, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Partial overlap: res=%d, se1=%d, se2=%d (expected res=%d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2), expected_cnt
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }

    /* 6. Непересекающиеся множества: все элементы обоих в результате */
    test_sub("subtest %d: disjoint sets", ++subnum);
    {
        int     vals1[] = {100, 200};
        int     vals2[] = {300, 400};
        hset    se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset    se2 = hset_from_intarr(vals2, COUNT(vals2));

        int     cnt1_before = hset_cnt(&se1);
        int     cnt2_before = hset_cnt(&se2);
        hset    res = hset_init_union(&se1, &se2);

        int     expected_total = COUNT(vals1) + COUNT(vals2);
        int     ok = (hset_cnt(&res) == expected_total) &&
                     (hset_cnt(&se1) == cnt1_before) &&
                     (hset_cnt(&se2) == cnt2_before);
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&res, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&res, LITERAL64_INT(vals2[i]));
        // исходные целы
        for (int i = 0; ok && i < COUNT(vals1); i++)
            ok = hset_get(&se1, LITERAL64_INT(vals1[i]));
        for (int i = 0; ok && i < COUNT(vals2); i++)
            ok = hset_get(&se2, LITERAL64_INT(vals2[i]));

        test_validatefree(
            ok,
            (hset_free(&se1), hset_free(&se2), hset_free(&res)),
            "Disjoint sets: res=%d, se1=%d, se2=%d (expected res=%d)",
            hset_cnt(&res), hset_cnt(&se1), hset_cnt(&se2), expected_total
        );
        hset_free(&se1);
        hset_free(&se2);
        hset_free(&res);
    }
    // fs
    test_sub("subtest %d: empty union empty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = hset_init_fs(10);
        hset res = hset_init_union(&a, &b);

        test_validatefree(
            res.count == 0,
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty ∪ empty should be empty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: empty union nonempty", ++subnum);
    {
        hset a = hset_init_fs(10);
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_union(&a, &b);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/x") &&
            HSET_HAS_FS(&res, "/tmp/y"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Empty ∪ nonempty should equal nonempty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: nonempty union empty", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset b = hset_init_fs(10);
        hset res = hset_init_union(&a, &b);

        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/b") &&
            HSET_HAS_FS(&res, "/tmp/c"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Nonempty ∪ empty should equal nonempty"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: partial overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4", "/tmp/5");
        hset res = hset_init_union(&a, &b);

        test_validatefree(
            res.count == 5 &&
            HSET_HAS_FS(&res, "/tmp/1") &&
            HSET_HAS_FS(&res, "/tmp/2") &&
            HSET_HAS_FS(&res, "/tmp/3") &&
            HSET_HAS_FS(&res, "/tmp/4") &&
            HSET_HAS_FS(&res, "/tmp/5"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Union of partially overlapping sets should contain all unique elements"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: no overlap", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2");
        hset b = HSET_CREATEFS_ASSTR("/tmp/3", "/tmp/4");
        hset res = hset_init_union(&a, &b);

        test_validatefree(
            res.count == 4 &&
            HSET_HAS_FS(&res, "/tmp/1") &&
            HSET_HAS_FS(&res, "/tmp/2") &&
            HSET_HAS_FS(&res, "/tmp/3") &&
            HSET_HAS_FS(&res, "/tmp/4"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Union of disjoint sets should be the combination"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: identical sets", ++subnum);
    {
        hset a = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset b = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset res = hset_init_union(&a, &b);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/x") &&
            HSET_HAS_FS(&res, "/tmp/y"),
            (hset_free(&a), hset_free(&b), hset_free(&res)),
            "Union of identical sets should equal original"
        );
        hset_free(&a);
        hset_free(&b);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: FS vs INT type mismatch raises SIGINT", ++subnum);
    {
        hset fs_set  = HSET_CREATEFS_ASSTR("/tmp/z");
        hset int_set = hset_init_int(10);
        hset_set(&int_set, LITERAL64_INT(42));

        if (!try()) {
            hset res = hset_init_union(&fs_set, &int_set);
            hset_free(&fs_set);
            hset_free(&int_set);
            hset_free(&res);
            test_validate(false, "Type mismatch should have raised SIGINT");
        } else {
            hset_free(&fs_set);
            hset_free(&int_set);
            logsimple("Exception correctly raised on type mismatch");
        }
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 21 ---------------------------------

static void             print_as_int(value64 v){
    printf("%4d\t", v.ival);
}

static void             dummy_const_proc(value64 v) {
    (void)v; /* ничего не делаем */
}

static int foreach_fs_count;
static void foreach_fs_count_and_log(value64 v) {
    fs *f = value64_fs(v);
    logsimple("const_foreach visited: %s", f ? fs_str(f) : "NULL");
    foreach_fs_count++;
}

static TestStatus
tf21(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ---------- 1. hset_const_foreach: только чтение ---------- */
    test_sub("subtest %d: const_foreach read-only", ++subnum);
    {
        int     vals[] = {10, 20, 30, 40, 50};
        hset    se = hset_from_intarr(vals, COUNT(vals) );
        // just a printf
        hset_const_foreach(&se, print_as_int);
        hset_free(&se);
    }

    // Так как мы убрали ctx, для сбора данных в const_foreach нужен другой подход.
    // Пока оставим простую проверку: просто вызовем и убедимся, что множество не изменилось.
    test_sub("subtest %d: const_foreach does not modify", ++subnum);
    {
        int     vals[] = {1, 2, 3};
        hset    se = hset_from_intarr(vals, COUNT(vals));

        hset_const_foreach(&se, dummy_const_proc); // ничего не делает, но гарантирует проход

        // Проверяем, что все элементы на месте и их количество не изменилось
        test_validatefree(
            hset_cnt(&se) == COUNT(vals),
            hset_free(&se),
            "const_foreach: cnt=%d, expected %d", hset_cnt(&se), COUNT(vals)
        );
        for (int i = 0; i < COUNT(vals); i++) {
            test_validatefree(
                hset_get(&se, LITERAL64_INT(vals[i])),
                hset_free(&se),
                "const_foreach: missing value %d", vals[i]
            );
        }
        hset_free(&se);
    }
    // fs
    test_sub("subtest %d: const_foreach over non‑empty FS set", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/ffffffffftmp/a", "/ffffffffffftmp/b", "/ffffffffffffftmp/c");
        int old_count = se.count;

        foreach_fs_count = 0;
        hset_const_foreach(&se, foreach_fs_count_and_log);  // теперь и считает, и печатает

        test_validatefree(
            foreach_fs_count == old_count,
            hset_free(&se),
            "const_foreach visited %d elements, expected %d",
            foreach_fs_count, old_count
        );
        test_validatefree(
            HSET_HAS_FS(&se, "/ffffffffftmp/a") &&
            HSET_HAS_FS(&se, "/ffffffffffftmp/b") &&
            HSET_HAS_FS(&se, "/ffffffffffffftmp/c"),
            hset_free(&se),
            "const_foreach must not modify the set"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: const_foreach over empty FS set", ++subnum);
    {
        hset se = hset_init_fs(10);
        foreach_fs_count = 0;
        hset_const_foreach(&se, foreach_fs_count_and_log);

        test_validatefree(
            foreach_fs_count == 0,
            hset_free(&se),
            "const_foreach on empty set should invoke callback 0 times, got %d",
            foreach_fs_count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: const_foreach callback sees valid FS values", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");

        foreach_fs_count = 0;
        hset_const_foreach(&se, foreach_fs_count_and_log);

        test_validatefree(
            foreach_fs_count == 2 &&
            hset_validate(stdout, &se),
            hset_free(&se),
            "const_foreach: visited %d elements, expected 2; set must be valid",
            foreach_fs_count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 22 ---------------------------------
static TestStatus
tf22(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Сумма int */
    test_sub("subtest %d: reduce sum int", ++subnum);
    {
        int     vals[] = {1, 2, 3, 4, 5};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_int(&se, hset_sum_int);

        test_validatefree(
            res.value.ival == 15,
            hset_free(&se),
            "Sum int: value = %d, expected 15", res.value.ival
        );
        test_validatefree(
            res.count == 5,
            hset_free(&se),
            "Sum int: count = %d, expected 5", res.count
        );
        hset_free(&se);
    }

    /* 2. Счётчик int */
    test_sub("subtest %d: reduce count int", ++subnum);
    {
        int     vals[] = {10, 20, 30};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_int(&se, hset_count_int);

        test_validatefree(
            res.count == COUNT(vals),
            hset_free(&se),
            "Count int: count = %d, expected %d", res.count, COUNT(vals)
        );
        // value не должно измениться (осталось нулевым)
        test_validatefree(
            res.value.ival == 0,
            hset_free(&se),
            "Count int: value = %d, expected 0 (unused)", res.value.ival
        );
        hset_free(&se);
    }

    /* 3. Максимум int */
    test_sub("subtest %d: reduce max int", ++subnum);
    {
        int     vals[] = {5, 2, 9, 1, 7};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_int(&se, hset_max_int);

        test_validatefree(
            res.value.ival == 9,
            hset_free(&se),
            "Max int: value = %d, expected 9", res.value.ival
        );
        test_validatefree(
            res.count == 1,
            hset_free(&se),
            "Max int: count = %d, expected 1 (flag)", res.count
        );
        hset_free(&se);
    }

    /* 4. Минимум int */
    test_sub("subtest %d: reduce min int", ++subnum);
    {
        int     vals[] = {3, 8, 1, 6, 4};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_int(&se, hset_min_int);

        test_validatefree(
            res.value.ival == 1,
            hset_free(&se),
            "Min int: value = %d, expected 1", res.value.ival
        );
        test_validatefree(
            res.count == 1,
            hset_free(&se),
            "Min int: count = %d, expected 1 (flag)", res.count
        );
        hset_free(&se);
    }

    /* 5. Пустое множество: сумма и счётчик */
    test_sub("subtest %d: reduce on empty set", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_INT);
        hset_accum res = hset_reduce_int(&se, hset_sum_int);

        test_validatefree(
            res.value.ival == 0,
            hset_free(&se),
            "Empty sum: value = %d, expected 0", res.value.ival
        );
        test_validatefree(
            res.count == 0,
            hset_free(&se),
            "Empty sum: count = %d, expected 0", res.count
        );
        hset_free(&se);
    }

    /* 6. Пустое множество: max (count остаётся 0) */
    test_sub("subtest %d: reduce max on empty set", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_INT);
        hset_accum res = hset_reduce_int(&se, hset_max_int);

        test_validatefree(
            res.count == 0,
            hset_free(&se),
            "Empty max: count = %d, expected 0 (no elements)", res.count
        );
        // value не важно, т.к. нет элементов
        hset_free(&se);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 23 ---------------------------------

static TestStatus
tf23(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Сумма double (только конечные числа) */
    test_sub("subtest %d: sum double", ++subnum);
    {
        double  vals[] = {1.5, 2.5, 3.5};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_dbl(&se, hset_sum_dbl);

        test_validatefree(
            fabs(res.value.dval - 7.5) < 0.0001,
            hset_free(&se),
            "Sum double: value = %f, expected 7.5", res.value.dval
        );
        test_validatefree(
            res.count == 3,
            hset_free(&se),
            "Sum double: count = %d, expected 3", res.count
        );
        hset_free(&se);
    }

    /* 2. Счётчик double (только конечные числа) */
    test_sub("subtest %d: count double", ++subnum);
    {
        double  vals[] = {10.0, 20.0, 30.0};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_dbl(&se, hset_count_dbl);

        test_validatefree(
            res.count == 3,
            hset_free(&se),
            "Count double: count = %d, expected 3", res.count
        );
        test_validatefree(
            res.value.dval == 0.0,
            hset_free(&se),
            "Count double: value = %f, expected 0.0 (unused)", res.value.dval
        );
        hset_free(&se);
    }

    /* 3. Максимум double */
    test_sub("subtest %d: max double", ++subnum);
    {
        double  vals[] = {5.2, 2.3, 9.8, 1.0, 7.4};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_dbl(&se, hset_max_dbl);

        test_validatefree(
            fabs(res.value.dval - 9.8) < 0.0001,
            hset_free(&se),
            "Max double: value = %f, expected 9.8", res.value.dval
        );
        test_validatefree(
            res.count == 1,
            hset_free(&se),
            "Max double: count = %d, expected 1 (flag)", res.count
        );
        hset_free(&se);
    }

    /* 4. Минимум double (все отрицательные) */
    test_sub("subtest %d: min double (all negative)", ++subnum);
    {
        double  vals[] = {-5.0, -2.5, -9.1, -1.0};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_dbl(&se, hset_min_dbl);

        test_validatefree(
            fabs(res.value.dval - (-9.1)) < 0.0001,
            hset_free(&se),
            "Min double: value = %f, expected -9.1", res.value.dval
        );
        test_validatefree(
            res.count == 1,
            hset_free(&se),
            "Min double: count = %d, expected 1 (flag)", res.count
        );
        hset_free(&se);
    }

    /* 5. Пустое множество: сумма double */
    test_sub("subtest %d: sum double on empty set", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_DBL);
        hset_accum res = hset_reduce_dbl(&se, hset_sum_dbl);

        test_validatefree(
            res.value.dval == 0.0,
            hset_free(&se),
            "Empty sum double: value = %f, expected 0.0", res.value.dval
        );
        test_validatefree(
            res.count == 0,
            hset_free(&se),
            "Empty sum double: count = %d, expected 0", res.count
        );
        hset_free(&se);
    }

    /* 6. Пустое множество: max double (count должен остаться 0) */
    test_sub("subtest %d: max double on empty set", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_DBL);
        hset_accum res = hset_reduce_dbl(&se, hset_max_dbl);

        test_validatefree(
            res.count == 0,
            hset_free(&se),
            "Empty max double: count = %d, expected 0", res.count
        );
        hset_free(&se);
    }

    /* 7. Несколько значений, включая очень большое, для sum */
    test_sub("subtest %d: sum double with large value", ++subnum);
    {
        double  vals[] = {1e10, 2e10, 3e10};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        hset_accum res = hset_reduce_dbl(&se, hset_sum_dbl);

        test_validatefree(
            fabs(res.value.dval - 6e10) < 1e5,
            hset_free(&se),
            "Sum double large: value = %f, expected 6e10", res.value.dval
        );
        test_validatefree(
            res.count == 3,
            hset_free(&se),
            "Sum double large: count = %d, expected 3", res.count
        );
        hset_free(&se);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 26 ---------------------------------
static TestStatus
tf26(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== hset_any ========== */

    /* 1. any: пустое с пустым -> false (нет элементов для поиска) */
    test_sub("subtest %d:hset_any  empty vs empty", ++subnum);
    {
        hset se1 = hset_init(10, VALUE64_INT);
        hset se2 = hset_init(10, VALUE64_INT);
        test_validatefree(
            !hset_any(&se1, &se2),   // false
            (hset_free(&se1), hset_free(&se2)),
            "hset_any empty vs empty must be false"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 2. any: пустое с непустым -> false */
    test_sub("subtest %d:hset_any  empty vs non-empty", ++subnum);
    {
        int vals[] = {1, 2, 3};
        hset empty = hset_init(10, VALUE64_INT);
        hset nonempty = hset_from_intarr(vals, COUNT(vals));
        test_validatefree(
            !hset_any(&empty, &nonempty),   // false
            (hset_free(&empty), hset_free(&nonempty)),
            "hset_any empty vs non-empty must be false"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    /* 3. any: непустое с пустым -> false (нет элементов в se2 для поиска) */
    test_sub("subtest %d: any non‑empty vs empty", ++subnum);
    {
        int vals[] = {1, 2, 3};
        hset empty = hset_init(10, VALUE64_INT);
        hset nonempty = hset_from_intarr(vals, COUNT(vals));
        test_validatefree(
            !hset_any(&nonempty, &empty),   // false
            (hset_free(&empty), hset_free(&nonempty)),
            "hset_any non‑empty vs empty must be false"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }
    /* 4. any: непустое с непустым, есть пересечение -> true */
    test_sub("subtest %d:hset_any  with overlap", ++subnum);
    {
        int vals1[] = {1, 3, 5, 7};
        int vals2[] = {5, 9, 11};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        test_validatefree(
            hset_any(&se1, &se2),   // true
            (hset_free(&se1), hset_free(&se2)),
            "hset_any with overlap must be true"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 5. any: непустое с непустым, нет пересечения -> false */
    test_sub("subtest %d:hset_any  disjoint", ++subnum);
    {
        int vals1[] = {100, 200};
        int vals2[] = {300, 400};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        test_validatefree(
            !hset_any(&se1, &se2),   // false
            (hset_free(&se1), hset_free(&se2)),
            "hset_any disjoint must be false"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 6. any: одинаковые множества -> true (все элементы se2 есть в se1) */
    test_sub("subtest %d:hset_any  identical sets", ++subnum);
    {
        int vals[] = {7, 8, 9};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        test_validatefree(
            hset_any(&se1, &se2),   // true
            (hset_free(&se1), hset_free(&se2)),
            "hset_any identical sets must be true"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* ========== hset_notexists ========== */

    /* 7. notexists: пустое с пустым -> true (нет общих элементов) */
    test_sub("subtest %d: notexists empty vs empty", ++subnum);
    {
        hset se1 = hset_init(10, VALUE64_INT);
        hset se2 = hset_init(10, VALUE64_INT);
        test_validatefree(
            hset_notexists(&se1, &se2),   // true
            (hset_free(&se1), hset_free(&se2)),
            "hset_notexists empty vs empty must be true"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 8. notexists: пустое с непустым -> true */
    test_sub("subtest %d: notexists empty vs non-empty", ++subnum);
    {
        int vals[] = {1, 2, 3};
        hset empty = hset_init(10, VALUE64_INT);
        hset nonempty = hset_from_intarr(vals, COUNT(vals));
        test_validatefree(
            hset_notexists(&empty, &nonempty),   // true
            (hset_free(&empty), hset_free(&nonempty)),
            "hset_notexists empty vs non-empty must be true"
        );
        hset_free(&empty);
        hset_free(&nonempty);
    }

    /* 9. notexists: непустое с непустым, нет пересечения -> true */
    test_sub("subtest %d: notexists disjoint", ++subnum);
    {
        int vals1[] = {10, 20};
        int vals2[] = {30, 40};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        test_validatefree(
            hset_notexists(&se1, &se2),   // true
            (hset_free(&se1), hset_free(&se2)),
            "hset_notexists disjoint must be true"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 10. notexists: непустое с непустым, есть пересечение -> false */
    test_sub("subtest %d: notexists with overlap", ++subnum);
    {
        int vals1[] = {1, 2, 3};
        int vals2[] = {3, 4, 5};
        hset se1 = hset_from_intarr(vals1, COUNT(vals1));
        hset se2 = hset_from_intarr(vals2, COUNT(vals2));
        test_validatefree(
            !hset_notexists(&se1, &se2),   // false
            (hset_free(&se1), hset_free(&se2)),
            "hset_notexists with overlap must be false"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    /* 11. notexists: одинаковые множества -> false */
    test_sub("subtest %d: notexists identical sets", ++subnum);
    {
        int vals[] = {5, 6, 7};
        hset se1 = hset_from_intarr(vals, COUNT(vals));
        hset se2 = hset_from_intarr(vals, COUNT(vals));
        test_validatefree(
            !hset_notexists(&se1, &se2),   // false
            (hset_free(&se1), hset_free(&se2)),
            "hset_notexists identical sets must be false"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST reduce_fs_count_max_min ---------------------------------
static TestStatus
tf_reduce_fs_count_max_min(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: count non-empty set", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset_accum acc = hset_reduce_fs(&se, hset_count_fs);
        test_validatefree(
            acc.count == 3,
            hset_free(&se),
            "Count: expected 3, got %d", acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: count empty set", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset_accum acc = hset_reduce_fs(&se, hset_count_fs);
        test_validatefree(
            acc.count == 0,
            hset_free(&se),
            "Count empty: expected 0, got %d", acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: max string (lexicographic)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/b", "/tmp/a", "/tmp/c");
        hset_accum acc = hset_reduce_fs(&se, hset_max_fs);
        const char *res = fs_str(hset_accum_fs(&acc) );
        test_validatefree(
            acc.value.fsval != NULL && res != NULL && strcmp(res, "/tmp/c") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc) ) ),
            "Max: expected '/tmp/c', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc) );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: max string single element", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/single");
        hset_accum acc = hset_reduce_fs(&se, hset_max_fs);
        const char *res = fs_str(acc.value.fsval);
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strcmp(res, "/tmp/single") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc) ) ),
            "Max single: expected '/tmp/single', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc) );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: min string (lexicographic)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/b", "/tmp/a", "/tmp/c");
        hset_accum acc = hset_reduce_fs(&se, hset_min_fs);
        const char *res = fs_str(hset_accum_fs(&acc) );   //OMG
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strcmp(res, "/tmp/a") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc) ) ),
            "Min: expected '/tmp/a', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc) );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: min string single element", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/single");
        hset_accum acc = hset_reduce_fs(&se, hset_min_fs);
        const char *res = fs_str(value64_fs(acc.value) );
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strcmp(res, "/tmp/single") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc) ) ),
            "Min single: expected '/tmp/single', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc) );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: max length string", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        hset_accum acc = hset_reduce_fs(&se, hset_maxlen_fs);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strcmp(res, "/tmp/ccc") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc))),
            "Maxlen: expected '/tmp/ccc', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc));
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: max length single element", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/single");
        hset_accum acc = hset_reduce_fs(&se, hset_maxlen_fs);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strcmp(res, "/tmp/single") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc))),
            "Maxlen single: expected '/tmp/single', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc));
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: max length with equal lengths", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/abc", "/tmp/xyz", "/tmp/123");
        hset_accum acc = hset_reduce_fs(&se, hset_maxlen_fs);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strlen(res) == 8,
            (hset_free(&se), fs_free(hset_accum_fs(&acc))),
            "Maxlen equal: expected any string of length 8, got '%s' (len %zu)",
            res ? res : "NULL", res ? strlen(res) : 0
        );
        fs_free(hset_accum_fs(&acc));
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: min length string", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/aa", "/tmp/b", "/tmp/ccc");
        hset_accum acc = hset_reduce_fs(&se, hset_minlen_fs);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strcmp(res, "/tmp/b") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc))),
            "Minlen: expected '/tmp/b', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc));
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: min length single element", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/single");
        hset_accum acc = hset_reduce_fs(&se, hset_minlen_fs);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strcmp(res, "/tmp/single") == 0,
            (hset_free(&se), fs_free(hset_accum_fs(&acc))),
            "Minlen single: expected '/tmp/single', got '%s'", res ? res : "NULL"
        );
        fs_free(hset_accum_fs(&acc));
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: min length with equal lengths", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/abc", "/tmp/xyz", "/tmp/123");
        hset_accum acc = hset_reduce_fs(&se, hset_minlen_fs);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            hset_accum_fs(&acc) != NULL && res != NULL && strlen(res) == 8,
            (hset_free(&se), fs_free(hset_accum_fs(&acc))),
            "Minlen equal: expected any string of length 8, got '%s' (len %zu)",
            res ? res : "NULL", res ? strlen(res) : 0
        );
        fs_free(hset_accum_fs(&acc));
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: sum of lengths", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/yy", "/tmp/zzz");
        hset_accum acc = hset_reduce_int(&se, hset_sumlen_fs);
        test_validatefree(
            hset_accum_long(&acc) == 21,
            hset_free(&se),
            "Sumlen: expected 21, got %ld", hset_accum_long(&acc)
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: sum of lengths empty set", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset_accum acc = hset_reduce_int(&se, hset_sumlen_fs);
        test_validatefree(
            hset_accum_long(&acc) == 0,
            hset_free(&se),
            "Sumlen empty: expected 0, got %ld", hset_accum_long(&acc)
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST reduce_fsagg ---------------------------------
static TestStatus
tf_reduce_fsagg(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: aggregate empty set", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset_accum acc = hset_reduce_fsagg(&se, hset_agg_fs, ", ");
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            res == NULL,                         // пустое множество → нет строки
            (hset_free(&se), hset_accum_free(&acc)),
            "Agg empty: expected NULL, got '%s'", res ? res : "NULL"
        );
        hset_accum_free(&acc);
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: aggregate single element, no separator", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x");
        hset_accum acc = hset_reduce_fsagg(&se, hset_agg_fs, NULL);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            res != NULL && strcmp(res, "/tmp/x") == 0,
            (hset_free(&se), hset_accum_free(&acc)),
            "Agg single: expected '/tmp/x', got '%s'", res ? res : "NULL"
        );
        hset_accum_free(&acc);
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: aggregate multiple elements with separator", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        hset_accum acc = hset_reduce_fsagg(&se, hset_agg_fs, ", ");
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            res &&
            strstr(res, "/tmp/a") && strstr(res, "/tmp/b") && strstr(res, "/tmp/c") &&
            strlen(res) == 22,
            (hset_free(&se), hset_accum_free(&acc)),
            "Agg multi: result '%s' must contain all three paths in any order",
            res ? res : "NULL"
        );
        hset_accum_free(&acc);
        hset_free(&se);
    }

    fs_alloc_check(true);

    test_sub("subtest %d: aggregate without separator (pure concat)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("abc", "def", "ghi");
        hset_accum acc = hset_reduce_fsagg(&se, hset_agg_fs, NULL);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            res &&
            strstr(res, "abc") && strstr(res, "def") && strstr(res, "ghi") &&
            strlen(res) == 9,
            (hset_free(&se), hset_accum_free(&acc)),
            "Agg no sep: result '%s' must contain all three parts in any order", res ? res : "NULL"
        );
        hset_accum_free(&acc);
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_filter / hset_init_filter FS -------------------------
static TestStatus
tf_filter_fs(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== hset_filter (in-place) ========== */
    test_sub("subtest %d: filter empty set", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset *res = hset_filter(&se, value64_filter_true, LITERAL64_ZERO);
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Filter empty: must return same pointer and stay empty"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filter keep all (true predicate)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        hset *res = hset_filter(&se, value64_filter_true, LITERAL64_ZERO);
        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/bb") &&
            HSET_HAS_FS(res, "/tmp/ccc"),
            hset_free(res),
            "Filter true must preserve all elements"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filter remove all (false predicate)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        hset *res = hset_filter(&se, value64_filter_false, LITERAL64_ZERO);
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Filter false must remove all elements"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filter by minimum length 7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        value64 data = LITERAL64_INT(7);   // минимальная длина 7
        hset *res = hset_filter(&se, value64_filter_fsminlen_int, data);
        test_validatefree(
            res == &se && se.count == 3 &&
            !HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/bb") &&
            HSET_HAS_FS(res, "/tmp/ccc") &&
            HSET_HAS_FS(res, "/tmp/dddd"),
            hset_free(res),
            "Filter minlen=7: should remove '/tmp/a' (length 6)"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filter by prefix '/tmp' (in-place)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/b", "/tmp/c", "/usr/d");
        value64 prefix = value64_createstr("/tmp");
        hset *res = hset_filter(&se, value64_filter_fsprefix_str, prefix);
        value64_freestr(&prefix);   // освобождаем параметр после фильтрации

        test_validatefree(
            res == &se && se.count == 2 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/c") &&
            !HSET_HAS_FS(res, "/var/b") &&
            !HSET_HAS_FS(res, "/usr/d"),
            hset_free(res),
            "Filter prefix '/tmp': should keep only /tmp/a and /tmp/c"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filter by exact string '/tmp/foo' (in-place)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/foo", "/tmp/bar", "/tmp/foo");
        value64 target = value64_createstr("/tmp/foo");
        hset *res = hset_filter(&se, value64_filter_fsequals_str, target);
        value64_freestr(&target);

        test_validatefree(
            res == &se && se.count == 1 &&
            HSET_HAS_FS(res, "/tmp/foo") &&
            !HSET_HAS_FS(res, "/tmp/bar"),
            hset_free(res),
            "Filter exact '/tmp/foo': should leave one element"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    /* ========== hset_init_filter (new set) ========== */
    test_sub("subtest %d: init_filter from empty", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset res = hset_init_filter(&se, value64_filter_true, LITERAL64_ZERO);
        test_validatefree(
            res.count == 0,
            (hset_free(&se), hset_free(&res)),
            "Init_filter from empty must return empty set"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: init_filter by prefix '/tmp'", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/b", "/tmp/c", "/usr/d");
        value64 prefix = value64_createstr("/tmp");
        hset res = hset_init_filter(&se, value64_filter_fsprefix_str, prefix);
        value64_freestr(&prefix);

        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/a") && HSET_HAS_FS(&res, "/tmp/c") &&
            !HSET_HAS_FS(&res, "/var/b") && !HSET_HAS_FS(&res, "/usr/d"),
            (hset_free(&se), hset_free(&res)),
            "Init_filter by prefix '/tmp': should collect only matching paths"
        );
        test_validatefree(
            se.count == 4,
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged after init_filter"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: init_filter by minimum length 7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        value64 data = LITERAL64_INT(7);
        hset res = hset_init_filter(&se, value64_filter_fsminlen_int, data);

        test_validatefree(
            res.count == 3 &&
            !HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/bb") &&
            HSET_HAS_FS(&res, "/tmp/ccc") &&
            HSET_HAS_FS(&res, "/tmp/dddd"),
            (hset_free(&se), hset_free(&res)),
            "Init_filter minlen=7: should skip '/tmp/a'"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: init_filter by exact string '/tmp/foo'", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/foo", "/tmp/bar", "/tmp/foo");
        value64 target = value64_createstr("/tmp/foo");
        hset res = hset_init_filter(&se, value64_filter_fsequals_str, target);
        value64_freestr(&target);

        test_validatefree(
            res.count == 1 && HSET_HAS_FS(&res, "/tmp/foo"),
            (hset_free(&se), hset_free(&res)),
            "Init_filter exact '/tmp/foo': should return one element"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);
    /* ========== maxlen (длина <= data) ========== */
    test_sub("subtest %d: filter maxlen <= 7 (in-place)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        value64 data = LITERAL64_INT(7);   // оставить только строки длиной <= 7
        hset *res = hset_filter(&se, value64_filter_fsmaxlen_int, data);
        test_validatefree(
            res == &se && se.count == 2 &&
            HSET_HAS_FS(res, "/tmp/a") &&   // длина 6
            HSET_HAS_FS(res, "/tmp/bb") &&  // длина 7
            !HSET_HAS_FS(res, "/tmp/ccc") && // длина 8
            !HSET_HAS_FS(res, "/tmp/dddd"), // длина 9
            hset_free(res),
            "Filter maxlen 7: should keep /tmp/a and /tmp/bb only"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: init_filter maxlen <= 7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        value64 data = LITERAL64_INT(7);
        hset res = hset_init_filter(&se, value64_filter_fsmaxlen_int, data);
        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/bb") &&
            !HSET_HAS_FS(&res, "/tmp/ccc") &&
            !HSET_HAS_FS(&res, "/tmp/dddd"),
            (hset_free(&se), hset_free(&res)),
            "Init_filter maxlen 7: should collect /tmp/a and /tmp/bb"
        );
        test_validatefree(
            se.count == 4,
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    /* ========== len_eq (длина == data) ========== */
    test_sub("subtest %d: filter len == 7 (in-place)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        value64 data = LITERAL64_INT(7);   // оставить только длину 7
        hset *res = hset_filter(&se, value64_filter_fslen_int, data);
        test_validatefree(
            res == &se && se.count == 1 &&
            HSET_HAS_FS(res, "/tmp/bb") &&
            !HSET_HAS_FS(res, "/tmp/a") &&
            !HSET_HAS_FS(res, "/tmp/ccc"),
            hset_free(res),
            "Filter len==7: should keep only /tmp/bb"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: init_filter len == 7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        value64 data = LITERAL64_INT(7);
        hset res = hset_init_filter(&se, value64_filter_fslen_int, data);
        test_validatefree(
            res.count == 1 &&
            HSET_HAS_FS(&res, "/tmp/bb"),
            (hset_free(&se), hset_free(&res)),
            "Init_filter len==7: should collect /tmp/bb only"
        );
        test_validatefree(
            se.count == 3,
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_filter_fs(u)like_str -------------------------
static TestStatus
tf_filter_fsulike_str(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== like ========== */
    test_sub("subtest %d: filter like (in-place)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/foo", "/var/tmp/bar", "/usr/tmp", "/home/user");
        value64 substr = value64_createstr("tmp");
        hset *res = hset_filter(&se, value64_filter_fslike_str, substr);
        value64_freestr(&substr);

        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_FS(res, "/tmp/foo") &&
            HSET_HAS_FS(res, "/var/tmp/bar") &&
            HSET_HAS_FS(res, "/usr/tmp") &&
            !HSET_HAS_FS(res, "/home/user"),
            hset_free(res),
            "Filter like 'tmp': must keep only paths containing 'tmp'"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: init_filter like", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/foo", "/var/tmp/bar", "/usr/tmp", "/home/user");
        value64 substr = value64_createstr("tmp");
        hset res = hset_init_filter(&se, value64_filter_fslike_str, substr);
        value64_freestr(&substr);

        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/foo") &&
            HSET_HAS_FS(&res, "/var/tmp/bar") &&
            HSET_HAS_FS(&res, "/usr/tmp"),
            (hset_free(&se), hset_free(&res)),
            "Init_filter like 'tmp': must collect matching paths"
        );
        test_validatefree(
            se.count == 4,
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    /* ========== ulike ========== */
    test_sub("subtest %d: filter ulike (case‑insensitive)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/TMP/foo", "/var/tmp/bar", "/usr/Tmp", "/home/user");
        value64 substr = value64_createstr("tmp");
        hset *res = hset_filter(&se, value64_filter_fsulike_str, substr);
        value64_freestr(&substr);

        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_FS(res, "/TMP/foo") &&
            HSET_HAS_FS(res, "/var/tmp/bar") &&
            HSET_HAS_FS(res, "/usr/Tmp") &&
            !HSET_HAS_FS(res, "/home/user"),
            hset_free(res),
            "Filter ulike 'tmp': must match regardless of case"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: init_filter ulike", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/TMP/foo", "/var/tmp/bar", "/usr/Tmp", "/home/user");
        value64 substr = value64_createstr("tmp");
        hset res = hset_init_filter(&se, value64_filter_fsulike_str, substr);
        value64_freestr(&substr);

        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/TMP/foo") &&
            HSET_HAS_FS(&res, "/var/tmp/bar") &&
            HSET_HAS_FS(&res, "/usr/Tmp"),
            (hset_free(&se), hset_free(&res)),
            "Init_filter ulike 'tmp': case‑insensitive collect"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_create/delete like/ulike API -------------------------
static TestStatus
tf_like_ulike_simpliriers(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ===== hset_create_fslike_str (чувствительный LIKE) ===== */
    test_sub("subtest %d: create like (sensitive)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/foo", "/var/tmp/bar", "/usr/tmp", "/home/user");
        hset res = hset_create_fslike_str(&se, "tmp");
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/foo") &&
            HSET_HAS_FS(&res, "/var/tmp/bar") &&
            HSET_HAS_FS(&res, "/usr/tmp") &&
            !HSET_HAS_FS(&res, "/home/user"),
            (hset_free(&se), hset_free(&res)),
            "Create LIKE 'tmp': must collect 3 paths containing 'tmp'"
        );
        test_validatefree(
            se.count == 4,
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    /* ===== hset_create_fsulike_str (без учёта регистра) ===== */
    test_sub("subtest %d: create ulike (case-insensitive)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/TMP/foo", "/var/tmp/bar", "/usr/Tmp", "/home/user");
        hset res = hset_create_fsulike_str(&se, "tmp");
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/TMP/foo") &&
            HSET_HAS_FS(&res, "/var/tmp/bar") &&
            HSET_HAS_FS(&res, "/usr/Tmp") &&
            !HSET_HAS_FS(&res, "/home/user"),
            (hset_free(&se), hset_free(&res)),
            "Create ULIKE 'tmp': case-insensitive, should collect 3 paths"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    /* ---------- delete NOT like (sensitive) ---------- */
    test_sub("subtest %d: delete NOT like (sensitive)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/foo", "/var/tmp/bar", "/usr/tmp", "/home/user");
        hset *res = hset_apply_fslike_str(&se, "tmp");   // удаляем всё, что НЕ содержит "tmp"
        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_FS(res, "/tmp/foo") &&
            HSET_HAS_FS(res, "/var/tmp/bar") &&
            HSET_HAS_FS(res, "/usr/tmp") &&
            !HSET_HAS_FS(res, "/home/user"),
            hset_free(res),
            "Delete NOT LIKE 'tmp': must keep only paths containing 'tmp'"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    /* ---------- delete NOT ulike (case‑insensitive) ---------- */
    test_sub("subtest %d: delete NOT ulike (case-insensitive)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/TMP/foo", "/var/tmp/bar", "/usr/Tmp", "/home/user");
        hset *res = hset_apply_fsulike_str(&se, "tmp"); // удаляем всё, что НЕ содержит "tmp" без учёта регистра
        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_FS(res, "/TMP/foo") &&
            HSET_HAS_FS(res, "/var/tmp/bar") &&
            HSET_HAS_FS(res, "/usr/Tmp") &&
            !HSET_HAS_FS(res, "/home/user"),
            hset_free(res),
            "Delete NOT ULIKE 'tmp': case‑insensitive keep"
        );
        hset_free(res);
    }
    fs_alloc_check(true);
    /* ===== Граничные случаи ===== */
    test_sub("subtest %d: create like on empty set", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset res = hset_create_fslike_str(&se, "tmp");
        test_validatefree(
            res.count == 0 && hset_validate(stdout, &res),
            (hset_free(&se), hset_free(&res)),
            "Create LIKE on empty set must return empty"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete like on empty set", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset *res = hset_apply_fslike_str(&se, "tmp");
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Delete LIKE on empty set must stay empty"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create like with no matches", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b");
        hset res = hset_create_fslike_str(&se, "xyz");
        test_validatefree(
            res.count == 0,
            (hset_free(&se), hset_free(&res)),
            "Create LIKE 'xyz' should return empty set"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete like with no matches", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b");
        hset *res = hset_apply_fslike_str(&se, "tmp");
        test_validatefree(
            res == &se && se.count == 2 &&
            HSET_HAS_FS(res, "/tmp/a") && HSET_HAS_FS(res, "/tmp/b"),
            hset_free(res),
            "Delete NOT LIKE 'tmp' must keep all elements"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_create_fsminlen_int / hset_apply_fsminlen_int -------------------------
static TestStatus
tf_minlen_simpliriers(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== hset_create_fsminlen_int (новое множество) ========== */
    test_sub("subtest %d: create minlen=7 (empty set)", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset res = hset_create_fsminlen_int(&se, 7);
        test_validatefree(
            res.count == 0 && hset_validate(stdout, &res),
            (hset_free(&se), hset_free(&res)),
            "Create minlen=7 from empty: must return empty set"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create minlen=7 (mixed lengths)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        // длины: 6, 7, 8, 9  → при minlen=7 должны остаться /tmp/bb, /tmp/ccc, /tmp/dddd
        hset res = hset_create_fsminlen_int(&se, 7);
        test_validatefree(
            res.count == 3 &&
            !HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/bb") &&
            HSET_HAS_FS(&res, "/tmp/ccc") &&
            HSET_HAS_FS(&res, "/tmp/dddd"),
            (hset_free(&se), hset_free(&res)),
            "Create minlen=7: should skip /tmp/a (len=6)"
        );
        // источник не должен измениться
        test_validatefree(
            se.count == 4,
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged after create"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create minlen=10 (no matches)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        hset res = hset_create_fsminlen_int(&se, 10);
        test_validatefree(
            res.count == 0,
            (hset_free(&se), hset_free(&res)),
            "Create minlen=10: no element should match"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create minlen=0 (all match)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        hset res = hset_create_fsminlen_int(&se, 0);
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/bb") &&
            HSET_HAS_FS(&res, "/tmp/ccc"),
            (hset_free(&se), hset_free(&res)),
            "Create minlen=0: all elements must be included"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    /* ========== hset_apply_fsminlen_int (in-place) ========== */
    test_sub("subtest %d: delete NOT minlen=7 (empty set)", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset *res = hset_apply_fsminlen_int(&se, 7);
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Delete NOT minlen=7 from empty: must stay empty"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT minlen=7 (mixed lengths)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        // длины: 6,7,8,9. NOT minlen=7 => удаляем те, у которых длина < 7, т.е. /tmp/a (6).
        // должны остаться /tmp/bb, /tmp/ccc, /tmp/dddd
        hset *res = hset_apply_fsminlen_int(&se, 7);
        test_validatefree(
            res == &se && se.count == 3 &&
            !HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/bb") &&
            HSET_HAS_FS(res, "/tmp/ccc") &&
            HSET_HAS_FS(res, "/tmp/dddd"),
            hset_free(res),
            "Delete NOT minlen=7: should remove /tmp/a (len=6)"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT minlen=10 (remove all)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        hset *res = hset_apply_fsminlen_int(&se, 10);
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Delete NOT minlen=10: all elements have length < 10, must be removed"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT minlen=0 (keep all)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        hset *res = hset_apply_fsminlen_int(&se, 0);
        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/bb") &&
            HSET_HAS_FS(res, "/tmp/ccc"),
            hset_free(res),
            "Delete NOT minlen=0: all lengths >=0, nothing removed"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_create_fsmaxlen_int / hset_apply_fsmaxlen_int
//              hset_create_fslen_int / hset_apply_fslen_int -------------------------
static TestStatus
tf_maxlen_simpliriers(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== maxlen ========== */
    test_sub("subtest %d: create maxlen=7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        // длины: 6,7,8,9 → при maxlen=7 должны остаться /tmp/a, /tmp/bb
        hset res = hset_create_fsmaxlen_int(&se, 7);
        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/bb") &&
            !HSET_HAS_FS(&res, "/tmp/ccc") &&
            !HSET_HAS_FS(&res, "/tmp/dddd"),
            (hset_free(&se), hset_free(&res)),
            "Create maxlen=7: should keep /tmp/a and /tmp/bb"
        );
        test_validatefree(se.count == 4, (hset_free(&se), hset_free(&res)), "Source unchanged");
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT maxlen=7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        // NOT maxlen=7 → удалить всё, что НЕ <= 7, т.е. удалить /tmp/ccc(8), /tmp/dddd(9)
        hset *res = hset_apply_fsmaxlen_int(&se, 7);
        test_validatefree(
            res == &se && se.count == 2 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/bb") &&
            !HSET_HAS_FS(res, "/tmp/ccc") &&
            !HSET_HAS_FS(res, "/tmp/dddd"),
            hset_free(res),
            "Delete NOT maxlen=7: must keep only length <= 7"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    /* ========== len_eq ========== */
    test_sub("subtest %d: create len=7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        // только /tmp/bb имеет длину 7
        hset res = hset_create_fslen_int(&se, 7);
        test_validatefree(
            res.count == 1 &&
            HSET_HAS_FS(&res, "/tmp/bb") &&
            !HSET_HAS_FS(&res, "/tmp/a") &&
            !HSET_HAS_FS(&res, "/tmp/ccc") &&
            !HSET_HAS_FS(&res, "/tmp/dddd"),
            (hset_free(&se), hset_free(&res)),
            "Create len=7: only /tmp/bb should match"
        );
        test_validatefree(se.count == 4, (hset_free(&se), hset_free(&res)), "Source unchanged");
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT len=7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc", "/tmp/dddd");
        hset *res = hset_apply_fslen_int(&se, 7);
        test_validatefree(
            res == &se && se.count == 1 &&
            HSET_HAS_FS(res, "/tmp/bb") &&
            !HSET_HAS_FS(res, "/tmp/a") &&
            !HSET_HAS_FS(res, "/tmp/ccc") &&
            !HSET_HAS_FS(res, "/tmp/dddd"),
            hset_free(res),
            "Delete NOT len=7: must remain /tmp/bb"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_create_fsprefix_str / hset_apply_fsprefix_str -------------------------
static TestStatus
tf_prefix_simpliriers(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== hset_create_fsprefix_str (новое множество) ========== */
    test_sub("subtest %d: create prefix '/tmp' (empty set)", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset res = hset_create_fsprefix_str(&se, "/tmp");
        test_validatefree(
            res.count == 0 && hset_validate(stdout, &res),
            (hset_free(&se), hset_free(&res)),
            "Create prefix '/tmp' from empty: must return empty set"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create prefix '/tmp' (mixed paths)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/tmp/b", "/tmp/c", "/usr/d");
        // Ожидаем только /tmp/a и /tmp/c
        hset res = hset_create_fsprefix_str(&se, "/tmp");
        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/a") &&
            HSET_HAS_FS(&res, "/tmp/c") &&
            !HSET_HAS_FS(&res, "/var/tmp/b") &&
            !HSET_HAS_FS(&res, "/usr/d"),
            (hset_free(&se), hset_free(&res)),
            "Create prefix '/tmp': must collect only matching paths"
        );
        test_validatefree(
            se.count == 4,
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged after create"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: create prefix '/nonexistent' (no matches)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/b", "/usr/c");
        hset res = hset_create_fsprefix_str(&se, "/nonexistent");
        test_validatefree(
            res.count == 0,
            (hset_free(&se), hset_free(&res)),
            "Create prefix '/nonexistent': must return empty set"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    /* ========== hset_apply_fsprefix_str (in-place) ========== */
    test_sub("subtest %d: delete NOT prefix '/tmp' (empty set)", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset *res = hset_apply_fsprefix_str(&se, "/tmp");
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Delete NOT prefix '/tmp' from empty: must stay empty"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT prefix '/tmp' (mixed paths)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/tmp/b", "/tmp/c", "/usr/d");
        // NOT prefix '/tmp' => удалить все, которые НЕ начинаются с '/tmp'.
        // Останутся только /tmp/a и /tmp/c.
        hset *res = hset_apply_fsprefix_str(&se, "/tmp");
        test_validatefree(
            res == &se && se.count == 2 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/tmp/c") &&
            !HSET_HAS_FS(res, "/var/tmp/b") &&
            !HSET_HAS_FS(res, "/usr/d"),
            hset_free(res),
            "Delete NOT prefix '/tmp': must keep only /tmp/a and /tmp/c"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT prefix '/nonexistent' (remove all)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/b", "/usr/c");
        hset *res = hset_apply_fsprefix_str(&se, "/nonexistent");
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Delete NOT prefix '/nonexistent': no element matches, all removed"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: delete NOT prefix '/' (remove none)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/b");
        // Все абсолютные пути начинаются с '/', поэтому предикат истинен для всех,
        // hset_filter оставляет все, ничего не удаляется.
        hset *res = hset_apply_fsprefix_str(&se, "/");
        test_validatefree(
            res == &se && se.count == 2 &&
            HSET_HAS_FS(res, "/tmp/a") &&
            HSET_HAS_FS(res, "/var/b"),
            hset_free(res),
            "Delete NOT prefix '/': no element removed"
        );
        hset_free(res);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST int:int simplifiers (gt, ge, ne) -------------------------
static TestStatus
tf_int_int_simpliriers(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== greater than ========== */
    test_sub("subtest %d: create int > 10", ++subnum);
    {
        int vals[] = {5, 12, 3, 8, 15, 10};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_create_intgt_int(&se, 10);
        test_validatefree(
            res.count == 2 &&
            HSET_HAS_INT(&res, 12) && HSET_HAS_INT(&res, 15) &&
            !HSET_HAS_INT(&res, 5) && !HSET_HAS_INT(&res, 3) &&
            !HSET_HAS_INT(&res, 8) && !HSET_HAS_INT(&res, 10),
            (hset_free(&se), hset_free(&res)),
            "Create int >10 must select 12,15"
        );
        hset_free(&se);
        hset_free(&res);
    }

    test_sub("subtest %d: delete int not > 10", ++subnum);
    {
        int vals[] = {5, 12, 3, 8, 15, 10};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_apply_intgt_int(&se, 10);
        test_validatefree(
            res == &se && se.count == 2 &&
            HSET_HAS_INT(res, 12) && HSET_HAS_INT(res, 15) &&
            !HSET_HAS_INT(res, 5) && !HSET_HAS_INT(res, 3) &&
            !HSET_HAS_INT(res, 8) && !HSET_HAS_INT(res, 10),
            hset_free(res),
            "Delete int not >10 must leave 12,15"
        );
        hset_free(res);
    }

    /* ========== greater or equal ========== */
    test_sub("subtest %d: create int >= 10", ++subnum);
    {
        int vals[] = {5, 12, 3, 8, 15, 10};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_create_intge_int(&se, 10);
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_INT(&res, 10) && HSET_HAS_INT(&res, 12) && HSET_HAS_INT(&res, 15) &&
            !HSET_HAS_INT(&res, 5) && !HSET_HAS_INT(&res, 3) && !HSET_HAS_INT(&res, 8),
            (hset_free(&se), hset_free(&res)),
            "Create int >=10 must select 10,12,15"
        );
        hset_free(&se);
        hset_free(&res);
    }

    test_sub("subtest %d: delete int not >= 10", ++subnum);
    {
        int vals[] = {5, 12, 3, 8, 15, 10};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_apply_intge_int(&se, 10);
        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_INT(res, 10) && HSET_HAS_INT(res, 12) && HSET_HAS_INT(res, 15),
            hset_free(res),
            "Delete int not >=10 must leave 10,12,15"
        );
        hset_free(res);
    }

    /* ========== not equal ========== */
    test_sub("subtest %d: create int != 8", ++subnum);
    {
        int vals[] = {5, 12, 3, 8, 15};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_create_intne_int(&se, 8);
        test_validatefree(
            res.count == 4 &&
            HSET_HAS_INT(&res, 5) && HSET_HAS_INT(&res, 12) &&
            HSET_HAS_INT(&res, 3) && HSET_HAS_INT(&res, 15) &&
            !HSET_HAS_INT(&res, 8),
            (hset_free(&se), hset_free(&res)),
            "Create int !=8 must select all except 8"
        );
        hset_free(&se);
        hset_free(&res);
    }

    test_sub("subtest %d: delete int not != 8", ++subnum);
    {
        int vals[] = {5, 12, 3, 8, 15};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_apply_intne_int(&se, 8);
        test_validatefree(
            res == &se && se.count == 4 &&
            HSET_HAS_INT(res, 5) && HSET_HAS_INT(res, 12) &&
            HSET_HAS_INT(res, 3) && HSET_HAS_INT(res, 15) &&
            !HSET_HAS_INT(res, 8),
            hset_free(res),
            "Delete int not !=8 must leave all except 8"
        );
        hset_free(res);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST int between simplifier -------------------------
static TestStatus
tf_intbetween_int_int_simpliriers(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== create between (new set) ========== */
    test_sub("subtest %d: create int between 3 and 8", ++subnum);
    {
        int vals[] = {1, 5, 3, 8, 10, 6, 12, 0};
        hset se = hset_from_intarr(vals, COUNT(vals));
        // Ожидаем элементы: 3,5,6,8 (включая границы)
        hset res = hset_create_intbetween_int_int(&se, 3, 8);
        test_validatefree(
            res.count == 4 &&
            HSET_HAS_INT(&res, 3) &&
            HSET_HAS_INT(&res, 5) &&
            HSET_HAS_INT(&res, 6) &&
            HSET_HAS_INT(&res, 8) &&
            !HSET_HAS_INT(&res, 1) &&
            !HSET_HAS_INT(&res, 10) &&
            !HSET_HAS_INT(&res, 12) &&
            !HSET_HAS_INT(&res, 0),
            (hset_free(&se), hset_free(&res)),
            "Create int between 3 and 8: must select values in [3,8]"
        );
        test_validatefree(
            hset_validate(logfile, &se),
            (hset_free(&se), hset_free(&res)),
            "Validation origin is failed"
        );
        test_validatefree(
            hset_validate(logfile, &res),
            (hset_free(&se), hset_free(&res)),
            "Validation created is failed"
        );
        // Исходное множество не должно измениться
        test_validatefree(
            se.count == COUNT(vals),
            (hset_free(&se), hset_free(&res)),
            "Source set must be unchanged after create"
        );
        hset_free(&se);
        hset_free(&res);
    }

    test_sub("subtest %d: create int between reversed bounds (8 and 3)", ++subnum);
    {
        // Если v1 > v2, предполагаем, что предикат возвращает false для всех элементов
        int vals[] = {1, 5, 3, 8, 10};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_create_intbetween_int_int(&se, 8, 3);
        test_validatefree(
            res.count == 0,
            (hset_free(&se), hset_free(&res)),
            "Create int between 8 and 3 (reversed): must return empty set"
        );
        hset_free(&se);
        hset_free(&res);
    }

    test_sub("subtest %d: create int between with no matching elements", ++subnum);
    {
        int vals[] = {1, 2, 10, 11};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_create_intbetween_int_int(&se, 5, 7);
        test_validatefree(
            res.count == 0,
            (hset_free(&se), hset_free(&res)),
            "Create int between 5 and 7: must be empty"
        );
        hset_free(&se);
        hset_free(&res);
    }

    test_sub("subtest %d: create int between where all elements match", ++subnum);
    {
        int vals[] = {4, 5, 6};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset res = hset_create_intbetween_int_int(&se, 4, 6);
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_INT(&res, 4) &&
            HSET_HAS_INT(&res, 5) &&
            HSET_HAS_INT(&res, 6),
            (hset_free(&se), hset_free(&res)),
            "Create int between 4 and 6: all elements must be selected"
        );
        hset_free(&se);
        hset_free(&res);
    }

    /* ========== apply between (in-place) ========== */
    test_sub("subtest %d: apply int between 3 and 8", ++subnum);
    {
        int vals[] = {1, 5, 3, 8, 10, 6, 12, 0};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_apply_intbetween_int_int(&se, 3, 8);
        // Ожидаем, что останутся 3,5,6,8
        test_validatefree(
            res == &se && se.count == 4 &&
            HSET_HAS_INT(res, 3) &&
            HSET_HAS_INT(res, 5) &&
            HSET_HAS_INT(res, 6) &&
            HSET_HAS_INT(res, 8) &&
            !HSET_HAS_INT(res, 1) &&
            !HSET_HAS_INT(res, 10) &&
            !HSET_HAS_INT(res, 12) &&
            !HSET_HAS_INT(res, 0),
            hset_free(res),
            "Apply int between 3 and 8: must keep values in [3,8]"
        );
        hset_free(res);
    }

    test_sub("subtest %d: apply int between reversed bounds (keep none)", ++subnum);
    {
        int vals[] = {1, 5, 3, 8, 10};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_apply_intbetween_int_int(&se, 8, 3);
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Apply int between 8 and 3: must result in empty set"
        );
        hset_free(res);
    }

    test_sub("subtest %d: apply int between with no matching elements", ++subnum);
    {
        int vals[] = {1, 2, 10, 11};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_apply_intbetween_int_int(&se, 5, 7);
        test_validatefree(
            res == &se && se.count == 0,
            hset_free(res),
            "Apply int between 5 and 7: no elements match, set becomes empty"
        );
        hset_free(res);
    }

    test_sub("subtest %d: apply int between where all elements match", ++subnum);
    {
        int vals[] = {4, 5, 6};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset *res = hset_apply_intbetween_int_int(&se, 4, 6);
        test_validatefree(
            res == &se && se.count == 3 &&
            HSET_HAS_INT(res, 4) &&
            HSET_HAS_INT(res, 5) &&
            HSET_HAS_INT(res, 6),
            hset_free(res),
            "Apply int between 4 and 6: all elements must remain"
        );
        hset_free(res);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_init_map (int add, fs append) -------------------------
static TestStatus
tf_init_map_int_add_fs_append(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== map int add ========== */
    test_sub("subtest %d: map int add 10", ++subnum);
    {
        int vals[] = {1, 2, 3};
        hset se = hset_from_intarr(vals, COUNT(vals));

        // Параметры: один int-параметр = 10
        value64_params_t params = VALUE64_PARAMS1(LITERAL64_INT(10));
        hset res = hset_init_map(&se, test_map_int_add_int, &params);

        // Ожидаем 11, 12, 13
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_INT(&res, 11) &&
            HSET_HAS_INT(&res, 12) &&
            HSET_HAS_INT(&res, 13),
            (hset_free(&se), hset_free(&res)),
            "Map int add 10: must be 11,12,13"
        );
        test_validatefree(se.count == 3, (hset_free(&se), hset_free(&res)), "Source unchanged");
        hset_free(&se);
        hset_free(&res);
    }

    test_sub("subtest %d: map int add 0 (no change)", ++subnum);
    {
        int vals[] = {5, -3, 0};
        hset se = hset_from_intarr(vals, COUNT(vals));
        value64_params_t params = VALUE64_PARAMS1(LITERAL64_INT(0));
        hset res = hset_init_map(&se, test_map_int_add_int, &params);
        test_validatefree(
            res.count == 3 &&
            HSET_HAS_INT(&res, 5) &&
            HSET_HAS_INT(&res, -3) &&
            HSET_HAS_INT(&res, 0),
            (hset_free(&se), hset_free(&res)),
            "Map int add 0: must be unchanged"
        );
        hset_free(&se);
        hset_free(&res);
    }

    /* ========== map fs append suffix ========== */
    test_sub("subtest %d: map fs append '.bak'", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        value64_params_t params = VALUE64_PARAMS1(LITERAL64_STR(".bak"));
        hset res = hset_init_map(&se, test_map_fs_append_str, &params);

        //HSET_TECH_PRINTALL(res);
        //HSET_TECH_PRINTALL(se);

        test_validatefree(
            res.count == 3 &&
            HSET_HAS_FS(&res, "/tmp/a.bak") &&
            HSET_HAS_FS(&res, "/tmp/b.bak") &&
            HSET_HAS_FS(&res, "/tmp/c.bak"),
            (hset_free(&se), hset_free(&res)),
            "Map fs append '.bak': must add suffix"
        );
        // Проверим, что исходное множество не изменилось
        test_validatefree(
            se.count == 3 &&
            HSET_HAS_FS(&se, "/tmp/a") &&
            HSET_HAS_FS(&se, "/tmp/b") &&
            HSET_HAS_FS(&se, "/tmp/c"),
            (hset_free(&se), hset_free(&res)),
            "Source must be intact"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: map fs append empty suffix (no change)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        value64_params_t params = VALUE64_PARAMS1(LITERAL64_STR(""));  // пустая строка
        hset res = hset_init_map(&se, test_map_fs_append_str, &params);
        test_validatefree(
            res.count == 2 &&
            HSET_HAS_FS(&res, "/tmp/x") &&
            HSET_HAS_FS(&res, "/tmp/y"),
            (hset_free(&se), hset_free(&res)),
            "Append empty suffix: paths unchanged"
        );
        hset_free(&se);
        hset_free(&res);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_reduce_filtered FS -------------------------
static TestStatus
tf_reduce_filtered_fs(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    // TODO:
    test_sub("subtest %d: filtered sumlen with prefix /tmp", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/var/y", "/tmp/z", "/usr/w");
        value64 prefix = LITERAL64_STR("/tmp");
        hset_accum acc = hset_reduce_filtered(&se, HSET_ACCUM_LNG_ZERO, hset_sumlen_fs,
                                              value64_filter_fsprefix_str, prefix);
        test_validatefree(
            hset_accum_long(&acc) == 12 && acc.count == 2,
            hset_free(&se),
            "Filtered sumlen /tmp: expected 12, got %ld (count %d)",
            hset_accum_long(&acc), acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filtered sumlen no match (empty result)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/var/y", "/usr/w");
        value64 prefix = LITERAL64_STR("/tmp");
        hset_accum acc = hset_reduce_filtered(&se, HSET_ACCUM_LNG_ZERO, hset_sumlen_fs,
                                              value64_filter_fsprefix_str, prefix);
        test_validatefree(
            hset_accum_long(&acc) == 0 && acc.count == 0,
            hset_free(&se),
            "Filtered sumlen no match: expected 0, got %ld (count %d)",
            hset_accum_long(&acc), acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filtered sumlen with NULL pred (all elements)", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/var/y", "/tmp/z", "/usr/w");
        // Все эти пути имеют длину 6 символов -> сумма 24
        hset_accum acc = hset_reduce_filtered(&se, HSET_ACCUM_LNG_ZERO, hset_sumlen_fs,
                                              NULL, LITERAL64_ZERO);
        test_validatefree(
            hset_accum_long(&acc) == 24 && acc.count == 4,
            hset_free(&se),
            "Filtered sumlen NULL pred: expected 24, got %ld (count %d)",
            hset_accum_long(&acc), acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filtered sumlen on empty set", ++subnum);
    {
        hset se = hset_init_fs(10);
        value64 prefix = LITERAL64_STR("/tmp");
        hset_accum acc = hset_reduce_filtered(&se, HSET_ACCUM_LNG_ZERO, hset_sumlen_fs,
                                              value64_filter_fsprefix_str, prefix);
        test_validatefree(
            hset_accum_int(&acc) == 0 && acc.count == 0,
            hset_free(&se),
            "Filtered sumlen on empty set: expected 0, got %ld (count %d)",
            hset_accum_long(&acc), acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST hset_filtereduce_* (all types) -------------------------
static TestStatus
tf_filtereduce_all_types(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* ========== INT ========== */
    test_sub("subtest %d: filtereduce_int sum of values > 5", ++subnum);
    {
        int vals[] = {1, 8, 3, 10, 6, 4};
        hset se = hset_from_intarr(vals, COUNT(vals));
        // Предикат: больше 5 -> 8,10,6; сумма = 24
        value64 threshold = LITERAL64_INT(5);
        hset_accum acc = hset_filtereduce_int(&se, hset_sum_int,
                                              value64_filter_intgt_int, threshold);
        test_validatefree(
            hset_accum_int(&acc) == 24 && acc.count == 3,
            hset_free(&se),
            "Filtered int >5 sum: expected 24, got %d (count %d)",
            hset_accum_int(&acc), acc.count
        );
        hset_free(&se);
    }

    test_sub("subtest %d: filtereduce_int with NULL pred (all)", ++subnum);
    {
        int vals[] = {1, 2, 3};
        hset se = hset_from_intarr(vals, COUNT(vals));
        hset_accum acc = hset_filtereduce_int(&se, hset_sum_int,
                                              NULL, LITERAL64_ZERO);
        test_validatefree(
            hset_accum_int(&acc) == 6 && acc.count == 3,
            hset_free(&se),
            "Filtered int NULL pred sum: expected 6, got %d", hset_accum_int(&acc)
        );
        hset_free(&se);
    }

    /* ========== LNG ========== */
    test_sub("subtest %d: filtereduce_lng sum of values >= 100", ++subnum);
    {
        long vals[] = {50, 150, 200, 75};
        hset se = hset_from_longarr(vals, COUNT(vals));
        value64 threshold = LITERAL64_LNG(100);
        hset_accum acc = hset_filtereduce_lng(&se, hset_sum_lng,
                                              value64_filter_lngge_lng, threshold);
        test_validatefree(
            hset_accum_long(&acc) == 350 && acc.count == 2,
            hset_free(&se),
            "Filtered lng >=100 sum: expected 350, got %ld (count %d)",
            hset_accum_long(&acc), acc.count
        );
        hset_free(&se);
    }

    /* ========== DBL ========== */
    test_sub("subtest %d: filtereduce_dbl max of values < 3.0", ++subnum);
    {
        double vals[] = {1.5, 2.7, 3.1, 0.9, 4.2};
        hset se = hset_from_dblarr(vals, COUNT(vals));
        value64 threshold = LITERAL64_DBL(3.0);
        hset_accum acc = hset_filtereduce_dbl(&se, hset_max_dbl,
                                              value64_filter_dbllt_dbl, threshold);
        test_validatefree(
            hset_accum_dbl(&acc) == 2.7 && acc.count > 0,   // count == 1
            hset_free(&se),
            "Filtered dbl <3.0 max: expected 2.7, got %f (count %d)",
            hset_accum_dbl(&acc), acc.count
        );
        hset_free(&se);
    }

    /* ========== FS ========== */
    test_sub("subtest %d: filtereduce_fs sumlen of paths with prefix /tmp", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/var/y", "/tmp/z", "/usr/w");
        value64 prefix = LITERAL64_STR("/tmp");
        hset_accum acc = hset_filtereduce_lng(&se, hset_sumlen_fs,
                                             value64_filter_fsprefix_str, prefix);
        // /tmp/x длина 6, /tmp/z длина 6 => 12
        test_validatefree(
            hset_accum_long(&acc) == 12 && acc.count == 2,
            hset_free(&se),
            "Filtered FS sumlen /tmp: expected 12, got %ld (count %d)",
            hset_accum_long(&acc), acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filtereduce_fs count of paths with minlen >= 7", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/bb", "/tmp/ccc");
        value64 minlen = LITERAL64_INT(7);
        hset_accum acc = hset_filtereduce_fs(&se, hset_count_fs,
                                             value64_filter_fsminlen_int, minlen);
        test_validatefree(
            acc.count == 2,  // /tmp/bb (7), /tmp/ccc (8)
            hset_free(&se),
            "Filtered FS minlen>=7 count: expected 2, got %d", acc.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    /* ========== FSAGG ========== */
    test_sub("subtest %d: filtereduce_fsagg concatenate with prefix /tmp", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/var/b", "/tmp/c", "/usr/d");
        value64 prefix = LITERAL64_STR("/tmp");
        hset_accum acc = hset_filtereduce_fsagg(&se, hset_agg_fs, ", ",
                                                value64_filter_fsprefix_str, prefix);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            res != NULL &&
            strstr(res, "/tmp/a") && strstr(res, "/tmp/c") &&
            !strstr(res, "/var") && !strstr(res, "/usr"),
            (hset_free(&se), hset_accum_free(&acc)),
            "Filtered FS agg: expected only /tmp paths, got '%s'", res ? res : "NULL"
        );
        hset_accum_free(&acc);
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: filtereduce_fsagg empty result", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/var/y", "/usr/w");
        value64 prefix = LITERAL64_STR("/tmp");
        hset_accum acc = hset_filtereduce_fsagg(&se, hset_agg_fs, ", ",
                                                value64_filter_fsprefix_str, prefix);
        const char *res = fs_str(hset_accum_fs(&acc));
        test_validatefree(
            res == NULL || strlen(res) == 0,
            (hset_free(&se), hset_accum_free(&acc)),
            "Filtered FS agg empty: expected empty string, got '%s'", res ? res : "NULL"
        );
        hset_accum_free(&acc);
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------------------------------------------------------------------------------------------------------------
int
main(int argc, const char *argv[])
{
    logsimpleinit("Start");
    bool    runall = argc == 1;

    while (runall || *++argv){
        int     num = INT_MAX;    // INT_MAX for all test
        if (!runall){
            num = atoi(*argv);
            if (num < 0){
                fprintf(stderr,"Invalid test num %d\n", num);
                continue;
            }
        }
            testenginestd_run(num,
                testnew(.f2 =  tf8,                                 .num =  9, .name = "Hset_in simple test"                        , .desc="", .mandatory=true)
              , testnew(.f2 =  tf9,                                 .num = 10, .name = "Hset_strictin simple test"                  , .desc="", .mandatory=true)
              , testnew(.f2 = tf10,                                 .num = 11, .name = "Hset_minus simple test"                     , .desc="", .mandatory=true)
              , testnew(.f2 = tf11,                                 .num = 12, .name = "Hset_init_minus simple test"                , .desc="", .mandatory=true)
              , testnew(.f2 = tf12,                                 .num = 13, .name = "hset_intersect simple test"                 , .desc="", .mandatory=true)
              , testnew(.f2 = tf13,                                 .num = 14, .name = "hset_init_intersect simple test"            , .desc="", .mandatory=true)
              , testnew(.f2 = tf14,                                 .num = 15, .name = "hset_init_symmdiff simple test"             , .desc="", .mandatory=true)
              , testnew(.f2 = tf15,                                 .num = 16, .name = "hset_symmdiff simple test"                  , .desc="", .mandatory=true)
              , testnew(.f2 = tf17,                                 .num = 18, .name = "hset_union simple test"                     , .desc="", .mandatory=true)
              , testnew(.f2 = tf18,                                 .num = 19, .name = "hset_init_union simple test"                , .desc="", .mandatory=true)
              , testnew(.f2 = tf21,                                 .num = 22, .name = "hset_const_foreach simple test"             , .desc="", .mandatory=true)
              , testnew(.f2 = tf22,                                 .num = 23, .name = "hset_initreduce int impl  simple test"      , .desc="", .mandatory=true)
              , testnew(.f2 = tf23,                                 .num = 24, .name = "hset_initreduce double int simple test"     , .desc="", .mandatory=true)
              , testnew(.f2 = tf26,                                 .num = 27, .name = "hset_any(), hset_nonexists() simple test"   , .desc="", .mandatory=true)
              , testnew(.f2 = tf_reduce_fs_count_max_min,           .num = 28, .name = "hset_initreduce fs simple test"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_reduce_fsagg,                      .num = 29, .name = "hset_reduce_fsagg simple test"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_filter_fs,                         .num = 30, .name = "hset(_init)_filter simple test"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_filter_fsulike_str,                .num = 31, .name = "hset_filter_fs(u)like_str simple test"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_like_ulike_simpliriers,            .num = 32, .name = "hset_apply_fs(u)like_str simplifiers"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_minlen_simpliriers,                .num = 33, .name = "hset_create/delete_fsminlen_int simplifiers"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_maxlen_simpliriers,                .num = 34, .name = "hset_create/delete_fs(max)len_int simplifiers"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_prefix_simpliriers,                .num = 35, .name = "hset_create/delete_fsprefix_str simplifiers"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_int_int_simpliriers,               .num = 36, .name = "hset_create/delete_int<ACT>_int simplifiers"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_intbetween_int_int_simpliriers,    .num = 37, .name = "hset_create/apply_intbetween_int_int simplifiers"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_init_map_int_add_fs_append,        .num = 38, .name = "hset_init_map simple test"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_reduce_filtered_fs,                .num = 39, .name = "hset_reduce_filtered() simple test"
                                , .desc="", .mandatory=true)
              , testnew(.f2 = tf_filtereduce_all_types,             .num = 40, .name = "hset_filtereduce_* (all types) simple test"
                                , .desc="", .mandatory=true)
            );
        if (runall)
            break;
    }

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* HASHSET_UTILS_TESTING */

