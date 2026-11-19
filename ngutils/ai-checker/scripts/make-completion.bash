# Usage: source scripts/make-completion.bash
# Or add to ~/.bashrc:
#   [ -f /path/to/ngutils/scripts/make-completion.bash ] && \
#       source /path/to/ngutils/scripts/make-completion.bash

_ngutils_make() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local targets
    targets=$(make _targets 2>/dev/null)
    COMPREPLY=($(compgen -W "$targets" -- "$cur"))
}
complete -F _ngutils_make make

