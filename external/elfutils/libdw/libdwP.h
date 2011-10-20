/* Internal definitions for libdwarf.
   Copyright (C) 2002, 2003, 2004 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _LIBDWP_H
#define _LIBDWP_H 1

#include <libintl.h>
#include <stdbool.h>

#include <libdw.h>


/* gettext helper macros.  */
#define _(Str) dgettext ("elfutils", Str)


/* Version of the DWARF specification we support.  */
#define DWARF_VERSION 2

/* Version of the CIE format.  */
#define CIE_VERSION 1


/* Known location lists.  */
struct loc_s
{
  void *addr;
  Dwarf_Loc *loc;
  size_t nloc;
};

/* Valid indeces for the section data.  */
enum
  {
    IDX_debug_info = 0,
    IDX_debug_abbrev,
    IDX_debug_aranges,
    IDX_debug_line,
    IDX_debug_frame,
    IDX_eh_frame,
    IDX_debug_loc,
    IDX_debug_pubnames,
    IDX_debug_str,
    IDX_debug_funcnames,
    IDX_debug_typenames,
    IDX_debug_varnames,
    IDX_debug_weaknames,
    IDX_debug_macinfo,
    IDX_last
  };


/* Error values.  */
enum
{
  DWARF_E_NOERROR = 0,
  DWARF_E_UNKNOWN_ERROR,
  DWARF_E_INVALID_ACCESS,
  DWARF_E_NO_REGFILE,
  DWARF_E_IO_ERROR,
  DWARF_E_INVALID_ELF,
  DWARF_E_NO_DWARF,
  DWARF_E_NOELF,
  DWARF_E_GETEHDR_ERROR,
  DWARF_E_NOMEM,
  DWARF_E_UNIMPL,
  DWARF_E_INVALID_CMD,
  DWARF_E_INVALID_VERSION,
  DWARF_E_INVALID_FILE,
  DWARF_E_NO_ENTRY,
  DWARF_E_INVALID_DWARF,
  DWARF_E_NO_STRING,
  DWARF_E_NO_ADDR,
  DWARF_E_NO_CONSTANT,
  DWARF_E_NO_REFERENCE,
  DWARF_E_INVALID_REFERENCE,
  DWARF_E_NO_DEBUG_LINE,
  DWARF_E_INVALID_DEBUG_LINE,
  DWARF_E_TOO_BIG,
  DWARF_E_VERSION,
  DWARF_E_INVALID_DIR_IDX,
  DWARF_E_ADDR_OUTOFRANGE,
  DWARF_E_NO_LOCLIST,
  DWARF_E_NO_BLOCK,
  DWARF_E_INVALID_LINE_IDX,
  DWARF_E_INVALID_ARANGE_IDX,
  DWARF_E_NO_MATCH,
  DWARF_E_NO_FLAG,
};


/* This is the structure representing the debugging state.  */
struct Dwarf
{
  /* The underlying ELF file.  */
  Elf *elf;

  /* The section data.  */
  Elf_Data *sectiondata[IDX_last];

  /* True if the file has a byte order different from the host.  */
  bool other_byte_order;

  /* If true, we allocated the ELF descriptor ourselves.  */
  bool free_elf;

  /* Information for traversing the .debug_pubnames section.  This is
     an array and separately allocated with malloc.  */
  struct pubnames_s
  {
    Dwarf_Off cu_offset;
    Dwarf_Off set_start;
    unsigned int cu_header_size;
    int address_len;
  } *pubnames_sets;
  size_t pubnames_nsets;

  /* Search tree for the CUs.  */
  void *cu_tree;
  Dwarf_Off next_cu_offset;

  /* Address ranges.  */
  Dwarf_Aranges *aranges;

  /* Internal memory handling.  This is basically a simplified
     reimplementation of obstacks.  Unfortunately the standard obstack
     implementation is not usable in libraries.  */
  struct libdw_memblock
  {
    size_t size;
    size_t remaining;
    struct libdw_memblock *prev;
    char mem[0];
  } *mem_tail;

  /* Default size of allocated memory blocks.  */
  size_t mem_default_size;

  /* Registered OOM handler.  */
  Dwarf_OOM oom_handler;
};


/* Abbreviation representation.  */
struct Dwarf_Abbrev
{
  unsigned int code;
  unsigned int tag;
  int has_children;
  unsigned int attrcnt;
  unsigned char *attrp;
  Dwarf_Off offset;
};

#include "dwarf_abbrev_hash.h"


/* Files in line information records.  */
struct Dwarf_Files_s
  {
    Dwarf *dbg;
    unsigned int nfiles;
    struct Dwarf_Fileinfo_s
    {
      char *name;
      Dwarf_Word mtime;
      Dwarf_Word length;
    } info[0];
  };
typedef struct Dwarf_Fileinfo_s Dwarf_Fileinfo;


/* Representation of a row in the line table.  */
struct Dwarf_Lines_s
  {
    size_t nlines;

    struct Dwarf_Line_s
    {
      Dwarf_Addr addr;
      unsigned int file;
      int line;
      unsigned short int column;
      unsigned int is_stmt:1;
      unsigned int basic_block:1;
      unsigned int end_sequence:1;
      unsigned int prologue_end:1;
      unsigned int epilogue_begin:1;

      Dwarf_Files *files;
    } info[0];
  };


/* Representation of address ranges.  */
struct Dwarf_Aranges_s
{
  Dwarf *dbg;
  size_t naranges;

  struct Dwarf_Arange_s
  {
    Dwarf_Addr addr;
    Dwarf_Word length;
    Dwarf_Off offset;
  } info[0];
};


/* CU representation.  */
struct Dwarf_CU
{
  Dwarf *dbg;
  Dwarf_Off start;
  Dwarf_Off end;
  uint8_t address_size;
  uint8_t offset_size;

  /* Hash table for the abbreviations.  */
  Dwarf_Abbrev_Hash abbrev_hash;
  /* Offset of the first abbreviation.  */
  size_t orig_abbrev_offset;
  /* Offset past last read abbreviation.  */
  size_t last_abbrev_offset;

  /* The srcline information.  */
  Dwarf_Lines *lines;

  /* The source file information.  */
  Dwarf_Files *files;

  /* Known location lists.  */
  void *locs;
};


/* We have to include the file at this point because the inline
   functions access internals of the Dwarf structure.  */
#include "memory-access.h"


/* Set error value.  */
extern void __libdw_seterrno (int value) internal_function;


/* Memory handling, the easy parts.  This macro does not do any locking.  */
#define libdw_alloc(dbg, type, tsize, cnt) \
  ({ struct libdw_memblock *_tail = (dbg)->mem_tail;			      \
     size_t _required = (tsize) * (cnt);				      \
     type *_result = (type *) (_tail->mem + (_tail->size - _tail->remaining));\
     size_t _padding = ((__alignof (type)				      \
			 - ((uintptr_t) _result & (__alignof (type) - 1)))    \
			& (__alignof (type) - 1));			      \
     if (unlikely (_tail->remaining < _required + _padding))		      \
       {								      \
	 _result = (type *) __libdw_allocate (dbg, _required);		      \
	 _tail = (dbg)->mem_tail;					      \
       }								      \
     else								      \
       {								      \
	 _required += _padding;						      \
	 _result = (type *) ((char *) _result + _padding);		      \
       }								      \
     _tail->remaining -= _required;					      \
     _result; })

#define libdw_typed_alloc(dbg, type) \
  libdw_alloc (dbg, type, sizeof (type), 1)

/* Callback to allocate more.  */
extern void *__libdw_allocate (Dwarf *dbg, size_t minsize)
     __attribute__ ((__malloc__)) __nonnull_attribute__ (1);

/* Default OOM handler.  */
extern void __libdw_oom (void) __attribute ((noreturn, visibility ("hidden")));

/* Find CU for given offset.  */
extern struct Dwarf_CU *__libdw_findcu (Dwarf *dbg, Dwarf_Off offset)
     __nonnull_attribute__ (1) internal_function;

/* Return tag of given DIE.  */
extern Dwarf_Abbrev *__libdw_findabbrev (struct Dwarf_CU *cu,
					 unsigned int code)
     __nonnull_attribute__ (1) internal_function;

/* Get abbreviation at given offset.  */
extern Dwarf_Abbrev *__libdw_getabbrev (Dwarf *dbg, struct Dwarf_CU *cu,
					Dwarf_Off offset, size_t *lengthp,
					Dwarf_Abbrev *result)
     __nonnull_attribute__ (1) internal_function;

/* Helper functions for form handling.  */
extern size_t __libdw_form_val_len (Dwarf *dbg, struct Dwarf_CU *cu,
				    unsigned int form, unsigned char *valp)
     __nonnull_attribute__ (1, 2, 4) internal_function;

/* Helper function to locate attribute.  */
extern unsigned char *__libdw_find_attr (Dwarf_Die *die,
					 unsigned int search_name,
					 unsigned int *codep,
					 unsigned int *formp)
     __nonnull_attribute__ (1) internal_function;

#endif	/* libdwP.h */
