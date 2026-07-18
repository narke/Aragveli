/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

void exit(int status);
int wait(int *status);
int exec(const char *path, char *const argv[]);
int fork(void);
