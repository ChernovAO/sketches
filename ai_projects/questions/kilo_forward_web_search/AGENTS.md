# Kilo Code – Web Search via Port Forwarding (Research Project)

## Goal

Enable web search in the Kilo Code VS Code plugin on a Linux machine with **no
direct internet access**, using a reverse SSH tunnel from a **Windows machine**
that has internet connectivity. The Windows machine initiates SSH to Linux
(since Linux cannot reach Windows).

---

## Architecture Overview (Confirmed Facts)

### How Kilo Code's Web Tools Work

Kilo Code has **two** web-access tools:

| Tool | Where it runs | Requirements |
|------|--------------|--------------|
| `websearch` | **Server-side** — via Kilo Gateway or OpenRouter API | Needs `api.kilo.ai:443` or OpenRouter reachable |
| `webfetch` | **Client-side** — direct HTTP from the VS Code extension host | Needs general internet access |

**Key insight:** `websearch` does NOT make direct search-engine calls from your
machine. The Kilo extension sends a tool-call request to the LLM provider (Kilo
Gateway or OpenRouter), and *the provider* executes the actual web search against
search APIs (Brave, Google, etc.). This means tunneling **only** the provider API
is sufficient for `websearch` to work.

### Domains That Must Be Reachable

| Domain | Port | Purpose | Critical? |
|--------|------|---------|-----------|
| `api.kilo.ai` | 443 | Chat completions API + websearch tool calls | Yes |
| `app.kilo.ai` | 443 | Authentication, account management, BYOK | Yes (for login) |
| `openrouter.ai` | 443 | OpenRouter API (if using OR provider) | Depends on provider |
| Any URL | 80/443 | `webfetch` fetches arbitrary web pages | Only if webfetch is needed |

### What Kilo Code Does NOT Have

- **No proxy configuration field** in `kilo.json` / `kilo.jsonc` settings schema
- **No HTTP_PROXY / SOCKS support** documented in Kilo Code settings
- **No custom search endpoint** option
- The sandbox `network` setting is a restriction mechanism, not a proxy config

*However* — VS Code's `http.proxy` setting and `--proxy-server` flag, plus
standard Node.js environment variables (`HTTP_PROXY`, `HTTPS_PROXY`), **may**
still influence the extension's network calls. This needs verification.

---

## Step-by-Step Implementation Plan

### Direction: Windows → Linux (Reverse Tunnel)

Linux **cannot** reach Windows on port 22. Windows initiates SSH to Linux and
creates a reverse SOCKS5 proxy on the Linux side. Any traffic sent to
`localhost:1080` on Linux gets tunneled back through the Windows machine to
the internet.

```
Windows (has internet) ──SSH──► Linux (air-gapped)
                                localhost:1080 → SOCKS5 → back through SSH → Windows → internet
```

### Phase 1 — Linux SSH Server Readiness

```bash
sudo systemctl enable --now sshd
```

Verify `/etc/ssh/sshd_config`:

```
AllowTcpForwarding yes
GatewayPorts clientspecified
```

### Phase 2 — Establish Reverse SOCKS Tunnel (Windows Side)

**Windows OpenSSH client** (built-in, PowerShell/CMD):

```cmd
ssh -R 1080 -o ServerAliveInterval=60 -o ServerAliveCountMax=3 kilotunnel@linux-ip
```

`-R 1080` (reverse dynamic forward) creates a SOCKS5 listener on Linux:1080 that
routes traffic through Windows to the internet. Key-based auth recommended
(see `IMPLEMENTATION_PLAN.md` for full key setup).

**PuTTY** (GUI alternative):

1. Session → Host Name: `linux-ip`
2. Connection → SSH → Tunnels: Source port `1080`, **Remote** radio, **Dynamic** radio → Add
3. Connection → Seconds between keepalives: `60`

**Verify from Linux:**

```bash
curl --proxy socks5h://localhost:1080 https://www.google.com -o /dev/null -w "%{http_code}\n"
# Must return 200
```

### Phase 3 — Route Kilo Code Traffic Through the Proxy

Four approaches, test in order:

| # | Approach | Command | Notes |
|---|----------|---------|-------|
| 1 | Env vars | `HTTP_PROXY=http://localhost:1080 code` | Unlikely to work (expects HTTP proxy, not SOCKS5) |
| 2 | VS Code flag | `code --proxy-server="socks5://localhost:1080"` | **Most promising** — Chromium-level SOCKS5 support |
| 3 | privoxy converter | `privoxy` (SOCKS→HTTP) + `HTTP_PROXY=http://localhost:8118 code` | Best for Node.js compatibility |
| 4 | proxychains | `proxychains4 code` | Fallback, wraps everything |

### Phase 4 — Testing

```bash
# Test tunnel
curl --proxy socks5h://localhost:1080 https://api.kilo.ai/health -v

# Launch VS Code with proxy
code --proxy-server="socks5://localhost:1080"

# Ask Kilo: "Search the web for the latest Linux kernel version"
```

### Phase 5 — Persist the Tunnel (Windows Side)

The tunnel is initiated from Windows, so persistence lives there. See
`IMPLEMENTATION_PLAN.md` for:

- **Scheduled Task** (start on Windows login, auto-reconnect loop)
- **Startup folder .bat** (simplest approach)
- **autossh** (if Git Bash / WSL is available)

**Linux monitoring script** (`~/bin/kilo-tunnel-check.sh`) — only monitors,
doesn't reconnect:

```bash
#!/bin/bash
ss -tlnp | grep -q ":1080 " && echo "Tunnel ACTIVE" || echo "Tunnel OFFLINE"
```

---

## Network Topology Diagram

```
┌─────────────────────────────┐      SSH Tunnel       ┌──────────────────────────┐
│   Air-Gapped Linux Machine  │ ◄═══════════════════► │  Windows Jump Host       │
│                             │   (port 22 TCP)        │  (has internet access)   │
│  ┌─────────────────────┐    │                        │                          │
│  │ VS Code + Kilo Code │    │                        │  ┌────────────────────┐  │
│  │                     │    │                        │  │ OpenSSH Client     │  │
│  │ HTTP calls via:     │    │                        │  │ (ssh -R 1080)      │  │
│  │ - SOCKS5:1080 ──────┼────┼────────────────────────┼──► connects to port 22│  │
│  │   (ssh -R creates)  │    │                        │  │                    │  │
│  │                     │    │                        │  └────────┬───────────┘  │
│  │ - Privoxy:8118 ─────┼────┤ SOCKS5:1080 → Windows  │          │              │
│  │   (HTTP proxy)      │    │                        │          ▼              │
│  └─────────────────────┘    │                        │  ┌────────────────────┐  │
│                             │                        │  │ Internet Access    │  │
│  Kilo API calls:            │                        │  │                    │  │
│  api.kilo.ai:443  ──────────┼─── through SOCKS5 ────►│  │ api.kilo.ai:443    │  │
│  openrouter.ai:443 ─────────┼─── through SOCKS5 ────►│  │ google.com:443     │  │
│                             │                        │  │ any-url:80/443     │  │
│  webfetch URLs:             │                        │  └────────────────────┘  │
│  any-site.com:443 ──────────┼─── through SOCKS5 ────►│                          │
└─────────────────────────────┘                        └──────────────────────────┘
```

---

## Research Questions — Answered

### 1. How does Kilo Code's web search work internally?

`websearch` is a **server-side** tool. It sends a function-call to the LLM
provider (Kilo Gateway / OpenRouter), which executes the search against Brave/Google
APIs and returns results. The local client only communicates with
`api.kilo.ai:443` or `openrouter.ai:443`.

`webfetch` is a **client-side** tool. It makes direct HTTP requests from the VS
Code extension host to arbitrary URLs using standard Node.js HTTP libraries.

**Source:** https://kilo.ai/docs/automate/tools, https://kilo.ai/docs/automate/how-tools-work

### 2. What ports does the Kilo Code plugin connect to?

- `api.kilo.ai:443` — Kilo Gateway API (chat completions, websearch)
- `app.kilo.ai:443` — Account management, authentication
- `openrouter.ai:443` — OpenRouter API (if configured)
- Arbitrary `*:80` and `*:443` — for `webfetch` direct fetches

### 3. How to configure SSH port forwarding?

**Direction: Windows → Linux (reverse tunnel).** Windows initiates SSH to Linux
and creates a SOCKS5 proxy on the Linux side that routes traffic back through
Windows to the internet.

**Recommended:** Reverse SOCKS5 dynamic proxy:
```bash
# From Windows (PowerShell/CMD):
ssh -R 1080 -o ServerAliveInterval=60 kilotunnel@linux-ip
```

**PuTTY GUI:** Connection → SSH → Tunnels → Source `1080`, Remote + Dynamic → Add.

**Prerequisites on Linux:** OpenSSH Server running, `AllowTcpForwarding yes`
in sshd_config.

### 4. What Kilo Code configuration options control web search behavior?

**None directly.** Kilo Code has no proxy settings in its config schema. The
sandbox settings (`network: allow | deny`, `allowed_hosts`) can restrict but not
route traffic.

Web search behavior is controlled by:
- Which AI provider is active (Kilo Gateway vs OpenRouter vs BYOK)
- Agent permissions (whether `websearch` and `webfetch` tools are allowed)

**Source:** https://kilo.ai/docs/getting-started/settings, https://kilo.ai/docs/customize/agent-permissions

### 5. Can a local proxy be configured for Kilo Code?

**Not natively.** Kilo Code has no proxy config field. However, traffic can be
routed through a proxy via:
- VS Code's `--proxy-server="socks5://localhost:1080"` flag (Chromium-level, supports SOCKS5)
- `HTTP_PROXY`/`HTTPS_PROXY` env vars (via privoxy SOCKS→HTTP conversion)
- `proxychains` wrapping the entire VS Code process

**Source:** https://code.visualstudio.com/docs/setup/network

---

## Files

```
AGENTS.md                    # This file — master plan and findings
IMPLEMENTATION_PLAN.md       # Detailed step-by-step (Windows→Linux, PuTTY guide)
notes/
    findings.md              # Raw research notes (fetched docs, test results)
    tunnel-test-log.md       # Tunnel diagnostics and test results
    proxy-test-matrix.md     # Which proxy approaches work/don't work
diagrams/
    network-topology.md      # Mermaid network diagrams
```

## References

| Topic | URL |
|-------|-----|
| Kilo Code tools (websearch, webfetch) | https://kilo.ai/docs/automate/tools |
| Kilo Code tool architecture | https://kilo.ai/docs/automate/how-tools-work |
| Kilo Code settings/sandboxing | https://kilo.ai/docs/getting-started/settings |
| Kilo Code agent permissions | https://kilo.ai/docs/customize/agent-permissions |
| Kilo Gateway API | https://kilo.ai/docs/gateway |
| Kilo Code providers | https://kilo.ai/docs/ai-providers |
| Windows OpenSSH install | https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_install_firstuse |
| Windows OpenSSH server config | https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh-server-configuration |
| ssh man page (-L/-R/-D) | https://man.openbsd.org/ssh.1 |
| sshd_config man page | https://man.openbsd.org/sshd_config.5 |
| VS Code network/proxy | https://code.visualstudio.com/docs/setup/network |
| Chromium SOCKS proxy | https://www.chromium.org/developers/design-documents/network-stack/socks-proxy |
