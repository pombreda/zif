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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined (__ZIF_H_INSIDE__) && !defined (ZIF_COMPILATION)
#error "Only <zif.h> can be included directly."
#endif

#ifndef __ZIF_STRING_ARRAY_H
#define __ZIF_STRING_ARRAY_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct ZifStringArray ZifStringArray;

ZifStringArray	*zif_string_array_new		(const GPtrArray	*value);
ZifStringArray	*zif_string_array_new_value	(GPtrArray		*value);
ZifStringArray	*zif_string_array_ref		(ZifStringArray		*array);
ZifStringArray	*zif_string_array_unref		(ZifStringArray		*array);
void		 zif_string_array_add		(ZifStringArray		*array,
						 const gchar		*text);
void		 zif_string_array_add_value	(ZifStringArray		*array,
						 gchar			*text);
ZifStringArray	*zif_string_array_unique	(ZifStringArray		*array);
guint		 zif_string_array_get_length	(ZifStringArray		*array);
const gchar	*zif_string_array_get_value	(ZifStringArray		*array,
						 guint			 index);

G_END_DECLS

#endif /* __ZIF_STRING_ARRAY_H */

