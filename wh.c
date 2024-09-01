#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wayland-client.h>

struct output_data {
    int32_t width;
    int32_t height;
    int32_t orientation;
    int done;
};

static void output_geometry(void *data, struct wl_output *wl_output,
                            int32_t x, int32_t y,
                            int32_t physical_width, int32_t physical_height,
                            int32_t subpixel, const char *make,
                            const char *model, int32_t transform) {
    struct output_data *output = data;
    output->orientation = transform;
    printf("Output geometry received. Transform: %d\n", transform);
}

static void output_mode(void *data, struct wl_output *wl_output,
                        uint32_t flags, int32_t width, int32_t height,
                        int32_t refresh) {
    struct output_data *output = data;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        output->width = width;
        output->height = height;
        printf("Output mode: %dx%d\n", width, height);
    }
}

static void output_done(void *data, struct wl_output *wl_output) {
    struct output_data *output = data;
    output->done = 1;
    printf("Output done received\n");
}

static void output_scale(void *data, struct wl_output *wl_output,
                         int32_t factor) {
    printf("Output scale: %d\n", factor);
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) {
    struct output_data *output = data;
    if (strcmp(interface, "wl_output") == 0) {
        struct wl_output *wl_output = wl_registry_bind(registry, name, &wl_output_interface, 2);
        wl_output_add_listener(wl_output, &output_listener, output);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name) {
    // Not needed for this example
}

const char* get_orientation_string(int32_t orientation) {
    switch (orientation) {
        case WL_OUTPUT_TRANSFORM_NORMAL: return "Normal";
        case WL_OUTPUT_TRANSFORM_90: return "90 degrees";
        case WL_OUTPUT_TRANSFORM_180: return "180 degrees";
        case WL_OUTPUT_TRANSFORM_270: return "270 degrees";
        case WL_OUTPUT_TRANSFORM_FLIPPED: return "Flipped";
        case WL_OUTPUT_TRANSFORM_FLIPPED_90: return "Flipped 90 degrees";
        case WL_OUTPUT_TRANSFORM_FLIPPED_180: return "Flipped 180 degrees";
        case WL_OUTPUT_TRANSFORM_FLIPPED_270: return "Flipped 270 degrees";
        default: return "Unknown";
    }
}

int main() {
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    struct output_data output = {0};
    struct wl_registry *registry = wl_display_get_registry(display);
    struct wl_registry_listener registry_listener = {
        .global = registry_global,
        .global_remove = registry_global_remove
    };

    wl_registry_add_listener(registry, &registry_listener, &output);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    // Keep dispatching events until we get the output information
    while (!output.done) {
        wl_display_dispatch(display);
    }

    if (output.width && output.height) {
        printf("Screen size: %dx%d\n", output.width, output.height);
        printf("Orientation: %s\n", get_orientation_string(output.orientation));
    } else {
        fprintf(stderr, "Failed to get screen information\n");
    }

    wl_display_disconnect(display);
    return 0;
}
