/* aide, Advanced Intrusion Detection Environment
 *
 * Copyright (C) 1999,2000,2001,2002 Rami Lehti,Pablo Virolainen
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

#ifndef _GEN_LIST_H_INCLUDED
#define _GEN_LIST_H_INCLUDED
#include "seltree.h"
#include "list.h"
#include "gnu_regex.h"

/* DB_FOO are anded together to form rx_rule's attr */

typedef struct rx_rule {
  char* rx; /* Regular expression in text form */
  regex_t* crx; /* Compiled regexp */
  int attr; /* Which attributes to save */
  long  conf_lineno; /* line no. of rule definition*/
} rx_rule;

/*
 * gen_list()
 * Generates the file list
 * from the lists of rx_rule's
 */

list* gen_list(list* prxlist,list* nrxlist,list* erxlist);

/* 
 * gen_tree()
 * Generates the file tree
 * from rx_rule's
 */
seltree* gen_tree(list* prxlist,list* nrxlist,list* erxlist);

/* 
 * populate_tree()
 * Populate tree with data from disk and db 
 * Also do comparing while adding to the tree
 */
void populate_tree(seltree* tree);

/*
 * strrxtok()
 * return a pointer to a copy of the non-regexp path part of the argument
 */

char* strrxtok(char*);

int check_list_for_match(list*,char*,int*);

int check_rxtree(char* filename,seltree* tree, int* attr);

db_line* get_file_attrs(char* filename,int attr);

seltree* get_seltree_node(seltree* tree,char* path);

#endif /*_GEN_LIST_H_INCLUDED*/