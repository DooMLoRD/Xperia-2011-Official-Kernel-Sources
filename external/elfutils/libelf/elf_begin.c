/* Create descriptor for processing file.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004 Red Hat, Inc.
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "libelfP.h"
#include "common.h"


/* Create descriptor for archive in memory.  */
static inline Elf *
file_read_ar (int fildes, void *map_address, off_t offset, size_t maxsize,
	      Elf_Cmd cmd, Elf *parent)
{
  Elf *elf;

  /* Create a descriptor.  */
  elf = allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
                      ELF_K_AR, 0);
  if (elf != NULL)
    {
      /* We don't read all the symbol tables in advance.  All this will
	 happen on demand.  */
      elf->state.ar.offset = offset + SARMAG;

      elf->state.ar.elf_ar_hdr.ar_rawname = elf->state.ar.raw_name;
    }

  return elf;
}


static size_t
get_shnum (void *map_address, unsigned char *e_ident, int fildes, off_t offset,
	   size_t maxsize)
{
  size_t result;
  union
  {
    Elf32_Ehdr *e32;
    Elf64_Ehdr *e64;
    void *p;
  } ehdr;
  bool is32 = e_ident[EI_CLASS] == ELFCLASS32;

  /* Make the ELF header available.  */
  if (likely (map_address != NULL) && e_ident[EI_DATA] == MY_ELFDATA
      && (ALLOW_UNALIGNED
	  || (((size_t) ((char *) map_address + offset))
	      & ((is32 ? __alignof__ (Elf32_Ehdr) : __alignof__ (Elf64_Ehdr))
		 - 1)) == 0))
    ehdr.p = (char *) map_address + offset;
  else
    {
      /* We have to read the data from the file.  */
      size_t len = is32 ? sizeof (Elf32_Ehdr) : sizeof (Elf64_Ehdr);

      ehdr.p = alloca (len);
      /* Fill it.  */
      if ((size_t) pread (fildes, ehdr.p, len, offset) != len)
	/* Failed reading.  */
	return (size_t) -1l;

      if (e_ident[EI_DATA] != MY_ELFDATA)
	{
	  if (is32)
	    {
	      CONVERT (ehdr.e32->e_shnum);
	      CONVERT (ehdr.e32->e_shoff);
	    }
	  else
	    {
	      CONVERT (ehdr.e64->e_shnum);
	      CONVERT (ehdr.e64->e_shoff);
	    }
	}
    }

  if (is32)
    {
      /* Get the number of sections from the ELF header.  */
      result = ehdr.e32->e_shnum;

      if (unlikely (result == 0) && ehdr.e32->e_shoff != 0)
	{
	  if (offset + ehdr.e32->e_shoff + sizeof (Elf32_Shdr) > maxsize)
	    /* Cannot read the first section header.  */
	    return (size_t) -1l;

	  if (likely (map_address != NULL) && e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((size_t) ((char *) map_address + offset))
		      & (__alignof__ (Elf32_Ehdr) - 1)) == 0))
	    /* We can directly access the memory.  */
	    result = ((Elf32_Shdr *) ((char *) map_address
				      + ehdr.e32->e_shoff
				      + offset))->sh_size;
	  else
	    {
	      Elf32_Word size;

	      if (pread (fildes, &size, sizeof (Elf32_Word),
			 offset + ehdr.e32->e_shoff
			 + offsetof (Elf32_Shdr, sh_size))
		  != sizeof (Elf32_Word))
		return (size_t) -1l;

	      if (e_ident[EI_DATA] != MY_ELFDATA)
		CONVERT (size);

	      result = size;
	    }
	}
    }
  else
    {
      /* Get the number of sections from the ELF header.  */
      result = ehdr.e64->e_shnum;

      if (unlikely (result == 0) && ehdr.e64->e_shoff != 0)
	{
	  if (offset + ehdr.e64->e_shoff + sizeof (Elf64_Shdr) > maxsize)
	    /* Cannot read the first section header.  */
	    return (size_t) -1l;

	  if (likely (map_address != NULL) && e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((size_t) ((char *) map_address + offset))
		      & (__alignof__ (Elf64_Ehdr) - 1)) == 0))
	    /* We can directly access the memory.  */
	    result = ((Elf64_Shdr *) ((char *) map_address
				      + ehdr.e64->e_shoff
				      + offset))->sh_size;
	  else
	    {
	      Elf64_Word size;

	      if (pread (fildes, &size, sizeof (Elf64_Word),
			 offset + ehdr.e64->e_shoff
			 + offsetof (Elf64_Shdr, sh_size))
		  != sizeof (Elf64_Word))
		return (size_t) -1l;

	      if (e_ident[EI_DATA] != MY_ELFDATA)
		CONVERT (size);

	      result = size;
	    }
	}
    }

  return result;
}


/* Create descriptor for ELF file in memory.  */
static Elf *
file_read_elf (int fildes, void *map_address, off_t offset, size_t maxsize,
	       Elf_Cmd cmd, Elf *parent)
{
  /* We only read the ELF header now.  */
  unsigned char *e_ident;
  size_t scncnt;
  Elf *elf;

  if (map_address != NULL)
    /* It's right at the beginning of the file.  No word access
       required, just bytes.  */
    e_ident = (unsigned char *) map_address + offset;
  else
    {
      e_ident = (unsigned char *) alloca (EI_NIDENT);

      if (pread (fildes, e_ident, EI_NIDENT, offset) != EI_NIDENT)
	{
	  __libelf_seterrno (ELF_E_READ_ERROR);
	  return NULL;
	}
    }

  /* Verify the binary is of the class we can handle.  */
  if ((e_ident[EI_CLASS] != ELFCLASS32
       && e_ident[EI_CLASS] != ELFCLASS64)
      /* We also can only handle two encodings.  */
      || (e_ident[EI_DATA] != ELFDATA2LSB
	  && e_ident[EI_DATA] != ELFDATA2MSB))
    {
      /* Cannot handle this.  */
      __libelf_seterrno (ELF_E_INVALID_FILE);
      return NULL;
    }

  /* Determine the number of sections.  */
  scncnt = get_shnum (map_address, e_ident, fildes, offset, maxsize);
  if (scncnt == (size_t) -1l)
    /* Could not determine the number of sections.  */
    return NULL;

  /* We can now allocate the memory.  */
  elf = allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
		      ELF_K_ELF, scncnt * sizeof (Elf_Scn));
  if (elf == NULL)
    /* Not enough memory.  */
    return NULL;

  /* Some more or less arbitrary value.  */
  elf->state.elf.scnincr = 10;

  if (e_ident[EI_CLASS] == ELFCLASS32)
    {
      /* This pointer might not be directly usable if the alignment is
	 not sufficient for the architecture.  */
      Elf32_Ehdr *ehdr = (Elf32_Ehdr *) ((char *) map_address + offset);
      size_t cnt;

      assert ((unsigned int) scncnt == scncnt);
      elf->state.elf32.scns.cnt = elf->state.elf32.scns.max = scncnt;

      /* This is a 32-bit binary.  */
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || ((((uintptr_t) ehdr) & (__alignof__ (Elf32_Ehdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_shoff)
		      & (__alignof__ (Elf32_Shdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_phoff)
		      & (__alignof__ (Elf32_Phdr) - 1)) == 0)))
	{
	  /* We can use the mmapped memory.  */
	  elf->state.elf32.ehdr =
	    (Elf32_Ehdr *) ((char *) map_address + offset);
	  elf->state.elf32.shdr =
	    (Elf32_Shdr *) ((char *) map_address + offset
			    + elf->state.elf32.ehdr->e_shoff);
	  if (elf->state.elf32.ehdr->e_phnum)
	    /* Assign a value only if there really is a program
	       header.  Otherwise the value remains NULL.  */
	    elf->state.elf32.phdr
	      = (Elf32_Phdr *) ((char *) map_address + offset
				+ elf->state.elf32.ehdr->e_phoff);

	  for (cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf32.scns.data[cnt].index = cnt;
	      elf->state.elf32.scns.data[cnt].elf = elf;
	      elf->state.elf32.scns.data[cnt].shdr.e32 =
		&elf->state.elf32.shdr[cnt];
	      elf->state.elf32.scns.data[cnt].rawdata_base =
		elf->state.elf32.scns.data[cnt].data_base =
		((char *) map_address + offset
		 + elf->state.elf32.shdr[cnt].sh_offset);
	      elf->state.elf32.scns.data[cnt].list = &elf->state.elf32.scns;
	    }
	}
      else
	{
	  /* Read the data.  */
	  if (pread (elf->fildes, &elf->state.elf32.ehdr_mem,
		     sizeof (Elf32_Ehdr), offset) != sizeof (Elf32_Ehdr))
	    {
	      /* We must be able to read the ELF header.  */
	      __libelf_seterrno (ELF_E_INVALID_FILE);
	      return NULL;
	    }

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (elf->state.elf32.ehdr_mem.e_type);
	      CONVERT (elf->state.elf32.ehdr_mem.e_machine);
	      CONVERT (elf->state.elf32.ehdr_mem.e_version);
	      CONVERT (elf->state.elf32.ehdr_mem.e_entry);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phoff);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shoff);
	      CONVERT (elf->state.elf32.ehdr_mem.e_flags);
	      CONVERT (elf->state.elf32.ehdr_mem.e_ehsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phentsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phnum);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shentsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shnum);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shstrndx);
	    }

	  elf->state.elf32.ehdr = &elf->state.elf32.ehdr_mem;

	  for (cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf32.scns.data[cnt].index = cnt;
	      elf->state.elf32.scns.data[cnt].elf = elf;
	      elf->state.elf32.scns.data[cnt].list = &elf->state.elf32.scns;
	    }
	}

      /* So far only one block with sections.  */
      elf->state.elf32.scns_last = &elf->state.elf32.scns;
    }
  else
    {
      /* This pointer might not be directly usable if the alignment is
	 not sufficient for the architecture.  */
      Elf64_Ehdr *ehdr = (Elf64_Ehdr *) ((char *) map_address + offset);
      size_t cnt;

      assert ((unsigned int) scncnt == scncnt);
      elf->state.elf64.scns.cnt = elf->state.elf64.scns.max = scncnt;

      /* This is a 64-bit binary.  */
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || ((((uintptr_t) ehdr) & (__alignof__ (Elf64_Ehdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_shoff)
		      & (__alignof__ (Elf64_Shdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_phoff)
		      & (__alignof__ (Elf64_Phdr) - 1)) == 0)))
	{
	  /* We can use the mmapped memory.  */
	  elf->state.elf64.ehdr =
	    (Elf64_Ehdr *) ((char *) map_address + offset);
	  elf->state.elf64.shdr =
	    (Elf64_Shdr *) ((char *) map_address + offset
			    + elf->state.elf64.ehdr->e_shoff);
	  if (elf->state.elf64.ehdr->e_phnum)
	    /* Assign a value only if there really is a program
	       header.  Otherwise the value remains NULL.  */
	    elf->state.elf64.phdr
	      = (Elf64_Phdr *) ((char *) map_address + offset
				+ elf->state.elf64.ehdr->e_phoff);

	  for (cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf64.scns.data[cnt].index = cnt;
	      elf->state.elf64.scns.data[cnt].elf = elf;
	      elf->state.elf64.scns.data[cnt].shdr.e64 =
		&elf->state.elf64.shdr[cnt];
	      elf->state.elf64.scns.data[cnt].rawdata_base =
		elf->state.elf64.scns.data[cnt].data_base =
		((char *) map_address + offset
		 + elf->state.elf64.shdr[cnt].sh_offset);
	      elf->state.elf64.scns.data[cnt].list = &elf->state.elf64.scns;
	    }
	}
      else
	{
	  /* Read the data.  */
	  if (pread (elf->fildes, &elf->state.elf64.ehdr_mem,
		     sizeof (Elf64_Ehdr), offset) != sizeof (Elf64_Ehdr))
	    {
	      /* We must be able to read the ELF header.  */
	      __libelf_seterrno (ELF_E_INVALID_FILE);
	      return NULL;
	    }

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (elf->state.elf64.ehdr_mem.e_type);
	      CONVERT (elf->state.elf64.ehdr_mem.e_machine);
	      CONVERT (elf->state.elf64.ehdr_mem.e_version);
	      CONVERT (elf->state.elf64.ehdr_mem.e_entry);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phoff);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shoff);
	      CONVERT (elf->state.elf64.ehdr_mem.e_flags);
	      CONVERT (elf->state.elf64.ehdr_mem.e_ehsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phentsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phnum);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shentsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shnum);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shstrndx);
	    }

	  elf->state.elf64.ehdr = &elf->state.elf64.ehdr_mem;

	  for (cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf64.scns.data[cnt].index = cnt;
	      elf->state.elf64.scns.data[cnt].elf = elf;
	      elf->state.elf64.scns.data[cnt].list = &elf->state.elf64.scns;
	    }
	}

      /* So far only one block with sections.  */
      elf->state.elf64.scns_last = &elf->state.elf64.scns;
    }

  /* Make the class easily available.  */
  elf->class = e_ident[EI_CLASS];

  return elf;
}


Elf *
internal_function_def
__libelf_read_mmaped_file (int fildes, void *map_address,  off_t offset,
			   size_t maxsize, Elf_Cmd cmd, Elf *parent)
{
  /* We have to find out what kind of file this is.  We handle ELF
     files and archives.  To find out what we have we must look at the
     header.  The header for an ELF file is EI_NIDENT bytes in size,
     the header for an archive file SARMAG bytes long.  */
  Elf_Kind kind;

  /* See what kind of object we have here.  */
  kind = determine_kind (map_address + offset, maxsize);

  switch (kind)
    {
    case ELF_K_ELF:
      return file_read_elf (fildes, map_address, offset, maxsize, cmd, parent);

    case ELF_K_AR:
      return file_read_ar (fildes, map_address, offset, maxsize, cmd, parent);

    default:
      break;
    }

  /* This case is easy.  Since we cannot do anything with this file
     create a dummy descriptor.  */
  return allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
		       ELF_K_NONE, 0);
}


static Elf *
read_unmmaped_file (int fildes, off_t offset, size_t maxsize, Elf_Cmd cmd,
		    Elf *parent)
{
  /* We have to find out what kind of file this is.  We handle ELF
     files and archives.  To find out what we have we must read the
     header.  The header for an ELF file is EI_NIDENT bytes in size,
     the header for an archive file SARMAG bytes long.  Read the
     maximum of these numbers.

     XXX We have to change this for the extended `ar' format some day.  */
  unsigned char header[MAX (EI_NIDENT, SARMAG)];
  ssize_t nread;
  Elf_Kind kind;

  /* Read the head of the file.  */
  nread = pread (fildes, header, MIN (MAX (EI_NIDENT, SARMAG), maxsize),
		 offset);
  if (nread == -1)
    /* We cannot even read the head of the file.  Maybe FILDES is associated
       with an unseekable device.  This is nothing we can handle.  */
    return NULL;

  /* See what kind of object we have here.  */
  kind = determine_kind (header, nread);

  switch (kind)
    {
    case ELF_K_AR:
      return file_read_ar (fildes, NULL, offset, maxsize, cmd, parent);

    case ELF_K_ELF:
      /* Make sure at least the ELF header is contained in the file.  */
      if (maxsize >= (header[EI_CLASS] == ELFCLASS32
		      ? sizeof (Elf32_Ehdr) : sizeof (Elf64_Ehdr)))
	return file_read_elf (fildes, NULL, offset, maxsize, cmd, parent);
      /* FALLTHROUGH */

    default:
      break;
    }

  /* This case is easy.  Since we cannot do anything with this file
     create a dummy descriptor.  */
  return allocate_elf (fildes, NULL, offset, maxsize, cmd, parent,
		       ELF_K_NONE, 0);
}


/* Open a file for reading.  If possible we will try to mmap() the file.  */
static struct Elf *
read_file (int fildes, off_t offset, size_t maxsize,
	   Elf_Cmd cmd, Elf *parent)
{
  void *map_address = NULL;
  int use_mmap = (cmd == ELF_C_READ_MMAP || cmd == ELF_C_RDWR_MMAP
		  || cmd == ELF_C_WRITE_MMAP
		  || cmd == ELF_C_READ_MMAP_PRIVATE);

  if (use_mmap)
    {
      if (parent == NULL)
	{
	  if (maxsize == ~((size_t) 0))
	    {
	      /* We don't know in the moment how large the file is.
		 Determine it now.  */
	      struct stat st;

	      if (fstat (fildes, &st) == 0
		  && (sizeof (size_t) >= sizeof (st.st_size)
		      || st.st_size <= ~((size_t) 0)))
		maxsize = (size_t) st.st_size;
	    }

	  /* We try to map the file ourself.  */
	  map_address = mmap (NULL, maxsize, (cmd == ELF_C_READ_MMAP
					      ? PROT_READ
					      : PROT_READ|PROT_WRITE),
			      cmd == ELF_C_READ_MMAP_PRIVATE
			      ? MAP_PRIVATE : MAP_SHARED,
			      fildes, offset);

	  if (map_address == MAP_FAILED)
	    map_address = NULL;
	}
      else
	{
	  /* The parent is already loaded.  Use it.  */
	  assert (maxsize != ~((size_t) 0));

	  map_address = parent->map_address;
	}
    }

  /* If we have the file in memory optimize the access.  */
  if (map_address != NULL)
    {
      struct Elf *result;

      result = __libelf_read_mmaped_file (fildes, map_address, offset, maxsize,
					  cmd, parent);

      /* If something went wrong during the initialization unmap the
	 memory if we mmaped here.  */
      if (result == NULL
	  && (parent == NULL
	      || parent->map_address != map_address))
	munmap (map_address, maxsize);
      else if (parent == NULL)
	/* Remember that we mmap()ed the memory.  */
	result->flags |= ELF_F_MMAPPED;

      return result;
    }

  /* Otherwise we have to do it the hard way.  We read as much as necessary
     from the file whenever we need information which is not available.  */
  return read_unmmaped_file (fildes, offset, maxsize, cmd, parent);
}


/* Find the entry with the long names for the content of this archive.  */
static const char *
read_long_names (Elf *elf)
{
  off_t offset = SARMAG;	/* This is the first entry.  */
  struct ar_hdr hdrm;
  struct ar_hdr *hdr;
  char *newp;
  size_t len;

  while (1)
    {
      if (elf->map_address != NULL)
	{
	  if (offset + sizeof (struct ar_hdr) > elf->maximum_size)
	    return NULL;

	  /* The data is mapped.  */
	  hdr = (struct ar_hdr *) (elf->map_address + offset);
	}
      else
	{
	  /* Read the header from the file.  */
	  if (pread (elf->fildes, &hdrm, sizeof (hdrm),
		     elf->start_offset + offset) != sizeof (hdrm))
	    return NULL;

	  hdr = &hdrm;
	}

      len = atol (hdr->ar_size);

      if (memcmp (hdr->ar_name, "//              ", 16) == 0)
	break;

      offset += sizeof (struct ar_hdr) + ((len + 1) & ~1l);
    }

  /* Due to the stupid format of the long name table entry (which are not
     NUL terminted) we have to provide an appropriate representation anyhow.
     Therefore we always make a copy which has the appropriate form.  */
  newp = (char *) malloc (len);
  if (newp != NULL)
    {
      char *runp;

      if (elf->map_address != NULL)
	/* Simply copy it over.  */
	elf->state.ar.long_names = (char *) memcpy (newp,
						    elf->map_address + offset
						    + sizeof (struct ar_hdr),
						    len);
      else
	{
	  if ((size_t) pread (elf->fildes, newp, len,
			      elf->start_offset + offset
			      + sizeof (struct ar_hdr))
	      != len)
	    {
	      /* We were not able to read all data.  */
	      free (newp);
	      elf->state.ar.long_names = NULL;
	      return NULL;
	    }
	  elf->state.ar.long_names = newp;
	}

      elf->state.ar.long_names_len = len;

      /* Now NUL-terminate the strings.  */
      runp = newp;
      while (1)
        {
	  runp = (char *) memchr (runp, '/', newp + len - runp);
	  if (runp == NULL)
	    /* This was the last entry.  */
	    break;

	  /* NUL-terminate the string.  */
	  *runp = '\0';

	  /* Skip the NUL bzte and the \012.  */
	  runp += 2;

	  /* A sanity check.  Somebody might have generated invalid
	     archive.  */
	  if (runp >= newp + len)
	    break;
	}
    }

  return newp;
}


/* Read the next archive header.  */
int
internal_function_def
__libelf_next_arhdr (elf)
     Elf *elf;
{
  struct ar_hdr *ar_hdr;
  Elf_Arhdr *elf_ar_hdr;

  if (elf->map_address != NULL)
    {
      /* See whether this entry is in the file.  */
      if (elf->state.ar.offset + sizeof (struct ar_hdr)
	  > elf->start_offset + elf->maximum_size)
	{
	  /* This record is not anymore in the file.  */
	  __libelf_seterrno (ELF_E_RANGE);
	  return -1;
	}
      ar_hdr = (struct ar_hdr *) (elf->map_address + elf->state.ar.offset);
    }
  else
    {
      ar_hdr = &elf->state.ar.ar_hdr;

      if (pread (elf->fildes, ar_hdr, sizeof (struct ar_hdr),
		 elf->state.ar.offset)
	  != sizeof (struct ar_hdr))
	{
	  /* Something went wrong while reading the file.  */
	  __libelf_seterrno (ELF_E_RANGE);
	  return -1;
	}
    }

  /* One little consistency check.  */
  if (memcmp (ar_hdr->ar_fmag, ARFMAG, 2) != 0)
    {
      /* This is no valid archive.  */
      __libelf_seterrno (ELF_E_ARCHIVE_FMAG);
      return -1;
    }

  /* Copy the raw name over to a NUL terminated buffer.  */
  *((char *) __mempcpy (elf->state.ar.raw_name, ar_hdr->ar_name, 16)) = '\0';

  elf_ar_hdr = &elf->state.ar.elf_ar_hdr;

  /* Now convert the `struct ar_hdr' into `Elf_Arhdr'.
     Determine whether this is a special entry.  */
  if (ar_hdr->ar_name[0] == '/')
    {
      if (ar_hdr->ar_name[1] == ' '
	  && memcmp (ar_hdr->ar_name, "/               ", 16) == 0)
	/* This is the index.  */
	elf_ar_hdr->ar_name = memcpy (elf->state.ar.ar_name, "/", 2);
      else if (ar_hdr->ar_name[1] == '/'
	       && memcmp (ar_hdr->ar_name, "//              ", 16) == 0)
	/* This is the array with the long names.  */
	elf_ar_hdr->ar_name = memcpy (elf->state.ar.ar_name, "//", 3);
      else if (isdigit (ar_hdr->ar_name[1]))
	{
	  size_t offset;

	  /* This is a long name.  First we have to read the long name
	     table, if this hasn't happened already.  */
	  if (elf->state.ar.long_names == NULL
	      && read_long_names (elf) == NULL)
	    {
	      /* No long name table although it is reference.  The archive is
		 broken.  */
	      __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
	      return -1;
	    }

	  offset = atol (ar_hdr->ar_name + 1);
	  if (offset >= elf->state.ar.long_names_len)
	    {
	      /* The index in the long name table is larger than the table.  */
	      __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
	      return -1;
	    }
	  elf_ar_hdr->ar_name = elf->state.ar.long_names + offset;
	}
      else
	{
	  /* This is none of the known special entries.  */
	  __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
	  return -1;
	}
    }
  else
    {
      char *endp;

      /* It is a normal entry.  Copy over the name.  */
      endp = (char *) memccpy (elf->state.ar.ar_name, ar_hdr->ar_name,
			       '/', 16);
      if (endp != NULL)
	endp[-1] = '\0';
      else
	elf->state.ar.raw_name[16] = '\0';

      elf_ar_hdr->ar_name = elf->state.ar.ar_name;
    }

  /* Since there are no specialized functions to convert ASCII to
     time_t, uid_t, gid_t, mode_t, and off_t we use either atol or
     atoll depending on the size of the types.  We are also prepared
     for the case where the whole field in the `struct ar_hdr' is
     filled in which case we cannot simply use atol/l but instead have
     to create a temporary copy.  */
  if (ar_hdr->ar_date[sizeof (ar_hdr->ar_date) - 1] == ' ')
    {
      if (ar_hdr->ar_date[0] == ' ')
	elf_ar_hdr->ar_date = 0;
      else
	elf_ar_hdr->ar_date = (sizeof (time_t) <= sizeof (long int)
			       ? (time_t) atol (ar_hdr->ar_date)
			       : (time_t) atoll (ar_hdr->ar_date));
    }
  else
    {
      char buf[sizeof (ar_hdr->ar_date) + 1];
      *((char *) __mempcpy (buf, ar_hdr->ar_date, sizeof (ar_hdr->ar_date)))
	= '\0';
      elf_ar_hdr->ar_date = (sizeof (time_t) <= sizeof (long int)
			     ? (time_t) atol (ar_hdr->ar_date)
			     : (time_t) atoll (ar_hdr->ar_date));
    }

  if (ar_hdr->ar_uid[sizeof (ar_hdr->ar_uid) - 1] == ' ')
    {
      if (ar_hdr->ar_uid[0] == ' ')
	elf_ar_hdr->ar_uid = 0;
      else
	elf_ar_hdr->ar_uid = (sizeof (uid_t) <= sizeof (long int)
			      ? (uid_t) atol (ar_hdr->ar_uid)
			      : (uid_t) atoll (ar_hdr->ar_uid));
    }
  else
    {
      char buf[sizeof (ar_hdr->ar_uid) + 1];
      *((char *) __mempcpy (buf, ar_hdr->ar_uid, sizeof (ar_hdr->ar_uid)))
	= '\0';
      elf_ar_hdr->ar_uid = (sizeof (uid_t) <= sizeof (long int)
			     ? (uid_t) atol (ar_hdr->ar_uid)
			     : (uid_t) atoll (ar_hdr->ar_uid));
    }

  if (ar_hdr->ar_gid[sizeof (ar_hdr->ar_gid) - 1] == ' ')
    {
      if (ar_hdr->ar_gid[0] == ' ')
	elf_ar_hdr->ar_gid = 0;
      else
	elf_ar_hdr->ar_gid = (sizeof (gid_t) <= sizeof (long int)
			      ? (gid_t) atol (ar_hdr->ar_gid)
			      : (gid_t) atoll (ar_hdr->ar_gid));
    }
  else
    {
      char buf[sizeof (ar_hdr->ar_gid) + 1];
      *((char *) __mempcpy (buf, ar_hdr->ar_gid, sizeof (ar_hdr->ar_gid)))
	= '\0';
      elf_ar_hdr->ar_gid = (sizeof (gid_t) <= sizeof (long int)
			     ? (gid_t) atol (ar_hdr->ar_gid)
			     : (gid_t) atoll (ar_hdr->ar_gid));
    }

  if (ar_hdr->ar_mode[sizeof (ar_hdr->ar_mode) - 1] == ' ')
    {
      if (ar_hdr->ar_mode[0] == ' ')
	elf_ar_hdr->ar_mode = 0;
      else
	elf_ar_hdr->ar_mode = (sizeof (mode_t) <= sizeof (long int)
			       ? (mode_t) strtol (ar_hdr->ar_mode, NULL, 8)
			       : (mode_t) strtoll (ar_hdr->ar_mode, NULL, 8));
    }
  else
    {
      char buf[sizeof (ar_hdr->ar_mode) + 1];
      *((char *) __mempcpy (buf, ar_hdr->ar_mode, sizeof (ar_hdr->ar_mode)))
	= '\0';
      elf_ar_hdr->ar_mode = (sizeof (mode_t) <= sizeof (long int)
			     ? (mode_t) strtol (ar_hdr->ar_mode, NULL, 8)
			     : (mode_t) strtoll (ar_hdr->ar_mode, NULL, 8));
    }

  if (ar_hdr->ar_size[sizeof (ar_hdr->ar_size) - 1] == ' ')
    {
      if (ar_hdr->ar_size[0] == ' ')
	/* Something is really wrong.  We cannot live without a size for
	   the member since it will not be possible to find the next
	   archive member.  */
	{
	  __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
	  return -1;
	}
      else
	elf_ar_hdr->ar_size = (sizeof (time_t) == sizeof (long int)
			       ? (off_t) atol (ar_hdr->ar_size)
			       : (off_t) atoll (ar_hdr->ar_size));
    }
  else
    {
      char buf[sizeof (ar_hdr->ar_size) + 1];
      *((char *) __mempcpy (buf, ar_hdr->ar_size, sizeof (ar_hdr->ar_size)))
	= '\0';
      elf_ar_hdr->ar_size = (sizeof (time_t) == sizeof (long int)
			     ? (off_t) atol (ar_hdr->ar_size)
			     : (off_t) atoll (ar_hdr->ar_size));
    }

  return 0;
}


/* We were asked to return a clone of an existing descriptor.  This
   function must be called with the lock on the parent descriptor
   being held. */
static Elf *
dup_elf (int fildes, Elf_Cmd cmd, Elf *ref)
{
  struct Elf *result;

  if (fildes == -1)
    /* Allow the user to pass -1 as the file descriptor for the new file.  */
    fildes = ref->fildes;
  /* The file descriptor better should be the same.  If it was disconnected
     already (using `elf_cntl') we do not test it.  */
  else if (ref->fildes != -1 && fildes != ref->fildes)
    {
      __libelf_seterrno (ELF_E_FD_MISMATCH);
      return NULL;
    }

  /* The mode must allow reading.  I.e., a descriptor creating with a
     command different then ELF_C_READ, ELF_C_WRITE and ELF_C_RDWR is
     not allowed.  */
  if (ref->cmd != ELF_C_READ && ref->cmd != ELF_C_READ_MMAP
      && ref->cmd != ELF_C_WRITE && ref->cmd != ELF_C_WRITE_MMAP
      && ref->cmd != ELF_C_RDWR && ref->cmd != ELF_C_RDWR_MMAP
      && ref->cmd != ELF_C_READ_MMAP_PRIVATE)
    {
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  /* Now it is time to distinguish between reading normal files and
     archives.  Normal files can easily be handled be incrementing the
     reference counter and return the same descriptor.  */
  if (ref->kind != ELF_K_AR)
    {
      ++ref->ref_count;
      return ref;
    }

  /* This is an archive.  We must create a descriptor for the archive
     member the internal pointer of the archive file desriptor is
     pointing to.  First read the header of the next member if this
     has not happened already.  */
  if (ref->state.ar.elf_ar_hdr.ar_name == NULL
      && __libelf_next_arhdr (ref) != 0)
    /* Something went wrong.  Maybe there is no member left.  */
    return NULL;

  /* We have all the information we need about the next archive member.
     Now create a descriptor for it.  */
  result = read_file (fildes, ref->state.ar.offset + sizeof (struct ar_hdr),
		      ref->state.ar.elf_ar_hdr.ar_size, cmd, ref);

  /* Enlist this new descriptor in the list of children.  */
  if (result != NULL)
    {
      result->next = ref->state.ar.children;
      ref->state.ar.children = result;
    }

  return result;
}


/* Return desriptor for empty file ready for writing.  */
static struct Elf *
write_file (int fd, Elf_Cmd cmd)
{
  /* We simply create an empty `Elf' structure.  */
#define NSCNSALLOC	10
  Elf *result = allocate_elf (fd, NULL, 0, 0, cmd, NULL, ELF_K_ELF,
			      NSCNSALLOC * sizeof (Elf_Scn));

  if (result != NULL)
    {
      /* We have to write to the file in any case.  */
      result->flags = ELF_F_DIRTY;

      /* Some more or less arbitrary value.  */
      result->state.elf.scnincr = NSCNSALLOC;

      /* We have allocated room for some sections.  */
      assert (offsetof (struct Elf, state.elf32.scns)
	      == offsetof (struct Elf, state.elf64.scns));
      result->state.elf.scns_last = &result->state.elf32.scns;
      result->state.elf32.scns.max = NSCNSALLOC;
    }

  return result;
}


/* Return a descriptor for the file belonging to FILDES.  */
Elf *
elf_begin (fildes, cmd, ref)
     int fildes;
     Elf_Cmd cmd;
     Elf *ref;
{
  Elf *retval;

  if (! __libelf_version_initialized)
    {
      /* Version wasn't set so far.  */
      __libelf_seterrno (ELF_E_NO_VERSION);
      return NULL;
    }

  if (ref != NULL)
    /* Make sure the descriptor is not suddenly going away.  */
    rwlock_rdlock (ref->lock);
  else if (fcntl (fildes, F_GETFL) == -1 && errno == EBADF)
    {
      /* We cannot do anything productive without a file descriptor.  */
      __libelf_seterrno (ELF_E_INVALID_FILE);
      return NULL;
    }

  switch (cmd)
    {
    case ELF_C_NULL:
      /* We simply return a NULL pointer.  */
      retval = NULL;
      break;

    case ELF_C_READ_MMAP_PRIVATE:
      /* If we have a reference it must also be opened this way.  */
      if (ref != NULL && ref->cmd != ELF_C_READ_MMAP_PRIVATE)
	{
	  __libelf_seterrno (ELF_E_INVALID_CMD);
	  retval = NULL;
	  break;
	}
      /* FALLTHROUGH */

    case ELF_C_READ:
    case ELF_C_READ_MMAP:
      if (ref != NULL)
	/* Duplicate the descriptor.  */
	retval = dup_elf (fildes, cmd, ref);
      else
	/* Create descriptor for existing file.  */
	retval = read_file (fildes, 0, ~((size_t) 0), cmd, NULL);
      break;

    case ELF_C_RDWR:
    case ELF_C_RDWR_MMAP:
      /* If we have a REF object it must also be opened using this
	 command.  */
      if (ref != NULL)
	{
	  if (ref->cmd != ELF_C_RDWR && ref->cmd != ELF_C_RDWR_MMAP
	      && ref->cmd != ELF_C_WRITE && ref->cmd != ELF_C_WRITE_MMAP)
	    {
	      /* This is not ok.  REF must also be opened for writing.  */
	      __libelf_seterrno (ELF_E_INVALID_CMD);
	      retval = NULL;
	    }
	  else
	    /* Duplicate this descriptor.  */
	    retval = dup_elf (fildes, cmd, ref);
	}
      else
	/* Create descriptor for existing file.  */
	retval = read_file (fildes, 0, ~((size_t) 0), cmd, NULL);
      break;

    case ELF_C_WRITE:
    case ELF_C_WRITE_MMAP:
      /* We ignore REF and prepare a descriptor to write a new file.  */
      retval = write_file (fildes, cmd);
      break;

    default:
      __libelf_seterrno (ELF_E_INVALID_CMD);
      retval = NULL;
      break;
    }

  /* Release the lock.  */
  if (ref != NULL)
    rwlock_unlock (ref->lock);

  return retval;
}
INTDEF(elf_begin)
