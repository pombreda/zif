/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

#ifndef __ZIF_PACKAGE_REMOTE_H
#define __ZIF_PACKAGE_REMOTE_H

#include <glib-object.h>

#include "zif-package.h"

G_BEGIN_DECLS

#define ZIF_TYPE_PACKAGE_REMOTE		(zif_package_remote_get_type ())
#define ZIF_PACKAGE_REMOTE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), ZIF_TYPE_PACKAGE_REMOTE, ZifPackageRemote))
#define ZIF_PACKAGE_REMOTE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), ZIF_TYPE_PACKAGE_REMOTE, ZifPackageRemoteClass))
#define ZIF_IS_PACKAGE_REMOTE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), ZIF_TYPE_PACKAGE_REMOTE))
#define ZIF_IS_PACKAGE_REMOTE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ZIF_TYPE_PACKAGE_REMOTE))
#define ZIF_PACKAGE_REMOTE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ZIF_TYPE_PACKAGE_REMOTE, ZifPackageRemoteClass))

typedef struct ZifPackageRemotePrivate ZifPackageRemotePrivate;

typedef struct
{
	ZifPackage		 parent;
	ZifPackageRemotePrivate	*priv;
} ZifPackageRemote;

typedef struct
{
	GObjectClass		 parent_class;
} ZifPackageRemoteClass;

GType			 zif_package_remote_get_type		(void) G_GNUC_CONST;
ZifPackageRemote	*zif_package_remote_new			(void);
gboolean		 zif_package_remote_set_from_repo	(ZifPackageRemote *pkg,
								 guint		 length,
								 gchar		**type,
								 gchar		**data,
								 const gchar	*repo_id,
								 GError		**error);

G_END_DECLS

#endif /* __ZIF_PACKAGE_REMOTE_H */
