#include <raylib.h>
#include <print>
#include <vector>
#include <thread>
#include <inttypes.h>

typedef uint64_t u64;
typedef uint32_t u32;

constexpr u64 max_iter = 50;
Color palette[max_iter];

Rectangle graph_rec = {0, 0, 1900, 1200};
Rectangle screen_rec = graph_rec;
Image graph_image;
Color bg_color = BLACK;

bool show_info = false;
bool changed = true;

Rectangle compute_rec = {-2.f, 2.f, 4.f, 4.f};

Vector2 x_axis = {-2.f, 2.f};
Vector2 y_axis = {-2.f, 2.f};

void draw_axis(Rectangle draw_rec, float thicc = 1.f, Color color = WHITE) {
    Vector2 start_x = {0.f, draw_rec.height / 2.f};
    Vector2 end_x = {draw_rec.width, draw_rec.height / 2.f};
    float unit = draw_rec.width / compute_rec.width;
    int i = 0;
    //DrawLineEx(start_x, end_x, thicc, color); 
    ImageDrawLineEx(&graph_image, start_x, end_x, thicc, color); 
    while (unit * i <= graph_rec.width) {
        ImageDrawLineEx(&graph_image, 
                        {draw_rec.x + draw_rec.width / 2.f + unit * i, draw_rec.y + draw_rec.height / 2.f + unit / 10.f}, 
                        {draw_rec.x + draw_rec.width / 2.f + unit * i, draw_rec.y + draw_rec.height / 2.f - unit / 10.f}, 
                        thicc, RED);
        ImageDrawLineEx(&graph_image, 
                        {draw_rec.x + draw_rec.width / 2.f - unit * i, draw_rec.y + draw_rec.height / 2.f + unit / 10.f}, 
                        {draw_rec.x + draw_rec.width / 2.f - unit * i, draw_rec.y + draw_rec.height / 2.f - unit / 10.f}, 
                        thicc, RED);
        i++;
    }

    Vector2 start_y = {draw_rec.width / 2.f, draw_rec.height};
    Vector2 end_y = {draw_rec.width / 2.f, 0.f};
    //DrawLineEx(start_y, end_y, thicc, color); 
    //ImageDrawLineEx(&graph_image, start_y, end_y, thicc, color); 
}

Vector2 to_graph(Vector2 screen_point) {
    return Vector2(screen_point.x / screen_rec.width * compute_rec.width + compute_rec.x, 
                   -1.f * screen_point.y / screen_rec.height * compute_rec.height + compute_rec.y);
}

Vector2 to_screen(Vector2 graph_point) {
    return Vector2(graph_point.x / compute_rec.width * screen_rec.width + screen_rec.width / 2.f,
                   -1.f * graph_point.y / compute_rec.height * screen_rec.height + screen_rec.height / 2.f);
}

int in_mandelbrot_set(Vector2 point) {
    double max_dist = 2.f;
    if (point.x * point.x + point.y * point.y > max_dist * max_dist) return 1;

    int n = 0;
    Vector2 z = {0};
    double x_sqared = 0;
    double y_sqared = 0;

    for (; n < max_iter; ++n) {
        x_sqared = z.x * z.x;
        y_sqared = z.y * z.y;
        double x_tmp = x_sqared - y_sqared + point.x; 
        z.y = 2.f * z.x * z.y + point.y; 
        z.x = x_tmp; 
        if (x_sqared + y_sqared > max_dist * max_dist) {
            return n;
        }
    }
    return 0;
}

void draw_mandelbrot_image(Rectangle rec, Image* img) {
    Vector2 graph_top_left = to_graph(Vector2(rec.x, rec.y));
    Vector2 graph_point = graph_top_left;
    Vector2 unit = Vector2(1.f / graph_rec.width * compute_rec.width, 1.f / graph_rec.height * compute_rec.height); 

    for (int y = rec.y; y < rec.y + rec.height; ++y) {
        graph_point.x = graph_top_left.x;
        for (int x = rec.x; x < rec.x + rec.width; ++x) {
            int n = in_mandelbrot_set(graph_point);
            if (n > 0) {
                ImageDrawPixel(img, x, y, palette[n]);
            }
            graph_point.x += unit.x;
        }
        graph_point.y -= unit.y;
    }
}

void fill_palette() {
    Color start = RED;
    Color end = BLACK;
    Vector3 hsv = ColorToHSV(start);

    for(int i = 0; i < max_iter; ++i) {
        //palette[i] = ColorLerp(start, end, i / (float)max_iter);
        palette[i] = ColorFromHSV(hsv.x, hsv.y, hsv.z); 
        hsv.x += 360.f / max_iter;
    }   
}


int main() {
    InitWindow(screen_rec.width, screen_rec.height, "Test");
    SetTraceLogLevel(LOG_INFO);
    SetTargetFPS(120);

    graph_image = GenImageColor(graph_rec.width, graph_rec.height, bg_color);
    Texture graph_tex = LoadTextureFromImage(graph_image);

    fill_palette();

    u64 num_threads = std::thread::hardware_concurrency();
    TraceLog(LOG_INFO, "threads count = %d", num_threads);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(bg_color);
        if (0 && changed) {
            ImageDrawRectangleRec(&graph_image, graph_rec, bg_color);
            std::vector<std::thread> render_jobs;
            float width = graph_rec.width / num_threads;
            for (int i = 0; i < num_threads; ++i) {
                Rectangle rec = {i * width, 0.f, width, graph_rec.height};
                render_jobs.emplace_back(draw_mandelbrot_image, rec, &graph_image);
            }
            for (auto& t: render_jobs) {
                t.join();
            }
            //draw_mandelbrot_image(graph_rec, &graph_image);
            changed = false;
        }

        draw_axis(graph_rec);
        UpdateTexture(graph_tex, graph_image.data);
        DrawTexturePro(graph_tex, graph_rec, screen_rec, {0.f, 0.f}, 0.f, WHITE);


        if (IsKeyDown(KEY_UP)) {
            compute_rec.width--;
            compute_rec.height--;
            changed = true;
        }

        if (IsKeyDown(KEY_DOWN)) {
            compute_rec.width++;
            compute_rec.height++;
            changed = true;
        }

        if (show_info) {
            float text_size = 20;
            Vector2 mouse_pos = GetMousePosition();
            Vector2 graph_point = to_graph(mouse_pos);
            DrawText(TextFormat("%f, %f", graph_point.x, graph_point.y), 0, 0, text_size, WHITE);
        }
        DrawFPS(screen_rec.width - 50, screen_rec.height - 50);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
