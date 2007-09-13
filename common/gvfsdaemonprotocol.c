#include <config.h>

#include <glib-object.h>
#include <dbus/dbus.h>
#include <glib/gi18n-lib.h>
#include <gvfsdaemonprotocol.h>
#include <gdbusutils.h>
#include <gio/gthemedicon.h>
#include <gio/gfileicon.h>
#include <gio/glocalfile.h>

gchar *
_g_dbus_type_from_file_attribute_type (GFileAttributeType type)
{
  char *dbus_type;

  switch (type)
    {
    case G_FILE_ATTRIBUTE_TYPE_STRING:
      dbus_type = DBUS_TYPE_STRING_AS_STRING;
      break;
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
      dbus_type = DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING;
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT32:
      dbus_type = DBUS_TYPE_UINT32_AS_STRING;
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT32:
      dbus_type = DBUS_TYPE_INT32_AS_STRING;
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT64:
      dbus_type = DBUS_TYPE_UINT64_AS_STRING;
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT64:
      dbus_type = DBUS_TYPE_INT64_AS_STRING;
      break;
    case G_FILE_ATTRIBUTE_TYPE_OBJECT:
      dbus_type = DBUS_TYPE_STRUCT_AS_STRING;
      break;
    default:
      dbus_type = NULL;
      g_warning ("Invalid attribute type %d, ignoring\n", type);
      break;
    }

  return dbus_type;
}

void
_g_dbus_append_file_attribute (DBusMessageIter *iter,
			       const char *attribute,
			       const GFileAttributeValue *value)
{
  DBusMessageIter variant_iter, inner_struct_iter, obj_struct_iter;
  char *dbus_type;
  guint32 v_uint32;
  GObject *obj = NULL;

  dbus_type = _g_dbus_type_from_file_attribute_type (value->type);

  if (!dbus_message_iter_open_container (iter,
					 DBUS_TYPE_STRUCT,
					 DBUS_TYPE_STRING_AS_STRING
					 DBUS_TYPE_VARIANT_AS_STRING,
					 &inner_struct_iter))
    _g_dbus_oom ();

  if (!dbus_message_iter_append_basic (&inner_struct_iter,
				       DBUS_TYPE_STRING,
				       &attribute))
    _g_dbus_oom ();

  if (!dbus_message_iter_open_container (&inner_struct_iter,
					 DBUS_TYPE_VARIANT,
					 dbus_type,
					 &variant_iter))
    _g_dbus_oom ();

  if (dbus_type[0] == DBUS_TYPE_ARRAY)
    _g_dbus_message_iter_append_cstring (&variant_iter, value->u.string);
  else if (dbus_type[0] == DBUS_TYPE_STRUCT)
    {
      obj = value->u.obj;

      if (G_IS_THEMED_ICON (obj))
	{
	  char **icons;

	  icons = g_themed_icon_get_names (G_THEMED_ICON (obj));
	  v_uint32 = 1;
	  if (!dbus_message_iter_append_basic (&obj_struct_iter,
					       DBUS_TYPE_UINT32, &v_uint32))
	    _g_dbus_oom ();

	  if (!dbus_message_iter_open_container (&variant_iter,
						 DBUS_TYPE_STRUCT,
						 DBUS_TYPE_UINT32_AS_STRING
						 DBUS_TYPE_ARRAY_AS_STRING
						 DBUS_TYPE_STRING_AS_STRING,
						 &obj_struct_iter))
	    _g_dbus_oom ();

	  if (!dbus_message_iter_append_fixed_array (&obj_struct_iter,
						     DBUS_TYPE_STRING,
						     icons,
						     g_strv_length (icons)))
	    _g_dbus_oom ();
	}
      else if (G_IS_FILE_ICON (obj))
	{
	  GFile *file;
	  char *path;

	  file = g_file_icon_get_file (G_FILE_ICON (obj));

	  if (G_IS_LOCAL_FILE (file))
	    {
	      if (!dbus_message_iter_open_container (&variant_iter,
						     DBUS_TYPE_STRUCT,
						     DBUS_TYPE_UINT32_AS_STRING
						     DBUS_TYPE_ARRAY_AS_STRING
						     DBUS_TYPE_BYTE_AS_STRING,
						     &obj_struct_iter))
		_g_dbus_oom ();
		    

	      v_uint32 = 2;
	      if (!dbus_message_iter_append_basic (&obj_struct_iter,
						   DBUS_TYPE_UINT32, &v_uint32))
		_g_dbus_oom ();
		  
	      path = g_file_get_path (file);
	      _g_dbus_message_iter_append_cstring (&obj_struct_iter, path);
	      g_free (path);
	    }
	  else
	    {
	      /* Seems unlikely that daemon backend will generate GFileIcons with
		 files on the vfs, so its probably not a problem not to support this.
		 (Its tricky to support, since we don't link the daemon to the client/
		 library directly.) */
	      g_warning ("Unknown file type for icon in %s, ignoring", attribute);

	      if (!dbus_message_iter_open_container (&variant_iter,
						     DBUS_TYPE_STRUCT,
						     DBUS_TYPE_UINT32_AS_STRING,
						     &obj_struct_iter))
		_g_dbus_oom ();
		  
	      v_uint32 = 0;
	      if (!dbus_message_iter_append_basic (&obj_struct_iter,
						   DBUS_TYPE_UINT32, &v_uint32))
		_g_dbus_oom ();
	    }
	  g_object_unref (file);
	}
      else
	{
	  /* NULL or unknown type: */
	  if (obj != NULL)
	    g_warning ("Unknown attribute object type for %s, ignoring", attribute);

	  if (!dbus_message_iter_open_container (&variant_iter,
						 DBUS_TYPE_STRUCT,
						 DBUS_TYPE_UINT32_AS_STRING,
						 &obj_struct_iter))
	    _g_dbus_oom ();
	      
	  v_uint32 = 0;
	  if (!dbus_message_iter_append_basic (&obj_struct_iter,
					       DBUS_TYPE_UINT32, &v_uint32))
	    _g_dbus_oom ();
	}
	  
      if (!dbus_message_iter_close_container (&variant_iter, &obj_struct_iter))
	_g_dbus_oom ();
    }
  else
    {
      if (!dbus_message_iter_append_basic (&variant_iter,
					   dbus_type[0], value))
	_g_dbus_oom ();
    }

  if (obj)
    g_object_unref (obj);
      
  if (!dbus_message_iter_close_container (&inner_struct_iter, &variant_iter))
    _g_dbus_oom ();
      
  if (!dbus_message_iter_close_container (iter, &inner_struct_iter))
    _g_dbus_oom ();
}

void
_g_dbus_append_file_info (DBusMessageIter *iter,
			  GFileInfo *info)
{
  DBusMessageIter struct_iter, array_iter;
  char **attributes;
  int i;

  attributes = g_file_info_list_attributes (info, NULL);

  if (!dbus_message_iter_open_container (iter,
					 DBUS_TYPE_STRUCT,
					 G_FILE_INFO_INNER_TYPE_AS_STRING,
					 &struct_iter))
    _g_dbus_oom ();


  if (!dbus_message_iter_open_container (&struct_iter,
					 DBUS_TYPE_ARRAY,
					 DBUS_STRUCT_BEGIN_CHAR_AS_STRING
					 DBUS_TYPE_STRING_AS_STRING	
					 DBUS_TYPE_VARIANT_AS_STRING	
					 DBUS_STRUCT_END_CHAR_AS_STRING,
					 &array_iter))
    _g_dbus_oom ();

  for (i = 0; attributes[i] != NULL; i++)
    {
      const GFileAttributeValue *value;

      value = g_file_info_get_attribute (info, attributes[i]);
      _g_dbus_append_file_attribute (&array_iter, attributes [i], value);
    }

  g_strfreev (attributes);

  if (!dbus_message_iter_close_container (&struct_iter, &array_iter))
    _g_dbus_oom ();
      
  if (!dbus_message_iter_close_container (iter, &struct_iter))
    _g_dbus_oom ();
}

gboolean
_g_dbus_get_file_attribute (DBusMessageIter *iter,
			    gchar **attribute,
			    GFileAttributeValue *value)
{
  char *str;
  char **strs;
  int n_elements;
  DBusMessageIter inner_struct_iter, variant_iter, cstring_iter, obj_iter;
  const gchar *attribute_temp;
  dbus_uint32_t obj_type;

  g_file_attribute_value_destroy (value);
  
  dbus_message_iter_recurse (iter, &inner_struct_iter);
      
  if (dbus_message_iter_get_arg_type (&inner_struct_iter) != DBUS_TYPE_STRING)
    goto error;
	
  dbus_message_iter_get_basic (&inner_struct_iter, &attribute_temp);
  *attribute = g_strdup (attribute_temp);

  dbus_message_iter_next (&inner_struct_iter);
	
  if (dbus_message_iter_get_arg_type (&inner_struct_iter) != DBUS_TYPE_VARIANT)
    goto error;

  dbus_message_iter_recurse (&inner_struct_iter, &variant_iter);

  switch (dbus_message_iter_get_arg_type (&variant_iter))
    {
    case DBUS_TYPE_STRING:
      value->type = G_FILE_ATTRIBUTE_TYPE_STRING;
      dbus_message_iter_get_basic (&variant_iter, &value->u.string);
      break;
    case DBUS_TYPE_ARRAY:
      if (dbus_message_iter_get_element_type (&variant_iter) != DBUS_TYPE_BYTE)
	goto error;

      value->type = G_FILE_ATTRIBUTE_TYPE_BYTE_STRING;

      dbus_message_iter_recurse (&variant_iter, &cstring_iter);
      dbus_message_iter_get_fixed_array (&cstring_iter,
					 &str, &n_elements);
      value->u.string = g_strndup (str, n_elements);
      break;
    case DBUS_TYPE_UINT32:
      dbus_message_iter_get_basic (&variant_iter, &value->u.uint32);
      value->type = G_FILE_ATTRIBUTE_TYPE_UINT32;
      break;
    case DBUS_TYPE_INT32:
      dbus_message_iter_get_basic (&variant_iter, &value->u.int32);
      value->type = G_FILE_ATTRIBUTE_TYPE_INT32;
      break;
    case DBUS_TYPE_UINT64:
      dbus_message_iter_get_basic (&variant_iter, &value->u.uint64);
      value->type = G_FILE_ATTRIBUTE_TYPE_UINT64;
      break;
    case DBUS_TYPE_INT64:
      dbus_message_iter_get_basic (&variant_iter, &value->u.int64);
      value->type = G_FILE_ATTRIBUTE_TYPE_INT64;
      break;
    case DBUS_TYPE_STRUCT:
      dbus_message_iter_recurse (&variant_iter, &obj_iter);
      if (dbus_message_iter_get_arg_type (&obj_iter) != DBUS_TYPE_UINT32)
	goto error;

      value->type = G_FILE_ATTRIBUTE_TYPE_OBJECT;

      dbus_message_iter_get_basic (&obj_iter, &obj_type);
      value->u.obj = NULL;
      /* 0 == NULL */
      if (obj_type == 1)
	{
	  /* G_THEMED_ICON */
	  if (_g_dbus_message_iter_get_args (&obj_iter,
					     NULL,
					     DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
					     &strs, &n_elements, 0))
	    {
	      value->u.obj = G_OBJECT (g_themed_icon_new_from_names (strs, -1));
	      dbus_free_string_array (strs);
	    }
	}
      else if (obj_type == 2)
	{
	  /* G_FILE_ICON, w/ local file */
	  if (_g_dbus_message_iter_get_args (&obj_iter,
					     NULL,
					     G_DBUS_TYPE_CSTRING, &str,
					     0))
	    {
	      GFile *file = g_file_get_for_path (str);
	      value->u.obj = G_OBJECT (g_file_icon_new (file));
	      g_free (str);
	    }
	}
      else
	{
	  /* NULL (or unsupported) */
	  if (obj_type != 0)
	    g_warning ("Unsupported object type in file attribute");
	}
      break;
    default:
      goto error;
    }

  return TRUE;

 error:
  return FALSE;
}

GFileInfo *
_g_dbus_get_file_info (DBusMessageIter *iter,
		       GError **error)
{
  GFileInfo *info;
  DBusMessageIter struct_iter, array_iter;
  gchar *attribute;
  GFileAttributeValue value = G_FILE_ATTRIBUTE_VALUE_INIT;

  info = g_file_info_new ();

  if (dbus_message_iter_get_arg_type (iter) != DBUS_TYPE_STRUCT)
    goto error;

  dbus_message_iter_recurse (iter, &struct_iter);

  if (dbus_message_iter_get_arg_type (&struct_iter) != DBUS_TYPE_ARRAY)
    goto error;
  
  dbus_message_iter_recurse (&struct_iter, &array_iter);

  while (dbus_message_iter_get_arg_type (&array_iter) == DBUS_TYPE_STRUCT)
    {
      if (!_g_dbus_get_file_attribute (&array_iter, &attribute, &value))
        goto error;

      g_file_info_set_attribute (info, attribute, &value);

      g_free (attribute);
      g_file_attribute_value_clear (&value);

      dbus_message_iter_next (&array_iter);
    }

  dbus_message_iter_next (iter);
  return info;

 error:
  g_object_unref (info);
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
	       _("Invalid file info format"));
  return NULL;
}
