/**
  \file G3D-base.lib/source/RegistryUtil.cpp

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/

#include "G3D-base/platform.h"

// This file is only used on Windows
#ifdef G3D_WINDOWS

#include "G3D-base/RegistryUtil.h"
#include "G3D-base/System.h"

namespace G3D {

// static helpers
static HKEY getRootKeyFromString(const char* str, size_t length);


bool RegistryUtil::keyExists(const String& key) {
    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if (hkey == nullptr) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_READ, &openKey);

    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        RegCloseKey(openKey);
        return true;
    } else {
        return false;
    }
}

bool RegistryUtil::valueExists(const String& key, const String& value) {
    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if ( hkey == nullptr ) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_READ, &openKey);
    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        uint32 dataSize = 0;
        result = RegQueryValueExA(openKey, value.c_str(), nullptr, nullptr, nullptr, reinterpret_cast<LPDWORD>(&dataSize));

        debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
        RegCloseKey(openKey);
    }
    return (result == ERROR_SUCCESS);
}


bool RegistryUtil::readInt32(const String& key, const String& value, int32& data) {
    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if ( hkey == nullptr ) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_READ, &openKey);
    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        uint32 dataSize = sizeof(int32);
        result = RegQueryValueExA(openKey, value.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(&data), reinterpret_cast<LPDWORD>(&dataSize));

        debugAssertM(result == ERROR_SUCCESS, "Could not read registry key value.");

        RegCloseKey(openKey);
    }
    return (result == ERROR_SUCCESS);
}

bool RegistryUtil::readBytes(const String& key, const String& value, uint8* data, uint32& dataSize) {
    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if (hkey == nullptr) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_READ, &openKey);
    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        if (data == nullptr) {
            result = RegQueryValueExA(openKey, value.c_str(), nullptr, nullptr, nullptr, reinterpret_cast<LPDWORD>(&dataSize));
        } else {
            result = RegQueryValueExA(openKey, value.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(&data), reinterpret_cast<LPDWORD>(&dataSize));
        }

        debugAssertM(result == ERROR_SUCCESS, "Could not read registry key value.");

        RegCloseKey(openKey);
    }
    return (result == ERROR_SUCCESS);
}


bool RegistryUtil::readString(const String& key, const String& value, String& data) {
    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if (hkey == nullptr) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_READ, &openKey);
    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        uint32 dataSize = 0;

        result = RegQueryValueExA(openKey, value.c_str(), nullptr, nullptr, nullptr, reinterpret_cast<LPDWORD>(&dataSize));

        // increment datasize to allow for non null-terminated strings in registry
        dataSize += 1;

        if (result == ERROR_SUCCESS) {
            char* tmpStr = static_cast<char*>(System::malloc(dataSize));
            System::memset(tmpStr, 0, dataSize);

            result = RegQueryValueExA(openKey, value.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(tmpStr), reinterpret_cast<LPDWORD>(&dataSize));
                
            if (result == ERROR_SUCCESS) {
                data = tmpStr;
            }

            RegCloseKey(openKey);
            System::free(tmpStr);
        }
    }

    return (result == ERROR_SUCCESS);
}


bool RegistryUtil::writeInt32(const String& key, const String& value, int32 data) {
    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if (hkey == nullptr) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_WRITE, &openKey);
    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        result = RegSetValueExA(openKey, value.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&data), sizeof(int32));

        debugAssertM(result == ERROR_SUCCESS, "Could not write registry key value.");

        RegCloseKey(openKey);
    }
    return (result == ERROR_SUCCESS);
}

bool RegistryUtil::writeBytes(const String& key, const String& value, const uint8* data, uint32 dataSize) {
    debugAssert(data);

    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if (hkey == nullptr) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_WRITE, &openKey);
    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        if (data) {
            result = RegSetValueExA(openKey, value.c_str(), 0, REG_BINARY, reinterpret_cast<const BYTE*>(data), dataSize);
        }

        debugAssertM(result == ERROR_SUCCESS, "Could not write registry key value.");

        RegCloseKey(openKey);
    }
    return (result == ERROR_SUCCESS);
}

bool RegistryUtil::writeString(const String& key, const String& value, const String& data) {
    size_t pos = key.find('\\', 0);
    if (pos == String::npos) {
        return false;
    }

    HKEY hkey = getRootKeyFromString(key.c_str(), pos);

    if (hkey == nullptr) {
        return false;
    }

    HKEY openKey;
    int32 result = RegOpenKeyExA(hkey, (key.c_str() + pos + 1), 0, KEY_WRITE, &openKey);
    debugAssert(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    if (result == ERROR_SUCCESS) {
        alwaysAssertM(data.size() < 0xFFFFFFFE, "String too long");
        result = RegSetValueExA(openKey, value.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(data.c_str()), (int)(data.size() + 1));                
        debugAssertM(result == ERROR_SUCCESS, "Could not write registry key value.");

        RegCloseKey(openKey);
    }
    return (result == ERROR_SUCCESS);
}


// static helpers
static HKEY getRootKeyFromString(const char* str, size_t length) {
    debugAssert(str);

    if (str) {
        if ( strncmp(str, "HKEY_CLASSES_ROOT", length) == 0 ) {
            return HKEY_CLASSES_ROOT;
        } else if  ( strncmp(str, "HKEY_CURRENT_CONFIG", length) == 0 ) {
            return HKEY_CURRENT_CONFIG;
        } else if  ( strncmp(str, "HKEY_CURRENT_USER", length) == 0 ) {
            return HKEY_CURRENT_USER;
        } else if  ( strncmp(str, "HKEY_LOCAL_MACHINE", length) == 0 ) {
            return HKEY_LOCAL_MACHINE;
        } else if  ( strncmp(str, "HKEY_PERFORMANCE_DATA", length) == 0 ) {
            return HKEY_PERFORMANCE_DATA;
        } else if  ( strncmp(str, "HKEY_PERFORMANCE_NLSTEXT", length) == 0 ) {
            return HKEY_PERFORMANCE_NLSTEXT;
        } else if  ( strncmp(str, "HKEY_PERFORMANCE_TEXT", length) == 0 ) {
            return HKEY_PERFORMANCE_TEXT;
        } else if  ( strncmp(str, "HKEY_CLASSES_ROOT", length) == 0 ) {
            return HKEY_CLASSES_ROOT;
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

} // namespace G3D

#endif // G3D_WINDOWS
