# ------------------------------------------------------------------------------------------------ #
#                               This file is part of CosmoScout VR                                #
# ------------------------------------------------------------------------------------------------ #

# SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
# SPDX-License-Identifier: MIT

option(CSP_MESSE "Enable compilation of this plugin" ON)

if (NOT CSP_MESSE)
  return()
endif()

file(GLOB SOURCE_FILES src/*.cpp)
file(GLOB HEADER_FILES src/*.hpp)
file(GLOB_RECURSE RESOUCRE_FILES gui/*)

add_library(csp-messe SHARED
  ${SOURCE_FILES}
  ${HEADER_FILES}
  ${RESOUCRE_FILES}
)

target_link_libraries(csp-messe PUBLIC
  cs-core
)

set_property(TARGET csp-messe PROPERTY FOLDER "plugins")
set_source_files_properties(${RESOUCRE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
  ${SOURCE_FILES}
  ${HEADER_FILES}
  ${RESOUCRE_FILES}
)

install(TARGETS csp-messe DESTINATION "share/plugins")
install(DIRECTORY "gui" DESTINATION "share/resources")
