#pragma once

struct irr_queue {
    int buff[1000];
    int last;
};

void enq_irr(struct irr_queue *irr, int value);
int deq_irr(struct irr_queue *irr);