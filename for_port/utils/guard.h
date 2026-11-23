#ifndef _GUARG_H
#define _GUARG_H

#include "error.h"

#ifndef NODEBUG

#define     RGUARDRAISE(M) userraiseint(ERR_GUARD_RAISE, "Guard raised %s:%s:%d (%s)\n", __FILE__, __func__, __LINE__, (M));

#define     GUARDK ({ static int    _guard_val = 1000;             _guard_val-- != 0; })
#define     GUARDM ({ static int    _guard_val = 1000000;          _guard_val-- != 0; })
#define     GUARDB ({ static int    _guard_val = 1000000000;       _guard_val-- != 0; })
#define     GUARDL ({ static long   _guard_val = 1000000000000;    _guard_val-- != 0; })

#define     RGUARDK ({ static int   _guard_val = 1000;             if(_guard_val-- == 0) RGUARDRAISE("K"); true; })
#define     RGUARDM ({ static int   _guard_val = 1000000;          if(_guard_val-- == 0) RGUARDRAISE("M"); true; })
#define     RGUARDB ({ static int   _guard_val = 1000000000;       if(_guard_val-- == 0) RGUARDRAISE("B"); true; })
#define     RGUARDL ({ static long  _guard_val = 1000000000000;    if(_guard_val-- == 0) RGUARDRAISE("T"); true; })

#define     FGUARDK(...) (({ static int  _guard_val = 1000;                      (_guard_val-- != 0) && (__VA_ARGS__); }))
#define     FGUARDM(...) (({ static int  _guard_val = 1000000;                   (_guard_val-- != 0) && (__VA_ARGS__); }))
#define     FGUARDB(...) (({ static int  _guard_val = 1000000000;                (_guard_val-- != 0) && (__VA_ARGS__); }))
#define     FGUARDL(...) (({ static long _guard_val = 1000000000000;;            (_guard_val-- != 0) && (__VA_ARGS__); }))

#define     FRGUARDK(...) (({ static int   _guard_val = 1000;             if(_guard_val-- == 0) RGUARDRAISE("K"); (__VA_ARGS__); }))
#define     FRGUARDM(...) (({ static int   _guard_val = 1000000;          if(_guard_val-- == 0) RGUARDRAISE("M"); (__VA_ARGS__); }))
#define     FRGUARDB(...) (({ static int   _guard_val = 1000000000;       if(_guard_val-- == 0) RGUARDRAISE("B"); (__VA_ARGS__); }))
#define     FRGUARDL(...) (({ static long  _guard_val = 1000000000000;    if(_guard_val-- == 0) RGUARDRAISE("T"); (__VA_ARGS__); }))

#else /* NODEBUG */

#define     GUARDK (true)
#define     GUARDM (true)
#define     GUARDB (true)
#define     GUARDL (true)

#define     RGUARDK (true)
#define     RGUARDM (true)
#define     RGUARDB (true)
#define     RGUARDL (true)

#define     FGUARDK(...) (__VA_ARGS__);
#define     FGUARDM(...) (__VA_ARGS__);
#define     FGUARDB(...) (__VA_ARGS__);
#define     FGUARDL(...) (__VA_ARGS__);

#define     FRGUARDK(...) (__VA_ARGS__);
#define     FRGUARDM(...) (__VA_ARGS__);
#define     FRGUARDB(...) (__VA_ARGS__);
#define     FRGUARDL(...) (__VA_ARGS__);

#endif /* !NODEBUG */

#define    SINGLETON(action)\
        ({ static bool _SINGLE_11 = true;\
            bool tmp = _SINGLE_11;\
          if (_SINGLE_11){\
                (action);\
                _SINGLE_11 = false;\
          }\
          tmp;\
        })

#define   MODEXEC(modval, action)\
        ({ static unsigned _MODVAL = 0;\
           bool res = (++_MODVAL % (modval) == 0);\
           if (res)\
               (action);\
            res;\
        })

#define   IFMOD(modval)\
            ({ static unsigned _MODVAL = 0;\
               ++_MODVAL % (modval) == 0; })

#define   MODEXECL(modval, action)\
        ({ static unsigned long _MODVALL = 0;\
           bool res = (++_MODVAL % (modval) == 0);\
           if (res)\
               (action);\
            res;\
        })

#define   IFMODL(modval)\
            ({ static unsigned long _MODVALL = 0;\
               ++_MODVAL % (modval) == 0; })

#endif /* !_GUARG_H */
