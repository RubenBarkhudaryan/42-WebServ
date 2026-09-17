#!/usr/bin/env python3
"""
webserv automated test runner.

For each valid config scenario:
  - starts the server (cwd = repo root, so relative `root` paths in
    configs resolve the same way they would for a developer running
    `./webserv configs/<scenario>/config.conf` by hand)
  - sends HTTP requests via http.client (no curl required)
  - verifies status codes, body fragments, and response headers
  - writes tests/reports/<scenario>/report.md

For each invalid config scenario:
  - runs the server and expects a non-zero exit code

Writes tests/reports/summary.md at the end.

NOTE on scope: this suite only exercises what RequestHandler::handle()
currently implements — static GET serving, location matching, redirects,
custom error pages, and method gating. POST/DELETE/PUT/PATCH/HEAD all
return 501 unless a location's `allow_methods` excludes them (405).
There is no autoindex, CGI execution, or upload/body-size enforcement
yet, so this file does not test those. A few "edge_cases" tests document
known protocol-parsing limitations (no Host-header requirement, no
request-line validation) rather than asserting nginx-like behaviour that
isn't implemented — update them if/when RequestHandler grows those
features.
"""
import datetime
import http.client
import os
import socket
import subprocess
import sys
import time

# ── Constants ─────────────────────────────────────────────────────────────────

BASE_DIR    = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEBSERV_BIN = os.path.join(BASE_DIR, "webserv")
REPORTS_DIR = os.path.join(BASE_DIR, "tests", "reports")
HOST         = "127.0.0.1"
STARTUP_WAIT = 3.0   # max seconds to wait for port to be ready
SHUTDOWN_WAIT = 2.0

# ANSI colours
_GREEN  = "\033[92m"
_RED    = "\033[91m"
_GREY   = "\033[90m"
_BOLD   = "\033[1m"
_RESET  = "\033[0m"

def _c(text, *codes):
    return "".join(codes) + text + _RESET


# ── HTTP helpers ──────────────────────────────────────────────────────────────

def http_req(port, method, path, extra_headers=None, body=None, timeout=5):
    """Send one HTTP/1.1 request; return (status_int, headers_dict, body_str).
    On any connection error returns (None, {}, error_message)."""
    try:
        conn = http.client.HTTPConnection(HOST, port, timeout=timeout)
        h = {"Host": "{}:{}".format(HOST, port), "Connection": "close"}
        if extra_headers:
            h.update(extra_headers)
        if isinstance(body, str):
            body = body.encode("utf-8")
        conn.request(method, path, body=body, headers=h)
        resp  = conn.getresponse()
        rbody = resp.read().decode("utf-8", errors="replace")
        rhdrs = {k.lower(): v for k, v in resp.getheaders()}
        conn.close()
        return resp.status, rhdrs, rbody
    except Exception as exc:
        return None, {}, str(exc)


def raw_http(port, data, timeout=5):
    """Send raw bytes over a plain TCP socket; return (status_int_or_None, raw_str).
    status is None if the server never sent a full status line before timing out
    (which currently happens whenever the request can't be parsed at all —
    see the edge_cases scenario)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((HOST, port))
    s.sendall(data if isinstance(data, bytes) else data.encode("latin-1"))
    buf = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            buf += chunk
    except socket.timeout:
        pass
    s.close()
    if not buf:
        return None, ""
    parts = buf.split(b" ", 2)
    status = int(parts[1]) if len(parts) >= 2 and parts[1].isdigit() else None
    return status, buf.decode("utf-8", errors="replace")


# ── Server lifecycle ──────────────────────────────────────────────────────────

def _port_ready(port):
    try:
        c = http.client.HTTPConnection(HOST, port, timeout=0.3)
        c.connect()
        c.close()
        return True
    except Exception:
        return False


def start_server(config_path, ports):
    proc = subprocess.Popen(
        [WEBSERV_BIN, config_path],
        cwd=BASE_DIR,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    deadline = time.time() + STARTUP_WAIT
    while time.time() < deadline:
        if proc.poll() is not None:
            stderr_output = proc.stderr.read().decode("utf-8", errors="replace")
            raise subprocess.CalledProcessError(proc.returncode, [WEBSERV_BIN, config_path], stderr=stderr_output)
        if all(_port_ready(p) for p in ports):
            time.sleep(0.05)  # tiny buffer for all listeners to settle
            return proc
        time.sleep(0.05)
    time.sleep(0.1)
    return proc


def stop_server(proc):
    if proc is None or proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=SHUTDOWN_WAIT)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


# ── Test-case runner ──────────────────────────────────────────────────────────

class Result:
    __slots__ = ("name", "method", "path", "exp", "actual", "passed", "note")
    def __init__(self, name, method, path, exp, actual, passed, note=""):
        self.name   = name
        self.method = method
        self.path   = path
        self.exp    = str(exp)
        self.actual = str(actual)
        self.passed = passed
        self.note   = note


def run_case(case):
    method  = case["method"]
    path    = case["path"]
    port    = case.get("port", 8080)
    exp     = case["expected_status"]
    body    = case.get("body", None)
    hdrs    = case.get("headers", {})
    timeout = case.get("timeout", 5)

    status, resp_hdrs, resp_body = http_req(port, method, path, hdrs, body, timeout)

    if status is None:
        return Result(case["name"], method, path, exp, "ERR", False,
                      "connection failed: " + resp_body[:120])

    notes = []

    for fragment in case.get("body_contains", []):
        if fragment not in resp_body:
            notes.append("body missing " + repr(fragment))

    for hkey, hval in case.get("headers_contain", {}).items():
        got = resp_hdrs.get(hkey.lower(), "")
        if hval not in got:
            notes.append("{}: expected {!r} in {!r}".format(hkey, hval, got))

    passed = (status == exp) and not notes
    note   = "; ".join(notes) if notes else ""
    actual = str(status) if status == exp else "{} (exp {})".format(status, exp)
    return Result(case["name"], method, path, exp, actual, passed, note)


def raw_case(case):
    """Cases built from raw_data either expect a numeric status, or (for
    requests this server currently never responds to at all) expect_hang."""
    port    = case.get("port", 8080)
    timeout = case.get("timeout", 2)
    status, _ = raw_http(port, case["raw_data"], timeout)

    if case.get("expect_hang"):
        passed = status is None
        actual = "no response (hang)" if passed else str(status)
        exp    = "no response"
        return Result(case["name"], "RAW", "<raw>", exp, actual, passed)

    exp = case["expected_status"]
    if status is None:
        return Result(case["name"], "RAW", "<raw>", exp, "ERR", False, "no response")
    passed = (status == exp)
    actual = str(status) if passed else "{} (exp {})".format(status, exp)
    return Result(case["name"], "RAW", "<raw>", exp, actual, passed)


# ── Report writers ────────────────────────────────────────────────────────────

def write_report(scenario, config_relpath, results, ts):
    out_dir = os.path.join(REPORTS_DIR, scenario)
    os.makedirs(out_dir, exist_ok=True)

    total  = len(results)
    passed = sum(1 for r in results if r.passed)
    badge  = "PASS" if passed == total else "FAIL"
    icon   = "✅" if passed == total else "❌"

    lines = [
        "# Test Report: " + scenario,
        "",
        "- **Date:** " + ts,
        "- **Config:** `" + config_relpath + "`",
        "- **Result:** {} {} ({}/{})".format(icon, badge, passed, total),
        "",
        "## Test Cases",
        "",
        "| # | Test | Method | Path | Expected | Actual | Result |",
        "|---|------|--------|------|----------|--------|--------|",
    ]
    for i, r in enumerate(results, 1):
        ri   = "✅ PASS" if r.passed else "❌ FAIL"
        note = " — " + r.note if r.note else ""
        lines.append("| {} | {} | `{}` | `{}` | {} | {} | {}{} |".format(
            i, r.name, r.method, r.path, r.exp, r.actual, ri, note))

    with open(os.path.join(out_dir, "report.md"), "w") as fh:
        fh.write("\n".join(lines) + "\n")

    return passed, total


def write_error_report(scenario, config_relpath, exit_code, stderr_text, ts):
    out_dir = os.path.join(REPORTS_DIR, scenario)
    os.makedirs(out_dir, exist_ok=True)

    passed = (exit_code != 0)
    badge  = "PASS" if passed else "FAIL"
    icon   = "✅" if passed else "❌"

    lines = [
        "# Test Report: " + scenario + " (invalid config)",
        "",
        "- **Date:** " + ts,
        "- **Config:** `" + config_relpath + "`",
        "- **Result:** {} {}".format(icon, badge),
        "",
        "## Expected Behaviour",
        "",
        "Server must exit with a **non-zero exit code** on config validation failure.",
        "",
        "| Expectation | exit code | Result |",
        "|-------------|-----------|--------|",
        "| exit code ≠ 0 | {} | {} |".format(exit_code, icon),
        "",
        "### stderr output",
        "```",
        stderr_text.strip() if stderr_text.strip() else "(none)",
        "```",
    ]
    with open(os.path.join(out_dir, "report.md"), "w") as fh:
        fh.write("\n".join(lines) + "\n")

    return (1, 1) if passed else (0, 1)


def write_summary(rows, ts):
    lines = [
        "# Test Summary — " + ts,
        "",
        "| Scenario | Tests | Passed | Failed | Result |",
        "|----------|-------|--------|--------|--------|",
    ]
    total_t = total_p = 0
    for name, n_pass, n_total in rows:
        n_fail = n_total - n_pass
        icon   = "✅ PASS" if n_pass == n_total else "❌ FAIL"
        lines.append("| {} | {} | {} | {} | {} |".format(
            name, n_total, n_pass, n_fail, icon))
        total_t += n_total
        total_p += n_pass

    total_f = total_t - total_p
    overall = "✅ ALL PASS" if total_p == total_t else "❌ {} FAILED".format(total_f)
    lines.append("| **TOTAL** | **{}** | **{}** | **{}** | **{}** |".format(
        total_t, total_p, total_f, overall))

    os.makedirs(REPORTS_DIR, exist_ok=True)
    with open(os.path.join(REPORTS_DIR, "summary.md"), "w") as fh:
        fh.write("\n".join(lines) + "\n")


# ── Scenario definitions ──────────────────────────────────────────────────────
#
# Grammar reminder (see parser/src/config/ConfigParserBlocks.cpp):
#   top level        : one or more `server { ... }` blocks, no `http {}` wrapper
#   server directives: listen, root, server_name, error_page, client_max_body_size, index
#   location directives: allow_methods, root, autoindex, index, return,
#                         cgi_extension, cgi_path
# RequestHandler::handle() only actually serves GET; every other method is
# 501 unless a location's allow_methods excludes it, in which case it's 405.

VALID_SCENARIOS = [
    # ── basic ──────────────────────────────────────────────────────────────
    {
        "name":   "basic",
        "config": "configs/basic/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / (index fallback)",       "method": "GET",    "path": "/",               "port": 8080, "expected_status": 200, "headers_contain": {"content-type": "text/html"}},
            {"name": "GET /missing",                  "method": "GET",    "path": "/missing",        "port": 8080, "expected_status": 404},
            {"name": "POST / -> 501 (not implemented)","method": "POST",  "path": "/",               "port": 8080, "expected_status": 501},
            {"name": "DELETE / -> 501 (not implemented)","method": "DELETE","path": "/",              "port": 8080, "expected_status": 501},
            {"name": "HEAD / -> 501",                 "method": "HEAD",   "path": "/",               "port": 8080, "expected_status": 501},
            {"name": "PUT / -> 501",                  "method": "PUT",    "path": "/",               "port": 8080, "expected_status": 501},
            {"name": "PATCH / -> 501",                "method": "PATCH",  "path": "/",               "port": 8080, "expected_status": 501},
            {"name": "GET /../../etc/passwd -> 400",  "method": "GET",    "path": "/../../etc/passwd","port": 8080, "expected_status": 400},
            {"name": "GET /old-page -> 301 to /",     "method": "GET",    "path": "/old-page",       "port": 8080, "expected_status": 301, "headers_contain": {"location": "/"}},
        ],
    },
    # ── allow_methods (405 vs 501 distinction) ──────────────────────────────
    {
        "name":   "allow_methods",
        "config": "configs/allow_methods/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / -> 200",                        "method": "GET",    "path": "/",     "port": 8080, "expected_status": 200},
            {"name": "POST / -> 405 (excluded)",            "method": "POST",   "path": "/",     "port": 8080, "expected_status": 405},
            {"name": "DELETE / -> 405 (excluded)",          "method": "DELETE", "path": "/",     "port": 8080, "expected_status": 405},
            {"name": "GET /open -> 404 (no such file)",     "method": "GET",    "path": "/open", "port": 8080, "expected_status": 404},
            {"name": "POST /open -> 501 (allowed, unimplemented)",  "method": "POST",   "path": "/open", "port": 8080, "expected_status": 501},
            {"name": "DELETE /open -> 501 (allowed, unimplemented)","method": "DELETE", "path": "/open", "port": 8080, "expected_status": 501},
        ],
    },
    # ── error_page ───────────────────────────────────────────────────────────
    {
        "name":   "error_page",
        "config": "configs/error_page/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / -> 200",                    "method": "GET", "path": "/",           "port": 8080, "expected_status": 200},
            {"name": "GET /missing -> custom 404",      "method": "GET", "path": "/missing",     "port": 8080, "expected_status": 404, "body_contains": ["custom-404-marker"]},
            {"name": "GET /also-missing -> custom 404", "method": "GET", "path": "/also-missing", "port": 8080, "expected_status": 404, "body_contains": ["custom-404-marker"]},
        ],
    },
    # ── redirect ──────────────────────────────────────────────────────────────
    {
        "name":   "redirect",
        "config": "configs/redirect/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / -> 200",              "method": "GET", "path": "/",     "port": 8080, "expected_status": 200},
            {"name": "GET /old -> 301 to /new/",  "method": "GET", "path": "/old",  "port": 8080, "expected_status": 301, "headers_contain": {"location": "/new/"}},
            {"name": "GET /new/ -> 200",           "method": "GET", "path": "/new/", "port": 8080, "expected_status": 200},
        ],
    },
    # ── multi_port (two server blocks) ───────────────────────────────────────
    {
        "name":   "multi_port",
        "config": "configs/multi_port/config.conf",
        "ports":  [8080, 8081],
        "cases": [
            {"name": "GET :8080/ -> site1", "method": "GET", "path": "/", "port": 8080, "expected_status": 200, "body_contains": ["Site 1 on 8080"]},
            {"name": "GET :8081/ -> site2", "method": "GET", "path": "/", "port": 8081, "expected_status": 200, "body_contains": ["Site 2 on 8081"]},
            {"name": "GET :8080/missing",   "method": "GET", "path": "/missing", "port": 8080, "expected_status": 404},
        ],
    },
    # ── location_matching (longest-prefix wins) ──────────────────────────────
    {
        "name":   "location_matching",
        "config": "configs/location_matching/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / -> general root",              "method": "GET", "path": "/",               "port": 8080, "expected_status": 200, "body_contains": ["general root index"]},
            {"name": "GET /deep -> falls back to / root",  "method": "GET", "path": "/deep",           "port": 8080, "expected_status": 200, "body_contains": ["deep index under general root"]},
            {"name": "GET /deep/nested -> most specific location wins", "method": "GET", "path": "/deep/nested", "port": 8080, "expected_status": 200, "body_contains": ["override-marker"]},
            {"name": "GET /deep/nested/nope -> 404 under override root","method": "GET", "path": "/deep/nested/nope", "port": 8080, "expected_status": 404},
        ],
    },
    # ── static_files (mime types, raw byte serving) ──────────────────────────
    {
        "name":   "static_files",
        "config": "configs/static_files/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET /test.png -> image/png",             "method": "GET", "path": "/test.png",         "port": 8080, "expected_status": 200, "headers_contain": {"content-type": "image/png"}},
            {"name": "GET /cgi-bin/hello.py -> served as-is (no CGI exec)", "method": "GET", "path": "/cgi-bin/hello.py", "port": 8080, "expected_status": 200, "body_contains": ["#!/usr/bin/env python3"]},
            {"name": "GET /cgi-bin/test.php -> served as-is (no CGI exec)", "method": "GET", "path": "/cgi-bin/test.php", "port": 8080, "expected_status": 200, "body_contains": ["<?php"]},
            {"name": "GET /errors/404.html -> plain static file", "method": "GET", "path": "/errors/404.html", "port": 8080, "expected_status": 200},
            {"name": "GET /missing.xyz -> 404",                 "method": "GET", "path": "/missing.xyz",      "port": 8080, "expected_status": 404},
        ],
    },
    # ── edge_cases (403, path traversal, and documented protocol limits) ────
    {
        "name":   "edge_cases",
        "config": "configs/edge_cases/config.conf",
        "ports":  [8080],
        "cases": [
            {"name": "GET / -> 200",                       "method": "GET",    "path": "/",                "port": 8080, "expected_status": 200},
            {"name": "GET /missing -> 404",                "method": "GET",    "path": "/missing",         "port": 8080, "expected_status": 404},
            {"name": "GET /../../../../etc/passwd -> 400", "method": "GET",    "path": "/../../../../etc/passwd", "port": 8080, "expected_status": 400},
            {"name": "GET /secret.txt (chmod 000) -> 403", "method": "GET",    "path": "/secret.txt",      "port": 8080, "expected_status": 403},
            {"name": "DELETE / -> 405 (excluded)",         "method": "DELETE", "path": "/",                "port": 8080, "expected_status": 405},
            {"name": "POST / -> 405 (excluded)",           "method": "POST",   "path": "/",                "port": 8080, "expected_status": 405},
            {"name": "DELETE /open -> 501 (unimplemented)","method": "DELETE", "path": "/open",            "port": 8080, "expected_status": 501},
            {"name": "PUT /open -> 501 (unimplemented)",   "method": "PUT",    "path": "/open",            "port": 8080, "expected_status": 501},

            # Known limitations of the current request parser (see
            # HttpRequest::parseHeaders / Client::isRequestComplete): a
            # request line that fails to parse is simply never marked
            # complete, so the server never responds at all instead of
            # sending 400. These two cases document that rather than
            # asserting a 400 that the code doesn't produce.
            {"name": "malformed request line -> currently hangs (no 400 yet)",
             "raw_data": b"GETINVALID\r\n\r\n", "port": 8080, "expect_hang": True, "timeout": 2},
            {"name": "bare CRLF empty request -> currently hangs (no 400 yet)",
             "raw_data": b"\r\n", "port": 8080, "expect_hang": True, "timeout": 2},

            # Host header and strict CRLF framing are not validated at all
            # right now, so both of these are accepted and served normally.
            {"name": "missing Host header -> not enforced, served normally",
             "raw_data": b"GET / HTTP/1.1\r\n\r\n", "port": 8080, "expected_status": 200},
            {"name": "LF-only line endings -> accepted (lenient parsing)",
             "raw_data": b"GET / HTTP/1.1\nHost: 127.0.0.1\n\n", "port": 8080, "expected_status": 200},
        ],
    },
]

ERROR_SCENARIOS = [
    {"name": "bad_port",              "config": "configs/invalid/bad_port.conf"},
    {"name": "missing_semicolon",     "config": "configs/invalid/missing_semicolon.conf"},
    {"name": "duplicate_port",        "config": "configs/invalid/duplicate_port.conf"},
    {"name": "unknown_directive",     "config": "configs/invalid/unknown_directive.conf"},
    {"name": "duplicate_default_host","config": "configs/invalid/duplicate_default_host.conf"},
]


# ── Console helpers ───────────────────────────────────────────────────────────

_COL = 48  # column width for test name

def _print_header(title):
    print()
    print(_c("─" * 60, _GREY))
    print(_c("  " + title, _BOLD))
    print(_c("─" * 60, _GREY))


def _print_result(r):
    icon = _c("✅", _GREEN) if r.passed else _c("❌", _RED)
    note = _c("  ← " + r.note, _RED) if r.note else ""
    label = (r.name[:_COL - 1] + "…") if len(r.name) > _COL else r.name
    print("  {} {:<{}} {}{}".format(icon, label, _COL, r.actual, note))


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    os.makedirs(REPORTS_DIR, exist_ok=True)
    summary_rows = []

    if not os.path.isfile(WEBSERV_BIN):
        print(_c("  ✗ webserv binary not found at " + WEBSERV_BIN + " — run `make` first", _RED))
        sys.exit(1)

    # ── Valid config scenarios ──────────────────────────────────────────────
    for scenario in VALID_SCENARIOS:
        name   = scenario["name"]
        config = os.path.join(BASE_DIR, scenario["config"])
        ports  = scenario["ports"]
        cases  = scenario["cases"]
        rel    = scenario["config"]

        _print_header(name + "   [" + rel + "]")

        results = []
        try:
            proc = start_server(config, ports)
        except subprocess.CalledProcessError as e:
            print(_c("  ✗ server crashed on startup (rc={})".format(e.returncode), _RED))
            print("  stderr: " + e.stderr.strip())
            results.append(Result("server startup", "—", "—",
                                  "running", "crashed (rc={})".format(e.returncode),
                                  False, "stderr: " + e.stderr.strip()))
            proc = None

        if proc is not None:
            for case in cases:
                r = raw_case(case) if "raw_data" in case else run_case(case)
                results.append(r)
                _print_result(r)

        stop_server(proc)

        n_pass, n_total = write_report(name, rel, results, ts)
        summary_rows.append((name, n_pass, n_total))
        tag = _c("✅ {}/{} passed".format(n_pass, n_total), _GREEN) \
              if n_pass == n_total \
              else _c("❌ {}/{} passed".format(n_pass, n_total), _RED)
        print("  " + tag)

    # ── Error config scenarios ──────────────────────────────────────────────
    _print_header("Invalid-config validation (expect non-zero exit)")

    for scenario in ERROR_SCENARIOS:
        name   = scenario["name"]
        config = os.path.join(BASE_DIR, scenario["config"])
        rel    = scenario["config"]

        try:
            result = subprocess.run(
                [WEBSERV_BIN, config],
                cwd=BASE_DIR,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                timeout=5,
            )
            ec     = result.returncode
            stderr = result.stderr.decode("utf-8", errors="replace")
        except subprocess.TimeoutExpired:
            ec     = 0   # did not fail → bad
            stderr = "server did not exit within 5 seconds"

        passed = (ec != 0)
        icon   = _c("✅", _GREEN) if passed else _c("❌", _RED)
        print("  {} {:<30s} exit={}".format(icon, name, ec))

        n_pass, n_total = write_error_report(name, rel, ec, stderr, ts)
        summary_rows.append((name, n_pass, n_total))

    # ── Final summary ───────────────────────────────────────────────────────
    write_summary(summary_rows, ts)

    total_t = sum(n for _, _, n in summary_rows)
    total_p = sum(p for _, p, _ in summary_rows)
    total_f = total_t - total_p

    print()
    print(_c("═" * 60, _GREY))
    if total_f == 0:
        print(_c("  ✅  ALL PASS  — {}/{} tests".format(total_p, total_t), _GREEN + _BOLD))
    else:
        print(_c("  ❌  {} FAILED  — {}/{} passed".format(total_f, total_p, total_t), _RED + _BOLD))
    print(_c("  Reports → " + REPORTS_DIR + "/summary.md", _GREY))
    print(_c("═" * 60, _GREY))
    print()

    sys.exit(0 if total_f == 0 else 1)


if __name__ == "__main__":
    main()
