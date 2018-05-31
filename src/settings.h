#pragma once

/* Copyright (C) 2018 Alexander Chernov <cher@ejudge.ru> */

/*
 * This file is part of ejudge-fuse.
 *
 * Ejudge-fuse is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Ejudge-fuse is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ejudge-fuse.  If not, see <http://www.gnu.org/licenses/>.
 */

/* permissions for read-only directories (r-x------) */
enum { EJFUSE_DIR_PERMS = 0500 };

/* permissions for read-only files (r--------) */
enum { EJFUSE_FILE_PERMS = 0400 };

/* default directory size */
enum { EJFUSE_DIR_SIZE = 4096 };

/* server info cache timeout (in us - microseconds) */
enum { EJFUSE_CACHING_TIME = 30000000 }; // 30s

/* server error retry timeout (in us - microseconds) */
enum { EJFUSE_RETRY_TIME = 10000000 }; // 10s

