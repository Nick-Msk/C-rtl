 
#include "fs2slice.h"
#include <stdatomic.h>

 // mem leak checking
 static atomic_long              g_borrow_sum            = 0;

long
fs2slice_inc(void) {
    return atomic_fetch_add(&g_borrow_sum, 1); 
}

long
fs2slice_dec(void) {
    return atomic_fetch_add(&g_borrow_sum, -1); 
}

long 
fs2slice_sum(void) { 
    return atomic_load(&g_borrow_sum); 
}
