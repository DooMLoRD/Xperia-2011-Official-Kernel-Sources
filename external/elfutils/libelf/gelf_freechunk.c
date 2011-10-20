/* Release uninterpreted chunk of the file contents.
   Copyright (C) 2002 Red Hat, Inc.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <libelf.h>
#include <stddef.h>
#include <stdlib.h>

#include "libelfP.h"


void
gelf_freechunk (elf, ptr)
     Elf *elf;
     char *ptr;
{
  if (elf == NULL)
    /* No valid descriptor.  */
    return;

  /* We do not have to do anything if the pointer returned by
     gelf_rawchunk points into the memory allocated for the ELF
     descriptor.  */
  if (ptr < (char *) elf->map_address + elf->start_offset
      || ptr >= ((char *) elf->map_address + elf->start_offset
		 + elf->maximum_size))
    free (ptr);
}
