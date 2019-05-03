#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *__alloc(size_t sz) {
  if (sz >= 128)
    return malloc(sz);

  const size_t CHUNK_SIZE = 2048;
  static char *storage = NULL;
  static size_t used = CHUNK_SIZE;
  if (used + sz > CHUNK_SIZE) {
    storage = malloc(CHUNK_SIZE);
    used = 0;
  }

  void *res = storage + used;
  used += sz;
  return res;
}

void *__string__add(void *lhs, void *rhs) {
  uint64_t lhs_len = *((int64_t *)lhs - 1);
  uint64_t rhs_len = *((int64_t *)rhs - 1);
  uint64_t length = lhs_len + rhs_len;

  char *res_inst_ptr = (char *)__alloc(length + 8);
  *(int64_t *)res_inst_ptr = length;
  res_inst_ptr += 8;

  memcpy(res_inst_ptr, lhs, (size_t)lhs_len);
  memcpy((char *)res_inst_ptr + lhs_len, rhs, (size_t)rhs_len);
  return res_inst_ptr;
}

void *__string__substring(void *ptr, int64_t left, int64_t right) {
  int len = right - left;
  char *res_inst_ptr = (char *)__alloc(len + 8);
  *(int64_t *)(res_inst_ptr) = len;
  res_inst_ptr += 8;

  char *src_content_ptr = (char *)ptr + left;
  memcpy(res_inst_ptr, src_content_ptr, (size_t)len);
  return res_inst_ptr;
}

int64_t __string__ord(void *ptr, int64_t pos) {
  return (int64_t) * ((char *)ptr + pos);
}

int64_t __string__parseInt(void *ptr) {
  uint64_t len = *((int64_t *)ptr - 1);
  char buffer[256];
  buffer[len] = 0;
  memcpy(buffer, ptr, len);
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
  uint64_t len = *((int64_t *)ptr - 1);
  printf("%.*s", (int)len, (char *)ptr);
}

void _printInt(int num) {
  if (num == 0)
    putchar('0');
  if (num < 0) {
    num = -num;
    putchar('-');
  }
  int digits[10], len = 0;
  while (num > 0) {
    digits[len++] = num % 10;
    num /= 10;
  }
  for (int i = len - 1; i >= 0; --i)
    putchar('0' + digits[i]);
}

void _printlnInt(int x) {
  _printInt(x);
  putchar('\n');
}

void *getString() {
  char buffer[256];
  scanf("%256s", buffer);

  uint64_t len = strlen(buffer);
  char *res_inst_ptr = (char *)__alloc(8 + len);
  *(int64_t *)res_inst_ptr = len;
  res_inst_ptr += 8;
  memcpy(res_inst_ptr, buffer, len);
  return res_inst_ptr;
}

void *toString(int64_t val) {
  char buffer[64];
  sprintf(buffer, "%ld", val);
  int64_t len = strlen(buffer);

  char *res_inst_ptr = (char *)__alloc(8 + len);
  *(int64_t *)(res_inst_ptr) = len;
  res_inst_ptr += 8;
  memcpy(res_inst_ptr, buffer, len);
  return res_inst_ptr;
}

void println(void *ptr) {
  print(ptr);
  printf("\n");
}
