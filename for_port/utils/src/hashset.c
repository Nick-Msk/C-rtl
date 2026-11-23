/********************************************************************
                    HASH-BASED SET MODULE IMPLEMENTATION
********************************************************************/

#include "hashset.h"

// TODO: move that to context
static int                              HSET_ARRAY_CREATE_MULTIPLIER = 2;
static int                              HSET_MIN_SIZE                = 8;
static double                           HSET_NORMALIZE_FACTOR        = 1.5;

// ---------- pseudo-header for utility procedures -----------------

// ---------------------------- (Utility) printers -----------------------------
// generic
static int                 sortedlist_fprint(FILE *restrict out, const hset_elem *restrict elem, value64_type typ){
    invraisecode(ERR_NULLABLE_PTR, out != NULL, "Null pointer");
    int     cnt = 0;
    while (elem){
        cnt += value64_fprint(out, elem->v, typ);
        cnt += fprintf(out, " -> ");
        elem = elem->next;
    }
    return cnt;
}

// ------------------------------------ Utilities ------------------------------------------
// generic
static unsigned long       get_lhash(unsigned cnt, value64 value, value64_type typ){
    // probably it's better to calc hash by u64 attr (except fs for sure)
    unsigned long hash = value64_lhash(value, typ);
    return hash % cnt;
}

// seach the value in hset!
static hset_elem           *getprevelem(const hset *restrict se, value64 value,
        unsigned *restrict phash, hset_elem **restrict pnext, hset_elem **restrict pequal){

    unsigned hash = get_lhash(se->sz, value, hset_getype(se) );
    hset_elem *el = se->table[hash],
              *prevel = 0;
    while (el != 0 && value64_compare(value, el->v, hset_getype(se) ) < 0){
        //logmsg("el %p, prevel %p", el, prevel);
        prevel = el;
        el = el->next;
    }
    if (phash)
        *phash = hash;
    if (pnext)
        *pnext = el;
    if (pequal){
        if (el && value64_compare(el->v, value, hset_getype(se) ) == 0) //  found EXACLTY!
            *pequal = el;
        else
            *pequal = 0;        // NOT found exactly!
    }
    return prevel; //logsimpleret(prevel, "Found prev %p, next %p", prevel, el);   // < or = in the list
}

static hset_elem           *alloc_elem(void){
    return malloc(sizeof(hset_elem) );
}

static hset_elem           *create_elem(value64 val, value64_type typ){
    hset_elem *res = alloc_elem();
    if (!res)
        return logsimpleret( (hset_elem *) 0, "Unable to create elem");

    if (typ == VALUE64_FS) {    // TODO: not sure about  that solution
        if (val.fsval && !fs_bodyalloc(val.fsval))
            val.fsval = fs_heapcreate(val.fsval);
    }
    res->v = val;
    res->next = 0;

    return res;
}

static bool                 create_or_move_elem(hset * restrict se, hset_elem *restrict el, value64 val){
    if (el == NULL && hset_getype(se) == VALUE64_FS && (val.fsval == NULL || fs_str(val.fsval) == NULL) )
        return logsimpleret(false, "Unable to add Null FS value");

    bool         already_existed = false;
    unsigned     hash = 0;
    hset_elem   *equal = 0, *nextel = 0;
    value64      value = el ? el->v : val;

    hset_elem   *prevel = getprevelem(se, value, &hash, &nextel, &equal);
    if (equal) {
        already_existed = true;
        if (!el)
            value64_free(&val, hset_getype(se));  // освобождает память для FS/STR
    }
    else {
        hset_elem *newel;
        if (el)     // move
            newel = el;
        else {          // create a new one
            newel = create_elem(val, hset_getype(se) );
            if (!newel)
                userraiseint(ERR_UNABLE_ALLOCATE, "Can't create new element");
        }
        if (prevel)
            prevel->next = newel;
        else
            se->table[hash] = newel;
        newel->next = nextel;
        se->count++;
    }
    return logsimpleret(!already_existed, "Added the value prev existing %s", bool_str(already_existed) );
}

static hset_elem           *clone_elemlist(const hset_elem *el, value64_type typ){

    hset_elem  *prev = 0, *newel = 0, *retel = 0;
    while (el){
        if (!(newel = create_elem(el->v, typ) ) )
            return userraise((hset_elem *) 0, ERR_UNABLE_ALLOCATE, "Unable to create element");
        switch (typ){
            case VALUE64_FS:
                newel->v.fsval = fs_heapcreate(el->v.fsval);    // TODO: not sure
            break;
            /* case HSET_STR:
                newel->v.str = strdup(el->v.str);
            break;*/
            default:
                newel->v = el->v;
            break;
        }
        if (!retel)
            retel = newel;  //  first elem
        if (prev)
            prev->next = newel;
        // next iter
        prev = newel;
        el = el->next;
    }
    return logsimpleret(retel, "Chain is created %p", retel);
}

static inline int           count_elemlist(const hset_elem *el){
    int     cnt = 0;
    while (el){
        cnt++;
        el = el->next;
    }
    return cnt;
}
static int                   hset_calc_cnt(const hset *se){
    invraise(se != 0, "Null pointer");

    int     cnt = 0;
    for (int i = 0; i < se->sz; i++)
        if (se->table[i]){  // actually this check is excess
            cnt += count_elemlist(se->table[i] );
        }
    return logsimpleret(cnt, "Total %d", cnt);
}

static void                 free_elem(hset_elem *el, value64_type typ){
    switch (typ){
        case VALUE64_FS:
            fs_free(el->v.fsval);        // to free space, NO need to free(el->v.fsval)! That will do fs_free() because of FS_FLAG_MOVED
        break;
        default:
            // nothing here VALUE64_STR isn't supported now
        break;
    }
    free(el);
}
// return count of deleted elems
static int                  free_elemlist(hset_elem *el, value64_type typ){
    hset_elem       *next = 0;
    int              cnt = 0;
    while (el){
        next = el->next;
        free_elem(el, typ);
        el = next;
        cnt++;
    }
    return cnt;
}

static bool                 validate_elemlist(const hset_elem *el, value64_type typ, unsigned pos, unsigned sz){
    invraisecode(ERR_NULLABLE_PTR, el != NULL, "Null pointer");
    const hset_elem *prevel = 0;
    int              iternum = 0;
    while (el){
        unsigned hash = get_lhash(sz, el->v, typ);
        if (hash != pos)
            return logsimpleerr(false, "iter %d: %u, but must be %u", iternum, hash, pos);
        if (prevel)
            if (value64_compare(el->v, prevel->v, typ) < 0)
                return logsimpleret(false, "iter %d: ordering violation", iternum);  // TODO: valprev %s valnext %s must be here via fs
        el = el->next;
    }
    return logsimpleerr(true, "%u Ok", pos);
}

static value64           hset_createarrval(void *arr, int idx, value64_type typ) {
    invraisecode(ERR_NULLABLE_PTR, arr != NULL, "Null pointer");
    size_t elem_size;
    if (typ != VALUE64_FS)
        elem_size = value64_info_get(typ)->size; //hset_elem_sizes[typ];
    else
        elem_size = sizeof(fs); // befause for fs it's fs[], so size of element is sizeof(fs), but not sizeof(fs *)

    if (elem_size == 0)
        userraiseint(ERR_UNSUPPORTED_TYPE, "type %d", typ);

    char *base = (char *) arr;
    // caculate a real offset
    return value64_pmove(base + idx * elem_size, typ);     // hset_createval(base + idx * elem_size, typ);
}


// find every value in the list in se
static bool                 find_elems(const hset_elem *restrict el, const hset *restrict se){
    while (el){
        if (!hset_get(se, el->v) )
            return false;
        el = el->next;
    }
    return true;
}

// ------------------------------------- API -----------------------------------------------

// ---------------------------------- API Constructs/Destrucor  ----------------------------

// hset basic constructor
hset                        hset_init(int sz, value64_type typ){
    logenter("init sz %d - %s", sz, value64_typename(typ) );

    if (int_notin(typ, VALUE64_INT, VALUE64_LONG, VALUE64_DBL, VALUE64_FS, VALUE64_PTR) )
        userraiseint(ERR_UNSUPPORTED_TYPE, "%d", typ);

    if (sz <= 0)
        sz = 1;
    unsigned    newsz = next_prime(sz);
    hset res = HSET(newsz, typ);
    if ( (res.table = malloc(newsz * sizeof(hset_elem *) ) ) == 0)    // raise here - nothing to do
        userraiseint(ERR_UNABLE_ALLOCATE, "Unable to alloc newsz %u elems", newsz);
    clean_ptr( (void *) res.table,  newsz);

    return logret(res, "Created with %d", res.sz);
}
// construct via new hash table size
hset             hset_init_resize(hset *se, int newsz){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer");

    if (next_prime(newsz) == (unsigned) se->sz)
        return logsimpleret(*se, "No change");
    hset    res = hset_init(newsz, hset_getype(se) );
    // hset_foreach(se)
    for (int i = 0; i < se->sz; i++){
        hset_elem *el = se->table[i];
        while (el){
            hset_elem *next = el->next;
            hset_elem_move(&res, el); // el->next will be modified!
            el = next;  // go to next elem of ordered list
        }
    }
    // clean the table manually
    free(se->table);
    //hset_free(se);  // NO NEED cleanup
    *se = res;  // and fill with newly created
    return logsimpleret(res, "Resized to %d", res.sz);
}
// normalization
hset                        hset_normalize(hset *se){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer");

    double load_factor = (double)se->count / se->sz;
    // Изменяем размер только при выходе из желаемого диапазона
    if (load_factor > 0.75 || load_factor < 0.25) {
        int     newsz = (int) (se->count * HSET_NORMALIZE_FACTOR);   // или любой другой коэффициент
        if (newsz < HSET_MIN_SIZE)
            newsz = HSET_MIN_SIZE;                   // минимальный размер
        return hset_init_resize(se, newsz);
    }
    return logsimpleret(*se, "Normilized to %d / %d", se->sz, se->count);
}
// cloning
hset                        hset_clone(const hset *se){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer");

    int     newsz = se->sz;
    hset    res = hset_init(newsz - 1, hset_getype(se) );

    if ( (res.table = malloc(newsz * sizeof(hset_elem *) ) ) == 0)    // raise here - nothing to do
        userraiseint(ERR_UNABLE_ALLOCATE, "Unable to alloc newsz %u elems", newsz);

    for (int i = 0; i < res.sz; i++){
        res.table[i] = clone_elemlist(se->table[i], hset_getype(se) );
    }
    res.count = se->count;

    return logsimpleret(res, "Cloned");
}
// created with new type only (INT, LONG, DOUBLE, FS) as allowed
hset                        hset_cloneas(const hset *se, value64_type typ){
    invraise(se != 0, "Null pointer");

    if (int_notin(typ, VALUE64_INT, VALUE64_LONG, VALUE64_DBL, VALUE64_FS) 
            && int_notin(hset_getype(se), VALUE64_INT, VALUE64_LONG, VALUE64_DBL, VALUE64_FS))
        userraiseint(ERR_UNSUPPORTED_TYPE, "From %d:%s - to %d:%s",
            hset_getype(se), value64_typename(hset_getype(se) ), typ, value64_typename(typ) );

    hset    res = hset_init(se->sz - 1, typ);
    for (int i = 0; i < se->sz; i++){
        const hset_elem *el = se->table[i];     // probably better to create separate function
        while (el){
            /*if (!hset_set(&res, value64_convert(el->v, hset_getype(se), typ) ) ) {
                hset_free(&res);
                userraiseint(ERR_UNABLE_ALLOCATE, "Unable to create element");
            } */
            value64 tmp = value64_convert(el->v, hset_getype(se), typ);
            hset_move(&res, &tmp );
            el = el->next;
        }
    }
    return logsimpleret(res, "Cloned as %d:%s", typ, value64_typename(typ) );
}

hset                        hset_from_anyarr(const void *arr, int sz, value64_type typ){
    invraisecode(ERR_NULLABLE_PTR, arr != 0 && sz > 0 && sz < INT_MAX / 4, 
            "Incorrent input %p - %d", arr, sz);
    if (int_notin(typ, VALUE64_INT, VALUE64_LONG, VALUE64_DBL, VALUE64_PTR, VALUE64_FS) )
        userraiseint(ERR_UNSUPPORTED_TYPE, "%d", typ);

    if (sz <= 0)
        sz = 1;     // to avoid 0 initializing
    hset        res = hset_init(sz * HSET_ARRAY_CREATE_MULTIPLIER, typ);
    int         cnt = hset_loadanyarr(&res, (void *) arr, sz, typ);     // (void *) because of fs
    return logsimpleret(res, "Created hest with sz %u, loaded %d", res.sz, cnt);
}

void                        hset_free(hset *se){
    invraise(se != 0, "Null pointer");

    hset_clean(se);
    free(se->table);
    se->sz = se->flags = 0;
    if (hset_heap_alloc(se) ){
        free(se);
        logsimple("Heap freed");
    }
    logsimple("freed");
}

// ---------------------------------------------------------------------------
bool                        hset_validate(FILE *out, const hset *restrict se){
    logenter("...");
    if (!se){    // TODO: think about string creation via faststring here!!
        if (out)
            fprintf(out, "null pointer");
        return logerr(false, "null pointer");
    }
    if (se->sz <= 1){
        if (out)
            fprintf(out, "Incorrect sz (%d)", se->sz);
        return logerr(false, "Incorrect sz (%d)", se->sz);
    }
    // check count agaist hset_cnt()
    int cnt = hset_calc_cnt(se);
    if (se->count != cnt){
        if (out)
            fprintf(out, "Cnt %d not matched to calculated cnt %d", se->count, cnt);
        return logerr(false, "Cnt %d not matched to calculated cnt %d", se->count, cnt);
    }
    if (int_notin(hset_getype(se), VALUE64_INT, VALUE64_LONG, VALUE64_DBL, VALUE64_FS, VALUE64_PTR) ){
        if (out)
            fprintf(out, "Incorrect type %d", hset_getype(se) );
        return logerr(false, "Incorrect type %d", hset_getype(se) );
    }
    if (!se->table){
        if (out)
            fprintf(out, "null hashtable");
        return logerr(false, "null hashtable");
    }
    // cross validation value <=> hashkey
    for (int i = 0; i < se->sz; i++)
        if (se->table[i] )
            if (!validate_elemlist(se->table[i], hset_getype(se), i, se->sz) ){
                if (out)
                    fprintf(out, "Hash Validation failed on %d", i);
                return logerr(false, "Hash Validation failed on %d", i);
            }

    return logret(true, "Validated");
}

// ---------------------------------- General functions ------------------------------------

// check the equality as SET!
bool                        hset_eq(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);

    if (hset_getype(se1) != hset_getype(se2) )
        return userraise(false, ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1)), value64_typename(hset_getype(se2) ) );

    int     cnt1 = hset_cnt(se1);
    int     cnt2 = hset_cnt(se2);
    if (cnt1 != cnt2 )
        return logsimpleret(false, "Count's not equal %d vs %d", cnt1, cnt2);
    // chech the elements
    bool        res = true;
    for (int i = 0; i < se1->sz; i++)
        if (!find_elems(se1->table[i], se2) ){
            res = false;
            break;
        }

    return logsimpleret(res, "Equal %s", bool_str(res) );
}

// check NOT equality as SET
bool                        hset_noteq(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);

    if (hset_getype(se1) != hset_getype(se2) )
        return userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1)), value64_typename(hset_getype(se2)));

    int     cnt1 = hset_cnt(se1);
    int     cnt2 = hset_cnt(se2);
    if (cnt1 != cnt2 )
        return logsimpleret(true, "Not equal counts %d vs %d", cnt1, cnt2);
    bool    res = false;
    for (int i = 0; i < se1->sz; i++)
        if (!find_elems(se1->table[i], se2) ){
            res = true;
            break;  // found at least 1 elem which not match to se2
        }
    return logsimpleret(res,  "Equal %s", bool_str(!res) );
}


//  move and fill with ZERO 0
bool                        hset_move(hset *restrict se, value64 *restrict pval){
    invraisecode(ERR_NULLABLE_PTR, se != NULL && pval != NULL, "Null pointers %p %p", se, pval);

    bool added = create_or_move_elem(se, NULL, *pval);
    *pval = LITERAL64_ZERO;
    return added;; 
}
// move el from one hset to target se
bool                        hset_elem_move(hset * restrict se, hset_elem *restrict el){
    invraise(se != 0 && el != 0, "Null pointers");
    // No checking type here
    return create_or_move_elem(se, el, LITERAL64_ZERO);
}

bool                        hset_get(const hset *se, value64 val){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer");

    hset_elem   *equal = 0;
    getprevelem(se, val, 0, 0, &equal);
    return equal;
}
// try to delete elemenet, true if deleted, false if not found
bool                        hset_del(hset *se, value64 val){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer");

    unsigned            hash;
    hset_elem          *el = 0;
    hset_elem          *prevel = getprevelem(se, val, &hash, 0, &el);
    if (!el)    // not found
        return logsimpleerr(false, "Not found");
    // umount el
    if (!prevel)
         se->table[hash] = el->next;
    else
        prevel->next = el->next;
    free_elem(el, hset_getype(se) );
    se->count--;
    //hsetval_log(val, hset_getype(se) );
    return logsimpleret(true, "Deleted");
}

void                        hset_clean(hset *se){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer");

    for (int i = 0; i < se->sz; i++)
        if (se->table[i] ){
            se->count -= free_elemlist(se->table[i], hset_getype(se) );
            se->table[i] = 0;   // clean that chain
        }
}
// for array mass loading
int                         hset_loadanyarr(hset *restrict se, void *arr, int sz, value64_type typ){ 
    invraisecode(ERR_NULLABLE_PTR, arr != 0 && sz > 0 && sz < INT_MAX / 4, "Incorrent input %p - %d", arr, sz);
    if (int_notin(typ, VALUE64_INT, VALUE64_LONG, VALUE64_DBL, VALUE64_PTR, VALUE64_FS) )
        userraiseint(ERR_UNSUPPORTED_TYPE, "%d - %s", typ, value64_typename(typ) );

    int         cnt = 0;
    while (cnt < sz){
        // created a simple element from arr
        value64 val = hset_createarrval(arr, cnt, typ);
        hset_move(se, &val); // ok even return false (dublicate)
        cnt++;
    }
    return logsimpleret(cnt, "Loaded total %d", cnt);
}
// fs loading, heap allocated => MOVE semantic!
int                         hset_loadfs_str(hset *restrict se, const char *strings[]){
    invraisecode(ERR_NULLABLE_PTR, strings != NULL && se != NULL, 
            "Null pointers %p %p", se, strings);
    int     cnt = 0;
    while (*strings) {
        // convent c-str into fs * (heap allocated body)
        value64  val = value64_createfs_asstr(*strings);
        hset_move(se, &val);
        strings++;
        cnt++;
    }
    return logsimpleret(cnt, "Loaded(moved) c-str total %d", cnt);
}
/*
// only static literals!
int                         hset_loadfs_literal(hset *restrict se, const char *lits[]){
    int     cnt = 0;
    while (*lits) {
        fs l = FSLITERAL( (char *) *lits); // no allocatopn here
        value64 val = ???? somehow create fs_create() and link lits with static alloc
        hset_set(se, val);
        lits++;
        cnt++;
    }
    return logsimpleret(cnt, "Loaded literal total %d", cnt);
} */

// ------------------------------------- (API) printers ------------------------------------

int                         hset_techfprint(FILE *restrict out, const hset *se, int sz, const char *restrict name){
    invraisecode(ERR_NULLABLE_PTR, se != 0, "Null pointer %p", se);

    int     cnt = 0;
    if (out){
        sz = sz ? MIN(sz, se->sz) : se->sz;
        logauto(sz);
        cnt += fprintf(out, "HSET %s(sz %d, flags %d, typez %s)[\n", name ? name : "", se->sz, se->flags, value64_typename(hset_getype(se) ) );
        for (int i = 0; i < sz; i++)
            if (se->table[i]){
                cnt += fprintf(out, "%4d: ", i);
                cnt += sortedlist_fprint(out, se->table[i], hset_getype(se) );
                cnt += fprintf(out, "\n");
            }
        cnt += fprintf(out, "]\n");
    }
    return cnt;
}

// --------------------------------- SERIALIZATION -----------------------------------------

int                         hset_fsave(FILE  *restrict out, const hset *se) {
    invraisecode(se != NULL, ERR_NULLABLE_PTR,
                "Null pointer");

    value64_type   typ = hset_getype(se);

    invraisecode ( int_in(typ, VALUE64_INT, VALUE64_LONG, VALUE64_DBL, VALUE64_PTR, VALUE64_FS),
                ERR_UNSUPPORTED_TYPE,
                "%d - %s", typ, value64_typename(typ) );
    int         cnt = 0;
    if (out)
        fprintf(out, "HSET: %s : %d\n", value64_typename(typ), se->count);

    for (int i = 0; i < se->sz; i++) {
        const hset_elem *el = se->table[i];
        while (el) {
            value64_fprint(out, el->v, typ);
            el = el->next;
            cnt++;
            fputc('\n', out);
        }
    }
    if (out)
        fprintf(out, "HSET: DONE\n");
    return logsimpleret(cnt, "Saved %d", cnt);
}

int                         hset_fload(FILE *restrict in, hset *restrict se) {
    invraisecode(in != NULL && se != 0, ERR_NULLABLE_PTR, "Null pointer");

#define HSET_LOAD_BUF_SIZE 200
#define HSET_LOAD_BUF_FMT "199"   /* HSET_LOAD_BUF_SIZE - 1 */

    char        buf[HSET_LOAD_BUF_SIZE];
    int         cnt = 0;
    hset        res;
    hset       *target = se;
    bool        must_free_on_error = false;

    //  TODO: rework!!! via fs_fscanf()
    if (fscanf(in, " HSET: %" HSET_LOAD_BUF_FMT "s : %d ", buf, &cnt) != 2)
        return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Invalid header");

    logsimple("[%s]", buf);
    value64_type   file_type = value64_gettype(buf);
    logauto(file_type);

    if (!hset_isnoninit(se) && file_type != hset_getype(se))
        return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Type mismatch: set %s, file %s",
                      value64_typename(hset_getype(se)), buf);

    if (hset_isnoninit(se) ){
        res = hset_init(cnt, file_type);
        must_free_on_error = true;
        target = &res;
    }

    int     addcnt = 0;
    fs      buf1 = FS();                // TODO: rework of hset_fload is requied to use onle 1 fs buf!!!
    for (int i = 0; i < cnt; i++) {
        value64 val = LITERAL64_ZERO;
        if (!value64_freadval(in, file_type, &val, &buf1)) {
            if (must_free_on_error)
                hset_free(&res);
            return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Failed to read element %d", i);
        }
        addcnt += hset_move(target, &val);    // addcnt++ only if real adding
    }

    // Проверяем завершающую строку "HSET: DONE"
    if (fscanf(in, " HSET: %" HSET_LOAD_BUF_FMT "s", buf) != 1 || strcmp(buf, "DONE") != 0){
        if (must_free_on_error)
            hset_free(&res);
        fsfree(buf1);
        return userraise(-1, ERR_WRONG_INPUT_FORMAT, "Missing or invalid 'HSET: DONE'");
    }
    if (hset_isnoninit(se) )
        *se = res;
    fsfree(buf1);

    return logsimpleret(addcnt, "Loaded %d/%d elements into set", addcnt, se->count);
}

int                         hset_save(const char *restrict fname, const hset *se) {
    invraisecode(se != NULL && fname != NULL, ERR_NULLABLE_PTR, "Null pointer");

    FILE *f = fopen(fname, "w");
    if (!f)
        return logsimpleret(-1, "Unable to open file %s", fname);

    int         cnt = hset_fsave(f, se);

    fclose(f);
    return logsimpleret(cnt, "Saved %d into %s", cnt, fname);
}

int                         hset_load(const char *restrict fname, hset *restrict se) {
    invraisecode(fname != NULL, ERR_NULLABLE_PTR, "Null pointer");

    FILE *f = fopen(fname, "r");
    if (!f)
        return userraise(-1, ERR_UNABLE_OPEN_FILE_READ, "Cannot open '%s'", fname);

    int    res = hset_fload(f, se);
    fclose(f);
    return logsimpleret(res, "%d loaded from '%s'", res, fname);
}

// ---------------------------------------- Testing ------------------------------------------
#ifdef HSETTESTING

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

// TODO: temporary
bool                        test_noteq(const hset *restrict se1, const hset *restrict se2){
    invraisecode(ERR_NULLABLE_PTR, se1 != 0 && se2 != 0, "Null pointers %p %p", se1, se2);

    if (hset_getype(se1) != hset_getype(se2) )
        return userraiseint(ERR_TYPES_MISMATCH, "Incorrect type %s vs %s", value64_typename(hset_getype(se1)), value64_typename(hset_getype(se2)));

    int     cnt1 = hset_cnt(se1);
    int     cnt2 = hset_cnt(se2);
    if (cnt1 != cnt2 )
        return logsimpleret(true, "Not equal counts %d vs %d", cnt1, cnt2);
    bool    res = false;
    for (int i = 0; i < se1->sz; i++)
        if (!find_elems(se1->table[i], se2) ){
            res = true;
            break;  // found at least 1 elem which not match to se2
        }
    return logsimpleret(res,  "Equal %s", bool_str(!res) );
}


// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: init + free", ++subnum);
    {
        hset se1 = hset_init(100, VALUE64_INT);
        HSET_TECH_FPRINTALL(logfile, se1);
        test_validatefree(
            hset_validate(stdout, &se1), hset_free(&se1), "Validation failed"
        );
        hset_free(&se1);
    }
    test_sub("subtest %d: init + free", ++subnum);
    {
        hset sefs1 = hset_init(100, VALUE64_FS);
        HSET_TECH_FPRINTALL(logfile, sefs1);
        test_validatefree(
            hset_validate(stdout, &sefs1), hset_free(&sefs1), "Validation failed"
        );
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 2 ---------------------------------

static TestStatus
tf2(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: VALUE64_INT init + get + free", ++subnum);
    {
        hset    se1 = hset_init(100, VALUE64_INT);
        int     num = 77;
        // mustb return false
        test_validatefree(
            hset_set(&se1, LITERAL64_INT(num) ),
            hset_free(&se1), "Must be true"
        );
        HSET_TECH_FPRINTALL(logfile, se1);
        test_validatefree(
            hset_set(&se1, LITERAL64_INT(num) ) == false,
            hset_free(&se1), "Must be false because elem %d aleady in the set", num
        );
        HSET_TECH_FPRINTALL(logfile, se1);
        test_validatefree(
            hset_validate(stdout, &se1), hset_free(&se1), "Validation failed"
        );
        test_validatefree(
            hset_get(&se1, LITERAL64_INT(num) ), hset_free(&se1), "Must be true,  because already added %d", num
        );
        test_validatefree(
            hset_get(&se1, LITERAL64_INT(num + 1) ) == false, hset_free(&se1), "Must be false %d", num + 1
        );
        test_validatefree(
            hset_del(&se1, LITERAL64_INT(num) ), hset_free(&se1), "Must be true, because element %d exists", num
        );
        HSET_TECH_FPRINTALL(logfile, se1);
        test_validatefree(
            hset_del(&se1, LITERAL64_INT(num) ) == false, hset_free(&se1), "Must be false, because element %d already deleted", num
        );
        test_validatefree(
            hset_validate(stdout, &se1), hset_free(&se1), "Validation failed"
        );
        hset_free(&se1);
    }
    test_sub("subtest %d: VALUE64_INT multiple add/del", ++subnum);
    {
        int     cnt = 100, mul = 3;
        hset    se1 = hset_init(cnt, VALUE64_INT);

        for (int i = 0; i < cnt * mul; i++)
            test_validatefree(
                hset_set(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Already exists %d", i
            );
        HSET_TECH_FPRINTALL(logfile, se1);

        for (int i = 0; i < cnt * mul; i++)
            test_validatefree(
                hset_get(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Unable to get %d", i
            );

        int     delfrom = 40, delto = 50;
        for (int i = delfrom; i < delto; i++)
            test_validatefree(
                hset_del(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Unable to delete %d", i
            );

        for (int i = 0; i < cnt * mul; i++)
            if (i >= delfrom && i < delto){
                test_validatefree(
                    hset_get(&se1, LITERAL64_INT(i) ) == false, hset_free(&se1), "Element %d was deleted, but return found somehow!", i
                );
            } else {
                test_validatefree(
                    hset_get(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Element %d not found", i
                );
            }
        test_validatefree(
            hset_validate(stdout, &se1), hset_free(&se1), "Validation failed"
        );

        hset_free(&se1);
    }
    test_sub("subtest %d: VALUE64_INT multiple reversed add/del", ++subnum);
    {
        int     cnt = 100, mul = 3;
        hset    se1 = hset_init(cnt, VALUE64_INT);

        for (int i = cnt * mul - 1; i >= 0; i--)
            test_validatefree(
                hset_set(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Already exists %d", i
            );

        for (int i = 0; i < cnt * mul; i++)
            test_validatefree(
                hset_get(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Unable to get %d", i
            );
        int     delfrom = 40, delto = 50;
        for (int i = delfrom; i < delto ; i++)
            test_validatefree(
                hset_del(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Unable to delete %d", i
            );

        for (int i = 0; i < cnt * mul; i++)
            if (i >= delfrom && i < delto){
                test_validatefree(
                    hset_get(&se1, LITERAL64_INT(i) ) == false, hset_free(&se1), "Element %d was deleted, but return found somehow!", i
                );
            } else {
                test_validatefree(
                    hset_get(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Element %d not found", i
                );
            }

        hset_free(&se1);
    }
    test_sub("subtest %d: random int add/del", ++subnum);
    {
        int     cnt = 100, mul = 3;
        hset    se1 = hset_init(cnt, VALUE64_INT);

        for (int i = cnt * mul - 1; i >= 0; i--)
            test_validatefree(
                hset_set(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Already exists %d", i
            );
        // random del
        srand(time(NULL));
        for (int i = 0; i < cnt * mul; i += rndint(5) + 1 )
            //if (i < cnt * mul)
                test_validatefree(
                    hset_del(&se1, LITERAL64_INT(i) ), hset_free(&se1), "Unable to delete [%d]", i
                );

        test_validatefree(
            hset_validate(stdout, &se1), hset_free(&se1), "Validation failed"
        );
        hset_free(&se1);
    }

    test_sub("subtest %d: FS init + add + get + free", ++subnum);
    {
        hset se = hset_init(10, VALUE64_FS);

        fs s1 = fscopy("hello");   // локальная копия строки
        fs s2 = fscopy("world");
        fs s3 = fscopy("foo");

        // Вставляем (hset_createval сделает свою копию)
        test_validatefree(
            hset_set(&se, LITERAL64_FS(s1)) == true,
            hset_free(&se),
            "Failed to insert 'hello'"
        );
        test_validatefree(
            hset_set(&se, LITERAL64_FS(s2)) == true,
            hset_free(&se),
            "Failed to insert 'world'"
        );
        test_validatefree(
            hset_set(&se, LITERAL64_FS(s3)) == true,
            hset_free(&se),
            "Failed to insert 'foo'"
        );

        // Проверяем наличие (s1, s2, s3 всё ещё валидны)
        test_validatefree(
            hset_get(&se, LITERAL64_FS(s1)) == true,
            hset_free(&se),
            "Missing 'hello'"
        );
        test_validatefree(
            hset_get(&se, LITERAL64_FS(s2)) == true,
            hset_free(&se),
            "Missing 'world'"
        );
        test_validatefree(
            hset_get(&se, LITERAL64_FS(s3)) == true,
            hset_free(&se),
            "Missing 'foo'"
        );

        test_validatefree(
            hset_cnt(&se) == 3,
            hset_free(&se),
            "Count should be 3, got %d", hset_cnt(&se)
        );

        // Очищаем исходные строки (они больше не нужны)
        fsfree(s1); fsfree(s2); fsfree(s3);
        hset_free(&se);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: FS empty \"\" checking", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_FS);
        fs      s1 = fsliteral("");
        fs      s2 = fscopy("");
        test_validatefree(
            hset_set(&se, LITERAL64_FS(s1)) == true,
            hset_free(&se),
            "Failed to insert empty literal"
        );
        hset_clean(&se);
        test_validatefree(
            hset_set(&se, LITERAL64_FS(s2)) == true,
            hset_free(&se),
            "Failed to insert empty fs"
        );
        test_validatefree(
            hset_set(&se, LITERAL64_FS(s1)) == false,
            hset_free(&se),
            "Shiuld be false, because empty line again (literal)"
        );
        fsfree(s2);
        hset_free(&se);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 3 ---------------------------------

static TestStatus
tf3(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: clone...", ++subnum);
    {
        hset    se1 = hset_init(10, VALUE64_LONG);
        int     cnt = 50;

        for (int i = 0; i < cnt; i++)
            test_validatefree(
                hset_set(&se1, LITERAL64_LNG(i) ), hset_free(&se1), "Already exists %d", i
            );
        hset    se2 = hset_clone(&se1);
        HSET_TECH_PRINTALL(se2);
        // then compare one by one
        bool        r1, r2;
        for (int i = 0; i < cnt; i++){
            r1 = hset_get(&se1, LITERAL64_LNG(i) );
            r2 = hset_get(&se2, LITERAL64_LNG(i) );
            test_validatefree(
                (r1 ^ r2) == false, (hset_free(&se1), hset_free(&se2) ),
                "%d: Must be true all, but origin %s, clone %s", i, bool_str(r1), bool_str(r2)
            );
        }

        hset_free(&se1);

        HSET_TECH_FPRINTALL(logfile, se2);
        test_validatefree(
            hset_validate(stdout, &se2), hset_free(&se2), "Validation failed"
        );

        hset_free(&se2);
    }
    test_sub("subtest %d: create from int array", ++subnum);
    {
        Array arr = IArray_create(200, ARRAY_FILLTYPE_RND);

        hset    se1 = hset_from_intarr(arr.iv, arr.len);

        HSET_TECH_FPRINTALL(logfile, se1);

        for (int i = 0; i < arr.len; i++)
            test_validatefree(
                hset_get(&se1, LITERAL64_INT(arr.iv[i]) ), (Array_free(&arr), hset_free(&se1) ),
                "Element %d isn't found", arr.iv[i]
            );

        test_validatefree(
            hset_validate(stdout, &se1), hset_free(&se1), "Validation failed"
        );

        Array_free(&arr);
        hset_free(&se1);
    }
    test_sub("subtest %d: create from int array, contained 0 value", ++subnum);
    {
        Array arr = IArray_create(10, ARRAY_FILLTYPE_ZERO);
        hset    se1 = hset_from_intarr(arr.iv, arr.len);

        HSET_TECH_FPRINTALL(stdout, se1);

        for (int i = 0; i < arr.len; i++)
            test_validatefree(
                hset_get(&se1, LITERAL64_INT(arr.iv[i]) ), (Array_free(&arr), hset_free(&se1) ),
                "Element %d isn't found", arr.iv[i]
            );

        Array_free(&arr);
        hset_free(&se1);
    }
    // VALUE64_FS
    /* ---- FS: clone set ---- */
    test_sub("subtest %d: FS clone", ++subnum);
    {
        hset    se1 = hset_init(10, VALUE64_FS);
        const char *words[] = {"alpha", "beta", "gamma", "delta"};
        for (int i = 0; i < COUNT(words); i++) {
            fs tmp = fscopy(words[i]);
            test_validatefree(
                hset_set(&se1, LITERAL64_FS(tmp)),
                hset_free(&se1),
                "Failed to insert '%s'", words[i]
            );
            fsfree(tmp);
        }

        hset    se2 = hset_clone(&se1);

        // Проверяем, что клон содержит те же элементы
        for (int i = 0; i < COUNT(words); i++) {
            fs tmp = fscopy(words[i]);
            test_validatefree(
                hset_get(&se2, LITERAL64_FS(tmp)),
                (hset_free(&se1), hset_free(&se2)),
                "Clone: missing '%s'", words[i]
            );
            fsfree(tmp);
        }

        test_validatefree(
            hset_cnt(&se2) == COUNT(words),
            (hset_free(&se1), hset_free(&se2)),
            "Clone: count = %d, expected %d", hset_cnt(&se2), COUNT(words)
        );

        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    /* ---- FS: create from array of strings ---- */
    test_sub("subtest %d: FS from array", ++subnum);
    {
        const int cnt = 30;
        fs      strings[cnt];   // локальные fs
        //fs     *parr[cnt];      // массив указателей для hset_loadanyarr

        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("item_%d", i);   // если fscopyf ещё нет, используйте fscopy + snprintf
            // parr[i] = &strings[i];
        }

        hset    se = hset_init(cnt, VALUE64_FS);
        // TODO: it should be from fs[]!
        int       loaded = hset_loadanyarr(&se, strings, cnt, VALUE64_FS);
        //int     loaded = hset_loadanyarr(&se, parr, cnt, VALUE64_FS);

        fs_alloc_check(false);

        test_validatefree(
            loaded == cnt,
            hset_free(&se),
            "FS from array: loaded %d, expected %d", loaded, cnt
        );
        test_validatefree(
            hset_cnt(&se) == cnt,
            hset_free(&se),
            "FS from array: count = %d, expected %d", hset_cnt(&se), cnt
        );

        // Проверяем наличие всех строк
        for (int i = 0; i < cnt; i++) {
            fs tmp = fscopyf("item_%d", i);
            test_validatefree(
                hset_get(&se, LITERAL64_FS(tmp)),
                hset_free(&se),
                "FS from array: missing 'item_%d'", i
            );
            fsfree(tmp);
        }

        // Освобождаем исходные строки (множество владеет копиями)
        /*for (int i = 0; i < cnt; i++){
            //fstechprint(strings[i] );
            fsfree(strings[i]);
        }*/
        hset_free(&se);
    }
    fs_alloc_check(true);
    /* ---- FS: array with duplicates ---- */
    test_sub("subtest %d: FS from array with duplicates", ++subnum);
    {
        const char *words[] = {"one", "two", "two", "three", "three", "three", "two", "two", "yyyyyy", "two"};
        const int uniq = 4;   // one, two, three, yyyyyy
        fs      strings[COUNT(words)];
        //fs     *parr[COUNT(words)];

        for (int i = 0; i < COUNT(words); i++) {
            strings[i] = fscopy(words[i]);
            //parr[i] = &strings[i];
        }

        hset    se = hset_init(10, VALUE64_FS);
        int     loaded = hset_loadanyarr(&se, strings, COUNT(words), VALUE64_FS);

        // loaded должно быть равно общему числу попыток, но count — только уникальные
        test_validatefree(
            loaded == COUNT(words),
            hset_free(&se),
            "Duplicates: loaded %d, expected %d", loaded, COUNT(words)
        );
        test_validatefree(
            hset_cnt(&se) == uniq,
            hset_free(&se),
            "Duplicates: count = %d, expected %d", hset_cnt(&se), uniq
        );

        //for (int i = 0; i < COUNT(words); i++)
        //    fsfree(strings[i]);
        hset_free(&se);
    }
    fs_alloc_check(true);
    /* ---- FS: array with NULL pointers (empty strings) ---- */
    test_sub("subtest %d: FS array with NULL pointers", ++subnum);
    {
        // Создадим массив, где некоторые указатели NULL (пустые fs)
        fs      arr[5] = { FS(), fscopy("valid1"), fscopy("valid2"), FS(), FS() };

        hset    se = hset_init(10, VALUE64_FS);
        int     loaded = hset_loadanyarr(&se, arr, COUNT(arr), VALUE64_FS);
        logauto(loaded);

        test_validatefree(
            loaded == COUNT(arr),
            hset_free(&se),
            "NULL array: loaded %d, expected 5", loaded
        );
        test_validatefree(
            hset_cnt(&se) == 2, // FS() rejected for now
            hset_free(&se),
            "NULL array: count = %d, expected 3", hset_cnt(&se)
        );
        HSET_TECH_FPRINTALL(logfile, se);
        // Проверяем, что валидные строки есть
        fs tmp = fscopy("valid1");
        test_validatefree(
            hset_get(&se, LITERAL64_FS(tmp)),
            hset_free(&se),
            "Missing valid1"
        );
        fsfree(tmp);
        tmp = fscopy("valid2");
        test_validatefree(
            hset_get(&se, LITERAL64_FS(tmp)),
            hset_free(&se),
            "Missing valid2"
        );
        fsfree(tmp);

        // iterator musr be here
        for(int i = 0; i < COUNT(arr); i++)
            fsfree(arr[i]);

        hset_free(&se);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST hset_loadfs_str ---------------------------------
static TestStatus
tf_loadfs_str(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    test_sub("subtest %d: load several FS strings", ++subnum);
    {
        hset se = hset_init(10, VALUE64_FS);
        const char *strings[] = {"/tmp/a", "/tmp/b", "/tmp/c", NULL};

        int cnt = hset_loadfs_str(&se, strings);
        test_validatefree(
            cnt == 3 && se.count == 3,
            hset_free(&se),
            "Loaded %d elements, expected 3; set len %d", cnt, se.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: load from empty array", ++subnum);
    {
        hset se = hset_init(10, VALUE64_FS);
        const char *empty[] = {NULL};

        int cnt = hset_loadfs_str(&se, empty);
        test_validatefree(
            cnt == 0 && se.count == 0,
            hset_free(&se),
            "Loaded %d elements from empty array, expected 0", cnt
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: load with duplicates", ++subnum);
    {
        hset se = hset_init(10, VALUE64_FS);
        const char *dups[] = {"/tmp/x", "/tmp/y", "/tmp/x", "/tmp/x", "uuuuuu", NULL};

        int cnt = hset_loadfs_str(&se, dups);
        test_validatefree(
            cnt == COUNT(dups) - 1 && se.count == 3,
            hset_free(&se),
            "Loaded %d elements (3 calls), set len %d, expected 2 unique", cnt, se.count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: NULL set pointer raises SIGINT", ++subnum);
    {
        const char *strings[] = {"/tmp/a", NULL};
        if (!try()) {
            hset_loadfs_str(NULL, strings);
            test_validate(false, "Should have raised SIGINT for NULL set");
        } else {
            logmsg("Exception correctly raised on NULL set");
        }
    }
    fs_alloc_check(true);

    test_sub("subtest %d: NULL strings pointer raises SIGINT", ++subnum);
    {
        hset se = hset_init(10, VALUE64_FS);
        if (!try()) {
            hset_loadfs_str(&se, NULL);
            test_validatefree(
                false, hset_free(&se),
                "Should have raised SIGINT for NULL strings"
            );
        } else {
            hset_free(&se);
            logmsg("Exception correctly raised on NULL strings");
        }
    }
    fs_alloc_check(true);

    test_sub("subtest %d: validate set after load", ++subnum);
    {
        hset se = hset_init(10, VALUE64_FS);
        const char *strings[] = {"/tmp/1", "/tmp/2", NULL};
        hset_loadfs_str(&se, strings);
        test_validatefree(
            hset_validate(stdout, &se),
            hset_free(&se),
            "Set validation failed after load"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 4 ---------------------------------

static TestStatus
tf4(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: empty count", ++subnum);
    {
        hset    se1 = hset_init(100, VALUE64_LONG);
        int     res;
        test_validatefree(
            (res = hset_cnt(&se1) ) == 0, hset_free(&se1), "Must be zero, but not %d", res
        );

        hset_free(&se1);
    }
    test_sub("subtest %d: loaded count", ++subnum);
    {
        rndinit();
        Array   arr = DArray_create(500, ARRAY_FILLTYPE_ASC);
        hset    se2 = hset_from_dblarr(arr.dv, arr.len);
        int     res;
        // hset_techprint(&se2, 5);
        //Array_print(arr, 5);
        test_validatefree(
            (res = hset_cnt(&se2) ) == Arraylen(arr), (hset_free(&se2), Arrayfree(arr) ), "Must be == %d, but not %d", Arraylen(arr), res
        );
        Arrayfree(arr);
        hset_free(&se2);
    }
    test_sub("subtest %d: loaded count int", ++subnum);
    {
        Array   arr = IArray_create(500, ARRAY_FILLTYPE_ASC);
        hset    se2 = hset_from_intarr(arr.iv, arr.len);
        int     res;
        //hset_techprint(&se2, 0);
        test_validatefree(
            (res = hset_cnt(&se2) ) == Arraylen(arr), (hset_free(&se2), Arrayfree(arr) ), "Must be == %d, but not %d", Arraylen(arr), res
        );
    test_sub("subtest %d: count after clean", ++subnum);

        hset_clean(&se2);
        test_validatefree(
            (res = hset_cnt(&se2) ) == 0, (hset_free(&se2), Arrayfree(arr) ), "Must be zero after cleanbut not %d", res
        );
        Arrayfree(arr);
        hset_free(&se2);
    }
    // VALUE64_FS
    test_sub("subtest %d: FS empty count", ++subnum);
    {
        hset    se = hset_init(100, VALUE64_FS);
        int     res;
        test_validatefree(
            (res = hset_cnt(&se)) == 0,
            hset_free(&se),
            "FS empty count must be 0, but got %d", res
        );
        hset_free(&se);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: FS loaded count", ++subnum);
    {
        const int cnt = 500;
        fs      strings[cnt];

        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("str_%d", i);   // уникальные строки
            //parr[i] = &strings[i];
        }

        hset    se = hset_init(cnt, VALUE64_FS);
        int     loaded = hset_loadanyarr(&se, strings, cnt, VALUE64_FS);
        int     set_cnt = hset_cnt(&se);

        // Пdосле загрузки исходные строки можно освободить
        //for (int i = 0; i < cnt; i++)
          //  fsfree(strings[i]);

        test_validatefree(
            loaded == cnt && set_cnt == cnt,
            hset_free(&se),
            "FS loaded count: loaded %d, set count %d, expected both %d",
            loaded, set_cnt, cnt
        );
        hset_free(&se);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: FS count after clean", ++subnum);
    {
        const int cnt = 200;
        fs      strings[cnt];
        //fs     *parr[cnt];

        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("clean_%d", i);
            //parr[i] = &strings[i];
        }

        hset    se = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se, strings, cnt, VALUE64_FS);
        for (int i = 0; i < cnt; i++) fsfree(strings[i]);

        hset_clean(&se);
        int  cntclean;

        test_validatefree(
            (cntclean = hset_cnt(&se) ) == 0,
            hset_free(&se),
            "FS count after clean must be 0, but got %d", cntclean
        );
        hset_free(&se);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 5 ---------------------------------
static TestStatus
tf5(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: clone and compare", ++subnum);
    {
        Array   arr = IArray_create(200, ARRAY_FILLTYPE_RND);

        hset    se1 = hset_from_intarr(arr.iv, arr.len);
        int     elem = arr.iv[0];   // save one
        Arrayfree(arr);

        hset    se2 = hset_clone(&se1);

        test_validatefree(
            hset_eq(&se1, &se2), (hset_free(&se1), hset_free(&se2) ), "Must be equal!"
        );
        // remove one elem and check again
        test_validatefree(
            hset_del(&se1, LITERAL64_INT(elem) ), (hset_free(&se1), hset_free(&se2) ), "Element %d must exists in se1", elem
        );
        test_validatefree(
            !hset_eq(&se1, &se2), (hset_free(&se1), hset_free(&se2) ), "Must be NOT equal after deleting %d!", elem
        );
        test_validatefree(
            hset_del(&se2, LITERAL64_INT(elem) ), (hset_free(&se1), hset_free(&se2) ), "Element %d must exists in se2", elem
        );
        test_validatefree(
            hset_eq(&se1, &se2), (hset_free(&se1), hset_free(&se2) ), "Must be equal again!"
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    test_sub("subtest %d: compare with different hash table size", ++subnum);
    {
        Array   arr = IArray_create(200, ARRAY_FILLTYPE_RND);
        // create from array
        hset    se1 = hset_from_intarr(arr.iv, Arraylen(arr) );
        // manually creating, small
        hset    se2 = hset_init(100, VALUE64_INT);
        hset_loadiarr(&se2, arr.iv, Arraylen(arr) );
        test_validatefree(
            hset_eq(&se1, &se2), (hset_free(&se1), hset_free(&se2) ), "Must be equal!"
        );
        // reload se1
        hset_loadiarr(&se1, arr.iv, Arraylen(arr) );
        test_validatefree(
            hset_eq(&se1, &se2), (hset_free(&se1), hset_free(&se2) ), "Must be equal again!"
        );
        hset_free(&se1);
        hset_free(&se2);
    }

    // VALUE64_FS
    test_sub("subtest %d: FS clone and compare", ++subnum);
    {
        const int cnt = 200;
        fs      strings[cnt];
       // fs     *parr[cnt];

        // Создаём массив уникальных строк
        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("fs_str_%d", i);
            //parr[i] = &strings[i];
        }

        hset    se1 = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se1, strings, cnt, VALUE64_FS);

        // Сохраняем первый элемент для последующего удаления
        fs      first_elem = fscopyf("fs_str_%d", 0);   // или просто fscopyf("fs_str_0")
        //fs     *first_parr[1] = { &first_elem };

        // Клонируем se1
        hset    se2 = hset_clone(&se1);

        // 1. Исходно множества должны быть равны
        test_validatefree(
            hset_eq(&se1, &se2),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone: sets must be equal after clone"
        );

        // 2. Удаляем элемент из se1
        test_validatefree(
            hset_del(&se1, LITERAL64_FS(first_elem)),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone: failed to delete '%s' from se1", fsstr(first_elem)
        );
        // После удаления множества не должны быть равны
        test_validatefree(
            !hset_eq(&se1, &se2),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone: sets must NOT be equal after deletion from se1"
        );

        // 3. Удаляем тот же элемент из se2
        test_validatefree(
            hset_del(&se2, LITERAL64_FS(first_elem)),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone: failed to delete '%s' from se2", fsstr(first_elem)
        );
        // Теперь снова должны быть равны
        test_validatefree(
            hset_eq(&se1, &se2),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone: sets must be equal after both deletions"
        );

        // Освобождаем исходные строки (множества владеют копиями)
        //for (int i = 0; i < cnt; i++)
          //  fsfree(strings[i]);
        fsfree(first_elem);
        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: FS compare with different hash table size", ++subnum);
    {
        const int cnt = 200;
        fs      strings[cnt];
        //fs     *parr[cnt];

        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("diff_%d", i);
            //parr[i] = &strings[i];
        }

        // Первое множество – стандартное создание из массива
        hset    se1 = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se1, strings, cnt, VALUE64_FS);

        for (int i = 0; i < cnt; i++)
            strings[i] = fscopyf("diff_%d", i);

        // Второе – с заведомо меньшим начальным размером таблицы
        hset    se2 = hset_init(cnt / 2, VALUE64_FS);
        hset_loadanyarr(&se2, strings, cnt, VALUE64_FS);

        // Они должны быть равны, несмотря на разный размер таблиц
        test_validatefree(
            hset_eq(&se1, &se2),
            (hset_free(&se1), hset_free(&se2)),
            "FS diff size: sets must be equal after initial load"
        );

        for (int i = 0; i < cnt; i++)
            strings[i] = fscopyf("diff_%d", i);

        // Повторная загрузка тех же данных не должна испортить равенство
        hset_loadanyarr(&se1, strings, cnt, VALUE64_FS);
        test_validatefree(
            hset_eq(&se1, &se2),
            (hset_free(&se1), hset_free(&se2)),
            "FS diff size: sets must be equal after reloading duplicates"
        );

        //for (int i = 0; i < cnt; i++)
          //  fsfree(strings[i]);
        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 6 ---------------------------------
static TestStatus
tf6(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: clone and !=", ++subnum);
    {
        Array   arr = IArray_create(200, ARRAY_FILLTYPE_RND);

        hset    se1 = hset_from_intarr(arr.iv, arr.len);
        int     elem = arr.iv[0];   // save one
        Arrayfree(arr);

        hset    se2 = hset_clone(&se1);

        test_validatefree(
            hset_noteq(&se1, &se2) == false, (hset_free(&se1), hset_free(&se2) ), "Must be equal (not equalt must return false)!"
        );
        // remove one elem and check again
        test_validatefree(
            hset_del(&se1, LITERAL64_INT(elem) ), (hset_free(&se1), hset_free(&se2) ), "Element %d must exists in se1", elem
        );
        test_validatefree(
            hset_noteq(&se1, &se2), (hset_free(&se1), hset_free(&se2) ), "Must be NOT equal (returns true) after deleting %d!", elem
        );
        test_validatefree(
            hset_del(&se2, LITERAL64_INT(elem) ), (hset_free(&se1), hset_free(&se2) ), "Element %d must exists in se2", elem
        );
        test_validatefree(
            hset_noteq(&se1, &se2) == false, (hset_free(&se1), hset_free(&se2) ), "Must be equal again (returns false)!"
        );
        hset_free(&se1);
        hset_free(&se2);
    }
    test_sub("subtest %d: != with different hash table size", ++subnum);
    {
        Array   arr = IArray_create(150, ARRAY_FILLTYPE_RND);
        int     elem = arr.iv[0];   // save one
        // create from array
        hset    se1 = hset_from_intarr(arr.iv, Arraylen(arr) );
        // manually creating, small
        hset    se2 = hset_init(50, VALUE64_INT);
        hset_loadiarr(&se2, arr.iv, Arraylen(arr) );
        test_validatefree(
            hset_noteq(&se1, &se2) == false, (hset_free(&se1), hset_free(&se2) ), "Must be equal (returns false)!"
        );
        // reload se1
        hset_loadiarr(&se1, arr.iv, Arraylen(arr) );
        test_validatefree(
            hset_noteq(&se1, &se2) == false, (hset_free(&se1), hset_free(&se2) ), "Must be equal again (return false)!"
        );
        test_validatefree(
            hset_del(&se1, LITERAL64_INT(elem) ), (hset_free(&se1), hset_free(&se2) ), "Element %d must exists in se1", elem
        );
        test_validatefree(
            hset_noteq(&se1, &se2), (hset_free(&se1), hset_free(&se2) ), "Must be not equal after deleting %d (return tur)!", elem
        );
        test_validatefree(
            hset_del(&se2, LITERAL64_INT(elem) ), (hset_free(&se1), hset_free(&se2) ), "Element %d must exists in se2", elem
        );
        test_validatefree(
            hset_noteq(&se1, &se2) == false, (hset_free(&se1), hset_free(&se2) ),
            "Must be equal again after deleting %d from se2 (return false)!", elem
        );

        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    // VALUE64_FS
    test_sub("subtest %d: FS clone and !=", ++subnum);
    {
        const int cnt = 10;
        fs      strings[cnt];

        // Создаём массив уникальных строк
        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("fs_str_%d", i);
        }

        hset    se1 = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se1, strings, cnt, VALUE64_FS);

        // Клонируем se1
        hset    se2 = hset_clone(&se1);

        // 1. Исходно множества должны быть равны (hset_noteq возвращает false)
        test_validatefree(
            hset_noteq(&se1, &se2) == false,
            (hset_free(&se1), hset_free(&se2)),
            "FS clone !=: sets must be equal initially"
        );

        // Удаляем первый элемент из se1 (сохраняем ссылку на него для проверки)
        fs      first_elem = fscopyf("fs_str_0");
        test_validatefree(
            hset_del(&se1, LITERAL64_FS(first_elem)),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone !=: failed to delete first element from se1"
        );

        // 2. После удаления множества должны быть НЕ равны
        test_validatefree(
            hset_noteq(&se1, &se2) == true,
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone !=: sets must be not equal after deletion from se1"
        );

        // Удаляем тот же элемент из se2
        test_validatefree(
            hset_del(&se2, LITERAL64_FS(first_elem)),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone !=: failed to delete first element from se2"
        );

        // 3. Теперь снова должны быть равны
        test_validatefree(
            hset_noteq(&se1, &se2) == false,
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS clone !=: sets must be equal after both deletions"
        );

        // Освобождаем исходные строки (множества владеют копиями)
        //for (int i = 0; i < cnt; i++)
          //  fsfree(strings[i]);

        fsfree(first_elem);
        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    test_sub("subtest %d: FS != with different hash table size", ++subnum);
    {
        const int cnt = 15;
        fs      strings[cnt];

        for (int i = 0; i < cnt; i++)
            strings[i] = fscopyf("diff_fs_%d", i);

        // Первое множество – создание из массива
        hset    se1 = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se1, strings, cnt, VALUE64_FS);

        // Второе – с заведомо меньшим размером таблицы
        hset    se2 = hset_init(cnt / 3, VALUE64_FS);
        for (int i = 0; i < cnt; i++)
            strings[i] = fscopyf("diff_fs_%d", i);
        hset_loadanyarr(&se2, strings, cnt, VALUE64_FS);

        // 1. Должны быть равны, несмотря на разный размер таблиц
        test_validatefree(
            hset_noteq(&se1, &se2) == false,
            (hset_free(&se1), hset_free(&se2)),
            "FS diff size !=: sets must be equal initially"
        );

        // Повторная загрузка тех же данных не должна испортить равенство
        for (int i = 0; i < cnt; i++)
            strings[i] = fscopyf("diff_fs_%d", i);
        hset_loadanyarr(&se1, strings, cnt, VALUE64_FS);
        test_validatefree(
            hset_noteq(&se1, &se2) == false,
            (hset_free(&se1), hset_free(&se2)),
            "FS diff size !=: sets must be equal after reloading duplicates"
        );

        // Удаляем первый элемент из se1
        fs      first_elem = fscopyf("diff_fs_0");
        test_validatefree(
            hset_del(&se1, LITERAL64_FS(first_elem)),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS diff size !=: failed to delete first element from se1"
        );

        // 2. После удаления должны стать не равны
        test_validatefree(
            hset_noteq(&se1, &se2) == true,
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS diff size !=: sets must be not equal after deletion from se1"
        );
        // Удаляем тот же элемент из se2
        test_validatefree(
            hset_del(&se2, LITERAL64_FS(first_elem)),
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS diff size !=: failed to delete first element from se2"
        );
       // fs_alloc_check(true);
        // 3. Снова равны
        test_validatefree(
            hset_noteq(&se1, &se2) == false,
            (hset_free(&se1), hset_free(&se2), fsfree(first_elem)),
            "FS diff size !=: sets must be equal after both deletions"
        );

        fsfree(first_elem);

        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    /* Бонус: проверка неравенства после перемещения (VALUE64_FSMOVE) */
    test_sub("subtest %d: FS != after move", ++subnum);
    {
        hset    se1 = hset_init(10, VALUE64_FS);
        hset    se2 = hset_init(10, VALUE64_FS);

        // Вставляем одну и ту же строку, но в se1 через перемещение, в se2 через копирование
        value64 copyv = value64_createfs_asstr("move_vs_copy");
        hset_set(&se1, copyv);

        value64 movev = value64_createfs_asstr("move_vs_copy");
        hset_move(&se2, &movev);

        test_validatefree(
            value64_fs(movev) == NULL,
            (hset_free(&se1), hset_free(&se2), value64_freefs(&copyv), value64_freefs(&movev) ),
            "movev must be empty after moving"
        );

        // Множества должны быть равны, потому что строки одинаковы
        test_validatefree(
            hset_noteq(&se1, &se2) == false,
            (hset_free(&se1), hset_free(&se2), value64_freefs(&copyv), value64_freefs(&movev) ),
            "FS != after move: sets with moved and copied strings must be equal"
        );

        // Удаляем элемент из se1
        test_validatefree(
            hset_del(&se1, copyv ),
            (hset_free(&se1), hset_free(&se2), value64_freefs(&copyv), value64_freefs(&movev) ),
            "FS != after move: failed to delete from se1"
        );
        // Теперь должны быть не равны
        test_validatefree(
            hset_noteq(&se1, &se2) == true,
            (hset_free(&se1), hset_free(&se2), value64_freefs(&copyv), value64_freefs(&movev) ),
            "FS != after move: sets must be not equal after deletion"
        );
        fs_fprint_checker_cnt(logfile, "V");

        value64_freefs(&copyv), value64_freefs(&movev);
        //HSET_TECH_PRINTALL(se1);
        //HSET_TECH_PRINTALL(se2);

        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 7 ---------------------------------
static TestStatus
tf7(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: cloneas int -> long", ++subnum);
    {
        Array   arr = IArray_create(/*200*/ 10, ARRAY_FILLTYPE_ASC);

        hset    se1 = hset_from_intarr(arr.iv, arr.len);
        Arrayfree(arr);

        hset    se2 = hset_cloneas(&se1, VALUE64_LONG);

        test_validatefree(
            hset_validate(stdout, &se1) && hset_validate(stdout, &se2),
            (hset_free(&se1), hset_free(&se2) ), "Validation failed"
        );
        int     sz1 = se1.sz, sz2 = se2.sz;
        test_validatefree(
            se1.sz == se2.sz, (hset_free(&se1), hset_free(&se2) ), "Must have equal table size %d : %d", sz1, sz2
        );

        sz1 = hset_cnt(&se1);
        sz2 = hset_cnt(&se2);
        test_validatefree(
            se1.sz == se2.sz, (hset_free(&se1), hset_free(&se2) ), "Must have equal element count %d : %d", sz1, sz2
        );

        // compare directly!
        for (int i = 0; i < se1.sz; i++){
            const hset_elem *el = se1.table[i];
            while (el){
                int val = el->v.ival;
                test_validatefree(
                    hset_get(&se2, LITERAL64_LNG(val) ), (hset_free(&se1), hset_free(&se2) ), "Unable to find elem %d in se2", val
                );
                el = el->next;
            }
        }
        hset_free(&se1);
        hset_free(&se2);
    }
    test_sub("subtest %d: cloneas int -> double", ++subnum);
    {
        Array   arr = IArray_create(110, ARRAY_FILLTYPE_ASC);

        hset    se1 = hset_from_intarr(arr.iv, arr.len);
        Arrayfree(arr);

        hset    se2 = hset_cloneas(&se1, VALUE64_DBL);

        int     sz1 = se1.sz, sz2 = se2.sz;
        test_validatefree(
            se1.sz == se2.sz, (hset_free(&se1), hset_free(&se2) ), "Must have equal table size %d : %d", sz1, sz2
        );

        test_validatefree(
            hset_validate(stdout, &se1) && hset_validate(stdout, &se2),
            (hset_free(&se1), hset_free(&se2)), "Validation failed se1"
        );

        sz1 = hset_cnt(&se1);
        sz2 = hset_cnt(&se2);
        test_validatefree(
            se1.sz == se2.sz, (hset_free(&se1), hset_free(&se2) ), "Must have equal element count %d : %d", sz1, sz2
        );

        // compare directly! is it correct to compare int vs double?
        for (int i = 0; i < se1.sz; i++){
            const hset_elem *el = se1.table[i];
            while (el){
                int val = el->v.ival;
                test_validatefree(
                    hset_get(&se2, LITERAL64_DBL(val) ), (hset_free(&se1), hset_free(&se2) ), "Unable to find elem %d in se2", val
                );
                el = el->next;
            }
        }
        hset_free(&se1);
        hset_free(&se2);
    }
    /* ========== 1. cloneas int -> FS ========== */
    test_sub("subtest %d: cloneas int -> FS", ++subnum);
    {
        int     cnt = 10 /*200*/;
        Array   arr = IArray_create(cnt, ARRAY_FILLTYPE_ASC);   // 0,1,2,...,199
        hset    se_int = hset_from_intarr(arr.iv, arr.len);
        Arrayfree(arr);

        hset    se_fs = hset_cloneas(&se_int, VALUE64_FS);

        test_validatefree(
            hset_validate(stdout, &se_int) && hset_validate(stdout, &se_fs),
            (hset_free(&se_int), hset_free(&se_fs)),
            "Validation failed after int->FS clone"
        );

        int     sz1, sz2;
        test_validatefree(
            (sz1 = se_int.sz) == (sz2 = se_fs.sz),
            (hset_free(&se_int), hset_free(&se_fs)),
            "Table sizes differ: %d vs %d", sz1, sz2
        );

        int     cnt1 = hset_cnt(&se_int), cnt2 = hset_cnt(&se_fs);
        test_validatefree(
            (cnt1 = hset_cnt(&se_int) ) == (cnt2 = hset_cnt(&se_fs) ),
            (hset_free(&se_int), hset_free(&se_fs)),
            "Counts differ: %d vs %d", cnt1, cnt2
        );

        fs tmp = FS();
        HSET_FOREACH_INT(&se_int, ival) {
            fs_sprintf(&tmp, "%d", ival);
            test_validatefree(
                hset_get(&se_fs, LITERAL64_FS(tmp)),
                (hset_free(&se_int), hset_free(&se_fs), fsfree(tmp)),
                "FS set missing '%d'", ival
            );
        }
        fsfree(tmp);

        hset_free(&se_int);
        hset_free(&se_fs);
    }
    fs_alloc_check(true);
    /* ========== 2. cloneas FS -> int ========== */
    test_sub("subtest %d: cloneas FS -> int", ++subnum);
    {
        const int   cnt = 50;
        fs          strings[cnt];

        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("%d", i);
        }

        hset    se_fs = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se_fs, strings, cnt, VALUE64_FS);

        hset    se_int = hset_cloneas(&se_fs, VALUE64_INT);

        test_validatefree(
            hset_validate(stdout, &se_fs) && hset_validate(stdout, &se_int),
            (hset_free(&se_fs), hset_free(&se_int)),
            "Validation failed after FS->int clone"
        );

        int         cnt1, cnt2;
        test_validatefree(
            (cnt1 = hset_cnt(&se_fs) ) == (cnt2 = hset_cnt(&se_fs) ),
            (hset_free(&se_fs), hset_free(&se_int)),
            "Counts differ: %d vs %d", cnt1, cnt2
        );

        for (int i = 0; i < cnt; i++) {
            test_validatefree(
                hset_get(&se_int, LITERAL64_INT(i)),
                (hset_free(&se_fs), hset_free(&se_int)),
                "Int set missing %d", i
            );
        }

        hset_free(&se_fs);
        hset_free(&se_int);
    }
    fs_alloc_check(true);
    /* ========== 3. cloneas FS -> FS (identity) ========== */
    test_sub("subtest %d: cloneas FS -> FS (identity)", ++subnum);
    {
        const int cnt = 5;
        fs      strings[cnt];

        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("str_%d", i);
        }

        hset    se1 = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se1, strings, cnt, VALUE64_FS);

        hset    se2 = hset_cloneas(&se1, VALUE64_FS);

        test_validatefree(
            hset_validate(stdout, &se1) && hset_validate(stdout, &se2),
            (hset_free(&se1), hset_free(&se2)),
            "Validation failed after FS->FS clone"
        );

        fs      tmp = FS();
        for (int i = 0; i < cnt; i++) {
            fs_sprintf(&tmp, "str_%d", i);
            test_validatefree(
                hset_get(&se2, LITERAL64_FS(tmp)),
                (hset_free(&se1), hset_free(&se2), fsfree(tmp)),
                "Clone missing 'str_%d'", i
            );
        }
        fsfree(tmp);

        hset_free(&se1);
        hset_free(&se2);
    }
    fs_alloc_check(true);
    /* ========== 4. cloneas double -> FS ========== */
    test_sub("subtest %d: cloneas double -> FS", ++subnum);
    {
        const int cnt = 30;
        double  vals[cnt];
        // TODO: rework via DArray_create
        for (int i = 0; i < cnt; i++)
            vals[i] = i * 1.5;

        hset    se_dbl = hset_init(cnt, VALUE64_DBL);
        for (int i = 0; i < cnt; i++)
            hset_set(&se_dbl, LITERAL64_DBL(vals[i]));

        hset    se_fs = hset_cloneas(&se_dbl, VALUE64_FS);

        test_validatefree(
            hset_validate(stdout, &se_dbl) && hset_validate(stdout, &se_fs),
            (hset_free(&se_dbl), hset_free(&se_fs)),
            "Validation failed after double->FS clone"
        );

        fs      tmp = FS();
        for (int i = 0; i < cnt; i++) {
            fs_sprintf(&tmp, "%g", vals[i]);
            test_validatefree(
                hset_get(&se_fs, LITERAL64_FS(tmp)),
                (hset_free(&se_dbl), hset_free(&se_fs), fsfree(tmp)),
                "FS set missing '%g'", vals[i]
            );
        }
        fsfree(tmp);

        hset_free(&se_dbl);
        hset_free(&se_fs);
    }
    fs_alloc_check(true);
    /* ========== 5. cloneas FS -> double ========== */
    test_sub("subtest %d: cloneas FS -> double", ++subnum);
    {
        const int cnt = 30;
        double  vals[cnt];
        fs      strings[cnt];

        // TODO: replace to macro DArray_create( {iter * 1.5} )
        for (int i = 0; i < cnt; i++) {
            vals[i] = i * 1.5;
            strings[i] = fscopyf("%g", vals[i]);
        }

        hset    se_fs = hset_init(cnt, VALUE64_FS);
        hset_loadanyarr(&se_fs, strings, cnt, VALUE64_FS);
        for (int i = 0; i < cnt; i++)
            fsfree(strings[i]);

        hset    se_dbl = hset_cloneas(&se_fs, VALUE64_DBL);

        test_validatefree(
            hset_validate(stdout, &se_fs) && hset_validate(stdout, &se_dbl),
            (hset_free(&se_fs), hset_free(&se_dbl)),
            "Validation failed after FS->double clone"
        );

        int     cnt1, cnt2;
        test_validatefree(
            (cnt1 = hset_cnt(&se_fs) ) == (cnt2 = hset_cnt(&se_dbl) ),
            (hset_free(&se_fs), hset_free(&se_dbl)),
            "Counts differ: %d vs %d", cnt1, cnt2
        );

        for (int i = 0; i < cnt; i++) {
            test_validatefree(
                hset_get(&se_dbl, LITERAL64_DBL(vals[i])),
                (hset_free(&se_fs), hset_free(&se_dbl)),
                "Double set missing %g", vals[i]
            );
        }

        hset_free(&se_fs);
        hset_free(&se_dbl);
    }
    fs_alloc_check(true);
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}

// ------------------------- TEST 16 ---------------------------------
static TestStatus
tf16(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое множество увеличиваем – остаётся пустым */
    test_sub("subtest %d: empty resize larger", ++subnum);
    {
        hset se = hset_init(10, VALUE64_INT);          // sz станет 11
        int old_sz = se.sz;
        hset new_se = hset_init_resize(&se, 30);    // запросим 30

        int ok = (se.sz > old_sz) && (se.sz >= 30) &&
                 (hset_cnt(&se) == 0) && (new_se.sz == se.sz);
        test_validatefree(
            ok,
            hset_free(&se),
            "Empty resize: old_sz=%d -> new_sz=%d, cnt=%d (expected cnt=0)",
            old_sz, se.sz, hset_cnt(&se)
        );
        test_validatefree(
            hset_validate(logfile, &se), hset_free(&se),
            "Validation failed"
        );
        hset_free(&se);
    }

    /* 2. Непустое множество увеличиваем – все элементы сохраняются */
    test_sub("subtest %d: non‑empty resize larger", ++subnum);
    {
        int vals[] = {1, 2, 3, 4, 5};
        hset se = hset_from_intarr(vals, COUNT(vals));
        int old_cnt = hset_cnt(&se);
        int old_sz = se.sz;
        hset_init_resize(&se, 100);

        int ok = (se.sz > old_sz) && (hset_cnt(&se) == old_cnt);
        // проверяем, что все значения доступны
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Non‑empty resize larger: old_sz=%d -> new_sz=%d, cnt=%d (expected %d)",
            old_sz, se.sz, hset_cnt(&se), old_cnt
        );
        test_validatefree(
            hset_validate(logfile, &se), hset_free(&se),
            "Validation failed"
        );
        hset_free(&se);
    }

    /* 3. Непустое множество уменьшаем (но не меньше количества элементов) */
    test_sub("subtest %d: non‑empty resize smaller", ++subnum);
    {
        int vals[] = {10, 20, 30};
        hset se = hset_from_intarr(vals, COUNT(vals));
        int old_cnt = hset_cnt(&se);
        int old_sz = se.sz;
        hset_init_resize(&se, 3);   // 3 элемента, next_prime(3) = 3

        // Размер может стать меньше, но элементы должны остаться
        int ok = (hset_cnt(&se) == old_cnt);
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Non‑empty resize smaller: old_sz=%d -> new_sz=%d, cnt=%d (expected %d)",
            old_sz, se.sz, hset_cnt(&se), old_cnt
        );
        test_validatefree(
            hset_validate(logfile, &se), hset_free(&se),
            "Validation failed"
        );
        hset_free(&se);
    }

    /* 4. Попытка переразмерить на тот же размер – ничего не меняется */
    test_sub("subtest %d: resize to same size", ++subnum);
    {
        int vals[] = {7, 8, 9};
        hset se = hset_from_intarr(vals, COUNT(vals));
        int old_sz = se.sz;
        int old_cnt = hset_cnt(&se);
        hset_init_resize(&se, old_sz - 1); // передаём sz-1, чтобы next_prime вернуло old_sz

        int ok = (se.sz == old_sz) && (hset_cnt(&se) == old_cnt);
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Resize to same size: sz should stay %d, got %d, cnt=%d",
            old_sz, se.sz, hset_cnt(&se)
        );
        test_validatefree(
            hset_validate(logfile, &se), hset_free(&se),
            "Validation failed"
        );
        hset_free(&se);
    }

    /* 5. Последовательные ресайзы */
    test_sub("subtest %d: multiple resizes", ++subnum);
    {
        int vals[] = {100, 200, 300, 400};
        hset se = hset_from_intarr(vals, COUNT(vals));
        int cnt = hset_cnt(&se);

        hset_init_resize(&se, 50);
        hset_init_resize(&se, 10);
        hset_init_resize(&se, 200);

        int ok = (hset_cnt(&se) == cnt);
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Multiple resizes: final sz=%d, cnt=%d (expected %d)",
            se.sz, hset_cnt(&se), cnt
        );
        test_validatefree(
            hset_validate(logfile, &se), hset_free(&se),
            "Validation failed"
        );
        hset_free(&se);
    }

    /* 6. Ресайз с типом double (проверка, что тип не теряется) */
    test_sub("subtest %d: resize with double", ++subnum);
    {
        double dvals[] = {1.5, 2.5, 3.5};
        hset se = hset_from_dblarr(dvals, COUNT(dvals));
        int old_cnt = hset_cnt(&se);
        hset_init_resize(&se, 50);

        int ok = (hset_cnt(&se) == old_cnt) && (se.flags == VALUE64_DBL);
        for (int i = 0; ok && i < COUNT(dvals); i++)
            ok = hset_get(&se, LITERAL64_DBL(dvals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Resize double: sz=%d, cnt=%d (expected %d)",
            se.sz, hset_cnt(&se), old_cnt
        );
        test_validatefree(
            hset_validate(logfile, &se), hset_free(&se),
            "Validation failed"
        );
        hset_free(&se);
    }
    // fs
    test_sub("subtest %d: resize empty FS set", ++subnum);
    {
        hset se = hset_init_fs(10);
        hset_init_resize(&se, 20);
        test_validatefree(
            se.count == 0 && se.sz >= 20 && hset_validate(stdout, &se),
            hset_free(&se),
            "Empty FS set after resize should be valid and larger"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: enlarge non-empty FS set", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        int old_count = se.count;
        hset_init_resize(&se, 50);
        test_validatefree(
            se.count == old_count &&
            hset_validate(stdout, &se) &&
            HSET_HAS_FS(&se, "/tmp/a") &&
            HSET_HAS_FS(&se, "/tmp/b") &&
            HSET_HAS_FS(&se, "/tmp/c"),
            hset_free(&se),
            "All elements must be present after enlarge"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: shrink non-empty FS set", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        int old_count = se.count;
        hset_init_resize(&se, 3);   // новый размер меньше текущего количества элементов
        test_validatefree(
            se.count == old_count &&
            hset_validate(stdout, &se) &&
            HSET_HAS_FS(&se, "/tmp/x") &&
            HSET_HAS_FS(&se, "/tmp/y"),
            hset_free(&se),
            "Elements must survive shrinking"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: multiple resizes", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/1", "/tmp/2", "/tmp/3", "/tmp/4");
        int cnt = se.count;
        hset_init_resize(&se, 7);
        hset_init_resize(&se, 20);
        hset_init_resize(&se, 5);
        test_validatefree(
            se.count == cnt &&
            HSET_HAS_FS(&se, "/tmp/1") &&
            HSET_HAS_FS(&se, "/tmp/2") &&
            HSET_HAS_FS(&se, "/tmp/3") &&
            HSET_HAS_FS(&se, "/tmp/4"),
            hset_free(&se),
            "All elements must survive multiple resizes"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: resize to very small size", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/z");
        // resize до 0 или 1: реализация должна установить минимально допустимый размер
        hset_init_resize(&se, 0);
        test_validatefree(
            hset_validate(stdout, &se) && HSET_HAS_FS(&se, "/tmp/z"),
            hset_free(&se),
            "Resize to zero should not destroy element"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 19 ---------------------------------

static TestStatus
tf19(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Пустое множество: load factor = 0, должен уменьшиться до HSET_MIN_SIZE */
    test_sub("subtest %d: empty set normalized", ++subnum);
    {
        hset    se = hset_init(100, VALUE64_INT);    // sz будет 101
        int     old_sz = se.sz;
        int     old_cnt = hset_cnt(&se);
        hset    res = hset_normalize(&se);

        int     ok = (res.sz == se.sz) &&                     // возврат совпадает с se
                     (hset_cnt(&se) == old_cnt) &&           // пусто
                     (se.sz <= old_sz) &&                    // размер уменьшился
                     (se.sz >= HSET_MIN_SIZE);               // но не меньше минимума
        test_validatefree(
            ok,
            hset_free(&se),
            "Empty set: old_sz=%d -> new_sz=%d, cnt=%d (expected sz <= %d, >= %d)",
            old_sz, se.sz, hset_cnt(&se), old_sz, HSET_MIN_SIZE
        );
        hset_free(&se);
    }

    /* 2. Низкая загрузка (много пустых корзин) -> размер уменьшается */
    test_sub("subtest %d: low load factor", ++subnum);
    {
        int     vals[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        hset    se = hset_init(1000, VALUE64_INT);   // сразу задаём большой размер
        hset_loadiarr(&se, vals, COUNT(vals));    // загружаем элементы

        int     old_sz = se.sz;
        int     cnt_before = hset_cnt(&se);
        hset    res = hset_normalize(&se);

        int     ok = (res.sz == se.sz) && (hset_cnt(&se) == cnt_before) &&
                     (se.sz < old_sz);            // размер должен уменьшиться
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Low load factor: old_sz=%d -> new_sz=%d, cnt=%d (expected sz < %d)",
            old_sz, se.sz, hset_cnt(&se), old_sz
        );
        hset_free(&se);
    }
    /* 3. Высокая загрузка (>0.75) -> размер увеличивается */
    test_sub("subtest %d: high load factor", ++subnum);
    {
        int     vals[100];
        for (int i = 0; i < COUNT(vals); i++)
            vals[i] = i;

        hset    se = hset_init(100, VALUE64_INT);   // next_prime(100) = 101
        hset_loadiarr(&se, vals, COUNT(vals));   // 100 элементов, load factor ≈ 0.99
        int     old_sz = se.sz;
        int     cnt_before = hset_cnt(&se);
        hset    res = hset_normalize(&se);

        int     ok = (res.sz == se.sz) && (hset_cnt(&se) == cnt_before) &&
                     (se.sz > old_sz);            // размер должен вырасти
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "High load factor: old_sz=%d -> new_sz=%d, cnt=%d (expected sz > %d)",
            old_sz, se.sz, hset_cnt(&se), old_sz
        );
        hset_free(&se);
    }
    /* 4. Нормальная загрузка (в пределах 0.25..0.75) -> размер не меняется */
    test_sub("subtest %d: normal load factor unchanged", ++subnum);
    {
        int     vals[20];
        for (int i = 0; i < COUNT(vals); i++)
            vals[i] = i;

        hset    se = hset_init(50, VALUE64_INT);           // sz станет 53 (next_prime(50))
        hset_loadiarr(&se, vals, COUNT(vals));          // 20 элементов, load factor ≈ 0.38
        int     old_sz = se.sz;
        int     cnt_before = hset_cnt(&se);
        hset    res = hset_normalize(&se);

        int     ok = (res.sz == se.sz) && (hset_cnt(&se) == cnt_before) &&
                     (se.sz == old_sz);                 // размер не должен измениться
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Normal load factor: old_sz=%d -> new_sz=%d, cnt=%d (expected sz unchanged %d)",
            old_sz, se.sz, hset_cnt(&se), old_sz
        );
        hset_free(&se);
    }
    /* 5. После нормализации все элементы доступны (уже проверялось, но оставим явный) */
    test_sub("subtest %d: elements preserved after normalize", ++subnum);
    {
        int     vals[] = {5, 15, 25, 35, 45};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        hset_normalize(&se);   // после создания load factor ~5/7=0.71 – может не измениться, но проверим

        int     ok = (hset_cnt(&se) == COUNT(vals));
        for (int i = 0; ok && i < COUNT(vals); i++)
            ok = hset_get(&se, LITERAL64_INT(vals[i]));

        test_validatefree(
            ok,
            hset_free(&se),
            "Elements preserved: cnt=%d (expected %d)",
            hset_cnt(&se), COUNT(vals)
        );
        hset_free(&se);
    }
    // fs
    test_sub("subtest %d: normalize empty FS set", ++subnum);
    {
        hset se = hset_init_fs(100);
        hset_normalize(&se);   // уменьшит до минимального размера
        test_validatefree(
            se.count == 0 && hset_validate(stdout, &se),
            hset_free(&se),
            "Normalized empty set should be valid"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: normalize non-empty FS set", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        int old_count = se.count;
        int old_sz = se.sz;
        hset_normalize(&se);
        // После нормализации размер должен быть меньше или равен исходному, но не менее чем нужно для хранения элементов
        test_validatefree(
            se.count == old_count &&
            se.sz <= old_sz &&
            hset_validate(stdout, &se) &&
            HSET_HAS_FS(&se, "/tmp/a") &&
            HSET_HAS_FS(&se, "/tmp/b") &&
            HSET_HAS_FS(&se, "/tmp/c"),
            hset_free(&se),
            "Normalized set must retain all elements and have optimized size"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: double normalize", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");
        hset_normalize(&se);
        hset_normalize(&se);   // повторная нормализация не должна ломать
        test_validatefree(
            hset_validate(stdout, &se) &&
            HSET_HAS_FS(&se, "/tmp/x") &&
            HSET_HAS_FS(&se, "/tmp/y"),
            hset_free(&se),
            "Double normalize should be safe"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: normalize after many insertions", ++subnum);
    {
        hset se = hset_init_fs(5);
        // Добавляем много элементов, чтобы вызвать автоматическое расширение
        for (int i = 0; i < 50; i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "/tmp/item%d", i);
            value64 v = value64_createfs_asstr(buf);
            hset_move(&se, &v);
        }
        int count = se.count;
        hset_normalize(&se);
        test_validatefree(
            se.count == count && hset_validate(stdout, &se),
            hset_free(&se),
            "Normalize after bulk insert must preserve all elements"
        );
        // Проверим выборочно несколько элементов
        test_validatefree(
            HSET_HAS_FS(&se, "/tmp/item0") &&
            HSET_HAS_FS(&se, "/tmp/item49"),
            hset_free(&se),
            "Elements must be found after normalize"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 20 ---------------------------------

static TestStatus
tf20(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Сохранить и загрузить пустое множество */
    test_sub("subtest %d: empty save/load", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_INT);

        int     save_ret = hset_save("res/hashset/empty.hset", &se);

        test_validatefree(
            save_ret >= 0,
            hset_free(&se),
            "Empty save failed, ret=%d", save_ret
        );

        hset    loaded = HSET_NONINIT;
        save_ret = hset_load("res/hashset/empty.hset", &loaded);

        test_validatefree(
            save_ret >= 0,
            hset_free(&se),
            "Empty save failed, ret=%d", save_ret
        );
        test_validatefree(
            hset_cnt(&loaded) == 0,
            (hset_free(&se), hset_free(&loaded)),
            "Empty set: cnt=%d, expected 0", hset_cnt(&loaded)
        );
        test_validatefree(
            hset_validate(logfile, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Empty set: validation failed"
        );
        test_validatefree(
            hset_eq(&se, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Empty set: not equal after load"
        );

        hset_free(&se);
        hset_free(&loaded);
    }

    /* 2. Сохранить и загрузить непустое множество (int) — создание нового */
    test_sub("subtest %d: int save/load new", ++subnum);
    {
        int     vals[] = {19, 21, 13, 4555, 5678};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        int     save_ret = hset_save("res/hashset/int.hset", &se);

        test_validatefree(
            save_ret == COUNT(vals),
            hset_free(&se),
            "Int save returned %d, expected %d", save_ret, COUNT(vals)
        );

        hset    loaded = HSET_NONINIT;
        int     load_ret = hset_load("res/hashset/int.hset", &loaded);

        test_validatefree(
            load_ret == COUNT(vals),
            (hset_free(&se), hset_free(&loaded)),
            "Int load returned %d, expected %d", load_ret, COUNT(vals)
        );
        test_validatefree(
            hset_cnt(&loaded) == COUNT(vals),
            (hset_free(&se), hset_free(&loaded)),
            "Int set: cnt=%d, expected %d", hset_cnt(&loaded), COUNT(vals)
        );
        test_validatefree(
            hset_validate(logfile, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Int set: validation failed"
        );
        test_validatefree(
            hset_eq(&se, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Int set: not equal after load"
        );

        hset_free(&se);
        hset_free(&loaded);
    }

    /* 3. Дозагрузка в существующее множество (int) */
    test_sub("subtest %d: int append load", ++subnum);
    {
        int     vals1[] = {10, 20};
        int     vals2[] = {30, 40};
        hset    se = hset_from_intarr(vals1, COUNT(vals1));

        hset    tmp = hset_from_intarr(vals2, COUNT(vals2));
        int     save_ret = hset_save("res/hashset/append_int.hset", &tmp);
        hset_free(&tmp);

        test_validatefree(
            save_ret == COUNT(vals2),
            hset_free(&se),
            "Append save returned %d, expected %d", save_ret, COUNT(vals2)
        );

        int     load_ret = hset_load("res/hashset/append_int.hset", &se);
        test_validatefree(
            load_ret == COUNT(vals2),
            hset_free(&se),
            "Append load returned %d, expected %d", load_ret, COUNT(vals2)
        );

        int     all_vals[] = {10, 20, 30, 40};
        hset    expected = hset_from_intarr(all_vals, COUNT(all_vals));

        test_validatefree(
            hset_cnt(&se) == COUNT(all_vals),
            (hset_free(&se), hset_free(&expected)),
            "Append: cnt=%d, expected %d", hset_cnt(&se), COUNT(all_vals)
        );
        test_validatefree(
            hset_validate(logfile, &se),
            (hset_free(&se), hset_free(&expected)),
            "Append: validation failed"
        );
        test_validatefree(
            hset_eq(&se, &expected),
            (hset_free(&se), hset_free(&expected)),
            "Append: not equal to expected"
        );

        hset_free(&se);
        hset_free(&expected);
    }

    /* 4. Несовпадение типов — возвращается -1, множество не меняется */
    test_sub("subtest %d: type mismatch returns -1", ++subnum);
    {
        hset    se_int = hset_init(10, VALUE64_INT);
        hset_save("res/hashset/type_int.hset", &se_int);

        hset    se_dbl = hset_init(10, VALUE64_DBL);
        int     ret = hset_load("res/hashset/type_int.hset", &se_dbl);

        test_validatefree(
            ret == -1,
            (hset_free(&se_int), hset_free(&se_dbl)),
            "Type mismatch: return code %d, expected -1", ret
        );
        test_validatefree(
            hset_cnt(&se_dbl) == 0,
            (hset_free(&se_int), hset_free(&se_dbl)),
            "Type mismatch: target set cnt=%d, expected 0 (unchanged)", hset_cnt(&se_dbl)
        );

        hset_free(&se_int);
        hset_free(&se_dbl);
    }

    /* 5. Сохранить и загрузить long множество */
    test_sub("subtest %d: long save/load new", ++subnum);
    {
        long    vals[] = {100L, 200L, 300L, 400L, 500L};
        hset    se = hset_from_longarr(vals, COUNT(vals));
        int     save_ret = hset_save("res/hashset/long.hset", &se);

        test_validatefree(
            save_ret == COUNT(vals),
            hset_free(&se),
            "Append save returned %d, expected %d", save_ret, COUNT(vals)
        );

        hset    loaded = HSET_NONINIT;
        int     load_ret = hset_load("res/hashset/long.hset", &loaded);

        test_validatefree(
            load_ret == COUNT(vals),
            hset_free(&se),
            "Append load returned %d, expected %d", load_ret, COUNT(vals)
        );

        test_validatefree(
            hset_cnt(&loaded) == COUNT(vals),
            (hset_free(&se), hset_free(&loaded)),
            "Long set: cnt=%d, expected %d", hset_cnt(&loaded), COUNT(vals)
        );
        test_validatefree(
            hset_validate(logfile, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Long set: validation failed"
        );
        test_validatefree(
            hset_eq(&se, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Long set: not equal after load"
        );

        hset_free(&se);
        hset_free(&loaded);
    }

    /* 6. Сохранить и загрузить double множество */
    test_sub("subtest %d: double save/load new", ++subnum);
    {
        double  vals[] = {1.1, 2.2, 3.3, 4.4, 5.5};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        int     save_ret = hset_save("res/hashset/double.hset", &se);

        test_validatefree(
            save_ret == COUNT(vals),
            hset_free(&se),
            "Append save returned %d, expected %d", save_ret, COUNT(vals)
        );

        hset    loaded = HSET_NONINIT;
        int     load_ret = hset_load("res/hashset/double.hset", &loaded);

        test_validatefree(
            load_ret == COUNT(vals),
            hset_free(&se),
            "Append load returned %d, expected %d", load_ret, COUNT(vals)
        );
        test_validatefree(
            hset_cnt(&loaded) == COUNT(vals),
            (hset_free(&se), hset_free(&loaded)),
            "Double set: cnt=%d, expected %d", hset_cnt(&loaded), COUNT(vals)
        );
        test_validatefree(
            hset_validate(logfile, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Double set: validation failed"
        );
        test_validatefree(
            hset_eq(&se, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Double set: not equal after load"
        );

        hset_free(&se);
        hset_free(&loaded);
    }
    // fs
    /* 1. Сохранить и загрузить пустое множество */
    test_sub("subtest %d: empty FS save/load", ++subnum);
    {
        hset se = hset_init_fs(10);
        int save_ret = hset_save("res/hashset/empty_fs.hset", &se);
        test_validatefree(
            save_ret >= 0,
            hset_free(&se),
            "Empty FS save failed, ret=%d", save_ret
        );

        hset loaded = HSET_NONINIT;
        int load_ret = hset_load("res/hashset/empty_fs.hset", &loaded);
        test_validatefree(
            load_ret >= 0,
            (hset_free(&se), hset_free(&loaded)),
            "Empty FS load failed, ret=%d", load_ret
        );
        test_validatefree(
            hset_cnt(&loaded) == 0,
            (hset_free(&se), hset_free(&loaded)),
            "Empty FS set: cnt=%d, expected 0", hset_cnt(&loaded)
        );
        test_validatefree(
            hset_validate(stdout, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Empty FS set: validation failed"
        );
        test_validatefree(
            hset_eq(&se, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "Empty FS set: not equal after load"
        );

        hset_free(&se);
        hset_free(&loaded);
    }
    fs_alloc_check(true);

    /* 2. Сохранить и загрузить непустое множество (FS) */
    test_sub("subtest %d: FS save/load new", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        int cnt = se.count;
        int save_ret = hset_save("res/hashset/fs.hset", &se);
        test_validatefree(
            save_ret == cnt,
            hset_free(&se),
            "FS save returned %d, expected %d", save_ret, cnt
        );

        hset loaded = HSET_NONINIT;
        int load_ret = hset_load("res/hashset/fs.hset", &loaded);
        test_validatefree(
            load_ret == cnt,
            (hset_free(&se), hset_free(&loaded)),
            "FS load returned %d, expected %d", load_ret, cnt
        );
        test_validatefree(
            hset_cnt(&loaded) == cnt,
            (hset_free(&se), hset_free(&loaded)),
            "FS set: cnt=%d, expected %d", hset_cnt(&loaded), cnt
        );
        test_validatefree(
            hset_validate(stdout, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "FS set: validation failed"
        );
        test_validatefree(
            hset_eq(&se, &loaded),
            (hset_free(&se), hset_free(&loaded)),
            "FS set: not equal after load"
        );
        // Проверяем конкретные элементы
        test_validatefree(
            HSET_HAS_FS(&loaded, "/tmp/a") &&
            HSET_HAS_FS(&loaded, "/tmp/b") &&
            HSET_HAS_FS(&loaded, "/tmp/c"),
            (hset_free(&se), hset_free(&loaded)),
            "FS set: some elements missing after load"
        );

        hset_free(&se);
        hset_free(&loaded);
    }
    fs_alloc_check(true);

    /* 3. Дозагрузка в существующее множество (FS) */
    test_sub("subtest %d: FS append load", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y");

        hset tmp = HSET_CREATEFS_ASSTR("/tmp/z", "/tmp/w");
        int save_ret = hset_save("res/hashset/append_fs.hset", &tmp);
        hset_free(&tmp);

        test_validatefree(
            save_ret == 2,
            hset_free(&se),
            "Append FS save returned %d, expected 2", save_ret
        );

        int load_ret = hset_load("res/hashset/append_fs.hset", &se);
        test_validatefree(
            load_ret == 2,
            hset_free(&se),
            "Append FS load returned %d, expected 2", load_ret
        );

        test_validatefree(
            hset_cnt(&se) == 4,
            hset_free(&se),
            "Append FS: cnt=%d, expected 4", hset_cnt(&se)
        );
        test_validatefree(
            hset_validate(stdout, &se),
            hset_free(&se),
            "Append FS: validation failed"
        );
        test_validatefree(
            HSET_HAS_FS(&se, "/tmp/x") &&
            HSET_HAS_FS(&se, "/tmp/y") &&
            HSET_HAS_FS(&se, "/tmp/z") &&
            HSET_HAS_FS(&se, "/tmp/w"),
            hset_free(&se),
            "Append FS: some elements missing"
        );

        hset_free(&se);
    }
    fs_alloc_check(true);

    /* 4. Несовпадение типов (FS vs INT) */
    test_sub("subtest %d: FS type mismatch returns -1", ++subnum);
    {
        hset se_fs = HSET_CREATEFS_ASSTR("/tmp/a");
        hset_save("res/hashset/type_fs.hset", &se_fs);
        hset_free(&se_fs);

        hset se_int = hset_init_int(10);
        int ret = hset_load("res/hashset/type_fs.hset", &se_int);

        test_validatefree(
            ret == -1,
            hset_free(&se_int),
            "FS type mismatch: return code %d, expected -1", ret
        );
        test_validatefree(
            hset_cnt(&se_int) == 0,
            hset_free(&se_int),
            "FS type mismatch: target set cnt=%d, expected 0 (unchanged)", hset_cnt(&se_int)
        );

        hset_free(&se_int);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 24 ---------------------------------
static TestStatus
tf24(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Уникальность NaN, +inf, -inf и корректность поиска */
    test_sub("subtest %d: double special values uniqueness", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_DBL);

        double  normal_vals[] = {1.0, 2.0, 3.0};
        for (int i = 0; i < COUNT(normal_vals); i++)
            hset_set(&se, LITERAL64_DBL(normal_vals[i]));

        int     cnt_normal = hset_cnt(&se);
        test_validatefree(
            cnt_normal == COUNT(normal_vals),
            hset_free(&se),
            "After normal values: count = %d, expected %d", cnt_normal, COUNT(normal_vals)
        );

        /* ---- NaN ---- */
        test_validatefree(
            hset_set(&se, LITERAL64_DBL(NAN)),
            hset_free(&se),
            "First NaN was not added"
        );
        test_validatefree(
            hset_cnt(&se) == cnt_normal + 1,
            hset_free(&se),
            "After first NaN: count = %d, expected %d", hset_cnt(&se), cnt_normal + 1
        );

        test_validatefree(
            !hset_set(&se, LITERAL64_DBL(NAN)),
            hset_free(&se),
            "Second NaN was added, but should be duplicate"
        );
        test_validatefree(
            hset_cnt(&se) == cnt_normal + 1,
            hset_free(&se),
            "After second NaN attempt: count = %d, expected %d", hset_cnt(&se), cnt_normal + 1
        );

        /* ---- +inf ---- */
        test_validatefree(
            hset_set(&se, LITERAL64_DBL(INFINITY)),
            hset_free(&se),
            "First +inf was not added"
        );
        test_validatefree(
            hset_cnt(&se) == cnt_normal + 2,
            hset_free(&se),
            "After first +inf: count = %d, expected %d", hset_cnt(&se), cnt_normal + 2
        );

        test_validatefree(
            !hset_set(&se, LITERAL64_DBL(INFINITY)),
            hset_free(&se),
            "Second +inf was added, but should be duplicate"
        );
        test_validatefree(
            hset_cnt(&se) == cnt_normal + 2,
            hset_free(&se),
            "After second +inf attempt: count = %d, expected %d", hset_cnt(&se), cnt_normal + 2
        );

        /* ---- -inf ---- */
        test_validatefree(
            hset_set(&se, LITERAL64_DBL(-INFINITY)),
            hset_free(&se),
            "First -inf was not added"
        );
        test_validatefree(
            hset_cnt(&se) == cnt_normal + 3,
            hset_free(&se),
            "After first -inf: count = %d, expected %d", hset_cnt(&se), cnt_normal + 3
        );

        test_validatefree(
            !hset_set(&se, LITERAL64_DBL(-INFINITY)),
            hset_free(&se),
            "Second -inf was added, but should be duplicate"
        );
        test_validatefree(
            hset_cnt(&se) == cnt_normal + 3,
            hset_free(&se),
            "After second -inf attempt: count = %d, expected %d", hset_cnt(&se), cnt_normal + 3
        );

        /* ---- поиск всех значений ---- */
        for (int i = 0; i < COUNT(normal_vals); i++) {
            test_validatefree(
                hset_get(&se, LITERAL64_DBL(normal_vals[i])),
                hset_free(&se),
                "Missing normal value %f", normal_vals[i]
            );
        }
        test_validatefree(
            hset_get(&se, LITERAL64_DBL(NAN)),
            hset_free(&se),
            "Missing NaN after insertions"
        );
        test_validatefree(
            hset_get(&se, LITERAL64_DBL(INFINITY)),
            hset_free(&se),
            "Missing +inf after insertions"
        );
        test_validatefree(
            hset_get(&se, LITERAL64_DBL(-INFINITY)),
            hset_free(&se),
            "Missing -inf after insertions"
        );

        for (int i = 0; i < 15; i++)
            hset_set(&se, LITERAL64_DBL(i) );
        HSET_TECH_PRINTALL(se);

        /* ---- структурная целостность ---- */
        test_validatefree(
            hset_validate(logfile, &se),
            hset_free(&se),
            "Validation failed after special values"
        );

        hset_free(&se);
    }

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 25 ---------------------------------

static TestStatus
tf25(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Сумма int через макрос */
    test_sub("subtest %d: macro sum int", ++subnum);
    {
        int     vals[] = {1, 2, 3, 4, 5, 6, 7, 8};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        //hset_techprint(&se, 0);
        int     sum = 0;
        HSET_FOREACH_INT(&se, v){
            //printf("%d\n", v);
            sum += v;
        }

        test_validatefree(
            sum == 36,
            hset_free(&se),
            "Macro sum int: sum = %d, expected 15", sum
        );
        hset_free(&se);
    }

    /* 2. Сбор элементов int в массив (проверяем, что все на месте) */
    test_sub("subtest %d: macro collect int", ++subnum);
    {
        int     vals[] = {3, 1, 4, 1, 5, 9, 2, 6};
        hset    se = hset_from_intarr(vals, COUNT(vals)); // дубликаты будут удалены
        int     expected[] = {1, 2, 3, 4, 5, 6, 9};   // уникальные отсортированные
        int     collected[COUNT(expected)];
        int     idx = 0;

        HSET_FOREACH_INT(&se, v) {
            if (idx < COUNT(expected))
                collected[idx++] = v;
        }

        test_validatefree(
            idx == COUNT(expected),
            hset_free(&se),
            "Macro collect int: collected %d elements, expected %d", idx, COUNT(expected)
        );
        // Сортируем, так как порядок обхода не гарантирован
        qsort(collected, idx, sizeof(int), pint_cmp);
        for (int i = 0; i < idx; i++) {
            test_validatefree(
                collected[i] == expected[i],
                hset_free(&se),
                "Macro collect int: collected[%d] = %d, expected %d", i, collected[i], expected[i]
            );
        }
        hset_free(&se);
    }

    /* 3. Проверка break: прерываем цикл при нахождении значения */
    test_sub("subtest %d: macro break int", ++subnum);
    {
        int     vals[] = {10, 20, 30, 40, 50};
        hset    se = hset_from_intarr(vals, COUNT(vals));
        int     found = 0;
        HSET_FOREACH_INT(&se, v) {
            if (v == 30) {
                found = 1;
                break;
            }
        }

        test_validatefree(
            found == 1,
            hset_free(&se),
            "Macro break int: value 30 %s found", found ? "" : "not"
        );
        hset_free(&se);
    }

    /* 4. Пустое множество: тело цикла не должно выполняться */
    test_sub("subtest %d: macro on empty set", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_INT);
        int     called = 0;
        HSET_FOREACH_INT(&se, v){
            called++;
            (void) v;
        }

        test_validatefree(
            called == 0,
            hset_free(&se),
            "Macro on empty set: body executed %d times, expected 0", called
        );
        hset_free(&se);
    }

    /* 5. Double сумма через макрос */
    test_sub("subtest %d: macro sum double", ++subnum);
    {
        double  vals[] = {1.1, 2.2, 3.3, 4.4, 5.5};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        double  sum = 0.0;
        HSET_FOREACH_DBL(&se, v)
            sum += v;

        test_validatefree(
            fabs(sum - 16.5) < 0.0001,
            hset_free(&se),
            "Macro sum double: sum = %f, expected 16.5", sum
        );
        hset_free(&se);
    }
    /* 6. Double break: прерываем цикл при нахождении значения */
    test_sub("subtest %d: macro break double", ++subnum);
    {
        double  vals[] = {1.1, 2.2, 3.3, 4.4, 5.5};
        hset    se = hset_from_dblarr(vals, COUNT(vals));
        int     found = 0;
        HSET_FOREACH_DBL(&se, v) {
            if (fabs(v - 3.3) < 0.0001) {
                found = 1;
                break;
            }
        }

        test_validatefree(
            found == 1,
            hset_free(&se),
            "Macro break double: value 3.3 %s found", found ? "" : "not"
        );
        hset_free(&se);
    }

    /* 7. Long сумма и break */
    test_sub("subtest %d: macro sum long", ++subnum);
    {
        long    vals[] = {100L, 200L, 300L, 400L, 500L};
        hset    se = hset_from_longarr(vals, COUNT(vals));
        long    sum = 0;
        HSET_FOREACH_LONG(&se, v) sum += v;

        test_validatefree(
            sum == 1500L,
            hset_free(&se),
            "Macro sum long: sum = %ld, expected 1500", sum
        );
        hset_free(&se);
    }

    /* 8. Long break */
    test_sub("subtest %d: macro break long", ++subnum);
    {
        long    vals[] = {10L, 20L, 30L, 40L, 50L};
        hset    se = hset_from_longarr(vals, COUNT(vals));
        int     found = 0;
        HSET_FOREACH_LONG(&se, v) {
            if (v == 30L) {
                found = 1;
                break;
            }
        }

        test_validatefree(
            found == 1,
            hset_free(&se),
            "Macro break long: value 30 %s found", found ? "" : "not"
        );
        hset_free(&se);
    }

    /* 9. Указатели: проверяем, что компилируется и не падает (содержимое не проверяем) */
    test_sub("subtest %d: macro foreach pointer", ++subnum);
    {
        // Создадим множество указателей с тремя элементами
        int     a = 1, b = 2, c = 3;
        void   *vals[] = {&a, &b, &c};
        hset    se = hset_from_ptrarr( (const void **) vals, COUNT(vals));
        logauto(se.count);
        int     count = 0;
        HSET_FOREACH_PTR(&se, v) {
            count++;
            // просто убедимся, что v не NULL
            test_validatefree(
                v != NULL,
                hset_free(&se),
                "Macro foreach pointer: NULL pointer at pos %d", count
            );
        }

        test_validatefree(
            count == COUNT(vals),
            hset_free(&se),
            "Macro foreach pointer: count = %d, expected %d", count, COUNT(vals)
        );
        hset_free(&se);
    }
    // fs
    test_sub("subtest %d: iterate over non-empty FS set", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/a", "/tmp/b", "/tmp/c");
        int count = 0;

        HSET_FOREACH_FS(&se, ptr) {
            test_validatefree(
                ptr != NULL,
                hset_free(&se),
                "Foreach FS: NULL pointer encountered"
            );
            count++;
        }

        test_validatefree(
            count == 3 && se.count == 3,
            hset_free(&se),
            "Foreach FS: counted %d elements, expected 3; set count %d",
            count, se.count
        );
        // Убедимся, что итерация не повредила данные
        test_validatefree(
            HSET_HAS_FS(&se, "/tmp/a") &&
            HSET_HAS_FS(&se, "/tmp/b") &&
            HSET_HAS_FS(&se, "/tmp/c"),
            hset_free(&se),
            "Foreach FS: elements missing after iteration"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: iterate over empty FS set", ++subnum);
    {
        hset se = hset_init_fs(10);
        int count = 0;

        HSET_FOREACH_FS(&se, ptr) {
            (void)ptr;
            count++;
        }

        test_validatefree(
            count == 0,
            hset_free(&se),
            "Foreach FS: empty set should iterate 0 times, got %d", count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: break inside foreach FS", ++subnum);
    {
        hset se = HSET_CREATEFS_ASSTR("/tmp/x", "/tmp/y", "/tmp/z");
        int count = 0;

        HSET_FOREACH_FS(&se, ptr) {
            count++;
            if (ptr && fs_str(ptr) && strcmp(fs_str(ptr), "/tmp/y") == 0)
                break;
        }

        test_validatefree(
            count > 0 && count <= 3,
            hset_free(&se),
            "Foreach FS with break: counted %d, expected 1..3", count
        );
        // Множество должно остаться целым
        test_validatefree(
            hset_validate(stdout, &se),
            hset_free(&se),
            "Foreach FS: validation failed after break"
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    test_sub("subtest %d: foreach after move (elements ownership ensured)", ++subnum);
    {
        hset se = hset_init_fs(10);
        // Добавим элементы через move
        value64 v1 = value64_createfs_asstr("/tmp/1");
        value64 v2 = value64_createfs_asstr("/tmp/2");
        hset_move(&se, &v1);
        hset_move(&se, &v2);

        int count = 0;
        HSET_FOREACH_FS(&se, ptr) {
            count++;
            test_validatefree(
                ptr != NULL && fs_str(ptr) != NULL,
                hset_free(&se),
                "Foreach after move: invalid element"
            );
        }

        test_validatefree(
            count == 2,
            hset_free(&se),
            "Foreach after move: counted %d, expected 2", count
        );
        hset_free(&se);
    }
    fs_alloc_check(true);

    return logret(TEST_PASSED, "done");
}

// ------------------------- TEST 27 ---------------------------------
static TestStatus
tf27(const char *name)
{
    logenter("%s", name);
    int subnum = 0;

    /* 1. Перемещение одной строки: оригинал теряет владение */
    test_sub("subtest %d: move single string", ++subnum);
    {
        hset        se = hset_init(10, VALUE64_FS);

        value64     tmp = value64_createfs_asstr("move_me");
        test_validatefree(
            hset_move(&se, &tmp),     // перемещаем!
            hset_free(&se),
            "Failed to insert via FSMOVE"
        );

        // После перемещения оригинал должен быть пуст (не владеть строкой)
        fs *s = value64_fs(tmp);        // get a fs *
        test_validatefree(
            s == NULL || (fs_len(s) == 0 && fs_str(s) == NULL),
            hset_free(&se),
            "After move, original fs must be empty (len=%d, str=%p)",
            fs_len(s), (void*)fs_str(s)
        );

        // Проверяем, что строка во множестве
        fs      search = fsliteral("move_me");
        test_validatefree(
            hset_get(&se, LITERAL64_FS(search)),
            hset_free(&se),
            "Moved string 'move_me' not found in set"
        );

        fs_free(s);      // оригинал пуст, но fsfree безопасен
        hset_free(&se);
    }
    fs_alloc_check(true);
    /* 2. Перемещение нескольких строк */
    test_sub("subtest %d: move multiple strings", ++subnum);
    {
        hset        se = hset_init(10, VALUE64_FS);
        const int   cnt = 20;
        fs          strings[cnt];

        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("str_%d", i);
            value64 tmp = value64_movefs(&strings[i]); // strings[i] опустошается

            test_validatefree(
                hset_move(&se, &tmp),
                hset_free(&se),
                "Failed to move 'str_%d'", i
            );
            // Оригинал должен быть пуст
            test_validatefree(
                fslen(strings[i]) == 0 && fsstr(strings[i]) == NULL,
                hset_free(&se),
                "After move %d, original must be empty", i
            );
        }

        // Проверяем количество
        test_validatefree(
            hset_cnt(&se) == cnt,
            hset_free(&se),
            "Count after moves: %d, expected %d", hset_cnt(&se), cnt
        );

        // Проверяем наличие всех строк
        for (int i = 0; i < cnt; i++) {
            fs search = fscopyf("str_%d", i);
            test_validatefree(
                hset_get(&se, LITERAL64_FS(search)),    // using LITERAL64_FS isn't good
                hset_free(&se),
                "Moved string 'str_%d' not found", i
            );
            fsfree(search);
        }

        // Освобождаем пустые оригиналы
        //for (int i = 0; i < cnt; i++)
        //    fsfree(strings[i]);
        hset_free(&se);
    }
    fs_alloc_check(true);
    /* 3. Попытка переместить уже перемещённый (пустой) fs
    DISABLE FOR NOW 7 jul
    test_sub("subtest %d: move already moved (empty) fs", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_FS);

        value64 tmp = value64_createfs_asstr("first_move");
        bool added_first = hset_move(&se, &tmp);

        test_validatefree(
            added_first == true && hset_cnt(&se) == 1,
            hset_free(&se),
            "Moving empty fs should return false"
        );
        value64 tmp2 = value64_movefs(value64_fs(tmp) );
        bool added_second = hset_move(&se, &tmp2);

        test_validatefree(
            added_second == false && hset_cnt(&se) == 1,
            hset_free(&se),
            "Moving empty fs should return false %d", hset_cnt(&se)
        );

        hset_free(&se);
    }
    fs_alloc_check(true); */
    /* 4. Перемещение с последующим поиском через копию */
    test_sub("subtest %d: move then search by copy", ++subnum);
    {
        hset    se = hset_init(10, VALUE64_FS);

        value64      orig = value64_createfs_asstr("search_me");
        hset_move(&se, &orig);

        // Поиск через копию (VALUE64_FS делает глубокую копию)
        value64      copy = value64_createfs_asstr("search_me");
        test_validatefree(
            hset_get(&se, copy),
            hset_free(&se),
            "Moved string should be found by copy"
        );

        value64_free(&copy, VALUE64_FS);
        // no need for orig
        hset_free(&se);
    }
    fs_alloc_check(true);
    // ADDITIONAL
    test_sub("subtest %d: move 5 strings and clone", ++subnum);
    {
        const int cnt = 5;
        // Массив локальных fs – они будут перемещены и опустеют
        fs      strings[cnt];
        const char *expected[] = { "alpha", "beta", "gamma", "delta", "epsilon" };

        // Создаём строки через fscopyf (все в куче, с FS_FLAG_ALLOC)
        for (int i = 0; i < cnt; i++) {
            strings[i] = fscopyf("%s", expected[i]);
        }

        // Множество, в которое будем перемещать
        hset    se = hset_init_fs(cnt);

        // Перемещаем каждую строку (оригиналы опустеют)
        for (int i = 0; i < cnt; i++) {
            value64 tmp = value64_movefs(&strings[i]);
            test_validatefree(
                hset_move(&se, &tmp) == true,
                hset_free(&se),
                "Failed to move '%s' into set", expected[i]
            );
            // После перемещения strings[i] не должен владеть строкой
            test_validatefree(
                fslen(strings[i]) == 0 && fsstr(strings[i]) == NULL,
                hset_free(&se),
                "After move, original must be empty, but len=%d, str=%p",
                fslen(strings[i]), (void *) fsstr(strings[i])
            );
        }

        // Клонируем множество (внутренние строки должны быть глубоко скопированы)
        hset    clone = hset_clone(&se);

        // Проверяем, что в клоне есть все строки
        fs      tmp = FS();
        for (int i = 0; i < cnt; i++) {
            fs_sprintf(&tmp, "%s", expected[i]);   // временный ключ для поиска
            test_validatefree(
                hset_get(&clone, LITERAL64_FS(tmp)),    // not good but OK for now
                (hset_free(&se), hset_free(&clone), fsfree(tmp)),
                "Clone missing '%s'", expected[i]
            );
        }
        fsfree(tmp);

        // Количество элементов должно совпадать
        test_validatefree(
            hset_cnt(&clone) == cnt,
            (hset_free(&se), hset_free(&clone)),
            "Clone count = %d, expected %d", hset_cnt(&clone), cnt
        );

        // Структурная целостность клона
        test_validatefree(
            hset_validate(logfile, &clone),
            (hset_free(&se), hset_free(&clone)),
            "Clone validation failed"
        );

        // Оригинальные strings не нужно освобождать – они пусты после перемещения
        // (fsfree для пустого fs безопасен, но не обязателен)
        hset_free(&se);
        hset_free(&clone);
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
                fprintf(stderr, "Invalid test num %d\n", num);
                continue;
            }
        }
        printf("Num %d\n", num);
            testenginestd_run(num,
                testnew(.f2 =  tf1,             .num =  1, .name = "Simple init and validate test"              , .desc="", .mandatory=true)
              , testnew(.f2 =  tf2,             .num =  2, .name = "Simple init and add test"                   , .desc="", .mandatory=true)
              , testnew(.f2 =  tf3,             .num =  3, .name = "Simple clone and create from array test"    , .desc="", .mandatory=true)
              , testnew(.f2 =  tf_loadfs_str,   .num =  4, .name = "Simple create fs from c-str array test"     , .desc="", .mandatory=true)
              , testnew(.f2 =  tf4,             .num =  5, .name = "Simple count test"                          , .desc="", .mandatory=true)
              , testnew(.f2 =  tf5,             .num =  6, .name = "Comparation simple test"                    , .desc="", .mandatory=true)
              , testnew(.f2 =  tf6,             .num =  7, .name = "Not equal simple test"                      , .desc="", .mandatory=true)
              , testnew(.f2 =  tf7,             .num =  8, .name = "Cloneas simple test"                        , .desc="", .mandatory=true)
              , testnew(.f2 = tf16,             .num = 17, .name = "hset_init_resize simple test"               , .desc="", .mandatory=true)
              , testnew(.f2 = tf19,             .num = 20, .name = "hset_normalize simple test"                 , .desc="", .mandatory=true)
              , testnew(.f2 = tf20,             .num = 21, .name = "hset_save/load simple test"                 , .desc="", .mandatory=true)
              , testnew(.f2 = tf24,             .num = 25, .name = "inf/nan double int simple test"             , .desc="", .mandatory=true)
              , testnew(.f2 = tf25,             .num = 26, .name = "Macro-base iterator simple test"            , .desc="", .mandatory=true)
              , testnew(.f2 = tf27,             .num = 28, .name = "value64_movefs() simple test"               , .desc="", .mandatory=true)
            );
        if (runall)
            break;
    }

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* HSETTESTING */

