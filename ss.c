#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libinput.h>
#include <libudev.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#define MAXSLOTS 20
#define NOMOTION -999999

static int screenwidth = 1920;  // Default values, adjust as needed
static int screenheight = 1080;
static const char *device = "/dev/input/event2";  // Adjust this to your touchpad device
static int distancethreshold = 200;

double xstart[MAXSLOTS], ystart[MAXSLOTS];
double xend[MAXSLOTS], yend[MAXSLOTS];
int active_slots[MAXSLOTS] = {0};
unsigned nfdown = 0;

void take_screenshot() {
    GError *error = NULL;
    GDBusConnection *connection;
    GVariant *result;
    gchar *filename;
    time_t now;
    struct tm *local;

    now = time(NULL);
    local = localtime(&now);
    filename = g_strdup_printf("screenshot-%04d%02d%02d-%02d%02d%02d.png",
                               local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                               local->tm_hour, local->tm_min, local->tm_sec);

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!connection) {
        g_printerr("Failed to connect to session bus: %s", error->message);
        g_error_free(error);
        g_free(filename);
        return;
    }

    result = g_dbus_connection_call_sync(
        connection,
        "org.gnome.Shell.Screenshot",
        "/org/gnome/Shell/Screenshot",
        "org.gnome.Shell.Screenshot",
        "Screenshot",
        g_variant_new("(bbs)", FALSE, TRUE, filename),
        G_VARIANT_TYPE("(bs)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error) {
        g_printerr("Failed to take screenshot: %s", error->message);
        g_error_free(error);
    } else {
        gboolean success;
        const gchar *saved_filename;
        g_variant_get(result, "(bs)", &success, &saved_filename);
        
        if (success) {
            g_print("Screenshot saved as: %s", saved_filename);
        } else {
            g_print("Failed to take screenshot");
        }
        
        g_variant_unref(result);
    }

    g_free(filename);
    g_object_unref(connection);
}

static int open_restricted(const char *path, int flags, void *user_data) {
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    close(fd);
}

void handle_touch_down(struct libinput_event *event) {
    struct libinput_event_touch *touch_event = libinput_event_get_touch_event(event);
    int slot = libinput_event_touch_get_slot(touch_event);
    
    if (slot < MAXSLOTS) {
        xstart[slot] = libinput_event_touch_get_x_transformed(touch_event, screenwidth);
        ystart[slot] = libinput_event_touch_get_y_transformed(touch_event, screenheight);
        active_slots[slot] = 1;
        nfdown++;
    }
}

void handle_touch_up(struct libinput_event *event) {
    struct libinput_event_touch *touch_event = libinput_event_get_touch_event(event);
    int slot = libinput_event_touch_get_slot(touch_event);
    
    if (slot < MAXSLOTS && active_slots[slot]) {
        active_slots[slot] = 0;
        nfdown--;

        if (nfdown == 0 && xend[slot] != NOMOTION && yend[slot] != NOMOTION) {
            double total_y_diff = 0;
            int valid_touches = 0;

            for (int i = 0; i < MAXSLOTS; i++) {
                if (yend[i] != NOMOTION && ystart[i] != NOMOTION) {
                    total_y_diff += yend[i] - ystart[i];
                    valid_touches++;
                }
            }

            if (valid_touches == 3 && total_y_diff > distancethreshold) {
                take_screenshot();
            }

            // Reset all slots
            for (int i = 0; i < MAXSLOTS; i++) {
                xstart[i] = ystart[i] = xend[i] = yend[i] = NOMOTION;
            }
        }
    }
}

void handle_touch_motion(struct libinput_event *event) {
    struct libinput_event_touch *touch_event = libinput_event_get_touch_event(event);
    int slot = libinput_event_touch_get_slot(touch_event);
    
    if (slot < MAXSLOTS && active_slots[slot]) {
        xend[slot] = libinput_event_touch_get_x_transformed(touch_event, screenwidth);
        yend[slot] = libinput_event_touch_get_y_transformed(touch_event, screenheight);
    }
}

int main() {
    struct libinput *li;
    struct libinput_event *event;

    const struct libinput_interface interface = {
        .open_restricted = open_restricted,
        .close_restricted = close_restricted,
    };

    li = libinput_path_create_context(&interface, NULL);
    if (!li) {
        g_printerr("Failed to create libinput context\n");
        return 1;
    }

    if (libinput_path_add_device(li, device) == NULL) {
        g_printerr("Failed to add device: %s\n", strerror(errno));
        libinput_unref(li);
        return 1;
    }

    // Initialize all slots to NOMOTION
    for (int i = 0; i < MAXSLOTS; i++) {
        xstart[i] = ystart[i] = xend[i] = yend[i] = NOMOTION;
    }

    while (1) {
        libinput_dispatch(li);
        while ((event = libinput_get_event(li)) != NULL) {
            switch (libinput_event_get_type(event)) {
                case LIBINPUT_EVENT_TOUCH_DOWN:
                    handle_touch_down(event);
                    break;
                case LIBINPUT_EVENT_TOUCH_UP:
                    handle_touch_up(event);
                    break;
                case LIBINPUT_EVENT_TOUCH_MOTION:
                    handle_touch_motion(event);
                    break;
                default:
                    break;
            }
            libinput_event_destroy(event);
        }
    }

    libinput_unref(li);
    return 0;
}
