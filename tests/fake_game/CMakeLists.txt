# What the launch and browser tests start: it says what it was given and ends as told.

LktApp(LookoutFakeGame main.cpp)
LktAddWindowsManifest(LookoutFakeGame)

target_link_libraries(LookoutFakeGame PRIVATE Tge::Base)
