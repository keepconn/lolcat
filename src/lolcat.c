#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static char *
rainbow_esc(char *dst, size_t dst_sz, double freq, int iter, bool truecolor, bool invert)
{
    static const char *fg_256color = "\x1b[38;5;%um";
    static const char *bg_256color = "\x1b[48;5;%um";
    static const char *fg_truecolor = "\x1b[38;2;%u;%u;%um";
    static const char *bg_truecolor = "\x1b[48;2;%u;%u;%um";

    uint8_t c, s, r, g, b;
    const char *fmt;
    int wlen;

    c = truecolor ? 128 : 3;
    s = c - 1;
    r = sin(freq * iter) * s + c;
    g = sin(freq * iter + 2 * M_PI / 3) * s + c;
    b = sin(freq * iter + 4 * M_PI / 3) * s + c;

    if (truecolor) {
        fmt = invert ? bg_truecolor : fg_truecolor;
        wlen = snprintf(dst, dst_sz, fmt, r, g, b);
    } else {
        uint8_t cube = 16 + r * 36 + g * 6 + b;
        fmt = invert ? bg_256color : fg_256color;
        wlen = snprintf(dst, dst_sz, fmt, cube);
    }

    return ((size_t ) wlen >= dst_sz) ? NULL : dst;
}

static void
println(const char *line, double freq, int init_iter, bool truecolor, bool invert)
{
    static const char *fg_default = "\x1b[39m";
    static const char *bg_default = "\x1b[49m";

    const char *default_esc;
    char esc_buf[32];

    default_esc = invert ? bg_default : fg_default;

    printf("%s%s%s\n", rainbow_esc(esc_buf, sizeof(esc_buf), freq, init_iter, truecolor, invert), line, default_esc);
}

struct lolcat_ctx {
    bool animate;
    bool invert;
    bool truecolor;
    unsigned int seed;
    double spread;
    double freq;
    double animate_duration;
    double animate_speed;
};

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
    unsigned seed;

    help(argv[0]);

    seed = (int) time(NULL);
    println("test", 0.1, rand_r(&seed), true, false); 
}
