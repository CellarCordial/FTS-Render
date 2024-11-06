#ifndef CORE_FILE_H
#define CORE_FILE_H
#include <filesystem>


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

}













#endif