/* Interface for libebl_mips module.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _LIBEBL_MIPS_H
#define _LIBEBL_MIPS_H 1

#include <libeblP.h>


/* Constructor.  */
extern int mips_init (Elf *elf, GElf_Half machine, Ebl *eh, size_t ehlen);

/* Destructor.  */
extern void mips_destr (Ebl *bh);


/* Function to get relocation type name.  */
extern const char *mips_reloc_type_name (int type, char *buf, size_t len);

/* Function to get segment type name.  */
extern const char *mips_segment_type_name (int type, char *buf, size_t len);

/* Function to get setion type name.  */
extern const char *mips_section_type_name (int type, char *buf, size_t len);

/* Function to get machine flag name.  */
extern const char *mips_machine_flag_name (Elf64_Word *flags);

/* Function to get dynamic tag name.  */
extern const char *mips_dynamic_tag_name (int64_t tag, char *buf, size_t len);

#endif	/* libebl_mips.h */
