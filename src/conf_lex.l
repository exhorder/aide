SC	[\\'"netbgr\ @#]
C	[a-zA-Z0-9���������_\\=$%�@&/!\^\~;:.,\?\{\}\(\)\[\]\<\>\-\+\*\|\'\`]

E	[\ ]*"="[\ ]*

L	[a-zA-Z���������0-9_]
D	[0-9]

PC	\\[0-3][0-7][0-7]

EX      [" "\t]*

%{

#define YYDEBUG

/*
 * Copyright (C) 1999,2000,2001,2002 Rami Lehti, Pablo Virolainen
 * $Header$
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
#include "conf_yacc.h"
#include "report.h"
#include "list.h"
#include "symboltable.h"
#include "commandconf.h"
#include "db_config.h"

void includehandler() ;
void conf_put_token(const char*);

int firstnotempty(char* s);

typedef struct conf_buffer_type {
  char* buff;
  char* pos;
} conf_buffer_type;

list* l_symt=NULL;
list* conf_buffer=NULL;
int condition=0;
int varbol=0;

long conf_lineno=1;
int newlinelastinconfig=0;

#define MAX_INCLUDE_DEPTH 10
YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
int include_stack_ptr = 0;

#define YY_INPUT(buf,result,max_size) \
        if( ((result=conf_input_wrapper(buf,max_size,yyin)) == 0) \
            && ferror(yyin) ) \
          YY_FATAL_ERROR( "input in flex scanner failed" );

int var_in_conflval=0;

%}

%Start A EXPR DEFSTMT DEFSTMT2 IFDEFSTMT INCLUDE EXPREQUHUNT VARSUB CONFVALHUNT IFHOSTSTMT

%%

"#"[^\n]* {
 }


^[^\n]*"\@\@\{"({L}+)"}"[^\n]* {
  
  putbackvariable(conftext);

  error(230,"%i:Variable substitution\n",conf_lineno);
  
}



^[\ \t]*"=/"({C}|{PC})* {
  conflval.s=strdup(conftext+1+firstnotempty(conftext));
  BEGIN(EXPR);
  return (TEQURXRULE);
}

^[\ \t]*"/"({C}|{PC})* {
  conflval.s=strdup(conftext+firstnotempty(conftext));
  error(230,"Selrule\n");
  BEGIN(EXPR);
  return (TSELRXRULE );
}

^[\ \t]*"!/"({C}|{PC})* {  
  conflval.s=strdup(conftext+1+firstnotempty(conftext));
  error(230,"Negrule\n");
  return (TNEGRXRULE );
}

^[\ \t]*({L})+ {
  conflval.s=strdup(conftext+firstnotempty(conftext));
  error(230,"Equrule\n");
  BEGIN(EXPREQUHUNT);
  return (TSTRING);
}

<EXPREQUHUNT>[\ \t]*"="[\ \t]* {
  BEGIN(EXPR);
  return('=');
}


<EXPR>[\ \t]*({L}|{D}|">")+ {
  conflval.s=strdup(conftext+firstnotempty(conftext));
  return (TSTRING);
}

<EXPR>[\ \t]*\n {
  conf_lineno++;
  return (TNEWLINE);
  BEGIN 0;
}

<EXPR>\+ {
  return ('+');
}

<EXPR>\- {
  return ('-');
}

<DEFSTMT>({L})+ {
  error(230," %s",conftext);
  conflval.s=strdup(conftext);
  BEGIN (DEFSTMT2);
  return (TSTRING);
}

<DEFSTMT2>({C})+ {
  error(230," %s",conftext);
  conflval.s=strdup(conftext);
  return (TSTRING);
}

<DEFSTMT2>[\ \t]*"\n" { 
  error(230,"\n");
  conf_lineno++;
  BEGIN 0;
  return (TNEWLINE);
}

^[\ \t]*"\@\@define" {
  error(230,"@@define");
  BEGIN DEFSTMT;
  return (TDEFINE);
}

^[\ \t]*"\@\@undef" {
  error(230,"@@undef");
  BEGIN IFDEFSTMT;
  return (TUNDEF);
}


^[\ \t]*"\@\@ifndef" {
  error(230,"@@ifndef");
  BEGIN IFDEFSTMT;
  return (TIFNDEF);
}

^[\ \t]*"\@\@ifdef" {
  error(230,"@@ifdef");
  BEGIN IFDEFSTMT;
  return (TIFDEF);
}

^[\ \t]*"\@\@else" {
  error(230,"@@else");
  BEGIN 0;
  return (TELSE);
}

^[\ \t]*"\@\@endif" {
  error(230,"@@endif");
  BEGIN 0;
  return (TENDIF);
}

<IFDEFSTMT>({L})+ {
  error(230," %s",conftext);
  conflval.s=strdup(conftext);
  BEGIN 0;
  return (TSTRING);
}

^[\ \t]*"\@\@ifhost" {
  error(230,"@@ifhost");
  BEGIN IFHOSTSTMT;
  return (TIFHOST);
}

^[\ \t]*"\@\@ifnhost" {
  error(230,"@@ifnhost");
  BEGIN IFHOSTSTMT;
  return (TIFNHOST);
}

<IFHOSTSTMT>({L}|"-")+ {
  error(230," %s",conftext);
  conflval.s=strdup(conftext);
  BEGIN 0;
  return (TSTRING);
}

^[\ \t]*"\@\@db_spec" {
  conf_put_token("\n@@db_spec");
  error(230,"@@db_spec");
  return (TDBSPEC);
}

^[\ \t]*"\@\@begin_config" {
  error(230,"@@begin_config");
  BEGIN CONFVALHUNT;
  return (TBEGIN_CONFIG);
}

^[\ \t]*"\@\@end_config" {
  return (TEND_CONFIG);
}

^[\ \t]*"\@\@include" {
  BEGIN INCLUDE;
}

<INCLUDE>[^ \t\n]+ {
  includehandler();
  BEGIN 0;
  error(230,"@@include\n");
  return (TNEWLINE);
}

<<EOF>> {
        if (yy_hold_char=='\n'){
            newlinelastinconfig=1;
        }else {
            newlinelastinconfig=0;
        }
        if ( --include_stack_ptr < 0 )
            {
            yyterminate();
            }

        else
            {

            if (confin != stdin ) {
              fclose(confin);
              }

            conf_delete_buffer( YY_CURRENT_BUFFER );
            conf_switch_to_buffer(
                 include_stack[include_stack_ptr] );
            }
         return TEOF;
}


[\t\ ]+ {}

"\n" { 
  conf_lineno++;
  BEGIN 0;
  return (TNEWLINE);
 }

^[\t\ ]*"database"{E} {
  error(230,"database =\n");
  BEGIN CONFVALHUNT;
  return (TDATABASE);
} 

^[\t\ ]*"database_out"{E}  {
  error(230,"database_out =\n");
  BEGIN CONFVALHUNT;
  return (TDATABASE_OUT);
}

^[\t\ ]*"database_new"{E}  {
  error(230,"database_new =\n");
  BEGIN CONFVALHUNT;
  return (TDATABASE_NEW);
}

^[\t\ ]*"warn_dead_symlinks"{E} {
  error(230,"warn_dead_symlinks =\n");
  BEGIN CONFVALHUNT;
  return (TWARNDEADSYMLINKS);
}

^[\t\ ]*"acl_no_symlink_follow"{E} {
  error(230,"acl_no_symlink_follow =\n");
  BEGIN CONFVALHUNT;
  return (TACLNOSYMLINKFOLLOW);
}


^[\t\ ]*"verbose"{E} {
  error(230,"verbose =\n");
  return (TVERBOSE);  
}

^[\t\ ]*"report_url"{E} {
  error(230,"report_url =\n");
  BEGIN CONFVALHUNT;
  return (TREPORT_URL);  
}

^[\t\ ]*"gzip_dbout"{E} {
  error(230,"gzip_dbout =\n");
  BEGIN CONFVALHUNT;
  return (TGZIPDBOUT);
}

^[\t\ ]*"recstop"{E} {
  error(230,"recstop =\n");
  BEGIN CONFVALHUNT;
  return (TRECSTOP);
}

^[\ \t]*"config_version"{E} {
  error(230,"config_version =\n");
  BEGIN CONFVALHUNT;
  return (TCONFIG_VERSION);
}

<CONFVALHUNT>("yes"|"true") {
  BEGIN 0;
  return (TTRUE);
}

<CONFVALHUNT>("no"|"false") {
  BEGIN 0;
  return (TFALSE);
}

<CONFVALHUNT>({C})+ {
  conflval.s=strdup(conftext);
  BEGIN 0;
  return (TSTRING);
}


({L})+ {
  conflval.s=strdup(conftext);
  return (TSTRING);
}


[^\n] { return(conftext[0]); }


%%

int confwrap(){
  return 1;
}

void conf_put_token(const char* s){

  int i=0;

  for(i=strlen(s)-1;i>=0;i--){
       unput(s[i]);
       if(s[i]=='\n'){
          conf_lineno--;
       }
  }
  
}

int firstnotempty(char* s){
  
  int i=0;

  if (s==NULL) {
    return i;
  }


  while( s[i]==' ' || s[i]=='\t') {
    i++;
  }

  return i;

}

const char* aide_key_4=CONFHMACKEY_04;
const char* db_key_4=DBHMACKEY_04;

void includehandler() { 
  /* got the include file name */
  if ( include_stack_ptr >= MAX_INCLUDE_DEPTH )
    {
      error( 0, "Config includes nested too deeply\n" );
      exit( 1 );
    }

  if (strcmp(conftext,"-")==0) {
      confin = stdin;
    } else {
      confin = fopen( conftext, "r" );
    }
  if ( ! confin ) {
    error(0,"Cannot open config file %s\n",conftext);
    exit(IO_ERROR);
  } else {
    include_stack[include_stack_ptr++] = YY_CURRENT_BUFFER;
    conf_switch_to_buffer(conf_create_buffer( confin, YY_BUF_SIZE ) );
  }
  
}
