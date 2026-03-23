#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char u8;
typedef signed long long i64;
typedef unsigned long long u64;
typedef unsigned long u32;

const static u64 DEFAULT_ARENA_SIZE = 2 << 18;

const static u64 DEFAULT_ALIGNMENT = 2 * sizeof(void *);
const static bool USE_ALIGNED = false;

typedef struct{
  u8 * buffer;
  u64 capacity;
  u64 head;
} arena_allocator;

void arena_setup(arena_allocator * a, const i64 total) {
  assert(total > 0);
  a->capacity = total * sizeof(u8);
  a->buffer = (u8 *)malloc(a->capacity);
  a->head = 0;
}

void * arena_allocate_unaligned(arena_allocator * a, const u64 size) {
  void * alloc = a->buffer + a->head;
  a->head += size;
  assert(a->head < a->capacity && "Error: allocating above capacity.");
  return alloc;
}

void * arena_allocate_aligned(arena_allocator * a, const u64 size) {
  void * alloc = a->buffer + a->head;
  u64 offset = (u64)(alloc) & (DEFAULT_ALIGNMENT - 1);
  if (offset != 0)
    offset = DEFAULT_ALIGNMENT - offset;

  alloc += offset;
  a->head += size + offset;
  assert(a->head < a->capacity && "Error: allocating above capacity.");

  return alloc;
}

void * arena_alloc(arena_allocator * a, const u64 size) {
  if (USE_ALIGNED) {
    return arena_allocate_aligned(a, size);
  } else {
    return arena_allocate_unaligned(a, size);
  }
}

void arena_destroy(arena_allocator * a) {
  free(a->buffer);
}

// Testing with an ordered map

typedef struct node node;
struct node {
  i64 key;
  i64 value;
  node * left;
  node * right;
};

typedef struct ordered_map ordered_map;
struct ordered_map {
  arena_allocator alloc;
  node * top;
};

void ordered_map_setup(ordered_map * om, const u64 size) {
  arena_setup(&om->alloc, size);
  om->top = NULL;
}

void ordered_map_destroy(ordered_map * om) {
  arena_destroy(&om->alloc);
}

void node_setup(node * n, arena_allocator * a, i64 key, i64 value) {
  n->key = key;
  n->value = value;
  n->left = NULL;
  n->right = NULL;
}

void ordered_map_insert(ordered_map * om, i64 key, i64 value) {
  if (om->top == NULL) {
    om->top = (node *)arena_alloc(&om->alloc, sizeof(node));
    node_setup(om->top, &om->alloc, key, value);
    return;
  }

  node * last_node = NULL;
  node * current_node = om->top;
  bool right = false;

  while (current_node != NULL) {
    last_node = current_node;

    if (key == current_node->key) {
      current_node->value = value;
      return;
    } else if (key < current_node->key) {
      current_node = current_node->left;
      right = false;
    } else {
      current_node = current_node->right;
      right = true;
    }
  }

  if (current_node == NULL) {
    if (right) {
      last_node->right = (node *)arena_alloc(&om->alloc, sizeof(node));
      node_setup(last_node->right, &om->alloc, key, value);
    } else {
      last_node->left = (node *)arena_alloc(&om->alloc, sizeof(node));
      node_setup(last_node->left, &om->alloc, key, value);
    }
  }
}

i64 ordered_map_search(ordered_map * om, i64 key) {
  if (om->top == NULL) {
    printf("Error: key %lld does not exist.\n", key);
    exit(1);
  }

  node * current = om->top;

  while (current != NULL) {
    if (current->key == key) {
      return current->value;
    }

    if (key < current->key) {
      current = current->left;
    } else {
      current = current->right;
    }
  }

  printf("Error: key %lld does not exist.\n", key);
  exit(1);
}

// testing alignment with strings
typedef struct string string;
struct string {
  u8 * data;
  u64 size;
};

typedef struct strings strings;
typedef struct strings_node strings_node;

struct strings_node {
  string * str;
  strings_node * next;
};

struct strings {
  arena_allocator a;
  strings_node * head;
};

string * string_copy(arena_allocator * a, const char * src, const u64 len) {
  string * new_string = arena_alloc(a, sizeof(string));
  new_string->size = len;
  new_string->data = (u8 *)arena_alloc(a, len);
  memcpy(new_string->data, src, len);
  return new_string;
}

void strings_copy(arena_allocator * a, strings_node ** dest, const char * src, const u64 len) {
  assert(*dest == NULL && "Cannot allocate new string at non-NULL");

  *dest = (strings_node *)arena_alloc(a, sizeof(strings_node));
  (*dest)->next = NULL;
  (*dest)->str = string_copy(a, src, len);
}

bool string_equal(string * a, const char * b, const u64 len) {
  if (a->size != len) {
    return false;
  }

  for (u64 i = 0; i < len; i++) {
    if (a->data[i] != b[i]) return false;
  }
  return true;
}

void strings_setup(strings * strs, const u64 database_size) {
  arena_setup(&strs->a, database_size);
  strs->head = NULL;
}

void strings_destroy(strings * strs) {
  arena_destroy(&strs->a);
}

void strings_insert(strings * strs, char * cstr) {
  u64 len = strlen(cstr);
  if (len == 0) return;

  strings_node ** current = &strs->head;
  while (*current != NULL) {
    current = &(*current)->next;
  }

  strings_copy(&strs->a, current, cstr, len);
}

bool strings_exists(strings * const strs, const char * cstr) {
  u64 len = strlen(cstr);
  if (len == 0) return false;

  strings_node * last = NULL;
  strings_node * current = strs->head;

  while (current != NULL) {
    if (string_equal(current->str, cstr, len)) return true;
    last = current;
    current = current->next;
  }
  return false;
}

// tests

i64 test_offset(i64 size, i64 i) {
    return i + (i % 2 == 0 ? -size : size);
}

void test_ordered_map_search(const u64 num_keys) {
  ordered_map om;

  u64 size = num_keys > 0 ? num_keys * sizeof(node) + sizeof(node) : DEFAULT_ARENA_SIZE;
  ordered_map_setup(&om, size);
  // ordered_map_setup(&om, DEFAULT_ARENA_SIZE);

  for (i64 i = 0; i < num_keys / 2; i++) {
    i64 key = test_offset(num_keys, i) + i;
    i64 value = key + 1;
    ordered_map_insert(&om, key, value);
  }

  for (i64 i = num_keys / 2 - 1; i >= 0; i--) {
    i64 key = test_offset(-num_keys * 2, i) + i;
    i64 value = key + 1;
    ordered_map_insert(&om, key, value);
  }

  for (i64 i = 0; i < num_keys / 2; i++) {
    i64 key = test_offset(num_keys, i) + i;
    i64 value = ordered_map_search(&om, key);
    assert(value == key + 1);
  }
  for (i64 i = num_keys / 2 - 1; i >= 0; i--) {
    i64 key = test_offset(-num_keys * 2, i) + i;
    i64 value = ordered_map_search(&om, key);
    assert(value == key + 1);
  }

  ordered_map_destroy(&om);
}

void test_strings_find(const char * path) {
  FILE * f = fopen(path, "r");

  if (!f) {
    printf("Error: could not read file %s\n", path);
    exit(1);
  }

  char * buffer = NULL;
  u32 size = 0;
  u64 num_strs = 0;

  strings database = {0};
  strings_setup(&database, 2 << 20);

  while (getline(&buffer, &size, f) != -1) {
    if (num_strs % 2 == 0)
      strings_insert(&database, buffer);
    num_strs++;

    free(buffer);
    buffer = NULL;
    size = 0;
  }

  free(buffer);
  fclose(f);

  f = fopen(path, "r");

  if (!f) {
    printf("Error: could not read file %s\n", path);
    exit(1);
  }

  buffer = NULL;
  size = 0;
  u64 counter = 0;

  string * top = database.head->str;
  while (getline(&buffer, &size, f) != -1) {
    bool find = strings_exists(&database, buffer);
    if (counter % 2 == 0) {
      assert(find && "Error: didn't find a string");
    } else {
      assert(!find && "Error: shouldn't have found a string.");
    }

    counter++;

    free(buffer);
    buffer = NULL;
    size = 0;
  }

  free(buffer);
  fclose(f);

  strings_destroy(&database);
}

int main() {
  // ordered map tests
  test_ordered_map_search(0);
  test_ordered_map_search(1);
  test_ordered_map_search(2);
  test_ordered_map_search(11);
  test_ordered_map_search(101);
  test_ordered_map_search(1001);
  test_ordered_map_search(10001);
  // string tests
  // test_strings_find("./data/names.txt");
  return 0;
}
