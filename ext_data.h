
#ifndef __PHI_EXTEND_DATA__
#define __PHI_EXTEND_DATA__

#define MAXKEYLEN		256
#define MAXVALUELEN		256

int get_property(char **,char *,int,char *,int);
int match_property(char *,char *);
int match_oneproperty(char *,char *);
int set_property(char *,char *,int,char *,char *);
int get_propertyvalue(char *,char *,char *,int);

#endif
