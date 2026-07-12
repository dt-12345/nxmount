#include "common/unicode.hpp"
#include "fs/filesystem.hpp"

#include <sddl.h>

#include <chrono>
#include <climits>
#include <cwchar>
#include <vector>

namespace nxmount::fs {

struct SD {
	PSECURITY_DESCRIPTOR sd = nullptr;
	ULONG size = 0;

	SD(const wchar_t* sddlString) {
		if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(
			sddlString,
			SDDL_REVISION_1,
			std::addressof(sd),
			std::addressof(size)
		) == FALSE) {
			size = 0;
		}
	}

	~SD() {
		if (sd != nullptr) {
			::LocalFree(sd);
			sd = nullptr;
		}
		size = 0;
	}
};

/*
	Owner: System Built-In Administrators
	Group: System Built-In Administrators
	DACL: SE_DACL_PROTECTED, (Local System File Access All), (Built-In Administrators File Access All) (Everyone File Access All)
*/
static const SD cSD = L"O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)";

[[nodiscard]] ALWAYS_INLINE static auto GetFs(FSP_FILE_SYSTEM* fileSystem) -> IFileSystem* {
	return static_cast<IFileSystem::FsHandle*>(fileSystem->UserContext)->fs.get();
}

[[nodiscard]] static auto NormalizePath(std::wstring_view path) -> std::string {
	return common::Utf16ToUtf8(path);
}

[[nodiscard]] static auto ConvertAccess(DWORD access) -> OpenMode {
	OpenMode out{};
	if (access & FILE_GENERIC_READ & ~(SYNCHRONIZE | READ_CONTROL)) {
		out |= OpenMode::Read;
	}
	if (access & FILE_GENERIC_WRITE & ~(SYNCHRONIZE | READ_CONTROL)) {
		out |= OpenMode::Write;
	}
	if (access & FILE_EXECUTE) {
		out |= OpenMode::Execute;
	}
	return out;
}

static auto GetVolumeInfo(FSP_FILE_SYSTEM* fileSystem, FSP_FSCTL_VOLUME_INFO* volumeInfo) -> NTSTATUS {
	LOG_INFO("GetVolumeInfo");
	auto fs = GetFs(fileSystem);
	try {
		const auto label = common::Utf8ToUtf16(fs->getRoot()->getName());
		::wcscpy_s(volumeInfo->VolumeLabel, sizeof(volumeInfo->VolumeLabel) / sizeof(wchar_t), label.c_str());
	} catch (const std::runtime_error&) {
		/* ... */
	}
	volumeInfo->TotalSize = 0;
	volumeInfo->FreeSize = (std::numeric_limits<UINT64>::max)();

	return STATUS_SUCCESS;
}

static auto SetVolumeLabel_(FSP_FILE_SYSTEM*, PWSTR, FSP_FSCTL_VOLUME_INFO*) -> NTSTATUS {
	return STATUS_INVALID_DEVICE_REQUEST;
}

static auto GetSecurityByName(FSP_FILE_SYSTEM* fileSystem, PWSTR fileName, PUINT32 fileAttributes, PSECURITY_DESCRIPTOR securityDescriptor, SIZE_T* securityDescriptorSize) -> NTSTATUS {
	auto fs = GetFs(fileSystem);
	const auto path = NormalizePath(fileName);

	LOG_INFO("GetSecurityByName {}", path);

	DirectoryEntry entry;
	if (NX_FAILED(fs->getAttributes(path, std::addressof(entry)))) {
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (entry.type == Type::Directory) {
		*fileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	}

	std::memcpy(securityDescriptor, cSD.sd, cSD.size);
	*securityDescriptorSize = cSD.size;

	return STATUS_SUCCESS;
}

static auto Create(FSP_FILE_SYSTEM* fileSystem, PWSTR fileName, UINT32 createOptions, UINT32 grantedAccess, UINT32, PSECURITY_DESCRIPTOR, UINT64, PVOID* fileContext, FSP_FSCTL_FILE_INFO* fileInfo) -> NTSTATUS {
	auto fs = GetFs(fileSystem);
	const auto path = NormalizePath(fileName);

	LOG_INFO("Create {}", path);

	if (createOptions & FILE_DIRECTORY_FILE) {
		std::unique_ptr<IDirectory> dir;
		if (NX_FAILED(fs->createDirectory(std::addressof(dir), path))) {
			return STATUS_INVALID_DEVICE_REQUEST;
		}

		fileInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
		fileInfo->ReparseTag = 0;
		fileInfo->AllocationSize = common::AlignUp(fileInfo->FileSize, 0x200);
		fileInfo->FileSize = 0;
		fileInfo->CreationTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->LastWriteTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->ChangeTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->IndexNumber = 0;
		fileInfo->HardLinks = 0;
		fileInfo->EaSize = 0;

		*fileContext = dir.release();
		return STATUS_SUCCESS;
	}
	else {
		std::unique_ptr<IFile> file;
		if (NX_FAILED(fs->createFile(std::addressof(file), path, ConvertAccess(grantedAccess)))) {
			return STATUS_INVALID_DEVICE_REQUEST;
		}

		fileInfo->FileAttributes = FILE_ATTRIBUTE_READONLY;
		fileInfo->ReparseTag = 0;
		fileInfo->AllocationSize = common::AlignUp(fileInfo->FileSize, 0x200);
		fileInfo->FileSize = file->getSize();
		fileInfo->CreationTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->LastWriteTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->ChangeTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->IndexNumber = 0;
		fileInfo->HardLinks = 0;
		fileInfo->EaSize = 0;

		*fileContext = file.release();
		return STATUS_SUCCESS;
	}
}

static auto Open(FSP_FILE_SYSTEM* fileSystem, PWSTR fileName, UINT32, UINT32 grantedAccess, PVOID* fileContext, FSP_FSCTL_FILE_INFO* fileInfo) -> NTSTATUS {
	auto fs = GetFs(fileSystem);
	const auto path = NormalizePath(fileName);

	LOG_INFO("Open {}", path);

	DirectoryEntry entry;
	if (NX_FAILED(fs->getAttributes(path, std::addressof(entry)))) {
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	std::memset(fileInfo, 0, sizeof(*fileInfo));
	if (entry.type == Type::Directory) {
		std::unique_ptr<IDirectory> dir;
		if (FAILED(fs->openDirectory(std::addressof(dir), path))) {
			return STATUS_DATA_ERROR;
		}

		fileInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
		fileInfo->CreationTime = entry.createTime;
		fileInfo->LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->LastWriteTime = entry.createTime;
		fileInfo->ChangeTime = entry.createTime;

		*fileContext = dir.release();
		return STATUS_SUCCESS;
	} else {
		std::unique_ptr<IFile> file;
		if (FAILED(fs->openFile(std::addressof(file), path, ConvertAccess(grantedAccess)))) {
			return STATUS_DATA_ERROR;
		}

		fileInfo->FileAttributes = FILE_ATTRIBUTE_READONLY;
		fileInfo->FileSize = entry.fileSize;
		fileInfo->CreationTime = entry.createTime;
		fileInfo->LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		fileInfo->LastWriteTime = entry.createTime;
		fileInfo->ChangeTime = entry.createTime;

		*fileContext = file.release();
		return STATUS_SUCCESS;
	}
}

static auto Overwrite(FSP_FILE_SYSTEM*, PVOID, UINT32, BOOLEAN, UINT64, FSP_FSCTL_FILE_INFO*) -> NTSTATUS {
	return STATUS_INVALID_DEVICE_REQUEST;
}

static auto Cleanup(FSP_FILE_SYSTEM*, PVOID, PWSTR, ULONG) -> VOID {
	LOG_INFO("Cleanup");
}

static auto Close(FSP_FILE_SYSTEM*, PVOID fileContext) -> VOID {
	LOG_INFO("Close");
	delete static_cast<INode*>(fileContext);
}

static auto Read(FSP_FILE_SYSTEM*, PVOID fileContext, PVOID buffer, UINT64 offset, ULONG length, PULONG bytesTransferred) -> NTSTATUS {
	LOG_INFO("Read {:#x} {:#x}", length, offset);

	auto file = static_cast<IFile*>(fileContext);

	const auto size = file->read(buffer, length, offset);
	*bytesTransferred = static_cast<ULONG>(size);
	return size != 0 ? STATUS_SUCCESS : STATUS_END_OF_FILE;
}

static auto Write(FSP_FILE_SYSTEM*, PVOID fileContext, PVOID buffer, UINT64 offset, ULONG length, BOOLEAN, BOOLEAN constrainedIo, PULONG bytesTransferred, FSP_FSCTL_FILE_INFO*) -> NTSTATUS {
	LOG_INFO("Write {:#x} {:#x}", length, offset);

	auto file = static_cast<IFile*>(fileContext);
	
	std::size_t len = static_cast<std::size_t>(length);
	if (constrainedIo) {
		const auto fileSize = file->getSize();
		if (offset >= fileSize) {
			return STATUS_SUCCESS;
		}
		if (offset + len > fileSize) {
			len = fileSize - offset;
		}
	}

	*bytesTransferred = static_cast<ULONG>(file->write(buffer, len, offset));
	return STATUS_SUCCESS;
}

static auto Flush(FSP_FILE_SYSTEM*, PVOID fileContext, FSP_FSCTL_FILE_INFO* fileInfo) -> NTSTATUS {
	LOG_INFO("Flush");

	auto node = static_cast<INode*>(fileContext);
	if (node->getType() == Type::Directory) {
		fileInfo->FileAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY;
		fileInfo->FileSize = 0;
	} else {
		static_cast<IFile*>(node)->flush();
		fileInfo->FileAttributes = FILE_ATTRIBUTE_READONLY;
		fileInfo->FileSize = static_cast<IFile*>(node)->getSize();
	}
	fileInfo->ReparseTag = 0;
	fileInfo->AllocationSize = common::AlignUp(fileInfo->FileSize, 0x200);
	fileInfo->CreationTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->LastWriteTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->ChangeTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->IndexNumber = 0;
	fileInfo->HardLinks = 0;
	fileInfo->EaSize = 0;

	return STATUS_SUCCESS;
}

static auto GetFileInfo(FSP_FILE_SYSTEM*, PVOID fileContext, FSP_FSCTL_FILE_INFO* fileInfo) -> NTSTATUS {
	auto node = static_cast<INode*>(fileContext);
	LOG_INFO("GetFileInfo {}", node->getName());

	if (node->getType() == Type::Directory) {
		fileInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
		fileInfo->FileSize = 0;
	} else {
		fileInfo->FileAttributes = FILE_ATTRIBUTE_READONLY;
		fileInfo->FileSize = static_cast<IFile*>(node)->getSize();
	}
	fileInfo->ReparseTag = 0;
	fileInfo->AllocationSize = common::AlignUp(fileInfo->FileSize, 0x200);
	fileInfo->CreationTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // not correct, but we don't have a great way of fetching the fs create time rn (I probably should fix that but I'm lazy)
	fileInfo->LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->LastWriteTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->ChangeTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->IndexNumber = 0;
	fileInfo->HardLinks = 0;
	fileInfo->EaSize = 0;

	return STATUS_SUCCESS;
}

static auto SetFileSize(FSP_FILE_SYSTEM*, PVOID fileContext, UINT64 newSize, BOOLEAN, FSP_FSCTL_FILE_INFO* fileInfo) -> NTSTATUS {
	auto file = static_cast<IFile*>(fileContext);
	LOG_INFO("SetFileSize {}", file->getName());
	
	if (NX_FAILED(file->setSize(newSize))) {
		return STATUS_UNSUCCESSFUL;
	}

	fileInfo->FileAttributes = FILE_ATTRIBUTE_READONLY;
	fileInfo->FileSize = file->getSize();
	fileInfo->ReparseTag = 0;
	fileInfo->AllocationSize = common::AlignUp(fileInfo->FileSize, 0x200);
	fileInfo->CreationTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // not correct, but we don't have a great way of fetching the fs create time rn (I probably should fix that but I'm lazy)
	fileInfo->LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->LastWriteTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->ChangeTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	fileInfo->IndexNumber = 0;
	fileInfo->HardLinks = 0;
	fileInfo->EaSize = 0;

	return STATUS_SUCCESS;
}

static auto Rename(FSP_FILE_SYSTEM*, PVOID, PWSTR, PWSTR, BOOLEAN) -> NTSTATUS {
	LOG_INFO("Rename");
	return STATUS_INVALID_DEVICE_REQUEST;
}

static auto ReadDirectory(FSP_FILE_SYSTEM*, PVOID fileContext, PWSTR, PWSTR marker, PVOID buffer, ULONG bufferLength, PULONG bytesTransferred) -> NTSTATUS {
	auto dir = static_cast<IDirectory*>(fileContext);
	LOG_INFO("ReadDirectory {} (marker: {}, buffer: {:#x})", dir->getName(), marker == nullptr ? "" : common::Utf16ToUtf8(marker), bufferLength);

	std::size_t count;
	if (NX_FAILED(dir->getCount(std::addressof(count)))) {
		return STATUS_DATA_ERROR;
	}

	struct {
		void* buffer;
		ULONG bufferLength;
		PULONG bytesTransferred;
	} dirCbCaptures = { buffer, bufferLength, bytesTransferred };

	auto entryCb = [](const DirectoryEntry& entry, void* userdata) -> bool {
#pragma warning(disable : 4815) // zero-sized array in stack object
		union {
			UINT8 B[FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
			FSP_FSCTL_DIR_INFO D;
		} dirInfoBuf;
#pragma warning(default : 4815) // zero-sized array in stack object

		auto captures = reinterpret_cast<decltype(dirCbCaptures)*>(userdata);

		auto dirInfo = std::addressof(dirInfoBuf.D);
		dirInfo->Size = static_cast<UINT16>(offsetof(FSP_FSCTL_DIR_INFO, FileNameBuf) + entry.name.size() * sizeof(wchar_t));
		if (entry.type == Type::Directory) {
			dirInfo->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
			dirInfo->FileInfo.FileSize = 0;
		}
		else {
			dirInfo->FileInfo.FileAttributes = FILE_ATTRIBUTE_READONLY;
			dirInfo->FileInfo.FileSize = entry.fileSize;
		}
		dirInfo->FileInfo.ReparseTag = 0;
		dirInfo->FileInfo.AllocationSize = common::AlignUp(dirInfo->FileInfo.FileSize, 0x200);
		dirInfo->FileInfo.CreationTime = entry.createTime;
		dirInfo->FileInfo.LastAccessTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		dirInfo->FileInfo.LastWriteTime = entry.createTime;
		dirInfo->FileInfo.ChangeTime = entry.createTime;
		dirInfo->FileInfo.IndexNumber = 0;
		dirInfo->FileInfo.HardLinks = 0;
		dirInfo->FileInfo.EaSize = 0;
		const auto path = common::Utf8ToUtf16(entry.name);
		::wcscpy_s(dirInfo->FileNameBuf, MAX_PATH, path.c_str());

		return FspFileSystemAddDirInfo(dirInfo, captures->buffer, captures->bufferLength, captures->bytesTransferred);
	};

	dir->forEachEntry(entryCb, std::addressof(dirCbCaptures), marker != nullptr ? common::Utf16ToUtf8(marker) : "");

	return STATUS_SUCCESS;
}

const FSP_FILE_SYSTEM_INTERFACE IFileSystem::cFspInterface = {
	.GetVolumeInfo = GetVolumeInfo,
	.SetVolumeLabel = SetVolumeLabel_,
	.GetSecurityByName = GetSecurityByName,
	.Create = Create,
	.Open = Open,
	.Overwrite = Overwrite,
	.Cleanup = Cleanup,
	.Close = Close,
	.Read = Read,
	.Write = Write,
	.Flush = Flush,
	.GetFileInfo = GetFileInfo,
	.SetFileSize = SetFileSize,
	.Rename = Rename,
	.ReadDirectory = ReadDirectory,
};

} // namespace namespace nxmount::fs