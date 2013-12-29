ADD_LIBRARY(webdsplugin-dirlist SHARED main.cpp)
ADD_DEPENDENCIES(webdsplugin-dirlist webdsutil)
TARGET_LINK_LIBRARIES(webdsplugin-dirlist webdsutil ${CMAKE_DL_LIBS} ${GTK3_LIBRARIES} ${WEBKITGTK3_LIBRARIES})
