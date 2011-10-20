/* Print information from ELF file in human-readable form.
   Copyright (C) 2000, 2001, 2002, 2003, 2004 Red Hat, Inc.
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

#include <ar.h>
#include <argp.h>
#include <assert.h>
#include <ctype.h>
#include <dwarf.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <libdw.h>
#include <libebl.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <obstack.h>
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include <system.h>


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


/* Values for the parameters which have no short form.  */
#define OPT_DEFINED	0x100
#define OPT_MARK_WEAK	0x101

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output selection:"), 0 },
  { "debug-syms", 'a', NULL, 0, N_("Display debugger-only symbols"), 0 },
  { "defined-only", OPT_DEFINED, NULL, 0, N_("Display only defined symbols"),
    0 },
  { "dynamic", 'D', NULL, 0,
    N_("Display dynamic symbols instead of normal symbols"), 0 },
  { "extern-only", 'g', NULL, 0, N_("Display only external symbols"), 0 },
  { "undefined-only", 'u', NULL, 0, N_("Display only undefined symbols"), 0 },
  { "print-armap", 's', NULL, 0,
    N_("Include index for symbols from archive members"), 0 },

  { NULL, 0, NULL, 0, N_("Output format:"), 0 },
  { "print-file-name", 'A', NULL, 0,
    N_("Print name of the input file before every symbol"), 0 },
  { NULL, 'o', NULL, OPTION_HIDDEN, "Same as -A", 0 },
  { "format", 'f', "FORMAT", 0,
    N_("Use the output format FORMAT.  FORMAT can be `bsd', `sysv' or `posix'.  The default is `sysv'"),
    0 },
  { NULL, 'B', NULL, 0, N_("Same as --format=bsd"), 0 },
  { "portability", 'P', NULL, 0, N_("Same as --format=posix"), 0 },
  { "radix", 't', "RADIX", 0, N_("Use RADIX for printing symbol values"), 0 },
  { "mark-weak", OPT_MARK_WEAK, NULL, 0, N_("Mark weak symbols"), 0 },
  { "print-size", 'S', NULL, 0, N_("Print size of defined symbols"), 0 },

  { NULL, 0, NULL, 0, N_("Output options:"), 0 },
  { "numeric-sort", 'n', NULL, 0, N_("Sort symbols numerically by address"),
    0 },
  { "no-sort", 'p', NULL, 0, N_("Do not sort the symbols"), 0 },
  { "reverse-sort", 'r', NULL, 0, N_("Reverse the sense of the sort"), 0 },
  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Short description of program.  */
static const char doc[] = N_("List symbols from FILEs (a.out by default).");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Function to print some extra text in the help message.  */
static char *more_help (int key, const char *text, void *input);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, more_help, NULL
};


/* Print symbols in file named FNAME.  */
static int process_file (const char *fname, bool more_than_one);

/* Handle content of archive.  */
static int handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
		      const char *suffix);

/* Handle ELF file.  */
static int handle_elf (Elf *elf, const char *prefix, const char *fname,
		       const char *suffix);


#define INTERNAL_ERROR(fname) \
  error (EXIT_FAILURE, 0, gettext ("%s: INTERNAL ERROR %d (%s-%s): %s"),      \
	 fname, __LINE__, VERSION, __DATE__, elf_errmsg (-1))


/* Internal representation of symbols.  */
typedef struct GElf_SymX
{
  GElf_Sym sym;
  Elf32_Word xndx;
  char *where;
} GElf_SymX;


/* User-selectable options.  */

/* The selected output format.  */
static enum
{
  format_sysv = 0,
  format_bsd,
  format_posix
} format;

/* Print defined, undefined, or both?  */
static bool hide_undefined;
static bool hide_defined;

/* Print local symbols also?  */
static bool hide_local;

/* Nonzero if full filename should precede every symbol.  */
static bool print_file_name;

/* If true print size of defined symbols in BSD format.  */
static bool print_size;

/* If true print archive index.  */
static bool print_armap;

/* If true reverse sorting.  */
static bool reverse_sort;

/* Type of the section we are printing.  */
static GElf_Word symsec_type = SHT_SYMTAB;

/* Sorting selection.  */
static enum
{
  sort_name = 0,
  sort_numeric,
  sort_nosort
} sort;

/* Radix for printed numbers.  */
static enum
{
  radix_hex = 0,
  radix_decimal,
  radix_octal
} radix;

/* If nonzero weak symbols are distinguished from global symbols by adding
   a `*' after the identifying letter for the symbol class and type.  */
static bool mark_weak;


int
main (int argc, char *argv[])
{
  int remaining;
  int result = 0;

  /* Make memory leak detection possible.  */
  mtrace ();

  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  (void) bindtextdomain (PACKAGE, LOCALEDIR);

  /* Initialize the message catalog.  */
  (void) textdomain (PACKAGE);

  /* Parse and process arguments.  */
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Tell the library which version we are expecting.  */
  (void) elf_version (EV_CURRENT);

  if (remaining == argc)
    /* The user didn't specify a name so we use a.out.  */
    result = process_file ("a.out", false);
  else
    {
      /* Process all the remaining files.  */
      const bool more_than_one = remaining + 1 < argc;

      do
	result |= process_file (argv[remaining], more_than_one);
      while (++remaining < argc);
    }

  return result;
}


/* Print the version information.  */
static void
print_version (FILE *stream, /*@unused@*/ struct argp_state *state)
{
  fprintf (stream, "nm (%s) %s\n", PACKAGE_NAME, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2004");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, /*@unused@*/ struct argp_state *state)
{
  switch (key)
    {
    case 'a':
      /* XXX */
      break;

    case 'f':
      if (strcmp (arg, "bsd") == 0)
	format = format_bsd;
      else if (strcmp (arg, "posix") == 0)
	format = format_posix;
      else
	/* Be bug compatible.  The BFD implementation also defaulted to
	   using the SysV format if nothing else matches.  */
	format = format_sysv;
      break;

    case 'g':
      hide_local = true;
      break;

    case 'n':
      sort = sort_numeric;
      break;

    case 'p':
      sort = sort_nosort;
      break;

    case 't':
      if (strcmp (arg, "10") == 0 || strcmp (arg, "d") == 0)
	radix = radix_decimal;
      else if (strcmp (arg, "8") == 0 || strcmp (arg, "o") == 0)
	radix = radix_octal;
      else
	radix = radix_hex;
      break;

    case 'u':
      hide_undefined = false;
      hide_defined = true;
      break;

    case 'A':
    case 'o':
      print_file_name = true;
      break;

    case 'B':
      format = format_bsd;
      break;

    case 'D':
      symsec_type = SHT_DYNSYM;
      break;

    case 'P':
      format = format_posix;
      break;

    case OPT_DEFINED:
      hide_undefined = true;
      hide_defined = false;
      break;

    case OPT_MARK_WEAK:
      mark_weak = true;
      break;

    case 'S':
      print_size = true;
      break;

    case 's':
      print_armap = true;
      break;

    case 'r':
      reverse_sort = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static char *
more_help (int key, const char *text, /*@unused@*/ void *input)
{
  char *buf;

  switch (key)
    {
    case ARGP_KEY_HELP_EXTRA:
      /* We print some extra information.  */
      if (asprintf (&buf, gettext ("Please report bugs to %s.\n"),
		    PACKAGE_BUGREPORT) < 0)
	buf = NULL;
      return buf;

    default:
      break;
    }
  return (char *) text;
}


/* Open the file and determine the type.  */
static int
process_file (const char *fname, bool more_than_one)
{
  /* Open the file.  */
  int fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      error (0, errno, fname);
      return 1;
    }

  /* Now get the ELF descriptor.  */
  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (elf != NULL)
    {
      if (elf_kind (elf) == ELF_K_ELF)
	{
	  int result = handle_elf (elf, more_than_one ? "" : NULL,
				   fname, NULL);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (close (fd) != 0)
	    error (EXIT_FAILURE, errno, gettext ("while close `%s'"), fname);

	  return result;
	}
      else if (elf_kind (elf) == ELF_K_AR)
	{
	  int result = handle_ar (fd, elf, NULL, fname, NULL);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (close (fd) != 0)
	    error (EXIT_FAILURE, errno, gettext ("while close `%s'"), fname);

	  return result;
	}

      /* We cannot handle this type.  Close the descriptor anyway.  */
      if (elf_end (elf) != 0)
	INTERNAL_ERROR (fname);
    }

  error (0, 0, gettext ("%s: File format not recognized"), fname);

  return 1;
}


static int
handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
	   const char *suffix)
{
  size_t fname_len = strlen (fname) + 1;
  size_t prefix_len = prefix != NULL ? strlen (prefix) : 0;
  char new_prefix[prefix_len + fname_len + 2];
  size_t suffix_len = suffix != NULL ? strlen (suffix) : 0;
  char new_suffix[suffix_len + 2];
  Elf *subelf;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  int result = 0;

  char *cp = new_prefix;
  if (prefix != NULL)
    cp = stpcpy (cp, prefix);
  cp = stpcpy (cp, fname);
  stpcpy (cp, "[");

  cp = new_suffix;
  if (suffix != NULL)
    cp = stpcpy (cp, suffix);
  stpcpy (cp, "]");

  /* First print the archive index if this is wanted.  */
  if (print_armap)
    {
      Elf_Arsym *arsym = elf_getarsym (elf, NULL);

      if (arsym != NULL)
	{
	  Elf_Arhdr *arhdr = NULL;
	  size_t arhdr_off = 0;	/* Note: 0 is no valid offset.  */

	  puts (gettext("\nArchive index:"));

	  while (arsym->as_off != 0)
	    {
	      if (arhdr_off != arsym->as_off
		  && (elf_rand (elf, arsym->as_off) != arsym->as_off
		      || (subelf = elf_begin (fd, cmd, elf)) == NULL
		      || (arhdr = elf_getarhdr (subelf)) == NULL))
		{
		  error (0, 0, gettext ("invalid offset %zu for symbol %s"),
			 arsym->as_off, arsym->as_name);
		  continue;
		}

	      printf (gettext ("%s in %s\n"), arsym->as_name, arhdr->ar_name);

	      ++arsym;
	    }

	  if (elf_rand (elf, SARMAG) != SARMAG)
	    {
	      error (0, 0,
		     gettext ("cannot reset archive offset to beginning"));
	      return 1;
	    }
	}
    }

  /* Process all the files contained in the archive.  */
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      /* The the header for this element.  */
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      /* Skip over the index entries.  */
      if (strcmp (arhdr->ar_name, "/") != 0
	  && strcmp (arhdr->ar_name, "//") != 0)
	{
	  if (elf_kind (subelf) == ELF_K_ELF)
	    result |= handle_elf (subelf, new_prefix, arhdr->ar_name,
				  new_suffix);
	  else if (elf_kind (subelf) == ELF_K_AR)
	    result |= handle_ar (fd, subelf, new_prefix, arhdr->ar_name,
				 new_suffix);
	  else
	    {
	      error (0, 0, gettext ("%s%s%s: file format not recognized"),
		     new_prefix, arhdr->ar_name, new_suffix);
	      result = 1;
	    }
	}

      /* Get next archive element.  */
      cmd = elf_next (subelf);
      if (elf_end (subelf) != 0)
	INTERNAL_ERROR (fname);
    }

  return result;
}


/* Mapping of radix and binary class to length.  */
static const int length_map[2][3] =
{
  [ELFCLASS32 - 1] =
  {
    [radix_hex] = 8,
    [radix_decimal] = 10,
    [radix_octal] = 11
  },
  [ELFCLASS64 - 1] =
  {
    [radix_hex] = 16,
    [radix_decimal] = 20,
    [radix_octal] = 22
  }
};


struct global_name
{
  Dwarf_Global global;
  const char *name;
};


static int
global_compare (const void *p1, const void *p2)
{
  const Dwarf_Global *g1 = (const Dwarf_Global *) p1;
  const Dwarf_Global *g2 = (const Dwarf_Global *) p2;

  return strcmp (g1->name, g2->name);
}


static void *global_root;


static int
get_global (Dwarf *dbg, Dwarf_Global *global, void *arg)
{
  tsearch (memcpy (xmalloc (sizeof (Dwarf_Global)), global,
		   sizeof (Dwarf_Global)),
	   &global_root, global_compare);

  return DWARF_CB_OK;
}


struct local_name
{
  const char *name;
  const char *file;
  Dwarf_Word lineno;
  Dwarf_Addr lowpc;
  Dwarf_Addr highpc;
};


static int
local_compare (const void *p1, const void *p2)
{
  struct local_name *g1 = (struct local_name *) p1;
  struct local_name *g2 = (struct local_name *) p2;
  int result;

  result = strcmp (g1->name, g2->name);
  if (result == 0)
    {
      if (g1->lowpc <= g2->lowpc && g1->highpc >= g2->highpc)
	{
	  /* g2 is contained in g1.  Update the data.  */
	  g2->lowpc = g1->lowpc;
	  g2->highpc = g1->highpc;
	  result = 0;
	}
      else if (g2->lowpc <= g1->lowpc && g2->highpc >= g1->highpc)
	{
	  /* g1 is contained in g2.  Update the data.  */
	  g1->lowpc = g2->lowpc;
	  g1->highpc = g2->highpc;
	  result = 0;
	}
      else
	result = g1->lowpc < g2->lowpc ? -1 : 1;
    }

  return result;
}


static int
get_var_range (Dwarf_Die *die, Dwarf_Word *lowpc, Dwarf_Word *highpc)
{
  Dwarf_Attribute locattr_mem;
  Dwarf_Attribute *locattr = dwarf_attr (die, DW_AT_location, &locattr_mem);
  if  (locattr == NULL)
    return 1;

  Dwarf_Loc *loc;
  size_t nloc;
  if (dwarf_getloclist (locattr, &loc, &nloc) != 0)
    return 1;

  /* Interpret the location expressions.  */
  // XXX For now just the simple one:
  if (nloc == 1 && loc[0].atom == DW_OP_addr)
    {
      *lowpc = *highpc = loc[0].number;
      return 0;
    }

  return 1;
}



static void *local_root;


static void
get_local_names (Ebl *ebl, Dwarf *dbg)
{
  Dwarf_Off offset = 0;
  Dwarf_Off old_offset;
  size_t hsize;

  while (dwarf_nextcu (dbg, old_offset = offset, &offset, &hsize, NULL, NULL,
		       NULL) == 0)
    {
      Dwarf_Die cudie_mem;
      Dwarf_Die *cudie = dwarf_offdie (dbg, old_offset + hsize, &cudie_mem);

      /* If we cannot get the CU DIE there is no need to go on with
	 this CU.  */
      if (cudie == NULL)
	continue;
      /* This better be a CU DIE.  */
      if (dwarf_tag (cudie) != DW_TAG_compile_unit)
	continue;

      /* Get the line information.  */
      Dwarf_Files *files;
      size_t nfiles;
      if (dwarf_getsrcfiles (cudie, &files, &nfiles) != 0)
	continue;

      Dwarf_Die die_mem;
      Dwarf_Die *die = &die_mem;
      if (dwarf_child (cudie, die) == 0)
	/* Iterate over all immediate children of the CU DIE.  */
	do
	  {
	    int tag = dwarf_tag (die);
	    if (tag != DW_TAG_subprogram && tag != DW_TAG_variable)
	      continue;

	    /* We are interested in five attributes: name, decl_file,
	       decl_line, low_pc, and high_pc.  */
	    Dwarf_Attribute attr_mem;
	    Dwarf_Attribute *attr = dwarf_attr (die, DW_AT_name, &attr_mem);
	    const char *name = dwarf_formstring (attr);
	    if (name == NULL)
	      continue;

	    Dwarf_Word fileidx;
	    attr = dwarf_attr (die, DW_AT_decl_file, &attr_mem);
	    if (dwarf_formudata (attr, &fileidx) != 0 || fileidx >= nfiles)
	      continue;

	    Dwarf_Word lineno;
	    attr = dwarf_attr (die, DW_AT_decl_line, &attr_mem);
	    if (dwarf_formudata (attr, &lineno) != 0 || lineno == 0)
	      continue;

	    Dwarf_Addr lowpc;
	    Dwarf_Addr highpc;
	    if (tag == DW_TAG_subprogram)
	      {
		if (dwarf_lowpc (die, &lowpc) != 0
		    || dwarf_highpc (die, &highpc) != 0)
		  continue;
	      }
	    else
	      {
		if (get_var_range (die, &lowpc, &highpc) != 0)
		  continue;
	      }

	    /* We have all the information.  Create a record.  */
	    struct local_name *newp
	      = (struct local_name *) xmalloc (sizeof (*newp));
	    newp->name = name;
	    newp->file = dwarf_filesrc (files, fileidx, NULL, NULL);
	    newp->lineno = lineno;
	    newp->lowpc = lowpc;
	    newp->highpc = highpc;

	    /* Since we cannot deallocate individual memory we do not test
	       for duplicates in the tree.  This should not happen anyway.  */
	    if (tsearch (newp, &local_root, local_compare) == NULL)
	      error (EXIT_FAILURE, errno,
		     gettext ("cannot create search tree"));
	  }
	while (dwarf_siblingof (die, die) == 0);
    }
}


/* Show symbols in SysV format.  */
static void
show_symbols_sysv (Ebl *ebl, GElf_Word strndx,
		   const char *prefix, const char *fname, const char *fullname,
		   GElf_SymX *syms, size_t nsyms, int longest_name,
		   int longest_where)
{
  size_t shnum;
  if (elf_getshnum (ebl->elf, &shnum) < 0)
    INTERNAL_ERROR (fullname);

  bool scnnames_malloced = shnum * sizeof (const char *) > 128 * 1024;
  const char **scnnames;
  if (scnnames_malloced)
    scnnames = (const char **) xmalloc (sizeof (const char *) * shnum);
  else
    scnnames = (const char **) alloca (sizeof (const char *) * shnum);
  /* Get the section header string table index.  */
  size_t shstrndx;
  if (elf_getshstrndx (ebl->elf, &shstrndx) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* Cache the section names.  */
  Elf_Scn *scn = NULL;
  size_t cnt = 1;
  while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;

      assert (elf_ndxscn (scn) == cnt++);

      scnnames[elf_ndxscn (scn)]
	= elf_strptr (ebl->elf, shstrndx,
		      gelf_getshdr (scn, &shdr_mem)->sh_name);
    }

  int digits = length_map[gelf_getclass (ebl->elf) - 1][radix];

  /* We always print this prolog.  */
  if (prefix == NULL || 1)
    printf (gettext ("\n\nSymbols from %s:\n\n"), fullname);
  else
    printf (gettext ("\n\nSymbols from %s[%s]:\n\n"), prefix, fname);

  /* The header line.  */
  printf (gettext ("%*s%-*s %-*s Class  Type     %-*s %*s Section\n\n"),
	  print_file_name ? (int) strlen (fullname) + 1: 0, "",
	  longest_name, sgettext ("sysv|Name"),
	  /* TRANS: the "sysv|" parts makes the string unique.  */
	  digits, sgettext ("sysv|Value"),
	  /* TRANS: the "sysv|" parts makes the string unique.  */
	  digits, sgettext ("sysv|Size"),
	  /* TRANS: the "sysv|" parts makes the string unique.  */
	  longest_where, sgettext ("sysv|Line"));

  /* Which format string to use (different radix for numbers).  */
  const char *fmtstr;
  if (radix == radix_hex)
    fmtstr = "%-*s|%0*" PRIx64 "|%-6s|%-8s|%*" PRIx64 "|%*s|%s\n";
  else if (radix == radix_decimal)
    fmtstr = "%-*s|%*" PRId64 "|%-6s|%-8s|%*" PRId64 "|%*s|%s\n";
  else
    fmtstr = "%-*s|%0*" PRIo64 "|%-6s|%-8s|%*" PRIo64 "|%*s|%s\n";

  /* Iterate over all symbols.  */
  for (cnt = 0; cnt < nsyms; ++cnt)
    {
      const char *symstr = elf_strptr (ebl->elf, strndx,
				       syms[cnt].sym.st_name);
      char symbindbuf[50];
      char symtypebuf[50];
      char secnamebuf[1024];

      /* If we have to precede the line with the file name.  */
      if (print_file_name)
	{
	  fputs_unlocked (fullname, stdout);
	  putchar_unlocked (':');
	}

      /* Print the actual string.  */
      printf (fmtstr,
	      longest_name, symstr,
	      digits, syms[cnt].sym.st_value,
	      ebl_symbol_binding_name (ebl,
				       GELF_ST_BIND (syms[cnt].sym.st_info),
				       symbindbuf, sizeof (symbindbuf)),
	      ebl_symbol_type_name (ebl, GELF_ST_TYPE (syms[cnt].sym.st_info),
				    symtypebuf, sizeof (symtypebuf)),
	      digits, syms[cnt].sym.st_size, longest_where, syms[cnt].where,
	      ebl_section_name (ebl, syms[cnt].sym.st_shndx, syms[cnt].xndx,
				secnamebuf, sizeof (secnamebuf), scnnames,
				shnum));
    }

  if (scnnames_malloced)
    free (scnnames);
}


static char
class_type_char (GElf_Sym *sym)
{
  int local_p = GELF_ST_BIND (sym->st_info) == STB_LOCAL;

  /* XXX Add support for architecture specific types and classes.  */
  if (sym->st_shndx == SHN_ABS)
    return local_p ? 'a' : 'A';

  if (sym->st_shndx == SHN_UNDEF)
    /* Undefined symbols must be global.  */
    return 'U';

  char result = "NDTSFB          "[GELF_ST_TYPE (sym->st_info)];

  return local_p ? tolower (result) : result;
}


static void
show_symbols_bsd (Elf *elf, GElf_Ehdr *ehdr, GElf_Word strndx,
		  const char *prefix, const char *fname, const char *fullname,
		  GElf_SymX *syms, size_t nsyms)
{
  int digits = length_map[gelf_getclass (elf) - 1][radix];

  if (prefix != NULL && ! print_file_name)
    printf ("\n%s:\n", fname);

  static const char *const fmtstrs[] =
    {
      [radix_hex] = "%0*" PRIx64 " %c%s %s\n",
      [radix_decimal] = "%*" PRId64 " %c%s %s\n",
      [radix_octal] = "%0*" PRIo64 " %c%s %s\n"
    };
  static const char *const sfmtstrs[] =
    {
      [radix_hex] = "%2$0*1$" PRIx64 " %7$0*6$" PRIx64 " %3$c%4$s %5$s\n",
      [radix_decimal] = "%2$*1$" PRId64 " %7$*6$" PRId64 " %3$c%4$s %5$s\n",
      [radix_octal] = "%2$0*1$" PRIo64 " %7$0*6$" PRIo64 " %3$c%4$s %5$s\n"
    };

  /* Iterate over all symbols.  */
  for (size_t cnt = 0; cnt < nsyms; ++cnt)
    {
      const char *symstr = elf_strptr (elf, strndx, syms[cnt].sym.st_name);

      /* Printing entries with a zero-length name makes the output
	 not very well parseable.  Since these entries don't carry
	 much information we leave them out.  */
      if (symstr[0] == '\0')
	continue;

      /* If we have to precede the line with the file name.  */
      if (print_file_name)
	{
	  fputs_unlocked (fullname, stdout);
	  putchar_unlocked (':');
	}

      if (syms[cnt].sym.st_shndx == SHN_UNDEF)
	printf ("%*s U%s %s\n",
		digits, "",
		mark_weak
		? (GELF_ST_BIND (syms[cnt].sym.st_info) == STB_WEAK
		   ? "*" : " ")
		: "",
		elf_strptr (elf, strndx, syms[cnt].sym.st_name));
      else
	printf (print_size ? sfmtstrs[radix] : fmtstrs[radix],
		digits, syms[cnt].sym.st_value,
		class_type_char (&syms[cnt].sym),
		mark_weak
		? (GELF_ST_BIND (syms[cnt].sym.st_info) == STB_WEAK
		   ? "*" : " ")
		: "",
		elf_strptr (elf, strndx, syms[cnt].sym.st_name),
		digits, (uint64_t) syms[cnt].sym.st_size);
    }
}


static void
show_symbols_posix (Elf *elf, GElf_Ehdr *ehdr, GElf_Word strndx,
		    const char *prefix, const char *fname,
		    const char *fullname, GElf_SymX *syms, size_t nsyms)
{
  if (prefix != NULL && ! print_file_name)
    printf ("%s:\n", fullname);

  const char *fmtstr;
  if (radix == radix_hex)
    fmtstr = "%s %c%s %0*" PRIx64 " %0*" PRIx64 "\n";
  else if (radix == radix_decimal)
    fmtstr = "%s %c%s %*" PRId64 " %*" PRId64 "\n";
  else
    fmtstr = "%s %c%s %0*" PRIo64 " %0*" PRIo64 "\n";

  int digits = length_map[gelf_getclass (elf) - 1][radix];

  /* Iterate over all symbols.  */
  for (size_t cnt = 0; cnt < nsyms; ++cnt)
    {
      const char *symstr = elf_strptr (elf, strndx, syms[cnt].sym.st_name);

      /* Printing entries with a zero-length name makes the output
	 not very well parseable.  Since these entries don't carry
	 much information we leave them out.  */
      if (symstr[0] == '\0')
	continue;

      /* If we have to precede the line with the file name.  */
      if (print_file_name)
	{
	  fputs_unlocked (fullname, stdout);
	  putchar_unlocked (':');
	  putchar_unlocked (' ');
	}

      printf (fmtstr,
	      symstr,
	      class_type_char (&syms[cnt].sym),
	      mark_weak
	      ? (GELF_ST_BIND (syms[cnt].sym.st_info) == STB_WEAK ? "*" : " ")
	      : "",
	      digits, syms[cnt].sym.st_value,
	      digits, syms[cnt].sym.st_size);
    }
}


/* Maximum size of memory we allocate on the stack.  */
#define MAX_STACK_ALLOC	65536

static void
show_symbols (Ebl *ebl, GElf_Ehdr *ehdr, Elf_Scn *scn, Elf_Scn *xndxscn,
	      GElf_Shdr *shdr, const char *prefix, const char *fname,
	      const char *fullname)
{
  int sort_by_name (const void *p1, const void *p2)
    {
      GElf_SymX *s1 = (GElf_SymX *) p1;
      GElf_SymX *s2 = (GElf_SymX *) p2;
      int result;

      result = strcmp (elf_strptr (ebl->elf, shdr->sh_link, s1->sym.st_name),
		       elf_strptr (ebl->elf, shdr->sh_link, s2->sym.st_name));

      return reverse_sort ? -result : result;
    }

  int sort_by_address (const void *p1, const void *p2)
    {
      GElf_SymX *s1 = (GElf_SymX *) p1;
      GElf_SymX *s2 = (GElf_SymX *) p2;

      int result = (s1->sym.st_value < s2->sym.st_value
		    ? -1 : (s1->sym.st_value == s2->sym.st_value ? 0 : 1));

      return reverse_sort ? -result : result;
    }

  /* Get the section header string table index.  */
  size_t shstrndx;
  if (elf_getshstrndx (ebl->elf, &shstrndx) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* The section is that large.  */
  size_t size = shdr->sh_size;
  /* One entry is this large.  */
  size_t entsize = shdr->sh_entsize;

  /* Consistency checks.  */
  if (entsize != gelf_fsize (ebl->elf, ELF_T_SYM, 1, ehdr->e_version))
    error (0, 0,
	   gettext ("%s: entry size in section `%s' is not what we expect"),
	   fullname, elf_strptr (ebl->elf, shstrndx, shdr->sh_name));
  else if (size % entsize != 0)
    error (0, 0,
	   gettext ("%s: size of section `%s' is not multiple of entry size"),
	   fullname, elf_strptr (ebl->elf, shstrndx, shdr->sh_name));

  /* Compute number of entries.  Handle buggy entsize values.  */
  size_t nentries = size / (entsize ?: 1);


#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
  struct obstack whereob;
  obstack_init (&whereob);

  /* Get a DWARF debugging descriptor.  It's no problem if this isn't
     possible.  We just won't print any line number information.  */
  Dwarf *dbg = NULL;
  if (format == format_sysv)
    {
      dbg = dwarf_begin_elf (ebl->elf, DWARF_C_READ, NULL);
      if (dbg != NULL)
	{
	  (void) dwarf_getpubnames (dbg, get_global, NULL, 0);

	  get_local_names (ebl, dbg);
	}
    }

  /* Allocate the memory.

     XXX We can use a dirty trick here.  Since GElf_Sym == Elf64_Sym we
     can use the data memory instead of copying again if what we read
     is a 64 bit file.  */
  GElf_SymX *sym_mem;
  if (nentries * sizeof (GElf_SymX) < MAX_STACK_ALLOC)
    sym_mem = (GElf_SymX *) alloca (nentries * sizeof (GElf_SymX));
  else
    sym_mem = (GElf_SymX *) xmalloc (nentries * sizeof (GElf_SymX));

  /* Get the data of the section.  */
  Elf_Data *data = elf_getdata (scn, NULL);
  Elf_Data *xndxdata = elf_getdata (xndxscn, NULL);
  if (data == NULL || (xndxscn != NULL && xndxdata == NULL))
    INTERNAL_ERROR (fullname);

  /* Iterate over all symbols.  */
  int longest_name = 4;
  int longest_where = 4;
  size_t nentries_used = 0;
  for (size_t cnt = 0; cnt < nentries; ++cnt)
    {
      GElf_Sym *sym = gelf_getsymshndx (data, xndxdata, cnt,
					&sym_mem[nentries_used].sym,
					&sym_mem[nentries_used].xndx);
      if (sym == NULL)
	INTERNAL_ERROR (fullname);

      /* Filter out administrative symbols without a name and those
	 deselected by ther user with command line options.  */
      if ((hide_undefined && sym->st_shndx == SHN_UNDEF)
	  || (hide_defined && sym->st_shndx != SHN_UNDEF)
	  || (hide_local && GELF_ST_BIND (sym->st_info) == STB_LOCAL))
	continue;

      sym_mem[nentries_used].where = "";
      if (format == format_sysv)
	{
	  const char *symstr = elf_strptr (ebl->elf, shdr->sh_link,
					   sym->st_name);

	  longest_name = MAX ((size_t) longest_name, strlen (symstr));

	  if (sym->st_shndx != SHN_UNDEF
	      && GELF_ST_BIND (sym->st_info) != STB_LOCAL
	      && global_root != NULL)
	    {
	      Dwarf_Global fake = { .name = symstr };
	      Dwarf_Global **found = tfind (&fake, &global_root,
					    global_compare);
	      if (found != NULL)
		{
		  Dwarf_Die die_mem;
		  Dwarf_Die *die = dwarf_offdie (dbg, (*found)->die_offset,
						 &die_mem);

		  Dwarf_Die cudie_mem;
		  Dwarf_Die *cudie = NULL;

		  Dwarf_Addr lowpc;
		  Dwarf_Addr highpc;
		  if (die != NULL
		      && dwarf_lowpc (die, &lowpc) == 0
		      && lowpc <= sym->st_value
		      && dwarf_highpc (die, &highpc) == 0
		      && highpc > sym->st_value)
		    cudie = dwarf_offdie (dbg, (*found)->cu_offset,
					  &cudie_mem);
		  if (cudie != NULL)
		    {
		      Dwarf_Line *line = dwarf_getsrc_die (cudie,
							   sym->st_value);
		      if (line != NULL)
			{
			  /* We found the line.  */
			  int lineno;
			  (void) dwarf_lineno (line, &lineno);
			  int n;
			  n = obstack_printf (&whereob, "%s:%d%c",
					      basename (dwarf_linesrc (line,
								       NULL,
								       NULL)),
					      lineno, '\0');
			  sym_mem[nentries_used].where
			    = obstack_finish (&whereob);

			  /* The return value of obstack_print included the
			     NUL byte, so subtract one.  */
			  if (--n > (int) longest_where)
			    longest_where = (size_t) n;
			}
		    }
		}
	    }

	  /* Try to find the symol among the local symbols.  */
	  if (sym_mem[nentries_used].where[0] == '\0')
	    {
	      struct local_name fake =
		{
		  .name = symstr,
		  .lowpc = sym->st_value,
		  .highpc = sym->st_value,
		};
	      struct local_name **found = tfind (&fake, &local_root,
						 local_compare);
	      if (found != NULL)
		{
		  /* We found the line.  */
		  int n = obstack_printf (&whereob, "%s:%" PRIu64 "%c",
					  basename ((*found)->file),
					  (*found)->lineno,
					  '\0');
		  sym_mem[nentries_used].where = obstack_finish (&whereob);

		  /* The return value of obstack_print included the
		     NUL byte, so subtract one.  */
		  if (--n > (int) longest_where)
		    longest_where = (size_t) n;
		}
	    }
	}

      /* We use this entry.  */
      ++nentries_used;
    }
  /* Now we know the exact number.  */
  nentries = nentries_used;

  /* Sort the entries according to the users wishes.  */
  if (sort == sort_name)
    qsort (sym_mem, nentries, sizeof (GElf_SymX), sort_by_name);
  else if (sort == sort_numeric)
    qsort (sym_mem, nentries, sizeof (GElf_SymX), sort_by_address);

  /* Finally print according to the users selection.  */
  switch (format)
    {
    case format_sysv:
      show_symbols_sysv (ebl, shdr->sh_link, prefix, fname,
			 fullname, sym_mem, nentries, longest_name,
			 longest_where);
      break;

    case format_bsd:
      show_symbols_bsd (ebl->elf, ehdr, shdr->sh_link, prefix, fname,
			fullname, sym_mem, nentries);
      break;

    case format_posix:
    default:
      assert (format == format_posix);
      show_symbols_posix (ebl->elf, ehdr, shdr->sh_link, prefix, fname,
			  fullname, sym_mem, nentries);
      break;
    }

  /* Free all memory.  */
  if (nentries * sizeof (GElf_Sym) >= MAX_STACK_ALLOC)
    free (sym_mem);

  obstack_free (&whereob, NULL);

  if (dbg != NULL)
    {
      tdestroy (global_root, free);
      global_root = NULL;

      tdestroy (local_root, free);
      local_root = NULL;

      (void) dwarf_end (dbg);
    }
}


static int
handle_elf (Elf *elf, const char *prefix, const char *fname,
	    const char *suffix)
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t suffix_len = suffix == NULL ? 0 : strlen (suffix);
  size_t fname_len = strlen (fname) + 1;
  char fullname[prefix_len + 1 + fname_len + suffix_len];
  char *cp = fullname;
  Elf_Scn *scn = NULL;
  int any = 0;
  int result = 0;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  Ebl *ebl;

  /* Get the backend for this object file type.  */
  ebl = ebl_openbackend (elf);

  /* We need the ELF header in a few places.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    INTERNAL_ERROR (fullname);

  /* If we are asked to print the dynamic symbol table and this is
     executable or dynamic executable, fail.  */
  if (symsec_type == SHT_DYNSYM
      && ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
    {
      /* XXX Add machine specific object file types.  */
      error (0, 0, gettext ("%s%s%s%s: Invalid operation"),
	     prefix ?: "", prefix ? "(" : "", fname, prefix ? ")" : "");
      result = 1;
      goto out;
    }

  /* Create the full name of the file.  */
  if (prefix != NULL)
    cp = mempcpy (cp, prefix, prefix_len);
  cp = mempcpy (cp, fname, fname_len);
  if (suffix != NULL)
    memcpy (cp - 1, suffix, suffix_len + 1);

  /* Find the symbol table.

     XXX Can there be more than one?  Do we print all?  Currently we do.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fullname);

      if (shdr->sh_type == symsec_type)
	{
	  Elf_Scn *xndxscn = NULL;

	  /* We have a symbol table.  First make sure we remember this.  */
	  any = 1;

	  /* Look for an extended section index table for this section.  */
	  if (symsec_type == SHT_SYMTAB)
	    {
	      size_t scnndx = elf_ndxscn (scn);

	      while ((xndxscn = elf_nextscn (elf, xndxscn)) != NULL)
		{
		  GElf_Shdr xndxshdr_mem;
		  GElf_Shdr *xndxshdr = gelf_getshdr (xndxscn, &xndxshdr_mem);

		  if (xndxshdr == NULL)
		    INTERNAL_ERROR (fullname);

		  if (xndxshdr->sh_type == SHT_SYMTAB_SHNDX
		      && xndxshdr->sh_link == scnndx)
		    break;
		}
	    }

	  show_symbols (ebl, ehdr, scn, xndxscn, shdr, prefix, fname,
			fullname);
	}
    }

  if (! any)
    {
      error (0, 0, gettext ("%s%s%s: no symbols"),
	     prefix ?: "", prefix ? ":" : "", fname);
      result = 1;
    }

 out:
  /* Close the ELF backend library descriptor.  */
  ebl_closebackend (ebl);

  return result;
}
