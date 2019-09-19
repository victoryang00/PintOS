#ifndef __THREADS_FIXED_POINT_H
#define __THREADS_FIXED_POINT_H

typedef int fix_t;
#define FP_SHIFT_AMOUNT 16
#define FP(A) ((int)(A<<FP_SHIFT_AMOUNT))
#define FP_ADD(A,B) (A+B)
#define FP_ADD_INT(A,B) (A+(B<<FP_SHIFT_AMOUNT))
#define FP_SUB(A,B) (A-B)
#define FP_SUB_INT(A,B) (A-(B<<FP_SHIFT_AMOUNT))
#define FP_MUL(A,B) ((int)((((int64_t)A)*B)>>FP_SHIFT_AMOUNT))
#define FP_MUL_INT(A,B) (A*B)
#define FP_DIV(A,B) ((int)((((int64_t)A)<<FP_SHIFT_AMOUNT)/B))
#define FP_DIV_INT(A,B) (A/B)
#define FP_INT(A) (A>>FP_SHIFT_AMOUNT)
#define FP_ROUND(A) (A>=0? ((A+(1<<(FP_SHIFT_AMOUNT-1)))>>FP_SHIFT_AMOUNT) : ((A-(1<<(FP_SHIFT_AMOUNT-1)))>>FP_SHIFT_AMOUNT) )

#endif 
