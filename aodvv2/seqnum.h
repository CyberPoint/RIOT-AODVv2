#include <stdint.h>

#ifndef SEQNUM_H_
#define SEQNUM_H_

void seqNum_init(void);
uint16_t seqNum_get(void);
void seqNum_inc(void);
int seqnum_cmp(uint16_t s1, uint16_t s2);

#endif /* SEQNUM_H_ */
