#ifndef DICT_H
#define DICT_H

typedef struct dict_entry {
    const char *key;
    void *element;
} dict_entry;

typedef struct dict {
    int len;
    int cap;
    dict_entry *entry;
} dict, *dict_ptr;

int dict_find_index(dict_ptr dict, const char *key);

void* dict_find(dict_ptr dict, const char *key);

void dict_add(dict_ptr dict, const char *key, void *element);

dict_ptr dict_new(void);

void dict_free(dict_ptr dict);

#endif