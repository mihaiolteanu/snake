#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define SNAKE_MAX_SIZE 25
#define SNAKE_INITIAL_SIZE 5
#define SNAKE_LOOKS ACS_DIAMOND

typedef enum {
    SNAKE_LEFT,
    SNAKE_RIGHT,
    SNAKE_UP,
    SNAKE_DOWN,
    SNAKE_UNKNOWN
} snake_direction_enum;

typedef struct {
    int x, y;
    snake_direction_enum neighbour;
} snake_block_st;

typedef struct {
    int size;
    snake_block_st block[SNAKE_MAX_SIZE];
} snake_st;

static snake_direction_enum snake_direction;
static int should_exit;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static void *thread_func(void *arg) {
    int ch;
    int s;

    while(should_exit != 1) {
	ch = getch();
	if ((s = pthread_mutex_lock(&mtx)) != 0)
	    exit(1);

	switch (ch) {
	case KEY_LEFT:
	    if (snake_direction != SNAKE_RIGHT) /* Don't allow going backwards. */
		snake_direction = SNAKE_LEFT;
	    break;
	case KEY_RIGHT:
	    if (snake_direction != SNAKE_LEFT)
	    snake_direction = SNAKE_RIGHT;
	    break;
	case KEY_UP:
	    if (snake_direction != SNAKE_DOWN)
	    snake_direction = SNAKE_UP;
	    break;
	case KEY_DOWN:
	    if (snake_direction != SNAKE_UP)
		snake_direction = SNAKE_DOWN;
	    break;
	case 'q':
	    should_exit = 1;
	    break;
	default:
	    /* Keep going in the same direction as before. */
	    break;
	}
        
	if ((s = pthread_mutex_unlock(&mtx)) != 0)
	    exit(1);
    }
    
    return (void *)0;
}

static void snake_paint(snake_st *snake) {
    for (int i = 0; i < snake->size; i++)
    	mvaddch(snake->block[i].x, snake->block[i].y, SNAKE_LOOKS);
    refresh();
}

int main() {
    pthread_t t1;
    void *res;
    int s, ch, will_exit;
    int row, col;

    snake_st snake;
    snake_direction_enum direction;

    /* General initializations */
    will_exit = 0;
    should_exit = 0;
    snake_direction = SNAKE_LEFT; /* Threads not started yet, so this is safe. */

    /* Initialize curses */
    initscr();
    keypad(stdscr, TRUE);
    curs_set(0);
    noecho();
    cbreak();
    getmaxyx(stdscr, row, col);	/* Get window size */

    /* Initialize the snake */
    snake.size = 0;
    for (int i = 0; i < SNAKE_INITIAL_SIZE; i++) {
	snake.block[i].x = row/2;
	snake.block[i].y = col/2 - i;
	snake.block[i].neighbour = SNAKE_LEFT;
	snake.size++;
    }
    
    snake_paint(&snake);	/* Initial snake. */

    /* Watch for keyboard input. */
    if ((s = pthread_create(&t1, NULL, thread_func, "This is on thread")) != 0)
    	exit(1);

    /* Main loop. */
    while (will_exit != 1) {
	/* Maybe the user has changes the snake direction or wants to go home */
	if ((s = pthread_mutex_lock(&mtx)) != 0)
	    exit(1);
        direction = snake_direction;
	will_exit = should_exit;
	if ((s = pthread_mutex_unlock(&mtx)) != 0)
	    exit(1);

	switch(direction) {
	case SNAKE_LEFT:
	    for (int i = 0; i < snake.size; i++) {
		mvaddch(snake.block[i].x, snake.block[i].y, ' ');
		snake.block[i].y--;              
	    }
	    break;
	case SNAKE_RIGHT:
	    for (int i = 0; i < snake.size; i++) {
		mvaddch(snake.block[i].x, snake.block[i].y, ' ');
		snake.block[i].y++;
	    }
	    break;
	case SNAKE_UP:
	    for (int i = 0; i < snake.size; i++) {
		mvaddch(snake.block[i].x, snake.block[i].y, ' ');
		snake.block[i].x--;
	    }
	    break;
	case SNAKE_DOWN:
	    for (int i = 0; i < snake.size; i++) {
		mvaddch(snake.block[i].x, snake.block[i].y, ' ');
		snake.block[i].x++;
	    }
	    break;
	}

	snake_paint(&snake);
	/* Update head */
	/* mvaddch(snake.headx, snake.heady, SNAKE_LOOKS); */
	/* refresh(); */

	/* Kill tail */
	/* mvaddch(snake.tailx, --snake.taily, ' '); */

	usleep(100000);
    }
    
    if ((s = pthread_join(t1, &res)) != 0)
    	exit(1);

    endwin();
    return 0;
}

