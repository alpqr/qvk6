INCLUDEPATH += $$PWD/VulkanMemoryAllocator

clang {
    QMAKE_CFLAGS_WARN_ON += -Wno-unused-parameter -Wno-unused-variable -Wno-missing-field-initializers -Wno-tautological-compare
    QMAKE_CXXFLAGS_WARN_ON = $$QMAKE_CFLAGS_WARN_ON
}
