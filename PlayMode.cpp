#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint plant_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > plant_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("plant.pnct"));
	plant_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > plant_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("final.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = plant_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = plant_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > main_menu(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("main_menu.wav"));
});

Load< Sound::Sample > scary(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("spooky.wav"));
});

PlayMode::PlayMode() : scene(*plant_scene) {
	//get pointers to plant for convenience:
	for (auto &transform : scene.transforms) {
		std::cout << transform.name << '\n';
		if (transform.name == "plant") plant = &transform;
	}
	if (plant == nullptr) throw std::runtime_error("plant not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	music_loop = Sound::loop_3D(*main_menu, 1.0f, plant->position, 10.0f);

	camera->transform->rotation = glm::normalize(camera->transform->rotation * glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
	camera->transform->position = glm::vec3(47.0f, 4.3f, 15.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_LSHIFT) {
			if (plantSpawned)
			{
				plantSpawned = false;
				plant->position = glm::vec3(100.0f, 100.0f, 100.0f);
				score = std::max(std::min(scoreTimer, score), 0.0f);
				scoreTimer = 0.0f;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			if (!gameStarted) {
				gameStarted = true;
				(*music_loop).stop();
				camera->transform->position += glm::vec3(0.0f, 0.0f, 13.0f);
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_RETURN) {
			std::cout << camera->transform->position.x << '\n';
			std::cout << camera->transform->position.y << '\n';
			std::cout << camera->transform->position.z << '\n';
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			// camera->transform->rotation = glm::normalize(
			// 	camera->transform->rotation
			// 	* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
			// 	* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			// );
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//move sound to follow leg tip position:
	music_loop->set_position(plant->position, 1.0f / 60.0f);

	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		if (!gameStarted) move = glm::vec2(0.0f);

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	if (gameStarted)
	{
		plantTimer -= elapsed;
		if (plantSpawned) scoreTimer += elapsed;
		if (plantTimer <= 0.0f && !plantTimerElapsed)
		{
			plantTimerElapsed = true;
			plantTimer = 1.0f;
		}
		else if (plantTimer <= 0.0f && plantTimerElapsed)
		{
			plant->position = camera->transform->position - glm::vec3(40.0f, 0.0f, 0.0f);
			Sound::play_3D(*scary, 1.0f, plant->position, 5.0f);
			plantTimerElapsed = false;
			plantSpawned = true;
			plantTimer = (std::rand() % 10 / 2) + 3.0f;
		}
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		float ofs = 2.0f / drawable_size.y;
		if (!gameStarted)
		{
			lines.draw_text("Press Space to Start Game",
				glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Press Space to Start Game",
		 		glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
		 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		 		glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else
		{
			lines.draw_text("WASD moves; escape ungrabs mouse",
				glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("WASD moves; escape ungrabs mouse",
		 		glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
		 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		 		glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}

		if (score != 0.0f)
		{
			lines.draw_text(std::to_string(score),
		 		glm::vec3(-aspect + 0.1f * H, -1.0 + + 0.1f * H, 0.0),
		 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
		 		glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
	GL_ERRORS();
}
