/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#pragma once

#include "cdefs.h"
#include "stdint.h"

__DECL_BEGIN

typedef uint32_t size_t;
typedef int32_t ssize_t;
typedef int pid_t;
typedef int tid_t;
typedef unsigned long ino_t;
typedef short dev_t;
typedef uint32_t mode_t;
typedef unsigned short nlink_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;
typedef long off_t;
typedef long blksize_t;
typedef long blkcnt_t;

typedef long suseconds_t;
typedef unsigned long useconds_t;

typedef uint8_t u_char;
typedef uint16_t u_short;
typedef uint32_t u_int;
typedef ino_t u_long;

__DECL_END