#include <iostream>
#include <cstdint>
#include <sstream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <SDL.h>
#include <array>
#include <numeric>

#pragma pack(push, 1)
struct xcf_col_t
{
	uint8_t r, g, b;
	xcf_col_t(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
	xcf_col_t() : xcf_col_t(0, 0, 0) {}
};
#pragma pack(pop)

enum struct xcf_property_type : uint32_t
{
	end = 0,
	col_map = 1,
	layer_mode = 7,
	layer_offset = 15,
	compression = 17,
	guides = 18,
	resolution = 19,
	tattoo = 20,
	parasites = 21,
	unit = 22,
	paths = 23,
	user_unit = 24,
	vectors = 25,
	sample_points = 27,
};

enum struct xcf_col_mode : uint32_t
{
	rgb = 0,
	grayscale = 1,
	indexed = 2
};

enum struct xcf_layer_col_mode : uint32_t
{
	rgb = 0,
	rgb_a = 1,
	grayscale = 2,
	grayscale_a = 3,
	indexed = 4,
	indexed_a = 5
};

enum struct xcf_comp_mode : uint8_t
{
	none = 0,
	rle = 1,
	zlib = 2,
	fractal = 3
};

enum struct xcf_layer_mode : uint32_t
{
	normal = 0,
	dissolve = 1,
	behind = 2,
	multiply = 3,
	screen = 4,
	overlay = 5,
	difference = 6,
	addition = 7,
	subtract = 8,
	darken_only = 9,
	lighten_only = 10,
	hue = 11,
	saturation = 12,
	colour = 13,
	value = 14,
	divide = 15,
	dodge = 16,
	burn = 17,
	hard_light = 18,
	soft_light = 19,
	grain_extract = 20,
	grain_merge = 21
};

template<typename T>
T fread(std::ifstream& file, bool big_endian = false)
{
	T res{};
	file.read(reinterpret_cast<char*>(&res), sizeof(T));
	if (big_endian)
	{
		auto beg = reinterpret_cast<char*>(&res);
		std::reverse(beg, beg + sizeof(T));
	}
	return res;
}

size_t flength(std::ifstream& file)
{
	auto const pos = file.tellg();
	file.seekg(0, std::ios::end);
	auto const len = file.tellg();
	file.seekg(pos, std::ios::beg);
	return len;
}

void fread_force(std::ifstream& file, char* buff, size_t size)
{
	size_t
		pos = file.tellg(),
		len = flength(file),
		real_len = size;

	if (pos + size > len)
		real_len = len - pos;

	file.read(buff, real_len);

	if (auto const extra_len = size - real_len; extra_len > 0)
		std::fill_n(buff + real_len, extra_len, 0);
}

struct xcf_rect_t
{
	uint32_t x, y, w, h;
	xcf_rect_t(uint32_t x, uint32_t y, uint32_t w, uint32_t h) :
		x(x), y(y), w(w), h(h)
	{

	}
};

struct xcf_property_t
{
	xcf_property_type type;
	uint32_t payload;
};

using property_pair_t = std::pair<xcf_property_t, uint32_t>;
inline std::string xcf_read_string(std::ifstream& file, bool big_endian = false);
inline xcf_property_t xcf_read_property(std::ifstream& file, bool big_endian = false);
inline std::vector<property_pair_t> xcf_query_properties_list(std::ifstream& file);

struct xcf_property_col_map_t
{
	xcf_property_t head;
	uint32_t count;
	std::unique_ptr<xcf_col_t[]> palette;
	xcf_property_col_map_t(xcf_property_t head, std::ifstream& file) :
		head(head),
		count(fread<uint32_t>(file, true)),
		palette(std::make_unique<xcf_col_t[]>(count))
	{
		file.read(reinterpret_cast<char*>(palette.get()), count * sizeof(xcf_col_t));
	}
};

struct xcf_property_comp_t
{
	xcf_property_t head;
	xcf_comp_mode compression;
	xcf_property_comp_t(xcf_property_t head, std::ifstream& file) :
		head(head),
		compression(fread<xcf_comp_mode>(file))
	{

	}
};

struct xcf_property_res_t
{
	xcf_property_t head;
	float hres, vres; //ppi
	xcf_property_res_t(xcf_property_t head, std::ifstream& file) :
		head(head),
		hres(fread<float>(file, true)),
		vres(fread<float>(file, true))
	{

	}
};

struct xcf_property_layer_mode_t
{
	xcf_property_t head;
	xcf_layer_mode mode;
	xcf_property_layer_mode_t(xcf_property_t head, std::ifstream& file) :
		head(head),
		mode(fread<xcf_layer_mode>(file, true))
	{

	}
};

struct xcf_property_layer_offset_t
{
	xcf_property_t head;
	int32_t x_offset, y_offset;
	xcf_property_layer_offset_t(xcf_property_t head, std::ifstream& file) :
		head(head),
		x_offset(fread<int32_t>(file, true)),
		y_offset(fread<int32_t>(file, true))
	{

	}
};

struct xcf_layer_t
{
	uint32_t width, height;
	xcf_layer_col_mode type;
	std::string name;
	std::vector<property_pair_t> properties;
	uint32_t hierarchy_ptr, mask_ptr;
	xcf_layer_t(std::ifstream& file) :
		width(fread<uint32_t>(file, true)),
		height(fread<uint32_t>(file, true)),
		type(fread<xcf_layer_col_mode>(file, true)),
		name(xcf_read_string(file, true)),
		properties(xcf_query_properties_list(file)),
		hierarchy_ptr(fread<uint32_t>(file, true)),
		mask_ptr(fread<uint32_t>(file, true))
	{
		for (const auto& property : properties)
			auto[prop, pos] = property;
	}
};

struct xcf_hierarchy_t
{
	uint32_t width, height, bpp, level_ptr;
	xcf_hierarchy_t(std::ifstream& file) :
		width(fread<uint32_t>(file, true)),
		height(fread<uint32_t>(file, true)),
		bpp(fread<uint32_t>(file, true)),
		level_ptr(fread<uint32_t>(file, true))
	{
	}
};

std::vector<uint32_t> xcf_read_level_ptrs(std::ifstream& file)
{
	std::vector<uint32_t> res;
	res.reserve(0x10);

	while (true)
	{
		if (auto const level = fread<uint32_t>(file, true); level != 0)
			res.push_back(level);
		else break;
	}
	return res;
}

constexpr size_t xcf_tile_width = 64, xcf_tile_height = 64;
constexpr float xcf_tile_max_data_len_factor = 1.5f;

struct xcf_level_t
{
	uint32_t width, height, tile_x, tile_y, tile_c;
	xcf_level_t(std::ifstream& file) :
		width(fread<uint32_t>(file, true)),
		height(fread<uint32_t>(file, true)),
		tile_x(uint32_t(std::ceil(float(width) / float(xcf_tile_width)))),
		tile_y(uint32_t(std::ceil(float(height) / float(xcf_tile_height)))),
		tile_c(tile_x * tile_y)
	{
	}
};

inline std::vector<uint32_t> xcf_read_tile_ptrs(xcf_level_t const& level, std::ifstream& file)
{
	std::vector<uint32_t> res;
	res.reserve(level.tile_c);

	uint32_t tile_ptr = 0;
	do
	{
		if (tile_ptr = fread<uint32_t>(file, true); tile_ptr != 0)
			res.push_back(tile_ptr);
	} while (tile_ptr != 0);
	return res;
}

inline std::string raw_read_string(std::ifstream& file, size_t length)
{
	std::string res(length, '\0');
	file.read(reinterpret_cast<char*>(res.data()), length);
	return res.substr(0, length - 1);
}

inline std::string xcf_read_string(std::ifstream& file, bool big_endian)
{
	const uint32_t length = fread<uint32_t>(file, big_endian);
	if (length != 0)
		return raw_read_string(file, length);
	else return std::string("null");
}

inline xcf_property_t xcf_read_property(std::ifstream& file, bool big_endian)
{
	xcf_property_t res = {};
	res.type = fread<xcf_property_type>(file, big_endian);
	res.payload = fread<uint32_t>(file, big_endian);
	return res;
}

inline std::vector<property_pair_t> xcf_query_properties_list(std::ifstream& file)
{
	std::vector<property_pair_t> res;
	res.reserve(0x100);

	xcf_property_t property = {};
	uint32_t pos = 0;
	do
	{
		pos = file.tellg();
		property = xcf_read_property(file, true);


		if (property.payload != 0)
			file.seekg(property.payload, std::ios::cur);

		res.push_back(std::make_pair(property, pos));
	} while (property.type != xcf_property_type::end);
	return res;
}

inline std::vector<uint32_t> xcf_read_layer_pointers(std::ifstream& file)
{
	std::vector<uint32_t> res;
	res.reserve(0x100);

	uint32_t pointer = 0;
	do
	{
		if (pointer = fread<uint32_t>(file, true); pointer != 0)
			res.push_back(pointer);
	} while (pointer != 0);

	return res;
}

inline xcf_rect_t xcf_calc_tile_rect(xcf_level_t const& level, uint32_t tile_id)
{
	auto const tile_col_count = level.tile_x;
	auto const tile_row_count = level.tile_y;

	if (tile_id > tile_row_count * tile_col_count - 1)
		return xcf_rect_t(0, 0, 0, 0);

	auto const tile_rows = tile_id / tile_col_count;
	auto const tile_cols = tile_id % tile_col_count;

	xcf_rect_t rect(
		tile_cols * xcf_tile_width,
		tile_rows * xcf_tile_height,
		0, 0);

	if (tile_cols == tile_col_count - 1)
		rect.w = level.width - rect.x;
	else
		rect.w = xcf_tile_width;

	if (tile_rows == tile_row_count - 1)
		rect.h = level.height - rect.y;
	else
		rect.h = xcf_tile_height;

	return rect;
}

inline bool xcf_read_tile_rle(std::ifstream& file, xcf_rect_t const& tile_rect, uint32_t data_len, xcf_hierarchy_t const& hierarchy, xcf_level_t const& level, uint32_t ver, uint8_t* out_buffer, size_t& buffer_pos)
{
	auto const tile_size = hierarchy.bpp * tile_rect.w * tile_rect.h;
	auto tile_data = std::make_unique<uint8_t[]>(tile_size);

	if (data_len == 0)
		return false;

	auto xcf_data_smart = std::make_unique<uint8_t[]>(data_len);

	uint8_t
		*xcf_o_data = xcf_data_smart.get(),
		*xcf_data = xcf_o_data;

	auto const beg = file.tellg();
	fread_force(file, reinterpret_cast<char*>(xcf_data_smart.get()), data_len);
	auto const bytes_read = file.tellg() - beg;

	auto xcf_data_limit = &xcf_o_data[bytes_read - 1];

	for (size_t i = 0; i < hierarchy.bpp; i++)
	{
		auto data = tile_data.get() + i;
		auto size = int32_t(tile_rect.w * tile_rect.h);
		auto count = size_t(0);

		while (size > 0)
		{
			if (xcf_data > xcf_data_limit)
				return false;

			auto length = size_t(*xcf_data++);

			if (length >= 128)
			{
				if (length = 255 - (length - 1); length == 128)
				{
					if (xcf_data >= xcf_data_limit)
						return false;

					length = (*xcf_data << 8) + xcf_data[1];
					xcf_data += 2;
				}

				count += length;
				size -= length;

				if (size < 0)
					return false;

				if (&xcf_data[length - 1] > xcf_data_limit)
					return false;

				while (length-- > 0)
				{
					*data = *xcf_data++;
					data += hierarchy.bpp;
				}
			}
			else
			{
				length += 1;
				if (length == 128)
				{
					if (xcf_data >= xcf_data_limit)
						return false;

					length = (*xcf_data << 8) + xcf_data[1];
					xcf_data += 2;
				}

				count += length;
				size -= length;

				if (size < 0)
					return false;

				if (xcf_data > xcf_data_limit)
					return false;

				const auto val = *xcf_data++;
				for (size_t j = 0; j < length; j++)
				{
					*data = val;
					data += hierarchy.bpp;
				}
			}
		}
	}

	if (ver >= 12)
	{
		auto const comps = 0;
	}
	auto const tile_stride = tile_rect.w * hierarchy.bpp;
	auto const full_stride = hierarchy.width * hierarchy.bpp;

	for (size_t row = 0; row < tile_rect.h; row++)
	{
		auto const pos_src = (row * tile_rect.w) * hierarchy.bpp;
		auto const pos_dst = ((row + tile_rect.y) * hierarchy.width + tile_rect.x) * hierarchy.bpp;
		std::copy_n(tile_data.get() + pos_src, tile_stride, out_buffer + pos_dst);
	}

	buffer_pos += tile_size;
}

inline uint32_t tag_to_ver_num(std::string_view ver)
{
	if (ver.find_first_of("file") != std::string_view::npos)
		return 0;
	else if (ver[0] == 'v')
		return std::atoi(std::string(ver.substr(1).data()).c_str());
	else std::numeric_limits<uint32_t>::max();
}

struct raw_layer_t
{
	uint8_t* buffer;
	int32_t w, h, x, y;
	uint32_t bpp;
	xcf_col_mode mode;
	raw_layer_t() :
		buffer(nullptr),
		w(0),
		h(0),
		x(0),
		y(0),
		bpp(0),
		mode(xcf_col_mode::rgb) {}
};

void draw_pixel(std::array<xcf_col_t, 256> const& palette,
	size_t src_pos, size_t src_bpp, xcf_col_mode src_mode, uint8_t* src_buff,
	size_t dst_pos, size_t dst_bpp, xcf_col_mode dst_mode, uint8_t* dst_buff, bool swap = false)
{
	if (src_mode == xcf_col_mode::indexed)
	{
		if (dst_mode == xcf_col_mode::indexed)
		{
			switch (src_bpp)
			{
			case 1: {
				switch (dst_bpp)
				{
				case 1: {
					dst_buff[dst_pos] = src_buff[src_pos];
				} break;
				case 2: {
					dst_buff[dst_pos * 2 + 0] = src_buff[src_pos];
					dst_buff[dst_pos * 2 + 1] = 255;
				} break;
				}
			} break;
			case 2: {
				switch (dst_bpp)
				{
				case 2: {
					dst_buff[dst_pos * 2 + 0] = src_buff[src_pos * 2 + 0];
					dst_buff[dst_pos * 2 + 1] = src_buff[src_pos * 2 + 1];
				} break;
				}
			} break;
			}
		}
		else if (dst_mode == xcf_col_mode::rgb)
		{
			switch (src_bpp)
			{
			case 1: {
				switch (dst_bpp)
				{
				case 3: {
					auto const col = palette[src_buff[src_pos]];
					dst_buff[dst_pos * 3 + 0] = col.b;
					dst_buff[dst_pos * 3 + 1] = col.g;
					dst_buff[dst_pos * 3 + 2] = col.r;
				} break;
				case 4: {
					auto const col = palette[src_buff[src_pos]];
					dst_buff[dst_pos * 4 + 0] = col.b;
					dst_buff[dst_pos * 4 + 1] = col.g;
					dst_buff[dst_pos * 4 + 2] = col.r;
					dst_buff[dst_pos * 4 + 3] = 255;
				} break;
				}
			} break;
			case 2: {
				switch (dst_bpp)
				{
				case 3: {
					if (src_buff[src_pos * 2 + 1] > 0)
					{
						auto const col = palette[src_buff[src_pos * 2 + 0]];
						dst_buff[dst_pos * 3 + 0] = col.b;
						dst_buff[dst_pos * 3 + 1] = col.g;
						dst_buff[dst_pos * 3 + 2] = col.r;
					}
				} break;
				case 4: {
					if (src_buff[src_pos * 2 + 1] > 0)
					{
						auto const col = palette[src_buff[src_pos * 2 + 0]];
						dst_buff[dst_pos * 4 + 0] = col.b;
						dst_buff[dst_pos * 4 + 1] = col.g;
						dst_buff[dst_pos * 4 + 2] = col.r;
						dst_buff[dst_pos * 4 + 3] = src_buff[src_pos * 2 + 1];
					}
				} break;
				}
			} break;
			}
		}
	}
	else if (src_mode == xcf_col_mode::rgb)
	{
		if (dst_mode == xcf_col_mode::rgb)
		{
			switch (src_bpp)
			{
			case 3: {
				switch (dst_bpp)
				{
				case 3: {
					if (swap)
					{
						dst_buff[dst_pos * 3 + 0] = src_buff[src_pos * 3 + 2];
						dst_buff[dst_pos * 3 + 1] = src_buff[src_pos * 3 + 1];
						dst_buff[dst_pos * 3 + 2] = src_buff[src_pos * 3 + 0];
					}
					else
					{
						dst_buff[dst_pos * 3 + 0] = src_buff[src_pos * 3 + 0];
						dst_buff[dst_pos * 3 + 1] = src_buff[src_pos * 3 + 1];
						dst_buff[dst_pos * 3 + 2] = src_buff[src_pos * 3 + 2];
					}
				} break;
				case 4: {
					if (swap)
					{
						dst_buff[dst_pos * 4 + 0] = src_buff[src_pos * 3 + 2];
						dst_buff[dst_pos * 4 + 1] = src_buff[src_pos * 3 + 1];
						dst_buff[dst_pos * 4 + 2] = src_buff[src_pos * 3 + 0];
					}
					else
					{
						dst_buff[dst_pos * 4 + 0] = src_buff[src_pos * 3 + 0];
						dst_buff[dst_pos * 4 + 1] = src_buff[src_pos * 3 + 1];
						dst_buff[dst_pos * 4 + 2] = src_buff[src_pos * 3 + 2];
					}
					dst_buff[dst_pos * 4 + 3] = 255;
				} break;
				}
			} break;
			case 4: {
				switch (dst_bpp)
				{
				case 3: {
					if (src_buff[src_pos * 4 + 3] > 0)
					{
						if (swap)
						{
							dst_buff[dst_pos * 3 + 0] = src_buff[src_pos * 4 + 2];
							dst_buff[dst_pos * 3 + 1] = src_buff[src_pos * 4 + 1];
							dst_buff[dst_pos * 3 + 2] = src_buff[src_pos * 4 + 0];
						}
						else
						{
							dst_buff[dst_pos * 3 + 0] = src_buff[src_pos * 4 + 0];
							dst_buff[dst_pos * 3 + 1] = src_buff[src_pos * 4 + 1];
							dst_buff[dst_pos * 3 + 2] = src_buff[src_pos * 4 + 2];
						}
					}
				} break;
				case 4: {
					if (src_buff[src_pos * 4 + 3] > 0)
					{
						if (swap)
						{
							dst_buff[dst_pos * 4 + 0] = src_buff[src_pos * 4 + 2];
							dst_buff[dst_pos * 4 + 1] = src_buff[src_pos * 4 + 1];
							dst_buff[dst_pos * 4 + 2] = src_buff[src_pos * 4 + 0];
						}
						else
						{
							dst_buff[dst_pos * 4 + 0] = src_buff[src_pos * 4 + 0];
							dst_buff[dst_pos * 4 + 1] = src_buff[src_pos * 4 + 1];
							dst_buff[dst_pos * 4 + 2] = src_buff[src_pos * 4 + 2];
						}
						dst_buff[dst_pos * 4 + 3] = 255;
					}
				} break;
				}
			} break;
			}
		}
	}
	else if (src_mode == xcf_col_mode::grayscale)
	{
		if (dst_mode == xcf_col_mode::rgb)
		{
			switch (src_bpp)
			{
			case 1: {
				switch (dst_bpp)
				{
				case 3: {
					dst_buff[dst_pos * 3 + 0] = src_buff[src_pos];
					dst_buff[dst_pos * 3 + 1] = src_buff[src_pos];
					dst_buff[dst_pos * 3 + 2] = src_buff[src_pos];
				} break;
				case 4: {
					dst_buff[dst_pos * 4 + 0] = src_buff[src_pos];
					dst_buff[dst_pos * 4 + 1] = src_buff[src_pos];
					dst_buff[dst_pos * 4 + 2] = src_buff[src_pos];
					dst_buff[dst_pos * 4 + 3] = 255;
				} break;
				}
			} break;
			case 2: {
				switch (dst_bpp)
				{
				case 3: {
					if (src_buff[src_pos * 2 + 1] > 0)
					{
						auto const scale = src_buff[src_pos * 2 + 0];
						dst_buff[dst_pos * 3 + 0] = scale;
						dst_buff[dst_pos * 3 + 1] = scale;
						dst_buff[dst_pos * 3 + 2] = scale;
					}
				} break;
				case 4: {
					if (src_buff[src_pos * 2 + 1] > 0)
					{
						auto const scale = src_buff[src_pos * 2 + 0];
						dst_buff[dst_pos * 4 + 0] = scale;
						dst_buff[dst_pos * 4 + 1] = scale;
						dst_buff[dst_pos * 4 + 2] = scale;
						dst_buff[dst_pos * 4 + 3] = src_buff[src_pos * 2 + 1];
					}
				} break;
				}
			} break;
			}
		}
	}
}

void draw(raw_layer_t const& layer, raw_layer_t& target, std::array<xcf_col_t, 256> const& palette, bool swap = false)
{
	auto const
		layer_len = layer.w*layer.h,
		target_len = target.w*target.h;

	for (int32_t y = 0; y < layer.h; y++)
		for (int32_t x = 0; x < layer.w; x++)
		{
			auto const
				src_pos = (y * layer.w + x),
				dst_pos = ((y + layer.y) * target.w + (x + layer.x));

			if ((dst_pos < target_len && dst_pos >= 0) && (src_pos < layer_len && src_pos >= 0))
			{
				draw_pixel(palette,
					src_pos, layer.bpp, layer.mode, layer.buffer,
					dst_pos, target.bpp, target.mode, target.buffer, swap);
			}
		}
}

void combine_layers_indexed_alpha(raw_layer_t& result, raw_layer_t* layers, size_t layer_count, size_t canvas_w, size_t canvas_h, std::array<xcf_col_t, 256> const& palette)
{
	for (size_t i = 0; i < layer_count; i++)
		draw(layers[(layer_count - 1) - i], result, palette);
}

void import_xcf(std::ifstream& file)
{
	if (file.good())
	{
		SDL_Init(SDL_INIT_VIDEO);
		std::vector<SDL_Window*> windows;
		std::vector<uint8_t*> window_pixels;

		auto const sig = raw_read_string(file, 9);
		auto const ver = raw_read_string(file, 5);

		if (sig.find_first_of("gimp xcf ") != 0)
			throw std::runtime_error("invalid gimp file");

		uint32_t const width = fread<uint32_t>(file, true);
		uint32_t const height = fread<uint32_t>(file, true);
		xcf_col_mode const col_mode = fread<xcf_col_mode>(file, true);

		auto properties = xcf_query_properties_list(file);
		auto const post_props_pos = file.tellg();

		xcf_comp_mode compression = xcf_comp_mode::none;

		std::array<xcf_col_t, 256> palette;

		for (const auto& property_pair : properties)
		{
			const auto[prop, pos] = property_pair;

			file.seekg(pos + sizeof(xcf_property_t), std::ios::beg);
			switch (prop.type)
			{
			case xcf_property_type::col_map:
			{
				xcf_property_col_map_t col_map(prop, file);
				std::clog << "palette size: " << col_map.count << '\n';

				for (uint32_t i = 0; i < col_map.count; i++)
				{
					palette[i] = col_map.palette.get()[i];
					std::clog << "col " << i << ": " << '(' << uint32_t(col_map.palette.get()[i].r) << ", " << uint32_t(col_map.palette.get()[i].g) << ", " << uint32_t(col_map.palette.get()[i].b) << ")\n";
				}
			} break;
			case xcf_property_type::compression:
			{
				xcf_property_comp_t comp(prop, file);
				compression = comp.compression;
				std::clog << "comp mode: " << uint32_t(comp.compression) << '\n';
			} break;
			case xcf_property_type::resolution:
			{
				xcf_property_res_t res(prop, file);
			} break;
			}
		}

		file.seekg(post_props_pos, std::ios::beg);

		auto layer_ptrs = xcf_read_layer_pointers(file);
		auto layers = std::vector<xcf_layer_t>();
		layers.reserve(layer_ptrs.size());

		for (auto ptr : layer_ptrs)
		{
			file.seekg(ptr, std::ios::beg);
			layers.push_back(xcf_layer_t(file));
		}

		raw_layer_t* layer_buffers = new raw_layer_t[layers.size()];
		size_t layer_buffer_i = 0;
		for (const auto& layer : layers)
		{
			auto const[layer_off_x, layer_off_y] = [&]() {
				for (auto& const property : layer.properties)
				{
					auto const[prop, pos] = property;
					file.seekg(pos + sizeof(xcf_property_t), std::ios::beg);

					switch (property.first.type)
					{
					case xcf_property_type::layer_offset:
						xcf_property_layer_offset_t offset(prop, file);
						return std::make_pair(offset.x_offset, offset.y_offset);
					}
				}
			}();

			file.seekg(layer.hierarchy_ptr, std::ios::beg);
			xcf_hierarchy_t hierarchy(file);
			auto const levels = xcf_read_level_ptrs(file);

			file.seekg(hierarchy.level_ptr, std::ios::beg);
			xcf_level_t level(file);
			auto const tiles = xcf_read_tile_ptrs(level, file);

			auto const max_data_len = size_t(float(xcf_tile_width * xcf_tile_height * hierarchy.bpp) * xcf_tile_max_data_len_factor);
			auto const tile_cols = level.tile_x;
			auto const tile_rows = level.tile_y;
			auto const tile_count = level.tile_c;

			size_t pixel_buffer_pos = 0;
			auto const pixel_buffer_length = hierarchy.width * hierarchy.height * hierarchy.bpp;
			auto pixel_buffer = new uint8_t[pixel_buffer_length];

			auto offset0 = tiles.begin();

			for (size_t i = 0; i < tiles.size(); i++)
			{
				if (offset0 == tiles.end())
					break;

				auto const rect = xcf_calc_tile_rect(level, i);
				auto const saved_pos = file.tellg();
				auto const offset1 = [&]() {
					if (auto const it = offset0 + 1; it == tiles.end())
						return (*offset0) + (max_data_len);
					else
						return *it;
				}();

				file.seekg(*offset0, std::ios::beg);

				if (offset1 < *offset0 || offset1 - *offset0 > max_data_len)
					throw std::runtime_error("invalid tile data length");

				switch (compression)
				{
				case xcf_comp_mode::none:
					break;
				case xcf_comp_mode::rle:

					xcf_read_tile_rle(file, rect, offset1 - *offset0, hierarchy, level, tag_to_ver_num(ver), pixel_buffer, pixel_buffer_pos);

					break;
				case xcf_comp_mode::zlib:
					break;
				case xcf_comp_mode::fractal:
					break;
				default:
					throw std::runtime_error("unknown compression");
				}

				file.seekg(saved_pos, std::ios::beg);
				offset0++;
			}

			windows.push_back(SDL_CreateWindow(layer.name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, layer.width, layer.height, SDL_WindowFlags::SDL_WINDOW_SHOWN));
			window_pixels.push_back(reinterpret_cast<uint8_t*>(SDL_GetWindowSurface(windows.back())->pixels));
			auto pix = window_pixels.back();

			std::fill_n(reinterpret_cast<uint32_t*>(pix), layer.width*layer.height, 0xffff00ff);

			raw_layer_t win_layer = {};
			win_layer.bpp = 4;
			win_layer.mode = xcf_col_mode::rgb;
			win_layer.w = layer.width;
			win_layer.h = layer.height;
			win_layer.x = win_layer.y = 0;
			win_layer.buffer = pix;

			auto& raw_layer = layer_buffers[layer_buffer_i++];
			raw_layer.buffer = pixel_buffer;
			raw_layer.w = hierarchy.width;
			raw_layer.h = hierarchy.height;
			raw_layer.bpp = hierarchy.bpp;
			raw_layer.x = layer_off_x;
			raw_layer.y = layer_off_y;
			raw_layer.mode = col_mode;

			draw(raw_layer, win_layer, palette, col_mode == xcf_col_mode::rgb);
		}

		raw_layer_t canvas_layer = {};
		canvas_layer.bpp = 4;
		canvas_layer.mode = xcf_col_mode::rgb;
		canvas_layer.w = width;
		canvas_layer.h = height;
		canvas_layer.x = canvas_layer.y = 0;
		canvas_layer.buffer = new uint8_t[width*height * 4];

		combine_layers_indexed_alpha(canvas_layer, layer_buffers, layer_buffer_i, width, height, palette);
		windows.push_back(SDL_CreateWindow("COMBINE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN));
		window_pixels.push_back(reinterpret_cast<uint8_t*>(SDL_GetWindowSurface(windows.back())->pixels));
		auto pix = window_pixels.back();
		for (size_t i = 0; i < width * height; i++)
		{
			if (auto const opa = canvas_layer.buffer[i * 4 + 3]; opa > 0)
			{
				if (col_mode == xcf_col_mode::rgb)
				{
					pix[i * 4 + 0] = canvas_layer.buffer[i * 4 + 2];
					pix[i * 4 + 1] = canvas_layer.buffer[i * 4 + 1];
					pix[i * 4 + 2] = canvas_layer.buffer[i * 4 + 0];
				}
				else
				{
					pix[i * 4 + 0] = canvas_layer.buffer[i * 4 + 0];
					pix[i * 4 + 1] = canvas_layer.buffer[i * 4 + 1];
					pix[i * 4 + 2] = canvas_layer.buffer[i * 4 + 2];
				}
				pix[i * 4 + 3] = 255;
			}
		}

		delete[] canvas_layer.buffer;

		for (size_t i = 0; i < layer_buffer_i; i++)
			delete[] layer_buffers[i].buffer;
		delete[] layer_buffers;

		SDL_Event ev = {};
		while (ev.type != SDL_QUIT)
			for (auto window : windows)
			{
				SDL_PollEvent(&ev);
				SDL_UpdateWindowSurface(window);
			}

		for (auto window : windows)
			SDL_DestroyWindow(window);
		SDL_Quit();
	}
}