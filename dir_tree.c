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

#include "dir_tree.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include <stdio.h>

static const char PATH_DELIM = '/';

static void free_nameslist(struct dirent **nameslist, size_t count) {
  size_t i = 0;
  while (i < count) {
    free(nameslist[i]);
    ++i;
  }
  free(nameslist);
}

/* may free path */
static int dir_tree_mfp(struct dir_tree **el, char *path) {
  struct stat s;
  if (stat(path, &s) != 0) {
    printf("stat failed: %s\n", path);
    return -1;
  }
  if ((s.st_mode & S_IFDIR) != 0) {
    /* is dir */
    struct dir_tree *dir = calloc(1, sizeof(*dir));
    if (dir == 0) {
      return -1;
    }

    struct dirent **nameslist;
    size_t path_len = strlen(path);
    while ((path_len >= 1) && (path[path_len - 1] == '/')) {
      --path_len;
    }
    size_t count = scandir(path, &nameslist, 0, &alphasort);
    size_t i = 2; /* should ignore "." and ".." */
    while (i < count) {
      struct dirent *name = nameslist[i];
      size_t name_len = strlen(name->d_name) + 1;
      const size_t new_len = path_len + sizeof(PATH_DELIM) + name_len;
      char *new_path = malloc(new_len);
      if (new_path == 0) {
        dir_tree_free(dir);
        free_nameslist(nameslist, count);
        return -1;
      }
      memcpy(new_path, path, path_len);
      char *tmp = new_path + path_len;
      memcpy(tmp, &PATH_DELIM, sizeof(PATH_DELIM));
      memcpy(tmp + sizeof(PATH_DELIM), name->d_name, name_len);

      struct dir_tree *child = 0;
      if (dir_tree_mfp(&child, new_path) != 0) {
        free(new_path);
        dir_tree_free(dir);
        free_nameslist(nameslist, count);
        return -1;
      }

      if (child != 0) {
        /* add child to contents */
        size_t new_size = dir->size + 1;
        struct dir_tree **tmp = realloc(dir->contents, new_size * sizeof(*tmp));
        if (tmp == 0) {
          dir_tree_free(child);
          dir_tree_free(dir);
          free_nameslist(nameslist, count);
          return -1;
        }
        tmp[dir->size] = child;
        dir->contents = tmp;
        dir->size = new_size;
      }
      ++i;
    }
    *el = dir;
    free(path);
    free_nameslist(nameslist, count);
  } else if ((s.st_mode & S_IFREG) != 0) {
    /* is file */
    struct dir_tree_file *file = malloc(sizeof(struct dir_tree_file));
    if (file == 0) {
      return -1;
    }
    file->dir.contents = 0;
    file->dir.size = 1;
    file->path = path;
    *el = &file->dir;
  }
  return 0;
}

int dir_tree(struct dir_tree **el, char *path) {
  size_t path_len = strlen(path) + 1;
  char *heap_path = malloc(path_len);
  memcpy(heap_path, path, path_len);
  if (heap_path == 0) {
    return -1;
  } else {
    int rc = dir_tree_mfp(el, heap_path);
    if (rc != 0) {
      free(heap_path);
      return rc;
    }
  }
  return 0;
}

int dir_tree_multi(struct dir_tree **el, char **paths, size_t paths_size) {
  struct dir_tree *false_root;
  false_root = malloc(sizeof(*false_root));
  if (false_root == 0) {
    return -1;
  }
  false_root->contents = malloc(sizeof(*false_root->contents) * paths_size);
  if (false_root->contents == 0) {
    free(false_root);
    return -1;
  }
  false_root->size = 0;
  while (false_root->size < paths_size) {
    if (dir_tree(&false_root->contents[false_root->size],
                 paths[false_root->size]) != 0) {
      dir_tree_free(false_root);
      return -1;
    }
    ++false_root->size;
  }
  *el = false_root;
  return 0;
}

void dir_tree_print(struct dir_tree *el) {
  if ((el->contents == 0) && (el->size == 1)) {
    if (((struct dir_tree_file *)el)->path != 0) {
      /* is file */
      printf("%s\n", ((struct dir_tree_file *)el)->path);
    }
  } else {
    /* is dir */
    size_t i = 0;
    while (i < el->size) {
      dir_tree_print(el->contents[i]);
      ++i;
    }
  }
}

void dir_tree_free(struct dir_tree *el) {
  if ((el->contents == 0) && (el->size == 1)) {
    /* is file */
    free(((struct dir_tree_file *)el)->path);
  } else if (el->contents != 0) {
    /* is dir */
    size_t i = 0;
    while (i < el->size) {
      dir_tree_free(el->contents[i]);
      ++i;
    }
    free(el->contents);
  }
  free(el);
}
