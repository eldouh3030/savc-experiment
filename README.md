# SAVC Experiment

Reproducible evaluation code for the paper:

> A. A. Eldouh, "SAVC: A Dual-Mode Trusted Intermediary Framework for Securing Author–Venue Communication against Impersonation in Academic Publishing" (under review).

SAVC (Secure Academic Venue Communication) protects authors against journal and conference impersonation by moving trust from email to an independent intermediary. Venues register outgoing messages and attach a Venue Verification Code (VVC). Its authenticator is bound to the recipient, the message content, and an expiry time, and the venue's keys, revocations, and payment accounts are recorded in an append-only ledger.

This repository contains the single-file C++17 program that produces every table and figure in the evaluation, the raw results reported in the paper, and the scripts used to draw the figures.

## Repository layout

| Path | Contents |
|---|---|
| `src/savc_experiment.cpp` | Complete experiment (dependency-free C++17): SHA-256, HMAC-SHA256, dataset generator, five defenses, hash-chain ledger, timing |
| `results/` | Raw outputs used in the paper (CSV) and `run_info.txt` (program version, compiler, threads) |
| `scripts/plots.py` | Draws Figs. 5–7 from the CSV files |
| `scripts/diagrams.py` | Draws the architecture figures (Figs. 1–4) |
| `figures/` | All paper figures (PNG, plus PDF/SVG where available) |
| `formal/` | Tamarin model of the Verification Bridge mode and its verification output (Section 5.2) |

## Build and run

The program has no external dependencies.

**Windows (Visual Studio 2022 or later).** Open *x64 Native Tools Command Prompt for VS*, then:

```
cl /O2 /std:c++17 /EHsc src\savc_experiment.cpp
savc_experiment.exe results
```

Use the **x64** prompt. The default *Developer Command Prompt* builds a 32-bit binary, which gives correct detection results but slower timings.

**Linux / macOS.**

```
g++ -O2 -std=c++17 -pthread src/savc_experiment.cpp -o savc_experiment
./savc_experiment results
```

A full run takes a few minutes and needs about 1 GB of free memory. On startup the program checks its SHA-256 and HMAC-SHA256 implementations against the FIPS 180-4 and RFC 4231 test vectors and stops if either check fails.

## Reproducibility

All randomness comes from a self-implemented SplitMix64 generator with fixed seeds, so every detection result is identical on every platform and compiler. We verified this by obtaining identical outputs with GCC 13 on Linux and MSVC 19.51 on Windows. Only the timing files (`latency_scaling.csv`, `throughput.csv`, `ledger.csv`) depend on the hardware.

The reported results were produced with program version 1.1 on a Dell OptiPlex Tower 7010 (Intel Core i7-12700, 16 hardware threads, 16 GB RAM, Windows 10 Pro for Workstations, MSVC 19.51 x64). MSVC reports `__cplusplus` as 199711 unless `/Zc:__cplusplus` is set. This value appears in `run_info.txt` and does not affect the build.

## Output files

| File | Paper | Description |
|---|---|---|
| `detection_by_class.csv` | Fig. 5 | Detection rate per attack class and acceptance rate per legitimate category, for each defense (mean, min, max over 10 runs) |
| `overall_metrics.csv` | Table 5 | TP, FN, FP, TN, TPR, FPR, precision, F1, accuracy per defense and run |
| `sensitivity.csv` | Fig. 6 | False rejection rate versus forwarding rate and unregistered-message rate |
| `latency_scaling.csv` | Fig. 7a | Issuance and verification latency versus registry size (median, min, max of 7 batches of 50,000 operations) |
| `throughput.csv` | Sec. 7.3 | Verifications per second versus number of threads |
| `ledger.csv` | Fig. 7b | Append cost and full-chain validation time versus ledger length |
| `tamper.csv` | Table 6 | Detection and localization of 4 × 1,000 injected ledger modifications |
| `bruteforce.csv` | Sec. 5 | Expected time to forge an authenticator for different tag lengths |

## Experimental design (summary)

Each of the ten runs generates 200 venues (three representatives each, one or two verified payment accounts), 5,000 authors, 5,000 legitimate messages, and 625 messages for each of eight attack classes:

| Class | Attack |
|---|---|
| A1 | Exact-domain spoofing (no authentication) |
| A2 | Look-alike venue clone with valid SPF/DKIM/DMARC |
| A3 | Compromised representative mailbox (no signing key) |
| A4 | Genuine message replayed to a different recipient |
| A5 | Genuine message replayed after expiry |
| A6 | Content tampering (50% in transit, 50% before signing) |
| A7 | Forged VVC (scraped reference, guessed authenticator) |
| A8 | Payment redirection with stolen signing credentials |

Legitimate traffic includes two operational imperfections: 3% of messages pass through forwarders that break DKIM alignment, and 0.5% are not registered by the venue. The five defenses are *DMARC only*, *Whitelist only*, *Whitelist + DMARC*, *SAVC (VVC only)*, and *SAVC (VVC + payment)*. See Section 6 of the paper for details.

## Regenerating the figures

```
pip install -r scripts/requirements.txt
python scripts/plots.py results figures        # Figs. 5–7
python -m playwright install chromium          # once, for diagrams only
python scripts/diagrams.py                     # Figs. 1–4
```

## Citation

If you use this code, please cite the paper (citation details will be added after publication).

## License

MIT License. See `LICENSE`.

## Formal verification (Tamarin)

`formal/savc.spthy` is the symbolic model of the Verification Bridge mode used in Section 5.2 of the paper. It was verified with Tamarin 1.10.0 and Maude 3.5:

```
tamarin-prover --prove formal/savc.spthy
```

All ten lemmas complete in a few seconds. The expected outcome, recorded in `formal/tamarin_summary.txt`, is that the authentication, key-secrecy, and payment-safety lemmas are verified, while `payment_safety_vvc_only` and the two ablation lemmas are falsified with attack traces. The falsified lemmas show that the payment check, recipient binding, and content digest are each necessary. On Windows, Tamarin runs under WSL.
