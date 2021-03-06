/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * SECTION:zif-md-other-sql
 * @short_description: Other metadata
 *
 * Provide access to the other repo metadata.
 * This object is a subclass of #ZifMd
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <gio/gio.h>

#include "zif-changeset-private.h"
#include "zif-md.h"
#include "zif-md-other-sql.h"
#include "zif-package-remote.h"
#include "zif-state-private.h"

#define ZIF_MD_OTHER_SQL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ZIF_TYPE_MD_OTHER_SQL, ZifMdOtherSqlPrivate))

/**
 * ZifMdOtherSqlPrivate:
 *
 * Private #ZifMdOtherSql data
 **/
struct _ZifMdOtherSqlPrivate
{
	gboolean		 loaded;
	sqlite3			*db;
};

G_DEFINE_TYPE (ZifMdOtherSql, zif_md_other_sql, ZIF_TYPE_MD)

/**
 * zif_md_other_sql_unload:
 **/
static gboolean
zif_md_other_sql_unload (ZifMd *md, ZifState *state, GError **error)
{
	gboolean ret = FALSE;
	return ret;
}

/**
 * zif_md_other_sql_load:
 **/
static gboolean
zif_md_other_sql_load (ZifMd *md, ZifState *state, GError **error)
{
	const gchar *filename;
	gint rc;
	ZifMdOtherSql *other_sql = ZIF_MD_OTHER_SQL (md);

	g_return_val_if_fail (ZIF_IS_MD_OTHER_SQL (md), FALSE);
	g_return_val_if_fail (zif_state_valid (state), FALSE);

	/* already loaded */
	if (other_sql->priv->loaded)
		goto out;

	/* get filename */
	filename = zif_md_get_filename_uncompressed (md);
	if (filename == NULL) {
		g_set_error_literal (error, ZIF_MD_ERROR, ZIF_MD_ERROR_FAILED,
				     "failed to get filename for other_sql");
		goto out;
	}

	/* open database */
	zif_state_set_allow_cancel (state, FALSE);
	g_debug ("filename = %s", filename);
	rc = sqlite3_open (filename, &other_sql->priv->db);
	if (rc != 0) {
		g_warning ("Can't open database: %s\n", sqlite3_errmsg (other_sql->priv->db));
		g_set_error (error, ZIF_MD_ERROR, ZIF_MD_ERROR_BAD_SQL,
			     "can't open database: %s", sqlite3_errmsg (other_sql->priv->db));
		goto out;
	}

	/* we don't need to keep syncing */
	sqlite3_exec (other_sql->priv->db, "PRAGMA synchronous=OFF", NULL, NULL, NULL);
	other_sql->priv->loaded = TRUE;
out:
	return other_sql->priv->loaded;
}

/**
 * zif_md_other_sql_sqlite_create_changelog_cb:
 **/
static gint
zif_md_other_sql_sqlite_create_changelog_cb (void *data, gint argc, gchar **argv, gchar **col_name)
{
	GPtrArray *array = (GPtrArray *) data;
	ZifChangeset *changeset;
	gint i;
	guint64 date = 0;
	const gchar *author = NULL;
	const gchar *changelog = NULL;
	gboolean ret;
	GError *error = NULL;
	gchar *endptr = NULL;

	/* get the ID */
	for (i = 0; i < argc; i++) {
		if (g_strcmp0 (col_name[i], "date") == 0) {
			date = g_ascii_strtoull (argv[i], &endptr, 10);
			if (argv[i] == endptr)
				g_warning ("failed to parse date %s", argv[i]);
		} else if (g_strcmp0 (col_name[i], "author") == 0) {
			author = argv[i];
		} else if (g_strcmp0 (col_name[i], "changelog") == 0) {
			changelog = argv[i];
		} else {
			g_warning ("unrecognized: %s=%s", col_name[i], argv[i]);
		}
	}

	/* create new object */
	changeset = zif_changeset_new ();
	zif_changeset_set_date (changeset, date);
	zif_changeset_set_description (changeset, changelog);
	ret = zif_changeset_parse_header (changeset, author, &error);
	if (!ret) {
		g_warning ("failed to parse header: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add to the array */
	g_ptr_array_add (array, g_object_ref (changeset));
out:
	g_object_unref (changeset);
	return 0;
}

/**
 * zif_md_other_sql_search_pkgkey:
 **/
static GPtrArray *
zif_md_other_sql_search_pkgkey (ZifMdOtherSql *md, guint pkgkey,
			        ZifState *state, GError **error)
{
	gchar *statement = NULL;
	gchar *error_msg = NULL;
	gint rc;
	GPtrArray *array = NULL;

	g_return_val_if_fail (zif_state_valid (state), NULL);

	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	statement = g_strdup_printf ("SELECT author, date, changelog FROM changelog WHERE pkgKey = '%i' ORDER BY date DESC", pkgkey);
	rc = sqlite3_exec (md->priv->db, statement, zif_md_other_sql_sqlite_create_changelog_cb, array, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error, ZIF_MD_ERROR, ZIF_MD_ERROR_BAD_SQL,
			     "SQL error: %s\n", error_msg);
		sqlite3_free (error_msg);
		g_ptr_array_unref (array);
		array = NULL;
		goto out;
	}
out:
	g_free (statement);
	return array;
}

/**
 * zif_md_other_sql_sqlite_pkgkey_cb:
 **/
static gint
zif_md_other_sql_sqlite_pkgkey_cb (void *data, gint argc, gchar **argv, gchar **col_name)
{
	gint i;
	guint pkgkey;
	gchar *endptr = NULL;
	GPtrArray *array = (GPtrArray *) data;

	/* get the ID */
	for (i = 0; i < argc; i++) {
		if (g_strcmp0 (col_name[i], "pkgKey") == 0) {
			pkgkey = g_ascii_strtoull (argv[i], &endptr, 10);
			if (argv[i] == endptr)
				g_warning ("could not parse pkgKey '%s'", argv[i]);
			else
				g_ptr_array_add (array, GUINT_TO_POINTER (pkgkey));
		} else {
			g_warning ("unrecognized: %s=%s", col_name[i], argv[i]);
		}
	}
	return 0;
}

/**
 * zif_md_other_sql_get_changelog:
 **/
static GPtrArray *
zif_md_other_sql_get_changelog (ZifMd *md, const gchar *pkgid,
			        ZifState *state, GError **error)
{
	gchar *statement = NULL;
	gchar *error_msg = NULL;
	gint rc;
	gboolean ret;
	GError *error_local = NULL;
	GPtrArray *array = NULL;
	GPtrArray *array_tmp = NULL;
	GPtrArray *pkgkey_array = NULL;
	guint i, j;
	guint pkgkey;
	ZifState *state_local;
	ZifState *state_loop;
	ZifChangeset *changeset;
	ZifMdOtherSql *md_other_sql = ZIF_MD_OTHER_SQL (md);

	g_return_val_if_fail (zif_state_valid (state), NULL);

	/* setup steps */
	if (md_other_sql->priv->loaded) {
		ret = zif_state_set_steps (state,
					   error,
					   80, /* sql query */
					   20, /* search */
					   -1);
	} else {
		ret = zif_state_set_steps (state,
					   error,
					   60, /* load */
					   20, /* sql query */
					   20, /* search */
					   -1);
	}
	if (!ret)
		goto out;

	/* if not already loaded, load */
	if (!md_other_sql->priv->loaded) {
		state_local = zif_state_get_child (state);
		ret = zif_md_load (md, state_local, &error_local);
		if (!ret) {
			g_set_error (error, ZIF_MD_ERROR, ZIF_MD_ERROR_FAILED_TO_LOAD,
				     "failed to load md_other_sql file: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}

		/* this section done */
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
	}

	/* create data struct we can pass to the callback */
	zif_state_set_allow_cancel (state, FALSE);
	pkgkey_array = g_ptr_array_new ();
	statement = g_strdup_printf ("SELECT pkgKey FROM packages WHERE pkgId = '%s'", pkgid);
	rc = sqlite3_exec (md_other_sql->priv->db, statement, zif_md_other_sql_sqlite_pkgkey_cb, pkgkey_array, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error, ZIF_MD_ERROR, ZIF_MD_ERROR_BAD_SQL,
			     "SQL error: %s\n", error_msg);
		sqlite3_free (error_msg);
		goto out;
	}

	/* this section done */
	ret = zif_state_done (state, error);
	if (!ret)
		goto out;

	/* output array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* resolve each pkgkey to a package */
	state_local = zif_state_get_child (state);
	if (pkgkey_array->len > 0)
		zif_state_set_number_steps (state_local, pkgkey_array->len);
	for (i = 0; i < pkgkey_array->len; i++) {
		pkgkey = GPOINTER_TO_UINT (g_ptr_array_index (pkgkey_array, i));

		/* get changeset for pkgKey */
		state_loop = zif_state_get_child (state_local);
		array_tmp = zif_md_other_sql_search_pkgkey (md_other_sql, pkgkey, state_loop, error);
		if (array_tmp == NULL) {
			g_ptr_array_unref (array);
			array = NULL;
			goto out;
		}

		/* no results */
		if (array_tmp->len == 0)
			g_warning ("no changelog for pkgKey %i", pkgkey);
		for (j = 0; j < array_tmp->len; j++) {
			changeset = g_ptr_array_index (array_tmp, j);
			g_ptr_array_add (array, g_object_ref (changeset));
		}

		/* clear array */
		g_ptr_array_unref (array_tmp);

		/* this section done */
		ret = zif_state_done (state_local, error);
		if (!ret)
			goto out;
	}

	/* this section done */
	ret = zif_state_done (state, error);
	if (!ret)
		goto out;
out:
	g_free (statement);
	if (pkgkey_array != NULL)
		g_ptr_array_unref (pkgkey_array);
	return array;
}

/**
 * zif_md_other_sql_finalize:
 **/
static void
zif_md_other_sql_finalize (GObject *object)
{
	ZifMdOtherSql *md;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ZIF_IS_MD_OTHER_SQL (object));
	md = ZIF_MD_OTHER_SQL (object);

	sqlite3_close (md->priv->db);

	G_OBJECT_CLASS (zif_md_other_sql_parent_class)->finalize (object);
}

/**
 * zif_md_other_sql_class_init:
 **/
static void
zif_md_other_sql_class_init (ZifMdOtherSqlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ZifMdClass *md_class = ZIF_MD_CLASS (klass);
	object_class->finalize = zif_md_other_sql_finalize;

	/* map */
	md_class->load = zif_md_other_sql_load;
	md_class->unload = zif_md_other_sql_unload;
	md_class->get_changelog = zif_md_other_sql_get_changelog;
	g_type_class_add_private (klass, sizeof (ZifMdOtherSqlPrivate));
}

/**
 * zif_md_other_sql_init:
 **/
static void
zif_md_other_sql_init (ZifMdOtherSql *md)
{
	md->priv = ZIF_MD_OTHER_SQL_GET_PRIVATE (md);
	md->priv->loaded = FALSE;
	md->priv->db = NULL;
}

/**
 * zif_md_other_sql_new:
 *
 * Return value: A new #ZifMdOtherSql instance.
 *
 * Since: 0.1.0
 **/
ZifMd *
zif_md_other_sql_new (void)
{
	ZifMdOtherSql *md;
	md = g_object_new (ZIF_TYPE_MD_OTHER_SQL,
			   "kind", ZIF_MD_KIND_OTHER_SQL,
			   NULL);
	return ZIF_MD (md);
}

