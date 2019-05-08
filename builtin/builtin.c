#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void* __alloc(size_t sz) {
  return malloc(sz);
}

void *__string__add(void *lhs, void *rhs) {
  uint64_t lhs_len = *((int64_t *)lhs - 1);
  uint64_t rhs_len = *((int64_t *)rhs - 1);
  uint64_t length = lhs_len + rhs_len;

  char *res_inst_ptr = (char *)malloc(length + 8);
  *(int64_t *)res_inst_ptr = length;
  res_inst_ptr += 8;

  memcpy(res_inst_ptr, lhs, (size_t)lhs_len);
  memcpy((char *)res_inst_ptr + lhs_len, rhs, (size_t)rhs_len);
  return res_inst_ptr;
}

void *__string__substring(void *ptr, int64_t left, int64_t right) {
  int len = right - left + 1;
  char *res_inst_ptr = (char *)malloc(len + 8);
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

int64_t __string__equal(void *lhs, void *rhs) {
  uint64_t lhs_len = *((int64_t *)lhs - 1);
  uint64_t rhs_len = *((int64_t *)rhs - 1);
  if (lhs_len != rhs_len)
    return 0;
  int res = memcmp(lhs, rhs, lhs_len);
  return !res;
}

int64_t __string__inequal(void *lhs, void *rhs) {
  return !__string__equal(lhs, rhs);
}

int64_t __string__less(void *lhs, void *rhs) {
  uint64_t lhs_len = *((int64_t *)lhs - 1);
  uint64_t rhs_len = *((int64_t *)rhs - 1);
  uint64_t len = lhs_len < rhs_len ? lhs_len : rhs_len;
  int res = memcmp(lhs, rhs, len);
  if (res == 0)
    return lhs_len < rhs_len ? 1 : 0;
  return res < 0;
}

int64_t __string__less_equal(void *lhs, void *rhs) {
  return __string__equal(lhs, rhs) || __string__less(lhs, rhs);
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
  char *res_inst_ptr = (char *)malloc(8 + len);
  *(int64_t *)res_inst_ptr = len;
  res_inst_ptr += 8;
  memcpy(res_inst_ptr, buffer, len);
  return res_inst_ptr;
}

void *toString(int64_t val) {
  char buffer[64];
  sprintf(buffer, "%ld", val);
  int64_t len = strlen(buffer);

  char *res_inst_ptr = (char *)malloc(8 + len);
  *(int64_t *)(res_inst_ptr) = len;
  res_inst_ptr += 8;
  memcpy(res_inst_ptr, buffer, len);
  return res_inst_ptr;
}

void println(void *ptr) {
  print(ptr);
  printf("\n");
}
