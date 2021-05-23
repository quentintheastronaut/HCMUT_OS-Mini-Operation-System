#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	if (q->size == MAX_QUEUE_SIZE) return;
	q->proc[q->size++] = proc;
}
struct pcb_t * dequeue(struct queue_t * q) {
	
	if(empty(q))
		return NULL;
	int highest_priority= q->proc[0]->priority;
	int highest_index=0;
	for (int i = 1; i < q->size; i++){
		if (q->proc[i]->priority > highest_priority){
			highest_priority = q->proc[i]->priority;
			highest_index = i;
		}
	}
	struct pcb_t * result = q->proc[highest_index];
	for (int i=highest_index+1; i < q->size; i++) {
		q->proc[i-1] = q->proc[i];
	}
	q->size--;
	return result;
}