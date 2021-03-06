#include <gst/gst.h>
#include <stdio.h>

#include "logger.h"
#include "exception.h"
#include "topology.h"
#include "json.h"
#include "server.h"

// Local log category
#define GST_CAT_DEFAULT log_app_main
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

GMainLoop *main_loop = NULL;
guint msg_watch = 0;
GIOChannel *io_stdin = NULL;
RtspServer *server = NULL;
Topology *topology = NULL;

bool led = false;

void Stop() {

  if (msg_watch)
    g_source_remove (msg_watch);

  if (io_stdin)
    g_io_channel_unref (io_stdin);

  if (server) {
    delete server;
  }

  if (topology) {
    delete topology;
  }

  if (main_loop) {
    g_main_loop_quit(main_loop);
    g_main_loop_unref(main_loop);
  }

  // TODO shut down properly
  exit(0);
}

static const char *gst_stream_status_string(GstStreamStatusType status) {
  switch (status) {
    case GST_STREAM_STATUS_TYPE_CREATE:
      return "CREATE";
    case GST_STREAM_STATUS_TYPE_ENTER:
      return "ENTER";
    case GST_STREAM_STATUS_TYPE_LEAVE:
      return "LEAVE";
    case GST_STREAM_STATUS_TYPE_DESTROY:
      return "DESTROY";
    case GST_STREAM_STATUS_TYPE_START:
      return "START";
    case GST_STREAM_STATUS_TYPE_PAUSE:
      return "PAUSE";
    case GST_STREAM_STATUS_TYPE_STOP:
      return "STOP";
    default:
      return "UNKNOWN";
  }
}

static gboolean MessageHandler(GstBus * bus, GstMessage * msg, gpointer user_data) {
  GError *err;
  gchar *debug_info;
  GstState state;
  GstDebugLevel msg_level;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debug_info);
      msg_level = GST_LEVEL_ERROR;
      goto printMessage;
    case GST_MESSAGE_WARNING:
      gst_message_parse_warning(msg, &err, &debug_info);
      msg_level = GST_LEVEL_WARNING;
      goto printMessage;
    case GST_MESSAGE_INFO:
      gst_message_parse_info(msg, &err, &debug_info);
      msg_level = GST_LEVEL_INFO;
      goto printMessage;

    printMessage:
      GST_LOG(gst_debug_category_get_name(log_app_main), msg_level,
              "Message received from element %s: %s\nDebugging information: %s",
              GST_OBJECT_NAME (msg->src), err->message, debug_info ? debug_info : "none");

      if (err) g_clear_error(&err);
      if (debug_info) g_free(debug_info);
      break;

    case GST_MESSAGE_EOS:
      GST_ERROR("End-Of-Stream reached.");
      break;

    case GST_MESSAGE_STATE_CHANGED:
      if (GST_IS_PIPELINE(msg->src)) {
        gst_message_parse_state_changed (msg, NULL, &state, NULL);
        GST_INFO ("%s => %s", GST_MESSAGE_SRC_NAME(msg), gst_element_state_get_name (state));
      } else {
        GST_DEBUG("State change received from element %s:\n[ %s ]",
                  GST_OBJECT_NAME(msg->src),
                  gst_structure_to_string(gst_message_get_structure(msg)));
      }
      break;
    case GST_MESSAGE_STREAM_STATUS: {
      GstStreamStatusType stream_status;
      gst_message_parse_stream_status(msg, &stream_status, NULL);
      GST_DEBUG("Stream[%s]: %s", GST_OBJECT_NAME(msg->src), gst_stream_status_string(stream_status));
      break;
    }

    default:
//      g_print("Message type %s received from element %s:{ %s }\n",
//              GST_MESSAGE_TYPE_NAME(msg),
//              GST_OBJECT_NAME (msg->src),
//              gst_structure_to_string(gst_message_get_structure(msg)));
      break;
  }
  return TRUE;
}

/* Process keyboard input */
static gboolean KeyboardHandler(GIOChannel *source, GIOCondition cond, gpointer *data) {
  gchar *str;

  if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  switch (g_ascii_tolower (str[0])) {

    // Leave
    case 'q':
      Stop();
      break;


      // Test
    case '1':
      gst_element_set_state (topology->GetPipe("TestPipe"), GST_STATE_PLAYING);
      break;

    case '2':
      gst_element_set_state (topology->GetPipe("TestPipe"), GST_STATE_PAUSED);
      break;

    case '3':
      gst_element_set_state (topology->GetPipe("TestPipe"), GST_STATE_READY);
      break;

    case '4':
      gst_element_set_state (topology->GetPipe("TestPipe"), GST_STATE_NULL);
      break;

    case 'l':
      led = !led;
      g_object_set (topology->GetElement("MainSource"), "led-power", led, NULL);
      break;

      // Main
    case 'w':
      gst_element_set_state (topology->GetPipe("MainPipe"), GST_STATE_PLAYING);
      break;

    case 'e':
      gst_element_set_state (topology->GetPipe("MainPipe"), GST_STATE_PAUSED);
      break;

    case 'r':
      gst_element_set_state (topology->GetPipe("MainPipe"), GST_STATE_READY);
      break;

    case 't':
      gst_element_set_state (topology->GetPipe("MainPipe"), GST_STATE_NULL);
      break;

      // View
    case 's':
      gst_element_set_state (topology->GetPipe("ViewPipe"), GST_STATE_PLAYING);
      break;

    case 'd':
      gst_element_set_state (topology->GetPipe("ViewPipe"), GST_STATE_PAUSED);
      break;

    case 'f':
      gst_element_set_state (topology->GetPipe("ViewPipe"), GST_STATE_READY);
      break;

    case 'g':
      gst_element_set_state (topology->GetPipe("ViewPipe"), GST_STATE_NULL);
      break;

      // Web
    case 'x':
      gst_element_set_state (topology->GetPipe("WebPipe"), GST_STATE_PLAYING);
      break;

    case 'c':
      gst_element_set_state (topology->GetPipe("WebPipe"), GST_STATE_PAUSED);
      break;

    case 'v':
      gst_element_set_state (topology->GetPipe("WebPipe"), GST_STATE_READY);
      break;

    case 'b':
      gst_element_set_state (topology->GetPipe("WebPipe"), GST_STATE_NULL);
      break;

    default:
      break;
  }

  g_free(str);

  return TRUE;
}


int main(int argc, char *argv[]) {

  // Initialize GStreamer
  gst_init (&argc, &argv);

  // Set up logging
  GST_DEBUG_CATEGORY_INIT (
      GST_CAT_DEFAULT, "GCF_APP_MAIN", GST_DEBUG_FG_GREEN, "Main application"
  );
  Logger::Init();

  // Object to keep track of registered elements and properties
  topology = new Topology();

  try {
    // Build pipeline directly from json definitions
    Json("test.json").CreateTopology(topology);
  }
  catch (GcfException) {
    Stop();
  }


  GstBus *bus;

  // attach messagehandler
  bus  = gst_pipeline_get_bus (GST_PIPELINE (topology->GetPipe("TestPipe")));
  msg_watch = gst_bus_add_watch (bus, MessageHandler, NULL);
  gst_object_unref (bus);

  // attach messagehandler
  bus  = gst_pipeline_get_bus (GST_PIPELINE (topology->GetPipe("MainPipe")));
  msg_watch = gst_bus_add_watch (bus, MessageHandler, NULL);
  gst_object_unref (bus);

  // attach messagehandler
  bus  = gst_pipeline_get_bus (GST_PIPELINE (topology->GetPipe("ViewPipe")));
  msg_watch = gst_bus_add_watch (bus, MessageHandler, NULL);
  gst_object_unref (bus);

  // attach messagehandler
  bus  = gst_pipeline_get_bus (GST_PIPELINE (topology->GetPipe("WebPipe")));
  msg_watch = gst_bus_add_watch (bus, MessageHandler, NULL);
  gst_object_unref (bus);


  // Create the server
  server = new RtspServer();
  if (!server->RegisterRtspPipes(topology->GetRtspPipes())) {
    GST_ERROR ("Can't create server RTSP pipeline. Quit.");
    Stop();
  }

  // TODO -
  server->intersinks = topology->intersinks;
  server->queues = topology->queues;
  server->TODO_tee = topology->GetElement("MainTee");
  server->TODO_pipe = topology->GetPipe("MainPipe");

  server->Start();


  // User keypresses
#ifdef G_OS_WIN32
  io_stdin = g_io_channel_win32_new_fd (fileno (stdin));
#else
  io_stdin = g_io_channel_unix_new (fileno (stdin));
#endif
  g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc) KeyboardHandler, NULL);


  // Start playing
  if (gst_element_set_state(topology->GetPipe("MainPipe"), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR ("Unable to set the main pipeline to the playing state.");
    Stop();
  }

  // Create a GLib Main Loop and set it to run
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  // Main should hang here until g_main_loop_quit() is triggered

  return 0;
}
