/*
 * Audacious
 * Copyright © 2009 William Pitcock <nenolod@atheme.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include <glib.h>
#include <mowgli.h>

#include "audstrings.h"

/*
 * Canonization mode:
 *
 * CASE_INSENSITIVE_CANON:  Store pooled strings in the tree in normalized case.
 *                          This is slightly slower than without, but has a few benefits.
 *                          Specifically, case is normalized in the tuples, and memory usage is
 *                          reduced further (due to more dupes being killed).
 *
 * NO_CANON:                Use fast binary-exact lookups.  Performance is slightly faster, but
 *                          less dupe reduction is done.
 *
 * TODO:                    make this runtime configurable.
 */

#undef CASE_INSENSITIVE_CANON
#ifndef CASE_INSENSITIVE_CANON
# define NO_CANON
#endif

#ifdef NO_CANON

static void
noopcanon(gchar *str)
{
    return;
}

#else

#ifdef XXX_UTF8_CANON

static void
strcasecanon(gchar *str)
{
    gchar *c, *up;

    c = g_utf8_casefold(str, -1);
    up = c;

    /* we have to ensure we don't overflow str. *grumble* */
    while (*str && *up)
        *str++ = *up++;

    if (*str && !*up)
        *str = '\0';

    g_free(c);
}

#else

static void
strcasecanon(gchar *str)
{
    while (*str)
    {
        /* toupper() should ignore utf8 data.  if not, make XXX_UTF8_CANON work. */
        *str = g_ascii_toupper(*str);
        str++;
    }
}

#endif
#endif

/* XXX: Ugly check to see if we actually have mowgli.patricia.  Remove this when
   we require at least Mowgli 0.7 to build. */
#ifdef __MOWGLI_PATRICIA_H__

/* structure to handle string refcounting. */
typedef struct {
    gint refcount;
    gchar *str;
} PooledString;

static mowgli_patricia_t *stringpool_tree = NULL;

/* allocate a string if needed. */
gchar *
stringpool_get(const gchar *str)
{
    PooledString *ps;

    g_return_val_if_fail(str != NULL, NULL);

    if (!*str)
        return NULL;

    if (stringpool_tree == NULL)
    {
#ifdef NO_CANON
        stringpool_tree = mowgli_patricia_create(noopcanon);
#else
        stringpool_tree = mowgli_patricia_create(strcasecanon);
#endif
    }

    if ((ps = mowgli_patricia_retrieve(stringpool_tree, str)) != NULL)
    {
        ps->refcount++;
        return ps->str;
    }

    ps = g_slice_new0(PooledString);
    ps->refcount++;
    ps->str = str_to_utf8(str);
    mowgli_patricia_add(stringpool_tree, str, ps);

    return ps->str;
}

void
stringpool_unref(const gchar *str)
{
    PooledString *ps;

    g_return_if_fail(stringpool_tree != NULL);
    g_return_if_fail(str != NULL);

    ps = mowgli_patricia_retrieve(stringpool_tree, str);
    if (ps != NULL && --ps->refcount <= 0)
    {
        mowgli_patricia_delete(stringpool_tree, str);
        g_free(ps->str);
        g_slice_free(PooledString, ps);
    }
}

#else

/* compatibility for libmowgli without a patricia class. */
gchar *
stringpool_get(gchar *str)
{
    return str_to_utf8(str);
}

void
stringpool_unref(gchar *str)
{
    g_free(str);
}

#endif
