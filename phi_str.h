/* written by Qajin  */
/* made   2000/09/07 */
/* update 2000/11/22 */
/* Copyleft by Qajin */

#ifndef __PHI_STR_H__
#define __PHI_STR_H__

#if defined(__MINGW32__)
/* MinGW-w64 のヘッダ/ランタイムには strlcpy/strlcat が無いので、
   同梱 bsd/ の実装を使う。そのための宣言
   (FreeBSD/glibc では <string.h> が宣言する)。 */
#include <cstddef>
extern "C" size_t strlcpy(char *dst, const char *src, size_t siz);
extern "C" size_t strlcat(char *dst, const char *src, size_t siz);
#endif

#define CPYSTR(d,s)  strlcpy((d),(s),sizeof(d))
#define CATSTR(d,s)  strlcat((d),(s),sizeof(d))

char *dmprintf(int, int, char *, int, char *, ...);
void dmcheck_write(int, char *);
int cmtstrcpy(char *, int, char *, int);
void nstr(char *, char, int);

#endif
