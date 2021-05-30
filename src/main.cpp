#include "vcl/vcl.hpp"
#include <iostream>

#include "helpers/scene_helper.hpp"
#include "items/terrain.hpp"
#include "items/pyramid.hpp"
#include "items/vegetation.hpp"
#include "items/columns.hpp"
#include "items/bird.hpp"
#include "items/boat.hpp"
//#include "items/corde.hpp"
#include "helpers/environment_map.hpp"


using namespace vcl;


scene_environment scene;
user_interaction_parameters user;


void mouse_move_callback(GLFWwindow* window, double xpos, double ypos);
void window_size_callback(GLFWwindow* window, int width, int height);


void initialize_data();
void display_interface();
void display_frame();

timer_interval timer;

mesh terrain;
mesh_drawable terrain_visual;
mesh_drawable terrain_water;
mesh_drawable terrain_berge_haut;
mesh_drawable terrain_berge_milieu;
mesh_drawable terrain_berge_bas;
mesh_drawable terrain_herbe;
mesh_drawable terrain_dune;
perlin_noise_parameters parameters;
mesh_drawable sphere_current;    // sphere used to display the interpolated value
mesh_drawable sphere_keyframe;   // sphere used to display the key positions of berges
buffer<vec3> *courbes_fleuve;
float t = 0;

mesh_drawable cube_map;

mesh_drawable pyramid;
mesh_drawable palm_tree;
mesh_drawable column;
hierarchy_mesh_drawable bird;
mesh_drawable boat;
mesh_drawable fern;

vcl::buffer<vcl::vec3> particules;
vcl::buffer<vcl::vec3> vitesses;
vcl::buffer<float> L0_array;
vcl::buffer<float> raideurs;
vec3 pos_poteau;
mesh boat_mesh;
mesh_drawable boat_attached;
mesh_drawable sphere;

vcl::buffer<vec3> key_positions_bird;
vcl::buffer<float> key_times_bird;
vcl::buffer<vec3> follower_birds;
vcl::buffer<vec3> speeds_birds;
const int nb_follower_birds = 10;




int main(int, char* argv[])
{
	std::cout << "Run " << argv[0] << std::endl;

    int const width = 1280, height = 1024;
	GLFWwindow* window = create_window(width, height);
	window_size_callback(window, width, height);
	std::cout << opengl_info_display() << std::endl;;

	imgui_init(window);
	glfwSetCursorPosCallback(window, mouse_move_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);

	std::cout << "Initialize data ..." << std::endl;
	initialize_data();

	std::cout << "Start animation loop ..." << std::endl;
	user.fps_record.start();
	glEnable(GL_DEPTH_TEST);
	while (!glfwWindowShouldClose(window))
	{
		scene.light = scene.camera.position();
		user.fps_record.update();

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glClear(GL_DEPTH_BUFFER_BIT);
		imgui_create_frame();
		if (user.fps_record.event) {
			std::string const title = "VCL Display - " + str(user.fps_record.fps) + " fps";
			glfwSetWindowTitle(window, title.c_str());
		}

		ImGui::Begin("GUI", NULL, ImGuiWindowFlags_AlwaysAutoResize);
		user.cursor_on_gui = ImGui::IsAnyWindowFocused();

        //if (user.gui.display_frame) draw(user.global_frame, scene);

		display_interface();
		display_frame();


		ImGui::End();
		imgui_render_frame(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	imgui_cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}



void initialize_data()
{
	// Basic setups of shaders and camera
	GLuint const shader_mesh = opengl_create_shader_program(opengl_shader_preset("mesh_vertex"), opengl_shader_preset("mesh_fragment"));
	mesh_drawable::default_shader = shader_mesh;
	mesh_drawable::default_texture = opengl_texture_to_gpu(image_raw{ 1,1,image_color_type::rgba,{255,255,255,255} });

	user.global_frame = mesh_drawable(mesh_primitive_frame());
	user.gui.display_frame = false;
	scene.camera.distance_to_center = 2.5f;
	scene.camera.look_at({ -0.5f,2.5f,1 }, { 0,0,0 }, { 0,0,1 });

    // Create skybox
    // Read shaders
    GLuint const shader_skybox = opengl_create_shader_program(read_text_file("shader/skybox.vert.glsl"), read_text_file("shader/skybox.frag.glsl"));
    GLuint const shader_environment_map = opengl_create_shader_program(read_text_file("shader/environment_map.vert.glsl"), read_text_file("shader/environment_map.frag.glsl"));
    
    // Read cubemap texture
    GLuint texture_cubemap = cubemap_texture("pictures/skybox_def/");

    // Cube used to display the skybox
    mesh cube = mesh_primitive_cube({0,0,0},2.0f);
    cube_map = mesh_drawable( cube, shader_skybox, texture_cubemap);
    //rotate_terrain(cube, cube_map);

    // Create the terrain
    terrain = create_terrain();
    /*terrain_visual = mesh_drawable(terrain);
    update_terrain(terrain, terrain_visual, parameters);*/
    terrain_herbe = mesh_drawable(terrain);
    terrain_water = mesh_drawable(terrain, shader_environment_map, texture_cubemap);
    terrain_berge_bas = mesh_drawable(terrain);
    terrain_berge_milieu = mesh_drawable(terrain);
    terrain_berge_haut = mesh_drawable(terrain);
    terrain_dune = mesh_drawable(terrain);
    update_terrain(terrain,terrain_herbe,terrain_berge_bas, terrain_berge_milieu, terrain_berge_haut,terrain_dune,terrain_water, parameters, t);

    // Texture Images load and association
    terrain_dune.texture = texture("pictures/texture_sable.png");
    terrain_herbe.texture = texture("pictures/texture_grass.png");

	// Pyramid
	initialize_pyramid(pyramid, 0.01f);

	// Palm tree
	initialize_palm_tree(palm_tree, 1.0f);

    // column
    initialize_column_cyl(column, 1.0f);

	// Birds
	initialize_leader_bird(bird, 1.0f, key_positions_bird, key_times_bird);
	for (int i = 0; i < nb_follower_birds; i++) {
		follower_birds.push_back(key_positions_bird[0] + 1.0f*vec3(static_cast <float> (rand()) / (static_cast <float> (RAND_MAX)), static_cast <float> (rand()) / (static_cast <float> (RAND_MAX)), static_cast <float> (rand()) / (static_cast <float> (RAND_MAX))));
		speeds_birds.push_back({ 0.1f, 0.1f, 0.1f });
	}
	speeds_birds.push_back({ 0.1f, 0.1f, 0.1f });

    // Boat
    initialize_boat(boat, 1.0f);

    // Fern
    initialize_fern(fern, 1.0f);

    boat_mesh = create_boat(1.0f * 7.0f, 1.0f * 2.0f, 1.0f * 1.0f);
    initialize_boat(boat_mesh, boat_attached);
    pos_poteau = {4.0f,-6.0f,evaluate_terrain2(-1.0/16+0.5f,-13.0/30+0.5, terrain)[2]+0.1f};
    //initialize_corde(get_boat_pos(boat_mesh),pos_poteau, particules, vitesses, L0_array, raideurs);
    //sphere = mesh_drawable( mesh_primitive_sphere(0.05f));

	// Set timer bounds
	//  You should adapt these extremal values to the type of interpolation
	size_t const N = key_times_bird.size();
	timer.t_min = key_times_bird[1];    // Start the timer at the first time of the keyframe
	timer.t_max = key_times_bird[N - 2];  // Ends the timer at the last time of the keyframe
	timer.t = timer.t_min;
}



void display_frame()
{
	// Update the current time
	float t_prev = t;
    timer.update();
    timer.scale = 0.02f; // 0.02f
    t = timer.t;
    float dt = timer.scale;

    ImGui::Checkbox("Frame", &user.gui.display_frame);
    ImGui::Checkbox("Wireframe", &user.gui.display_wireframe);

    bool update = false;
    update |= ImGui::SliderFloat("Persistance", &parameters.persistency, 0.1f, 0.6f);
    update |= ImGui::SliderFloat("Frequency gain", &parameters.frequency_gain, 1.5f, 2.5f);
    update |= ImGui::SliderInt("Octave", &parameters.octave, 1, 8);
    update |= ImGui::SliderFloat("Height", &parameters.terrain_height, 0.1f, 1.5f);

    if(update){// if any slider has been changed - then update the terrain
        update_terrain_herbe(terrain, terrain_herbe, parameters);
        update_terrain_berge_bas(terrain, terrain_berge_bas, parameters);
        update_terrain_berge_haut(terrain, terrain_berge_haut, parameters);
        update_terrain_dune(terrain, terrain_dune, parameters);
        update_terrain_water(terrain, terrain_water, parameters, t);
        //update_terrain(terrain, terrain_visual, parameters);
    }

    update_terrain_water(terrain, terrain_water, parameters, t);
    //update_pos_boat(boat_attached, t);
    //update_pos_rope(particules,vitesses,L0_array,raideurs,terrain,dt);

    glDepthMask(GL_FALSE);
    draw_with_cubemap(cube_map, scene);
    glDepthMask(GL_TRUE);

    //draw(terrain_visual, scene);
    //draw(terrain_berge_bas, scene);
    //draw(terrain_berge_haut, scene);
    //draw(terrain_herbe, scene);
    vcl::draw(terrain_dune, scene);
    draw_with_cubemap(terrain_water, scene);
	vcl::draw(pyramid, scene);
	vcl::draw(palm_tree, scene);
	vcl::draw(column, scene);

	update_leader_bird(bird, t, dt, key_positions_bird, key_times_bird, speeds_birds);
	vcl::draw(bird, scene);
	update_follower_birds(bird, follower_birds, speeds_birds, t, dt, 0.0001f, 0.0001f, 0.005f);
	for (auto pos : follower_birds) {
		bird["body"].transform.translate = pos;
		bird.update_local_to_global_coordinates();
		vcl::draw(bird, scene);
		std::cout << pos[0] << " " << pos[1] << " " << pos[2] << std::endl;
	}


	vcl::draw(boat, scene);
	vcl::draw(boat_attached, scene);

	vcl::draw(fern, scene);
}



void display_interface()
{
	ImGui::SliderFloat("Time", &timer.t, timer.t_min, timer.t_max);
	ImGui::SliderFloat("Time scale", &timer.scale, 0.0f, 2.0f);
	ImGui::Checkbox("Frame", &user.gui.display_frame);
	ImGui::Checkbox("Surface", &user.gui.display_surface);
	ImGui::Checkbox("Wireframe", &user.gui.display_wireframe);
}


void window_size_callback(GLFWwindow*, int width, int height)
{
	glViewport(0, 0, width, height);
	float const aspect = width / static_cast<float>(height);
    float const fov = 50.0f * pi / 180.0f;
	float const z_min = 0.1f;
	float const z_max = 100.0f;
	scene.projection = projection_perspective(fov, aspect, z_min, z_max);
}

void mouse_move_callback(GLFWwindow* window, double xpos, double ypos)
{
	vec2 const  p1 = glfw_get_mouse_cursor(window, xpos, ypos);
	vec2 const& p0 = user.mouse_prev;
	glfw_state state = glfw_current_state(window);

	auto& camera = scene.camera;
	if (!user.cursor_on_gui) {
		if (state.mouse_click_left && !state.key_ctrl)
			scene.camera.manipulator_rotate_trackball(p0, p1);
		if (state.mouse_click_left && state.key_ctrl)
			camera.manipulator_translate_in_plane(p1 - p0);
		if (state.mouse_click_right)
			camera.manipulator_scale_distance_to_center((p1 - p0).y);
	}

	user.mouse_prev = p1;
}

