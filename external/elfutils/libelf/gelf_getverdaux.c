/* Get additional symbol version definition information at the given offset.
   Copyright (C) 1999, 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1999.

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


GElf_Verdaux *
gelf_getverdaux (data, offset, dst)
     Elf_Data *data;
     int offset;
     GElf_Verdaux *dst;
{
  GElf_Verdaux *result;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_VDEF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* It's easy to handle this type.  It has the same size for 32 and
     64 bit objects.  */
  assert (sizeof (GElf_Verdaux) == sizeof (Elf32_Verdaux));
  assert (sizeof (GElf_Verdaux) == sizeof (Elf64_Verdaux));

  rwlock_rdlock (((Elf_Data_Scn *) data)->s->elf->lock);

  /* The data is already in the correct form.  Just make sure the
     index is OK.  */
  if (unlikely (offset < 0)
      || unlikely (offset + sizeof (GElf_Verdaux) > data->d_size)
      || unlikely (offset % __alignof__ (GElf_Verdaux) != 0))
    {
      __libelf_seterrno (ELF_E_OFFSET_RANGE);
      result = NULL;
    }
  else
    result = (GElf_Verdaux *) memcpy (dst, (char *) data->d_buf + offset,
				      sizeof (GElf_Verdaux));


  rwlock_unlock (((Elf_Data_Scn *) data)->s->elf->lock);

  return result;
}
