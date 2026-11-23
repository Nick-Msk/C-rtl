#!/usr/bin/env bash
# Install shell completion for ai-checker/ngutils make targets.
# Detects shell: zsh if present, else bash. Override: --shell=bash|zsh.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AI_CHECKER_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# Dispatcher sits next to ai-checker (kr_book/ngutils); override if not.
DISPATCHER_DIR="${AI_DISPATCHER_DIR_OVERRIDE:-}"
if [ -z "$DISPATCHER_DIR" ]; then
    DISPATCHER_DIR="$(cd "$AI_CHECKER_DIR/../ngutils" 2>/dev/null && pwd || echo "")"
fi

usage() {
    echo "usage: install-completion.sh [--shell=bash|zsh|auto]" >&2
    exit 1
}

SHELL_KIND="auto"
for a in "$@"; do
    case "$a" in
        --shell=bash) SHELL_KIND="bash" ;;
        --shell=zsh)  SHELL_KIND="zsh" ;;
        --shell=auto) SHELL_KIND="auto" ;;
        -h|--help)    usage ;;
        *) echo "unknown arg: $a" >&2; usage ;;
    esac
done

detect() {
    if [ "$SHELL_KIND" != "auto" ]; then echo "$SHELL_KIND"; return; fi
    case "$(basename "${SHELL:-}")" in
        zsh)  echo "zsh" ;;
        bash) echo "bash" ;;
        *)
            if command -v zsh >/dev/null 2>&1; then echo "zsh"; else echo "bash"; fi
            ;;
    esac
}

KIND="$(detect)"
echo "[install-completion] shell:        $KIND"
echo "[install-completion] ai-checker:   $AI_CHECKER_DIR"
[ -n "$DISPATCHER_DIR" ] && echo "[install-completion] dispatcher:   $DISPATCHER_DIR"

install_zsh() {
    local dest_dir="$HOME/.zsh/completions"
    mkdir -p "$dest_dir"
    cp "$SCRIPT_DIR/_make" "$dest_dir/_make"
    local rc="$HOME/.zshrc"
    touch "$rc"
    if ! grep -q "AI_CHECKER_DIR" "$rc"; then
        cat >> "$rc" <<EOF

# --- ai-checker completion (added by scripts/install-completion.sh) ---
export AI_CHECKER_DIR="$AI_CHECKER_DIR"
export AI_DISPATCHER_DIR="$DISPATCHER_DIR"
fpath=("$dest_dir" \$fpath)
autoload -Uz compinit && compinit
EOF
    fi
    echo "[install-completion] installed -> $dest_dir/_make"
    echo "[install-completion] run: exec zsh"
}

install_bash() {
    local dest="$HOME/.bash_completion.d/ai-checker-make.bash"
    mkdir -p "$(dirname "$dest")"
    cp "$SCRIPT_DIR/make-completion.bash" "$dest"
    local rc="$HOME/.bashrc"
    touch "$rc"
    if ! grep -q "AI_CHECKER_DIR" "$rc"; then
        cat >> "$rc" <<EOF

# --- ai-checker completion (added by scripts/install-completion.sh) ---
export AI_CHECKER_DIR="$AI_CHECKER_DIR"
export AI_DISPATCHER_DIR="$DISPATCHER_DIR"
[ -f "$dest" ] && source "$dest"
EOF
    fi
    echo "[install-completion] installed -> $dest"
    echo "[install-completion] run: source ~/.bashrc"
}

case "$KIND" in
    zsh)  install_zsh ;;
    bash) install_bash ;;
esac

