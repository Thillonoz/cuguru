#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>     
#include <stdatomic.h>   
#include <math.h>        
#include "boards.h"      

#define CELL_SIZE 80
#define UI_WIDTH 220
#define SCREEN_WIDTH ((COLS * CELL_SIZE) + UI_WIDTH)
#define SCREEN_HEIGHT (ROWS * CELL_SIZE)
#define NUM_THREADS 4 

typedef struct {
    int value;      
    int region_id;  
    bool is_given;  
} Cell;

Color regionColors[] = {
    LIGHTGRAY, SKYBLUE, LIME, GOLD, ORANGE, VIOLET, PINK, BEIGE
};

atomic_bool puzzle_found = false;
pthread_mutex_t grid_mutex = PTHREAD_MUTEX_INITIALIZER;

Cell shared_winning_grid[COLS][ROWS]; 
Cell shared_solved_grid[COLS][ROWS]; 
atomic_int total_attempts = 0;        

bool IsMoveValid(Cell grid[COLS][ROWS], int cx, int cy, int val) {
    if (val == 0) return true; 
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            int nx = cx + dx; int ny = cy + dy;
            if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS) {
                if (grid[nx][ny].value == val) return false;
            }
        }
    }
    int target_region = grid[cx][cy].region_id;
    for (int x = 0; x < COLS; x++) {
        for (int y = 0; y < ROWS; y++) {
            if (x == cx && y == cy) continue; 
            if (grid[x][y].region_id == target_region && grid[x][y].value == val) return false;
        }
    }
    return true;
}

int GetRegionSize(Cell grid[COLS][ROWS], int r_id) {
    int size = 0;
    for (int x = 0; x < COLS; x++) {
        for (int y = 0; y < ROWS; y++) {
            if (grid[x][y].region_id == r_id) size++;
        }
    }
    return size;
}

bool SolveGrid(Cell grid[COLS][ROWS], int cx, int cy, int *guess_count) {
    if (*guess_count > 2000) return false; 
    
    if (cy >= ROWS) return true; 

    int nx = cx + 1; int ny = cy;
    if (nx >= COLS) { nx = 0; ny++; }

    if (grid[cx][cy].value != 0) return SolveGrid(grid, nx, ny, guess_count);

    int r_id = grid[cx][cy].region_id;
    int r_size = GetRegionSize(grid, r_id);
    
    int start_val = GetRandomValue(1, r_size);
    for (int i = 0; i < r_size; i++) {
        int v = ((start_val + i - 1) % r_size) + 1;
        if (IsMoveValid(grid, cx, cy, v)) {
            grid[cx][cy].value = v; 
            (*guess_count)++; 
            if (SolveGrid(grid, nx, ny, guess_count)) return true; 
            grid[cx][cy].value = 0; 
        }
    }
    return false; 
}

void CountSolutions(Cell grid[COLS][ROWS], int cx, int cy, int *solution_count, int *guess_count) {
    if (*solution_count > 1 || *guess_count > 2000) return; 

    if (cy >= ROWS) {
        (*solution_count)++;
        return; 
    }

    int nx = cx + 1; int ny = cy;
    if (nx >= COLS) { nx = 0; ny++; }

    if (grid[cx][cy].value != 0) {
        CountSolutions(grid, nx, ny, solution_count, guess_count);
        return;
    }

    int r_id = grid[cx][cy].region_id;
    int r_size = GetRegionSize(grid, r_id);
    
    for (int v = 1; v <= r_size; v++) {
        if (IsMoveValid(grid, cx, cy, v)) {
            grid[cx][cy].value = v; 
            (*guess_count)++;
            CountSolutions(grid, nx, ny, solution_count, guess_count);
            grid[cx][cy].value = 0; 
        }
    }
}

void* GeneratorWorker(void* arg) {
    int difficulty = *(int*)arg;
    Cell local_grid[COLS][ROWS];
    Cell local_solved_grid[COLS][ROWS]; 

    while (!atomic_load(&puzzle_found)) {
        atomic_fetch_add(&total_attempts, 1);

        int chosen_idx = GetRandomValue(0, NUM_LAYOUTS - 1);
        for (int x = 0; x < COLS; x++) {
            for (int y = 0; y < ROWS; y++) {
                local_grid[x][y].region_id = layouts[chosen_idx][y][x];
                local_grid[x][y].value = 0;
                local_grid[x][y].is_given = true;
            }
        }

        int solve_guesses = 0;
        if (!SolveGrid(local_grid, 0, 0, &solve_guesses)) continue;

        memcpy(local_solved_grid, local_grid, sizeof(local_solved_grid));

        int coords[COLS * ROWS][2];
        int count = 0;
        for (int x = 0; x < COLS; x++) {
            for (int y = 0; y < ROWS; y++) {
                coords[count][0] = x; coords[count][1] = y;
                count++;
            }
        }
        for (int i = 0; i < count; i++) {
            int swap_idx = GetRandomValue(0, count - 1);
            int temp_x = coords[i][0]; int temp_y = coords[i][1];
            coords[i][0] = coords[swap_idx][0]; coords[i][1] = coords[swap_idx][1];
            coords[swap_idx][0] = temp_x; coords[swap_idx][1] = temp_y;
        }

        int target_erasures = 63; 
        if (difficulty == 0) target_erasures = 30; 
        if (difficulty == 1) target_erasures = 45; 

        int erased_count = 0;
        bool thread_stuck = false;
        int cumulative_guesses = 0; 

        for (int i = 0; i < count; i++) {
            if (erased_count >= target_erasures) break;
            if (atomic_load(&puzzle_found)) break; 

            int cx = coords[i][0]; int cy = coords[i][1];

            int temp_val = local_grid[cx][cy].value;
            local_grid[cx][cy].value = 0;
            local_grid[cx][cy].is_given = false;

            int solution_count = 0;
            int guess_count = 0;
            
            CountSolutions(local_grid, 0, 0, &solution_count, &guess_count);
            cumulative_guesses += guess_count; 

            if (solution_count != 1 || guess_count > 2000) {
                local_grid[cx][cy].value = temp_val;
                local_grid[cx][cy].is_given = true;
            } else {
                erased_count++;
            }

            if (cumulative_guesses > 15000) {
                thread_stuck = true;
                break;
            }
        }

        if (thread_stuck || atomic_load(&puzzle_found)) continue;

        pthread_mutex_lock(&grid_mutex);
        if (!atomic_load(&puzzle_found)) { 
            memcpy(shared_winning_grid, local_grid, sizeof(shared_winning_grid));
            memcpy(shared_solved_grid, local_solved_grid, sizeof(shared_solved_grid)); 
            atomic_store(&puzzle_found, true);
        }
        pthread_mutex_unlock(&grid_mutex);
    }
    return NULL;
}

bool CheckWinCondition(Cell grid[COLS][ROWS]) {
    for (int x = 0; x < COLS; x++) {
        for (int y = 0; y < ROWS; y++) {
            if (grid[x][y].value == 0) return false;
            
            int temp_val = grid[x][y].value;
            grid[x][y].value = 0;
            bool valid = IsMoveValid(grid, x, y, temp_val);
            grid[x][y].value = temp_val; 
            
            if (!valid) return false;
        }
    }
    return true;
}

bool DrawButton(Rectangle rect, const char* text, Color baseColor, bool isSelected) {
    bool clicked = false;
    Vector2 mouse = GetMousePosition();
    Color drawColor = baseColor;
    
    if (isSelected) {
        drawColor = ColorBrightness(baseColor, -0.3f); 
    } else if (CheckCollisionPointRec(mouse, rect)) {
        drawColor = ColorBrightness(baseColor, 0.2f); 
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = true; 
    }

    DrawRectangleRec(rect, drawColor);
    DrawRectangleLinesEx(rect, isSelected ? 4.0f : 2.0f, BLACK);
    
    int fontSize = 20;
    int textWidth = MeasureText(text, fontSize);
    Color textColor = isSelected ? WHITE : BLACK;
    DrawText(text, rect.x + (rect.width / 2) - (textWidth / 2), rect.y + (rect.height / 2) - (fontSize / 2), fontSize, textColor);
    
    return clicked;
}

int main(void) {
    InitializeLayouts();
    
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Cuguru");
    SetTargetFPS(60);

    Cell grid[COLS][ROWS] = {0};
    Cell initial_grid[COLS][ROWS] = {0};
    Cell solution_grid[COLS][ROWS] = {0}; 

    int ui_start_x = COLS * CELL_SIZE;
    Rectangle btnNew = { ui_start_x + 20, 20, UI_WIDTH - 40, 45 };
    Rectangle btnReset = { ui_start_x + 20, 75, UI_WIDTH - 40, 45 };
    Rectangle btnHint = { ui_start_x + 20, 130, UI_WIDTH - 40, 45 }; 
    
    Rectangle btnEasy = { ui_start_x + 20, 220, UI_WIDTH - 40, 35 };
    Rectangle btnMed = { ui_start_x + 20, 265, UI_WIDTH - 40, 35 };
    Rectangle btnHard = { ui_start_x + 20, 310, UI_WIDTH - 40, 35 };

    int selected_x = -1;
    int selected_y = -1;
    int current_difficulty = 1; 
    
    int hint_blink_x = -1;
    int hint_blink_y = -1;
    float hint_timer = 0.0f;
    
    pthread_t threads[NUM_THREADS];
    int thread_args[NUM_THREADS];
    bool is_generating = true;

    atomic_store(&puzzle_found, false);
    atomic_store(&total_attempts, 0);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = current_difficulty;
        pthread_create(&threads[i], NULL, GeneratorWorker, &thread_args[i]);
    }

    while (!WindowShouldClose()) {
        bool is_game_won = CheckWinCondition(grid);

        if (hint_timer > 0.0f) {
            hint_timer -= GetFrameTime(); 
            if (hint_timer <= 0.0f) {
                hint_blink_x = -1;
                hint_blink_y = -1;
            }
        }

        if (is_generating) {
            if (atomic_load(&puzzle_found)) {
                for (int i = 0; i < NUM_THREADS; i++) {
                    pthread_join(threads[i], NULL);
                }
                
                pthread_mutex_lock(&grid_mutex);
                memcpy(grid, shared_winning_grid, sizeof(grid));
                memcpy(solution_grid, shared_solved_grid, sizeof(solution_grid)); 
                pthread_mutex_unlock(&grid_mutex);

                memcpy(initial_grid, grid, sizeof(initial_grid)); 
                is_generating = false; 
                selected_x = -1; selected_y = -1;
            }
        } 
        else if (!is_game_won) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mousePos = GetMousePosition();
                if (mousePos.x < COLS * CELL_SIZE) { 
                    selected_x = mousePos.x / CELL_SIZE;
                    selected_y = mousePos.y / CELL_SIZE;
                } else {
                    selected_x = -1; selected_y = -1;
                }
            }

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                if (selected_x == -1) { selected_x = 0; selected_y = ROWS - 1; }
                else if (selected_y > 0) selected_y--;
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                if (selected_x == -1) { selected_x = 0; selected_y = 0; }
                else if (selected_y < ROWS - 1) selected_y++;
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                if (selected_x == -1) { selected_x = COLS - 1; selected_y = 0; }
                else if (selected_x > 0) selected_x--;
            }
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                if (selected_x == -1) { selected_x = 0; selected_y = 0; }
                else if (selected_x < COLS - 1) selected_x++;
            }

            if (selected_x != -1 && selected_y != -1 && !grid[selected_x][selected_y].is_given) {
                int key = GetKeyPressed();
                if (key >= KEY_ONE && key <= KEY_FIVE) {
                    grid[selected_x][selected_y].value = key - KEY_ZERO; 
                } else if (key == KEY_BACKSPACE || key == KEY_ZERO) {
                    grid[selected_x][selected_y].value = 0;
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int x = 0; x < COLS; x++) {
            for (int y = 0; y < ROWS; y++) {
                Rectangle cellRect = { x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE };
                Color bgColor = regionColors[grid[x][y].region_id % 8];
                
                if (x == selected_x && y == selected_y && !is_game_won && !is_generating) {
                    bgColor = ColorBrightness(bgColor, 0.4f);
                }
                
                bool is_blinking = false;
                if (x == hint_blink_x && y == hint_blink_y && hint_timer > 0.0f) {
                    if (fmodf(hint_timer, 1.0f) > 0.5f) {
                        bgColor = WHITE;
                        is_blinking = true;
                    }
                }
                
                DrawRectangleRec(cellRect, bgColor);

                if (grid[x][y].value > 0 && !is_generating) {
                    char numStr[16];
                    sprintf(numStr, "%d", grid[x][y].value);
                    
                    bool isValid = IsMoveValid(grid, x, y, grid[x][y].value);
                    Color textColor = BLACK;
                    if (!isValid) textColor = RED;         
                    if (grid[x][y].is_given) textColor = ColorAlpha(BLACK, 0.6f); 
                    
                    if (is_blinking) textColor = BLACK; 
                    
                    int fontSize = 40;
                    int textWidth = MeasureText(numStr, fontSize);
                    DrawText(numStr, x * CELL_SIZE + (CELL_SIZE / 2) - (textWidth / 2), 
                                     y * CELL_SIZE + (CELL_SIZE / 2) - (fontSize / 2), fontSize, textColor);
                }
            }
        }

        for (int x = 0; x < COLS; x++) {
            for (int y = 0; y < ROWS; y++) {
                int current_region = grid[x][y].region_id;
                DrawRectangleLines(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, ColorAlpha(BLACK, 0.2f));

                if (x == COLS - 1 || grid[x+1][y].region_id != current_region) {
                    DrawLineEx((Vector2){(x+1)*CELL_SIZE, y*CELL_SIZE}, (Vector2){(x+1)*CELL_SIZE, (y+1)*CELL_SIZE}, 4.0f, BLACK);
                }
                if (y == ROWS - 1 || grid[x][y+1].region_id != current_region) {
                    DrawLineEx((Vector2){x*CELL_SIZE, (y+1)*CELL_SIZE}, (Vector2){(x+1)*CELL_SIZE, (y+1)*CELL_SIZE}, 4.0f, BLACK);
                }
                if (x == 0) DrawLineEx((Vector2){0, y*CELL_SIZE}, (Vector2){0, (y+1)*CELL_SIZE}, 4.0f, BLACK);
                if (y == 0) DrawLineEx((Vector2){x*CELL_SIZE, 0}, (Vector2){(x+1)*CELL_SIZE, 0}, 4.0f, BLACK);
            }
        }

        DrawRectangle(ui_start_x, 0, UI_WIDTH, SCREEN_HEIGHT, DARKGRAY);
        DrawLine(ui_start_x, 0, ui_start_x, SCREEN_HEIGHT, BLACK);

        if (!is_generating) {
            if (DrawButton(btnNew, "New Puzzle", LIGHTGRAY, false)) {
                is_generating = true;
                atomic_store(&puzzle_found, false);
                atomic_store(&total_attempts, 0);
                for (int i = 0; i < NUM_THREADS; i++) {
                    thread_args[i] = current_difficulty;
                    pthread_create(&threads[i], NULL, GeneratorWorker, &thread_args[i]);
                }
            }
            
            if (DrawButton(btnReset, "Reset Grid", LIGHTGRAY, false)) {
                memcpy(grid, initial_grid, sizeof(grid)); 
                selected_x = -1; selected_y = -1;
                hint_timer = 0.0f; 
            }

            if (!is_game_won) {
                if (DrawButton(btnHint, "Get a Hint", SKYBLUE, false)) {
                    int empty_coords[COLS * ROWS][2];
                    int empty_count = 0;
                    for(int x = 0; x < COLS; x++) {
                        for(int y = 0; y < ROWS; y++) {
                            if (grid[x][y].value == 0) {
                                empty_coords[empty_count][0] = x;
                                empty_coords[empty_count][1] = y;
                                empty_count++;
                            }
                        }
                    }
                    
                    if (empty_count > 0) {
                        int pick = GetRandomValue(0, empty_count - 1);
                        int hx = empty_coords[pick][0];
                        int hy = empty_coords[pick][1];
                        
                        grid[hx][hy].value = solution_grid[hx][hy].value;
                        grid[hx][hy].is_given = true; 
                        
                        hint_blink_x = hx;
                        hint_blink_y = hy;
                        hint_timer = 3.0f; 
                    }
                }
            }

            DrawText("DIFFICULTY:", ui_start_x + 20, 195, 18, RAYWHITE);
            
            for (int diff = 0; diff <= 2; diff++) {
                Rectangle btn = (diff == 0) ? btnEasy : (diff == 1) ? btnMed : btnHard;
                const char* txt = (diff == 0) ? "Easy" : (diff == 1) ? "Medium" : "Hard";
                Color col = (diff == 0) ? LIME : (diff == 1) ? ORANGE : RED;

                if (DrawButton(btn, txt, col, current_difficulty == diff) && current_difficulty != diff) {
                    current_difficulty = diff;
                    is_generating = true;
                    atomic_store(&puzzle_found, false);
                    atomic_store(&total_attempts, 0);
                    for (int i = 0; i < NUM_THREADS; i++) {
                        thread_args[i] = current_difficulty;
                        pthread_create(&threads[i], NULL, GeneratorWorker, &thread_args[i]);
                    }
                }
            }
        }

        if (is_generating) {
            DrawRectangle(0, 0, COLS * CELL_SIZE, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            DrawText("GENERATING...", 100, SCREEN_HEIGHT / 2 - 20, 40, RAYWHITE);
            
            char attemptStr[64];
            sprintf(attemptStr, "Attempts: %d", atomic_load(&total_attempts));
            DrawText(attemptStr, 100, SCREEN_HEIGHT / 2 + 30, 20, LIGHTGRAY);
        }
        else if (is_game_won) {
            DrawRectangle(0, 0, COLS * CELL_SIZE, SCREEN_HEIGHT, ColorAlpha(LIME, 0.5f));
            
            const char* winText = "PUZZLE SOLVED!";
            int wSize = 50;
            int textW = MeasureText(winText, wSize);
            
            DrawText(winText, ((COLS * CELL_SIZE) / 2) - (textW / 2) + 4, (SCREEN_HEIGHT / 2) - (wSize / 2) + 4, wSize, ColorAlpha(BLACK, 0.5f));
            DrawText(winText, ((COLS * CELL_SIZE) / 2) - (textW / 2), (SCREEN_HEIGHT / 2) - (wSize / 2), wSize, WHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}