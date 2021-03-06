/*
 * GstAVSynth:
 * Copyright (C) 2009 LRN <lrn1986 _at_ gmail _dot_ com>
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
 * http://www.gnu.org/copyleft/gpl.html .
 */

#ifdef HAVE_CONFIG_H
# ifndef GST_LICENSE   /* don't include config.h twice, it has no guards */
#  include "config.h"
# endif
#endif

#include "gstavsynth_loader.h"

extern gchar *plugindir_var;

const gchar *gst_avsynth_get_plugin_directory()
{
  return AVSYNTHPLUGINDIR;
}

struct AVS_CLoaderData
{
  /* GStreamer plugin that registers the type */
  GstPlugin *plugin;
  /* Path to the plugin module (UTF-8) */
  gchar *filename;
  /* Prefix for full names*/
  gchar *fullnameprefix;
};

gint AVSC_CC
_avs_loader_se_add_function (AVS_ScriptEnvironment * p, const gchar * name, const gchar * paramstr, 
                 const gchar* srccapstr, const gchar* sinkcapstr,
                 AVSApplyFunc applyf, void * user_data)
{
  GstAVSynthVideoFilterClassParams *params;
  gchar *type_name;
  gchar *plugin_name;

  GType type;
  gint rank;

  GTypeInfo typeinfo = {
    sizeof (GstAVSynthVideoFilterClass),
    (GBaseInitFunc) gst_avsynth_video_filter_base_init,
    NULL,
    (GClassInitFunc) gst_avsynth_video_filter_class_init,
    NULL,
    NULL,
    sizeof (GstAVSynthVideoFilter),
    0,
    (GInstanceInitFunc) gst_avsynth_video_filter_init,
  };

  AVS_CLoaderData *data = (AVS_CLoaderData *)p->internal;

  GST_DEBUG ("Trying filter %s(%s)", name, paramstr);

  /* construct the type */
  plugin_name = g_strdup ((gchar *) name);
  g_strdelimit (plugin_name, NULL, '_');
  type_name = g_strdup_printf ("avsynth_%s", plugin_name);
  g_free (plugin_name);

  /* if it's already registered, drop it */
  if (g_type_from_name (type_name)) {
    g_free (type_name);
    return 0;
  }

  params = g_new0 (GstAVSynthVideoFilterClassParams, 1);
  params->name = g_strdup (name);
  params->fullname = g_strdup_printf ("%s_%s", data->fullnameprefix, (gchar *) name);
  params->srccaps = gst_caps_from_string (srccapstr);
  params->sinkcaps = gst_caps_from_string (sinkcapstr);
  params->params = g_strdup ((gchar *) paramstr);
  params->filename = g_strdup (data->filename);
  params->c = TRUE;

  /* create the gtype now */
  type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, (GTypeFlags) 0);
  g_type_set_qdata (type, GST_AVSYNTH_VIDEO_FILTER_PARAMS_QDATA, (gpointer) params);

  rank = GST_RANK_NONE;

  if (!gst_element_register (data->plugin, type_name, rank, type))
    g_warning ("Failed to register %s", type_name);

  /* Don't free params contents because class uses them now */
  g_free (type_name);
  return 0;
}

void AVSC_CC
_avs_loader_se_init (AVS_ScriptEnvironment *e, GstPlugin *plugin, gchar *filename, gchar *fullnameprefix)
{
  e->avs_se_add_function = _avs_loader_se_add_function;
  ((AVS_CLoaderData *)e->internal)->plugin = plugin;
  ((AVS_CLoaderData *)e->internal)->filename = filename;
  ((AVS_CLoaderData *)e->internal)->fullnameprefix = fullnameprefix;
}

AVS_ScriptEnvironment * AVSC_CC
_avs_loader_se_new (GstPlugin *plugin, gchar *filename, gchar *fullnameprefix)
{
  AVS_ScriptEnvironment *e = _avs_se_new (NULL);
  e->internal = g_new0 (AVS_CLoaderData, 1);
  _avs_loader_se_init (e, plugin, filename, fullnameprefix);
  return e;
}


void AVSC_CC
_avs_loader_se_free (AVS_ScriptEnvironment *e)
{
  g_free (e->internal);
  e->internal = NULL;
  _avs_se_free (e);
}

/*
bool
LoaderScriptEnvironment::FunctionExists(const char* func_name)
{
  if (g_type_from_name ((gchar *) func_name))
    return true;
  else
    return false;
}
*/
LoaderScriptEnvironment::LoaderScriptEnvironment(): plugin(NULL), filename(""), fullnameprefix("")
{
}

void
LoaderScriptEnvironment::SetFilename(gchar *name)
{
  filename = name;
}

void
LoaderScriptEnvironment::SetPrefix(gchar *prefix)
{
  fullnameprefix = prefix;
}

void
LoaderScriptEnvironment::SetPlugin(GstPlugin *plug)
{
  plugin = plug;
}

void
LoaderScriptEnvironment::AddFunction(const char* name, const char* paramstr, const char* srccapstr, const char* sinkcapstr, ApplyFunc apply, void* user_data)
{
  GstAVSynthVideoFilterClassParams *params;
  gchar *type_name;
  gchar *plugin_name;

  GType type;
  gint rank;

  GTypeInfo typeinfo = {
    sizeof (GstAVSynthVideoFilterClass),
    (GBaseInitFunc) gst_avsynth_video_filter_base_init,
    NULL,
    (GClassInitFunc) gst_avsynth_video_filter_class_init,
    NULL,
    NULL,
    sizeof (GstAVSynthVideoFilter),
    0,
    (GInstanceInitFunc) gst_avsynth_video_filter_init,
  };

  GST_DEBUG ("Trying filter %s(%s)", name, paramstr);

  /* construct the type */
  plugin_name = g_strdup ((gchar *) name);
  g_strdelimit (plugin_name, NULL, '_');
  type_name = g_strdup_printf ("avsynth_%s", plugin_name);
  g_free (plugin_name);

  /* if it's already registered, drop it */
  if (g_type_from_name (type_name)) {
    g_free (type_name);
    return;
  }

  params = g_new0 (GstAVSynthVideoFilterClassParams, 1);
  params->name = g_strdup (name);
  params->fullname = g_strdup_printf ("%s_%s", fullnameprefix, (gchar *) name);
  params->srccaps = gst_caps_from_string (srccapstr);
  params->sinkcaps = gst_caps_from_string (sinkcapstr);
  params->params = g_strdup ((gchar *) paramstr);
  params->filename = g_strdup (filename);
  params->c = FALSE;

  /* create the gtype now */
  type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, (GTypeFlags) 0);
  g_type_set_qdata (type, GST_AVSYNTH_VIDEO_FILTER_PARAMS_QDATA, (gpointer) params);

  rank = GST_RANK_NONE;

  if (!gst_element_register (plugin, type_name, rank, type))
    g_warning ("Failed to register %s", type_name);

  /* Don't free params contents because class uses them now */

  g_free (type_name);
}

LoaderScriptEnvironment::~LoaderScriptEnvironment()
{
  GST_LOG ("Destroying LoaderScriptEnvironment");
}

gboolean
gst_avsynth_video_filter_register (GstPlugin * plugin, gchar *plugindirs)
{
  GDir *plugin_dir = NULL;
  GError *error = NULL;
  gchar **a_plugindirs = NULL;
  gchar **i_plugindirs = NULL;

  LoaderScriptEnvironment *env = new LoaderScriptEnvironment();

  if (!g_module_supported())
  {
    GST_LOG ("No module support, can't load plugins");
    goto cleanup;
  }
  else
    GST_LOG ("Module support is present");

  if (plugindirs)
    a_plugindirs = g_strsplit_set (plugindirs, ";", -1);

  for (i_plugindirs = a_plugindirs; i_plugindirs != NULL; ++i_plugindirs) {
    gchar *plugin_dir_name = NULL;
    const gchar *filename = NULL;
    const gchar *plugin_dir_name_utf8 = *i_plugindirs;

    if (*i_plugindirs == NULL)
    {
      i_plugindirs--;
      if (plugindir_var == NULL)
        plugindir_var = g_strdup_printf ("%s", g_filename_from_utf8 (*i_plugindirs, -1, NULL, NULL, NULL));
      break;
    }

    if (!(plugin_dir_name = g_filename_from_utf8 (plugin_dir_name_utf8, -1, NULL, NULL, NULL)))
    {
      GST_LOG ("Failed to convert plugin directory name %s to filesystem encoding", plugin_dir_name_utf8);
      continue;
    }
    else
      GST_LOG ("Converted to filesystem encoding successfully");

    plugin_dir = g_dir_open (plugin_dir_name, 0, &error);
    if (error)
    {
      g_error_free (error);
      error = NULL;
      GST_LOG ("Failed to open plugin directory %s", plugin_dir_name);
      g_free (plugin_dir_name);
      continue;
    }
    else
      GST_LOG ("Opened the directory successfully");

    GST_LOG ("Registering decoders");

    while ((filename = g_dir_read_name (plugin_dir)) != NULL)
    {
      gchar *filename_utf8 = NULL;
      gchar *full_filename_utf8 = NULL;
      gchar *full_filename = NULL;
  
      filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      if (!filename_utf8)
      {
        GST_LOG ("Failed to convert filename %s to UTF8", filename);
        continue;
      }
  
      full_filename_utf8 = g_strdup_printf ("%s%s%s", plugin_dir_name_utf8, G_DIR_SEPARATOR_S, filename_utf8);
  
      if ((full_filename = g_filename_from_utf8 (full_filename_utf8, -1, NULL, NULL, NULL)))
      {
        /* FIXME: use more sophisticated check (suppress "not-a-dll" error messages and load each file blindly?) */
        if (g_str_has_suffix (full_filename, G_MODULE_SUFFIX))
        {
          GModule *plugin_module = NULL;
    
          plugin_module = g_module_open (full_filename, (GModuleFlags) G_MODULE_BIND_LAZY);
          if (plugin_module)
          {
            AvisynthPluginInitFunc init_func;
            AvisynthCPluginInitFunc init_func_c;
            const char *ret = NULL;
            /* Assuming that g_path_get_basename() takes utf-8 string */
            gchar *prefix = g_path_get_basename (full_filename_utf8);
    
            GST_LOG ("Opened %s", full_filename_utf8);
             
            if(
                 g_module_symbol (plugin_module, "AvisynthPluginInit2", (gpointer *) &init_func) ||
                 g_module_symbol (plugin_module, "AvisynthPluginInit2@4", (gpointer *) &init_func)
              )
            {
              env->SetFilename (full_filename_utf8);
              env->SetPrefix (prefix);
              env->SetPlugin (plugin);
    
              GST_LOG ("Entering Init function");
              ret = init_func (env);
              GST_LOG ("Exited Init function");
            }
            else if (
                g_module_symbol (plugin_module, "avisynth_c_plugin_init", (gpointer *) &init_func_c) ||
                g_module_symbol (plugin_module, "avisynth_c_plugin_init@4", (gpointer *) &init_func_c)
            )
            {
              AVS_ScriptEnvironment *env_c;
              env_c = _avs_loader_se_new(plugin, full_filename_utf8, prefix);
    
              GST_LOG ("Entering Init function");
              ret = init_func_c (env_c);
              GST_LOG ("Exited Init function");
              _avs_loader_se_free (env_c);
            }
            else
              GST_LOG ("Can't find AvisynthPluginInit2 or AvisynthPluginInit2@4");
            g_free (prefix);
            g_module_close (plugin_module);
          }
          else
            GST_LOG ("g_module_open failed for %s", filename_utf8);
          g_free (full_filename);
          full_filename = NULL;
        }
        else
          GST_LOG ("%s is not a dll", full_filename_utf8);
      }
      else
        GST_LOG ("Failed to convert filename %s to native encoding", full_filename_utf8);
  
      g_free (full_filename_utf8);
      full_filename_utf8 = NULL;
      g_free (filename_utf8);
      filename_utf8 = NULL;
    }

    if (plugin_dir)
      g_dir_close (plugin_dir);

    g_free (plugin_dir_name);

    GST_LOG ("Finished Registering decoders");
  }

  if (a_plugindirs)
    g_strfreev (a_plugindirs);

cleanup:

  GST_LOG ("Cleaning up");

  delete env;

  return TRUE;
}
