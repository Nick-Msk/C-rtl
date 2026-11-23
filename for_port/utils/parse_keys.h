#ifndef _PARSE_KEY_H
#define _PARSE_KEY_H

#define                 parse_value(c, name, func)\
case c:\
    if (argv[0][1] == '\0') {\
        if (argv[1]){\
            ptr = argv[1];\
            argv++;\
        } else\
             return userraise(-1, ERR_WRONG_PARAMETER, "-"#c" option without value (must be string followed), ex '-lstring' or '-l string'");\
    } else\
        ptr = argv[0] + 1;\
    ke->name = func(ptr);\
    argv[0] += strlen(argv[0]) - 1;\
    params++;\
break;

// note - name must be a part of Keys structure, c must be char literal
#define                 parse_string(c, name) parse_value(c, name, )

// note - name must be a part of Keys structure, c must be char literal
#define                 parse_int(c, name) parse_value(c, name, atoi)

// note - name must be a part of Keys structure, c must be char literal
#define                 parse_long(c, name) parse_value(c, name, atol)

#define                 parse_bool(c, name)\
case c:\
     ke->name = true;\
     params++;\
break;

#define                 parse_bool_false(c, name)\
case c:\
     ke->name = false;\
     params++;\
break;

#endif /* !_PARSE_KEY_H */

