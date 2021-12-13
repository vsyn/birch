/* Copyright 2021 Julian Ingram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bit_arr.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

/* index should start as size_bytes - 1 */
static unsigned char mul_10(unsigned char *arr, size_t index) {
  unsigned char cur = arr[index];
  unsigned char shift_1 = (cur << 1);
  unsigned char shift_3 = (cur << 3);
  unsigned char sum = 0;
  if (index > 0) {
    size_t prv_index = index - 1;
    unsigned char prv = arr[prv_index];
    shift_1 |= (prv >> (CHAR_BIT - 1));
    shift_3 |= (prv >> (CHAR_BIT - 3));
    sum = mul_10(arr, prv_index);
  }
  sum += shift_1;
  unsigned char cout = (sum < shift_1) ? 1 : 0;
  sum += shift_3;
  cout |= (sum < shift_3) ? 1 : 0;
  arr[index] = sum;
  return cout;
}

static void add_uchar(unsigned char *arr, size_t size_bytes, unsigned char c) {
  if (size_bytes == 0) {
    return;
  }
  arr[0] += c;
  if (size_bytes == 1) {
    return;
  }
  if (arr[0] < c) {
    size_t i = 0;
    do {
      ++i;
      arr[i] += 1;
    } while (arr[i] == 0);
  }
}

static void lshift(unsigned char *arr, size_t size_bytes, unsigned int shift) {
  if ((size_bytes == 0) || (shift == 0)) {
    return;
  }
  assert(shift < CHAR_BIT);
  unsigned int rshift = CHAR_BIT - shift;
  unsigned char prev = 0;
  size_t i = 0;
  while (i < size_bytes) {
    unsigned char cur = arr[i];
    arr[i] = (cur << shift) | (prev >> rshift);
    prev = cur;
    ++i;
  }
}

static unsigned char hex_to_nibble(char hex) {
  if ((hex >= '0') && (hex <= '9')) {
    return hex - '0';
  } else if ((hex >= 'a') && (hex <= 'f')) {
    return 0xa + (hex - 'a');
  } else if ((hex >= 'A') && (hex <= 'F')) {
    return 0xa + (hex - 'A');
  }
  return 0xff;
}

static unsigned char *from_str(char *s, size_t size_bytes, unsigned char shift,
                               unsigned char max_char) {
  unsigned char *arr = calloc(size_bytes, sizeof(unsigned char));
  if (arr == 0) {
    return 0;
  }
  while (*s != 0) {
    lshift(arr, size_bytes, shift);
    unsigned char v = hex_to_nibble(*s);
    if (v > max_char) {
      free(arr);
      return 0;
    }
    add_uchar(arr, size_bytes, v);
    ++s;
  }
  return arr;
}

static unsigned char *from_str_hex(char *s, size_t size_bytes) {
  return from_str(s, size_bytes, 4, 0xf);
}

static unsigned char *from_str_oct(char *s, size_t size_bytes) {
  return from_str(s, size_bytes, 3, 7);
}

static unsigned char *from_str_dec(char *s, size_t size_bytes) {
  unsigned char *arr = calloc(size_bytes, sizeof(unsigned char));
  if (arr == 0) {
    return 0;
  }
  while (*s != 0) {
    char *jkl = bit_arr_to_str(arr, size_bytes);
    free(jkl);
    mul_10(arr, size_bytes - 1);
    jkl = bit_arr_to_str(arr, size_bytes);
    free(jkl);
    unsigned char v = hex_to_nibble(*s);
    if (v > 0x9) {
      free(arr);
      return 0;
    }
    add_uchar(arr, size_bytes, v);
    ++s;
  }
  return arr;
}

char *bit_arr_to_str(unsigned char *arr, size_t size_bytes) {
  size_t tmp = size_bytes << 1;
  if (tmp < size_bytes) {
    return 0;
  }
  size_t alloc_size = tmp + 3;
  if (alloc_size < tmp) {
    return 0;
  }
  char *str = malloc(alloc_size);
  if (str == 0) {
    return 0;
  }
  snprintf(str, 3, "0x");
  char *cur = str;
  while (size_bytes > 0) {
    --size_bytes;
    cur += 2;
    snprintf(cur, 3, "%02X", arr[size_bytes]);
  }
  return str;
}

/* if the allocated size != expected size then free and return 0 */
unsigned char *bit_arr_from_str(char *str, size_t size_bytes) {
  if (str[0] == '0') {
    if ((str[1] == 'x') || (str[1] == 'X')) {
      return from_str_hex(&str[2], size_bytes);
    }
    return from_str_oct(&str[1], size_bytes);
  }
  return from_str_dec(str, size_bytes);
}
