# CMake module configuration for mod-twow-item-level-scaling
#
# Sources under src/ are discovered automatically by the Turtle vMaNGOS module loader,
# conf/*.conf.dist is copied automatically to the build output directory,
# and Addmod_twow_item_level_scalingScripts() is called on module initialization.
#
# This file is evaluated during two phases:
#   TORTOISE_MODULE_CMAKE_PHASE=DISCOVERY
#   TORTOISE_MODULE_CMAKE_PHASE=POST_TARGETS
#
# Uses TW_* macros exclusively. AC_* macros are strictly forbidden.

if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "DISCOVERY")
    message(STATUS "[mod-twow-item-level-scaling] Discovering module sources...")
endif()

if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "POST_TARGETS")
    message(STATUS "[mod-twow-item-level-scaling] Configured target integration.")
endif()

# Compilation Policy: Handled exclusively by the user/operator (Compilation Default OFF).
option(ITEM_SCALING_BUILD_TESTS "Build mod-twow-item-level-scaling standalone tests" OFF)
