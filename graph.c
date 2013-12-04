#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "graph.h"

#define SIZE 7

/*  priority queue   */
void init_pq(struct pq_t **pq, int max_size){
    (*pq) = (struct pq_t *)calloc(1, sizeof(struct pq_t));
    
    (*pq)->array = (struct node_t **)calloc(max_size+1, sizeof(struct node_t*));
    (*pq)->max_size = max_size;
    (*pq)->size = 0;

    (*pq)->array[0] = NULL; // 0-th node is used as sentinal, but not in this code
}


int push_graph(struct pq_t *pq, struct node_t *node, int *dist) {
    assert(pq != NULL);
    assert(node != NULL);
    assert(dist != NULL);
    
    int ind;

    if (pq->size == pq->max_size) {
	printf("Warning! pq is already full\n");
	return -1;
    }

    ind = pq->size+1; // attempted position    
    while (ind > 1 && dist[pq->array[ind/2]->id] > dist[node->id]) {
	pq->array[ind] = pq->array[ind/2];
	ind = ind/2;
    }

    pq->array[ind] = node;
    pq->size++;

    return 0;
}

struct node_t *pop_graph(struct pq_t *pq, int *dist) {
    assert(pq != NULL);
    assert(pq->size > 0);

    struct node_t *ret = NULL;
    struct node_t *last = NULL;
    int i, child;

    ret = pq->array[1];

    // restore, by percolating down
    last = pq->array[pq->size];
    pq->array[pq->size] = NULL;
    pq->size--;

    for (i = 1; i <= pq->size/2; i = child) {
	child = 2*i;
	//if (child < heap->size && pq->array[child+1]->dist < pq->array[child]->dist)
	if (child < pq->size && dist[pq->array[child+1]->id] < dist[pq->array[child]->id])
	    ++child;

	//if (pq->array[child]->dist < last->dist)
	if (dist[pq->array[child]->id] < dist[last->id])
	    pq->array[i] = pq->array[child];
	else
	    break;
    }
    pq->array[i] = last;

    return ret;
}

int is_empty(struct pq_t *pq) {
    if (pq->size > 0)
	return 0;
    else
	return 1;
}

// priority queue done


int dijkstra(int graph[][SIZE], int *visited, int *dist, int size, int s_id, int t_id){
    
    struct node_t s;
    struct pq_t *pq = NULL;
    struct node_t *node = NULL;
    struct node_t *new_node = NULL;
    int i, new_dist;

    s.id = s_id;
    s.prev = -1;

    init_pq(&pq, size*size);
    push_graph(pq, &s, dist);

    while (!is_empty(pq)) {
	node = pop_graph(pq, dist);

	assert(visited[node->id] == 0); // 
	visited[node->id] = 1;

	if (node->id == t_id) {
	    printf("found node_%d\n", t_id);
	    return dist[node->id];
	}
	
	// check all unvisited neighbors of node
	for (i = 0; i < size; i++) {
	    if (graph[node->id][i] > 0) {
		
		if (visited[i]) 
		    continue;
		
		new_dist = dist[node->id] + graph[node->id][i];
		if (new_dist >= dist[i]) 
		    continue;
		
		dist[i] = new_dist;
		printf("checking node_%d, ", node->id);
		printf("new dist[%d]=%d, ", i, new_dist);
		    
		new_node = (struct node_t *)calloc(1, sizeof(struct node_t));
		new_node->id = i;
		new_node->prev = node->id;
		
		push_graph(pq, new_node, dist);

		int j;
		printf("in qp, node_id:dist are ");
		for (j = 0; j < size; j++) {
		    if (pq->array[j] != NULL)
			printf("%d:%d ", pq->array[j]->id, dist[pq->array[j]->id]);
		}
		printf("\n");
	    }
	}
    }

    return -1;

}


#ifdef _TEST_GRAPH_

#define MAX_DIST 1024

int main(){
    
    int i; 
    int graph[][SIZE] = {{0,2,-1,1,-1,-1,-1},
			     {-1,0,-1,3,10,-1,-1},
			     {4,-1,0,-1,-1,5,-1},
			     {-1,-1,2,0,2,8,4},
			     {-1,-1,-1,-1,0,-1,6},
			     {-1,-1,-1,-1,-1,0,-1},
			     {-1,-1,-1,-1,-1,1,0}};

    int visited[SIZE] = {0,0,0,0,0,0,0};
    int dist[SIZE] = {MAX_DIST, MAX_DIST, MAX_DIST, MAX_DIST, MAX_DIST, MAX_DIST, MAX_DIST};
    dist[0] = 0;
		
    int dist_s;

    dist_s = dijkstra(graph, visited, dist, SIZE, 0, 6);
    printf("shortest distance:%d\n", dist_s);

}


#endif

#ifdef _TEST_PQ_

int main(){

    struct node_t node0, node1, node2, node3, node4;
    int dist[] = {4,3,2,1,0};

    node0.id = 0;
    node1.id = 1;
    node2.id = 2;
    node3.id = 3;
    node4.id = 4;

    struct pq_t *pq = NULL;
    init_pq(&pq, 10);
    
    push(pq, &node0, dist);
    push(pq, &node1, dist);
    push(pq, &node2, dist);
    push(pq, &node3, dist);
    push(pq, &node4, dist);

    int i;
    for (i = 0; i < 5; i++) {
	printf("%d ", dist[pq->array[i+1]->id]);
    }

    printf("\n");

}

#endif
