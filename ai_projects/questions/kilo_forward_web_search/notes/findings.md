# Raw Research Findings

## Kilo Code Architecture (fetched 2026-07-16)

### Sources
- https://kilo.ai/docs/automate/tools
- https://kilo.ai/docs/automate/how-tools-work
- https://kilo.ai/docs/getting-started/settings
- https://kilo.ai/docs/gateway
- https://kilo.ai/docs/customize/agent-permissions
- https://kilo.ai/docs/ai-providers

### Confirmed Facts

1. **Two web tools exist:** `webfetch` and `websearch`
2. **`websearch` is server-side:** Available only to "Kilo/OpenRouter users" —
   meaning the search is executed by the provider's backend, not the local client
3. **`webfetch` is client-side:** Fetches arbitrary URLs from the VS Code
   extension host. The sandbox docs confirm "built-in HTTP tools" are restricted
   by network sandbox, implying local HTTP calls
4. **No proxy config in Kilo settings:** Searched all settings pages — no
   `proxy`, `http.proxy`, `HTTP_PROXY`, `SOCKS` fields exist
5. **Domains:** `api.kilo.ai:443` (gateway), `app.kilo.ai:443` (account),
   `openrouter.ai:443` (alternative provider)
6. **Sandbox network:** `network: allow | deny` and `allowed_hosts` — this
   restricts, doesn't route

### Assumptions (Need Verification)

- [ ] Does `HTTP_PROXY`/`HTTPS_PROXY` env var influence Kilo Code's HTTP client?
- [ ] Does VS Code's `http.proxy` setting affect the Kilo extension?
- [ ] Is `webfetch` ever routed through the gateway (for some providers)?
- [ ] Which specific Node.js HTTP library does Kilo Code use? (`undici`? `node-fetch`?)

---

## SSH Tunneling — Windows Jump Host (fetched 2026-07-16)

### Sources
- https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_install_firstuse
- https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh-server-configuration
- https://man.openbsd.org/ssh.1
- https://man.openbsd.org/sshd_config.5

### Confirmed Facts

1. **Windows 10/11 needs manual OpenSSH Server install** (client is default)
2. **sshd_config keys for tunneling:**
   - `AllowTcpForwarding yes` — enables -L/-R/-D
   - `GatewayPorts clientspecified` — for -R external access
   - `PermitOpen any` — no destination restrictions
   - `DisableForwarding` must NOT be set
3. **Windows OpenSSH limitations:**
   - `AllowStreamLocalForwarding` — NOT available
   - `PermitTunnel` — NOT available
   - `X11Forwarding` — NOT available
   - `AcceptEnv` — NOT available (no env var forwarding from client)

### Key Commands

```bash
# SOCKS5 dynamic proxy (best option)
ssh -D 1080 -N -f user@windows-host

# Local forward (specific domains only)
ssh -L 8888:api.kilo.ai:443 -N -f user@windows-host

# SOCKS5 proxy with curl (note: socks5h for DNS-through-proxy)
curl --proxy socks5h://localhost:1080 https://api.example.com
```

---

## VS Code Proxy Configuration (fetched 2026-07-16)

### Sources
- https://code.visualstudio.com/docs/setup/network

### Confirmed Facts

1. **`http.proxy` setting is LEGACY** — only applies to extensions and CLI
   (`code --install-extension`), NOT the main VS Code window
2. **`--proxy-server` flag** — Chromium-level, supports SOCKS5
   (`--proxy-server="socks5://localhost:1080"`)
3. **SOCKS5 authentication NOT supported** — Chromium bug
   (https://bugs.chromium.org/p/chromium/issues/detail?id=256785)
4. **`HTTP_PROXY`/`HTTPS_PROXY` env vars** — VS Code docs mention these for
   Remote SSH scenarios, but they only support HTTP proxies, not SOCKS5 directly

### Proxy Approaches (ordered by likelihood of working)

| Approach | Mechanism | SOCKS5? | Affects Kilo Code? |
|----------|-----------|---------|-------------------|
| `--proxy-server="socks5://..."` | Chromium flag | Yes | Likely (entire window) |
| `HTTP_PROXY` via privoxy | Env var + converter | Via converter | Likely (Node.js) |
| `proxychains code` | LD_PRELOAD wrapper | Yes | Certainly (everything) |
| `http.proxy` settings.json | VS Code setting | Depends | Extension only |

---

## Test Results (to be filled during implementation)

| Date | Test | Approach | Result | Notes |
|------|------|----------|--------|-------|
| - | - | - | - | - |
