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

#ifndef __ZIF_REPO_MD_H
#define __ZIF_REPO_MD_H

#include <glib-object.h>
#include <gio/gio.h>

#include "zif-repo-md.h"
#include "zif-completion.h"

G_BEGIN_DECLS

#define ZIF_TYPE_REPO_MD		(zif_repo_md_get_type ())
#define ZIF_REPO_MD(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ZIF_TYPE_REPO_MD, ZifRepoMd))
#define ZIF_REPO_MD_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ZIF_TYPE_REPO_MD, ZifRepoMdClass))
#define ZIF_IS_REPO_MD(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ZIF_TYPE_REPO_MD))
#define ZIF_IS_REPO_MD_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), ZIF_TYPE_REPO_MD))
#define ZIF_REPO_MD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ZIF_TYPE_REPO_MD, ZifRepoMdClass))

typedef struct _ZifRepoMd		ZifRepoMd;
typedef struct _ZifRepoMdPrivate	ZifRepoMdPrivate;
typedef struct _ZifRepoMdClass		ZifRepoMdClass;

struct _ZifRepoMd
{
	GObject			 parent;
	ZifRepoMdPrivate	*priv;
};

struct _ZifRepoMdClass
{
	GObjectClass				 parent_class;
	/* vtable */
	gboolean	 (*load)		(ZifRepoMd		*md,
						 GError			**error);
	gboolean	 (*clean)		(ZifRepoMd		*md,
						 GError			**error);
};

GType		 zif_repo_md_get_type			(void);
ZifRepoMd	*zif_repo_md_new			(void);

/* setters */
gboolean	 zif_repo_md_set_id			(ZifRepoMd	*md,
							 const gchar	*id);
gboolean	 zif_repo_md_set_filename		(ZifRepoMd	*md,
							 const gchar	*filename);

/* getters */
const gchar	*zif_repo_md_get_id			(ZifRepoMd	*md);
const gchar	*zif_repo_md_get_filename		(ZifRepoMd	*md);
const gchar	*zif_repo_md_get_filename_uncompressed	(ZifRepoMd	*md);
guint		 zif_repo_md_get_age			(ZifRepoMd	*md,
							 GError		**error);

/* actions */
gboolean	 zif_repo_md_load			(ZifRepoMd	*md,
							 GError		**error);
gboolean	 zif_repo_md_clean			(ZifRepoMd	*md,
							 GError		**error);

G_END_DECLS

#endif /* __ZIF_REPO_MD_H */

