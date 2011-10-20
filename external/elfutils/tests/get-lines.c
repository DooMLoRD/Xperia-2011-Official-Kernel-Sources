/* Copyright (C) 2002, 2004 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#include <fcntl.h>
#include <inttypes.h>
#include <libelf.h>
#include <libdw.h>
#include <stdio.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);

      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if  (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[cnt], dwarf_errmsg (-1));
	  close  (fd);
	  continue;
	}

      Dwarf_Off cuoff = 0;
      Dwarf_Off old_cuoff = 0;
      size_t hsize;
      Dwarf_Off ao;
      uint8_t asz;
      uint8_t osz;
      while (dwarf_nextcu (dbg, cuoff, &cuoff, &hsize, &ao, &asz, &osz) == 0)
	{
	  printf ("cuhl = %zu, o = %llu, asz = %hhu, osz = %hhu, ncu = %llu\n",
		  hsize, (unsigned long long int) ao,
		  asz, osz, (unsigned long long int) cuoff);

	  /* Get the DIE for the CU.  */
	  Dwarf_Die die;
 	  if (dwarf_offdie (dbg, old_cuoff + hsize, &die) == NULL)
	    {
	      /* Something went wrong.  */
	      printf ("%s: cannot get CU die\n", argv[cnt]);
	      result = 1;
	      break;
	    }
	  old_cuoff = cuoff;

	  Dwarf_Lines *lb;
	  size_t nlb;
	  if (dwarf_getsrclines (&die, &lb, &nlb) != 0)
	    {
	      printf ("%s: cannot get lines\n", argv[cnt]);
	      result = 1;
	      break;
	    }

	  printf (" %zu lines\n", nlb);

	  for (size_t i = 0; i < nlb; ++i)
	    {
	      Dwarf_Line *l = dwarf_onesrcline (lb, i);
	      if (l == NULL)
		{
		  printf ("%s: cannot get individual line\n", argv[cnt]);
		  result = 1;
		  break;
		}

	      Dwarf_Addr addr;
	      if (dwarf_lineaddr (l, &addr) != 0)
		addr = 0;
	      const char *file = dwarf_linesrc (l, NULL, NULL);
	      int line;
	      if (dwarf_lineno (l, &line) != 0)
		line = 0;

	      printf ("%" PRIx64 ": %s:%d:", (uint64_t) addr,
		      file ?: "???", line);

	      int column;
	      if (dwarf_linecol (l, &column) != 0)
		column = 0;
	      if (column >= 0)
		printf ("%d:", column);

	      bool is_stmt;
	      if (dwarf_linebeginstatement (l, &is_stmt) != 0)
		is_stmt = false;
	      bool end_sequence;
	      if (dwarf_lineendsequence (l, &end_sequence) != 0)
		end_sequence = false;
	      bool basic_block;
	      if (dwarf_lineblock (l, &basic_block) != 0)
		basic_block = false;
	      bool prologue_end;
	      if (dwarf_lineprologueend (l, &prologue_end) != 0)
		prologue_end = false;
	      bool epilogue_begin;
	      if (dwarf_lineepiloguebegin (l, &epilogue_begin) != 0)
		epilogue_begin = false;

	      printf (" is_stmt:%s, end_seq:%s, bb:%s, prologue:%s, epilogue:%s\n",
		      is_stmt ? "yes" : "no", end_sequence ? "yes" : "no",
		      basic_block ? "yes" : "no", prologue_end  ? "yes" : "no",
		      epilogue_begin ? "yes" : "no");
	    }
	}

      dwarf_end (dbg);
      close (fd);
    }

  return result;
}
