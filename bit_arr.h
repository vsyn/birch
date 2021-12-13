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

#ifndef BIT_ARR_H
#define BIT_ARR_H

#include <stdlib.h>

typedef unsigned long long int bit_size_t;

char *bit_arr_to_str(unsigned char *arr, size_t size_bytes);
unsigned char *bit_arr_from_str(char *str, size_t size_bytes);

#endif
