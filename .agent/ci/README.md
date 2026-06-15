# ASR5K Minimum Governance Gate

Run from the repository root:

```powershell
python .agent/ci/run_checks.py
```

The gate performs:

1. Python syntax validation without generating `__pycache__`.
2. YAML parse validation.
3. Stale-link validation for documents marked `ACTIVE` in
   `DOCUMENT_STATUS_REGISTRY.md`.
4. Frozen architecture and M5 state-contract validation.

The default link check is status-aware. Historical, superseded, and
reference-only documents are not allowed to block current production work, but
their stale links remain maintenance debt.

To audit every Markdown file:

```powershell
python .agent/ci/run_checks.py --all-links
```

The all-links audit is expected to remain stricter than the production gate
until historical absolute `file:///` links are cleaned.

A passing gate does not:

- approve architecture changes;
- replace the required document reading order;
- prove hardware behavior;
- close D10 checksum or production EMIF1 verification.
