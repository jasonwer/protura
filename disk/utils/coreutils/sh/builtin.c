/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

// sh - shell, command line interpreter
#define UTILITY_NAME "sh"

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "shell.h"
#include "builtin.h"

struct builtin_cmd {
    const char *id;
    int (*cmd) (struct prog_desc *);
};

static char *modify_cwd(char *new_cwd, size_t *len, const char *path, size_t path_len)
{
    char *modified_cwd;

    if (path_len == 0)
        return new_cwd;

    if (strncmp(path, ".", path_len) == 0) {
        modified_cwd = new_cwd;

    } else if (strncmp(path, "..", path_len) == 0) {
        if (*len == 1)
            return new_cwd;

        while (new_cwd[*len - 1] != '/')
            (*len)--;

        if (*len > 1)
            (*len)--;

        modified_cwd = new_cwd;

    } else {
        int add_slash = 1;
        if (new_cwd[*len - 1] == '/')
            add_slash = 0;

        *len += add_slash + path_len;
        modified_cwd = malloc(*len + 1);

        strcpy(modified_cwd, new_cwd);
        if (add_slash)
            strcat(modified_cwd, "/");
        strncat(modified_cwd, path, path_len);

        free(new_cwd);
    }

    return modified_cwd;
}

static int cd(struct prog_desc *prog)
{
    int ret;
    char *cwd_new;
    size_t cwd_len = strlen(cwd);
    const char *dir_start, *dir_end;

    if (prog->argc <= 1)
        return 1;

    cwd_new = strdup(cwd);

    dir_end = prog->argv[1];

    do {
        dir_start = dir_end;

        while (*dir_end && *dir_end != '/')
            dir_end++;

        cwd_new = modify_cwd(cwd_new, &cwd_len, dir_start, dir_end - dir_start);

        if (*dir_end)
            dir_end++;
    } while (*dir_end);

    cwd_new[cwd_len] = '\0';

    ret = chdir(cwd_new);

    if (ret == -1) {
        printf("Error changing directory: %s\n", strerror(errno));
        free(cwd_new);
    } else {
        free(cwd);
        cwd = cwd_new;
    }

    return 0;
}

static int echo(struct prog_desc *prog)
{
    int i;

    if (prog->argc <= 1)
        return 0;

    dprintf(prog->stdout_fd, "%s", prog->argv[1]);

    for (i = 2; i < prog->argc; i++)
        dprintf(prog->stdout_fd, " %s", prog->argv[i]);


    write(prog->stdout_fd, &(const char){ '\n' }, 1);

    return 0;
}

static int pwd(struct prog_desc *prog)
{
    dprintf(prog->stdout_fd, "%s\n", cwd);

    return 0;
}

static struct builtin_cmd cmds[] = {
    { .id = "cd", .cmd = cd },
    { .id = "echo", .cmd = echo },
    { .id = "pwd", .cmd = pwd },
    { .id = NULL, .cmd = NULL }
};

int builtin_exec(struct prog_desc *prog, int *ret)
{
    struct builtin_cmd *cmd = cmds;

    if (!prog->file)
        return 1;

    for (; cmd->id; cmd++) {
        if (strcmp(prog->file, cmd->id) == 0) {
            int k;

            k = (cmd->cmd) (prog);

            if (ret)
                *ret = k;

            return 0;
        }
    }

    return 1;
}

