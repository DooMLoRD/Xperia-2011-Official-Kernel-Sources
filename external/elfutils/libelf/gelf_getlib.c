/* Get library from table at the given index.
   Copyright (C) 2004 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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

#include <assert.h>
#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Lib *
gelf_getlib (data, ndx, dst)
     Elf_Data *data;
     int ndx;
     GElf_Lib *dst;
{
  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_LIB))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;

  rwlock_rdlock (data_scn->s->elf->lock);

  /* The on disk format of Elf32_Lib and Elf64_Lib is identical.  So
     we can simplify things significantly.  */
  assert (sizeof (GElf_Lib) == sizeof (Elf32_Lib));
  assert (sizeof (GElf_Lib) == sizeof (Elf64_Lib));

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  GElf_Lib *result = NULL;
  if (unlikely ((ndx + 1) * sizeof (GElf_Lib) > data->d_size))
    __libelf_seterrno (ELF_E_INVALID_INDEX);
  else
    {
      *dst = ((GElf_Lib *) data->d_buf)[ndx];

      result = dst;
    }

  rwlock_unlock (data_scn->s->elf->lock);

  return result;
}
