/* Copyright (C) 2002 Red Hat, Inc.
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
#include <libasm.h>
#include <libelf.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


static const char fname[] = "asm-tst1-out.o";


static const GElf_Ehdr expected_ehdr =
  {
    .e_ident = { [EI_MAG0] = ELFMAG0,
		 [EI_MAG1] = ELFMAG1,
		 [EI_MAG2] = ELFMAG2,
		 [EI_MAG3] = ELFMAG3,
		 [EI_CLASS] = ELFCLASS32,
		 [EI_DATA] = ELFDATA2LSB,
		 [EI_VERSION] = EV_CURRENT },
    .e_type = ET_REL,
    .e_machine = EM_386,
    .e_version = EV_CURRENT,
    .e_shoff = 88,
    .e_ehsize = sizeof (Elf32_Ehdr),
    .e_shentsize = sizeof (Elf32_Shdr),
    .e_shnum = 4,
    .e_shstrndx = 3
  };


static const char *scnnames[4] =
  {
    [0] = "",
    [1] = ".text",
    [2] = ".data",
    [3] = ".shstrtab"
  };


int
main (void)
{
  AsmCtx_t *ctx;
  AsmScn_t *scn1;
  AsmScn_t *scn2;
  int fd;
  Elf *elf;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  int result = 0;
  size_t cnt;

  elf_version (EV_CURRENT);

  ctx = asm_begin (fname, false, EM_386, ELFCLASS32, ELFDATA2LSB);
  if (ctx == NULL)
    {
      printf ("cannot create assembler context: %s\n", asm_errmsg (-1));
      return 1;
    }

  /* Create two sections.  */
  scn1 = asm_newscn (ctx, ".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
  scn2 = asm_newscn (ctx, ".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
  if (scn1 == NULL || scn2 == NULL)
    {
      printf ("cannot create section in output file: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
    }

  /* Special alignment for the .text section.  */
  if (asm_align (scn1, 32) != 0)
    {
      printf ("cannot align .text section: %s\n", asm_errmsg (-1));
      result = 1;
    }

  /* Create the output file.  */
  if (asm_end (ctx) != 0)
    {
      printf ("cannot create output file: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
    }

  /* Check the file.  */
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open generated file: %m\n");
      result = 1;
      goto out;
    }

  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      result = 1;
      goto out_close;
    }
  if (elf_kind (elf) != ELF_K_ELF)
    {
      puts ("not a valid ELF file");
      result = 1;
      goto out_close2;
    }

  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      printf ("cannot get ELF header: %s\n", elf_errmsg (-1));
      result = 1;
      goto out_close2;
    }

  if (memcmp (ehdr, &expected_ehdr, sizeof (GElf_Ehdr)) != 0)
    {
      puts ("ELF header does not match");
      result = 1;
      goto out_close2;
    }

  for (cnt = 1; cnt < 4; ++cnt)
    {
      Elf_Scn *scn;
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr;

      scn = elf_getscn (elf, cnt);
      if (scn == NULL)
	{
	  printf ("cannot get section %Zd: %s\n", cnt, elf_errmsg (-1));
	  result = 1;
	  continue;
	}

      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  printf ("cannot get section header for section %Zd: %s\n",
		  cnt, elf_errmsg (-1));
	  result = 1;
	  continue;
	}

      if (strcmp (elf_strptr (elf, ehdr->e_shstrndx, shdr->sh_name),
		  scnnames[cnt]) != 0)
	{
	  printf ("section %Zd's name differs: %s vs %s\n", cnt,
		  elf_strptr (elf, ehdr->e_shstrndx, shdr->sh_name),
		  scnnames[cnt]);
	  result = 1;
	}

      if (shdr->sh_type != (cnt == 3 ? SHT_STRTAB : SHT_PROGBITS))
	{
	  printf ("section %Zd's type differs\n", cnt);
	  result = 1;
	}

      if ((cnt == 1 && shdr->sh_flags != (SHF_ALLOC | SHF_EXECINSTR))
	  || (cnt == 2 && shdr->sh_flags != (SHF_ALLOC | SHF_WRITE))
	  || (cnt == 3 && shdr->sh_flags != 0))
	{
	  printf ("section %Zd's flags differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_addr != 0)
	{
	  printf ("section %Zd's address differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_offset != ((sizeof (Elf32_Ehdr) + 31) & ~31))
	{
	  printf ("section %Zd's offset differs\n", cnt);
	  result = 1;
	}

      if ((cnt != 3 && shdr->sh_size != 0)
	  || (cnt == 3 && shdr->sh_size != 23))
	{
	  printf ("section %Zd's size differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_link != 0)
	{
	  printf ("section %Zd's link differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_info != 0)
	{
	  printf ("section %Zd's info differs\n", cnt);
	  result = 1;
	}

      if ((cnt == 1 && shdr->sh_addralign != 32)
	  || (cnt != 1 && shdr->sh_addralign != 1))
	{
	  printf ("section %Zd's addralign differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_entsize != 0)
	{
	  printf ("section %Zd's entsize differs\n", cnt);
	  result = 1;
	}
    }

 out_close2:
  elf_end (elf);
 out_close:
  close (fd);
 out:
  /* We don't need the file anymore.  */
  unlink (fname);

  return result;
}
