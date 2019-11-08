#include <stdio.h>
#include <curses.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>


#define BOX_ROWS 30
#define BOX_COLS 40
#define CORNER_X 5
#define CORNER_Y 40

#define INIT_X 10
#define INIT_Y 20
#define INIT_X_INTERVAL 20
#define INIT_Y_INTERVAL 20
#define SYMBOL 'O'

#define GUARD_LEN 10
#define BLANK ' '


typedef struct ball
{
	int x_pos, y_pos;
	int x_dir, y_dir;
	int xmove_interval;
	int ymove_interval;
	int xtick, ytick;
	char symbol;
}ball;

typedef struct guard
{
	int len;
	int y_pos;
	int x_pos;
	char *entity;
}guard;


void initialize();

void set_scr_mode();

void draw_box();

void set_interval_timer();

void set_alrm_handler();

void set_io_handler();

void set_tty_mode();

void tty_mode_handler(int how);

void io_handler(int signum);

void set_int_handler();

void int_handler(int signum);

void ball_move(int signum);

void accelerate(int score);

void guard_move(int dir);

int is_bounce(int x_pos, int y_pos);

int is_fail(int x_pos, int y_pos);

void bounce();

void fix();

void destroy();

void show_score(int score);

static ball ball1;
static guard guard1;
static WINDOW *win;


int main()
{
	tty_mode_handler(0);
	set_scr_mode();
	initialize();

	set_int_handler();
	draw_box();

	set_interval_timer();
	set_alrm_handler();

	set_tty_mode();
	set_io_handler();
	
	while(1)
		pause();
	
	return 0;
}

void destroy()
{
	free(guard1.entity);
	delwin(win);
	endwin();
	printf("\033[?25h");
	tty_mode_handler(1);

}


void set_interval_timer()
{
	struct itimerval itv;

	itv.it_value.tv_sec = 3;
	itv.it_value.tv_usec = 0;
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 10000;

	if(setitimer(ITIMER_REAL, &itv, NULL) == -1){
		perror("set interval timer");
		destroy();
	}
}

void set_alrm_handler()
{
	struct sigaction sa;
	sa.sa_handler = ball_move;
	sa.sa_flags = SA_RESTART;
	
	if(sigaction(SIGALRM, &sa, NULL) == -1)
	{
		perror("set alrm_handler");
		destroy();
		exit(EXIT_FAILURE);
	}
}

void tty_mode_handler(int how)
{	
	static int fd_flags;

	if(how == 0)
	{
		fd_flags = fcntl(0, F_GETFL);
	}
	else if(how == 1)
	{
		fcntl(0, F_SETFL, fd_flags);
	}
}

void set_tty_mode()
{
	if(fcntl(0, F_SETOWN, getpid()) == -1)
	{
		perror("F_SETOWN");
		destroy();
	}

	int fd_flags = fcntl(0, F_GETFL);
	fd_flags |= O_ASYNC;

	if(fcntl(0, F_SETFL, fd_flags) == -1)
	{
		perror("O_ASYNC");
		destroy();
	}
}

void set_io_handler()
{
	struct sigaction sa;
	sa.sa_handler = io_handler;

	if(sigaction(SIGIO, &sa, NULL) == -1)
	{
		perror("set io_handler");
		destroy();
	}
}

void io_handler(int signum)
{
	signal(SIGIO, SIG_IGN);

	int dir = getch();
	if(tolower(dir) == 'q')
	{
		destroy();
		exit(EXIT_SUCCESS);
	}

	switch(dir)
	{
		case KEY_LEFT:
			guard_move(-1);
			break;
		
		case KEY_RIGHT:
			guard_move(1);
			break;
	}

	set_io_handler();
}


void set_int_handler()
{
	struct sigaction sa;
	sa.sa_handler = int_handler;

	if(sigaction(SIGINT, &sa, NULL) == -1)
	{
		perror("set int_handler");
		exit(EXIT_FAILURE);
	}
}


void int_handler(int signum)
{
	destroy();
	exit(EXIT_SUCCESS);
}



void set_scr_mode()
{	
	printf("\033[?25l");
    cbreak();
    noecho();

	initscr();
	keypad(stdscr, TRUE);
    refresh();
}

void initialize()
{
	ball1.x_pos = INIT_X;
	ball1.y_pos = INIT_Y;
	ball1.x_dir = -1;
	ball1.y_dir = -1;
	ball1.xmove_interval = INIT_X_INTERVAL;
	ball1.ymove_interval = INIT_Y_INTERVAL;
	ball1.xtick = 0;
	ball1.ytick = 0;
	ball1.symbol = SYMBOL;

	guard1.len = GUARD_LEN;
	guard1.x_pos = BOX_ROWS - 1;
	guard1.y_pos = BOX_COLS / 2;

	guard1.entity = (char*)malloc(sizeof(char) * (GUARD_LEN + 1));
	for(int i = 0; i < GUARD_LEN; ++i)
		guard1.entity[i] = '_';
	guard1.entity[GUARD_LEN] = '\0';

	win = newwin(BOX_ROWS, BOX_COLS, CORNER_X, CORNER_Y);
}


void guard_move(int dir)
{
	int old_y_pos = guard1.y_pos;
	guard1.y_pos += dir;
	
	if(dir == 1)
	{
		if(guard1.y_pos + guard1.len - 1 > BOX_COLS - 1)
		{
			guard1.y_pos -= dir;
			return;
		}

		mvwaddch(win, guard1.x_pos, old_y_pos, BLANK);
		mvwaddch(win, guard1.x_pos, guard1.y_pos + guard1.len - 1, '_');
	}
	else
	{
		if(guard1.y_pos < 0)
		{
			guard1.y_pos -= dir;
			return;
		}

		mvwaddch(win, guard1.x_pos, old_y_pos + guard1.len - 1, BLANK);
		mvwaddch(win, guard1.x_pos, guard1.y_pos, '_');
	}

	wrefresh(win);
}


void bounce()
{
	if(ball1.x_pos <= 0 || ball1.x_pos >= guard1.x_pos)
	{
		ball1.x_dir = (ball1.x_dir) * (-1);
	}

	if(ball1.y_pos <= 0 || ball1.y_pos >= BOX_COLS - 1)
	{
		ball1.y_dir = (ball1.y_dir) * (-1);
	}
}


void ball_move(int signum)
{
	signal(SIGALRM, SIG_IGN);

	++ball1.xtick;
	++ball1.ytick;
	
	int moved = 0;
	static int last_bounced = 0;
	static int score = 0;

	int old_x_pos = ball1.x_pos;
	int old_y_pos = ball1.y_pos;

	if(ball1.xtick >= ball1.xmove_interval)
	{
		ball1.x_pos += ball1.x_dir;
		ball1.xtick = 0;
		moved = 1;
	}

	if(ball1.ytick >= ball1.ymove_interval)
	{
		ball1.y_pos += ball1.y_dir;
		ball1.ytick = 0;
		moved = 1;
	}
	
	if(moved)
	{
		mvwaddch(win, ball1.x_pos, ball1.y_pos, ball1.symbol);
		mvwaddch(win, old_x_pos, old_y_pos, BLANK);

		if(last_bounced == 1)
		{
			fix();
			last_bounced = 0;
		}
		
		wrefresh(win);

		if(is_fail(ball1.x_pos, ball1.y_pos))
		{
			mvwaddstr(win, BOX_ROWS / 2,
					  BOX_COLS / 2 - 3,
					  "Game Over");
			wrefresh(win);
			sleep(3);
			destroy();
			exit(EXIT_SUCCESS);
		}

		if(is_bounce(ball1.x_pos, ball1.y_pos))
		{
			bounce();
			last_bounced = 1;
		}
	
		++score;
		accelerate(score);	
		show_score(score);
	}
	
	set_alrm_handler();
}

void accelerate(int score)
{
	switch(score)
	{
		case 30:
		case 100:
		case 200:
		case 500:
		case 800:
		case 2000:
			ball1.xmove_interval -= 3;
			ball1.ymove_interval -= 3;
	}
}


void fix()
{
	box(win, ACS_VLINE, ACS_HLINE);
	
	for(int col = 0; col < BOX_COLS; ++col)
    {
		mvwaddch(win, guard1.x_pos, col, BLANK);
    }

    mvwaddstr(win, guard1.x_pos, guard1.y_pos, guard1.entity);	
}


void draw_box()
{
	box(win, ACS_VLINE, ACS_HLINE);
	for(int col = 0; col < BOX_COLS; ++col)
	{
		mvwaddch(win, guard1.x_pos, col, BLANK);
	}
	
	mvwaddch(win, ball1.x_pos, ball1.y_pos, ball1.symbol);
	mvwaddstr(win, guard1.x_pos, guard1.y_pos, guard1.entity);
	
	wrefresh(win);
}

int is_fail(int x_pos, int y_pos)
{
	return x_pos > guard1.x_pos ? 1 : 0;
}

int is_bounce(int x_pos, int y_pos)
{
	if(y_pos <= 0 || y_pos >= BOX_COLS - 1 ||
	   x_pos <= 0)
		return 1;

	
	if(x_pos == guard1.x_pos && 
	   y_pos >= guard1.y_pos &&
	   y_pos <= guard1.y_pos + guard1.len - 1)
		return 1;

	return 0;
}


void show_score(int score)
{
	int x_pos,y_pos;
	
	x_pos = CORNER_X + BOX_ROWS / 2;
    y_pos = CORNER_Y + BOX_COLS + 8;

	mvprintw(x_pos, y_pos, "SCORE: %d", score);
	wrefresh(stdscr);
}








