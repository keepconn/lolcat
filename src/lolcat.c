#include <stdio.h>

static const char *help_s = "\n"
        "Usage: %1$s [OPTION]... [FILE]...\n"
        "\n"
        "Concatenate FILE(s), or standard input, to standard output.\n"
        "With no FILE, or when FILE is -, read standard input.\n"
        "\n"
        "  -p, --spread=<f>      Rainbow spread (default: 3.0)\n"
        "  -F, --freq=<f>        Rainbow frequency (default: 0.1)\n"
        "  -S, --seed=<i>        Rainbow seed, 0 = random (default: 0)\n"
        "  -a, --animate         Enable psychedelics\n"
        "  -d, --duration=<i>    Animation duration (default: 12)\n"
        "  -s, --speed=<f>       Animation speed (default: 20.0)\n"
        "  -i, --invert          Invert fg and bg\n"
        "  -t, --truecolor       24-bit (truecolor)\n"
        "  -f, --force           Force color even when stdout is not a tty\n"
        "  -v, --version         Print version and exit\n"
        "  -h, --help            Show this message\n"
        "\n"
        "Examples:\n"
        "  %1$s f - g      Output f's contents, then stdin, then g's contents.\n"
        "  %1$s            Copy standard input to standard output.\n"
        "  fortune | %1$s  Display a rainbow cookie.\n"
        "\n"
        "Report lolcat bugs to <https://github.com/wmil/lolcat/issues>\n"
        "lolcat home page: <https://github.com/wmil/lolcat/>\n"
        "Ruby (original) implementation: <https://github.com/busyloop/lolcat/>\n"
        "Another C implementation: <https://github.com/jaseg/lolcat/>\n"
        "Report lolcat translation bugs to <http://speaklolcat.com/>\n";

static void
help(const char *exec_name)
{
    printf(help_s, exec_name);
}

int
main(int argc __attribute__((unused)), char **argv)
{
    help(argv[0]);
}
