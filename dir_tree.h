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

#ifndef DIR_TREE_H
#define DIR_TREE_H

#include <stdlib.h>

struct dir_tree {
  struct dir_tree **contents;
  size_t size;
};

struct dir_tree_file {
  struct dir_tree dir;
  char *path;
  void *usr;
};

int dir_tree(struct dir_tree **el, char *path);
int dir_tree_multi(struct dir_tree **el, char **paths, size_t paths_size);
void dir_tree_print(struct dir_tree *el);
void dir_tree_free(struct dir_tree *el);

#endif
