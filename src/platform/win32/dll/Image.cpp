#include "Image.hpp"
#include "Utils.hpp"

#include "FileVer-inl.hpp"
#include "SemVer-inl.hpp"

namespace
{
// Build ID of a loaded module: the CodeView PDB GUID and age from its debug directory, in symbol server form (32 hex
// digits GUID, then the age in hex). The Windows counterpart of the Mac LC_UUID. Empty if there is none.
std::string ReadBuildId(HMODULE aModule)
{
    // CV_INFO_PDB70, the CodeView record MSVC links into the debug directory
    struct CodeViewPdb70
    {
        DWORD signature;
        GUID guid;
        DWORD age;
        // char pdbFileName[]
    };
    constexpr DWORD rsdsSignature = 0x53445352; // "RSDS"

    const auto base = reinterpret_cast<const uint8_t*>(aModule);
    const auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return {};
    }

    const auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
        ntHeaders->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG)
    {
        return {};
    }

    const uint64_t sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    const auto& debugDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!debugDir.VirtualAddress || uint64_t{debugDir.VirtualAddress} + debugDir.Size > sizeOfImage)
    {
        return {};
    }

    const auto entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(base + debugDir.VirtualAddress);
    for (DWORD i = 0; i < debugDir.Size / sizeof(IMAGE_DEBUG_DIRECTORY); i++)
    {
        const auto& entry = entries[i];
        // AddressOfRawData is 0 when the record isn't mapped into memory
        if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || !entry.AddressOfRawData ||
            entry.SizeOfData < sizeof(CodeViewPdb70) || uint64_t{entry.AddressOfRawData} + entry.SizeOfData > sizeOfImage)
        {
            continue;
        }

        const auto codeView = reinterpret_cast<const CodeViewPdb70*>(base + entry.AddressOfRawData);
        if (codeView->signature != rsdsSignature)
        {
            continue;
        }

        const auto& g = codeView->guid;
        return fmt::format("{:08X}{:04X}{:04X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:X}", g.Data1, g.Data2,
                           g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6],
                           g.Data4[7], codeView->age);
    }

    return {};
}
} // namespace

Image::Image()
    : m_isAttila(false)
    , m_fileVersion(TWASE_FILEVER(0, 0, 0, 0))
    , m_productVersion(TWASE_SEMVER(0, 0, 0))
{
    std::wstring fileName;
    auto hr = wil::GetModuleFileNameW(nullptr, fileName);
    if (FAILED(hr))
    {
        SHOW_LAST_ERROR_MESSAGE_FILE_LINE(L"Could not get executable's file name.");
        return;
    }

    auto size = GetFileVersionInfoSize(fileName.c_str(), nullptr);
    if (!size)
    {
        auto lastError = GetLastError();
        if (lastError == ERROR_RESOURCE_DATA_NOT_FOUND || lastError == ERROR_RESOURCE_TYPE_NOT_FOUND)
        {
            // Fail silently, executables might not have the version information.
            return;
        }

        SHOW_LAST_ERROR_MESSAGE_FILE_LINE(L"Could not retrieve version info size.\n\nFile name: {}", fileName);
    }

    std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[size]());
    if (!data)
    {
        SHOW_MESSAGE_BOX_FILE_LINE(MB_ICONERROR | MB_OK, L"Could not allocate {} byte(s).\n\nFile name: {}", size,
                                   fileName);
        return;
    }

    if (!GetFileVersionInfo(fileName.c_str(), 0, size, data.get()))
    {
        SHOW_LAST_ERROR_MESSAGE_FILE_LINE(L"Could not retrieve version info.\n\nFile name: {}", fileName);
        return;
    }

    struct LangAndCodePage
    {
        WORD language;
        WORD codePage;
    }* translations;
    uint32_t translationsBytes;

    if (!VerQueryValue(data.get(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations),
                       &translationsBytes))
    {
        SHOW_LAST_ERROR_MESSAGE_FILE_LINE(L"Could not retrieve translations.\n\nFile name: {}", fileName);
        return;
    }

    for (uint32_t i = 0; i < (translationsBytes / sizeof(LangAndCodePage)); i++)
    {
        wchar_t* productName;
        auto subBlock = fmt::format(L"\\StringFileInfo\\{:04x}{:04x}\\ProductName", translations[i].language,
                                    translations[i].codePage);

        if (VerQueryValue(data.get(), subBlock.c_str(), reinterpret_cast<void**>(&productName), &translationsBytes))
        {
            constexpr std::wstring_view expectedProductName = L"Total War: ATTILA";
            if (productName == expectedProductName)
            {
                m_isAttila = true;
                break;
            }
        }
    }

    if (m_isAttila)
    {
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT fileInfoBytes;

        if (!VerQueryValue(data.get(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoBytes))
        {
            SHOW_LAST_ERROR_MESSAGE_FILE_LINE(L"Could not retrieve the file's info.\n\nFile name: {}", fileName);
            return;
        }

        constexpr auto signature = 0xFEEF04BD;
        if (fileInfo->dwSignature != signature)
        {
            SHOW_LAST_ERROR_MESSAGE_FILE_LINE(
                L"The file's signature ({:#x}) did not match the expected value ({:#x}).\n\nFile name: {}",
                fileInfo->dwSignature, signature, fileName);
            return;
        }

        {
            uint16_t major = (fileInfo->dwFileVersionMS >> 16) & 0xFF;
            uint16_t minor = fileInfo->dwFileVersionMS & 0xFFFF;
            uint16_t build = (fileInfo->dwFileVersionLS >> 16) & 0xFFFF;
            uint16_t revision = fileInfo->dwFileVersionLS & 0xFFFF;

            m_fileVersion = TWASE_FILEVER(major, minor, build, revision);
        }

        {
            uint8_t major = (fileInfo->dwProductVersionMS >> 16) & 0xFF;
            uint16_t minor = fileInfo->dwProductVersionMS & 0xFFFF;
            uint32_t patch = (fileInfo->dwProductVersionLS >> 16) & 0xFFFF;

            m_productVersion = TWASE_SEMVER(major, minor, patch);
        }
    }
}

Image* Image::Get()
{
    static Image instance;
    return &instance;
}

bool Image::IsAttila() const
{
    return m_isAttila;
}

bool Image::IsSupported() const
{
    return m_isAttila && m_gameBuild != sdk::Attila::Build::Unknown;
}

void Image::SelectBuild(HMODULE aEmpireDll)
{
    m_buildId = ReadBuildId(aEmpireDll);
    if (m_buildId == sdk::Attila::SteamBuildId)
    {
        m_gameBuild = sdk::Attila::Build::Steam;
    }
    else if (m_buildId == sdk::Attila::GogBuildId)
    {
        m_gameBuild = sdk::Attila::Build::GOG;
    }
    sdk::Attila::detail::selectedBuild = m_gameBuild;
}

const std::string& Image::GetBuildId() const
{
    return m_buildId;
}

const FileVer& Image::GetFileVersion() const
{
    return m_fileVersion;
}

const SemVer& Image::GetProductVersion() const
{
    return m_productVersion;
}
