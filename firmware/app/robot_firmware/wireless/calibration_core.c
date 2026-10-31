#include "calibration_core.h"
#include <string.h>
#include <stdio.h>

void calibration_capture(calibration_core_t *c, const char *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        char ch = data[i];
        c->log[c->written % CAL_LOG_CAPACITY] = ch;
        ++c->written;
        if (ch == '\r') { continue; }
        if (c->line_length < CAL_LINE_CAPACITY - 1u) {
            c->line[c->line_length++] = ch;
        } else {
            c->line_overflow = true;
        }
        if (ch == '\n') {
            c->line[c->line_length] = '\0';
            if (!c->line_overflow) {
                if (strncmp(c->line, "[OPTICAL START]", 15u)==0) {
                    c->optical_result[0]=0;
                } else if(strncmp(c->line,"[OPTICAL RESULT]",16u)==0) {
                    snprintf(c->optical_result,sizeof(c->optical_result),"%.*s",(int)sizeof(c->optical_result)-1,c->line);
                } else if (strncmp(c->line, "[MOTION GO]", 11u) == 0 || strncmp(c->line, "[SCAN START]", 12u) == 0) {
                    c->optical_result[0]=0;
                    c->course_result[0] = c->stop[0] = c->settled[0] = c->summary[0] = c->timing[0] = c->barcode[0] = c->trace[0][0] = c->trace[1][0] = '\0';
                } else if (strncmp(c->line, "[MOTION STOP]", 13u) == 0 ||
                           strncmp(c->line, "[STOP]", 6u) == 0 || strncmp(c->line, "[SCAN STOP]", 11u) == 0) {
                    memcpy(c->stop, c->line, c->line_length + 1u);
                    c->course_result[0] = c->settled[0] = c->summary[0] = c->timing[0] = c->barcode[0] = c->trace[0][0] = c->trace[1][0] = '\0';
                } else if (strncmp(c->line, "[MOTION SETTLED]", 16u) == 0) {
                    memcpy(c->settled, c->line, c->line_length + 1u);
                } else if (strncmp(c->line, "[CHECK]", 7u) == 0) {
                    memcpy(c->summary, c->line, c->line_length + 1u);
                } else if (strncmp(c->line, "[BARCODE RESULT]", 16u) == 0) {
                    memcpy(c->barcode, c->line, c->line_length + 1u);
                } else if (strncmp(c->line, "[SCAN EDGES 1]", 14u) == 0) {
                    memcpy(c->trace[0], c->line, c->line_length + 1u);
                } else if (strncmp(c->line, "[SCAN EDGES 2]", 14u) == 0) {
                    memcpy(c->trace[1], c->line, c->line_length + 1u);
                } else if (strncmp(c->line, "[COURSE RESULT]", 15u) == 0) {
                    memcpy(c->course_result,c->line,c->line_length+1u);
                } else if (strncmp(c->line, "[TIMING]", 8u) == 0) {
                    memcpy(c->timing, c->line, c->line_length + 1u);
                }
            }
            c->line_length = 0u;
            c->line_overflow = false;
        }
    }
}

size_t calibration_read(const calibration_core_t *c, uint32_t since,
                        char *out, size_t capacity, uint32_t *next, bool *gap)
{
    /* Cursor session bounded to <4 GiB logs; a boot ID handles power resets. */
    uint32_t oldest = c->written > CAL_LOG_CAPACITY ? c->written - CAL_LOG_CAPACITY : 0u;
    *gap = since < oldest || since > c->written;
    if (*gap) { since = oldest; }
    size_t n = c->written - since;
    if (n > capacity) { n = capacity; }
    for (size_t i = 0; i < n; ++i) { out[i] = c->log[(since + i) % CAL_LOG_CAPACITY]; }
    *next = since + (uint32_t)n;
    return n;
}

bool calibration_command(calibration_core_t *c, char command)
{
    if (command == 'x') { c->pending = 'x'; return true; }
    if (!c->selection_allowed || c->pending != 0) { return false; }
    if (command != 'f' && command != 'l' && command != 'r' && command != 'u' && command != 't' && command != 'b' && command != 'c' && command != 's' && command != 'v') { return false; }
    c->pending = command;
    return true;
}

int calibration_take_command(calibration_core_t *c)
{
    int ch = c->pending == 0 ? -1 : c->pending;
    c->pending = 0;
    return ch;
}

bool calibration_parse_cursor(const char *text, uint32_t *value)
{
    uint32_t n = 0u;
    if (*text == '\0') { return false; }
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') { return false; }
        unsigned digit = (unsigned)(*text - '0');
        if (n > (UINT32_MAX - digit) / 10u) { return false; }
        n = n * 10u + digit;
    }
    *value = n;
    return true;
}

/* Strict bounded decimal URL, no partial parse or silent truncation. */
bool calibration_course_command(calibration_core_t *c,const char *path)
{
    if(!c->selection_allowed || c->pending)return false;
    size_t n=strlen(path);
    if((n!=15 && n!=16) || strncmp(path,"/mission/",9)!=0 || path[10]!='/' || path[12]!='/')return false;
    uint32_t advance;
    if(!calibration_parse_cursor(path+13,&advance) || advance<60 || advance>250 || path[13]=='0')return false;
    course_config_t config={path[9],path[11],(uint16_t)advance};
    if(!course_config_valid(config))return false;
    c->course=config;c->pending='q';return true;
}
