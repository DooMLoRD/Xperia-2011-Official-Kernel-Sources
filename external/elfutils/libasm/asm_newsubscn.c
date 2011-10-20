/* Create new subsection section in given section.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <stdlib.h>

#include <libasmP.h>
#include <system.h>


AsmScn_t *
asm_newsubscn (asmscn, nr)
     AsmScn_t *asmscn;
     unsigned int nr;
{
  AsmScn_t *runp;
  AsmScn_t *newp;

  /* Just return if no section is given.  The error must have been
     somewhere else.  */
  if (asmscn == NULL)
    return NULL;

  /* Determine whether there is already a subsection with this number.  */
  runp = asmscn->subsection_id == 0 ? asmscn : asmscn->data.up;
  while (1)
    {
      if (runp->subsection_id == nr)
	/* Found it.  */
	return runp;

      if (runp->subnext == NULL || runp->subnext->subsection_id > nr)
	break;

      runp = runp->subnext;
    }

  newp = (AsmScn_t *) malloc (sizeof (AsmScn_t));
  if (newp == NULL)
    return NULL;

  /* Same assembler context than the original section.  */
  newp->ctx = runp->ctx;

  /* User provided the subsectio nID.  */
  newp->subsection_id = nr;

  /* Inherit the parent's type.  */
  newp->type = runp->type;

  /* Pointer to the zeroth subsection.  */
  newp->data.up = runp->subsection_id == 0 ? runp : runp->data.up;

  /* We start at offset zero.  */
  newp->offset = 0;
  /* And generic alignment.  */
  newp->max_align = 1;

  /* No output yet.  */
  newp->content = NULL;

  /* Inherit the fill pattern from the section this one is derived from.  */
  newp->pattern = asmscn->pattern;

  /* Enqueue at the right position in the list.  */
  newp->subnext = runp->subnext;
  runp->subnext = newp;

  return newp;
}
