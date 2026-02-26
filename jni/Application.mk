APP_ABI := arm64-v8a armeabi-v7a
APP_STL := c++_static
APP_OPTIM := release
APP_SHORT_COMMANDS := true
APP_PLATFORM := android-24
APP_CPPFLAGS += -Oz -flto -fvisibility=hidden
APP_LDFLAGS += -Oz -flto -Wl,--gc-sections -Wl,--icf=safe
