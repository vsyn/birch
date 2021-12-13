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

#include "birch.h"

#include <stdio.h>
#include <string.h>

#define FILE_BUF_SIZE (1024 * 16)

static const char PATH_DELIM = '/';

static unsigned int path_delim_count(char *path) {
  unsigned int diff = 0;
  while (*path != '\0') {
    if (*path == PATH_DELIM) {
      ++diff;
    }
    ++path;
  }
  return diff;
}

static unsigned int path_dir_diff(char *patha, char *pathb) {
  while ((*patha == *pathb) && (*patha != '\0')) {
    ++patha;
    ++pathb;
  }
  return path_delim_count(patha) + path_delim_count(pathb);
}

static void ptn_group_match_dist_calc(unsigned long int dist[4],
                                      struct birch_match *lsa,
                                      struct birch_match *lsb) {
  if ((lsa->ptn == 0) || (lsb->ptn == 0)) {
    dist[MATCH_NEXIST] = 1;
    dist[MATCH_DIR_DIFF] = 0;
    dist[MATCH_FILE_DIFF] = 0;
    dist[MATCH_OFFS_DIFF] = 0;
  } else {
    dist[MATCH_NEXIST] = 0;
    dist[MATCH_DIR_DIFF] = path_dir_diff(lsa->path, lsb->path);
    dist[MATCH_FILE_DIFF] = (strcmp(lsa->path, lsb->path) != 0) ? 1 : 0;
    bit_size_t tmp = lsa->offs - lsb->offs;
    if (tmp > lsa->offs) {
      tmp = lsb->offs - lsa->offs;
    }
    dist[MATCH_OFFS_DIFF] = tmp;
  }
}

static void ptn_group_match_dist_update(struct birch_ptn_groups *groups,
                                        struct birch_match *match_old,
                                        struct birch_match *match_new) {
  if (groups->size == 1) {
    unsigned int j = 0;
    while (j < BIRCH_MATCH_DIST_SIZE) {
      groups->match_dist[j] = 0;
      ++j;
    }
    return;
  }
  size_t i = 0;
  while (i < groups->size) {
    struct birch_ptn_group *group = &groups->groups[i];
    struct birch_match *ls = &group->match;
    ++i;
    if (ls == match_old) {
      continue;
    }
    unsigned long int old_match_dist[BIRCH_MATCH_DIST_SIZE];
    unsigned long int new_match_dist[BIRCH_MATCH_DIST_SIZE];
    ptn_group_match_dist_calc(old_match_dist, ls, match_old);
    ptn_group_match_dist_calc(new_match_dist, ls, match_new);
    unsigned int j = 0;
    while (j < BIRCH_MATCH_DIST_SIZE) {
      groups->match_dist[j] += new_match_dist[j] - old_match_dist[j];
      ++j;
    }
  }
}

static char ptn_group_match_dist_cmp(unsigned long int match_dista[4],
                                     unsigned long int match_distb[4]) {
  unsigned int i = 0;
  while (i < BIRCH_MATCH_DIST_SIZE) {
    if (match_dista[i] != match_distb[i]) {
      return (match_dista[i] > match_distb[i]) ? 1 : -1;
    }
    ++i;
  }
  return 0;
}

static int ptn_match(struct birch_ptn *ptn, unsigned char c);

static void ptn_match_backtrack(struct birch_ptn *ptn, size_t count) {
  ptn->index = 0;
  size_t i = 1;
  while (i < count) {
    ptn_match(ptn, ptn->ptn[i]);
    ++i;
  }
}

static int ptn_match(struct birch_ptn *ptn, unsigned char c) {
  if ((c & ptn->mask[ptn->index]) == ptn->ptn[ptn->index]) {
    ++ptn->index;
    if (ptn->index == ptn->size_bytes) {
      ptn_match_backtrack(ptn, ptn->index);
      return 1;
    }
  } else if (ptn->index != 0) {
    ptn_match_backtrack(ptn, ptn->index);
    ptn_match(ptn, c);
  }
  return 0;
}

static void result_swap(struct birch_ptn_group *a, struct birch_ptn_group *b) {
  struct birch_match tmp = a->match;
  a->match = b->match;
  b->match = tmp;
}

/* groups must have the same topology */
static void results_swap(struct birch_ptn_groups *a,
                         struct birch_ptn_groups *b) {
  size_t i = 0;
  while (i < a->size) {
    result_swap(&a->groups[i], &b->groups[i]);
    ++i;
  }
  i = 0;
  while (i < BIRCH_MATCH_DIST_SIZE) {
    unsigned long int tmp = a->match_dist[i];
    a->match_dist[i] = b->match_dist[i];
    b->match_dist[i] = tmp;
    ++i;
  }
}

/* does not copy group, copys just match info */
static void results_cpy(struct birch_ptn_groups *to,
                        struct birch_ptn_groups *from) {
  size_t i = 0;
  while (i < to->size) {
    to->groups[i].match = from->groups[i].match;
    ++i;
  }
  memcpy(to->match_dist, from->match_dist, sizeof(from->match_dist));
}

/* Returns 1 if the groups share any non-null matches, otherwise 0 */
static unsigned char groups_cmp(struct birch_ptn_groups *a,
                                struct birch_ptn_groups *b) {
  size_t i = 0;
  while (i < a->size) {
    if ((a->groups[i].match.ptn != 0) &&
        (memcmp(&a->groups[i].match, &b->groups[i].match,
                sizeof(a->groups[i].match)) == 0)) {
      return 1;
    }
    ++i;
  }
  return 0;
}

static void result_add(struct birch_ptn_groups *groups,
                       struct birch_ptn_groups *results, size_t results_size) {
  /* look through results and check for any similar matches */
  unsigned char added = 0;
  size_t i = results_size;
  while (i > 0) {
    --i;
    if (groups_cmp(&results[i], groups) != 0) {
      /* match - replace if lower */
      if (ptn_group_match_dist_cmp(groups->match_dist, results[i].match_dist) <
          0) {
        results_cpy(&results[i], groups);
        added = 1;
        break;
      } else {
        return;
      }
    }
  }

  if (added == 0) {
    i = results_size - 1;
    /* replace highest if distance lower */
    if (ptn_group_match_dist_cmp(groups->match_dist, results[i].match_dist) <
        0) {
      results_cpy(&results[i], groups);
    }
  }
  size_t j = i - 1;
  while ((i > 0) && (ptn_group_match_dist_cmp(results[i].match_dist,
                                              results[j].match_dist) < 0)) {
    results_swap(&results[i], &results[j]);
    --i;
    --j;
  }
}

int birch_file(struct birch_ptn_groups *results, size_t results_size,
               char *path, struct birch_ptn_groups *groups) {
  FILE *fp = fopen(path, "rb");
  size_t group_index = 0;
  while (group_index < groups->size) {
    struct birch_ptn_group *group = &groups->groups[group_index];
    size_t ptn_index = 0;
    while (ptn_index < group->size) {
      struct birch_ptn *ptn = &group->ptns[ptn_index];
      ptn->index = 0;
      ++ptn_index;
    }
    ++group_index;
  }

  unsigned char buf[FILE_BUF_SIZE];
  size_t size_read;
  size_t file_index = 0;
  do {
    size_read = fread(&buf, sizeof(unsigned char), FILE_BUF_SIZE, fp);
    if (ferror(fp) != 0) {
      fclose(fp);
      return -1;
    }

    size_t buf_index = 0;
    while (buf_index < size_read) {
      group_index = 0;
      while (group_index < groups->size) {
        struct birch_ptn_group *group = &groups->groups[group_index];
        size_t ptn_index = 0;
        while (ptn_index < group->size) {
          struct birch_ptn *ptn = &group->ptns[ptn_index];
          if (ptn_match(ptn, buf[buf_index]) != 0) {
            struct birch_match match = {
                .ptn = ptn,
                .path = path,
                .offs = ((((file_index + buf_index) * CHAR_BIT) + ptn->offs) -
                         ptn->size) +
                        CHAR_BIT};
            ptn_group_match_dist_update(groups, &group->match, &match);
            group->match = match;
            /* ptn match */
            result_add(groups, results, results_size);
          }
          ++ptn_index;
        }
        ++group_index;
      }
      ++buf_index;
    }
    file_index += size_read;
  } while (size_read == FILE_BUF_SIZE);
  fclose(fp);
  return 0;
}
