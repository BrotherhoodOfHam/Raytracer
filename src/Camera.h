/*
	Camera
*/

#pragma once

#include <chrono>
#include <iostream>
#include <algorithm>

#include "Common.h"
#include <SDL_events.h>

class Camera
{
private:

	enum Action : uint16_t
	{
		NONE  = 0,
		FWD   = 1 << 0,
		BACK  = 1 << 1,
		LEFT  = 1 << 2,
		RIGHT = 1 << 3,
		DOWN  = 1 << 4,
		UP    = 1 << 5,

		RUP    = 1 << 6,
		RDOWN  = 1 << 7,
		RLEFT  = 1 << 8,
		RRIGHT = 1 << 9
	};

	float angleH = 0; //angle around horizontal x axis
	float angleV = 0; //angle around vertical Y axis
	glm::vec3 _position;
	uint16_t  _acts;

	std::chrono::high_resolution_clock::time_point _lastUpdate;

public:

	Camera() :
		_position(0,-1,0),
		_acts(NONE),
		_lastUpdate(std::chrono::high_resolution_clock::now())
	{}

	glm::mat4 matrix() const
	{
		const glm::mat4 i = glm::identity<glm::mat4>();
		return glm::translate(i, _position) *
			glm::rotate(i, glm::radians(angleV), glm::vec3(0.0f, -1, 0)) *
			glm::rotate(i, glm::radians(angleH), glm::vec3(1, 0, 0));
	}

	void key(const SDL_KeyboardEvent& event)
	{
		if (event.type == SDL_KEYDOWN)
		{
			_acts |= fromKey(event.keysym.sym);
		}
		else if (event.type == SDL_KEYUP)
		{
			Action a = fromKey(event.keysym.sym);
			if (a != NONE)
				_acts &= ~a;
		}
	}

	void update()
	{
		using namespace std::chrono;

		auto now = high_resolution_clock::now();
		double dt = ((double)duration_cast<milliseconds>(now - _lastUpdate).count()) / 1000.0;
		_lastUpdate = now;

		const float speed = 4.0f;
		const float angularSpeed = 40.0f; //degrees per second
		glm::vec3 dir(0,0,0);

		/*
			+y down
			+z forward
			+x right
		*/
		if (_acts & RIGHT) dir.x += +1;
		if (_acts & LEFT)  dir.x += -1;
		if (_acts & DOWN)  dir.y += +1;
		if (_acts & UP)    dir.y += -1;
		if (_acts & FWD)   dir.z += +1;
		if (_acts & BACK)  dir.z += -1;

		const float deg = angularSpeed * dt;

		if (_acts & RUP)     angleH += deg;
		if (_acts & RDOWN)   angleH -= deg;
		if (_acts & RLEFT)   angleV += deg;
		if (_acts & RRIGHT)  angleV -= deg;

		angleH = (angleH < 90.0f) ? ((angleH > -90.0f) ? angleH : -90.0f) : 90.0f;
		angleV = fmodf(angleV, 360.0f);

		const auto rot = glm::rotate(glm::mat4(1.0f), glm::radians(angleV), glm::vec3(0.0f, -1, 0));
		dir = glm::mat3(rot) * dir;
		dir = (dir.x || dir.y || dir.z) ? glm::normalize(dir) : glm::vec3(0, 0, 0);

		_position += ((float)(speed * dt) * dir);
		//log("angleH:", angleH, "angleV:", angleV, "\n");
		//log("camera: (", _position.x, ", ", _position.y, ", ", _position.z, ")");
	}

private:

	Action fromKey(SDL_Keycode code)
	{
		switch (code)
		{
			case SDLK_w:     return FWD;
			case SDLK_a:     return LEFT;
			case SDLK_s:     return BACK;
			case SDLK_d:     return RIGHT;
			case SDLK_LCTRL: return DOWN;
			case SDLK_SPACE: return UP;
			case SDLK_UP:    return RUP;
			case SDLK_DOWN:  return RDOWN;
			case SDLK_LEFT:  return RLEFT;
			case SDLK_RIGHT: return RRIGHT;
		}
		return NONE;
	}
};
