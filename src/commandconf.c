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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>

#include "commandconf.h"
#include "aide.h"
#include "conf_lex.h"
#include "conf_yacc.h"
#include "db.h"
#include "db_config.h"
#include "gen_list.h"
#include "symboltable.h"
#include "util.h"
#include "base64.h"
/*for locale support*/
#include "locale-aide.h"
/*for locale support*/


#define BUFSIZE 4096
#define ZBUFSIZE 16384

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

int commandconf(const char mode,const char* line)
{
  static char* before=NULL;
  static char* config=NULL;
  static char* after=NULL;
  char* all=NULL;
  char* tmp=NULL;
  int l=0;

  switch(mode){
  case 'B':{
    if(before==NULL){
      before=strdup(line);
    }
    else {
      tmp=(char*)malloc(sizeof(char)
			*(strlen(before)+strlen(line)+2));
      tmp[0]='\0';
      strcat(tmp,before);
      strcat(tmp,"\n");
      strcat(tmp,line);
      free(before);
      before=tmp;
    }
    break;
  }
  case 'C':{
    config=strdup(line);
    break;
  }
  case 'A':{
    if(after==NULL){
      after=strdup(line);
    }
    else {
      tmp=(char*)malloc(sizeof(char)
			*(strlen(after)+strlen(line)+2));
      strcat(tmp,after);
      strcat(tmp,"\n");
      strcat(tmp,line);
      free(after);
      after=tmp;
    }
    break;
  }
  case 'D': {
    /* Let's do it */
    int rv=-1;
    char* new_config=NULL;
    char* homedir=NULL;

    /* support for ~ in the config file string 
       ~ must be the first character and it will be
       replaced with HOME-environment variable
     */
    if(config[0]=='~'){
      if((homedir=getenv("HOME"))){
	new_config=(char*)malloc(sizeof(char)*
				 (strlen(config)+strlen(homedir)+1));
	memcpy(new_config,homedir,strlen(homedir));
	memcpy(new_config+strlen(homedir),
	       config+sizeof(char),strlen(config+sizeof(char)));
	l=(strlen(config)+strlen(homedir));
	new_config[l]='\0';
	free(config);
	config=new_config;
	/* Don't free(homedir); because it is not safe on some platforms */
      }
    }
    if (config!=NULL && strcmp(config,"-")==0) {
      error(255,_("Config from stdin\n"));
      rv=0;
    } else {
      
      rv=access(config,R_OK);
      if(rv==-1){
	error(0,_("Cannot access config file:%s:%s\n"),config,strerror(errno));
      }
    }
    
    if(before==NULL&&after==NULL&&
       (config==NULL||strcmp(config,"")==0||rv==-1)){
      error(0,_("No config defined\n"));
      return RETFAIL;
    }
    if(before!=NULL) {
      l+=strlen(before);
    }
    if(config!=NULL) {
      l+=strlen(config);
    }
    if(after!=NULL) {
      l+=strlen(after);
    }
    l+=strlen("@@include \n\n\n")+1;
    
    all=(char*)malloc(sizeof(char)*l);

    memset(all,0,l);
    if(before!=NULL){
      strcat(all,before);
      strcat(all,"\n");
    }
    strcat(all,"@@include ");
    strcat(all,config);
    strcat(all,"\n");
    if(after!=NULL){
      strcat(all,after);
      strcat(all,"\n");
    }
    
    error(200,"commandconf():%s\n",all);
    
    conf_scan_string(all);
    
    if(confparse()){
      return RETFAIL;
    }
    
    break;
  }
  default: {
    error(0,_("Illegal argument %c to commmandconf()\n"),mode);
    break;
  }
  }
  return RETOK;
}

int conf_input_wrapper(char* buf, int max_size, FILE* in)
{
  int retval=0;
  int c=0;
  char* tmp=NULL;
  void* key=NULL;
  int keylen=0;

  /* FIXME Add support for gzipped config. :) */
#ifdef WITH_MHASH
  /* Read a character at a time until we are doing md */
  if(conf->do_configmd){
    retval=fread(buf,1,max_size,in);
  }else {
    c=fgetc(in);
    retval= (c==EOF) ? 0 : (buf[0] = c,1);
  }
#else
  retval=fread(buf,1,max_size,in);
#endif 

#ifdef WITH_MHASH    
  if(conf->do_configmd||conf->config_check){
    if(((conf->do_configmd==1)&&conf->config_check)||!conf->confmd){
      if(conf->do_configmd==1){
	conf->do_configmd+=1;
      }
      if((key=get_conf_key())!=NULL){
	keylen=get_conf_key_len();
	
	if( (conf->confmd=
	     mhash_hmac_init(conf->confhmactype,
			     key,
			     keylen,
			     mhash_get_hash_pblock(conf->confhmactype)))==
	    MHASH_FAILED){
	  error(0, "mhash_hmac_init() failed for %i for config check. Aborting\n",
		conf->confhmactype);
	  abort();
	}
      } else {
	conf->do_configmd=0;
      }
    }
    /* FIXME This does not handle the case that @@end_config is on 
       buffer boundary. */
    if((tmp=strnstr(buf,"@@end_config",retval))!=NULL){
      /* We have end of config don't feed the last line to mhash */
      mhash(conf->confmd,(void*)buf,tmp-buf);
    } else {
      mhash(conf->confmd,(void*)buf,retval);
    }
#endif
  }
  return retval;
}

int db_input_wrapper(char* buf, int max_size, int db)
{
  int retval=0;
  int c=0;
  char* tmp=NULL;
  int err=0;
  int* domd=0;
#ifdef WITH_MHASH
  MHASH* md=NULL;
  void* key=NULL;
  int keylen;
#endif
  FILE** db_filep=NULL;
#ifdef WITH_ZLIB
  gzFile* db_gzp=NULL;
#endif
 
  switch(db) {
  case DB_OLD: {
    domd=&(conf->do_dboldmd);
    md=&(conf->dboldmd);
    db_filep=&(conf->db_in);
#ifdef WITH_ZLIB
    db_gzp=&(conf->db_gzin);
#endif
    break;
  }
  case DB_NEW: {
    domd=&(conf->do_dbnewmd);
    md=&(conf->dbnewmd);
    db_filep=&(conf->db_new);
#ifdef WITH_ZLIB
    db_gzp=&(conf->db_gznew);
#endif
    break;
  }
  }

  /* Read a character at a time until we are doing md */
#ifdef WITH_ZLIB
  if((*db_gzp==NULL)&&(*domd)){
    retval=fread(buf,1,max_size,*db_filep);
  }
  if((*db_gzp!=NULL)&&(*domd)){
    if(gzeof(*db_gzp)){
      retval=0;
      buf[0]='\0';
    }else {
      if((retval=gzread(*db_gzp,buf,max_size))<0){
	error(0,_("gzread() failed:gzerr=%s!\n"),gzerror(*db_gzp,&err));
	retval=0;
	buf[0]='\0';
      } else {
	/* gzread returns 0 even if uncompressed bytes were read*/
	error(240,"nread=%d,strlen(buf)=%d,errno=%s,gzerr=%s\n",
		retval,strlen((char*)buf),strerror(errno),
	      gzerror(*db_gzp,&err));
	if(retval==0){
	  retval=strlen((char*)buf);
	}
      }
    }
  }
  if((*db_gzp!=NULL)&&!(*domd)){
    c=gzgetc(*db_gzp);
    retval= (c==EOF) ? 0 : (buf[0] = c,1);
  }
  if((*db_gzp==NULL)&&!(*domd)){
    c=fgetc(*db_filep);
    if(c==(unsigned char)'\037'){
      c=fgetc(*db_filep);
      if(c==(unsigned char)'\213'){
	/* We got gzip header. */
	error(255,"Got Gzip header. Handling..\n");
	fseek(*db_filep,0L,SEEK_SET);
	*db_gzp=gzdopen(fileno(*db_filep),"rb");
	c=gzgetc(*db_gzp);
      }else {
	/* False alarm */
	ungetc(c,*db_filep);
      }
    }
    retval= (c==EOF) ? 0 : (buf[0] = c,1);
  }

#else /* WITH_ZLIB */
#ifdef WITH_MHASH
  if(*domd){
    retval=fread(buf,1,max_size,*db_filep);
  }else {
    c=fgetc(*db_filep);
    retval= (c==EOF) ? 0 : (buf[0] = c,1);
  }
#else /* WITH_MHASH */
  retval=fread(buf,1,max_size,*db_filep);
#endif /* WITH_MHASH */ 
#endif /* WITH_ZLIB */


#ifdef WITH_MHASH    
  if(*domd){
    if(!*md){
      if((key=get_db_key())!=NULL){
	keylen=get_db_key_len();
	
	if( (*md=
	     mhash_hmac_init(conf->dbhmactype,
			     key,
			     keylen,
			     mhash_get_hash_pblock(conf->dbhmactype)))==
	    MHASH_FAILED){
	  error(0, "mhash_hmac_init() failed for db check. Aborting\n");
	  abort();
	}
      } else {
	*domd=0;
      }
    }
    /* FIXME This does not handle the case that @@end_config is on 
       buffer boundary. */
    if (*domd!=0) {
      if((tmp=strnstr(buf,"@@end_db",retval))!=NULL){
	/* We have end of db don't feed the last line to mhash */
	mhash(*md,(void*)buf,tmp-buf);
	/* We don't want to come here again after the *md has been deinited 
	   by db_readline_file() */
	*domd=0;
      } else {
	mhash(*md,(void*)buf,retval);
      }
    }
#endif
  }
  return retval;
}

int check_db_order(DB_FIELD* d,int size, DB_FIELD a)
{
  int i=0;
  for(i=0;i<size;i++){
    if(d[i]==a)
      return RETFAIL;
  }
  return RETOK;
}

int check_dboo(DB_FIELD a){
  return check_db_order(conf->db_out_order,conf->db_out_size,a);
}

void update_db_out_order(int attr)
{
  /* First we add those attributes that must be there */
  if (check_dboo(db_linkname)==RETOK) {
    conf->db_out_order[conf->db_out_size++]=db_linkname;
  }
  if (check_dboo(db_attr)==RETOK) {
    conf->db_out_order[conf->db_out_size++]=db_attr;
  }
  if((attr&DB_PERM) && (check_dboo(db_perm)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_perm;
  }
  if((attr&DB_BCOUNT) && (check_dboo(db_bcount)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_bcount;
  }
  if((attr&DB_UID) && (check_dboo(db_uid)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_uid;
  }
  if((attr&DB_GID) && (check_dboo(db_gid)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_gid;
  }
  if((attr&DB_SIZE) && (check_dboo(db_size)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_size;
  }
  if((attr&DB_SIZEG) && (check_dboo(db_size)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_size;
  }
  if((attr&DB_ATIME) && (check_dboo(db_atime)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_atime;
  }
  if((attr&DB_MTIME) && (check_dboo(db_mtime)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_mtime;
  }
  if((attr&DB_CTIME) && (check_dboo(db_ctime)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_ctime;
  }
  if((attr&DB_INODE) && (check_dboo(db_inode)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_inode;
  }
  if((attr&DB_LNKCOUNT) && (check_dboo(db_lnkcount)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_lnkcount;
  }
  if((attr&DB_MD5) && (check_dboo(db_md5)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_md5;
  }
  if((attr&DB_SHA1) && (check_dboo(db_sha1)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_sha1;
  }
  if((attr&DB_RMD160) && (check_dboo(db_rmd160)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_rmd160;
  }
  if((attr&DB_TIGER) && (check_dboo(db_tiger)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_tiger;
  }
  /*
#ifdef WITH_MHASH
  */
  if((attr&DB_CRC32) && (check_dboo(db_crc32)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_crc32;
  }
  if((attr&DB_HAVAL) && (check_dboo(db_haval)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_haval;
  }
  if((attr&DB_GOST) && (check_dboo(db_gost)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_gost;
  }
  if((attr&DB_CRC32B) && (check_dboo(db_crc32b)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_crc32b;
  }
  /*
#endif
  */
#ifdef WITH_ACL
  if((attr&DB_ACL) && (check_dboo(db_acl)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_acl;
  }
#endif
  if((attr&DB_CHECKMASK) && (check_dboo(db_checkmask)!=RETFAIL)){
    conf->db_out_order[conf->db_out_size++]=db_checkmask;
  }
}


char* get_variable_value(char* var)
{
  list* r=NULL;
  
  if((r=list_find(var,conf->defsyms))){
    return (((symba*)r->data)->value);
  };

  return NULL;
}

void putbackvariable(char* var)
{
  char* a=NULL;
  
  char* v=strdup(var);
  
  char* subst_begin=strstr(v,"@@{");
  char* subst_end=strstr(subst_begin,"}");
  
  char* tmp=(char*)malloc((subst_end-subst_begin)+1);
  
  tmp = strncpy(tmp,subst_begin+3,subst_end-subst_begin-3);
  
  tmp[subst_end-subst_begin-3]='\0';
  
  conf_put_token(subst_end+1);
  
  if((a=get_variable_value(tmp))!=NULL){
    conf_put_token(a);
  }
  else {
    
    error(230,_("Variable %s not defined\n"),tmp);

    /*
     * We can use nondefined variable
     */ 
  }
  
  subst_begin[0]='\0';
  
  conf_put_token(v);
  conf_put_token("\n");
  free(v);

}


void do_define(char* name, char* value)
{
  symba* s=NULL;
  list* l=NULL;

  if(!(l=list_find(name,conf->defsyms))){
    s=(symba*)malloc(sizeof(symba));
    s->name=name;
    s->value=value;
    conf->defsyms=list_append(conf->defsyms,(void*)s);
  }
  else {
    free(((symba*)l->data)->value);
    ((symba*)l->data)->value=NULL;
    ((symba*)l->data)->value=value;
  }
}

void do_undefine(char* name)
{
  list*r=NULL;

  if((r=list_find(name,conf->defsyms))){
    free(((symba*)r->data)->value);
    free((symba*)r->data);
    r->data=NULL;
    conf->defsyms=list_delete_item(r);
  }
}

int handle_endif(int doit,int allow_else){
  
  if(doit){
    int count=1;
    error(230,_("\nEating until @@endif\n"));
    do {
      int i = conflex();
      switch (i) {
      case TIFDEF : {
	count++;
	break;
      }
      case TIFNDEF : {
	count++;
	break;
      }
      case TENDIF : {
	count--;
	break;
      }

      case TIFHOST : {
	count++;
	break;
      }

      case TIFNHOST : {
	count++;
	break;
      }
      
      case TELSE : {
	
	if (count==1) {
	  /*
	   * We have done enough 
	   */ 
	  if (allow_else) {
	    return 0;
	  }
	  else {
	    conferror("Ambigous else");
	    return -1;
	  }
	}
	
	break;
      }
      
      case 0 : {
	conferror("@@endif or @@else expected");
	return -1;
	count=0;
      }
      
      default : {
	/*
 	 * empty default
	 */
      }
      }
      
      
    } while (count!=0);
    
    conf_put_token("\n@@endif\n");
    error(230,"\nEating done\n");
  }
  
  return 0;
  
}

int do_ifxdef(int mode,char* name)
{
  int doit;
  doit=mode;

  if((list_find(name,conf->defsyms))){
    doit=1-doit;
  }
  
  return (handle_endif(doit,1));

}

int do_ifxhost(int mode,char* name)
{
  int doit;
  char* s=NULL;
  doit=mode;
  s=(char*)malloc(sizeof(char)*MAXHOSTNAMELEN+1);

  if(gethostname(s,MAXHOSTNAMELEN)==-1){
    error(0,_("Couldn't get hostname %s"),name);
    free(s);
    return -1;
  }  
  if(strcmp(name,s)==0) {
    doit=1-doit;
  }
  free(s);
  return (handle_endif(doit,1));
}

list* append_rxlist(char* rx,int attr,list* rxlst)
{
  extern long conf_lineno; /* defined & set in conf_lex.l */
    
  rx_rule* r=NULL;
  r=(rx_rule*)malloc(sizeof(rx_rule));
  r->rx=rx;
  r->attr=attr;
  r->conf_lineno = conf_lineno;
  update_db_out_order(r->attr);
  rxlst=list_append(rxlst,(void*)r);
  
  return rxlst;
}

void do_groupdef(char* group,int value)
{
  list* r=NULL;
  symba* s=NULL;

  if((r=list_find(group,conf->groupsyms))){
      ((symba*)r->data)->ival=value;
      return;
  }
  /* This is a new group */
  s=(symba*)malloc(sizeof(symba));
  s->name=group;
  s->ival=value;
  conf->groupsyms=list_append(conf->groupsyms,(void*)s);
}

int get_groupval(char* group)
{
  list* r=NULL;
  if((r=list_find(group,conf->groupsyms))){
    return (((symba*)r->data)->ival);
  }
  return -1;
}



void do_dbdef(int dbtype,char* val)
{
  url_t* u=NULL;
  url_t** conf_db_url;

  switch(dbtype) {
  case DB_OLD: {
    conf_db_url=&(conf->db_in_url);
    break;
  }
  case DB_WRITE: {
    conf_db_url=&(conf->db_out_url);
    break;
  }
  case DB_NEW: {
    conf_db_url=&(conf->db_new_url);
    break;
  }
  default : {
    error(0,"Invalid call of do_dbdef\n");
    return;
  }
  }

  if(*conf_db_url==NULL){
    u=parse_url(val);
    /* FIXME Check the URL if you add support for databases that cannot be 
     * both input and output urls */
    switch (dbtype) {
    case DB_OLD:
    case DB_NEW:{
      if(u==NULL||u->type==url_unknown||u->type==url_stdout
	 ||u->type==url_stderr) {
	error(0,_("Unsupported input URL-type:%s\n"),val);
      }
      else {
	*conf_db_url=u;
      }
      break;
    }
    case DB_WRITE: {
      if(u==NULL||u->type==url_unknown||u->type==url_stdin){
	error(0,_("Unsupported output URL-type:%s\n"),val);
      }
      else{
	conf->db_out_url=u;
	error(200,_("Output database set to \"%s\" \"%s\"\n"),val,u->value);
      }
      break;
    }
    }
  }
  free(val);
}

void do_dbindef(char* val)
{
  url_t* u=NULL;

  if(conf->db_in_url==NULL){
    u=parse_url(val);
    /* FIXME Check the URL if you add support for databases that cannot be 
     * both input and output urls */
    if(u==NULL||u->type==url_unknown||u->type==url_stdout
       ||u->type==url_stderr) {
      error(0,_("Unsupported input URL-type:%s\n"),val);
    }
    else {
      conf->db_in_url=u;
    }
  }

  free(val);
}

void do_dboutdef(char* val)
{
  url_t* u=NULL;

  error(200,_("Setting output database \"%s\"\n"),val);

  if(conf->db_out_url==NULL){
    u=parse_url(val);
    /* FIXME Check the URL if you add support for databases that cannot be 
     * both input and output urls */
    if(u==NULL||u->type==url_unknown||u->type==url_stdin){
      error(0,_("Unsupported output URL-type:%s\n"),val);
    }
    else{
      conf->db_out_url=u;
      error(200,_("Output database set to \"%s\" \"%s\"\n"),val,u->value);
    }
  } else {
    error(200,_("Output database already set\n"));
  }

  free(val);
}

void do_repurldef(char* val)
{
  url_t* u=NULL;

  
  u=parse_url(val);
  /* FIXME Check the URL if you add support for databases that cannot be 
   * both input and output urls */
  if(u==NULL||u->type==url_unknown||u->type==url_stdin){
    error(0,_("Unsupported output URL-type:%s\n"),val);
  } else {
    error_init(u,0);
  }
  
}

void do_verbdef(char* val)
{
  char* err=NULL;
  long a=0;
  
  a=strtol(val,&err,10);
  if(*err!='\0' || a>255 || a<0 || errno==ERANGE){
    error(0, _("Illegal verbosity level:%s\n"),val);
    error(10,_("Using previous value:%i\n"),conf->verbose_level);
    return;
  }    
  else {
    if(conf->verbose_level==-1){
      conf->verbose_level=a;
    }else {
      error(210,_("Verbosity already defined to %i\n"),conf->verbose_level);
    }
  }
}


const char* aide_key_7=CONFHMACKEY_07;
const char* db_key_7=DBHMACKEY_07;

void* get_conf_key() {
  void* r;
  char* m=(char*)malloc(strlen(aide_key_1)+
			strlen(aide_key_2)+
			strlen(aide_key_3)+
			strlen(aide_key_4)+
			strlen(aide_key_5)+
			strlen(aide_key_6)+
			strlen(aide_key_7)+
			strlen(aide_key_8)+
			strlen(aide_key_9)+
			strlen(aide_key_0)+1);
  m[0]=0;
  strcat(m,aide_key_0);
  strcat(m,aide_key_1);
  strcat(m,aide_key_2);
  strcat(m,aide_key_3);
  strcat(m,aide_key_4);
  strcat(m,aide_key_5);
  strcat(m,aide_key_6);
  strcat(m,aide_key_7);
  strcat(m,aide_key_8);
  strcat(m,aide_key_9);
  
  r=decode_base64(m,strlen(m));

  memset(m,0,strlen(m));
  free(m);
  return r;
}

size_t get_conf_key_len() {
  size_t len=0;
  char* m=(char*)malloc(strlen(aide_key_1)+
			strlen(aide_key_2)+
			strlen(aide_key_3)+
			strlen(aide_key_4)+
			strlen(aide_key_5)+
			strlen(aide_key_6)+
			strlen(aide_key_7)+
			strlen(aide_key_8)+
			strlen(aide_key_9)+
			strlen(aide_key_0)+1);
  m[0]=0;
  strcat(m,aide_key_0);
  strcat(m,aide_key_1);
  strcat(m,aide_key_2);
  strcat(m,aide_key_3);
  strcat(m,aide_key_4);
  strcat(m,aide_key_5);
  strcat(m,aide_key_6);
  strcat(m,aide_key_7);
  strcat(m,aide_key_8);
  strcat(m,aide_key_9);
  
  len=length_base64(m,strlen(m));

  memset(m,0,strlen(m));
  free(m);
  return len;
}

void* get_db_key() {
  void* r;
  char* m=(char*)malloc(strlen(db_key_1)+
			strlen(db_key_2)+
			strlen(db_key_3)+
			strlen(db_key_4)+
			strlen(db_key_5)+
			strlen(db_key_6)+
			strlen(db_key_7)+
			strlen(db_key_8)+
			strlen(db_key_9)+
			strlen(db_key_0)+1);
  m[0]=0;
  strcat(m,db_key_0);
  strcat(m,db_key_1);
  strcat(m,db_key_2);
  strcat(m,db_key_3);
  strcat(m,db_key_4);
  strcat(m,db_key_5);
  strcat(m,db_key_6);
  strcat(m,db_key_7);
  strcat(m,db_key_8);
  strcat(m,db_key_9);
  
  r=decode_base64(m,strlen(m));
  
  memset(m,0,strlen(m));
  free(m);
  return r;
}

size_t get_db_key_len() {
  size_t len=0;
  char* m=(char*)malloc(strlen(db_key_1)+
			strlen(db_key_2)+
			strlen(db_key_3)+
			strlen(db_key_4)+
			strlen(db_key_5)+
			strlen(db_key_6)+
			strlen(db_key_7)+
			strlen(db_key_8)+
			strlen(db_key_9)+
			strlen(db_key_0)+1);
  m[0]=0;
  strcat(m,db_key_0);
  strcat(m,db_key_1);
  strcat(m,db_key_2);
  strcat(m,db_key_3);
  strcat(m,db_key_4);
  strcat(m,db_key_5);
  strcat(m,db_key_6);
  strcat(m,db_key_7);
  strcat(m,db_key_8);
  strcat(m,db_key_9);
  
  len=length_base64(m,strlen(m));
  
  memset(m,0,strlen(m));
  free(m);
  return len;
}