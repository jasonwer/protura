/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_A_OUT_H
#define INCLUDE_FS_A_OUT_H

#include <protura/types.h>
#include <protura/compiler.h>

struct exec {
    
} __packed;

void aout_register(void);
void aout_unregister(void);

#endif
