/* Return section header.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Shdr *
gelf_getshdr (scn, dst)
     Elf_Scn *scn;
     GElf_Shdr *dst;
{
  GElf_Shdr *result = NULL;

  if (scn == NULL)
    return NULL;

  if (dst == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_OPERAND);
      return NULL;
    }

  rwlock_rdlock (scn->elf->lock);

  if (scn->elf->class == ELFCLASS32)
    {
      /* Copy the elements one-by-one.  */
      Elf32_Shdr *shdr = scn->shdr.e32 ?: INTUSE(elf32_getshdr) (scn);

      if (shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_OPERAND);
	  goto out;
	}

#define COPY(name) \
      dst->name = shdr->name
      COPY (sh_name);
      COPY (sh_type);
      COPY (sh_flags);
      COPY (sh_addr);
      COPY (sh_offset);
      COPY (sh_size);
      COPY (sh_link);
      COPY (sh_info);
      COPY (sh_addralign);
      COPY (sh_entsize);

      result = dst;
    }
  else
    {
      Elf64_Shdr *shdr = scn->shdr.e64 ?: INTUSE(elf64_getshdr) (scn);

      if (shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_OPERAND);
	  goto out;
	}

      /* We only have to copy the data.  */
      result = memcpy (dst, shdr, sizeof (GElf_Shdr));
    }

 out:
  rwlock_unlock (scn->elf->lock);

  return result;
}
INTDEF(gelf_getshdr)
