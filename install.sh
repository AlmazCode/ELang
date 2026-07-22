#!/bin/bash
# ELang installer — builds and installs elc compiler + runtime libraries
# Usage: ./install.sh [--prefix=DIR] [--no-build] [--uninstall]
set -e

# ── Defaults ──────────────────────────────────────────────────────────
PREFIX="/usr/local"
BUILD=1
UNINSTALL=0
ELANG_VERSION="0.46.0"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ── Colors ────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${CYAN}[elang]${NC} $*"; }
ok()    { echo -e "${GREEN}[elang]${NC} $*"; }
warn()  { echo -e "${YELLOW}[elang]${NC} $*"; }
err()   { echo -e "${RED}[elang]${NC} $*" >&2; exit 1; }

# ── Parse args ────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --prefix=*)    PREFIX="${arg#--prefix=}" ;;
        --no-build)    BUILD=0 ;;
        --uninstall)   UNINSTALL=1 ;;
        --help|-h)
            echo "ELang Installer v${ELANG_VERSION}"
            echo ""
            echo "Usage: ./install.sh [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --prefix=DIR     Install prefix (default: /usr/local)"
            echo "  --no-build       Don't rebuild, use existing bin/elc"
            echo "  --uninstall      Remove installed files"
            echo "  -h, --help       Show this help"
            echo ""
            echo "Examples:"
            echo "  ./install.sh                        # Install to /usr/local"
            echo "  ./install.sh --prefix=\$HOME/.local  # Install to ~/.local"
            echo "  ./install.sh --uninstall            # Remove installation"
            exit 0 ;;
        *) err "Unknown option: $arg (use --help)" ;;
    esac
done

# ── Resolve prefix ────────────────────────────────────────────────────
mkdir -p "$PREFIX" 2>/dev/null || true
if [ -d "$PREFIX" ]; then
    PREFIX="$(cd "$PREFIX" && pwd)"
else
    err "Cannot create/access prefix directory: $PREFIX"
fi
BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/share/elang/lib"

# ── Detect if sudo needed ─────────────────────────────────────────────
SUDO=""
if [ ! -w "$BIN_DIR" ] 2>/dev/null && [ "$PREFIX" != "$HOME/.local" ]; then
    if command -v sudo &>/dev/null; then
        SUDO="sudo"
    else
        err "No write access to $BIN_DIR and sudo not available.\nTry: --prefix=\$HOME/.local"
    fi
fi

# ── Uninstall ─────────────────────────────────────────────────────────
if [ "$UNINSTALL" -eq 1 ]; then
    info "Uninstalling ELang from $PREFIX..."
    $SUDO rm -f  "$BIN_DIR/elc" "$BIN_DIR/elang" "$BIN_DIR/elc-wrapper"
    $SUDO rm -rf "$PREFIX/share/elang"
    ok "Removed: $BIN_DIR/elc, $BIN_DIR/elang"
    ok "Removed: $PREFIX/share/elang/"
    ok "ELang uninstalled."
    exit 0
fi

# ── Banner ────────────────────────────────────────────────────────────
echo ""
echo "  ╔══════════════════════════════════════╗"
echo "  ║     ELang Installer v${ELANG_VERSION}          ║"
echo "  ╚══════════════════════════════════════╝"
echo ""

# ── Check dependencies ────────────────────────────────────────────────
missing=()
for cmd in gcc nasm ld; do
    command -v "$cmd" &>/dev/null || missing+=("$cmd")
done
if [ ${#missing[@]} -gt 0 ]; then
    err "Missing: ${missing[*]}\nInstall: sudo apt install build-essential nasm"
fi

# ── Build ─────────────────────────────────────────────────────────────
if [ "$BUILD" -eq 1 ]; then
    info "Building compiler..."
    # Clean build artifacts (ignore errors if bin/elc is locked)
    rm -f "$SCRIPT_DIR"/src/*.o 2>/dev/null || true
    make -C "$SCRIPT_DIR" -s 2>&1 | grep -v "cannot open output" || true

    # If make failed to produce bin/elc, build directly
    if [ ! -f "$SCRIPT_DIR/bin/elc" ]; then
        warn "make failed (bin/elc locked?), building directly..."
        gcc -Wall -Wextra -std=c11 -I"$SCRIPT_DIR/include" -D_GNU_SOURCE \
            -o /tmp/elc_build "$SCRIPT_DIR"/src/*.c 2>/dev/null
        cp /tmp/elc_build "$SCRIPT_DIR/bin/elc" 2>/dev/null || \
            cp /tmp/elc_build /tmp/elc_fallback
    fi

    info "Building runtime libraries..."
    make -C "$SCRIPT_DIR/lib" -s clean 2>/dev/null || true
    make -C "$SCRIPT_DIR/lib" -s 2>&1 | tail -1
else
    [ -f "$SCRIPT_DIR/bin/elc" ] || err "bin/elc not found. Run without --no-build."
    info "Using existing build: bin/elc"
fi

# Verify binary
ELC_BIN="$SCRIPT_DIR/bin/elc"
if [ ! -f "$ELC_BIN" ]; then
    # Try fallback
    if [ -f /tmp/elc_fallback ]; then
        ELC_BIN=/tmp/elc_fallback
    elif [ -f /tmp/elc_build ]; then
        ELC_BIN=/tmp/elc_build
    else
        err "Cannot find compiler binary. Try: make clean && make"
    fi
fi
file "$ELC_BIN" | grep -q ELF || err "Compiler binary is not valid ELF"

# Verify libraries
for lib in core std math; do
    [ -f "$SCRIPT_DIR/lib/build/$lib.o" ] || err "lib/build/$lib.o missing. Run: make -C lib"
done

# ── Install ───────────────────────────────────────────────────────────
info "Installing to $PREFIX..."

$SUDO mkdir -p "$BIN_DIR" "$LIB_DIR"

# Install binary
$SUDO install -m 755 "$ELC_BIN" "$BIN_DIR/elc"
ok "Binary: $BIN_DIR/elc"

# Install runtime libraries (.o + .asm)
for f in core std math; do
    $SUDO install -m 644 "$SCRIPT_DIR/lib/build/$f.o" "$LIB_DIR/$f.o"
    $SUDO install -m 644 "$SCRIPT_DIR/lib/$f.asm"   "$LIB_DIR/$f.asm"
done
ok "Libraries: $LIB_DIR/"

# Install elang wrapper script
$SUDO tee "$BIN_DIR/elang" >/dev/null <<'WRAPPER'
#!/bin/bash
# ELang compiler wrapper — compile+run or compile only
set -e

ELC="__ELC_PATH__"
ELANG_LIB_PATH="__LIB_DIR__"

COMPILE_ONLY=0; TYPE_CHECK=0; OUTPUT=""; INPUT=""; EXTRA_ARGS=""

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
            echo "ELang Compiler v__VERSION__"
            echo ""
            echo "Usage:"
            echo "  elang <file.el>            Compile and run"
            echo "  elang -c <file.el>         Compile only"
            echo "  elang -o <out> <file.el>   Compile with custom output name"
            echo "  elang -check <file.el>     Type check only"
            echo "  elang -t <file.el>         Show tokens"
            echo "  elang -a <file.el>         Show AST"
            echo "  elang -v, --version        Show version"
            echo "  elang -h, --help           Show this help"
            exit 0 ;;
        -v|--version) echo "elang v__VERSION__"; exit 0 ;;
        -c)           COMPILE_ONLY=1; shift ;;
        -check)       TYPE_CHECK=1; shift ;;
        -o)           OUTPUT="$2"; shift 2 ;;
        -t|-a|-l)     EXTRA_ARGS="$EXTRA_ARGS $1"; shift ;;
        -*)           echo "elang: unknown option '$1'"; exit 1 ;;
        *)            INPUT="$1"; shift ;;
    esac
done

[ -n "$INPUT" ] || { echo "Usage: elang <file.el>"; exit 1; }
[ -f "$INPUT" ] || { echo "elang: file not found: $INPUT"; exit 1; }

[ "$TYPE_CHECK" -eq 1 ] && { "$ELC" -check "$INPUT"; exit $?; }

if [ -z "$OUTPUT" ]; then
    BASENAME=$(basename "$INPUT" .el)
    OUTPUT="/tmp/elang_${BASENAME}_$$"
    CLEANUP=1
else
    OUTPUT="${OUTPUT%.el}"
    CLEANUP=0
fi

export ELANG_LIB_PATH
"$ELC" -o "$OUTPUT" $EXTRA_ARGS "$INPUT"

if [ "$COMPILE_ONLY" -eq 0 ]; then
    "$OUTPUT"
    EXIT_CODE=$?
    [ "$CLEANUP" -eq 1 ] && rm -f "$OUTPUT" "${OUTPUT}.asm" "${OUTPUT}.o" "${OUTPUT}.o.tmp"
    exit $EXIT_CODE
else
    echo "elang: output: $OUTPUT"
fi
WRAPPER

# Substitute paths in wrapper
$SUDO sed -i \
    -e "s|__ELC_PATH__|$BIN_DIR/elc|g" \
    -e "s|__LIB_DIR__|$LIB_DIR|g" \
    -e "s|__VERSION__|$ELANG_VERSION|g" \
    "$BIN_DIR/elang"
$SUDO chmod 755 "$BIN_DIR/elang"
ok "Wrapper: $BIN_DIR/elang"

# ── Done ──────────────────────────────────────────────────────────────
echo ""
ok "Installation complete!  v${ELANG_VERSION}"
echo ""
echo "  Installed:"
echo "    $BIN_DIR/elc       — compiler"
echo "    $BIN_DIR/elang     — wrapper (compile + run)"
echo "    $LIB_DIR/          — runtime"
echo ""
echo "  Quick start:"
echo "    elang hello.el"
echo "    elang -c hello.el"
echo ""
echo "  Uninstall:"
echo "    $0 --uninstall"
echo ""
