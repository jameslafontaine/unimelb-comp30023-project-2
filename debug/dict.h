#ifndef DICT_H
#define DICT_H

#define MAX_KEY_LEN 1001

typedef struct dict_entry {
    char key[MAX_KEY_LEN];
    void *element;
} dict_entry;

typedef struct dict {
    int len;
    int cap;
    dict_entry *entry;
} dict, *dict_ptr;

int dict_find_index(dict_ptr dict, char *key);

void* dict_find(dict_ptr dict, char *key);

void dict_add(dict_ptr dict, char *key, void *element);

dict_ptr dict_new(void);

void dict_free(dict_ptr dict);

#endif