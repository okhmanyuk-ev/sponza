#include <iostream>
#include <unordered_map>
#include <tiny_gltf.h>
#include <imgui.h>
#include <skygfx/utils.h>
#include "../sky/lib/skygfx/examples/utils/imgui_helper.h"
#include <sky/sky.h>

static double cursor_saved_pos_x = 0.0;
static double cursor_saved_pos_y = 0.0;
static bool cursor_is_interacting = false;

bool IsImguiInteracting()
{
	return !(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByPopup)
		&& !ImGui::IsAnyItemActive());
}

void MouseButtonCallback(const Platform::Input::Mouse::ButtonEvent& e)
{
	if (e.button == Platform::Input::Mouse::Button::Left)
	{
		if (e.type == Platform::Input::Mouse::ButtonEvent::Type::Pressed && !cursor_is_interacting)
		{
			if (IsImguiInteracting())
				return;

			cursor_is_interacting = true;
			auto pos = PLATFORM->getCursorPos().value();
			cursor_saved_pos_x = (double)pos.x;
			cursor_saved_pos_y = (double)pos.y;
			PLATFORM->setCursorMode(Platform::Input::CursorMode::Hidden);
		}
		else if (e.type == Platform::Input::Mouse::ButtonEvent::Type::Released && cursor_is_interacting)
		{
			cursor_is_interacting = false;
			PLATFORM->setCursorPos((int)cursor_saved_pos_x, (int)cursor_saved_pos_y);
			PLATFORM->setCursorMode(Platform::Input::CursorMode::Normal);
		}
	}
}

static auto gTechnique = skygfx::utils::DrawSceneOptions::Technique::DeferredShading;
static auto gNormalMapping = true;

void KeyCallback(const Platform::Input::Keyboard::Event& e)
{
	if (e.type != Platform::Input::Keyboard::Event::Type::Pressed && e.type != Platform::Input::Keyboard::Event::Type::Repeat)
		return;

	if (e.key == Platform::Input::Keyboard::Key::R)
	{
		gTechnique = gTechnique == skygfx::utils::DrawSceneOptions::Technique::ForwardShading ?
			skygfx::utils::DrawSceneOptions::Technique::DeferredShading :
			skygfx::utils::DrawSceneOptions::Technique::ForwardShading;
	}
	else if (e.key == Platform::Input::Keyboard::Key::T)
	{
		gNormalMapping = !gNormalMapping;
	}
}

struct Material
{
	std::shared_ptr<skygfx::Texture> color_texture;
	std::shared_ptr<skygfx::Texture> normal_texture;
	std::shared_ptr<skygfx::Texture> metallic_roughness_texture;
	glm::vec4 color;
};

struct RenderBuffer
{
	struct DrawData
	{
		skygfx::utils::Mesh::Vertices vertices; // for normals debug
		skygfx::utils::Mesh::Indices indices; // for normals debug
		skygfx::Topology topology;
		skygfx::utils::Mesh mesh;
		skygfx::utils::commands::DrawMesh::DrawCommand draw_command;
	};

	std::unordered_map<std::shared_ptr<Material>, std::vector<DrawData>> meshes;
};

RenderBuffer BuildRenderBuffer(const tinygltf::Model& model)
{
	// https://github.com/syoyo/tinygltf/blob/master/examples/glview/glview.cc
	// https://github.com/syoyo/tinygltf/blob/master/examples/basic/main.cpp

	RenderBuffer result;

	const auto& scene = model.scenes.at(0);

	std::unordered_map<int, std::shared_ptr<skygfx::Texture>> textures_cache;

	auto get_or_create_texture = [&](int index) -> std::shared_ptr<skygfx::Texture> {
		if (index == -1)
			return nullptr;

		if (!textures_cache.contains(index))
		{
			const auto& texture = model.textures.at(index);
			const auto& image = model.images.at(texture.source);
			textures_cache[index] = std::make_shared<skygfx::Texture>((uint32_t)image.width,
				(uint32_t)image.height, skygfx::PixelFormat::RGBA8UNorm, (void*)image.image.data(), true);
		}

		return textures_cache.at(index);
	};

	for (auto node_index : scene.nodes)
	{
		const auto& node = model.nodes.at(node_index);

		auto mesh_index = node.mesh;

		const auto& mesh = model.meshes.at(mesh_index);

		for (const auto& primitive : mesh.primitives)
		{
			static const std::unordered_map<int, skygfx::Topology> ModesMap = {
				{ TINYGLTF_MODE_POINTS, skygfx::Topology::PointList },
				{ TINYGLTF_MODE_LINE, skygfx::Topology::LineList },
			//	{ TINYGLTF_MODE_LINE_LOOP, skygfx::Topology:: },
				{ TINYGLTF_MODE_LINE_STRIP, skygfx::Topology::LineStrip },
				{ TINYGLTF_MODE_TRIANGLES, skygfx::Topology::TriangleList },
				{ TINYGLTF_MODE_TRIANGLE_STRIP, skygfx::Topology::TriangleStrip },
			//	{ TINYGLTF_MODE_TRIANGLE_FAN, skygfx::Topology:: } 
			};

			auto topology = ModesMap.at(primitive.mode);

			const static std::unordered_map<int, int> IndexStride = {
				{ TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, 2 },
				{ TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, 4 },
			};

			/* buffer_view.target is:
				TINYGLTF_TARGET_ARRAY_BUFFER,
				TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER
			*/

			const auto& index_buffer_accessor = model.accessors.at(primitive.indices);
			const auto& index_buffer_view = model.bufferViews.at(index_buffer_accessor.bufferView);
			const auto& index_buffer = model.buffers.at(index_buffer_view.buffer);

			auto index_buf_size = index_buffer_view.byteLength;
			auto index_buf_stride = IndexStride.at(index_buffer_accessor.componentType);
			auto index_buf_data = (void*)((size_t)index_buffer.data.data() + index_buffer_view.byteOffset);

			auto index_count = index_buffer_accessor.count;
			auto index_offset = index_buffer_accessor.byteOffset / 2;

			const auto& positions_buffer_accessor = model.accessors.at(primitive.attributes.at("POSITION"));
			const auto& positions_buffer_view = model.bufferViews.at(positions_buffer_accessor.bufferView);
			const auto& positions_buffer = model.buffers.at(positions_buffer_view.buffer);

			const auto& normal_buffer_accessor = model.accessors.at(primitive.attributes.at("NORMAL"));
			const auto& normal_buffer_view = model.bufferViews.at(normal_buffer_accessor.bufferView);
			const auto& normal_buffer = model.buffers.at(normal_buffer_view.buffer);

			const auto& texcoord_buffer_accessor = model.accessors.at(primitive.attributes.at("TEXCOORD_0"));
			const auto& texcoord_buffer_view = model.bufferViews.at(texcoord_buffer_accessor.bufferView);
			const auto& texcoord_buffer = model.buffers.at(texcoord_buffer_view.buffer);

			if (!primitive.attributes.contains("TANGENT"))
				continue;

			const auto& tangents_buffer_accessor = model.accessors.at(primitive.attributes.at("TANGENT"));
			const auto& tangents_buffer_view = model.bufferViews.at(tangents_buffer_accessor.bufferView);
			const auto& tangents_buffer = model.buffers.at(tangents_buffer_view.buffer);

			//const auto& bitangents_buffer_accessor = model.accessors.at(primitive.attributes.at("BITANGENT"));
			//const auto& bitangents_buffer_view = model.bufferViews.at(bitangents_buffer_accessor.bufferView);
			//const auto& bitangents_buffer = model.buffers.at(bitangents_buffer_view.buffer);

			auto positions_ptr = (glm::vec3*)(((size_t)positions_buffer.data.data()) + positions_buffer_view.byteOffset);
			auto texcoord_ptr = (glm::vec2*)(((size_t)texcoord_buffer.data.data()) + texcoord_buffer_view.byteOffset);
			auto normal_ptr = (glm::vec3*)(((size_t)normal_buffer.data.data()) + normal_buffer_view.byteOffset);
			auto tangents_ptr = (glm::vec3*)(((size_t)tangents_buffer.data.data()) + tangents_buffer_view.byteOffset);
			//auto bitangents_ptr = (glm::vec3*)(((size_t)bitangents_buffer.data.data()) + bitangents_buffer_view.byteOffset);

			auto indices = skygfx::utils::Mesh::Indices();

			for (int i = 0; i < index_buffer_accessor.count; i++)
			{
				uint32_t index;

				if (index_buf_stride == 2)
					index = static_cast<uint32_t>(((uint16_t*)index_buf_data)[i]);
				else
					index = ((uint32_t*)index_buf_data)[i];

				indices.push_back(index);
			}

			skygfx::utils::Mesh::Vertices vertices;

			for (int i = 0; i < positions_buffer_accessor.count; i++)
			{
				skygfx::utils::Mesh::Vertex vertex;

				vertex.pos = positions_ptr[i];
				vertex.normal = normal_ptr[i];
				vertex.texcoord = texcoord_ptr[i];
				vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // TODO: colors_ptr[i]
				vertex.tangent = tangents_ptr[i];
				//vertex.bitangent = bitangents_ptr[i];

				vertices.push_back(vertex);
			}

			auto mesh = skygfx::utils::Mesh();
			mesh.setIndices(indices);
			mesh.setVertices(vertices);

			auto draw_command = skygfx::utils::commands::DrawMesh::DrawIndexedVerticesCommand{
				.index_count = (uint32_t)index_count,
				.index_offset = (uint32_t)index_offset
			};

			const auto& material = model.materials.at(primitive.material);
			const auto& baseColorTexture = material.pbrMetallicRoughness.baseColorTexture;
			const auto& metallicRoughnessTexture = material.pbrMetallicRoughness.metallicRoughnessTexture;
			const auto& baseColorFactor = material.pbrMetallicRoughness.baseColorFactor;
			const auto& occlusionTexture = material.occlusionTexture;

			auto _material = std::make_shared<Material>();
			_material->color_texture = get_or_create_texture(baseColorTexture.index);
			_material->normal_texture = get_or_create_texture(material.normalTexture.index);
			_material->metallic_roughness_texture = get_or_create_texture(metallicRoughnessTexture.index);
			_material->color = {
				baseColorFactor.at(0),
				baseColorFactor.at(1),
				baseColorFactor.at(2),
				baseColorFactor.at(3)
			};

			auto draw_data = RenderBuffer::DrawData{
				.vertices = std::move(vertices),
				.indices = std::move(indices),
				.topology = topology,
				.mesh = std::move(mesh),
				.draw_command = draw_command
			};

			result.meshes[_material].push_back(std::move(draw_data));
		}
		// TODO: dont forget to draw childrens of node
	}

	return result;
}

void UpdateCamera(skygfx::utils::PerspectiveCamera& camera)
{
	if (cursor_is_interacting)
	{
		auto pos = PLATFORM->getCursorPos().value();

		auto dx = (double)pos.x - cursor_saved_pos_x;
		auto dy = (double)pos.y - cursor_saved_pos_y;

		const auto sensitivity = 0.25f;

		dx *= sensitivity;
		dy *= sensitivity;

		camera.yaw += glm::radians(static_cast<float>(dx));
		camera.pitch -= glm::radians(static_cast<float>(dy));

		PLATFORM->setCursorPos((int)cursor_saved_pos_x, (int)cursor_saved_pos_y);
	}

	static auto before = sky::Now();
	auto now = sky::Now();
	auto dtime = sky::ToSeconds(now - before);
	before = now;

	auto speed = dtime * 500.0f;

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::LeftShift))
		speed *= 3.0f;

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::LeftCtrl))
		speed /= 6.0f;

	glm::vec2 direction = { 0.0f, 0.0f };

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::W))
		direction.y = 1.0f;

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::S))
		direction.y = -1.0f;

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::A))
		direction.x = -1.0f;

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::D))
		direction.x = 1.0f;

	if (glm::length(direction) > 0.0f)
	{
		direction = glm::normalize(direction);
		direction *= speed;
	}

	auto angles_speed = dtime * 100.0f;

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::Right))
		camera.yaw += glm::radians(static_cast<float>(angles_speed));

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::Left))
		camera.yaw -= glm::radians(static_cast<float>(angles_speed));

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::Up))
		camera.pitch += glm::radians(static_cast<float>(angles_speed));

	if (PLATFORM->isKeyPressed(Platform::Input::Keyboard::Key::Down))
		camera.pitch -= glm::radians(static_cast<float>(angles_speed));

	constexpr auto limit = glm::pi<float>() / 2.0f - 0.01f;

	camera.pitch = fmaxf(-limit, camera.pitch);
	camera.pitch = fminf(+limit, camera.pitch);

	auto pi = glm::pi<float>();

	while (camera.yaw > pi)
		camera.yaw -= pi * 2.0f;

	while (camera.yaw < -pi)
		camera.yaw += pi * 2.0f;

	auto sin_yaw = glm::sin(camera.yaw);
	auto sin_pitch = glm::sin(camera.pitch);

	auto cos_yaw = glm::cos(camera.yaw);
	auto cos_pitch = glm::cos(camera.pitch);

	const float fov = 70.0f;
	const float near_plane = 1.0f;
	const float far_plane = 8192.0f;
	const glm::vec3 world_up = { 0.0f, 1.0f, 0.0f };

	auto front = glm::normalize(glm::vec3(cos_yaw * cos_pitch, sin_pitch, sin_yaw * cos_pitch));
	auto right = glm::normalize(glm::cross(front, world_up));
	//auto up = glm::normalize(glm::cross(right, front));

	if (glm::length(direction) > 0.0f)
	{
		camera.position += front * direction.y;
		camera.position += right * direction.x;
	}
}

template<typename T>
std::string GetPosteffectName()
{
	static_assert(sizeof(T) == -1, "GetPosteffectName<T> must be specialized for T");
	return "";
}

template<>
std::string GetPosteffectName<skygfx::utils::DrawSceneOptions::GrayscalePosteffect>() { return "Grayscale"; }

template<>
std::string GetPosteffectName<skygfx::utils::DrawSceneOptions::BloomPosteffect>() { return "Bloom"; }

template<>
std::string GetPosteffectName<skygfx::utils::DrawSceneOptions::GaussianBlurPosteffect>() { return "Gaussian Blur"; }

void DrawPosteffectOptions(skygfx::utils::DrawSceneOptions::GrayscalePosteffect& effect, int index)
{
	ImGui::SliderFloat(("Intensity##" + std::to_string(index)).c_str(), &effect.intensity, 0.0f, 1.0f);
}

void DrawPosteffectOptions(skygfx::utils::DrawSceneOptions::BloomPosteffect& effect, int index)
{
	ImGui::SliderFloat(("Threshold##" + std::to_string(index)).c_str(), &effect.threshold, 0.0f, 1.0f);
	ImGui::SliderFloat(("Intensity##" + std::to_string(index)).c_str(), &effect.intensity, 0.0f, 10.0f);
}

void DrawPosteffectOptions(skygfx::utils::DrawSceneOptions::GaussianBlurPosteffect& effect, int index)
{
}

static int gDrawcalls = 0;

void DrawGui(skygfx::utils::PerspectiveCamera& camera,
	skygfx::utils::DrawSceneOptions& options, bool& animate_lights, bool& show_normals)
{
	const int ImGuiWindowFlags_Overlay = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_Overlay & ~ImGuiWindowFlags_NoInputs);
	ImGui::SetWindowPos(ImVec2(10.0f, 10.0f));

	static int fps = 0;
	static int frame_count = 0;
	static auto before = sky::Now();

	auto now = sky::Now();
	frame_count++;

	if (sky::ToSeconds(now - before) >= 1.0)
	{
		fps = frame_count;
		frame_count = 0;
		before = now;
	}

	ImGui::Text("FPS: %d", fps);
	ImGui::Text("Drawcalls: %d", gDrawcalls);
	ImGui::Separator();
	ImGui::SliderAngle("Pitch##1", &camera.pitch, -89.0f, 89.0f);
	ImGui::SliderAngle("Yaw##1", &camera.yaw, -180.0f, 180.0f);
	ImGui::DragFloat3("Position##1", (float*)&camera.position);
	ImGui::Separator();
	ImGui::Checkbox("Textures", &options.use_color_textures);
	ImGui::Checkbox("Normal Mapping", &gNormalMapping);
	ImGui::SliderFloat("Mipmap bias", &options.mipmap_bias, -8.0f, 8.0f);
	ImGui::Checkbox("Animate Lights", &animate_lights);
	ImGui::Checkbox("Show Normals", &show_normals);
	ImGui::Separator();
	if (ImGui::RadioButton("Forward Shading", options.technique == skygfx::utils::DrawSceneOptions::Technique::ForwardShading))
		gTechnique = skygfx::utils::DrawSceneOptions::Technique::ForwardShading;
	if (ImGui::RadioButton("Deferred Shading", options.technique == skygfx::utils::DrawSceneOptions::Technique::DeferredShading))
		gTechnique = skygfx::utils::DrawSceneOptions::Technique::DeferredShading;
	ImGui::Separator();

	for (int i = 0; i < options.posteffects.size(); i++)
	{
		std::visit(cases{
			[&](auto& posteffect) {
				using T = std::decay_t<decltype(posteffect)>;
				auto name = GetPosteffectName<T>();
				ImGui::Text("%s", name.c_str());
				DrawPosteffectOptions(posteffect, i);
			}
		}, options.posteffects.at(i));

		ImGui::SameLine();
		if (ImGui::Button(("Remove##" + std::to_string(i)).c_str()))
		{
			options.posteffects.erase(options.posteffects.begin() + i);
		}

		ImGui::Separator();
	}

	static const std::vector<skygfx::utils::DrawSceneOptions::Posteffect> AvailablePosteffects = {
		skygfx::utils::DrawSceneOptions::GrayscalePosteffect{},
		skygfx::utils::DrawSceneOptions::BloomPosteffect{},
		skygfx::utils::DrawSceneOptions::GaussianBlurPosteffect{},
	};

	for (const auto& posteffect : AvailablePosteffects)
	{
		std::visit(cases{
			[&](auto& posteffect) {
				using T = std::decay_t<decltype(posteffect)>;
				auto name = GetPosteffectName<T>();

				if (ImGui::Button(("Add " + name + " Posteffect").c_str()))
					options.posteffects.emplace_back(posteffect);
			}
		}, posteffect);
	}

	ImGui::End();
}

skygfx::utils::Mesh CreateNormalsDebugMesh(const RenderBuffer& render_buffer)
{
	skygfx::utils::MeshBuilder mesh_builder;

	for (const auto& [material, draw_datas] : render_buffer.meshes)
	{
		for (const auto& draw_data : draw_datas)
		{
			auto draw_vertex = [&](const skygfx::utils::Mesh::Vertex& vertex) {
				mesh_builder.begin(skygfx::utils::MeshBuilder::Mode::Lines);
				mesh_builder.vertex({ .pos = vertex.pos, .color = { 0.0f, 1.0f, 0.0f, 1.0f } });
				mesh_builder.vertex({ .pos = vertex.pos + (vertex.normal * 25.0f), .color = { 0.0f, 1.0f, 0.0f, 1.0f } });
				mesh_builder.end();
			};

			std::visit(cases{
				[&](const skygfx::utils::commands::DrawMesh::DrawVerticesCommand& draw) {
					auto vertex_count = draw.vertex_count.value_or((uint32_t)draw_data.vertices.size());
					auto vertex_offset = draw.vertex_offset;

					for (uint32_t i = vertex_offset; i < vertex_count; i++)
					{
						const auto& vertex = draw_data.vertices.at(i);
						draw_vertex(vertex);
					}
				},
				[&](const skygfx::utils::commands::DrawMesh::DrawIndexedVerticesCommand& draw) {
					auto index_count = draw.index_count.value_or((uint32_t)draw_data.indices.size());
					auto index_offset = draw.index_offset;

					for (uint32_t i = index_offset; i < index_count; i++)
					{
						auto index = draw_data.indices.at(i);
						const auto& vertex = draw_data.vertices.at(index);
						draw_vertex(vertex);
					}
				}
			}, draw_data.draw_command);
		}
	}

	skygfx::utils::Mesh mesh;
	mesh_builder.setToMesh(mesh);

	return mesh;
}

void DrawNormals(const skygfx::utils::PerspectiveCamera& camera, const RenderBuffer& render_buffer)
{
	static auto mesh = CreateNormalsDebugMesh(render_buffer);;

	skygfx::utils::ExecuteCommands({
		skygfx::utils::commands::SetCamera(camera),
		skygfx::utils::commands::SetMesh(&mesh),
		skygfx::utils::commands::DrawMesh{}
	});
}

void sky_main()
{
	sky::Locator<sky::Application>::Init("sponza");

	auto mouseEventListener = sky::Listener<Platform::Input::Mouse::ButtonEvent>(MouseButtonCallback);
	auto keyboardEventListener = sky::Listener<Platform::Input::Keyboard::Event>(KeyCallback);

	sky::RunTask([] -> sky::Task<> {
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		auto path = "assets/sponza/sponza.glb";

		bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);

		auto camera = skygfx::utils::PerspectiveCamera();

		auto render_buffer = BuildRenderBuffer(model);

		auto directional_light = skygfx::utils::DirectionalLight();
		directional_light.ambient = { 0.125f, 0.125f, 0.125f };
		directional_light.diffuse = { 0.125f, 0.125f, 0.125f };
		directional_light.specular = { 1.0f, 1.0f, 1.0f };
		directional_light.shininess = 16.0f;
		directional_light.direction = { 0.5f, -1.0f, 0.5f };

		auto base_light = skygfx::utils::PointLight();
		base_light.shininess = 32.0f;
		base_light.constant_attenuation = 0.0f;
		base_light.linear_attenuation = 0.00128f;
		base_light.quadratic_attenuation = 0.0f;

		auto red_light = base_light;
		red_light.ambient = { 0.0625f, 0.0f, 0.0f };
		red_light.diffuse = { 0.5f, 0.0f, 0.0f };
		red_light.specular = { 1.0f, 0.0f, 0.0f };

		auto green_light = base_light;
		green_light.ambient = { 0.0f, 0.0625f, 0.0f };
		green_light.diffuse = { 0.0f, 0.5f, 0.0f };
		green_light.specular = { 0.0f, 1.0f, 0.0f };

		auto blue_light = base_light;
		blue_light.ambient = { 0.0f, 0.0f, 0.0625f };
		blue_light.diffuse = { 0.0f, 0.0f, 0.5f };
		blue_light.specular = { 0.0f, 0.0f, 1.0f };

		auto lightblue_light = base_light;
		lightblue_light.ambient = { 0.0f, 0.0625f, 0.0625f };
		lightblue_light.diffuse = { 0.0f, 0.5f, 0.5f };
		lightblue_light.specular = { 0.0f, 1.0f, 1.0f };

		struct MovingLight
		{
			skygfx::utils::PointLight light;
			glm::vec3 begin;
			glm::vec3 end;
			float multiplier = 1.0f;
		};

		std::vector<MovingLight> moving_lights = {
			// first floor
			{ red_light, { 1200.0f, 256.0f, -36.0f }, { -1200.0f, 256.0f, -36.0f }, 4.0f },
			{ green_light, { 1200.0f, 256.0f, -36.0f }, { -1200.0f, 256.0f, -36.0f }, 3.0f },
			{ blue_light, { 1200.0f, 256.0f, -36.0f }, { -1200.0f, 256.0f, -36.0f }, 2.0f },

			// second floor
			{ green_light, { 1100.0f, 550.0f, 400.0f }, { 1100.0f, 550.0f, -400.0f }, 1.0f },
			{ red_light, { -1200.0f, 550.0f, -400.0f }, { -1200.0f, 550.0f, 400.0f }, 2.0f },
			{ blue_light, { 1100.0f, 550.0f, 400.0f }, { -1200.0f, 550.0f, 400.0f }, 3.0f },
			{ lightblue_light, { 1100.0f, 550.0f, -400.0f }, { -1200.0f, 550.0f, -400.0f }, 4.0f }
		};

		std::vector<skygfx::utils::Model> models;

		for (const auto& [material, draw_datas] : render_buffer.meshes)
		{
			for (const auto& draw_data : draw_datas)
			{
				skygfx::utils::Model model;
				model.mesh = (skygfx::utils::Mesh*)&draw_data.mesh;
				model.draw_command = draw_data.draw_command;
				model.color = material->color;
				model.color_texture = material->color_texture.get();
				model.normal_texture = material->normal_texture.get();
				model.cull_mode = skygfx::CullMode::Front;
				model.texture_address = skygfx::TextureAddress::Wrap;
				model.depth_mode = skygfx::ComparisonFunc::LessEqual;
				models.push_back(model);
			}
		}

		skygfx::utils::DrawSceneOptions options = {
			.posteffects = {
				skygfx::utils::DrawSceneOptions::BloomPosteffect{}
			}
		};

		bool animate_lights = true;
		bool show_normals = false;
		auto before = sky::Now();
		auto time = sky::Now() - before;

		StageViewer stage_viewer;
		skygfx::utils::SetStageViewer(&stage_viewer);

		while (true)
		{
			DrawGui(camera, options, animate_lights, show_normals);

			options.technique = gTechnique;
			options.use_normal_textures = gNormalMapping;

			UpdateCamera(camera);

			if (animate_lights)
				time = sky::Now() - before;

			std::vector<skygfx::utils::Light> lights = { directional_light };

			for (auto& moving_light : moving_lights)
			{
				moving_light.light.position = glm::lerp(moving_light.begin, moving_light.end, (glm::sin(sky::ToSeconds(time) / moving_light.multiplier) + 1.0f) * 0.5f);
				lights.push_back(moving_light.light);
			}

			skygfx::utils::DrawScene(nullptr, camera, models, lights, options);

			if (show_normals)
				DrawNormals(camera, render_buffer);

		//	stage_viewer.show();

			co_await sky::Tasks::NextFrame();
		}
	}());

	sky::Locator<sky::Application>::Get()->run();
	sky::Locator<sky::Application>::Reset();
}