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

#include "../config.h"
#define _POSIX_C_SOURCE 199506L
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>

#include "seltree.h"
#include "gen_list.h"
#include "types.h"
#include "base64.h"
#include "db_disk.h"
#include "conf_yacc.h"
#include "util.h"
#include "aide.h"
#include "db_sql.h" /* typedefs */
#include "commandconf.h"
/*for locale support*/
#include "locale-aide.h"
/*for locale support*/

#ifdef WITH_MHASH
#include <mhash.h>
#endif

#ifdef WITH_ZLIB
#include <zlib.h>
#endif

DIR* dirh=NULL;
struct AIDE_DIRENT_TYPE* entp=NULL;
struct AIDE_DIRENT_TYPE** resp=NULL;

struct seltree* r=NULL;


char* dot=".";
char* dotdot="..";

seltree* tree;

long td=-1;
int rdres=0;
int attr;
char* start_path="/";

int root_handled=0;

int  open_dir();

void next_in_dir() {
#ifdef HAVE_READDIR_R
  if (dirh!=NULL) 
    rdres=AIDE_READDIR_R_FUNC(dirh,entp,resp);
#else
#ifdef HAVE_READDIR
  if (dirh!=NULL) {
    entp=AIDE_READDIR_FUNC(dirh);
    td=telldir(dirh);
  }
#endif
#endif
  
}

int in_this() {
#ifdef HAVE_READDIR_R
  return (dirh!=NULL&&rdres==0&&(*resp)!=NULL);
#else
#ifdef HAVE_READDIR
  return entp!=NULL&&td!=telldir(dirh);
#endif
#endif
}

char* name_construct(char* s) {
  char* ret;
  int len2=strlen(r->path);
  int len=len2+strlen(s)+2;
  
  if (r->path[len2-1]!='/') {
    len++;
  }

  ret=(char*)malloc(len);
  ret[0]=(char)0;
  strcpy(ret,r->path);
  if (r->path[len2-1]!='/') {
    strcat(ret,"/");
  }
  strcat(ret,s);
  return ret;
}

void add_child(db_line* fil) {
  int i;
  struct seltree* new_r;
  
  error(255,"Adding child %s\n",fil->filename);
  
  new_r=get_seltree_node(r,fil->filename);
  if (new_r!=NULL) {
    if(S_ISDIR(fil->perm_o)){
      ;
    }else {
      new_r->checked|=NODE_CHECKED;
      new_r->checked|=NODE_TRAVERSE;
    }
    return;
  }
  
  new_r=malloc(sizeof(seltree));
  
  new_r->attr=0;
  i=strlen(fil->filename);

  /*
  new_r->path_len=r->path_len+i+1;
  */

  new_r->path=malloc(i+1);
  strcpy(new_r->path,fil->filename);
  //new_r->path[i]='/';
  //new_r->path[i+1]=0;
  new_r->childs=NULL;
  new_r->sel_rx_lst=NULL;
  new_r->neg_rx_lst=NULL;
  new_r->equ_rx_lst=NULL;
  new_r->parent=r;
  new_r->checked=0;
  new_r->new_data=NULL;
  new_r->old_data=NULL;
  if(S_ISDIR(fil->perm_o)){
    ;
  }else{
    new_r->checked|=NODE_CHECKED;
    new_r->checked|=NODE_TRAVERSE;
  }
  r->childs=list_append(r->childs,new_r);
}

/*
  It might be a good idea to make this non recursive.
  Now implemented with goto-statement. Yeah, it's ugly and easy.
*/

db_line* db_readline_disk(int db){
  db_line* fil=NULL;
  //struct AIDE_STAT_TYPE fs;
  char* fullname;
  //int sres=0;
  int add=0;

  /* root needs special handling */
  if(!root_handled){
    fullname="/";
    add=check_rxtree(fullname,conf->tree,&attr);
    error(240,"%s match=%d, tree=%i, attr=%i\n",fullname, add,conf->tree,attr);
    
    if (add) {
      fil=get_file_attrs(fullname,attr);
      
      error(240,"%s attr=%i\n",fullname,attr);
      if (fil!=NULL) {
	error(240,"%s attr=%i\n",fil->filename,fil->attr);	
      }
      
      if (fil==NULL) {
	/*
	  Something went wrong during read process -> 
	  Let's try next one.
	*/
	free_db_line(fil); /* Filename is freeed? */
	free(fil);
	fil=NULL;
      }
    }
    root_handled=1;
  }
 recursion:
  next_in_dir();
  
  if (in_this()) {

    /*
      Let's check if we have '.' or '..' entry.
      If have, just skipit.
      If don't do the 'normal' thing.
    */
    if (strcmp(entp->d_name,dot)==0 ||
	strcmp(entp->d_name,dotdot)==0) {
      //next_in_dir();
      goto recursion; // return db_readline_disk(db);
    }

    /*
      Now we know that we actually can do something.
    */

    fullname=name_construct(entp->d_name);

    /*
      Now we have a filename, which we must remember to free if it is
      not used. 
      
      Next thing is to see if we want to do something with it.
      If not call, db_readline_disk again...
    */
    
    add=check_rxtree(fullname,conf->tree,&attr);
    error(240,"%s match=%d, tree=%i, attr=%i\n",fullname, add,conf->tree,attr);
    
    if (add) {
      fil=get_file_attrs(fullname,attr);
      
      error(240,"%s attr=%i\n",fullname,attr);
      if (fil!=NULL) {
	error(240,"%s attr=%i\n",fil->filename,fil->attr);	
      }
      /*
	Hack.
       */
      //db_writeline(fil,conf);
      
      if (fil==NULL) {
	/*
	  Something went wrong during read process -> 
	  Let's try next one.
	*/
	free_db_line(fil); /* Filename is freeed? */
	fil=NULL;
	//next_in_dir();
	goto recursion; // return db_readline_disk(db);
      }
      
      if (add==1) {
	/*
	  add_children -> if dir, then add to children list.
	*/
	/* If ee are adding a file that is not a dir */
	/* add_child can make the determination and mark the tree
	   accordingly
	 */
	add_child(fil);
      } else if (add==2) {
	/*
	  Don't add to children list.
	*/
	
	/*
	  Should we do something?
	*/
      }
    } else {
      /*
	Make us traverse the tree:)
      */
      
      /*
	We have no use for fullname.
      */

      free(fullname);
      
      /*
      fil=get_file_attrs(fullname,DB_FILENAME|DB_PERM);
      error(255,"Taverse... \n");
      if (fil!=NULL && S_ISDIR(fil->perm_o)) {
	error(255,"addind %s \n",fullname);
	add_childs(fil);
      }
      
      free_db_line(fil);
      fil=NULL;
      */

      //next_in_dir();
      goto recursion; // return db_readline_disk(db);
    }
    /*
      Make sure that next time we enter
      we have something.
    */
    //next_in_dir();  
  } else {

    if (r==NULL) {
      return NULL;
    }
    
    error(255,"r->childs %i, r->parent %i, r->checked %i\n",r->childs,
	  r->parent,r->checked);
    
    if ((0==(r->checked&NODE_CHECKED))&&r->childs!=NULL) {
      seltree* rr;
      list* l;
      /*
      r->childs=r->childs->header->head;
      */
      
      l=r->childs->header->head;
      
      while (l!=NULL&&(((seltree*)(l->data))->checked&NODE_TRAVERSE)!=0) {
	l=l->next;
      }
      if (l!=NULL){
	if (l==l->header->tail) {
	  r->checked|=NODE_CHECKED;
	}
	
	rr=(seltree*)l->data;
	
	error(255,"rr->checked %i\n",rr->checked);
	rr->checked|=NODE_TRAVERSE;
	
	r=rr;
	
	error(255,"r->childs %i, r->parent %i,r->checked %i\n",r->childs,
	      r->parent,r->checked);      
	/*
	  Hack.
	*/
	start_path=r->path;
      
	error(255,"New start_path=%s\n",start_path);
      
	if (open_dir()==RETFAIL) {
	  /* open_dir failed so we need to know why and print 
	     an errormessage if needed.
	     errno should still be the one from opendir() since it's global
	  */ 
	  if(errno == ENOENT && r->old_data != NULL &&
	     r->sel_rx_lst==NULL && r->neg_rx_lst==NULL && 
	     r->equ_rx_lst==NULL) {
	    /* The path did not exist and there is old data for this node
	       and there are no regexps for this node
	       There is no new data for this node otherwise it would not
	       come to this part of the code.
	       So we don't print any error message.
	       */	    
	  }else {
	    /* In any other case we print the message. */
	    char* er=strerror(errno);
	    if (er!=NULL) {
	      error(5,"open_dir():%s: %s\n",er , start_path);
	    } else {
	      error(5,"open_dir():%i: %s\n",errno ,start_path);      
	    }
	    if(errno == ENOENT &&
	       ((r->sel_rx_lst!=NULL || r->neg_rx_lst!=NULL || 
	       r->equ_rx_lst!=NULL)||r->childs!=NULL)) {
	      /* The dir did not exist and there are regexps referring to
		 this node or there are children to this node. 
		 The only way a nonexistant dirnode can have children is by 
		 having rules referring to them.
	       */
	      error(5,"There are rules referring to non-existant directories!\n");
	    }
	  }
	  r->checked|=NODE_TRAVERSE|NODE_CHECKED;
	  r=r->parent;
	  error(255,"dropping back to parent\n");
	}
      }else {
	r->checked|=NODE_TRAVERSE|NODE_CHECKED;
	r=r->parent;
	/* We have gone out of the tree. This happens in some instances */
	if(r==NULL){
	  return NULL;
	}
	error(255,"dropping back to parent\n");
      }  
      goto recursion; // return db_readline_disk(db);
    }
    
    if (r->parent!=NULL) {
      /*
	Go back in time:)
      */
      r->checked|=NODE_CHECKED;
      
      r=r->parent;
      
      goto recursion; // return db_readline_disk(db);
    }
    /*
      The end has been reached. Nothing to do.
    */
  }
  /*
    int add=check_rxtree(path,tree,&attr);
  */

  return fil;
}

int  open_dir() {
  if (dirh!=NULL) {
    if (closedir(dirh)!=0) {
      /*
	Closedir did not success?
       */
    }

  }
  
  dirh=opendir(start_path);
  if (dirh==NULL) {
    /* Errors should be printed here because then we get too many
       errormessages. */
    /*    char* er=strerror(errno);
    if (er!=NULL) {
      error(5,"open_dir():%s: %s\n",er , start_path);
    } else {
      error(5,"open_dir():%i: %s\n",errno ,start_path);      
    }
    */
    return RETFAIL;
  }
  
  /*
    Init the first time.
  */
  //next_in_dir();
  return RETOK;
  
}

int db_disk_init() {
  
  r=conf->tree;
  
#  ifdef HAVE_READDIR_R
  resp=(struct AIDE_DIRENT_TYPE**)
    malloc(sizeof(struct AIDE_DIRENT_TYPE)+_POSIX_PATH_MAX);
  entp=(struct AIDE_DIRENT_TYPE*)
    malloc(sizeof(struct AIDE_DIRENT_TYPE)+_POSIX_PATH_MAX);
#  else
#   ifdef HAVE_READDIR
  /*
    Should we do something here?
    
  */
#   else
#    error AIDE needs readdir or readdir_r
#   endif
#  endif

  open_dir();

  return RETOK;
}

int db_disk_read_spec(int db){
  return RETOK;
}

#if 0
void foo() {
  
  dirh=opendir(tree->path);
  
  for(entp=AIDE_READDIR_FUNC(dirh);
      (entp!=NULL&&td!=telldir(dirh));
      entp=AIDE_READDIR_FUNC(dirh)){
    td=telldir(dirh);
  }
  
}
#endif

/*
  We don't support writing to disk, since we are'n a backup/restore software
 */

int db_writespec_disk(db_config* conf)
{
  return RETFAIL;
}

#ifdef WITH_SUN_ACL
int db_writeacl(acl_type* acl,FILE* file,int a){
  return RETFAIL;
}
#endif

int db_writeline_disk(db_line* line,db_config* conf){
  return RETFAIL;
}

int db_close_disk(db_config* conf){
  return RETOK;
}

const char* aide_key_6=CONFHMACKEY_06;
const char* db_key_6=DBHMACKEY_06;