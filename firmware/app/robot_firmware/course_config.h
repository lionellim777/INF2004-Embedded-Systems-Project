#ifndef COURSE_CONFIG_H
#define COURSE_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
/* Y/Z are test-track symbols. Their actions are user-configured, not official
 * assessment meanings. Delayed actions count crossings after the barcode. */
typedef struct { char y_action,z_action; uint16_t advance_mm; } course_config_t;
static inline const char *course_action_name(char a)
{
    switch(a){
    case 's':return "STRAIGHT_CANCEL_PENDING";
    case 'L':return "LEFT_NOW";case 'R':return "RIGHT_NOW";case 'U':return "UTURN_NOW";
    case 'l':return "LEFT_NEXT_JUNCTION";case 'r':return "RIGHT_NEXT_JUNCTION";
    case 'a':return "SKIP1_THEN_LEFT";case 'b':return "SKIP1_THEN_RIGHT";
    case 'c':return "SKIP2_THEN_LEFT";case 'd':return "SKIP2_THEN_RIGHT";
    case 'x':return "STOP";
    default:return "INVALID";
    }
}
static inline bool course_action_valid(char a)
{return a=='s'||a=='L'||a=='R'||a=='U'||a=='l'||a=='r'||a=='a'||a=='b'||a=='c'||a=='d'||a=='x';}
static inline unsigned course_action_junctions(char a)
{return a=='l'||a=='r'?1u:a=='a'||a=='b'?2u:a=='c'||a=='d'?3u:0u;}
static inline bool course_action_left(char a)
{return a=='L'||a=='l'||a=='a'||a=='c';}
static inline bool course_config_valid(course_config_t c)
{return course_action_valid(c.y_action)&&course_action_valid(c.z_action)&&c.advance_mm>=60&&c.advance_mm<=250;}
#endif
