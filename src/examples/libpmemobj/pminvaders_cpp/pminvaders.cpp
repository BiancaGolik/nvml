/*
 * Copyright (c) 2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pminvaders.cpp -- example of usage c++ bindings in nvml
 */

#include <iostream>
#include <fstream>
#include <ncurses.h>
#include <libpmemobj.hpp>


#define LAYOUT_NAME "pminvaders"
#define SIZE 40
#define MAX_SIZE 38
#define MAX_BOMBS 5
#define KEY_SPACE 32
#define	RAND_FIELD rand() % (SIZE - 2) + 1
#define EXPLOSION_TIME 20
#define	EXPLOSION_COUNTER 80
#define SLEEP_TIME 2 * CLOCKS_PER_SEC
#define GAME_DELAY 40000
#define SLEEP(t)\
	clock_t time = clock() + t;\
	while(time > clock()) {}\

using namespace pmem;
using namespace std;

typedef enum positions {
	UP_LEFT,
	UP_RIGHT,
	DOWN_LEFT,
	DOWN_RIGHT,
	POS_MIDDLE,
	POS_MAX
} position;

typedef enum directions {
	DOWN,
	RIGHT,
	UP,
	LEFT,
	STOP
} direction;

typedef enum fields {
	FREE,
	FOOD,
	WALL,
	PLAYER,
	ALIEN,
	EXPLOSION,
	BONUS,
	LIFE,
	BOMB
} field;

class point {
	public:
		p<int> x;
		p<int> y;
		p<int> prev_x;
		p<int> prev_y;
		p<field> cur_field;
		p<field> prev_field;
		point() {};
		point(int xf, int yf);
		point(position cor);
		void move_back();
		void move_home();
	protected:
		p<direction> dir;
		void move();
	private:
		position home;
};

class bomb : public point {
	public:
		bomb(int xf, int yf);
		p<bool> exploded;
		p<bool> used;
		void progress();
		void explosion();
		void print_time();
	private:
		p<unsigned> timer;
};

typedef vector<
	persistent_ptr<bomb>,
	pmem_allocator_basic<persistent_ptr<bomb>>
> bomb_vec;

class player : public point {
	public:
		player(position cor);
		void progress(int in, bomb_vec *bombs);
};

class alien : public point {
	public:
		alien(position cor);
		void progress();
};

class intro : public point {
	public:
		intro(int x, int y, direction d);
		void progress();
	private:
		p<int> color_flag;
		p<unsigned> num;
};

class state {
	public:
		p<unsigned> level;
		p<unsigned> timer;
		p<unsigned> n_aliens;
		p<unsigned> highscore;
		p<unsigned> score;
		p<bool> game_over;
		state();
		void print(unsigned hs);
		void reset();
		void dead();
		bool is_free(int x, int y);
		bool is_explosion(int x, int y);
		void set_board_elm(persistent_ptr<point> p);
		void add_points(int x, int y);
		bool is_last_alien_killed(int x, int y);
		void set_explosion(int x, int y, field f);
		void explosion(int x, int y, field f);
		inline field get_board_elm(int x, int y) {
			return board[y * SIZE + x];
		}
		inline void set_board_elm(int x, int y, field f) {
			board[y * SIZE + x] = f;
		}
	private:
		p<unsigned> life;
		vector<
			p<field>,
			pmem_allocator_basic<p<field>>
		> board;
		int shape(field f);
		void set_bonus(field f);
		void set_board();
		int find_wall(int x, int y, direction dir);
	};

class my_root {
	public:
		bool init();
		void game();
	private:
		persistent_ptr<player> pl;
		persistent_ptr<state> st;
		vector<
			persistent_ptr<alien>,
			pmem_allocator_basic<persistent_ptr<alien>>
		> aliens;
		vector<
			persistent_ptr<intro>,
			pmem_allocator_basic<persistent_ptr<intro>>
		> intro_p;
		bomb_vec bombs;
		p<unsigned> highscore;
		bool intro_loop();
		void print_start();
		void print_game_over();
		void new_game();
		void resume();
		void one_move(int in);
		void collision();
		void reset();
		void next_level();
		void reset_bombs();
		bool is_collision(persistent_ptr<point> p1,
						persistent_ptr<point> p2);
		void remove_alien(persistent_ptr<alien> a);
		void remove_bomb(persistent_ptr<bomb> b);
	};

pool<my_root> pop;

/*
 * point::point -- overloaded constructor for point class
 */
point::point(int xf, int yf)
{
	x = xf;
	y = yf;
	prev_x = xf;
	prev_y = yf;
	prev_field = FREE;
};

/*
 * point::point -- overloaded constructor for point class
 */
point::point(position cor)
{
	home = cor;
	prev_field = FREE;
	move_home();
}

/*
 * point::move_home -- move object to it's home possition
 */
void
point::move_home()
{
	prev_x = x;
	prev_y = y;

	switch (home) {
		case(UP_LEFT):
			x = 1;
			y = 1;
			break;
		case(UP_RIGHT):
			x = MAX_SIZE;
			y = 1;
			break;
		case(DOWN_LEFT):
			x = 1;
			y = MAX_SIZE;
			break;
		case(DOWN_RIGHT):
			x = MAX_SIZE;
			y = MAX_SIZE;
			break;
		case(POS_MIDDLE):
			x = MAX_SIZE / 2;
			y = MAX_SIZE / 2;
			break;
		default:
			break;
	}
}

/*
 * point::move_back -- move object to it's previous possition
 */
void
point::move_back()
{
	x = prev_x;
	y = prev_y;
}

/*
 * point::move -- move object in proper direction
 */
void
point::move()
{
	int tmp_x = 0, tmp_y = 0;
	switch(dir) {
		case LEFT:
			tmp_x = -1;
			break;
		case RIGHT:
			tmp_x = 1;
			break;
		case UP:
			tmp_y = -1;
			break;
		case DOWN:
			tmp_y = 1;
			break;
		default:
			break;
	}
	prev_x = x;
	prev_y = y;
	x = x + tmp_x;
	y = y + tmp_y;
}

/*
 * intro::intro -- overloaded constructor for intro class
 */
intro::intro(int x, int y, direction d) : point (x, y)
{
	color_flag = (field)(rand() % BOMB);
	if (d == DOWN || d == LEFT)
		num = y;
	else
		num = SIZE - y;
	dir = d;
}

/*
 * intro::progress -- perform one move
 */
void
intro::progress() {
	move();
	mvaddch(y, x * 2, COLOR_PAIR(color_flag) | ACS_DIAMOND);
	int max_size =  SIZE - num;
	if ((x == num && y == num) || (x == num && y == max_size) ||
		(x == max_size && y == num) || (x == max_size && y == max_size))
		dir = (direction)((dir + 1) % STOP);
}

/*
 * bomb::bomb -- overloaded constructor for bomb class
 */
bomb::bomb(int xf, int yf) : point(xf, yf)
{
	cur_field = BOMB;
	exploded = false;
	used = false;
	timer = EXPLOSION_COUNTER;
}

/*
 * bomb::progress -- checks in which state is bomb
 */
void
bomb::progress()
{
	timer -= 1;
	if (exploded == false && timer == 0) {
		explosion();
	}
	else if (timer == 0)
		used = true;
}

/*
 * bomb::explosion -- change state of bomb on exploded
 */
void
bomb::explosion() {
	exploded = true;
	timer = EXPLOSION_TIME;
}

/*
 * bomb::print_time -- print time to explosion
 */
void
bomb::print_time()
{
	if (!exploded)
		mvprintw(y, x * 2, "%u", timer / 10);
}

/*
 * player::player -- overloaded constructor for player class
 */
player::player(position cor) : point(cor)
{
	 cur_field = PLAYER;
}

/*
 * player::progress -- checks input from keyboard and sets proper direction
 */
void
player::progress(int in, bomb_vec *bombs)
{
	switch(in) {
	case KEY_LEFT:
		dir = LEFT;
		break;
	case KEY_RIGHT:
		dir = RIGHT;
		break;
	case KEY_UP:
		dir = UP;
		break;
	case KEY_DOWN:
		dir = DOWN;
		break;
	case KEY_SPACE: {
		dir = STOP;
		if (bombs->size() <= MAX_BOMBS)
			bombs->push_back(make_persistent<bomb>(x, y));
		}
		break;
	}
	move();
	dir = STOP;
}

/*
 * alien::alien -- overloaded constructor for alien class
 */
alien::alien(position cor) : point(cor)
{
	 cur_field = ALIEN;
	 prev_field = FOOD;
}

/*
 * alien::progress -- rand and set direction and move alien
 */
void
alien::progress()
{
	if (rand() % 3 == 0)
		dir = (direction)(rand() % STOP);
	move();
}

/*
 * state -- constructor for class state initializes boards and needed variables
 */
state::state()
{

	life = 3;
	level = 1;
	n_aliens = 1;
	score = 0;
	timer = 0;
	game_over = false;
	for (int i = 0; i < SIZE * SIZE; ++i)
		board.push_back(FREE);
	set_board();
}

/*
 * state::print -- print current board and informations about game
 */
void
state::print(unsigned hs)
{
	for (int i = 0; i < SIZE; i++) {
		for (int j = 0; j < SIZE; j++) {
			if (get_board_elm(j, i) != FREE)
				mvaddch(i, j * 2, shape(get_board_elm(j, i)));
		}
	}
	if (score > hs)
		highscore = score;
	mvprintw(SIZE + 1, 0, "Score: %d\t\tHighscore: %u\t\tLevel: %u\t"
				"   Timer: %u", score, highscore, level, timer);
	mvaddch(8, SIZE * 2 + 5, shape(FOOD));
	mvprintw(8, SIZE * 2 + 10, " +1 point");
	mvaddch(16, SIZE * 2 + 5, shape(BONUS));
	mvprintw(16, SIZE * 2 + 10, " +50 point");
	mvaddch(24, SIZE * 2 + 5, shape(ALIEN));
	mvprintw(24, SIZE * 2 + 10, " +100 point");
	mvaddch(32, SIZE * 2 + 5, shape(LIFE));
	mvprintw(32, SIZE * 2 + 10, " +1 life");

	for (int i = 0; i < life; i++)
		mvaddch(SIZE + 3, SIZE + life - i * 2, shape(PLAYER));

}

/*
 * state::dead -- executed when player lose life
 */
void
state::dead()
{
	life = life - 1;
	if (life <= 0) {
		game_over = true;
	}
}

/*
 * state::reset -- clean board to start new level
 */
void
state::reset()
{
	set_board();
	n_aliens = level;
	timer = 0;
}

/*
 * state::is_free -- check wheter field is free
 */
bool
state::is_free(int x, int y)
{
	if (get_board_elm(x, y) == WALL || get_board_elm(x, y) == BOMB)
		return false;
	return true;
}

/*
 * state::is_explosion -- check wheter given field is exploding
 */
bool
state::is_explosion(int x, int y)
{
	if (get_board_elm(x, y) == EXPLOSION)
		return true;
	return false;
}

/*
 * state::add_points -- check type of field and give proper number of points
 */
void
state::add_points(int x, int y)
{
	switch(get_board_elm(x, y)) {
		case FOOD :
			score++;
			break;
		case BONUS : {
			score = score + 50;
			set_bonus(BONUS);
		}
			break;
		case LIFE : {
			if (life < 3)
				life++;
			set_bonus(LIFE);
		}
			break;
		default:
			break;
	}
}

/*
 * state::is_last_alien_killed -- remove alien from board and check wheter
 * any other alien staied on board
 */
bool
state::is_last_alien_killed(int x, int y)
{
	set_board_elm(x, y, FREE);
	n_aliens -= 1;
	score = score + 100;
	if (n_aliens != 0)
		return false;
	level++;
	return true;
}

/*
 * state::set_board_elm -- set object on its current possition on the board
 * and clean previous possition
 */
void
state::set_board_elm(persistent_ptr<point> p)
{
	set_board_elm(p->x, p->y, p->cur_field);
	if (!(p->x == p->prev_x && p->y == p->prev_y))
		set_board_elm(p->prev_x, p->prev_y, p->prev_field);
}

/*
 * state::set_explosion --set exploded fields in proper way
 */
void
state::set_explosion(int x, int y, field f)
{
	field prev_f = get_board_elm(x, y);
	if (prev_f == BONUS || prev_f == LIFE)
		set_bonus(prev_f);
	set_board_elm(x, y, f);
}

/*
 * state::explosion -- mark exploded fields as exploded or free
 */
void
state::explosion(int x, int y, field f)
{
	for(int i = find_wall(x, y, UP); i < find_wall(x, y, DOWN); i++)
		set_explosion(x, i, f);

	for(int i = find_wall(x, y, LEFT); i < find_wall(x, y, RIGHT); i++)
		set_explosion(i, y, f);
}

/*
 * state::shape -- assign proper shape to different types of fields
 */
int
state::shape(field f)
{
	int color = COLOR_PAIR(f);
	if (f == FOOD)
		return color | ACS_BULLET;
	else if (f == WALL || f == EXPLOSION)
		return color | ACS_CKBOARD;
	else
		return color | ACS_DIAMOND;
}

/*
 * state::set_bonus -- find free field and set the bonus there
 */
void
state::set_bonus(field f)
{

	int x, y;
	x = RAND_FIELD;
	y = RAND_FIELD;
	while (get_board_elm(x, y) != FOOD && get_board_elm(x, y) != FREE) {
		x = RAND_FIELD;
		y = RAND_FIELD;
	}
	set_board_elm(x, y, f);
}

/*
 * state::set_board -- set board with initial values from file
 */
void
state::set_board()
{
	ifstream board_file;
	board_file.open("map");
	char num;
	int j = 0;
	for (unsigned i = 0; i < SIZE * (SIZE + 2); i++) {
		board_file.get(num);
		if (num >= '0' && num <= '9') {
			board[j] = ((field)(num - '0'));
			j++;
		}
	}
	board_file.close();
	set_bonus(BONUS);
	set_bonus(LIFE);
}

/*
 * state::find_wall -- finds first wall from given point in given direction
 */
int
state::find_wall(int x, int y, direction dir)
{
	switch(dir) {
		case LEFT: {
			for(int i = x; i >= 0; i--) {
				if (get_board_elm(i, y) == WALL)
					return i + 1;
			}
		}
		break;
		case RIGHT: {
			for(int i = x; i <= SIZE; i++) {
				if (get_board_elm(i, y) == WALL)
					return i;
			}
		}
		break;
		case UP: {
			for(int i = y; i >= 0; i--) {
				if (get_board_elm(x, i) == WALL)
					return i + 1;
			}
		}
		break;
		case DOWN: {
			for(int i = y; i <= SIZE; i++) {
				if (get_board_elm(x, i) == WALL)
					return i;
			}
		}
		break;
		default:
		break;
	}
	return 0;
}

/*
 * my_root::init -- initialize game
 */
bool
my_root::init()
{
	int in;
	if (st == nullptr || pl == nullptr)
		new_game();
	else {
		while ((in = getch()) != 'y') {
			mvprintw(SIZE / 4, SIZE / 4,
					"Do you want to continue game? [y/n]");
			if (in == 'n') {
				resume();
				break;
			}
		}
		if (in == 'y' && intro_p.size() == 0)
			return false;
	}

	try {
		pop.exec_tx([&]() {
			if (intro_p.size() == 0) {
				for (int i = 0; i < SIZE / 4; i++) {
					intro_p.push_back(make_persistent<intro>
								(i, i, DOWN));
					intro_p.push_back(make_persistent<intro>
							(SIZE - i, i, LEFT));
					intro_p.push_back(make_persistent<intro>
							(i, SIZE - i, RIGHT));
					intro_p.push_back(make_persistent<intro>
						(SIZE - i, SIZE - i, UP));
				}
			}
		});
	} catch (transaction_error err) {
			cout << err.what() << endl;
	}

	if (intro_loop() == true)
		return true;

	try {
		pop.exec_tx([&]() {
			for (auto p : intro_p)
				delete_persistent(p);
			intro_p.clear();
		});
	} catch (transaction_error err) {
			cout << err.what() << endl;
	}
	return false;
}

/*
 * my_root::game -- process game loop
 */
void
my_root::game()
{
	int in;
	while ((in = getch()) != 'q') {

		SLEEP(GAME_DELAY);
		erase();
		if (in == 'r')
			resume();

		if (!st->game_over)
			one_move(in);
		else
			print_game_over();
	}
}

/*
 * my_root::intro_loop -- display intro an wait for user's reaction
 */
bool
my_root::intro_loop()
{
	int in;
	while ((in = getch()) != 's') {
		print_start();
		try {
			pop.exec_tx([&]() {
				for (auto p : intro_p)
					p->progress();
			});
		} catch (transaction_error err) {
				cout << err.what() << endl;
		}
		SLEEP(GAME_DELAY);
		if (in == 'q')
			return true;
	}
	return false;
}

/*
 * my_root::print_start -- print intro incription
 */
void
my_root::print_start()
{
	erase();
	int x = SIZE / 1.8;
	int y = SIZE / 2.5;
	mvprintw(y + 0, x, "#######   #     #   #######   #    #");
	mvprintw(y + 1, x, "#     #   ##   ##   #     #   ##   #");
	mvprintw(y + 2, x, "#######   # # # #   #######   # #  #");
	mvprintw(y + 3, x, "#         #  #  #   #     #   #  # #");
	mvprintw(y + 4, x, "#         #     #   #     #   #   ##");
	mvprintw(y + 8, x, "          Press 's' to start        ");
	mvprintw(y + 9, x, "          Press 'q' to quit        ");
}

/*
 * my_root::print_game_over -- print game over inscription
 */
void
my_root::print_game_over()
{
	erase();
	int x = SIZE / 3;
	int y = SIZE / 6;
	mvprintw(y + 0, x, "#######   #######   #     #   #######");
	mvprintw(y + 1, x, "#         #     #   ##   ##   #      ");
	mvprintw(y + 2, x, "#   ###   #######   # # # #   ####   ");
	mvprintw(y + 3, x, "#     #   #     #   #  #  #   #      ");
	mvprintw(y + 4, x, "#######   #     #   #     #   #######");

	mvprintw(y + 6, x, "#######   #     #    #######   #######");
	mvprintw(y + 7, x, "#     #   #     #    #         #     #");
	mvprintw(y + 8, x, "#     #    #   #     ####      #######");
	mvprintw(y + 9, x, "#     #     # #      #         #   #  ");
	mvprintw(y + 10, x, "#######      #       #######   #     #");

	mvprintw(y + 13, x, "       Your final score is %u         ",
       							st->score);
	if (st->score == highscore)
		mvprintw(y + 14, x, "       YOU BET YOUR BEST SCORE!       ");
	mvprintw(y + 16, x, "          Press 'q' to quit           ");
	mvprintw(y + 17, x, "         Press 'r' to resume          ");
}
/*
 * my_root::new_game -- allocate state, player and aliens if root is empty
 */
void
my_root::new_game()
{
	try {
		pop.exec_tx([&]() {
			st = make_persistent<state>();
			pl = make_persistent<player>(POS_MIDDLE);
			aliens.push_back(make_persistent<alien>(UP_LEFT));
		});
	} catch (transaction_error err) {
		cout << err.what() << endl;
	}
}

/*
 * my_root::resume -- clean root pointer and start a new game
 */
void
my_root::resume()
{
	try {
		pop.exec_tx([&]() {
			delete_persistent(st);
			st = nullptr;
			delete_persistent(pl);
			pl = nullptr;
			for (auto a : aliens)
				delete_persistent(a);
			aliens.clear();
			for (auto b : bombs)
				delete_persistent(b);
			bombs.clear();
			for (auto p : intro_p)
				delete_persistent(p);
			intro_p.clear();
		});
	} catch (transaction_error err) {
		cout << err.what() << endl;
	}
	new_game();
}

/*
 * my_root::one_move -- process one round where every object moves one time
 */
void
my_root::one_move(int in)
{
	try {
		pop.exec_tx([&]() {
			st->timer = st->timer + 1;
			pl->progress(in, &bombs);
			for(auto a: aliens)
				a->progress();
			for(auto b: bombs) {
				b->progress();
				if (b->exploded)
					st->explosion(b->x, b->y, EXPLOSION);
				if (b->used) {
					st->explosion(b->x, b->y, FREE);
					remove_bomb(b);
					delete_persistent(b);
				}
			}
			collision();
			st->print(highscore);
			highscore = st->highscore;
			for(auto b: bombs)
					b->print_time();
		});
	} catch (transaction_error err) {
		cout << err.what() << endl;
	}
}

/*
 * my_root::collision -- check if is collision withe every two objects and
 * perform specified action
 */
void
my_root::collision()
{
	for (auto b : bombs) {
		if (!b->exploded) {
			if (st->is_explosion(b->x, b->y))
				b->explosion();
			st->set_board_elm(b);
		}
	}
	for (auto a : aliens) {
		if (st->is_explosion(a->x, a->y)) {
			bool is_over = st->is_last_alien_killed(
							a->prev_x, a->prev_y);
			remove_alien(a);
			delete_persistent(a);
			if (is_over == true) {
				next_level();
				return;
			}
		}
	}
	bool dead = false;
	for (auto a : aliens) {

		/*check collision alien with wall or bomb */
		if (!st->is_free(a->x, a->y))
			a->move_back();

		/*check collision alien with player */
		if (is_collision(pl, a))
			dead = true;

		/*check collision alien with alien */
		for (auto a2 : aliens) {
			if (a != a2 && is_collision(a, a2)) {
				a->move_back();
				break;
			}
		}
		field prev_f = st->get_board_elm(a->x, a->y);
		st->set_board_elm(a);
		if (prev_f != ALIEN && prev_f != PLAYER)
			a->prev_field = prev_f;
	}
	if (!st->is_free(pl->x, pl->y))
		pl->move_back();

	if (st->is_explosion(pl->x, pl->y) || dead) {
		st->dead();
		reset();
		return;
	}
	st->add_points(pl->x, pl->y);
	st->set_board_elm(pl);
	SLEEP(10000);

}

/*
 * my_root::reset -- move objects on their home positions
 */
void
my_root::reset()
{
	for(auto a: aliens) {
		a->move_home();
		st->set_board_elm(a);
	}
	pl->move_back();
	pl->move_home();
	st->set_board_elm(pl);
	reset_bombs();
}

/*
 * my_root::next_level -- clean board, create proper number of aliens and
 * start new level
 */
void
my_root::next_level()
{
	reset_bombs();
	st->reset();
	for (int i = 0; i < st->n_aliens; i++)
		aliens.push_back(make_persistent<alien>(
			(position)((UP_LEFT + i) % (POS_MAX - 1))));
	pl->move_home();
}

/*
 * my_root::reset_bombs -- remove all bombs
 */
void
my_root::reset_bombs()
{
	for (auto b : bombs) {
		if(b->exploded)
			st->explosion(b->x, b->y, FREE);
		delete_persistent(b);
	}
	bombs.clear();
}

/*
 * my_root::is_collision -- check if there is collision between given objects
 */
bool
my_root::is_collision(persistent_ptr<point> p1, persistent_ptr<point> p2)
{
	if (p1->x == p1->prev_x && p1->y == p2->prev_y)
		return false;
	if (p1->x == p2->x && p1->y == p2->y)
		return true;
	else if (p1->prev_x == p2->x && p1->prev_y == p2->y &&
			p1->x == p2->prev_x && p1->y == p2->prev_y)
		return true;
	return false;
}

/*
 * my_root::remove_alien -- removes alien from middle of vector
 */
void
my_root::remove_alien(persistent_ptr<alien> al)
{
	vector<persistent_ptr<alien>> tmp;
	for (auto a : aliens) {
		if (a != al)
			tmp.push_back(a);
	}
	aliens.clear();
	for (auto a : tmp)
		aliens.push_back(a);
}

/*
 * my_root::remove_bomb -- remove bomb from middle of vector
 */
void
my_root::remove_bomb(persistent_ptr<bomb> bb)
{
	vector<persistent_ptr<bomb>> tmp;
	for (auto b : bombs) {
		if (b != bb)
			tmp.push_back(b);
	}
	bombs.clear();
	for (auto b : tmp)
		bombs.push_back(b);
}

int
main(int argc, char *argv[])
{
	if (argc != 2)
		return 0;

	string name = argv[1];

	if (pop.exists(name, LAYOUT_NAME))
		pop.open(name, LAYOUT_NAME);
	else
		pop.create(name, LAYOUT_NAME);

	initscr();
	start_color();
	init_pair(FOOD, COLOR_YELLOW, COLOR_BLACK);
	init_pair(WALL, COLOR_WHITE, COLOR_BLACK);
	init_pair(PLAYER, COLOR_CYAN, COLOR_BLACK);
	init_pair(ALIEN, COLOR_RED, COLOR_BLACK);
	init_pair(EXPLOSION, COLOR_CYAN, COLOR_BLACK);
	init_pair(BONUS, COLOR_YELLOW, COLOR_BLACK);
	init_pair(LIFE, COLOR_MAGENTA, COLOR_BLACK);
	nodelay(stdscr, true);
	curs_set(0);
	keypad(stdscr, true);
	auto r = pop.get_root();

	if (r->init())
		goto out;
	r->game();
out:
	endwin();
	pop.close();
	return 0;
}
