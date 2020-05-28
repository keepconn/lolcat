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
        bool animate;
        bool invert;
        bool truecolor;
        double spread;
        double freq;
        double vertical_freq;
        unsigned long animate_duration;
        double animate_speed;
    } config;
    struct {
        double spread_inverse;
        unsigned long line_count;
        unsigned long char_count;
        double line_base;
        unsigned long n_column;
        struct timespec animate_interval;
    } runtime;
};

static const char *fg_default = "\033[39m";
static const char *bg_default = "\033[49m";

static void
print_plain(const char *data, unsigned long data_len, struct context *ctx)
{
    const char *default_esc = ctx->config.invert ? bg_default : fg_default;

    for (unsigned long i = 0; i < data_len; i++) {
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
                ctx->config.truecolor,
                ctx->config.invert);
        assert(rainbow_escape != NULL);

        printf("%s%c", rainbow_escape, data[i]);

        ctx->runtime.char_count++;
    }
}

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static void
print_animate(const char *line, unsigned long line_len, struct context *ctx)
{
    static const char *save_cursor = "\0337";
    static const char *restore_cursor = "\0338";

    const char *default_esc = ctx->config.invert ? bg_default : fg_default;

    while (line_len > 0) {
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
                        ctx->config.truecolor,
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
            "Report lolcat bugs to <https://github.com/wmil/lolcat/issues>\n"
            "lolcat home page: <https://github.com/wmil/lolcat/>\n"
            "Ruby (original) implementation: <https://github.com/busyloop/lolcat/>\n"
            "Another C implementation: <https://github.com/jaseg/lolcat/>\n"
            "Report lolcat translation bugs to <http://speaklolcat.com/>\n";

    char msg[2048];
    int wlen = snprintf(msg, sizeof(msg), help_s, exec_name);
    assert((size_t) wlen < sizeof(msg));

    print_plain(msg, strlen(msg), ctx);
}

static int
cat(char **files, struct context *ctx)
{
    static const char *hide_cursor = "\033[?25l";
    static const char *show_cursor = "\033[?25h";

    int eno = EXIT_FAILURE;
    FILE *src = NULL;
    size_t l_sz = 0;
    char *line = NULL;

    if (ctx->config.animate) {
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

        do {
            l_len = getline(&line, &l_sz, src);
            if (l_len < 0) {
                if (errno != 0) {
                    fprintf(stderr,
                            "getline(%s) failed: %s\n",
                            file, strerror(errno));
                    goto error;
                }
                break;
            }

            if (ctx->config.animate) {
                print_animate(line, l_len, ctx);
            } else {
                print_plain(line, l_len, ctx);
            }
        } while (true);

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
    if (ctx->config.animate) {
        printf("%s", show_cursor);
    }

    return eno;
}

struct context default_ctx = {
    .config = {
        .animate = false,
        .invert = false,
        .truecolor = true,
        .spread = 3.0,
        .freq = 0.1,
        .vertical_freq = 1.0,
        .animate_duration = 12,
        .animate_speed = 20.0,
    },
    .runtime = {
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
main(int argc __attribute__ ((unused)), char **argv)
{
    // TODO:
    // 1. Cat [Done]
    // 2. Terminal detection & SIGWINCH
    // 3. Options
    // 4. Clean up on signal
    // 5. Input escape filter
    // 6. Precision

    char my_buffer[2048], my_buffer2[2048];
    // const char *sample = "The quick brown fox jumps over the lazy dog";
    const char *sample = "那只敏捷的棕色狐狸🦊跳过了一只懒惰的狗🐶";
    unsigned seed;
    struct timespec ts;

    snprintf(my_buffer, sizeof(my_buffer), "%s\n%s\n%s\n%s\nThe quick brown ", sample, sample, sample, sample);
    snprintf(my_buffer2, sizeof(my_buffer2), "fox jumps over the lazy dog\n%s\n%s\n%s\n%s\n", sample, sample, sample, sample);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    seed = (unsigned) ts.tv_sec ^ (unsigned) (ts.tv_nsec >> 20);

    struct context ctx = default_ctx;

    ctx.config.animate = true;
    ctx.runtime.spread_inverse = 1.0 / ctx.config.spread;
    ctx.runtime.line_base = M_PI * rand_r(&seed) / RAND_MAX;

    double interval_s, interval_fractional, interval_integral;
    interval_s = 1.0 / ctx.config.animate_speed;
    interval_fractional = modf(interval_s, &interval_integral);
    ctx.runtime.animate_interval.tv_sec = interval_integral;
    ctx.runtime.animate_interval.tv_nsec = interval_fractional * 1000000000L;

    cat(&argv[1], &ctx);

    // help(argv[0], &ctx);

    return 0;
}
