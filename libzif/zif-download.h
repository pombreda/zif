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

#ifndef __ZIF_DOWNLOAD_H
#define __ZIF_DOWNLOAD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ZIF_TYPE_DOWNLOAD		(zif_download_get_type ())
#define ZIF_DOWNLOAD(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), ZIF_TYPE_DOWNLOAD, ZifDownload))
#define ZIF_DOWNLOAD_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), ZIF_TYPE_DOWNLOAD, ZifDownloadClass))
#define ZIF_IS_DOWNLOAD(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), ZIF_TYPE_DOWNLOAD))
#define ZIF_IS_DOWNLOAD_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), ZIF_TYPE_DOWNLOAD))
#define ZIF_DOWNLOAD_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), ZIF_TYPE_DOWNLOAD, ZifDownloadClass))

typedef struct ZifDownloadPrivate ZifDownloadPrivate;

typedef struct
{
	GObject				 parent;
	ZifDownloadPrivate		*priv;
} ZifDownload;

typedef struct
{
	GObjectClass	parent_class;
	/* Signals */
	void		(* percentage_changed)		(ZifDownload	*download,
							 guint		 value);
	/* Padding for future expansion */
	void (*_zif_reserved1) (void);
	void (*_zif_reserved2) (void);
	void (*_zif_reserved3) (void);
	void (*_zif_reserved4) (void);
} ZifDownloadClass;

GType		 zif_download_get_type			(void) G_GNUC_CONST;
ZifDownload	*zif_download_new			(void);
gboolean	 zif_download_set_proxy			(ZifDownload		*download,
							 const gchar		*http_proxy,
							 GError			**error);
gboolean	 zif_download_file			(ZifDownload		*download,
							 const gchar		*uri,
							 const gchar		*filename,
							 GError			**error);
gboolean	 zif_download_cancel			(ZifDownload		*download,
							 GError			**error);

G_END_DECLS

#endif /* __ZIF_DOWNLOAD_H */
