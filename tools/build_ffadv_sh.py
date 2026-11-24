import base64
from pathlib import Path

ROOT = Path(r"C:\Users\runneradmin\Downloads\projecr")
REPO = ROOT / "ffadv-rapz"
PAYLOAD = ROOT / "ffadv-src/ffadv/libs/arm64-v8a/libsheyan.so"
INJECTOR = ROOT / "ffadv-build/injector/rapzinj"
OUT = REPO / "ffadv.sh"


def b64(path: Path) -> str:
    return base64.b64encode(path.read_bytes()).decode("ascii")


payload_b64 = b64(PAYLOAD)
injector_b64 = b64(INJECTOR)

script = f'''#!/system/bin/sh
# ============================================================
#  FULL RAPZ - FF ADV ptrace injector
#  Path   : /data/local/tmp/rap/ffadv.sh
#  Target : com.dts.freefireadv
#  Credit : FULL RAPZ
#  Tidak butuh Magisk / Zygisk. Cukup root + arm64-v8a.
#
#  sh /data/local/tmp/rap/ffadv.sh            -> inject
#  sh /data/local/tmp/rap/ffadv.sh uninstall  -> hapus file
# ============================================================

DIR="/data/local/tmp/rap"
PKG="com.dts.freefireadv"
PAYLOAD="$DIR/libsheyan.so"
INJECTOR="$DIR/rapzinj"

say()  {{ echo "[FULL RAPZ] $*"; }}
fail() {{ echo "[FULL RAPZ] ERROR: $*"; exit 1; }}

if [ "$(id -u)" -ne 0 ]; then
  fail "butuh root. jalankan lewat su."
fi

if [ "$1" = "uninstall" ] || [ "$1" = "remove" ]; then
  rm -f "$PAYLOAD" "$INJECTOR"
  say "file dihapus dari $DIR"
  exit 0
fi

if command -v base64 >/dev/null 2>&1; then
  DEC="base64 -d"
elif command -v toybox >/dev/null 2>&1; then
  DEC="toybox base64 -d"
elif command -v busybox >/dev/null 2>&1; then
  DEC="busybox base64 -d"
else
  fail "decoder base64 tidak ditemukan."
fi

mkdir -p "$DIR" || fail "gagal membuat $DIR"

say "menulis payload..."
$DEC <<'RAPZ_PAYLOAD_EOF' > "$PAYLOAD"
{payload_b64}
RAPZ_PAYLOAD_EOF
[ -s "$PAYLOAD" ] || fail "gagal menulis payload."

say "menulis injector..."
$DEC <<'RAPZ_INJECTOR_EOF' > "$INJECTOR"
{injector_b64}
RAPZ_INJECTOR_EOF
[ -s "$INJECTOR" ] || fail "gagal menulis injector."

chmod 0755 "$INJECTOR" || fail "gagal chmod injector."
chmod 0644 "$PAYLOAD" || fail "gagal chmod payload."

if ! pidof "$PKG" >/dev/null 2>&1; then
  say "menjalankan game..."
  monkey -p "$PKG" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
fi

say "menyuntik ke $PKG..."
"$INJECTOR" "$PAYLOAD" || fail "inject gagal. pastikan game sudah terbuka."

say "selesai. menu FULL RAPZ aktif."
say "name ESP + count ESP ON, menu bisa di-drag & scroll."
exit 0
'''

REPO.mkdir(parents=True, exist_ok=True)
OUT.write_text(script, encoding="utf-8", newline="\n")
print(OUT)
print(OUT.stat().st_size)
