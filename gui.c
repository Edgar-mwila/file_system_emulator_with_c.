#include <gtk/gtk.h>
#include <string.h>
#include <glib.h>
#include <stdbool.h>
#include "simulatedFileSystem.h"

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
GtkToolItem *details_button;
GtkToolItem *secure_button;
GtkToolItem *read_button;
GtkToolItem *write_button;
GtkWidget *back_button;
GtkWidget *up_button;
GtkWidget *path_bar;
GtkWidget *output_view;

GList *directory_stack = NULL;
char current_path[256] = "root";

enum {
    COL_NAME,
    COL_IS_DIRECTORY,
    COL_ICON,
    COL_SECURED,
    NUM_COLS
};

static void log_event(const char *message) {
    time_t now;
    struct tm *timeinfo;
    char timestamp[20];
    
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    FILE *log_file = fopen("file_system_gui.log", "a");
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fclose(log_file);
    }
}

static void store_password(const char *path, const char *password) {
    if (secured_item_count < MAX_SECURED_ITEMS) {
        strncpy(secured_items[secured_item_count].path, path, sizeof(secured_items[secured_item_count].path) - 1);
        strncpy(secured_items[secured_item_count].password, password, sizeof(secured_items[secured_item_count].password) - 1);
        secured_item_count++;
    } else {
        log_event("Maximum number of secured items reached");
    }
}

static gboolean check_password(const char *path) {
    for (int i = 0; i < secured_item_count; i++) {
        if (strcmp(secured_items[i].path, path) == 0) {
            GtkWidget *dialog = gtk_dialog_new_with_buttons("Enter Password", GTK_WINDOW(window), GTK_DIALOG_MODAL, 
                                                            "Cancel", GTK_RESPONSE_CANCEL, 
                                                            "OK", GTK_RESPONSE_OK, 
                                                            NULL);
            GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
            GtkWidget *entry = gtk_entry_new();
            gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
            gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter password");
            gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
            gtk_widget_show_all(dialog);

            gint response = gtk_dialog_run(GTK_DIALOG(dialog));
            gboolean result = FALSE;
            if (response == GTK_RESPONSE_OK) {
                const gchar *entered_password = gtk_entry_get_text(GTK_ENTRY(entry));
                if (strcmp(entered_password, secured_items[i].password) == 0) {
                    result = TRUE;
                } else {
                    GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                        GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_OK,
                        "Incorrect password. Operation cancelled.");
                    gtk_dialog_run(GTK_DIALOG(error_dialog));
                    gtk_widget_destroy(error_dialog);
                }
            }
            gtk_widget_destroy(dialog);
            return result;
        }
    }
    return TRUE; // If the item is not found in the secured items list, it's not secured
}

static void refresh_view() {
    gtk_list_store_clear(store);

    // Use the current_path when calling do_ls
    struct file_data *files = do_ls(current_path);
    struct file_data *current_file = files;

    GdkPixbuf *folder_icon = gdk_pixbuf_new_from_file_at_size("/usr/share/icons/Adwaita/32x32/places/folder.png", 32, 32, NULL);
    GdkPixbuf *file_icon = gdk_pixbuf_new_from_file_at_size("/usr/share/icons/Adwaita/32x32/mimetypes/text-x-generic.png", 32, 32, NULL);

    while (current_file != NULL) {
        GdkPixbuf *icon = current_file->is_directory ? folder_icon : file_icon;
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, current_file->name);
        gboolean is_secured = FALSE;
        
        for (int i = 0; i < secured_item_count; i++) {
            if (strcmp(secured_items[i].path, full_path) == 0) {
                is_secured = TRUE;
                break;
            }
        }

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 
                           COL_NAME, current_file->name, 
                           COL_IS_DIRECTORY, current_file->is_directory,
                           COL_ICON, icon,
                           COL_SECURED, is_secured,
                           -1);
        current_file = current_file->next;
    }
    
    gtk_label_set_text(GTK_LABEL(path_bar), current_path);
    log_event("Directory contents refreshed");

    g_object_unref(folder_icon);
    g_object_unref(file_icon);

    // Free the file_data structure
    while (files != NULL) {
        struct file_data *temp = files;
        files = files->next;
        free(temp->name);
        free(temp);
    }
}

static void item_activated(GtkIconView *icon_view, GtkTreePath *tree_path, gpointer user_data) {
    GtkTreeIter iter;
    gchar *name;
    gboolean is_directory, is_secured;
    
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, tree_path);
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                       COL_NAME, &name, 
                       COL_IS_DIRECTORY, &is_directory,
                       COL_SECURED, &is_secured,
                       -1);
    
    if (is_secured) {
        if (!check_password(name)) {
            g_free(name);
            return;
        }
    }

    if (is_directory) {
        gchar *new_path = g_build_filename(current_path, name, NULL);
        
        if (new_path) {
            if (strlen(new_path) < sizeof(current_path)) {
                if (do_chdir(name, "") == 1) {
                    directory_stack = g_list_prepend(directory_stack, g_strdup(current_path));
                    strncpy(current_path, new_path, sizeof(current_path) - 1);
                    current_path[sizeof(current_path) - 1] = '\0';
                    refresh_view();
                } else {
                    log_event("Error changing directory");
                }
            } else {
                log_event("Path too long");
            }
            g_free(new_path);
        } else {
            log_event("Error creating new path");
        }
    }
    
    g_free(name);
}

static void new_folder_clicked(GtkToolButton *button, gpointer user_data) {
    // Show a dialog to get the new folder name
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
            // Call your mkdir function here
            if (do_mkdir((char *)folder_name, "") == 0) {
                log_event("Folder created successfully");
                refresh_view();
            } else {
                log_event("Error creating folder");
            }
        }
    }
    gtk_widget_destroy(dialog);
}

static void new_file_clicked(GtkToolButton *button, gpointer user_data) {
    // Show a dialog to get the new file name
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
            // Call your mkfil function here
            if (do_mkfil((char *)file_name, "0") == 0) {
                log_event("File created successfully");
                refresh_view();
            } else {
                log_event("Error creating file");
            }
        }
    }
    gtk_widget_destroy(dialog);
}

static void delete_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        gboolean is_directory, is_secured;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                           COL_NAME, &name, 
                           COL_IS_DIRECTORY, &is_directory,
                           COL_SECURED, &is_secured,
                           -1);
        
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, name);
        
        if (is_secured && !check_password(full_path)) {
            log_event("Access denied: Incorrect password");
        } else {
            if (is_directory) {
                // Call your rmdir function here
                if (do_rmdir(name, "") == 0) {
                    log_event("Folder deleted successfully");
                } else {
                    log_event("Error deleting folder");
                }
            } else {
                // Call your rmfil function here
                if (do_rmfil(name, "") == 0) {
                    log_event("File deleted successfully");
                } else {
                    log_event("Error deleting file");
                }
            }
            refresh_view();
        }
        
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    }
}

static void rename_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        gboolean is_directory, is_secured;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                           COL_NAME, &name, 
                           COL_IS_DIRECTORY, &is_directory,
                           COL_SECURED, &is_secured,
                           -1);

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, name);
        
        if (is_secured && !check_password(full_path)) {
            log_event("Access denied: Incorrect password");
        } else {
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
                        log_event(is_directory ? "Folder renamed successfully" : "File renamed successfully");
                        refresh_view();
                    } else {
                        const char *error_message = is_directory ? 
                            "Error renaming folder. Please check if the destination name is valid and not already in use." :
                            "Error renaming file. Please check if the destination name is valid and not already in use.";
                        
                        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                            GTK_DIALOG_MODAL,
                            GTK_MESSAGE_ERROR,
                            GTK_BUTTONS_OK,
                            "%s", error_message);
                        gtk_dialog_run(GTK_DIALOG(error_dialog));
                        gtk_widget_destroy(error_dialog);
                        
                        log_event(is_directory ? "Error renaming folder" : "Error renaming file");
                    }
                }
            }
            
            gtk_widget_destroy(dialog);
        }
        
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    }
}

static int get_item_details(char *name, gboolean is_directory, char *details, size_t details_size) {
    int block_index = find_block(name, (bool)is_directory);
    if (block_index == -1) {
        return -1;
    }
    
    if (is_directory) {
        dir_type *dir = (dir_type*)(disk + block_index * BLOCK_SIZE);
        snprintf(details, details_size,
                 "Type: Directory\n"
                 "Name: %s\n"
                 "Parent: %s\n"
                 "Number of items: %d",
                 dir->name, dir->top_level, dir->subitem_count);
    } else {
        file_type *file = (file_type*)(disk + block_index * BLOCK_SIZE);
        snprintf(details, details_size,
                 "Type: File\n"
                 "Name: %s\n"
                 "Parent: %s\n"
                 "Size: %d bytes\n"
                 "Number of data blocks: %d",
                 file->name, file->top_level, file->size, file->data_block_count);
    }
    
    return 0;
}

static void details_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        gboolean is_directory, is_secured;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                           COL_NAME, &name, 
                           COL_IS_DIRECTORY, &is_directory,
                           COL_SECURED, &is_secured,
                           -1);
        
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, name);
        
        if (is_secured && !check_password(full_path)) {
            log_event("Access denied: Incorrect password");
        } else {
            // Get item details
            char details[1024];
            if (get_item_details(name, is_directory, details, sizeof(details)) == 0) {
                // Show details in a dialog
                GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                           GTK_DIALOG_MODAL,
                                                           GTK_MESSAGE_INFO,
                                                           GTK_BUTTONS_OK,
                                                           "Details for %s:\n\n%s",
                                                           name, details);
                gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_destroy(dialog);
            } else {
                log_event("Error getting item details");
            }
        }
        
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    }
}

static void secure_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_NAME, &name, -1);

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, name);

        GtkWidget *dialog = gtk_dialog_new_with_buttons("Set Password", GTK_WINDOW(window), GTK_DIALOG_MODAL, 
                                                        "Cancel", GTK_RESPONSE_CANCEL, 
                                                        "Set", GTK_RESPONSE_OK, 
                                                        NULL);
        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget *entry = gtk_entry_new();
        gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);  // Hide password
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter password");
        gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
        gtk_widget_show_all(dialog);

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response == GTK_RESPONSE_OK) {
            const gchar *password = gtk_entry_get_text(GTK_ENTRY(entry));
            if (strlen(password) > 0) {
                store_password(full_path, password);
                log_event("Item secured successfully");
                refresh_view();  // Refresh to show lock icon
            }
        }
        gtk_widget_destroy(dialog);
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    }
}

static void read_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        gboolean is_directory, is_secured;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                           COL_NAME, &name, 
                           COL_IS_DIRECTORY, &is_directory,
                           COL_SECURED, &is_secured,
                           -1);
        
        if (!is_directory) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, name);
            
            if (is_secured && !check_password(full_path)) {
                log_event("Access denied: Incorrect password");
            } else {
                char *content = do_read(name);
                if (content) {
                    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                               GTK_DIALOG_MODAL,
                                                               GTK_MESSAGE_INFO,
                                                               GTK_BUTTONS_OK,
                                                               "File content:\n\n%s",
                                                               content);
                    gtk_dialog_run(GTK_DIALOG(dialog));
                    gtk_widget_destroy(dialog);
                } else {
                    log_event("Error reading file");
                }
            }
        } else {
            log_event("Cannot read a directory");
        }
        
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    }
}

static void write_clicked(GtkToolButton *button, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    if (selected_items) {
        GtkTreeIter iter;
        gchar *name;
        gboolean is_directory, is_secured;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
                           COL_NAME, &name, 
                           COL_IS_DIRECTORY, &is_directory,
                           COL_SECURED, &is_secured,
                           -1);
        
        if (!is_directory) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, name);
            
            if (is_secured && !check_password(full_path)) {
                log_event("Access denied: Incorrect password");
            } else {
                GtkWidget *dialog = gtk_dialog_new_with_buttons("Write to File", GTK_WINDOW(window), GTK_DIALOG_MODAL, 
                                                                "Cancel", GTK_RESPONSE_CANCEL, 
                                                                "Write", GTK_RESPONSE_OK, 
                                                                NULL);
                GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
                GtkWidget *entry = gtk_entry_new();
                gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter file content");
                gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
                gtk_widget_show_all(dialog);

                gint response = gtk_dialog_run(GTK_DIALOG(dialog));
                if (response == GTK_RESPONSE_OK) {
                    const gchar *content = gtk_entry_get_text(GTK_ENTRY(entry));
                    if (do_write(name, (char *)content) == 0) {
                        log_event("File written successfully");
                        refresh_view();
                    } else {
                        log_event("Error writing to file");
                    }
                }
                gtk_widget_destroy(dialog);
            }
        } else {
            log_event("Cannot write to a directory");
        }
        
        g_free(name);
        g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
    }
}

static void back_clicked(GtkToolButton *button, gpointer user_data) {
    if (directory_stack) {
        char parent_path[256];
        strncpy(parent_path, current_path, sizeof(parent_path));
        char *last_slash = strrchr(parent_path, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
        if (do_chdir(parent_path, "") == 0) {
            gchar *prev_directory = (gchar *)directory_stack->data;
            directory_stack = g_list_remove(directory_stack, prev_directory);
            strncpy(current_path, prev_directory, sizeof(current_path) - 1);
            current_path[sizeof(current_path) - 1] = '\0';
            g_free(prev_directory);
            refresh_view();
        } else {
            log_event("Error changing to parent directory");
        }
    } else {
        log_event("No previous directory in stack");
    }
}

static void up_clicked(GtkToolButton *button, gpointer user_data) {
    char parent_path[256];
    strncpy(parent_path, current_path, sizeof(parent_path));
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash && last_slash != parent_path) {
        *last_slash = '\0';
        if (do_chdir(parent_path, "") == 0) {
            strncpy(current_path, parent_path, sizeof(current_path) - 1);
            current_path[sizeof(current_path) - 1] = '\0';
            refresh_view();
        } else {
            log_event("Error changing to parent directory");
        }
    } else {
        log_event("Already at root directory");
    }
}

static void on_selection_changed(GtkIconView *icon_view, gpointer user_data) {
    GList *selected_items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    gboolean has_selection = (selected_items != NULL);
    
    gtk_widget_set_sensitive(GTK_WIDGET(delete_button), has_selection);
    gtk_widget_set_sensitive(GTK_WIDGET(rename_button), has_selection);
    gtk_widget_set_sensitive(GTK_WIDGET(details_button), has_selection);
    if (has_selection) {
        GtkTreeIter iter;
        gboolean is_secured;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_SECURED, &is_secured, -1);
        
        gtk_widget_set_sensitive(GTK_WIDGET(secure_button), !is_secured);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(secure_button), FALSE);
    }
    if (has_selection) {
        GtkTreeIter iter;
        gboolean is_directory;
        
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, (GtkTreePath*)selected_items->data);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_IS_DIRECTORY, &is_directory, -1);
        
        gtk_widget_set_sensitive(GTK_WIDGET(read_button), !is_directory);
        gtk_widget_set_sensitive(GTK_WIDGET(write_button), !is_directory);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(read_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(write_button), FALSE);
    }
    
    g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
}

static void activate(GtkApplication *app, gpointer user_data) {
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "EDGAR's FILE SYSTEM EMULATOR");
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
    gtk_widget_set_sensitive(GTK_WIDGET(delete_button), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), delete_button, -1);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(delete_clicked), NULL);

    rename_button = gtk_tool_button_new(NULL, "Rename");
    gtk_widget_set_sensitive(GTK_WIDGET(rename_button), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), rename_button, -1);
    g_signal_connect(rename_button, "clicked", G_CALLBACK(rename_clicked), NULL);

    details_button = gtk_tool_button_new(NULL, "Details");
    gtk_widget_set_sensitive(GTK_WIDGET(details_button), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), details_button, -1);
    g_signal_connect(details_button, "clicked", G_CALLBACK(details_clicked), NULL);

    secure_button = gtk_tool_button_new(NULL, "Secure");
    gtk_widget_set_sensitive(GTK_WIDGET(secure_button), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), secure_button, -1);
    g_signal_connect(secure_button, "clicked", G_CALLBACK(secure_clicked), NULL);

    read_button = gtk_tool_button_new(NULL, "Read File");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), read_button, -1);
    g_signal_connect(read_button, "clicked", G_CALLBACK(read_clicked), NULL);

    write_button = gtk_tool_button_new(NULL, "Write to File");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), write_button, -1);
    g_signal_connect(write_button, "clicked", G_CALLBACK(write_clicked), NULL);

    GtkWidget *path_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_grid_attach(GTK_GRID(grid), path_box, 0, 1, 2, 1);

    back_button = gtk_button_new_with_label("Back");
    g_signal_connect(back_button, "clicked", G_CALLBACK(back_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(path_box), back_button, FALSE, FALSE, 0);

    path_bar = gtk_label_new(current_path);
    gtk_box_pack_start(GTK_BOX(path_box), path_bar, TRUE, TRUE, 0);

    up_button = gtk_button_new_with_label("Up");
    g_signal_connect(up_button, "clicked", G_CALLBACK(up_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(path_box), up_button, FALSE, FALSE, 0);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 2, 1, 1);

    store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN);

    icon_view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(icon_view), COL_NAME);
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(icon_view), COL_ICON);
    gtk_container_add(GTK_CONTAINER(scrolled_window), icon_view);
    gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(icon_view), 10);
    gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(icon_view), 10);
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(icon_view), 100);
    gtk_icon_view_set_columns(GTK_ICON_VIEW(icon_view), -1);

    g_signal_connect(icon_view, "selection-changed", G_CALLBACK(on_selection_changed), NULL);
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

    GtkApplication *app;
    int status;

    app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
