ADD_LIBRARY(webdsplugin-cmd SHARED main.cpp)
ADD_DEPENDENCIES(webdsplugin-cmd webdsutil)
TARGET_LINK_LIBRARIES(webdsplugin-cmd webdsutil ${CMAKE_DL_LIBS} ${GTK3_LIBRARIES} ${WEBKITGTK3_LIBRARIES})
