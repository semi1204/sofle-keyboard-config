if(CONFIG_ZMK_BEHAVIOR_PROFILE_MODIFIER)
  zephyr_library()
  zephyr_library_sources(src/behavior_profile_modifier.c)
endif()
