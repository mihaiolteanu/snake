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
static SnakeBlock snake_new_head_pos(Snake *snake, SnakeBlock block, bool keep_tail) {
    SnakeBlock oldtail;

    /* Handle the head. */
    if (snake->head == snake->capacity - 1)
	/* Reached the end of buffer list, start from the beginning. */
	snake->head = 0;
    else 
	snake->head++;
    snake->block[snake->head] = block;
    
    /* Handle the tail. */
    oldtail = snake->block[snake->tail];
    if (keep_tail == false) {
	if (snake->tail == snake->capacity - 1)
	    snake->tail = 0;
	else
	    snake->tail++;
    }
    /* else */
	/* snake->size++; */
    return oldtail;
}

static SnakeBlock snake_head_coordinates(Snake *snake) {
    SnakeBlock head;

    head = snake->block[snake->head];
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
    SNAKE_UNKNOWN,
    SNAKE_UP,
    SNAKE_DOWN,
} SnakeDirection;

static SnakeDirection snake_direction;
static int should_exit;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static void mutex_lock() {
    int ret;
    if ((ret = pthread_mutex_lock(&mtx)) != 0)
	exit(1);
}

static void mutex_unlock() {
    int ret;
    if ((ret = pthread_mutex_unlock(&mtx)) != 0)
	exit(1);
}

static void *snake_update_direction(void *arg) {
    int key, new_direction, diff;
    
    while(should_exit != 1) {
	key = getch();      
	new_direction = key == KEY_LEFT ? SNAKE_LEFT:
	    key == KEY_RIGHT ? SNAKE_RIGHT:
	    key == KEY_UP ? SNAKE_UP:
	    key == KEY_DOWN ? SNAKE_DOWN : SNAKE_UNKNOWN;

	/* Current direction and the new direction cannot be next to
	   each other in the enum. This would mean going backwards. */
	diff = new_direction - snake_direction;
	if ((diff != 1) && (diff != -1)) {
            mutex_lock();
	    snake_direction = new_direction;
	    mutex_unlock();
	}
	
	usleep(100000);		/* Avoid goind backwards by tapping keys in quick succession. */
    }
    return (void *)0;
}

typedef enum {
    GOODIE_APPLE,
    GOODIE_TOTAL
} Goodie;

static chtype goodies[GOODIE_TOTAL] = {
    'x'
};

// https://stackoverflow.com/questions/2509679/how-to-generate-a-random-number-from-within-a-range
static unsigned int rand_interval(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    do
    {
        r = rand();
    } while (r >= limit);
    return min + (r / buckets);
}

/* Put a new goodie on the x, y map */
static void world_new_goodie(Goodie goodie, int x, int y) {
    int xpos, ypos;

    xpos = rand_interval(0, x);
    ypos = rand_interval(0, y);

    mvaddch(xpos, ypos, goodies[goodie]);
}

int main() {
    pthread_t t1;  
    int ch, will_exit;
    int row, col, next_goodie;
    bool goodie_on_screen;
    Snake snake;
    SnakeDirection direction;

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

    /* Initialize the world. */
    next_goodie = 0;
    goodie_on_screen = false;

    /* Watch for keyboard input. */
    if ((pthread_create(&t1, NULL, snake_update_direction, NULL)) != 0)
    	exit(1);

    /* Main loop. */
    while (will_exit != 1) {
    	/* Maybe the user has changes the snake direction or wants to go home */
	mutex_lock();
        direction = snake_direction;
    	will_exit = should_exit;
	mutex_unlock();
        
	SnakeBlock head, tail;
	head = snake_head_coordinates(&snake);
        
    	switch(direction) {
    	case SNAKE_LEFT:
	    head.y = head.y == 0 ? col : --head.y;
	    break;
    	case SNAKE_RIGHT:
	    head.y = head.y == col ? 0 : ++head.y;
	    break;
    	case SNAKE_UP:
	    head.x = head.x == 0 ? row : --head.x;
	    break;
    	case SNAKE_DOWN:
	    head.x = head.x == row ? 0 : ++head.x;
	    break;
    	}

        /* It looks bad if the next head position is a snake block. */
	chtype next_head = mvinch(head.x, head.y);
	if (next_head != SNAKE_LOOKS) {
	    if (next_head == goodies[GOODIE_APPLE]) {
		tail = snake_new_head_pos(&snake, head, true);
		goodie_on_screen = false;
	    }
	    else {
		tail = snake_new_head_pos(&snake, head, false);
		snake_paint_block(tail, ' ');
	    }
	    snake_paint(&snake);
	    /* Tail erase. */
	    /* mvprintw(25, 0, "Old: %d, %d", oldtail.x, oldtail.y); */
	    refresh();
	}

	if (next_goodie == 0) {
	    if (goodie_on_screen == false) {
		world_new_goodie(GOODIE_APPLE, row, col);
		goodie_on_screen = true;
		next_goodie = 25;
	    }
	    else
		; /* Wait for goodie to be catched */

	}
	else
	    next_goodie--;
        
	usleep(50000);
    }

    void *res;
    if ((pthread_join(t1, &res)) != 0)
    	exit(1);

    endwin();
    return 0;
}

