/* Return object file type name.
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
ebl_object_type_name (ebl, object, buf, len)
     Ebl *ebl;
     int object;
     char *buf;
     size_t len;
{
  const char *res;

  res = ebl != NULL ? ebl->object_type_name (object, buf, len) : NULL;
  if (res == NULL)
    {
      /* Handle OS-specific section names.  */
      if (object >= ET_LOOS && object <= ET_HIOS)
	snprintf (buf, len, "LOOS+%x", object - ET_LOOS);
      /* Handle processor-specific section names.  */
      else if (object >= ET_LOPROC && object <= ET_HIPROC)
	snprintf (buf, len, "LOPROC+%x", object - ET_LOPROC);
      else
	snprintf (buf, len, "%s: %d", gettext ("<unknown>"), object);

      res = buf;
    }

  return res;
}
