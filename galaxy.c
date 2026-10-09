#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

#define WIDTH 80
#define HEIGHT 24

// Galaxy pattern: spiral with stars
char buffer[HEIGHT][WIDTH];
int arm_map[HEIGHT][WIDTH];

// Galaxy parameters
float galaxy_radius = 10.0;
float twist_factor = 1.0;
float twinkle_phase = 0.0;
int num_arms = 4;

static struct termios orig_termios;

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?1049l"); // Exit alternate screen buffer
    printf("\033[?25h");   // Show cursor
}

void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void clear_buffer() {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            buffer[i][j] = ' ';
            arm_map[i][j] = 0;
        }
    }
}

int kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    return select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) == 1;
}

void draw_galaxy(float rotation) {
    clear_buffer();
    
    int center_x = WIDTH / 2;
    int center_y = HEIGHT / 2;
    
    // Draw spiral galaxy with variable twist
    float spiral_growth = galaxy_radius / (4.0f * M_PI);
    float arm_spacing = 2.0f * M_PI / num_arms;
    for (float angle = 0; angle < 4 * M_PI; angle += 0.05f) {
        float r = angle * spiral_growth;
        float base_angle = angle * twist_factor + rotation;
        float twinkle = sin(twinkle_phase + angle * 0.6f);
        
        // Draw multiple arms with adjustable count
        for (int arm = 0; arm < num_arms; arm++) {
            float arm_angle = base_angle + arm * arm_spacing;
            int ax = (int)(center_x + r * cos(arm_angle));
            int ay = (int)(center_y + r * sin(arm_angle) * 0.5f);
            
            if (ax >= 0 && ax < WIDTH && ay >= 0 && ay < HEIGHT) {
                char star_char = (twinkle > 0.25f) ? '*' : '+';
                buffer[ay][ax] = star_char;
                arm_map[ay][ax] = arm + 1; // mark which arm this star belongs to
            }
        }
    }
    
    // Add many stars distributed throughout the galaxy
    int num_stars = (int)(20 * galaxy_radius / 10.0);  // Scale with radius
    for (int s = 0; s < num_stars; s++) {
        float angle = (float)s / num_stars * 2 * M_PI;
        float r = (float)(s % 5 + 1) / 5.0 * galaxy_radius;
        
        float rotated_angle = angle + rotation + (angle * twist_factor);
        int x = (int)(center_x + r * cos(rotated_angle));
        int y = (int)(center_y + r * sin(rotated_angle) * 0.5);
        
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            // Individual star twinkling
            float twinkle = sin(twinkle_phase + s * 0.5);
            if (twinkle > 0.0) {
                char star_char = (twinkle > 0.7) ? '*' : '+';
                if (buffer[y][x] == ' ') {
                    buffer[y][x] = star_char;
                    arm_map[y][x] = 0; // not part of a main arm
                }
            }
        }
    }
    
    // Add bright core
    for (int i = center_y - 1; i <= center_y + 1; i++) {
        for (int j = center_x - 1; j <= center_x + 1; j++) {
            if (i >= 0 && i < HEIGHT && j >= 0 && j < WIDTH) {
                buffer[i][j] = '#';
            }
        }
    }
    buffer[center_y][center_x] = '@';  // Central black hole
    arm_map[center_y][center_x] = 0;
}

void display_buffer(float speed, float radius, float twist, int arms) {
    // Clear screen and move cursor to home
    printf("\033[H\033[J");

    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            char ch = buffer[i][j];
            const char *start = "";
            const char *end = "";
            int arm = arm_map[i][j];
            if (arm > 0) {
                const char *acolor = "";
                switch (arm) {
                    case 1: acolor = "\033[1;36m"; break; // cyan
                    case 2: acolor = "\033[1;32m"; break; // green
                    case 3: acolor = "\033[1;31m"; break; // red
                    case 4: acolor = "\033[1;34m"; break; // blue
                    default: acolor = "\033[1;37m"; break;
                }
                printf("%s%c\033[0m", acolor, ch);
            } else {
                switch (ch) {
                    case '@': start = "\033[1;35m"; end = "\033[0m"; break; // magenta
                    case '#': start = "\033[1;33m"; end = "\033[0m"; break; // yellow
                    case '*': start = "\033[1;37m"; end = "\033[0m"; break; // bright white
                    case '+': start = "\033[0;37m"; end = "\033[0m"; break; // normal white
                    default: start = ""; end = ""; break;
                }
                if (start[0]) printf("%s%c%s", start, ch, end); else putchar(ch);
            }
        }
        putchar('\n');
    }

    // Live parameter line (colored labels)
    printf("\033[1;36mSpeed:\033[0m %.3f   \033[1;36mRadius:\033[0m %.1f   \033[1;36mTwist:\033[0m %.2f   \033[1;36mArms:\033[0m %d\n",
           speed, radius, twist, arms);
    printf("Controls: s/S speed  r/R radius  t/T twist  a/A arms  q quit\n");
}

void print_instructions(float speed, float radius, float twist, int arms) {
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║          ASCII Galaxy Rotation Animation              ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║ Current Settings:                                      ║\n");
    printf("║   Speed:  %.3f rad/frame                               ║\n", speed);
    printf("║   Radius: %.2f                                         ║\n", radius);
    printf("║   Twist:  %.2f                                         ║\n", twist);
    printf("║   Arms:   %d                                           ║\n", arms);
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║ Controls:                                              ║\n");
    printf("║   s/S : Decrease/Increase speed                        ║\n");
    printf("║   r/R : Decrease/Increase radius                       ║\n");
    printf("║   t/T : Decrease/Increase twist                        ║\n");
    printf("║   a/A : Decrease/Increase arms (2-4)                   ║\n");
    printf("║   q   : Quit                                           ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
}

int main(int argc, char *argv[]) {
    float speed = 0.010;     // Default rotation speed
    float radius = 10.0;     // Default galaxy radius
    float twist = 0.10;      // Default twist factor
    int arms = 4;            // Default number of arms
    
    // Parse command-line arguments
    if (argc > 1) {
        speed = atof(argv[1]);
        if (speed < 0.002f) speed = 0.002f;
        if (speed > 0.030f) speed = 0.030f;
        speed = roundf(speed / 0.002f) * 0.002f;
    }
    if (argc > 2) {
        radius = atof(argv[2]);
        if (radius < 8.0f) radius = 8.0f;
        if (radius > 25.0f) radius = 25.0f;
    }
    if (argc > 3) {
        twist = atof(argv[3]);
        if (twist < 0.00f) twist = 0.00f;
        if (twist > 0.50f) twist = 0.50f;
        twist = roundf(twist / 0.02f) * 0.02f;
    }
    if (argc > 4) {
        arms = atoi(argv[4]);
        if (arms < 2) arms = 2;
        if (arms > 4) arms = 4;
    }
    
    galaxy_radius = radius;
    twist_factor = twist;
    num_arms = arms;
    
    float rotation = 0.0;
    
    enable_raw_mode();
    printf("\033[?25l");  // Hide cursor
    printf("\033[?1049h"); // Use alternate screen buffer

    print_instructions(speed, radius, twist, arms);
    usleep(3000000);  // Show instructions for 3 seconds

    // Main animation loop
    while (1) {
        twinkle_phase += 0.1;
        if (twinkle_phase > 2 * M_PI) {
            twinkle_phase -= 2 * M_PI;
        }
        
        draw_galaxy(rotation);
        display_buffer(speed, galaxy_radius, twist_factor, num_arms);
        
        // Handle keyboard input (non-blocking)
        if (kbhit()) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) == 1) {
                switch (ch) {
                    case 'q':
                    case 'Q':
                        printf("\nGalaxy stopped. Goodbye!\n");
                        return 0;
                    case 's':
                        speed -= 0.002f;
                        if (speed < 0.002f) speed = 0.002f;
                        break;
                    case 'S':
                        speed += 0.002f;
                        if (speed > 0.030f) speed = 0.030f;
                        break;
                    case 'r':
                        galaxy_radius -= 0.5f;
                        if (galaxy_radius < 8.0f) galaxy_radius = 8.0f;
                        break;
                    case 'R':
                        galaxy_radius += 0.5f;
                        if (galaxy_radius > 25.0f) galaxy_radius = 25.0f;
                        break;
                    case 't':
                        twist_factor -= 0.02f;
                        if (twist_factor < 0.00f) twist_factor = 0.00f;
                        break;
                    case 'T':
                        twist_factor += 0.02f;
                        if (twist_factor > 0.50f) twist_factor = 0.50f;
                        break;
                    case 'a':
                        num_arms--;
                        if (num_arms < 2) num_arms = 2;
                        break;
                    case 'A':
                        num_arms++;
                        if (num_arms > 4) num_arms = 4;
                        break;
                }
            }
        }
        
        rotation += speed;
        if (rotation > 2 * M_PI) {
            rotation -= 2 * M_PI;
        }
        
        usleep(50000);  // 50ms per frame (~20 FPS)
    }
    
    // Cleanup is handled by atexit()
    return 0;
}
