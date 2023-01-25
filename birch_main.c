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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "birch.h"
#include "dir_tree.h"

static const char HELP_STR[] =
    "Binary search with options for string, ints of any "
    "size, and standard C floats. All sizes and offsets in bits.\n"
    "Attempts to find the \"smallest\" collection of one match from each "
    "search group. Collections will not span multiple directory tree branches."
    "\n"
    "Usage: birch ROOTS... PATTERNS... [OPTIONS...]\n"
    "ROOTS: Pathnames at which to start the search, can be files or "
    "directories, "
    "if directories, a resursive search will be performed within.\n"
    "PATTENRS: Of the form: \"type size pattern\".\n"
    "type:\n"
    "\tf: float\n"
    "\ti: int\n"
    "\ts: string\n"
    "\ta: aligned\n"
    "\tu: unaligned\n"
    "\tl: little endian\n"
    "\tb: big endian\n"
    "\tn: native endian\n"
    "\tg: group with last\n"
    "Data type, alignment and endian can all be combined, further args "
    "maintain "
    "type settings from previous.\n"
    "Example: \"-ial 32 42 -gf 32 42\"\n"
    "a pattern group containing a 32 bit aligned little endian integer and a "
    "32 bit aligned little endian float.\n"
    "OPTIONS: \"-r\": number of results to print, default 1.\n";
static const unsigned int ENDIAN_TEST = 1;

struct roots {
  char **roots;
  size_t size;
};

static void *malloc_safe(size_t num, size_t size) {
  size_t alloc_size = num * size;
  if ((alloc_size / num) != size) {
    exit(-1);
  }
  void *p = malloc(alloc_size);
  if (p == 0) {
    exit(-1);
  }
  return p;
}

static void *realloc_safe(void *ptr, size_t num, size_t size) {
  size_t alloc_size = num * size;
  if ((alloc_size / num) != size) {
    exit(-1);
  }
  void *p = realloc(ptr, alloc_size);
  if (p == 0) {
    exit(-1);
  }
  return p;
}

static void *calloc_safe(size_t num, size_t size) {
  void *p = calloc(num, size);
  if (p == 0) {
    exit(-1);
  }
  return p;
}

static unsigned char *ptn_mask_gen(bit_size_t size, size_t size_bytes) {
  unsigned char *mask = malloc_safe(size_bytes, sizeof(*mask));
  size_t i = 0;
  while (i < (size_bytes - 1)) {
    mask[i] = -1;
    ++i;
  }

  mask[i] = (1 << (size % 8)) - 1;
  if (mask[i] == 0) {
    mask[i] = -1;
  }
  return mask;
}

/* returns a copy of the bit array shifted left 1 bit, extending the length by 1
  character if necessary */
static unsigned char *lshift_copy(unsigned char *arr, size_t size_bytes,
                                  size_t shifted_size_bytes) {
  if (shifted_size_bytes == 0) {
    return 0;
  }
  unsigned char *shifted = malloc_safe(shifted_size_bytes, sizeof(*shifted));
  unsigned int rshift = CHAR_BIT - 1;
  unsigned char prev = 0;
  size_t i = 0;
  while (i < size_bytes) {
    shifted[i] = (arr[i] << 1) | (prev >> rshift);
    prev = arr[i];
    ++i;
  }
  if (i < shifted_size_bytes) {
    shifted[i] = prev >> rshift;
  }

  return shifted;
}

/* takes a ptn, smears to CHAR_BIT ptns - the ptn shifted to all
 * possible character alignments */
static void ptn_unalign(struct birch_ptn unaligned[CHAR_BIT]) {
  unsigned int shift = 1;
  while (shift < CHAR_BIT) {
    struct birch_ptn *prev = &unaligned[shift - 1];
    struct birch_ptn *cur = &unaligned[shift];
    unsigned int new_offs = prev->offs + 1;
    bit_size_t new_size = prev->size + new_offs;
    size_t new_size_bytes = ((new_size - 1) / CHAR_BIT) + 1;
    cur->arg_str = prev->arg_str;
    cur->type = prev->type;
    cur->alignment = prev->alignment;
    cur->endian = prev->endian;
    cur->ptn = lshift_copy(prev->ptn, prev->size_bytes, new_size_bytes);
    cur->mask = lshift_copy(prev->mask, prev->size_bytes, new_size_bytes);
    cur->offs = new_offs;
    cur->size = prev->size;
    cur->size_bytes = new_size_bytes;
    ++shift;
  }
}

static void endian_reverse_common(unsigned char *arr, size_t size_bytes) {
  size_t i = 0;
  size_t j = size_bytes - 1;
  while (i < j) {
    unsigned char tmp = arr[j];
    arr[j] = arr[i];
    arr[i] = tmp;
    ++i;
    --j;
  }
}

static void endian_reverse(unsigned char *arr, bit_size_t size) {
  endian_reverse_common(arr, ((size - 1) / CHAR_BIT) + 1);
}

static unsigned char *endian_reverse_copy(unsigned char *arr, bit_size_t size) {
  size_t size_bytes = ((size - 1) / CHAR_BIT) + 1;
  unsigned char *copy = malloc_safe(size_bytes, sizeof(*copy));
  memcpy(copy, arr, size_bytes);
  endian_reverse_common(copy, size_bytes);
  return copy;
}

static int ptn_group_modify(struct birch_ptn *group, enum alignment alignment,
                            enum endian endian, enum endian type_endian) {
  unsigned int esl = 1;
  if (alignment == ALIGNMENT_UNALIGNED) {
    ptn_unalign(&group[0]);
    esl = CHAR_BIT;
  }
  if (endian == ENDIAN_BOTH) {
    unsigned int from = 0;
    while (from < esl) {
      unsigned int to = from + esl;
      group[to].ptn =
          endian_reverse_copy(group[from].ptn, group[from].size_bytes);
      group[to].mask =
          endian_reverse_copy(group[from].mask, group[from].size_bytes);
      group[to].offs = group[from].offs;
      group[to].size = group[from].size;
      group[to].size_bytes = group[from].size_bytes;
      ++from;
    }
  } else if (endian != type_endian) {
    unsigned int i = 0;
    while (i < esl) {
      endian_reverse(group[i].ptn, group[i].size_bytes);
      endian_reverse(group[i].mask, group[i].size_bytes);
      ++i;
    }
  }
  return 0;
}

static void ptn_group_free(struct birch_ptn_group *group) {
  size_t i = 0;
  while (i < group->size) {
    free(group->ptns[i].ptn);
    free(group->ptns[i].mask);
    ++i;
  }
  free(group->ptns);
}

static void ptn_groups_free(struct birch_ptn_groups *groups) {
  size_t i = 0;
  while (i < groups->size) {
    ptn_group_free(&groups->groups[i]);
    ++i;
  }
  free(groups->groups);
}

static int group_add_ptn(struct birch_ptn_group *group, char *arg_str,
                         enum data_type type, enum alignment alignment,
                         enum endian endian, bit_size_t size) {
  const enum endian ENDIAN_NATIVE =
      (*((char *)&ENDIAN_TEST) == 1) ? ENDIAN_LITTLE : ENDIAN_BIG;
  size_t prev_group_size = group->size;

  if ((endian == ENDIAN_BOTH) && (type != DATA_TYPE_STRING)) {
    if (alignment == ALIGNMENT_UNALIGNED) {
      group->size += CHAR_BIT << 1;
    } else {
      group->size += 2;
    }
  } else if (alignment == ALIGNMENT_UNALIGNED) {
    group->size += CHAR_BIT;
  } else {
    ++group->size;
  }

  struct birch_ptn *tmp = realloc_safe(group->ptns, group->size, sizeof(*tmp));
  group->ptns = tmp;

  struct birch_ptn *ptn = &tmp[prev_group_size];
  size_t size_bytes = (size + (CHAR_BIT - 1)) / CHAR_BIT;

  ptn->mask = ptn_mask_gen(size, size_bytes);
  if (ptn->mask == 0) {
    free(ptn->ptn);
    free(tmp);
    return -1;
  }
  ptn->offs = 0;
  ptn->size = size;
  ptn->size_bytes = size_bytes;
  ptn->arg_str = arg_str;
  ptn->type = type;
  ptn->alignment = alignment;
  ptn->endian = endian;

  switch (type) {
  case DATA_TYPE_INTEGER:
    ptn->ptn = bit_arr_from_str(arg_str, size_bytes);
    if (ptn->ptn == 0) {
      return -1;
    }
    ptn_group_modify(ptn, alignment, endian, ENDIAN_LITTLE);
    break;
  case DATA_TYPE_FLOAT: {
    char *end;
    char *expected_end = arg_str + strlen(arg_str);
    if (size == (sizeof(float) * CHAR_BIT)) {
      float f = strtof(arg_str, &end);
      ptn->ptn = malloc_safe(1, sizeof(f));
      memcpy(ptn->ptn, &f, sizeof(f));
    } else if (size == (sizeof(double) * CHAR_BIT)) {
      double d = strtod(arg_str, &end);
      ptn->ptn = malloc_safe(1, sizeof(d));
      memcpy(ptn->ptn, &d, sizeof(d));
    } else {
      return -2;
    }
    if (end != expected_end) {
      return -1;
    }
    ptn_group_modify(ptn, alignment, endian, ENDIAN_NATIVE);
    break;
  }
  case DATA_TYPE_STRING:
    if (strlen(arg_str) < ptn->size_bytes) {
      return -1;
    }
    /* ignore endian for strings */
    size_t arg_len = strlen(arg_str) + 1;
    ptn->ptn = malloc_safe(arg_len, sizeof(*ptn->ptn));
    strncpy((char *)ptn->ptn, arg_str, arg_len);
    if (alignment == ALIGNMENT_UNALIGNED) {
      ptn_unalign(ptn);
    }
    break;
  }

  return 0;
}

/* coppies just enough to be useful as a result */
static void result_from_groups(struct birch_ptn_groups *to,
                               struct birch_ptn_groups *from) {
  to->groups = calloc_safe(from->size, sizeof(*to->groups));
  to->size = from->size;
  memcpy(to->match_dist, from->match_dist, sizeof(from->match_dist));

  ++to->match_dist[0];

  size_t i = 0;
  while (i < from->size) {
    struct birch_ptn_group *group_to = &to->groups[i];
    struct birch_ptn_group *group_from = &from->groups[i];
    group_to->ptns = group_from->ptns;
    group_to->size = group_from->size;
    group_to->match.ptn = 0;
    group_to->match.path = 0;
    group_to->match.offs = 0;
    ++i;
  }
}

static size_t factorial(size_t n) {
  size_t r = 1;
  while (n > 0) {
    r *= n;
    --n;
  }
  return r;
}

static size_t combinations2(struct birch_ptn_groups *groups) {
  size_t size = groups->size;
  return (size < 2) ? 1 : factorial(size) / (factorial(size - 2) << 1);
}

static ssize_t parse_args(struct roots *roots, struct birch_ptn_groups *groups,
                          int argc, char *argv[]) {
  if (argc < 3) {
    printf("requires 2+ args\n");
    return -1;
  }

  const enum endian ENDIAN_NATIVE =
      (*((char *)&ENDIAN_TEST) == 1) ? ENDIAN_LITTLE : ENDIAN_BIG;

  roots->roots = 0;
  roots->size = 0;
  groups->groups = 0;
  groups->size = 0;
  unsigned char state = 0;
  enum alignment alignment = ALIGNMENT_ALIGNED;
  enum endian endian = ENDIAN_NATIVE;
  enum data_type data_type = DATA_TYPE_STRING;
  bit_size_t data_size = CHAR_BIT;
  unsigned char group_link = 0;
  size_t results_size = 1;

  int i = 1;
  while (i < argc) {
    char *arg = argv[i];
    if (arg[0] == '-') {
      if (state != 0) {
        printf("unexpected \"-\" arg %d/n", i);
      }
      unsigned char endian_set = 0;
      size_t j = 1;
      while (arg[j] != '\0') {
        switch (arg[j]) {
        case 'h':
          printf("%s", HELP_STR);
          break;
        case 'u':
          alignment = ALIGNMENT_UNALIGNED;
          state = 1;
          break;
        case 'a':
          alignment = ALIGNMENT_ALIGNED;
          state = 1;
          break;
        case 'l':
          if (endian_set == 0) {
            endian = ENDIAN_LITTLE;
          } else if (endian != ENDIAN_LITTLE) {
            endian = ENDIAN_BOTH;
          }
          endian_set = 1;
          state = 1;
          break;
        case 'b':
          if (endian_set == 0) {
            endian = ENDIAN_BIG;
          } else if (endian != ENDIAN_BIG) {
            endian = ENDIAN_BOTH;
          }
          endian_set = 1;
          state = 1;
          break;
        case 'n':
          if (endian_set == 0) {
            endian = ENDIAN_NATIVE;
          } else if (endian != ENDIAN_NATIVE) {
            endian = ENDIAN_BOTH;
          }
          endian_set = 1;
          state = 1;
          break;
        case 'i':
          data_type = DATA_TYPE_INTEGER;
          state = 1;
          break;
        case 's':
          data_type = DATA_TYPE_STRING;
          state = 1;
          break;
        case 'f':
          data_type = DATA_TYPE_FLOAT;
          state = 1;
          break;
        case 'g':
          group_link = 1;
          break;
        case 'r':
          state = 3;
          break;
        default:
          printf("unrecognised arg: %c/n", arg[j]);
          return -1;
        }
        ++j;
      }
    } else if (state == 0) {
      /* search root */
      int new_roots_size = roots->size + 1;
      char **tmp = realloc_safe(roots->roots, new_roots_size, sizeof(*tmp));
      roots->roots = tmp;
      roots->roots[roots->size] = arg;
      roots->size = new_roots_size;
    } else if (state == 1) {
      data_size = strtol(arg, 0, 0);
      state = 2;
    } else if (state == 3) {
      results_size = strtol(arg, 0, 0);
      state = 0;
    } else {
      /* search patterns */
      if ((group_link == 0) || (groups->size == 0)) {
        /* new_group */
        ++groups->size;
        struct birch_ptn_group *tmp =
            realloc_safe(groups->groups, groups->size, sizeof(*tmp));
        groups->groups = tmp;
        struct birch_ptn_group *new_group = &tmp[groups->size - 1];
        new_group->ptns = 0;
        new_group->size = 0;
        new_group->match.ptn = 0;
        new_group->match.path = 0;
        new_group->match.offs = 0;
      } else {
        group_link = 0;
      }

      if (group_add_ptn(&groups->groups[groups->size - 1], arg, data_type,
                        alignment, endian, data_size) != 0) {
        return -1;
      }
      state = 0;
    }
    ++i;
  }

  groups->match_dist[MATCH_NEXIST] = combinations2(groups);
  groups->match_dist[MATCH_DIR_DIFF] = 0;
  groups->match_dist[MATCH_FILE_DIFF] = 0;
  groups->match_dist[MATCH_OFFS_DIFF] = 0;

  return results_size;
}

/*
static void ptn_print(struct birch_ptn *ptn) {
  printf("ptn:\t");
  size_t i = ptn->size_bytes;
  while (i > 0) {
    --i;
    printf("%02x", ptn->ptn[i]);
  }
  printf("\nmask:\t");
  i = ptn->size_bytes;
  while (i > 0) {
    --i;
    printf("%02x", ptn->mask[i]);
  }
  printf("\n");
}

static void group_print(struct birch_ptn_group *group) {
  size_t i = 0;
  printf("group size %lu\n", group->size);
  while (i < group->size) {
    ptn_print(&group->ptns[i]);
    ++i;
  }
}

static void groups_print(struct birch_ptn_groups *groups) {
  size_t i = 0;
  while (i < groups->size) {
    group_print(&groups->groups[i]);
    ++i;
  }
}
*/

static const char *type_to_str(enum data_type type) {
  static const char ti[] = "i";
  static const char tf[] = "f";
  static const char ts[] = "s";
  static const char u[] = "";

  switch (type) {
  case DATA_TYPE_INTEGER:
    return ti;
  case DATA_TYPE_FLOAT:
    return tf;
  case DATA_TYPE_STRING:
    return ts;
  }
  return u;
}

static const char *alignment_to_str(enum alignment alignment) {
  static const char ua[] = "u";
  static const char al[] = "a";
  static const char u[] = "";

  switch (alignment) {
  case ALIGNMENT_UNALIGNED:
    return ua;
  case ALIGNMENT_ALIGNED:
    return al;
  }
  return u;
}

static const char *endian_to_str(enum endian endian) {
  static const char le[] = "l";
  static const char be[] = "b";
  static const char me[] = "lb";
  static const char u[] = "";

  switch (endian) {
  case ENDIAN_LITTLE:
    return le;
  case ENDIAN_BIG:
    return be;
  case ENDIAN_BOTH:
    return me;
  }
  return u;
}

static void match_print(struct birch_ptn_group *result) {
  if (result->match.ptn != 0) {
    struct birch_match *match = &result->match;
    struct birch_ptn *ptn = match->ptn;
    printf("\t%s %s%s%s %s 0x%llX\n", ptn->arg_str, type_to_str(ptn->type),
           alignment_to_str(ptn->alignment), endian_to_str(ptn->endian),
           match->path, match->offs);
  }
}

static void result_print(struct birch_ptn_groups *result) {
  size_t i = 0;
  while (i < result->size) {
    match_print(&result->groups[i]);
    ++i;
  }
}

static void results_print(struct birch_ptn_groups *results,
                          size_t results_size) {
  size_t nexist_max = combinations2(results);
  size_t i = 0;
  while (i < results_size) {
    struct birch_ptn_groups *result = &results[i];
    if (result->match_dist[MATCH_NEXIST] > nexist_max) {
      break;
    }

    printf("%lu: %lx %lx %lx %lx\n", i + 1, result->match_dist[MATCH_NEXIST],
           result->match_dist[MATCH_DIR_DIFF],
           result->match_dist[MATCH_FILE_DIFF],
           result->match_dist[MATCH_OFFS_DIFF]);
    result_print(result);
    ++i;
  }
}

static int dir_tree_search_file(struct birch_ptn_groups *results,
                                size_t results_size, struct dir_tree *el,
                                struct birch_ptn_groups *groups) {
  return ((el->contents == 0) && (el->size == 1))
             ? birch_file(results, results_size,
                          ((struct dir_tree_file *)el)->path, groups)
             : 0;
}

static int dir_tree_search_dir(struct birch_ptn_groups *results,
                               size_t results_size, struct dir_tree *el,
                               struct birch_ptn_groups *groups) {
  int rc = 0;
  if (el->contents != 0) {
    /* is dir */
    size_t i = 0;
    while (i < el->size) {
      rc = dir_tree_search_file(results, results_size, el->contents[i], groups);
      if (rc != 0) {
        return rc;
      }
      ++i;
    }

    i = 0;
    while (i < el->size) {
      rc = dir_tree_search_dir(results, results_size, el->contents[i], groups);
      if (rc != 0) {
        return rc;
      }
      ++i;
    }
  }
  return rc;
}

int main(int argc, char *argv[]) {
  struct roots roots;
  struct birch_ptn_groups groups;
  ssize_t results_size = parse_args(&roots, &groups, argc, argv);
  if (results_size <= 0) {
    free(roots.roots);
    ptn_groups_free(&groups);
    return -1;
  }

  if (roots.size == 0) {
    printf("At least one root path required\n");
    free(roots.roots);
    ptn_groups_free(&groups);
    return -1;
  }

  struct dir_tree *tree;
  if (dir_tree_multi(&tree, roots.roots, roots.size) != 0) {
    printf("File tree walk failed, roots:\n");
    size_t i = 0;
    while (i < roots.size) {
      printf("%s\n", roots.roots[i]);
      ++i;
    }
    free(roots.roots);
    ptn_groups_free(&groups);
    return -1;
  }

  free(roots.roots);

  /*
  dir_tree_print(tree);
  groups_print(&groups);
  */

  /* allocate and initialise results */
  struct birch_ptn_groups *results =
      malloc_safe(results_size, sizeof(*results));
  ssize_t i = 0;
  while (i < results_size) {
    result_from_groups(&results[i], &groups);
    ++i;
  }

  int r = -1;
  if (dir_tree_search_dir(results, results_size, tree, &groups) == 0) {
    results_print(results, results_size);
    r = 0;
  }

  i = 0;
  while (i < results_size) {
    free(results[i].groups);
    ++i;
  }
  ptn_groups_free(&groups);
  free(results);
  dir_tree_free(tree);

  return r;
}
