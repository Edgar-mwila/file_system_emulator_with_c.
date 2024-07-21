#include <gtk/gtk.h>
#include <string.h>
#include "simulatedFileSystem.h"
#include <time.h>

GtkWidget *window;
GtkWidget *grid;
GtkWidget *scrolled_window;
GtkWidget *icon_view;
GtkListStore *store;
GtkTreeIter iter;
GtkWidget *toolbar;
GtkToolItem *new_folder_button;
GtkToolItem *new_file_button;
GtkToolItem *delete_button;
GtkToolItem *rename_button;
GtkToolItem *back_button;
GtkToolItem *up_button;
GtkWidget *path_bar;
GtkWidget *output_view;

char current_path[256] = "/";
GList *directory_stack = NULL; // Stack to maintain directory history

enum {
    COL_NAME,
    COL_IS_DIRECTORY,
    COL_ICON,
    NUM_COLS
};

// static void append_to_output(const char *message) {
//     GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_view));
//     GtkTextIter end;
//     gtk_text_buffer_get_end_iter(buffer, &end);
//     gtk_text_buffer_insert(buffer, &end, message, -1);
//     gtk_text_buffer_insert(buffer, &end, "\n", -1);
// }

static FILE *log_file = NULL;

static void log_action(const char *message) {
    if (!log_file) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char filename[64];
        strftime(filename, sizeof(filename), "file_system_gui_%Y%m%d_%H%M%S.log", tm);
        log_file = fopen(filename, "w");
        if (!log_file) {
            fprintf(stderr, "Error opening log file\n");
            return;
        }
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", tm);

    fprintf(log_file, "%s %s\n", timestamp, message);
    fflush(log_file);
}

static void update_button_sensitivity() {
    gboolean has_selection = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view)) != NULL;
    gtk_widget_set_sensitive(GTK_WIDGET(delete_button), has_selection);
    gtk_widget_set_sensitive(GTK_WIDGET(rename_button), has_selection);
}

static void refresh_view() {
    gtk_list_store_clear(store);

    struct file_data *files = do_ls(current_path);
    struct file_data *current_file = files;

    GdkPixbuf *folder_icon = gdk_pixbuf_new_from_file_at_size("/usr/share/icons/Adwaita/32x32/places/folder.png", 32, 32, NULL);
    GdkPixbuf *file_icon = gdk_pixbuf_new_from_file_at_size("/usr/share/icons/Adwaita/32x32/mimetypes/text-x-generic.png", 32, 32, NULL);

    while (current_file != NULL) {
        GdkPixbuf *icon = current_file->is_directory ? folder_icon : file_icon;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 
                           COL_NAME, current_file->name, 
                           COL_IS_DIRECTORY, current_file->is_directory,
                           COL_ICON, icon,
                           -1);
        current_file = current_file->next;
    }

    gtk_label_set_text(GTK_LABEL(path_bar), current_path);
    log_action("Directory contents refreshed");

    g_object_unref(folder_icon);
    g_object_unref(file_icon);

    update_button_sensitivity();
}


static void item_activated(GtkIconView *icon_view, GtkTreePath *tree_path, gpointer user_data) {
    GtkTreeIter iter;
    gchar *name;
    gboolean is_directory;
    
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, tree_path);
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                       COL_NAME, &name, 
                       COL_IS_DIRECTORY, &is_directory, 
                       -1);
    
    if (is_directory) {
        // Push the current path onto the stack
        directory_stack = g_list_prepend(directory_stack, g_strdup(current_path));
        
        // Change directory
        strcat(current_path, name);
        strcat(current_path, "/");
        refresh_view();
    }
    
    g_free(name);
}

static void new_folder_clicked(GtkToolButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("New Folder", GTK_WINDOW(window), GTK_DIALOG_MODAL, 
                                                    "Cancel", GTK_RESPONSE_CANCEL, 
                                                    "Create", GTK_RESPONSE_OK, 
                                                    NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Folder name");
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        const gchar *folder_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (g_strcmp0(folder_name, "") != 0) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s%s", current_path, folder_name);
            if (do_mkdir((char *)full_path, "") == 0) {
                log_action("Folder created successfully");
            } else {
                log_action("Error creating folder");
            }
            refresh_view();
        }
    }
    gtk_widget_destroy(dialog);
}

static void new_file_clicked(GtkToolButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("New File", GTK_WINDOW(window), GTK_DIALOG_MODAL, 
                                                    "Cancel", GTK_RESPONSE_CANCEL, 
                                                    "Create", GTK_RESPONSE_OK, 
                                                    NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "File name");
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        const gchar *file_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (g_strcmp0(file_name, "") != 0) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s%s", current_path, file_name);
            if (do_mkfil((char *)full_path, "0") == 0) {
                log_action("File created successfully");
            } else {
                log_action("Error creating file");
            }
            refresh_view();
        }
    }
    gtk_widget_destroy(dialog);
}

static void delete_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        gboolean is_directory;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                           COL_NAME, &name, 
                           COL_IS_DIRECTORY, &is_directory, 
                           -1);
        
        int result;
        if (is_directory) {
            result = do_rmdir(name, "");
        } else {
            result = do_rmfil(name, "");
        }
        
        if (result == 0) {
            log_action("Item deleted successfully");
        } else {
            log_action("Error deleting item");
        }
        
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
        refresh_view();
    }
}

static void rename_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        gboolean is_directory;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                           COL_NAME, &name, 
                           COL_IS_DIRECTORY, &is_directory, 
                           -1);

        GtkWidget *dialog = gtk_dialog_new_with_buttons("Rename", GTK_WINDOW(window), GTK_DIALOG_MODAL, 
                                                        "Cancel", GTK_RESPONSE_CANCEL, 
                                                        "Rename", GTK_RESPONSE_OK, 
                                                        NULL);
        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget *entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), name);
        gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
        gtk_widget_show_all(dialog);

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response == GTK_RESPONSE_OK) {
            const gchar *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
            if (g_strcmp0(new_name, "") != 0 && g_strcmp0(new_name, name) != 0) {
                int result;
                if (is_directory) {
                    result = do_mvdir(name, (char *)new_name);
                } else {
                    result = do_mvfil(name, (char *)new_name);
                }

                if (result == 0) {
                    log_action("Item renamed successfully");
                } else {
                    log_action("Error renaming item");
                }
                refresh_view();
            }
        }
        gtk_widget_destroy(dialog);
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    }
}

static void back_clicked(GtkToolButton *button, gpointer user_data) {
    if (directory_stack) {
        gchar *prev_directory = (gchar *)directory_stack->data;
        directory_stack = g_list_remove(directory_stack, prev_directory);
        strcpy(current_path, prev_directory);
        g_free(prev_directory);
        refresh_view();
    } else {
        log_action("No previous directory in stack");
    }
}

static void up_clicked(GtkToolButton *button, gpointer user_data) {
    if (strcmp(current_path, "/") != 0) {
        char *last_slash = strrchr(current_path, '/');
        if (last_slash != current_path) {
            *last_slash = '\0';
        } else {
            *(last_slash + 1) = '\0';
        }
        refresh_view();
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "File System GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    toolbar = gtk_toolbar_new();
    gtk_grid_attach(GTK_GRID(grid), toolbar, 0, 0, 2, 1);

    new_folder_button = gtk_tool_button_new(NULL, "New Folder");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_folder_button, -1);
    g_signal_connect(new_folder_button, "clicked", G_CALLBACK(new_folder_clicked), NULL);

    new_file_button = gtk_tool_button_new(NULL, "New File");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_file_button, -1);
    g_signal_connect(new_file_button, "clicked", G_CALLBACK(new_file_clicked), NULL);

    delete_button = gtk_tool_button_new(NULL, "Delete");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), delete_button, -1);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(delete_clicked), NULL);

    rename_button = gtk_tool_button_new(NULL, "Rename");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), rename_button, -1);
    g_signal_connect(rename_button, "clicked", G_CALLBACK(rename_clicked), NULL);

    back_button = gtk_tool_button_new(NULL, "Back");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back_button, -1);
    g_signal_connect(back_button, "clicked", G_CALLBACK(back_clicked), NULL);

    up_button = gtk_tool_button_new(NULL, "Up");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), up_button, -1);
    g_signal_connect(up_button, "clicked", G_CALLBACK(up_clicked), NULL);

    path_bar = gtk_label_new(current_path);
    gtk_grid_attach(GTK_GRID(grid), path_bar, 0, 1, 2, 1);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 2, 1, 1);

    store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF);

    icon_view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(icon_view), COL_NAME);
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(icon_view), COL_ICON);
    gtk_container_add(GTK_CONTAINER(scrolled_window), icon_view);

    g_signal_connect(icon_view, "selection-changed", G_CALLBACK(update_button_sensitivity), NULL);
    g_signal_connect(icon_view, "item-activated", G_CALLBACK(item_activated), NULL);

    GtkWidget *output_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(output_scrolled_window, TRUE);
    gtk_widget_set_hexpand(output_scrolled_window, TRUE);
    gtk_grid_attach(GTK_GRID(grid), output_scrolled_window, 1, 2, 1, 1);

    output_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output_view), FALSE);
    gtk_container_add(GTK_CONTAINER(output_scrolled_window), output_view);

    refresh_view();

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    // Initialize the file system
    do_root("", "");
    log_action("File System GUI started");

    GtkApplication *app;
    int status;

    app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    if (log_file) {
        fclose(log_file);
    }
    return status;
}
