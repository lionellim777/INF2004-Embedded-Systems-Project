# v5.6 barcode threshold — 28 October 2026

User supplied four labelled stationary screenshots at the existing setup:
wide black1992–2002, narrow black254–260, narrow white196–202,
wide white182–187. The previous black-on935 classified narrow black as white.
Three completed v5.5 slides each captured only six black regions; no frame decoded.
See [completed capture evidence](barcode-v5_5-physical-three-slides.md).

User explicitly requested >250 be black. The GP26 barcode sampler now enters
black at251 and returns to white at220 or lower. Between221–250 it retains its
previous state; two consecutive samples still confirm transitions. The220 release
level is an initial calibration choice, above the observed202 maximum white.
Entry margin above the251 threshold is only3 at the observed minimum narrow black;
these screenshots do not establish noise/height tolerance during motion.

Build: `build/barcode-stationary/robocar_barcode_stationary_v5_6.uf2`.
v5.5 artifact retained. Motors remain disabled in this stationary build.
The shared GP26 acquisition change will also be used in future motion builds;
the GP28/GP27 line thresholds and calibrated motor targets were not edited.
Page/banner/result identify v5.6 and thresholds. Start-on-white rejection now
uses the configured220 limit rather than a stale800 message.

Validation: strict-warning Pico W build; 16 actual-main host scenarios with all
motor writes LOW; measured-range synthetic A/Z captures in both directions;
250/251 entry and220/221 release boundaries, hysteresis and isolated outlier
rejection; existing acquisition/motion/line/guard regressions; wireless core and
normal/stationary transport tests. UF2 headers/family/addresses/banner checked;
motion, encoder and terrain symbols absent; v5.5 hash preserved. Build report
records hashes. These are software checks, not successful physical decoding.

Next: user flashes v5.6, refreshes page, keeps paper flat and sensor at its mounted
height, starts on white with GP20, waits for SCAN READY, slides large A once,
finishes scan, then downloads the result. No paper-height manipulation required.
Look for decoded=A valid=1 and30 transitions. If incomplete or rejected, analyse
the actual trace before claiming calibrated reliability. No flash or robot
movement was performed by the agent.
