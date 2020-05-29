/*
Copyright (C) 2020 by M Wang <wm@keepconn.com>

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>

// Escape Sequence: http://www.termsys.demon.co.uk/vtansi.htm

static char *
rainbow(char *dst, size_t dst_sz, double base, double freq, double cycle,
        bool truecolor, bool invert)
{
    static const char *fg_256color = "\033[38;5;%um";
    static const char *bg_256color = "\033[48;5;%um";
    static const char *fg_truecolor = "\033[38;2;%u;%u;%um";
    static const char *bg_truecolor = "\033[48;2;%u;%u;%um";

    uint8_t c, s, r, g, b;
    const char *fmt;
    int wlen;

    c = truecolor ? 128 : 3;
    s = c - 1;
    r = sin(base + freq * cycle) * s + c;
    g = sin(base + freq * cycle + 2 * M_PI / 3) * s + c;
    b = sin(base + freq * cycle + 4 * M_PI / 3) * s + c;

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

struct context {
    struct {
        unsigned seed;
        bool force_truecolor;
        bool force_lol;
        bool version;
        bool help;
    } args;
    struct {
        bool animate;
        bool invert;
        double spread;
        double freq;
        double vertical_freq;
        unsigned long animate_duration;
        double animate_speed;
    } config;
    struct {
        volatile bool terminate;
        bool truecolor;
        bool lol;
        double spread_inverse;
        unsigned long line_count;
        unsigned long char_count;
        double line_base;
        volatile unsigned short n_column;
        struct timespec animate_interval;
    } runtime;
};

static const char *fg_default = "\033[39m";
static const char *bg_default = "\033[49m";
static const char *hide_cursor = "\033[?25l";
static const char *show_cursor = "\033[?25h";

static void
print_plain(const char *data, unsigned long data_len, struct context *ctx)
{
    const char *default_esc = ctx->config.invert ? bg_default : fg_default;

    for (unsigned long i = 0; !ctx->runtime.terminate && i < data_len; i++) {
        char escape[32];
        const char *rainbow_escape;

        if (ctx->runtime.char_count == 0) {
            ctx->runtime.line_count++;
            ctx->runtime.line_base +=
                ctx->config.vertical_freq * ctx->runtime.spread_inverse;
        }

        if (data[i] == '\n') {
            printf("%s\n", default_esc);
            ctx->runtime.char_count = 0;
            continue;
        }

        rainbow_escape = rainbow(escape, sizeof(escape),
                ctx->runtime.line_base,
                ctx->config.freq,
                ctx->runtime.char_count * ctx->runtime.spread_inverse,
                ctx->runtime.truecolor,
                ctx->config.invert);
        assert(rainbow_escape != NULL);

        printf("%s%c", rainbow_escape, data[i]);

        ctx->runtime.char_count++;
    }
}

#define MIN(x, y) ({ typeof(x) xx = (x); typeof(y) yy = (y); (xx < yy) ? (xx) : (yy); })

static void
print_animate(const char *line, unsigned long line_len, struct context *ctx)
{
    static const char *save_cursor = "\0337";
    static const char *restore_cursor = "\0338";

    const char *default_esc = ctx->config.invert ? bg_default : fg_default;

    while (!ctx->runtime.terminate && line_len > 0) {
        unsigned long limit = MIN(line_len, ctx->runtime.n_column);
        unsigned long end;

        for (end = 0; end < limit && line[end] != '\n'; end++);

        ctx->runtime.line_count++;
        ctx->runtime.line_base +=
            ctx->config.vertical_freq * ctx->runtime.spread_inverse;

        printf("%s", save_cursor);

        for (unsigned long i = 0; i < ctx->config.animate_duration; i++) {
            double duration_base = ctx->config.spread * i;

            printf("%s", restore_cursor);

            for (unsigned long j = 0; j < end; j++) {
                char escape[32];
                const char *rainbow_escape;

                rainbow_escape = rainbow(escape, sizeof(escape),
                        ctx->runtime.line_base,
                        ctx->config.freq,
                        ctx->runtime.spread_inverse * j + duration_base,
                        ctx->runtime.truecolor,
                        ctx->config.invert);
                assert(rainbow_escape != NULL);

                printf("%s%c", rainbow_escape, line[j]);
            }

            nanosleep(&ctx->runtime.animate_interval, NULL);
        }

        printf("%s\n", default_esc);

        if (end < limit) {
            assert(line[end] == '\n');
            end++;
        }
        line = &line[end];
        line_len -= end;
    }
}

static int
lolcat(char **files, struct context *ctx)
{
    int eno = EXIT_FAILURE;
    FILE *src = NULL;
    size_t l_sz = 0;
    char *line = NULL;

    if (ctx->runtime.lol && ctx->config.animate) {
        printf("%s", hide_cursor);
    }

    while (*files) {
        const char *file = *(files++);
        ssize_t l_len;

        if (strcmp(file, "-") == 0) {
            src = stdin;
        } else {
            src = fopen(file, "r");
            if (src == NULL) {
                fprintf(stderr,
                        "fopen(%s) failed: %s\n",
                        file, strerror(errno));
                goto error;
            }
        }

        while (!ctx->runtime.terminate) {
            clearerr(src);
            l_len = getline(&line, &l_sz, src);
            if (l_len < 0) {
                if (feof(src)) {
                    break;
                } else {
                    assert(ferror(src));
                    fprintf(stderr,
                            "getline(%s) failed: %s\n",
                            file, strerror(errno));
                    goto error;
                }
                break;
            }

            if (!ctx->runtime.lol) {
                fwrite(line, 1, l_len, stdout);
            } else if (ctx->config.animate) {
                print_animate(line, l_len, ctx);
            } else {
                print_plain(line, l_len, ctx);
            }
        }

        if (src != stdin) {
            fclose(src);
            src = NULL;
        }
    }

    eno = EXIT_SUCCESS;

error:
    if (src && src != stdin) {
        fclose(src);
        src = NULL;
    }
    if (line) {
        free(line);
        line = NULL;
    }
    if (ctx->runtime.lol && ctx->config.animate) {
        printf("%s", show_cursor);
    }

    return eno;
}

static void
help(const char *exec_name, struct context *ctx)
{
    static const char *help_s = "\n"
            "Usage: %1$s [OPTION]... [FILE]...\n"
            "\n"
            "Concatenate FILE(s), or standard input, to standard output.\n"
            "With no FILE, or when FILE is -, read standard input.\n"
            "\n"
            "  -p, --spread=<f>      Rainbow spread (default: 3.0)\n"
            "  -F, --freq=<f>        Rainbow frequency (default: 0.1)\n"
            "  -V, --vertical=<f>    Rainbow vertical frequency (default: 1.0)\n"
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
            "lolcat home page:  <https://github.com/wmil/lolcat/>\n"
            "Ruby (original):   <https://github.com/busyloop/lolcat/>\n"
            "Python:            <https://github.com/tehmaze/lolcat/>\n"
            "Another C:         <https://github.com/jaseg/lolcat/>\n"
            "Report lolcat translation bugs to <http://speaklolcat.com/>\n";

    char msg[2048];
    int wlen = snprintf(msg, sizeof(msg), help_s, exec_name);
    assert((size_t) wlen < sizeof(msg));

    if (!ctx->runtime.lol) {
        printf("%s", msg);
    } else if (ctx->config.animate) {
        printf("%s", hide_cursor);
        print_animate(msg, strlen(msg), ctx);
        printf("%s", show_cursor);
    } else {
        print_plain(msg, strlen(msg), ctx);
    }
}

static unsigned short
n_column()
{
    const unsigned short default_ws_col = 80;

    int io;
    struct winsize w;

    if (!isatty(STDOUT_FILENO)) {
        return default_ws_col;
    }

    io = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (io == 0) {
        return w.ws_col;
    } else {
        fprintf(stderr,
                "ioctl() failed: %s\n",
                strerror(errno));
        return default_ws_col;
    }
}

static struct context *g_ctx = NULL;

static void
signal_action(int sig, siginfo_t *info __attribute__ ((unused)),
        void *ucontext __attribute__ ((unused)))
{
    switch (sig) {
        case SIGWINCH:
            g_ctx->runtime.n_column = n_column();
            break;
        case SIGINT:
            g_ctx->runtime.terminate = true;
            break;
    }
}

static int
signal_register(struct context *ctx)
{
    struct sigaction sa;

    g_ctx = ctx;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = &signal_action;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGWINCH, &sa, NULL);

    return 0;
}

static int
optparse(int argc, char **argv, struct context *ctx)
{
    static const char *optstring = ":p:F:V:S:ad:s:itfvh";
    static const struct option longopts[] ={
        { "spread",         required_argument,  NULL, 'p' },
        { "freq",           required_argument,  NULL, 'F' },
        { "vertical",       required_argument,  NULL, 'V' },
        { "seed",           required_argument,  NULL, 'S' },
        { "animate",        no_argument,        NULL, 'a' },
        { "duration",       required_argument,  NULL, 'd' },
        { "speed",          required_argument,  NULL, 's' },
        { "invert",         no_argument,        NULL, 'i' },
        { "truecolor",      no_argument,        NULL, 't' },
        { "force",          no_argument,        NULL, 'f' },
        { "version",        no_argument,        NULL, 'v' },
        { "help",           no_argument,        NULL, 'h' },
        { NULL,             no_argument,        NULL, '\0' },
    };

    extern char *optarg;
    extern int optind, opterr, optopt;

    char opt;
    int longindex;

    while ((opt = getopt_long(argc, argv, optstring, longopts, &longindex)) != -1) {
        switch (opt) {
            case 'p':
                ctx->config.spread = strtod(optarg, NULL);
                if (ctx->config.spread < 0.1) {
                    fprintf(stderr, "Error: argument '--spread' must be >= 0.1.\n");
                    return -1;
                }
                break;
            case 'F':
                ctx->config.freq = strtod(optarg, NULL);
                break;
            case 'V':
                ctx->config.vertical_freq = strtod(optarg, NULL);
                break;
            case 'S':
                ctx->args.seed = strtoul(optarg, NULL, 10);
                break;
            case 'a':
                ctx->config.animate = true;
                break;
            case 'd':
                ctx->config.animate_duration = strtoul(optarg, NULL, 10);
                if (ctx->config.animate_duration < 1) {
                    fprintf(stderr, "Error: argument '--duration' must be >= 1.\n");
                    return -1;
                }
                break;
            case 's':
                ctx->config.animate_speed = strtod(optarg, NULL);
                if (ctx->config.animate_speed < 0.1) {
                    fprintf(stderr, "Error: argument '--speed' must be >= 0.1.\n");
                    return -1;
                }
                break;
            case 'i':
                ctx->config.invert = true;
                break;
            case 't':
                ctx->args.force_truecolor = true;
                break;
            case 'f':
                ctx->args.force_lol = true;
                break;
            case 'v':
                ctx->args.version = true;
                break;
            case 'h':
                ctx->args.help = true;
                break;
            case '?':
                fprintf(stderr, "Error: unknown argument '%s'.\n", argv[optind - 1]);
                return -1;
            case ':':
                fprintf(stderr, "Error: missing argument '%s'.\n", argv[optind - 1]);
                return -1;
            default:
                fprintf(stderr, "Error: default '%s'.\n", optarg);
                return -1;
        };
    }

    return optind;
}

struct context default_ctx = {
    .args = {
        .seed = 0,
        .force_truecolor = false,
        .force_lol = false,
        .version = false,
        .help = false,
    },
    .config = {
        .animate = false,
        .invert = false,
        .spread = 3.0,
        .freq = 0.1,
        .vertical_freq = 1.0,
        .animate_duration = 12,
        .animate_speed = 20.0,
    },
    .runtime = {
        .terminate = false,
        .truecolor = false,
        .lol = true,
        .spread_inverse = 1.0 / 3.0,
        .line_count = 0,
        .char_count = 0,
        .line_base = 0.0,
        .n_column = 80,
        .animate_interval = {
            .tv_sec = 0,
            .tv_nsec = 50000000,
        },
    },
};

int
main(int argc, char **argv)
{
    // TODO:
    // 1. Input escape filter
    // 2. Wide character
    // 3. File operation error handling
    // 4. Precision optimization

    struct context ctx = default_ctx;
    int optind;

    if ((optind = optparse(argc, argv, &ctx)) < 0) {
        fprintf(stderr, "Try --help for help.\n");
        return EXIT_FAILURE;
    }

    if (ctx.args.seed == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ctx.args.seed = (unsigned) ts.tv_sec ^ (unsigned) (ts.tv_nsec >> 20);
    }
    ctx.runtime.line_base = M_PI * rand_r(&ctx.args.seed) / RAND_MAX;

    if (ctx.config.animate) {
        double interval_s, interval_fractional, interval_integral;
        interval_s = 1.0 / ctx.config.animate_speed;
        interval_fractional = modf(interval_s, &interval_integral);
        ctx.runtime.animate_interval.tv_sec = interval_integral;
        ctx.runtime.animate_interval.tv_nsec = interval_fractional * 1000000000L;
    }

    ctx.runtime.truecolor = ctx.args.force_truecolor;
    if (!ctx.runtime.truecolor) {
        const char *env_ct = getenv("COLORTERM");
        if (env_ct != NULL) {
            ctx.runtime.truecolor =
                strcmp(env_ct, "truecolor") == 0 ||
                strcmp(env_ct, "24bit") == 0;
        }
    }

    ctx.runtime.lol = isatty(STDOUT_FILENO) || ctx.args.force_lol;
    ctx.runtime.spread_inverse = 1.0 / ctx.config.spread;
    ctx.runtime.n_column = n_column();

    signal_register(&ctx);

    if (ctx.args.version) {
        static const char *version = "lolcat 0.0";
        printf("%s\n", version);
        return EXIT_SUCCESS;
    } else if (ctx.args.help) {
        help(argv[0], &ctx);
        return EXIT_SUCCESS;
    } else {
        static char *files_stdout[] = { "-", NULL };
        char **files;
        if (argv[optind] == NULL) {
            files = files_stdout;
        } else {
            files = &argv[optind];
        }
        return lolcat(files, &ctx);
    }
}
