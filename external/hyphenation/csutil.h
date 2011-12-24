#ifndef __CSUTILHXX__
#define __CSUTILHXX__


/* First some base level utility routines */

/* remove end of line char(s) */
void   mychomp(char * s);

/* duplicate string  */
char * mystrdup(const char * s);

/* parse into tokens with char delimiter */
char * mystrsep(char ** sptr, const char delim);


/* character encoding information */

struct cs_info {
  unsigned char ccase;
  unsigned char clower;
  unsigned char cupper;
};


struct enc_entry {
  const char * enc_name;
  struct cs_info * cs_table;
};

/* language to encoding default map */

struct lang_map {
  const char * lang;
  const char * def_enc;
};

struct cs_info * get_current_cs(const char * es);

const char * get_default_enc(const char * lang);

/* convert null terminated string to all caps using encoding  */
void enmkallcap(char * d, const char * p, const char * encoding);

/* convert null terminated string to all little using encoding */
void enmkallsmall(char * d, const char * p, const char * encoding);

/* convert null terminated string to have intial capital using encoding */
void enmkinitcap(char * d, const char * p, const char * encoding);

/* convert null terminated string to all caps  */
void mkallcap(char * p, const struct cs_info * csconv);

/* convert null terminated string to all little */
void mkallsmall(char * p, const struct cs_info * csconv);

/* convert null terminated string to have intial capital */
void mkinitcap(char * p, const struct cs_info * csconv);


#endif
