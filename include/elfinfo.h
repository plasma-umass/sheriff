/*
  Copyright (C) 2011 University of Massachusetts Amherst.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/*
 * @file   elfinfo.h
 * @brief  elf info, borrowed from Linux.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 
    

#ifndef __ELFINFO_H__
#define __ELFINFO_H__


#include<elf.h>

#ifdef X86_32BIT 
#define Elf_Ehdr    Elf32_Ehdr
#define Elf_Shdr    Elf32_Shdr
#define Elf_Sym     Elf32_Sym
#define Elf_Addr    Elf32_Addr
#define Elf_Section Elf32_Section

#define ELF_ST_BIND(x)    ((x) >> 4)
#define ELF_ST_TYPE(x)    (((unsigned int) x) & 0xf)

#define Elf_Rel     Elf32_Rel
#define Elf_Rela    Elf32_Rela
#define ELF_R_SYM   ELF32_R_SYM
#define ELF_R_TYPE  ELF32_R_TYPE

struct elf_info {
    unsigned long size;
    Elf_Ehdr     * hdr;
    Elf32_Shdr     * sechdrs;
    Elf32_Sym      * symtab_start;
    Elf32_Sym      * symtab_stop;
    const char   *strtab;
};
#else
#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Shdr    Elf64_Shdr
#define Elf_Sym     Elf64_Sym
#define Elf_Addr    Elf64_Addr
#define Elf_Section Elf64_Section

#define ELF_ST_BIND(x)    ((x) >> 4)
#define ELF_ST_TYPE(x)    (((unsigned int) x) & 0xf)

#define Elf_Rel     Elf64_Rel
#define Elf_Rela    Elf64_Rela
#define ELF_R_SYM   ELF64_R_SYM
#define ELF_R_TYPE  ELF64_R_TYPE

struct elf_info {
    unsigned long size;
    Elf_Ehdr     * hdr;
    Elf64_Shdr     * sechdrs;
    Elf64_Sym      * symtab_start;
    Elf64_Sym      * symtab_stop;
    const char   *strtab;
};

#endif

#endif /* __ELFINFO_H__ */
