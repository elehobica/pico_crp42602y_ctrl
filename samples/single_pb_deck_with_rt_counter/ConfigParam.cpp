/*-----------------------------------------------------------/
/ ConfigParam.h
/------------------------------------------------------------/
/ Copyright (c) 2021, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#include <cstdio>
#include <cstring>
#include <cinttypes>
#include "UserFlash.h"
#include "ConfigParam.h"

using namespace ConfigParam;


ConfigParamUser& ConfigParamUser::instance()
{
    static ConfigParamUser* _instPtr = nullptr;
    if (_instPtr == nullptr) {
        _instPtr = new ConfigParamUser(__NUM_CFG_PARAMS__);
        //_instPtr->initialize(LoadBehavior::FORCE_LOAD_DEFAULT);  // uncomment this instead of below to clear flash to default
        _instPtr->initialize();
        _instPtr->incBootCount();
    }
    return *_instPtr;
}

ConfigParamUser::ConfigParamUser(int numParams)
{
    _numParams = numParams;
    for (int i = 0; i < _numParams; i++) {
        ParamItem_t *item = &configParamItems[i];
        if (item->ptr == nullptr) {
            item->ptr = malloc(item->size);
        }
    }
    loadDefault();
}

ConfigParamUser::~ConfigParamUser()
{
    for (int i = 0; i < _numParams; i++) {
        ParamItem_t *item = &configParamItems[i];
        if (item->ptr != nullptr) {
            free(item->ptr);
        }
    }
    delete &instance();
}

void ConfigParamUser::printInfo()
{
    printf("=== ConfigParamUser ===\n");
    for (int i = 0; i < _numParams; i++) {
        ParamItem_t *item = &configParamItems[i];
        switch (item->paramType) {
            case ParamType::CFG_BOOL_T:
            case ParamType::CFG_UINT8_T:
                {
                    uint8_t *ptr = reinterpret_cast<uint8_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIu8 "d (0x%" PRIx8 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_UINT16_T:
                {
                    uint16_t *ptr = reinterpret_cast<uint16_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIu16 "d (0x%" PRIx16 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_UINT32_T:
                {
                    uint32_t *ptr = reinterpret_cast<uint32_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIu32 "d (0x%" PRIx32 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_UINT64_T:
                {
                    uint64_t *ptr = reinterpret_cast<uint64_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIu64 "d (0x%" PRIx64 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_INT8_T:
                {
                    int8_t *ptr = reinterpret_cast<int8_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIi8 "d (0x%" PRIx8 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_INT16_T:
                {
                    int16_t *ptr = reinterpret_cast<int16_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIi16 "d (0x%" PRIx16 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_INT32_T:
                {
                    int32_t *ptr = reinterpret_cast<int32_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIi32 "d (0x%" PRIx32 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_INT64_T:
                {
                    int64_t *ptr = reinterpret_cast<int64_t *>(item->ptr);
                    printf("0x%04x %s: %" PRIi64 "d (0x%" PRIx64 ")\n", item->flashAddr, item->name, *ptr, *ptr);
                }
                break;
            case ParamType::CFG_STRING_T:
                {
                    char *ptr = reinterpret_cast<char *>(item->ptr);
                    printf("0x%04x %s: %s\n", item->flashAddr, item->name, ptr);
                }
                break;
            default:
                break;
        }
    }
}

void ConfigParamUser::loadDefault()
{
    for (int i = 0; i < _numParams; i++) {
        ParamItem_t *item = &configParamItems[i];
        switch (item->paramType) {
            case ParamType::CFG_BOOL_T:
            case ParamType::CFG_UINT8_T:
                {
                    uint8_t *ptr = reinterpret_cast<uint8_t *>(item->ptr);
                    *ptr = static_cast<uint8_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_UINT16_T:
                {
                    uint16_t *ptr = reinterpret_cast<uint16_t *>(item->ptr);
                    *ptr = static_cast<uint16_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_UINT32_T:
                {
                    uint32_t *ptr = reinterpret_cast<uint32_t *>(item->ptr);
                    *ptr = static_cast<uint32_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_UINT64_T:
                {
                    uint64_t *ptr = reinterpret_cast<uint64_t *>(item->ptr);
                    *ptr = static_cast<uint64_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_INT8_T:
                {
                    int8_t *ptr = reinterpret_cast<int8_t *>(item->ptr);
                    *ptr = static_cast<int8_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_INT16_T:
                {
                    int16_t *ptr = reinterpret_cast<int16_t *>(item->ptr);
                    *ptr = static_cast<int16_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_INT32_T:
                {
                    int32_t *ptr = reinterpret_cast<int32_t *>(item->ptr);
                    *ptr = static_cast<int32_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_INT64_T:
                {
                    int64_t *ptr = reinterpret_cast<int64_t *>(item->ptr);
                    *ptr = static_cast<int64_t>(atoi(item->defaultValue));
                }
                break;
            case ParamType::CFG_STRING_T:
                {
                    char *ptr = reinterpret_cast<char *>(item->ptr);
                    memset(ptr, 0, item->size);
                    strncpy(ptr, item->defaultValue, item->size-1);
                }
                break;
            default:
                break;
        }
    }
}

uint32_t ConfigParamUser::getBootCountFromFlash()
{
    ParamItem_t *item = &configParamItems[CFG_BOOT_COUNT];
    uint32_t bootCount;
    userFlash.read(item->flashAddr, item->size, &bootCount);
    return bootCount;
}

void ConfigParamUser::incBootCount()
{
    uint32_t bootCount;
    this->read(CFG_BOOT_COUNT, &bootCount);
    bootCount++;
    this->write(CFG_BOOT_COUNT, &bootCount);
}

void ConfigParamUser::initialize(LoadBehavior loadDefaultBehavior)
{
    // load default
    loadDefault();
    // switch to step to load from Flash
    if (loadDefaultBehavior == LoadBehavior::FORCE_LOAD_DEFAULT) { return; }
    printInfo();
    if (loadDefaultBehavior == LoadBehavior::LOAD_DEFAULT_IF_FLASH_IS_BLANK && getBootCountFromFlash() == 0xffffffffUL) { return; }
    // load from Flash
    for (int i = 0; i < _numParams; i++) {
        ParamItem_t *item = &configParamItems[i];
        userFlash.read(item->flashAddr, item->size, item->ptr);
    }
}

void ConfigParamUser::finalize()
{
    // store to Flash
    for (int i = 0; i < _numParams; i++) {
        ParamItem_t *item = &configParamItems[i];
        userFlash.writeReserve(item->flashAddr, item->size, item->ptr);
    }
    userFlash.program();
}

void ConfigParamUser::read(uint32_t id, void *ptr)
{
    if (id < _numParams) {
        ParamItem_t *item = &configParamItems[id];
        memcpy(ptr, item->ptr, item->size);
    }
}

void ConfigParamUser::write(uint32_t id, const void *ptr)
{
    if (id < _numParams) {
        ParamItem_t *item = &configParamItems[id];
        memcpy(item->ptr, ptr, item->size);
    }
}

bool ConfigParamUser::getBool(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_BOOL_T);
    uint8_t value = *(reinterpret_cast<uint8_t *>(configParamItems[id].ptr));
    return (value != 0) ? true : false;
}

uint8_t ConfigParamUser::getU8(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_UINT8_T);
    return *(reinterpret_cast<uint8_t *>(configParamItems[id].ptr));
}

uint16_t ConfigParamUser::getU16(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_UINT16_T);
    return *(reinterpret_cast<uint16_t *>(configParamItems[id].ptr));
}

uint32_t ConfigParamUser::getU32(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_UINT32_T);
    return *(reinterpret_cast<uint32_t *>(configParamItems[id].ptr));
}

uint64_t ConfigParamUser::getU64(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_UINT64_T);
    return *(reinterpret_cast<uint64_t *>(configParamItems[id].ptr));
}

int8_t ConfigParamUser::getI8(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_INT8_T);
    return *(reinterpret_cast<int8_t *>(configParamItems[id].ptr));
}

int16_t ConfigParamUser::getI16(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_INT16_T);
    return *(reinterpret_cast<int16_t *>(configParamItems[id].ptr));
}

int32_t ConfigParamUser::getI32(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_INT32_T);
    return *(reinterpret_cast<int32_t *>(configParamItems[id].ptr));
}

int64_t ConfigParamUser::getI64(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_INT64_T);
    return *(reinterpret_cast<int64_t *>(configParamItems[id].ptr));
}

char *ConfigParamUser::getStr(uint32_t id)
{
    assert(configParamItems[id].paramType == CFG_STRING_T);
    return reinterpret_cast<char *>(configParamItems[id].ptr);
}

void ConfigParamUser::setU8(uint32_t id, uint8_t val)
{
    assert(configParamItems[id].paramType == CFG_UINT8_T);
    *(reinterpret_cast<uint8_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setU16(uint32_t id, uint16_t val)
{
    assert(configParamItems[id].paramType == CFG_UINT16_T);
    *(reinterpret_cast<uint16_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setU32(uint32_t id, uint32_t val)
{
    assert(configParamItems[id].paramType == CFG_UINT32_T);
    *(reinterpret_cast<uint32_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setU64(uint32_t id, uint64_t val)
{
    assert(configParamItems[id].paramType == CFG_UINT64_T);
    *(reinterpret_cast<uint64_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setI8(uint32_t id, int8_t val)
{
    assert(configParamItems[id].paramType == CFG_INT8_T);
    *(reinterpret_cast<int8_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setI16(uint32_t id, int16_t val)
{
    assert(configParamItems[id].paramType == CFG_INT16_T);
    *(reinterpret_cast<int16_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setI32(uint32_t id, int32_t val)
{
    assert(configParamItems[id].paramType == CFG_INT32_T);
    *(reinterpret_cast<int32_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setI64(uint32_t id, int64_t val)
{
    assert(configParamItems[id].paramType == CFG_INT64_T);
    *(reinterpret_cast<int64_t *>(configParamItems[id].ptr)) = val;
}

void ConfigParamUser::setStr(uint32_t id, char *str)
{
    assert(configParamItems[id].paramType == CFG_STRING_T);
    char *ptr = reinterpret_cast<char *>(configParamItems[id].ptr);
    memset(ptr, 0, configParamItems[id].size);
    strncpy(ptr, str, configParamItems[id].size-1);
}
