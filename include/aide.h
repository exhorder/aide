/* aide, Advanced Intrusion Detection Environment
 *
 * Copyright (C) 1999,2000,2001,2002 Rami Lehti, Pablo Virolainen
 * $Header$
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _AIDE_H_INCLUDED
#define _AIDE_H_INCLUDED

#include "report.h"
#include "db_config.h"
#include "config.h"

#ifndef __NetBSD__
#ifndef _POSIX_C_SOURCE
/* For _POSIX_THREAD_SEMANTICS _REENTRANT */
#define _POSIX_C_SOURCE 199506L
#endif /* _POSIX_C_SOURCE */
#endif /* __NetBSD__ */


#define AIDEVERSION VERSION

#define ARGUMENT_SIZE 65536

/* This is a structure that has all configuration info */
extern db_config* conf;

void print_version(void);

void usage(int);

int read_param(int argc,char**argv);

#endif
