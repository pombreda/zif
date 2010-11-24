/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2010 Richard Hughes <richard@hughsie.com>
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
 * SECTION:zif-package
 * @short_description: Generic object to represent an installed or remote package.
 *
 * This object is subclassed by #ZifPackageLocal and #ZifPackageRemote.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "zif-depend.h"
#include "zif-utils.h"
#include "zif-config.h"
#include "zif-package.h"
#include "zif-repos.h"
#include "zif-string.h"
#include "zif-legal.h"

#define ZIF_PACKAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ZIF_TYPE_PACKAGE, ZifPackagePrivate))

struct _ZifPackagePrivate
{
	gchar			**package_id_split;
	gchar			*package_id;
	gchar			*cache_filename;
	GFile			*cache_file;
	ZifString		*summary;
	ZifString		*description;
	ZifString		*license;
	ZifString		*url;
	ZifString		*category;
	ZifString		*location_href;
	ZifString		*group;
	guint64			 size;
	guint64			 time_file;
	GPtrArray		*files;
	GPtrArray		*requires;
	GPtrArray		*provides;
	gboolean		 provides_set;
	GPtrArray		*obsoletes;
	GPtrArray		*conflicts;
	GHashTable		*requires_hash;
	GHashTable		*provides_hash;
	GHashTable		*obsoletes_hash;
	GHashTable		*conflicts_hash;
	GHashTable		*requires_any_hash;
	GHashTable		*provides_any_hash;
	GHashTable		*obsoletes_any_hash;
	GHashTable		*conflicts_any_hash;
	gboolean		 any_file_requires;
	gboolean		 any_file_provides;
	gboolean		 any_file_obsoletes;
	gboolean		 any_file_conflicts;
	gboolean		 installed;
};

G_DEFINE_TYPE (ZifPackage, zif_package, G_TYPE_OBJECT)

static gboolean
zif_package_ensure_data (ZifPackage *package, ZifPackageEnsureType type,
			 ZifState *state, GError **error);

/**
 * zif_package_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
GQuark
zif_package_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("zif_package_error");
	return quark;
}

/**
 * zif_package_compare:
 * @a: the first package to compare
 * @b: the second package to compare
 *
 * Compares one package versions against each other.
 *
 * Return value: 1 for a>b, 0 for a==b, -1 for b>a, or G_MAXINT for error
 *
 * Since: 0.1.0
 **/
gint
zif_package_compare (ZifPackage *a, ZifPackage *b)
{
	gchar **splita;
	gchar **splitb;
	gint val = G_MAXINT;

	g_return_val_if_fail (ZIF_IS_PACKAGE (a), G_MAXINT);
	g_return_val_if_fail (ZIF_IS_PACKAGE (b), G_MAXINT);

	/* trivial optimisation */
	if (a == b) {
		val = 0;
		goto out;
	}

	/* no-copy */
	splita = a->priv->package_id_split;
	splitb = b->priv->package_id_split;

	g_return_val_if_fail (splita != NULL, G_MAXINT);
	g_return_val_if_fail (splitb != NULL, G_MAXINT);

	/* check name the same */
	if (g_strcmp0 (splita[ZIF_PACKAGE_ID_NAME], splitb[ZIF_PACKAGE_ID_NAME]) != 0)
		goto out;

	/* do a version compare */
	val = zif_compare_evr (splita[ZIF_PACKAGE_ID_VERSION], splitb[ZIF_PACKAGE_ID_VERSION]);

	/* if the packages are equal, prefer the same architecture */
	if (val == 0)
		val = g_strcmp0 (splitb[ZIF_PACKAGE_ID_ARCH], splita[ZIF_PACKAGE_ID_ARCH]);
out:
	return val;
}


/**
 * zif_package_is_compatible_arch:
 * @a: the first package to compare
 * @b: the second package to compare
 *
 * Finds if the package architectures are compatible.
 * In this sense, i386 is compatible with i586, but not x86_64
 *
 * Return value: %TRUE is compatible
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_is_compatible_arch (ZifPackage *a, ZifPackage *b)
{
	const gchar *archa;
	const gchar *archb;

	g_return_val_if_fail (ZIF_IS_PACKAGE (a), FALSE);
	g_return_val_if_fail (ZIF_IS_PACKAGE (b), FALSE);

	archa = a->priv->package_id_split[ZIF_PACKAGE_ID_ARCH];
	archb = b->priv->package_id_split[ZIF_PACKAGE_ID_ARCH];

	return zif_arch_is_native (archa, archb);
}

/**
 * zif_package_print:
 * @package: the #ZifPackage object
 *
 * Prints details about a package to %STDOUT.
 *
 * Since: 0.1.0
 **/
void
zif_package_print (ZifPackage *package)
{
	guint i;
	const gchar *text;
	ZifDepend *depend;
	GPtrArray *array;

	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (package->priv->package_id_split != NULL);

	g_print ("id=%s\n", package->priv->package_id);
	g_print ("summary=%s\n", zif_string_get_value (package->priv->summary));
	g_print ("description=%s\n", zif_string_get_value (package->priv->description));
	g_print ("license=%s\n", zif_string_get_value (package->priv->license));
	g_print ("group=%s\n", zif_string_get_value (package->priv->group));
	g_print ("category=%s\n", zif_string_get_value (package->priv->category));
	if (package->priv->url != NULL)
		g_print ("url=%s\n", zif_string_get_value (package->priv->url));
	g_print ("size=%"G_GUINT64_FORMAT"\n", package->priv->size);

	if (package->priv->files != NULL) {
		g_print ("files:\n");
		array = package->priv->files;
		for (i=0; i<array->len; i++)
			g_print ("\t%s\n", (const gchar *) g_ptr_array_index (array, i));
	}
	if (package->priv->requires != NULL) {
		g_print ("requires:\n");
		array = package->priv->requires;
		for (i=0; i<array->len; i++) {
			depend = g_ptr_array_index (array, i);
			text = zif_depend_get_description (depend);
			g_print ("\t%s\n", text);
		}
	}
	if (package->priv->provides != NULL) {
		g_print ("provides:\n");
		array = package->priv->provides;
		for (i=0; i<array->len; i++) {
			depend = g_ptr_array_index (array, i);
			text = zif_depend_get_description (depend);
			g_print ("\t%s\n", text);
		}
	}
	if (package->priv->obsoletes != NULL) {
		g_print ("obsoletes:\n");
		array = package->priv->obsoletes;
		for (i=0; i<array->len; i++) {
			depend = g_ptr_array_index (array, i);
			text = zif_depend_get_description (depend);
			g_print ("\t%s\n", text);
		}
	}
	if (package->priv->conflicts != NULL) {
		g_print ("conflicts:\n");
		array = package->priv->conflicts;
		for (i=0; i<array->len; i++) {
			depend = g_ptr_array_index (array, i);
			text = zif_depend_get_description (depend);
			g_print ("\t%s\n", text);
		}
	}
}

/**
 * zif_package_is_devel:
 * @package: the #ZifPackage object
 *
 * Finds out if a package is a development package.
 *
 * Return value: %TRUE or %FALSE
 *
 * Since: 0.1.0
 **/
gboolean
zif_package_is_devel (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), FALSE);
	g_return_val_if_fail (package->priv->package_id_split != NULL, FALSE);

	if (g_str_has_suffix (package->priv->package_id_split[ZIF_PACKAGE_ID_NAME], "-debuginfo"))
		return TRUE;
	if (g_str_has_suffix (package->priv->package_id_split[ZIF_PACKAGE_ID_NAME], "-devel"))
		return TRUE;
	if (g_str_has_suffix (package->priv->package_id_split[ZIF_PACKAGE_ID_NAME], "-static"))
		return TRUE;
	if (g_str_has_suffix (package->priv->package_id_split[ZIF_PACKAGE_ID_NAME], "-libs"))
		return TRUE;
	return FALSE;
}

/**
 * zif_package_is_gui:
 * @package: the #ZifPackage object
 *
 * Finds out if a package is a GUI package.
 *
 * Return value: %TRUE or %FALSE
 *
 * Since: 0.1.0
 **/
gboolean
zif_package_is_gui (ZifPackage *package)
{
	gboolean ret = FALSE;
	guint i;
	ZifDepend *depend;
	GPtrArray *array;
	ZifState *state_tmp;
	const gchar *name;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), FALSE);
	g_return_val_if_fail (package->priv->package_id_split != NULL, FALSE);

	/* get list of requires */
	state_tmp = zif_state_new ();
	array = zif_package_get_requires (package, state_tmp, NULL);
	if (array == NULL)
		goto out;
	for (i=0; i<array->len; i++) {
		depend = g_ptr_array_index (array, i);
		name = zif_depend_get_name (depend);
		if (g_strstr_len (name, -1, "gtk") != NULL ||
		    g_strstr_len (name, -1, "kde") != NULL) {
			ret = TRUE;
			break;
		}
	}
	g_ptr_array_unref (array);
out:
	g_object_unref (state_tmp);
	return ret;
}

/**
 * zif_package_is_installed:
 * @package: the #ZifPackage object
 *
 * Finds out if a package is installed.
 *
 * Return value: %TRUE or %FALSE
 *
 * Since: 0.1.0
 **/
gboolean
zif_package_is_installed (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), FALSE);
	g_return_val_if_fail (package->priv->package_id_split != NULL, FALSE);
	return package->priv->installed;
}

/**
 * zif_package_is_native:
 * @package: the #ZifPackage object
 *
 * Finds out if a package is the native architecture for the system.
 *
 * Return value: %TRUE or %FALSE
 *
 * Since: 0.1.0
 **/
gboolean
zif_package_is_native (ZifPackage *package)
{
	gchar **array;
	guint i;
	const gchar *arch;
	gboolean ret = FALSE;
	ZifConfig *config;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), FALSE);
	g_return_val_if_fail (package->priv->package_id_split != NULL, FALSE);

	/* is package in arch array */
	arch = package->priv->package_id_split[ZIF_PACKAGE_ID_ARCH];
	config = zif_config_new ();
	array = zif_config_get_basearch_array (config);
	for (i=0; array[i] != NULL; i++) {
		if (g_strcmp0 (array[i], arch) == 0) {
			ret = TRUE;
			break;
		}
	}
	g_object_unref (config);
	return ret;
}

/**
 * zif_package_is_free:
 * @package: the #ZifPackage object
 *
 * Check the string license_text for free licenses, indicated by
 * their short names as documented at
 * http://fedoraproject.org/wiki/Licensing
 *
 * Licenses can be grouped by " or " to indicate that the package
 * can be redistributed under any of the licenses in the group.
 * For instance: GPLv2+ or Artistic or FooLicense.
 *
 * Also, if a license ends with "+", the "+" is removed before
 * comparing it to the list of valid licenses.  So if license
 * "FooLicense" is free, then "FooLicense+" is considered free.
 *
 * Groups of licenses can be grouped with " and " to indicate
 * that parts of the package are distributed under one group of
 * licenses, while other parts of the package are distributed
 * under another group.  Groups may be wrapped in parenthesis.
 * For instance:
 * (GPLv2+ or Artistic) and (GPL+ or Artistic) and FooLicense.
 *
 * At least one license in each group must be free for the
 * package to be considered Free Software.  If the license_text
 * is empty, the package is considered non-free.
 *
 * Return value: %TRUE or %FALSE
 *
 * Since: 0.1.0
 **/
gboolean
zif_package_is_free (ZifPackage *package)
{
	GError *error = NULL;
	gboolean ret;
	gboolean is_free = FALSE;
	ZifLegal *legal;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), FALSE);
	g_return_val_if_fail (package->priv->package_id_split != NULL, FALSE);

	/* see if license is free */
	legal = zif_legal_new ();
	ret = zif_legal_is_free (legal,
				 zif_string_get_value (package->priv->license),
				 &is_free, &error);
	if (!ret) {
		g_warning ("failed to get free status: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_object_unref (legal);
	return is_free;
}

/**
 * zif_package_provides:
 * @package: the #ZifPackage object
 * @depend: the dependency to try and satisfy
 * @satisfies: the matched dependency, free with g_object_unref() if not %NULL
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package dependency that satisfies the supplied dependency.
 *
 * Return value: %TRUE if the package was searched.
 * Use @satisfies == %NULL to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_provides (ZifPackage *package,
		      ZifDepend *depend,
		      ZifDepend **satisfies,
		      ZifState *state,
		      GError **error)
{
	gboolean ret = TRUE;
	guint i;
	ZifDepend *depend_tmp;
	const gchar *depend_id;

	g_return_val_if_fail (package != NULL, FALSE);
	g_return_val_if_fail (depend != NULL, FALSE);
	g_return_val_if_fail (satisfies != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* this is quicker than just getting an array we don't use */
	if (!package->priv->provides_set) {
		ret = zif_package_ensure_data (package,
					       ZIF_PACKAGE_ENSURE_TYPE_PROVIDES,
					       state,
					       error);
		if (!ret)
			goto out;
	}
	if (package->priv->files == NULL) {
		ret = zif_package_ensure_data (package,
					       ZIF_PACKAGE_ENSURE_TYPE_FILES,
					       state,
					       error);
		if (!ret)
			goto out;
	}

	/* this is a file depend, but we know there are none so don't
	 * even try to lookup using either hash */
	if (zif_depend_get_name (depend)[0] == '/' &&
	    !package->priv->any_file_provides) {
		ret = TRUE;
		*satisfies = NULL;
		goto out;
	}

	/* search in the 'any' cache first */
	if (zif_depend_get_flag (depend) == ZIF_DEPEND_FLAG_ANY) {
		depend_id = zif_depend_get_name (depend);
		ret = g_hash_table_lookup_extended (package->priv->provides_any_hash,
						    depend_id,
						    NULL,
						    (void **) &depend_tmp);
		if (ret) {
			/* object is in the cache */
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* object is not in the cache, but we already added all entries */
			ret = TRUE;
			*satisfies = NULL;
		}
		goto out;
	}

	/* then the 'description' cache */
	depend_id = zif_depend_get_description (depend);
	ret = g_hash_table_lookup_extended (package->priv->provides_hash,
					    depend_id,
					    NULL,
					    (void **) &depend_tmp);
	if (ret) {
		if (depend_tmp != NULL) {
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* cache hit, but does not provide */
			*satisfies = NULL;
		}
		goto out;
	}

	/* set to unfound */
	*satisfies = NULL;

	/* find what we're looking for */
	for (i=0; i<package->priv->provides->len; i++) {
		depend_tmp = g_ptr_array_index (package->priv->provides, i);
		ret = zif_depend_satisfies (depend_tmp, depend);
		if (ret) {
			*satisfies = g_object_ref (depend_tmp);
			break;
		}
	}

	/* success either way */
	ret = TRUE;

	/* insert into cache */
	g_hash_table_insert (package->priv->provides_hash,
			     (gpointer) depend_id,
			     *satisfies);
out:
	return ret;
}

/**
 * zif_package_requires:
 * @package: the #ZifPackage object
 * @depend: the dependency to try and satisfy
 * @satisfies: the matched dependency, free with g_object_unref() if not %NULL
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package dependency that satisfies the supplied dependency.
 *
 * Return value: %TRUE if the package was searched.
 * Use @satisfies == %NULL to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_requires (ZifPackage *package,
		      ZifDepend *depend,
		      ZifDepend **satisfies,
		      ZifState *state,
		      GError **error)
{
	gboolean ret = TRUE;
	guint i;
	ZifDepend *depend_tmp;
	const gchar *depend_id;

	/* this is quicker than just getting an array we don't use */
	if (package->priv->requires == NULL) {
		ret = zif_package_ensure_data (package,
					       ZIF_PACKAGE_ENSURE_TYPE_REQUIRES,
					       state,
					       error);
		if (!ret)
			goto out;
	}

	/* this is a file depend, but we know there are none so don't
	 * even try to lookup using either hash */
	if (zif_depend_get_name (depend)[0] == '/' &&
	    !package->priv->any_file_requires) {
		ret = TRUE;
		*satisfies = NULL;
		goto out;
	}

	/* search in the 'any' cache first */
	if (zif_depend_get_flag (depend) == ZIF_DEPEND_FLAG_ANY) {
		depend_id = zif_depend_get_name (depend);
		ret = g_hash_table_lookup_extended (package->priv->requires_any_hash,
						    depend_id,
						    NULL,
						    (void **) &depend_tmp);
		if (ret) {
			/* object is in the cache */
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* object is not in the cache, but we already added all entries */
			ret = TRUE;
			*satisfies = NULL;
		}
		goto out;
	}

	/* then the 'description' cache */
	depend_id = zif_depend_get_description (depend);
	ret = g_hash_table_lookup_extended (package->priv->requires_hash,
					    depend_id,
					    NULL,
					    (void **) &depend_tmp);
	if (ret) {
		if (depend_tmp != NULL) {
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* cache hit, but does not provide */
			*satisfies = NULL;
		}
		goto out;
	}

	/* set to unfound */
	*satisfies = NULL;

	/* find what we're looking for */
	for (i=0; i<package->priv->requires->len; i++) {
		depend_tmp = g_ptr_array_index (package->priv->requires, i);
		if (zif_depend_satisfies (depend_tmp, depend)) {
			g_debug ("%s satisfied by %s",
				 zif_depend_get_description (depend_tmp),
				 zif_package_get_id (package));
			*satisfies = g_object_ref (depend_tmp);
			break;
		}
	}

	/* success either way */
	ret = TRUE;

	/* insert into cache */
	g_hash_table_insert (package->priv->requires_hash,
			     (gpointer) depend_id,
			     *satisfies);
out:
	return ret;
}

/**
 * zif_package_conflicts:
 * @package: the #ZifPackage object
 * @depend: the dependency to try and satisfy
 * @satisfies: the matched dependency, free with g_object_unref() if not %NULL
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package dependency that satisfies the supplied dependency.
 *
 * Return value: %TRUE if the package was searched.
 * Use @satisfies == %NULL to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_conflicts (ZifPackage *package,
		       ZifDepend *depend,
		       ZifDepend **satisfies,
		       ZifState *state,
		       GError **error)
{
	gboolean ret = TRUE;
	guint i;
	ZifDepend *depend_tmp;
	const gchar *depend_id;

	/* this is quicker than just getting an array we don't use */
	if (package->priv->conflicts == NULL) {
		ret = zif_package_ensure_data (package,
					       ZIF_PACKAGE_ENSURE_TYPE_CONFLICTS,
					       state,
					       error);
		if (!ret)
			goto out;
	}

	/* this is a file depend, but we know there are none so don't
	 * even try to lookup using either hash */
	if (zif_depend_get_name (depend)[0] == '/' &&
	    !package->priv->any_file_conflicts) {
		ret = TRUE;
		*satisfies = NULL;
		goto out;
	}

	/* search in the 'any' cache first */
	if (zif_depend_get_flag (depend) == ZIF_DEPEND_FLAG_ANY) {
		depend_id = zif_depend_get_name (depend);
		ret = g_hash_table_lookup_extended (package->priv->conflicts_any_hash,
						    depend_id,
						    NULL,
						    (void **) &depend_tmp);
		if (ret) {
			/* object is in the cache */
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* object is not in the cache, but we already added all entries */
			ret = TRUE;
			*satisfies = NULL;
		}
		goto out;
	}

	/* then the 'description' cache */
	depend_id = zif_depend_get_description (depend);
	ret = g_hash_table_lookup_extended (package->priv->conflicts_hash,
					    depend_id,
					    NULL,
					    (void **) &depend_tmp);
	if (ret) {
		if (depend_tmp != NULL) {
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* cache hit, but does not provide */
			*satisfies = NULL;
		}
		goto out;
	}

	/* set to unfound */
	*satisfies = NULL;

	/* find what we're looking for */
	for (i=0; i<package->priv->conflicts->len; i++) {
		depend_tmp = g_ptr_array_index (package->priv->conflicts, i);
		ret = zif_depend_satisfies (depend_tmp, depend);
		if (ret) {
			*satisfies = g_object_ref (depend_tmp);
			break;
		}
	}

	/* success either way */
	ret = TRUE;

	/* insert into cache */
	g_hash_table_insert (package->priv->conflicts_hash,
			     (gpointer) depend_id,
			     *satisfies);
out:
	return ret;
}

/**
 * zif_package_obsoletes:
 * @package: the #ZifPackage object
 * @depend: the dependency to try and satisfy
 * @satisfies: the matched dependency, free with g_object_unref() if not %NULL
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package dependency that satisfies the supplied dependency.
 *
 * Return value: %TRUE if the package was searched.
 * Use @satisfies == %NULL to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_obsoletes (ZifPackage *package,
		       ZifDepend *depend,
		       ZifDepend **satisfies,
		       ZifState *state,
		       GError **error)
{
	gboolean ret = TRUE;
	guint i;
	ZifDepend *depend_tmp;
	const gchar *depend_id;

	/* this is quicker than just getting an array we don't use */
	if (package->priv->obsoletes == NULL) {
		ret = zif_package_ensure_data (package,
					       ZIF_PACKAGE_ENSURE_TYPE_OBSOLETES,
					       state,
					       error);
		if (!ret)
			goto out;
	}

	/* this is a file depend, but we know there are none so don't
	 * even try to lookup using either hash */
	if (zif_depend_get_name (depend)[0] == '/' &&
	    !package->priv->any_file_obsoletes) {
		ret = TRUE;
		*satisfies = NULL;
		goto out;
	}

	/* search in the 'any' cache first */
	if (zif_depend_get_flag (depend) == ZIF_DEPEND_FLAG_ANY) {
		depend_id = zif_depend_get_name (depend);
		ret = g_hash_table_lookup_extended (package->priv->obsoletes_any_hash,
						    depend_id,
						    NULL,
						    (void **) &depend_tmp);
		if (ret) {
			/* object is in the cache */
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* object is not in the cache, but we already added all entries */
			ret = TRUE;
			*satisfies = NULL;
		}
		goto out;
	}

	/* then the 'description' cache */
	depend_id = zif_depend_get_description (depend);
	ret = g_hash_table_lookup_extended (package->priv->obsoletes_hash,
					    depend_id,
					    NULL,
					    (void **) &depend_tmp);
	if (ret) {
		if (depend_tmp != NULL) {
			*satisfies = g_object_ref (depend_tmp);
		} else {
			/* cache hit, but does not provide */
			*satisfies = NULL;
		}
		goto out;
	}

	/* set to unfound */
	*satisfies = NULL;

	/* find what we're looking for */
	for (i=0; i<package->priv->obsoletes->len; i++) {
		depend_tmp = g_ptr_array_index (package->priv->obsoletes, i);
		ret = zif_depend_satisfies (depend_tmp, depend);
		if (ret) {
			*satisfies = g_object_ref (depend_tmp);
			break;
		}
	}

	/* success either way */
	ret = TRUE;

	/* insert into cache */
	g_hash_table_insert (package->priv->obsoletes_hash,
			     (gpointer) depend_id,
			     *satisfies);
out:
	return ret;
}

/**
 * zif_package_get_id:
 * @package: the #ZifPackage object
 *
 * Gets the id uniquely identifying the package in all repos.
 *
 * Return value: the PackageId representing the package.
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_id (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	return package->priv->package_id;
}

/**
 * zif_package_get_name:
 * @package: the #ZifPackage object
 *
 * Gets the package name.
 *
 * Return value: the package name.
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_name (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	return package->priv->package_id_split[ZIF_PACKAGE_ID_NAME];
}

/**
 * zif_package_get_version:
 * @package: the #ZifPackage object
 *
 * Gets the package version.
 *
 * Return value: the package version, e.g. "0.1.2".
 *
 * Since: 0.1.1
 **/
const gchar *
zif_package_get_version (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	return package->priv->package_id_split[ZIF_PACKAGE_ID_VERSION];
}

/**
 * zif_package_get_arch:
 * @package: the #ZifPackage object
 *
 * Gets the package architecture, e.g. "i386".
 *
 * Return value: the package architecture.
 *
 * Since: 0.1.1
 **/
const gchar *
zif_package_get_arch (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	return package->priv->package_id_split[ZIF_PACKAGE_ID_ARCH];
}

/**
 * zif_package_get_data:
 * @package: the #ZifPackage object
 *
 * Gets the package source data, e.g. "fedora".
 *
 * Return value: the package data.
 *
 * Since: 0.1.1
 **/
const gchar *
zif_package_get_data (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	return package->priv->package_id_split[ZIF_PACKAGE_ID_DATA];
}

/**
 * zif_package_get_package_id:
 * @package: the #ZifPackage object
 *
 * Gets the id (as text) uniquely identifying the package in all repos.
 *
 * Return value: The package-id representing the package.
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_package_id (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	return package->priv->package_id;
}

/**
 * zif_package_ensure_type_to_string:
 * @type: the #ZifPackageEnsureType enumerated value
 *
 * Gets the string representation of a #ZifPackageEnsureType
 *
 * Return value: The #ZifPackageEnsureType represented as a string
 **/
const gchar *
zif_package_ensure_type_to_string (ZifPackageEnsureType type)
{
	if (type == ZIF_PACKAGE_ENSURE_TYPE_FILES)
		return "files";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_SUMMARY)
		return "summary";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_LICENCE)
		return "licence";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_DESCRIPTION)
		return "description";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_URL)
		return "url";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_SIZE)
		return "size";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_GROUP)
		return "group";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_REQUIRES)
		return "requires";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_PROVIDES)
		return "provides";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_CONFLICTS)
		return "conflicts";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_OBSOLETES)
		return "obsoletes";
	if (type == ZIF_PACKAGE_ENSURE_TYPE_CONFLICTS)
		return "conflicts";
	return "unknown";
}

/**
 * zif_package_ensure_data:
 **/
static gboolean
zif_package_ensure_data (ZifPackage *package, ZifPackageEnsureType type,
			 ZifState *state, GError **error)
{
	gboolean ret = FALSE;
	ZifPackageClass *klass = ZIF_PACKAGE_GET_CLASS (package);

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), FALSE);
	g_return_val_if_fail (zif_state_valid (state), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* no support */
	if (klass->ensure_data == NULL) {
		g_set_error (error, ZIF_PACKAGE_ERROR, ZIF_PACKAGE_ERROR_FAILED,
			     "cannot get %s data from %s",
			     zif_package_ensure_type_to_string (type),
			     zif_package_get_id (package));
		goto out;
	}

	ret = klass->ensure_data (package, type, state, error);
out:
	return ret;
}

/**
 * zif_package_get_summary:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package summary.
 *
 * Return value: the const string or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_summary (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->summary == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_SUMMARY, state, error);
		if (!ret)
			return NULL;
	}

	/* return const string */
	return zif_string_get_value (package->priv->summary);
}

/**
 * zif_package_get_description:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package description.
 *
 * Return value: the const string or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_description (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->description == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_DESCRIPTION, state, error);
		if (!ret)
			return NULL;
	}

	/* return const string */
	return zif_string_get_value (package->priv->description);
}

/**
 * zif_package_get_license:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package licence.
 *
 * Return value: the const string or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_license (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->license == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_LICENCE, state, error);
		if (!ret)
			return NULL;
	}

	/* return const string */
	return zif_string_get_value (package->priv->license);
}

/**
 * zif_package_get_url:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the homepage URL for the package.
 *
 * Return value: the const string or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_url (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->url == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_URL, state, error);
		if (!ret)
			return NULL;
	}

	/* return const string */
	return zif_string_get_value (package->priv->url);
}

/**
 * zif_package_get_filename:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the remote filename for the package, e.g. Packages/net-snmp-5.4.2-3.fc10.i386.rpm
 *
 * Return value: the const string or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_filename (ZifPackage *package, ZifState *state, GError **error)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* doesn't make much sense */
	if (package->priv->installed) {
		g_set_error_literal (error, ZIF_PACKAGE_ERROR, ZIF_PACKAGE_ERROR_FAILED,
				     "cannot get remote filename for installed package");
		return NULL;
	}

	/* not exists */
	if (package->priv->location_href == NULL) {
		g_set_error (error, ZIF_PACKAGE_ERROR, ZIF_PACKAGE_ERROR_FAILED,
			     "no data for %s", package->priv->package_id_split[ZIF_PACKAGE_ID_NAME]);
		return NULL;
	}

	/* return const string */
	return zif_string_get_value (package->priv->location_href);
}

/**
 * zif_package_get_category:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the category the packag is in.
 *
 * Return value: the const string or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_category (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->category == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_CATEGORY, state, error);
		if (!ret)
			return NULL;
	}

	/* return const string */
	return zif_string_get_value (package->priv->category);
}

/**
 * zif_package_get_group:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the package group.
 *
 * Return value: the group name string
 *
 * Since: 0.1.0
 **/
const gchar *
zif_package_get_group (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->group == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_GROUP, state, error);
		if (!ret)
			return NULL;
	}

	return zif_string_get_value (package->priv->group);
}

/**
 * zif_package_get_filename:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the filename used to create this package when using
 * zif_package_local_set_from_filename() for a local package or the
 * filename of the cached file for a remote package.
 *
 * For example, the full local path of a remote package would be:
 * /var/cache/yum/i386/fedora/hal-0.5.7-1.fc13.rpm
 *
 * Return value: The package filename, or %NULL
 *
 * Since: 0.1.3
 **/
const gchar *
zif_package_get_cache_filename (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->cache_filename == NULL) {
		ret = zif_package_ensure_data (package,
					       ZIF_PACKAGE_ENSURE_TYPE_CACHE_FILENAME,
					       state,
					       error);
		if (!ret)
			return NULL;
	}

	return package->priv->cache_filename;
}

/**
 * zif_package_get_cache_file:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the #GFile used to create this package when using
 * zif_package_local_set_from_filename() for a local package or the
 * filename of the cached file for a remote package.
 *
 * Return value: The package GFile, or %NULL. Do not unref this object.
 *
 * Since: 0.1.3
 **/
GFile *
zif_package_get_cache_file (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->cache_filename == NULL) {
		ret = zif_package_ensure_data (package,
					       ZIF_PACKAGE_ENSURE_TYPE_CACHE_FILENAME,
					       state,
					       error);
		if (!ret)
			return NULL;
	}

	return package->priv->cache_file;
}

/**
 * zif_package_get_size:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the size of the package.
 * This is the installed size for installed packages, and the download size for
 * remote packages.
 *
 * Return value: the package size, or 0 for failure
 *
 * Since: 0.1.0
 **/
guint64
zif_package_get_size (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), 0);
	g_return_val_if_fail (package->priv->package_id_split != NULL, 0);
	g_return_val_if_fail (zif_state_valid (state), 0);
	g_return_val_if_fail (error == NULL || *error == NULL, 0);

	if (package->priv->size == 0) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_SIZE, state, error);
		if (!ret)
			return 0;
	}

	return package->priv->size;
}

/**
 * zif_package_get_files:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets the file list for the package.
 *
 * Return value: the reference counted #GPtrArray, use g_ptr_array_unref() when done
 *
 * Since: 0.1.0
 **/
GPtrArray *
zif_package_get_files (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->files == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_FILES, state, error);
		if (!ret)
			return NULL;
	}

	/* return refcounted */
	return g_ptr_array_ref (package->priv->files);
}

/**
 * zif_package_get_requires:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Gets all the package requires.
 *
 * Return value: an array of ZifDepend's, use g_ptr_array_unref() when done
 *
 * Since: 0.1.0
 **/
GPtrArray *
zif_package_get_requires (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->requires == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_REQUIRES, state, error);
		if (!ret)
			return NULL;
	}

	/* return refcounted */
	return g_ptr_array_ref (package->priv->requires);
}

/**
 * zif_package_get_provides:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Get all the package provides.
 *
 * Return value: an array of ZifDepend's, use g_ptr_array_unref() when done
 *
 * Since: 0.1.0
 **/
GPtrArray *
zif_package_get_provides (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (!package->priv->provides_set) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_PROVIDES, state, error);
		if (!ret)
			return NULL;
	}
	if (package->priv->files == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_FILES, state, error);
		if (!ret)
			return NULL;
	}

	/* return refcounted */
	return g_ptr_array_ref (package->priv->provides);
}

/**
 * zif_package_get_obsoletes:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Get all the package obsoletes.
 *
 * Return value: an array of ZifDepend's, use g_ptr_array_unref() when done
 *
 * Since: 0.1.3
 **/
GPtrArray *
zif_package_get_obsoletes (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->obsoletes == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_OBSOLETES, state, error);
		if (!ret)
			return NULL;
	}

	/* return refcounted */
	return g_ptr_array_ref (package->priv->obsoletes);
}

/**
 * zif_package_get_conflicts:
 * @package: the #ZifPackage object
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Get all the package conflicts.
 *
 * Return value: the reference counted #GPtrArray, use g_ptr_array_unref() when done
 *
 * Since: 0.1.3
 **/
GPtrArray *
zif_package_get_conflicts (ZifPackage *package, ZifState *state, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (ZIF_IS_PACKAGE (package), NULL);
	g_return_val_if_fail (package->priv->package_id_split != NULL, NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* not exists */
	if (package->priv->conflicts == NULL) {
		ret = zif_package_ensure_data (package, ZIF_PACKAGE_ENSURE_TYPE_CONFLICTS, state, error);
		if (!ret)
			return NULL;
	}

	/* return refcounted */
	return g_ptr_array_ref (package->priv->conflicts);
}

/**
 * zif_package_set_time_file:
 * @package: the #ZifPackage object
 * @time_file: the unix time the file was created
 *
 * Sets the UNIX time the file was created.
 *
 * Since: 0.1.2
 **/
void
zif_package_set_time_file (ZifPackage *package, guint64 time_file)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	package->priv->time_file = time_file;
}

/**
 * zif_package_get_time_file:
 * @package: the #ZifPackage object
 *
 * Get the time the file was created.
 *
 * Return value: the UNIX time, or 0 for unknown.
 *
 * Since: 0.1.2
 **/
guint64
zif_package_get_time_file (ZifPackage *package)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), 0);
	return package->priv->time_file;
}

/**
 * zif_package_set_installed:
 * @package: the #ZifPackage object
 * @installed: If the package is installed
 *
 * Sets the package installed status.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_installed (ZifPackage *package, gboolean installed)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	package->priv->installed = installed;
}

/**
 * zif_package_set_id:
 * @package: the #ZifPackage object
 * @package_id: A PackageId defining the object
 *
 * Sets the unique id for the package.
 *
 * Return value: %TRUE for success, %FALSE for failure
 *
 * Since: 0.1.0
 **/
gboolean
zif_package_set_id (ZifPackage *package, const gchar *package_id, GError **error)
{
	g_return_val_if_fail (ZIF_IS_PACKAGE (package), FALSE);
	g_return_val_if_fail (package_id != NULL, FALSE);
	g_return_val_if_fail (package->priv->package_id == NULL, FALSE);

	/* not a valid package id */
	if (!zif_package_id_check (package_id)) {
		g_set_error (error, ZIF_PACKAGE_ERROR, ZIF_PACKAGE_ERROR_FAILED,
			     "not a valid package-id: %s", package_id);
		return FALSE;
	}
	package->priv->package_id = g_strdup (package_id);
	package->priv->package_id_split = zif_package_id_split (package_id);
	return TRUE;
}

/**
 * zif_package_set_summary:
 * @package: the #ZifPackage object
 * @summary: the package summary
 *
 * Sets the package summary.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_summary (ZifPackage *package, ZifString *summary)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (summary != NULL);
	g_return_if_fail (package->priv->summary == NULL);

	package->priv->summary = zif_string_ref (summary);
}

/**
 * zif_package_set_description:
 * @package: the #ZifPackage object
 * @description: the package description
 *
 * Sets the package description.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_description (ZifPackage *package, ZifString *description)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (description != NULL);
	g_return_if_fail (package->priv->description == NULL);

	package->priv->description = zif_string_ref (description);
}

/**
 * zif_package_set_license:
 * @package: the #ZifPackage object
 * @license: license
 *
 * Sets the package license.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_license (ZifPackage *package, ZifString *license)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (license != NULL);
	g_return_if_fail (package->priv->license == NULL);

	package->priv->license = zif_string_ref (license);
}

/**
 * zif_package_set_url:
 * @package: the #ZifPackage object
 * @url: The package homepage URL
 *
 * Sets the project homepage URL.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_url (ZifPackage *package, ZifString *url)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (url != NULL);
	g_return_if_fail (package->priv->url == NULL);

	package->priv->url = zif_string_ref (url);
}

/**
 * zif_package_set_location_href:
 * @package: the #ZifPackage object
 * @location_href: the remote download filename
 *
 * Sets the remote download location.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_location_href (ZifPackage *package, ZifString *location_href)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (location_href != NULL);
	g_return_if_fail (package->priv->location_href == NULL);

	package->priv->location_href = zif_string_ref (location_href);
}

/**
 * zif_package_set_category:
 * @package: the #ZifPackage object
 * @category: category
 *
 * Sets the package category.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_category (ZifPackage *package, ZifString *category)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (category != NULL);
	g_return_if_fail (package->priv->category == NULL);

	package->priv->category = zif_string_ref (category);
}

/**
 * zif_package_set_group:
 * @package: the #ZifPackage object
 * @group: the package group
 *
 * Sets the package group.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_group (ZifPackage *package, ZifString *group)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (group != NULL);
	g_return_if_fail (package->priv->group == NULL);

	package->priv->group = zif_string_ref (group);
}

/**
 * zif_package_set_cache_filename:
 * @package: the #ZifPackage object
 * @cache_filename: the cache filename
 *
 * Sets the cache filename, which is the full location of the local
 * package file on the filesystem.
 *
 * Note: this doesn't actually have to exist, but it must point to the
 * default location.
 *
 * Since: 0.1.3
 **/
void
zif_package_set_cache_filename (ZifPackage *package, const gchar *cache_filename)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (cache_filename != NULL);
	g_return_if_fail (package->priv->cache_filename == NULL);

	package->priv->cache_filename = g_strdup (cache_filename);
	if (cache_filename != NULL)
		package->priv->cache_file = g_file_new_for_path (cache_filename);
}

/**
 * zif_package_set_size:
 * @package: the #ZifPackage object
 * @size: the package size in bytes
 *
 * Sets the package size in bytes.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_size (ZifPackage *package, guint64 size)
{
	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (size != 0);
	g_return_if_fail (package->priv->size == 0);

	package->priv->size = size;
}

/**
 * zif_package_set_files:
 * @package: the #ZifPackage object
 * @files: an array of strings.
 *
 * Sets the package file list.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_files (ZifPackage *package, GPtrArray *files)
{
	const gchar *filename;
	guint i;
	ZifDepend *depend_tmp;

	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (files != NULL);
	g_return_if_fail (package->priv->files == NULL);

	/* add files as provides to 'any' cache */
	for (i=0; i<files->len; i++) {
		filename = g_ptr_array_index (files, i);
		depend_tmp = zif_depend_new ();
		zif_depend_set_flag (depend_tmp, ZIF_DEPEND_FLAG_ANY);
		zif_depend_set_name (depend_tmp, filename);
		g_hash_table_insert (package->priv->provides_any_hash,
				     (gpointer) filename,
				     depend_tmp);
		g_ptr_array_add (package->priv->provides, g_object_ref (depend_tmp));
		package->priv->any_file_provides = TRUE;
	}

	package->priv->files = g_ptr_array_ref (files);
}

/**
 * zif_package_set_requires:
 * @package: the #ZifPackage object
 * @requires: the package requires
 *
 * Sets the package requires.
 *
 * Since: 0.1.0
 **/
void
zif_package_set_requires (ZifPackage *package, GPtrArray *requires)
{
	const gchar *name_tmp;
	guint i;
	ZifDepend *depend_tmp;

	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (requires != NULL);
	g_return_if_fail (package->priv->requires == NULL);

	/* add items to 'any' cache */
	for (i=0; i<requires->len; i++) {
		depend_tmp = g_ptr_array_index (requires, i);
		name_tmp = zif_depend_get_name (depend_tmp);
		g_hash_table_insert (package->priv->requires_any_hash,
				     (gpointer) name_tmp,
				     g_object_ref (depend_tmp));

		/* this is a file depend */
		if (name_tmp[0] == '/')
			package->priv->any_file_requires = TRUE;
	}

	package->priv->requires = g_ptr_array_ref (requires);
}

/**
 * zif_package_set_provides:
 * @package: the #ZifPackage object
 * @provides: the package provides
 *
 * Sets the package provides
 *
 * Since: 0.1.0
 **/
void
zif_package_set_provides (ZifPackage *package, GPtrArray *provides)
{
	const gchar *name_tmp;
	guint i;
	ZifDepend *depend_tmp;

	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (provides != NULL);
	g_return_if_fail (!package->priv->provides_set);

	/* track this as a bool, as the array is already created */
	package->priv->provides_set = TRUE;

	/* add items to 'any' cache */
	for (i=0; i<provides->len; i++) {
		depend_tmp = g_ptr_array_index (provides, i);
		name_tmp = zif_depend_get_name (depend_tmp);
		g_hash_table_insert (package->priv->provides_any_hash,
				     (gpointer) name_tmp,
				     g_object_ref (depend_tmp));

		/* this is a file depend */
		if (name_tmp[0] == '/')
			package->priv->any_file_provides = TRUE;
	}

	/* add to the array, not replace */
	for (i=0; i<provides->len; i++) {
		depend_tmp = g_ptr_array_index (provides, i);
		g_ptr_array_add (package->priv->provides, g_object_ref (depend_tmp));
	}
}

/**
 * zif_package_set_obsoletes:
 * @package: the #ZifPackage object
 * @obsoletes: the package obsoletes
 *
 * Sets the package obsoletes.
 *
 * Since: 0.1.3
 **/
void
zif_package_set_obsoletes (ZifPackage *package, GPtrArray *obsoletes)
{
	const gchar *name_tmp;
	guint i;
	ZifDepend *depend_tmp;

	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (obsoletes != NULL);
	g_return_if_fail (package->priv->obsoletes == NULL);

	/* add items to 'any' cache */
	for (i=0; i<obsoletes->len; i++) {
		depend_tmp = g_ptr_array_index (obsoletes, i);
		name_tmp = zif_depend_get_name (depend_tmp);
		g_hash_table_insert (package->priv->obsoletes_any_hash,
				     (gpointer) name_tmp,
				     g_object_ref (depend_tmp));

		/* this is a file depend */
		if (name_tmp[0] == '/')
			package->priv->any_file_obsoletes = TRUE;
	}

	package->priv->obsoletes = g_ptr_array_ref (obsoletes);
}

/**
 * zif_package_set_conflicts:
 * @package: the #ZifPackage object
 * @conflicts: the package conflicts
 *
 * Sets the package conflicts.
 *
 * Since: 0.1.3
 **/
void
zif_package_set_conflicts (ZifPackage *package, GPtrArray *conflicts)
{
	const gchar *name_tmp;
	guint i;
	ZifDepend *depend_tmp;

	g_return_if_fail (ZIF_IS_PACKAGE (package));
	g_return_if_fail (conflicts != NULL);
	g_return_if_fail (package->priv->conflicts == NULL);

	/* add items to 'any' cache */
	for (i=0; i<conflicts->len; i++) {
		depend_tmp = g_ptr_array_index (conflicts, i);
		name_tmp = zif_depend_get_name (depend_tmp);
		g_hash_table_insert (package->priv->conflicts_any_hash,
				     (gpointer) name_tmp,
				     g_object_ref (depend_tmp));

		/* this is a file depend */
		if (name_tmp[0] == '/')
			package->priv->any_file_conflicts = TRUE;
	}

	package->priv->conflicts = g_ptr_array_ref (conflicts);
}

/**
 * zif_package_finalize:
 **/
static void
zif_package_finalize (GObject *object)
{
	ZifPackage *package;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ZIF_IS_PACKAGE (object));
	package = ZIF_PACKAGE (object);

	g_free (package->priv->cache_filename);
	if (package->priv->cache_file != NULL)
		g_object_unref (package->priv->cache_file);
	g_free (package->priv->package_id);
	g_strfreev (package->priv->package_id_split);
	if (package->priv->summary != NULL)
		zif_string_unref (package->priv->summary);
	if (package->priv->description != NULL)
		zif_string_unref (package->priv->description);
	if (package->priv->license != NULL)
		zif_string_unref (package->priv->license);
	if (package->priv->url != NULL)
		zif_string_unref (package->priv->url);
	if (package->priv->category != NULL)
		zif_string_unref (package->priv->category);
	if (package->priv->location_href != NULL)
		zif_string_unref (package->priv->location_href);
	if (package->priv->files != NULL)
		g_ptr_array_unref (package->priv->files);
	if (package->priv->requires != NULL)
		g_ptr_array_unref (package->priv->requires);
	if (package->priv->provides != NULL)
		g_ptr_array_unref (package->priv->provides);
	if (package->priv->obsoletes != NULL)
		g_ptr_array_unref (package->priv->obsoletes);
	if (package->priv->conflicts != NULL)
		g_ptr_array_unref (package->priv->conflicts);
	g_hash_table_destroy (package->priv->requires_hash);
	g_hash_table_destroy (package->priv->provides_hash);
	g_hash_table_destroy (package->priv->obsoletes_hash);
	g_hash_table_destroy (package->priv->conflicts_hash);
	g_hash_table_destroy (package->priv->requires_any_hash);
	g_hash_table_destroy (package->priv->provides_any_hash);
	g_hash_table_destroy (package->priv->obsoletes_any_hash);
	g_hash_table_destroy (package->priv->conflicts_any_hash);

	G_OBJECT_CLASS (zif_package_parent_class)->finalize (object);
}

/**
 * zif_package_class_init:
 **/
static void
zif_package_class_init (ZifPackageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = zif_package_finalize;
	g_type_class_add_private (klass, sizeof (ZifPackagePrivate));
}

/**
 * zif_package_init:
 **/
static void
zif_package_init (ZifPackage *package)
{
	package->priv = ZIF_PACKAGE_GET_PRIVATE (package);

	/* we have to create this now to allow us to call
	 * zif_package_set_files() before zif_package_set_provides() */
	package->priv->provides = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* this provides a O(1) lookup for the entire provide */
	package->priv->requires_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      NULL);
	package->priv->provides_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      NULL);
	package->priv->obsoletes_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      NULL);
	package->priv->conflicts_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      NULL);

	/* this provides a O(1) lookup for the provide name, which
	 * may seem odd, but it's required for the ZIF_DEPEND_FLAG_ANY
	 * check, and 'any' happens 99.5% of the time in reality */
	package->priv->requires_any_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      (GDestroyNotify) g_object_unref);
	package->priv->provides_any_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      (GDestroyNotify) g_object_unref);
	package->priv->obsoletes_any_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      (GDestroyNotify) g_object_unref);
	package->priv->conflicts_any_hash = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      NULL,
							      (GDestroyNotify) g_object_unref);
}

/**
 * zif_package_new:
 *
 * Return value: A new #ZifPackage class instance.
 *
 * Since: 0.1.0
 **/
ZifPackage *
zif_package_new (void)
{
	ZifPackage *package;
	package = g_object_new (ZIF_TYPE_PACKAGE, NULL);
	return ZIF_PACKAGE (package);
}

