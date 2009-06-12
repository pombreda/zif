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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>

#include "zif-config.h"
#include "zif-store-remote.h"
#include "zif-repos.h"
#include "zif-utils.h"
#include "zif-monitor.h"

#include "egg-debug.h"
#include "egg-string.h"

#define ZIF_REPOS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ZIF_TYPE_REPOS, ZifReposPrivate))

struct ZifReposPrivate
{
	gboolean		 loaded;
	gchar			*repos_dir;
	ZifMonitor		*monitor;
	GPtrArray		*list;
	GPtrArray		*enabled;
};

G_DEFINE_TYPE (ZifRepos, zif_repos, G_TYPE_OBJECT)
static gpointer zif_repos_object = NULL;

/**
 * zif_repos_set_repos_dir:
 **/
gboolean
zif_repos_set_repos_dir (ZifRepos *repos, const gchar *repos_dir, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (ZIF_IS_REPOS (repos), FALSE);
	g_return_val_if_fail (repos->priv->repos_dir == NULL, FALSE);
	g_return_val_if_fail (!repos->priv->loaded, FALSE);
	g_return_val_if_fail (repos_dir != NULL, FALSE);

	/* check directory exists */
	ret = g_file_test (repos_dir, G_FILE_TEST_IS_DIR);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "repo directory %s does not exist", repos_dir);
		goto out;
	}

	/* setup watch */
	ret = zif_monitor_add_watch (repos->priv->monitor, repos_dir, &error_local);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to setup watch: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	repos->priv->repos_dir = g_strdup (repos_dir);
out:
	return ret;
}

/**
 * zif_repos_get_for_filename:
 **/
static gboolean
zif_repos_get_for_filename (ZifRepos *repos, const gchar *filename, GError **error)
{
	GError *error_local = NULL;
	GKeyFile *file;
	gchar **repos_groups = NULL;
	ZifStoreRemote *store;
	gboolean ret;
	gchar *path;
	guint i;

	/* find all the id's in this file */
	file = g_key_file_new ();
	path = g_build_filename (repos->priv->repos_dir, filename, NULL);
	ret = g_key_file_load_from_file (file, path, G_KEY_FILE_NONE, &error_local);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to load %s: %s", path, error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* for each group, add a store object */
	repos_groups = g_key_file_get_groups (file, NULL);
	for (i=0; repos_groups[i] != NULL; i++) {
		store = zif_store_remote_new ();
		ret = zif_store_remote_set_from_file (store, path, repos_groups[i], &error_local);
		if (!ret) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to set from %s: %s", path, error_local->message);
			g_error_free (error_local);
			break;
		}
		g_ptr_array_add (repos->priv->list, store);
	}
out:
	g_strfreev (repos_groups);
	g_free (path);
	g_key_file_free (file);
	return ret;
}

/**
 * zif_repos_load:
 **/
gboolean
zif_repos_load (ZifRepos *repos, GError **error)
{
	gboolean ret = TRUE;
	ZifStoreRemote *store;
	GError *error_local = NULL;
	GDir *dir;
	const gchar *filename;
	guint i;

	g_return_val_if_fail (ZIF_IS_REPOS (repos), FALSE);
	g_return_val_if_fail (repos->priv->repos_dir != NULL, FALSE);

	/* already loaded */
	if (repos->priv->loaded)
		goto out;

	/* search repos dir */
	dir = g_dir_open (repos->priv->repos_dir, 0, &error_local);
	if (dir == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to list directory: %s", error_local->message);
		g_error_free (error_local);
		ret = FALSE;
		goto out;
	}

	/* get all repo files */
	filename = g_dir_read_name (dir);
	while (filename != NULL) {
		if (g_str_has_suffix (filename, ".repo")) {

			/* setup watch */
			ret = zif_monitor_add_watch (repos->priv->monitor, filename, &error_local);
			if (!ret) {
				if (error != NULL)
					*error = g_error_new (1, 0, "failed to setup watch: %s", error_local->message);
				g_error_free (error_local);
				break;
			}

			/* add all repos for filename */
			ret = zif_repos_get_for_filename (repos, filename, &error_local);
			if (!ret) {
				if (error != NULL)
					*error = g_error_new (1, 0, "failed to get filename %s: %s", filename, error_local->message);
				g_error_free (error_local);
				g_ptr_array_foreach (repos->priv->list, (GFunc) g_object_unref, NULL);
				g_ptr_array_set_size (repos->priv->list, 0);
				ret = FALSE;
				break;
			}
		}
		filename = g_dir_read_name (dir);
	}
	g_dir_close (dir);

	/* we failed one file, abandon attempt */
	if (!ret)
		goto out;

	/* find enabled */
	for (i=0; i<repos->priv->list->len; i++) {
		store = g_ptr_array_index (repos->priv->list, i);

		/* get repo enabled state */
		ret = zif_store_remote_get_enabled (store, &error_local);
		if (error_local != NULL) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to get repo state for %s: %s", zif_store_get_id (ZIF_STORE (store)), error_local->message);
			g_ptr_array_foreach (repos->priv->enabled, (GFunc) g_object_unref, NULL);
			g_ptr_array_set_size (repos->priv->enabled, 0);
			ret = FALSE;
			goto out;
		}

		/* if enabled, add to array */
		if (ret)
			g_ptr_array_add (repos->priv->enabled, g_object_ref (store));
	}
	/* all loaded okay */
	repos->priv->loaded = TRUE;
	ret = TRUE;

out:
	return ret;
}

/**
 * zif_repos_get_stores:
 **/
GPtrArray *
zif_repos_get_stores (ZifRepos *repos, GError **error)
{
	GPtrArray *array = NULL;
	GError *error_local;
	ZifStoreRemote *store;
	gboolean ret;
	guint i;

	g_return_val_if_fail (ZIF_IS_REPOS (repos), FALSE);

	/* if not already loaded, load */
	if (!repos->priv->loaded) {
		ret = zif_repos_load (repos, &error_local);
		if (!ret) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to load repos: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}
	}

	/* make a copy */
	array = g_ptr_array_new ();
	for (i=0; i<repos->priv->list->len; i++) {
		store = g_ptr_array_index (repos->priv->list, i);
		g_ptr_array_add (array, g_object_ref (store));
	}
out:
	return array;
}

/**
 * zif_repos_get_stores_enabled:
 **/
GPtrArray *
zif_repos_get_stores_enabled (ZifRepos *repos, GError **error)
{
	GPtrArray *array = NULL;
	GError *error_local;
	ZifStoreRemote *store;
	gboolean ret;
	guint i;

	g_return_val_if_fail (ZIF_IS_REPOS (repos), FALSE);

	/* if not already loaded, load */
	if (!repos->priv->loaded) {
		ret = zif_repos_load (repos, &error_local);
		if (!ret) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to load enabled repos: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}
	}

	/* make a copy */
	array = g_ptr_array_new ();
	for (i=0; i<repos->priv->enabled->len; i++) {
		store = g_ptr_array_index (repos->priv->enabled, i);
		g_ptr_array_add (array, g_object_ref (store));
	}
out:
	return array;
}

/**
 * zif_repos_get_store:
 **/
ZifStoreRemote *
zif_repos_get_store (ZifRepos *repos, const gchar	*id, GError **error)
{
	guint i;
	ZifStoreRemote *store = NULL;
	ZifStoreRemote *store_tmp;
	const gchar *id_tmp;
	GError *error_local;
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_REPOS (repos), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	/* if not already loaded, load */
	if (!repos->priv->loaded) {
		ret = zif_repos_load (repos, &error_local);
		if (!ret) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to load repos: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}
	}

	/* search though all the cached repos */
	for (i=0; i<repos->priv->list->len; i++) {
		store_tmp = g_ptr_array_index (repos->priv->list, i);

		/* get the id */
		id_tmp = zif_store_get_id (ZIF_STORE (store_tmp));
		if (id_tmp == NULL) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to get id");
			goto out;
		}

		/* is it what we want? */
		if (strcmp (id_tmp, id) == 0) {
			store = g_object_ref (store_tmp);
			break;
		}
	}
out:
	return store;
}

/**
 * zif_repos_file_monitor_cb:
 **/
static void
zif_repos_file_monitor_cb (ZifMonitor *monitor, ZifRepos *repos)
{
	g_ptr_array_foreach (repos->priv->list, (GFunc) g_object_unref, NULL);
	g_ptr_array_foreach (repos->priv->enabled, (GFunc) g_object_unref, NULL);
	g_ptr_array_set_size (repos->priv->list, 0);
	g_ptr_array_set_size (repos->priv->enabled, 0);
	egg_debug ("repo file changed");
}

/**
 * zif_repos_finalize:
 **/
static void
zif_repos_finalize (GObject *object)
{
	ZifRepos *repos;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ZIF_IS_REPOS (object));
	repos = ZIF_REPOS (object);

	g_object_unref (repos->priv->monitor);
	g_free (repos->priv->repos_dir);

	g_ptr_array_foreach (repos->priv->list, (GFunc) g_object_unref, NULL);
	g_ptr_array_foreach (repos->priv->enabled, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (repos->priv->list, TRUE);
	g_ptr_array_free (repos->priv->enabled, TRUE);

	G_OBJECT_CLASS (zif_repos_parent_class)->finalize (object);
}

/**
 * zif_repos_class_init:
 **/
static void
zif_repos_class_init (ZifReposClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = zif_repos_finalize;
	g_type_class_add_private (klass, sizeof (ZifReposPrivate));
}

/**
 * zif_repos_init:
 **/
static void
zif_repos_init (ZifRepos *repos)
{
	repos->priv = ZIF_REPOS_GET_PRIVATE (repos);
	repos->priv->repos_dir = NULL;
	repos->priv->list = g_ptr_array_new ();
	repos->priv->enabled = g_ptr_array_new ();
	repos->priv->monitor = zif_monitor_new ();
	g_signal_connect (repos->priv->monitor, "changed", G_CALLBACK (zif_repos_file_monitor_cb), repos);
}

/**
 * zif_repos_new:
 * Return value: A new repos class instance.
 **/
ZifRepos *
zif_repos_new (void)
{
	if (zif_repos_object != NULL) {
		g_object_ref (zif_repos_object);
	} else {
		zif_repos_object = g_object_new (ZIF_TYPE_REPOS, NULL);
		g_object_add_weak_pointer (zif_repos_object, &zif_repos_object);
	}
	return ZIF_REPOS (zif_repos_object);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
zif_repos_test (EggTest *test)
{
	ZifStoreRemote *store;
	ZifConfig *config;
	ZifRepos *repos;
	GPtrArray *array;
	GError *error = NULL;
	const gchar *value;
	guint i;
	gchar *repos_dir;
	gboolean ret;

	if (!egg_test_start (test, "ZifRepos"))
		return;

	/* set this up as zifmy */
	config = zif_config_new ();
	zif_config_set_filename (config, "../test/etc/yum.conf", NULL);
	repos_dir = zif_config_get_string (config, "reposdir", NULL);

	/************************************************************/
	egg_test_title (test, "get repos");
	repos = zif_repos_new ();
	egg_test_assert (test, repos != NULL);

	/************************************************************/
	egg_test_title (test, "set repos dir %s", repos_dir);
	ret = zif_repos_set_repos_dir (repos, repos_dir, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to set repos dir '%s'", error->message);

	/************************************************************/
	egg_test_title (test, "get list of repos");
	array = zif_repos_get_stores (repos, &error);
	if (array != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to load metadata '%s'", error->message);

	/************************************************************/
	egg_test_title (test, "list correct length");
	if (array->len == 2)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect length %i", array->len);

	for (i=0; i<array->len; i++) {
		store = g_ptr_array_index (array, i);
		zif_store_print (ZIF_STORE (store));
	}

	g_ptr_array_foreach (array, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (array, TRUE);

	/************************************************************/
	egg_test_title (test, "get list of enabled repos");
	array = zif_repos_get_stores_enabled (repos, &error);
	if (array != NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to load metadata '%s'", error->message);

	/************************************************************/
	egg_test_title (test, "enabled correct length");
	if (array->len == 2)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect length %i", array->len);

	/* get ref for next test */
	store = g_object_ref (g_ptr_array_index (array, 0));
	g_ptr_array_foreach (array, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (array, TRUE);

	/************************************************************/
	egg_test_title (test, "get name");
	value = zif_store_remote_get_name (store, NULL);
	if (egg_strequal (value, "Fedora 10 - i386"))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid name '%s'", value);
	g_object_unref (store);

	g_object_unref (repos);
	g_object_unref (config);
	g_free (repos_dir);

	egg_test_end (test);
}
#endif
