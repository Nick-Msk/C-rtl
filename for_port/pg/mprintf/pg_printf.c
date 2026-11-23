#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PG_MODULE_MAGIC;

static void format_one_arg(const char *spec, const char *arg_text, char *out_buf, size_t out_size)
{
    char spec_type = spec[strlen(spec) - 1];
    switch (spec_type)
    {
        case 'd': case 'i':
        {
            long v = strtol(arg_text, NULL, 10);
            snprintf(out_buf, out_size, spec, v);
            break;
        }
        case 'u': case 'o': case 'x': case 'X':
        {
            unsigned long v = strtoul(arg_text, NULL, 10);
            snprintf(out_buf, out_size, spec, v);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
        {
            double v = strtod(arg_text, NULL);
            snprintf(out_buf, out_size, spec, v);
            break;
        }
        case 's':
            snprintf(out_buf, out_size, spec, arg_text);
            break;
        case 'c':
            snprintf(out_buf, out_size, spec, arg_text[0]);
            break;
        default:
            snprintf(out_buf, out_size, "%s", arg_text);
    }
}

PG_FUNCTION_INFO_V1(_printf_core);
PG_FUNCTION_INFO_V1(_printf_core2);

Datum
_printf_core(PG_FUNCTION_ARGS)
{
    char *fmt = text_to_cstring(PG_GETARG_TEXT_PP(0));
    int nargs = PG_NARGS();
    StringInfoData buf;
    const char *p = fmt;
    const char *lit = p;
    int idx = 1;

    initStringInfo(&buf);

    while (*p)
    {
        if (*p == '%')
        {
            appendBinaryStringInfo(&buf, lit, p - lit);

            if (*(p + 1) == '%')
            {
                appendStringInfoChar(&buf, '%');
                p += 2;
                lit = p;
                continue;
            }

            const char *spec_start = p;
            p++;
            while (*p && !strchr("diouxXeEfFgGaAcsCpmn", *p))
                p++;
            if (*p) p++;

            char spec[64];
            int spec_len = p - spec_start;
            if (spec_len >= sizeof(spec)) spec_len = sizeof(spec) - 1;
            memcpy(spec, spec_start, spec_len);
            spec[spec_len] = '\0';

            if (idx < nargs)
            {
                Oid argtypid = get_fn_expr_argtype(fcinfo->flinfo, idx);
                Datum d = PG_GETARG_DATUM(idx);
                Oid typoutput;
                bool typisvarlena;
                getTypeOutputInfo(argtypid, &typoutput, &typisvarlena);
                char *str = OidOutputFunctionCall(typoutput, d);

				/*ereport(NOTICE,
                	(errmsg("arg %d: typid=%u, str='%s'", idx, argtypid, str ? str : "(null)")));
				ereport(NOTICE,
					(errmsg("spec='%s', str='%s'", spec, str))); */

                char tmp[256];
                format_one_arg(spec, str, tmp, sizeof(tmp));
			 	//snprintf(tmp, sizeof(tmp), "%s", str); 
				//ereport(NOTICE, (errmsg("tmp='%s'", tmp))); 

                appendStringInfoString(&buf, tmp);
                pfree(str);
                idx++;
            }
            else
            {
                appendStringInfoString(&buf, spec);
            }

            lit = p;
        }
        else
            p++;
    }

    appendBinaryStringInfo(&buf, lit, p - lit);

    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

Datum
_printf_core2(PG_FUNCTION_ARGS)
{
    char *fmt = text_to_cstring(PG_GETARG_TEXT_PP(0));
    int nargs = PG_NARGS();   // общее количество аргументов, включая формат
    char **arg_strs = NULL;
    int i;

    // 1. Преобразуем все аргументы в строки заранее (один раз, чтобы не вызывать OidOutputFunctionCall дважды)
    if (nargs > 1)
    {
        arg_strs = palloc(sizeof(char *) * (nargs - 1));
        for (i = 1; i < nargs; i++)
        {
            Oid argtypid = get_fn_expr_argtype(fcinfo->flinfo, i);
            Datum d = PG_GETARG_DATUM(i);
            Oid typoutput;
            bool typisvarlena;
            getTypeOutputInfo(argtypid, &typoutput, &typisvarlena);
            arg_strs[i - 1] = OidOutputFunctionCall(typoutput, d);
        }
    }

    // 2. Первый проход: вычисляем общую длину результата
    size_t total_len = 0;
    const char *p = fmt;
    const char *lit = p;
    int idx = 0;   // индекс в arg_strs

    while (*p)
    {
        if (*p == '%')
        {
            // литерал до '%'
            total_len += (p - lit);

            if (*(p + 1) == '%')
            {
                total_len++;   // один '%'
                p += 2;
                lit = p;
                continue;
            }

            // извлекаем спецификатор
            const char *spec_start = p;
            p++;
            while (*p && !strchr("diouxXeEfFgGaAcsCpmn", *p))
                p++;
            if (*p) p++;
            int spec_len = p - spec_start;
            char spec[64];
            if (spec_len >= sizeof(spec)) spec_len = sizeof(spec) - 1;
            memcpy(spec, spec_start, spec_len);
            spec[spec_len] = '\0';

            // если есть аргумент, вычисляем длину отформатированного значения
            if (idx < nargs - 1)
            {
                char tmp[256];
                format_one_arg(spec, arg_strs[idx], tmp, sizeof(tmp));
                total_len += strlen(tmp);
                idx++;
            }
            else
            {
                // нет аргумента — вставим сам спецификатор (как есть)
                total_len += spec_len;
            }

            lit = p;
        }
        else
        {
            p++;
        }
    }
    // оставшийся литерал
    total_len += (p - lit);

    // 3. Выделяем буфер точного размера
    char *result = palloc(total_len + 1);
    char *out = result;

    // 4. Второй проход: заполняем буфер
    p = fmt;
    lit = p;
    idx = 0;

    while (*p)
    {
        if (*p == '%')
        {
            // копируем литерал
            if (p > lit)
            {
                memcpy(out, lit, p - lit);
                out += (p - lit);
            }

            if (*(p + 1) == '%')
            {
                *out++ = '%';
                p += 2;
                lit = p;
                continue;
            }

            // извлекаем спецификатор (повторно, но это быстро)
            const char *spec_start = p;
            p++;
            while (*p && !strchr("diouxXeEfFgGaAcsCpmn", *p))
                p++;
            if (*p) p++;
            int spec_len = p - spec_start;
            char spec[64];
            if (spec_len >= sizeof(spec)) spec_len = sizeof(spec) - 1;
            memcpy(spec, spec_start, spec_len);
            spec[spec_len] = '\0';

            if (idx < nargs - 1)
            {
                // форматируем прямо в result
                int written = snprintf(out, total_len - (out - result) + 1,
                                       "%s", "");   // временная заглушка, см. ниже
                // Удобнее: используем format_one_arg, которая вернёт длину? 
                // format_one_arg пишет в буфер, мы можем вызвать её во временный буфер, 
                // а потом скопировать. Но мы уже знаем длину из первого прохода.
                // Поступим проще: снова вызываем format_one_arg во временный буфер и копируем.
                char tmp[256];
                format_one_arg(spec, arg_strs[idx], tmp, sizeof(tmp));
                int len = strlen(tmp);
                memcpy(out, tmp, len);
                out += len;
                idx++;
            }
            else
            {
                memcpy(out, spec, spec_len);
                out += spec_len;
            }

            lit = p;
        }
        else
        {
            p++;
        }
    }

    // последний литерал
    if (p > lit)
    {
        memcpy(out, lit, p - lit);
        out += (p - lit);
    }

    // завершающий нуль
    *out = '\0';

    // освобождаем строки аргументов
    if (arg_strs)
    {
        for (i = 0; i < nargs - 1; i++)
            pfree(arg_strs[i]);
        pfree(arg_strs);
    }

    PG_RETURN_TEXT_P(cstring_to_text(result));
}


