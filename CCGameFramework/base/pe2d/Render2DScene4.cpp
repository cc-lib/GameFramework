﻿#include "stdafx.h"
#include "PhysicsEngine2D.h"
#include "render/Direct2DRenderTarget.h"
#include "Geometries2D.h"
#include "lua_ext/ext.h"

#define N 256
#define TRACE_POINT 1
#define TRACE_N 64
#define TRACE_NORMAL_MIN 1
#define TRACE_NORMAL_SCALE 0.02f
#define MAX_DEPTH 5
#define BIAS 1e-4f

extern float PI;
extern float PI2;

extern DrawSceneBag bag;

static std::shared_ptr<Geo2DObject> root;

static color beerLambert(color a, float d) {
    return color(expf(-a.r * d), expf(-a.g * d), expf(-a.b * d));
}

static float fresnel(float cosi, float cost, float etai, float etat) {
    const auto rs = (etat * cosi - etai * cost) / (etat * cosi + etai * cost);
    const auto rp = (etai * cosi - etat * cost) / (etai * cosi + etat * cost);
    return (rs * rs + rp * rp) * 0.5f;
}

static color trace4(vector2 o, vector2 d, int depth = 0) {
    const auto r = root->sample(o, d);
    if (r.body)
    {
        auto sum = r.body->L;
        if (depth < MAX_DEPTH ) {
            const auto I = d;
            const auto normal = r.inside ? -r.max_pt.normal : r.min_pt.normal;
            const auto pos = r.inside ? r.max_pt.position : r.min_pt.position;
            const auto idotn = DotProduct(I, normal);
            auto refl = r.body->R;
            if (r.body->eta > 0)
            {
                const auto eta = r.inside ? r.body->eta : (1.0f / r.body->eta);
                const auto k = 1.0f - eta * eta * (1.0f - idotn * idotn);
                if (k >= 0.0f) // 可以折射，不是全反射
                {
                    const auto a = eta * idotn + sqrtf(k);
                    const auto refraction = eta * d - a * normal;
                    const auto cosi = -(DotProduct(d, normal));
                    const auto cost = -(DotProduct(refraction, normal));
                    refl = refl * (r.inside ? fresnel(cosi, cost, eta, 1.0f) : fresnel(cosi, cost, 1.0f, eta));
                    refl.Normalize();
                    sum.Add((refl.Negative(1.0f)) * trace4(pos - BIAS * normal, refraction, depth + 1));
                }
                else // 不折射则为全内反射
                    refl.Set(1.0f);
            }
            if (refl.Valid())
            {
                const auto idotn2 = idotn * 2.0f;
                const auto reflect = I - normal * idotn2;
                sum.Add(refl * trace4(pos + BIAS * normal, reflect, depth + 1));
            }
        }
        return sum * beerLambert(r.body->S, r.inside ? r.max_pt.distance : r.min_pt.distance);
    }
    static color black;
    return black;
}

static color sample4(float x, float y) {
    color sum;
    for (auto i = 0; i < N; i++) {
        const auto a = PI2 * (i + float(rand()) / RAND_MAX) / N;
        const auto c = trace4(vector2(x, y), vector2(cosf(a), sinf(a)));
        sum.Add(c);
    }
    return sum * (1.0f / N);
}

#ifdef TRACE_POINT
static void setpixel(int x, int y)
{
    const auto pixel = &bag.g_buf[(y * bag.g_width + x) * 4];
    pixel[0] = 0;
    pixel[1] = 0;
    pixel[2] = 0;
    pixel[3] = 255;
}

// 画直线
// Refer: https://zhuanlan.zhihu.com/p/30553006
// Modified from https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
static void bresenham(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;

    while (setpixel(x0, y0), x0 != x1 || y0 != y1) {
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}
#endif

static int clamp(int value, int lower, int upper)
{
    return max(min(value, upper), lower);
}

static void DrawScene4(int part)
{
    auto buffer = bag.g_buf;
    auto width = bag.g_width;
    auto height = bag.g_height;
    auto m = min(width, height);
    for (auto y = 0; y < height; y++)
    {
        if (y % 4 == part)
        {
            for (auto x = 0; x < width; x++)
            {
                const auto color = sample4(float(x) / m, float(y) / m);
                buffer[0] = BYTE(fminf(color.b, 1.0f) * 255.0f);
                buffer[1] = BYTE(fminf(color.g, 1.0f) * 255.0f);
                buffer[2] = BYTE(fminf(color.r, 1.0f) * 255.0f);
                buffer[3] = 255;
                buffer += 4;
            }
        }
        else
            buffer += 4 * width;
    }
    bag.mtx.lock();
    bag.g_cnt++;
    if (bag.g_cnt == 4)
    {
#ifdef TRACE_POINT
        const auto _mouse_x = int(g_ui_map["CG-2-MOUSE-X"]);
        const auto _mouse_y = int(g_ui_map["CG-2-MOUSE-Y"]);
        const auto mouse_x = 1.0f * _mouse_x / m;
        const auto mouse_y = 1.0f * _mouse_y / m;
        const auto mouse_t = int(g_ui_map["CG-2-MOUSE-TYPE"]);
        if (0 <= mouse_x && 0 <= mouse_y && mouse_x < width && mouse_y < height)
        {
            for (auto i = 0; i < TRACE_N; i++) {
                const auto a = PI2 * (i + float(rand()) / RAND_MAX) / TRACE_N;
                const auto r = root->sample(vector2(mouse_x, mouse_y), vector2(cosf(a), sinf(a)));
                if (r.body)
                {
                    if (mouse_t == 1)
                    {
                        bresenham(clamp(_mouse_x, 0, width), clamp(_mouse_y, 0, height - 1),
                            clamp(int(r.min_pt.position.x * m), 0, width), clamp(int(r.min_pt.position.y * m), 0, height - 1));
#if TRACE_NORMAL_MIN == 1
                        bresenham(clamp(int(r.min_pt.position.x * m), 0, width), clamp(int(r.min_pt.position.y * m), 0, height - 1),
                            clamp(int((r.min_pt.position.x + r.min_pt.normal.x * TRACE_NORMAL_SCALE) * m), 0, width),
                            clamp(int((r.min_pt.position.y + r.min_pt.normal.y * TRACE_NORMAL_SCALE) * m), 0, height - 1));
#endif
                    }
                    else if (mouse_t == 2)
                    {
                        bresenham(clamp(_mouse_x, 0, width), clamp(_mouse_y, 0, height - 1),
                            clamp(int(r.max_pt.position.x * m), 0, width), clamp(int(r.max_pt.position.y * m), 0, height - 1));
                        bresenham(clamp(int(r.max_pt.position.x * m), 0, width), clamp(int(r.max_pt.position.y * m), 0, height - 1),
                            clamp(int((r.max_pt.position.x + r.max_pt.normal.x * TRACE_NORMAL_SCALE) * m), 0, width),
                            clamp(int((r.max_pt.position.y + r.max_pt.normal.y * TRACE_NORMAL_SCALE) * m), 0, height - 1));
                    }
                }
            }
        }
#endif
        *bag.g_painted = true;
    }
    bag.mtx.unlock();
}

void PhysicsEngine::Render2DScene4(CComPtr<ID2D1RenderTarget> rt, CRect bounds)
{
    scene = DrawScene4;

    // --------------------------------------
    // 场景设置
    if (!buf.get())
    {
        root =
            Geo2DOr(
                Geo2DAnd(
                    Geo2DNewCircle(1.1f, 0.2f, 0.4f, color(0.0f, 0.0f, 0.0f), color(0.2f, 0.2f, 0.2f), 1.5f, color(2.0f, 2.0f, 2.0f)),
                    Geo2DNewBox(1.1f, 0.4f, 0.4f, 0.2f, 0.0f, color(0.0f, 0.0f, 0.0f), color(0.2f, 0.2f, 0.2f), 1.5f, color(2.0f, 2.0f, 2.0f))),
                Geo2DOr(
                    Geo2DNewCircle(0.9f, -0.6f, 0.4f, color(3.0f, 0.2f, 0.2f)),
                    Geo2DNewCircle(1.3f, -0.6f, 0.4f, color(0.2f, 0.2f, 3.0f))));
    }

    // --------------------------------------

    RenderSceneIntern(rt, bounds);

    if (painted)
        root.reset();
}
