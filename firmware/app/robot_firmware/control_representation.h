#ifndef CONTROL_REPRESENTATION_H
#define CONTROL_REPRESENTATION_H
/* Internal state layout. Default ON (minimum-bit); build with
 * -DROBOCAR_COMPACT_CONTROL_STATE=0 for the old layout. Motion owns these
 * fields in the single-core RTOS target; they are never copied as wire bytes.
 * Do not enable for another owner/core without review. Boolean bitfields retain
 * C's nonzero-to-true conversion. Enum arguments remain wide at API boundaries
 * and are validated before internal storage. Counter, time and controller
 * arithmetic widths are deliberately unchanged.
 */
#ifndef ROBOCAR_COMPACT_CONTROL_STATE
#define ROBOCAR_COMPACT_CONTROL_STATE 1
#endif
/* One-bit Boolean field: "bool flag ROBOCAR_BIT1;" is "bool flag : 1;" in the
 * compact layout and a plain bool in the old one. Use it only for groups of
 * flags that one task (or one IRQ, with foreground access under IRQ exclusion)
 * owns; flags written by different contexts must not share a byte. */
#if ROBOCAR_COMPACT_CONTROL_STATE
#define ROBOCAR_BIT1 : 1
#else
#define ROBOCAR_BIT1
#endif
#endif
