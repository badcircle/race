#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <sqlite3.h>

#define NUM_HORSES 6
#define HORSE_NAMES_COUNT 24
#define STARTING_MONEY 100.0
#define LOAN_INTEREST 0.199  // 19.9%
#define MIN_ODDS 1
#define MAX_ODDS 30
#define LEFT_PADDING 4
#define TRACK_WIDTH 80
#define MAX_BET 20000.0

typedef struct {
    float balance;
    float debt;
} PlayerState;

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
    "Thunder Roll", "Sir Trots-a-Lot", "Midnight Star", "Couch Potato", "Silver Wind", "Tax Deduction", "Noble Spirit", "Coffee Break", "Storm Chaser", "Naptime Champion", "Wild Wisdom", "Soup to Nuts", "Dark Victory", "Window Cleaner", "Desert Rose", "Certified Snack", "Morning Glory", "WiFi Password", "Royal Fortune", "Homework Eater", "Shadow Heart", "Fluffy Business", "River Dance", "Zaza Express"
};

typedef struct {
    char name[20];
    int odds;
    int position;
    const char* color;
} Horse;

void initDatabase(sqlite3 **db) {
    int rc = sqlite3_open("horse_racing.db", db);
    if (rc) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(*db));
        return;
    }
    
    // Betting history table
    const char *sql_history = "CREATE TABLE IF NOT EXISTS betting_history ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
                     "horse_name TEXT,"
                     "odds INTEGER,"
                     "bet_amount REAL,"
                     "win_amount REAL,"
                     "result TEXT,"
                     "balance REAL,"
                     "debt REAL)";
                     
    // Player state table
    const char *sql_player = "CREATE TABLE IF NOT EXISTS player_state ("
                     "id INTEGER PRIMARY KEY CHECK (id = 1),"  // Ensure only one row
                     "balance REAL,"
                     "debt REAL)";
    
    char *errMsg = 0;
    rc = sqlite3_exec(*db, sql_history, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
    
    rc = sqlite3_exec(*db, sql_player, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

void loadPlayerState(sqlite3 *db, PlayerState *player) {
    const char *sql = "SELECT balance, debt FROM player_state WHERE id = 1";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch player state: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        player->balance = sqlite3_column_double(stmt, 0);
        player->debt = sqlite3_column_double(stmt, 1);
    } else {
        // No existing state, initialize with default values and insert them
        player->balance = STARTING_MONEY;
        player->debt = 0.0;
        
        const char *insert_sql = "INSERT INTO player_state (id, balance, debt) VALUES (1, ?, ?)";
        sqlite3_stmt *insert_stmt;
        rc = sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, 0);
        
        if (rc == SQLITE_OK) {
            sqlite3_bind_double(insert_stmt, 1, player->balance);
            sqlite3_bind_double(insert_stmt, 2, player->debt);
            sqlite3_step(insert_stmt);
            sqlite3_finalize(insert_stmt);
        }
    }
    
    sqlite3_finalize(stmt);
}

void savePlayerState(sqlite3 *db, PlayerState *player) {
    const char *sql = "UPDATE player_state SET balance = ?, debt = ? WHERE id = 1";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, player->balance);
        sqlite3_bind_double(stmt, 2, player->debt);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void recordBet(sqlite3 *db, const char *horse_name, int odds, float bet_amount, 
               float win_amount, const char *result, float balance, float debt) {
    char *sql = sqlite3_mprintf(
        "INSERT INTO betting_history (horse_name, odds, bet_amount, win_amount, result, balance, debt) "
        "VALUES ('%q', %d, %f, %f, '%q', %f, %f)",
        horse_name, odds, bet_amount, win_amount, result, balance, debt);
    
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
    sqlite3_free(sql);
}

void displayHistory(sqlite3 *db) {
    const char *sql = "SELECT * FROM betting_history ORDER BY timestamp DESC LIMIT 5";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    printf("\nRecent Betting History:\n");
    printf("-------------------------------------------------------------------------------\n");
    printf("Horse            Odds         Bet  Result           Balance                Debt\n");
    printf("-------------------------------------------------------------------------------\n");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *horse = sqlite3_column_text(stmt, 2);
        int odds = sqlite3_column_int(stmt, 3);
        double bet = sqlite3_column_double(stmt, 4);
        const unsigned char *result = sqlite3_column_text(stmt, 6);
        double balance = sqlite3_column_double(stmt, 7);
        double debt = sqlite3_column_double(stmt, 8);
        
        printf("%-16s %2d:1  $%9.2f  %-8s   $%12.2f       $%12.2f\n", 
               horse, odds, bet, result, balance, debt);
    }
    
    printf("-------------------------------------------------------------------------------\n\n");
    sqlite3_finalize(stmt);
}

void clearScreen() {
    system("cls");
}

void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void updateHorsePosition(Horse horses[], int selectedHorse, int trackStartY) {
    for (int i = 0; i < NUM_HORSES; i++) {
        gotoxy(LEFT_PADDING, trackStartY + 1 + i);
        
        // Clear the line
        for (int j = 0; j < TRACK_WIDTH; j++) {
            printf(" ");
        }
        
        // Move cursor back to start of line
        gotoxy(LEFT_PADDING, trackStartY + 1 + i);
        
        // Draw the horse
        for (int j = 0; j < horses[i].position; j++) {
            printf(" ");
        }
        
        // Add player indicator if this is their horse
        if (i == selectedHorse - 1) {
            printf("%s>=>%s", horses[i].color, ANSI_RESET);
        } else {
            printf("%s>->%s", horses[i].color, ANSI_RESET);
        }
    }
    fflush(stdout);
}

void displayRace(Horse horses[], int selectedHorse, int firstDraw) {
    int trackStartY = 4;  // Adjust based on your header content
    
    if (firstDraw) {
        // Draw simple stadium seating above track
        gotoxy(LEFT_PADDING + 5, trackStartY);
        printf("---___---___---___---___---___---___---___---___---___---___---");
        
        // Draw track top border (moved to next line)
        gotoxy(LEFT_PADDING, trackStartY + 1);
        printf("========================================================================");
        printf("\n"); // Ensure we move to next line after border
    }
    
    updateHorsePosition(horses, selectedHorse, trackStartY + 2); // Adjust starting position
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

void resetColor() {
    printf(ANSI_RESET);
    fflush(stdout);
}

float getValidFloat(const char* prompt, float min, float max) {
    float value;
    char line[256];
    int valid = 0;
    
    do {
        printf("%s", prompt);
        if (fgets(line, sizeof(line), stdin)) {
            char *endptr;
            value = strtof(line, &endptr);
            
            // Check if conversion was successful and within range
            if (endptr != line && (*endptr == '\n' || *endptr == '\0')) {
                if (value >= min && value <= max) {
                    valid = 1;
                } else {
                    printf("Please enter a value between %.2f and %.2f\n", min, max);
                }
            } else {
                printf("Invalid input. Please enter a number\n");
                // Clear any remaining input
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
            }
        }
    } while (!valid);
    
    return value;
}

int getValidInt(const char* prompt, int min, int max) {
    float value = getValidFloat(prompt, min, max);
    return (int)value;
}

float getLoan(PlayerState *player) {
    char prompt[100];
    sprintf(prompt, "\nYou're broke! How much would you like to borrow? (19.9%% interest): $");
    float loan_amount = getValidFloat(prompt, 0.01, 1000.0);
    
    player->balance += loan_amount;
    player->debt += loan_amount * (1 + LOAN_INTEREST);
    
    printf("Loan granted! New balance: $%.2f (Debt: $%.2f)\n", 
           player->balance, player->debt);
    return loan_amount;
}

void initHorses(Horse horses[]) {
    int usedNames[HORSE_NAMES_COUNT] = {0};
    
    // Ensure a mix of odds ranges
    int odds_ranges[NUM_HORSES][2] = {
        {2, 3},    // Low odds (favorite)
        {4, 8},    // Low-mid odds
        {9, 15},   // Mid odds
        {16, 22},  // Mid-high odds
        {23, 27},  // High odds
        {28, 30}   // Very high odds (long shot)
    };
    
    for (int i = 0; i < NUM_HORSES; i++) {
        int nameIndex;
        do {
            nameIndex = rand() % HORSE_NAMES_COUNT;
        } while (usedNames[nameIndex]);
        usedNames[nameIndex] = 1;
        
        strcpy(horses[i].name, HORSE_NAMES[nameIndex]);
        horses[i].odds = (rand() % (odds_ranges[i][1] - odds_ranges[i][0] + 1)) + odds_ranges[i][0];
        horses[i].position = 0;
        horses[i].color = COLORS[i];
    }
    
    // Shuffle the horses to randomize the display order
    for (int i = 0; i < NUM_HORSES; i++) {
        int j = rand() % NUM_HORSES;
        Horse temp = horses[i];
        horses[i] = horses[j];
        horses[j] = temp;
    }
}

void displayHorseInfo(Horse horses[]) {
    printf("\n     Available Horses:\n\n");
    for (int i = 0; i < NUM_HORSES; i++) {
        printf("  %d. %s%s%s (Odds: %d:1)\n",
            i + 1, horses[i].color, horses[i].name, ANSI_RESET, horses[i].odds);
    }
    printf("\n");
    resetColor();
}

void displayPlayerState(PlayerState *player) {
    printf("\nBalance: $%.2f", player->balance);
    if (player->debt > 0) {
        printf("  |  Outstanding Debt: $%.2f", player->debt);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    // Process command line arguments
    for (int i = 1; i < argc; i++) {
        // Check for help flags
        if (strcmp(argv[i], "-h") == 0 || 
            strcmp(argv[i], "--h") == 0 || 
            strcmp(argv[i], "--help") == 0) {
            
            printf("\nUsage: %s [options]\n\n", argv[0]);
            printf("Options:\n");
            printf("  -h, --h, --help    Display this help message\n");
            printf("\nDescription:\n");
            printf("  To start a fresh book, simply move, delete or rename the horce_racing.db file in this directory.\n\n");
            
            return 0;  // Exit after displaying help
        }
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    
    sqlite3 *db;
    initDatabase(&db);
    
    srand(time(NULL));
    Horse horses[NUM_HORSES];
    PlayerState player = {STARTING_MONEY, 0};
    int choice, winner = -1;
    float winnings = 0;
    
    loadPlayerState(db, &player);

    while (1) {
        initHorses(horses);
        
        clearScreen();
        printf("\n");
        printf("------------------------------------------------------------------------------+\n");
        printf("                                                                    THE RACES |\n");
        printf("------------------------------------------------------------------------------+\n");
        
        displayHistory(db);
        displayPlayerState(&player);
        displayHorseInfo(horses);
        resetColor();
        
        int choice;
        choice = getValidInt("Choose your horse (1-6, 0 to quit): ", 0, NUM_HORSES);
        if (choice == 0) {
            resetColor();
            sqlite3_close(db);
            return 0;
        }

        float bet;
        char bet_prompt[100];
        do {
            if (player.balance <= 0) {
                getLoan(&player);
            }
            sprintf(bet_prompt, "Place your bet ($0.01-$%.2f): ", 
                   player.balance > MAX_BET ? MAX_BET : player.balance);
            bet = getValidFloat(bet_prompt, 0.01, 
                              player.balance > MAX_BET ? MAX_BET : player.balance);
        } while (bet <= 0 || bet > player.balance || bet > MAX_BET);
        
        player.balance -= bet;
        
        // After updating player state (win/loss/debt repayment), save it
        savePlayerState(db, &player);

        printf("\nPress Enter to start the race!");
        while (getchar() != '\n'); // Clear any remaining newline
        
        hideCursor();
        int racing = 1;
        int firstDraw = 1;  // Reset for each race
        
        while (racing) {
            if (firstDraw) {
                clearScreen();
                printf("\n    Race in progress! Your horse: %s%s%s\n\n", 
                       horses[choice-1].color, horses[choice-1].name, ANSI_RESET);
            }
            
            displayRace(horses, choice, firstDraw);
            firstDraw = 0;
            
            for (int i = 0; i < NUM_HORSES; i++) {
              // Find the lead horse's position
              int maxPosition = 0;
              for (int j = 0; j < NUM_HORSES; j++) {
                  if (horses[j].position > maxPosition) {
                      maxPosition = horses[j].position;
                  }
              }
              
              // Calculate how far behind this horse is
              int distanceBehind = maxPosition - horses[i].position;
              
              // Calculate move chance based on odds
              int moveChance = 45 - horses[i].odds; // Base chance
              
              // Add rally bonus for horses that are behind
              // More bonus for horses further behind, up to +15% for horses very far back
              int rallyBonus = 0;
              if (distanceBehind > 0) {  // Only trigger rally attempts when significantly behind
                  rallyBonus = (distanceBehind * 4); // 3% bonus per position behind
                  if (rallyBonus > 25) rallyBonus = 25; // Cap the bonus at 15%
                  
                  // Small random chance for a "super rally" - horse gets very motivated
                  if (rand() % 100 < 5) {  // 5% chance of super rally
                      rallyBonus *= 2;  // Double the rally bonus
                  }
              }
              
              moveChance += rallyBonus;
              
              // Ensure minimum and maximum chances
              if (moveChance < 20) moveChance = 20; // Even 30:1 horses get 20% chance
              if (moveChance > 60) moveChance = 60; // Increased max chance during rallies
              
              // Attempt movement
              if (rand() % 100 < moveChance) {
                  // When rallying, occasionally move 2 positions instead of 1
                  if (rallyBonus > 0 && rand() % 100 < 20) {  // 20% chance of double move during rally
                      horses[i].position += 3;
                  } else {
                      horses[i].position++;
                  }
                  
                  if (horses[i].position >= TRACK_WIDTH - 4) {
                      racing = 0;
                      winner = i;
                      break;
                  }
              }
          }
            
            Sleep(100);
        }

        
        clearScreen();
        printf("\n   Race finished!\n\n");
        displayRace(horses, choice, firstDraw);
        showCursor();
        
        printf("\n%sAND THE WINNER IS... %s%s%s!\n", 
               ANSI_RESET, horses[winner].color, horses[winner].name, ANSI_RESET);
        
        if (winner == choice - 1) {
            winnings = bet * horses[winner].odds;
            player.balance += winnings;
            printf("\nCONGRATULATIONS! You won $%.2f!\n", winnings);
            
            // Allow debt repayment if won
            if (player.debt > 0 && player.balance > 0) {
                float max_repayment = (player.balance < player.debt) ? player.balance : player.debt;
                char repay_prompt[100];
                sprintf(repay_prompt, "\nYou have $%.2f in debt. How much would you like to repay? (0-%.2f): $", 
                       player.debt, max_repayment);
                float repayment = getValidFloat(repay_prompt, 0, max_repayment);
                if (repayment > 0 && repayment <= max_repayment) {
                    player.balance -= repayment;
                    player.debt -= repayment;
                    printf("Debt payment successful! Remaining debt: $%.2f\n", player.debt);
                }
            }
            
            recordBet(db, horses[choice-1].name, horses[choice-1].odds, bet, 
                     winnings, "WON", player.balance, player.debt);
        } else {
            printf("\nSorry, you lost $%.2f. Better luck next time!\n", bet);
            recordBet(db, horses[choice-1].name, horses[choice-1].odds, bet, 
                     0, "LOST", player.balance, player.debt);
        }
        
        displayPlayerState(&player);
        resetColor();
        printf("\nPress Enter to continue");
        getchar();
    }
    
    resetColor();
    sqlite3_close(db);
    return 0;
}