#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define SNAKE_MAX_SIZE 25
#define SNAKE_INITIAL_SIZE 5
#define SNAKE_LOOKS ACS_DIAMOND

typedef struct {
    int x, y;
} SnakeBlock;

typedef struct {
    int size;
    int capacity;
    int head;
    int tail;
    SnakeBlock block[SNAKE_MAX_SIZE];
} Snake;

/* Initialize the snake starting from pos (x, y) */
static void snake_init(Snake *snake, int x, int y) {
    snake->size = SNAKE_INITIAL_SIZE;
    snake->capacity = SNAKE_MAX_SIZE;
    snake->head = SNAKE_INITIAL_SIZE - 1;
    snake->tail = 0;
    for (int i = 0; i < snake->size; i++) {
	snake->block[i].x = x;
	snake->block[i].y = y--;
    }
}

/* Move snake head to a new position, advancing the tail as well.
 The old tail position is returned to the calling function. */
static SnakeBlock snake_new_head_pos(Snake *snake, int newx, int newy) {
    SnakeBlock oldtail;

    /* Handle the head. */
    if (snake->head == snake->capacity)
	/* Reached the end of buffer list, start from the beginning. */
	snake->head = 0;
    else 
	snake->head++;
    snake->block[snake->head].x = newx;
    snake->block[snake->head].y = newy;    
    
    /* Handle the tail. */
    oldtail.x = snake->block[snake->tail].x;
    oldtail.y = snake->block[snake->tail].y;
    if (snake->tail == snake->capacity)
	snake->tail = 0;
    else
	snake->tail++;
    
    return oldtail;
}

static SnakeBlock snake_head_coordinates(Snake *snake) {
    SnakeBlock head;

    head.x = snake->block[snake->head].x;
    head.y = snake->block[snake->head].y;

    return head;
}

static void snake_paint_block(SnakeBlock block, chtype looks) {
    mvaddch(block.x, block.y, looks);
}

static void snake_paint(Snake *snake) {
    int start1, stop1, start2, stop2;

    start1 = stop1 = stop2 = 0;
    start2 = 1;			/* Don't do extra painting if not needed. */

    if (snake->head > snake->tail) {
	start1 = snake->tail;
	stop1 = snake->head;
    }
    else {
	/* Two sections to paint, one at the start, one at the end of the array */
    	start1 = 0;
    	stop1 = snake->head;
        start2 = snake->tail;
    	stop2 = snake->capacity - 1;
    }
    
    for (int i = start1; i <= stop1; i++)
	snake_paint_block(snake->block[i], SNAKE_LOOKS);
    for (int i = start2; i <= stop2; i++)
	snake_paint_block(snake->block[i], SNAKE_LOOKS);
    refresh();
}

typedef enum {
    SNAKE_LEFT,
    SNAKE_RIGHT,
    SNAKE_UP,
    SNAKE_DOWN,
    SNAKE_UNKNOWN
} snake_direction_enum;


static snake_direction_enum snake_direction;
static int should_exit;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static void *snake_update_direction(void *arg) {
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
	
	usleep(100000);		/* Avoid goind backwards by tapping keys in quick succession. */
    }
    
    return (void *)0;
}

int main() {
    pthread_t t1;
    void *res;
    int s, ch, will_exit;
    int row, col;

    Snake snake;
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
    snake_init(&snake, row/2, col/2);
    snake_paint(&snake);

    /* Watch for keyboard input. */
    if ((s = pthread_create(&t1, NULL, snake_update_direction, NULL)) != 0)
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

	SnakeBlock head, tail;
	head = snake_head_coordinates(&snake);
	int nextx, nexty;

    	switch(direction) {
    	case SNAKE_LEFT:
            nextx = head.x;
	    nexty = head.y - 1;
    	    break;
    	case SNAKE_RIGHT:
            nextx = head.x;
	    nexty = head.y + 1;
    	    break;
    	case SNAKE_UP:
            nextx = head.x - 1;
	    nexty = head.y;
    	    break;
    	case SNAKE_DOWN:
            nextx = head.x + 1;
	    nexty = head.y;
    	    break;
    	}

        tail = snake_new_head_pos(&snake, nextx, nexty);
	snake_paint(&snake);
	snake_paint_block(tail, ' '); /* Tail erase. */
	/* mvprintw(25, 0, "Old: %d, %d", oldtail.x, oldtail.y); */
	refresh();
        
	usleep(100000);
    }

    if ((s = pthread_join(t1, &res)) != 0)
    	exit(1);

    endwin();
 
    return 0;
}

