/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2011 Richard Hughes <richard@hughsie.com>
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

#if !defined (__ZIF_H_INSIDE__) && !defined (ZIF_COMPILATION)
#error "Only <zif.h> can be included directly."
#endif

#ifndef __ZIF_STORE_REMOTE_PRIVATE_H
#define __ZIF_STORE_REMOTE_PRIVATE_H

#include "zif-md.h"
#include "zif-store-remote.h"

G_BEGIN_DECLS

GPtrArray	*zif_store_remote_get_files		(ZifStoreRemote		*store,
							 ZifPackage		*package,
							 ZifState		*state,
							 GError			**error);
GPtrArray	*zif_store_remote_get_provides		(ZifStoreRemote		*store,
							 ZifPackage		*package,
							 ZifState		*state,
							 GError			**error);
GPtrArray	*zif_store_remote_get_requires		(ZifStoreRemote		*store,
							 ZifPackage		*package,
							 ZifState		*state,
							 GError			**error);
GPtrArray	*zif_store_remote_get_conflicts		(ZifStoreRemote		*store,
							 ZifPackage		*package,
							 ZifState		*state,
							 GError			**error);
GPtrArray	*zif_store_remote_get_obsoletes		(ZifStoreRemote		*store,
							 ZifPackage		*package,
							 ZifState		*state,
							 GError			**error);
gboolean	 zif_store_remote_download_repomd	(ZifStoreRemote		*store,
							 ZifState		*state,
							 GError			**error);
const gchar	*zif_store_remote_get_local_directory	(ZifStoreRemote		*store);
ZifMd		*zif_store_remote_get_md_from_type	(ZifStoreRemote		*store,
							 ZifMdKind		 type);

G_END_DECLS

#endif /* __ZIF_STORE_REMOTE_PRIVATE_H */

