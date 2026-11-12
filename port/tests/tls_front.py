"""A TLS front for a local free-direct-server, as nginx is in front of a public one
(port/docs/FREE_DIRECT_SERVER.md): the wss:// tests of the desktop's TLS (net/tls.h) use it with
certificates made here by openssl. Used by net_tls.py and net_session.py --tls; never a server for
anything but 127.0.0.1.
"""
import asyncio
from pathlib import Path
import shutil
import ssl
import subprocess
import threading

SKIP = 77  # CTest's SKIP_RETURN_CODE for the TLS tests: no openssl on this machine


def openssl():
    return shutil.which("openssl")


def _run(*args, cwd):
    subprocess.run([openssl(), *args], cwd=cwd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def make_certs(work):
    """A test CA, a server certificate it signs for the name localhost only, and another CA that
    signs nothing here. Returns {"ca", "other_ca", "cert", "key"}: paths of PEM files."""
    work = Path(work)
    work.mkdir(parents=True, exist_ok=True)
    for name, cn in (("ca", "Blupi test CA"), ("other_ca", "Blupi other test CA")):
        _run("req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes",
             "-keyout", f"{name}.key", "-out", f"{name}.pem", "-days", "3650", "-subj", f"/CN={cn}",
             "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign",
             cwd=work)
    (work / "server.ext").write_text("basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\n"
                                     "extendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n", encoding="ascii")
    _run("req", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes", "-keyout", "server.key",
         "-out", "server.csr", "-subj", "/CN=localhost", cwd=work)
    _run("x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key", "-set_serial", "2",
         "-days", "3650", "-extfile", "server.ext", "-out", "server.pem", cwd=work)
    return {"ca": work / "ca.pem", "other_ca": work / "other_ca.pem",
            "cert": work / "server.pem", "key": work / "server.key"}


class TlsFront:
    """Accepts TLS on 127.0.0.1 (a free port: .port) and passes the bytes on to 127.0.0.1:upstream,
    in a thread of its own until stop()."""

    def __init__(self, cert, key, upstream):
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(str(cert), str(key))
        self.upstream = upstream
        self.loop = asyncio.new_event_loop()
        # A client that refuses the certificate ends the handshake; that is no error here.
        self.loop.set_exception_handler(lambda loop, context: None)
        started = threading.Event()
        self.thread = threading.Thread(target=self._serve, args=(started,), daemon=True)
        self.thread.start()
        if not started.wait(10):
            raise RuntimeError("the TLS front did not start")

    def _serve(self, started):
        asyncio.set_event_loop(self.loop)
        self.server = self.loop.run_until_complete(
            asyncio.start_server(self._connection, "127.0.0.1", 0, ssl=self.context))
        self.port = self.server.sockets[0].getsockname()[1]
        started.set()
        self.loop.run_forever()

    async def _connection(self, reader, writer):
        try:
            up_reader, up_writer = await asyncio.open_connection("127.0.0.1", self.upstream)
        except OSError:
            writer.close()
            return

        async def pump(source, target):
            try:
                while data := await source.read(65536):
                    target.write(data)
                    await target.drain()
            except (OSError, ssl.SSLError):
                pass
            finally:
                try:
                    target.close()
                except (OSError, RuntimeError):
                    pass

        await asyncio.gather(pump(reader, up_writer), pump(up_reader, writer))

    def stop(self):
        def close():
            self.server.close()
            self.loop.stop()
        self.loop.call_soon_threadsafe(close)
        self.thread.join(10)
