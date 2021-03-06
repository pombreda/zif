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
 * SECTION:zif-md-filelists-xml
 * @short_description: FilelistsXml metadata
 *
 * Provide access to the filelists_xml repo metadata.
 * This object is a subclass of #ZifMd
 */

typedef enum {
	ZIF_MD_FILELISTS_XML_SECTION_LIST,
	ZIF_MD_FILELISTS_XML_SECTION_UNKNOWN
} ZifMdFilelistsXmlSection;

typedef enum {
	ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE,
	ZIF_MD_FILELISTS_XML_SECTION_LIST_UNKNOWN
} ZifMdFilelistsXmlSectionList;

typedef enum {
	ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_FILE,
	ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_UNKNOWN
} ZifMdFilelistsXmlSectionListPackage;

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <gio/gio.h>

#include "zif-config.h"
#include "zif-md-filelists-xml.h"
#include "zif-md.h"
#include "zif-package-private.h"
#include "zif-package-remote.h"
#include "zif-state-private.h"
#include "zif-string.h"

#define ZIF_MD_FILELISTS_XML_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ZIF_TYPE_MD_FILELISTS_XML, ZifMdFilelistsXmlPrivate))

/**
 * ZifMdFilelistsXmlPrivate:
 *
 * Private #ZifMdFilelistsXml data
 **/
struct _ZifMdFilelistsXmlPrivate
{
	gboolean			 loaded;
	ZifMdFilelistsXmlSection	 section;
	ZifMdFilelistsXmlSectionList	 section_list;
	ZifMdFilelistsXmlSectionListPackage	section_list_package;
	ZifPackage			*package_temp;
	GPtrArray			*array;
	GPtrArray			*array_temp;
	ZifConfig			*config;
	ZifPackageCompareMode		 compare_mode;
};

G_DEFINE_TYPE (ZifMdFilelistsXml, zif_md_filelists_xml, ZIF_TYPE_MD)

/**
 * zif_md_filelists_xml_unload:
 **/
static gboolean
zif_md_filelists_xml_unload (ZifMd *md, ZifState *state, GError **error)
{
	gboolean ret = FALSE;
	return ret;
}

/**
 * zif_md_filelists_xml_parser_start_element:
 **/
static void
zif_md_filelists_xml_parser_start_element (GMarkupParseContext *context,
					   const gchar *element_name,
					   const gchar **attribute_names,
					   const gchar **attribute_values,
					   gpointer user_data,
					   GError **error)
{
	guint i;
	ZifMdFilelistsXml *filelists_xml = user_data;
	ZifString *tmp;

	g_return_if_fail (ZIF_IS_MD_FILELISTS_XML (filelists_xml));

	/* group element */
	if (filelists_xml->priv->section == ZIF_MD_FILELISTS_XML_SECTION_UNKNOWN) {

		/* start of update */
		if (g_strcmp0 (element_name, "filelists") == 0) {
			filelists_xml->priv->section = ZIF_MD_FILELISTS_XML_SECTION_LIST;
			goto out;
		}

		g_warning ("unhandled element: %s", element_name);
		goto out;
	}

	/* update element */
	if (filelists_xml->priv->section == ZIF_MD_FILELISTS_XML_SECTION_LIST) {

		if (filelists_xml->priv->section_list == ZIF_MD_FILELISTS_XML_SECTION_LIST_UNKNOWN) {

			if (g_strcmp0 (element_name, "package") == 0) {
				filelists_xml->priv->section_list = ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE;
				filelists_xml->priv->package_temp = ZIF_PACKAGE (zif_package_remote_new ());
				zif_package_set_compare_mode (filelists_xml->priv->package_temp,
							      filelists_xml->priv->compare_mode);
				filelists_xml->priv->array_temp = g_ptr_array_new_with_free_func (g_free);
				for (i = 0; attribute_names[i] != NULL; i++) {
					if (g_strcmp0 (attribute_names[i], "pkgid") == 0) {
						tmp = zif_string_new (attribute_values[i]);
						zif_package_set_pkgid (filelists_xml->priv->package_temp,
								       tmp);
						zif_string_unref (tmp);
					}
				}
				goto out;
			}

			g_warning ("unhandled update list tag: %s", element_name);
			goto out;

		}
		if (filelists_xml->priv->section_list == ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE) {

			if (g_strcmp0 (element_name, "version") == 0) {
				filelists_xml->priv->section_list_package = ZIF_MD_FILELISTS_XML_SECTION_LIST_UNKNOWN;
				goto out;
			}

			if (g_strcmp0 (element_name, "file") == 0) {
				filelists_xml->priv->section_list_package = ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_FILE;
				goto out;
			}
			g_warning ("unhandled update package tag: %s", element_name);
			goto out;
		}
		g_warning ("unhandled package tag: %s", element_name);
	}

	g_warning ("unhandled base tag: %s", element_name);

out:
	return;
}

/**
 * zif_md_filelists_xml_parser_end_element:
 **/
static void
zif_md_filelists_xml_parser_end_element (GMarkupParseContext *context, const gchar *element_name,
				      gpointer user_data, GError **error)
{
	ZifMdFilelistsXml *filelists_xml = user_data;

	/* no element */
	if (filelists_xml->priv->section == ZIF_MD_FILELISTS_XML_SECTION_UNKNOWN) {
		g_warning ("unhandled base end tag: %s", element_name);
		goto out;
	}

	/* update element */
	if (filelists_xml->priv->section == ZIF_MD_FILELISTS_XML_SECTION_LIST) {

		/* update element */
		if (filelists_xml->priv->section_list == ZIF_MD_FILELISTS_XML_SECTION_LIST_UNKNOWN) {

			/* end of list */
			if (g_strcmp0 (element_name, "filelists") == 0) {
				filelists_xml->priv->section = ZIF_MD_FILELISTS_XML_SECTION_UNKNOWN;
				goto out;
			}
			g_warning ("unhandled outside tag: %s", element_name);
			goto out;
		}

		/* update element */
		if (filelists_xml->priv->section_list == ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE) {

			if (filelists_xml->priv->section_list_package == ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_UNKNOWN) {

				if (g_strcmp0 (element_name, "version") == 0)
					goto out;

				/* end of list */
				if (g_strcmp0 (element_name, "package") == 0) {
					zif_package_set_files (filelists_xml->priv->package_temp, filelists_xml->priv->array_temp);
					zif_package_set_provides_files (filelists_xml->priv->package_temp, filelists_xml->priv->array_temp);
					g_ptr_array_add (filelists_xml->priv->array, filelists_xml->priv->package_temp);
					filelists_xml->priv->package_temp = NULL;
					filelists_xml->priv->array_temp = NULL;
					filelists_xml->priv->section_list = ZIF_MD_FILELISTS_XML_SECTION_LIST_UNKNOWN;
					goto out;
				}
				g_warning ("unhandled package tag: %s", element_name);
				goto out;
			}

			if (filelists_xml->priv->section_list_package == ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_FILE) {
				if (g_strcmp0 (element_name, "file") == 0) {
					filelists_xml->priv->section_list_package = ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_UNKNOWN;
					goto out;
				}
				g_warning ("unhandled end of file tag: %s", element_name);
				goto out;
			}
			g_warning ("unhandled end of package tag: %s", element_name);
			goto out;
		}

		g_warning ("unhandled update end tag: %s", element_name);
		goto out;
	}

	g_warning ("unhandled end tag: %s", element_name);
out:
	return;
}

/**
 * zif_md_filelists_xml_parser_text:
 **/
static void
zif_md_filelists_xml_parser_text (GMarkupParseContext *context, const gchar *text, gsize text_len,
			       gpointer user_data, GError **error)

{
	ZifMdFilelistsXml *filelists_xml = user_data;

	/* skip whitespace */
	if (text_len < 1 || text[0] == ' ' || text[0] == '\t' || text[0] == '\n')
		goto out;

	/* group section */
	if (filelists_xml->priv->section == ZIF_MD_FILELISTS_XML_SECTION_LIST) {
		if (filelists_xml->priv->section_list == ZIF_MD_FILELISTS_XML_SECTION_LIST_UNKNOWN) {
			g_warning ("not saving: %s", text);
			goto out;
		}
		if (filelists_xml->priv->section_list == ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE) {
			if (filelists_xml->priv->section_list_package == ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_FILE) {
				g_ptr_array_add (filelists_xml->priv->array_temp, g_strdup (text));
				goto out;
			};
			g_warning ("not saving: %s", text);
			goto out;
		}
		g_warning ("not saving: %s", text);
		goto out;
	}
out:
	return;
}

/**
 * zif_md_filelists_xml_load:
 **/
static gboolean
zif_md_filelists_xml_load (ZifMd *md, ZifState *state, GError **error)
{
	const gchar *filename;
	gboolean ret;
	gchar *contents = NULL;
	gsize size;
	ZifMdFilelistsXml *filelists_xml = ZIF_MD_FILELISTS_XML (md);
	GMarkupParseContext *context = NULL;
	const GMarkupParser gpk_md_filelists_xml_markup_parser = {
		zif_md_filelists_xml_parser_start_element,
		zif_md_filelists_xml_parser_end_element,
		zif_md_filelists_xml_parser_text,
		NULL, /* passthrough */
		NULL /* error */
	};

	g_return_val_if_fail (ZIF_IS_MD_FILELISTS_XML (md), FALSE);
	g_return_val_if_fail (zif_state_valid (state), FALSE);

	/* already loaded */
	if (filelists_xml->priv->loaded)
		goto out;

	/* get the compare mode */
	filelists_xml->priv->compare_mode = zif_config_get_enum (filelists_xml->priv->config,
								 "pkg_compare_mode",
								 zif_package_compare_mode_from_string,
								 error);
	if (filelists_xml->priv->compare_mode == G_MAXUINT)
		goto out;

	/* get filename */
	filename = zif_md_get_filename_uncompressed (md);
	if (filename == NULL) {
		g_set_error_literal (error, ZIF_MD_ERROR, ZIF_MD_ERROR_FAILED,
				     "failed to get filename for filelists_xml");
		goto out;
	}

	/* open database */
	g_debug ("filename = %s", filename);
	zif_state_set_allow_cancel (state, FALSE);
	ret = g_file_get_contents (filename, &contents, &size, error);
	if (!ret)
		goto out;

	/* create parser */
	context = g_markup_parse_context_new (&gpk_md_filelists_xml_markup_parser, G_MARKUP_PREFIX_ERROR_POSITION, filelists_xml, NULL);

	/* parse data */
	zif_state_set_allow_cancel (state, FALSE);
	ret = g_markup_parse_context_parse (context, contents, (gssize) size, error);
	if (!ret)
		goto out;

	/* we don't need to keep syncing */
	filelists_xml->priv->loaded = TRUE;
out:
	if (context != NULL)
		g_markup_parse_context_free (context);
	g_free (contents);
	return filelists_xml->priv->loaded;
}

/**
 * zif_md_filelists_xml_get_files:
 **/
static GPtrArray *
zif_md_filelists_xml_get_files (ZifMd *md, ZifPackage *package,
				ZifState *state, GError **error)
{
	GPtrArray *array = NULL;
	GPtrArray *packages;
	ZifPackage *package_tmp;
	ZifPackage *package_found = NULL;
	guint i;
	gboolean ret;
	const gchar *pkgid;
	const gchar *pkgid_tmp;
	GError *error_local = NULL;
	ZifState *state_local;
	ZifMdFilelistsXml *md_filelists = ZIF_MD_FILELISTS_XML (md);

	g_return_val_if_fail (ZIF_IS_MD_FILELISTS_XML (md), NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* setup state */
	if (md_filelists->priv->loaded) {
		zif_state_set_number_steps (state, 1);
	} else {
		ret = zif_state_set_steps (state,
					   error,
					   80, /* load */
					   20, /* get files */
					   -1);
		if (!ret)
			goto out;
	}

	/* if not already loaded, load */
	if (!md_filelists->priv->loaded) {
		state_local = zif_state_get_child (state);
		ret = zif_md_load (ZIF_MD (md), state_local, &error_local);
		if (!ret) {
			g_set_error (error,
				     ZIF_MD_ERROR,
				     ZIF_MD_ERROR_FAILED_TO_LOAD,
				     "failed to load md_filelists_xml file: %s",
				     error_local->message);
			g_error_free (error_local);
			goto out;
		}

		/* this section done */
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
	}

	/* search array */
	pkgid = zif_package_get_pkgid (package);
	packages = md_filelists->priv->array;
	for (i = 0; i < packages->len; i++) {
		package_tmp = g_ptr_array_index (packages, i);
		pkgid_tmp = zif_package_get_pkgid (package_tmp);
		if (g_strcmp0 (pkgid, pkgid_tmp) == 0) {
			package_found = package_tmp;
			break;
		}
	}

	/* nothing found */
	if (package_found == NULL) {
		g_set_error (error,
			     ZIF_MD_ERROR,
			     ZIF_MD_ERROR_FAILED,
			     "failed to find package %s",
			     pkgid);
		goto out;
	}

	/* get files */
	state_local = zif_state_get_child (state);
	array = zif_package_get_files (package_found,
				       state_local,
				       error);
	if (array == NULL)
		goto out;

	/* this section done */
	ret = zif_state_done (state, error);
	if (!ret)
		goto out;

out:
	return array;
}

/**
 * zif_md_filelists_xml_search_file:
 **/
static GPtrArray *
zif_md_filelists_xml_search_file (ZifMd *md, gchar **search,
				  ZifState *state, GError **error)
{
	GPtrArray *array = NULL;
	GPtrArray *packages;
	ZifPackage *package;
	GPtrArray *files = NULL;
	const gchar *filename;
	guint i, j, k;
	gboolean ret;
	const gchar *pkgid;
	GError *error_local = NULL;
	ZifState *state_local;
	ZifState *state_loop;
	ZifMdFilelistsXml *md_filelists = ZIF_MD_FILELISTS_XML (md);

	g_return_val_if_fail (ZIF_IS_MD_FILELISTS_XML (md), NULL);
	g_return_val_if_fail (zif_state_valid (state), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* setup state */
	if (md_filelists->priv->loaded) {
		zif_state_set_number_steps (state, 1);
	} else {
		ret = zif_state_set_steps (state,
					   error,
					   80, /* load */
					   20, /* search */
					   -1);
		if (!ret)
			goto out;
	}

	/* if not already loaded, load */
	if (!md_filelists->priv->loaded) {
		state_local = zif_state_get_child (state);
		ret = zif_md_load (ZIF_MD (md), state_local, &error_local);
		if (!ret) {
			g_set_error (error, ZIF_MD_ERROR, ZIF_MD_ERROR_FAILED_TO_LOAD,
				     "failed to load md_filelists_xml file: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}

		/* this section done */
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
	}

	/* create results array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* no entries, so shortcut */
	if (md_filelists->priv->array->len == 0) {
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
		goto out;
	}

	/* setup steps */
	state_local = zif_state_get_child (state);
	zif_state_set_number_steps (state_local, md_filelists->priv->array->len);

	/* search array */
	packages = md_filelists->priv->array;
	for (i = 0; i < packages->len; i++) {
		package = g_ptr_array_index (packages, i);
		pkgid = zif_package_get_pkgid (package);
		state_loop = zif_state_get_child (state_local);
		files = zif_package_get_files (package, state_loop, NULL);
		for (k=0; k<files->len; k++) {
			filename = g_ptr_array_index (files, k);
			for (j = 0; search[j] != NULL; j++) {
				if (g_strcmp0 (filename, search[j]) == 0) {
					g_ptr_array_add (array, g_strdup (pkgid));
					break;
				}
			}
		}

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
	if (files != NULL)
		g_ptr_array_unref (files);
	return array;
}

/**
 * zif_md_filelists_xml_finalize:
 **/
static void
zif_md_filelists_xml_finalize (GObject *object)
{
	ZifMdFilelistsXml *md;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ZIF_IS_MD_FILELISTS_XML (object));
	md = ZIF_MD_FILELISTS_XML (object);

	g_ptr_array_unref (md->priv->array);
	g_object_unref (md->priv->config);

	G_OBJECT_CLASS (zif_md_filelists_xml_parent_class)->finalize (object);
}

/**
 * zif_md_filelists_xml_class_init:
 **/
static void
zif_md_filelists_xml_class_init (ZifMdFilelistsXmlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ZifMdClass *md_class = ZIF_MD_CLASS (klass);
	object_class->finalize = zif_md_filelists_xml_finalize;

	/* map */
	md_class->load = zif_md_filelists_xml_load;
	md_class->unload = zif_md_filelists_xml_unload;
	md_class->search_file = zif_md_filelists_xml_search_file;
	md_class->get_files = zif_md_filelists_xml_get_files;

	g_type_class_add_private (klass, sizeof (ZifMdFilelistsXmlPrivate));
}

/**
 * zif_md_filelists_xml_init:
 **/
static void
zif_md_filelists_xml_init (ZifMdFilelistsXml *md)
{
	md->priv = ZIF_MD_FILELISTS_XML_GET_PRIVATE (md);
	md->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	md->priv->loaded = FALSE;
	md->priv->section = ZIF_MD_FILELISTS_XML_SECTION_UNKNOWN;
	md->priv->section_list = ZIF_MD_FILELISTS_XML_SECTION_LIST_UNKNOWN;
	md->priv->section_list_package = ZIF_MD_FILELISTS_XML_SECTION_LIST_PACKAGE_UNKNOWN;
	md->priv->package_temp = NULL;
	md->priv->array_temp = NULL;
	md->priv->config = zif_config_new ();
}

/**
 * zif_md_filelists_xml_new:
 *
 * Return value: A new #ZifMdFilelistsXml instance.
 *
 * Since: 0.1.0
 **/
ZifMd *
zif_md_filelists_xml_new (void)
{
	ZifMdFilelistsXml *md;
	md = g_object_new (ZIF_TYPE_MD_FILELISTS_XML,
			   "kind", ZIF_MD_KIND_FILELISTS_XML,
			   NULL);
	return ZIF_MD (md);
}

