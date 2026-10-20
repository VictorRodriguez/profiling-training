# Intel® VTune™ Profiler AI Assistant — Installation, Setup, and Validation

A practical Linux setup guide for installing VTune, preparing the AI Assistant, and validating both profiling and LLM access. It includes the fixes encountered during our native Linux and WSL installations: corporate proxies, missing build tools, Python dependencies, AWS clock skew, and restricted hardware counters.

## 1. Environment and installation paths

Our setup used **VTune 2026.4.0 (build 632893)** with **AWS Bedrock**. The package-manager examples below target Ubuntu/Debian; use your distribution's equivalents on other Linux systems.

| Installation | Typical oneAPI root | Requires sudo to install? |
|---|---|---|
| System-wide | `/opt/intel/oneapi` | Yes |
| Current user | `$HOME/intel/oneapi` | No, when the installer supports a user installation |

Both layouts work with the AI Assistant. Installing VTune without root does not grant access to restricted hardware counters.

Follow the sections in order. Skip proxy configuration on a network that does not require it.

## 2. Configure network access before installing

### Shell proxy: downloads, uv, Python, and provider connections

Replace the example address with the proxy supplied by your network administrator:

```bash
export http_proxy="http://proxy.example.com:port"
export https_proxy="$http_proxy"
export HTTP_PROXY="$http_proxy"
export HTTPS_PROXY="$https_proxy"
export no_proxy="localhost,127.0.0.1,::1"
export NO_PROXY="$no_proxy"
```

An HTTPS destination commonly uses an `http://` proxy URL with HTTP CONNECT; use the protocol required by your proxy. Add internal hosts to `no_proxy` only when they must be reached directly. Put these exports in `~/.bashrc` if they should apply to new interactive Bash sessions.

Verify connectivity before running setup:

```bash
curl -I --connect-timeout 10 --max-time 30 https://pypi.org/simple/
curl -I --connect-timeout 10 --max-time 30 https://astral.sh/
```

`setup.sh` downloads dependencies through Astral/uv and PyPI. Configuring APT alone does not configure these connections. If your network inspects TLS, install the organization's trusted CA using its instructions; do not disable certificate verification.

### Persistent APT proxy

APT can use its own configuration file, including when invoked with `sudo`:

```bash
sudoedit /etc/apt/apt.conf.d/95proxies
```

Add the following, replacing the example address:

```text
Acquire::http::Proxy "http://proxy.example.com:port/";
Acquire::https::Proxy "http://proxy.example.com:port/";
```

Then test:

```bash
sudo apt-get update
```

For a single invocation instead of a persistent file:

```bash
sudo apt-get \
  -o Acquire::http::Proxy="$http_proxy" \
  -o Acquire::https::Proxy="$https_proxy" \
  update
```

The one-time options apply only to that command. Remove the proxy configuration when moving to a network that does not use it. For authenticated APT proxies, use `apt_auth.conf(5)` rather than putting passwords in a world-readable configuration file.

### Keep proxy settings when using sudo

`sudo` normally filters the caller's environment. A command that connects successfully as your user may therefore time out when run with `sudo`.

For a single command, preserve only the proxy variables (subject to local sudo policy):

```bash
sudo --preserve-env=http_proxy,https_proxy,HTTP_PROXY,HTTPS_PROXY,no_proxy,NO_PROXY \
  /absolute/path/to/command arguments
```

To retain them for future sudo commands, edit a sudoers drop-in with syntax checking:

```bash
sudo visudo -f /etc/sudoers.d/proxy
```

Add:

```text
Defaults env_keep += "http_proxy https_proxy HTTP_PROXY HTTPS_PROXY no_proxy NO_PROXY"
```

Validate the configuration:

```bash
sudo visudo -c
```

This retains variables already exported by the calling shell; it does not define a proxy. `sudo -E` requests broader environment preservation and is generally unnecessary for this purpose.

Run the AI Assistant as your regular user by default. Proxy preservation does not preserve AWS credentials, the user's home directory, or oneAPI's `PATH`. For an elevated VTune command, use its absolute executable path. Run checks as the same user who will run the Assistant; use elevated profiling only when needed.

## 3. Install Linux prerequisites

Install the toolchain before running AI setup:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  git curl wget ca-certificates \
  python3 python3-dev python3-venv
```

`build-essential` provides **GCC, G++, make, and the standard development headers**. Installing only `make` does not fix a missing compiler.

Verify:

```bash
cc --version
gcc --version
g++ --version
make --version
python3 --version
```

These tools support native Python dependency builds and the training workloads. VTune itself does not require that every workload be compiled with GCC.

## 4. Install Intel VTune Profiler

Download the Linux installer from the [official VTune download page](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler-download.html). Choose a release that includes the AI Assistant.

For the installer used in our setup:

```bash
# From the directory containing the downloaded installer.
# User installation: select a writable installation directory in the installer.
sh ./intel-vtune-2026.4.0.23.sh
```

For a system-wide installation, run the installer with sudo, preserving the proxy if the installer needs network access:

```bash
sudo --preserve-env=http_proxy,https_proxy,HTTP_PROXY,HTTPS_PROXY,no_proxy,NO_PROXY \
  sh ./intel-vtune-2026.4.0.23.sh
```

Use the actual downloaded filename for other releases. The offline installer is useful when the target cannot download installation payloads; the AI Assistant still needs dependency and LLM-provider access.

Initialize **one** of the following layouts in the current shell:

```bash
# System-wide installation:
source /opt/intel/oneapi/setvars.sh

# OR current-user installation:
# source "$HOME/intel/oneapi/setvars.sh"
```

If `setvars.sh` is unavailable, initialize VTune directly with the appropriate path:

```bash
source /opt/intel/oneapi/vtune/latest/env/vars.sh
# User-install equivalent:
# source "$HOME/intel/oneapi/vtune/latest/env/vars.sh"
```

Verify installation:

```bash
command -v vtune
vtune --version
```

Our system installation resolved to `/opt/intel/oneapi/vtune/2026.4/bin64/vtune`. Initialize the environment in each new shell, or add the appropriate initialization to your shell startup file.

## 5. Prepare a writable AI Assistant workspace

The Assistant is bundled under `vtune/latest/vtune-profiler-ai-assistant`. Copy it to a writable directory so setup can create its local environment without modifying the installed package.

Set the path for your installation, then copy:

```bash
VTUNE_ROOT="/opt/intel/oneapi/vtune/latest"
# For a user installation, use instead:
# VTUNE_ROOT="$HOME/intel/oneapi/vtune/latest"

# Use a new destination; do not copy over an existing configured workspace.
cp -a "$VTUNE_ROOT/vtune-profiler-ai-assistant" "$HOME/vtune-ai"
cd "$HOME/vtune-ai"
```

If `~/vtune-ai` already exists, use that workspace or choose another new destination. Inspect the bundled README and templates for your release; filenames and configuration options may change.

Run setup **without sudo**:

```bash
./setup.sh
```

Our setup bootstrapped a local uv installation under `~/vtune-ai/.bin`. A system-wide uv installation was not required.

If setup reports that `cc`, `gcc`, or `make` is missing, install the prerequisites in Section 3 and rerun it. If dependency downloads stall, verify the shell proxy and TLS trust first.

### Python dependency compatibility

In one installation, setup selected Python 3.15 and a `tiktoken` dependency fell back to a source build that required Rust. A compatible Python 3.13 environment avoided that particular dependency problem.

If you encounter the same failure, after setup has bootstrapped `.bin/uv`:

```bash
cd "$HOME/vtune-ai"
./.bin/uv python install 3.13

# Preserve a failed environment before recreating it, if one exists.
if [ -d .venv ]; then
  mv .venv ".venv.backup.$(date +%Y%m%d%H%M%S)"
fi

./.bin/uv venv --python 3.13 .venv
UV_PYTHON=3.13 ./setup.sh
```

Check the setup output to confirm which interpreter it uses. Python 3.13 is a workaround for the dependency failure we encountered, not a universal version requirement. Follow your release's bundled instructions if its setup script explicitly pins another interpreter.

## 6. Configure the LLM provider

Create local configuration files from the bundled templates **only if you have not already configured them**:

```bash
cd "$HOME/vtune-ai"
[ -f vtune-ai.toml ] || cp vtune-ai.toml.example vtune-ai.toml
[ -f .env ] || cp .env.template .env
chmod 600 .env
```

Edit `vtune-ai.toml` using the supplied schema. Our installation used the `bedrock` profile; where the template provides this setting, select:

```toml
active_profile = "bedrock"
```

Set the region and model/inference-profile identifier in the fields provided by your release's template. Our successful readiness check displayed `us.anthropic.claude-opus-4-8`; this is an observed profile identifier, not a requirement for other accounts. Your account needs access to the selected model and region.

For AWS Bedrock, use your approved credential mechanism. If using environment credentials, the standard variables are:

```bash
export AWS_ACCESS_KEY_ID="<access-key>"
export AWS_SECRET_ACCESS_KEY="<secret-key>"
export AWS_REGION="<aws-region>"
export AWS_DEFAULT_REGION="$AWS_REGION"

# Required when using temporary credentials:
export AWS_SESSION_TOKEN="<session-token>"
```

Replace placeholders with your own values using your normal credential workflow. Set `AWS_SESSION_TOKEN` only for temporary credentials; an expired token can break authentication. If using the bundled `.env` mechanism, fill in the keys documented in `.env.template`.

Keep credentials and the configured `.env` outside this training repository. Do not print them with `env | grep AWS` or include them in logs, screenshots, or commits.

## 7. Validate readiness and start the Assistant

From the configured workspace:

```bash
cd "$HOME/vtune-ai"
./bin/vtune-ai check
```

Our native Linux installation reported:

```text
LLM provider
Active profile: bedrock (AWS Bedrock · us.anthropic.claude-opus-4-8)
✓ Required environment is set.
✓ Live auth probe succeeded.

VTune & host readiness
✓ VTune found: Intel(R) VTune(TM) Profiler 2026.4.0
✓ Supported analysis types: hotspots, hotspots-hw, threading,
  performance-snapshot, uarch-exploration, memory-access

Setup looks good.
```

The check validates provider authentication and reports host readiness. Available analyses depend on the CPU, operating system, drivers, permissions, and virtualization. A successful check does not replace a real collection test.

Start an interactive session:

```bash
./bin/vtune-ai chat
```

Inspect the installed release's options with:

```bash
./bin/vtune-ai --help
./bin/vtune-ai chat --help
```

## 8. Validate profiling separately from LLM authentication

Check the installation with Intel's bundled self-checker where available:

```bash
"$VTUNE_ROOT/bin64/vtune-self-checker.sh"
```

For a small end-to-end CPU collection, from the root of this training repository:

```bash
gcc -g -O2 -fno-loop-interchange -DCOLUMN_MAJOR \
  01_loop_interchange/loop_interchange.c -o /tmp/vtune-loop-bad
vtune -collect hotspots -result-dir /tmp/vtune-hotspots-check -- /tmp/vtune-loop-bad 20
vtune -report hotspots -r /tmp/vtune-hotspots-check
```

Use a fresh result directory for each run. This checks basic Hotspots collection and symbol reporting; it does not prove hardware event-based analyses are available. Validate the specific analysis you intend to use on your target host.

## 9. WSL, virtualization, and counter permissions

Our WSL installation authenticated successfully and found VTune, but reported:

```text
Supported: hotspots, threading, performance-snapshot, uarch-exploration
Unsupported on this host: hotspots-hw, memory-access
Running in a virtualized environment. Uncore PMU events are not available.
perf_event_paranoid is 2 (restrictive).
```

Treat this as the observed check output for that host, not a promise of complete microarchitectural data under WSL. Confirm availability through an actual collection. Sudo cannot expose PMU or uncore events that the virtualization layer does not provide.

Inspect Linux permissions with:

```bash
cat /proc/sys/kernel/perf_event_paranoid
```

If a collection fails because of permissions on native Linux, use the self-checker and your organization's approved profiling setup to determine whether perf permissions or VTune sampling drivers need adjustment. Avoid permanently changing system-wide counter access solely to make a readiness message disappear.

## 10. Troubleshooting

### Proxy works as my user but sudo commands hang

Use Section 2's `--preserve-env` option or persistent `env_keep` configuration. APT's `95proxies` file applies only to APT. Also check the executable path: sudo may not retain oneAPI's `PATH`.

### `vtune: command not found`

Initialize the correct oneAPI environment from Section 4. Then run `command -v vtune` and `vtune --version`. For elevated profiling, use the resolved absolute executable path.

### Setup cannot compile a dependency

Check `cc --version` and `make --version`, then install `build-essential`. If the failure involves Python wheels or a Rust build for `tiktoken`, see Section 5's Python compatibility workaround. A compiler installation alone does not supply Rust.

### AWS credentials are not detected or authentication fails

Run `./bin/vtune-ai check` as the configured user. Verify the selected profile, credential expiration, region, and model access. A root invocation may use a different home directory and credential environment.

### AWS signature or clock-skew errors

AWS request signatures depend on accurate time. Check it before replacing credentials:

```bash
date -u
timedatectl status
```

On a system managed by chrony:

```bash
chronyc tracking
chronyc sources -v
sudo chronyc makestep
```

In our failure, all sources showed `^?` with `Reach 0`. `makestep` returned `200 OK`, but there was no usable time source, so the clock remained wrong. Configure a reachable, organization-approved NTP source and confirm synchronization with `chronyc tracking`; the command's success message alone does not establish synchronization. An HTTP/HTTPS proxy does not automatically relay NTP traffic.

On systems using systemd's time service instead, inspect `timedatectl timesync-status` and the configured synchronization service. In WSL, also verify the Windows host clock; a WSL restart may be necessary after correcting host time.

Do not blindly run `hwclock --hctosys`: it only helps if the hardware clock is correct, and an accessible hardware clock may not exist under virtualization. After correcting time, rerun:

```bash
./bin/vtune-ai check
```

Clock correction resolved our signature failure. Other 403 errors can indicate permissions, expired credentials, region, or model access problems.

## 11. Daily startup cheat sheet

After completing installation and configuration:

```bash
# Load your proxy exports first if your network requires them.
source /opt/intel/oneapi/setvars.sh
# For a user installation, use instead:
# source "$HOME/intel/oneapi/setvars.sh"

command -v vtune
vtune --version
date -u

cd "$HOME/vtune-ai"
./bin/vtune-ai check
./bin/vtune-ai chat
```

Before profiling a new host, verify provider authentication, VTune readiness, and a real collection. Preserve the check output and collection errors when asking for help, with credentials removed.

## References

- [Intel VTune Profiler downloads](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler-download.html)
- The AI Assistant README and configuration templates bundled with your installed VTune release.
- [APT configuration reference](https://manpages.debian.org/bookworm/apt/apt.conf.5.en.html) and [APT authentication files](https://manpages.debian.org/bookworm/apt/apt_auth.conf.5.en.html).
- [sudoers environment handling](https://github.com/sudo-project/sudo/blob/main/docs/sudoers.man.in).
- [uv environment variables](https://docs.astral.sh/uv/reference/environment/).
