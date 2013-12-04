#ifndef _LIST_H_
#define _LIST_H_

struct list_node_t {
  void *data;
  struct list_node_t *next;
};

//int init_list(struct list_node_t **list);
int push(struct list_node_t **list, void *data);
struct list_node_t *pop(struct list_node_t **list);
int print_list(struct list_node_t *list, void (*printer)(void *data));

void printer_int(void *d);
void printer_str(void *s);

int list_ind(struct list_node_t *list, void *data, int (*comparor)(void *d1, void* d2));
struct list_node_t *list_node(struct list_node_t *list, int ind);

int comparor_str(void *s, void *t);
int comparor_int(void *a, void *b);

int list_size(struct list_node_t *list);


#endif
