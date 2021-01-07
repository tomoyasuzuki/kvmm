#include "interrupt.h"

void enq_irr(struct irr_queue *irr, int value) {
    irr->buff[irr->last] = value;
    irr->last++;
}

int deq_irr(struct irr_queue *irr) {
   int out = irr->buff[0];
   for (int i = 0; i <= irr->last; i++) {
       irr->buff[i] = irr->buff[i+1];
   }
   irr->last--;
}   