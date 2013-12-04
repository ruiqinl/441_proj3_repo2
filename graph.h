#ifndef _GRAPH_H_
#define _GRAPH_H_


struct node_t {
    int id;
    //int distance; // isolate this
    //int is_visited;// isolate this into a table
    int prev;
};


struct pq_t {
    struct node_t **array;
    int size;
    int max_size;
};

void init_pq(struct pq_t **pq, int max_size);
int push_graph(struct pq_t *pq, struct node_t *node, int *dist);
struct node_t *pop_graph(struct pq_t *pq, int *dist);
int is_empty(struct pq_t *pq);

int dijkstra(int graph[][7], int *visited, int *dist, int size, int s_id, int t_id);

#endif
