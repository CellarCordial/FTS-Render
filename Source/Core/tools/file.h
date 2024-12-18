#ifndef CORE_FILE_H
#define CORE_FILE_H
#include <filesystem>
#include <string>
#include <fstream>

namespace fantasy
{
	inline bool is_file_exist(const char* path)
	{
		const std::filesystem::path file_path(path);
		return std::filesystem::exists(file_path);
	}

	inline std::filesystem::file_time_type get_file_last_write_time(const char* path)
	{
		const std::filesystem::path file_path(path);
		return std::filesystem::last_write_time(file_path);
	}

	inline bool compare_file_write_time(const char* file0, const char* file1)
	{
		return get_file_last_write_time(file0) > get_file_last_write_time(file1);
	}

	inline std::string remove_file_extension(const char* path)
	{
		const std::filesystem::path file_path(path);
		return file_path.filename().replace_extension().string();
	}

	inline void replace_back_slashes(std::string& str) 
	{
		std::string::size_type pos = 0;
		while ((pos = str.find("\\", pos)) != std::string::npos) 
		{
			str.replace(pos, 1, "/");
			pos += 1;
		}
	}


	namespace serialization
	{
		class BinaryOutput
		{
		public:
			BinaryOutput(const std::string& file_name) : _output(file_name, std::ios::binary) {}
			~BinaryOutput() { _output.close(); }

			template <typename... Args>
			void operator()(Args&&... arguments)
			{
				(process(arguments), ...);
			}

		public:
			void save_binary_data(const void* data, int64_t size)
			{
				if (_output.is_open())
				{
					_output.write(static_cast<const char*>(data), size);
					_output.write("\n", 1);
				}
			}

		private:
			template <typename T>
			void process(T&& value)
			{
				process_impl(value);
			}

			template <typename T>
			void process_impl(const std::vector<T>& value)
			{
				uint64_t size = value.size();
				save_binary_data(&size, sizeof(uint64_t));
				for (const T& element : value)
				{
					process_impl(element);
				}
			}

			void process_impl(const std::string& value)
			{
				uint64_t size = value.size();
				save_binary_data(&size, sizeof(uint64_t));
				save_binary_data(value.data(), static_cast<int64_t>(size));
			}

			void process_impl(uint64_t value) { save_binary_data(&value, sizeof(uint64_t)); }
			void process_impl(uint32_t value) { save_binary_data(&value, sizeof(uint32_t)); }
			void process_impl(float value) { save_binary_data(&value, sizeof(float)); }

		private:
			std::ofstream _output;
		};

		class BinaryInput
		{
		public:
			BinaryInput(const std::string& file_name) : _input(file_name, std::ios::binary) {}
			~BinaryInput() noexcept { _input.close(); }

			template <typename... Args>
			void operator()(Args&&... arguments)
			{
				(process(arguments), ...);
			}

		public:
			void load_binary_data(void* out_data, int64_t size)
			{
				if (_input.is_open())
				{
					_input.read(static_cast<char*>(out_data), size);
					char new_line;
					_input.read(&new_line, 1);
				}
			}

		private:
			template <typename T>
			void process(T&& value)
			{
				process_impl(value);
			}

			template <typename T>
			void process_impl(std::vector<T>& out)
			{
				uint64_t size = 0;
				load_binary_data(&size, sizeof(uint64_t));

				out.resize(size);
				for (T& element : out)
				{
					process_impl(element);
				}
			}

			void process_impl(std::string& out)
			{
				uint64_t size = 0;
				load_binary_data(&size, sizeof(uint64_t));

				out.resize(size);
				load_binary_data(out.data(), static_cast<int64_t>(size));
			}

			void process_impl(uint64_t& OutValue) { load_binary_data(&OutValue, sizeof(uint64_t)); }
			void process_impl(uint32_t& OutValue) { load_binary_data(&OutValue, sizeof(uint32_t)); }
			void process_impl(float& OutValue) { load_binary_data(&OutValue, sizeof(float)); }

		private:
			std::ifstream _input;
		};
	}
}













#endif