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

#ifndef BIRCH_H
#define BIRCH_H

#include "bit_arr.h"

#include <limits.h>
#include <stdlib.h>

#define BIRCH_MATCH_DIST_SIZE (4)

enum endian { ENDIAN_LITTLE, ENDIAN_BIG, ENDIAN_BOTH };

enum alignment { ALIGNMENT_UNALIGNED, ALIGNMENT_ALIGNED };

enum data_type { DATA_TYPE_INTEGER, DATA_TYPE_FLOAT, DATA_TYPE_STRING };

enum match_dist_indices {
  MATCH_NEXIST,
  MATCH_DIR_DIFF,
  MATCH_FILE_DIFF,
  MATCH_OFFS_DIFF
};

struct birch_ptn {
  char *arg_str;
  enum data_type type;
  enum alignment alignment;
  enum endian endian;
  unsigned char *ptn;
  unsigned char *mask;
  unsigned int offs; /* bits until pattern starts, assumed to be < CHAR_BIT */
  bit_size_t size;   /* does not include offs */
  size_t index;      /* used in the match process */
  size_t size_bytes;
};

struct birch_match {
  struct birch_ptn *ptn;
  char *path;
  bit_size_t offs;
};

struct birch_ptn_group {
  struct birch_ptn *ptns;
  size_t size;
  struct birch_match match;
};

struct birch_ptn_groups {
  struct birch_ptn_group *groups;
  size_t size;
  unsigned long int match_dist[BIRCH_MATCH_DIST_SIZE];
};

int birch_file(struct birch_ptn_groups *results, size_t results_size,
               char *path, struct birch_ptn_groups *groups);

#endif
