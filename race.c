#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define NUM_HORSES 6
#define TRACK_LENGTH 40
#define HORSE_NAMES_COUNT 12

// ANSI color codes
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"

const char* COLORS[] = {
    ANSI_RED, ANSI_GREEN, ANSI_YELLOW,
    ANSI_BLUE, ANSI_MAGENTA, ANSI_CYAN
};

const char* HORSE_NAMES[] = {
    "Thunder", "Lightning", "Shadow", "Spirit",
    "Storm", "Flash", "Blitz", "Dash",
    "Arrow", "Comet", "Rocket", "Bolt"
};

typedef struct {
    char name[20];
    float odds;
    int position;
    const char* color;
} Horse;

void clearScreen() {
    system("cls");
}

void hideCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void showCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = TRUE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void initHorses(Horse horses[]) {
    int usedNames[HORSE_NAMES_COUNT] = {0};
    
    for (int i = 0; i < NUM_HORSES; i++) {
        // Select unique random name
        int nameIndex;
        do {
            nameIndex = rand() % HORSE_NAMES_COUNT;
        } while (usedNames[nameIndex]);
        usedNames[nameIndex] = 1;
        
        strcpy(horses[i].name, HORSE_NAMES[nameIndex]);
        horses[i].odds = (float)(rand() % 400 + 100) / 100.0f; // Odds between 1.0 and 5.0
        horses[i].position = 0;
        horses[i].color = COLORS[i];
    }
}

void displayTrack(Horse horses[]) {
    printf("\n\n");
    printf("╔════════════════════════════════════════════════╗\n");
    
    for (int i = 0; i < NUM_HORSES; i++) {
        printf("║");
        int pos = horses[i].position;
        
        for (int j = 0; j < TRACK_LENGTH; j++) {
            if (j == pos) {
                printf("%s>=>%s", horses[i].color, ANSI_RESET);
            } else {
                printf(" ");
            }
        }
        printf("║\n");
    }
    
    printf("╚════════════════════════════════════════════════╝\n");
}

void displayHorseInfo(Horse horses[]) {
    printf("\n     Available Horses:\n\n");
    for (int i = 0; i < NUM_HORSES; i++) {
        printf("  %d. %s%s%s (Odds: %.2f:1)\n",
            i + 1, horses[i].color, horses[i].name, ANSI_RESET, horses[i].odds);
    }
    printf("\n");
}

int main() {
    // Enable ANSI escape codes in Windows console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    
    srand(time(NULL));
    Horse horses[NUM_HORSES];
    int choice, winner = -1;
    float winnings = 0;
    
    // Initialize horses
    initHorses(horses);
    
    // Display welcome screen
    clearScreen();
    printf("\n");
    printf("    ╔══════════════════════════════════════╗\n");
    printf("    ║         TERMINAL HORSE RACE          ║\n");
    printf("    ╚══════════════════════════════════════╝\n");
    
    displayHorseInfo(horses);
    
    // Get player's bet
    do {
        printf("Choose your horse (1-%d): ", NUM_HORSES);
        scanf("%d", &choice);
    } while (choice < 1 || choice > NUM_HORSES);
    
    printf("Place your bet ($): ");
    float bet;
    scanf("%f", &bet);
    
    printf("\nPress Enter to start the race!");
    getchar(); // Clear buffer
    getchar(); // Wait for Enter
    
    // Race loop
    hideCursor();
    int racing = 1;
    while (racing) {
        clearScreen();
        displayTrack(horses);
        
        // Move horses
        for (int i = 0; i < NUM_HORSES; i++) {
            if (rand() % 100 < 30) { // 30% chance to move each frame
                horses[i].position++;
                if (horses[i].position >= TRACK_LENGTH - 3) {
                    racing = 0;
                    winner = i;
                    break;
                }
            }
        }
        
        Sleep(100); // Delay between frames
    }
    
    // Display final result
    clearScreen();
    displayTrack(horses);
    showCursor();
    
    printf("\n%sAND THE WINNER IS... %s%s!\n", ANSI_RESET, horses[winner].color, horses[winner].name);
    
    if (winner == choice - 1) {
        winnings = bet * horses[winner].odds;
        printf("\nCONGRATULATIONS! You won $%.2f!\n", winnings);
    } else {
        printf("\nSorry, you lost $%.2f. Better luck next time!\n", bet);
    }
    
    return 0;
}