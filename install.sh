#!/bin/bash
# ELang installer — builds and installs elc compiler + runtime libraries
# Usage: ./install.sh [--prefix=DIR] [--no-build] [--uninstall]

set -e

# ── Defaults ──────────────────────────────────────────────────────────
PREFIX="/usr/local"
BUILD=1
UNINSTALL=0
ELANG_VERSION="0.44.0"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ── Colors ────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

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
            echo "  ./install.sh                      # Install to /usr/local"
            echo "  ./install.sh --prefix=\$HOME/.local  # Install to ~/.local"
            echo "  ./install.sh --uninstall           # Remove installation"
            exit 0
            ;;
        *) err "Unknown option: $arg (use --help)" ;;
    esac
done

# ── Resolve prefix ────────────────────────────────────────────────────
PREFIX="$(mkdir -p "$PREFIX" && cd "$PREFIX" && pwd)"
BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/share/elang/lib"

# ── Uninstall ─────────────────────────────────────────────────────────
if [ "$UNINSTALL" -eq 1 ]; then
    info "Uninstalling ELang from $PREFIX..."
    rm -f "$BIN_DIR/elc"
    rm -rf "$PREFIX/share/elang"
    ok "Removed: $BIN_DIR/elc"
    ok "Removed: $PREFIX/share/elang/"
    ok "ELang uninstalled."
    exit 0
fi

# ── Check dependencies ────────────────────────────────────────────────
check_deps() {
    local missing=()
    for cmd in gcc nasm ld; do
        if ! command -v "$cmd" &>/dev/null; then
            missing+=("$cmd")
        fi
    done
    if [ ${#missing[@]} -gt 0 ]; then
        err "Missing required tools: ${missing[*]}\nInstall them:\n  sudo apt install build-essential nasm    # Debian/Ubuntu\n  sudo dnf install gcc nasm binutils       # Fedora"
    fi
}

# ── Build ─────────────────────────────────────────────────────────────
build() {
    if [ ! -f "$SCRIPT_DIR/bin/elc" ]; then
        BUILD=1
    fi

    if [ "$BUILD" -eq 1 ]; then
        info "Building compiler..."
        make -C "$SCRIPT_DIR" -s
        info "Building runtime libraries..."
        make -C "$SCRIPT_DIR/lib" -s
    else
        if [ ! -f "$SCRIPT_DIR/bin/elc" ]; then
            err "bin/elc not found. Run without --no-build."
        fi
        info "Using existing build: bin/elc"
    fi

    # Verify binary works
    if ! "$SCRIPT_DIR/bin/elc" --help &>/dev/null 2>&1; then
        # Binary might not support --help, just check it's a valid ELF
        if ! file "$SCRIPT_DIR/bin/elc" | grep -q "ELF"; then
            err "bin/elc is not a valid ELF binary. Try rebuilding."
        fi
    fi

    # Verify libraries exist
    for lib in core std math; do
        if [ ! -f "$SCRIPT_DIR/lib/build/$lib.o" ]; then
            err "lib/build/$lib.o not found. Run: make -C lib"
        fi
    done
}

# ── Install ───────────────────────────────────────────────────────────
install_files() {
    info "Installing to $PREFIX..."

    # Determine if we need sudo
    local SUDO=""
    if [ ! -w "$BIN_DIR" ] 2>/dev/null; then
        SUDO="sudo"
        info "Need sudo to write to $BIN_DIR"
    fi

    # Create directories
    $SUDO mkdir -p "$BIN_DIR"
    $SUDO mkdir -p "$LIB_DIR"

    # Install binary
    $SUDO install -m 755 "$SCRIPT_DIR/bin/elc" "$BIN_DIR/elc"
    ok "Installed binary: $BIN_DIR/elc"

    # Install runtime libraries
    for lib in core std math; do
        $SUDO install -m 644 "$SCRIPT_DIR/lib/build/$lib.o" "$LIB_DIR/$lib.o"
    done
    ok "Installed libraries: $LIB_DIR/"

    # Install source .asm files (useful for debugging / rebuilding)
    for asm in core std math; do
        $SUDO install -m 644 "$SCRIPT_DIR/lib/$asm.asm" "$LIB_DIR/$asm.asm"
    done
    ok "Installed sources:  $LIB_DIR/"

    # Create wrapper script
    $SUDO tee "$BIN_DIR/elang" >/dev/null <<WRAPPER
#!/bin/bash
# ELang compiler wrapper — compile+run or compile only
# Installed by ELang installer v${ELANG_VERSION}
# Binary: $BIN_DIR/elc
# Libraries: $LIB_DIR

set -e

ELC="$BIN_DIR/elc"
ELANG_LIB_PATH="$LIB_DIR"

usage() {
    echo "ELang Compiler v${ELANG_VERSION}"
    echo ""
    echo "Usage:"
    echo "  elang <file.el>            Compile and run"
    echo "  elang -c <file.el>         Compile only"
    echo "  elang -o <out> <file.el>   Compile with custom output name"
    echo "  elang -check <file.el>     Type check only"
    echo "  elang -t <file.el>         Show tokens"
    echo "  elang -a <file.el>         Show AST"
    echo "  elang -h                   Show this help"
}

COMPILE_ONLY=0
TYPE_CHECK=0
OUTPUT=""
INPUT=""
EXTRA_ARGS=""

while [ \$# -gt 0 ]; do
    case "\$1" in
        -h|--help) usage; exit 0 ;;
        -c) COMPILE_ONLY=1; shift ;;
        -check) TYPE_CHECK=1; shift ;;
        -o) OUTPUT="\$2"; shift 2 ;;
        -t|-a|-l) EXTRA_ARGS="\$EXTRA_ARGS \$1"; shift ;;
        -*) echo "elang: unknown option '\$1'"; usage; exit 1 ;;
        *) INPUT="\$1"; shift ;;
    esac
done

if [ -z "\$INPUT" ]; then
    usage
    exit 1
fi

if [ ! -f "\$INPUT" ]; then
    echo "elang: file not found: \$INPUT"
    exit 1
fi

# Type check
if [ "\$TYPE_CHECK" -eq 1 ]; then
    "\$ELC" -check "\$INPUT"
    exit \$?
fi

# Generate output name
if [ -z "\$OUTPUT" ]; then
    BASENAME=\$(basename "\$INPUT" .el)
    OUTPUT="/tmp/elang_\${BASENAME}_\$\$"
    CLEANUP=1
else
    OUTPUT="\${OUTPUT%.el}"
    CLEANUP=0
fi

# Compile
export ELANG_LIB_PATH
"\$ELC" -o "\$OUTPUT" \$EXTRA_ARGS "\$INPUT"

# Run unless compile-only
if [ "\$COMPILE_ONLY" -eq 0 ]; then
    "\$OUTPUT"
    EXIT_CODE=\$?
    if [ "\$CLEANUP" -eq 1 ]; then
        rm -f "\$OUTPUT" "\${OUTPUT}.asm" "\${OUTPUT}.o" "\${OUTPUT}.o.tmp"
    fi
    exit \$EXIT_CODE
else
    echo "elang: output: \$OUTPUT"
fi
WRAPPER
    $SUDO chmod 755 "$BIN_DIR/elang"
    ok "Installed wrapper: $BIN_DIR/elang"

    # Also install the original elc wrapper for backward compat
    $SUDO tee "$BIN_DIR/elc-wrapper" >/dev/null <<WRAPPER
#!/bin/bash
# Backward-compatible alias — calls the real elc binary directly
exec "$BIN_DIR/elc" "\$@"
WRAPPER
    $SUDO chmod 755 "$BIN_DIR/elc-wrapper"
}

# ── Main ──────────────────────────────────────────────────────────────
echo ""
echo "  ╔══════════════════════════════════════╗"
echo "  ║     ELang Installer v${ELANG_VERSION}          ║"
echo "  ╚══════════════════════════════════════╝"
echo ""

check_deps
build
install_files

echo ""
ok "Installation complete!"
echo ""
echo "  Installed files:"
echo "    $BIN_DIR/elc         (compiler binary)"
echo "    $BIN_DIR/elang       (wrapper: compile + run)"
echo "    $LIB_DIR/            (runtime libraries)"
echo ""
echo "  Quick start:"
echo "    elang hello.el       # compile and run"
echo "    elang -c hello.el    # compile only"
echo ""
echo "  Uninstall:"
echo "    $0 --uninstall"
echo ""
