#include "dict.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h> 

// THIS CODE HAS BEEN CREATED BY SOMEONE ELSE AND WAS SOURCED FROM
// https://stackoverflow.com/questions/4384359/quick-way-to-implement-dictionary-in-c
// SLIGHT MODIFICATIONS HAVE BEEN MADE (MOSTLY DATA TYPES)

int dict_find_index(dict_ptr dict, char *key) {
    for (int i = 0; i < dict->len; i++) {
        if (!strcmp(dict->entry[i].key, key)) {
            return i;
        }
    }
    return -1;
}

void* dict_find(dict_ptr dict, char *key) {
    //printf("Searching dictionary for key: %s\n", key);
    int idx = dict_find_index(dict, key);
    return idx == -1 ? NULL : dict->entry[idx].element;
}

void dict_add(dict_ptr dict, char *key, void *element) {
   int idx = dict_find_index(dict, key);
   if (idx != -1) {
       dict->entry[idx].element = element;
       return;
   }
   if (dict->len == dict->cap) {
       dict->cap *= 2;
       dict->entry = realloc(dict->entry, dict->cap * sizeof(dict_entry));
   }
   strcpy(dict->entry[dict->len].key, key);
   dict->entry[dict->len].element = element;
   dict->len++;
   return;
}

dict_ptr dict_new(void) {
    dict proto = {0, 10, malloc(10 * sizeof(dict_entry))};
    dict_ptr d = malloc(sizeof(dict));
    *d = proto;
    return d;
}

void dict_free(dict_ptr dict) {
    //for (int i = 0; i < dict->len; i++) {
    //    free(dict->entry[i].key);
    //}
    free(dict->entry);
    free(dict);
}