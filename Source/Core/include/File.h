#ifndef CORE_FILE_H
#define CORE_FILE_H
#include <filesystem>
#include <string>
#include <fstream>
#include "SysCall.h"

namespace FTS
{
	inline bool IsFileExist(const char* InPath)
	{
		const std::filesystem::path Path(InPath);
		return std::filesystem::exists(Path);
	}

	inline std::filesystem::file_time_type GetFileLastWriteTime(const char* InPath)
	{
		const std::filesystem::path Path(InPath);
		return std::filesystem::last_write_time(Path);
	}

	inline bool CompareFileWriteTime(const char* FirstFile, const char* SecondFile)
	{
		return GetFileLastWriteTime(FirstFile) > GetFileLastWriteTime(SecondFile);
	}

	inline std::string RemoveFileExtension(const char* InPath)
	{
		const std::filesystem::path Path(InPath);
		return Path.filename().replace_extension().string();
	}


	namespace Serialization
	{
		class BinaryOutput
		{
		public:
			BinaryOutput(const std::string& InFileName) : Output(InFileName, std::ios::binary) {}
			~BinaryOutput() { Output.close(); }

			template <typename... Args>
			void operator()(Args&&... Arguments)
			{
				(Process(Arguments), ...);
			}

		public:
			void SaveBinaryData(const void* InData, INT64 InSize)
			{
				if (Output.is_open())
				{
					Output.write(static_cast<const char*>(InData), InSize);
					Output.write("\n", 1);
				}
			}

		private:
			template <typename T>
			void Process(T&& InValue)
			{
				ProcessImpl(InValue);
			}

			template <typename T>
			void ProcessImpl(const std::vector<T>& InValue)
			{
				UINT64 Size = InValue.size();
				SaveBinaryData(&Size, sizeof(UINT64));
				for (const T& Element : InValue)
				{
					ProcessImpl(Element);
				}
			}

			void ProcessImpl(const std::string& InValue)
			{
				UINT64 Size = InValue.size();
				SaveBinaryData(&Size, sizeof(UINT64));
				SaveBinaryData(InValue.data(), static_cast<INT64>(Size));
			}

			void ProcessImpl(UINT64 InValue) { SaveBinaryData(&InValue, sizeof(UINT64)); }
			void ProcessImpl(UINT32 InValue) { SaveBinaryData(&InValue, sizeof(UINT32)); }
			void ProcessImpl(FLOAT InValue) { SaveBinaryData(&InValue, sizeof(FLOAT)); }

		private:
			std::ofstream Output;
		};

		class BinaryInput
		{
		public:
			BinaryInput(const std::string& InFileName) : Input(InFileName, std::ios::binary) {}
			~BinaryInput() noexcept { Input.close(); }

			template <typename... Args>
			void operator()(Args&&... Arguments)
			{
				(Process(Arguments), ...);
			}

		public:
			void LoadBinaryData(void* OutData, INT64 InSize)
			{
				if (Input.is_open())
				{
					Input.read(static_cast<char*>(OutData), InSize);
					CHAR cNewline;
					Input.read(&cNewline, 1);
				}
			}

		private:
			template <typename T>
			void Process(T&& InValue)
			{
				ProcessImpl(InValue);
			}

			template <typename T>
			void ProcessImpl(std::vector<T>& OutValue)
			{
				UINT64 Size = 0;
				LoadBinaryData(&Size, sizeof(UINT64));

				OutValue.resize(Size);
				for (T& Element : OutValue)
				{
					ProcessImpl(Element);
				}
			}

			void ProcessImpl(std::string& OutValue)
			{
				UINT64 Size = 0;
				LoadBinaryData(&Size, sizeof(UINT64));

				OutValue.resize(Size);
				LoadBinaryData(OutValue.data(), static_cast<INT64>(Size));
			}

			void ProcessImpl(UINT64& OutValue) { LoadBinaryData(&OutValue, sizeof(UINT64)); }
			void ProcessImpl(UINT32& OutValue) { LoadBinaryData(&OutValue, sizeof(UINT32)); }
			void ProcessImpl(FLOAT& OutValue) { LoadBinaryData(&OutValue, sizeof(FLOAT)); }

		private:
			std::ifstream Input;
		};
	}
}













#endif