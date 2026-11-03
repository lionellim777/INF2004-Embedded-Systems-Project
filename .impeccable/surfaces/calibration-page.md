# Surface brief: car diagnostic page

Target: `firmware/app/robot_firmware/wireless/calibration.html` · Mode: Operate

## Direction contract

THESIS: The car's state reads like a concourse departure board: one fixed-column board everyone can read at arm's length. Refuses the grey card-stack dashboard.

OWN-WORLD: Matte near-black board faces on a brushed graphite frame; white condensed caps in fixed cells; amber row lamps for warnings, dim red for faults, green-white for OK. Tabular numerals everywhere. Standard web controls only.

STORY: Teammate glances: is it connected, what is it doing, what barcode did it read, why did it stop. Assessor sees a clean, credible instrument of the run.

FIRST VIEWPORT: Header board row (car, version, link, idle/running). Controls row (test select, Prepare, Stop). NOW board (state + plain meaning). Barcode board (big letter cell, sequence, Code 39 element figure). Event board (time · system · event · meaning, filterable).

FORM: Fused challenger signals-instruments-split-flap-concourse; seed:ee11ebf7b119765d.

SIGNATURE: State changes cascade character-by-character in the NOW and barcode cells (single swap under reduced motion); log rows restyle by severity without breaking the column grid.

## Decision log

- 2026-11-03: A Haulix-style dashboard (lime accent, rounded pill controls) was tried, then the user chose the departure-board direction above. The board is the current design.
- The flap cascade is CSS-only: the real letter is always in the DOM and shows whenever an animation is paused or throttled.
