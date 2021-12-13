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

#include "../bit_arr.h"

#include <assert.h>
#include <string.h>

int main() {
  char test_str_hex[] = "0x12345678910111213141516171819202";
  char test_str_oct[] = "0221505317044200421102305012426056140311002";
  char test_str_dec[] = "24197857200254328746765703854004736514";

  /* hex */
  unsigned char *arr = bit_arr_from_str(test_str_hex, 16);
  assert(arr != 0);
  char *str = bit_arr_to_str(arr, 16);
  printf("%s\n", str);
  assert(strcmp(str, test_str_hex) == 0);
  free(arr);
  free(str);

  /* oct */
  arr = bit_arr_from_str(test_str_oct, 16);
  assert(arr != 0);
  str = bit_arr_to_str(arr, 16);
  printf("%s\n", str);
  assert(strcmp(str, test_str_hex) == 0);
  free(arr);
  free(str);

  /* dec */
  arr = bit_arr_from_str(test_str_dec, 16);
  assert(arr != 0);
  str = bit_arr_to_str(arr, 16);
  printf("%s\n", str);
  assert(strcmp(str, test_str_hex) == 0);
  free(arr);
  free(str);

  return 0;
}
