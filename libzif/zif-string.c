/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:zif-string
 * @short_description: Create and manage reference counted strings
 *
 * To avoid frequent malloc/free, we use reference counted strings to
 * optimise many of the zif internals.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include "zif-utils.h"
#include "zif-string.h"

/* private structure */
typedef struct {
	gchar		*value;
	guint		 count;
} ZifStringInternal;

/**
 * zif_string_new:
 * @value: string to copy
 *
 * Creates a new referenced counted string
 *
 * Return value: New allocated object
 *
 * Since: 0.1.0
 **/
ZifString *
zif_string_new (const gchar *value)
{
	ZifStringInternal *string;
	string = g_new0 (ZifStringInternal, 1);
	string->count = 1;
	string->value = g_strdup (value);
	return (ZifString *) string;
}

/**
 * zif_string_new_value:
 * @value: string to use
 *
 * Creates a new referenced counted string, using the allocated memory.
 * Do not free this string as it is now owned by the #ZifString.
 *
 * Return value: New allocated object
 *
 * Since: 0.1.0
 **/
ZifString *
zif_string_new_value (gchar *value)
{
	ZifStringInternal *string;
	string = g_new0 (ZifStringInternal, 1);
	string->count = 1;
	string->value = value;
	return (ZifString *) string;
}

/**
 * zif_string_ref:
 * @string: the #ZifString object
 *
 * Increases the reference count on the object.
 *
 * Return value: the #ZifString object
 *
 * Since: 0.1.0
 **/
ZifString *
zif_string_ref (ZifString *string)
{
	ZifStringInternal *internal = (ZifStringInternal *) string;
	g_return_val_if_fail (internal != NULL, NULL);
	internal->count++;
	return string;
}

/**
 * zif_string_unref:
 * @string: the #ZifString object
 *
 * Decreses the reference count on the object, and frees the value if
 * it calls to zero.
 *
 * Return value: the #ZifString object
 *
 * Since: 0.1.0
 **/
ZifString *
zif_string_unref (ZifString *string)
{
	ZifStringInternal *internal = (ZifStringInternal *) string;
	g_return_val_if_fail (internal != NULL, NULL);
	internal->count--;
	if (internal->count == 0) {
		g_free (internal->value);
		g_free (internal);
		internal = NULL;
	}
	return (ZifString *) internal;
}

