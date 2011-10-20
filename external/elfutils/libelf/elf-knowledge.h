/* Accumulation of various pieces of knowledge about ELF.
   Copyright (C) 2000, 2001, 2002, 2003 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#ifndef _ELF_KNOWLEDGE_H
#define _ELF_KNOWLEDGE_H	1


/* Test whether a section can be stripped or not.  */
#define SECTION_STRIP_P(ebl, elf, ehdr, shdr, name, remove_comment, remove_debug) \
  (/* Sections which are allocated are not removed.  */			      \
   ((shdr)->sh_flags & SHF_ALLOC) == 0					      \
   /* We never remove .note sections.  */				      \
   && (shdr)->sh_type != SHT_NOTE					      \
   /* If only debug information should be removed check the name.  There      \
      is unfortunately no way.  */					      \
   && (!remove_debug							      \
       || ebl_debugscn_p (ebl, name)					      \
       || (((shdr)->sh_type == SHT_RELA || (shdr)->sh_type == SHT_REL)	      \
	   && ({ Elf_Scn *scn_l = elf_getscn (elf, (shdr)->sh_info);	      \
		 GElf_Shdr shdr_mem_l;					      \
		 GElf_Shdr *shdr_l = gelf_getshdr (scn_l, &shdr_mem_l);	      \
		 const char *s_l;					      \
		 shdr_l != NULL						      \
		   && (s_l = elf_strptr (elf,				      \
					 ((GElf_Ehdr *) (ehdr))->e_shstrndx,  \
					 shdr_l->sh_name)) != NULL	      \
		   && ebl_debugscn_p (ebl, s_l); })))			      \
   && ((shdr)->sh_type != SHT_PROGBITS					      \
   /* Never remove .gnu.warning.* sections.  */				      \
       || (strncmp (name, ".gnu.warning.", sizeof ".gnu.warning." - 1) != 0   \
   /* We remove .comment sections only if explicitly told to do so.  */	      \
	   && ((remove_comment) || strcmp (name, ".comment") != 0)))	      \
   /* So far we do not remove any of the non-standard sections.		      \
      XXX Maybe in future.  */						      \
   && (shdr)->sh_type < SHT_NUM)


/* Test whether `sh_info' field in section header contains a section
   index.  There are two kinds of sections doing this:

   - the sections containing relocation information reference in this
     field the section to which the relocations apply;

   - section with the SHF_INFO_LINK flag set to signal that `sh_info'
     references a section.  This allows correct handling of unknown
     sections.  */
#define SH_INFO_LINK_P(Shdr) \
  ((Shdr)->sh_type == SHT_REL || (Shdr)->sh_type == SHT_RELA		      \
   || ((Shdr)->sh_flags & SHF_INFO_LINK) != 0)


/* When combining ELF section flags we must distinguish two kinds:

   - flags which cause problem if not added to the result even if not
     present in all input sections

   - flags which cause problem if added to the result if not present
     in all input sections

   The following definition is for the general case.  There might be
   machine specific extensions.  */
#define SH_FLAGS_COMBINE(Flags1, Flags2) \
  (((Flags1 | Flags2)							      \
    & (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR | SHF_LINK_ORDER		      \
       | SHF_OS_NONCONFORMING | SHF_GROUP))				      \
   | (Flags1 & Flags2 & (SHF_MERGE | SHF_STRINGS | SHF_INFO_LINK)))

/* Similar macro: return the bits of the flags which necessarily must
   match if two sections are automatically combined.  Sections still
   can be forcefully combined in which case SH_FLAGS_COMBINE can be
   used to determine the combined flags.  */
#define SH_FLAGS_IMPORTANT(Flags) \
  ((Flags) & ~((GElf_Xword) 0 | SHF_LINK_ORDER | SHF_OS_NONCONFORMING))


/* Size of an entry in the hash table.  The ELF specification says all
   entries are regardless of platform 32-bits in size.  Early 64-bit
   ports (namely Alpha for Linux) got this wrong.  The wording was not
   clear.

   Several years later the ABI for the 64-bit S390s was developed.
   Many things were copied from the IA-64 ABI (which uses the correct
   32-bit entry size) but what do these people do?  They use 64-bit
   entries.  It is really shocking to see what kind of morons are out
   there.  And even worse: they are allowed to design ABIs.  */
#define SH_ENTSIZE_HASH(Ehdr) \
  ((Ehdr)->e_machine == EM_ALPHA					      \
   || ((Ehdr)->e_machine == EM_S390					      \
       && (Ehdr)->e_ident[EI_CLASS] == ELFCLASS64) ? 8 : 4)

#endif	/* elf-knowledge.h */
