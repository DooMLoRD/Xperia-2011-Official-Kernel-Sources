/* Initialization of i386 specific backend library.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <libebl_i386.h>


int
i386_init (elf, machine, eh, ehlen)
     Elf *elf;
     GElf_Half machine;
     Ebl *eh;
     size_t ehlen;
{
  /* Check whether the Elf_BH object has a sufficent size.  */
  if (ehlen < sizeof (Ebl))
    return 1;

  /* We handle it.  */
  eh->name = "Intel 80386";
  eh->reloc_type_name = i386_reloc_type_name;
  eh->reloc_type_check = i386_reloc_type_check;
  eh->reloc_valid_use = i386_reloc_valid_use;
  eh->gotpc_reloc_check = i386_gotpc_reloc_check;
  eh->core_note = i386_core_note;
  eh->destr = i386_destr;

  return 0;
}
