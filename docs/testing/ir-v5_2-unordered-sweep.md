# Stationary IR contrast check — 26 October 2026

Raw evidence: `serial-output/20261026-v5_2-ir-unordered-sweep.txt`.
SHA-256: cfffd27daba4395e8e34211bab4a7872804160fbadd45e6be51d040c2480a2ad. Boot 3ff4da57; 144 IR samples, no Run=1 samples.
User swept each sensor repeatedly in no strict sequence and observed the sensor LED
extinguish over black. Do not assign physical roles from the chronological order.

| Channel / pin | Low cluster (<250), median | High cluster (>1000), median | Existing black threshold |
|---|---|---|---|
| L / GP26 | 173–195, 188 | 2639–3245, 3150.5 | 935 |
| C / GP28 | 167–188, 176 | 1721–3186, 3157.5 | 861 |
| R / GP27 | 141–215, 151 | 1119–1684, 1531.5 | 317 |

Clusters are an analysis of observed values, not labelled reference measurements.
The >1000 grouping is only for summarisation; it is not a new firmware threshold.
Existing thresholds follow the current normalised blackness >=250 calculation.
All three channels have clearly separated low/high readings. Retain current
thresholds for now; R's stronger high response than the historical black=842
reference does not by itself justify overwriting calibration or tuning the module
potentiometer. Normalised R saturates earlier; continuous steering scaling should
be reviewed once physical roles and fixed mounting-height references are known.

The random sweep establishes response but cannot identify which electrical channel
belongs to the offset barcode reader or the left/right members of the central pair.
Minimum remaining identification: black only under the offset sensor, note which
IR value rises; then black only under the left sensor of the central pair (view from
behind facing forward), note its value. The remaining channel is central-right.
Keep motor power off and Pico/Wi-Fi on, without GP20. No need to repeat the full
sweep or reflash for this identification. Then adapt dedicated barcode and two-sensor
line control together; existing three-sensor line/barcode logic is not validated
for this physical layout. Motor targets remain unchanged.
