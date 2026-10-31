#ifndef LINE_CHECK_H
#define LINE_CHECK_H
#include <stdbool.h>
#include <stdint.h>
/* Diagnostic only. Two white readings cannot distinguish a centred narrow
 * line from absent track. Bound that ambiguity by time and wheel travel. */
#define LINE_CHECK_LIMIT_US 2000000u
#define LINE_CHECK_LIMIT_COUNTS 3537
#define IR_BLACK_LEVEL 250
#define IR_BARCODE_CHANNEL 0
#define IR_LINE_LEFT_CHANNEL 1
#define IR_LINE_RIGHT_CHANNEL 2
#define LINE_CORRECT_INNER_PWM 0.25f
#define LINE_CORRECT_OUTER_PWM 0.55f
typedef struct { bool barcode, left, right; } ir_roles_t;
static inline ir_roles_t ir_roles(const int32_t levels[3])
{
    return (ir_roles_t){levels[IR_BARCODE_CHANNEL]>=IR_BLACK_LEVEL,
        levels[IR_LINE_LEFT_CHANNEL]>=IR_BLACK_LEVEL,
        levels[IR_LINE_RIGHT_CHANNEL]>=IR_BLACK_LEVEL};
}
typedef struct { float left, right; bool stop; const char *state; } line_check_output_t;
static inline line_check_output_t line_check_step(ir_roles_t ir, uint64_t elapsed,
                                                 int32_t left, int32_t right,
                                                 bool encoder_invalid)
{
    if(encoder_invalid) return (line_check_output_t){0,0,true,"LINE_ENCODER_FAULT"};
    if(left < -64 || right < -64) return (line_check_output_t){0,0,true,"LINE_WRONG_DIRECTION"};
    if(elapsed>=LINE_CHECK_LIMIT_US) return (line_check_output_t){0,0,true,"LINE_TIME_LIMIT"};
    if(left>=LINE_CHECK_LIMIT_COUNTS || right>=LINE_CHECK_LIMIT_COUNTS)
        return (line_check_output_t){0,0,true,"LINE_TRAVEL_LIMIT"};
    if(ir.left && ir.right) return (line_check_output_t){0,0,true,"LINE_BOTH_BLACK_STOP"};
    if(ir.left) return (line_check_output_t){LINE_CORRECT_INNER_PWM,LINE_CORRECT_OUTER_PWM,false,"LINE_CORRECT_LEFT"};
    if(ir.right) return (line_check_output_t){LINE_CORRECT_OUTER_PWM,LINE_CORRECT_INNER_PWM,false,"LINE_CORRECT_RIGHT"};
    return (line_check_output_t){.40f,.40f,false,"LINE_BETWEEN_OR_LOST"};
}
#endif
