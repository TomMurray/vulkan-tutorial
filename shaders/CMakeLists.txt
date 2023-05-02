
add_custom_command(OUTPUT vertex.spirv
  COMMAND glslc -fshader-stage=vertex ${CMAKE_CURRENT_SOURCE_DIR}/vertex.glsl -o vertex.spirv
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vertex.glsl
  VERBATIM
)

add_custom_command(OUTPUT fragment.spirv
  COMMAND glslc -fshader-stage=fragment ${CMAKE_CURRENT_SOURCE_DIR}/fragment.glsl -o fragment.spirv
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fragment.glsl
  VERBATIM
)

add_custom_target(shaders DEPENDS
  vertex.spirv
  fragment.spirv
)