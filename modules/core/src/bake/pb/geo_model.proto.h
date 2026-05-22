#pragma once
#include <ylt/struct_pb.hpp>

namespace ysm::bake::pb {

struct CubeLegacy : public iguana::base_impl<CubeLegacy>  {
	CubeLegacy() = default;
    uint32_t face_count = 0;
	std::vector<float>pos; // x0 y0 z0 x1 y1 ...
	std::vector<uint32_t>pos_indices; // pos indices of each face
	std::vector<float>uv;
	std::vector<uint32_t>uv_indices;
	std::vector<float>normal;
};
YLT_REFL(CubeLegacy, face_count, pos, pos_indices, uv, uv_indices, normal);

struct Cubes : public iguana::base_impl<Cubes>  {
	Cubes() = default;
	std::vector<CubeLegacy>cubes_legacy;
};
YLT_REFL(Cubes, cubes_legacy);

struct Bone : public iguana::base_impl<Bone>  {
	Bone() = default;
	std::string name;
	std::string parent;
	std::vector<float>pivot;
	std::vector<float>rotate;
	bool debug = false;
	uint32_t cube_count = 0;
};
YLT_REFL(Bone, name, parent, pivot, rotate, debug, cube_count);

struct GeoProperties : public iguana::base_impl<GeoProperties>  {
	GeoProperties() = default;
	std::string identifier;
	float texture_height = 0;
	float texture_width = 0;
	float visible_bounds_height = 0;
	float visible_bounds_width = 0;
	std::vector<float>visible_bounds_offset;
};
YLT_REFL(GeoProperties, identifier, texture_height, texture_width, visible_bounds_height, visible_bounds_width, visible_bounds_offset);

struct GeoModel : public iguana::base_impl<GeoModel>  {
	GeoModel() = default;
	std::vector<Bone>bones;
	GeoProperties properties;
	bool legacy_format = false;
	Cubes cubes;
};
YLT_REFL(GeoModel, bones, properties, legacy_format, cubes);

}
