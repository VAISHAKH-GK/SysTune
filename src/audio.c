#include <adwaita.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "option/audio.h"
#include "command/command.h"

#define MAX_SINKS 16
#define MAX_DESC_LENGTH 256

GtkWidget *AudioPage;

typedef struct {
  uint32_t index;
  char description[256];
} SinkInfo;

SinkInfo *sinks;    // Array to store sink information
SinkInfo *sources;    // Array to store source information
int sink_count = 0; // Counter for the number of sinks
int source_count = 0; // Counter for the number of source

void change_panel_to_audio(gpointer user_data) {
  GtkStack *stack = GTK_STACK(user_data);
  audio_to_stack(stack);
  gtk_stack_set_visible_child_name(stack, "audio_page");
}

int current_sink_volume() {
  char *result = execute_command("pactl get-sink-volume @DEFAULT_SINK@");
  int volume;

  char *line = strtok(result, "\n");
  /* sscanf(line, "%*[^/]/%d%%%*[^/]/%d%%", &volume, &volume); */
  sscanf(line, "%*[^/]/%d", &volume);
  return volume;
}

int current_source_volume() {
  char *result = execute_command("pactl get-source-volume @DEFAULT_SOURCE@");
  int volume;

  char *line = strtok(result, "\n");
  /* sscanf(line, "%*[^/]/%d%%%*[^/]/%d%%", &volume, &volume); */
  sscanf(line, "%*[^/]/%d", &volume);
  return volume;
}

void get_audio_sources(GtkStringList *sink_list) {
  char *output =
      execute_command("pactl list sinks | grep -E \"Sink #|Description:\" | "
                      "awk '/Sink #/{idx=$2} /Description:/{print idx, $0}'");

  sinks = malloc(10 * sizeof(SinkInfo));

  char *line = strtok(output, "\n"); // Split output into lines
  while (line) {
    SinkInfo s;
    sscanf(line, "#%d Description: %[^\n]", &s.index, &s.description);
    gtk_string_list_append(sink_list, s.description);
    line = strtok(NULL, "\n"); // Split output into lines
    sinks[sink_count] = s;
    sink_count++;
    g_print("Found sink: %s\n", line);
    
  }
  g_print("\n");
}

void get_audio_sources_mic(GtkStringList *source_list) {
  char *output =
      execute_command("pactl list sources | grep -E \"Source #|Description:\" | "
                      "awk '/Source #/{idx=$2} /Description:/{print idx, $0}'");

  sources = malloc(15 * sizeof(SinkInfo));
  
  char *line = strtok(output, "\n");
  while (line) {
    SinkInfo s;
    // Parse the line containing index and description
    sscanf(line, "#%d Description: %[^\n]", &s.index, &s.description);
    gtk_string_list_append(source_list, s.description);
    line = strtok(NULL, "\n");
    sources[source_count] = s;
    source_count++;
  }
}

// Callback function for when the slider value changes
static void on_volume_changed(GtkRange *range, gpointer user_data) {
  // Get the current value of the slider
  int volume = (int)gtk_range_get_value(range);

  // adjust the system volume using a command-line tool (e.g., `amixer` or
  // `pactl`)
  char command[256];
  snprintf(command, sizeof(command),
           "pactl set-sink-volume @DEFAULT_SINK@ %d%%", volume);
  system(command);
}

// Callback function for when the slider value changes
static void on_mic_volume_changed(GtkRange *range, gpointer user_data) {
  // Get the current value of the slider
  int volume = (int)gtk_range_get_value(range);

  // adjust the system volume using a command-line tool (e.g., `amixer` or
  // `pactl`)
  char command[256];
  snprintf(command, sizeof(command), "pactl set-source-volume @DEFAULT_SOURCE@ %d%%", volume);
  system(command);
}

// Callback function when output device selection changes
void on_output_device_changed(AdwComboRow *combo, gpointer user_data) {
  GtkStringList *sink_list = GTK_STRING_LIST(adw_combo_row_get_model(combo));
  guint selected_index = adw_combo_row_get_selected(combo);

  if (selected_index == GTK_INVALID_LIST_POSITION) {
    g_print("No device selected.\n");
    return;
  }

  int index = sinks[selected_index].index;

  char command[512];
  sprintf(command, "pactl set-default-sink %d", index);
  execute_command(command);
}

// Callback function when output device selection changes
void on_input_device_changed(AdwComboRow *combo, gpointer user_data) {
  GtkStringList *sink_list = GTK_STRING_LIST(adw_combo_row_get_model(combo));
  guint selected_index = adw_combo_row_get_selected(combo);

  if (selected_index == GTK_INVALID_LIST_POSITION) {
    g_print("No device selected.\n");
    return;
  }

  int index = sources[selected_index].index;
  g_print("index is %d", index);
  g_print("source is %s", sources[selected_index].description);

  char command[512];
  sprintf(command, "pactl set-default-source %d", index);
  execute_command(command);
}

static void audio_to_stack(GtkStack *stack) {
  if (AudioPage) {
    return;
  }

  const char *ui_paths[] = {
    "ui/audio.ui",
    "/usr/share/systune/ui/audio.ui"
  };

  GtkBuilder *audio_builder = NULL;
  size_t num_paths = sizeof(ui_paths) / sizeof(ui_paths[0]);

  for (size_t i = 0; i < num_paths; i++) {
    if (g_file_test(ui_paths[i], G_FILE_TEST_EXISTS)) {
      audio_builder = gtk_builder_new_from_file(ui_paths[i]);
      if (audio_builder != NULL) {
        g_print("Loaded UI file from: %s\n", ui_paths[i]);
        break;
      }
    }
  }

  if (audio_builder == NULL) {
    g_warning("UI file 'audio.ui' not found in any of the expected locations");
    return;
  }

  AudioPage = GTK_WIDGET(gtk_builder_get_object(audio_builder, "audio_page"));
  if (AudioPage == NULL) {
    g_printerr("Failed to get audio_page from audio.ui\n");
    g_object_unref(audio_builder);
    return;
  }

  GtkStringList *sink_list = GTK_STRING_LIST(gtk_builder_get_object(audio_builder, "audio_sink_list"));
  GtkStringList *sink_list_mic = GTK_STRING_LIST(gtk_builder_get_object(audio_builder, "audio_source_list"));

  GtkAdjustment *adjustment = GTK_ADJUSTMENT(gtk_builder_get_object(audio_builder, "slider"));
  gtk_adjustment_set_value(adjustment, current_sink_volume());

  GtkAdjustment *adjustment_mic = GTK_ADJUSTMENT(gtk_builder_get_object(audio_builder, "slider_mic"));
  gtk_adjustment_set_value(adjustment_mic, current_source_volume());

  get_audio_sources(sink_list);
  get_audio_sources_mic(sink_list_mic);

  AdwComboRow *combo = ADW_COMBO_ROW(gtk_builder_get_object(audio_builder, "output_device"));
  AdwComboRow *combo_input = ADW_COMBO_ROW(gtk_builder_get_object(audio_builder, "input_device"));

  // Connect the selection change event
  g_signal_connect(combo, "notify::selected", G_CALLBACK(on_output_device_changed), NULL);
  g_signal_connect(combo_input, "notify::selected", G_CALLBACK(on_input_device_changed), NULL);

  // Retrieve the GtkScale from the builder for speaker
  GtkWidget *slider = GTK_WIDGET(gtk_builder_get_object(audio_builder, "adjustment_slider"));
  g_signal_connect(slider, "value-changed", G_CALLBACK(on_volume_changed), NULL);

  // Retrieve the GtkScale from the builder for mic
  GtkWidget *mic_slider = GTK_WIDGET(gtk_builder_get_object(audio_builder, "adjustment_slider_mic"));
  g_signal_connect(mic_slider, "value-changed", G_CALLBACK(on_mic_volume_changed), NULL);

  gtk_stack_add_named(stack, AudioPage, "audio_page");
  g_object_unref(audio_builder);
}
