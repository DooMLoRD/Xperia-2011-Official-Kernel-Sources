/* Write changed data structures.
   Copyright (C) 2000, 2001, 2002, 2004 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <libelf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include "libelfP.h"


#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


static int
compare_sections (const void *a, const void *b)
{
  const Elf_Scn **scna = (const Elf_Scn **) a;
  const Elf_Scn **scnb = (const Elf_Scn **) b;

  if ((*scna)->shdr.ELFW(e,LIBELFBITS)->sh_offset
      < (*scnb)->shdr.ELFW(e,LIBELFBITS)->sh_offset)
    return -1;

  if ((*scna)->shdr.ELFW(e,LIBELFBITS)->sh_offset
      > (*scnb)->shdr.ELFW(e,LIBELFBITS)->sh_offset)
    return 1;

  if ((*scna)->index < (*scnb)->index)
    return -1;

  if ((*scna)->index > (*scnb)->index)
    return 1;

  return 0;
}


/* Insert the sections in the list into the provided array and sort
   them according to their start offsets.  For sections with equal
   start offsets the section index is used.  */
static void
sort_sections (Elf_Scn **scns, Elf_ScnList *list)
{
  Elf_Scn **scnp = scns;
  do
    {
      size_t cnt;

      for (cnt = 0; cnt < list->cnt; ++cnt)
	*scnp++ = &list->data[cnt];
    }
  while ((list = list->next) != NULL);

  qsort (scns, scnp - scns, sizeof (*scns), compare_sections);
}


int
internal_function_def
__elfw2(LIBELFBITS,updatemmap) (Elf *elf, int change_bo, size_t shnum)
{
  ElfW2(LIBELFBITS,Ehdr) *ehdr;
  xfct_t fctp;
  char *last_position;

  /* We need the ELF header several times.  */
  ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

  /* Write out the ELF header.  */
  if ((elf->state.ELFW(elf,LIBELFBITS).ehdr_flags | elf->flags) & ELF_F_DIRTY)
    {
      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Ehdr))
	      == elf_typesize (LIBELFBITS, ELF_T_EHDR, 1));

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR]
#endif

	  /* Do the real work.  */
	  (*fctp) ((char *) elf->map_address + elf->start_offset, ehdr,
		   sizeof (ElfW2(LIBELFBITS,Ehdr)), 1);
	}
      else
	memcpy (elf->map_address + elf->start_offset, ehdr,
		sizeof (ElfW2(LIBELFBITS,Ehdr)));

      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags &= ~ELF_F_DIRTY;
    }

  /* Write out the program header table.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL
      && ((elf->state.ELFW(elf,LIBELFBITS).phdr_flags | elf->flags)
	  & ELF_F_DIRTY))
    {
      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Phdr))
	      == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

      /* Maybe the user wants a gap between the ELF header and the program
	 header.  */
      if (ehdr->e_phoff > ehdr->e_ehsize)
	memset (elf->map_address + elf->start_offset + ehdr->e_ehsize,
		__libelf_fill_byte, ehdr->e_phoff - ehdr->e_ehsize);

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR]
#endif

	  /* Do the real work.  */
	  (*fctp) (elf->map_address + elf->start_offset + ehdr->e_phoff,
		   elf->state.ELFW(elf,LIBELFBITS).phdr,
		   sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum, 1);
	}
      else
	memcpy (elf->map_address + elf->start_offset + ehdr->e_phoff,
		elf->state.ELFW(elf,LIBELFBITS).phdr,
		sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum);

      elf->state.ELFW(elf,LIBELFBITS).phdr_flags &= ~ELF_F_DIRTY;
    }

  /* From now on we have to keep track of the last position to eventually
     fill the gaps with the prescribed fill byte.  */
  last_position = ((char *) elf->map_address + elf->start_offset
		   + MAX (elf_typesize (LIBELFBITS, ELF_T_EHDR, 1),
			  ehdr->e_phoff)
		   + elf_typesize (LIBELFBITS, ELF_T_PHDR, ehdr->e_phnum));

  /* Write all the sections.  Well, only those which are modified.  */
  if (shnum > 0)
    {
      ElfW2(LIBELFBITS,Shdr) *shdr_dest;
      Elf_ScnList *list = &elf->state.ELFW(elf,LIBELFBITS).scns;
      Elf_Scn **scns = (Elf_Scn **) alloca (shnum * sizeof (Elf_Scn *));
      char *shdr_start = ((char *) elf->map_address + elf->start_offset
			  + ehdr->e_shoff);
      char *shdr_end = shdr_start + ehdr->e_shnum * ehdr->e_shentsize;
      size_t cnt;

#if EV_NUM != 2
      xfct_t shdr_fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR];
#else
# undef shdr_fctp
# define shdr_fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR]
#endif
      shdr_dest = (ElfW2(LIBELFBITS,Shdr) *)
	((char *) elf->map_address + elf->start_offset + ehdr->e_shoff);

      /* Get all sections into the array and sort them.  */
      sort_sections (scns, list);

      /* Iterate over all the section in the order in which they
	 appear in the output file.  */
      for (cnt = 0; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn = scns[cnt];
	  ElfW2(LIBELFBITS,Shdr) *shdr;
	  char *scn_start;
	  Elf_Data_List *dl;

	  shdr = scn->shdr.ELFW(e,LIBELFBITS);

	  scn_start = ((char *) elf->map_address
		       + elf->start_offset + shdr->sh_offset);
	  dl = &scn->data_list;

	  if (shdr->sh_type != SHT_NOBITS && scn->data_list_rear != NULL)
	    do
	      {
		if ((scn->flags | dl->flags | elf->flags) & ELF_F_DIRTY)
		  {
		    if (scn_start + dl->data.d.d_off != last_position)
		      {
			if (scn_start + dl->data.d.d_off > last_position)
			  {
			    /* This code assumes that the data blocks for
			       a section are ordered by offset.  */
			    size_t written = 0;

			    if (last_position < shdr_start)
			      {
				written = MIN (scn_start + dl->data.d.d_off
					       - last_position,
					       shdr_start - last_position);

				memset (last_position, __libelf_fill_byte,
					written);
			      }

			    if (last_position + written
				!= scn_start + dl->data.d.d_off
				&& shdr_end < scn_start + dl->data.d.d_off)
			      memset (shdr_end, __libelf_fill_byte,
				      scn_start + dl->data.d.d_off - shdr_end);

			    last_position = scn_start + dl->data.d.d_off;
			  }
		      }

		    if (unlikely (change_bo))
		      {
#if EV_NUM != 2
			fctp = __elf_xfctstom[__libelf_version - 1][dl->data.d.d_version - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
#else
# undef fctp
			fctp = __elf_xfctstom[0][0][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
#endif

			/* Do the real work.  */
			(*fctp) (last_position, dl->data.d.d_buf,
				 dl->data.d.d_size, 1);

			last_position += dl->data.d.d_size;
		      }
		    else
		      last_position = mempcpy (last_position,
					       dl->data.d.d_buf,
					       dl->data.d.d_size);
		  }
		else
		  last_position += dl->data.d.d_size;

		dl->flags &= ~ELF_F_DIRTY;

		dl = dl->next;
	      }
	    while (dl != NULL);
	  else if (shdr->sh_type != SHT_NOBITS && scn->index != 0)
	    /* We have to trust the existing section header information.  */
	    last_position += shdr->sh_size;

	  /* Write the section header table entry if necessary.  */
	  if ((scn->shdr_flags | elf->flags) & ELF_F_DIRTY)
	    {
	      if (unlikely (change_bo))
		(*shdr_fctp) (&shdr_dest[scn->index],
			      scn->shdr.ELFW(e,LIBELFBITS),
			      sizeof (ElfW2(LIBELFBITS,Shdr)), 1);
	      else
		memcpy (&shdr_dest[scn->index],
			scn->shdr.ELFW(e,LIBELFBITS),
			sizeof (ElfW2(LIBELFBITS,Shdr)));

	      scn->shdr_flags  &= ~ELF_F_DIRTY;
	    }

	  scn->flags &= ~ELF_F_DIRTY;
	}

      /* Fill the gap between last section and section header table if
	 necessary.  */
      if ((elf->flags & ELF_F_DIRTY)
	  && last_position < ((char *) elf->map_address + elf->start_offset
			       + ehdr->e_shoff))
	memset (last_position, __libelf_fill_byte,
		(char *) elf->map_address + elf->start_offset + ehdr->e_shoff
		- last_position);
    }

  /* That was the last part.  Clear the overall flag.  */
  elf->flags &= ~ELF_F_DIRTY;

  return 0;
}


/* Size of the buffer we use to generate the blocks of fill bytes.  */
#define FILLBUFSIZE	4096

/* If we have to convert the section buffer contents we have to use
   temporary buffer.  Only buffers up to MAX_TMPBUF bytes are allocated
   on the stack.  */
#define MAX_TMPBUF	32768


/* Helper function to write out fill bytes.  */
static int
fill (int fd, off_t pos, size_t len, char *fillbuf, size_t *filledp)
{
  size_t filled = *filledp;
  size_t fill_len = MIN (len, FILLBUFSIZE);

  if (unlikely (fill_len > filled) && filled < FILLBUFSIZE)
    {
      /* Initialize a few more bytes.  */
      memset (fillbuf + filled, __libelf_fill_byte, fill_len - filled);
      *filledp = filled = fill_len;
    }

  do
    {
      /* This many bytes we want to write in this round.  */
      size_t n = MIN (filled, len);

      if (unlikely ((size_t) pwrite (fd, fillbuf, n, pos) != n))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      pos += n;
      len -= n;
    }
  while (len > 0);

  return 0;
}


int
internal_function_def
__elfw2(LIBELFBITS,updatefile) (Elf *elf, int change_bo, size_t shnum)
{
  char fillbuf[FILLBUFSIZE];
  size_t filled = 0;
  ElfW2(LIBELFBITS,Ehdr) *ehdr;
  xfct_t fctp;
  off_t last_offset;

  /* We need the ELF header several times.  */
  ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

  /* Write out the ELF header.  */
  if ((elf->state.ELFW(elf,LIBELFBITS).ehdr_flags | elf->flags) & ELF_F_DIRTY)
    {
      ElfW2(LIBELFBITS,Ehdr) tmp_ehdr;
      ElfW2(LIBELFBITS,Ehdr) *out_ehdr = ehdr;

      /* If the type sizes should be different at some time we have to
	 rewrite this code.  */
      assert (sizeof (ElfW2(LIBELFBITS,Ehdr))
	      == elf_typesize (LIBELFBITS, ELF_T_EHDR, 1));

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_EHDR]
#endif

	  /* Write the converted ELF header in a temporary buffer.  */
	  (*fctp) (&tmp_ehdr, ehdr, sizeof (ElfW2(LIBELFBITS,Ehdr)), 1);

	  /* This is the buffer we want to write.  */
	  out_ehdr = &tmp_ehdr;
	}

      /* Write out the ELF header.  */
      if (unlikely (pwrite (elf->fildes, out_ehdr,
			    sizeof (ElfW2(LIBELFBITS,Ehdr)), 0)
		    != sizeof (ElfW2(LIBELFBITS,Ehdr))))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      elf->state.ELFW(elf,LIBELFBITS).ehdr_flags &= ~ELF_F_DIRTY;
    }

  /* If the type sizes should be different at some time we have to
     rewrite this code.  */
  assert (sizeof (ElfW2(LIBELFBITS,Phdr))
	  == elf_typesize (LIBELFBITS, ELF_T_PHDR, 1));

  /* Write out the program header table.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr != NULL
      && ((elf->state.ELFW(elf,LIBELFBITS).phdr_flags | elf->flags)
	  & ELF_F_DIRTY))
    {
      ElfW2(LIBELFBITS,Phdr) *tmp_phdr = NULL;
      ElfW2(LIBELFBITS,Phdr) *out_phdr = elf->state.ELFW(elf,LIBELFBITS).phdr;

      /* Maybe the user wants a gap between the ELF header and the program
	 header.  */
      if (ehdr->e_phoff > ehdr->e_ehsize
	  && unlikely (fill (elf->fildes, ehdr->e_ehsize,
			     ehdr->e_phoff - ehdr->e_ehsize, fillbuf, &filled)
		       != 0))
	return 1;

      if (unlikely (change_bo))
	{
	  /* Today there is only one version of the ELF header.  */
#if EV_NUM != 2
	  fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR];
#else
# undef fctp
# define fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_PHDR]
#endif

	  /* Allocate sufficient memory.  */
	  tmp_phdr = (ElfW2(LIBELFBITS,Phdr) *)
	    malloc (sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum);
	  if (tmp_phdr == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      return 1;
	    }

	  /* Write the converted ELF header in a temporary buffer.  */
	  (*fctp) (tmp_phdr, elf->state.ELFW(elf,LIBELFBITS).phdr,
		   sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum, 1);

	  /* This is the buffer we want to write.  */
	  out_phdr = tmp_phdr;
	}

      /* Write out the ELF header.  */
      if (unlikely ((size_t) pwrite (elf->fildes, out_phdr,
				     sizeof (ElfW2(LIBELFBITS,Phdr))
				     * ehdr->e_phnum, ehdr->e_phoff)
		    != sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}

      /* This is a no-op we we have not allocated any memory.  */
      free (tmp_phdr);

      elf->state.ELFW(elf,LIBELFBITS).phdr_flags &= ~ELF_F_DIRTY;
    }

  /* From now on we have to keep track of the last position to eventually
     fill the gaps with the prescribed fill byte.  */
  if (elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
    last_offset = elf_typesize (LIBELFBITS, ELF_T_EHDR, 1);
  else
    last_offset = (ehdr->e_phoff
		   + sizeof (ElfW2(LIBELFBITS,Phdr)) * ehdr->e_phnum);

  /* Write all the sections.  Well, only those which are modified.  */
  if (shnum > 0)
    {
      off_t shdr_offset;
      Elf_ScnList *list = &elf->state.ELFW(elf,LIBELFBITS).scns;
      ElfW2(LIBELFBITS,Shdr) *shdr_data;
      Elf_Scn **scns = (Elf_Scn **) alloca (shnum * sizeof (Elf_Scn *));
      int shdr_flags;
      size_t cnt;

      shdr_offset = elf->start_offset + ehdr->e_shoff;
#if EV_NUM != 2
      xfct_t shdr_fctp = __elf_xfctstom[__libelf_version - 1][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR];
#else
# undef shdr_fctp
# define shdr_fctp __elf_xfctstom[0][EV_CURRENT - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][ELF_T_SHDR]
#endif

      if (change_bo || elf->state.ELFW(elf,LIBELFBITS).shdr == NULL)
	shdr_data = (ElfW2(LIBELFBITS,Shdr) *)
	  alloca (shnum * sizeof (ElfW2(LIBELFBITS,Shdr)));
      else
	shdr_data = elf->state.ELFW(elf,LIBELFBITS).shdr;
      shdr_flags = elf->flags;

      /* Get all sections into the array and sort them.  */
      sort_sections (scns, list);

      for (cnt = 0; cnt < shnum; ++cnt)
	{
	  Elf_Scn *scn = scns[cnt];
	  ElfW2(LIBELFBITS,Shdr) *shdr;
	  off_t scn_start;
	  Elf_Data_List *dl;

	  shdr = scn->shdr.ELFW(e,LIBELFBITS);

	  scn_start = elf->start_offset + shdr->sh_offset;
	  dl = &scn->data_list;

	  if (shdr->sh_type != SHT_NOBITS && scn->data_list_rear != NULL
	      && scn->index != 0)
	    do
	      {
		if ((scn->flags | dl->flags | elf->flags) & ELF_F_DIRTY)
		  {
		    char tmpbuf[MAX_TMPBUF];
		    void *buf = dl->data.d.d_buf;

		    if (scn_start + dl->data.d.d_off != last_offset)
		      {
			assert (last_offset < scn_start + dl->data.d.d_off);

			if (unlikely (fill (elf->fildes, last_offset,
					    (scn_start + dl->data.d.d_off)
					    - last_offset, fillbuf,
					    &filled) != 0))
			  return 1;

			last_offset = scn_start + dl->data.d.d_off;
		      }

		    if (unlikely (change_bo))
		      {
#if EV_NUM != 2
			fctp = __elf_xfctstom[__libelf_version - 1][dl->data.d.d_version - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
#else
# undef fctp
			fctp = __elf_xfctstom[0][0][ELFW(ELFCLASS, LIBELFBITS) - 1][dl->data.d.d_type];
#endif

			buf = tmpbuf;
			if (dl->data.d.d_size > MAX_TMPBUF)
			  {
			    buf = malloc (dl->data.d.d_size);
			    if (buf == NULL)
			      {
				__libelf_seterrno (ELF_E_NOMEM);
				return 1;
			      }
			  }

			/* Do the real work.  */
			(*fctp) (buf, dl->data.d.d_buf, dl->data.d.d_size, 1);
		      }

		    if (unlikely ((size_t) pwrite (elf->fildes, buf,
						   dl->data.d.d_size,
						   last_offset)
				  != dl->data.d.d_size))
		      {
			if (buf != dl->data.d.d_buf && buf != tmpbuf)
			  free (buf);

			__libelf_seterrno (ELF_E_WRITE_ERROR);
			return 1;
		      }

		    if (buf != dl->data.d.d_buf && buf != tmpbuf)
		      free (buf);
		  }

		last_offset += dl->data.d.d_size;

		dl->flags &= ~ELF_F_DIRTY;

		dl = dl->next;
	      }
	    while (dl != NULL);
	  else if (shdr->sh_type != SHT_NOBITS && scn->index != 0)
	    last_offset = scn_start + shdr->sh_size;

	  /* Collect the section header table information.  */
	  if (unlikely (change_bo))
	    (*shdr_fctp) (&shdr_data[scn->index],
			  scn->shdr.ELFW(e,LIBELFBITS),
			  sizeof (ElfW2(LIBELFBITS,Shdr)), 1);
	  else if (elf->state.ELFW(elf,LIBELFBITS).shdr == NULL)
	    memcpy (&shdr_data[scn->index], scn->shdr.ELFW(e,LIBELFBITS),
		    sizeof (ElfW2(LIBELFBITS,Shdr)));

	  shdr_flags |= scn->shdr_flags;
	  scn->shdr_flags  &= ~ELF_F_DIRTY;
	}

      /* Fill the gap between last section and section header table if
	 necessary.  */
      if ((elf->flags & ELF_F_DIRTY) && last_offset < shdr_offset
	  && unlikely (fill (elf->fildes, last_offset,
			     shdr_offset - last_offset,
			     fillbuf, &filled) != 0))
	return 1;

      /* Write out the section header table.  */
      if (shdr_flags & ELF_F_DIRTY
	  && unlikely ((size_t) pwrite (elf->fildes, shdr_data,
					sizeof (ElfW2(LIBELFBITS,Shdr))
					* shnum, shdr_offset)
		       != sizeof (ElfW2(LIBELFBITS,Shdr)) * shnum))
	{
	  __libelf_seterrno (ELF_E_WRITE_ERROR);
	  return 1;
	}
    }

  /* That was the last part.  Clear the overall flag.  */
  elf->flags &= ~ELF_F_DIRTY;

  return 0;
}
