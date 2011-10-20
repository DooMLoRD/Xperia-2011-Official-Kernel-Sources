/* Discard section not used at runtime from object files.
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

#include <argp.h>
#include <assert.h>
#include <byteswap.h>
#include <endian.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/time.h>

#include <elf-knowledge.h>
#include <libebl.h>
#include <system.h>


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


/* Values for the parameters which have no short form.  */
#define OPT_REMOVE_COMMENT	0x100
#define OPT_PERMISSIVE		0x101


/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output selection:") },
  { NULL, 'o', "FILE", 0, N_("Place stripped output into FILE") },
  { NULL, 'f', "FILE", 0, N_("Extract the removed sections into FILE") },

  { NULL, 0, NULL, 0, N_("Output options:") },
  { "strip-debug", 'g', NULL, 0, N_("Remove all debugging symbols") },
  { "preserve-dates", 'p', NULL, 0,
    N_("Copy modified/access timestamps to the output") },
  { "remove-comment", OPT_REMOVE_COMMENT, NULL, 0,
    N_("Remove .comment section") },
  { "permissive", OPT_PERMISSIVE, NULL, 0,
    N_("Relax a few rules to handle slightly broken ELF files") },
  { NULL, 0, NULL, 0, NULL }
};

/* Short description of program.  */
static const char doc[] = N_("Discard symbols from object files.");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Function to print some extra text in the help message.  */
static char *more_help (int key, const char *text, void *input);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, more_help
};


/* Print symbols in file named FNAME.  */
static int process_file (const char *fname);

/* Handle one ELF file.  */
static int handle_elf (int fd, Elf *elf, const char *prefix,
		       const char *fname, mode_t mode, struct timeval tvp[2]);

/* Handle all files contained in the archive.  */
static int handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
		      struct timeval tvp[2]);

#define INTERNAL_ERROR(fname) \
  error (EXIT_FAILURE, 0, gettext ("%s: INTERNAL ERROR %d (%s-%s): %s"),      \
	 fname, __LINE__, VERSION, __DATE__, elf_errmsg (-1))


/* Name of the output file.  */
static const char *output_fname;

/* Name of the debug output file.  */
static const char *debug_fname;

/* If true output files shall have same date as the input file.  */
static bool preserve_dates;

/* If true .comment sections will be removed.  */
static bool remove_comment;

/* If true remove all debug sections.  */
static bool remove_debug;

/* If true relax some ELF rules for input files.  */
static bool permissive;


int
main (int argc, char *argv[])
{
  int remaining;
  int result = 0;

  /* Make memory leak detection possible.  */
  mtrace ();

  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  bindtextdomain (PACKAGE, LOCALEDIR);

  /* Initialize the message catalog.  */
  textdomain (PACKAGE);

  /* Parse and process arguments.  */
  argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  /* Tell the library which version we are expecting.  */
  elf_version (EV_CURRENT);

  if (remaining == argc)
    /* The user didn't specify a name so we use a.out.  */
    result = process_file ("a.out");
  else
    {
      /* If we have seen the `-o' or '-f' option there must be exactly one
	 input file.  */
      if ((output_fname != NULL || debug_fname != NULL)
	  && remaining + 1 < argc)
	error (EXIT_FAILURE, 0, gettext ("\
Only one input file allowed together with '-o' and '-f'"));

      /* Process all the remaining files.  */
      do
	result |= process_file (argv[remaining]);
      while (++remaining < argc);
    }

  return result;
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state)
{
  fprintf (stream, "strip (%s) %s\n", PACKAGE_NAME, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2004");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'f':
      debug_fname = arg;
      break;

    case 'o':
      output_fname = arg;
      break;

    case 'p':
      preserve_dates = true;
      break;

    case OPT_REMOVE_COMMENT:
      remove_comment = true;
      break;

    case 'g':
      remove_debug = true;
      break;

    case OPT_PERMISSIVE:
      permissive = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static char *
more_help (int key, const char *text, void *input)
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


static int
process_file (const char *fname)
{
  /* If we have to preserve the modify and access timestamps get them
     now.  We cannot use fstat() after opening the file since the open
     would change the access time.  */
  struct stat64 pre_st;
  struct timeval tv[2];
 again:
  if (preserve_dates)
    {
      if (stat64 (fname, &pre_st) != 0)
	{
	  error (0, errno, gettext ("cannot stat input file \"%s\""), fname);
	  return 1;
	}

      /* If we have to preserve the timestamp, we need it in the
	 format utimes() understands.  */
      TIMESPEC_TO_TIMEVAL (&tv[0], &pre_st.st_atim);
      TIMESPEC_TO_TIMEVAL (&tv[1], &pre_st.st_mtim);
    }

  /* Open the file.  */
  int fd = open (fname, O_RDWR);
  if (fd == -1)
    {
      error (0, errno, gettext ("while opening \"%s\""), fname);
      return 1;
    }

  /* We always use fstat() even if we called stat() before.  This is
     done to make sure the information returned by stat() is for the
     same file.  */
  struct stat64 st;
  if (fstat64 (fd, &st) != 0)
    {
      error (0, errno, gettext ("cannot stat input file \"%s\""), fname);
      return 1;
    }
  /* Paranoid mode on.  */
  if (preserve_dates
      && (st.st_ino != pre_st.st_ino || st.st_dev != pre_st.st_dev))
    {
      /* We detected a race.  Try again.  */
      close (fd);
      goto again;
    }

  /* Now get the ELF descriptor.  */
  Elf *elf = elf_begin (fd, ELF_C_RDWR, NULL);
  int result;
  switch (elf_kind (elf))
    {
    case ELF_K_ELF:
      result = handle_elf (fd, elf, NULL, fname, st.st_mode & ACCESSPERMS,
			   preserve_dates ? tv : NULL);
      break;

    case ELF_K_AR:
      /* It is not possible to strip the content of an archive direct
	 the output to a specific file.  */
      if (unlikely (output_fname != NULL))
	{
	  error (0, 0, gettext ("%s: cannot use -o when stripping archive"),
		 fname);
	  result = 1;
	}
      else
	result = handle_ar (fd, elf, NULL, fname, preserve_dates ? tv : NULL);
      break;

    default:
      error (0, 0, gettext ("%s: File format not recognized"), fname);
      result = 1;
      break;
    }

  if (unlikely (elf_end (elf) != 0))
    INTERNAL_ERROR (fname);

  close (fd);

  return result;
}


/* Maximum size of array allocated on stack.  */
#define MAX_STACK_ALLOC	(400 * 1024)


static uint32_t
crc32_file (int fd, uint32_t *resp)
{
  unsigned char buffer[1024 * 8];
  uint32_t crc = 0;
  ssize_t count;

  /* We have to rewind.  */
  if (lseek (fd, 0, SEEK_SET) < 0)
    return 1;

  while ((count = TEMP_FAILURE_RETRY (read (fd, buffer, sizeof (buffer)))) > 0)
    crc = crc32 (crc, buffer, count);

  *resp = crc;

  return count != 0;
}


static int
handle_elf (int fd, Elf *elf, const char *prefix, const char *fname,
	    mode_t mode, struct timeval tvp[2])
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char *fullname = alloca (prefix_len + 1 + fname_len);
  char *cp = fullname;
  Elf *newelf;
  Elf *debugelf = NULL;
  char *tmp_debug_fname = NULL;
  int result = 0;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  size_t shstrndx;
  size_t shnum;
  struct shdr_info
  {
    Elf_Scn *scn;
    GElf_Shdr shdr;
    Elf_Data *data;
    const char *name;
    Elf32_Word idx;		/* Index in new file.  */
    Elf32_Word old_sh_link;	/* Original value of shdr.sh_link.  */
    Elf32_Word symtab_idx;
    Elf32_Word version_idx;
    Elf32_Word group_idx;
    Elf32_Word group_cnt;
    Elf_Scn *newscn;
    struct Ebl_Strent *se;
    Elf32_Word *newsymidx;
  } *shdr_info = NULL;
  Elf_Scn *scn;
  size_t cnt;
  size_t idx;
  bool changes;
  GElf_Ehdr newehdr_mem;
  GElf_Ehdr *newehdr;
  GElf_Ehdr debugehdr_mem;
  GElf_Ehdr *debugehdr;
  struct Ebl_Strtab *shst = NULL;
  uint32_t debug_crc;
  bool any_symtab_changes = false;
  Elf_Data *shstrtab_data = NULL;

  /* Create the full name of the file.  */
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);

  /* If we are not replacing the input file open a new file here.  */
  if (output_fname != NULL)
    {
      fd = open (output_fname, O_RDWR | O_CREAT, mode);
      if (unlikely (fd == -1))
	{
	  error (0, errno, gettext ("cannot open `%s'"), output_fname);
	  return 1;
	}
    }

  int debug_fd = -1;

  /* Get the EBL handling.  The -g option is currently the only reason
     we need EBL so dont open the backend unless necessary.  */
  Ebl *ebl = NULL;
  if (remove_debug)
    {
      ebl = ebl_openbackend (elf);
      if (ebl == NULL)
	{
	  error (0, errno, gettext ("cannot open EBL backend"));
	  result = 1;
	  goto fail;
	}
    }

  /* Open the additional file the debug information will be stored in.  */
  if (debug_fname != NULL)
    {
      /* Create a temporary file name.  We do not want to overwrite
	 the debug file if the file would not contain any
	 information.  */
      size_t debug_fname_len = strlen (debug_fname);
      tmp_debug_fname = (char *) alloca (debug_fname_len + sizeof (".XXXXXX"));
      strcpy (mempcpy (tmp_debug_fname, debug_fname, debug_fname_len),
	      ".XXXXXX");

      debug_fd = mkstemp (tmp_debug_fname);
      if (unlikely (debug_fd == -1))
	{
	  error (0, errno, gettext ("cannot open `%s'"), debug_fname);
	  result = 1;
	  goto fail;
	}
    }

  /* Get the information from the old file.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    INTERNAL_ERROR (fname);

  /* Get the section header string table index.  */
  if (unlikely (elf_getshstrndx (elf, &shstrndx) < 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* We now create a new ELF descriptor for the same file.  We
     construct it almost exactly in the same way with some information
     dropped.  */
  if (output_fname != NULL)
    newelf = elf_begin (fd, ELF_C_WRITE_MMAP, NULL);
  else
    newelf = elf_clone (elf, ELF_C_EMPTY);

  if (unlikely (gelf_newehdr (newelf, gelf_getclass (elf)) == 0)
      || (ehdr->e_type != ET_REL
	  && unlikely (gelf_newphdr (newelf, ehdr->e_phnum) == 0)))
    {
      error (0, 0, gettext ("cannot create new file `%s': %s"),
	     output_fname, elf_errmsg (-1));
      goto fail;
    }

  /* Copy over the old program header if needed.  */
  if (ehdr->e_type != ET_REL)
    for (cnt = 0; cnt < ehdr->e_phnum; ++cnt)
      {
	GElf_Phdr phdr_mem;
	GElf_Phdr *phdr;

	phdr = gelf_getphdr (elf, cnt, &phdr_mem);
	if (phdr == NULL
	    || unlikely (gelf_update_phdr (newelf, cnt, phdr) == 0))
	  INTERNAL_ERROR (fname);
      }

  if (debug_fname != NULL)
    {
      /* Also create an ELF descriptor for the debug file */
      debugelf = elf_begin (debug_fd, ELF_C_WRITE_MMAP, NULL);
      if (unlikely (gelf_newehdr (debugelf, gelf_getclass (elf)) == 0)
	  || (ehdr->e_type != ET_REL
	      && unlikely (gelf_newphdr (debugelf, ehdr->e_phnum) == 0)))
	{
	  error (0, 0, gettext ("cannot create new file `%s': %s"),
		 debug_fname, elf_errmsg (-1));
	  goto fail_close;
	}

      /* Copy over the old program header if needed.  */
      if (ehdr->e_type != ET_REL)
	for (cnt = 0; cnt < ehdr->e_phnum; ++cnt)
	  {
	    GElf_Phdr phdr_mem;
	    GElf_Phdr *phdr;

	    phdr = gelf_getphdr (elf, cnt, &phdr_mem);
	    if (phdr == NULL
		|| unlikely (gelf_update_phdr (debugelf, cnt, phdr) == 0))
	      INTERNAL_ERROR (fname);
	  }
    }

  /* Number of sections.  */
  if (unlikely (elf_getshnum (elf, &shnum) < 0))
    {
      error (0, 0, gettext ("cannot determine number of sections: %s"),
	     elf_errmsg (-1));
      goto fail_close;
    }

  /* Storage for section information.  We leave room for two more
     entries since we unconditionally create a section header string
     table.  Maybe some weird tool created an ELF file without one.
     The other one is used for the debug link section.  */
  if ((shnum + 2) * sizeof (struct shdr_info) > MAX_STACK_ALLOC)
    shdr_info = (struct shdr_info *) xcalloc (shnum + 2,
					      sizeof (struct shdr_info));
  else
    {
      shdr_info = (struct shdr_info *) alloca ((shnum + 2)
					       * sizeof (struct shdr_info));
      memset (shdr_info, '\0', (shnum + 2) * sizeof (struct shdr_info));
    }

  /* Prepare section information data structure.  */
  scn = NULL;
  cnt = 1;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      /* This should always be true (i.e., there should not be any
	 holes in the numbering).  */
      assert (elf_ndxscn (scn) == cnt);

      shdr_info[cnt].scn = scn;

      /* Get the header.  */
      if (gelf_getshdr (scn, &shdr_info[cnt].shdr) == NULL)
	INTERNAL_ERROR (fname);

      /* Get the name of the section.  */
      shdr_info[cnt].name = elf_strptr (elf, shstrndx,
					shdr_info[cnt].shdr.sh_name);
      if (shdr_info[cnt].name == NULL)
	{
	  error (0, 0, gettext ("illformed file `%s'"), fname);
	  goto fail_close;
	}

      /* Mark them as present but not yet investigated.  */
      shdr_info[cnt].idx = 1;

      /* Remember the shdr.sh_link value.  */
      shdr_info[cnt].old_sh_link = shdr_info[cnt].shdr.sh_link;

      /* Sections in files other than relocatable object files which
	 are not loaded can be freely moved by us.  In relocatable
	 object files everything can be moved.  */
      if (ehdr->e_type == ET_REL
	  || (shdr_info[cnt].shdr.sh_flags & SHF_ALLOC) == 0)
	shdr_info[cnt].shdr.sh_offset = 0;

      /* If this is an extended section index table store an
	 appropriate reference.  */
      if (unlikely (shdr_info[cnt].shdr.sh_type == SHT_SYMTAB_SHNDX))
	{
	  assert (shdr_info[shdr_info[cnt].shdr.sh_link].symtab_idx == 0);
	  shdr_info[shdr_info[cnt].shdr.sh_link].symtab_idx = cnt;
	}
      else if (unlikely (shdr_info[cnt].shdr.sh_type == SHT_GROUP))
	{
	  Elf32_Word *grpref;
	  size_t inner;

	  /* Cross-reference the sections contained in the section
	     group.  */
	  shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
	  if (shdr_info[cnt].data == NULL)
	    INTERNAL_ERROR (fname);

	  /* XXX Fix for unaligned access.  */
	  grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
	  for (inner = 1;
	       inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
	       ++inner)
	    shdr_info[grpref[inner]].group_idx = cnt;

	  if (inner == 1 || (inner == 2 && (grpref[0] & GRP_COMDAT) == 0))
	    /* If the section group contains only one element and this
	       is n COMDAT section we can drop it right away.  */
	    shdr_info[cnt].idx = 0;
	  else
	    shdr_info[cnt].group_cnt = inner - 1;
	}
      else if (unlikely (shdr_info[cnt].shdr.sh_type == SHT_GNU_versym))
	{
	  assert (shdr_info[shdr_info[cnt].shdr.sh_link].version_idx == 0);
	  shdr_info[shdr_info[cnt].shdr.sh_link].version_idx = cnt;
	}

      /* If this section is part of a group make sure it is not
	 discarded right away.  */
      if ((shdr_info[cnt].shdr.sh_flags & SHF_GROUP) != 0)
	{
	  assert (shdr_info[cnt].group_idx != 0);

	  if (shdr_info[shdr_info[cnt].group_idx].idx == 0)
	    {
	      /* The section group section will be removed.  */
	      shdr_info[cnt].group_idx = 0;
	      shdr_info[cnt].shdr.sh_flags &= ~SHF_GROUP;
	    }
	}

      /* Increment the counter.  */
      ++cnt;
    }

  /* Now determine which sections can go away.  The general rule is that
     all sections which are not used at runtime are stripped out.  But
     there are a few exceptions:

     - special sections named ".comment" and ".note" are kept
     - OS or architecture specific sections are kept since we might not
       know how to handle them
     - if a section is referred to from a section which is not removed
       in the sh_link or sh_info element it cannot be removed either
  */
  for (cnt = 1; cnt < shnum; ++cnt)
    /* Check whether the section can be removed.  */
    if (SECTION_STRIP_P (ebl, elf, ehdr, &shdr_info[cnt].shdr,
			 shdr_info[cnt].name, remove_comment, remove_debug))
      {
	/* For now assume this section will be removed.  */
	shdr_info[cnt].idx = 0;

	idx = shdr_info[cnt].group_idx;
	while (idx != 0)
	  {
	    /* If the references section group is a normal section
	       group and has one element remaining, or if it is an
	       empty COMDAT section group it is removed.  */
	    bool is_comdat;

	    /* The section group data is already loaded.  */
	    assert (shdr_info[idx].data != NULL);

	    is_comdat = (((Elf32_Word *) shdr_info[idx].data->d_buf)[0]
			 & GRP_COMDAT) != 0;

	    --shdr_info[idx].group_cnt;
	    if ((!is_comdat && shdr_info[idx].group_cnt == 1)
		|| (is_comdat && shdr_info[idx].group_cnt == 0))
	      {
		shdr_info[idx].idx = 0;
		/* Continue recursively.  */
		idx = shdr_info[idx].group_idx;
	      }
	    else
	      break;
	  }
      }

  /* Mark the SHT_NULL section as handled.  */
  shdr_info[0].idx = 2;


  /* Handle exceptions: section groups and cross-references.  We might
     have to repeat this a few times since the resetting of the flag
     might propagate.  */
  do
    {
      changes = false;

      for (cnt = 1; cnt < shnum; ++cnt)
	{
	  if (shdr_info[cnt].idx == 0)
	    {
	      /* If a relocation section is marked as being removed make
		 sure the section it is relocating is removed, too.  */
	      if ((shdr_info[cnt].shdr.sh_type == SHT_REL
		   || shdr_info[cnt].shdr.sh_type == SHT_RELA)
		  && shdr_info[shdr_info[cnt].shdr.sh_info].idx != 0)
		shdr_info[cnt].idx = 1;
	    }

	  if (shdr_info[cnt].idx == 1)
	    {
	      /* The content of symbol tables we don't remove must not
		 reference any section which we do remove.  Otherwise
		 we cannot remove the section.  */
	      if (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM
		  || shdr_info[cnt].shdr.sh_type == SHT_SYMTAB)
		{
		  Elf_Data *symdata;
		  Elf_Data *xndxdata;
		  size_t elsize;

		  /* Make sure the data is loaded.  */
		  if (shdr_info[cnt].data == NULL)
		    {
		      shdr_info[cnt].data
			= elf_getdata (shdr_info[cnt].scn, NULL);
		      if (shdr_info[cnt].data == NULL)
			INTERNAL_ERROR (fname);
		    }
		  symdata = shdr_info[cnt].data;

		  /* If there is an extended section index table load it
		     as well.  */
		  if (shdr_info[cnt].symtab_idx != 0
		      && shdr_info[shdr_info[cnt].symtab_idx].data == NULL)
		    {
		      assert (shdr_info[cnt].shdr.sh_type == SHT_SYMTAB);

		      shdr_info[shdr_info[cnt].symtab_idx].data
			= elf_getdata (shdr_info[shdr_info[cnt].symtab_idx].scn,
				       NULL);
		      if (shdr_info[shdr_info[cnt].symtab_idx].data == NULL)
			INTERNAL_ERROR (fname);
		    }
		  xndxdata = shdr_info[shdr_info[cnt].symtab_idx].data;

		  /* Go through all symbols and make sure the section they
		     reference is not removed.  */
		  elsize = gelf_fsize (elf, ELF_T_SYM, 1, ehdr->e_version);

		  for (size_t inner = 0;
		       inner < shdr_info[cnt].data->d_size / elsize;
		       ++inner)
		    {
		      GElf_Sym sym_mem;
		      Elf32_Word xndx;
		      GElf_Sym *sym;
		      size_t scnidx;

		      sym = gelf_getsymshndx (symdata, xndxdata, inner,
					      &sym_mem, &xndx);
		      if (sym == NULL)
			INTERNAL_ERROR (fname);

		      scnidx = sym->st_shndx;
		      if (scnidx == SHN_UNDEF || scnidx >= shnum
			  || (scnidx >= SHN_LORESERVE
			      && scnidx <= SHN_HIRESERVE
			      && scnidx != SHN_XINDEX)
			  /* Don't count in the section symbols.  */
			  || GELF_ST_TYPE (sym->st_info) == STT_SECTION)
			/* This is no section index, leave it alone.  */
			continue;
		      else if (scnidx == SHN_XINDEX)
			scnidx = xndx;

		      if (shdr_info[scnidx].idx == 0)
			{
			  /* Mark this section as used.  */
			  shdr_info[scnidx].idx = 1;
			  changes |= scnidx < cnt;
			}
		    }
		}

	      /* Cross referencing happens:
		 - for the cases the ELF specification says.  That are
		 + SHT_DYNAMIC in sh_link to string table
		 + SHT_HASH in sh_link to symbol table
		 + SHT_REL and SHT_RELA in sh_link to symbol table
		 + SHT_SYMTAB and SHT_DYNSYM in sh_link to string table
		 + SHT_GROUP in sh_link to symbol table
		 + SHT_SYMTAB_SHNDX in sh_link to symbol table
		 Other (OS or architecture-specific) sections might as
		 well use this field so we process it unconditionally.
		 - references inside section groups
		 - specially marked references in sh_info if the SHF_INFO_LINK
		 flag is set
	      */

	      if (shdr_info[shdr_info[cnt].shdr.sh_link].idx == 0)
		{
		  shdr_info[shdr_info[cnt].shdr.sh_link].idx = 1;
		  changes |= shdr_info[cnt].shdr.sh_link < cnt;
		}

	      /* Handle references through sh_info.  */
	      if (SH_INFO_LINK_P (&shdr_info[cnt].shdr)
		  && shdr_info[shdr_info[cnt].shdr.sh_info].idx == 0)
		{
		  shdr_info[shdr_info[cnt].shdr.sh_info].idx = 1;
		  changes |= shdr_info[cnt].shdr.sh_info < cnt;
		}

	      /* Mark the section as investigated.  */
	      shdr_info[cnt].idx = 2;
	    }
	}
    }
  while (changes);

  /* Write out a copy of all the sections to the debug output file.
     The ones that are not removed in the stripped file are SHT_NOBITS */
  if (debug_fname != NULL)
    {
      for (cnt = 1; cnt < shnum; ++cnt)
	{
	  Elf_Data *debugdata;
	  GElf_Shdr debugshdr;
	  int discard_section;

	  scn = elf_newscn (debugelf);
	  if (scn == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("while generating output file: %s"),
		   elf_errmsg (-1));

	  discard_section = shdr_info[cnt].idx > 0 && cnt != ehdr->e_shstrndx;

	  /* Set the section header in the new file.  */
	  debugshdr = shdr_info[cnt].shdr;
	  if (discard_section)
	    debugshdr.sh_type = SHT_NOBITS;

	  if (unlikely (gelf_update_shdr (scn, &debugshdr)) == 0)
	    /* There cannot be any overflows.  */
	    INTERNAL_ERROR (fname);

	  /* Get the data from the old file if necessary. */
	  if (shdr_info[cnt].data == NULL)
	    {
	      shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
	      if (shdr_info[cnt].data == NULL)
		INTERNAL_ERROR (fname);
	    }

	  /* Set the data.  This is done by copying from the old file.  */
	  debugdata = elf_newdata (scn);
	  if (debugdata == NULL)
	    INTERNAL_ERROR (fname);

	  /* Copy the structure.  */
	  *debugdata = *shdr_info[cnt].data;
	  if (discard_section)
	    debugdata->d_buf = NULL;
	}

      /* Finish the ELF header.  Fill in the fields not handled by
	 libelf from the old file.  */
      debugehdr = gelf_getehdr (debugelf, &debugehdr_mem);
      if (debugehdr == NULL)
	INTERNAL_ERROR (fname);

      memcpy (debugehdr->e_ident, ehdr->e_ident, EI_NIDENT);
      debugehdr->e_type = ehdr->e_type;
      debugehdr->e_machine = ehdr->e_machine;
      debugehdr->e_version = ehdr->e_version;
      debugehdr->e_entry = ehdr->e_entry;
      debugehdr->e_flags = ehdr->e_flags;
      debugehdr->e_shstrndx = ehdr->e_shstrndx;

      if (unlikely (gelf_update_ehdr (debugelf, debugehdr)) == 0)
	{
	  error (0, 0, gettext ("%s: error while creating ELF header: %s"),
		 debug_fname, elf_errmsg (-1));
	  result = 1;
	  goto fail_close;
	}

      /* Finally write the file.  */
      if (unlikely (elf_update (debugelf, ELF_C_WRITE)) == -1)
	{
	  error (0, 0, gettext ("while writing `%s': %s"),
		 debug_fname, elf_errmsg (-1));
	  result = 1;
	  goto fail_close;
	}

      /* Create the real output file.  First rename, then change the
	 mode.  */
      if (rename (tmp_debug_fname, debug_fname) != 0
	  || fchmod (debug_fd, mode) != 0)
	{
	  error (0, errno, gettext ("while creating '%s'"), debug_fname);
	  result = 1;
	  goto fail_close;
	}

      /* The temporary file does not exist anymore.  */
      tmp_debug_fname = NULL;

      /* Compute the checksum which we will add to the executable.  */
      if (crc32_file (debug_fd, &debug_crc) != 0)
	{
	  error (0, errno,
		 gettext ("while computing checksum for debug information"));
	  unlink (debug_fname);
	  result = 1;
	  goto fail_close;
	}

    }

  /* Mark the section header string table as unused, we will create
     a new one.  */
  shdr_info[shstrndx].idx = 0;

  /* We need a string table for the section headers.  */
  shst = ebl_strtabinit (true);
  if (shst == NULL)
    error (EXIT_FAILURE, errno, gettext ("while preparing output for `%s'"),
	   output_fname ?: fname);

  /* Assign new section numbers.  */
  shdr_info[0].idx = 0;
  for (cnt = idx = 1; cnt < shnum; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	shdr_info[cnt].idx = idx++;

	/* Create a new section.  */
	shdr_info[cnt].newscn = elf_newscn (newelf);
	if (shdr_info[cnt].newscn == NULL)
	  error (EXIT_FAILURE, 0, gettext ("while generating output file: %s"),
		 elf_errmsg (-1));

	assert (elf_ndxscn (shdr_info[cnt].newscn) == shdr_info[cnt].idx);

	/* Add this name to the section header string table.  */
	shdr_info[cnt].se = ebl_strtabadd (shst, shdr_info[cnt].name, 0);
      }

  /* Test whether we are doing anything at all.  */
  if (cnt == idx)
    /* Nope, all removable sections are already gone.  */
    goto fail_close;

  /* Create the reference to the file with the debug info.  */
  if (debug_fname != NULL)
    {
      char *debug_basename;
      off_t crc_offset;

      /* Add the section header string table section name.  */
      shdr_info[cnt].se = ebl_strtabadd (shst, ".gnu_debuglink", 15);
      shdr_info[cnt].idx = idx++;

      /* Create the section header.  */
      shdr_info[cnt].shdr.sh_type = SHT_PROGBITS;
      shdr_info[cnt].shdr.sh_flags = 0;
      shdr_info[cnt].shdr.sh_addr = 0;
      shdr_info[cnt].shdr.sh_link = SHN_UNDEF;
      shdr_info[cnt].shdr.sh_info = SHN_UNDEF;
      shdr_info[cnt].shdr.sh_entsize = 0;
      shdr_info[cnt].shdr.sh_addralign = 4;
      /* We set the offset to zero here.  Before we write the ELF file the
	 field must have the correct value.  This is done in the final
	 loop over all section.  Then we have all the information needed.  */
      shdr_info[cnt].shdr.sh_offset = 0;

      /* Create the section.  */
      shdr_info[cnt].newscn = elf_newscn (newelf);
      if (shdr_info[cnt].newscn == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("while create section header section: %s"),
	       elf_errmsg (-1));
      assert (elf_ndxscn (shdr_info[cnt].newscn) == shdr_info[cnt].idx);

      shdr_info[cnt].data = elf_newdata (shdr_info[cnt].newscn);
      if (shdr_info[cnt].data == NULL)
	error (EXIT_FAILURE, 0, gettext ("cannot allocate section data: %s"),
	       elf_errmsg (-1));

      debug_basename = basename (debug_fname);
      crc_offset = strlen (debug_basename) + 1;
      /* Align to 4 byte boundary */
      crc_offset = ((crc_offset - 1) & ~3) + 4;

      shdr_info[cnt].data->d_align = 4;
      shdr_info[cnt].shdr.sh_size = shdr_info[cnt].data->d_size
	= crc_offset + 4;
      shdr_info[cnt].data->d_buf = xcalloc (1, shdr_info[cnt].data->d_size);

      strcpy (shdr_info[cnt].data->d_buf, debug_basename);
      /* Store the crc value in the correct byteorder */
      if ((__BYTE_ORDER == __LITTLE_ENDIAN
	   && ehdr->e_ident[EI_DATA] == ELFDATA2MSB)
	  || (__BYTE_ORDER == __BIG_ENDIAN
	      && ehdr->e_ident[EI_DATA] == ELFDATA2LSB))
	debug_crc = bswap_32 (debug_crc);
      memcpy ((char *)shdr_info[cnt].data->d_buf + crc_offset,
	      (char *) &debug_crc, 4);

      /* One more section done.  */
      ++cnt;
    }

  /* Index of the section header table in the shdr_info array.  */
  size_t shdridx = cnt;

  /* Add the section header string table section name.  */
  shdr_info[cnt].se = ebl_strtabadd (shst, ".shstrtab", 10);
  shdr_info[cnt].idx = idx;

  /* Create the section header.  */
  shdr_info[cnt].shdr.sh_type = SHT_STRTAB;
  shdr_info[cnt].shdr.sh_flags = 0;
  shdr_info[cnt].shdr.sh_addr = 0;
  shdr_info[cnt].shdr.sh_link = SHN_UNDEF;
  shdr_info[cnt].shdr.sh_info = SHN_UNDEF;
  shdr_info[cnt].shdr.sh_entsize = 0;
  /* We set the offset to zero here.  Before we write the ELF file the
     field must have the correct value.  This is done in the final
     loop over all section.  Then we have all the information needed.  */
  shdr_info[cnt].shdr.sh_offset = 0;
  shdr_info[cnt].shdr.sh_addralign = 1;

  /* Create the section.  */
  shdr_info[cnt].newscn = elf_newscn (newelf);
  if (shdr_info[cnt].newscn == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("while create section header section: %s"),
	   elf_errmsg (-1));
  assert (elf_ndxscn (shdr_info[cnt].newscn) == idx);

  /* Finalize the string table and fill in the correct indices in the
     section headers.  */
  shstrtab_data = elf_newdata (shdr_info[cnt].newscn);
  if (shstrtab_data == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("while create section header string table: %s"),
	   elf_errmsg (-1));
  ebl_strtabfinalize (shst, shstrtab_data);

  /* We have to set the section size.  */
  shdr_info[cnt].shdr.sh_size = shstrtab_data->d_size;

  /* Update the section information.  */
  GElf_Off lastoffset = 0;
  for (cnt = 1; cnt <= shdridx; ++cnt)
    if (shdr_info[cnt].idx > 0)
      {
	Elf_Data *newdata;

	scn = elf_getscn (newelf, shdr_info[cnt].idx);
	assert (scn != NULL);

	/* Update the name.  */
	shdr_info[cnt].shdr.sh_name = ebl_strtaboffset (shdr_info[cnt].se);

	/* Update the section header from the input file.  Some fields
	   might be section indeces which now have to be adjusted.  */
	if (shdr_info[cnt].shdr.sh_link != 0)
	  shdr_info[cnt].shdr.sh_link =
	    shdr_info[shdr_info[cnt].shdr.sh_link].idx;

	if (shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	  {
	    assert (shdr_info[cnt].data != NULL);

	    Elf32_Word *grpref = (Elf32_Word *) shdr_info[cnt].data->d_buf;
	    for (size_t inner = 0;
		 inner < shdr_info[cnt].data->d_size / sizeof (Elf32_Word);
		 ++inner)
	      grpref[inner] = shdr_info[grpref[inner]].idx;
	  }

	/* Handle the SHT_REL, SHT_RELA, and SHF_INFO_LINK flag.  */
	if (SH_INFO_LINK_P (&shdr_info[cnt].shdr))
	  shdr_info[cnt].shdr.sh_info =
	    shdr_info[shdr_info[cnt].shdr.sh_info].idx;

	/* Get the data from the old file if necessary.  We already
           created the data for the section header string table.  */
	if (cnt < shnum)
	  {
	    if (shdr_info[cnt].data == NULL)
	      {
		shdr_info[cnt].data = elf_getdata (shdr_info[cnt].scn, NULL);
		if (shdr_info[cnt].data == NULL)
		  INTERNAL_ERROR (fname);
	      }

	    /* Set the data.  This is done by copying from the old file.  */
	    newdata = elf_newdata (scn);
	    if (newdata == NULL)
	      INTERNAL_ERROR (fname);

	    /* Copy the structure.  */
	    *newdata = *shdr_info[cnt].data;

	    /* We know the size.  */
	    shdr_info[cnt].shdr.sh_size = shdr_info[cnt].data->d_size;

	    /* We have to adjust symtol tables.  The st_shndx member might
	       have to be updated.  */
	    if (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM
		|| shdr_info[cnt].shdr.sh_type == SHT_SYMTAB)
	      {
		Elf_Data *versiondata = NULL;
		Elf_Data *shndxdata = NULL;

		size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1,
					    ehdr->e_version);

		if (shdr_info[cnt].symtab_idx != 0)
		  {
		    assert (shdr_info[cnt].shdr.sh_type == SHT_SYMTAB_SHNDX);
		    /* This section has extended section information.
		       We have to modify that information, too.  */
		    shndxdata = elf_getdata (shdr_info[shdr_info[cnt].symtab_idx].scn,
					     NULL);

		    assert ((versiondata->d_size / sizeof (Elf32_Word))
			    >= shdr_info[cnt].data->d_size / elsize);
		  }

		if (shdr_info[cnt].version_idx != 0)
		  {
		    assert (shdr_info[cnt].shdr.sh_type == SHT_DYNSYM);
		    /* This section has associated version
		       information.  We have to modify that
		       information, too.  */
		    versiondata = elf_getdata (shdr_info[shdr_info[cnt].version_idx].scn,
					       NULL);

		    assert ((versiondata->d_size / sizeof (GElf_Versym))
			    >= shdr_info[cnt].data->d_size / elsize);
		  }

		shdr_info[cnt].newsymidx
		  = (Elf32_Word *) xcalloc (shdr_info[cnt].data->d_size
					    / elsize, sizeof (Elf32_Word));

		bool last_was_local = true;
		size_t destidx;
		size_t inner;
		for (destidx = inner = 1;
		     inner < shdr_info[cnt].data->d_size / elsize;
		     ++inner)
		  {
		    Elf32_Word sec;
		    GElf_Sym sym_mem;
		    Elf32_Word xshndx;
		    GElf_Sym *sym = gelf_getsymshndx (shdr_info[cnt].data,
						      shndxdata, inner,
						      &sym_mem, &xshndx);
		    if (sym == NULL)
		      INTERNAL_ERROR (fname);

		    if (sym->st_shndx == SHN_UNDEF
			|| (sym->st_shndx >= shnum
			    && sym->st_shndx != SHN_XINDEX))
		      {
			/* This is no section index, leave it alone
			   unless it is moved.  */
			if (destidx != inner
			    && gelf_update_symshndx (shdr_info[cnt].data,
						     shndxdata,
						     destidx, sym,
						     xshndx) == 0)
			  INTERNAL_ERROR (fname);

			shdr_info[cnt].newsymidx[inner] = destidx++;

			if (last_was_local
			    && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
			  {
			    last_was_local = false;
			    shdr_info[cnt].shdr.sh_info = destidx - 1;
			  }

			continue;
		      }

		    /* Get the full section index, if necessary from the
		       XINDEX table.  */
		    if (sym->st_shndx != SHN_XINDEX)
		      sec = shdr_info[sym->st_shndx].idx;
		    else
		      {
			assert (shndxdata != NULL);

			sec = shdr_info[xshndx].idx;
		      }

		    if (sec != 0)
		      {
			GElf_Section nshndx;
			Elf32_Word nxshndx;

			if (sec < SHN_LORESERVE)
			  {
			    nshndx = sec;
			    nxshndx = 0;
			  }
			else
			  {
			    nshndx = SHN_XINDEX;
			    nxshndx = sec;
			  }

			assert (sec < SHN_LORESERVE || shndxdata != NULL);

			if ((inner != destidx || nshndx != sym->st_shndx
			     || (shndxdata != NULL && nxshndx != xshndx))
			    && (sym->st_shndx = nshndx,
				gelf_update_symshndx (shdr_info[cnt].data,
						      shndxdata,
						      destidx, sym,
						      nxshndx) == 0))
			  INTERNAL_ERROR (fname);

			shdr_info[cnt].newsymidx[inner] = destidx++;

			if (last_was_local
			    && GELF_ST_BIND (sym->st_info) != STB_LOCAL)
			  {
			    last_was_local = false;
			    shdr_info[cnt].shdr.sh_info = destidx - 1;
			  }
		      }
		    else
		      /* This is a section symbol for a section which has
			 been removed.  */
		      assert (GELF_ST_TYPE (sym->st_info) == STT_SECTION);
		  }

		if (destidx != inner)
		  {
		    /* The size of the symbol table changed.  */
		    shdr_info[cnt].shdr.sh_size = newdata->d_size
		      = destidx * elsize;
		    any_symtab_changes = true;
		  }
		else
		  {
		    /* The symbol table didn't really change.  */
		    free (shdr_info[cnt].newsymidx);
		    shdr_info[cnt].newsymidx = NULL;
		  }
	      }
	  }

	/* If we have to, compute the offset of the section.  */
	if (shdr_info[cnt].shdr.sh_offset == 0)
	  shdr_info[cnt].shdr.sh_offset
	    = ((lastoffset + shdr_info[cnt].shdr.sh_addralign - 1)
	       & ~((GElf_Off) (shdr_info[cnt].shdr.sh_addralign - 1)));

	/* Set the section header in the new file.  */
	if (unlikely (gelf_update_shdr (scn, &shdr_info[cnt].shdr) == 0))
	  /* There cannot be any overflows.  */
	  INTERNAL_ERROR (fname);

	/* Remember the last section written so far.  */
	GElf_Off filesz = (shdr_info[cnt].shdr.sh_type != SHT_NOBITS
			   ? shdr_info[cnt].shdr.sh_size : 0);
	if (lastoffset < shdr_info[cnt].shdr.sh_offset + filesz)
	  lastoffset = shdr_info[cnt].shdr.sh_offset + filesz;
      }

  /* Adjust symbol references if symbol tables changed.  */
  if (any_symtab_changes)
    {
      /* Find all relocation sections which use this
	 symbol table.  */
      for (cnt = 1; cnt <= shdridx; ++cnt)
	{
	  if (shdr_info[cnt].idx == 0)
	    /* Ignore sections which are discarded.  */
	    continue;

	  if (shdr_info[cnt].shdr.sh_type == SHT_REL
	      || shdr_info[cnt].shdr.sh_type == SHT_RELA)
	    {
	      /* If the symbol table hasn't changed, do not do anything.  */
	      if (shdr_info[shdr_info[cnt].old_sh_link].newsymidx == NULL)
		continue;

	      Elf32_Word *newsymidx
		= shdr_info[shdr_info[cnt].old_sh_link].newsymidx;
	      Elf_Data *d = elf_getdata (elf_getscn (newelf,
						     shdr_info[cnt].idx),
					 NULL);
	      assert (d != NULL);
	      size_t nrels = (shdr_info[cnt].shdr.sh_size
			      / shdr_info[cnt].shdr.sh_entsize);

	      if (shdr_info[cnt].shdr.sh_type == SHT_REL)
		for (size_t relidx = 0; relidx < nrels; ++relidx)
		  {
		    GElf_Rel rel_mem;
		    if (gelf_getrel (d, relidx, &rel_mem) == NULL)
		      INTERNAL_ERROR (fname);

		    size_t symidx = GELF_R_SYM (rel_mem.r_info);
		    if (newsymidx[symidx] != symidx)
		      {
			rel_mem.r_info
			  = GELF_R_INFO (newsymidx[symidx],
					 GELF_R_TYPE (rel_mem.r_info));

			if (gelf_update_rel (d, relidx, &rel_mem) == 0)
			  INTERNAL_ERROR (fname);
		      }
		  }
	      else
		for (size_t relidx = 0; relidx < nrels; ++relidx)
		  {
		    GElf_Rela rel_mem;
		    if (gelf_getrela (d, relidx, &rel_mem) == NULL)
		      INTERNAL_ERROR (fname);

		    size_t symidx = GELF_R_SYM (rel_mem.r_info);
		    if (newsymidx[symidx] != symidx)
		      {
			rel_mem.r_info
			  = GELF_R_INFO (newsymidx[symidx],
					 GELF_R_TYPE (rel_mem.r_info));

			if (gelf_update_rela (d, relidx, &rel_mem) == 0)
			  INTERNAL_ERROR (fname);
		      }
		  }
	    }
	  else if (shdr_info[cnt].shdr.sh_type == SHT_HASH)
	    {
	      /* We have to recompute the hash table.  */
	      Elf32_Word symtabidx = shdr_info[cnt].old_sh_link;

	      /* We do not have to do anything if the symbol table was
		 not changed.  */
	      if (shdr_info[symtabidx].newsymidx == NULL)
		continue;

	      /* The symbol version section in the new file.  */
	      scn = elf_getscn (newelf, shdr_info[cnt].idx);

	      /* The symbol table data.  */
	      Elf_Data *symd = elf_getdata (elf_getscn (newelf,
							shdr_info[symtabidx].idx),
					    NULL);
	      assert (symd != NULL);

	      /* The hash table data.  */
	      Elf_Data *hashd = elf_getdata (scn, NULL);
	      assert (hashd != NULL);

	      if (shdr_info[cnt].shdr.sh_entsize == sizeof (Elf32_Word))
		{
		  /* Sane arches first.  */
		  Elf32_Word *bucket = (Elf32_Word *) hashd->d_buf;

		  size_t strshndx = shdr_info[symtabidx].old_sh_link;
		  size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1,
					      ehdr->e_version);

		  /* Convert to the correct byte order.  */
		  if (gelf_xlatetom (newelf, hashd, hashd,
				     BYTE_ORDER == LITTLE_ENDIAN
				     ? ELFDATA2LSB : ELFDATA2MSB) == NULL)
		    INTERNAL_ERROR (fname);

		  /* Adjust the nchain value.  The symbol table size
		     changed.  We keep the same size for the bucket array.  */
		  bucket[1] = symd->d_size / elsize;
		  Elf32_Word nbucket = bucket[0];
		  bucket += 2;
		  Elf32_Word *chain = bucket + nbucket;

		  /* New size of the section.  */
		  GElf_Shdr shdr_mem;
		  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
		  shdr->sh_size = hashd->d_size
		    = (2 + symd->d_size / elsize + nbucket)
		      * sizeof (Elf32_Word);
		  (void) gelf_update_shdr (scn, shdr);

		  /* Clear the arrays.  */
		  memset (bucket, '\0',
			  (symd->d_size / elsize + nbucket)
			  * sizeof (Elf32_Word));

		  for (size_t inner = shdr_info[symtabidx].shdr.sh_info;
		       inner < symd->d_size / elsize; ++inner)
		    {
		      GElf_Sym sym_mem;
		      GElf_Sym *sym = gelf_getsym (symd, inner, &sym_mem);
		      assert (sym != NULL);

		      const char *name = elf_strptr (elf, strshndx,
						     sym->st_name);
		      assert (name != NULL);
		      size_t hidx = elf_hash (name) % nbucket;

		      if (bucket[hidx] == 0)
			bucket[hidx] = inner;
		      else
			{
			  hidx = bucket[hidx];

			  while (chain[hidx] != 0)
			    hidx = chain[hidx];

			  chain[hidx] = inner;
			}
		    }
		}
	      else
		{
		  /* Alpha and S390 64-bit use 64-bit SHT_HASH entries.  */
		  assert (shdr_info[cnt].shdr.sh_entsize
			  == sizeof (Elf64_Xword));

		  Elf64_Xword *bucket = (Elf64_Xword *) hashd->d_buf;

		  size_t strshndx = shdr_info[symtabidx].old_sh_link;
		  size_t elsize = gelf_fsize (elf, ELF_T_SYM, 1,
					      ehdr->e_version);

		  /* Convert to the correct byte order.  */
		  if (gelf_xlatetom (newelf, hashd, hashd,
				     BYTE_ORDER == LITTLE_ENDIAN
				     ? ELFDATA2LSB : ELFDATA2MSB) == NULL)
		    INTERNAL_ERROR (fname);

		  /* Adjust the nchain value.  The symbol table size
		     changed.  We keep the same size for the bucket array.  */
		  bucket[1] = symd->d_size / elsize;
		  Elf64_Xword nbucket = bucket[0];
		  bucket += 2;
		  Elf64_Xword *chain = bucket + nbucket;

		  /* New size of the section.  */
		  GElf_Shdr shdr_mem;
		  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
		  shdr->sh_size = hashd->d_size
		    = (2 + symd->d_size / elsize + nbucket)
		      * sizeof (Elf64_Xword);
		  (void) gelf_update_shdr (scn, shdr);

		  /* Clear the arrays.  */
		  memset (bucket, '\0',
			  (symd->d_size / elsize + nbucket)
			  * sizeof (Elf64_Xword));

		  for (size_t inner = shdr_info[symtabidx].shdr.sh_info;
		       inner < symd->d_size / elsize; ++inner)
		    {
		      GElf_Sym sym_mem;
		      GElf_Sym *sym = gelf_getsym (symd, inner, &sym_mem);
		      assert (sym != NULL);

		      const char *name = elf_strptr (elf, strshndx,
						     sym->st_name);
		      assert (name != NULL);
		      size_t hidx = elf_hash (name) % nbucket;

		      if (bucket[hidx] == 0)
			bucket[hidx] = inner;
		      else
			{
			  hidx = bucket[hidx];

			  while (chain[hidx] != 0)
			    hidx = chain[hidx];

			  chain[hidx] = inner;
			}
		    }
	        }

	      /* Convert back to the file byte order.  */
	      if (gelf_xlatetof (newelf, hashd, hashd,
				 BYTE_ORDER == LITTLE_ENDIAN
				 ? ELFDATA2LSB : ELFDATA2MSB) == NULL)
		INTERNAL_ERROR (fname);
	    }
	  else if (shdr_info[cnt].shdr.sh_type == SHT_GNU_versym)
	    {
	      /* If the symbol table changed we have to adjust the
		 entries.  */
	      Elf32_Word symtabidx = shdr_info[cnt].old_sh_link;

	      /* We do not have to do anything if the symbol table was
		 not changed.  */
	      if (shdr_info[symtabidx].newsymidx == NULL)
		continue;

	      /* The symbol version section in the new file.  */
	      scn = elf_getscn (newelf, shdr_info[cnt].idx);

	      /* The symbol table data.  */
	      Elf_Data *symd = elf_getdata (elf_getscn (newelf,
							shdr_info[symtabidx].idx),
					    NULL);
	      assert (symd != NULL);

	      /* The version symbol data.  */
	      Elf_Data *verd = elf_getdata (scn, NULL);
	      assert (verd != NULL);

	      /* Convert to the correct byte order.  */
	      if (gelf_xlatetom (newelf, verd, verd,
				 BYTE_ORDER == LITTLE_ENDIAN
				 ? ELFDATA2LSB : ELFDATA2MSB) == NULL)
		INTERNAL_ERROR (fname);

	      /* The symbol version array.  */
	      GElf_Half *verstab = (GElf_Half *) verd->d_buf;

	      /* New indices of the symbols.  */
	      Elf32_Word *newsymidx = shdr_info[symtabidx].newsymidx;

	      /* Walk through the list and */
	      size_t elsize = gelf_fsize (elf, verd->d_type, 1,
					  ehdr->e_version);
	      for (size_t inner = 1; inner < verd->d_size / elsize; ++inner)
		if (newsymidx[inner] != 0)
		  /* Overwriting the same array works since the
		     reordering can only move entries to lower indices
		     in the array.  */
		  verstab[newsymidx[inner]] = verstab[inner];

	      /* New size of the section.  */
	      GElf_Shdr shdr_mem;
	      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	      shdr->sh_size = verd->d_size
		= gelf_fsize (newelf, verd->d_type,
			      symd->d_size / gelf_fsize (elf, symd->d_type, 1,
							 ehdr->e_version),
			      ehdr->e_version);
	      (void) gelf_update_shdr (scn, shdr);

	      /* Convert back to the file byte order.  */
	      if (gelf_xlatetof (newelf, verd, verd,
				 BYTE_ORDER == LITTLE_ENDIAN
				 ? ELFDATA2LSB : ELFDATA2MSB) == NULL)
		INTERNAL_ERROR (fname);
	    }
	  else if (shdr_info[cnt].shdr.sh_type == SHT_GROUP)
	    {
	      /* Check whether the associated symbol table changed.  */
	      if (shdr_info[shdr_info[cnt].old_sh_link].newsymidx != NULL)
		{
		  /* Yes the symbol table changed.  Update the section
		     header of the section group.  */
		  scn = elf_getscn (newelf, shdr_info[cnt].idx);
		  GElf_Shdr shdr_mem;
		  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
		  assert (shdr != NULL);

		  size_t stabidx = shdr_info[cnt].old_sh_link;
		  shdr->sh_info = shdr_info[stabidx].newsymidx[shdr->sh_info];

		  (void) gelf_update_shdr (scn, shdr);
		}
	    }
	}
    }

  /* Finally finish the ELF header.  Fill in the fields not handled by
     libelf from the old file.  */
  newehdr = gelf_getehdr (newelf, &newehdr_mem);
  if (newehdr == NULL)
    INTERNAL_ERROR (fname);

  memcpy (newehdr->e_ident, ehdr->e_ident, EI_NIDENT);
  newehdr->e_type = ehdr->e_type;
  newehdr->e_machine = ehdr->e_machine;
  newehdr->e_version = ehdr->e_version;
  newehdr->e_entry = ehdr->e_entry;
  newehdr->e_flags = ehdr->e_flags;
  newehdr->e_phoff = ehdr->e_phoff;
  /* We need to position the section header table.  */
  const size_t offsize = gelf_fsize (elf, ELF_T_OFF, 1, EV_CURRENT);
  newehdr->e_shoff = ((shdr_info[shdridx].shdr.sh_offset
		       + shdr_info[shdridx].shdr.sh_size + offsize - 1)
		      & ~((GElf_Off) (offsize - 1)));
  newehdr->e_shentsize = gelf_fsize (elf, ELF_T_SHDR, 1, EV_CURRENT);

  /* The new section header string table index.  */
  if (likely (idx < SHN_HIRESERVE) && likely (idx != SHN_XINDEX))
    newehdr->e_shstrndx = idx;
  else
    {
      /* The index does not fit in the ELF header field.  */
      shdr_info[0].scn = elf_getscn (elf, 0);

      if (gelf_getshdr (shdr_info[0].scn, &shdr_info[0].shdr) == NULL)
	INTERNAL_ERROR (fname);

      shdr_info[0].shdr.sh_link = idx;
      (void) gelf_update_shdr (shdr_info[0].scn, &shdr_info[0].shdr);

      newehdr->e_shstrndx = SHN_XINDEX;
    }

  if (gelf_update_ehdr (newelf, newehdr) == 0)
    {
      error (0, 0, gettext ("%s: error while creating ELF header: %s"),
	     fname, elf_errmsg (-1));
      return 1;
    }

  /* We have everything from the old file.  */
  if (elf_cntl (elf, ELF_C_FDDONE) != 0)
    {
      error (0, 0, gettext ("%s: error while reading the file: %s"),
	     fname, elf_errmsg (-1));
      return 1;
    }

  /* The ELF library better follows our layout when this is not a
     relocatable object file.  */
  elf_flagelf (newelf, ELF_C_SET,
	       (ehdr->e_type != ET_REL ? ELF_F_LAYOUT : 0)
	       | (permissive ? ELF_F_PERMISSIVE : 0));

  /* Finally write the file.  */
  if (elf_update (newelf, ELF_C_WRITE) == -1)
    {
      error (0, 0, gettext ("while writing `%s': %s"),
	     fname, elf_errmsg (-1));
      result = 1;
    }

 fail_close:
  if (shdr_info != NULL)
    {
      /* For some sections we might have created an table to map symbol
	 table indices.  */
      if (any_symtab_changes)
	for (cnt = 1; cnt <= shdridx; ++cnt)
	  free (shdr_info[cnt].newsymidx);

      /* Free the memory.  */
      if ((shnum + 2) * sizeof (struct shdr_info) > MAX_STACK_ALLOC)
	free (shdr_info);
    }

  /* Free other resources.  */
  if (shstrtab_data != NULL)
    free (shstrtab_data->d_buf);
  if (shst != NULL)
    ebl_strtabfree (shst);

  /* That was it.  Close the descriptors.  */
  if (elf_end (newelf) != 0)
    {
      error (0, 0, gettext ("error while finishing `%s': %s"), fname,
	     elf_errmsg (-1));
      result = 1;
    }

  if (debugelf != NULL && elf_end (debugelf) != 0)
    {
      error (0, 0, gettext ("error while finishing `%s': %s"), debug_fname,
	     elf_errmsg (-1));
      result = 1;
    }

 fail:
  /* Close the EBL backend.  */
  if (ebl != NULL)
    ebl_closebackend (ebl);

  /* Close debug file descriptor, if opened */
  if (debug_fd >= 0)
    {
      if (tmp_debug_fname != NULL)
	unlink (tmp_debug_fname);
      close (debug_fd);
    }

  /* If requested, preserve the timestamp.  */
  if (tvp != NULL)
    {
      if (futimes (fd, tvp) != 0)
	{
	  error (0, errno, gettext ("\
cannot set access and modification date of \"%s\""),
		 output_fname ?: fname);
	  result = 1;
	}
    }

  /* Close the file descriptor if we created a new file.  */
  if (output_fname != NULL)
    close (fd);

  return result;
}


static int
handle_ar (int fd, Elf *elf, const char *prefix, const char *fname,
	   struct timeval tvp[2])
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char new_prefix[prefix_len + 1 + fname_len];
  char *cp = new_prefix;

  /* Create the full name of the file.  */
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);


  /* Process all the files contained in the archive.  */
  Elf *subelf;
  Elf_Cmd cmd = ELF_C_RDWR;
  int result = 0;
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      /* The the header for this element.  */
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      if (elf_kind (subelf) == ELF_K_ELF)
	result |= handle_elf (fd, subelf, new_prefix, arhdr->ar_name, 0, NULL);
      else if (elf_kind (subelf) == ELF_K_AR)
	result |= handle_ar (fd, subelf, new_prefix, arhdr->ar_name, NULL);

      /* Get next archive element.  */
      cmd = elf_next (subelf);
      if (unlikely (elf_end (subelf) != 0))
	INTERNAL_ERROR (fname);
    }

  if (tvp != NULL)
    {
      if (unlikely (futimes (fd, tvp) != 0))
	{
	  error (0, errno, gettext ("\
cannot set access and modification date of \"%s\""), fname);
	  result = 1;
	}
    }

  if (unlikely (close (fd) != 0))
    error (EXIT_FAILURE, errno, gettext ("while closing `%s'"), fname);

  return result;
}
