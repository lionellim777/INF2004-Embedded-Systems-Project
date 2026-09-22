# Contributing

## Branch and review workflow

1. Pull the latest `main` before starting.
2. Create a short-lived branch such as `feature/wheel-encoders`, `test/ir-calibration`, or `fix/ultrasonic-timeout`.
3. Keep each change focused on one subsystem or integration task.
4. Build the affected firmware and record the hardware test performed.
5. Open a pull request and obtain review before merging into `main`.

Do not commit generated build directories, UF2 files, Wi-Fi credentials, MQTT passwords, personal notes, or raw recordings.

## Commit messages

Use a short imperative summary, for example:

```text
Add quadrature encoder counter
Fix ultrasonic echo timeout
Document verified servo limits
```

## Definition of done

A hardware change is complete only when:

- GPIO and power requirements are documented.
- The program builds from a clean build directory.
- Motors default to stopped after boot or failure.
- Timeouts exist for operations that could otherwise block forever.
- The test procedure and observed result are recorded.
- Any remaining limitation is stated explicitly.

## Coding guidelines

- Use C11 and fixed-width integer types where size matters.
- Prefer small modules with explicit interfaces over a single large `main.c`.
- Avoid unexplained pin numbers and thresholds; use named constants.
- Validate pointers and sensor results before use.
- Keep interrupt handlers short and non-blocking.
- Use bounded waits and safe failure states.
- Follow the supplied BARR-C guidance where practical for the module.

