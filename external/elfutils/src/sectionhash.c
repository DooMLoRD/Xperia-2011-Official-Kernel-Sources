/* Section hash table implementation.
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

#include <string.h>

#include <elf-knowledge.h>
#include <ld.h>


/* Comparison function for sections.  */
static int
scnhead_compare (struct scnhead *one, struct scnhead *two)
{
  int result = strcmp (one->name, two->name);

  if (result == 0)
    {
      result = one->type - two->type;

      if (result == 0)
	{
	  GElf_Xword diff = (SH_FLAGS_IMPORTANT (one->flags)
			     - SH_FLAGS_IMPORTANT (two->flags));
	  result = diff < 0 ? -1 : diff == 0 ? 0 : 1;

	  if (result == 0)
	    {
	      result = one->entsize - two->entsize;

	      if (result == 0)
		{
		  result = (one->grp_signature == NULL
			    ? (two->grp_signature == NULL ? 0 : -1)
			    : (two->grp_signature == NULL
			       ? 1 : strcmp (one->grp_signature,
					     two->grp_signature)));

		  if (result == 0)
		    result = one->kind - two->kind;
		}
	    }
	}
    }

  return result;
}

/* Definitions for the section hash table.  */
#define TYPE struct scnhead *
#define NAME ld_section_tab
#define ITERATE 1
#define COMPARE(a, b) scnhead_compare (a, b)

#include "../lib/dynamicsizehash.c"
