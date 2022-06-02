#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "load_elf.h"
#include "memcpy_device.c"

#define DRAM_BASE 0x80000000
#define DRAM_SIZE 0x10000000
#define MAX_FILE_SIZE 0x1000000

static void *memSet(void *s, int c, size_t n)
{
  if (NULL == s || n < 0)
    return NULL;
  char * tmpS = (char *)s;
  while(n-- > 0)
    *tmpS++ = c;
    return s; 
}

static void *memCpy(void *dest, const void *src, size_t n)
{
  if (NULL == dest || NULL == src || n < 0)
    return NULL;
  char *tempDest = (char *)dest;
  char *tempSrc = (char *)src;
 
  while (n-- > 0)
    *tempDest++ = *tempSrc++;
  return dest;  
}


int main(int argc, char *argv[]) {
  char* to_device = argv[1];
  char* from_device = argv[2];
  char* elf_name = argv[3];
  FILE* elf_fd = 0;
  uint32_t elf_size = 0;
  elf_fd = fopen(elf_name, "r");
  if (elf_fd == NULL) {
    printf("Wrong ELF file\n");
    return 1;
  }
  fseek(elf_fd, 0, SEEK_END);
  elf_size = ftell(elf_fd);
  printf("%d\n", elf_size);
  fseek(elf_fd, 0, SEEK_SET);
  uint8_t* elf = (uint8_t*)calloc(elf_size + 1, sizeof(uint8_t));

  uint32_t count = fread(elf, sizeof(uint8_t), elf_size, elf_fd);
  fclose(elf_fd);

//  memcpy_to_device(to_device, (uint64_t)(DRAM_BASE + DRAM_SIZE - MAX_FILE_SIZE), elf, elf_size + 1);
//  printf("%d\n", sizeof(Elf64_Ehdr));
//
//  uint8_t* elf_test = (uint8_t*)calloc(elf_size + 1, sizeof(uint8_t));
//  memcpy_from_device(from_device, elf_test, (uint64_t)(DRAM_BASE + DRAM_SIZE - MAX_FILE_SIZE), elf_size + 1);
//  int cmp = strncmp(elf, elf_test, elf_size + 1);
//  printf("[SJ] Copy_elf result: %d\n", cmp);
//  free(elf_test);

  // sanity checks
  if(elf_size <= sizeof(Elf64_Ehdr))
    return 1;                   /* too small */

  const Elf64_Ehdr *eh = (const Elf64_Ehdr *)elf;
  if(!IS_ELF64(*eh))
    return 2;                   /* not a elf64 file */

  const Elf64_Phdr *ph = (const Elf64_Phdr *)(elf + eh->e_phoff);
  if(elf_size < eh->e_phoff + eh->e_phnum*sizeof(*ph))
    return 3;                   /* internal damaged */

  uint32_t i;
  for(i=0; i<eh->e_phnum; i++) {
    if(ph[i].p_type == PT_LOAD && ph[i].p_memsz) { /* need to load this physical section */
      printf("[load_elf]Writing %ld bytes to ", ph[i].p_filesz);
      printf("0x%lx \n\r", (uintptr_t)ph[i].p_paddr);
      printf("0x%lx \n\r", (uintptr_t)ph[i].p_offset);
      
      if(ph[i].p_filesz) {                         /* has data */
        if(elf_size < ph[i].p_offset + ph[i].p_filesz)
          return 3;             /* internal damaged */
        memcpy_to_device(to_device, (uint64_t)(ph[i].p_paddr/* - 0x80000000*/), elf + ph[i].p_offset, ph[i].p_filesz);
      }
      if(ph[i].p_memsz > ph[i].p_filesz) { /* zero padding */
        char* zero_pad = (char*)calloc((ph[i].p_memsz - ph[i].p_filesz), sizeof(char));
        memcpy_to_device(to_device, (uint64_t)(ph[i].p_paddr + ph[i].p_filesz/* - 0x80000000*/), zero_pad, ph[i].p_memsz - ph[i].p_filesz);
        free(zero_pad);
      }
    }
  }
  for(i=0; i<eh->e_phnum; i++) {
    char* test = (char*)calloc(ph[i].p_filesz + 1, sizeof(char));
    memcpy_from_device(from_device, test, (uint64_t)(ph[i].p_paddr/* - 0x80000000*/), ph[i].p_filesz);
    int cmp = strncmp(elf + ph[i].p_offset, test, ph[i].p_filesz);
    printf("[SJ] Copy (%d) result: %d\n", ph[i].p_filesz, cmp);
    free(test);
  }

  char* dummy_boot = (char*)calloc(64, sizeof(char));
  memcpy_from_device(from_device, dummy_boot, 0x800000000000000, 64);

  free(elf);
  free(dummy_boot);
  return 0;
}
