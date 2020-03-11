/**************************************************************************
 *
 * Copyright (c) 2004-18 Simon Peter
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#ident "AppImage by Simon Peter, http://appimage.org/"

/*
 * Optional daempon to watch directories for AppImages
 * and register/unregister them with the system
 *
 * TODO (feel free to send pull requests):
 * - Switch to https://developer.gnome.org/gio/stable/GFileMonitor.html (but with subdirectories)
 *   which would drop the dependency on libinotifytools.so.0
 * - Add and remove subdirectories on the fly at runtime -
 *   see https://github.com/paragone/configure-via-inotify/blob/master/inotify/src/inotifywatch.c
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <mntent.h>

#include <inotifytools/inotifytools.h>
#include <inotifytools/inotify.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <pthread.h>

#include <appimage/appimage.h>
#include <xdg-basedir.h>

#include "notify.h"

#ifndef RELEASE_NAME
#define RELEASE_NAME "continuous build"
#endif

static gboolean verbose = FALSE;
static gboolean showVersionOnly = FALSE;
static gboolean install = FALSE;
static gboolean uninstall = FALSE;
static gboolean no_install = FALSE;
static GMutex print_mutex;
static GMutex time_mutex;
static const gint64 time_update_interval = 3 * 1000000; // 3 seconds (in microseconds)
static gint64 time_last_change = 0; // in microseconds
static gint64 time_last_update = 0; // in microseconds
static const gchar* program_update_desktop = "update-desktop-database";
static const gchar* program_update_mime = "update-mime-database";
static const gchar* program_gtk_update_icon_cache = "gtk-update-icon-cache";
static const gchar* program_kbuildsycoca5 = "kbuildsycoca5";
static const gchar* cmd_update_desktop = "update-desktop-database ~/.local/share/applications/";
static const gchar* cmd_update_mime = "update-mime-database ~/.local/share/mime/";
static const gchar* cmd_gtk_update_icon_cache = "gtk-update-icon-cache ~/.local/share/icons/hicolor/ -t";
static const gchar* cmd_kbuildsycoca5 = "kbuildsycoca5";
static gboolean is_update_desktop_available = FALSE;
static gboolean is_update_mime_available = FALSE;
static gboolean is_gtk_update_icon_cache_available = FALSE;
static gboolean is_kbuildsycoca5_available = FALSE;
gchar** remaining_args = NULL;

static GOptionEntry entries[] =
    {
        {"verbose",          'v', 0, G_OPTION_ARG_NONE,           &verbose,         "Be verbose",                                 NULL},
        {"install",          'i', 0, G_OPTION_ARG_NONE,           &install,         "Install this appimaged instance to $HOME",   NULL},
        {"uninstall",        'u', 0, G_OPTION_ARG_NONE,           &uninstall,       "Uninstall an appimaged instance from $HOME", NULL},
        {"no-install",       'n', 0, G_OPTION_ARG_NONE,           &no_install,      "Force run without installation",             NULL},
        {"version",          0,   0, G_OPTION_ARG_NONE,           &showVersionOnly, "Show version number",                        NULL},
        {G_OPTION_REMAINING, 0,   0, G_OPTION_ARG_FILENAME_ARRAY, &remaining_args, NULL},
        {NULL}
    };

#define EXCLUDE_CHUNK 1024
#define WR_EVENTS (IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF)

// to ensure we don't garble stdout, we have to use this in the threads
#define THREADSAFE_G_PRINT(str, ...) \
    g_mutex_lock(&print_mutex);\
    g_print(str, ##__VA_ARGS__); \
    g_mutex_unlock(&print_mutex)

/* Run the actual work in treads;
 * pthread allows to pass only one argument to the thread function,
 * hence we use a struct as the argument in which the real arguments are */
struct arg_struct {
    char* path;
    gboolean verbose;
};

void update_desktop() {
    const gchar* error_msgfmt = "Warning: %s retuned non-zero exit code:\n";
    if (is_update_desktop_available && system(cmd_update_desktop) != 0) {
        THREADSAFE_G_PRINT(error_msgfmt, program_update_desktop);
    }
    if (is_update_mime_available && system(cmd_update_mime) != 0) {
        THREADSAFE_G_PRINT(error_msgfmt, program_update_mime);
    }
    if (is_gtk_update_icon_cache_available && system(cmd_gtk_update_icon_cache) != 0) {
        THREADSAFE_G_PRINT(error_msgfmt, program_gtk_update_icon_cache);
    }
    if (is_kbuildsycoca5_available && system(cmd_kbuildsycoca5) != 0) {
        THREADSAFE_G_PRINT(error_msgfmt, program_kbuildsycoca5);
    }
}

void update_desktop_set_dirty() {
    gint64 time = g_get_real_time();
    g_mutex_lock(&time_mutex);
    time_last_change = time;
    // this is very unlikely to happen, but theoretically possible due to
    // timer precision. in such an unlikely case we want to just run the update again.
    if (time_last_change == time_last_update) {
        time_last_change++;
    }
    g_mutex_unlock(&time_mutex);
}

bool is_appimage(char* path, gboolean verbose) {
    return appimage_get_type(path, verbose) != -1;
}


GKeyFile* load_desktop_entry(const char* desktop_file_path) {
    GKeyFile* key_file_structure = g_key_file_new();
    gboolean success = g_key_file_load_from_file(key_file_structure, desktop_file_path,
                                                 G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    if (!success) {
        // Don't remove the brackets or the macro will segfault
        THREADSAFE_G_PRINT("Failed to load the deployed desktop entry, '%s'\n", "a");
    }

    return key_file_structure;
}

void setup_firejail_on_desktop_entry(GKeyFile* key_file_structure) {
    char* oldExecValue = g_key_file_get_value(key_file_structure,
                                              G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);

    char* firejail_exec = g_strdup_printf("firejail --env=DESKTOPINTEGRATION=appimaged --noprofile --appimage %s",
                                          oldExecValue);
    g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, firejail_exec);

    gchar* firejail_profile_group = "Desktop Action FirejailProfile";
    gchar* firejail_profile_exec = g_strdup_printf(
        "firejail --env=DESKTOPINTEGRATION=appimaged --private --appimage %s", oldExecValue);
    gchar* firejail_tryexec = "firejail";
    g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_NAME,
                         "Run without sandbox profile");
    g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_EXEC,
                         firejail_profile_exec);
    g_key_file_set_value(key_file_structure, firejail_profile_group, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC,
                         firejail_tryexec);
    g_key_file_set_value(key_file_structure, G_KEY_FILE_DESKTOP_GROUP, "Actions", "FirejailProfile;");

    g_free(firejail_profile_exec);
    g_free(oldExecValue);
    g_free(firejail_exec);
}

void save_desktop_entry(GKeyFile* key_file_structure, const char* desktop_file_path) {
    gboolean success = g_key_file_save_to_file(key_file_structure, desktop_file_path, NULL);

    if (!success) {
        // Don't remove the brackets or the macro will segfault
        THREADSAFE_G_PRINT("Failed to save the deployed desktop entry\n");
    }
}


void enable_firejail_if_available(const char* appimage_path) {
    /* If firejail is on the $PATH, then use it to run AppImages */
    char* firejail_paht = g_find_program_in_path("firejail");
    if (firejail_paht) {
        const char* desktop_file_path = appimage_registered_desktop_file_path(appimage_path, NULL, false);

        if (desktop_file_path != NULL && g_file_test(desktop_file_path, G_FILE_TEST_EXISTS)) {
            GKeyFile* key_file_structure = load_desktop_entry(desktop_file_path);

            setup_firejail_on_desktop_entry(key_file_structure);
            save_desktop_entry(key_file_structure, desktop_file_path);

            g_key_file_unref(key_file_structure);
            g_free(firejail_paht);
        }

        g_free(desktop_file_path);
    }
}

void* thread_appimage_register_in_system(void* arguments) {
    struct arg_struct* args = arguments;
    if (args->verbose) {
        THREADSAFE_G_PRINT("%s (%s)\n", __FUNCTION__, args->path);
    }

    bool is_appimage_result = is_appimage(args->path, args->verbose);
    bool appimage_is_registered_in_system_result = is_appimage_result && appimage_is_registered_in_system(args->path);
    if (is_appimage_result && !appimage_is_registered_in_system_result) {
        int failed = appimage_register_in_system(args->path, args->verbose);

        if (!failed) {
            enable_firejail_if_available(args->path);
            update_desktop_set_dirty();
        }

        if (args->verbose) {
            THREADSAFE_G_PRINT("appimage_register_in_system result: %d\n", failed);
        }

    } else if (args->verbose) {
        THREADSAFE_G_PRINT("appimage_register_in_system call skipped. "
                           "is_appimage_result: %d appimage_is_registered_in_system_result: %d\n",
                           is_appimage_result, appimage_is_registered_in_system_result);
    }

    pthread_exit(NULL);
}

void* thread_appimage_unregister_in_system(void* arguments) {
    struct arg_struct* args = arguments;
    if (args->verbose) {
        THREADSAFE_G_PRINT("%s (%s)\n", __FUNCTION__, args->path);
    }

    bool result = appimage_unregister_in_system(args->path, args->verbose);
    if (args->verbose) {
        THREADSAFE_G_PRINT("appimage_unregister_in_system (%s): $d\n", __FUNCTION__, args->path, result);
    }
    update_desktop_set_dirty();
    pthread_exit(NULL);
}

// thread which checks if an update of the desktop is necessary and updates it accordingly.
void* thread_update_desktop() {
    while (TRUE) {
        gboolean do_update = FALSE;

        // update only after a specific interval (time_update_interval) has passed since the last change.
        // the lock is here to ensure that the desktop is never in an inconsistent state.
        g_mutex_lock(&time_mutex);
        if (time_last_change != time_last_update && g_get_real_time() > time_last_change + time_update_interval) {
            time_last_update = g_get_real_time();
            time_last_change = time_last_update;
            do_update = TRUE;
        }
        g_mutex_unlock(&time_mutex);

        if (do_update) {
            THREADSAFE_G_PRINT("Updating desktop...\n");
            gint64 update_start = g_get_real_time();
            update_desktop();
            gint64 update_end = g_get_real_time();
            THREADSAFE_G_PRINT("Finished updating desktop in %ld milliseconds.\n", (update_end - update_start) / 1000);
        }

        // sleep one second
        g_usleep(1000000);
    }
}

// check the availability of a single program in the $PATH.
gboolean check_for_program(const gchar* program_name) {
    gboolean result = FALSE;

    gchar* tmp = g_find_program_in_path(program_name);
    if (tmp != NULL) {
        g_free(tmp);
        result = TRUE;
    }

    return result;
}

// check if update programs are available
void check_update_programs() {
    is_update_desktop_available = check_for_program(program_update_desktop);
    is_update_mime_available = check_for_program(program_update_mime);
    is_gtk_update_icon_cache_available = check_for_program(program_gtk_update_icon_cache);
    is_kbuildsycoca5_available = check_for_program(program_kbuildsycoca5);
}

/* Recursively process the files in this directory and its subdirectories,
 * http://stackoverflow.com/questions/8436841/how-to-recursively-list-directories-in-c-on-linux
 */
void initially_register(const char* name, int level) {
    DIR* dir;
    struct dirent* entry;

    if (!(dir = opendir(name))) {
        if (verbose) {
            if (errno == EACCES) {
                THREADSAFE_G_PRINT("_________________________\nPermission denied on dir '%s'\n", name);
            } else {
                THREADSAFE_G_PRINT("_________________________\nFailed to open dir '%s'\n", name);
            }
        }
        closedir(dir);
        return;
    }

    if (!(entry = readdir(dir))) {
        if (verbose) {
            THREADSAFE_G_PRINT("_________________________\nInvalid directory stream descriptor '%s'\n", name);
        }
        closedir(dir);
        return;
    }

    do {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            int len = snprintf(path, sizeof(path) - 1, "%s/%s", name, entry->d_name);
            path[len] = 0;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            initially_register(path, level + 1);
        } else {
            int ret;
            gchar* absolute_path = g_build_path(G_DIR_SEPARATOR_S, name, entry->d_name, NULL);
            if (g_file_test(absolute_path, G_FILE_TEST_IS_REGULAR)) {
                pthread_t some_thread;
                struct arg_struct args;
                args.path = absolute_path;
                args.verbose = verbose;
                ret = pthread_create(&some_thread, NULL, thread_appimage_register_in_system, &args);
                if (!ret) {
                    pthread_join(some_thread, NULL);
                }
            }
            g_free(absolute_path);
        }
    } while ((entry = readdir(dir)) != NULL);
    closedir(dir);
}

void add_dir_to_watch(const char* directory) {
    GError* err = NULL;

    // XXX: Fails silently if file doesn’t exist.  Maybe log?
    if (NULL == directory || g_file_test(directory, G_FILE_TEST_EXISTS)) {
        return;
    }

    // Follow symlinks.
    const char* realdir = g_file_test(directory, G_FILE_TEST_IS_SYMLINK)
      ? g_file_read_link(directory, &err)
      : directory;

    if (NULL != err) {
        THREADSAFE_G_PRINT("Error #%d following symlink %s: %s\n",
                           err->code, directory, err->message);
        return;
    }

    if (g_file_test(realdir, G_FILE_TEST_IS_DIR)) {
        if (!inotifytools_watch_file(realdir, WR_EVENTS)) {
            fprintf(stderr, "%s: %s\n", realdir, strerror(inotifytools_error()));
            exit(1);
        }
        initially_register(realdir, 0);
        THREADSAFE_G_PRINT("Watching %s\n", realdir);
    }
}

void handle_event(struct inotify_event* event) {
    int ret;
    gchar* absolute_path = g_build_path(G_DIR_SEPARATOR_S, inotifytools_filename_from_wd(event->wd), event->name, NULL);

    if ((event->mask & IN_CLOSE_WRITE) | (event->mask & IN_MOVED_TO)) {
        if (g_file_test(absolute_path, G_FILE_TEST_IS_REGULAR)) {
            pthread_t some_thread;
            struct arg_struct args;
            args.path = absolute_path;
            args.verbose = verbose;
            g_print("_________________________\n");
            ret = pthread_create(&some_thread, NULL, thread_appimage_register_in_system, &args);
            if (!ret) {
                pthread_join(some_thread, NULL);
            }
        }
    }

    if ((event->mask & IN_MOVED_FROM) | (event->mask & IN_DELETE)) {
        pthread_t some_thread;
        struct arg_struct args;
        args.path = absolute_path;
        args.verbose = verbose;
        g_print("_________________________\n");
        ret = pthread_create(&some_thread, NULL, thread_appimage_unregister_in_system, &args);
        if (!ret) {
            pthread_join(some_thread, NULL);
        }
    }

    g_free(absolute_path);

    /* Too many FS events were received, some event notifications were potentially lost */
    if (event->mask & IN_Q_OVERFLOW) {
        printf("Warning: AN OVERFLOW EVENT OCCURRED\n");
    }

    if (event->mask & IN_IGNORED) {
        printf("Warning: AN IN_IGNORED EVENT OCCURRED\n");
    }

}

int main(int argc, char** argv) {
    GError* error = NULL;
    GOptionContext* context;

    context = g_option_context_new("");
    g_option_context_add_main_entries(context, entries, NULL);
    // g_option_context_add_group (context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        exit(1);
    }

    // always show version, but exit immediately if only the version number was requested
    fprintf(
        stderr,
        "appimaged, %s (commit %s), build %s built on %s\n",
        RELEASE_NAME, APPIMAGED_GIT_COMMIT, APPIMAGED_BUILD_NUMBER, APPIMAGED_BUILD_DATE
    );

    if (showVersionOnly)
        exit(0);

    if (!inotifytools_initialize()) {
        fprintf(stderr, "inotifytools_initialize error\n");
        exit(1);
    }

    gchar* user_bin_dir = g_build_filename(g_get_home_dir(), "/.local/bin", NULL);
    gchar* installed_appimaged_location = g_build_filename(user_bin_dir, "appimaged", NULL);
    const gchar* appimage_location = g_getenv("APPIMAGE");
    gchar* own_desktop_file_location = g_build_filename(g_getenv("APPDIR"), "/appimaged.desktop", NULL);
    gchar* global_autostart_file = "/etc/xdg/autostart/appimaged.desktop";
    gchar* global_systemd_file = "/usr/lib/systemd/user/appimaged.service";
    gchar* partial_path = g_strdup_printf("autostart/appimagekit-appimaged.desktop");
    char* config_home = xdg_config_home();
    gchar* destination = g_build_filename(config_home, partial_path, NULL);
    free(config_home);

    if (uninstall) {
        if (g_file_test(installed_appimaged_location, G_FILE_TEST_EXISTS))
            fprintf(stderr, "* Please delete %s\n", installed_appimaged_location);
        if (g_file_test(destination, G_FILE_TEST_EXISTS))
            fprintf(stderr, "* Please delete %s\n", destination);
        fprintf(stderr, "* To remove all AppImage desktop integration, run\n");
        fprintf(stderr, "  find ~/.local/share -name 'appimagekit_*' -exec rm {} \\;\n\n");
        exit(0);
    }

    if (install) {
        if (((appimage_location != NULL)) && ((own_desktop_file_location != NULL))) {
            printf("Running from within %s\n", appimage_location);
            if ((!g_file_test("/usr/bin/appimaged", G_FILE_TEST_EXISTS)) &&
                (!g_file_test(global_autostart_file, G_FILE_TEST_EXISTS)) &&
                (!g_file_test(global_systemd_file, G_FILE_TEST_EXISTS))) {
                printf("%s is not installed, moving it to %s\n", argv[0], installed_appimaged_location);
                g_mkdir_with_parents(user_bin_dir, 0755);
                gchar* command = g_strdup_printf("mv \"%s\" \"%s\"", appimage_location, installed_appimaged_location);
                system(command);
                /* When appimaged installs itself, then to the $XDG_CONFIG_HOME/autostart/ directory, falling back to ~/.config/autostart/ */
                fprintf(stderr, "Installing to autostart: %s\n", own_desktop_file_location);
                g_mkdir_with_parents(g_path_get_dirname(destination), 0755);
                gchar* command2 = g_strdup_printf("cp \"%s\" \"%s\"", own_desktop_file_location, destination);
                system(command2);
                if (g_file_test(installed_appimaged_location, G_FILE_TEST_EXISTS))
                    fprintf(stderr, "* Installed %s\n", installed_appimaged_location);
                if (g_file_test(destination, G_FILE_TEST_EXISTS)) {
                    gchar* command3 = g_strdup_printf("sed -i -e 's|^Exec=.*|Exec=%s|g' '%s'",
                                                      installed_appimaged_location, destination);
                    if (verbose)
                        fprintf(stderr, "%s\n", command3);
                    system(command3);
                    fprintf(stderr, "* Installed %s\n", destination);
                }
                if (g_file_test(installed_appimaged_location, G_FILE_TEST_EXISTS))
                    fprintf(stderr, "\nTo uninstall, run %s --uninstall and follow the instructions\n\n",
                            installed_appimaged_location);
                char* title;
                char* body;
                title = g_strdup_printf("Please log out");
                body = g_strdup_printf("and log in again to complete the installation");
                notify(title, body, 15);
                exit(0);
            }
        } else {
            printf("Not running from within an AppImage. This binary cannot be installed in this way.\n");
            exit(1);
        }
    }

    /* When we run from inside an AppImage, then we check if we are installed
     * in a per-user location and if not, we install ourselves there */
    if (!no_install && (appimage_location != NULL && own_desktop_file_location != NULL)) {
        if ((!g_file_test("/usr/bin/appimaged", G_FILE_TEST_EXISTS)) &&
            ((!g_file_test(global_autostart_file, G_FILE_TEST_EXISTS)) ||
             (!g_file_test(destination, G_FILE_TEST_EXISTS))) &&
            (!g_file_test(global_systemd_file, G_FILE_TEST_EXISTS)) &&
            (!g_file_test(installed_appimaged_location, G_FILE_TEST_EXISTS)) &&
            (g_file_test(own_desktop_file_location, G_FILE_TEST_IS_REGULAR))) {
            char* title;
            char* body;
            title = g_strdup_printf("Not installed\n");
            body = g_strdup_printf("Please run %s --install", argv[0]);
            notify(title, body, 15);
            exit(1);
        }
    }

    // check which update programs are available.
    check_update_programs();
    
    // Workaround for: Directory '/home/me/.local/share/mime/packages' does not exist! # https://github.com/AppImage/appimaged/issues/93
    g_mkdir_with_parents(g_build_filename(g_get_home_dir(), ".local/share/mime/packages", NULL), 0755);

    // set the time
    time_last_update = g_get_real_time();
    time_last_change = time_last_update;

    // launch the update thread
    pthread_t update_thread;
    if (pthread_create(&update_thread, NULL, thread_update_desktop, NULL) != 0) {
        THREADSAFE_G_PRINT("Failed to create update thread.");
        exit(1);
    }

    add_dir_to_watch(user_bin_dir);
    add_dir_to_watch(g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD));
    add_dir_to_watch(g_build_filename(g_get_home_dir(), "/bin", NULL));
    add_dir_to_watch(g_build_filename(g_get_home_dir(), "/.bin", NULL));
    add_dir_to_watch(g_build_filename(g_get_home_dir(), "/Applications", NULL));
    add_dir_to_watch(g_build_filename("/Applications", NULL));
    add_dir_to_watch(g_build_filename("/opt", NULL));
    add_dir_to_watch(g_build_filename("/usr/local/bin", NULL));

    // Watch "/Applications" on all mounted partitions, if it exists.
    // TODO: Notice when partitions are mounted and unmounted (patches welcome!)
    struct mntent* ent;
    FILE* aFile;
    aFile = setmntent("/proc/mounts", "r");
    if (aFile == NULL) {
        perror("setmntent");
        exit(1);
    }
    while (NULL != (ent = getmntent(aFile))) {
        gchar* applicationsdir = NULL;
        applicationsdir = g_build_filename(ent->mnt_dir, "Applications", NULL);
        if (applicationsdir != NULL) {
            if (g_file_test(applicationsdir, G_FILE_TEST_IS_DIR)) {
                add_dir_to_watch(applicationsdir);
            }
        }
        g_free(applicationsdir);
    }
    endmntent(aFile);

    struct inotify_event* event = inotifytools_next_event(-1);
    while (event) {
        if (verbose) {
            inotifytools_printf(event, "%w%f %e\n");
        }
        fflush(stdout);
        handle_event(event);
        fflush(stdout);
        event = inotifytools_next_event(-1);
    }
}
