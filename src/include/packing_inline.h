/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * Throughout this code we have to be aware of default argument conversion.
 *
 * Refer to Chapter 8 of "Expert C Programming" by Peter van der Linden for the gory details. The
 * short version is that we have less cases to deal with because the compiler promotes shorter types
 * to int or unsigned int.
 */
typedef struct {
    union {
        int64_t i;
        uint64_t u;
        const char *s;
        WT_ITEM item;
    } u;
    uint32_t size;
    int8_t havesize;
    char type;
} WT_PACK_VALUE;

/* Default to size = 1 if there is no size prefix. */
#define WT_PACK_VALUE_INIT \
    {                      \
        {0}, 1, 0, 0       \
    }
#define WT_DECL_PACK_VALUE(pv) WT_PACK_VALUE pv = WT_PACK_VALUE_INIT

typedef struct {
    WT_SESSION_IMPL *session;
    const char *cur, *end, *orig;
    unsigned long repeats;
    WT_PACK_VALUE lastv;
} WT_PACK;

#define WT_PACK_INIT                                  \
    {                                                 \
        NULL, NULL, NULL, NULL, 0, WT_PACK_VALUE_INIT \
    }
#define WT_DECL_PACK(pack) WT_PACK pack = WT_PACK_INIT

typedef struct {
    WT_CONFIG config;
    char buf[20];
    int count;
    bool iskey;
    int genname;
} WT_PACK_NAME;

/*
 * __pack_initn --
 *     Initialize a pack iterator with the specified string and length.
 */
static inline int
__pack_initn(WT_SESSION_IMPL *session, WT_PACK *pack, const char *fmt, size_t len)
{
    if (*fmt == '@' || *fmt == '<' || *fmt == '>')
        return (EINVAL);
    if (*fmt == '.') {
        ++fmt;
        if (len > 0)
            --len;
    }

    pack->session = session;
    pack->cur = pack->orig = fmt;
    pack->end = fmt + len;
    pack->repeats = 0;
    return (0);
}

/*
 * __pack_init --
 *     Initialize a pack iterator with the specified string.
 */
static inline int
__pack_init(WT_SESSION_IMPL *session, WT_PACK *pack, const char *fmt)
{
    return (__pack_initn(session, pack, fmt, strlen(fmt)));
}

/*
 * __pack_name_init --
 *     Initialize the name of a pack iterator.
 */
static inline void
__pack_name_init(WT_SESSION_IMPL *session, WT_CONFIG_ITEM *names, bool iskey, WT_PACK_NAME *pn)
{
    WT_CLEAR(*pn);
    pn->iskey = iskey;

    if (names->str != NULL)
        __wt_config_subinit(session, &pn->config, names);
    else
        pn->genname = 1;
}

/*
 * __pack_name_next --
 *     Get the next field type from a pack iterator.
 */
static inline int
__pack_name_next(WT_PACK_NAME *pn, WT_CONFIG_ITEM *name)
{
    WT_CONFIG_ITEM ignore;

    if (pn->genname) {
        WT_RET(
          __wt_snprintf(pn->buf, sizeof(pn->buf), (pn->iskey ? "key%d" : "value%d"), pn->count));
        WT_CLEAR(*name);
        name->str = pn->buf;
        name->len = strlen(pn->buf);
        /*
         * C++ treats nested structure definitions differently to C, as such we need to use scope
         * resolution to fully define the type.
         */
#ifdef __cplusplus
        name->type = WT_CONFIG_ITEM::WT_CONFIG_ITEM_STRING;
#else
        name->type = WT_CONFIG_ITEM_STRING;
#endif
        pn->count++;
    } else
        WT_RET(__wt_config_next(&pn->config, name, &ignore));

    return (0);
}

/*
 * __pack_next --
 *     Next pack iterator.
 */
static inline int
__pack_next(WT_PACK *pack, WT_PACK_VALUE *pv)
{
    char *endsize;

    if (pack->repeats > 0) {
        *pv = pack->lastv;
        --pack->repeats;
        return (0);
    }

next:
    if (pack->cur == pack->end)
        return (WT_NOTFOUND);

    if (__wt_isdigit((u_char)*pack->cur)) {
        pv->havesize = 1;
        pv->size = WT_STORE_SIZE(strtoul(pack->cur, &endsize, 10));
        pack->cur = endsize;
    } else {
        pv->havesize = 0;
        pv->size = 1;
    }

    pv->type = *pack->cur++;
    pack->repeats = 0;

    switch (pv->type) {
    case 'S':
        return (0);
    case 's':
        if (pv->size < 1)
            WT_RET_MSG(pack->session, EINVAL,
              "Fixed length strings must be at least 1 byte in format '%.*s'",
              (int)(pack->end - pack->orig), pack->orig);
        return (0);
    case 'x':
        return (0);
    case 't':
        if (pv->size < 1 || pv->size > 8)
            WT_RET_MSG(pack->session, EINVAL,
              "Bitfield sizes must be between 1 and 8 bits in format '%.*s'",
              (int)(pack->end - pack->orig), pack->orig);
        return (0);
    case 'u':
        /* Special case for items with a size prefix. */
        pv->type = (!pv->havesize && *pack->cur != '\0') ? 'U' : 'u';
        return (0);
    case 'U':
        /*
         * Don't change the type. 'U' is used internally, so this type was already changed to
         * explicitly include the size.
         */
        return (0);
    case 'b':
    case 'h':
    case 'i':
    case 'B':
    case 'H':
    case 'I':
    case 'l':
    case 'L':
    case 'q':
    case 'Q':
    case 'r':
    case 'R':
        /* Integral types repeat <size> times. */
        if (pv->size == 0)
            goto next;
        pv->havesize = 0;
        pack->repeats = pv->size - 1;
        pack->lastv = *pv;
        return (0);
    default:
        WT_RET_MSG(pack->session, EINVAL, "Invalid type '%c' found in format '%.*s'", pv->type,
          (int)(pack->end - pack->orig), pack->orig);
    }
}

#define WT_PACK_GET(session, pv, ap)                                                   \
    do {                                                                               \
        WT_ITEM *__item;                                                               \
        switch ((pv).type) {                                                           \
        case 'x':                                                                      \
            break;                                                                     \
        case 's':                                                                      \
        case 'S':                                                                      \
            (pv).u.s = va_arg(ap, const char *);                                       \
            break;                                                                     \
        case 'U':                                                                      \
        case 'u':                                                                      \
            __item = va_arg(ap, WT_ITEM *);                                            \
            (pv).u.item.data = __item->data;                                           \
            (pv).u.item.size = __item->size;                                           \
            break;                                                                     \
        case 'b':                                                                      \
        case 'h':                                                                      \
        case 'i':                                                                      \
        case 'l':                                                                      \
            /* Use the int type as compilers promote smaller sizes to int for variadic \
             * arguments.                                                              \
             * Note: 'l' accommodates 4 bytes                                          \
             */                                                                        \
            (pv).u.i = va_arg(ap, int);                                                \
            break;                                                                     \
        case 'B':                                                                      \
        case 'H':                                                                      \
        case 'I':                                                                      \
        case 'L':                                                                      \
        case 't':                                                                      \
            /* Use the int type as compilers promote smaller sizes to int for variadic \
             * arguments.                                                              \
             * Note: 'L' accommodates 4 bytes                                          \
             */                                                                        \
            (pv).u.u = va_arg(ap, unsigned int);                                       \
            break;                                                                     \
        case 'q':                                                                      \
            (pv).u.i = va_arg(ap, int64_t);                                            \
            break;                                                                     \
        case 'Q':                                                                      \
        case 'r':                                                                      \
        case 'R':                                                                      \
            (pv).u.u = va_arg(ap, uint64_t);                                           \
            break;                                                                     \
        default:                                                                       \
            /* User format strings have already been validated. */                     \
            return (__wt_illegal_value(session, (pv).type));                           \
        }                                                                              \
    } while (0)

/*
 * __pack_size --
 *     Get the size of a packed value.
 */
static inline int
__pack_size(WT_SESSION_IMPL *session, WT_PACK_VALUE *pv, size_t *vp)
{
    size_t s, pad;

    switch (pv->type) {
    case 'x':
        *vp = pv->size;
        return (0);
    case 'j':
    case 'J':
    case 'K':
        /* These formats are only used internally. */
        if (pv->type == 'j' || pv->havesize)
            s = pv->size;
        else {
            ssize_t len;

            /* The string was previously validated. */
            len = __wt_json_strlen((const char *)pv->u.item.data, pv->u.item.size);
            WT_ASSERT(session, len >= 0);
            s = (size_t)len + (pv->type == 'K' ? 0 : 1);
        }
        *vp = s;
        return (0);
    case 's':
    case 'S':
        if (pv->type == 's' || pv->havesize) {
            s = pv->size;
            WT_ASSERT(session, s != 0);
        } else
            s = strlen(pv->u.s) + 1;
        *vp = s;
        return (0);
    case 'U':
    case 'u':
        s = pv->u.item.size;
        pad = 0;
        if (pv->havesize && pv->size < s)
            s = pv->size;
        else if (pv->havesize)
            pad = pv->size - s;
        if (pv->type == 'U')
            s += __wt_vsize_uint(s + pad);
        *vp = s + pad;
        return (0);
    case 'b':
    case 'B':
    case 't':
        *vp = 1;
        return (0);
    case 'h':
    case 'i':
    case 'l':
    case 'q':
        *vp = __wt_vsize_int(pv->u.i);
        return (0);
    case 'H':
    case 'I':
    case 'L':
    case 'Q':
    case 'r':
        *vp = __wt_vsize_uint(pv->u.u);
        return (0);
    case 'R':
        *vp = sizeof(uint64_t);
        return (0);
    }

    WT_RET_MSG(session, EINVAL, "unknown pack-value type: %c", (int)pv->type);
}

/*
 * __pack_write --
 *     Pack a value into a buffer.
 */
static inline int
__pack_write(WT_SESSION_IMPL *session, WT_PACK_VALUE *pv, uint8_t **pp, size_t maxlen)
{
    size_t s, pad;
    uint8_t *oldp;

    switch (pv->type) {
    case 'x':
        WT_SIZE_CHECK_PACK(pv->size, maxlen);
        memset(*pp, 0, pv->size);
        *pp += pv->size;
        break;
    case 's':
        WT_SIZE_CHECK_PACK(pv->size, maxlen);
        memcpy(*pp, pv->u.s, pv->size);
        *pp += pv->size;
        break;
    case 'S':
        /*
         * When preceded by a size, that indicates the maximum number of bytes the string can store,
         * this does not include the terminating NUL character. In a string with characters less
         * than the specified size, the remaining bytes are NULL padded.
         */
        if (pv->havesize) {
            s = __wt_strnlen(pv->u.s, pv->size);
            pad = (s < pv->size) ? pv->size - s : 0;
        } else {
            s = strlen(pv->u.s);
            pad = 1;
        }
        WT_SIZE_CHECK_PACK(s + pad, maxlen);
        if (s > 0)
            memcpy(*pp, pv->u.s, s);
        *pp += s;
        if (pad > 0) {
            memset(*pp, 0, pad);
            *pp += pad;
        }
        break;
    case 'j':
    case 'J':
    case 'K':
        /* These formats are only used internally. */
        s = pv->u.item.size;
        if ((pv->type == 'j' || pv->havesize) && pv->size < s) {
            s = pv->size;
            pad = 0;
        } else if (pv->havesize)
            pad = pv->size - s;
        else if (pv->type == 'K')
            pad = 0;
        else
            pad = 1;
        if (s > 0) {
            oldp = *pp;
            WT_RET(__wt_json_strncpy(
              (WT_SESSION *)session, (char **)pp, maxlen, (const char *)pv->u.item.data, s));
            maxlen -= (size_t)(*pp - oldp);
        }
        if (pad > 0) {
            WT_SIZE_CHECK_PACK(pad, maxlen);
            memset(*pp, 0, pad);
            *pp += pad;
        }
        break;
    case 'U':
    case 'u':
        s = pv->u.item.size;
        pad = 0;
        if (pv->havesize && pv->size < s)
            s = pv->size;
        else if (pv->havesize)
            pad = pv->size - s;
        if (pv->type == 'U') {
            oldp = *pp;
            /*
             * Check that there is at least one byte available: the low-level routines treat zero
             * length as unchecked.
             */
            WT_SIZE_CHECK_PACK(1, maxlen);
            WT_RET(__wt_vpack_uint(pp, maxlen, s + pad));
            maxlen -= (size_t)(*pp - oldp);
        }
        WT_SIZE_CHECK_PACK(s + pad, maxlen);
        if (s > 0)
            memcpy(*pp, pv->u.item.data, s);
        *pp += s;
        if (pad > 0) {
            memset(*pp, 0, pad);
            *pp += pad;
        }
        break;
    case 'b':
        /* Translate to maintain ordering with the sign bit. */
        WT_SIZE_CHECK_PACK(1, maxlen);
        **pp = (uint8_t)(pv->u.i + 0x80);
        *pp += 1;
        break;
    case 'B':
    case 't':
        WT_SIZE_CHECK_PACK(1, maxlen);
        **pp = (uint8_t)pv->u.u;
        *pp += 1;
        break;
    case 'h':
    case 'i':
    case 'l':
    case 'q':
        /*
         * Check that there is at least one byte available: the low-level routines treat zero length
         * as unchecked.
         */
        WT_SIZE_CHECK_PACK(1, maxlen);
        WT_RET(__wt_vpack_int(pp, maxlen, pv->u.i));
        break;
    case 'H':
    case 'I':
    case 'L':
    case 'Q':
    case 'r':
        /*
         * Check that there is at least one byte available: the low-level routines treat zero length
         * as unchecked.
         */
        WT_SIZE_CHECK_PACK(1, maxlen);
        WT_RET(__wt_vpack_uint(pp, maxlen, pv->u.u));
        break;
    case 'R':
        WT_SIZE_CHECK_PACK(sizeof(uint64_t), maxlen);
        *(uint64_t *)*pp = pv->u.u;
        *pp += sizeof(uint64_t);
        break;
    default:
        WT_RET_MSG(session, EINVAL, "unknown pack-value type: %c", (int)pv->type);
    }

    return (0);
}

/*
 * __unpack_read --
 *     Read a packed value from a buffer.
 */
static inline int
__unpack_read(WT_SESSION_IMPL *session, WT_PACK_VALUE *pv, const uint8_t **pp, size_t maxlen)
{
    size_t s;

    switch (pv->type) {
    case 'x':
        WT_SIZE_CHECK_UNPACK(pv->size, maxlen);
        *pp += pv->size;
        break;
    case 's':
    case 'S':
        if (pv->type == 's' || pv->havesize) {
            s = pv->size;
            WT_ASSERT(session, s != 0);
        } else
            s = strlen((const char *)*pp) + 1;
        if (s > 0)
            pv->u.s = (const char *)*pp;
        WT_SIZE_CHECK_UNPACK(s, maxlen);
        *pp += s;
        break;
    case 'U':
        /*
         * Check that there is at least one byte available: the low-level routines treat zero length
         * as unchecked.
         */
        WT_SIZE_CHECK_UNPACK(1, maxlen);
        WT_RET(__wt_vunpack_uint(pp, maxlen, &pv->u.u));
    /* FALLTHROUGH */
    case 'u':
        if (pv->havesize)
            s = pv->size;
        else if (pv->type == 'U')
            s = (size_t)pv->u.u;
        else
            s = maxlen;
        WT_SIZE_CHECK_UNPACK(s, maxlen);
        pv->u.item.data = *pp;
        pv->u.item.size = s;
        *pp += s;
        break;
    case 'b':
        /* Translate to maintain ordering with the sign bit. */
        WT_SIZE_CHECK_UNPACK(1, maxlen);
        pv->u.i = (int8_t)(*(*pp)++ - 0x80);
        break;
    case 'B':
    case 't':
        WT_SIZE_CHECK_UNPACK(1, maxlen);
        pv->u.u = *(*pp)++;
        break;
    case 'h':
    case 'i':
    case 'l':
    case 'q':
        /*
         * Check that there is at least one byte available: the low-level routines treat zero length
         * as unchecked.
         */
        WT_SIZE_CHECK_UNPACK(1, maxlen);
        WT_RET(__wt_vunpack_int(pp, maxlen, &pv->u.i));
        break;
    case 'H':
    case 'I':
    case 'L':
    case 'Q':
    case 'r':
        /*
         * Check that there is at least one byte available: the low-level routines treat zero length
         * as unchecked.
         */
        WT_SIZE_CHECK_UNPACK(1, maxlen);
        WT_RET(__wt_vunpack_uint(pp, maxlen, &pv->u.u));
        break;
    case 'R':
        WT_SIZE_CHECK_UNPACK(sizeof(uint64_t), maxlen);
        pv->u.u = *(const uint64_t *)*pp;
        *pp += sizeof(uint64_t);
        break;
    default:
        WT_RET_MSG(session, EINVAL, "unknown pack-value type: %c", (int)pv->type);
    }

    return (0);
}

#define WT_UNPACK_PUT(session, pv, ap)                                              \
    do {                                                                            \
        WT_ITEM *__item;                                                            \
        switch ((pv).type) {                                                        \
        case 'x':                                                                   \
            break;                                                                  \
        case 's':                                                                   \
        case 'S':                                                                   \
            *va_arg(ap, const char **) = (pv).u.s;                                  \
            break;                                                                  \
        case 'U':                                                                   \
        case 'u':                                                                   \
            __item = va_arg(ap, WT_ITEM *);                                         \
            __item->data = (pv).u.item.data;                                        \
            __item->size = (pv).u.item.size;                                        \
            break;                                                                  \
        case 'b':                                                                   \
            *va_arg(ap, int8_t *) = (int8_t)(pv).u.i;                               \
            break;                                                                  \
        case 'h':                                                                   \
            *va_arg(ap, int16_t *) = (short)(pv).u.i;                               \
            break;                                                                  \
        case 'i':                                                                   \
        case 'l':                                                                   \
            *va_arg(ap, int32_t *) = (int32_t)(pv).u.i;                             \
            break;                                                                  \
        case 'q':                                                                   \
            *va_arg(ap, int64_t *) = (pv).u.i;                                      \
            break;                                                                  \
        case 'B':                                                                   \
        case 't':                                                                   \
            *va_arg(ap, uint8_t *) = (uint8_t)(pv).u.u;                             \
            break;                                                                  \
        case 'H':                                                                   \
            *va_arg(ap, uint16_t *) = (uint16_t)(pv).u.u;                           \
            break;                                                                  \
        case 'I':                                                                   \
        case 'L':                                                                   \
            *va_arg(ap, uint32_t *) = (uint32_t)(pv).u.u;                           \
            break;                                                                  \
        case 'Q':                                                                   \
        case 'r':                                                                   \
        case 'R':                                                                   \
            *va_arg(ap, uint64_t *) = (pv).u.u;                                     \
            break;                                                                  \
        default:                                                                    \
            __wt_err(session, EINVAL, "unknown unpack-put type: %c", (int)pv.type); \
            break;                                                                  \
        }                                                                           \
    } while (0)

/*
 * __wt_struct_packv --
 *     Pack a byte string (va_list version).
 */
static inline int
__wt_struct_packv(WT_SESSION_IMPL *session, void *buffer, size_t size, const char *fmt, va_list ap)
{
    WT_DECL_PACK_VALUE(pv);
    WT_DECL_RET;
    WT_PACK pack;
    uint8_t *p, *end;

    p = (uint8_t *)buffer;
    end = p + size;

    if (fmt[0] != '\0' && fmt[1] == '\0') {
        pv.type = fmt[0];
        WT_PACK_GET(session, pv, ap);
        return (__pack_write(session, &pv, &p, size));
    }

    WT_RET(__pack_init(session, &pack, fmt));
    while ((ret = __pack_next(&pack, &pv)) == 0) {
        WT_PACK_GET(session, pv, ap);
        WT_RET(__pack_write(session, &pv, &p, (size_t)(end - p)));
    }
    WT_RET_NOTFOUND_OK(ret);

    /* Be paranoid - __pack_write should never overflow. */
    WT_ASSERT(session, p <= end);

    return (0);
}

/*
 * __wt_struct_sizev --
 *     Calculate the size of a packed byte string (va_list version).
 */
static inline int
__wt_struct_sizev(WT_SESSION_IMPL *session, size_t *sizep, const char *fmt, va_list ap)
{
    WT_DECL_PACK_VALUE(pv);
    WT_DECL_RET;
    WT_PACK pack;
    size_t v;

    *sizep = 0;

    if (fmt[0] != '\0' && fmt[1] == '\0') {
        pv.type = fmt[0];
        WT_PACK_GET(session, pv, ap);
        return (__pack_size(session, &pv, sizep));
    }

    WT_RET(__pack_init(session, &pack, fmt));
    while ((ret = __pack_next(&pack, &pv)) == 0) {
        WT_PACK_GET(session, pv, ap);
        WT_RET(__pack_size(session, &pv, &v));
        *sizep += v;
    }
    WT_RET_NOTFOUND_OK(ret);

    return (0);
}

/*
 * __wt_struct_unpackv --
 *     Unpack a byte string (va_list version).
 */
static inline int
__wt_struct_unpackv(
  WT_SESSION_IMPL *session, const void *buffer, size_t size, const char *fmt, va_list ap)
{
    WT_DECL_PACK_VALUE(pv);
    WT_DECL_RET;
    WT_PACK pack;
    const uint8_t *p, *end;

    p = (uint8_t *)buffer;
    end = p + size;

    if (fmt[0] != '\0' && fmt[1] == '\0') {
        pv.type = fmt[0];
        WT_RET(__unpack_read(session, &pv, &p, size));
        WT_UNPACK_PUT(session, pv, ap);
        return (0);
    }

    WT_RET(__pack_init(session, &pack, fmt));
    while ((ret = __pack_next(&pack, &pv)) == 0) {
        WT_RET(__unpack_read(session, &pv, &p, (size_t)(end - p)));
        WT_UNPACK_PUT(session, pv, ap);
    }
    WT_RET_NOTFOUND_OK(ret);

    /* Be paranoid - __pack_write should never overflow. */
    WT_ASSERT(session, p <= end);

    return (0);
}

/*
 * __wt_struct_size_adjust --
 *     Adjust the size field for a packed structure. Sometimes we want to include the size as a
 *     field in a packed structure. This is done by calling __wt_struct_size with the expected
 *     format and a size of zero. Then we want to pack the structure using the final size. This
 *     function adjusts the size appropriately (taking into account the size of the final size or
 *     the size field itself).
 */
static inline void
__wt_struct_size_adjust(WT_SESSION_IMPL *session, size_t *sizep)
{
    size_t curr_size, field_size, prev_field_size;

    curr_size = *sizep;
    prev_field_size = 1;

    while ((field_size = __wt_vsize_uint(curr_size)) != prev_field_size) {
        curr_size += field_size - prev_field_size;
        prev_field_size = field_size;
    }

    /* Make sure the field size we calculated matches the adjusted size. */
    WT_ASSERT(session, field_size == __wt_vsize_uint(curr_size));

    *sizep = curr_size;
}


/*
 * A set of helpers for variadic macros
 */

#define WT_NARG(...)      WT_NARG_(__VA_ARGS__, WT_RSEQ_N())
#define WT_NARG_(...)     WT_ARG_N(__VA_ARGS__)
#define WT_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define WT_RSEQ_N() \
         63,62,61,60, \
         59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0

/* https://stackoverflow.com/questions/74728883/c-preprocessor-concatenate-macro-call-with-token */
#define WT_CONCAT(A, B)      WT_CONCAT_(A, B)
#define WT_CONCAT_(A, B)     A##B


/*
 * Packing functions
 */

/* H I L Q r -> uint */
#define TYPE_ALIAS__H   uint
#define TYPE_ALIAS__I   uint
#define TYPE_ALIAS__L   uint
#define TYPE_ALIAS__Q   uint
#define TYPE_ALIAS__r   uint
/* R -> int64 */
/*#define TYPE_ALIAS_R   int64*/
/* h i l q -> vint */
#define TYPE_ALIAS_h   vint
#define TYPE_ALIAS_i   vint
#define TYPE_ALIAS_l   vint
#define TYPE_ALIAS_q   vint
/* B t -> uint8 */
#define TYPE_ALIAS_B   uint8
#define TYPE_ALIAS_t   uint8
/* b -> int8 */
#define TYPE_ALIAS_b   int8
/* x -> skip {size} bytes */
/* s -> null-terminated C string */
#define TYPE_ALIAS_s   cstr
/* S -> size-bound C string */
/* u -> size-bound binary data */
/* U -> size=vuint; data[size] */


/*
 * __pack_size_* functions
 */

#define __pack_size__uint8_t(X)   (1)
#define __pack_size__uint16_t(X)  __wt_vsize_uint(*(X))
#define __pack_size__uint32_t(X)  __wt_vsize_uint(*(X))
#define __pack_size__uint64_t(X)  __wt_vsize_uint(*(X))
#define __pack_size__int8(X)      (1)
#define __pack_size__int16_t(X)   __wt_vsize_int(*(X))
#define __pack_size__int32_t(X)   __wt_vsize_int(*(X))
#define __pack_size__int64_t(X)   __wt_vsize_int(*(X))
/*#define __pack_size__cstr(X)    strlen(X) + 1*/
#define __pack_size__WT_ITEM(X)   (__wt_vsize_uint((X)->size) + (X)->size)

#define __pack_size__2(T1, V1)  __pack_size__##T1(V1)
#define __pack_size__4(T1, V1, T2, V2)  __pack_size__2(T1, V1) + __pack_size__2(T2, V2)
#define __pack_size__6(T1, V1, T2, V2, T3, V3)  __pack_size__2(T1, V1) + __pack_size__4(T2, V2, T3, V3)
#define __pack_size__8(T1, V1, T2, V2, T3, V3, T4, V4)  __pack_size__4(T1, V1, T2, V2) + __pack_size__4(T3, V3, T4, V4)
#define __pack_size__10(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5)  __pack_size__4(T1, V1, T2, V2) + __pack_size__6(T3, V3, T4, V4, T5, V5)
#define __pack_size__12(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6)  __pack_size__6(T1, V1, T2, V2, T3, V3) + __pack_size__6(T4, V4, T5, V5, T6, V6)
#define __pack_size__14(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7)  __pack_size__6(T1, V1, T2, V2, T3, V3) + __pack_size__8(T4, V4, T5, V5, T6, V6, T7, V7)
#define __pack_size__16(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8)  __pack_size__8(T1, V1, T2, V2, T3, V3, T4, V4) + __pack_size__8(T5, V5, T6, V6, T7, V7, T8, V8)
#define __pack_size__18(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9)  __pack_size__8(T1, V1, T2, V2, T3, V3, T4, V4) + __pack_size__10(T5, V5, T6, V6, T7, V7, T8, V8, T9, V9)
#define __pack_size__20(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9, T10, V10)  __pack_size__10(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5) + __pack_size__10(T6, V6, T7, V7, T8, V8, T9, V9, T10, V10)

#define __pack_size_direct(...)   (WT_CONCAT(__pack_size__, WT_NARG(__VA_ARGS__))(__VA_ARGS__))


/*
 * __pack_encode_* functions
 */

static inline int
__pack_encode__WT_ITEM(uint8_t **pp, size_t maxlen, WT_ITEM *item)
{
    uint8_t *end = *pp + maxlen;
    WT_RET(__wt_vpack_uint(pp, maxlen, item->size));
    WT_SIZE_CHECK_PACK(item->size, (size_t)(end - *pp));
    memcpy(*pp, item->data, item->size);
    *pp += item->size;
    return (0);
}

#define __pack_encode__uint8_t(pp, maxlen, x)   __wt_vpack_uint(pp, maxlen, *(x))
#define __pack_encode__uint16_t(pp, maxlen, x)  __wt_vpack_uint(pp, maxlen, *(x))
#define __pack_encode__uint32_t(pp, maxlen, x)  __wt_vpack_uint(pp, maxlen, *(x))
#define __pack_encode__uint64_t(pp, maxlen, x)  __wt_vpack_uint(pp, maxlen, *(x))
#define __pack_encode__int8_t(pp, maxlen, x)    __wt_vpack_int(pp, maxlen, *(x))
#define __pack_encode__int16_t(pp, maxlen, x)   __wt_vpack_int(pp, maxlen, *(x))
#define __pack_encode__int32_t(pp, maxlen, x)   __wt_vpack_int(pp, maxlen, *(x))
#define __pack_encode__int64_t(pp, maxlen, x)   __wt_vpack_int(pp, maxlen, *(x))


#define __pack_encode__2(p, end, T1, V1)  \
    /* Check that there is at least one byte available: the low-level routines treat zero length as unchecked. */ \
    WT_SIZE_CHECK_PACK(1, (size_t)(end-p)); \
    WT_RET(__pack_encode__##T1(&p, (size_t)(end - p), V1));
#define __pack_encode__4(p, end, T1, V1, T2, V2)  \
    __pack_encode__2(p, end, T1, V1); \
    __pack_encode__2(p, end, T2, V2);
#define __pack_encode__6(p, end, T1, V1, T2, V2, T3, V3)  \
    __pack_encode__2(p, end, T1, V1); \
    __pack_encode__4(p, end, T2, V2, T3, V3);
#define __pack_encode__8(p, end, T1, V1, T2, V2, T3, V3, T4, V4)  \
    __pack_encode__4(p, end, T1, V1, T2, V2); \
    __pack_encode__4(p, end, T3, V3, T4, V4);
#define __pack_encode__10(p, end, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5)  \
    __pack_encode__4(p, end, T1, V1, T2, V2); \
    __pack_encode__6(p, end, T3, V3, T4, V4, T5, V5);
#define __pack_encode__12(p, end, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6)  \
    __pack_encode__6(p, end, T1, V1, T2, V2, T3, V3); \
    __pack_encode__6(p, end, T4, V4, T5, V5, T6, V6);
#define __pack_encode__14(p, end, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7)  \
    __pack_encode__6(p, end, T1, V1, T2, V2, T3, V3); \
    __pack_encode__8(p, end, T4, V4, T5, V5, T6, V6, T7, V7);
#define __pack_encode__16(p, end, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8)  \
    __pack_encode__8(p, end, T1, V1, T2, V2, T3, V3, T4, V4); \
    __pack_encode__8(p, end, T5, V5, T6, V6, T7, V7, T8, V8);
#define __pack_encode__18(p, end, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9)  \
    __pack_encode__8(p, end, T1, V1, T2, V2, T3, V3, T4, V4); \
    __pack_encode__10(p, end, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9);
#define __pack_encode__20(p, end, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9, T10, V10)  \
    __pack_encode__10(p, end, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5); \
    __pack_encode__10(p, end, T6, V6, T7, V7, T8, V8, T9, V9, T10, V10);

#define __pack_encode_direct(p, end, ...)   do { WT_CONCAT(__pack_encode__, WT_NARG(__VA_ARGS__))(p, end, __VA_ARGS__) } while (0)


/*
 * __pack_decode_* functions
 */

#define __pack_decode__uintAny(pp, maxlen, TYPE, pval)  do { \
        uint64_t v; \
        /* Check that there is at least one byte available: the low-level routines treat zero length as unchecked. */ \
        WT_SIZE_CHECK_UNPACK(1, maxlen); \
        WT_RET(__wt_vunpack_uint(pp, (maxlen), &v)); \
        *pval = (TYPE)v; \
    } while (0)

#define __pack_decode__uint8_t    __pack_decode__uintAny
#define __pack_decode__uint16_t   __pack_decode__uintAny
#define __pack_decode__uint32_t   __pack_decode__uintAny
#define __pack_decode__uint64_t   __pack_decode__uintAny

#define __pack_decode__WT_ITEM(pp, maxlen, TYPE, pval)  do { \
        const uint8_t *end = *pp + maxlen; \
        __pack_decode__uintAny(pp, maxlen, size_t, &pval->size); \
        WT_SIZE_CHECK_UNPACK(pval->size, (size_t)(end - *pp)); \
        pval->data = *pp; \
        *pp += pval->size; \
    } while (0)

#define __pack_decode__2(pp, maxlen, T1, V1)  \
    /* Check that there is at least one byte available: the low-level routines treat zero length as unchecked. */ \
    WT_SIZE_CHECK_UNPACK(1, maxlen); \
    __pack_decode__##T1(pp, maxlen, T1, V1);
#define __pack_decode__4(p, maxlen, T1, V1, T2, V2)  \
    __pack_decode__2(p, maxlen, T1, V1); \
    __pack_decode__2(p, maxlen, T2, V2);
#define __pack_decode__6(p, maxlen, T1, V1, T2, V2, T3, V3)  \
    __pack_decode__2(p, maxlen, T1, V1); \
    __pack_decode__4(p, maxlen, T2, V2, T3, V3);
#define __pack_decode__8(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4)  \
    __pack_decode__4(p, maxlen, T1, V1, T2, V2); \
    __pack_decode__4(p, maxlen, T3, V3, T4, V4);
#define __pack_decode__10(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5)  \
    __pack_decode__4(p, maxlen, T1, V1, T2, V2); \
    __pack_decode__6(p, maxlen, T3, V3, T4, V4, T5, V5);
#define __pack_decode__12(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6)  \
    __pack_decode__6(p, maxlen, T1, V1, T2, V2, T3, V3); \
    __pack_decode__6(p, maxlen, T4, V4, T5, V5, T6, V6);
#define __pack_decode__14(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7)  \
    __pack_decode__6(p, maxlen, T1, V1, T2, V2, T3, V3); \
    __pack_decode__8(p, maxlen, T4, V4, T5, V5, T6, V6, T7, V7);
#define __pack_decode__16(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8)  \
    __pack_decode__8(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4); \
    __pack_decode__8(p, maxlen, T5, V5, T6, V6, T7, V7, T8, V8);
#define __pack_decode__18(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9)  \
    __pack_decode__8(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4); \
    __pack_decode__10(p, maxlen, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9);
#define __pack_decode__20(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9, T10, V10)  \
    __pack_decode__10(p, maxlen, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5); \
    __pack_decode__10(p, maxlen, T6, V6, T7, V7, T8, V8, T9, V9, T10, V10);

#define __pack_decode_direct(p, maxlen, ...)   do { WT_CONCAT(__pack_decode__, WT_NARG(__VA_ARGS__))(p, maxlen, __VA_ARGS__) } while (0)


/*
 * Macros for defining implementations of packing functions
 */

#define WT_NARG_FUNCARGS_2(T1, V1)  T1 *V1
#define WT_NARG_FUNCARGS_4(T1, V1, T2, V2)  T1 *V1, T2 *V2
#define WT_NARG_FUNCARGS_6(T1, V1, T2, V2, T3, V3)  T1 *V1, T2 *V2, T3 *V3
#define WT_NARG_FUNCARGS_8(T1, V1, T2, V2, T3, V3, T4, V4)  T1 *V1, T2 *V2, T3 *V3, T4 *V4
#define WT_NARG_FUNCARGS_10(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5)  T1 *V1, T2 *V2, T3 *V3, T4 *V4, T5 *V5
#define WT_NARG_FUNCARGS_12(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6)  T1 *V1, T2 *V2, T3 *V3, T4 *V4, T5 *V5, T6 *V6
#define WT_NARG_FUNCARGS_14(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7)  T1 *V1, T2 *V2, T3 *V3, T4 *V4, T5 *V5, T6 *V6, T7 *V7
#define WT_NARG_FUNCARGS_16(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8)  T1 *V1, T2 *V2, T3 *V3, T4 *V4, T5 *V5, T6 *V6, T7 *V7, T8 *V8
#define WT_NARG_FUNCARGS_18(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9)  T1 *V1, T2 *V2, T3 *V3, T4 *V4, T5 *V5, T6 *V6, T7 *V7, T8 *V8, T9 *V9
#define WT_NARG_FUNCARGS_20(T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8, T9, V9, T10, V10)  T1 *V1, T2 *V2, T3 *V3, T4 *V4, T5 *V5, T6 *V6, T7 *V7, T8 *V8, T9 *V9, T10 *V10

#define WT_NARG_FUNCARGS(...)   WT_CONCAT(WT_NARG_FUNCARGS_, WT_NARG(__VA_ARGS__))(__VA_ARGS__)


#define WT_DEFINE_PACKING(NAME, ...) \
    static inline int \
    __wt_size_##NAME(WT_SESSION_IMPL *session, size_t *sizep, WT_NARG_FUNCARGS(__VA_ARGS__)) \
    { \
        WT_UNUSED(session); \
        *sizep = __pack_size_direct(__VA_ARGS__); \
        return (0); \
    } \
    static inline int \
    __wt_pack_##NAME(WT_SESSION_IMPL *session, uint8_t *p, uint8_t *end, WT_NARG_FUNCARGS(__VA_ARGS__)) \
    { \
        WT_UNUSED(session); \
        __pack_encode_direct(p, end, __VA_ARGS__); \
        return (0); \
    } \
    static inline int \
    __wt_unpack_##NAME(WT_SESSION_IMPL *session, const uint8_t **pp, size_t size, WT_NARG_FUNCARGS(__VA_ARGS__)) \
    { \
        WT_UNUSED(session); \
        __pack_decode_direct(pp, size, __VA_ARGS__); \
        return (0); \
    }


/*
 * Specialized packing functions implementations
 */

WT_DEFINE_PACKING(system_record,  /* __wt_size_system_record(), __wt_pack_system_record(), __wt_unpack_system_record() */
    uint32_t, rectype)

WT_DEFINE_PACKING(checkpoint_start,  /* __wt_size_checkpoint_start(), __wt_pack_checkpoint_start(), __wt_unpack_checkpoint_start() */
    uint32_t, rectype, uint32_t, recsize)

WT_DEFINE_PACKING(commit,  /* __wt_size_commit(), __wt_pack_commit(), __wt_unpack_commit() */
    uint32_t, rectype, uint64_t, txnid)

WT_DEFINE_PACKING(file_sync,  /* __wt_size_file_sync(), __wt_pack_file_sync(), __wt_unpack_file_sync() */
    uint32_t, rectype, uint32_t, btree_id, uint32_t, checkpoint_start)

WT_DEFINE_PACKING(prev_lsn,  /* __wt_size_prev_lsn(), __wt_pack_prev_lsn(), __wt_unpack_prev_lsn() */
    uint32_t, rectype, uint32_t, recsize, uint32_t, file, uint32_t, offset)

WT_DEFINE_PACKING(col_remove,  /* __wt_size_col_remove(), __wt_pack_col_remove(), __wt_unpack_col_remove() */
    uint32_t, optype, uint32_t, recsize, uint32_t, fileid, uint64_t, recno)

WT_DEFINE_PACKING(row_remove,  /* __wt_size_row_remove(), __wt_pack_row_remove(), __wt_unpack_row_remove() */
    uint32_t, optype, uint32_t, recsize, uint32_t, fileid, WT_ITEM, key)

WT_DEFINE_PACKING(checkpoint,  /* __wt_size_checkpoint(), __wt_pack_checkpoint(), __wt_unpack_checkpoint() */
    uint32_t, rectype, uint32_t, file, uint32_t, offset, uint32_t, nsnapshot, WT_ITEM, snapshot)

WT_DEFINE_PACKING(col_truncate,  /* __wt_size_col_truncate(), __wt_pack_col_truncate(), __wt_unpack_col_truncate() */
    uint32_t, optype, uint32_t, recsize, uint32_t, fileid, uint64_t, start, uint64_t, stop)

WT_DEFINE_PACKING(col_put,  /* __wt_size_col_put(), __wt_pack_col_put(), __wt_unpack_col_put() */
    uint32_t, optype, uint32_t, recsize, uint32_t, fileid, uint64_t, recno, WT_ITEM, value)

WT_DEFINE_PACKING(row_put,  /* __wt_size_row_put(), __wt_pack_row_put(), __wt_unpack_row_put() */
    uint32_t, optype, uint32_t, recsize, uint32_t, fileid, WT_ITEM, key, WT_ITEM, value)

WT_DEFINE_PACKING(pack_row_truncate,  /* __wt_size_pack_row_truncate(), __wt_pack_pack_row_truncate(), __wt_unpack_pack_row_truncate() */
    uint32_t, optype, uint32_t, recsize, uint32_t, fileid, WT_ITEM, start,
    WT_ITEM, stop, uint32_t, mode)

WT_DEFINE_PACKING(txn_timestamp,  /* __wt_size_txn_timestamp(), __wt_pack_txn_timestamp(), __wt_unpack_txn_timestamp() */
    uint32_t, optype, uint32_t, recsize, uint64_t, time_sec, uint64_t, time_nsec, uint64_t, commit_ts,
    uint64_t, durable_ts, uint64_t, first_commit_ts, uint64_t, prepare_ts, uint64_t, read_ts)


