/* Host check: rounded-rect SDF keeps interior solid (no hollow pill). */
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(cond, msg)                                                                         \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            fprintf(stderr, "FAIL: %s\n", msg);                                                   \
            fails++;                                                                             \
        } else {                                                                                 \
            printf("ok: %s\n", msg);                                                              \
        }                                                                                        \
    } while (0)

static float rr_sdf(float px, float py, float hw, float hh, float r)
{
    float dx = fabsf(px) - hw + r;
    float dy = fabsf(py) - hh + r;
    float ax = dx > 0.f ? dx : 0.f;
    float ay = dy > 0.f ? dy : 0.f;
    float inside = fmaxf(dx, dy);
    float corner = sqrtf(ax * ax + ay * ay);
    return (inside > 0.f ? corner : inside) - r;
}

static int coverage_count(int w, int h, int r)
{
    int x, y, n = 0;
    float cx = w * 0.5f, cy = h * 0.5f, hw = w * 0.5f, hh = h * 0.5f;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            float d = rr_sdf(x + 0.5f - cx, y + 0.5f - cy, hw, hh, (float)r);
            if (d <= 0.75f)
                n++;
        }
    }
    return n;
}

int main(void)
{
    int pill = coverage_count(168, 28, 14);
    int box = coverage_count(64, 90, 10);
    int area_pill = 168 * 28;
    int area_box = 64 * 90;

    /* Interior must be mostly filled — hollow bug left ~ends only (~15%). */
    CHECK(pill > (area_pill * 80) / 100, "pill coverage > 80%");
    CHECK(box > (area_box * 80) / 100, "cover coverage > 80%");

    /* Center pixel of pill must be inside */
    {
        float d = rr_sdf(0.f, 0.f, 84.f, 14.f, 14.f);
        CHECK(d < -1.f, "pill center deep inside");
    }

    if (fails)
        return 1;
    printf("all roundrect tests passed\n");
    return 0;
}
