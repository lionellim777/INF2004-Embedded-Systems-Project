#ifndef CONTROL_REPRESENTATION_H
#define CONTROL_REPRESENTATION_H
/* Internal state layout. Default ON (minimum-bit); build with -DROBOCAR_COMPACT_CONTROL_STATE=0 for the old layout.
 * Motion owns these fields in the single-core RTOS target; they are never
 * copied as wire bytes. Do not enable for another owner/core without review.
 * Boolean bitfields retain C's nonzero-to-true conversion. Enum arguments
 * remain wide at API boundaries and are validated before internal storage.
 * Counter, time and controller arithmetic widths are deliberately unchanged.
 */
#ifndef ROBOCAR_COMPACT_CONTROL_STATE
#define ROBOCAR_COMPACT_CONTROL_STATE 1
#endif
#endif
