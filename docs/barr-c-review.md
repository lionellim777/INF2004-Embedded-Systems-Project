# BARR-C:2018 review checklist

The attached BARR-C:2018 standard governs project-owned C sources. Run
`python tools/check_barr.py` and compile with the firmware warnings enabled.
The script checks 80-column lines, tabs, final newlines, and selected forbidden
tokens. It is intentionally a mechanical subset, not a BARR-C certification.
Review the remaining rules by hand before merging a module.

| Rule | Review point |
| --- | --- |
| 1.1 | Project application code uses C99; device extensions stay in the port |
| 1.2 | Source and header lines have at most 80 columns |
| 1.3 | Braces surround all control bodies and open on separate lines |
| 1.4 | Compound logical operands have clear parentheses |
| 1.7 | No `auto`, `register`, or forbidden jump functions |
| 1.8 | Private symbols use `static`; inputs use `const` where suitable |
| 3.4–3.5 | Four-space indentation and no tab characters |
| 4.1–4.3 | Lowercase module names and matching guarded headers |
| 5.1–5.3 | Module-prefixed types and fixed-width integers |
| 6.1–6.2 | Module-prefixed functions, descriptive names, short functions |
| 6.4 | RTOS task functions end in `_task` |
| 7.1–7.2 | Descriptive variables, required prefixes, initialization |
| 8.1–8.6 | One declaration per line, defaults, loop constants, equality style |

The micro T-Kernel BSP and Pico SDK remain upstream dependencies and are not
reformatted by this project. Record any project-owned exception with the
exact BARR-C rule, affected source, justification, and reviewer:

| Source | Rule | Reason | Review status |
| --- | --- | --- | --- |
| `src/app_main.c` | 6.1.i | RTOS requires exported `usermain` symbol | Pending team review |
| `port/mtk_pico_main.c` | 1.1.c | SDK hardware exception registration needs its API | Pending team review |

The port uses a fixed 96 KiB pool for micro T-Kernel's internal allocator.
Application telemetry buffers and queues have static storage. lwIP and SDK
may still allocate within their own networking internals. Verify peak memory,
stack use, and timing on the actual board before accepting the full system.
