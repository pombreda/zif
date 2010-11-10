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
 * SECTION:zif-manifest
 * @short_description: Parse and run .manifest files.
 *
 * A manifest file is a file that describes a transaction and optionally
 * details the pre and post system state.
 * It is used to verify results of #ZifTransaction.
 * A manifest file looks like:
 *
 * Zif Manifest
 * AddLocal=
 * AddRemote=hal
 * TransactionInstall=hal;0.0.1;i386;meta
 * PostInstalled=hal;0.0.1;i386;meta
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <libsoup/soup.h>

#include "zif-package-meta.h"
#include "zif-state.h"
#include "zif-store-array.h"
#include "zif-store-meta.h"
#include "zif-transaction.h"
#include "zif-manifest.h"
#include "zif-utils.h"

#define ZIF_MANIFEST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ZIF_TYPE_MANIFEST, ZifManifestPrivate))

/**
 * ZifManifestPrivate:
 *
 * Private #ZifManifest data
 **/
struct _ZifManifestPrivate
{
	gpointer		 padding;
};

typedef enum {
	ZIF_MANIFEST_ACTION_INSTALL,
	ZIF_MANIFEST_ACTION_UPDATE,
	ZIF_MANIFEST_ACTION_REMOVE
} ZifManifestAction;

G_DEFINE_TYPE (ZifManifest, zif_manifest, G_TYPE_OBJECT)

/**
 * zif_manifest_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.3
 **/
GQuark
zif_manifest_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("zif_manifest_error");
	return quark;
}


/**
 * zif_manifest_add_package_to_store:
 **/
static gboolean
zif_manifest_add_package_to_store (ZifManifest *manifest,
				   ZifStore *store,
				   ZifPackage *package,
				   GError **error)
{
	gboolean ret;
	GError *error_local = NULL;

	/* add to store */
	ret = zif_store_meta_add_package (ZIF_STORE_META (store), package, &error_local);
	if (!ret) {
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_POST_INSTALL,
			     "Failed to add package %s: %s",
			     zif_package_get_id (package),
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	return ret;
}

/**
 * zif_manifest_add_filename_to_store:
 **/
static gboolean
zif_manifest_add_filename_to_store (ZifManifest *manifest,
				   ZifStore *store,
				   const gchar *filename,
				   GError **error)
{
	ZifPackage *package;
	gboolean ret;

	/* create metapackage */
	package = zif_package_meta_new ();
	ret = zif_package_meta_set_from_filename (ZIF_PACKAGE_META (package), filename, error);
	if (!ret) {
		g_assert (error == NULL || *error != NULL);
		goto out;
	}

	/* add to store */
	ret = zif_manifest_add_package_to_store (manifest, store, package, error);
out:
	g_object_ref (package);
	return ret;
}

/**
 * zif_manifest_add_package_id_to_store:
 **/
static gboolean
zif_manifest_add_package_id_to_store (ZifManifest *manifest,
				      ZifStore *store,
				      const gchar *package_id,
				      GError **error)
{
	ZifPackage *package;
	gboolean ret;

	/* create metapackage */
	package = zif_package_meta_new ();
	ret = zif_package_set_id (package, package_id, error);
	if (!ret) {
		g_assert (error == NULL || *error != NULL);
		goto out;
	}

	/* add to store */
	ret = zif_manifest_add_package_to_store (manifest, store, package, error);
out:
	g_object_ref (package);
	return ret;
}


/**
 * zif_manifest_add_package_id_with_data_to_store:
 **/
static gboolean
zif_manifest_add_package_id_with_data_to_store (ZifManifest *manifest,
						 ZifStore *store,
						 const gchar *package_id,
						 gchar **extra_data,
						 GError **error)
{
	ZifPackage *package;
	gboolean ret;

	/* create metapackage */
	package = zif_package_meta_new ();
	ret = zif_package_set_id (package, package_id, error);
	if (!ret) {
		g_assert (error == NULL || *error != NULL);
		goto out;
	}

	/* add extra data */
	zif_package_meta_set_from_data (ZIF_PACKAGE_META (package), extra_data);

	/* add to store */
	ret = zif_manifest_add_package_to_store (manifest, store, package, error);
out:
	g_object_ref (package);
	return ret;
}

/**
 * zif_manifest_add_packages_to_store:
 **/
static gboolean
zif_manifest_add_packages_to_store (ZifManifest *manifest,
				    ZifStore *store,
				    const gchar *dirname,
				    const gchar *packages,
				    GError **error)
{
	guint i;
	gchar **split;
	gchar **data;
	gchar *filename;
	gboolean ret = TRUE;

	/* add each package */
	split = g_strsplit (packages, ",", -1);
	for (i=0; split[i] != NULL; i++) {

		/* specified as an package-id with data */
		if (g_strstr_len (split[i], -1, "@") != NULL) {
			data = g_strsplit (split[i], "@", -1);
			g_debug ("adding package-id %s", data[0]);
			ret = zif_manifest_add_package_id_with_data_to_store (manifest, store, data[0], data+1, error);
			if (!ret)
				goto out;
			continue;
		}

		/* specified as a package-id */
		if (zif_package_id_check (split[i])) {
			g_debug ("adding package-id %s", split[i]);
			ret = zif_manifest_add_package_id_to_store (manifest, store, split[i], error);
			if (!ret)
				goto out;
			continue;
		}

		/* specified as a filename */
		filename = g_strdup_printf ("%s/%s.spec", dirname, split[i]);
		ret = g_file_test (filename, G_FILE_TEST_EXISTS);
		if (ret) {
			g_debug ("adding file %s", filename);
			ret = zif_manifest_add_filename_to_store (manifest, store, filename, error);
			g_free (filename);
			if (!ret)
				break;
			continue;
		}

		/* not recognised */
		ret = FALSE;
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_POST_INSTALL,
			     "Failed to add invalid item %s",
			     split[i]);
		goto out;
	}
out:
	g_strfreev (split);
	return ret;
}

/**
 * zif_manifest_add_package_to_transaction:
 **/
static gboolean
zif_manifest_add_package_to_transaction (ZifManifest *manifest,
					 ZifTransaction *transaction,
					 ZifStore *store,
					 ZifManifestAction action,
					 const gchar *package_id,
					 GError **error)
{
	ZifPackage *package;
	gboolean ret = FALSE;
	ZifState *state;
	GError *error_local = NULL;

	/* get metapackage */
	state = zif_state_new ();
	package = zif_store_find_package (store, package_id, state, &error_local);
	if (package == NULL) {
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_POST_INSTALL,
			     "Failed to add package_id to transaction %s: %s",
			     package_id,
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* add it to the transaction */
	if (action == ZIF_MANIFEST_ACTION_INSTALL)
		ret = zif_transaction_add_install (transaction, package, &error_local);
	else if (action == ZIF_MANIFEST_ACTION_REMOVE)
		ret = zif_transaction_add_remove (transaction, package, &error_local);
	else if (action == ZIF_MANIFEST_ACTION_UPDATE)
		ret = zif_transaction_add_update (transaction, package, &error_local);
	if (!ret) {
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_POST_INSTALL,
			     "Failed to add package to transaction %s: %s",
			     zif_package_get_id (package),
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	g_object_unref (state);
	if (package != NULL)
		g_object_unref (package);
	return ret;
}

/**
 * zif_manifest_add_packages_to_transaction:
 **/
static gboolean
zif_manifest_add_packages_to_transaction (ZifManifest *manifest,
					  ZifTransaction *transaction,
					  ZifStore *store,
					  ZifManifestAction action,
					  const gchar *packages,
					  GError **error)
{
	guint i;
	gchar **split;
	gboolean ret = TRUE;

	/* add each package */
	split = g_strsplit (packages, ",", -1);
	for (i=0; split[i] != NULL; i++) {
		ret = zif_manifest_add_package_to_transaction (manifest, transaction, store, action, split[i], error);
		if (!ret)
			break;
	}
	g_strfreev (split);
	return ret;
}

/**
 * zif_manifest_check_post_installed:
 **/
static gboolean
zif_manifest_check_post_installed (ZifManifest *manifest,
				   ZifStore *store,
				   const gchar *packages,
				   GError **error)
{
	guint i;
	gboolean ret = TRUE;
	gchar **split;
	GPtrArray *array = NULL;
	ZifState *state;
	ZifPackage *package;
	GError *error_local = NULL;

	/* find each package */
	state = zif_state_new ();
	split = g_strsplit (packages, ",", -1);
	for (i=0; split[i] != NULL; i++) {
		zif_state_reset (state);
		package = zif_store_find_package (store, split[i], state, &error_local);
		if (package == NULL) {
			ret = FALSE;
			g_set_error (error,
				     ZIF_MANIFEST_ERROR,
				     ZIF_MANIFEST_ERROR_POST_INSTALL,
				     "Failed to find post-installed package %s: %s",
				     split[i],
				     error_local->message);
			g_error_free (error_local);
			goto out;
		}
		g_debug ("found %s", split[i]);
		g_object_unref (package);
	}

	/* ensure same size */
	zif_state_reset (state);
	array = zif_store_get_packages (store, state, &error_local);
	if (array == NULL) {
		ret = FALSE;
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_POST_INSTALL,
			     "Failed to get store packages: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
	if (g_strv_length (split) != array->len) {
		ret = FALSE;
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_POST_INSTALL,
			     "post install database wrong size %i when supposed to be %i",
			     array->len, g_strv_length (split));
		g_debug ("listing files in store");
		for (i=0; i<array->len; i++) {
			package = g_ptr_array_index (array, i);
			g_debug ("%i.\t%s", i+1, zif_package_get_id (package));
		}
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_strfreev (split);
	g_object_unref (state);
	return ret;
}

/**
 * zif_manifest_check:
 * @manifest: the #ZifManifest object
 * @filename: the maifest file to use
 * @error: a #GError which is used on failure, or %NULL
 *
 * Resolves and checks a transaction.
 *
 * Return value: %TRUE for success, %FALSE for failure
 *
 * Since: 0.1.3
 **/
gboolean
zif_manifest_check (ZifManifest *manifest,
				const gchar *filename,
				GError **error)
{
	gboolean added_something = FALSE;
	gboolean ret;
	gchar *dirname = NULL;
	gchar *packages_local = NULL;
	gchar *packages_remote = NULL;
	gchar *post_installed = NULL;
	gchar *transaction_install = NULL;
	gchar *transaction_remove = NULL;
	gchar *transaction_update = NULL;
	GError *error_local = NULL;
	GKeyFile *keyfile;
	GPtrArray *remote_array = NULL;
	GPtrArray *resolve_install = NULL;
	GPtrArray *resolve_remove = NULL;
	ZifState *state = NULL;
	ZifStore *local = NULL;
	ZifStore *remote = NULL;
	ZifTransaction *transaction = NULL;

	g_return_val_if_fail (ZIF_IS_MANIFEST (manifest), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* load file */
	dirname = g_path_get_dirname (filename);
	keyfile = g_key_file_new ();
	g_debug ("             ---            ");
	g_debug ("loading manifest %s", filename);
	ret = g_key_file_load_from_file (keyfile, filename, 0, &error_local);
	if (!ret) {
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_POST_INSTALL,
			     "Failed to load manifest file %s: %s",
			     filename,
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* skip this */
	ret = g_key_file_get_boolean (keyfile, "Zif Manifest", "Disable", NULL);
	if (ret) {
		g_debug ("skipping file");
		goto out;
	}

	/* get local store */
	local = zif_store_meta_new ();
	zif_store_meta_set_is_local (ZIF_STORE_META (local), TRUE);
	packages_local = g_key_file_get_string (keyfile, "Zif Manifest", "AddLocal", NULL);
	if (packages_local != NULL) {
		ret = zif_manifest_add_packages_to_store (manifest,
							  local,
							  dirname,
							  packages_local,
							  error);
		if (!ret)
			goto out;
	}

	/* get remote store */
	remote = zif_store_meta_new ();
	packages_remote = g_key_file_get_string (keyfile, "Zif Manifest", "AddRemote", NULL);
	if (packages_remote != NULL) {
		ret = zif_manifest_add_packages_to_store (manifest,
							  remote,
							  dirname,
							  packages_remote,
							  error);
		if (!ret)
			goto out;
	}

	/* setup transaction */
	transaction = zif_transaction_new ();
	remote_array = zif_store_array_new ();
	zif_store_array_add_store (remote_array, remote);
	zif_transaction_set_store_local (transaction, local);
	zif_transaction_set_stores_remote (transaction, remote_array);

	/* installs */
	transaction_install = g_key_file_get_string (keyfile, "Zif Manifest", "TransactionInstall", NULL);
	if (transaction_install != NULL) {
		ret = zif_manifest_add_packages_to_transaction (manifest,
								transaction,
								remote,
								ZIF_MANIFEST_ACTION_INSTALL,
								transaction_install,
								error);
		if (!ret)
			goto out;
		added_something = TRUE;
	}

	/* remove */
	transaction_install = g_key_file_get_string (keyfile, "Zif Manifest", "TransactionRemove", NULL);
	if (transaction_install != NULL) {
		ret = zif_manifest_add_packages_to_transaction (manifest,
								transaction,
								local,
								ZIF_MANIFEST_ACTION_REMOVE,
								transaction_install,
								error);
		if (!ret)
			goto out;
		added_something = TRUE;
	}

	/* update */
	transaction_install = g_key_file_get_string (keyfile, "Zif Manifest", "TransactionUpdate", NULL);
	if (transaction_install != NULL) {
		ret = zif_manifest_add_packages_to_transaction (manifest,
								transaction,
								local,
								ZIF_MANIFEST_ACTION_UPDATE,
								transaction_install,
								error);
		if (!ret)
			goto out;
		added_something = TRUE;
	}

	/* was anything added? */
	if (!added_something) {
		ret = FALSE;
		g_set_error_literal (error,
				     ZIF_MANIFEST_ERROR,
				     ZIF_MANIFEST_ERROR_FAILED,
				     "nothing was added to the transaction!");
		goto out;
	}

	/* resolve */
	state = zif_state_new ();
	ret = zif_transaction_resolve (transaction, state, &error_local);
	if (!ret) {
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_FAILED,
			     "failed to add resolve transaction: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* add the output of the resolve to the fake local repo */
	resolve_install = zif_transaction_get_install (transaction);
	ret = zif_store_meta_add_packages (ZIF_STORE_META (local), resolve_install, &error_local);
	if (!ret) {
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_FAILED,
			     "failed to add transaction set to local store: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* remove the output of the resolve to the fake local repo */
	resolve_remove = zif_transaction_get_remove (transaction);
	ret = zif_store_meta_remove_packages (ZIF_STORE_META (local), resolve_remove, &error_local);
	if (!ret) {
		g_set_error (error,
			     ZIF_MANIFEST_ERROR,
			     ZIF_MANIFEST_ERROR_FAILED,
			     "failed to remove transaction set from local store: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* check state */
	post_installed = g_key_file_get_string (keyfile, "Zif Manifest", "PostInstalled", NULL);
	if (post_installed != NULL) {
		ret = zif_manifest_check_post_installed (manifest,
							 local,
							 post_installed,
							 error);
		if (!ret)
			goto out;
	} else {
		g_warning ("PostInstalled usually required in %s...", filename);
	}
out:
	g_free (transaction_install);
	g_free (transaction_remove);
	g_free (transaction_update);
	g_free (packages_local);
	g_free (packages_remote);
	g_free (post_installed);
	if (state != NULL)
		g_object_unref (state);
	if (local != NULL)
		g_object_unref (local);
	if (remote != NULL)
		g_object_unref (remote);
	if (transaction != NULL)
		g_object_unref (transaction);
	if (remote_array != NULL)
		g_ptr_array_unref (remote_array);
	if (resolve_install != NULL)
		g_ptr_array_unref (resolve_install);
	if (resolve_remove != NULL)
		g_ptr_array_unref (resolve_remove);
	g_key_file_free (keyfile);
	g_free (dirname);
	return ret;
}

/**
 * zif_manifest_finalize:
 **/
static void
zif_manifest_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (ZIF_IS_MANIFEST (object));
	G_OBJECT_CLASS (zif_manifest_parent_class)->finalize (object);
}

/**
 * zif_manifest_class_init:
 **/
static void
zif_manifest_class_init (ZifManifestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = zif_manifest_finalize;

	g_type_class_add_private (klass, sizeof (ZifManifestPrivate));
}

/**
 * zif_manifest_init:
 **/
static void
zif_manifest_init (ZifManifest *manifest)
{
	manifest->priv = ZIF_MANIFEST_GET_PRIVATE (manifest);
}

/**
 * zif_manifest_new:
 *
 * Return value: A new manifest class instance.
 *
 * Since: 0.1.3
 **/
ZifManifest *
zif_manifest_new (void)
{
	ZifManifest *manifest;
	manifest = g_object_new (ZIF_TYPE_MANIFEST, NULL);
	return ZIF_MANIFEST (manifest);
}
