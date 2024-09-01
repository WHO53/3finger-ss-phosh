#include <gio/gio.h>
#include <time.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    GError *error = NULL;
    GDBusConnection *connection;
    GVariant *result;
    char filename[64];
    time_t t;
    struct tm *tm_info;

    // Get the current time
    t = time(NULL);
    tm_info = localtime(&t);

    // Format the filename as "current-time.png"
    strftime(filename, sizeof(filename), "current-time-%Y%m%d-%H%M%S.png", tm_info);

    // Get a connection to the session bus
    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!connection) {
        g_printerr("Failed to connect to the session bus: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    // Call the Screenshot method
    result = g_dbus_connection_call_sync(
        connection,
        "org.gnome.Shell.Screenshot",            // destination bus name
        "/org/gnome/Shell/Screenshot",           // object path
        "org.gnome.Shell.Screenshot",            // interface name
        "Screenshot",                            // method name
        g_variant_new("(bbs)", FALSE, TRUE, filename),  // parameters
        G_VARIANT_TYPE("(bs)"),                  // expected reply type
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error) {
        g_printerr("Failed to call Screenshot method: %s\n", error->message);
        g_error_free(error);
        g_object_unref(connection);
        return 1;
    }

    // Handle the result (in this case, print the result)
    gboolean success;
    const gchar *screenshot_filename;
    g_variant_get(result, "(bs)", &success, &screenshot_filename);

    if (success) {
        g_print("Screenshot saved as: %s\n", screenshot_filename);
    } else {
        g_print("Failed to take screenshot\n");
    }

    // Clean up
    g_variant_unref(result);
    g_object_unref(connection);

    return 0;
}
