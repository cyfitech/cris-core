#!/bin/bash

set -e

for cmd in grep ps sed xargs; do
    if ! which "$cmd" >/dev/null 2>&1; then
        printf '\033[31m[ERROR] Missing command "%s".\033[0m\n' "$cmd" >&2
        exit 1
    fi
done

mkdir -p ~/'.ssh/config.d'
chmod go-w ~/'.ssh/config.d'

for repo in $(
    [ ! -d ~/'.ssh/github_deploy_cyfitech' ]                                                \
    || find ~/'.ssh/github_deploy_cyfitech' -name 'id_ed25519' -type f -not -name '*.pub'   \
    | xargs -rI{} dirname {}                                                                \
    | xargs -rI{} basename {}                                                               \
    | sort -u
); do
    mkdir -p ~/'.ssh/config.d/github_deploy_cyfitech.d'

    printf 'Match host github.com exec "set -e;
        pid=$$;
        until [ $pid -le 0 ]
            || ps ho args $pid
            | grep -v grep
            | grep ssh
            | grep '"'"'github\\.com'"'"'
            | grep cyfitech
            | grep '"'"'%s\\.git'"'"';
        do
            pid=$(ps ho ppid $pid | tr -d [[:space:]]);
        done;
        [ $pid -gt 0 ];
    "' "$repo" | tr '\n\r' ' ' | sed 's/  */ /g'                                        >  ~/".ssh/config.d/github_deploy_cyfitech.d/$repo.conf.partial"
    printf '\n'                                                                         >> ~/".ssh/config.d/github_deploy_cyfitech.d/$repo.conf.partial"
    printf '    IdentityFile %s/.ssh/github_deploy_cyfitech/%s/id_ed25519\n'  ~ "$repo" >> ~/".ssh/config.d/github_deploy_cyfitech.d/$repo.conf.partial"
    printf '    StrictHostKeyChecking no\n'                                             >> ~/".ssh/config.d/github_deploy_cyfitech.d/$repo.conf.partial"

    mv -f ~/".ssh/config.d/github_deploy_cyfitech.d/$repo.conf"{.partial,}
    chmod -R go-w ~/'.ssh/config.d/github_deploy_cyfitech.d'
    cat ~/".ssh/config.d/github_deploy_cyfitech.d/$repo.conf"

    printf 'Include %s/.ssh/config.d/github_deploy_cyfitech.d/*.conf\n'       ~         >  ~/".ssh/config.d/github_deploy_cyfitech.conf.partial"
    mv -f ~/".ssh/config.d/github_deploy_cyfitech.conf"{.partial,}
    chmod go-w ~/'.ssh/config.d/github_deploy_cyfitech.conf'
done

printf 'Host github.com\n'              >  ~/".ssh/config.d/github.conf.partial"
printf '    StrictHostKeyChecking no\n' >> ~/".ssh/config.d/github.conf.partial"
mv -f ~/".ssh/config.d/github.conf"{.partial,}
chmod go-w ~/'.ssh/config.d/github.conf'

touch ~/'.ssh/config'
grep -i '^[[:space:]]*Include[[:space:]][[:space:]]*.*config\.d/\*\.conf' ~/'.ssh/config' >/dev/null    \
|| printf 'Include %s/.ssh/config.d/*.conf\n' ~ >> ~/'.ssh/config'
chmod go-w ~/'.ssh' ~/'.ssh/config'

tree -gpuash ~/'.ssh'

printf '\033[32m[INFO] GitHub SSH key configured.\033[0m\n' >&2
