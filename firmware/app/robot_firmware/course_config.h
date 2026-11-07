#ifndef COURSE_CONFIG_H
#define COURSE_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
/* Y/Z are test-track symbols. Their actions are user-configured, not official
 * assessment meanings. Delayed actions count crossings after the barcode. */
typedef struct {
    char y_action,z_action; uint16_t advance_mm;
    /* Other letters A-X: index letter-'A'. 0 = unmapped (record only).
     * Y and Z always use y_action/z_action above. */
    char letter_action[26];
} course_config_t;
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
/* Action for a decoded letter, or 0 if that letter is not mapped. */
static inline char course_config_action(const course_config_t *c,char letter)
{
    if(letter=='Y')return c->y_action;
    if(letter=='Z')return c->z_action;
    if(letter<'A'||letter>'X')return 0;
    return c->letter_action[letter-'A'];
}
static inline bool course_config_valid(course_config_t c)
{
    if(!course_action_valid(c.y_action)||!course_action_valid(c.z_action)||c.advance_mm<60||c.advance_mm>250)
        return false;
    for(unsigned i=0;i<24u;i++)
        if(c.letter_action[i]&&!course_action_valid(c.letter_action[i]))return false;
    return true;
}
#endif
