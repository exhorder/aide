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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include "aide.h"
#include "db_config.h"
#include "db_file.h"
#include "report.h"

#ifdef WITH_PSQL
#include "libpq-fe.h"
#endif
/*for locale support*/
#include "locale-aide.h"
/*for locale support*/

#ifdef WITH_PSQL
int be_sql_readinit(psql_data* ret) {
  /* Yes.. we don't want to know about two first result.. 
     and we want no memoryleaking.
  */
  int i,j,nFields;
  char* s;
  char declare []="DECLARE aidecursor CURSOR FOR select * from ";
  
  s = (char*)malloc(strlen(declare)+strlen(ret->table)+1);
  s[0]=0;
  s=strcat(s,declare);
  s=strcat(s,ret->table);
  
  ret->res=PQexec(ret->conn,s);
  /*
  PQclear(ret->res);
  ret->res=PQexec(ret->conn,
		  ret->table
		  );
  */
		  
  if (!ret->res || PQresultStatus(ret->res) != PGRES_COMMAND_OK) {
    
    if (ret->res!=NULL) {
      error(255,"Psql error: %s\n",PQresStatus(PQresultStatus(ret->res)));
      PQclear(ret->res);
    }
    return RETFAIL;
  }
  PQclear(ret->res);
  
  ret -> res = PQexec(ret->conn, "FETCH ALL in aidecursor");
  
  if (!ret->res || PQresultStatus(ret->res) != PGRES_TUPLES_OK)
    {
      error(0, "FETCH ALL command didn't return tuples properly\n");
      PQclear(ret->res);
      abort();
    }
  
  
  /* first, print out the attribute names */
  nFields = PQnfields(ret->res);
  for (i = 0; i < nFields; i++)
    error(255,"%-15s", PQfname(ret->res, i));
  error(255,"\n\n");
  
  
  for(i=0;i<db_unknown;i++){
    ret->des[i]=PQfnumber(ret->res,db_names[i]);
    if (ret->des[i]!=-1) {
      error(255,"Field %i,%s \n",ret->des[i],db_names[i]);
    }
  }
  
  ret->curread=0;
  ret->maxread=PQntuples(ret->res);
  /* And now we know how many fields we have.. */
  
  error(0,"%i tuples\n",ret->maxread);
  
  return RETOK;
  
}

char* get_first_value(char** in){
  int i=0;
  char* ret = (*in);
  while((*in)[i]!=':' && (*in)[i]!='\0') {
    i++;
  }
  /*  
      if (i==0) {
      return NULL;
      }
  */
  if ((*in)[i]!='\0') { /* Lets not go beond the sting.. */
    (*in)[i]='\0';
    (*in)+=i+1;
  }
  return ret;
}

#endif

void* be_init(int inout,url_t* u,int iszipped)
{
  void* fh=NULL;
  long a=0;
  char* err=NULL;

  if (u==NULL) {
    return NULL;
  }

  switch (u->type) {
  case url_file : {
    error(200,_("Opening file \"%s\" for %s\n"),u->value,inout?"r":"w+");
#ifdef WITH_ZLIB
    if(iszipped && !inout){
      fh=gzopen(u->value,"wb9+");
      if(fh==NULL){
	error(0,_("Couldn't open file %s for %s"),u->value,
	      inout?"reading\n":"writing\n");
      }
    }
    else{
#endif
      fh=fopen(u->value,inout?"r":"w+");
      if(fh==NULL){
	error(0,_("Couldn't open file %s for %s"),u->value,
	      inout?"reading\n":"writing\n");
      }
#ifdef WITH_ZLIB
    }
#endif
    return fh;
    }
  case url_stdout : {
#ifdef WITH_ZLIB
    if(iszipped){
      return gzdopen(fileno(stdout),"wb");
    }
    else{
#endif
    return stdout;
#ifdef WITH_ZLIB
    }
#endif
  }
  case url_stdin : {
#ifdef WITH_ZLIB
    if(iszipped){
      return gzdopen(fileno(stdin),"r");
    }
    else{
#endif
      return stdin;
#ifdef WITH_ZLIB
    }
#endif
  }
  case url_stderr : {
#ifdef WITH_ZLIB
    if(iszipped){
      return gzdopen(fileno(stderr),"wb");
    }
    else{
#endif
      return stderr;
#ifdef WITH_ZLIB
    }
#endif
  }
  case url_fd : {
    a=strtol(u->value,&err,10);
    if(*err!='\0'||errno==ERANGE){
      error(0,"Illegal file descriptor value:%s\n",u->value);
    }
#ifdef WITH_ZLIB
    if(iszipped && !inout){
      fh=gzdopen(a,"w");
      if(fh==NULL){
	error(0,"Couldn't reopen file descriptor %i\n",a);
      }
    }
    else{
#endif
      fh=fdopen(a,inout?"r":"w");
      if(fh==NULL){
	error(0,"Couldn't reopen file descriptor %i\n",a);
      }
#ifdef WITH_ZLIB
    }
#endif
    return fh;
  }
#ifdef WITH_PSQL
  case url_sql : {
    char *pghost, *pgport, *pgoptions, *pgtty, *dbName, *login, *pwd;
    char *tmp,*tmp2;
    
    psql_data* ret = (psql_data*) malloc(sizeof(psql_data)*1);
    
    if (ret==NULL) {
      error(0,"Not enough memory for postgres sql connection\n");
      return ret;
    }
    
    tmp=strdup(u->value);
    tmp2=tmp;
    
    pgtty=NULL;pgoptions=NULL;
    
    if ((pghost=get_first_value(&tmp)) == NULL) {
      error(0,"Must define host for Postgres sql connection\n");
      free(tmp2);
      return NULL;
    } else {
      error(100,"Psql host is %s\n",pghost);
      if ((pgport=get_first_value(&tmp)) == NULL) {
	error(0,"Must define port for Postgres sql connection\n");
	free(tmp2);
	return NULL;
      } else {
	error(100,"Psql port is %s\n",pgport);
	if ((dbName=get_first_value(&tmp)) == NULL) {
	  error(0,"Must define name for database for Postgres sql connection\n");
	  free(tmp2);
	  return NULL;
	} else {
	  error(100,"Psql db is %s\n",dbName);
	  if ((login=get_first_value(&tmp)) == NULL) {
	    error(0,"Must define login for Postgres sql connection\n");
	    free(tmp2);
	    return NULL;
	  } else {
	    error(100,"Psql login is %s\n",login);
	    if ((pwd=get_first_value(&tmp)) == NULL) {
	      error(0,"Must define password for database for Postgres sql connection\n");
	      free(tmp2);
	      return NULL;
	    } else {
	      error(100,"Psql passwd is %s\n",pwd);
	      if ((ret->table=get_first_value(&tmp))==NULL) {
		error(0,"Must define table for sql..\n");
		free(tmp2);
		return NULL;
	      } else {
		if (ret->table[0]=='\0') {
		  error(0,"Must define table for sql..\n");
		  free(tmp2);
		  return NULL;
		} else {
		  /* everything went ok.. */
		}
	      }
	    }
	  }
	}
      }
    }
   
    /*printf("host:\"%s\" port:\"%s\" database:\"%s\" login:\"%s\" passwd:\"%s\" table:\"%s\"\n",pghost,pgport,dbName,login,pwd,ret->table); */
    
    
    if (login[0] == '\0' ) {
      login = NULL;
    }
    if (pwd[0] == '\0' ) {
      pwd = NULL;
    }
    
    ret->conn = PQsetdbLogin(pghost,pgport,pgoptions,pgtty,dbName,login,pwd);
    if (PQstatus(ret->conn) == CONNECTION_BAD){
      error(0,"Postgres sql error during connection\n");
      free(tmp2);
      return NULL;
    }
    /* Otherwise we would become to situation that name of table would
       be freeed 
    */
    ret->table = strdup(ret->table);
    
    /* And now we have made a connection to database.. 
       Next thing we do is to begin a new transaction block */
    
    ret->res = PQexec(ret->conn, "BEGIN");
    
    if (!ret->res || PQresultStatus(ret->res) != PGRES_COMMAND_OK) {
      error(0,"BEGIN command failed... \n");
      PQclear(ret->res);
      free(ret);
      ret=NULL;
    } else {
      PQclear(ret->res);
      if ((inout?be_sql_readinit(ret):RETOK)!=RETOK) {
	error(255,"Something went wrong with sql backend init.\n");
	return NULL;
      }
    }
    free(tmp2);
    return ret;
  }
#endif

  default:{
    error(0,"Unsupported backend:%i", u->type);
    return NULL;
  }    
  }
  /* Not reached */
  return NULL;

}

const char* aide_key_8=CONFHMACKEY_08;
const char* db_key_8=DBHMACKEY_08;