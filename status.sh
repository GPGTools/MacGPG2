function escape {
    printf "\033[%sm" $1
}

function underline {
    out=$(escape "4;$1")
    printf $out
}

function bold {
    out=$(escape "1;${1}")
    printf $out
}

function color {
    out=$(escape "0;$1")
    printf $out
}

function reset {
    out=$(escape "0")
    printf $out
}

function red {
    out=$(underline "31")
    printf $out
}

function blue {
    out=$(bold "34")
    printf $out
}

function black {
    out=$(bold "30")
    printf $out
}

function white {
    out=$(bold "39")
    printf $out
}

function green {
    out=$(color 92)
    printf $out
}

function status {
    echo -e $(printf "%s==>%s %s $1" "$(blue)" "$(reset)" '-')
}

function title {
    echo -e $(printf "%s==>%s $1%s" "$(blue)" "$(white)" "$(reset)")
}

function success {
    echo -e $(printf "%s==>%s $1%s" "$(green)" "$(white)" "$(reset)")
}

function error {
    echo -e $(printf "%sError%s: %s" "$(red)" "$(reset)" "$1")
}

