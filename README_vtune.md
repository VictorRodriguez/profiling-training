# Intel® VTune™ Profiler AI Assistant — Setup and Validation

This document summarizes the commands used to install, configure, validate, and troubleshoot the Intel® VTune™ Profiler AI Assistant on Linux.

The goal is to provide a reproducible setup that other team members can follow when enabling the AI-assisted VTune workflow.

---

## 1. Environment

The setup described here was validated with:

```text
OS: Linux
Intel VTune Profiler: 2026.4.0
VTune build: 632893
LLM provider: AWS Bedrock
Region: configured through the VTune AI profile
```

VTune was installed under:

```bash
/opt/intel/oneapi/vtune/2026.4/
```

The main VTune command-line tool is:

```bash
/opt/intel/oneapi/vtune/2026.4/bin64/vtune
```

---

## 2. Verify VTune Installation

Check that VTune is installed:

```bash
/opt/intel/oneapi/vtune/2026.4/bin64/vtune --version
```

Expected output should be similar to:

```text
Intel(R) VTune(TM) Profiler 2026.4.0
```

If the oneAPI environment has already been initialized, the shorter command can also be used:

```bash
vtune --version
```

---

## 3. VTune AI Assistant Location

The VTune AI Assistant is included with the Linux VTune installation.

The associated README can be found under:

```bash
/opt/intel/oneapi/vtune/latest/vtune-profiler-ai-assistant/README.md
```

Enter the AI Assistant directory:

```bash
cd /opt/intel/oneapi/vtune/latest/vtune-profiler-ai-assistant
```

Inspect the files:

```bash
ls -la
```

---

## 4. Validate the VTune AI Environment

From the VTune AI Assistant directory, run:

```bash
./bin/vtune-ai check
```

A correctly configured installation should report something similar to:

```text
LLM provider
Active profile: bedrock (AWS Bedrock)
✓ Required environment is set.
✓ Live auth probe succeeded.

VTune & host readiness
✓ VTune found: Intel(R) VTune(TM) Profiler 2026.4.0
✓ Supported analysis types:
  hotspots
  hotspots-hw
  threading
  performance-snapshot
  uarch-exploration
  memory-access

Setup looks good.
```

This is the main command to verify that:

* the LLM configuration is valid,
* authentication works,
* VTune can be found,
* and the supported VTune analysis types are available.

---

## 5. AWS Bedrock Authentication

When AWS Bedrock is used as the LLM provider, valid AWS credentials must be available in the environment or through the configured AWS authentication mechanism.

For static credentials, the standard AWS environment variables are:

```bash
export AWS_ACCESS_KEY_ID="<access-key>"
export AWS_SECRET_ACCESS_KEY="<secret-key>"
```

If temporary credentials are used:

```bash
export AWS_ACCESS_KEY_ID="<access-key>"
export AWS_SECRET_ACCESS_KEY="<secret-key>"
export AWS_SESSION_TOKEN="<session-token>"
```

The region may also be configured when required:

```bash
export AWS_REGION="<aws-region>"
export AWS_DEFAULT_REGION="<aws-region>"
```

Do **not** place real credentials in this README or commit them to Git.

Verify AWS-related variables with:

```bash
env | grep AWS
```

Avoid printing credentials in shared terminals, screenshots, logs, or documentation.

---

## 6. Important: System Clock Synchronization

AWS authentication depends on correctly timestamped requests.

A machine whose clock is significantly incorrect may produce errors such as:

```text
403 InvalidSignature
```

or AWS signature/time-skew authentication failures.

Check the machine time:

```bash
date
```

Check time synchronization:

```bash
timedatectl
```

Check the hardware clock:

```bash
sudo hwclock
```

If the system clock needs to be synchronized with the hardware clock:

```bash
sudo hwclock --hctosys
```

Then verify again:

```bash
date
```

and rerun:

```bash
./bin/vtune-ai check
```

In our setup, correcting the system clock resolved the AWS Bedrock authentication failure.

---

## 7. Verify the VTune Binary Manually

If `vtune-ai check` cannot locate VTune, verify the executable directly:

```bash
ls -l /opt/intel/oneapi/vtune/2026.4/bin64/vtune
```

Then:

```bash
/opt/intel/oneapi/vtune/2026.4/bin64/vtune --version
```

It may also be convenient to initialize the oneAPI environment:

```bash
source /opt/intel/oneapi/setvars.sh
```

Then verify:

```bash
which vtune
```

Expected path:

```text
/opt/intel/oneapi/vtune/2026.4/bin64/vtune
```

---

## 8. Supported Analysis Types

The validated configuration reported support for:

```text
hotspots
hotspots-hw
threading
performance-snapshot
uarch-exploration
memory-access
```

These correspond to common VTune workflows such as:

### Hotspots

Identify CPU-intensive functions and code regions.

```text
hotspots
```

### Hardware Event-Based Hotspots

Use PMU/hardware-event information for deeper CPU characterization.

```text
hotspots-hw
```

### Threading

Analyze thread utilization, concurrency, and synchronization behavior.

```text
threading
```

### Performance Snapshot

Provide a high-level overview of workload performance.

```text
performance-snapshot
```

### Microarchitecture Exploration

Investigate pipeline utilization and microarchitectural bottlenecks.

```text
uarch-exploration
```

### Memory Access

Analyze memory behavior, bandwidth, latency, and locality.

```text
memory-access
```

---

## 9. Basic Validation Sequence

For a new machine, the following sequence provides a quick sanity check.

### Step 1 — Verify the time

```bash
date
timedatectl
```

### Step 2 — Initialize oneAPI

```bash
source /opt/intel/oneapi/setvars.sh
```

### Step 3 — Verify VTune

```bash
vtune --version
```

### Step 4 — Enter the AI Assistant directory

```bash
cd /opt/intel/oneapi/vtune/latest/vtune-profiler-ai-assistant
```

### Step 5 — Validate the AI configuration

```bash
./bin/vtune-ai check
```

Successful output should end with:

```text
Setup looks good.
```

---

## 10. Troubleshooting

### `vtune: command not found`

Initialize oneAPI:

```bash
source /opt/intel/oneapi/setvars.sh
```

or use the full executable path:

```bash
/opt/intel/oneapi/vtune/2026.4/bin64/vtune
```

---

### AWS `403 InvalidSignature`

First verify the clock:

```bash
date
timedatectl
```

If the system clock is incorrect:

```bash
sudo hwclock --hctosys
```

Then retry:

```bash
./bin/vtune-ai check
```

---

### AWS credentials are not detected

Check:

```bash
env | grep AWS
```

Verify that the required authentication variables are available to the shell running `vtune-ai`.

Never paste the credentials into shared logs or documentation.

---

### VTune AI cannot find VTune

Verify:

```bash
which vtune
```

and:

```bash
vtune --version
```

If necessary:

```bash
source /opt/intel/oneapi/setvars.sh
```

---

## 11. Useful Diagnostic Commands

```bash
# Current date/time
date

# Time synchronization state
timedatectl

# Hardware clock
sudo hwclock

# VTune location
which vtune

# VTune version
vtune --version

# AWS environment
env | grep AWS

# VTune AI configuration check
./bin/vtune-ai check
```

For additional VTune AI documentation:

```bash
cat /opt/intel/oneapi/vtune/latest/vtune-profiler-ai-assistant/README.md
```

or:

```bash
less /opt/intel/oneapi/vtune/latest/vtune-profiler-ai-assistant/README.md
```

---

## 12. Quick Setup Cheat Sheet

```bash
# Initialize Intel oneAPI
source /opt/intel/oneapi/setvars.sh

# Verify VTune
vtune --version

# Check system time
date
timedatectl

# Go to VTune AI Assistant
cd /opt/intel/oneapi/vtune/latest/vtune-profiler-ai-assistant

# Validate configuration
./bin/vtune-ai check
```

If AWS authentication reports a signature/time error:

```bash
sudo hwclock --hctosys
date
./bin/vtune-ai check
```

A successful configuration should report:

```text
✓ Required environment is set.
✓ Live auth probe succeeded.
✓ VTune found
Setup looks good.
```

---

## 13. Recommended Team Validation

Before using the AI Assistant for performance analysis, each machine should successfully pass:

```bash
./bin/vtune-ai check
```

This ensures that the complete path is functioning:

```text
Application
    ↓
VTune Profiler
    ↓
VTune AI Assistant
    ↓
LLM Provider
    ↓
AWS Bedrock Authentication
```

Once this validation succeeds, the environment is ready for AI-assisted VTune performance-analysis workflows.
