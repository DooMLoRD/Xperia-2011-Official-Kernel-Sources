/* Return section type name.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <libeblP.h>


const char *
ebl_section_type_name (ebl, section, buf, len)
     Ebl *ebl;
     int section;
     char *buf;
     size_t len;
{
  const char *res = ebl->section_type_name (section, buf, len);

  if (res == NULL)
    {
      static const char *knowntypes[] =
	{
#define KNOWNSTYPE(name) [SHT_##name] = #name
	  KNOWNSTYPE (NULL),
	  KNOWNSTYPE (PROGBITS),
	  KNOWNSTYPE (SYMTAB),
	  KNOWNSTYPE (STRTAB),
	  KNOWNSTYPE (RELA),
	  KNOWNSTYPE (HASH),
	  KNOWNSTYPE (DYNAMIC),
	  KNOWNSTYPE (NOTE),
	  KNOWNSTYPE (NOBITS),
	  KNOWNSTYPE (REL),
	  KNOWNSTYPE (SHLIB),
	  KNOWNSTYPE (DYNSYM),
	  KNOWNSTYPE (INIT_ARRAY),
	  KNOWNSTYPE (FINI_ARRAY),
	  KNOWNSTYPE (PREINIT_ARRAY),
	  KNOWNSTYPE (GROUP),
	  KNOWNSTYPE (SYMTAB_SHNDX)
	};

      /* Handle standard names.  */
      if ((size_t) section < sizeof (knowntypes) / sizeof (knowntypes[0])
	  && knowntypes[section] != NULL)
	res = knowntypes[section];
      /* The symbol versioning/Sun extensions.  */
      else if (section >= SHT_LOSUNW && section <= SHT_HISUNW)
	{
	  static const char *sunwtypes[] =
	    {
#undef KNOWNSTYPE
#define KNOWNSTYPE(name) [SHT_##name - SHT_LOSUNW] = #name
	      KNOWNSTYPE (SUNW_move),
	      KNOWNSTYPE (SUNW_COMDAT),
	      KNOWNSTYPE (SUNW_syminfo),
	      KNOWNSTYPE (GNU_verdef),
	      KNOWNSTYPE (GNU_verneed),
	      KNOWNSTYPE (GNU_versym)
	    };
	  res = sunwtypes[section - SHT_LOSUNW];
	}
      else
	{
	  /* A few GNU additions.  */
	  if (section == SHT_CHECKSUM)
	    res = "CHECKSUM";
	  else if (section == SHT_GNU_LIBLIST)
	    res = "GNU_LIBLIST";
	  /* Handle OS-specific section names.  */
	  else
	    {
	      if (section >= SHT_LOOS && section <= SHT_HIOS)
		snprintf (buf, len, "SHT_LOOS+%x", section - SHT_LOOS);
	      /* Handle processor-specific section names.  */
	      else if (section >= SHT_LOPROC && section <= SHT_HIPROC)
		snprintf (buf, len, "SHT_LOPROC+%x", section - SHT_LOPROC);
	      else if ((unsigned int) section >= SHT_LOUSER
		       && (unsigned int) section <= SHT_HIUSER)
		snprintf (buf, len, "SHT_LOUSER+%x", section - SHT_LOUSER);
	      else
		snprintf (buf, len, "%s: %d", gettext ("<unknown>"), section);

	      res = buf;
	    }
	}
    }

  return res;
}
