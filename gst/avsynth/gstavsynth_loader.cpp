#include "gstavsynth_sdk.h"
#include "gstavsynth_videofilter.h"
#include "gstavsynth_loader.h"

gchar *gst_avsynth_get_plugin_directory()
{
  return "d:\\progs\\gstreamer\\0.10\\lib\\gstreamer-0.10";
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
LoaderScriptEnvironment::LoaderScriptEnvironment()
{
  fullnameprefix = "";
  plugin = NULL;
  filename = "";
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
  GstCaps *srccaps = NULL, *sinkcaps = NULL;
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
  type_name = g_strdup_printf ("gstavsynth_%s", plugin_name);
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

  /* create the gtype now */
  type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, (GTypeFlags) 0);
  g_type_set_qdata (type, GST_AVSYNTH_VIDEO_FILTER_PARAMS_QDATA, (gpointer) params);

  rank = GST_RANK_NONE;

  if (!gst_element_register (plugin, type_name, rank, type)) {
    g_warning ("Failed to register %s", type_name);
    g_free (type_name);
    return;
  }

  g_free (type_name);
}

LoaderScriptEnvironment::~LoaderScriptEnvironment()
{
  GST_LOG ("Destroying LoaderScriptEnvironment");
}

gboolean
gst_avsynth_video_filter_register (GstPlugin * plugin)
{
  gchar *plugin_dir_name = NULL;
  gchar *plugin_dir_name_utf8 = NULL;
  GDir *plugin_dir = NULL;
  GError *error = NULL;
  const gchar *filename = NULL;

  LoaderScriptEnvironment *env = new LoaderScriptEnvironment();

  if (!g_module_supported())
  {
    GST_LOG ("No module support, can't load plugins");
    return NULL;
  }
  else
    GST_LOG ("Module support is present");

  if (!(plugin_dir_name_utf8 = gst_avsynth_get_plugin_directory ()))
  {
    GST_LOG ("Failed to get plugin directory name");
    goto cleanup;
  }
  else
    GST_LOG ("Directory name is %s", plugin_dir_name_utf8);

  if (!(plugin_dir_name = g_filename_from_utf8 (plugin_dir_name_utf8, -1, NULL, NULL, NULL)))
  {
    GST_LOG ("Failed to convert plugin directory name %s to filesystem encoding", plugin_dir_name_utf8);
    goto cleanup;
  }
  else
    GST_LOG ("Converted to filesystem encoding successfully");

  plugin_dir = g_dir_open (plugin_dir_name, 0, &error);
  if (error)
  {
    GST_LOG ("Failed to open plugin directory %s", plugin_dir_name);
    goto cleanup;
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
      /* FIXME: use more sophisticated check (suppress "not-a-dll" error messages?) */
      /* What about portability? */
      if (g_str_has_suffix (full_filename, ".dll"))
      {
        GModule *plugin_module = NULL;
  
        plugin_module = g_module_open (full_filename, (GModuleFlags) G_MODULE_BIND_LAZY);
        if (plugin_module)
        {
          AvisynthPluginInitFunc init_func;
          const char *ret = NULL;
  
          GST_LOG ("Opened %s", full_filename_utf8);
           
          if(
                g_module_symbol (plugin_module, "AvisynthPluginInit2", (gpointer *) &init_func) ||
                g_module_symbol (plugin_module, "AvisynthPluginInit2@4", (gpointer *) &init_func)
            )
          {
            /* Assuming that g_path_get_basename() takes utf-8 string */
            gchar *prefix = g_path_get_basename (full_filename_utf8);
            /* TODO: remove the file extension (if any) */
            env->SetFilename (full_filename_utf8);
            env->SetPrefix (prefix);
            env->SetPlugin (plugin);
  
            GST_LOG ("Entering Init function");
            ret = init_func (env);
            GST_LOG ("Exited Init function");
            g_free (prefix);
          }
          else
            GST_LOG ("Can't find AvisynthPluginInit2 or AvisynthPluginInit2@4");
          g_module_close (plugin_module);
        }
        else
          GST_LOG ("g_module_open failed for %s", filename_utf8);
        g_free (full_filename);
      }
      else
        GST_LOG ("%s is not a dll", full_filename_utf8);
    }
    else
      GST_LOG ("Failed to convert filename %s to native encoding", full_filename_utf8);

    g_free (full_filename_utf8);
    g_free (filename_utf8);
  }

cleanup:

  GST_LOG ("Cleaning up");

  delete env;

  if (plugin_dir)
    g_dir_close (plugin_dir);

  g_free (plugin_dir_name);
  g_free (plugin_dir_name_utf8);
  g_free (plugin_dir);

  GST_LOG ("Finished Registering decoders");

  return TRUE;
}
