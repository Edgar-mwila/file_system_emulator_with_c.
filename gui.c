#include <gtk/gtk.h>
#include "simulatedFileSystem.h"

// Declare GUI elements
GtkWidget *window;
GtkWidget *grid;
GtkWidget *entry_command;
GtkWidget *btn_execute;
GtkWidget *text_view;
GtkTextBuffer *buffer;

// Function to execute command
static void execute_command(GtkWidget *widget, gpointer data) {
    const gchar *command = gtk_entry_get_text(GTK_ENTRY(entry_command));
    char cmd[20], arg1[20], arg2[20];
    sscanf(command, "%s %s %s", cmd, arg1, arg2);

    int result = -1;
    if (strcmp(cmd, "root") == 0) result = do_root(arg1, arg2);
    else if (strcmp(cmd, "print") == 0) result = do_print(arg1, arg2);
    else if (strcmp(cmd, "chdir") == 0) result = do_chdir(arg1, arg2);
    else if (strcmp(cmd, "mkdir") == 0) result = do_mkdir(arg1, arg2);
    else if (strcmp(cmd, "rmdir") == 0) result = do_rmdir(arg1, arg2);
    else if (strcmp(cmd, "mvdir") == 0) result = do_mvdir(arg1, arg2);
    else if (strcmp(cmd, "mkfil") == 0) result = do_mkfil(arg1, arg2);
    else if (strcmp(cmd, "rmfil") == 0) result = do_rmfil(arg1, arg2);
    else if (strcmp(cmd, "mvfil") == 0) result = do_mvfil(arg1, arg2);
    else if (strcmp(cmd, "szfil") == 0) result = do_szfil(arg1, arg2);
    else if (strcmp(cmd, "exit") == 0) gtk_main_quit();

    char output[1000];
    snprintf(output, sizeof(output), "Command executed. Result: %d", result);
    gtk_text_buffer_set_text(buffer, output, -1);
}

// Function to create and show the GUI
static void activate(GtkApplication *app, gpointer user_data) {
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "File System Emulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    entry_command = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_command, 0, 0, 1, 1);

    btn_execute = gtk_button_new_with_label("Execute");
    g_signal_connect(btn_execute, "clicked", G_CALLBACK(execute_command), NULL);
    gtk_grid_attach(GTK_GRID(grid), btn_execute, 1, 0, 1, 1);

    text_view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_grid_attach(GTK_GRID(grid), text_view, 0, 1, 2, 1);

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    // Initialize the file system
    do_root("", "");

    GtkApplication *app;
    int status;

    app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}