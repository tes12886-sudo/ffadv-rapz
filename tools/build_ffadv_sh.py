import base64
from pathlib import Path

VERSION = "rapz-v10"

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
#  Build  : {VERSION}
#  Log    : /data/local/tmp/rap/inject.log
#  Update : GitHub Release latest (auto)
#  Tidak butuh Magisk / Zygisk. Cukup root + arm64-v8a.
#
#  sh /data/local/tmp/rap/ffadv.sh             -> inject
#  sh /data/local/tmp/rap/ffadv.sh uninstall   -> hapus file
#  sh /data/local/tmp/rap/ffadv.sh --no-update -> inject tanpa cek update
# ============================================================

BUILD="{VERSION}"
DIR="/data/local/tmp/rap"
CANON="$DIR/ffadv.sh"
PKG="com.dts.freefireadv"
PAYLOAD="$DIR/libsheyan.so"
INJECTOR="$DIR/rapzinj"
LOG="$DIR/inject.log"
NEW="$DIR/ffadv.sh.new"

RAW_BASE="https://raw.githubusercontent.com/tes12886-sudo/ffadv-rapz/main"
RELEASE_URL="https://github.com/tes12886-sudo/ffadv-rapz/releases/latest/download/ffadv.sh"
RAW_URL="$RAW_BASE/ffadv.sh"
API_URL="https://api.github.com/repos/tes12886-sudo/ffadv-rapz/releases/latest"

mkdir -p "$DIR" 2>/dev/null
: > "$LOG" 2>/dev/null

say()  {{
  _m="[FULL RAPZ] $*"
  echo "$_m" >> "$LOG" 2>/dev/null
  echo "$_m"
}}

dump_log() {{
  echo "----- inject.log -----"
  cat "$LOG" 2>/dev/null
  echo "----------------------"
}}

fail() {{
  _m="[FULL RAPZ] ERROR: $*"
  echo "$_m" >> "$LOG" 2>/dev/null
  echo "$_m"
  dump_log
  exit 1
}}

UPDATED=0
if [ "$1" = "--updated" ]; then UPDATED=1; shift; fi
if [ "$1" = "--no-update" ]; then shift; RAPZ_NO_UPDATE=1; fi

if [ "$(id -u)" -ne 0 ]; then
  fail "butuh root. jalankan lewat su."
fi

if [ "$1" = "uninstall" ] || [ "$1" = "remove" ]; then
  rm -f "$CANON" "$PAYLOAD" "$INJECTOR" "$LOG" "$NEW"
  say "file dihapus dari $DIR"
  exit 0
fi

# Selalu normalisasi ke /data/local/tmp/rap/ffadv.sh
if [ "$0" != "$CANON" ]; then
  if cp "$0" "$CANON" 2>/dev/null; then
    chmod 0755 "$CANON" 2>/dev/null
    say "script dinormalisasi: $0 -> $CANON"
    exec sh "$CANON" "$@"
  fi
fi

say "build=$BUILD"
say "uid=$(id -u) arch=$(uname -m) kernel=$(uname -r)"
say "script=$0"
say "log=$LOG"

fetch() {{
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --connect-timeout 5 --max-time 20 "$1"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O - -T 8 -t 1 "$1"
  elif command -v toybox >/dev/null 2>&1 && toybox wget >/dev/null 2>&1; then
    toybox wget -q -O - "$1"
  elif command -v busybox >/dev/null 2>&1 && busybox wget >/dev/null 2>&1; then
    busybox wget -q -O - "$1"
  else
    return 1
  fi
}}

if [ "$UPDATED" -ne 1 ] && [ "$RAPZ_NO_UPDATE" != "1" ]; then
  say "cek update GitHub Release..."
  REL_TAG=$(fetch "$API_URL" 2>/dev/null | tr -d '\\r' | grep -m1 '"tag_name"' | sed 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\\([^"]*\\)".*/\\1/')
  REMOTE_VER=""
  if [ -n "$REL_TAG" ]; then
    REMOTE_VER="rapz-${{REL_TAG#v}}"
    say "release terbaru: $REL_TAG -> $REMOTE_VER"
  fi
  if [ -z "$REMOTE_VER" ]; then
    REMOTE_VER=$(fetch "$RAW_BASE/VERSION" 2>/dev/null | tr -d ' \\r\\n' | head -1)
  fi

  if [ -n "$REMOTE_VER" ] && [ "$REMOTE_VER" != "$BUILD" ]; then
    say "update tersedia: $BUILD -> $REMOTE_VER"
    rm -f "$NEW"

    OK=0
    if fetch "$RELEASE_URL" > "$NEW" 2>/dev/null; then OK=1; fi
    if [ "$OK" -ne 1 ]; then
      rm -f "$NEW"
      if fetch "$RAW_URL" > "$NEW" 2>/dev/null; then OK=1; fi
    fi

    if [ "$OK" = "1" ] && [ -s "$NEW" ] \\
       && grep -q "#!/system/bin/sh" "$NEW" \\
       && grep -q "build=$REMOTE_VER" "$NEW"; then
      chmod 0755 "$NEW" 2>/dev/null
      mv -f "$NEW" "$CANON"
      say "update terpasang, restart dengan $BUILD -> $REMOTE_VER"
      exec sh "$CANON" --updated "$@"
    else
      say "update gagal verifikasi, tetap pakai $BUILD"
      rm -f "$NEW"
    fi
  else
    say "release sudah terbaru: $BUILD"
  fi
else
  say "update dilewati"
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
say "decoder=$DEC"

mkdir -p "$DIR" || fail "gagal membuat $DIR"

say "menulis payload..."
$DEC <<'RAPZ_PAYLOAD_EOF' > "$PAYLOAD"
{payload_b64}
RAPZ_PAYLOAD_EOF
[ -s "$PAYLOAD" ] || fail "gagal menulis payload."
say "payload=$(ls -l "$PAYLOAD" 2>/dev/null)"

say "menulis injector..."
$DEC <<'RAPZ_INJECTOR_EOF' > "$INJECTOR"
{injector_b64}
RAPZ_INJECTOR_EOF
[ -s "$INJECTOR" ] || fail "gagal menulis injector."
say "injector=$(ls -l "$INJECTOR" 2>/dev/null)"

chmod 0755 "$INJECTOR" || fail "gagal chmod injector."
chmod 0644 "$PAYLOAD" || fail "gagal chmod payload."

say "pidof sebelum: $(pidof "$PKG" 2>/dev/null)"
if ! pidof "$PKG" >/dev/null 2>&1; then
  say "menjalankan game..."
  monkey -p "$PKG" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
  sleep 3
  say "pidof sesudah: $(pidof "$PKG" 2>/dev/null)"
fi

say "menyuntik ke $PKG..."
"$INJECTOR" "$PAYLOAD" >> "$LOG" 2>&1
_rc=$?
say "injector exit=$_rc"

if [ $_rc -ne 0 ]; then
  fail "inject gagal. lihat log di atas."
fi

say "--- log injector ---"
cat "$LOG" 2>/dev/null
say "selesai. menu FULL RAPZ aktif."
say "name ESP + count ESP ON, menu bisa di-drag & scroll."
exit 0
'''

REPO.mkdir(parents=True, exist_ok=True)
OUT.write_text(script, encoding="utf-8", newline="\n")
(REPO / "VERSION").write_text(VERSION + "\n", encoding="utf-8", newline="\n")
print(OUT)
print(OUT.stat().st_size)
