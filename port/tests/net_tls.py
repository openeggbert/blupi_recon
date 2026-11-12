"""wss:// from the desktop (net/tls.h) to a local Free Direct server behind a TLS front.

Makes a test CA and a certificate it signs for localhost (tls_front.py, openssl), starts
free-direct-server and the TLS front for it, and runs the probe (test_net_tls) once per case, each
a fresh process with its own SSL_CERT_FILE / SSL_CERT_DIR:
  - wss://localhost with the test CA trusted (a file, or a directory): the server's welcome;
  - another CA trusted: "not trusted";  wss://127.0.0.1 (not the certificate's name): "another name";
  - a certificate file without certificates: refused before connecting;
  - ws:// to the TLS front, and wss:// to the server's plain port: refused, never hanging.
Exit 77 (skipped) without openssl. Never runs an original executable.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from types import SimpleNamespace

from net_session import start_server
import tls_front


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, help="test_net_tls (or its launcher)")
    parser.add_argument("--relay", required=True, help="free-direct-server executable")
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    if not tls_front.openssl():
        print("no openssl: cannot make the test certificates")
        return tls_front.SKIP
    work = args.work.resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    certs = tls_front.make_certs(work / "certs")
    (work / "ca-dir").mkdir()
    shutil.copy(certs["ca"], work / "ca-dir" / "ca.pem")
    (work / "empty.pem").write_text("no certificates here\n", encoding="ascii")

    server, log, plain = start_server(SimpleNamespace(relay=args.relay), work)
    front = tls_front.TlsFront(certs["cert"], certs["key"], plain)
    tls = front.port
    cases = [
        ("trusted CA file", f"wss://localhost:{tls}", {"SSL_CERT_FILE": certs["ca"]}, ["ok"]),
        ("trusted CA directory", f"wss://localhost:{tls}", {"SSL_CERT_DIR": work / "ca-dir"}, ["ok"]),
        ("another CA", f"wss://localhost:{tls}", {"SSL_CERT_FILE": certs["other_ca"]}, ["fail", "not trusted"]),
        ("another name", f"wss://127.0.0.1:{tls}", {"SSL_CERT_FILE": certs["ca"]}, ["fail", "another name"]),
        ("no certificates", f"wss://localhost:{tls}", {"SSL_CERT_FILE": work / "empty.pem"},
         ["fail", "no certificates in SSL_CERT_FILE"]),
        ("ws:// to the TLS front", f"ws://localhost:{tls}", {"SSL_CERT_FILE": certs["ca"]},
         ["fail", "the server closed the connection"]),
        ("wss:// to the plain server", f"wss://localhost:{plain}", {"SSL_CERT_FILE": certs["ca"]},
         ["fail", "the server"]),
    ]
    failures = 0
    try:
        for name, address, trust, expect in cases:
            env = {k: v for k, v in os.environ.items() if k not in ("SSL_CERT_FILE", "SSL_CERT_DIR")}
            env.update({k: str(v) for k, v in trust.items()})
            started = time.monotonic()
            result = subprocess.run([args.probe, address, *expect], env=env, capture_output=True, text=True,
                                    timeout=60)
            seconds = time.monotonic() - started
            answer = (result.stdout + result.stderr).strip().replace("\n", " | ")
            ok = result.returncode == 0
            failures += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {name} ({seconds:.1f} s): {answer}")
    finally:
        front.stop()
        server.kill()
        server.wait()
        log.close()
    print("wss:// checks: " + ("FAILED" if failures else "passed"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
