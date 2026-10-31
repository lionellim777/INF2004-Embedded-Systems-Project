# v5.10: two A passes and three Z passes

28 October 2026. Source: `serial-output/20261028-v5_10-A2-Z3.txt`.
Runs labelled by user-provided order. Retained result duplicates Z3 and is not a sixth run.

|Run|Decode|Stop|Capture|
|---|---|---|---|
|A1|A|BARCODE_READ|30 edges;1 frame;0 rejected|
|A2|None|BARCODE_BOTH_LINE_BLACK|33 total edges; first30 form rejected frame|
|Z1|None|BARCODE_BOTH_LINE_BLACK|11 total edges;0 frames;4 timeouts|
|Z2|Z|BARCODE_READ|30 edges;1 frame;0 rejected|
|Z3|Z|BARCODE_READ|30 edges;1 frame;0 rejected|

All five have zero encoder invalid transitions and no sampler overflow/gap/integrity fault. Max sampling gaps1009–1032us. Successful clear-interval mean cps A1=1209/1192, Z2=1145/1159, Z3=1153/1166. A2/Z1 means include later travel after failed capture; they must not be used to claim constant instantaneous scan velocity.

## A2 rejection

First30 edge timestamps give29 durations. Replaying the classification arithmetic yields reverse-direction start/end patterns0x052 and letter0x121 (reversing gives expected0x094/0x109). However one guard symbol has largest narrow79ms and smallest wide133ms: ratio1.684, below decoder's1.8 requirement. Thus the complete frame is rejected. This is an explicit validation failure, not missing polling samples. Log shows transient wheel rates1884/2254 and1887/2334cps alongside line-correction activity, supporting timing distortion from variable motion; raw transitions alone do not exclude optical distortion. Do not weaken the decoder simply to force this known A to pass.

## Z1 acquisition failure

Only11 edges over5.247s versus30 in good runs. Early black stretches764,847,830ms exceed750ms capture timeout. At measured wheel speeds near1200cps, GP26 remains above the220 white-release threshold for long intervals. Narrow gaps were not reliably classified as white in this pass. Exact physical cause is unproven: alignment, sensor height, paper flatness or changed reflectance may contribute. Two later Z passes worked without a firmware change. Faster polling cannot reconstruct contrast absent from the thresholded signal.

## Stop and follow-up

A2 stopped near4.963s, Z1 near5.247s on BOTH_LINE_BLACK, not pitch/fall detection. User specifically reports Z1 ran off foam. IMU hump messages occurred around the disturbance and while repositioning; they are not valid course hump evidence and did not command the stop. No dedicated cliff detector exists. The diagnostic currently continues after rejected frame/timeouts until another bound or stop condition occurs; future diagnostic change should stop explicitly on failed capture, retaining full rejected-frame evidence. A pre-capture distance/time bound must still account for a sensor missing all edges.

Next engineering work: separate scan failure from successful decode in motor stop behavior; constrain steering-induced scan-speed excursions or use measured spatial widths; preserve raw optical evidence for Z white-gap failures. Do not repeat unattended runs toward foam edges or claim reliable barcode completion from3/5 successes.
