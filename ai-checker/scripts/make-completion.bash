# Bash completion for ngutils/ai-checker make targets.
# Requires AI_CHECKER_DIR and AI_DISPATCHER_DIR (set by install-completion.sh).

_ai_fixlist_candidates() {
    # $1 = project name (may be empty for all projects)
    (cd "${AI_CHECKER_DIR:-.}" 2>/dev/null && \
     uv run src/fixlist_candidates.py "$1" 2>/dev/null) || true
}

_ngutils_make() {
    local cur="${COMP_WORDS[COMP_CWORD]}"

    # FIXLIST=<TAB> -> eligible finding keys
    case "$cur" in
        FIXLIST=*)
            local project="" w
            for w in "${COMP_WORDS[@]}"; do
                case "$w" in
                    ai-fix-propose-*) project="${w#ai-fix-propose-}" ;;
                    ai-fix-commit-*)  project="${w#ai-fix-commit-}"  ;;
                    ai-fix-*)          project="${w#ai-fix-}"          ;;
                esac
            done
            local keys
            keys=$(_ai_fixlist_candidates "$project")
            COMPREPLY=($(compgen -W "$keys" -- "${cur#FIXLIST=}"))
            return
            ;;
    esac

    # default: make targets from the dispatcher
    local targets
    targets=$(cd "${AI_DISPATCHER_DIR:-.}" 2>/dev/null && \
              make _targets 2>/dev/null)
    COMPREPLY=($(compgen -W "$targets" -- "$cur"))
}

complete -F _ngutils_make make

