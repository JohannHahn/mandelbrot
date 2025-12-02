#include <raylib.h>
#include <print>
#include <vector>
#include <thread>
#include <assert.h>


constexpr uint64_t max_iter = 500;
//Color palette[max_iter];
//
//Rectangle graph_rec = {0, 0, 1900, 1200};
//Rectangle screen_rec = graph_rec;
//Image graph_image;
//Color bg_color = BLACK;
//
//bool show_info = false;
//bool changed = true;

// Rectangle compute_rec = {-2.f, 2.f, 4.f, 4.f};

//
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
// screen_rec = graph_rec also ist screen point hier ok, aber falls es sich ändert muss man noch zusätzlich in onscreen_graph_space umrechnen
Vector2 to_graph(Vector2 screen_point, Rectangle graph_rec, Rectangle mandelbrot_rec) {
    return Vector2(screen_point.x / graph_rec.width * mandelbrot_rec.width + mandelbrot_rec.x, 
                   -1.f * screen_point.y / graph_rec.height * mandelbrot_rec.height + mandelbrot_rec.y);
}

void draw_mandelbrot_image(Rectangle mandelbrot_rec, Rectangle draw_rec, Rectangle graph_rec, Image* graph_image, Color* palette) {
    Vector2 graph_top_left = to_graph(Vector2(draw_rec.x, draw_rec.y), graph_rec, mandelbrot_rec);
    Vector2 graph_point = graph_top_left;
    Vector2 unit = Vector2(1.f / graph_rec.width * mandelbrot_rec.width, 1.f / graph_rec.height * mandelbrot_rec.height); 

    for (int y = draw_rec.y; y < draw_rec.y + draw_rec.height; ++y) {
        graph_point.x = graph_top_left.x;
        for (int x = draw_rec.x; x < draw_rec.x + draw_rec.width; ++x) {
            int n = in_mandelbrot_set(graph_point);
            if (n > 0) {
                ImageDrawPixel(graph_image, x, y, palette[n]);
            }
            graph_point.x += unit.x;
        }
        graph_point.y -= unit.y;
    }
}


struct Mandelbrot {
    Rectangle mandelbrot_rec = {-2.2f, 1.4f, 3.2f, 2.8f};

};

struct Window {
    Rectangle graph_rec;
    Image graph_image;
    Color palette[max_iter];

    Rectangle menu_rec = {0};
    Vector2 screen_size;

    Texture graph_texture;
    Color bg_color;
    
    Image copy_img;
    Texture copy_texture;

    Vector2 to_screen(Vector2 graph_point, Rectangle mandelbrot_rec) {
        return Vector2(graph_point.x / mandelbrot_rec.width * screen_size.x + screen_size.x / 2.f,
                       -1.f * graph_point.y / mandelbrot_rec.height * screen_size.y + screen_size.y / 2.f);
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

    void draw_axis(const Mandelbrot& mandelbrot, float thicc = 1.f, Color color = WHITE) {
        Vector2 start_x = {0.f, graph_rec.height / 2.f};
        Vector2 end_x = {graph_rec.width, graph_rec.height / 2.f};
        float unit = graph_rec.width / mandelbrot.mandelbrot_rec.width;
        int i = 0;
        //DrawLineEx(start_x, end_x, thicc, color); 
        ImageDrawLineEx(&graph_image, start_x, end_x, thicc, color); 
        while (unit * i <= graph_rec.width) {
            ImageDrawLineEx(&graph_image, 
                            {graph_rec.x + graph_rec.width / 2.f + unit * i, graph_rec.y + graph_rec.height / 2.f + unit / 10.f}, 
                            {graph_rec.x + graph_rec.width / 2.f + unit * i, graph_rec.y + graph_rec.height / 2.f - unit / 10.f}, 
                            thicc, color);
            ImageDrawLineEx(&graph_image, 
                            {graph_rec.x + graph_rec.width / 2.f - unit * i, graph_rec.y + graph_rec.height / 2.f + unit / 10.f}, 
                            {graph_rec.x + graph_rec.width / 2.f - unit * i, graph_rec.y + graph_rec.height / 2.f - unit / 10.f}, 
                            thicc, color);
            i++;
        }

        Vector2 start_y = {graph_rec.width / 2.f, graph_rec.height};
        Vector2 end_y = {graph_rec.width / 2.f, 0.f};
        //DrawLineEx(start_y, end_y, thicc, color); 
        //ImageDrawLineEx(&graph_image, start_y, end_y, thicc, color); 
    }

    void begin_frame() {
        BeginDrawing();
        ClearBackground(bg_color);
    }

    void render_to_img(const Mandelbrot& mandelbrot, uint64_t num_threads) {
        ImageDrawRectangleRec(&graph_image, graph_rec, bg_color);
        std::vector<std::thread> render_jobs;
        float width = graph_rec.width / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            Rectangle draw_rec = {i * width, 0.f, width, graph_rec.height};
            render_jobs.emplace_back(draw_mandelbrot_image, mandelbrot.mandelbrot_rec, draw_rec, graph_rec, &graph_image, palette);
        }
        for (auto& t: render_jobs) {
            t.join();
        }

        draw_axis(mandelbrot);
        UpdateTexture(graph_texture, graph_image.data);
    }

    void draw_frame(const Mandelbrot& mandelbrot, Color tint) {

        DrawTexturePro(graph_texture, graph_rec, {0, 0, screen_size.x, screen_size.y}, {0.f, 0.f}, 0.f, tint);

        DrawFPS(screen_size.x - 50, screen_size.y - 50);
        EndDrawing();
    }

    //Vector2 to_graph(Vector2 screen_point) {
    //    return Vector2(screen_point.x / screen_rec.width * compute_rec.width + compute_rec.x, 
    //                   -1.f * screen_point.y / screen_rec.height * compute_rec.height + compute_rec.y);
    //}

    Vector2 to_screen(const Mandelbrot& mandelbrot, Vector2 graph_point) {
        return Vector2(graph_point.x / mandelbrot.mandelbrot_rec.width * screen_size.x + screen_size.x / 2.f,
                       -1.f * graph_point.y / mandelbrot.mandelbrot_rec.height * screen_size.y + screen_size.y / 2.f);
    }
};

struct App {
    Window window;
    Mandelbrot mandelbrot;
    bool new_input = true;
    bool show_info = false;
    uint64_t num_threads;

    void controls() {
        if (IsKeyPressed(KEY_UP)) {
            if (mandelbrot.mandelbrot_rec.width > 1.f) {
                mandelbrot.mandelbrot_rec.width--;
                mandelbrot.mandelbrot_rec.x += 0.5f;
            }
            if (mandelbrot.mandelbrot_rec.height > 1.f) {
                mandelbrot.mandelbrot_rec.height--;
                mandelbrot.mandelbrot_rec.y -= 0.5f;
            }
            new_input = true;
        }

        if (IsKeyPressed(KEY_DOWN)) {
            mandelbrot.mandelbrot_rec.width++;
            mandelbrot.mandelbrot_rec.height++;
            mandelbrot.mandelbrot_rec.y += 0.5f;
            mandelbrot.mandelbrot_rec.x -= 0.5f;
            new_input = true;
        }

        if (show_info) {
            float text_size = 20;
            Vector2 mouse_pos = GetMousePosition();
            Vector2 graph_point = to_graph(mouse_pos, window.graph_rec, mandelbrot.mandelbrot_rec);

            window.copy_img = ImageCopy(window.graph_image);

            ImageDrawText(&window.copy_img, TextFormat("%f, %f", graph_point.x, graph_point.y), 0, 0, text_size, WHITE);

            UpdateTexture(window.copy_texture, window.copy_img.data);
            DrawTexture(window.copy_texture, 0, 0, WHITE);
        }

    }

    void new_frame() {
        window.begin_frame();


        // render new view to graph_image
        if (new_input) {
            //render_to_img();
            window.render_to_img(mandelbrot, num_threads);
            new_input = false;
        }

        controls();
        // draw current view from graph_image on screen
        Color tint = WHITE;
        if (show_info) tint.a = 128;
        window.draw_frame(mandelbrot, tint);

    }

};

Window init_window(int width, int height, const char* title) {
    Window window;
    window.screen_size = {(float)width, (float)height};
    window.graph_rec = {0, 0, (float)width, (float)height};
    window.bg_color = BLACK;
    window.fill_palette();

    InitWindow(window.screen_size.x, window.screen_size.y, title);
    SetTraceLogLevel(LOG_INFO);
    SetTargetFPS(120);

    window.graph_image = GenImageColor(window.graph_rec.width, window.graph_rec.height, window.bg_color);
    window.graph_texture = LoadTextureFromImage(window.graph_image);

    window.copy_img = ImageCopy(window.graph_image);
    window.copy_texture = LoadTextureFromImage(window.copy_img);

    return window;
}



App init_app(int width, int height, const char* title) {
    App app;
    app.num_threads = std::thread::hardware_concurrency();

    app.window = init_window(width, height, title);

    return app;
}

int main() {
    App app = init_app(900, 600, "Mandelbrot");

    while(!WindowShouldClose()) {
        app.new_frame();
    }

    CloseWindow();
    return 0;
}
