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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "md.h"

#include "db_config.h"
#include "do_md.h"
#include "report.h"
#include "list.h"
#include "aide.h"
/*for locale support*/
#include "locale-aide.h"
/*for locale support*/


/* This define should be somewhere else */
#define READ_BLOCK_SIZE 16384
#define READ_BLOCK_SIZE 16777216

#ifdef WITH_MHASH
#include <mhash.h>
#endif /* WITH_MHASH */

/* Redhat 5.0 needs this */
#ifdef HAVE_MMAP
#ifndef MAP_FAILED
#define MAP_FAILED  (-1)
#endif
#define MMAP_BLOCK_SIZE 16777216
#endif

/*
#include <gcrypt.h>
*/
void md_init_fail(const char* s,db_line* db,byte** hash,unsigned i) {
  error(0,"Message diggest %s initialise failed\nDisabling %s for file %s\n",s,s,db->filename);
  db->attr=db->attr&(~i);
  (*hash)=0;
}

void free_hashes(db_line* dl){

  /* #define free_hash(a) if(dl->a!=NULL) {free(dl->a); dl->a=NULL;} */
#define free_hash(a) dl->a=NULL

  free_hash(md5);
  free_hash(sha1);
  free_hash(rmd160);
  free_hash(tiger);
#ifdef WITH_MHASH
  free_hash(crc32);
  free_hash(haval);
  free_hash(gost);
  free_hash(crc32b);  
#endif
}

list* do_md(list* file_lst,db_config* conf)
{
  abort();
  return file_lst;
}

int stat_cmp(struct AIDE_STAT_TYPE* f1,struct AIDE_STAT_TYPE* f2) {
  if (f1==NULL || f2==NULL) {
    return RETFAIL;
  }
#define stat_cmp_helper(n) (f1->n==f2->n)

  if (stat_cmp_helper(st_ino)&&
      stat_cmp_helper(st_mode)&&
      stat_cmp_helper(st_nlink)&&
      stat_cmp_helper(st_size)&&
      stat_cmp_helper(st_mtime)&&
      stat_cmp_helper(st_ctime)&&
      stat_cmp_helper(st_blocks)&&
      stat_cmp_helper(st_blksize)&&
      stat_cmp_helper(st_rdev)&&
      stat_cmp_helper(st_gid)&&
      stat_cmp_helper(st_uid)&&
      stat_cmp_helper(st_nlink)&&
      stat_cmp_helper(st_dev)) {
    return RETOK;
  }
  return RETFAIL;
}


void no_hash(db_line* line);

void calc_md(struct AIDE_STAT_TYPE* old_fs,db_line* line) {
  /*
    We stat after opening just to make sure that the file
    from we are about to calculate the hash is the correct one,
    and we don't read from a pipe :)
   */
  struct AIDE_STAT_TYPE fs;
  int sres=0;
#ifdef _PARAMETER_CHECK_
  if (line==NULL) {
    abort();
  }
#endif  

  int filedes=open(line->filename,O_RDONLY);

  error(255,"calc_md called\n");
  if (filedes==-1) {
    char* er=strerror(errno);
    if (er!=NULL) {
      error(5,"do_md():open() for %s failed:%s\n",
	    line->filename,er);
    } else {
      error(5,"do_md():open() for %s failed:%i\n",
	    line->filename,errno);
    }
    /*
      Nop. Cannot cal hashes. Mark it.
     */
    no_hash(line);
    return;
  }
  
  sres=AIDE_FSTAT_FUNC(filedes,&fs);
  
  if (stat_cmp(&fs,old_fs)==RETOK) {
    /*
      Now we have a 'valid' filehandle to read from a file.
     */
    off_t r_size=0;
    off_t size=0;
    char* buf;

    struct md_container mdc;
    
    mdc.todo_attr=line->attr;
    
    if (init_md(&mdc)==RETOK) {
#ifdef HAVE_MMAP
      off_t curpos=0;

      r_size=fs.st_size;
      /* in mmap branch r_size is used as size remaining */
      while(r_size>0){
	if(r_size<MMAP_BLOCK_SIZE){
	  buf = mmap(0,r_size,PROT_READ,MAP_SHARED,filedes,curpos);
	  curpos+=r_size;
	  size=r_size;
	  r_size=0;
	}else {
	  buf = mmap(0,MMAP_BLOCK_SIZE,PROT_READ,MAP_SHARED,filedes,curpos);
	  curpos+=MMAP_BLOCK_SIZE;
	  size=MMAP_BLOCK_SIZE;
	  r_size-=MMAP_BLOCK_SIZE;
	}
	if ( buf == MAP_FAILED ) {
	  error(0,"error mmap'ing %s\n", line->filename);
	  close(filedes);
	  close_md(&mdc);
	  return;
	}
	conf->catch_mmap=1;
	if (update_md(&mdc,buf,size)!=RETOK) {
	  error(0,"Message digest failed during update\n");
	  close_md(&mdc);
	  munmap(buf,size);
	  return;
	}
	munmap(buf,size);
	conf->catch_mmap=0;
      }
#else /* not HAVE_MMAP */
      buf=malloc(READ_BLOCK_SIZE);
#if READ_BLOCK_SIZE>SSIZE_MAX
#error "READ_BLOCK_SIZE" is too large. Max value is SSIZE_MAX, and current is READ_BLOCK_SIZE
#endif
      while ((size=read(filedes,buf,READ_BLOCK_SIZE))>0) {
	if (update_md(&mdc,buf,size)!=RETOK) {
	  error(0,"Message digest failed during update\n");
	  close_md(&mdc);
	  return;
	}
	r_size+=size;
      }
      free(buf);
#endif /* HAVE_MMAP else branch */    
      close_md(&mdc);
      md2line(&mdc,line);
      
    } else {
      error(5,"Message diggest initialization failed.\n");
      no_hash(line);
      close(filedes);
      return;
    }
  } else {
    /*
      Something just wasn't correct, so no hash calculated.
    */
    
    error(5,"File %s was changed so that hash cannot be calculated for it\n"
	  ,line->filename);
    
    no_hash(line);
    close(filedes);
    return;
  }
  close(filedes);
  return;
}

void fs2db_line(struct AIDE_STAT_TYPE* fs,db_line* line) {
  
  if(DB_INODE&line->attr){
    line->inode=fs->st_ino;
  } else {
    line->inode=0;
  }

  if(DB_UID&line->attr) {
    line->uid=fs->st_uid;
  }else {
    line->uid=0;
  }

  if(DB_GID&line->attr){
    line->gid=fs->st_gid;
  }else{
    line->gid=0;
  }

  if(DB_PERM&line->attr){
    line->perm=fs->st_mode;
  }else{
    line->perm=0;
  }

  if(DB_SIZE&line->attr||DB_SIZEG&line->attr){
    line->size=fs->st_size;
  }else{
    line->size=0;
  }
  
  if(DB_LNKCOUNT&line->attr){
    line->nlink=fs->st_nlink;
  }else {
    line->nlink=0;
  }

  if(DB_MTIME&line->attr){
    line->mtime=fs->st_mtime;
  }else{
    line->mtime=0;
  }

  if(DB_CTIME&line->attr){
    line->ctime=fs->st_ctime;
  }else{
    line->ctime=0;
  }
  
  if(DB_ATIME&line->attr){
    line->atime=fs->st_atime;
  }else{
    line->atime=0;
  }

  if(DB_BCOUNT&line->attr){
    line->bcount=fs->st_blocks;
  } else {
    line->bcount=0;
  }
  
}

#ifdef WITH_ACL
void acl2line(db_line* line) {
  if(DB_ACL&line->attr) { /* There might be a bug here. */
    int res;
    line->acl=malloc(sizeof(acl_type));
    line->acl->entries=acl(line->filename,GETACLCNT,0,NULL);
    if (line->acl->entries==-1) {
      char* er=strerror(errno);
      line->acl->entries=0;
      if (er==NULL) {
	error(0,"ACL query failed for %s. strerror failed for %i\n",line->filename,errno);
      } else {
	error(0,"ACL query failed for %s:%s\n",line->filename,er);
      }
    } else {
      line->acl->acl=malloc(sizeof(aclent_t)*line->acl->entries);
      res=acl(line->filename,GETACL,line->acl->entries,line->acl->acl);
      if (res==-1) {
	error(0,"ACL error %s\n",strerror(errno));
      } else {
	if (res!=line->acl->entries) {
	  error(0,"Tried to read %i acl but got %i\n",line->acl->entries,res);
	}
      }
    }
  }else{
    line->acl=NULL;
  }
}
#endif

void no_hash(db_line* line) {
  line->attr&=~DB_HASHES;
}
