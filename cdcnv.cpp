
/*  Phantasmal Island ( Character Data Converter ) */

#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#include  <math.h>
#include  <sys/time.h>
#include  <time.h>
#include  <sys/types.h>
#ifdef Mips
#include  <sys/timeb.h>
#else
#include  <stdlib.h>
#endif
#include  <errno.h>

#ifdef DEBUG
char debug_buf[50];
#endif

#include "phi_dm.cnf"

#ifndef EPHI_FastBind_CD_NUM
#define EPHI_FastBind_CD_NUM 256
#endif

#include "phi_str.h"

static char *cd[2] = {"phi.cd","phi.sav"};
static char *old[2] = {"phi_cd.old","phi_sav.old"};
static char *bak[2] = {"phi.cd.bak","phi.sav.bak"};
static char *cdi[2] = {"phi_cdi/cd.%i","phi_cdi/sav.%i"};


void usage() {
    fprintf(stderr, "Usage: cdcnv (options)\n");
    fprintf(stderr, " option:\n");
    fprintf(stderr, "  -n [num] : separate phi.cd into cdi files of specified number.\n");
    fprintf(stderr, "           : default num is EPHI_FastBind_CD_NUM(256).\n");
    fprintf(stderr, "  -r       : reverse from cdi into phi.cd.\n");
    fprintf(stderr, "  -v       : verbose display process.\n");
    fprintf(stderr, "  -s       : process phi.sav.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  under options no action with -r.\n");
    fprintf(stderr, "  -d [num] : do not move character specifed days no access .\n");
    fprintf(stderr, "  -e [num] : do not move character under specified experience.\n");
    exit(0);
}

int    main(int argc, char **argv)
{
  char   buf[1024];
  char   nm[32];
  char   file[32];
  int    n;
  int    i, j, k;
  FILE  *rfp,*wfp;
  int    mode;
  int    hash;
  unsigned long   last,clast;
  long   exp,cexp;
  int    verbose;
  int    s;

  n = 0;
  mode = 0;
  last = 0;
  exp = 0;
  verbose = 0;
  s = 0;
  for (i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-r")) {
        mode = -1;
    } else if (!strcmp(argv[i],"-n")) {
        i++;
        if (i < argc)
            sscanf(argv[i],"%i",&n);
        else
            usage();
    } else if (!strcmp(argv[i],"-d")) {
    /* check last time */
        i++;
        if (i < argc) {
            sscanf(argv[i],"%i",&j);
            last = time(NULL) - j * 60 * 60 * 24;
        }
        else
            usage();
    } else if (!strcmp(argv[i],"-e")) {
    /* check experience */
        i++;
        if (i < argc)
            sscanf(argv[i],"%li",&exp);
        else
            usage();
    } else if (!strncmp(argv[i],"-h",2)) {
        usage();
        exit(0);
    } else if (!strcmp(argv[i],"-v")) {
        verbose = 1;
    } else if (!strcmp(argv[i],"-s")) {
        s = 1;
    }
  }

  if (!n)
    n = EPHI_FastBind_CD_NUM;

  if ((rfp = fopen(cd[s], "rb")) == NULL) {
    fprintf(stderr, "Can not open %s\n",cd[s]);
    exit(0);
  }

  if(!mode) {
    /* separate mode */
    /* phi.cd -> cd.old */
    if ((wfp = fopen(old[s], "rb")) != NULL) {
      fclose(wfp);
      fprintf(stderr, "Exist %s\n",old[s]);
      fprintf(stderr, "Stop convert to refuse rewriting %s\n",old[s]);
      exit(0);
    }
    if ((wfp = fopen(old[s], "wb")) == NULL) {
      fclose(rfp);
      fprintf(stderr, "Can not open %s\n",old[s]);
      exit(0);
    }
  } else {
    /* reverse mode */
    /* phi.cd -> phi.cd.bak */
    if ((wfp = fopen(bak[s], "wb")) == NULL) {
      fclose(rfp);
      fprintf(stderr, "Can not open %s\n",bak[s]);
      exit(0);
    }
  }

  fprintf(stderr, "Start backup %s...\n",cd[s]);
  /* phi.cd backup */
  while (1) {
    if (fgets(buf, 255, rfp) == NULL)
      break;
    if (*buf == '.') /* for safety ... */
      break;
    fputs(buf, wfp);
    while (1) {
      if (fgets(buf, 255, rfp) == NULL)
        break;
      fputs(buf, wfp);
      if (*buf == '.')
        break;
    }
  }
  CPYSTR(buf, ".\n");
  fputs(buf, wfp);
  fclose(wfp);
  fclose(rfp);
  fprintf(stderr, "Backup complete.\n");

  if (!mode) {
    fprintf(stderr, "Start separating...\n");
    if ((rfp = fopen(old[s], "rb")) == NULL) {
      fprintf(stderr, "Can not open %s\n",old[s]);
      exit(0);
    }
    while (1) {
      if (fgets(buf, 255, rfp) == NULL)
        break;
      if (*buf == '.') /* for safety ... */
        break;
      if (*buf != ':') /* not ID ...? */
        continue;
      hash = 0;
      for(i=1;i < 16 && buf[i] != ' ' && buf[i] != '\n';i++)
        hash += (short)(int)buf[i];
      hash = hash % n;
      sscanf(buf+116,"%08lx",&clast);
      sscanf(buf+97,"%08li",&cexp);
      strlcpy(nm,buf+17,32);
      if (clast < last || cexp < exp) {
        if (verbose)
            fprintf(stderr,(clast < last ? "%s : no access\n":"%s : less experience\n"),nm);
        while (1) {
          if (fgets(buf, 255, rfp) == NULL)
            break;
          if (*buf == '.')
            break;
        }
      } else {
        snprintf(file,sizeof(file),cdi[s],hash);
        if (verbose)
            fprintf(stderr,"%s -> %s\n",nm,file);
        if ((wfp = fopen(file, "ab")) == NULL) {
          snprintf(buf,sizeof(buf)-1,"Can not open %s\n",file);
          fprintf(stderr, buf);
          fclose(rfp);
          exit(0);
        }
        fputs(buf, wfp);
        while (1) {
          if (fgets(buf, 255, rfp) == NULL)
            break;
          fputs(buf, wfp);
          if (*buf == '.')
            break;
        }
        fclose(wfp);
      }
    }
    fclose(rfp);

/* cdiは末尾 ．2行にはならない(しない) */
/* 中身0の cdiのみ ．1行出力 */
    for(hash=0;hash < n;hash++) {
      snprintf(file,sizeof(file),cdi[s],hash);
      if ((rfp = fopen(file, "rb")) != NULL) {
        fclose(rfp);
      } else {
        if ((wfp = fopen(file, "wb")) == NULL) {
          snprintf(buf,sizeof(buf),"Can not open %s\n",file);
          fprintf(stderr, buf);
          exit(0);
        }
        CPYSTR(buf, ".\n");
        fputs(buf, wfp);
        fclose(wfp);
      }
    }
/* 二連続起動による cd破壊が起き易いので削除
    if ((wfp = fopen(cd[s], "wb")) == NULL) {
      fprintf(stderr,"Can not open %s\n",cd[s]);
      exit(0);
    }
    CPYSTR(buf, ".\n");
    fputs(buf, wfp);
    fclose(wfp);
*/
  } else {
    fprintf(stderr, "Start reversing...\n");
    if (verbose)
      fprintf(stderr, "import from %s...\n",bak[s]);
    if ((rfp = fopen(bak[s], "rb")) == NULL) {
      fprintf(stderr, "Can not open %s\n",bak[s]);
      exit(0);
    }
    if ((wfp = fopen(cd[s], "wb")) == NULL) {
      fclose(rfp);
      fprintf(stderr, "Can not open %s\n",cd[s]);
      exit(0);
    }
    while (1) {
      if (fgets(buf, 255, rfp) == NULL)
        break;
      if (*buf == '.') /* for safety ... */
        break;
      if (*buf != ':') /* not ID ...? */
        continue;
      fputs(buf, wfp);
      while (1) {
        if (fgets(buf, 255, rfp) == NULL)
          break;
        fputs(buf, wfp);
        if (*buf == '.')
          break;
      }
    }
    fclose(rfp);
    if (verbose)
      fprintf(stderr, "%s complete.\n",bak[s]);

    for(hash=0;hash < 0xffff;hash++) {
      snprintf(file,sizeof(file),cdi[s],hash);
      if ((rfp = fopen(file, "rb")) == NULL) /* no files after this...? */
        break;
      if (verbose)
        fprintf(stderr,"import from %s.\n",file);
      while (1) {
        if (fgets(buf, 255, rfp) == NULL)
          break;
        if (*buf == '.') /* for safety ... */
          break;
        if (*buf != ':') /* not ID ...? */
          continue;
        fputs(buf, wfp);
        while (1) {
          if (fgets(buf, 255, rfp) == NULL)
            break;
          fputs(buf, wfp);
          if (*buf == '.')
            break;
        }
      }
      fclose(rfp);
      if (verbose)
        fprintf(stderr,"%s complete.\n",file);
    }
/*    CPYSTR(buf, ".\n");
    fputs(buf, wfp); */
    fclose(wfp);
  }

  fprintf(stderr,"Convert compleated.\n");
}
