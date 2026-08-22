#include "tempify/cli/ShellCompletion.h"

#include "tempify/support/Errors.h"

#include <string>

namespace tempify {

namespace {

std::string bash_completion() {
    return R"BASH(_tempify_template_ids() {
    tempify list 2>/dev/null | cut -f1
}

_tempify() {
    local cur cmd
    cur="${COMP_WORDS[COMP_CWORD]}"
    cmd="${COMP_WORDS[1]}"

    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "list info doctor completion validate inspect lint test refresh reapply process --help --version $(_tempify_template_ids)" -- "$cur") )
        return
    fi

    case "$cmd" in
        info|validate|inspect|lint|test)
            if [[ ${COMP_CWORD} -eq 2 ]]; then
                COMPREPLY=( $(compgen -W "$(_tempify_template_ids)" -- "$cur") )
                return
            fi
            COMPREPLY=( $(compgen -W "--json -h --help" -- "$cur") )
            ;;
        list|doctor|refresh)
            COMPREPLY=( $(compgen -W "--json -h --help" -- "$cur") )
            ;;
        completion)
            if [[ ${COMP_CWORD} -eq 2 ]]; then
                COMPREPLY=( $(compgen -W "bash zsh fish" -- "$cur") )
                return
            fi
            COMPREPLY=( $(compgen -W "-h --help" -- "$cur") )
            ;;
        reapply)
            if [[ ${COMP_CWORD} -eq 2 ]]; then
                COMPREPLY=( $(compgen -W "$(_tempify_template_ids)" -- "$cur") )
                return
            fi
            COMPREPLY=( $(compgen -W "--set --var --answers --write-answers --non-interactive --strict --hook-timeout-ms --report --json --tui -f -s --accept-hooks --no-hooks -h --help" -- "$cur") )
            ;;
        process)
            ;;
        *)
            COMPREPLY=( $(compgen -W "--set --var --answers --write-answers --non-interactive --strict --hook-timeout-ms --dry-run --plan-json --diff --reapply --report --json --tui -f -s --accept-hooks --no-hooks -q --questions -h --help" -- "$cur") )
            ;;
    esac
}

complete -F _tempify tempify
)BASH";
}

std::string zsh_completion() {
    return R"ZSH(#compdef tempify

_tempify_template_ids() {
    reply=("${(@f)$(tempify list 2>/dev/null | cut -f1)}")
}

_tempify() {
    local -a commands shells
    commands=(list info doctor completion validate inspect lint test refresh reapply process)
    shells=(bash zsh fish)
    _tempify_template_ids

    if (( CURRENT == 2 )); then
        compadd -- --help --version $commands $reply
        return
    fi

    case "$words[2]" in
        info|validate|inspect|lint|test)
            if (( CURRENT == 3 )); then
                compadd -- $reply
                return
            fi
            compadd -- --json -h --help
            ;;
        list|doctor|refresh)
            compadd -- --json -h --help
            ;;
        completion)
            if (( CURRENT == 3 )); then
                compadd -- $shells
                return
            fi
            compadd -- -h --help
            ;;
        reapply)
            if (( CURRENT == 3 )); then
                compadd -- $reply
                return
            fi
            compadd -- --set --var --answers --write-answers --non-interactive --strict --hook-timeout-ms --report --json --tui -f -s --accept-hooks --no-hooks -h --help
            ;;
        process)
            ;;
        *)
            compadd -- --set --var --answers --write-answers --non-interactive --strict --hook-timeout-ms --dry-run --plan-json --diff --reapply --report --json --tui -f -s --accept-hooks --no-hooks -q --questions -h --help
            ;;
    esac
}

compdef _tempify tempify
)ZSH";
}

std::string fish_completion() {
    return R"FISH(complete -c tempify -f
complete -c tempify -n "__fish_use_subcommand" -a "list info doctor completion validate inspect lint test refresh reapply process"
complete -c tempify -n "__fish_use_subcommand" -a "(tempify list 2>/dev/null | cut -f1)"
complete -c tempify -n "__fish_use_subcommand" -s h -l help -d "Displays help"
complete -c tempify -n "__fish_use_subcommand" -l version -d "Prints Tempify version"
complete -c tempify -n "__fish_seen_subcommand_from list doctor refresh info validate inspect lint test" -l json -d "Outputs machine-readable JSON"
complete -c tempify -n "__fish_seen_subcommand_from completion" -a "bash zsh fish"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l set -d "Sets template variable"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l var -d "Alias for --set"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l answers -d "Loads answer values from JSON file"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l write-answers -d "Writes resolved answers to JSON file"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l non-interactive -d "Fails instead of prompting"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l strict -d "Rejects unknown or invalid answers"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l hook-timeout-ms -d "Aborts hook phases after timeout"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l diff -d "Compares managed output without writing files"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l reapply -d "Applies safe managed-file updates when lock permits"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l report -d "With --reapply, prints report only"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l json -d "Outputs machine-readable render report"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l dry-run -d "Shows build plan without writing files"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l plan-json -d "Outputs build plan as JSON"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l tui -d "Uses wizard frontend"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l accept-hooks -d "Controls whether template hooks run"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -l no-hooks -d "Alias for --accept-hooks no"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -s f -l overwrite-if-exists -d "Replaces conflicting files"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -s s -l skip-if-file-exists -d "Skips conflicting files"
complete -c tempify -n "not __fish_seen_subcommand_from list info doctor completion validate inspect lint test refresh reapply process" -s q -l questions -d "Shows question overview"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -a "(tempify list 2>/dev/null | cut -f1)"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l set -d "Sets template variable"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l var -d "Alias for --set"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l answers -d "Loads answer values from JSON file"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l write-answers -d "Writes resolved answers to JSON file"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l non-interactive -d "Fails instead of prompting"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l strict -d "Rejects unknown or invalid answers"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l hook-timeout-ms -d "Aborts hook phases after timeout"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l report -d "Prints report only"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l json -d "Outputs machine-readable reapply report"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l tui -d "Uses wizard frontend"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l accept-hooks -d "Controls whether template hooks run"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -l no-hooks -d "Alias for --accept-hooks no"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -s f -l overwrite-if-exists -d "Replaces conflicting files"
complete -c tempify -n "__fish_seen_subcommand_from reapply" -s s -l skip-if-file-exists -d "Skips conflicting files"
)FISH";
}

} // namespace

std::string render_shell_completion(const std::string &shell) {
    if (shell == "bash") {
        return bash_completion();
    }
    if (shell == "zsh") {
        return zsh_completion();
    }
    if (shell == "fish") {
        return fish_completion();
    }
    throw TempifyError("Invalid shell for `completion`: " + shell + ". Expected bash, zsh, or fish.");
}

} // namespace tempify
