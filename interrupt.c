// struct interrupt_buffer *init_irr_buff() {
//     struct interrupt_buffer *irr_buff = malloc(sizeof(struct interrupt_buffer));
//     if (irr_buff == NULL) 
//         error("fail to create interrupt buffer\n");
//     irr_buff->head = 0;
//     irr_buff->end = 0;
//     irr_buff->count = 0;
//     irr_buff->max = 1000;
//     irr_buff->buff = malloc(sizeof(4 * 1000));
//     if (irr_buff->buff == NULL) {
//         free(irr_buff);
//         error("irr_buff is NULL");
//         return NULL;
//     }

//     return irr_buff;
// }

// int is_full(struct interrupt_buffer *buff) {
//     return buff->count == buff->max;
// } 

// int is_empty(struct interrupt_buffer *buff) {
//     return buff->count == 0;
// }

// void enqueue_irr(struct interrupt_buffer *irr_buff, int value) {
//     if (is_full(irr_buff)) return;
//     irr_buff->buff[irr_buff->end++] = value;
//     irr_buff->count++;
//     if (irr_buff->end == irr_buff->max)
//         irr_buff->end = 0;
// }

// void dequeque_irr(struct interrupt_buffer *irr_buff) {
//     if (is_empty(irr_buff)) return;
//     int value = irr_buff->buff[irr_buff->head++];
//     irr_buff->count--;
//     if (irr_buff->head == irr_buff->max)
//         irr_buff->head = 0;
// }
