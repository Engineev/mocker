#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ___array____ctor_(void *ptr, int64_t sz, int64_t element_sz) {
  char *inst_ptr = (char *)(ptr);
  *(int64_t *)(inst_ptr) = sz;
  void *addr = malloc((size_t)(sz * element_sz));
  *(void **)(inst_ptr + 8) = addr;
}

uint64_t ___array___size(void *ptr) { return *((uint64_t *)ptr - 1); }

uint64_t __string__length(void *ptr) { return *(uint64_t *)ptr; }

void *__string__add(void *lhs, void *rhs) {
  void *res_inst_ptr = malloc(16);

  uint64_t lhs_len = __string__length(lhs);
  uint64_t rhs_len = __string__length(rhs);
  uint64_t length = lhs_len + rhs_len;
  *(uint64_t *)res_inst_ptr = length;

  void *res_content_ptr = malloc((size_t)length);
  *((void **)(res_inst_ptr) + 1) = (void *)res_content_ptr;

  void *lhs_content_ptr = *(void **)((char *)lhs + 8);
  void *rhs_content_ptr = *(void **)((char *)rhs + 8);
  memcpy(res_content_ptr, lhs_content_ptr, (size_t)lhs_len);
  memcpy((char *)res_content_ptr + lhs_len, rhs_content_ptr, (size_t)rhs_len);
  return res_inst_ptr;
}

void *__string__substring(void *ptr, int64_t left, int64_t right) {
  void *res_inst_ptr = malloc(16);

  int len = right - left;
  *(int64_t *)(res_inst_ptr) = len;

  void *res_content_ptr = malloc((size_t)len);
  char *res_content_ptr_ptr = (char *)res_inst_ptr + 8;
  *(char **)res_content_ptr_ptr = (char *)res_content_ptr;

  void *src_content_ptr_ptr = (char *)ptr + 8;
  char *src_content_ptr = *(char **)(src_content_ptr_ptr) + left;
  memcpy(res_content_ptr, src_content_ptr, (size_t)len);
  return res_inst_ptr;
}

int64_t __string__ord(void *ptr, int64_t pos) {
  return (int64_t) * (*(char **)((char *)ptr + 8) + pos);
}

int64_t __string__parseInt(void *ptr) {
  uint64_t len = *(int64_t *)(ptr);
  char buffer[len + 1];
  buffer[len] = 0;
  void *content_ptr_ptr = (char *)ptr + 8;
  char *content_ptr = *(char **)content_ptr_ptr;
  memcpy(buffer, content_ptr, len);
  return strtol(buffer, NULL, 10);
}

void __string___ctor_(void *ptr) { *(uint64_t *)ptr = 0; }

int getInt() {
  char c = getchar();
  int neg = 0;
  while (c < '0' || c > '9') {
    if (c == '-')
      neg = 1;
    c = getchar();
  }
  int num = c - '0';
  c = getchar();
  while (c >= '0' && c <= '9') {
    num = num * 10 + c - '0';
    c = getchar();
  }
  if (neg)
    return -num;
  return num;
}

void print(void *ptr) {
  uint64_t length = *(uint64_t *)ptr;
  char **content_ptr_ptr = (char **)ptr + 1;
  char *content_ptr = *content_ptr_ptr;
  printf("%.*s", (int)length, content_ptr);
}

void *getString() {
  void *res_inst_ptr = malloc(16);
  char buffer[256];
  scanf("%256s", buffer);

  uint64_t len = strlen(buffer);
  *(int64_t *)res_inst_ptr = len;

  void *res_content_ptr = malloc(len);
  *((char **)(res_inst_ptr) + 1) = (char *)res_content_ptr;
  memcpy(res_content_ptr, buffer, len);

  return res_inst_ptr;
}

void *toString(int64_t val) {
  char buffer[64];
  sprintf(buffer, "%ld", val);

  void *res_inst_ptr = malloc(16);
  int64_t len = strlen(buffer);
  *(int64_t *)(res_inst_ptr) = len;
  void *content_ptr = malloc(len);
  *((char **)(res_inst_ptr) + 1) = (char *)content_ptr;
  memcpy(content_ptr, buffer, len);

  return res_inst_ptr;
}

void println(void *ptr) {
  print(ptr);
  printf("\n");
}
