/* written by Qajin  */
/* made   2000/09/07 */
/* update 2000/11/22 */
/* Copyleft by Qajin */

#ifndef __PHI_STR_H__
#define __PHI_STR_H__

#define CPYSTR(d,s)  strlcpy((d),(s),sizeof(d))
#define CATSTR(d,s)  strlcat((d),(s),sizeof(d))

char *dmprintf(int, int, char *, int, char *, ...);
void dmcheck_write(int, char *);
int cmtstrcpy(char *, int, char *, int);
void nstr(char *, char, int);

#endif
