# Step-by-Step Implementation Plan (Windows → Linux Reverse Tunnel)

Direction: **Windows initiates SSH to Linux** — because Linux cannot reach Windows.

---

### Phase 1 — Linux SSH Server Readiness

**Goal:** Ensure the Linux machine accepts SSH connections and supports reverse
tunneling.

#### 1.1 Verify sshd is Running on Linux

```bash
sudo systemctl status sshd
# If not running:
sudo systemctl enable --now sshd
```

#### 1.2 Configure sshd_config for Reverse Forwarding

Edit `/etc/ssh/sshd_config` and verify these lines (uncommented):

```
AllowTcpForwarding yes
GatewayPorts clientspecified
```

**What each does:**
- `AllowTcpForwarding yes` — enables `-L`, `-R`, `-D` forwarding (default is `yes`)
- `GatewayPorts clientspecified` — allows the client (Windows) to decide whether
  the remote port binds to `localhost` only or to all interfaces. Default `no`
  binds to localhost only, which is what we want (security).

```bash
sudo systemctl restart sshd
```

#### 1.3 Create a Dedicated User Account (optional but recommended)

```bash
sudo useradd -m -s /bin/bash kilotunnel
sudo passwd kilotunnel
```

#### 1.4 Verify Connectivity from Windows

From Windows (PowerShell or CMD):

```cmd
ssh kilotunnel@linux-machine-ip
```

---

### Phase 2 — Establish Reverse SOCKS Tunnel (Windows Side)

**Goal:** Windows connects to Linux via SSH and creates a SOCKS5 proxy on the
Linux machine that routes traffic back through Windows to the internet.

**Mechanism:** `ssh -R` with no destination creates a reverse dynamic forward:
a SOCKS5 proxy that listens on the **remote** (Linux) side and routes traffic
through the **local** (Windows) machine.

```
Windows (has internet) ──SSH──► Linux (air-gapped)
                                  │
                                  └── localhost:1080 (SOCKS5)
                                      Any app on Linux sending traffic here
                                      gets tunneled back to Windows → internet
```

#### 2.1 Windows OpenSSH Client (native, recommended)

The OpenSSH client is installed by default on Windows 10 build 1809+ and
Windows 11. Run from PowerShell or CMD:

```cmd
ssh -R 1080 kilotunnel@linux-machine-ip
```

Add keepalive and background flags:

```cmd
ssh -R 1080 -o ServerAliveInterval=60 -o ServerAliveCountMax=3 kilotunnel@linux-machine-ip
```

**Flags:**
- `-R 1080` — reverse dynamic SOCKS5 proxy. Creates listener on Linux:1080,
  routes traffic through Windows to the internet
- `ServerAliveInterval=60` — send keepalive every 60s to prevent timeout
- `ServerAliveCountMax=3` — disconnect after 3 failed keepalives (~180s)

#### 2.2 PuTTY (GUI alternative)

1. Open PuTTY
2. Session tab: Host Name = `linux-machine-ip`, Port = `22`
3. **Connection → SSH → Tunnels**
   - Source port: `1080`
   - Select **Remote** radio button
   - Select **Dynamic** radio button (if available; if not, leave destination blank)
   - Click **Add**
4. **Connection**: Set "Seconds between keepalives" to `60`
5. Back to Session tab: Save as "kilo-tunnel", click **Open**

After connecting, verify the tunnel entry shows:
```
R1080  (remote dynamic from Linux:1080)
```

#### 2.3 Key-Based Authentication (recommended)

On Windows (PowerShell):

```cmd
ssh-keygen -t ed25519 -f %USERPROFILE%\.ssh\id_ed25519_kilo
type %USERPROFILE%\.ssh\id_ed25519_kilo.pub | ssh kilotunnel@linux-machine-ip "cat >> ~/.ssh/authorized_keys"
```

Then connect without password:

```cmd
ssh -i %USERPROFILE%\.ssh\id_ed25519_kilo -R 1080 kilotunnel@linux-machine-ip
```

#### 2.4 Verify the Tunnel from Linux

```bash
# Check the SOCKS proxy is listening
ss -tlnp | grep 1080

# Test internet access through the tunnel
curl --proxy socks5h://localhost:1080 https://www.google.com -o /dev/null -w "%{http_code}\n"
# Should return 200
```

---

### Phase 3 — Route Kilo Code Traffic Through the Tunnel

**Goal:** Make Kilo Code's network calls go through the SOCKS5 proxy on
`localhost:1080`.

The proxy is now available locally on the Linux machine. These four approaches
are unchanged from the original plan.

There are **four approaches** — test them in order of simplicity.

#### 3.1 Approach 1: Environment Variables (easiest)

Set these before launching VS Code:

```bash
export HTTP_PROXY="http://localhost:1080"
export HTTPS_PROXY="http://localhost:1080"
export NO_PROXY="localhost,127.0.0.1"
```

**Problem:** `HTTP_PROXY`/`HTTPS_PROXY` expect an HTTP proxy (CONNECT method for
HTTPS), not SOCKS5. Node.js's built-in `fetch` / `undici` may not use SOCKS5
directly. **Likely won't work without a converter.** Test first.

#### 3.2 Approach 2: VS Code --proxy-server Flag

VS Code's Chromium engine **does** support SOCKS5:

```bash
code --proxy-server="socks5://localhost:1080"
```

This affects the entire VS Code window, including the extension host where Kilo
Code runs. **This is the most promising approach for `webfetch`.**

**Caveat:** SOCKS5 authentication is not supported by Chromium.

#### 3.3 Approach 3: SOCKS5 → HTTP Proxy Converter

Use `privoxy` to convert SOCKS5 to a standard HTTP proxy:

```bash
# Install privoxy
sudo apt install privoxy

# Configure /etc/privoxy/config:
#   listen-address 127.0.0.1:8118
#   forward-socks5t / localhost:1080 .

sudo systemctl restart privoxy

# Now use standard HTTP_PROXY with privoxy's HTTP endpoint:
export HTTP_PROXY="http://localhost:8118"
export HTTPS_PROXY="http://localhost:8118"
export NO_PROXY="localhost,127.0.0.1"
```

Start VS Code with these environment variables set. This should work for ALL
Node.js-based network calls.

#### 3.4 Approach 4: proxychains (universal fallback)

Wraps the entire VS Code process through the SOCKS5 proxy:

```bash
# Install
sudo apt install proxychains4

# Configure /etc/proxychains4.conf:
#   [ProxyList]
#   socks5 127.0.0.1 1080

# Launch VS Code
proxychains4 code
```

**Pros:** Guaranteed to work for any network call from VS Code.
**Cons:** Wraps everything, may slow down local operations.

---

### Phase 4 — Testing and Validation

#### 4.1 Verify Tunnel is Active

```bash
# Check SOCKS listener
ss -tlnp | grep 1080

# Test internet access
curl --proxy socks5h://localhost:1080 https://api.kilo.ai/health -v
curl --proxy socks5h://localhost:1080 https://www.google.com -o /dev/null -w "%{http_code}\n"
```

#### 4.2 Test websearch in Kilo Code

1. Launch VS Code with the proxy configuration from Phase 3 applied.
2. Ask Kilo Code: "Search the web for the latest Linux kernel version."
3. It should invoke the `websearch` tool and return results.
4. If it fails, check Kilo Code's output/logs for connection errors.

#### 4.3 Test webfetch (if needed)

1. Ask Kilo Code: "Fetch https://example.com and tell me what the page says."
2. It should invoke the `webfetch` tool.
3. If `websearch` works but `webfetch` doesn't: the SOCKS5 proxy works for the
   Kilo API call but the direct fetch is not going through the proxy → try
   approaches 3 or 4 from Phase 3.

#### 4.4 Diagnostic Commands

```bash
# Check if any process is using the proxy
ss -tnp | grep 1080

# Monitor tunnel traffic
sudo tcpdump -i lo port 1080 -n

# Test with the exact URL patterns Kilo uses
curl --proxy socks5h://localhost:1080 \
  -H "Authorization: Bearer $(cat ~/.config/kilo/auth_token 2>/dev/null || echo 'check-token')" \
  https://api.kilo.ai/v1/models
```

---

### Phase 5 — Persist the Tunnel

**Goal:** The tunnel must survive reboots and connection drops. Tunnel persistence
needs to be handled on **Windows** (the initiator) since Windows initiates the SSH
connection to Linux.

#### 5.1 Windows: Scheduled Task (auto-start on login)

Create a task that starts the tunnel when the Windows user logs in.

Save as `kilo-tunnel.ps1`:

```powershell
# C:\Users\<username>\Scripts\kilo-tunnel.ps1
$ErrorActionPreference = "Stop"

# Loop: reconnect on disconnect
while ($true) {
    try {
        ssh -R 1080 `
            -o ServerAliveInterval=60 `
            -o ServerAliveCountMax=3 `
            -o ExitOnForwardFailure=yes `
            -o StrictHostKeyChecking=accept-new `
            -i $env:USERPROFILE\.ssh\id_ed25519_kilo `
            kilotunnel@linux-machine-ip
    } catch {
        Write-Host "SSH tunnel lost, reconnecting in 10s..."
        Start-Sleep 10
    }
}
```

Set up a Scheduled Task (PowerShell as Administrator):

```powershell
$action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-WindowStyle Hidden -File C:\Users\$env:USERNAME\Scripts\kilo-tunnel.ps1"
$trigger = New-ScheduledTaskTrigger -AtLogOn
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -RestartInterval (New-TimeSpan -Minutes 1) -RestartCount 999
Register-ScheduledTask -TaskName "Kilo-Tunnel" -Action $action -Trigger $trigger -Settings $settings -Description "SSH reverse SOCKS tunnel for Kilo Code"
```

#### 5.2 Windows: Alternative — OpenSSH Client with autossh

If you have Git Bash / WSL installed on Windows, use `autossh`:

```bash
# In Git Bash or WSL:
autossh -M 0 -R 1080 \
    -o "ServerAliveInterval=30" \
    -o "ServerAliveCountMax=3" \
    -o "ExitOnForwardFailure=yes" \
    kilotunnel@linux-machine-ip
```

Or create a `.bat` file in the Windows Startup folder:

```bat
@echo off
:loop
ssh -R 1080 -o ServerAliveInterval=60 -o ServerAliveCountMax=3 -o ExitOnForwardFailure=yes kilotunnel@linux-machine-ip
timeout /t 10
goto loop
```

Save as `kilo-tunnel.bat` and place in `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\`.

#### 5.3 Linux: Monitor Script (checks if tunnel is alive)

Create `~/bin/kilo-tunnel-check.sh` — runs on Linux to verify the tunnel is up:

```bash
#!/bin/bash
# Checks if reverse SOCKS tunnel from Windows is alive on Linux

SOCKS_PORT="1080"

if ss -tlnp | grep -q ":${SOCKS_PORT} "; then
    echo "Tunnel ACTIVE (localhost:${SOCKS_PORT})"
    exit 0
else
    echo "Tunnel OFFLINE — waiting for Windows to reconnect"
    exit 1
fi
```

```bash
chmod +x ~/bin/kilo-tunnel-check.sh
```

**Note:** This script only monitors. The reconnection logic lives on Windows
(Phase 5.1/5.2) since Windows initiates the tunnel.

---

### Phase 6 — Launch VS Code with Proxy (Daily Workflow)

Once the tunnel is established, launch VS Code from Linux with the proxy:

```bash
code --proxy-server="socks5://localhost:1080"
```

Or if using privoxy (Approach 3.3):

```bash
HTTP_PROXY="http://localhost:8118" HTTPS_PROXY="http://localhost:8118" code
```

---

## Summary: Command Cheatsheet

### On Linux (one-time setup)
```bash
sudo systemctl enable --now sshd
```

### On Windows (every time)
```cmd
ssh -R 1080 kilotunnel@linux-ip
```

### On Linux (once tunnel is up, every VS Code session)
```bash
code --proxy-server="socks5://localhost:1080"
```

---

## PuTTY — Reverse SOCKS Tunnel Settings (Detailed)

If the Windows OpenSSH client is not available, use PuTTY.

### Step-by-Step PuTTY Configuration

1. **Session tab** (Category → Session):
   - Host Name (or IP address): `linux-machine-ip`
   - Port: `22`
   - Connection type: `SSH`
   - Saved Sessions: `kilo-tunnel` → click **Save**

2. **Connection → SSH → Tunnels**:
   - Source port: `1080`
   - Select **Remote** radio button
   - Select **Auto** or **Dynamic** radio button
   - Click **Add**
   - You should see `R1080` in the "Forwarded ports" list

3. **Connection** (Category → Connection):
   - Seconds between keepalives: `60`
   - Enable TCP keepalives: ✓ checked

4. **Connection → SSH**:
   - Enable compression: ✓ checked (optional, reduces bandwidth)

5. Back to **Session** tab → click **Save** to persist settings.

### What the PuTTY Settings Mean

| Setting | Value | Equivalent | Meaning |
|---------|-------|------------|---------|
| Source port | `1080` | Port number after `-R` | Port on Linux that becomes the SOCKS5 proxy |
| Remote radio | Selected | `-R` flag | Forward from remote (Linux) to local (Windows) |
| Dynamic radio | Selected | No destination specified | Creates SOCKS5 proxy, not a static port forward |
| Keepalive | `60` | `ServerAliveInterval=60` | Prevents idle timeout |

### Verifying PuTTY Tunnel Works

After connecting via PuTTY, on the Linux machine:

```bash
# Check the port is listening (should show sshd or PuTTY process)
ss -tlnp | grep 1080

# Test internet access through the tunnel
curl --proxy socks5h://localhost:1080 https://www.google.com -o /dev/null -w "%{http_code}\n"
# Must return 200
```

### PuTTY Troubleshooting

| Problem | Solution |
|---------|----------|
| "Remote port forwarding failed" | Check `AllowTcpForwarding yes` in `/etc/ssh/sshd_config` on Linux, restart sshd |
| Port 1080 already in use on Linux | Check with `ss -tlnp \| grep 1080` — kill the process or use a different port |
| Tunnel works but PuTTY window closes on error | In PuTTY → **Terminal** → set "Close window on exit" to "Never" |
| Connection drops after idle | Set keepalives (Connection → Seconds between keepalives: 60) |
| "Couldn't agree a key exchange algorithm" | Update PuTTY to latest version, or add legacy KEX to Linux sshd_config |
