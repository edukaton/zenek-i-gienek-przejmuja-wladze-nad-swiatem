/*! \file empty.c
 *  \brief Empty gamestate.
 */
/*
 * Copyright (c) Sebastian Krzyszkowiak <dos@dosowisko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../common.h"
#include <libsuperderpy.h>
#include <math.h>
#include <stdio.h>

enum ENTITY_TYPE {
	TYPE_ENEMY,
	TYPE_USER,
	TYPE_BULLET,
	TYPE_MESSAGE,
	TYPE_FAKE,
	TYPE_EXPLOSION
};

struct Entity {
	double x, y, angle, distance;
	enum ENTITY_TYPE type;
	bool used;
	int score;
};

struct GamestateResources {
	// This struct is for every resource allocated and used by your gamestate.
	// It gets created on load and then gets passed around to all other function calls.

	ALLEGRO_BITMAP *internet, *bg, *pixelator;
	ALLEGRO_FONT *font, *bff;
	double x, y, angle;
	int w, h;

	int score;

	bool spawning;

	bool up, left, down, right;

	struct Character *car, *police, *teeth, *user, *fake, *news, *bad, *explosion;

	struct Timeline* timeline;

	struct Entity entities[8192];
	int entities_count;

	int fake_counter;

	double wskaznik;

	int count;
	int pew;
	int tilt;

	bool move;
	bool showlogo;
	ALLEGRO_BITMAP *logo, *endscreen, *endscreen1, *endscreen2, *endscreen3;
	int fade;

	struct {
		ALLEGRO_SAMPLE* sample;
		ALLEGRO_SAMPLE_INSTANCE* sound;
		bool used;
	} bullets[10];

	struct {
		ALLEGRO_SAMPLE* sample;
		ALLEGRO_SAMPLE_INSTANCE* sound;
		bool used;
	} explosions[8];

	bool ended;
};

int Gamestate_ProgressCount = 1; // number of loading steps as reported by Gamestate_Load

/* Function: al_transform_coordinates_4d
 */
void al_transform_coordinates_4d(const ALLEGRO_TRANSFORM* trans,
  float* x, float* y, float* z, float* w) {
	float rx, ry, rz, rw;

#define M(i, j) trans->m[i][j]

	rx = M(0, 0) * *x + M(1, 0) * *y + M(2, 0) * *z + M(3, 0) * *w;
	ry = M(0, 1) * *x + M(1, 1) * *y + M(2, 1) * *z + M(3, 1) * *w;
	rz = M(0, 2) * *x + M(1, 2) * *y + M(2, 2) * *z + M(3, 2) * *w;
	rw = M(0, 3) * *x + M(1, 3) * *y + M(2, 3) * *z + M(3, 3) * *w;

#undef M

	*x = rx;
	*y = ry;
	*z = rz;
	*w = rw;
}

/* Function: al_transform_coordinates_3d_projective
 */
void al_transform_coordinates_3d_projective(const ALLEGRO_TRANSFORM* trans,
  float* x, float* y, float* z) {
	float w = 1;
	al_transform_coordinates_4d(trans, x, y, z, &w);
	*x /= w;
	*y /= w;
	*z /= w;
}

static bool Speak(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);
	ALLEGRO_AUDIO_STREAM* stream = TM_GetArg(action->arguments, 1);
	char* text = TM_GetArg(action->arguments, 2);
	char* person = TM_GetArg(action->arguments, 3);

	if (state == TM_ACTIONSTATE_INIT) {
		al_set_audio_stream_playing(stream, false);
		al_set_audio_stream_playmode(stream, ALLEGRO_PLAYMODE_ONCE);
	}

	if (state == TM_ACTIONSTATE_START) {
		game->data->skip = false;
		game->data->text = text;
		game->data->person = person;
		//al_rewind_audio_stream(stream);
		al_attach_audio_stream_to_mixer(stream, game->audio.voice);
		al_set_audio_stream_playing(stream, true);
	}

	if (state == TM_ACTIONSTATE_RUNNING) {
		return !al_get_audio_stream_playing(stream) || game->data->skip;
	}

	if (state == TM_ACTIONSTATE_DESTROY) {
		al_destroy_audio_stream(stream);
		game->data->text = NULL;
	}
	return false;
}

static bool ShowLogo(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);

	if (state == TM_ACTIONSTATE_START) {
		data->showlogo = true;
	}
	return true;
}

static bool SwitchEndScreen(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);
	ALLEGRO_BITMAP* screen = TM_GetArg(action->arguments, 1);

	if (state == TM_ACTIONSTATE_START) {
		data->endscreen = screen;
	}
	return true;
}

static bool HideLogo(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);

	if (state == TM_ACTIONSTATE_START) {
		data->showlogo = false;
	}
	return true;
}

static bool StartGame(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);

	if (state == TM_ACTIONSTATE_START) {
		data->move = true;
	}
	return true;
}

static struct Entity* SpawnEntity(struct Game* game, struct GamestateResources* data, double x, double y, double angle, enum ENTITY_TYPE type) {
	while (data->entities[data->entities_count].used) {
		data->entities_count++;
		if (data->entities_count >= 8192) {
			data->entities_count = 0;
		}
	}

	int id = data->entities_count;
	data->entities[id].used = true;
	data->entities[id].type = type;
	data->entities[id].x = x;
	data->entities[id].y = y;
	data->entities[id].angle = angle;
	data->entities[id].distance = 0;

	data->entities_count++;
	if (data->entities_count >= 8192) {
		data->entities_count = 0;
	}

	if (type == TYPE_FAKE) {
		data->fake_counter++;
	}

	return &data->entities[id];
}

static bool SpawnEnemies(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);

	if (state == TM_ACTIONSTATE_START) {
		for (int i = 0; i < 32; i++) {
			double angle = rand() / ALLEGRO_PI;
			SpawnEntity(game, data, data->x + sin(angle) * (222 + rand() % 300), data->y + cos(angle) * (222 + rand() % 300), rand() / ALLEGRO_PI, TYPE_USER);
		}
		for (int i = 0; i < 4; i++) {
			double angle = rand() / ALLEGRO_PI;
			SpawnEntity(game, data, data->x + sin(angle) * (222 + rand() % 300), data->y + cos(angle) * (222 + rand() % 300), rand() / ALLEGRO_PI, TYPE_MESSAGE);
		}
		for (int i = 0; i < 2; i++) {
			double angle = rand() / ALLEGRO_PI;
			SpawnEntity(game, data, data->x + sin(angle) * (222 + rand() % 300), data->y + cos(angle) * (222 + rand() % 300), rand() / ALLEGRO_PI, TYPE_ENEMY);
		}
		data->spawning = true;
	}
	return true;
}

static bool SpawnSingleFake(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);

	if (state == TM_ACTIONSTATE_START) {
		SpawnEntity(game, data, data->x + sin(data->angle) * 200, data->y + cos(data->angle) * 200, rand() / ALLEGRO_PI, TYPE_FAKE);
	}
	return true;
}

static bool SpawnSingleEnemy(struct Game* game, struct TM_Action* action, enum TM_ActionState state) {
	struct GamestateResources* data = TM_GetArg(action->arguments, 0);

	if (state == TM_ACTIONSTATE_START) {
		SpawnEntity(game, data, data->x + sin(data->angle) * 200, data->y + cos(data->angle) * 200, rand() / ALLEGRO_PI, TYPE_ENEMY);
	}
	return true;
}

void Gamestate_Logic(struct Game* game, struct GamestateResources* data, double delta) {
	// Called 60 times per second (by default). Here you should do all your game logic.
	TM_Process(data->timeline);

	if (data->ended) {
		return;
	}

	data->count++;

	if (!data->move) {
		return;
	}

	if (data->fade < 255) {
		data->fade++;
	}

	if (data->fake_counter > 64) {
		int s = rand() % 8;
		al_stop_sample_instance(data->explosions[s].sound);
		al_play_sample_instance(data->explosions[s].sound);
		data->ended = true;
		TM_CleanQueue(data->timeline);
		TM_AddDelay(data->timeline, 2000);

		TM_AddAction(data->timeline, &SwitchEndScreen, TM_AddToArgs(NULL, 2, data, data->endscreen1), "switch");

		TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/outro1.flac"), 4, 1024), "", ""), "speak");
		TM_AddAction(data->timeline, &SwitchEndScreen, TM_AddToArgs(NULL, 2, data, data->endscreen2), "switch");

		TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/outro2.flac"), 4, 1024), "", ""), "speak");

		TM_AddAction(data->timeline, &SwitchEndScreen, TM_AddToArgs(NULL, 2, data, data->endscreen3), "switch");

		return;
	}

	AnimateCharacter(game, data->car, delta, 1.0);
	AnimateCharacter(game, data->teeth, delta, 1.0);
	AnimateCharacter(game, data->user, delta, 1.0);
	AnimateCharacter(game, data->fake, delta, 1.0);
	AnimateCharacter(game, data->news, delta, 1.0);
	AnimateCharacter(game, data->bad, delta, 1.0);
	AnimateCharacter(game, data->explosion, delta, 1.0);

	if (data->spawning) {
		if (data->count % 60 * 10 == 0) {
			double angle = rand() / ALLEGRO_PI;
			SpawnEntity(game, data, data->x + sin(angle) * (222 + rand() % 300), data->y + cos(angle) * (222 + rand() % 300), rand() / ALLEGRO_PI, rand() % 4 == 0 ? TYPE_ENEMY : TYPE_USER);
		}
	}

	if (data->left) {
		data->angle -= 0.02;
	}

	if (data->right) {
		data->angle += 0.02;
	}

	data->x += sin(data->angle) * 1;
	data->y += cos(data->angle) * 1;

	if (data->up) {
		data->x += sin(data->angle) * 1;
		data->y += cos(data->angle) * 1;
	}

	if (data->down) {
		data->x -= sin(data->angle) * 0.5;
		data->y -= cos(data->angle) * 0.5;
	}

	if (data->pew) {
		data->pew--;
	}

	if (data->tilt) {
		data->tilt--;
	}

	for (int j = 0; j < 8192; j++) {
		if (data->entities[j].used) {
			if ((data->entities[j].type != TYPE_BULLET) && (data->entities[j].type != TYPE_EXPLOSION)) {
				if ((fabs(data->x - data->entities[j].x) < 12) && (fabs(data->y - data->entities[j].y) < 12)) {
					if (data->entities[j].type == TYPE_FAKE) {
						data->fake_counter--;
						data->entities[j].score = 100;
					} else if (data->entities[j].type != TYPE_ENEMY) {
						data->fake_counter += 2;
						data->entities[j].score = -500;
					} else {
						data->entities[j].score = 500;
					}
					data->score += data->entities[j].score;
					int s = rand() % 8;
					al_stop_sample_instance(data->explosions[s].sound);
					al_play_sample_instance(data->explosions[s].sound);

					data->entities[j].type = TYPE_EXPLOSION;
					data->entities[j].distance = 0;
					data->tilt += 20;
					break;
				}
			}
		}
	}

	for (int i = 0; i < 8192; i++) {
		if (data->entities[i].used) {
			if (data->entities[i].type == TYPE_BULLET) {
				data->entities[i].x += sin(data->entities[i].angle) * 5;
				data->entities[i].y += cos(data->entities[i].angle) * 5;

				data->entities[i].distance += sqrt(pow(sin(data->entities[i].angle) * 5, 2) + pow(cos(data->entities[i].angle) * 5, 2));

				if (data->entities[i].distance > 300) {
					data->entities[i].used = false;
				}

				for (int j = 0; j < 8192; j++) {
					if (data->entities[j].used) {
						if ((data->entities[j].type != TYPE_BULLET) && (data->entities[j].type != TYPE_EXPLOSION)) {
							if ((fabs(data->entities[i].x - data->entities[j].x) < 8) && (fabs(data->entities[i].y - data->entities[j].y) < 8)) {
								if (data->entities[j].type == TYPE_FAKE) {
									data->fake_counter--;
									data->entities[j].score = 100;
								} else if (data->entities[j].type != TYPE_ENEMY) {
									data->fake_counter += 2;
									data->entities[j].score = -500;
								} else {
									data->entities[j].score = 500;
								}
								data->score += data->entities[j].score;

								int s = rand() % 8;
								al_stop_sample_instance(data->explosions[s].sound);
								al_play_sample_instance(data->explosions[s].sound);

								data->entities[j].type = TYPE_EXPLOSION;
								data->entities[j].distance = 0;
								data->entities[i].used = false;
								data->tilt += 20;
								break;
							}
						}
					}
				}
			}

			if ((data->entities[i].type == TYPE_USER) || (data->entities[i].type == TYPE_ENEMY)) {
				data->entities[i].x += sin(data->entities[i].angle) * 0.5;
				data->entities[i].y += cos(data->entities[i].angle) * 0.5;

				data->entities[i].distance += sqrt(pow(sin(data->entities[i].angle) * 0.5, 2) + pow(cos(data->entities[i].angle) * 0.5, 2));

				if (data->entities[i].distance > 200) {
					data->entities[i].angle = rand() / ALLEGRO_PI;
					data->entities[i].distance = 0;

					if (data->entities[i].type == TYPE_ENEMY) {
						SpawnEntity(game, data, data->entities[i].x, data->entities[i].y, 0, TYPE_FAKE);
					} else {
						if (rand() % 30 > 20) {
							SpawnEntity(game, data, data->entities[i].x, data->entities[i].y, 0, TYPE_MESSAGE);
						}
					}
				}
			}

			if (data->entities[i].type == TYPE_EXPLOSION) {
				data->entities[i].distance++;
				if (data->entities[i].distance > 16) {
					data->entities[i].used = false;
				}
			}
		}
	}
}

void Gamestate_Draw(struct Game* game, struct GamestateResources* data) {
	// Called as soon as possible, but no sooner than next Gamestate_Logic call.
	// Draw everything to the screen here.
	//al_draw_scaled_bitmap(data->internet, 0, 0, 4096, 4096, 0, 0, 320, 180, 0);

	if (data->ended) {
		if (data->endscreen) {
			al_set_target_backbuffer(game->display);
			al_draw_bitmap(data->endscreen, 0, 0, 0);
		}
		return;
	}

	ALLEGRO_TRANSFORM transform, perspective, camera;

	al_set_target_bitmap(data->pixelator);
	al_clear_to_color(al_map_rgb(0 + data->pew * 1.5 + 5 + sin(data->count / 10.0) * 5, 62 + data->pew * 4 + 5 + sin(data->count / 10.0) * 5, 0 + data->pew * 1.5 + 5 + sin(data->count / 10.0) * 5));

	al_identity_transform(&camera);
	al_build_camera_transform(&camera,
	  0, 0, -2, 0, 0, 0, 0, 1, 0);

	al_identity_transform(&transform);
	al_translate_transform(&transform, -data->x, -data->y);
	//al_translate_transform(&transform, 0, 180 / 2);
	al_rotate_transform(&transform, data->angle);
	//al_translate_transform(&transform, 0, -180 / 2);
	al_translate_transform(&transform, 0, -180 / 4);
	al_rotate_transform_3d(&transform, 1, 0, 0, 0.005);
	if (data->tilt) {
		al_translate_transform(&transform, rand() % 3 - 1, rand() % 3 - 1);
	}
	al_compose_transform(&transform, &camera);

	al_identity_transform(&perspective);
	al_perspective_transform(&perspective, -320 / 4, -180 / 4, -1.0, 320 / 4, 180 / 4, 1);

	al_use_transform(&transform);
	al_use_projection_transform(&perspective);
	float x = data->w / 2, y = data->h / 2, z = 0;
	al_draw_bitmap(data->internet, 0, 0, 0);
	for (int i = 0; i < 8192; i++) {
		if (data->entities[i].used) {
			if (data->entities[i].type == TYPE_BULLET) {
				al_draw_filled_rectangle(round(data->entities[i].x) - 2, round(data->entities[i].y) - 2, round(data->entities[i].x) + 2, round(data->entities[i].y) + 2, al_map_rgb(254, 232, 0));
			}
		}
	}

	al_set_target_backbuffer(game->display);
	al_draw_bitmap(data->pixelator, 0, 0, 0);

	ALLEGRO_TRANSFORM projview;
	al_identity_transform(&projview);
	al_compose_transform(&projview, &transform);
	al_compose_transform(&projview, &perspective);
	al_transform_coordinates_3d_projective(&projview, &x, &y, &z);

	//PrintConsole(game, "x %f, y %f, z %f", x, y, z);
	x = x * 320 / 2 + 320 / 2;
	y = y * -180 / 2 + 180 / 2;
	//al_draw_filled_rectangle(x - 2, y - 2, x + 2, y + 2, al_map_rgb(0, 0, 255));

	for (int i = 0; i < 8192; i++) {
		if (data->entities[i].used) {
			x = data->entities[i].x;
			y = data->entities[i].y;
			z = 0;
			al_transform_coordinates_3d_projective(&projview, &x, &y, &z);
			x = x * 320 / 2 + 320 / 2;
			y = y * -180 / 2 + 180 / 2;

			if (data->entities[i].type == TYPE_FAKE) {
				DrawCentered(data->fake->bitmap, x, y, 0);
			}
			if (data->entities[i].type == TYPE_MESSAGE) {
				DrawCentered(data->news->bitmap, x, y, 0);
			}
			if (data->entities[i].type == TYPE_USER) {
				DrawCentered(data->user->bitmap, x, y, 0);
			}
			if (data->entities[i].type == TYPE_ENEMY) {
				DrawCentered(data->bad->bitmap, x, y, 0);
			}
			if (data->entities[i].type == TYPE_EXPLOSION) {
				DrawCentered(data->explosion->bitmap, x, y, 0);
				al_draw_textf(data->font, al_map_rgb(0, 0, 0), x + 1 + 3, y - 5 + 1, ALLEGRO_ALIGN_CENTER, "%d", data->entities[i].score);
				al_draw_textf(data->font, al_map_rgb(255, 255, 255), x + 3, y - 5, ALLEGRO_ALIGN_CENTER, "%d", data->entities[i].score);
			}

			if (((data->entities[i].type == TYPE_ENEMY) || (data->entities[i].type == TYPE_FAKE)) && (z > 0)) {
				//PrintConsole(game, "%f %f %f", x, y, z);
				int w = 8, h = 8;
				bool marker = false;
				if (x < 0) {
					x = 0;
					w = 2;
					marker = true;
				} else if (x > 320) {
					x = 318;
					w = 2;
					marker = true;
				}
				if (y < 0) {
					y = 0;
					h = 2;
					marker = true;
				} else if (y > 180) {
					y = 178;
					h = 2;
					marker = true;
				}

				if (marker) {
					al_draw_filled_rectangle(x, y, x + w, y + h, al_map_rgb(255, 0, 0));
				}
			}
		}
	}

	SetCharacterPosition(game, data->car, 320 / 2 - 23, 3 * 180 / 4, 0);
	DrawCharacter(game, data->car);

	SetCharacterPosition(game, data->police, 320 / 2 - 23 + 13, 3 * 180 / 4 + 4, 0);
	DrawCharacter(game, data->police);

	al_draw_textf(data->font, al_map_rgb(0, 0, 0), 3 + 1, 180 - 11 + 1, ALLEGRO_ALIGN_LEFT, "%d", data->score);
	al_draw_textf(data->font, al_map_rgb(255, 255, 255), 3, 180 - 11, ALLEGRO_ALIGN_LEFT, "%d", data->score);

	SetCharacterPosition(game, data->teeth, 209, 164, 0);
	DrawCharacter(game, data->teeth);

	al_draw_filled_rectangle(228, 167, 316, 176, al_premul_rgba_f(0, 0, 0, 0.8));
	al_draw_filled_rectangle(229, 168, 229 + (315 - 229) * (data->fake_counter / 64.0), 175, al_premul_rgba_f(1, 1, 1, 1));

	al_draw_filled_rectangle(0, 0, 320, 180, al_premul_rgba(0, 0, 0, 255 - data->fade));

	if (game->data->text) {
		al_draw_filled_rectangle(0, 0, 320, 53, al_map_rgba(0, 0, 0, 128));
		al_draw_text(data->font, al_map_rgb(255, 255, 255), 3, 3, ALLEGRO_ALIGN_LEFT, game->data->person);
		DrawWrappedText(data->font, al_map_rgb(255, 255, 255), 3, 3 + 10, 320 - 6, ALLEGRO_ALIGN_LEFT, game->data->text);
	}

	if (data->showlogo) {
		al_draw_bitmap(data->logo, 0, (int)(sin(data->count / 10.0) * 6) + 3, 0);
	}
}

void Gamestate_ProcessEvent(struct Game* game, struct GamestateResources* data, ALLEGRO_EVENT* ev) {
	TM_HandleEvent(data->timeline, ev);
	// Called for each event in Allegro event queue.
	// Here you can handle user input, expiring timers etc.
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE)) {
		UnloadCurrentGamestate(game); // mark this gamestate to be stopped and unloaded
		// When there are no active gamestates, the engine will quit.
	}

	// TODO: add as a helper function to the engine
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_LEFT)) {
		data->left = true;
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_UP) && (ev->keyboard.keycode == ALLEGRO_KEY_LEFT)) {
		data->left = false;
	}

	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_RIGHT)) {
		data->right = true;
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_UP) && (ev->keyboard.keycode == ALLEGRO_KEY_RIGHT)) {
		data->right = false;
	}

	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_UP)) {
		data->up = true;
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_UP) && (ev->keyboard.keycode == ALLEGRO_KEY_UP)) {
		data->up = false;
	}

	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_DOWN)) {
		data->down = true;
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_UP) && (ev->keyboard.keycode == ALLEGRO_KEY_DOWN)) {
		data->down = false;
	}

	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_FULLSTOP)) {
		game->data->skip = true;
	}

	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_SPACE)) {
		SelectSpritesheet(game, data->police, "ban");

		SpawnEntity(game, data, data->x + sin(data->angle + ALLEGRO_PI / 2) * 10, data->y + cos(data->angle + ALLEGRO_PI / 2) * 10, data->angle, TYPE_BULLET);

		int s = rand() % 10;
		al_stop_sample_instance(data->bullets[s].sound);
		al_play_sample_instance(data->bullets[s].sound);

		data->pew = 10;
	}

	if ((ev->type == ALLEGRO_EVENT_KEY_UP) && (ev->keyboard.keycode == ALLEGRO_KEY_SPACE)) {
		SelectSpritesheet(game, data->police, "normal");
	}
}

void* Gamestate_Load(struct Game* game, void (*progress)(struct Game*)) {
	// Called once, when the gamestate library is being loaded.
	// Good place for allocating memory, loading bitmaps etc.
	//
	// NOTE: Depending on engine configuration, this may be called from a separate thread.
	// Unless you're sure what you're doing, avoid using drawing calls and other things that
	// require main OpenGL context.

	struct GamestateResources* data = calloc(1, sizeof(struct GamestateResources));
	progress(game); // report that we progressed with the loading, so the engine can move a progress bar

	data->w = 8192;
	data->h = 8192;
	data->internet = al_create_bitmap(data->w, data->h);

	data->bg = al_load_bitmap(GetDataFilePath(game, "bg.png"));

	data->timeline = TM_Init(game, "timeline");

	int flags = al_get_new_bitmap_flags();
	al_set_new_bitmap_flags(flags ^ ALLEGRO_MAG_LINEAR);
	data->pixelator = CreateNotPreservedBitmap(320, 180);
	data->logo = al_load_bitmap(GetDataFilePath(game, "logo.png"));
	data->endscreen1 = al_load_bitmap(GetDataFilePath(game, "outro1.png"));
	data->endscreen2 = al_load_bitmap(GetDataFilePath(game, "outro2.png"));
	data->endscreen3 = al_load_bitmap(GetDataFilePath(game, "outro3.png"));

	data->car = CreateCharacter(game, "car");
	RegisterSpritesheet(game, data->car, "car");
	LoadSpritesheets(game, data->car);

	data->police = CreateCharacter(game, "police");
	RegisterSpritesheet(game, data->police, "normal");
	RegisterSpritesheet(game, data->police, "ban");
	LoadSpritesheets(game, data->police);

	data->teeth = CreateCharacter(game, "teeth");
	RegisterSpritesheet(game, data->teeth, "teeth");
	LoadSpritesheets(game, data->teeth);

	data->user = CreateCharacter(game, "user");
	RegisterSpritesheet(game, data->user, "user");
	LoadSpritesheets(game, data->user);

	data->fake = CreateCharacter(game, "fake");
	RegisterSpritesheet(game, data->fake, "fake");
	LoadSpritesheets(game, data->fake);

	data->news = CreateCharacter(game, "news");
	RegisterSpritesheet(game, data->news, "news");
	LoadSpritesheets(game, data->news);

	data->bad = CreateCharacter(game, "bad");
	RegisterSpritesheet(game, data->bad, "bad");
	LoadSpritesheets(game, data->bad);

	data->explosion = CreateCharacter(game, "explosion");
	RegisterSpritesheet(game, data->explosion, "explosion");
	LoadSpritesheets(game, data->explosion);

	data->font = al_load_font(GetDataFilePath(game, "fonts/MonkeyIsland.ttf"), 8, ALLEGRO_TTF_MONOCHROME);
	data->bff = al_load_font(GetDataFilePath(game, "fonts/MonkeyIsland.ttf"), 32, ALLEGRO_TTF_MONOCHROME);

	al_set_new_bitmap_flags(flags);

	for (int i = 0; i < 10; i++) {
		char* filename = malloc(255 * sizeof(char));
		snprintf(filename, 255, "bullet/%d.flac", i + 1);

		data->bullets[i].sample = al_load_sample(GetDataFilePath(game, filename));
		data->bullets[i].sound = al_create_sample_instance(data->bullets[i].sample);
		al_attach_sample_instance_to_mixer(data->bullets[i].sound, game->audio.fx);
		al_set_sample_instance_playmode(data->bullets[i].sound, ALLEGRO_PLAYMODE_ONCE);
		free(filename);
	}

	for (int i = 0; i < 8; i++) {
		char* filename = malloc(255 * sizeof(char));
		snprintf(filename, 255, "explosions/%d.flac", i + 1);

		data->explosions[i].sample = al_load_sample(GetDataFilePath(game, filename));
		data->explosions[i].sound = al_create_sample_instance(data->explosions[i].sample);
		al_attach_sample_instance_to_mixer(data->explosions[i].sound, game->audio.fx);
		al_set_sample_instance_playmode(data->explosions[i].sound, ALLEGRO_PLAYMODE_ONCE);
		free(filename);
	}

	return data;
}

void Gamestate_Unload(struct Game* game, struct GamestateResources* data) {
	// Called when the gamestate library is being unloaded.
	// Good place for freeing all allocated memory and resources.

	al_destroy_bitmap(data->internet);
	al_destroy_bitmap(data->pixelator);
	al_destroy_bitmap(data->bg);
	DestroyCharacter(game, data->police);
	DestroyCharacter(game, data->car);
	DestroyCharacter(game, data->teeth);
	DestroyCharacter(game, data->user);
	DestroyCharacter(game, data->fake);
	DestroyCharacter(game, data->news);
	DestroyCharacter(game, data->bad);
	DestroyCharacter(game, data->explosion);
	TM_Destroy(data->timeline);
	al_destroy_font(data->font);
	al_destroy_font(data->bff);

	free(data);
}

void Gamestate_Start(struct Game* game, struct GamestateResources* data) {
	// Called when this gamestate gets control. Good place for initializing state,
	// playing music etc.

	data->fake_counter = 2;

	SelectSpritesheet(game, data->car, "car");
	SelectSpritesheet(game, data->police, "normal");
	SelectSpritesheet(game, data->teeth, "teeth");
	SelectSpritesheet(game, data->user, "user");
	SelectSpritesheet(game, data->fake, "fake");
	SelectSpritesheet(game, data->news, "news");
	SelectSpritesheet(game, data->bad, "bad");
	SelectSpritesheet(game, data->explosion, "explosion");

	data->angle = ALLEGRO_PI;

	al_set_target_bitmap(data->internet);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));

	ALLEGRO_VERTEX vertices[6] = {
	  // bottom left triangle
	  {.x = 0, .y = 0, .u = 0, .v = 0, .color = al_map_rgb(255, 255, 255)}, // top left
	  {.x = 0, .y = data->h, .u = 0, .v = data->h, .color = al_map_rgb(255, 255, 255)}, // bottom left
	  {.x = data->w, .y = data->h, .u = data->w, .v = data->h, .color = al_map_rgb(255, 255, 255)}, // bottom right
	  // up right triangle
	  {.x = 0, .y = 0, .u = 0, .v = 0, .color = al_map_rgb(255, 255, 255)}, // top left
	  {.x = data->w, .y = 0, .u = data->w, .v = 0, .color = al_map_rgb(255, 255, 255)}, // top right
	  {.x = data->w, .y = data->h, .u = data->w, .v = data->h, .color = al_map_rgb(255, 255, 255)}, // bottom right
	};

	al_draw_prim(vertices, NULL, data->bg, 0, 6, ALLEGRO_PRIM_TRIANGLE_LIST);

	//al_draw_filled_rectangle(data->w / 2 - 5, data->h / 2 - 5, data->w / 2 + 5, data->h / 2 + 5, al_map_rgb(255, 0, 0));

	al_set_target_backbuffer(game->display);

	data->x = data->w / 2;
	data->y = 3 * data->h / 4;

	TM_AddDelay(data->timeline, 1500);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/zenek.flac"), 4, 1024), "To co dzisiaj robimy, Gienek?", "ZENEK"), "speak");
	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/gienek.flac"), 4, 1024), "No jak to co Zenek, przejmujemy wladze nad swiatem!", "GIENEK"), "speak");
	TM_AddAction(data->timeline, &ShowLogo, TM_AddToArgs(NULL, 1, data), "logo");
	TM_AddDelay(data->timeline, 5000);
	TM_AddAction(data->timeline, &HideLogo, TM_AddToArgs(NULL, 1, data), "logo");
	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/intro1.flac"), 4, 1024), "*dryn dryn*", "TELEFON"), "speak");
	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/intro2.flac"), 4, 1024), "Halo, policja? Prosze przyjechac na Fejsbuga™!", "GLOS Z TELEFONU"), "speak");
	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/intro3.flac"), 4, 1024), "Sie robi.", "KOMISARZ ZIEBA"), "speak");
	TM_AddAction(data->timeline, &StartGame, TM_AddToArgs(NULL, 1, data), "start");

	TM_AddDelay(data->timeline, 2000);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/1.flac"), 4, 1024), "Nazywam sie Zieba. Komisarz Zieba.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/2.flac"), 4, 1024), "Dbam o porzadek na Fejsbugu™, by nikt nie przeszkadzal uzytkownikom wiesc ich spokojnego uzytkowniczego zycia.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/3.flac"), 4, 1024), "Poruszam sie moim cybernetycznym poduszkowcem po cyberprzestrzeni za pomoca KLAWISZY STRZALEK", "KOMISARZ ZIEBA"), "speak");
	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/4.flac"), 4, 1024), "a wymierzam sprawiedliwosc moim wiernym Banhammerem za pomoca SPACJI.", "KOMISARZ ZIEBA"), "speak");

	TM_AddDelay(data->timeline, 1000);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/5.flac"), 4, 1024), "Swietnie.", "KOMISARZ ZIEBA"), "speak");

	TM_AddDelay(data->timeline, 1000);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/6.flac"), 4, 1024), "Znowu ktos wypuszcza Fake Newsy.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/7.flac"), 4, 1024), "Musze je unicestwic zanim uzytkownicy znajda sie pod ich wplywem.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &SpawnSingleFake, TM_AddToArgs(NULL, 1, data), "spawnsinglenews");

	TM_AddDelay(data->timeline, 4000);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/8.flac"), 4, 1024), "I po sprawie.", "KOMISARZ ZIEBA"), "speak");

	TM_AddDelay(data->timeline, 2000);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/9.flac"), 4, 1024), "Oto i jest. Manipulator.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/10.flac"), 4, 1024), "Nastawia ludzi przeciwko sobie, aby byli podatni na manipulacje, zeby ich zmanipulowac.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &SpawnSingleEnemy, TM_AddToArgs(NULL, 1, data), "spawnsinglenews");

	TM_AddDelay(data->timeline, 4000);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/12.flac"), 4, 1024), "Jest ich wiecej.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &SpawnEnemies, TM_AddToArgs(NULL, 1, data), "spawn");

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/ostroznie2.flac"), 4, 1024), "Nie moge banowac zwyklych uzytkownikow i ich zwyklych tresci, bo zaczna sie buntowac.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/13.flac"), 4, 1024), "Gdy uzytkownicy przekrocza Pulap Spolecznego Zgrzytania Zebami Przeciwko Sobie, zamkna sie w swoich bankach informacyjnych i beda pod pelna kontrola zloczynców.", "KOMISARZ ZIEBA"), "speak");

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/14.flac"), 4, 1024), "Nie moge do tego dopuscic.", "KOMISARZ ZIEBA"), "speak");

	TM_AddDelay(data->timeline, 20000);

	TM_AddAction(data->timeline, &Speak, TM_AddToArgs(NULL, 4, data, al_load_audio_stream(GetDataFilePath(game, "voices/16.flac"), 4, 1024), "Zle sily rosna w sile. Usiluja ze mna wygrac, ale jestem silniejszy.", "KOMISARZ ZIEBA"), "speak");
}

void Gamestate_Stop(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets stopped. Stop timers, music etc. here.
}

void Gamestate_Pause(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets paused (so only Draw is being called, no Logic nor ProcessEvent)
	// Pause your timers and/or sounds here.
}

void Gamestate_Resume(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets resumed. Resume your timers and/or sounds here.
}

void Gamestate_Reload(struct Game* game, struct GamestateResources* data) {
	// Called when the display gets lost and not preserved bitmaps need to be recreated.
	// Unless you want to support mobile platforms, you should be able to ignore it.
}
