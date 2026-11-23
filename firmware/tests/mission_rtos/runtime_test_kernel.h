#ifndef RUNTIME_TEST_KERNEL_H
#define RUNTIME_TEST_KERNEL_H
#include <stdint.h>
#define MIN_SYS_STACK_SIZE 128
#define DEFAULT_SYS_STKSZ 128
#define TA_HLNG 1u
#define TA_RNG3 2u
#define TTS_DMT 16u
#define E_OK 0
typedef int ID;
typedef int ER;
typedef struct {void *exinf;unsigned tskatr;void (*task)(int,void *);int itskpri,stksz;void *bufptr;} T_CTSK;
typedef struct {unsigned tskstat;int stksz;void *istack;} TD_RTSK;
ID tk_cre_tsk(const T_CTSK *);
ER td_ref_tsk(ID,TD_RTSK *);
ER tk_sta_tsk(ID,int);
ER tk_del_tsk(ID);
#endif
