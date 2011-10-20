/* Define new symbol for current position in given section.
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <system.h>


AsmSym_t *
asm_newsym (asmscn, name, size, type, binding)
     AsmScn_t *asmscn;
     const char *name;
     GElf_Xword size;
     int type;
     int binding;
{
#define TEMPSYMLEN 10
  char tempsym[TEMPSYMLEN];
  AsmSym_t *result;

  if (asmscn == NULL)
    /* Something went wrong before.  */
    return NULL;

  /* Generate a temporary symbol if necessary.  */
  if (name == NULL)
    {
      /* If a local symbol name is created the symbol better have
	 local binding.  */
      if (binding != STB_LOCAL)
	{
	  __libasm_seterrno (ASM_E_INVALID);
	  return NULL;
	}

      // XXX This requires getting the format from the machine backend.  */
      snprintf (tempsym, TEMPSYMLEN, ".L%07u", asmscn->ctx->tempsym_count++);
    }

  result = (AsmSym_t *) malloc (sizeof (AsmSym_t));
  if (result == NULL)
    return NULL;

  rwlock_wrlock (asmscn->ctx->lock);

  result->scn = asmscn;
  result->offset = asmscn->offset;
  result->size = size;
  result->type = type;
  result->binding = binding;
  result->symidx = 0;
  result->strent = ebl_strtabadd (asmscn->ctx->symbol_strtab,
				  name ?: tempsym, 0);

  if (unlikely (asmscn->ctx->textp))
    {
      /* We are only interested in the name and don't need to know whether
	 it is a local name or not.  */
      if (name == NULL)
	name = tempsym;

      /* First print the binding pseudo-op.  */
      if (binding == STB_GLOBAL)
	fprintf (asmscn->ctx->out.file, "\t.globl\t%s\n", name);
      else if (binding == STB_WEAK)
	fprintf (asmscn->ctx->out.file, "\t.weak\t%s\n", name);

      /* Next the symbol type.  */
      if (type == STT_OBJECT)
	fprintf (asmscn->ctx->out.file, "\t.type\t%s,@object\n", name);
      else if (type == STT_FUNC)
	fprintf (asmscn->ctx->out.file, "\t.type\t%s,@function\n", name);

      /* Finally the size and the label.  */
      fprintf (asmscn->ctx->out.file, "\t.size\t%s,%" PRIuMAX "\n%s:\n",
	       name, (uintmax_t) size, name);
    }
  else
    {
      /* Put the symbol in the hash table so that we can later find it.  */
      if (asm_symbol_tab_insert (&asmscn->ctx->symbol_tab,
				 elf_hash (name ?: tempsym), result) != 0)
	{
	  /* The symbol already exists.  */
	  __libasm_seterrno (ASM_E_DUPLSYM);
	  free (result);
	  result = NULL;
	}
      else if (name != NULL && asm_emit_symbol_p (name))
	/* Only count non-private symbols.  */
	++asmscn->ctx->nsymbol_tab;
    }

  rwlock_unlock (asmscn->ctx->lock);

  return result;
}
