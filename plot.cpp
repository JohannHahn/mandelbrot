#include <raylib.h>
#include <raymath.h>
#include <print>
#include <vector>
#include <thread>
#include <mpfr.h>
#include <string>
#include <format>
#include <assert.h>

constexpr uint64_t max_iter_initial = 100;
constexpr uint64_t float_precision = 128;
constexpr uint64_t window_width = 600;
constexpr uint64_t window_height = 400;
constexpr uint64_t max_threads = 64;

enum ComputeMode {
    DOUBLE,
    MPFR
};

struct Vector2D {
    double x;
    double y;
};

struct RectangleD {
    double x;
    double y;
    double width;
    double height;
};

struct Vector2AP {
    mpfr_t x;
    mpfr_t y;

    void init() {
        mpfr_init(x);
        mpfr_init(y);
    }
};

struct RectangleAP {
    mpfr_t x;
    mpfr_t y;
    mpfr_t width;
    mpfr_t height;

    void init() {
        mpfr_init(x);
        mpfr_init(y);
        mpfr_init(width);
        mpfr_init(height);
    }
};

struct DrawVectors {
    Vector2AP unit;
    Vector2AP top_left;
    Vector2AP graph_point;

    void init() {
        unit.init();
        top_left.init();
        graph_point.init();
    }
};

struct MandelbrotVectors {
    Vector2AP z;
    Vector2AP square;
    mpfr_t tmp;

    void init() {
        z.init();
        square.init();
        mpfr_init(tmp);
    }
};


DrawVectors thread_draw_vectors[max_threads] = {0};
MandelbrotVectors thread_mandelbrot_vectors[max_threads] = {0};

Vector2AP temp;
mpfr_t dif_halved;
mpfr_t tmp;


template <class T>
void print_rec(T rec, const char* name = "rec") {
    std::println("{} = {}, {}, {}, {}", name, rec.x, rec.y, rec.width, rec.height);
}

template <class T>
void print_vec(T vec, const char* name = "vec") {
    std::println("{} = {}, {}", name, vec.x, vec.y);
}

void zoom_on_center(RectangleD& rec, float zoom_factor = 1.1f) {
    if (zoom_factor <= 0) return;

    double new_width = rec.width * zoom_factor;

    rec.x += (rec.width - new_width) / 2.f;
    rec.width = new_width;

    double new_height = rec.height * zoom_factor;  
    rec.y -= (rec.height - new_height) / 2.f;
    rec.height = new_height;  
}

void zoom_on_center(RectangleAP& rec, float zoom_factor = 1.1f) {
    if (zoom_factor <= 0) return;

    mpfr_mul_d(tmp, rec.width, zoom_factor, MPFR_RNDN);

    //rec.x += (rec.width - new_width) / 2.f;
    mpfr_sub(dif_halved, rec.width, tmp, MPFR_RNDN);
    mpfr_div_d(dif_halved, dif_halved, 2.f, MPFR_RNDN);
    mpfr_add(rec.x, dif_halved, rec.x, MPFR_RNDN);

    mpfr_set(rec.width, tmp, MPFR_RNDN);


   // new_height = rec.height * zoom_factor;  
   // rec.y -= (rec.height - new_height) / 2.f;
   // rec.height = new_height;  
    
    mpfr_mul_d(tmp, rec.height, zoom_factor, MPFR_RNDN);
    mpfr_sub(dif_halved, rec.height, tmp, MPFR_RNDN);

    mpfr_div_d(dif_halved, dif_halved, 2.f, MPFR_RNDN);
    mpfr_sub(rec.y, rec.y, dif_halved, MPFR_RNDN);

    mpfr_set(rec.height, tmp, MPFR_RNDN);

}

void center_on_point(const Vector2D& point, RectangleD& rec) {
    rec.x = point.x - rec.width / 2.f;
    rec.y = point.y + rec.height / 2.f;
}

void center_on_point(const Vector2AP& point, RectangleAP& rec) {
    
    //rec.x = point.x - rec.width / 2.f;
    mpfr_div_d(tmp, rec.width, 2.f, MPFR_RNDN);
    mpfr_sub(rec.x, point.x, tmp, MPFR_RNDN);

    //rec.y = point.y + rec.height / 2.f;
    mpfr_div_d(tmp, rec.height, 2.f, MPFR_RNDN);
    mpfr_add(rec.y, point.y, tmp, MPFR_RNDN);
}

int in_mandelbrot_set(const Vector2D& point, uint64_t max_iter) {
    double max_dist = 2.f;

    if (point.x * point.x + point.y * point.y > max_dist * max_dist) return 1;
    int n = 0;

    Vector2D z = {0};
    double x_squared, y_squared;

    for (; n < max_iter; ++n) {
        x_squared = z.x * z.x;
        y_squared = z.y * z.y;

        z.y = (double)2.f * z.x * z.y + point.y; 
        z.x = x_squared - y_squared + point.x; 

        if (x_squared + y_squared > max_dist * max_dist) {
            return n;
        }
    }
    return 0;
}

// screen_rec = graph_rec also ist screen point hier ok, aber falls es sich ändert muss man noch zusätzlich in onscreen_graph_space umrechnen
void to_graph(Vector2& point, Rectangle graph_rec, RectangleD mandelbrot_rec) {
    point.x = point.x / graph_rec.width * mandelbrot_rec.width + mandelbrot_rec.x; 
    point.y = -1.f * point.y / graph_rec.height * mandelbrot_rec.height + mandelbrot_rec.y;
}

void to_graph(Vector2D& point, RectangleD graph_rec, RectangleD mandelbrot_rec) {
    point.x = point.x / graph_rec.width * mandelbrot_rec.width + mandelbrot_rec.x; 
    point.y = -1.f * point.y / graph_rec.height * mandelbrot_rec.height + mandelbrot_rec.y;
}

void draw_mandelbrot_image_d(RectangleD mandelbrot_rec, RectangleD draw_rec, RectangleD graph_rec, Image* graph_image, Color* palette, uint64_t max_iter) {
    // !! Immder die selben draw_recs -> vorberechnen ?
    Vector2D graph_top_left = {draw_rec.x, draw_rec.y};
    to_graph(graph_top_left, graph_rec, mandelbrot_rec);

    Vector2D graph_point = graph_top_left;

    Vector2D unit;
    unit.x = mandelbrot_rec.width / graph_rec.width; 
    unit.y = mandelbrot_rec.height / graph_rec.height; 


    for (int y = draw_rec.y; y < draw_rec.y + draw_rec.height; ++y) {
        graph_point.x = graph_top_left.x;
        for (int x = draw_rec.x; x < draw_rec.x + draw_rec.width; ++x) {
            int n = in_mandelbrot_set(graph_point, max_iter);
            if (n > 0) {
                ImageDrawPixel(graph_image, x, y, palette[n]);
            }
            graph_point.x += unit.x;
        }
        graph_point.y -= unit.y;
    }
}



int in_mandelbrot_set(const Vector2AP& point, MandelbrotVectors& vectors, uint64_t max_iter) {
    double max_dist = 2.f;
    mpfr_t& x_sqared = vectors.square.x;
    mpfr_t& y_sqared = vectors.square.y;

    //if (point.x * point.x + point.y * point.y > max_dist * max_dist) return 1;
    mpfr_mul(x_sqared, point.x, point.x, MPFR_RNDN);
    mpfr_mul(y_sqared, point.y, point.y, MPFR_RNDN);
    mpfr_add(x_sqared, x_sqared, y_sqared, MPFR_RNDN);


    if (mpfr_cmp_d(x_sqared, max_dist * max_dist) > 0) return 1;

    int n = 0;

    Vector2AP& z = vectors.z;
    mpfr_set_d(z.x, 0.f, MPFR_RNDN);
    mpfr_set_d(z.y, 0.f, MPFR_RNDN);

    mpfr_set_d(x_sqared, 0.f, MPFR_RNDN);
    mpfr_set_d(y_sqared, 0.f, MPFR_RNDN);

    //mpfr_t& x_tmp = vectors.tmp; 

    for (; n < max_iter; ++n) {
        mpfr_mul(x_sqared, z.x, z.x, MPFR_RNDN);
        mpfr_mul(y_sqared, z.y, z.y, MPFR_RNDN);

        //z.y = 2.f * z.x * z.y + point.y; 
        mpfr_mul(z.y, z.x, z.y, MPFR_RNDN);
        mpfr_mul_d(z.y, z.y, 2.f, MPFR_RNDN);
        mpfr_add(z.y, z.y, point.y, MPFR_RNDN);

        //x_tmp = x_sqared - y_sqared + point.x; 
        mpfr_sub(z.x, x_sqared, y_sqared, MPFR_RNDN);
        mpfr_add(z.x, z.x, point.x, MPFR_RNDN);


        //if (x_sqared + y_sqared > max_dist * max_dist) {
        mpfr_add(x_sqared, x_sqared, y_sqared, MPFR_RNDN);
        int cmp = mpfr_cmp_d(x_sqared, max_dist * max_dist);
        if (cmp > 0) {
            return n;
        }
    }
    return 0;
}

Vector2 to_rec(Vector2 point, Rectangle from_rec, Rectangle to_rec) {
    return Vector2(point.x / from_rec.width * to_rec.width + to_rec.x,
                   point.y / from_rec.height * to_rec.height + to_rec.y);
}

Vector2D to_rec(Vector2D point, RectangleD from_rec, RectangleD to_rec) {
    return Vector2D(point.x / from_rec.width * to_rec.width + to_rec.x,
                   point.y / from_rec.height * to_rec.height + to_rec.y);
}

// screen_rec = graph_rec also ist screen point hier ok, aber falls es sich ändert muss man noch zusätzlich in onscreen_graph_space umrechnen
void to_graph(Vector2AP& point, RectangleD graph_rec, const RectangleAP& mandelbrot_rec) {
    //point.x = point.x / graph_rec.width * mandelbrot_rec.width + mandelbrot_rec.x; 
    mpfr_div_d(point.x, point.x, graph_rec.width, MPFR_RNDN);
    mpfr_mul(point.x, point.x, mandelbrot_rec.width, MPFR_RNDN);
    mpfr_add(point.x, point.x, mandelbrot_rec.x, MPFR_RNDN);

    //point.y = -1.f * point.y / graph_rec.height * mandelbrot_rec.height + mandelbrot_rec.y;
    mpfr_div_d(point.y, point.y, graph_rec.height, MPFR_RNDN);
    mpfr_mul(point.y, point.y, mandelbrot_rec.height, MPFR_RNDN);
    mpfr_mul_d(point.y, point.y, -1.f, MPFR_RNDN);
    mpfr_add(point.y, point.y, mandelbrot_rec.y, MPFR_RNDN);
}

void draw_mandelbrot_image(MandelbrotVectors& mandelbrot_vectors, DrawVectors& draw_vectors, const RectangleAP& mandelbrot_rec, Rectangle draw_rec, Rectangle graph_rec, Image* graph_image, Color* palette, uint64_t max_iter) {
    // !! Immder die selben draw_recs -> vorberechnen ?
    Vector2AP& graph_top_left = draw_vectors.top_left;
    mpfr_set_d(graph_top_left.x, draw_rec.x, MPFR_RNDN);
    mpfr_set_d(graph_top_left.y, draw_rec.y, MPFR_RNDN);
    to_graph(graph_top_left, (RectangleD){graph_rec.x, graph_rec.y, graph_rec.width, graph_rec.height}, mandelbrot_rec);

    Vector2AP& graph_point = draw_vectors.graph_point;
    mpfr_set(graph_point.x, graph_top_left.x, MPFR_RNDN);
    mpfr_set(graph_point.y, graph_top_left.y, MPFR_RNDN);

    Vector2AP& unit = draw_vectors.unit;
    //unit.x = 1.f / graph_rec.width * mandelbrot_rec.width; 
    //unit.y = 1.f / graph_rec.height * mandelbrot_rec.height; 
    mpfr_div_d(unit.x, mandelbrot_rec.width, graph_rec.width, MPFR_RNDN);
    mpfr_div_d(unit.y, mandelbrot_rec.height, graph_rec.height, MPFR_RNDN);


    for (int y = draw_rec.y; y < draw_rec.y + draw_rec.height; ++y) {
        mpfr_set(graph_point.x, graph_top_left.x, MPFR_RNDN);
        for (int x = draw_rec.x; x < draw_rec.x + draw_rec.width; ++x) {
            int n = in_mandelbrot_set(graph_point, mandelbrot_vectors, max_iter);
            if (n > 0) {
                ImageDrawPixel(graph_image, x, y, palette[n]);
            }
            mpfr_add(graph_point.x, graph_point.x, unit.x, MPFR_RNDN);
        }
        mpfr_sub(graph_point.y, graph_point.y, unit.y, MPFR_RNDN);
    }
}



struct Mandelbrot {
    RectangleAP mandelbrot_rec_mpfr;
    RectangleD mandelbrot_rec_d;
};

struct Window {
    Rectangle graph_rec;
    Image graph_image;
    std::vector<Color> palette;

    Rectangle menu_rec = {0};
    Vector2 screen_size;

    Texture graph_texture;
    Color bg_color;
    
    Image copy_img;
    Texture copy_texture;

    void fill_palette(uint64_t max_iter) {
        Color start = RED;
        Color end = BLACK;
        Vector3 hsv = ColorToHSV(start);
        palette.clear();
        palette.reserve(max_iter);
        for(int i = 0; i < max_iter; ++i) {
            //palette[i] = ColorLerp(start, end, i / (float)max_iter);
            palette.emplace_back(ColorFromHSV(hsv.x, hsv.y, hsv.z));
            hsv.x += 360.f / max_iter;
        }   
    }

    void draw_axis(RectangleD mandelbrot_rec, float thicc = 1.f, Color color = WHITE) {
        Vector2D zero = {std::abs(mandelbrot_rec.x), std::abs(mandelbrot_rec.y)};
        // flipped??
        RectangleD graph_rec_d = {graph_rec.x, graph_rec.y, graph_rec.width, graph_rec.height};
        zero = to_rec(zero, mandelbrot_rec, graph_rec_d);

        //Vector2 start_x = {0.f, graph_rec.height / 2.f};
        Vector2 start_x = {0.f, (float)zero.y};
        //Vector2 end_x = {graph_rec.width, graph_rec.height / 2.f};
        Vector2 end_x = {(float)graph_rec.width, (float)zero.y};

        //DrawLineEx(start_x, end_x, thicc, color); 
        ImageDrawLineEx(&graph_image, start_x, end_x, thicc, color); 
        
        //Vector2 start_y = {graph_rec.width / 2.f, graph_rec.height};
        //Vector2 end_y = {graph_rec.width / 2.f, 0.f};
        Vector2 start_y = {(float)zero.x, (float)graph_rec.height};
        Vector2 end_y = {(float)zero.x, 0.f};
        //DrawLineEx(start_y, end_y, thicc, color); 
        ImageDrawLineEx(&graph_image, start_y, end_y, thicc, color); 
    }

    void begin_frame() {
        BeginDrawing();
        ClearBackground(bg_color);
    }

    void render_to_img(const RectangleD& mandelbrot_rec, uint64_t num_threads, uint64_t max_iter) {
        std::vector<std::thread> render_jobs;

        ImageDrawRectangleRec(&graph_image, graph_rec, bg_color);

        float width = graph_rec.width / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            RectangleD draw_rec = {i * width, 0.f, width, graph_rec.height};
            RectangleD graph_rec_d = {graph_rec.x, graph_rec.y, graph_rec.width, graph_rec.height};
            render_jobs.emplace_back(draw_mandelbrot_image_d, 
                                     mandelbrot_rec, draw_rec, graph_rec_d, &graph_image, &palette[0], max_iter);
        }

        for (auto& t: render_jobs) {
            t.join();
        }

        draw_axis(mandelbrot_rec);

        UpdateTexture(graph_texture, graph_image.data);
    }


    void render_to_img(const RectangleAP& mandelbrot_rec, uint64_t num_threads, uint64_t max_iter) {
        std::vector<std::thread> render_jobs;

        ImageDrawRectangleRec(&graph_image, graph_rec, bg_color);

        float width = graph_rec.width / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            Rectangle draw_rec = {i * width, 0.f, width, graph_rec.height};
            render_jobs.emplace_back(draw_mandelbrot_image, 
                                     std::ref(thread_mandelbrot_vectors[i]), std::ref(thread_draw_vectors[i]), mandelbrot_rec, draw_rec, graph_rec, &graph_image, &palette[0], max_iter);
        }

        for (auto& t: render_jobs) {
            t.join();
        }

        //draw_axis(RectangleD(mpfr_get_d(mandelbrot_rec.x, MPFR_RNDN), mpfr_get_d(mandelbrot_rec.y, MPFR_RNDN)));

        UpdateTexture(graph_texture, graph_image.data);
    }

    void draw_frame(Color tint) {

        DrawTexturePro(graph_texture, graph_rec, {0, 0, (float)screen_size.x, (float)screen_size.y}, {0.f, 0.f}, 0.f, tint);

        DrawFPS(screen_size.x - 50, screen_size.y - 50);
        EndDrawing();
    }
};

struct App {
    Window window;
    Mandelbrot mandelbrot;
    bool new_input = true;
    bool show_info = false;
    uint64_t num_threads = 1;
    uint64_t max_iter = 500;
    ComputeMode compute_mode = DOUBLE;

    void controls() {
        float zoom_factor = 0.1f;

        if (IsKeyPressed(KEY_UP) || GetMouseWheelMove() > 0.f) {

            if (compute_mode == MPFR) {
                zoom_on_center(mandelbrot.mandelbrot_rec_mpfr, 1.f - zoom_factor);

            } else if (compute_mode == DOUBLE) {
                zoom_on_center(mandelbrot.mandelbrot_rec_d, 1.f - zoom_factor);
            }

            new_input = true;
        }

        if (IsKeyPressed(KEY_DOWN) || GetMouseWheelMove() < 0.f) {
            if (compute_mode == MPFR) {
                zoom_on_center(mandelbrot.mandelbrot_rec_mpfr, 1.f + zoom_factor);

            } else if (compute_mode == DOUBLE) {
                zoom_on_center(mandelbrot.mandelbrot_rec_d, 1.f + zoom_factor);
            }
            new_input = true;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), window.graph_rec)) {
            Vector2 mouse_pos = GetMousePosition();

            if (compute_mode == MPFR) {
                mpfr_set_d(temp.x, mouse_pos.x, MPFR_RNDN);
                mpfr_set_d(temp.y, mouse_pos.y, MPFR_RNDN);
                RectangleD graph_rec_d = {window.graph_rec.x, window.graph_rec.y, window.graph_rec.width, window.graph_rec.height};
                to_graph(temp, graph_rec_d, mandelbrot.mandelbrot_rec_mpfr);
                center_on_point(temp, mandelbrot.mandelbrot_rec_mpfr);

            } else if (compute_mode == DOUBLE) {
                //print_vec(mouse_pos, "mouse_pos screen space");
                to_graph(mouse_pos, window.graph_rec, mandelbrot.mandelbrot_rec_d);
                //print_vec(mouse_pos, "mouse_pos graph space");
                center_on_point({mouse_pos.x, mouse_pos.y}, mandelbrot.mandelbrot_rec_d);
            }

            new_input = true;
        }

        if (IsKeyPressed(KEY_SPACE)) {
            max_iter *= 2;
            window.fill_palette(max_iter);
            new_input = true;
        }

        if (show_info) {
            float text_size = 20;
            Vector2 mouse_pos = GetMousePosition();

            if (compute_mode == MPFR) {
                mpfr_set_d(temp.x, mouse_pos.x, MPFR_RNDN);
                mpfr_set_d(temp.y, mouse_pos.y, MPFR_RNDN);
                //Vector2AP mouse_pos_mandelbrot = to_graph(mouse_pos_ap, window.graph_rec, mandelbrot.mandelbrot_rec);
                RectangleD graph_rec_d = {window.graph_rec.x, window.graph_rec.y, window.graph_rec.width, window.graph_rec.height};
                to_graph(temp, graph_rec_d, mandelbrot.mandelbrot_rec_mpfr);

                window.copy_img = ImageCopy(window.graph_image);

                ImageDrawText(&window.copy_img, TextFormat("%f, %f", mpfr_get_d(temp.x, MPFR_RNDN), mpfr_get_d(temp.y, MPFR_RNDN)), 0, 0, text_size, WHITE);

            } else if (compute_mode == DOUBLE) {
                to_graph(mouse_pos, window.graph_rec, mandelbrot.mandelbrot_rec_d);
                window.copy_img = ImageCopy(window.graph_image);
                ImageDrawText(&window.copy_img, TextFormat("%f, %f", mouse_pos.x, mouse_pos.y), 0, 0, text_size, WHITE);
            }

            UpdateTexture(window.copy_texture, window.copy_img.data);
            DrawTexture(window.copy_texture, 0, 0, WHITE);
        }

    }

    void new_frame() {
        window.begin_frame();

        // render new view to graph_image
        if (new_input) {
            //render_to_img();
            if (compute_mode == MPFR) {
                window.render_to_img(mandelbrot.mandelbrot_rec_mpfr, num_threads, max_iter);

            } else if (compute_mode == DOUBLE) {
                window.render_to_img(mandelbrot.mandelbrot_rec_d, num_threads, max_iter);
            }
            new_input = false;
        }

        controls();
        // draw current view from graph_image on screen
        Color tint = WHITE;
        if (show_info) tint.a = 128;
        window.draw_frame(tint);

    }

};

Window init_window(int width, int height, const char* title) {
    Window window;
    window.screen_size = {(float)width, (float)height};
    window.graph_rec = {0, 0, (float)width, (float)height};
    window.bg_color = BLACK;
    window.fill_palette(max_iter_initial);

    InitWindow(window.screen_size.x, window.screen_size.y, title);
    SetTraceLogLevel(LOG_WARNING);
    //SetTargetFPS(120);

    window.graph_image = GenImageColor(window.graph_rec.width, window.graph_rec.height, window.bg_color);
    window.graph_texture = LoadTextureFromImage(window.graph_image);

    window.copy_img = ImageCopy(window.graph_image);
    window.copy_texture = LoadTextureFromImage(window.copy_img);

    return window;
}



App init_app(int width, int height, const char* title, uint64_t num_threads) {
    App app;

    app.num_threads = num_threads;
    app.window = init_window(width, height, title);
    app.mandelbrot.mandelbrot_rec_d = {-2.2f, 1.f, 3.2f, 2.f};

    app.mandelbrot.mandelbrot_rec_mpfr.init();
    mpfr_set_d(app.mandelbrot.mandelbrot_rec_mpfr.x,      -2.2f, MPFR_RNDN);
    mpfr_set_d(app.mandelbrot.mandelbrot_rec_mpfr.y,       1.f,  MPFR_RNDN);
    mpfr_set_d(app.mandelbrot.mandelbrot_rec_mpfr.width,   3.2f, MPFR_RNDN);
    mpfr_set_d(app.mandelbrot.mandelbrot_rec_mpfr.height,  2.f,  MPFR_RNDN);

    return app;
}

void init_mpfr_containers(uint64_t num_threads) {
    mpfr_init(temp.x);
    mpfr_init(temp.y);

    mpfr_init(dif_halved);
    mpfr_init(tmp);

    for(int i = 0; i < num_threads; ++i) {
        thread_mandelbrot_vectors[i].init();
        thread_draw_vectors[i].init();
    }
}

int main() {

    mpfr_set_default_prec(float_precision);

    uint64_t num_threads = std::thread::hardware_concurrency();
    if (num_threads > max_threads) num_threads = max_threads;

    init_mpfr_containers(num_threads);

    App app = init_app(window_width, window_height, "Mandelbrot", num_threads);

    while(!WindowShouldClose()) {
        app.new_frame();
    }

    CloseWindow();
    return 0;
}
