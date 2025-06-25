// SPDX-License-Identifier: MIT
// Mode handlers.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "mode.h"

#include "application.h"
#include "imglist.h"
#include "info.h"
#include "shellcmd.h"
#include "ui.h"

#include <stdlib.h>
#include <string.h>

/**
 * Execute system command for the specified images.
 * @param expr command expression
 * @param path file paths to substitute into expression
 */
static void execute_cmd(const char* expr, const char** paths)
{
    const size_t max_status = 60;
    struct array* out = NULL;
    struct array* err = NULL;
    char* msg = NULL;
    char* cmd;
    int rc;

    // contruct and execute command
    cmd = shellcmd_expr(expr, paths);
    if (!cmd) {
        info_update(info_status, "Error: no command to execute");
        app_redraw();
        return;
    }
    rc = shellcmd_exec(cmd, &out, &err);

    // duplicate output to stdout/stderr
    if (out) {
        fprintf(stdout, "%.*s", (int)out->size, out->data);
    }
    if (err) {
        fprintf(stderr, "%.*s", (int)err->size, err->data);
    }

    // show execution status
    if (rc == 0) {
        if (out) {
            str_append((const char*)out->data, out->size, &msg);
        } else {
            str_dup("Success: ", &msg);
            str_append(cmd, 0, &msg);
        }
    } else if (rc == SHELLCMD_TIMEOUT) {
        str_dup("Child process timed out: ", &msg);
        str_append(cmd, 0, &msg);
    } else {
        char desc[256];
        snprintf(desc, sizeof(desc), "Error %d: ", rc);
        str_dup(desc, &msg);
        if (err) {
            str_append((const char*)err->data, err->size, &msg);
        } else if (out) {
            str_append((const char*)out->data, out->size, &msg);
        } else {
            str_append(strerror(rc), 0, &msg);
        }
    }
    if (strlen(msg) > max_status) {
        // trim long output text
        const char ellipsis[] = "...";
        memcpy(msg + max_status - sizeof(ellipsis), ellipsis, sizeof(ellipsis));
    }

    info_update(info_status, "%s", msg);

    free(cmd);
    free(msg);
    arr_free(out);
    arr_free(err);

    app_redraw();
}

static const char** collect_paths(void)
{
    size_t paths_num = 0;
    for (struct image* img = imglist_first(); img; img = imglist_next(img, false)) {
        if (img->marked) {
            ++paths_num;
        }
    }
    if (paths_num == 0) {
        return NULL;
    }

    const char** paths = malloc((paths_num + 1) * sizeof(*paths));
    if (!paths) {
        return NULL;
    }

    const char** next_path = paths;
    for (struct image* img = imglist_first(); img; img = imglist_next(img, false)) {
        if (img->marked) {
            *next_path = img->source;
            ++next_path;
        }
    }
    *next_path = NULL;
    return paths;
}

void mode_handle(struct mode* mode, const struct action* action)
{
    switch (action->type) {
        case action_info:
            info_switch(action->params);
            app_redraw();
            break;
        case action_status:
            info_update(info_status, "%s", action->params);
            app_redraw();
            break;
        case action_fullscreen:
            ui_toggle_fullscreen();
            break;
        case action_mode:
            app_switch_mode(action->params);
            break;
        case action_exec:
            {
                const char* paths[] = {
                    mode->get_current()->source,
                    NULL,
                };
                execute_cmd(action->params, paths);
            }
            break;
        case action_mark:
            info_update_mark(image_toggle_marked(mode->get_current()));
            app_redraw();
            break;
        case action_exec_marked:
            {
                const char** paths = collect_paths();
                if (paths) {
                    execute_cmd(action->params, paths);
                    free(paths);
                }
            }
            break;
        case action_help:
            if (help_visible()) {
                help_hide();
            } else {
                help_show(mode->get_keybinds());
            }
            app_redraw();
            break;
        case action_exit:
            if (help_visible()) {
                help_hide();
                app_redraw();
            } else {
                app_exit(0);
            }
            break;
        default:
            if (!mode->handle_action(action)) {
                info_update(info_status, "Unhandled action: %s",
                            action_typename(action));
                app_redraw();
            }
            break;
    }
}
